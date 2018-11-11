/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TimeManager.h"

#include "mozilla/dom/Date.h"
#include "mozilla/dom/MozTimeManagerBinding.h"
#include "nsITimeService.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace dom {
namespace time {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TimeManager)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(TimeManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TimeManager)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TimeManager, mWindow)

JSObject*
TimeManager::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MozTimeManagerBinding::Wrap(aCx, this, aGivenProto);
}

void
TimeManager::Set(Date& aDate)
{
  Set(aDate.ToDouble());
}

void
TimeManager::Set(double aTime)
{
  nsCOMPtr<nsITimeService> timeService = do_GetService(TIMESERVICE_CONTRACTID);
  if (timeService) {
    timeService->Set(aTime);
  }
}

} // namespace time
} // namespace dom
} // namespace mozilla
