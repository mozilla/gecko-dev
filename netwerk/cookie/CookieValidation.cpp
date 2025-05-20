/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CookieValidation.h"
#include "CookieLogging.h"
#include "CookieService.h"
#include "mozilla/dom/nsMixedContentBlocker.h"
#include "mozilla/StaticPrefs_network.h"

constexpr uint32_t kMaxBytesPerCookie = 4096;
constexpr uint32_t kMaxBytesPerDomain = 1024;
constexpr uint32_t kMaxBytesPerPath = 1024;

using namespace mozilla::net;

NS_IMPL_ISUPPORTS(CookieValidation, nsICookieValidation)

CookieValidation::CookieValidation(const CookieStruct& aCookieData)
    : mCookieData(aCookieData) {}

// static
already_AddRefed<CookieValidation> CookieValidation::Validate(
    const CookieStruct& aCookieData) {
  RefPtr<CookieValidation> cv = new CookieValidation(aCookieData);
  cv->ValidateInternal();
  return cv.forget();
}

// static
already_AddRefed<CookieValidation> CookieValidation::ValidateForHost(
    const CookieStruct& aCookieData, nsIURI* aHostURI,
    const nsACString& aBaseDomain, bool aRequireHostMatch, bool aFromHttp) {
  RefPtr<CookieValidation> cv = new CookieValidation(aCookieData);
  cv->ValidateForHostInternal(aHostURI, aBaseDomain, aRequireHostMatch,
                              aFromHttp);
  return cv.forget();
}

// static
already_AddRefed<CookieValidation> CookieValidation::ValidateInContext(
    const CookieStruct& aCookieData, nsIURI* aHostURI,
    const nsACString& aBaseDomain, bool aRequireHostMatch, bool aFromHttp,
    bool aIsForeignAndNotAddon, bool aPartitionedOnly,
    bool aIsInPrivateBrowsing) {
  RefPtr<CookieValidation> cv = new CookieValidation(aCookieData);
  cv->ValidateInContextInternal(aHostURI, aBaseDomain, aRequireHostMatch,
                                aFromHttp, aIsForeignAndNotAddon,
                                aPartitionedOnly, aIsInPrivateBrowsing);
  return cv.forget();
}

void CookieValidation::ValidateInternal() {
  MOZ_ASSERT(mResult == eOK);

  // reject cookie if name and value are empty, per RFC6265bis
  if (mCookieData.name().IsEmpty() && mCookieData.value().IsEmpty()) {
    mResult = eRejectedEmptyNameAndValue;
    return;
  }

  // reject cookie if it's over the size limit, per RFC2109
  if (!CheckNameAndValueSize(mCookieData)) {
    mResult = eRejectedNameValueOversize;
    return;
  }

  if (!CheckName(mCookieData)) {
    mResult = eRejectedInvalidCharName;
    return;
  }

  if (!CheckValue(mCookieData)) {
    mResult = eRejectedInvalidCharValue;
    return;
  }

  if (mCookieData.path().Length() > kMaxBytesPerPath) {
    mResult = eRejectedAttributePathOversize;
    return;
  }

  if (mCookieData.path().Contains('\t')) {
    mResult = eRejectedInvalidPath;
    return;
  }

  if (mCookieData.host().Length() > kMaxBytesPerDomain) {
    mResult = eRejectedAttributeDomainOversize;
    return;
  }

  // If a cookie is nameless, then its value must not start with
  // `__Host-` or `__Secure-`
  if (mCookieData.name().IsEmpty() && (HasSecurePrefix(mCookieData.value()) ||
                                       HasHostPrefix(mCookieData.value()))) {
    mResult = eRejectedInvalidPrefix;
    return;
  }

  // If same-site is explicitly set to 'none' but this is not a secure context,
  // let's abort the parsing.
  if (!mCookieData.isSecure() &&
      mCookieData.sameSite() == nsICookie::SAMESITE_NONE) {
    if (StaticPrefs::network_cookie_sameSite_noneRequiresSecure()) {
      mResult = eRejectedNoneRequiresSecure;
      return;
    }

    // Still warn about the missing Secure attribute when not enforcing.
    mWarnings.mSameSiteNoneRequiresSecureForBeta = true;
  }

  // Ensure the partitioned cookie is set with the secure attribute if CHIPS
  // is enabled.
  if (StaticPrefs::network_cookie_CHIPS_enabled() &&
      mCookieData.isPartitioned() && !mCookieData.isSecure()) {
    mResult = eRejectedPartitionedRequiresSecure;
    return;
  }
}

