/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BounceTrackingProtection.h"

#include "BounceTrackingAllowList.h"
#include "BounceTrackingProtectionStorage.h"
#include "BounceTrackingState.h"
#include "BounceTrackingRecord.h"
#include "BounceTrackingMapEntry.h"
#include "ClearDataCallback.h"
#include "PromiseNativeWrapper.h"

#include "BounceTrackingStateGlobal.h"
#include "ErrorList.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Logging.h"
#include "mozilla/Maybe.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/dom/Promise.h"
#include "nsDebug.h"
#include "nsHashPropertyBag.h"
#include "nsIClearDataService.h"
#include "nsIObserverService.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsISupports.h"
#include "nsISupportsPrimitives.h"
#include "nsServiceManagerUtils.h"
#include "nscore.h"
#include "prtime.h"
#include "mozilla/dom/BrowsingContext.h"
#include "xpcpublic.h"
#include "mozilla/glean/GleanMetrics.h"
#include "mozilla/ContentBlockingLog.h"
#include "mozilla/glean/GleanPings.h"
#include "mozilla/dom/WindowContext.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "nsIConsoleService.h"
#include "mozilla/intl/Localization.h"

#define TEST_OBSERVER_MSG_RECORD_BOUNCES_FINISHED "test-record-bounces-finished"

namespace mozilla {

NS_IMPL_ISUPPORTS(BounceTrackingProtection, nsIObserver,
                  nsISupportsWeakReference, nsIBounceTrackingProtection);

LazyLogModule gBounceTrackingProtectionLog("BounceTrackingProtection");

static StaticRefPtr<BounceTrackingProtection> sBounceTrackingProtection;
static bool sInitFailed = false;

// Keeps track of whether the feature is enabled based on pref state.
// Initialized on first call of GetSingleton.
Maybe<bool> BounceTrackingProtection::sFeatureIsEnabled;

static const char kBTPModePref[] = "privacy.bounceTrackingProtection.mode";

static constexpr uint32_t TRACKER_PURGE_FLAGS =
    nsIClearDataService::CLEAR_ALL_CACHES | nsIClearDataService::CLEAR_COOKIES |
    nsIClearDataService::CLEAR_DOM_STORAGES |
    nsIClearDataService::CLEAR_CLIENT_AUTH_REMEMBER_SERVICE |
    nsIClearDataService::CLEAR_EME | nsIClearDataService::CLEAR_MEDIA_DEVICES |
    nsIClearDataService::CLEAR_STORAGE_ACCESS |
    nsIClearDataService::CLEAR_AUTH_TOKENS |
    nsIClearDataService::CLEAR_AUTH_CACHE;

// static
already_AddRefed<BounceTrackingProtection>
BounceTrackingProtection::GetSingleton() {
  MOZ_ASSERT(XRE_IsParentProcess());

  // Init previously failed, don't try again.
  if (sInitFailed) {
    return nullptr;
  }

  // First call to GetSingleton, check main feature pref and record telemetry.
  if (sFeatureIsEnabled.isNothing()) {
    if (StaticPrefs::privacy_bounceTrackingProtection_mode() ==
        nsIBounceTrackingProtection::MODE_DISABLED) {
      sFeatureIsEnabled = Some(false);

      glean::bounce_tracking_protection::enabled_at_startup.Set(false);
      glean::bounce_tracking_protection::enabled_dry_run_mode_at_startup.Set(
          false);

      // Feature is disabled.
      return nullptr;
    }
    sFeatureIsEnabled = Some(true);

    glean::bounce_tracking_protection::enabled_at_startup.Set(true);
    glean::bounce_tracking_protection::enabled_dry_run_mode_at_startup.Set(
        StaticPrefs::privacy_bounceTrackingProtection_mode() ==
        nsIBounceTrackingProtection::MODE_ENABLED_DRY_RUN);
  }
  MOZ_ASSERT(sFeatureIsEnabled.isSome());

  // Feature is disabled.
  if (!sFeatureIsEnabled.value()) {
    return nullptr;
  }

  // Feature is enabled, lazily create singleton instance.
  if (!sBounceTrackingProtection) {
    sBounceTrackingProtection = new BounceTrackingProtection();
    RunOnShutdown([] {
      if (sBounceTrackingProtection &&
          sBounceTrackingProtection->mRemoteExceptionList) {
        Unused << sBounceTrackingProtection->mRemoteExceptionList->Shutdown();
      }
      sBounceTrackingProtection = nullptr;
    });

    nsresult rv = sBounceTrackingProtection->Init();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      sInitFailed = true;
      return nullptr;
    }
  }

  return do_AddRef(sBounceTrackingProtection);
}

nsresult BounceTrackingProtection::Init() {
  MOZ_ASSERT(StaticPrefs::privacy_bounceTrackingProtection_mode() !=
                 nsIBounceTrackingProtection::MODE_DISABLED,
             "Mode pref must have an enabled state for init to be called.");
  MOZ_LOG(
      gBounceTrackingProtectionLog, LogLevel::Info,
      ("Init BounceTrackingProtection. Config: mode: %d, "
       "bounceTrackingActivationLifetimeSec: %d, bounceTrackingGracePeriodSec: "
       "%d, bounceTrackingPurgeTimerPeriodSec: %d, "
       "clientBounceDetectionTimerPeriodMS: %d, requireStatefulBounces: %d, "
       "HasMigratedUserActivationData: %d",
       StaticPrefs::privacy_bounceTrackingProtection_mode(),
       StaticPrefs::
           privacy_bounceTrackingProtection_bounceTrackingActivationLifetimeSec(),
       StaticPrefs::
           privacy_bounceTrackingProtection_bounceTrackingGracePeriodSec(),
       StaticPrefs::
           privacy_bounceTrackingProtection_bounceTrackingPurgeTimerPeriodSec(),
       StaticPrefs::
           privacy_bounceTrackingProtection_clientBounceDetectionTimerPeriodMS(),
       StaticPrefs::privacy_bounceTrackingProtection_requireStatefulBounces(),
       StaticPrefs::
           privacy_bounceTrackingProtection_hasMigratedUserActivationData()));

  mStorage = new BounceTrackingProtectionStorage();

  nsresult rv = mStorage->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = MaybeMigrateUserInteractionPermissions();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Error,
            ("user activation permission migration failed"));
  }

  // Register feature pref listener which dynamically enables or disables the
  // feature depending on feature pref state.
  rv = Preferences::RegisterCallback(&BounceTrackingProtection::OnPrefChange,
                                     kBTPModePref);
  NS_ENSURE_SUCCESS(rv, rv);

  // Run the remaining init logic.
  return OnModeChange(true);
}

