/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCookieBannerService.h"

#include "CookieBannerDomainPrefService.h"
#include "ErrorList.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/EventQueue.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/glean/GleanMetrics.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPrefs_cookiebanners.h"
#include "nsCOMPtr.h"
#include "nsCookieBannerRule.h"
#include "nsCookieInjector.h"
#include "nsCRT.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIClickRule.h"
#include "nsICookieBannerListService.h"
#include "nsICookieBannerRule.h"
#include "nsICookie.h"
#include "nsIEffectiveTLDService.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsStringFwd.h"
#include "nsThreadUtils.h"
#include "Cookie.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(nsCookieBannerService, nsICookieBannerService, nsIObserver)

LazyLogModule gCookieBannerLog("nsCookieBannerService");

static const char kCookieBannerServiceModePref[] = "cookiebanners.service.mode";
static const char kCookieBannerServiceModePBMPref[] =
    "cookiebanners.service.mode.privateBrowsing";

static StaticRefPtr<nsCookieBannerService> sCookieBannerServiceSingleton;

namespace {

// A helper function that converts service modes to strings.
nsCString ConvertModeToStringForTelemetry(uint32_t aModes) {
  switch (aModes) {
    case nsICookieBannerService::MODE_DISABLED:
      return "disabled"_ns;
    case nsICookieBannerService::MODE_REJECT:
      return "reject"_ns;
    case nsICookieBannerService::MODE_REJECT_OR_ACCEPT:
      return "reject_or_accept"_ns;
    default:
      // Fall back to return "invalid" if we got any unsupported service
      // mode. Note this this also includes MODE_UNSET.
      return "invalid"_ns;
  }
}

}  // anonymous namespace

// static
already_AddRefed<nsCookieBannerService> nsCookieBannerService::GetSingleton() {
  if (!sCookieBannerServiceSingleton) {
    sCookieBannerServiceSingleton = new nsCookieBannerService();

    RunOnShutdown([] {
      MOZ_LOG(gCookieBannerLog, LogLevel::Debug,
              ("RunOnShutdown. Mode: %d. Mode PBM: %d.",
               StaticPrefs::cookiebanners_service_mode(),
               StaticPrefs::cookiebanners_service_mode_privateBrowsing()));

      // Unregister pref listeners.
      DebugOnly<nsresult> rv = Preferences::UnregisterCallback(
          &nsCookieBannerService::OnPrefChange, kCookieBannerServiceModePref);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "Unregistering kCookieBannerServiceModePref callback failed");
      rv = Preferences::UnregisterCallback(&nsCookieBannerService::OnPrefChange,
                                           kCookieBannerServiceModePBMPref);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "Unregistering kCookieBannerServiceModePBMPref callback failed");

      rv = sCookieBannerServiceSingleton->Shutdown();
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "nsCookieBannerService::Shutdown failed.");

      sCookieBannerServiceSingleton = nullptr;
    });
  }

  return do_AddRef(sCookieBannerServiceSingleton);
}

// static
void nsCookieBannerService::OnPrefChange(const char* aPref, void* aData) {
  RefPtr<nsCookieBannerService> service = GetSingleton();

  // If the feature is enabled for normal or private browsing, init the service.
  if (StaticPrefs::cookiebanners_service_mode() !=
          nsICookieBannerService::MODE_DISABLED ||
      StaticPrefs::cookiebanners_service_mode_privateBrowsing() !=
          nsICookieBannerService::MODE_DISABLED) {
    MOZ_LOG(
        gCookieBannerLog, LogLevel::Info,
        ("Initializing nsCookieBannerService after pref change. %s", aPref));
    DebugOnly<nsresult> rv = service->Init();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "nsCookieBannerService::Init failed");
    return;
  }

  MOZ_LOG(gCookieBannerLog, LogLevel::Info,
          ("Disabling nsCookieBannerService after pref change. %s", aPref));

  DebugOnly<nsresult> rv = service->Shutdown();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "nsCookieBannerService::Shutdown failed");
}

NS_IMETHODIMP
nsCookieBannerService::Observe(nsISupports* aSubject, const char* aTopic,
                               const char16_t* aData) {
  // Report the daily telemetry for the cookie banner service on "idle-daily".
  if (nsCRT::strcmp(aTopic, "idle-daily") == 0) {
    DailyReportTelemetry();
    return NS_OK;
  }

  // Initializing the cookie banner service on startup on
  // "profile-after-change".
  if (nsCRT::strcmp(aTopic, "profile-after-change") == 0) {
    nsresult rv = Preferences::RegisterCallback(
        &nsCookieBannerService::OnPrefChange, kCookieBannerServiceModePBMPref);
    NS_ENSURE_SUCCESS(rv, rv);

    return Preferences::RegisterCallbackAndCall(
        &nsCookieBannerService::OnPrefChange, kCookieBannerServiceModePref);
  }

  // Clear the executed data for private session when the last private browsing
  // session exits.
  if (nsCRT::strcmp(aTopic, "last-pb-context-exited") == 0) {
    return RemoveAllExecutedRecords(true);
  }

  return NS_OK;
}

