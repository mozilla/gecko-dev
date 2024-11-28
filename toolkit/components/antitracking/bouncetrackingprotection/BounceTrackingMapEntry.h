/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BounceTrackingMapEntry_h
#define mozilla_BounceTrackingMapEntry_h

#include "mozilla/OriginAttributes.h"
#include "nsIBounceTrackingMapEntry.h"
#include "nsString.h"

namespace mozilla {

/**
 * Represents an entry in the global bounce tracker or user activation map.
 */
class BTPMapEntry {
 public:
  OriginAttributes& OriginAttributesRef() { return mOriginAttributes; }

  nsACString& SiteHostRef() { return mSiteHost; }

  PRTime& TimeStampRef() { return mTimeStamp; }

 protected:
  BTPMapEntry(const OriginAttributes& aOriginAttributes,
              const nsACString& aSiteHost, PRTime aTimeStamp)
      : mOriginAttributes(aOriginAttributes),
        mSiteHost(aSiteHost),
        mTimeStamp(aTimeStamp) {}

  OriginAttributes mOriginAttributes;
  nsAutoCString mSiteHost;
  PRTime mTimeStamp;
};

class BounceTrackingMapEntry final : public BTPMapEntry,
                                     public nsIBounceTrackingMapEntry {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIBOUNCETRACKINGMAPENTRY

  BounceTrackingMapEntry(const OriginAttributes& aOriginAttributes,
                         const nsACString& aSiteHost, PRTime aTimeStamp)
      : BTPMapEntry(aOriginAttributes, aSiteHost, aTimeStamp) {}

 private:
  ~BounceTrackingMapEntry() = default;
};

/**
 * Represents a log entry for a purged bounce tracker. Extends
 * BounceTrackingMapEntry with the time of purge.
 */
class BounceTrackingPurgeEntry final : public BTPMapEntry,
                                       public nsIBounceTrackingPurgeEntry {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIBOUNCETRACKINGMAPENTRY
  NS_DECL_NSIBOUNCETRACKINGPURGEENTRY
  BounceTrackingPurgeEntry(const OriginAttributes& aOriginAttributes,
                           const nsACString& aSiteHost, PRTime aBounceTime,
                           PRTime aPurgeTime)
      : BTPMapEntry(aOriginAttributes, aSiteHost, aBounceTime),
        mPurgeTime(aPurgeTime) {}

  PRTime& BounceTimeRef() { return mTimeStamp; }

  PRTime& PurgeTimeRef() { return mPurgeTime; }

  const PRTime& PurgeTimeRefConst() const { return mPurgeTime; }

 private:
  ~BounceTrackingPurgeEntry() = default;
  // Timestamp of when the purge completed. mTimeStamp is the time of when the
  // bounce ocurred.
  PRTime mPurgeTime;
};

}  // namespace mozilla

#endif