nsresult BounceTrackingProtection::UpdateBounceTrackingPurgeTimer(
    bool aShouldEnable) {
  // Cancel the existing timer.
  // If disabling: we're done now.
  // If enabling: schedule a new timer so interval changes (as controlled by the
  // pref) are taken into account.
  if (mBounceTrackingPurgeTimer) {
    mBounceTrackingPurgeTimer->Cancel();
    mBounceTrackingPurgeTimer = nullptr;
  }

  if (!aShouldEnable) {
    return NS_OK;
  }

  // Schedule timer for tracker purging. The timer interval is determined by
  // pref.
  uint32_t purgeTimerPeriod = StaticPrefs::
      privacy_bounceTrackingProtection_bounceTrackingPurgeTimerPeriodSec();

  // The pref can be set to 0 to disable interval purging.
  if (purgeTimerPeriod == 0) {
    return NS_OK;
  }

  MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
          ("Scheduling mBounceTrackingPurgeTimer. Interval: %d seconds.",
           purgeTimerPeriod));

  return NS_NewTimerWithCallback(
      getter_AddRefs(mBounceTrackingPurgeTimer),
      [](auto) {
        if (!sBounceTrackingProtection) {
          return;
        }
        sBounceTrackingProtection->PurgeBounceTrackers()->Then(
            GetMainThreadSerialEventTarget(), __func__,
            [] {
              MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
                      ("%s: PurgeBounceTrackers finished after timer call.",
                       __FUNCTION__));
            },
            [] { NS_WARNING("RunPurgeBounceTrackers failed"); });
      },
      purgeTimerPeriod * PR_MSEC_PER_SEC, nsITimer::TYPE_REPEATING_SLACK,
      "mBounceTrackingPurgeTimer");
}

// static
void BounceTrackingProtection::OnPrefChange(const char* aPref, void* aData) {
  MOZ_ASSERT(sBounceTrackingProtection);
  MOZ_ASSERT(strcmp(kBTPModePref, aPref) == 0);
  sBounceTrackingProtection->OnModeChange(false);
}

nsresult BounceTrackingProtection::OnModeChange(bool aIsStartup) {
  // Get feature mode from pref and ensure its within bounds.
  uint8_t modeInt = StaticPrefs::privacy_bounceTrackingProtection_mode();
  NS_ENSURE_TRUE(modeInt <= nsIBounceTrackingProtection::MAX_MODE_VALUE,
                 NS_ERROR_FAILURE);
  nsIBounceTrackingProtection::Modes mode =
      static_cast<nsIBounceTrackingProtection::Modes>(modeInt);

  MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
          ("%s: mode: %d.", __FUNCTION__, mode));
  if (sInitFailed) {
    return NS_ERROR_FAILURE;
  }

  nsresult result = NS_OK;

  if (!aIsStartup) {
    // Clear bounce tracker candidate map for any mode change so it's not leaked
    // into other modes. For example if we switch from dry-run mode into fully
    // enabled we want a clean slate to not purge trackers that we've classified
    // in dry-run mode. User activation data must be kept to avoid false
    // positives.
    MOZ_ASSERT(mStorage);
    result = mStorage->ClearByType(
        BounceTrackingProtectionStorage::EntryType::BounceTracker);
  }

  // On disable
  if (mode == nsIBounceTrackingProtection::MODE_DISABLED ||
      mode == nsIBounceTrackingProtection::MODE_ENABLED_STANDBY) {
    // No further cleanup needed if we're just starting up.
    if (aIsStartup) {
      MOZ_ASSERT(!mStorageObserver);
      MOZ_ASSERT(!mBounceTrackingPurgeTimer);
      return result;
    }

    // Destroy storage observer to stop receiving storage notifications.
    mStorageObserver = nullptr;

    // Stop regular purging.
    nsresult rv = UpdateBounceTrackingPurgeTimer(false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      result = rv;
      // Even if this step fails try to do more cleanup.
    }

    // Clear all per-tab state.
    BounceTrackingState::DestroyAll();
    return result;
  }

  // On enable
  MOZ_ASSERT(mode == nsIBounceTrackingProtection::MODE_ENABLED ||
             mode == nsIBounceTrackingProtection::MODE_ENABLED_DRY_RUN);

  // Create and init storage observer.
  mStorageObserver = new BounceTrackingStorageObserver();
  nsresult rv = mStorageObserver->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  // Schedule regular purging.
  rv = UpdateBounceTrackingPurgeTimer(true);
  NS_ENSURE_SUCCESS(rv, rv);

  return result;
}