void CookieValidation::ValidateForHostInternal(nsIURI* aHostURI,
                                               const nsACString& aBaseDomain,
                                               bool aRequireHostMatch,
                                               bool aFromHttp) {
  MOZ_ASSERT(mResult == eOK);

  ValidateInternal();
  if (mResult != eOK) {
    return;
  }

  if (!aBaseDomain.IsEmpty() &&
      !CheckDomain(mCookieData, aHostURI, aBaseDomain, aRequireHostMatch)) {
    mResult = eRejectedInvalidDomain;
    return;
  }

  // if the new cookie is httponly, make sure we're not coming from script
  if (!aFromHttp && mCookieData.isHttpOnly()) {
    mResult = eRejectedHttpOnlyButFromScript;
    return;
  }

  bool potentiallyTrustworthy =
      nsMixedContentBlocker::IsPotentiallyTrustworthyOrigin(aHostURI);

  if (!CheckPrefixes(mCookieData, potentiallyTrustworthy)) {
    mResult = eRejectedInvalidPrefix;
    return;
  }

  // If the new cookie is non-https and wants to set secure flag,
  // browser have to ignore this new cookie.
  // (draft-ietf-httpbis-cookie-alone section 3.1)
  if (mCookieData.isSecure() && !potentiallyTrustworthy) {
    mResult = eRejectedSecureButNonHttps;
    return;
  }

  if (mCookieData.sameSite() == nsICookie::SAMESITE_UNSET) {
    bool laxByDefault =
        StaticPrefs::network_cookie_sameSite_laxByDefault() &&
        !nsContentUtils::IsURIInPrefList(
            aHostURI, "network.cookie.sameSite.laxByDefault.disabledHosts");
    if (laxByDefault) {
      mWarnings.mSameSiteLaxForced = true;
    } else if (StaticPrefs::
                   network_cookie_sameSite_laxByDefaultWarningsForBeta()) {
      mWarnings.mSameSiteLaxForcedForBeta = true;
    }
  }
}

void CookieValidation::ValidateInContextInternal(
    nsIURI* aHostURI, const nsACString& aBaseDomain, bool aRequireHostMatch,
    bool aFromHttp, bool aIsForeignAndNotAddon, bool aPartitionedOnly,
    bool aIsInPrivateBrowsing) {
  MOZ_ASSERT(mResult == eOK);

  ValidateForHostInternal(aHostURI, aBaseDomain, aRequireHostMatch, aFromHttp);
  if (mResult != eOK) {
    return;
  }

  // If the cookie is same-site but in a cross site context, browser must
  // ignore the cookie.
  bool laxByDefault =
      StaticPrefs::network_cookie_sameSite_laxByDefault() &&
      !nsContentUtils::IsURIInPrefList(
          aHostURI, "network.cookie.sameSite.laxByDefault.disabledHosts");
  uint32_t sameSite = mCookieData.sameSite();
  if (sameSite == nsICookie::SAMESITE_UNSET) {
    sameSite =
        laxByDefault ? nsICookie::SAMESITE_LAX : nsICookie::SAMESITE_NONE;
  }

  if (sameSite != nsICookie::SAMESITE_NONE && aIsForeignAndNotAddon) {
    mResult = eRejectedForNonSameSiteness;
    return;
  }
}

