/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThirdPartyCookieBlockingExceptions.h"

#include "mozilla/Components.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Promise-inl.h"

#include "nsIChannel.h"

namespace mozilla {
namespace net {

RefPtr<GenericNonExclusivePromise>
ThirdPartyCookieBlockingExceptions::EnsureInitialized() {
  if (mInitPromise) {
    return mInitPromise;
  }

  // Get the remote third-party cookie blocking exception list service instance.
  nsresult rv;
  m3PCBExceptionService = do_GetService(
      NS_NSITHIRDPARTYCOOKIEBLOCKINGEXCEPTIONLISTSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,
                    GenericNonExclusivePromise::CreateAndReject(rv, __func__));

  RefPtr<mozilla::dom::Promise> initPromise;
  rv = m3PCBExceptionService->Init(getter_AddRefs(initPromise));
  NS_ENSURE_SUCCESS(rv,
                    GenericNonExclusivePromise::CreateAndReject(rv, __func__));

  // Bail out earlier if we don't have a init promise.
  if (!initPromise) {
    return GenericNonExclusivePromise::CreateAndReject(rv, __func__);
  }

  mInitPromise = new GenericNonExclusivePromise::Private(__func__);

  initPromise->AddCallbacksWithCycleCollectedArgs(
      [&self = *this](JSContext*, JS::Handle<JS::Value>,
                      mozilla::ErrorResult&) {
        self.mInitPromise->Resolve(true, __func__);
      },
      [&self = *this](JSContext*, JS::Handle<JS::Value>,
                      mozilla::ErrorResult& error) {
        nsresult rv = error.StealNSResult();
        self.mInitPromise->Reject(rv, __func__);
        return;
      });

  return mInitPromise;
}

void ThirdPartyCookieBlockingExceptions::Shutdown() {
  if (m3PCBExceptionService) {
    Unused << m3PCBExceptionService->Shutdown();
    m3PCBExceptionService = nullptr;
  }

  // Reject the init promise during the shutdown.
  if (mInitPromise) {
    mInitPromise->Reject(NS_ERROR_ABORT, __func__);
    mInitPromise = nullptr;
  }
}

void ThirdPartyCookieBlockingExceptions::Insert(const nsACString& aException) {
  m3PCBExceptionsSet.Insert(aException);
}

void ThirdPartyCookieBlockingExceptions::Remove(const nsACString& aException) {
  m3PCBExceptionsSet.Remove(aException);
}

bool ThirdPartyCookieBlockingExceptions::CheckWildcardException(
    const nsACString& aThirdPartySite) {
  nsAutoCString key;
  Create3PCBExceptionKey("*"_ns, aThirdPartySite, key);

  return m3PCBExceptionsSet.Contains(key);
}

bool ThirdPartyCookieBlockingExceptions::CheckException(
    const nsACString& aFirstPartySite, const nsACString& aThirdPartySite) {
  nsAutoCString key;
  Create3PCBExceptionKey(aFirstPartySite, aThirdPartySite, key);

  return m3PCBExceptionsSet.Contains(key);
}

bool ThirdPartyCookieBlockingExceptions::CheckExceptionForURIs(
    nsIURI* aFirstPartyURI, nsIURI* aThirdPartyURI) {
  MOZ_ASSERT(XRE_IsParentProcess());
  NS_ENSURE_TRUE(aFirstPartyURI, false);
  NS_ENSURE_TRUE(aThirdPartyURI, false);

  RefPtr<nsEffectiveTLDService> eTLDService =
      nsEffectiveTLDService::GetInstance();
  NS_ENSURE_TRUE(eTLDService, false);

  nsAutoCString thirdPartySite;
  nsresult rv = eTLDService->GetSite(aThirdPartyURI, thirdPartySite);
  NS_ENSURE_SUCCESS(rv, false);

  bool isInExceptionList = CheckWildcardException(thirdPartySite);

  if (isInExceptionList) {
    return true;
  }

  nsAutoCString firstPartySite;
  rv = eTLDService->GetSite(aFirstPartyURI, firstPartySite);
  NS_ENSURE_SUCCESS(rv, false);

  return CheckException(firstPartySite, thirdPartySite);
}

bool ThirdPartyCookieBlockingExceptions::CheckExceptionForChannel(
    nsIChannel* aChannel) {
  MOZ_ASSERT(XRE_IsParentProcess());
  NS_ENSURE_TRUE(aChannel, false);

  RefPtr<nsEffectiveTLDService> eTLDService =
      nsEffectiveTLDService::GetInstance();
  NS_ENSURE_TRUE(eTLDService, false);

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aChannel->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, false);

  nsAutoCString thirdPartySite;
  rv = eTLDService->GetSite(uri, thirdPartySite);
  NS_ENSURE_SUCCESS(rv, false);

  bool isInExceptionList = CheckWildcardException(thirdPartySite);

  if (isInExceptionList) {
    return true;
  }

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();

  RefPtr<BrowsingContext> bc;
  loadInfo->GetBrowsingContext(getter_AddRefs(bc));
  if (!bc) {
    bc = loadInfo->GetWorkerAssociatedBrowsingContext();
  }

  nsAutoCString firstPartySite;

  // If the channel is not associated with a browsing context, we will try to
  // get the first party site from the partition key.
  if (!bc) {
    nsCOMPtr<nsICookieJarSettings> cjs;
    nsresult rv = loadInfo->GetCookieJarSettings(getter_AddRefs(cjs));
    NS_ENSURE_SUCCESS(rv, false);

    nsAutoString partitionKey;
    rv = cjs->GetPartitionKey(partitionKey);
    NS_ENSURE_SUCCESS(rv, false);

    nsAutoString site;
    if (!OriginAttributes::ExtractSiteFromPartitionKey(partitionKey, site)) {
      return false;
    }

    firstPartySite.Assign(NS_ConvertUTF16toUTF8(site));
  } else {
    RefPtr<WindowGlobalParent> topWGP =
        bc->Top()->Canonical()->GetCurrentWindowGlobal();
    if (!topWGP) {
      return false;
    }

    nsCOMPtr<nsIPrincipal> topPrincipal = topWGP->DocumentPrincipal();

    // If the top window is an about page, we don't need to do anything. This
    // could happen when fetching system resources, such as pocket's images
    if (topPrincipal->SchemeIs("about")) {
      return false;
    }

    nsCOMPtr<nsIURI> topURI = topPrincipal->GetURI();

    nsAutoCString site;
    nsresult rv = eTLDService->GetSite(topURI, firstPartySite);
    NS_ENSURE_SUCCESS(rv, false);
  }

  return CheckException(firstPartySite, thirdPartySite);
}

void ThirdPartyCookieBlockingExceptions::GetExceptions(
    nsTArray<nsCString>& aExceptions) {
  aExceptions.Clear();

  for (const auto& host : m3PCBExceptionsSet) {
    aExceptions.AppendElement(host);
  }
}

}  // namespace net
}  // namespace mozilla
