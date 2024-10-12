/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PermissionStatusSink_h
#define mozilla_dom_PermissionStatusSink_h

#include "mozilla/dom/PermissionsBinding.h"
#include "mozilla/dom/PermissionStatusBinding.h"
#include "mozilla/MozPromise.h"
#include "mozilla/Mutex.h"
#include "nsIPermission.h"

class nsPIDOMWindowInner;

namespace mozilla::dom {

class PermissionObserver;
class PermissionStatus;
class WeakWorkerRef;

class PermissionStatusSink {
 public:
  using PermissionStatePromise = MozPromise<uint32_t, nsresult, true>;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PermissionStatusSink)

  PermissionStatusSink(PermissionStatus* aPermissionStatus,
                       PermissionName aPermissionName,
                       const nsACString& aPermissionType);

  RefPtr<PermissionStatePromise> Init();

  // These functions should be called when an permission is updated which may
  // change the state of this PermissionStatus. MaybeUpdatedByOnMainThread
  // accepts the permission object itself that is update. When the permission's
  // key is not same-origin with this object's owner window/worker, such as for
  // secondary-keyed permissions like `3rdPartyFrameStorage^...`,
  // MaybeUpdatedByNotifyOnlyOnMainThread will be called with the updated
  // window/worker as an argument. MaybeUpdatedByNotifyOnly must be defined by
  // PermissionStatus inheritors that are double-keyed.
  virtual bool MaybeUpdatedByOnMainThread(nsIPermission* aPermission);
  virtual bool MaybeUpdatedByNotifyOnlyOnMainThread(
      nsPIDOMWindowInner* aInnerWindow);

  void PermissionChangedOnMainThread();

  PermissionName Name() const { return mPermissionName; }

  void Disentangle();

 protected:
  virtual ~PermissionStatusSink();

  virtual RefPtr<PermissionStatePromise> ComputeStateOnMainThread();

  RefPtr<PermissionStatePromise> ComputeStateOnMainThreadInternal(
      nsPIDOMWindowInner* aWindow);

  nsCOMPtr<nsISerialEventTarget> mSerialEventTarget;
  nsCOMPtr<nsIPrincipal> mPrincipalForPermission;

  RefPtr<PermissionObserver> mObserver;

  RefPtr<PermissionStatus> mPermissionStatus;

  Mutex mMutex;

  // Protected by mutex.
  // Created and released on worker-thread. Used also on main-thread.
  RefPtr<WeakWorkerRef> mWorkerRef MOZ_GUARDED_BY(mMutex);

  PermissionName mPermissionName;
  nsCString mPermissionType;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_permissionstatusSink_h