NS_IMETHODIMP
CookieValidation::GetResult(nsICookieValidation::ValidationError* aRetval) {
  NS_ENSURE_ARG_POINTER(aRetval);
  *aRetval = mResult;
  return NS_OK;
}

// static
bool CookieValidation::CheckDomain(const CookieStruct& aCookieData,
                                   nsIURI* aHostURI,
                                   const nsACString& aBaseDomain,
                                   bool aRequireHostMatch) {
  // Note: The logic in this function is mirrored in
  // toolkit/components/extensions/ext-cookies.js:checkSetCookiePermissions().
  // If it changes, please update that function, or file a bug for someone
  // else to do so.

  if (aCookieData.host().IsEmpty()) {
    return false;
  }

  // get host from aHostURI
  nsAutoCString hostFromURI;
  nsContentUtils::GetHostOrIPv6WithBrackets(aHostURI, hostFromURI);

  // check whether the host is either an IP address, an alias such as
  // 'localhost', an eTLD such as 'co.uk', or the empty string. in these
  // cases, require an exact string match for the domain, and leave the cookie
  // as a non-domain one. bug 105917 originally noted the requirement to deal
  // with IP addresses.
  if (aRequireHostMatch) {
    return hostFromURI.Equals(aCookieData.host());
  }

  nsCString cookieHost = aCookieData.host();
  // Tolerate leading '.' characters, but not if it's otherwise an empty host.
  if (aCookieData.host().Length() > 1 && aCookieData.host().First() == '.') {
    cookieHost.Cut(0, 1);
  }

  // ensure the proposed domain is derived from the base domain; and also
  // that the host domain is derived from the proposed domain (per RFC2109).
  if (CookieCommons::IsSubdomainOf(cookieHost, aBaseDomain) &&
      CookieCommons::IsSubdomainOf(hostFromURI, cookieHost)) {
    return true;
  }

  /*
   * note: RFC2109 section 4.3.2 requires that we check the following:
   * that the portion of host not in domain does not contain a dot.
   * this prevents hosts of the form x.y.co.nz from setting cookies in the
   * entire .co.nz domain. however, it's only a only a partial solution and
   * it breaks sites (IE doesn't enforce it), so we don't perform this check.
   */
  return false;
}

// static
bool CookieValidation::HasSecurePrefix(const nsACString& aString) {
  return StringBeginsWith(aString, "__Secure-"_ns,
                          nsCaseInsensitiveCStringComparator);
}

// static
bool CookieValidation::HasHostPrefix(const nsACString& aString) {
  return StringBeginsWith(aString, "__Host-"_ns,
                          nsCaseInsensitiveCStringComparator);
}

// CheckPrefixes
//
// Reject cookies whose name starts with the magic prefixes from
// https://datatracker.ietf.org/doc/html/draft-ietf-httpbis-rfc6265bis
// if they do not meet the criteria required by the prefix.
bool CookieValidation::CheckPrefixes(const CookieStruct& aCookieData,
                                     bool aSecureRequest) {
  bool hasSecurePrefix = HasSecurePrefix(aCookieData.name());
  bool hasHostPrefix = HasHostPrefix(aCookieData.name());

  if (!hasSecurePrefix && !hasHostPrefix) {
    // not one of the magic prefixes: carry on
    return true;
  }

  if (!aSecureRequest || !aCookieData.isSecure()) {
    // the magic prefixes may only be used from a secure request and
    // the secure attribute must be set on the cookie
    return false;
  }

  if (hasHostPrefix) {
    // The host prefix requires that the path is "/" and that the cookie had no
    // domain attribute. FixDomain() and FixPath() from CookieParser MUST be
    // run first to make sure invalid attributes are rejected and to
    // regularlize them. In particular all explicit domain attributes result in
    // a host that starts with a dot, and if the host doesn't start with a dot
    // it correctly matches the true host.
    if (aCookieData.host()[0] == '.' ||
        !aCookieData.path().EqualsLiteral("/")) {
      return false;
    }
  }

  return true;
}

