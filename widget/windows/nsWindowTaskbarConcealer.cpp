/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWindowTaskbarConcealer.h"

#include "nsIWinTaskbar.h"
#define NS_TASKBAR_CONTRACTID "@mozilla.org/windows-taskbar;1"

#include "mozilla/Logging.h"
#include "mozilla/StaticPrefs_widget.h"
#include "WinUtils.h"

using namespace mozilla;

/**
 * TaskbarConcealerImpl
 *
 * Implement Windows-fullscreen marking.
 *
 * nsWindow::TaskbarConcealer implements logic determining _whether_ to tell
 * Windows that a given window is fullscreen. TaskbarConcealerImpl performs the
 * platform-specific work of actually communicating that fact to Windows.
 *
 * (This object is not persistent; it's constructed on the stack when needed.)
 */
struct TaskbarConcealerImpl {
  void MarkAsHidingTaskbar(HWND aWnd, bool aMark);

  // Determination of the mechanism used to set the window state. (Hopefully
  // temporary: see comments in StaticPrefList.yaml for the relevant pref.)
  enum class MarkingMethod : uint32_t {
    NonRudeHwnd = 1,
    PrepareFullScreen = 2,
  };
  static MarkingMethod GetMarkingMethod() {
    uint32_t const val =
        StaticPrefs::widget_windows_fullscreen_marking_method();
    if (val >= 1 && val <= 3) return MarkingMethod(val);

    // By default, use both.
    // - Bug 1952284 shows that NonRudeHwnd is insufficient.
    // - Bug 1949079 comment 15 shows that PrepareFullScreen is insufficient.
    return MarkingMethod(3);
  }

 private:
  nsCOMPtr<nsIWinTaskbar> mTaskbarInfo;

