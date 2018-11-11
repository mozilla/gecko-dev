/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "jsapi.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Hal.h"
#include "TimeService.h"

namespace mozilla {
namespace dom {
namespace time {

NS_IMPL_ISUPPORTS(TimeService, nsITimeService)

/* static */ StaticRefPtr<TimeService> TimeService::sSingleton;

/* static */ already_AddRefed<TimeService>
TimeService::GetInstance()
{
  if (!sSingleton) {
    sSingleton = new TimeService();
    ClearOnShutdown(&sSingleton);
  }
  RefPtr<TimeService> service = sSingleton.get();
  return service.forget();
}

NS_IMETHODIMP
TimeService::Set(int64_t aTimeInMS) {
  hal::AdjustSystemClock(aTimeInMS - (JS_Now() / PR_USEC_PER_MSEC));
  return NS_OK;
}

} // namespace time
} // namespace dom
} // namespace mozilla