void CookieValidation::RetrieveErrorLogData(uint32_t* aFlags,
                                            nsACString& aCategory,
                                            nsACString& aKey,
                                            nsTArray<nsString>& aParams) const {
  MOZ_ASSERT(aFlags);
  MOZ_ASSERT(aParams.IsEmpty());

  *aFlags = nsIScriptError::errorFlag;

#define SET_LOG_DATA(category, x) \
  aCategory = category;           \
  aKey = x;                       \
  aParams.AppendElement(NS_ConvertUTF8toUTF16(mCookieData.name()));

  switch (mResult) {
    case eOK:
      return;

    case eRejectedEmptyNameAndValue: {
      *aFlags = nsIScriptError::warningFlag;
      aCategory.Assign(CONSOLE_REJECTION_CATEGORY);
      aKey.Assign("CookieRejectedEmptyNameAndValue"_ns);
      return;
    }

    case eRejectedNoneRequiresSecure: {
      SET_LOG_DATA(CONSOLE_SAMESITE_CATEGORY,
                   "CookieRejectedNonRequiresSecure2"_ns);
      return;
    }

    case eRejectedPartitionedRequiresSecure: {
      SET_LOG_DATA(CONSOLE_REJECTION_CATEGORY,
                   "CookieRejectedPartitionedRequiresSecure"_ns);
      return;
    }

    case eRejectedNameValueOversize: {
      *aFlags = nsIScriptError::warningFlag;
      aCategory.Assign(CONSOLE_OVERSIZE_CATEGORY);
      aKey.Assign("CookieOversize"_ns);

      aParams.AppendElement(NS_ConvertUTF8toUTF16(mCookieData.name()));

      nsString size;
      size.AppendInt(kMaxBytesPerCookie);
      aParams.AppendElement(size);
      return;
    }

    case eRejectedInvalidCharName: {
      SET_LOG_DATA(CONSOLE_REJECTION_CATEGORY,
                   "CookieRejectedInvalidCharName"_ns);
      return;
    }

    case eRejectedInvalidCharValue: {
      SET_LOG_DATA(CONSOLE_REJECTION_CATEGORY,
                   "CookieRejectedInvalidCharValue"_ns);
      return;
    }

    case eRejectedAttributePathOversize: {
      SET_LOG_DATA(CONSOLE_REJECTION_CATEGORY,
                   "CookieRejectedAttributePathOversize"_ns);
      return;
    }

    case eRejectedAttributeDomainOversize: {
      SET_LOG_DATA(CONSOLE_REJECTION_CATEGORY,
                   "CookieRejectedAttributeDomainOversize"_ns);
      return;
    }

    case eRejectedInvalidPath: {
      SET_LOG_DATA(CONSOLE_REJECTION_CATEGORY, "CookieRejectedInvalidPath"_ns);
      return;
    }

    case eRejectedInvalidDomain: {
      SET_LOG_DATA(CONSOLE_REJECTION_CATEGORY,
                   "CookieRejectedInvalidDomain"_ns);
      return;
    }

    case eRejectedInvalidPrefix: {
      SET_LOG_DATA(CONSOLE_REJECTION_CATEGORY,
                   "CookieRejectedInvalidPrefix"_ns);
      return;
    }

    case eRejectedHttpOnlyButFromScript: {
      SET_LOG_DATA(CONSOLE_REJECTION_CATEGORY,
                   "CookieRejectedHttpOnlyButFromScript"_ns);
      return;
    }

    case eRejectedSecureButNonHttps: {
      SET_LOG_DATA(CONSOLE_REJECTION_CATEGORY,
                   "CookieRejectedSecureButNonHttps"_ns);
      return;
    }

    case eRejectedForNonSameSiteness: {
      SET_LOG_DATA(CONSOLE_SAMESITE_CATEGORY,
                   "CookieRejectedForNonSameSiteness"_ns);
      return;
    }
  }

#undef SET_LOG_DATA
}