  // local cache
  MarkingMethod const mMarkingMethod = GetMarkingMethod();
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(TaskbarConcealerImpl::MarkingMethod);

/**
 * nsWindow::TaskbarConcealer
 *
 * Issue taskbar-hide requests to the OS as needed.
 */

/*
  Per MSDN [0], one should mark and unmark fullscreen windows via the
  ITaskbarList2::MarkFullscreenWindow method. Unfortunately, Windows pays less
  attention to this than one might prefer -- in particular, it typically fails
  to show the taskbar when switching focus from a window marked as fullscreen to
  one not thus marked. [1]

  Experimentation suggests that its behavior has usually been reasonable [2]
  when switching between multiple monitors, or between a set of windows which
  are all from different processes [3]. This leaves us to handle the
  same-monitor, same-process case.

  Rather than do anything subtle here, we take the blanket approach of simply
  listening for every potentially-relevant state change, and then explicitly
  marking or unmarking every potentially-visible toplevel window.

  ----

  [0] Relevant link:
      https://docs.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-itaskbarlist2-markfullscreenwindow

  [1] This is an oversimplification; Windows' actual behavior here is...
      complicated. See bug 1732517 comment 6 for some examples.

  [2] (2025-02-24) Unfortunately, the heuristics appear not to be static. Recent
      versions of Windows 10, at least, may misinterpret a simple maximized
      windows with custom titlebar as full-screen.

  [3] A comment in Chromium asserts that this is actually different threads. For
      us, of course, that makes no difference.
      https://github.com/chromium/chromium/blob/2b822268bd3/ui/views/win/hwnd_message_handler.cc#L1342
*/

/**************************************************************
 *
 * SECTION: TaskbarConcealer utilities
 *
 **************************************************************/

static mozilla::LazyLogModule sTaskbarConcealerLog("TaskbarConcealer");

// Map of all relevant Gecko windows, along with the monitor on which each
// window was last known to be located.
/* static */
MOZ_RUNINIT nsTHashMap<HWND, HMONITOR>
    nsWindow::TaskbarConcealer::sKnownWindows;

// Returns Nothing if the window in question is irrelevant (for any reason),
// or Some(the window's current state) otherwise.
/* static */
Maybe<nsWindow::TaskbarConcealer::WindowState>
nsWindow::TaskbarConcealer::GetWindowState(HWND aWnd) {
  // Classical Win32 visibility conditions.
  if (!::IsWindowVisible(aWnd)) {
    return Nothing();
  }
  if (::IsIconic(aWnd)) {
    return Nothing();
  }

  // Non-nsWindow windows associated with this thread may include file dialogs
  // and IME input popups.
  nsWindow* pWin = widget::WinUtils::GetNSWindowPtr(aWnd);
  if (!pWin) {
    return Nothing();
  }

  // nsWindows of other window-classes include tooltips and drop-shadow-bearing
  // menus.
  if (pWin->mWindowType != WindowType::TopLevel) {
    return Nothing();
  }

  // Cloaked windows are (presumably) on a different virtual desktop.
  // https://devblogs.microsoft.com/oldnewthing/20200302-00/?p=103507
  if (pWin->mIsCloaked) {
    return Nothing();
  }

  return Some(
      WindowState{::MonitorFromWindow(aWnd, MONITOR_DEFAULTTONULL),
                  pWin->mFrameState->GetSizeMode() == nsSizeMode_Fullscreen});
}

/**************************************************************
 *
 * SECTION: TaskbarConcealer::UpdateAllState
 *
 **************************************************************/

// Update all Windows-fullscreen-marking state and internal caches to represent
// the current state of the system.
/* static */
void nsWindow::TaskbarConcealer::UpdateAllState(
    HWND destroyedHwnd /* = nullptr */
) {
  // sKnownWindows is otherwise-unprotected shared state
  MOZ_ASSERT(NS_IsMainThread(),
             "TaskbarConcealer can only be used from the main thread!");

  if (MOZ_LOG_TEST(sTaskbarConcealerLog, LogLevel::Info)) {
    static size_t sLogCounter = 0;
    MOZ_LOG(sTaskbarConcealerLog, LogLevel::Info,
            ("Calling UpdateAllState() for the %zuth time", sLogCounter++));

    MOZ_LOG(sTaskbarConcealerLog, LogLevel::Info, ("Last known state:"));
    if (sKnownWindows.IsEmpty()) {
      MOZ_LOG(sTaskbarConcealerLog, LogLevel::Info,
              ("  none (no windows known)"));
    } else {
      for (const auto& entry : sKnownWindows) {
        MOZ_LOG(
            sTaskbarConcealerLog, LogLevel::Info,
            ("  window %p was on monitor %p", entry.GetKey(), entry.GetData()));
      }
    }
  }

  // Array of all our potentially-relevant HWNDs, in Z-order (topmost first),
  // along with their associated relevant state.
  struct Item {
    HWND hwnd;
    HMONITOR monitor;
    bool isGkFullscreen;
  };
  const nsTArray<Item> windows = [&] {
    nsTArray<Item> windows;

    // USE OF UNDOCUMENTED BEHAVIOR: The EnumWindows family of functions
    // enumerates windows in Z-order, topmost first. (This has been true since
    // at least Windows 2000, and possibly since Windows 3.0.)
    //
    // It's necessarily unreliable if windows are reordered while being
    // enumerated; but in that case we'll get a message informing us of that
    // fact, and can redo our state-calculations then.
    //
    // There exists no documented interface to acquire this information (other
    // than ::GetWindow(), which is racy).
    mozilla::EnumerateThreadWindows([&](HWND hwnd) {
      // Depending on details of window-destruction that probably shouldn't be
      // relied on, this HWND may or may not still be in the window list.
      // Pretend it's not.
      if (hwnd == destroyedHwnd) {
        return;
      }

      const auto maybeState = GetWindowState(hwnd);
      if (!maybeState) {
        return;
      }
      const WindowState& state = *maybeState;

      windows.AppendElement(Item{.hwnd = hwnd,
                                 .monitor = state.monitor,
                                 .isGkFullscreen = state.isGkFullscreen});
    });

    return windows;
  }();

  // Relevant monitors are exactly those with relevant windows.
  const nsTHashSet<HMONITOR> relevantMonitors = [&]() {
    nsTHashSet<HMONITOR> relevantMonitors;
    for (const Item& item : windows) {
      relevantMonitors.Insert(item.monitor);
    }
    return relevantMonitors;
  }();

  // Update the cached mapping from windows to monitors. (This is only used as
  // an optimization in TaskbarConcealer::OnWindowPosChanged().)
  sKnownWindows.Clear();
  for (const Item& item : windows) {
    MOZ_LOG(
        sTaskbarConcealerLog, LogLevel::Debug,
        ("Found relevant window %p on monitor %p", item.hwnd, item.monitor));
    sKnownWindows.InsertOrUpdate(item.hwnd, item.monitor);
  }

  // Auxiliary function. Does what it says on the tin.
  const auto FindUppermostWindowOn = [&windows](HMONITOR aMonitor) -> HWND {
    for (const Item& item : windows) {
      if (item.monitor == aMonitor) {
        MOZ_LOG(sTaskbarConcealerLog, LogLevel::Info,
                ("on monitor %p, uppermost relevant HWND is %p", aMonitor,
                 item.hwnd));
        return item.hwnd;
      }
    }

    // This should never happen, since we're drawing our monitor-set from the
    // set of relevant windows.
    MOZ_LOG(sTaskbarConcealerLog, LogLevel::Warning,
            ("on monitor %p, no relevant windows were found", aMonitor));
    return nullptr;
  };

  TaskbarConcealerImpl impl;

  // Mark all relevant windows as not hiding the taskbar, unless they're both
  // Gecko-fullscreen and the uppermost relevant window on their monitor.
  for (HMONITOR monitor : relevantMonitors) {
    const HWND topmost = FindUppermostWindowOn(monitor);

    for (const Item& item : windows) {
      if (item.monitor != monitor) continue;
      impl.MarkAsHidingTaskbar(item.hwnd,
                               item.isGkFullscreen && item.hwnd == topmost);
    }
  }
}  // nsWindow::TaskbarConcealer::UpdateAllState()

// Mark this window as requesting to occlude, or not occlude, the taskbar. (The
// caller is responsible for keeping any local state up-to-date.)
void TaskbarConcealerImpl::MarkAsHidingTaskbar(HWND aWnd, bool aMark) {
  // ## NOTE ON UNDERDOCUMENTED BEHAVIOR:
  //
  // A section of the `ITaskbarList2::MarkFullscreenWindow` documentation
  // follows: [0]
  //
  //    Setting the value of _fFullscreen_ to **TRUE**, the Shell treats this
  //    window as a full-screen window, and the taskbar is moved to the bottom
  //    of the z-order when this window is active. Setting the value of
  //    _fFullscreen_ to **FALSE** removes the full-screen marking, but does not
  //    cause the Shell to treat the window as though it were definitely not
  //    full-screen. With a **FALSE** _fFullscreen_ value, the Shell depends on
  //    its automatic detection facility to specify how the window should be
  //    treated, possibly still flagging the window as full-screen.
  //
  //    **Since Windows 7**, call `SetProp(hwnd, L”NonRudeHWND”,
  //    reinterpret_cast<HANDLE>(TRUE))` before showing a window to indicate to
  //    the Shell that the window should not be treated as full-screen.
  //
  // This is not entirely accurate. Furthermore, even where accurate, it's
  // underspecified, and the behavior has differed in important ways.
  //
  // * Under Windows 8.1 and early versions of Windows 10, a window will never
  //   be considered fullscreen if the window-property "NonRudeHWND" is set to
  //   `TRUE` before the window is shown, even if that property is later
  //   removed. (See commentary in patch D146635.)
  //
  //   (Note: no record was made of what happened if the property was only added
  //   after window creation. Presumably it didn't help.)
  //
  // * Under Windows 7 and current versions of Windows 10+, a window will not be
  //   considered fullscreen if the window-property "NonRudeHWND" is set to
  //   `TRUE` when a check for fullscreenness is performed, regardless of
  //   whether it was ever previously set. (Again, see commentary in patch
  //   D146635.)
  //
  // * Under at least some versions of Windows 10, explicitly calling
  //   `MarkFullscreenWindow(hwnd, FALSE)` on a window _already marked `FALSE`_
  //   will sometimes cause a window improperly detected as fullscreen to no
  //   longer be thus misdetected. (See `TaskbarConcealer::OnWindowMaximized()`,
  //   and commentary in patch D239277.)
  //
  // The version of Win10 in which this behavior was adjusted is not presently
  // known -- indeed, at time of writing, there's no evidence that the developer
  // responsible for the claims in that first bullet point (also the present
  // author) didn't simply perform the tests improperly. (See comments in bug
  // 1950441 for the current known bounds.)
  //
  // For now, we implement both methods of marking, and use an `about:config`
  // pref to select which of them to use.
  //
  // [0] https://web.archive.org/web/20211223073250/https://docs.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-itaskbarlist2-markfullscreenwindow

  const char* const sMark = aMark ? "true" : "false";

  bool const useNonRudeHWND = !!(mMarkingMethod & MarkingMethod::NonRudeHwnd);
  bool const usePrepareFullScreen =
      !!(mMarkingMethod & MarkingMethod::PrepareFullScreen);

  // at least one must be set
  MOZ_ASSERT(useNonRudeHWND || usePrepareFullScreen);

  if (useNonRudeHWND) {
    MOZ_LOG(sTaskbarConcealerLog, LogLevel::Info,
            ("Setting %p[L\"NonRudeHWND\"] to %s", aWnd, sMark));

    // (setting the property to `FALSE` is not known to be functionally distinct
    // from removing it)
    ::SetPropW(aWnd, L"NonRudeHWND", (HANDLE)uintptr_t(aMark ? FALSE : TRUE));
  } else {
    ::RemovePropW(aWnd, L"NonRudeHWND");
  }

  if (usePrepareFullScreen) {
    if (!mTaskbarInfo) {
      mTaskbarInfo = do_GetService(NS_TASKBAR_CONTRACTID);

      if (!mTaskbarInfo) {
        MOZ_LOG(
            sTaskbarConcealerLog, LogLevel::Warning,
            ("could not acquire IWinTaskbar (aWnd %p, aMark %s)", aWnd, sMark));
        return;
      }
    }

    MOZ_LOG(sTaskbarConcealerLog, LogLevel::Info,
            ("Calling PrepareFullScreen(%p, %s)", aWnd, sMark));

    const nsresult hr = mTaskbarInfo->PrepareFullScreen(aWnd, aMark);

    if (FAILED(hr)) {
      MOZ_LOG(sTaskbarConcealerLog, LogLevel::Error,
              ("Call to PrepareFullScreen(%p, %s) failed with nsresult %x",
               aWnd, sMark, uint32_t(hr)));
    }
  }
}

/**************************************************************
 *
 * SECTION: TaskbarConcealer event callbacks
 *
 **************************************************************/

void nsWindow::TaskbarConcealer::OnWindowDestroyed(HWND aWnd) {
  MOZ_LOG(sTaskbarConcealerLog, LogLevel::Info,
          ("==> OnWindowDestroyed() for HWND %p", aWnd));

  UpdateAllState(aWnd);
}

void nsWindow::TaskbarConcealer::OnFocusAcquired(nsWindow* aWin) {
  // Update state unconditionally.
  //
  // This is partially because focus-acquisition only updates the z-order, which
  // we don't cache and therefore can't notice changes to -- but also because
  // it's probably a good idea to give the user a natural way to refresh the
  // current fullscreen-marking state if it's somehow gone bad.

  MOZ_LOG(sTaskbarConcealerLog, LogLevel::Info,
          ("==> OnFocusAcquired() for HWND %p on HMONITOR %p", aWin->mWnd,
           ::MonitorFromWindow(aWin->mWnd, MONITOR_DEFAULTTONULL)));

  UpdateAllState();
}

void nsWindow::TaskbarConcealer::OnWindowMaximized(nsWindow* aWin) {
  MOZ_LOG(sTaskbarConcealerLog, LogLevel::Info,
          ("==> OnWindowMaximized() for HWND %p on HMONITOR %p", aWin->mWnd,
           ::MonitorFromWindow(aWin->mWnd, MONITOR_DEFAULTTONULL)));

  // This is a workaround for a failure of `PrepareFullScreen`, and is only
  // useful when that's the only marking-mechanism in play.
  if (MOZ_LIKELY(TaskbarConcealerImpl::GetMarkingMethod() !=
                 TaskbarConcealerImpl::MarkingMethod::PrepareFullScreen)) {
    return;
  }

  // If we're not using a custom nonclient area, then it's obvious to Windows
  // that we're not trying to be fullscreen, so the bug won't occur.
  if (!aWin->mCustomNonClient) {
    return;
  }

  // Mark this window, and only this window, as not-fullscreen. Everything else
  // can stay as it is. (This matches what UpdateAllState would do, if called.)
  //
  // Note: this is an unjustified hack. According to the documentation of
  // `ITaskbarList2::MarkFullscreenWindow()`, it should have no effect, but
  // testing confirms that it sometimes does. See bug 1949079.
  //
  (TaskbarConcealerImpl{}).MarkAsHidingTaskbar(aWin->mWnd, false);
}

void nsWindow::TaskbarConcealer::OnFullscreenChanged(nsWindow* aWin,
                                                     bool enteredFullscreen) {
  MOZ_LOG(sTaskbarConcealerLog, LogLevel::Info,
          ("==> OnFullscreenChanged() for HWND %p on HMONITOR %p", aWin->mWnd,
           ::MonitorFromWindow(aWin->mWnd, MONITOR_DEFAULTTONULL)));

  UpdateAllState();
}

void nsWindow::TaskbarConcealer::OnWindowPosChanged(nsWindow* aWin) {
  // Optimization: don't bother updating the state if the window hasn't moved
  // from its monitor (including appearances and disappearances).
  const HWND myHwnd = aWin->mWnd;
  const HMONITOR oldMonitor = sKnownWindows.Get(myHwnd);  // or nullptr
  const HMONITOR newMonitor = GetWindowState(myHwnd)
                                  .map([](auto state) { return state.monitor; })
                                  .valueOr(nullptr);

  if (oldMonitor == newMonitor) {
    return;
  }

  MOZ_LOG(sTaskbarConcealerLog, LogLevel::Info,
          ("==> OnWindowPosChanged() for HWND %p (HMONITOR %p -> %p)", myHwnd,
           oldMonitor, newMonitor));

  UpdateAllState();
}

void nsWindow::TaskbarConcealer::OnAsyncStateUpdateRequest(HWND hwnd) {
  MOZ_LOG(sTaskbarConcealerLog, LogLevel::Info,
          ("==> OnAsyncStateUpdateRequest()"));

  // Work around a race condition in explorer.exe.
  //
  // When a window is unminimized (and on several other events), the taskbar
  // receives a notification that it needs to recalculate the current
  // is-a-fullscreen-window-active-here-state ("rudeness") of each monitor.
  // Unfortunately, this notification is sent concurrently with the
  // WM_WINDOWPOSCHANGING message that performs the unminimization.
  //
  // Until that message is resolved, the window's position is still "minimized".
  // If the taskbar processes its notification faster than the window handles
  // its WM_WINDOWPOSCHANGING message, then the window will appear to the
  // taskbar to still be minimized, and won't be taken into account for
  // computing rudeness. This usually presents as a just-unminimized Firefox
  // fullscreen-window occasionally having the taskbar stuck above it.
  //
  // Unfortunately, it's a bit difficult to improve Firefox's speed-of-response
  // to WM_WINDOWPOSCHANGING messages (we can, and do, execute JavaScript during
  // these), and even if we could that wouldn't always fix it. We instead adopt
  // a variant of a strategy by Etienne Duchamps, who has investigated and
  // documented this issue extensively[0]: we simply send another signal to the
  // shell to notify it to recalculate the current rudeness state of all
  // monitors.
  //
  // [0] https://github.com/dechamps/RudeWindowFixer#a-race-condition-activating-a-minimized-window
  //
  static UINT const shellHookMsg = ::RegisterWindowMessageW(L"SHELLHOOK");
  if (shellHookMsg != 0) {
    // Identifying the particular thread of the particular instance of the
    // shell associated with our current desktop is probably possible, but
    // also probably not worth the effort. Just broadcast the message
    // globally.
    DWORD info = BSM_APPLICATIONS;
    ::BroadcastSystemMessage(BSF_POSTMESSAGE | BSF_IGNORECURRENTTASK, &info,
                             shellHookMsg, HSHELL_WINDOWACTIVATED,
                             (LPARAM)hwnd);
  }
}

void nsWindow::TaskbarConcealer::OnCloakChanged() {
  MOZ_LOG(sTaskbarConcealerLog, LogLevel::Info, ("==> OnCloakChanged()"));

  UpdateAllState();
}
