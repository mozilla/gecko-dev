/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_ORIGINOPERATIONCALLBACKS_H_
#define DOM_QUOTA_ORIGINOPERATIONCALLBACKS_H_

#include "mozilla/Maybe.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/quota/ForwardDecls.h"

namespace mozilla::dom::quota {

struct OriginOperationCallbackOptions {
  bool mWantWillFinish = false;
  bool mWantWillFinishSync = false;
  bool mWantDidFinish = false;
  bool mWantDidFinishSync = false;
};

struct OriginOperationCallbacks {
  Maybe<RefPtr<BoolPromise>> mWillFinishPromise;
  Maybe<RefPtr<ExclusiveBoolPromise>> mWillFinishSyncPromise;
  Maybe<RefPtr<BoolPromise>> mDidFinishPromise;
  Maybe<RefPtr<ExclusiveBoolPromise>> mDidFinishSyncPromise;
};

class OriginOperationCallbackHolders {
 protected:
  MozPromiseHolder<BoolPromise> mWillFinishPromiseHolder;
  MozPromiseHolder<ExclusiveBoolPromise> mWillFinishSyncPromiseHolder;
  MozPromiseHolder<BoolPromise> mDidFinishPromiseHolder;
  MozPromiseHolder<ExclusiveBoolPromise> mDidFinishSyncPromiseHolder;

 public:
  OriginOperationCallbacks GetCallbacks(
      const OriginOperationCallbackOptions& aCallbackOptions) {
    OriginOperationCallbacks callbacks;

    if (aCallbackOptions.mWantWillFinish) {
      callbacks.mWillFinishPromise =
          Some(mWillFinishPromiseHolder.Ensure(__func__));
    }

    if (aCallbackOptions.mWantWillFinishSync) {
      callbacks.mWillFinishSyncPromise =
          Some(mWillFinishSyncPromiseHolder.Ensure(__func__));
    }

    if (aCallbackOptions.mWantDidFinish) {
      callbacks.mDidFinishPromise =
          Some(mDidFinishPromiseHolder.Ensure(__func__));
    }

    if (aCallbackOptions.mWantDidFinishSync) {
      callbacks.mDidFinishSyncPromise =
          Some(mDidFinishSyncPromiseHolder.Ensure(__func__));
    }

    return callbacks;
  }
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_ORIGINOPERATIONCALLBACKS_H_
