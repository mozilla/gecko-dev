/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PerformanceResourceTiming_h___
#define mozilla_dom_PerformanceResourceTiming_h___

#include "nsCOMPtr.h"
#include "nsPerformance.h"
#include "nsIChannel.h"
#include "nsITimedChannel.h"
#include "nsDOMNavigationTiming.h"
#include "PerformanceEntry.h"

namespace mozilla {
namespace dom {

// http://www.w3.org/TR/resource-timing/#performanceresourcetiming
class PerformanceResourceTiming final : public PerformanceEntry
{
public:
  typedef mozilla::TimeStamp TimeStamp;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(
      PerformanceResourceTiming,
      PerformanceEntry)

  PerformanceResourceTiming(nsPerformanceTiming* aPerformanceTiming,
                            nsPerformance* aPerformance,
                            const nsAString& aName);

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;


  virtual DOMHighResTimeStamp StartTime() const override;

  virtual DOMHighResTimeStamp Duration() const override
  {
    return ResponseEnd() - StartTime();
  }

  void GetInitiatorType(nsAString& aInitiatorType) const
  {
    aInitiatorType = mInitiatorType;
  }

  void SetInitiatorType(const nsAString& aInitiatorType)
  {
    mInitiatorType = aInitiatorType;
  }

  DOMHighResTimeStamp FetchStart() const {
    return mTiming
        ? mTiming->FetchStartHighRes()
        : 0;
  }

  DOMHighResTimeStamp RedirectStart() const {
    // We have to check if all the redirect URIs had the same origin (since
    // there is no check in RedirectEndHighRes())
    return mTiming && mTiming->ShouldReportCrossOriginRedirect()
        ? mTiming->RedirectStartHighRes()
        : 0;
  }

  DOMHighResTimeStamp RedirectEnd() const {
    // We have to check if all the redirect URIs had the same origin (since
    // there is no check in RedirectEndHighRes())
    return mTiming && mTiming->ShouldReportCrossOriginRedirect()
        ? mTiming->RedirectEndHighRes()
        : 0;
  }

  DOMHighResTimeStamp DomainLookupStart() const {
    return mTiming && mTiming->TimingAllowed()
        ? mTiming->DomainLookupStartHighRes()
        : 0;
  }

  DOMHighResTimeStamp DomainLookupEnd() const {
    return mTiming && mTiming->TimingAllowed()
        ? mTiming->DomainLookupEndHighRes()
        : 0;
  }

  DOMHighResTimeStamp ConnectStart() const {
    return mTiming && mTiming->TimingAllowed()
        ? mTiming->ConnectStartHighRes()
        : 0;
  }

  DOMHighResTimeStamp ConnectEnd() const {
    return mTiming && mTiming->TimingAllowed()
        ? mTiming->ConnectEndHighRes()
        : 0;
  }

  DOMHighResTimeStamp RequestStart() const {
    return mTiming && mTiming->TimingAllowed()
        ? mTiming->RequestStartHighRes()
        : 0;
  }

  DOMHighResTimeStamp ResponseStart() const {
    return mTiming && mTiming->TimingAllowed()
        ? mTiming->ResponseStartHighRes()
        : 0;
  }

  DOMHighResTimeStamp ResponseEnd() const {
    return mTiming
        ? mTiming->ResponseEndHighRes()
        : 0;
  }

  DOMHighResTimeStamp SecureConnectionStart() const
  {
    // This measurement is not available for Navigation Timing either.
    // There is a different bug submitted for it.
    return 0;
  }

protected:
  virtual ~PerformanceResourceTiming();

  nsString mInitiatorType;
  nsRefPtr<nsPerformanceTiming> mTiming;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_PerformanceResourceTiming_h___ */
