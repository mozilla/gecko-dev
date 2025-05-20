/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CookieValidation_h
#define mozilla_net_CookieValidation_h

#include "nsICookieValidation.h"
#include "Cookie.h"

class nsIConsoleReportCollector;

namespace mozilla {
namespace net {

constexpr auto CONSOLE_CHIPS_CATEGORY = "cookiesCHIPS"_ns;
constexpr auto CONSOLE_OVERSIZE_CATEGORY = "cookiesOversize"_ns;
constexpr auto CONSOLE_REJECTION_CATEGORY = "cookiesRejection"_ns;
constexpr auto CONSOLE_SAMESITE_CATEGORY = "cookieSameSite"_ns;
constexpr auto SAMESITE_MDN_URL =
    "https://developer.mozilla.org/docs/Web/HTTP/Reference/Headers/Set-Cookie#"
    u"samesitesamesite-value"_ns;

class CookieValidation final : public nsICookieValidation {
  NS_DECL_ISUPPORTS
  NS_DECL_NSICOOKIEVALIDATION

 public:
  static already_AddRefed<CookieValidation> Validate(
      const CookieStruct& aCookieData);

  static already_AddRefed<CookieValidation> ValidateForHost(
      const CookieStruct& aCookieData, nsIURI* aHostURI,
      const nsACString& aBaseDomain, bool aRequireHostMatch, bool aFromHttp);

  static already_AddRefed<CookieValidation> ValidateInContext(
      const CookieStruct& aCookieData, nsIURI* aHostURI,
      const nsACString& aBaseDomain, bool aRequireHostMatch, bool aFromHttp,
      bool aIsForeignAndNotAddon, bool aPartitionedOnly,
      bool aIsInPrivateBrowsing);

  static CookieValidation* Cast(nsICookieValidation* aValidation) {
    return static_cast<CookieValidation*>(aValidation);
  }

  nsICookieValidation::ValidationError Result() const { return mResult; }

  void ReportErrorsAndWarnings(nsIConsoleReportCollector* aCRC,
                               nsIURI* aHostURI) const;

 private:
  explicit CookieValidation(const CookieStruct& aCookieData);
  ~CookieValidation() = default;

  void ValidateInternal();

  void ValidateForHostInternal(nsIURI* aHostURI, const nsACString& aBaseDomain,
                               bool aRequireHostMatch, bool aFromHttp);

  void ValidateInContextInternal(
      nsIURI* aHostURI, const nsACString& aBaseDomain, bool aRequireHostMatch,
      bool aFromHttp, bool aIsForeignAndNotAddon, bool aPartitionedOnly,
      bool aIsInPrivateBrowsing);

  static bool CheckNameAndValueSize(const CookieStruct& aCookieData);

  static bool CheckName(const CookieStruct& aCookieData);

  static bool CheckValue(const CookieStruct& aCookieData);

  static bool CheckDomain(const CookieStruct& aCookieData, nsIURI* aHostURI,
                          const nsACString& aBaseDomain,
                          bool aRequireHostMatch);

  static bool CheckPrefixes(const CookieStruct& aCookieData,
                            bool aSecureRequest);

  static bool HasSecurePrefix(const nsACString& aString);
  static bool HasHostPrefix(const nsACString& aString);

  CookieStruct mCookieData;

  nsICookieValidation::ValidationError mResult = eOK;

  void RetrieveErrorLogData(uint32_t* aFlags, nsACString& aCategory,
                            nsACString& aKey,
                            nsTArray<nsString>& aParams) const;

  struct Warnings {
    bool mSameSiteLaxForced = false;
    bool mSameSiteLaxForcedForBeta = false;
    bool mSameSiteNoneRequiresSecureForBeta = false;
  } mWarnings;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_CookieValidation_h