nsresult BounceTrackingProtection::RecordStatefulBounces(
    BounceTrackingState* aBounceTrackingState) {
  NS_ENSURE_ARG_POINTER(aBounceTrackingState);

  MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
          ("%s: aBounceTrackingState: %s", __FUNCTION__,
           aBounceTrackingState->Describe().get()));

  // Assert: navigable’s bounce tracking record is not null.
  const Maybe<BounceTrackingRecord>& record =
      aBounceTrackingState->GetBounceTrackingRecord();
  NS_ENSURE_TRUE(record, NS_ERROR_FAILURE);

  // Get the bounce tracker map and the user activation map.
  RefPtr<BounceTrackingStateGlobal> globalState =
      mStorage->GetOrCreateStateGlobal(aBounceTrackingState);
  MOZ_ASSERT(globalState);

  nsTArray<nsCString> classifiedHosts;

  // For each host in navigable’s bounce tracking record's bounce set:
  for (const nsACString& host : record->GetBounceHosts()) {
    // Skip "null" entries, they are only used for logging purposes.
    if (host.EqualsLiteral("null")) {
      continue;
    }

    // If host equals navigable’s bounce tracking record's initial host,
    // continue.
    if (host == record->GetInitialHost()) {
      MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
              ("%s: Skip host == initialHost: %s", __FUNCTION__,
               PromiseFlatCString(host).get()));
      continue;
    }
    // If host equals navigable’s bounce tracking record's final host, continue.
    if (host == record->GetFinalHost()) {
      MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
              ("%s: Skip host == finalHost: %s", __FUNCTION__,
               PromiseFlatCString(host).get()));
      continue;
    }

    // If user activation map contains host, continue.
    if (globalState->HasUserActivation(host)) {
      MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
              ("%s: Skip host with recent user activation: %s", __FUNCTION__,
               PromiseFlatCString(host).get()));
      continue;
    }

    // If stateful bounce tracking map contains host, continue.
    if (globalState->HasBounceTracker(host)) {
      MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
              ("%s: Skip already existing host: %s", __FUNCTION__,
               PromiseFlatCString(host).get()));
      continue;
    }

    // If navigable’s bounce tracking record's storage access set does not
    // contain host, continue.
    if (StaticPrefs::
            privacy_bounceTrackingProtection_requireStatefulBounces() &&
        !record->GetStorageAccessHosts().Contains(host)) {
      MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
              ("%s: Skip host without storage access: %s", __FUNCTION__,
               PromiseFlatCString(host).get()));
      continue;
    }

    // Set stateful bounce tracking map[host] to topDocument’s relevant settings
    // object's current wall time.
    PRTime now = PR_Now();
    MOZ_ASSERT(!globalState->HasBounceTracker(host));
    nsresult rv = globalState->RecordBounceTracker(host, now);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }

    classifiedHosts.AppendElement(host);

    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Info,
            ("%s: Added bounce tracker candidate. siteHost: %s, "
             "aBounceTrackingState: %s",
             __FUNCTION__, PromiseFlatCString(host).get(),
             aBounceTrackingState->Describe().get()));
  }

  // Set navigable’s bounce tracking record to null.
  aBounceTrackingState->ResetBounceTrackingRecord();
  MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
          ("%s: Done, reset aBounceTrackingState: %s", __FUNCTION__,
           aBounceTrackingState->Describe().get()));

  // Log a message to the web console for each classified host.
  nsresult rv = LogBounceTrackersClassifiedToWebConsole(aBounceTrackingState,
                                                        classifiedHosts);
  NS_ENSURE_SUCCESS(rv, rv);

  // If running in test automation, dispatch an observer message indicating
  // we're finished recording bounces.
  if (StaticPrefs::privacy_bounceTrackingProtection_enableTestMode()) {
    nsCOMPtr<nsIObserverService> obsSvc =
        mozilla::services::GetObserverService();
    NS_ENSURE_TRUE(obsSvc, NS_ERROR_FAILURE);

    RefPtr<nsHashPropertyBag> props = new nsHashPropertyBag();

    nsresult rv = props->SetPropertyAsUint64(
        u"browserId"_ns, aBounceTrackingState->GetBrowserId());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = obsSvc->NotifyObservers(
        ToSupports(props), TEST_OBSERVER_MSG_RECORD_BOUNCES_FINISHED, nullptr);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult BounceTrackingProtection::RecordUserActivation(
    nsIPrincipal* aPrincipal, Maybe<PRTime> aActivationTime) {
  MOZ_ASSERT(XRE_IsParentProcess());
  NS_ENSURE_ARG_POINTER(aPrincipal);

  RefPtr<BounceTrackingProtection> btp = GetSingleton();
  // May be nullptr if feature is disabled.
  if (!btp) {
    return NS_OK;
  }

  if (!BounceTrackingState::ShouldTrackPrincipal(aPrincipal)) {
    return NS_OK;
  }

  nsAutoCString siteHost;
  nsresult rv = aPrincipal->GetBaseDomain(siteHost);
  NS_ENSURE_SUCCESS(rv, rv);

  MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
          ("%s: siteHost: %s", __FUNCTION__, siteHost.get()));

  RefPtr<BounceTrackingStateGlobal> globalState =
      btp->mStorage->GetOrCreateStateGlobal(aPrincipal);
  MOZ_ASSERT(globalState);

  // aActivationTime defaults to current time if no value is provided.
  return globalState->RecordUserActivation(siteHost,
                                           aActivationTime.valueOr(PR_Now()));
}