nsresult nsCookieBannerService::Init() {
  MOZ_LOG(gCookieBannerLog, LogLevel::Debug,
          ("%s. Mode: %d. Mode PBM: %d.", __FUNCTION__,
           StaticPrefs::cookiebanners_service_mode(),
           StaticPrefs::cookiebanners_service_mode_privateBrowsing()));

  // Check if already initialized.
  if (mIsInitialized) {
    return NS_OK;
  }

  // Initialize the service which fetches cookie banner rules.
  mListService = do_GetService(NS_COOKIEBANNERLISTSERVICE_CONTRACTID);
  NS_ENSURE_TRUE(mListService, NS_ERROR_FAILURE);

  mDomainPrefService = CookieBannerDomainPrefService::GetOrCreate();
  NS_ENSURE_TRUE(mDomainPrefService, NS_ERROR_FAILURE);

  // Setting mIsInitialized before importing rules, because the list service
  // needs to call nsCookieBannerService methods that would throw if not
  // marked initialized.
  mIsInitialized = true;

  // Import initial rule-set, domain preference and enable rule syncing. Uses
  // NS_DispatchToCurrentThreadQueue with idle priority to avoid early
  // main-thread IO caused by the list service accessing RemoteSettings.
  nsresult rv = NS_DispatchToCurrentThreadQueue(
      NS_NewRunnableFunction("CookieBannerListService init startup",
                             [&] {
                               if (!mIsInitialized) {
                                 return;
                               }
                               nsresult rv = mListService->Init();
                               NS_ENSURE_SUCCESS_VOID(rv);
                               mDomainPrefService->Init();
                             }),
      EventQueuePriority::Idle);
  NS_ENSURE_SUCCESS(rv, rv);

  // Initialize the cookie injector.
  RefPtr<nsCookieInjector> injector = nsCookieInjector::GetSingleton();

  nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
  NS_ENSURE_TRUE(obsSvc, NS_ERROR_FAILURE);

  return obsSvc->AddObserver(this, "last-pb-context-exited", false);
}

nsresult nsCookieBannerService::Shutdown() {
  MOZ_LOG(gCookieBannerLog, LogLevel::Debug,
          ("%s. Mode: %d. Mode PBM: %d.", __FUNCTION__,
           StaticPrefs::cookiebanners_service_mode(),
           StaticPrefs::cookiebanners_service_mode_privateBrowsing()));

  // Check if already shutdown.
  if (!mIsInitialized) {
    return NS_OK;
  }

  // Shut down the list service which will stop updating mRules.
  nsresult rv = mListService->Shutdown();
  NS_ENSURE_SUCCESS(rv, rv);

  // Clear all stored cookie banner rules. They will be imported again on Init.
  mRules.Clear();

  // Clear executed records for normal and private browsing.
  rv = RemoveAllExecutedRecords(false);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = RemoveAllExecutedRecords(true);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
  NS_ENSURE_TRUE(obsSvc, NS_ERROR_FAILURE);

  rv = obsSvc->RemoveObserver(this, "last-pb-context-exited");
  NS_ENSURE_SUCCESS(rv, rv);

  mIsInitialized = false;

  return NS_OK;
}

NS_IMETHODIMP
nsCookieBannerService::GetIsEnabled(bool* aResult) {
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = mIsInitialized;

  return NS_OK;
}

NS_IMETHODIMP
nsCookieBannerService::GetRules(nsTArray<RefPtr<nsICookieBannerRule>>& aRules) {
  aRules.Clear();

  // Service is disabled, throw with empty array.
  if (!mIsInitialized) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Append global rules if enabled. We don't have to deduplicate here because
  // global rules are stored by ID and every ID maps to exactly one rule.
  if (StaticPrefs::cookiebanners_service_enableGlobalRules()) {
    AppendToArray(aRules, mGlobalRules.Values());
  }

  // Append domain-keyed rules.
  // Since multiple domains can map to the same rule in mRules we need to
  // deduplicate using a set before returning a rules array.
  nsTHashSet<nsRefPtrHashKey<nsICookieBannerRule>> rulesSet;

  for (const nsCOMPtr<nsICookieBannerRule>& rule : mRules.Values()) {
    rulesSet.Insert(rule);
  }

  AppendToArray(aRules, rulesSet);

  return NS_OK;
}

