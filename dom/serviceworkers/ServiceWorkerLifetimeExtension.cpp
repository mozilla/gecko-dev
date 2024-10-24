/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ServiceWorkerLifetimeExtension.h"

namespace mozilla::dom {

bool ServiceWorkerLifetimeExtension::LifetimeExtendsIntoTheFuture(
    uint32_t aRequiredSecs) const {
  // Figure out the new deadline, a null TimeStamp means no change.
  return this->match(
      [](const NoLifetimeExtension& nle) { return false; },
      [aRequiredSecs](const PropagatedLifetimeExtension& ple) {
        TimeStamp minFuture =
            TimeStamp::NowLoRes() + TimeDuration::FromSeconds(aRequiredSecs);

        // Ignore null deadlines or deadlines that don't extend sufficiently
        // into the future.
        return !(ple.mDeadline.IsNull() || ple.mDeadline < minFuture);
      },
      [](const FullLifetimeExtension& fle) { return true; });
}

}  // namespace mozilla::dom
