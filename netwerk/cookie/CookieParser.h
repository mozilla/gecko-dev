/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CookieParser_h
#define mozilla_net_CookieParser_h

#include "CookieCommons.h"

#include "mozilla/net/NeckoChannelParams.h"
#include "nsTArray.h"
#include "nsCOMPtr.h"

class nsIConsoleReportCollector;
class nsIURI;

namespace mozilla {
namespace net {

class CookieParser final {
 public:
  enum Rejection {
    // The cookie is OK or not parsed yet.
    NoRejection,

    RejectedInvalidCharAttributes,
    RejectedNoneRequiresSecure,
    RejectedPartitionedRequiresSecure,
    RejectedEmptyNameAndValue,
    RejectedNameValueOversize,
    RejectedInvalidCharName,
    RejectedInvalidDomain,
    RejectedInvalidPrefix,
    RejectedInvalidCharValue,
    RejectedHttpOnlyButFromScript,
    RejectedSecureButNonHttps,
    RejectedForNonSameSiteness,
    RejectedForeignNoPartitionedError,
    RejectedByPermissionManager,
    RejectedNonsecureOverSecure,
  };

  CookieParser(nsIConsoleReportCollector* aCRC, nsIURI* aHostURI);
  ~CookieParser();

  nsIURI* HostURI() const { return mHostURI; }

  void Parse(const nsACString& aBaseDomain, bool aRequireHostMatch,
             CookieStatus aStatus, nsCString& aCookieHeader,
             const nsACString& aDateHeader, bool aFromHttp,
             bool aIsForeignAndNotAddon, bool aPartitionedOnly,
             bool aIsInPrivateBrowsing);

  bool ContainsCookie() const {
    MOZ_ASSERT_IF(mContainsCookie, mRejection == NoRejection);
    return mContainsCookie;
  }

  void RejectCookie(Rejection aRejection);

  CookieStruct& CookieData() {
    MOZ_ASSERT(ContainsCookie());
    return mCookieData;
  }

  void GetCookieString(nsACString& aCookieString) const;

  // Public for testing
  bool ParseMaxAgeAttribute(const nsACString& aMaxage, int64_t* aValue);

 private:
  static void GetTokenValue(nsACString::const_char_iterator& aIter,
                            nsACString::const_char_iterator& aEndIter,
                            nsDependentCSubstring& aTokenString,
                            nsDependentCSubstring& aTokenValue,
                            bool& aEqualsFound);

  void ParseAttributes(nsCString& aCookieHeader, nsACString& aExpires,
                       nsACString& aMaxage, bool& aAcceptedByParser);

  bool GetExpiry(CookieStruct& aCookieData, const nsACString& aExpires,
                 const nsACString& aMaxage, int64_t aCurrentTime,
                 const nsACString& aDateHeader, bool aFromHttp);

  bool CheckPath();
  bool CheckAttributeSize(const nsACString& currentValue,
                          const char* aAttribute, const nsACString& aValue);

  static bool CheckPrefixes(CookieStruct& aCookieData, bool aSecureRequest);
  static bool CheckDomain(CookieStruct& aCookieData, nsIURI* aHostURI,
                          const nsACString& aBaseDomain,
                          bool aRequireHostMatch);
  static bool HasSecurePrefix(const nsACString& aString);
  static bool HasHostPrefix(const nsACString& aString);

  nsCOMPtr<nsIConsoleReportCollector> mCRC;
  nsCOMPtr<nsIURI> mHostURI;

  // True if the parsing succeeded.
  bool mContainsCookie = false;

  Rejection mRejection = NoRejection;

  struct Warnings {
    nsTArray<const char*> mAttributeOversize;
    nsTArray<const char*> mAttributeOverwritten;

    bool mInvalidSameSiteAttribute = false;
    bool mInvalidMaxAgeAttribute = false;
    bool mSameSiteNoneRequiresSecureForBeta = false;
    bool mSameSiteLaxForced = false;
    bool mSameSiteLaxForcedForBeta = false;
    bool mForeignNoPartitionedWarning = false;
  } mWarnings;

  CookieStruct mCookieData;
  nsCString mCookieString;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_CookieParser_h
