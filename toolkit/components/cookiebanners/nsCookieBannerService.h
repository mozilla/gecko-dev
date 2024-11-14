/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_nsCookieBannerService_h__
#define mozilla_nsCookieBannerService_h__

#include "nsICookieBannerRule.h"
#include "nsICookieBannerService.h"
#include "nsICookieBannerListService.h"
#include "nsCOMPtr.h"
#include "nsTHashMap.h"
#include "nsIObserver.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {

class CookieBannerDomainPrefService;

class nsCookieBannerService final : public nsIObserver,
                                    public nsICookieBannerService {
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  NS_DECL_NSICOOKIEBANNERSERVICE

 public:
  static already_AddRefed<nsCookieBannerService> GetSingleton();

 private:
  nsCookieBannerService() = default;
  ~nsCookieBannerService() = default;

  // Whether the service is enabled and ready to accept requests.
  bool mIsInitialized = false;

  nsCOMPtr<nsICookieBannerListService> mListService;
  RefPtr<CookieBannerDomainPrefService> mDomainPrefService;

  // Map of site specific cookie banner rules keyed by domain.
  nsTHashMap<nsCStringHashKey, nsCOMPtr<nsICookieBannerRule>> mRules;

  // Map of global cookie banner rules keyed by id.
  nsTHashMap<nsCStringHashKey, nsCOMPtr<nsICookieBannerRule>> mGlobalRules;

  // Pref change callback which initializes and shuts down the service. This is
  // also called on startup.
  static void OnPrefChange(const char* aPref, void* aData);

  /**
   * Initializes internal state. Will be called on profile-after-change and on
   * pref changes.
   */
  [[nodiscard]] nsresult Init();

  /**
   * Cleanup method to be called on shutdown or pref change.
   */
  [[nodiscard]] nsresult Shutdown();

  nsresult GetClickRulesForDomainInternal(
      const nsACString& aDomain, const bool aIsTopLevel,
      nsTArray<RefPtr<nsIClickRule>>& aRules);

  nsresult GetCookieRulesForDomainInternal(
      const nsACString& aBaseDomain, const nsICookieBannerService::Modes aMode,
      const bool aIsTopLevel, nsTArray<RefPtr<nsICookieRule>>& aCookies);

  nsresult HasRuleForBrowsingContextInternal(
      mozilla::dom::BrowsingContext* aBrowsingContext, bool aIgnoreDomainPref,
      bool& aHasClickRule, bool& aHasCookieRule);

  nsresult GetRuleForDomain(const nsACString& aDomain,
                            nsICookieBannerRule** aRule);

  nsresult GetServiceModeForBrowsingContext(
      dom::BrowsingContext* aBrowsingContext, bool aIgnoreDomainPref,
      nsICookieBannerService::Modes* aMode);

  /**
   * Lookup a domain pref by base domain.
   */
  nsresult GetDomainPrefInternal(const nsACString& aBaseDomain,
                                 const bool aIsPrivate,
                                 nsICookieBannerService::Modes* aModes);

  nsresult SetDomainPrefInternal(nsIURI* aTopLevelURI,
                                 nsICookieBannerService::Modes aModes,
                                 const bool aIsPrivate,
                                 const bool aPersistInPrivateBrowsing);

  void DailyReportTelemetry();
  // A record that stores whether we have executed the banner click for the
  // context.
  typedef struct ExecutedData {
    ExecutedData()
        : countExecutedInTop(0),
          countExecutedInFrame(0),
          countExecutedInTopPrivate(0),
          countExecutedInFramePrivate(0) {}

    uint8_t countExecutedInTop;
    uint8_t countExecutedInFrame;
    uint8_t countExecutedInTopPrivate;
    uint8_t countExecutedInFramePrivate;
  } ExecutedData;

  // Map of the sites (eTLD+1) that we have executed the cookie banner handling
  // for this session.
  nsTHashMap<nsCStringHashKey, ExecutedData> mExecutedDataForSites;
};

}  // namespace mozilla

#endif
