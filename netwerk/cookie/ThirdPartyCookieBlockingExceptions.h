/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_ThirdPartyCookieBlockingExceptions_h
#define mozilla_net_ThirdPartyCookieBlockingExceptions_h

#include "mozilla/MozPromise.h"
#include "nsEffectiveTLDService.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsTHashSet.h"
#include "nsIThirdPartyCookieBlockingExceptionListService.h"

class nsIEffectiveTLDService;
class nsIURI;
class nsIChannel;

namespace mozilla {
namespace net {

class ThirdPartyCookieBlockingExceptions final {
 public:
  // Initializes the foreign cookie blocking exception list.
  void Initialize();

  // Check if the given top-level and third-party URIs are in the exception
  // list.
  bool CheckExceptionForURIs(nsIURI* aFirstPartyURI, nsIURI* aThirdPartyURI);

  // Check if the given channel is in the exception list.
  bool CheckExceptionForChannel(nsIChannel* aChannel);

  void Insert(const nsACString& aException);
  void Remove(const nsACString& aException);

  void GetExceptions(nsTArray<nsCString>& aExceptions);

  void Shutdown();

  bool IsInitialized() const { return mIsInitialized; }

 private:
  nsCOMPtr<nsIThirdPartyCookieBlockingExceptionListService>
      m3PCBExceptionService;

  // A helper function to create a key for the exception list.
  static void Create3PCBExceptionKey(const nsACString& aFirstPartySite,
                                     const nsACString& aThirdPartySite,
                                     nsACString& aKey) {
    aKey.Truncate();
    aKey.Append(aFirstPartySite);
    aKey.AppendLiteral(",");
    aKey.Append(aThirdPartySite);
  }

  // Check if the given third-party site is in the wildcard exception list.
  // The wildcard exception list is used to allow third-party cookies under
  // every top-level site.
  bool CheckWildcardException(const nsACString& aThirdPartySite);

  // Check if the given first-party and third-party sites are in the exception
  // list.
  bool CheckException(const nsACString& aFirstPartySite,
                      const nsACString& aThirdPartySite);

  // The flag that indicates if the 3PCB exception service is initialized.
  bool mIsInitialized = false;
  nsTHashSet<nsCStringHashKey> m3PCBExceptionsSet;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_ThirdPartyCookieBlockingExceptions_h