nsresult BounceTrackingProtection::RecordUserActivation(
    dom::WindowContext* aWindowContext) {
  NS_ENSURE_ARG_POINTER(aWindowContext);

  if (XRE_IsContentProcess()) {
    dom::WindowGlobalChild* wgc = aWindowContext->GetWindowGlobalChild();
    NS_ENSURE_TRUE(wgc, NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(wgc->SendRecordUserActivationForBTP(), NS_ERROR_FAILURE);
    return NS_OK;
  }
  MOZ_ASSERT(XRE_IsParentProcess());

  dom::WindowGlobalParent* wgp = aWindowContext->Canonical();
  MOZ_ASSERT(wgp);

  NS_ENSURE_TRUE(wgp->RecvRecordUserActivationForBTP(), NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
BounceTrackingProtection::Observe(nsISupports* aSubject, const char* aTopic,
                                  const char16_t* aData) {
  MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
          ("%s: aTopic: %s", __FUNCTION__, aTopic));

  if (!strcmp(aTopic, "idle-daily")) {
    // Submit custom telemetry ping.
    glean_pings::BounceTrackingProtection.Submit();
  }
  return NS_OK;
}

NS_IMETHODIMP
BounceTrackingProtection::TestGetBounceTrackerCandidateHosts(
    JS::Handle<JS::Value> aOriginAttributes, JSContext* aCx,
    nsTArray<RefPtr<nsIBounceTrackingMapEntry>>& aCandidates) {
  MOZ_ASSERT(aCx);

  OriginAttributes oa;
  if (!aOriginAttributes.isObject() || !oa.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }

  BounceTrackingStateGlobal* globalState = mStorage->GetOrCreateStateGlobal(oa);
  MOZ_ASSERT(globalState);

  for (auto iter = globalState->BounceTrackersMapRef().ConstIter();
       !iter.Done(); iter.Next()) {
    RefPtr<nsIBounceTrackingMapEntry> candidate =
        new BounceTrackingMapEntry(iter.Key(), iter.Data());
    aCandidates.AppendElement(candidate);
  }

  return NS_OK;
}

NS_IMETHODIMP
BounceTrackingProtection::TestGetUserActivationHosts(
    JS::Handle<JS::Value> aOriginAttributes, JSContext* aCx,
    nsTArray<RefPtr<nsIBounceTrackingMapEntry>>& aHosts) {
  MOZ_ASSERT(aCx);

  OriginAttributes oa;
  if (!aOriginAttributes.isObject() || !oa.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }

  BounceTrackingStateGlobal* globalState = mStorage->GetOrCreateStateGlobal(oa);
  MOZ_ASSERT(globalState);

  for (auto iter = globalState->UserActivationMapRef().ConstIter();
       !iter.Done(); iter.Next()) {
    RefPtr<nsIBounceTrackingMapEntry> candidate =
        new BounceTrackingMapEntry(iter.Key(), iter.Data());
    aHosts.AppendElement(candidate);
  }

  return NS_OK;
}

NS_IMETHODIMP
BounceTrackingProtection::ClearAll() {
  BounceTrackingState::ResetAll();
  return mStorage->Clear();
}

NS_IMETHODIMP
BounceTrackingProtection::ClearBySiteHostAndOriginAttributes(
    const nsACString& aSiteHost, JS::Handle<JS::Value> aOriginAttributes,
    JSContext* aCx) {
  NS_ENSURE_ARG_POINTER(aCx);

  OriginAttributes originAttributes;
  if (!aOriginAttributes.isObject() ||
      !originAttributes.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }

  // Reset per tab state for tabs matching the given OriginAttributes.
  BounceTrackingState::ResetAllForOriginAttributes(originAttributes);

  return mStorage->ClearBySiteHost(aSiteHost, &originAttributes);
}

NS_IMETHODIMP
BounceTrackingProtection::ClearBySiteHostAndOriginAttributesPattern(
    const nsACString& aSiteHost, JS::Handle<JS::Value> aOriginAttributesPattern,
    JSContext* aCx) {
  NS_ENSURE_ARG_POINTER(aCx);

  OriginAttributesPattern pattern;
  if (!aOriginAttributesPattern.isObject() ||
      !pattern.Init(aCx, aOriginAttributesPattern)) {
    return NS_ERROR_INVALID_ARG;
  }

  // Clear per-tab state.
  BounceTrackingState::ResetAllForOriginAttributesPattern(pattern);

  // Clear global state including on-disk state.
  return mStorage->ClearByOriginAttributesPattern(pattern,
                                                  Some(nsCString(aSiteHost)));
}

NS_IMETHODIMP
BounceTrackingProtection::ClearByTimeRange(PRTime aFrom, PRTime aTo) {
  NS_ENSURE_TRUE(aFrom >= 0, NS_ERROR_INVALID_ARG);
  NS_ENSURE_TRUE(aFrom < aTo, NS_ERROR_INVALID_ARG);

  // Clear all BounceTrackingState, we don't keep track of time ranges.
  BounceTrackingState::ResetAll();

  return mStorage->ClearByTimeRange(aFrom, aTo);
}

NS_IMETHODIMP
BounceTrackingProtection::ClearByOriginAttributesPattern(
    const nsAString& aPattern) {
  OriginAttributesPattern pattern;
  if (!pattern.Init(aPattern)) {
    return NS_ERROR_INVALID_ARG;
  }

  // Reset all per-tab state matching the given OriginAttributesPattern.
  BounceTrackingState::ResetAllForOriginAttributesPattern(pattern);

  return mStorage->ClearByOriginAttributesPattern(pattern);
}

NS_IMETHODIMP
BounceTrackingProtection::AddSiteHostExceptions(
    const nsTArray<nsCString>& aSiteHosts) {
  for (const auto& host : aSiteHosts) {
    mRemoteSiteHostExceptions.Insert(host);
  }

  return NS_OK;
}

NS_IMETHODIMP
BounceTrackingProtection::RemoveSiteHostExceptions(
    const nsTArray<nsCString>& aSiteHosts) {
  for (const auto& host : aSiteHosts) {
    mRemoteSiteHostExceptions.Remove(host);
  }

  return NS_OK;
}

NS_IMETHODIMP
BounceTrackingProtection::TestGetSiteHostExceptions(
    nsTArray<nsCString>& aSiteHostExceptions) {
  aSiteHostExceptions.Clear();

  for (const auto& host : mRemoteSiteHostExceptions) {
    aSiteHostExceptions.AppendElement(host);
  }

  return NS_OK;
}

NS_IMETHODIMP
BounceTrackingProtection::TestRunPurgeBounceTrackers(
    JSContext* aCx, mozilla::dom::Promise** aPromise) {
  NS_ENSURE_ARG_POINTER(aCx);
  NS_ENSURE_ARG_POINTER(aPromise);

  nsIGlobalObject* globalObject = xpc::CurrentNativeGlobal(aCx);
  if (!globalObject) {
    return NS_ERROR_UNEXPECTED;
  }

  ErrorResult result;
  RefPtr<dom::Promise> promise = dom::Promise::Create(globalObject, result);
  if (result.Failed()) {
    return result.StealNSResult();
  }

  // PurgeBounceTrackers returns a MozPromise, wrap it in a dom::Promise
  // required for XPCOM.
  PurgeBounceTrackers()->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [promise](const PurgeBounceTrackersMozPromise::ResolveValueType&
                    purgedSiteHosts) {
        promise->MaybeResolve(purgedSiteHosts);
      },
      [promise](const PurgeBounceTrackersMozPromise::RejectValueType& error) {
        promise->MaybeReject(error);
      });

  promise.forget(aPromise);
  return NS_OK;
}

NS_IMETHODIMP
BounceTrackingProtection::TestClearExpiredUserActivations() {
  return ClearExpiredUserInteractions();
}

NS_IMETHODIMP
BounceTrackingProtection::TestAddBounceTrackerCandidate(
    JS::Handle<JS::Value> aOriginAttributes, const nsACString& aHost,
    const PRTime aBounceTime, JSContext* aCx) {
  MOZ_ASSERT(aCx);

  OriginAttributes oa;
  if (!aOriginAttributes.isObject() || !oa.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }

  BounceTrackingStateGlobal* stateGlobal = mStorage->GetOrCreateStateGlobal(oa);
  MOZ_ASSERT(stateGlobal);

  // Ensure aHost is lowercase to match nsIURI and nsIPrincipal.
  nsAutoCString host(aHost);
  ToLowerCase(host);

  // Can not have a host in both maps.
  nsresult rv = stateGlobal->TestRemoveUserActivation(host);
  NS_ENSURE_SUCCESS(rv, rv);
  return stateGlobal->RecordBounceTracker(host, aBounceTime);
}

NS_IMETHODIMP
BounceTrackingProtection::TestAddUserActivation(
    JS::Handle<JS::Value> aOriginAttributes, const nsACString& aHost,
    const PRTime aActivationTime, JSContext* aCx) {
  MOZ_ASSERT(aCx);

  OriginAttributes oa;
  if (!aOriginAttributes.isObject() || !oa.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }

  BounceTrackingStateGlobal* stateGlobal = mStorage->GetOrCreateStateGlobal(oa);
  MOZ_ASSERT(stateGlobal);

  // Ensure aHost is lowercase to match nsIURI and nsIPrincipal.
  nsAutoCString host(aHost);
  ToLowerCase(host);

  return stateGlobal->RecordUserActivation(host, aActivationTime);
}

NS_IMETHODIMP
BounceTrackingProtection::TestMaybeMigrateUserInteractionPermissions() {
  return MaybeMigrateUserInteractionPermissions();
}

// static
nsresult BounceTrackingProtection::LogBounceTrackersClassifiedToWebConsole(
    BounceTrackingState* aBounceTrackingState,
    const nsTArray<nsCString>& aSiteHosts) {
  NS_ENSURE_ARG(aBounceTrackingState);

  // Nothing to log.
  if (aSiteHosts.IsEmpty()) {
    return NS_OK;
  }

  RefPtr<dom::BrowsingContext> browsingContext =
      aBounceTrackingState->CurrentBrowsingContext();
  if (!browsingContext) {
    return NS_OK;
  }

  // Get the localized copy from antiTracking.ftl and insert the variables.
  nsTArray<nsCString> resourceIDs = {"toolkit/global/antiTracking.ftl"_ns};
  RefPtr<intl::Localization> l10n =
      intl::Localization::Create(resourceIDs, true);

  for (const nsACString& siteHost : aSiteHosts) {
    auto l10nArgs = dom::Optional<intl::L10nArgs>();
    l10nArgs.Construct();

    auto siteHostArg = l10nArgs.Value().Entries().AppendElement();
    siteHostArg->mKey = "siteHost";
    siteHostArg->mValue.SetValue().SetAsUTF8String().Assign(siteHost);

    auto gracePeriodArg = l10nArgs.Value().Entries().AppendElement();
    gracePeriodArg->mKey = "gracePeriodSeconds";
    gracePeriodArg->mValue.SetValue().SetAsDouble() = StaticPrefs::
        privacy_bounceTrackingProtection_bounceTrackingGracePeriodSec();

    // Construct the localized string.
    nsAutoCString message;
    ErrorResult errorResult;
    l10n->FormatValueSync("btp-warning-tracker-classified"_ns, l10nArgs,
                          message, errorResult);
    if (NS_WARN_IF(errorResult.Failed())) {
      return errorResult.StealNSResult();
    }

    // Log to the console via nsIScriptError object.
    nsresult rv = NS_OK;
    nsCOMPtr<nsIScriptError> error =
        do_CreateInstance(NS_SCRIPTERROR_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = error->InitWithWindowID(
        NS_ConvertUTF8toUTF16(message), ""_ns, 0, 0,
        nsIScriptError::warningFlag, "bounceTrackingProtection",
        browsingContext->GetCurrentInnerWindowId(), true);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIConsoleService> consoleService =
        do_GetService(NS_CONSOLESERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // The actual log call.
    rv = consoleService->LogMessage(error);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

RefPtr<GenericNonExclusivePromise>
BounceTrackingProtection::EnsureRemoteExceptionListService() {
  // mRemoteExceptionList already initialized or currently initializing.
  if (mRemoteExceptionListInitPromise) {
    return mRemoteExceptionListInitPromise;
  }

  // Create the service instance.
  nsresult rv;
  mRemoteExceptionList =
      do_GetService(NS_NSIBTPEXCEPTIONLISTSERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mRemoteExceptionListInitPromise =
        GenericNonExclusivePromise::CreateAndReject(rv, __func__);
    return mRemoteExceptionListInitPromise;
  }

  // Call the init method and get the Promise. It resolves once the allow-list
  // entries have been imported.
  RefPtr<dom::Promise> jsPromise;
  rv = mRemoteExceptionList->Init(this, getter_AddRefs(jsPromise));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mRemoteExceptionListInitPromise =
        GenericNonExclusivePromise::CreateAndReject(rv, __func__);
    return mRemoteExceptionListInitPromise;
  }
  MOZ_ASSERT(jsPromise);

  // Convert to MozPromise so it can be handled from C++ side. Also store the
  // promise so that subsequent calls to this method can wait for init too.
  mRemoteExceptionListInitPromise =
      PromiseNativeWrapper::ConvertJSPromiseToMozPromise(jsPromise);

  return mRemoteExceptionListInitPromise;
}

RefPtr<BounceTrackingProtection::PurgeBounceTrackersMozPromise>
BounceTrackingProtection::PurgeBounceTrackers() {
  // Only purge when the feature is actually enabled.
  if (StaticPrefs::privacy_bounceTrackingProtection_mode() !=
          nsIBounceTrackingProtection::MODE_ENABLED &&
      StaticPrefs::privacy_bounceTrackingProtection_mode() !=
          nsIBounceTrackingProtection::MODE_ENABLED_DRY_RUN) {
    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
            ("%s: Skip: Purging disabled via mode pref.", __FUNCTION__));
    return PurgeBounceTrackersMozPromise::CreateAndReject(
        nsresult::NS_ERROR_NOT_AVAILABLE, __func__);
  }

  // Prevent multiple purge operations from running at the same time.
  if (mPurgeInProgress) {
    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
            ("%s: Skip: Purge already in progress.", __FUNCTION__));
    return PurgeBounceTrackersMozPromise::CreateAndReject(
        nsresult::NS_ERROR_NOT_AVAILABLE, __func__);
  }
  mPurgeInProgress = true;

  RefPtr<PurgeBounceTrackersMozPromise::Private> resultPromise =
      new PurgeBounceTrackersMozPromise::Private(__func__);

  RefPtr<BounceTrackingProtection> self = this;

  // Wait for the remote exception list service to be ready before purging.
  EnsureRemoteExceptionListService()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [self, resultPromise](
          const GenericNonExclusivePromise::ResolveOrRejectValue& aResult) {
        if (aResult.IsReject()) {
          nsresult rv = aResult.RejectValue();
          resultPromise->Reject(rv, __func__);
          return;
        }
        // Remote exception list is ready.

        // Obtain a cache of allow-list permissions so we only need to
        // fetch permissions once even when we do multiple base domain lookups.
        BounceTrackingAllowList bounceTrackingAllowList;

        // Collect promises for all clearing operations to later await on.
        nsTArray<RefPtr<ClearDataMozPromise>> clearPromises;

        // Run the purging algorithm for all global state objects.
        for (const auto& entry : self->mStorage->StateGlobalMapRef()) {
          const OriginAttributes& originAttributes = entry.GetKey();
          BounceTrackingStateGlobal* stateGlobal = entry.GetData();
          MOZ_ASSERT(stateGlobal);

          if (MOZ_LOG_TEST(gBounceTrackingProtectionLog, LogLevel::Debug)) {
            nsAutoCString oaSuffix;
            originAttributes.CreateSuffix(oaSuffix);
            MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
                    ("%s: Running purge algorithm for OA: '%s'", __FUNCTION__,
                     oaSuffix.get()));
          }

          nsresult rv = self->PurgeBounceTrackersForStateGlobal(
              stateGlobal, bounceTrackingAllowList, clearPromises);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            resultPromise->Reject(rv, __func__);
            return;
          }
        }

        // Wait for all data clearing operations to complete. mClearPromises
        // contains one promise per host / clear task.
        ClearDataMozPromise::AllSettled(GetCurrentSerialEventTarget(),
                                        clearPromises)
            ->Then(
                GetCurrentSerialEventTarget(), __func__,
                [resultPromise,
                 self](ClearDataMozPromise::AllSettledPromiseType::
                           ResolveOrRejectValue&& aResults) {
                  MOZ_ASSERT(aResults.IsResolve(), "AllSettled never rejects");

                  MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
                          ("%s: Done. Cleared %zu hosts.", __FUNCTION__,
                           aResults.ResolveValue().Length()));

                  if (!aResults.ResolveValue().IsEmpty()) {
                    glean::bounce_tracking_protection::num_hosts_per_purge_run
                        .AccumulateSingleSample(
                            aResults.ResolveValue().Length());
                  }

                  // Check if any clear call failed.
                  bool anyFailed = false;

                  nsTArray<nsCString> purgedSiteHosts;

                  // If any clear call failed reject.
                  for (auto& result : aResults.ResolveValue()) {
                    if (result.IsReject()) {
                      anyFailed = true;
                    } else {
                      purgedSiteHosts.AppendElement(result.ResolveValue());
                    }
                  }

                  // Record successful purges via nsITrackingDBService for
                  // tracker stats.
                  if (purgedSiteHosts.Length() > 0) {
                    ReportPurgedTrackersToAntiTrackingDB(purgedSiteHosts);
                  }

                  self->mPurgeInProgress = false;

                  // If any clear call failed reject the promise.
                  if (anyFailed) {
                    resultPromise->Reject(NS_ERROR_FAILURE, __func__);
                    return;
                  }
                  resultPromise->Resolve(std::move(purgedSiteHosts), __func__);
                });
      });
  return resultPromise.forget();
}

