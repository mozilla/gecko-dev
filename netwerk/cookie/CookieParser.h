/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CookieParser_h
#define mozilla_net_CookieParser_h

#include "CookieCommons.h"

#include "CookieValidation.h"
#include "mozilla/net/NeckoChannelParams.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"

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
    RejectedHttpOnlyButFromScript,
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
             bool aIsInPrivateBrowsing, bool aOn3pcbException);

  bool ContainsCookie() const {
    return mValidation && mValidation->Result() == nsICookieValidation::eOK;
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

  static bool CheckAttributeSize(const nsACString& currentValue,
                                 const char* aAttribute,
                                 const nsACString& aValue,
                                 CookieParser* aParser = nullptr);
  static void FixPath(CookieStruct& aCookieData, nsIURI* aHostURI);
  static void FixDomain(CookieStruct& aCookieData, nsIURI* aHostURI,
                        const nsACString& aBaseDomain, bool aRequireHostMatch);

  nsCOMPtr<nsIConsoleReportCollector> mCRC;
  nsCOMPtr<nsIURI> mHostURI;

  Rejection mRejection = NoRejection;
  RefPtr<CookieValidation> mValidation;

  struct Warnings {
    nsTArray<const char*> mAttributeOversize;
    nsTArray<const char*> mAttributeOverwritten;

    bool mInvalidSameSiteAttribute = false;
    bool mInvalidMaxAgeAttribute = false;
    bool mForeignNoPartitionedWarning = false;
  } mWarnings;

  CookieStruct mCookieData;
  nsCString mCookieString;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_CookieParser_h