NS_IMETHODIMP
nsCookieBannerService::ResetRules(const bool doImport) {
  // Service is disabled, throw.
  if (!mIsInitialized) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mRules.Clear();
  mGlobalRules.Clear();

  if (doImport) {
    NS_ENSURE_TRUE(mListService, NS_ERROR_FAILURE);
    nsresult rv = mListService->ImportAllRules();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult nsCookieBannerService::GetRuleForDomain(const nsACString& aDomain,
                                                 nsICookieBannerRule** aRule) {
  NS_ENSURE_ARG_POINTER(aRule);
  *aRule = nullptr;

  // Service is disabled, throw with null.
  if (!mIsInitialized) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsICookieBannerRule> rule = mRules.Get(aDomain);
  if (rule) {
    rule.forget(aRule);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCookieBannerService::GetCookiesForURI(
    nsIURI* aURI, const bool aIsPrivateBrowsing,
    nsTArray<RefPtr<nsICookieRule>>& aCookies) {
  NS_ENSURE_ARG_POINTER(aURI);
  aCookies.Clear();

  // We only need URI spec for logging, avoid getting it otherwise.
  if (MOZ_LOG_TEST(gCookieBannerLog, LogLevel::Debug)) {
    nsAutoCString spec;
    nsresult rv = aURI->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);
    MOZ_LOG(gCookieBannerLog, LogLevel::Debug,
            ("%s. aURI: %s. aIsPrivateBrowsing: %d", __FUNCTION__, spec.get(),
             aIsPrivateBrowsing));
  }

  // Service is disabled, throw with empty array.
  if (!mIsInitialized) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Check which cookie banner service mode applies for this request. This
  // depends on whether the browser is in private browsing or normal browsing
  // mode.
  uint32_t mode;
  if (aIsPrivateBrowsing) {
    mode = StaticPrefs::cookiebanners_service_mode_privateBrowsing();
  } else {
    mode = StaticPrefs::cookiebanners_service_mode();
  }
  MOZ_LOG(
      gCookieBannerLog, LogLevel::Debug,
      ("%s. Found nsICookieBannerRule. Computed mode: %d", __FUNCTION__, mode));

  // We don't need to check the domain preference if the cookie banner handling
  // service is disabled by pref.
  if (mode != nsICookieBannerService::MODE_DISABLED &&
      !StaticPrefs::cookiebanners_service_detectOnly()) {
    // Get the domain preference for the uri, the domain preference takes
    // precedence over the pref setting. Note that the domain preference is
    // supposed to stored only for top level URIs.
    nsICookieBannerService::Modes domainPref;
    nsresult rv = GetDomainPref(aURI, aIsPrivateBrowsing, &domainPref);
    NS_ENSURE_SUCCESS(rv, rv);

    if (domainPref != nsICookieBannerService::MODE_UNSET) {
      mode = domainPref;
    }
  }

  // Service is disabled for current context (normal, private browsing or domain
  // preference), return empty array. Same for detect-only mode where no cookies
  // should be injected.
  if (mode == nsICookieBannerService::MODE_DISABLED ||
      StaticPrefs::cookiebanners_service_detectOnly()) {
    MOZ_LOG(gCookieBannerLog, LogLevel::Debug,
            ("%s. Returning empty array. Got MODE_DISABLED for "
             "aIsPrivateBrowsing: %d.",
             __FUNCTION__, aIsPrivateBrowsing));
    return NS_OK;
  }

  nsresult rv;
  // Compute the baseDomain from aURI.
  nsCOMPtr<nsIEffectiveTLDService> eTLDService(
      do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString baseDomain;
  rv = eTLDService->GetBaseDomain(aURI, 0, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  return GetCookieRulesForDomainInternal(
      baseDomain, static_cast<nsICookieBannerService::Modes>(mode), true,
      aCookies);
}

NS_IMETHODIMP
nsCookieBannerService::GetClickRulesForDomain(
    const nsACString& aDomain, const bool aIsTopLevel,
    nsTArray<RefPtr<nsIClickRule>>& aRules) {
  return GetClickRulesForDomainInternal(aDomain, aIsTopLevel, aRules);
}

nsresult nsCookieBannerService::GetClickRulesForDomainInternal(
    const nsACString& aDomain, const bool aIsTopLevel,
    nsTArray<RefPtr<nsIClickRule>>& aRules) {
  aRules.Clear();

  // Service is disabled, throw with empty rule.
  if (!mIsInitialized) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsICookieBannerRule> ruleForDomain;
  nsresult rv = GetRuleForDomain(aDomain, getter_AddRefs(ruleForDomain));
  NS_ENSURE_SUCCESS(rv, rv);

  bool useGlobalSubFrameRules =
      StaticPrefs::cookiebanners_service_enableGlobalRules_subFrames();

  // Extract click rule from an nsICookieBannerRule and if found append it to
  // the array returned.
  auto appendClickRule = [&](const nsCOMPtr<nsICookieBannerRule>& bannerRule,
                             bool isGlobal) {
    nsCOMPtr<nsIClickRule> clickRule;
    rv = bannerRule->GetClickRule(getter_AddRefs(clickRule));
    NS_ENSURE_SUCCESS(rv, rv);

    if (!clickRule) {
      return NS_OK;
    }

    // Evaluate the rule's runContext field and skip it if the caller's context
    // doesn't match. See nsIClickRule::RunContext for possible values.
    nsIClickRule::RunContext runContext;
    rv = clickRule->GetRunContext(&runContext);
    NS_ENSURE_SUCCESS(rv, rv);

    bool runContextMatchesRule =
        (runContext == nsIClickRule::RUN_ALL) ||
        (runContext == nsIClickRule::RUN_TOP && aIsTopLevel) ||
        (runContext == nsIClickRule::RUN_CHILD && !aIsTopLevel);

    if (!runContextMatchesRule) {
      return NS_OK;
    }

    // If global sub-frame rules are disabled skip adding them.
    if (!useGlobalSubFrameRules && isGlobal && !aIsTopLevel) {
      if (MOZ_LOG_TEST(gCookieBannerLog, LogLevel::Debug)) {
        nsAutoCString ruleId;
        rv = bannerRule->GetId(ruleId);
        NS_ENSURE_SUCCESS(rv, rv);

        MOZ_LOG(gCookieBannerLog, LogLevel::Debug,
                ("%s. Skip adding global sub-frame rule: %s.", __FUNCTION__,
                 ruleId.get()));
      }

      return NS_OK;
    }

    aRules.AppendElement(clickRule);
    return NS_OK;
  };

  // If there is a domain-specific rule it takes precedence over the global
  // rules.
  if (ruleForDomain) {
    return appendClickRule(ruleForDomain, false);
  }

  if (!StaticPrefs::cookiebanners_service_enableGlobalRules()) {
    // Global rules are disabled, skip adding them.
    return NS_OK;
  }

  // Append all global click rules.
  for (nsICookieBannerRule* globalRule : mGlobalRules.Values()) {
    rv = appendClickRule(globalRule, true);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCookieBannerService::InsertRule(nsICookieBannerRule* aRule) {
  NS_ENSURE_ARG_POINTER(aRule);

  // Service is disabled, throw.
  if (!mIsInitialized) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCookieBannerRule::LogRule(gCookieBannerLog, "InsertRule:", aRule,
                              LogLevel::Debug);

  nsTArray<nsCString> domains;
  nsresult rv = aRule->GetDomains(domains);
  NS_ENSURE_SUCCESS(rv, rv);

  // Global rules are stored in a separate map mGlobalRules.
  // They are identified by having an empty domains array.
  // They are keyed by the unique ID field.
  if (domains.IsEmpty()) {
    nsAutoCString id;
    rv = aRule->GetId(id);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(!id.IsEmpty(), NS_ERROR_FAILURE);

    // Global rules must not have cookies. We shouldn't set cookies for every
    // site without indication that they handle banners. Click rules are
    // different, because they have a "presence" indicator and only click if it
    // is reasonable to do so.
    rv = aRule->ClearCookies();
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsICookieBannerRule> result =
        mGlobalRules.InsertOrUpdate(id, aRule);
    NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);

    return NS_OK;
  }

  // Multiple domains can be mapped to the same rule.
  for (auto& domain : domains) {
    nsCOMPtr<nsICookieBannerRule> result = mRules.InsertOrUpdate(domain, aRule);
    NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCookieBannerService::RemoveRule(nsICookieBannerRule* aRule) {
  NS_ENSURE_ARG_POINTER(aRule);

  // Service is disabled, throw.
  if (!mIsInitialized) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCookieBannerRule::LogRule(gCookieBannerLog, "RemoveRule:", aRule,
                              LogLevel::Debug);

  nsTArray<nsCString> domains;
  nsresult rv = aRule->GetDomains(domains);
  NS_ENSURE_SUCCESS(rv, rv);

  // Remove global rule by ID.
  if (domains.IsEmpty()) {
    nsAutoCString id;
    rv = aRule->GetId(id);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(!id.IsEmpty(), NS_ERROR_FAILURE);

    mGlobalRules.Remove(id);
    return NS_OK;
  }

  // Remove all entries pointing to the rule.
  for (auto& domain : domains) {
    mRules.Remove(domain);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCookieBannerService::HasRuleForBrowsingContextTree(
    mozilla::dom::BrowsingContext* aBrowsingContext, bool* aResult) {
  NS_ENSURE_ARG_POINTER(aBrowsingContext);
  NS_ENSURE_ARG_POINTER(aResult);
  MOZ_ASSERT(XRE_IsParentProcess());
  *aResult = false;

  MOZ_LOG(gCookieBannerLog, LogLevel::Debug, ("%s", __FUNCTION__));

  // Service is disabled, throw.
  if (!mIsInitialized) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv = NS_OK;
  // Keep track of the HasRuleForBrowsingContextInternal needed, used for
  // logging.
  uint32_t numChecks = 0;

  // TODO: Optimization: We could avoid unecessary rule lookups by remembering
  // which domains we already looked up rules for. This would also need to take
  // isPBM and isTopLevel into account, because some rules only apply in certain
  // contexts.
  auto checkFn =
      [&](dom::BrowsingContext* bc) -> dom::BrowsingContext::WalkFlag {
    numChecks++;

    bool hasClickRule = false;
    bool hasCookieRule = false;
    // Pass ignoreDomainPref=true: when checking whether a suitable rule exists
    // we don't care what the domain-specific user pref is set to.
    rv = HasRuleForBrowsingContextInternal(bc, true, hasClickRule,
                                           hasCookieRule);
    // If the method failed abort the walk. We will return the stored error
    // result when exiting the method.
    if (NS_FAILED(rv)) {
      return dom::BrowsingContext::WalkFlag::Stop;
    }

    *aResult = hasClickRule || hasCookieRule;

    // Greedily return when we found a rule.
    if (*aResult) {
      return dom::BrowsingContext::WalkFlag::Stop;
    }

    return dom::BrowsingContext::WalkFlag::Next;
  };

  // Walk the BC (sub-)tree and return greedily when a rule is found for a
  // BrowsingContext.
  aBrowsingContext->PreOrderWalk(checkFn);

  MOZ_LOG(gCookieBannerLog, LogLevel::Debug,
          ("%s. success: %d, hasRule: %d, numChecks: %d", __FUNCTION__,
           NS_SUCCEEDED(rv), *aResult, numChecks));

  return rv;
}

nsresult nsCookieBannerService::HasRuleForBrowsingContextInternal(
    mozilla::dom::BrowsingContext* aBrowsingContext, bool aIgnoreDomainPref,
    bool& aHasClickRule, bool& aHasCookieRule) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(mIsInitialized);
  NS_ENSURE_ARG_POINTER(aBrowsingContext);

  MOZ_LOG(gCookieBannerLog, LogLevel::Debug, ("%s", __FUNCTION__));

  aHasClickRule = false;
  aHasCookieRule = false;

  // First, check if our current mode is disabled. If so there is no applicable
  // rule.
  nsICookieBannerService::Modes mode;
  nsresult rv = GetServiceModeForBrowsingContext(aBrowsingContext,
                                                 aIgnoreDomainPref, &mode);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mode == nsICookieBannerService::MODE_DISABLED ||
      StaticPrefs::cookiebanners_service_detectOnly()) {
    return NS_OK;
  }

  // In order to lookup rules we need to get the base domain associated with the
  // BrowsingContext.

  // 1. Get the window running in the BrowsingContext.
  RefPtr<dom::WindowGlobalParent> windowGlobalParent =
      aBrowsingContext->Canonical()->GetCurrentWindowGlobal();
  NS_ENSURE_TRUE(windowGlobalParent, NS_ERROR_FAILURE);

  // 2. Get the base domain from the content principal.
  nsCOMPtr<nsIPrincipal> principal = windowGlobalParent->DocumentPrincipal();
  NS_ENSURE_TRUE(principal, NS_ERROR_FAILURE);

  nsCString baseDomain;
  rv = principal->GetBaseDomain(baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(!baseDomain.IsEmpty(), NS_ERROR_FAILURE);

  MOZ_LOG(gCookieBannerLog, LogLevel::Debug,
          ("%s. baseDomain: %s", __FUNCTION__, baseDomain.get()));

  // 3. Look up click rules by baseDomain.
  // TODO: Optimization: We currently do two nsICookieBannerRule lookups, one
  // for click rules and one for cookie rules.
  nsTArray<RefPtr<nsIClickRule>> clickRules;
  rv = GetClickRulesForDomainInternal(baseDomain, aBrowsingContext->IsTop(),
                                      clickRules);
  NS_ENSURE_SUCCESS(rv, rv);

  // 3.1. Check if there is a non-empty click rule for the current environment.
  for (RefPtr<nsIClickRule>& rule : clickRules) {
    NS_ENSURE_TRUE(rule, NS_ERROR_NULL_POINTER);

    nsAutoCString optOut;
    rv = rule->GetOptOut(optOut);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!optOut.IsEmpty()) {
      aHasClickRule = true;
      break;
    }

    if (mode == nsICookieBannerService::MODE_REJECT_OR_ACCEPT) {
      nsAutoCString optIn;
      rv = rule->GetOptIn(optIn);
      NS_ENSURE_SUCCESS(rv, rv);

      if (!optIn.IsEmpty()) {
        aHasClickRule = true;
        break;
      }
    }
  }

  // 4. Check for cookie rules by baseDomain.
  nsTArray<RefPtr<nsICookieRule>> cookies;
  rv = GetCookieRulesForDomainInternal(baseDomain, mode,
                                       aBrowsingContext->IsTop(), cookies);
  NS_ENSURE_SUCCESS(rv, rv);

  aHasCookieRule = !cookies.IsEmpty();

  return NS_OK;
}

nsresult nsCookieBannerService::GetCookieRulesForDomainInternal(
    const nsACString& aBaseDomain, const nsICookieBannerService::Modes aMode,
    const bool aIsTopLevel, nsTArray<RefPtr<nsICookieRule>>& aCookies) {
  MOZ_ASSERT(mIsInitialized);
  MOZ_LOG(gCookieBannerLog, LogLevel::Debug,
          ("%s. aBaseDomain: %s", __FUNCTION__,
           PromiseFlatCString(aBaseDomain).get()));

  aCookies.Clear();

  // No cookie rules if disabled or in detect-only mode. Cookie injection is not
  // supported for the detect-only mode.
  if (aMode == nsICookieBannerService::MODE_DISABLED ||
      StaticPrefs::cookiebanners_service_detectOnly()) {
    return NS_OK;
  }

  // Cookies should only be injected for top-level frames.
  if (!aIsTopLevel) {
    return NS_OK;
  }

  nsCOMPtr<nsICookieBannerRule> cookieBannerRule;
  nsresult rv = GetRuleForDomain(aBaseDomain, getter_AddRefs(cookieBannerRule));
  NS_ENSURE_SUCCESS(rv, rv);

  // No rule found.
  if (!cookieBannerRule) {
    MOZ_LOG(
        gCookieBannerLog, LogLevel::Debug,
        ("%s. Returning empty array. No nsICookieBannerRule matching domain.",
         __FUNCTION__));
    return NS_OK;
  }

  // MODE_REJECT: In this mode we only handle the banner if we can reject. We
  // don't care about the opt-in cookies.
  rv = cookieBannerRule->GetCookies(true, aBaseDomain, aCookies);
  NS_ENSURE_SUCCESS(rv, rv);

  // MODE_REJECT_OR_ACCEPT: In this mode we will try to opt-out, but if we don't
  // have any opt-out cookies we will fallback to the opt-in cookies.
  if (aMode == nsICookieBannerService::MODE_REJECT_OR_ACCEPT &&
      aCookies.IsEmpty()) {
    MOZ_LOG(gCookieBannerLog, LogLevel::Debug,
            ("%s. Returning opt-in cookies for %s.", __FUNCTION__,
             PromiseFlatCString(aBaseDomain).get()));

    return cookieBannerRule->GetCookies(false, aBaseDomain, aCookies);
  }

  MOZ_LOG(gCookieBannerLog, LogLevel::Debug,
          ("%s. Returning opt-out cookies for %s.", __FUNCTION__,
           PromiseFlatCString(aBaseDomain).get()));
  return NS_OK;
}

NS_IMETHODIMP
nsCookieBannerService::GetDomainPref(nsIURI* aTopLevelURI,
                                     const bool aIsPrivate,
                                     nsICookieBannerService::Modes* aModes) {
  NS_ENSURE_ARG_POINTER(aTopLevelURI);
  NS_ENSURE_ARG_POINTER(aModes);

  if (!mIsInitialized) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv;
  nsCOMPtr<nsIEffectiveTLDService> eTLDService(
      do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString baseDomain;
  rv = eTLDService->GetBaseDomain(aTopLevelURI, 0, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  return GetDomainPrefInternal(baseDomain, aIsPrivate, aModes);
}

nsresult nsCookieBannerService::GetDomainPrefInternal(
    const nsACString& aBaseDomain, const bool aIsPrivate,
    nsICookieBannerService::Modes* aModes) {
  MOZ_ASSERT(mIsInitialized);
  NS_ENSURE_ARG_POINTER(aModes);

  auto pref = mDomainPrefService->GetPref(aBaseDomain, aIsPrivate);

  *aModes = nsICookieBannerService::MODE_UNSET;

  if (pref.isSome()) {
    *aModes = pref.value();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCookieBannerService::SetDomainPref(nsIURI* aTopLevelURI,
                                     nsICookieBannerService::Modes aModes,
                                     const bool aIsPrivate) {
  NS_ENSURE_ARG_POINTER(aTopLevelURI);

  return SetDomainPrefInternal(aTopLevelURI, aModes, aIsPrivate, false);
}

NS_IMETHODIMP
nsCookieBannerService::SetDomainPrefAndPersistInPrivateBrowsing(
    nsIURI* aTopLevelURI, nsICookieBannerService::Modes aModes) {
  NS_ENSURE_ARG_POINTER(aTopLevelURI);

  return SetDomainPrefInternal(aTopLevelURI, aModes, true, true);
};

nsresult nsCookieBannerService::SetDomainPrefInternal(
    nsIURI* aTopLevelURI, nsICookieBannerService::Modes aModes,
    const bool aIsPrivate, const bool aPersistInPrivateBrowsing) {
  NS_ENSURE_ARG_POINTER(aTopLevelURI);

  if (!mIsInitialized) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv;
  nsCOMPtr<nsIEffectiveTLDService> eTLDService(
      do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString baseDomain;
  rv = eTLDService->GetBaseDomain(aTopLevelURI, 0, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDomainPrefService->SetPref(baseDomain, aModes, aIsPrivate,
                                   aPersistInPrivateBrowsing);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsCookieBannerService::RemoveDomainPref(nsIURI* aTopLevelURI,
                                        const bool aIsPrivate) {
  NS_ENSURE_ARG_POINTER(aTopLevelURI);

  if (!mIsInitialized) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv;
  nsCOMPtr<nsIEffectiveTLDService> eTLDService(
      do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString baseDomain;
  rv = eTLDService->GetBaseDomain(aTopLevelURI, 0, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  return mDomainPrefService->RemovePref(baseDomain, aIsPrivate);
}

NS_IMETHODIMP
nsCookieBannerService::RemoveAllDomainPrefs(const bool aIsPrivate) {
  if (!mIsInitialized) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return mDomainPrefService->RemoveAll(aIsPrivate);
}

NS_IMETHODIMP
nsCookieBannerService::ShouldStopBannerClickingForSite(const nsACString& aSite,
                                                       const bool aIsTopLevel,
                                                       const bool aIsPrivate,
                                                       bool* aShouldStop) {
  if (!mIsInitialized) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  uint8_t threshold =
      StaticPrefs::cookiebanners_bannerClicking_maxTriesPerSiteAndSession();

  // Don't stop banner clicking if the pref is set to zero.
  if (threshold == 0) {
    *aShouldStop = false;
    return NS_OK;
  }

  // Ensure we won't use an overflowed threshold.
  threshold = std::min(threshold, std::numeric_limits<uint8_t>::max());

  auto entry = mExecutedDataForSites.MaybeGet(aSite);

  if (!entry) {
    return NS_OK;
  }

  auto& data = entry.ref();
  uint8_t cnt = 0;

  if (aIsPrivate) {
    cnt = aIsTopLevel ? data.countExecutedInTopPrivate
                      : data.countExecutedInFramePrivate;
  } else {
    cnt = aIsTopLevel ? data.countExecutedInTop : data.countExecutedInFrame;
  }

  *aShouldStop = cnt >= threshold;

  return NS_OK;
}

NS_IMETHODIMP
nsCookieBannerService::MarkSiteExecuted(const nsACString& aSite,
                                        const bool aIsTopLevel,
                                        const bool aIsPrivate) {
  NS_ENSURE_TRUE(!aSite.IsEmpty(), NS_ERROR_INVALID_ARG);

  if (!mIsInitialized) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  auto& data = mExecutedDataForSites.LookupOrInsert(aSite);
  uint8_t* count = nullptr;

  if (aIsPrivate) {
    if (aIsTopLevel) {
      count = &data.countExecutedInTopPrivate;
    } else {
      count = &data.countExecutedInFramePrivate;
    }
  } else {
    if (aIsTopLevel) {
      count = &data.countExecutedInTop;
    } else {
      count = &data.countExecutedInFrame;
    }
  }

  MOZ_ASSERT(count);

  // Ensure we never overflow.
  if (*count < std::numeric_limits<uint8_t>::max()) {
    (*count) += 1;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCookieBannerService::RemoveExecutedRecordForSite(const nsACString& aSite,
                                                   const bool aIsPrivate) {
  if (!mIsInitialized) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  auto entry = mExecutedDataForSites.Lookup(aSite);

  if (!entry) {
    return NS_OK;
  }

  auto data = entry.Data();

  if (aIsPrivate) {
    data.countExecutedInTopPrivate = 0;
    data.countExecutedInFramePrivate = 0;
  } else {
    data.countExecutedInTop = 0;
    data.countExecutedInFrame = 0;
  }

  // We can remove the entry if there is no flag set after removal.
  if (!data.countExecutedInTop && !data.countExecutedInFrame &&
      !data.countExecutedInTopPrivate && !data.countExecutedInFramePrivate) {
    entry.Remove();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCookieBannerService::RemoveAllExecutedRecords(const bool aIsPrivate) {
  if (!mIsInitialized) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  for (auto iter = mExecutedDataForSites.Iter(); !iter.Done(); iter.Next()) {
    auto& data = iter.Data();
    // Clear the flags.
    if (aIsPrivate) {
      data.countExecutedInTopPrivate = 0;
      data.countExecutedInFramePrivate = 0;
    } else {
      data.countExecutedInTop = 0;
      data.countExecutedInFrame = 0;
    }

    // Remove the entry if there is no flag set.
    if (!data.countExecutedInTop && !data.countExecutedInFrame &&
        !data.countExecutedInTopPrivate && !data.countExecutedInFramePrivate) {
      iter.Remove();
    }
  }

  return NS_OK;
}

void nsCookieBannerService::DailyReportTelemetry() {
  MOZ_ASSERT(NS_IsMainThread());

  // Convert modes to strings
  uint32_t mode = StaticPrefs::cookiebanners_service_mode();
  uint32_t modePBM = StaticPrefs::cookiebanners_service_mode_privateBrowsing();

  nsCString modeStr = ConvertModeToStringForTelemetry(mode);
  nsCString modePBMStr = ConvertModeToStringForTelemetry(modePBM);

  nsTArray<nsCString> serviceModeLabels = {
      "disabled"_ns,
      "reject"_ns,
      "reject_or_accept"_ns,
      "invalid"_ns,
  };

  // Record the service mode glean.
  for (const auto& label : serviceModeLabels) {
    glean::cookie_banners::normal_window_service_mode.Get(label).Set(
        modeStr.Equals(label));
    glean::cookie_banners::private_window_service_mode.Get(label).Set(
        modePBMStr.Equals(label));
  }

  // Report the state of the cookiebanners.service.detectOnly pref.
  glean::cookie_banners::service_detect_only.Set(
      StaticPrefs::cookiebanners_service_detectOnly());
}

nsresult nsCookieBannerService::GetServiceModeForBrowsingContext(
    dom::BrowsingContext* aBrowsingContext, bool aIgnoreDomainPref,
    nsICookieBannerService::Modes* aMode) {
  MOZ_ASSERT(XRE_IsParentProcess());
  NS_ENSURE_ARG_POINTER(aBrowsingContext);
  NS_ENSURE_ARG_POINTER(aMode);

  bool usePBM = false;
  nsresult rv = aBrowsingContext->GetUsePrivateBrowsing(&usePBM);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t mode;
  if (usePBM) {
    mode = StaticPrefs::cookiebanners_service_mode_privateBrowsing();
  } else {
    mode = StaticPrefs::cookiebanners_service_mode();
  }

  // We can skip checking domain-specific prefs if passed the skip pref or if
  // the mode pref disables the feature. Per-domain modes enabling the service
  // sites while it's globally disabled is not supported.
  if (aIgnoreDomainPref || mode == nsICookieBannerService::MODE_DISABLED) {
    *aMode = static_cast<nsICookieBannerService::Modes>(mode);
    return NS_OK;
  }

  // Check if there is a per-domain service mode, disabling the feature for a
  // specific domain or overriding the mode.
  RefPtr<dom::WindowGlobalParent> topWGP =
      aBrowsingContext->Top()->Canonical()->GetCurrentWindowGlobal();
  NS_ENSURE_TRUE(topWGP, NS_ERROR_FAILURE);

  // Get the base domain from the content principal
  nsCOMPtr<nsIPrincipal> principal = topWGP->DocumentPrincipal();
  NS_ENSURE_TRUE(principal, NS_ERROR_NULL_POINTER);

  nsCString baseDomain;
  rv = principal->GetBaseDomain(baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(!baseDomain.IsEmpty(), NS_ERROR_FAILURE);

  // Get the domain preference for the top-level baseDomain, the domain
  // preference takes precedence over the global pref setting.
  nsICookieBannerService::Modes domainPref;
  rv = GetDomainPrefInternal(baseDomain, usePBM, &domainPref);
  NS_ENSURE_SUCCESS(rv, rv);

  if (domainPref != nsICookieBannerService::MODE_UNSET) {
    mode = domainPref;
  }

  *aMode = static_cast<nsICookieBannerService::Modes>(mode);

  return NS_OK;
}

}  // namespace mozilla