// static
void BounceTrackingProtection::ReportPurgedTrackersToAntiTrackingDB(
    const nsTArray<nsCString>& aPurgedSiteHosts) {
  MOZ_ASSERT(!aPurgedSiteHosts.IsEmpty());

  ContentBlockingLog log;
  for (const nsCString& host : aPurgedSiteHosts) {
    nsAutoCString origin("https://");
    origin.Append(host);

    log.RecordLogParent(
        origin, nsIWebProgressListener::STATE_PURGED_BOUNCETRACKER, true);
  }
  log.ReportLog();
}

nsresult BounceTrackingProtection::PurgeBounceTrackersForStateGlobal(
    BounceTrackingStateGlobal* aStateGlobal,
    BounceTrackingAllowList& aBounceTrackingAllowList,
    nsTArray<RefPtr<ClearDataMozPromise>>& aClearPromises) {
  MOZ_ASSERT(aStateGlobal);
  MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
          ("%s: %s", __FUNCTION__, aStateGlobal->Describe().get()));

  // Ensure we only purge when pref configuration allows it.
  if (StaticPrefs::privacy_bounceTrackingProtection_mode() !=
          nsIBounceTrackingProtection::MODE_ENABLED &&
      StaticPrefs::privacy_bounceTrackingProtection_mode() !=
          nsIBounceTrackingProtection::MODE_ENABLED_DRY_RUN) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  const PRTime now = PR_Now();

  // 1. Remove hosts from the user activation map whose user activation flag has
  // expired.
  nsresult rv = ClearExpiredUserInteractions(aStateGlobal);
  NS_ENSURE_SUCCESS(rv, rv);

  // 2. Go over bounce tracker candidate map and purge state.
  rv = NS_OK;
  nsCOMPtr<nsIClearDataService> clearDataService =
      do_GetService("@mozilla.org/clear-data-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Collect hosts to remove from the bounce trackers map. We can not remove
  // them while iterating over the map.
  nsTArray<nsCString> bounceTrackerCandidatesToRemove;

  for (auto hostIter = aStateGlobal->BounceTrackersMapRef().ConstIter();
       !hostIter.Done(); hostIter.Next()) {
    const nsACString& host = hostIter.Key();
    const PRTime& bounceTime = hostIter.Data();

    // If bounceTime + bounce tracking grace period is after now, then continue.
    // The host is still within the grace period and must not be purged.
    if (bounceTime +
            StaticPrefs::
                    privacy_bounceTrackingProtection_bounceTrackingGracePeriodSec() *
                PR_USEC_PER_SEC >
        now) {
      MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
              ("%s: Skip host within bounce tracking grace period %s",
               __FUNCTION__, PromiseFlatCString(host).get()));

      continue;
    }

    // If there is a top-level traversable whose active document's origin's
    // site's host equals host, then continue.
    // TODO: Bug 1842047: Implement a more accurate check that calls into the
    // browser implementations to determine whether the site is currently open
    // on the top level.
    bool hostIsActive;
    rv = BounceTrackingState::HasBounceTrackingStateForSite(
        host, aStateGlobal->OriginAttributesRef(), hostIsActive);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      hostIsActive = false;
    }
    if (hostIsActive) {
      MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
              ("%s: Skip host which is active %s", __FUNCTION__,
               PromiseFlatCString(host).get()));
      continue;
    }

    // Gecko specific: If the host is on the content blocking allow-list or
    // allow-listed via RemoteSettings continue.
    bool isAllowListed = mRemoteSiteHostExceptions.Contains(host);
    // If remote settings doesn't allowlist also check the content blocking
    // allow-list.
    if (!isAllowListed) {
      rv = aBounceTrackingAllowList.CheckForBaseDomain(
          host, aStateGlobal->OriginAttributesRef(), isAllowListed);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        continue;
      }
    }
    if (isAllowListed) {
      if (MOZ_LOG_TEST(gBounceTrackingProtectionLog, LogLevel::Debug)) {
        nsAutoCString originAttributeSuffix;
        aStateGlobal->OriginAttributesRef().CreateSuffix(originAttributeSuffix);
        MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
                ("%s: Skip allow-listed: host: %s, "
                 "originAttributes: %s",
                 __FUNCTION__, PromiseFlatCString(host).get(),
                 originAttributeSuffix.get()));
      }
      // Remove allow-listed host so we don't need to check in again next purge
      // run. If it gets classified again and the allow-list entry gets removed
      // it will be purged in the next run.
      bounceTrackerCandidatesToRemove.AppendElement(host);
      continue;
    }

    // No exception above applies, clear state for the given host.
    RefPtr<ClearDataMozPromise::Private> clearPromise =
        new ClearDataMozPromise::Private(__func__);
    RefPtr<ClearDataCallback> cb =
        new ClearDataCallback(clearPromise, host, bounceTime);

    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Info,
            ("%s: Purging bounce tracker. siteHost: %s, bounceTime: %" PRIu64
             " aStateGlobal: %s",
             __FUNCTION__, PromiseFlatCString(host).get(), bounceTime,
             aStateGlobal->Describe().get()));

    if (StaticPrefs::privacy_bounceTrackingProtection_mode() ==
        nsIBounceTrackingProtection::MODE_ENABLED_DRY_RUN) {
      // In dry-run mode, we don't actually clear the data, but we still want to
      // resolve the promise to indicate that the data would have been cleared.
      cb->OnDataDeleted(0);
    } else {
      // TODO: Bug 1842067: Clear by site + OA.

      // nsIClearDataService expects a schemeless site which for IPV6 addresses
      // includes brackets. Add them if needed.
      nsAutoCString hostToPurge(host);
      nsContentUtils::MaybeFixIPv6Host(hostToPurge);

      rv = clearDataService->DeleteDataFromSiteAndOriginAttributesPatternString(
          hostToPurge, u""_ns, false, TRACKER_PURGE_FLAGS, cb);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        clearPromise->Reject(0, __func__);
      }
    }

    aClearPromises.AppendElement(clearPromise);

    // Remove it from the bounce trackers map, it's about to be purged. If the
    // clear call fails still remove it. We want to avoid an ever growing list
    // of hosts in case of repeated failures.
    bounceTrackerCandidatesToRemove.AppendElement(host);
  }

  // Remove hosts from the bounce trackers map which we executed purge calls
  // for.
  return aStateGlobal->RemoveBounceTrackers(bounceTrackerCandidatesToRemove);
}

