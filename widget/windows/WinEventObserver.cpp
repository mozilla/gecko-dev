/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <winuser.h>
#include <wtsapi32.h>

#include "WinEventObserver.h"
#include "WinWindowOcclusionTracker.h"

#include "mozilla/Assertions.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Logging.h"
#include "mozilla/Maybe.h"
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
namespace evtwin_details {
static HWND sHiddenWindow = nullptr;
static bool sHiddenWindowShutdown = false;
}  // namespace evtwin_details
}  // namespace

/* static */
void WinEventWindow::Ensure() {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  using namespace evtwin_details;

  if (sHiddenWindow) return;
  if (sHiddenWindowShutdown) return;

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

  // It should be harmless to leak this window until destruction -- but other
  // parts of Gecko may expect all windows to be destroyed, so do that.
  mozilla::RunOnShutdown([]() {
    sHiddenWindowShutdown = true;
    ::DestroyWindow(sHiddenWindow);
    sHiddenWindow = nullptr;
  });
};

/* static */
HWND WinEventWindow::GetHwndForTestingOnly() {
  return evtwin_details::sHiddenWindow;
}

// Callbacks for individual event types. These are private and internal
// implementation details of WinEventWindow.
namespace {
namespace evtwin_details {

static void OnSessionChange(WPARAM wParam, LPARAM lParam) {
  if (wParam == WTS_SESSION_LOCK || wParam == WTS_SESSION_UNLOCK) {
    Maybe<bool> isCurrentSession;
    DWORD currentSessionId = 0;
    if (!::ProcessIdToSessionId(::GetCurrentProcessId(), &currentSessionId)) {
      isCurrentSession = Nothing();
    } else {
      OBS_LOG(
          "WinEventWindow OnSessionChange() wParam %zu lParam "
          "%" PRIdLPTR " currentSessionId %lu",
          wParam, lParam, currentSessionId);

      isCurrentSession = Some(static_cast<DWORD>(lParam) == currentSessionId);
    }
    if (auto* wwot = WinWindowOcclusionTracker::Get()) {
      wwot->OnSessionChange(wParam, isCurrentSession);
    }
  }
}

static void OnPowerBroadcast(WPARAM wParam, LPARAM lParam) {
  if (wParam == PBT_POWERSETTINGCHANGE) {
    POWERBROADCAST_SETTING* setting = (POWERBROADCAST_SETTING*)lParam;

    if (setting &&
        ::IsEqualGUID(setting->PowerSetting, GUID_SESSION_DISPLAY_STATUS) &&
        setting->DataLength == sizeof(DWORD)) {
      bool displayOn = PowerMonitorOff !=
                       static_cast<MONITOR_DISPLAY_STATE>(setting->Data[0]);

      OBS_LOG("WinEventWindow OnPowerBroadcast() displayOn %d", displayOn);

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
