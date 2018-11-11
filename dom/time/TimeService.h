/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_time_TimeService_h
#define mozilla_dom_time_TimeService_h

#include "mozilla/StaticPtr.h"
#include "nsITimeService.h"

namespace mozilla {
namespace dom {
namespace time {

/**
 * This class implements a service which lets us modify the system clock time.
 */
class TimeService : public nsITimeService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMESERVICE

  static already_AddRefed<TimeService> GetInstance();

private:
  virtual ~TimeService() {};

  static StaticRefPtr<TimeService> sSingleton;
};

} // namespace time
} // namespace dom
} // namespace mozilla

#endif //mozilla_dom_time_TimeService_h
