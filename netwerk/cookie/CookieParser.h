/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CookieParser_h
#define mozilla_net_CookieParser_h

#include <nsCOMPtr.h>

class nsIConsoleReportCollector;
class nsIURI;

namespace mozilla {
namespace net {

class CookieParser final {
 public:
  static bool CanSetCookie(nsIURI* aHostURI, const nsACString& aBaseDomain,
                           CookieStruct& aCookieData, bool aRequireHostMatch,
                           CookieStatus aStatus, nsCString& aCookieHeader,
                           bool aFromHttp, bool aIsForeignAndNotAddon,
                           bool aPartitionedOnly, bool aIsInPrivateBrowsing,
                           nsIConsoleReportCollector* aCRC, bool& aSetCookie);

 private:
  static bool GetTokenValue(nsACString::const_char_iterator& aIter,
                            nsACString::const_char_iterator& aEndIter,
                            nsDependentCSubstring& aTokenString,
                            nsDependentCSubstring& aTokenValue,
                            bool& aEqualsFound);

  static bool ParseAttributes(nsIConsoleReportCollector* aCRC, nsIURI* aHostURI,
                              nsCString& aCookieHeader,
                              CookieStruct& aCookieData, nsACString& aExpires,
                              nsACString& aMaxage, bool& aAcceptedByParser);

  static bool GetExpiry(CookieStruct& aCookieData, const nsACString& aExpires,
                        const nsACString& aMaxage, int64_t aCurrentTime,
                        bool aFromHttp);

  static bool CheckPath(CookieStruct& aCookieData,
                        nsIConsoleReportCollector* aCRC, nsIURI* aHostURI);
  static bool CheckPrefixes(CookieStruct& aCookieData, bool aSecureRequest);
  static bool CheckDomain(CookieStruct& aCookieData, nsIURI* aHostURI,
                          const nsACString& aBaseDomain,
                          bool aRequireHostMatch);
  static bool HasSecurePrefix(const nsACString& aString);
  static bool HasHostPrefix(const nsACString& aString);
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_CookieParser_h