nsresult BounceTrackingProtection::ClearExpiredUserInteractions(
    BounceTrackingStateGlobal* aStateGlobal) {
  if (!aStateGlobal && mStorage->StateGlobalMapRef().IsEmpty()) {
    // Nothing to clear.
    return NS_OK;
  }

  const PRTime now = PR_Now();

  // Convert the user activation lifetime into microseconds for calculation with
  // PRTime values. The pref is a 32-bit value. Cast into 64-bit before
  // multiplying so we get the correct result.
  int64_t activationLifetimeUsec =
      static_cast<int64_t>(
          StaticPrefs::
              privacy_bounceTrackingProtection_bounceTrackingActivationLifetimeSec()) *
      PR_USEC_PER_SEC;

  // Clear user activation for the given state global.
  if (aStateGlobal) {
    return aStateGlobal->ClearUserActivationBefore(now -
                                                   activationLifetimeUsec);
  }

  // aStateGlobal not passed, clear user activation for all state globals.
  for (const auto& entry : mStorage->StateGlobalMapRef()) {
    const RefPtr<BounceTrackingStateGlobal>& stateGlobal = entry.GetData();
    MOZ_ASSERT(stateGlobal);

    nsresult rv =
        stateGlobal->ClearUserActivationBefore(now - activationLifetimeUsec);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult BounceTrackingProtection::MaybeMigrateUserInteractionPermissions() {
  // Only run the migration once.
  if (StaticPrefs::
          privacy_bounceTrackingProtection_hasMigratedUserActivationData()) {
    return NS_OK;
  }

  MOZ_LOG(
      gBounceTrackingProtectionLog, LogLevel::Debug,
      ("%s: Importing user activation data from permissions", __FUNCTION__));

  // Get all user activation permissions that are within our user activation
  // lifetime. We don't care about the rest since they are considered expired
  // for BTP.

  nsresult rv = NS_OK;
  nsCOMPtr<nsIPermissionManager> permManager =
      do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(permManager, NS_ERROR_FAILURE);

  // Construct the since time param. The permission manager expects epoch in
  // miliseconds.
  int64_t nowMS = PR_Now() / PR_USEC_PER_MSEC;
  int64_t activationLifetimeMS =
      static_cast<int64_t>(
          StaticPrefs::
              privacy_bounceTrackingProtection_bounceTrackingActivationLifetimeSec()) *
      PR_MSEC_PER_SEC;
  int64_t since = nowMS - activationLifetimeMS;
  MOZ_ASSERT(since > 0);

  // Get all user activation permissions last modified between "since" and now.
  nsTArray<RefPtr<nsIPermission>> userActivationPermissions;
  rv = permManager->GetAllByTypeSince("storageAccessAPI"_ns, since,
                                      userActivationPermissions);
  NS_ENSURE_SUCCESS(rv, rv);

  MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
          ("%s: Found %zu (non-expired) user activation permissions",
           __FUNCTION__, userActivationPermissions.Length()));

  for (const auto& perm : userActivationPermissions) {
    nsCOMPtr<nsIPrincipal> permPrincipal;

    rv = perm->GetPrincipal(getter_AddRefs(permPrincipal));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }
    MOZ_ASSERT(permPrincipal);

    // The time the permission was last modified is the time of last user
    // activation.
    int64_t modificationTimeMS;
    rv = perm->GetModificationTime(&modificationTimeMS);
    NS_ENSURE_SUCCESS(rv, rv);
    MOZ_ASSERT(modificationTimeMS >= since,
               "Unexpected permission modification time");

    // We may end up with duplicates here since user activation permissions are
    // tracked by origin, while BTP tracks user activation by site host.
    // RecordUserActivation is responsible for only keeping the most recent user
    // activation flag for a given site host and needs to make sure existing
    // activation flags are not overwritten by older timestamps.
    // RecordUserActivation expects epoch in microseconds.
    rv = RecordUserActivation(permPrincipal,
                              Some(modificationTimeMS * PR_USEC_PER_MSEC));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }
  }

  // Migration successful, set the pref to indicate that we have migrated.
  return mozilla::Preferences::SetBool(
      "privacy.bounceTrackingProtection.hasMigratedUserActivationData", true);
}

}  // namespace mozilla