void CookieValidation::ReportErrorsAndWarnings(nsIConsoleReportCollector* aCRC,
                                               nsIURI* aHostURI) const {
  if (mResult != eOK) {
    uint32_t flags;
    nsAutoCString category;
    nsAutoCString key;
    nsTArray<nsString> params;

    RetrieveErrorLogData(&flags, category, key, params);

    CookieLogging::LogMessageToConsole(aCRC, aHostURI, flags, category, key,
                                       params);
    return;
  }

  if (mWarnings.mSameSiteNoneRequiresSecureForBeta) {
    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::warningFlag, CONSOLE_SAMESITE_CATEGORY,
        "CookieRejectedNonRequiresSecureForBeta3"_ns,
        AutoTArray<nsString, 2>{NS_ConvertUTF8toUTF16(mCookieData.name()),
                                SAMESITE_MDN_URL});
  }

  if (mWarnings.mSameSiteLaxForced) {
    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::infoFlag, CONSOLE_SAMESITE_CATEGORY,
        "CookieLaxForced2"_ns,
        AutoTArray<nsString, 1>{NS_ConvertUTF8toUTF16(mCookieData.name())});
  }

  if (mWarnings.mSameSiteLaxForcedForBeta) {
    CookieLogging::LogMessageToConsole(
        aCRC, aHostURI, nsIScriptError::warningFlag, CONSOLE_SAMESITE_CATEGORY,
        "CookieLaxForcedForBeta2"_ns,
        AutoTArray<nsString, 2>{NS_ConvertUTF8toUTF16(mCookieData.name()),
                                SAMESITE_MDN_URL});
  }
}

NS_IMETHODIMP
CookieValidation::GetErrorString(nsAString& aResult) {
  if (mResult == eOK) {
    return NS_OK;
  }

  uint32_t flags;
  nsAutoCString category;
  nsAutoCString key;
  nsTArray<nsString> params;

  RetrieveErrorLogData(&flags, category, key, params);

  return nsContentUtils::FormatLocalizedString(
      nsContentUtils::eNECKO_PROPERTIES_en_US, key.get(), params, aResult);
}

// static
bool CookieValidation::CheckNameAndValueSize(const CookieStruct& aCookieData) {
  // reject cookie if it's over the size limit, per RFC2109
  return (aCookieData.name().Length() + aCookieData.value().Length()) <=
         kMaxBytesPerCookie;
}

bool CookieValidation::CheckName(const CookieStruct& aCookieData) {
  const char illegalNameCharacters[] = {
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x0A, 0x0B, 0x0C, 0x0D,
      0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
      0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x3B, 0x3D, 0x7F, 0x00};

  const auto* start = aCookieData.name().BeginReading();
  const auto* end = aCookieData.name().EndReading();

  auto charFilter = [&](unsigned char c) {
    if (StaticPrefs::network_cookie_blockUnicode() && c >= 0x80) {
      return true;
    }
    return std::find(std::begin(illegalNameCharacters),
                     std::end(illegalNameCharacters),
                     c) != std::end(illegalNameCharacters);
  };

  return std::find_if(start, end, charFilter) == end;
}

bool CookieValidation::CheckValue(const CookieStruct& aCookieData) {
  // reject cookie if value contains an RFC 6265 disallowed character - see
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1191423
  // NOTE: this is not the full set of characters disallowed by 6265 - notably
  // 0x09, 0x20, 0x22, 0x2C, and 0x5C are missing from this list.
  const char illegalCharacters[] = {
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x0A, 0x0B, 0x0C,
      0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
      0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x3B, 0x7F, 0x00};

  const auto* start = aCookieData.value().BeginReading();
  const auto* end = aCookieData.value().EndReading();

  auto charFilter = [&](unsigned char c) {
    if (StaticPrefs::network_cookie_blockUnicode() && c >= 0x80) {
      return true;
    }
    return std::find(std::begin(illegalCharacters), std::end(illegalCharacters),
                     c) != std::end(illegalCharacters);
  };

  return std::find_if(start, end, charFilter) == end;
}
