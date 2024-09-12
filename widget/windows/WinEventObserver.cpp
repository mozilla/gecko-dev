/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <corecrt_memcpy_s.h>
#include <windows.h>
#include <winternl.h>
#include <winuser.h>
#include <wtsapi32.h>

#include "WinEventObserver.h"
#include "WinWindowOcclusionTracker.h"

#include "mozilla/Assertions.h"
#include "mozilla/Logging.h"
#include "nsWindowDbg.h"
#include "nsdefs.h"
#include "nsXULAppAPI.h"

// borrowed from devblogs.microsoft.com/oldnewthing/20041025-00/?p=37483, by way
// of the Chromium sandboxing code's "current_module.h"
extern "C" IMAGE_DOS_HEADER __ImageBase;
#define CURRENT_MODULE() reinterpret_cast<HMODULE>(&__ImageBase)

// N.B.: if and when we eliminate the existing `WindowType::Invisible` hidden
// window, we must switch to use of `kClassNameHidden` for the class name. (See
// commentary therebeside.)
const wchar_t kClassNameHidden2[] = L"MozillaHiddenWindowClass2";

namespace mozilla::widget {

LazyLogModule gWinEventWindowLog("WinEventWindow");
#define OBS_LOG(...) \
  MOZ_LOG(gWinEventWindowLog, ::mozilla::LogLevel::Info, (__VA_ARGS__))

namespace {
static HWND sHiddenWindow = nullptr;
}

/* static */
void WinEventWindow::Ensure() {
  if (sHiddenWindow) return;

  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  HMODULE const hSelf = CURRENT_MODULE();
  WNDCLASSW const wc = {.lpfnWndProc = WinEventWindow::WndProc,
                        .hInstance = hSelf,
                        .lpszClassName = kClassNameHidden2};
  ATOM const atom = ::RegisterClassW(&wc);
  if (!atom) {
    // This is known to be possible when the atom table no longer has free
    // entries, which unfortunately happens more often than one might expect.
    // See bug 1571516.
    auto volatile const err [[maybe_unused]] = ::GetLastError();
    MOZ_CRASH("could not register broadcast-receiver window-class");
  }

  sHiddenWindow =
      ::CreateWindowW((LPCWSTR)(uintptr_t)atom, L"WinEventWindow", 0, 0, 0, 0,
                      0, nullptr, nullptr, hSelf, nullptr);

  if (!sHiddenWindow) {
    MOZ_CRASH("could not create broadcast-receiver window");
  }
};

/* static */
HWND WinEventWindow::GetHwndForTestingOnly() { return sHiddenWindow; }

// Callbacks for individual event types. These are private and internal
// implementation details of WinEventWindow.
namespace {
namespace evtwin_details {

static void OnSessionChange(WPARAM wParam, LPARAM lParam) {
  if (wParam == WTS_SESSION_LOCK || wParam == WTS_SESSION_UNLOCK) {
    DWORD currentSessionId;
    BOOL const rv =
        ::ProcessIdToSessionId(::GetCurrentProcessId(), &currentSessionId);
    if (rv) {
      // A process should always have the relevant access privileges for itself,
      // but the above call could still fail if, e.g., someone's playing games
      // with function imports. If so, just assert and/or skip out.
      //
      // Should this turn out to somehow be a real concern, we could do
      // ```
      //    DWORD const currentSessionId =
      //       ::NtCurrentTeb()->ProcessEnvironmentBlock->SessionId;
      // ```
      // instead, which is actually documented (albeit abjured against).
      MOZ_ASSERT(false, "::ProcessIdToSessionId() failed");
      return;
    }

    OBS_LOG("WinEventWindow OnSessionChange(): wParam=%zu lParam=%" PRIdLPTR
            " currentSessionId=%lu",
            wParam, lParam, currentSessionId);

    // Ignore lock/unlock messages for other sessions -- which Windows actually
    // _does_ send in some scenarios; see review of Chromium changeset 1929489:
    //
    // https://chromium-review.googlesource.com/c/chromium/src/+/1929489
    if (currentSessionId != (DWORD)lParam) {
      return;
    }

    if (auto* wwot = WinWindowOcclusionTracker::Get()) {
      wwot->OnSessionChange(wParam);
    }
  }
}

static void OnPowerBroadcast(WPARAM wParam, LPARAM lParam) {
  if (wParam == PBT_POWERSETTINGCHANGE) {
    POWERBROADCAST_SETTING* setting = (POWERBROADCAST_SETTING*)lParam;
    MOZ_ASSERT(setting);

    if (::IsEqualGUID(setting->PowerSetting, GUID_SESSION_DISPLAY_STATUS) &&
        setting->DataLength == sizeof(DWORD)) {
      MONITOR_DISPLAY_STATE state{};
      errno_t const err =
          ::memcpy_s(&state, sizeof(state), setting->Data, setting->DataLength);
      if (err) {
        MOZ_ASSERT(false, "bad data in POWERBROADCAST_SETTING in lParam");
        return;
      }

      bool const displayOn = MONITOR_DISPLAY_STATE::PowerMonitorOff != state;

      OBS_LOG("WinEventWindow OnPowerBroadcast(): displayOn=%d",
              int(displayOn ? 1 : 0));

      if (auto* wwot = WinWindowOcclusionTracker::Get()) {
        wwot->OnDisplayStateChanged(displayOn);
      }
    }
  }
}
}  // namespace evtwin_details
}  // namespace

// static
LRESULT CALLBACK WinEventWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                         LPARAM lParam) {
  NativeEventLogger eventLogger("WinEventWindow", hwnd, msg, wParam, lParam);

  switch (msg) {
    case WM_WINDOWPOSCHANGING: {
      // prevent rude external programs from making hidden window visible
      LPWINDOWPOS info = (LPWINDOWPOS)lParam;
      info->flags &= ~SWP_SHOWWINDOW;
    } break;

    case WM_WTSSESSION_CHANGE: {
      evtwin_details::OnSessionChange(wParam, lParam);
    } break;

    case WM_POWERBROADCAST: {
      evtwin_details::OnPowerBroadcast(wParam, lParam);
    } break;
  }

  LRESULT ret = ::DefWindowProcW(hwnd, msg, wParam, lParam);
  eventLogger.SetResult(ret, false);
  return ret;
}

#undef OBS_LOG

}  // namespace mozilla::widget
