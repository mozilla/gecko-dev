/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layout_ipc_VsyncChild_h
#define mozilla_layout_ipc_VsyncChild_h

#include "mozilla/layout/PVsyncChild.h"
#include "nsISupportsImpl.h"
#include "mozilla/RefPtr.h"

namespace mozilla {

class VsyncObserver;

namespace ipc {
class BackgroundChildImpl;
}  // namespace ipc

namespace layout {

// The PVsyncChild actor receives a vsync event from the main process and
// delivers it to the child process. Currently this is restricted to the main
// thread only. The actor will stay alive until the process dies or its
// PVsyncParent actor dies.
class VsyncChild final : public PVsyncChild {
  NS_INLINE_DECL_REFCOUNTING(VsyncChild)

  friend class mozilla::ipc::BackgroundChildImpl;

 public:
  // Hide the SendObserve/SendUnobserve in PVsyncChild. We add an flag
  // mObservingVsync to handle the race problem of unobserving vsync event.
  bool SendObserve();
  bool SendUnobserve();

  // Bind a VsyncObserver into VsyncChild after ipc channel connected.
  void SetVsyncObserver(VsyncObserver* aVsyncObserver);
  // GetVsyncRate is a getter for mVsyncRate which sends a requests to
  // VsyncParent to retreive the hardware vsync rate if mVsyncRate
  // hasn't already been set.
  TimeDuration GetVsyncRate();
  // VsyncRate is a getter for mVsyncRate which always returns
  // mVsyncRate directly, potentially returning
  // TimeDuration::Forever() if mVsyncRate hasn't been set by calling
  // GetVsyncRate.
  TimeDuration VsyncRate();

 private:
  VsyncChild();
  virtual ~VsyncChild();

  virtual mozilla::ipc::IPCResult RecvNotify(
      const TimeStamp& aVsyncTimestamp) override;
  virtual mozilla::ipc::IPCResult RecvVsyncRate(
      const float& aVsyncRate) override;
  virtual void ActorDestroy(ActorDestroyReason aActorDestroyReason) override;

  bool mObservingVsync;
  bool mIsShutdown;

  // The content side vsync observer.
  RefPtr<VsyncObserver> mObserver;
  TimeDuration mVsyncRate;
};

}  // namespace layout
}  // namespace mozilla

#endif  // mozilla_layout_ipc_VsyncChild_h
