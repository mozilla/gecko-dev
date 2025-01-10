/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThirdPartyCookieBlockingExceptions.h"

#include "mozilla/Components.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Promise-inl.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/Logging.h"

#include "nsIChannel.h"

namespace mozilla {
namespace net {

LazyLogModule g3PCBExceptionLog("3pcbexception");

void ThirdPartyCookieBlockingExceptions::Initialize() {
  if (mIsInitialized) {
    return;
  }

  // Get the remote third-party cookie blocking exception list service instance.
  nsresult rv;
  m3PCBExceptionService = do_GetService(
      NS_NSITHIRDPARTYCOOKIEBLOCKINGEXCEPTIONLISTSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS_VOID(rv);

  RefPtr<mozilla::dom::Promise> initPromise;
  rv = m3PCBExceptionService->Init(getter_AddRefs(initPromise));
  NS_ENSURE_SUCCESS_VOID(rv);

  // Bail out earlier if we don't have a init promise.
  if (!initPromise) {
    MOZ_LOG(g3PCBExceptionLog, LogLevel::Error,
            ("Failed to initialize 3PCB exception service: no init promise"));
    return;
  }

  initPromise->AddCallbacksWithCycleCollectedArgs(
      [&self = *this](JSContext*, JS::Handle<JS::Value>,
                      mozilla::ErrorResult&) { self.mIsInitialized = true; },
      [](JSContext*, JS::Handle<JS::Value>, mozilla::ErrorResult& error) {
        nsresult rv = error.StealNSResult();

        nsAutoCString name;
        GetErrorName(rv, name);

        MOZ_LOG(
            g3PCBExceptionLog, LogLevel::Error,
            ("Failed to initialize 3PCB exception service: %s", name.get()));
      });
}

void ThirdPartyCookieBlockingExceptions::Shutdown() {
  if (m3PCBExceptionService) {
    Unused << m3PCBExceptionService->Shutdown();
    m3PCBExceptionService = nullptr;
  }

  mIsInitialized = false;
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

  if (!mIsInitialized) {
    return false;
  }

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

  if (!mIsInitialized) {
    return false;
  }

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

  RefPtr<dom::BrowsingContext> bc;
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
    RefPtr<dom::WindowGlobalParent> topWGP =
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
