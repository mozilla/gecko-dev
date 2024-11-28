/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BounceTrackingStateGlobal_h
#define mozilla_BounceTrackingStateGlobal_h

#include "BounceTrackingMapEntry.h"
#include "BounceTrackingProtectionStorage.h"
#include "mozilla/WeakPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTHashMap.h"
#include "nsTArray.h"
#include "nsISupports.h"

namespace mozilla {

/**
 * This class holds the global state maps which are used to keep track of
 * potential bounce trackers and user activations.
 * @see BounceTrackingState for the per browser / tab state.
 *
 * Updates to the state maps are persisted to storage.
 */
class BounceTrackingStateGlobal final {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(BounceTrackingStateGlobal);
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(BounceTrackingStateGlobal);

  BounceTrackingStateGlobal(BounceTrackingProtectionStorage* aStorage,
                            const OriginAttributes& aAttrs);

  bool IsPrivateBrowsing() const {
    return mOriginAttributes.IsPrivateBrowsing();
  }

  bool ShouldPersistToDisk() const { return !IsPrivateBrowsing(); }

  const OriginAttributes& OriginAttributesRef() const {
    return mOriginAttributes;
  };

  bool HasUserActivation(const nsACString& aSiteHost) const;

  // Store a user interaction flag for  the given host. This will remove the
  // host from the bounce tracker map if it exists.
  [[nodiscard]] nsresult RecordUserActivation(const nsACString& aSiteHost,
                                              PRTime aTime,
                                              bool aSkipStorage = false);

  // Test-only method to clear a user activation flag.
  [[nodiscard]] nsresult TestRemoveUserActivation(const nsACString& aSiteHost);

  // Clear any user interactions that happened before aTime.
  [[nodiscard]] nsresult ClearUserActivationBefore(PRTime aTime);

  bool HasBounceTracker(const nsACString& aSiteHost) const;

  // Store a bounce tracker flag for the given host. A host which received user
  // interaction recently can not be recorded as a bounce tracker.
  [[nodiscard]] nsresult RecordBounceTracker(const nsACString& aSiteHost,
                                             PRTime aTime,
                                             bool aSkipStorage = false);

  // Record the fact that we have purged state for a bounce tracker. This is
  // used in the purged trackers log.
  [[nodiscard]] nsresult RecordPurgedTracker(
      const RefPtr<BounceTrackingPurgeEntry>& aEntry);

  // Remove one or many bounce trackers identified by site host.
  [[nodiscard]] nsresult RemoveBounceTrackers(
      const nsTArray<nsCString>& aSiteHosts);

  // Clear user activation or bounce tracker map.
  [[nodiscard]] nsresult ClearByType(
      BounceTrackingProtectionStorage::EntryType aType, bool aSkipStorage);

  [[nodiscard]] nsresult ClearSiteHost(const nsACString& aSiteHost,
                                       bool aSkipStorage = false);

  [[nodiscard]] nsresult ClearByTimeRange(
      PRTime aFrom, Maybe<PRTime> aTo = Nothing(),
      Maybe<BounceTrackingProtectionStorage::EntryType> aEntryType = Nothing(),
      bool aSkipStorage = false);

  const nsTHashMap<nsCStringHashKey, PRTime>& UserActivationMapRef() {
    return mUserActivation;
  }

  const nsTHashMap<nsCStringHashKey, PRTime>& BounceTrackersMapRef() {
    return mBounceTrackers;
  }

  const nsTHashMap<nsCStringHashKey,
                   nsTArray<RefPtr<BounceTrackingPurgeEntry>>>&
  RecentPurgesMapRef() {
    return mRecentPurges;
  }

  // Create a string that describes this object. Used for logging.
  nsCString Describe();

 private:
  ~BounceTrackingStateGlobal() = default;

  // The storage which manages this state global. Used to persist changes to
  // this state global in storage.
  // This needs to be a weak pointer to avoid BounceTrackingProtectionStorage
  // and BounceTrackingStateGlobal holding strong references to each other
  // leading to memory leaks.
  WeakPtr<BounceTrackingProtectionStorage> mStorage;

  // Origin attributes this state global is associated with. e.g. if the state
  // was associated with a PBM window this would set privateBrowsingId: 1.
  OriginAttributes mOriginAttributes;

  // Map of site hosts to moments. The moments represent the most recent wall
  // clock time at which the user activated a top-level document on the
  // associated site host.
  nsTHashMap<nsCStringHashKey, PRTime> mUserActivation;

  // Map of site hosts to moments. The moments represent the first wall clock
  // time since the last execution of the bounce tracking timer at which a page
  // on the given site host performed an action that could indicate stateful
  // bounce tracking took place.
  nsTHashMap<nsCStringHashKey, PRTime> mBounceTrackers;

  // Log of purges which happened since application startup. Keyed by site host.
  // The log is used for both troubleshooting purposes and for logging warnings
  // to the web console for affected sites.
  nsTHashMap<nsCStringHashKey, nsTArray<RefPtr<BounceTrackingPurgeEntry>>>
      mRecentPurges;

  // Helper to create a string representation of a siteHost -> timestamp map.
  static nsCString DescribeMap(
      const nsTHashMap<nsCStringHashKey, PRTime>& aMap);
};

}  // namespace mozilla

#endif
