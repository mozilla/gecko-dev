/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _WaylandVsyncSource_h_
#define _WaylandVsyncSource_h_

#include "base/thread.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"
#include "mozilla/Monitor.h"
#include "mozilla/layers/NativeLayerWayland.h"
#include "MozContainer.h"
#include "nsWaylandDisplay.h"
#include "VsyncSource.h"
#include "WaylandSurface.h"

namespace mozilla {

using layers::NativeLayerRootWayland;

/*
 * WaylandVsyncSource
 *
 * This class provides a per-widget VsyncSource under Wayland, emulated using
 * frame callbacks on the widget surface with empty surface commits.
 *
 * Wayland does not expose vsync/vblank, as it considers that an implementation
 * detail the clients should not concern themselves with. Instead, frame
 * callbacks are provided whenever the compositor believes it is a good time to
 * start drawing the next frame for a particular surface, giving us as much
 * time as possible to do so.
 *
 * Note that the compositor sends frame callbacks only when it sees fit, and
 * when that may be is entirely up to the compositor. One cannot expect a
 * certain rate of callbacks, or any callbacks at all. Examples of common
 * variations would be surfaces moved between outputs with different refresh
 * rates, and surfaces that are hidden and therefore do not receieve any
 * callbacks at all. Other hypothetical scenarios of variation could be
 * throttling to conserve power, or because a user has requested it.
 *
 */
class WaylandVsyncSource final : public gfx::VsyncSource {
 public:
  explicit WaylandVsyncSource(nsWindow* aWindow);
  virtual ~WaylandVsyncSource();

  static Maybe<TimeDuration> GetFastestVsyncRate();

  void EnableVSyncSource();
  void DisableVSyncSource();

  // Regular VSync callback. Runs for visible windows only.
  // aTime = 0 means emulated frame and use current time.
  void VisibleWindowCallback(uint32_t aTime = 0);

  // Idle callback for hidden windows.
  // Returns whether we should keep firing.
  bool HiddenWindowCallback();

  TimeDuration GetVsyncRate() override;

  // Called from gecko code. It generally enables/disables VSync state
  // regardless of actual VSync source.
  void EnableVsync() override;
  void DisableVsync() override;
  bool IsVsyncEnabled() override;
  void Shutdown() override;

  // We addref/unref this during init so we should not
  // call it from constructor.
  void Init();

 private:
  Maybe<TimeDuration> GetVsyncRateIfEnabled();

  void CalculateVsyncRateLocked(const MutexAutoLock& aProofOfLock,
                                TimeStamp aVsyncTimestamp);
  void* GetWindowForLogging() { return mWindow; };

  void SetHiddenWindowVSync();

  Mutex mMutex;

  // Main thread only, except for logging.
  RefPtr<nsWindow> mWindow;
  RefPtr<widget::WaylandSurface> mWaylandSurface MOZ_GUARDED_BY(mMutex);

  bool mIsShutdown MOZ_GUARDED_BY(mMutex) = false;
  bool mVsyncEnabled MOZ_GUARDED_BY(mMutex) = false;
  bool mVsyncSourceEnabled MOZ_GUARDED_BY(mMutex) = false;

  TimeDuration mVsyncRate MOZ_GUARDED_BY(mMutex);
  TimeStamp mLastVsyncTimeStamp MOZ_GUARDED_BY(mMutex);
  uint32_t mLastFrameTime MOZ_GUARDED_BY(mMutex) = 0;

  guint mHiddenWindowTimerID = 0;    // Main thread only.
  const guint mHiddenWindowTimeout;  // Main thread only.
};

}  // namespace mozilla

#endif  // _WaylandVsyncSource_h_
