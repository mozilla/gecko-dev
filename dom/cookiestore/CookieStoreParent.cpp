/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CookieStoreParent.h"
#include "CookieStoreNotificationWatcher.h"
#include "CookieStoreSubscriptionService.h"

#include "mozilla/Maybe.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/net/Cookie.h"
#include "mozilla/net/CookieParser.h"
#include "mozilla/Components.h"
#include "mozilla/net/CookieCommons.h"
#include "mozilla/net/CookieValidation.h"
#include "mozilla/net/CookieServiceParent.h"
#include "mozilla/net/NeckoParent.h"
#include "mozilla/Unused.h"
#include "nsICookieManager.h"
#include "nsICookieService.h"
#include "nsProxyRelease.h"
#include "mozilla/ipc/URIUtils.h"  // for IPDLParamTraits<nsIURI*>
#include "nsIEffectiveTLDService.h"

using namespace mozilla::ipc;
using namespace mozilla::net;

namespace mozilla::dom {

namespace {

bool CheckContentProcessSecurity(ThreadsafeContentParentHandle* aParent,
                                 const nsACString& aDomain,
                                 const OriginAttributes& aOriginAttributes) {
  AssertIsOnMainThread();

  // ContentParent is null if we are dealing with the same process.
  if (!aParent) {
    return true;
  }

  RefPtr<ContentParent> contentParent = aParent->GetContentParent();
  if (!contentParent) {
    return true;
  }

  PNeckoParent* neckoParent =
      LoneManagedOrNullAsserts(contentParent->ManagedPNeckoParent());
  if (!neckoParent) {
    return true;
  }

  PCookieServiceParent* csParent =
      LoneManagedOrNullAsserts(neckoParent->ManagedPCookieServiceParent());
  if (!csParent) {
    return true;
  }

  auto* cs = static_cast<CookieServiceParent*>(csParent);

  return cs->ContentProcessHasCookie(aDomain, aOriginAttributes);
}

}  // namespace

CookieStoreParent::CookieStoreParent() { AssertIsOnBackgroundThread(); }

CookieStoreParent::~CookieStoreParent() {
  AssertIsOnBackgroundThread();
  CookieStoreNotificationWatcher::ReleaseOnMainThread(
      mNotificationWatcherOnMainThread.forget());
}

mozilla::ipc::IPCResult CookieStoreParent::RecvGetRequest(
    nsIURI* aCookieURI, const OriginAttributes& aOriginAttributes,
    const Maybe<OriginAttributes>& aPartitionedOriginAttributes,
    const bool& aThirdPartyContext, const bool& aPartitionForeign,
    const bool& aUsingStorageAccess, const bool& aIsOn3PCBExceptionList,
    const bool& aMatchName, const nsString& aName, const nsCString& aPath,
    const bool& aOnlyFirstMatch, GetRequestResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  InvokeAsync(GetMainThreadSerialEventTarget(), __func__,
              [self = RefPtr(this), uri = RefPtr{aCookieURI}, aOriginAttributes,
               aPartitionedOriginAttributes, aThirdPartyContext,
               aPartitionForeign, aUsingStorageAccess, aIsOn3PCBExceptionList,
               aMatchName, aName, aPath, aOnlyFirstMatch]() {
                CopyableTArray<CookieStruct> results;
                self->GetRequestOnMainThread(
                    uri, aOriginAttributes, aPartitionedOriginAttributes,
                    aThirdPartyContext, aPartitionForeign, aUsingStorageAccess,
                    aIsOn3PCBExceptionList, aMatchName, aName, aPath,
                    aOnlyFirstMatch, results);
                return GetRequestPromise::CreateAndResolve(std::move(results),
                                                           __func__);
              })
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [aResolver = std::move(aResolver)](
                 const GetRequestPromise::ResolveOrRejectValue& aResult) {
               MOZ_ASSERT(aResult.IsResolve());
               aResolver(aResult.ResolveValue());
             });

  return IPC_OK();
}

mozilla::ipc::IPCResult CookieStoreParent::RecvSetRequest(
    nsIURI* aCookieURI, const OriginAttributes& aOriginAttributes,
    const bool& aThirdPartyContext, const bool& aPartitionForeign,
    const bool& aUsingStorageAccess, const bool& aIsOn3PCBExceptionList,
    const nsString& aName, const nsString& aValue, const bool& aSession,
    const int64_t& aExpires, const nsString& aDomain, const nsString& aPath,
    const int32_t& aSameSite, const bool& aPartitioned,
    const nsID& aOperationID, SetRequestResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  RefPtr<ThreadsafeContentParentHandle> parent =
      BackgroundParent::GetContentParentHandle(Manager());

  InvokeAsync(
      GetMainThreadSerialEventTarget(), __func__,
      [self = RefPtr(this), parent = RefPtr(parent), uri = RefPtr{aCookieURI},
       aDomain, aOriginAttributes, aThirdPartyContext, aPartitionForeign,
       aUsingStorageAccess, aIsOn3PCBExceptionList, aName, aValue, aSession,
       aExpires, aPath, aSameSite, aPartitioned, aOperationID]() {
        bool waitForNotification = self->SetRequestOnMainThread(
            parent, uri, aDomain, aOriginAttributes, aThirdPartyContext,
            aPartitionForeign, aUsingStorageAccess, aIsOn3PCBExceptionList,
            aName, aValue, aSession, aExpires, aPath, aSameSite, aPartitioned,
            aOperationID);
        return SetDeleteRequestPromise::CreateAndResolve(waitForNotification,
                                                         __func__);
      })
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [aResolver = std::move(aResolver)](
                 const SetDeleteRequestPromise::ResolveOrRejectValue& aResult) {
               MOZ_ASSERT(aResult.IsResolve());
               aResolver(aResult.ResolveValue());
             });

  return IPC_OK();
}

mozilla::ipc::IPCResult CookieStoreParent::RecvDeleteRequest(
    nsIURI* aCookieURI, const OriginAttributes& aOriginAttributes,
    const bool& aThirdPartyContext, const bool& aPartitionForeign,
    const bool& aUsingStorageAccess, const bool& aIsOn3PCBExceptionList,
    const nsString& aName, const nsString& aDomain, const nsString& aPath,
    const bool& aPartitioned, const nsID& aOperationID,
    DeleteRequestResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  RefPtr<ThreadsafeContentParentHandle> parent =
      BackgroundParent::GetContentParentHandle(Manager());

  InvokeAsync(
      GetMainThreadSerialEventTarget(), __func__,
      [self = RefPtr(this), parent = RefPtr(parent), uri = RefPtr{aCookieURI},
       aDomain, aOriginAttributes, aThirdPartyContext, aPartitionForeign,
       aUsingStorageAccess, aIsOn3PCBExceptionList, aName, aPath, aPartitioned,
       aOperationID]() {
        bool waitForNotification = self->DeleteRequestOnMainThread(
            parent, uri, aDomain, aOriginAttributes, aThirdPartyContext,
            aPartitionForeign, aUsingStorageAccess, aIsOn3PCBExceptionList,
            aName, aPath, aPartitioned, aOperationID);
        return SetDeleteRequestPromise::CreateAndResolve(waitForNotification,
                                                         __func__);
      })
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [aResolver = std::move(aResolver)](
                 const SetDeleteRequestPromise::ResolveOrRejectValue& aResult) {
               MOZ_ASSERT(aResult.IsResolve());
               aResolver(aResult.ResolveValue());
             });
  return IPC_OK();
}

mozilla::ipc::IPCResult CookieStoreParent::RecvGetSubscriptionsRequest(
    const PrincipalInfo& aPrincipalInfo, const nsCString& aScopeURL,
    GetSubscriptionsRequestResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  InvokeAsync(GetMainThreadSerialEventTarget(), __func__,
              [self = RefPtr(this), aPrincipalInfo, aScopeURL]() {
                CookieStoreSubscriptionService* service =
                    CookieStoreSubscriptionService::Instance();
                if (!service) {
                  return GetSubscriptionsRequestPromise::CreateAndReject(
                      NS_ERROR_FAILURE, __func__);
                }

                nsTArray<CookieSubscription> subscriptions;
                service->GetSubscriptions(aPrincipalInfo, aScopeURL,
                                          subscriptions);

                return GetSubscriptionsRequestPromise::CreateAndResolve(
                    std::move(subscriptions), __func__);
              })
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [aResolver = std::move(aResolver)](
                 const GetSubscriptionsRequestPromise::ResolveOrRejectValue&
                     aResult) {
               if (aResult.IsResolve()) {
                 aResolver(aResult.ResolveValue());
                 return;
               }

               aResolver(nsTArray<CookieSubscription>());
             });

  return IPC_OK();
}

mozilla::ipc::IPCResult CookieStoreParent::RecvSubscribeOrUnsubscribeRequest(
    const PrincipalInfo& aPrincipalInfo, const nsCString& aScopeURL,
    const CopyableTArray<CookieSubscription>& aSubscriptions,
    bool aSubscription, SubscribeOrUnsubscribeRequestResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  InvokeAsync(GetMainThreadSerialEventTarget(), __func__,
              [self = RefPtr(this), aPrincipalInfo, aScopeURL, aSubscriptions,
               aSubscription]() {
                CookieStoreSubscriptionService* service =
                    CookieStoreSubscriptionService::Instance();
                if (!service) {
                  return SubscribeOrUnsubscribeRequestPromise::CreateAndReject(
                      NS_ERROR_FAILURE, __func__);
                }

                if (aSubscription) {
                  service->Subscribe(aPrincipalInfo, aScopeURL, aSubscriptions);
                } else {
                  service->Unsubscribe(aPrincipalInfo, aScopeURL,
                                       aSubscriptions);
                }

                return SubscribeOrUnsubscribeRequestPromise::CreateAndResolve(
                    true, __func__);
              })
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [aResolver = std::move(aResolver)](
              const SubscribeOrUnsubscribeRequestPromise::ResolveOrRejectValue&
                  aResult) { aResolver(aResult.IsResolve()); });

  return IPC_OK();
}

mozilla::ipc::IPCResult CookieStoreParent::RecvClose() {
  AssertIsOnBackgroundThread();

  Unused << Send__delete__(this);
  return IPC_OK();
}

namespace util {

bool HasHostPrefix(const nsAString& aCookieName) {
  return StringBeginsWith(aCookieName, u"__Host-"_ns,
                          nsCaseInsensitiveStringComparator);
}

}  // namespace util

void CookieStoreParent::GetRequestOnMainThread(
    nsIURI* aCookieURI, const OriginAttributes& aOriginAttributes,
    const Maybe<OriginAttributes>& aPartitionedOriginAttributes,
    bool aThirdPartyContext, bool aPartitionForeign, bool aUsingStorageAccess,
    bool aIsOn3PCBExceptionList, bool aMatchName, const nsAString& aName,
    const nsACString& aPath, bool aOnlyFirstMatch,
    nsTArray<CookieStruct>& aResults) {
  nsresult rv;
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsICookieService> service =
      do_GetService(NS_COOKIESERVICE_CONTRACTID);
  if (!service) {
    return;
  }

  nsAutoCString baseDomain;
  nsCOMPtr<nsIEffectiveTLDService> etld =
      mozilla::components::EffectiveTLD::Service();
  bool requireMatch = false;
  rv = CookieCommons::GetBaseDomain(etld, aCookieURI, baseDomain, requireMatch);
  if (NS_FAILED(rv)) {
    return;
  }

  nsAutoCString hostName;
  rv = nsContentUtils::GetHostOrIPv6WithBrackets(aCookieURI, hostName);
  if (NS_FAILED(rv)) {
    return;
  }

  NS_ConvertUTF16toUTF8 matchName(aName);

  nsTArray<OriginAttributes> attrsList;
  attrsList.AppendElement(aOriginAttributes);

  if (aPartitionedOriginAttributes) {
    attrsList.AppendElement(aPartitionedOriginAttributes.value());
  }

  nsTArray<CookieStruct> list;

  for (const OriginAttributes& attrs : attrsList) {
    nsTArray<RefPtr<Cookie>> cookies;
    service->GetCookiesFromHost(baseDomain, attrs, cookies);

    for (Cookie* cookie : cookies) {
      if (!CookieCommons::DomainMatches(cookie, hostName)) {
        continue;
      }
      if (cookie->IsHttpOnly()) {
        continue;
      }

      if (aThirdPartyContext &&
          !CookieCommons::ShouldIncludeCrossSiteCookie(
              cookie, aCookieURI, aPartitionForeign, attrs.IsPrivateBrowsing(),
              aUsingStorageAccess, aIsOn3PCBExceptionList)) {
        continue;
      }

      if (aMatchName && !matchName.Equals(cookie->Name())) {
        continue;
      }

      if (!net::CookieCommons::PathMatches(cookie->Path(), aPath)) {
        continue;
      }

      list.AppendElement(cookie->ToIPC());

      if (aOnlyFirstMatch) {
        break;
      }
    }

    if (!list.IsEmpty() && aOnlyFirstMatch) {
      break;
    }
  }

  aResults.SwapElements(list);
}

bool CookieStoreParent::SetRequestOnMainThread(
    ThreadsafeContentParentHandle* aParent, nsIURI* aCookieURI,
    const nsAString& aDomain, const OriginAttributes& aOriginAttributes,
    bool aThirdPartyContext, bool aPartitionForeign, bool aUsingStorageAccess,
    bool aIsOn3PCBExceptionList, const nsAString& aName,
    const nsAString& aValue, bool aSession, int64_t aExpires,
    const nsAString& aPath, int32_t aSameSite, bool aPartitioned,
    const nsID& aOperationID) {
  MOZ_ASSERT(NS_IsMainThread());
  nsresult rv;

  bool requireMatch = false;
  NS_ConvertUTF16toUTF8 domain(aDomain);
  nsAutoCString domainWithDot;

  if (util::HasHostPrefix(aName) && !domain.IsEmpty()) {
    MOZ_DIAGNOSTIC_CRASH("This should not be allowed by CookieStore");
    return false;
  }

  // If aDomain is `domain.com` then domainWithDot will be `.domain.com`
  // Otherwise, when aDomain is empty, domain and domainWithDot will both
  // be the host of aCookieURI
  if (!domain.IsEmpty()) {
    MOZ_ASSERT(!domain.IsEmpty());
    domainWithDot.Insert('.', 0);
  } else {
    domain.Truncate();
    rv = nsContentUtils::GetHostOrIPv6WithBrackets(aCookieURI, domain);
    if (NS_FAILED(rv)) {
      return false;
    }
    requireMatch = true;
  }
  domainWithDot.Append(domain);

  if (!CheckContentProcessSecurity(aParent, domain, aOriginAttributes)) {
    return false;
  }

  if (aThirdPartyContext &&
      !CookieCommons::ShouldIncludeCrossSiteCookie(
          aCookieURI, aSameSite,
          aPartitioned && !aOriginAttributes.mPartitionKey.IsEmpty(),
          aPartitionForeign, aOriginAttributes.IsPrivateBrowsing(),
          aUsingStorageAccess, aIsOn3PCBExceptionList)) {
    return false;
  }

  nsCOMPtr<nsICookieManager> service =
      do_GetService(NS_COOKIEMANAGER_CONTRACTID);
  if (!service) {
    return false;
  }

  bool notified = false;
  auto notificationCb = [&]() { notified = true; };

  CookieStoreNotificationWatcher* notificationWatcher =
      GetOrCreateNotificationWatcherOnMainThread(aOriginAttributes);
  if (!notificationWatcher) {
    return false;
  }

  notificationWatcher->CallbackWhenNotified(aOperationID, notificationCb);

  OriginAttributes attrs(aOriginAttributes);
  rv = service->AddNative(
      aCookieURI, domainWithDot, NS_ConvertUTF16toUTF8(aPath),
      NS_ConvertUTF16toUTF8(aName), NS_ConvertUTF16toUTF8(aValue),
      /* secure: */ true,
      /* http-only: */ false, aSession, aSession ? INT64_MAX : aExpires, &attrs,
      aSameSite, nsICookie::SCHEME_HTTPS, aPartitioned, /* from http: */ false,
      &aOperationID, [&](mozilla::net::CookieStruct& aCookieStruct) -> bool {
        AssertIsOnMainThread();

        RefPtr<CookieValidation> validation = CookieValidation::ValidateForHost(
            aCookieStruct, aCookieURI, domain, requireMatch, false);
        MOZ_ASSERT(validation);

        if (validation->Result() == nsICookieValidation::eOK) {
          return true;
        }

        RefPtr<ContentParent> contentParent = aParent->GetContentParent();
        if (!contentParent) {
          return false;
        }

        contentParent->KillHard(
            "CookieStore does not accept invalid cookies in the parent "
            "process");
        return false;
      });

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  notificationWatcher->ForgetOperationID(aOperationID);

  return notified;
}

bool CookieStoreParent::DeleteRequestOnMainThread(
    ThreadsafeContentParentHandle* aParent, nsIURI* aCookieURI,
    const nsAString& aDomain, const OriginAttributes& aOriginAttributes,
    bool aThirdPartyContext, bool aPartitionForeign, bool aUsingStorageAccess,
    bool aIsOn3PCBExceptionList, const nsAString& aName, const nsAString& aPath,
    bool aPartitioned, const nsID& aOperationID) {
  MOZ_ASSERT(NS_IsMainThread());
  nsresult rv;

  nsAutoCString baseDomain;
  nsCOMPtr<nsIEffectiveTLDService> etld =
      mozilla::components::EffectiveTLD::Service();
  bool requireMatch = false;
  rv = CookieCommons::GetBaseDomain(etld, aCookieURI, baseDomain, requireMatch);
  if (NS_FAILED(rv)) {
    return false;
  }

  nsAutoCString hostName;
  nsContentUtils::GetHostOrIPv6WithBrackets(aCookieURI, hostName);

  nsAutoCString cookiesForDomain;
  if (aDomain.IsEmpty()) {
    cookiesForDomain = hostName;
  } else {
    cookiesForDomain = NS_ConvertUTF16toUTF8(aDomain);
  }

  if (!CheckContentProcessSecurity(aParent, cookiesForDomain,
                                   aOriginAttributes)) {
    return false;
  }

  nsCOMPtr<nsICookieService> service =
      do_GetService(NS_COOKIESERVICE_CONTRACTID);
  if (!service) {
    return false;
  }
  nsCOMPtr<nsICookieManager> cookieManager = do_QueryInterface(service);

  NS_ConvertUTF16toUTF8 matchName(aName);
  NS_ConvertUTF16toUTF8 matchPath(aPath);

  nsTArray<RefPtr<Cookie>> cookies;
  OriginAttributes attrs(aOriginAttributes);
  service->GetCookiesFromHost(baseDomain, attrs, cookies);

  for (Cookie* cookie : cookies) {
    MOZ_ASSERT(cookie);
    if (!matchName.Equals(cookie->Name())) {
      continue;
    }
    if (!CookieCommons::DomainMatches(cookie, cookiesForDomain)) {
      continue;
    }

    if (!matchPath.IsEmpty() && !matchPath.Equals(cookie->Path())) {
      continue;
    }

    if (cookie->IsPartitioned() != aPartitioned) continue;

    if (aThirdPartyContext) {
      int32_t sameSiteAttr = cookie->SameSite();

      if (!CookieCommons::ShouldIncludeCrossSiteCookie(
              aCookieURI, sameSiteAttr,
              aPartitioned && !aOriginAttributes.mPartitionKey.IsEmpty(),
              aPartitionForeign, attrs.IsPrivateBrowsing(), aUsingStorageAccess,
              aIsOn3PCBExceptionList)) {
        return false;
      }
    }

    bool notified = false;
    auto notificationCb = [&]() { notified = true; };

    CookieStoreNotificationWatcher* notificationWatcher =
        GetOrCreateNotificationWatcherOnMainThread(aOriginAttributes);
    if (!notificationWatcher) {
      return false;
    }

    notificationWatcher->CallbackWhenNotified(aOperationID, notificationCb);

    rv = cookieManager->RemoveNative(cookie->Host(), matchName, cookie->Path(),
                                     &attrs, /* from http: */ false,
                                     &aOperationID);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }

    notificationWatcher->ForgetOperationID(aOperationID);

    return notified;
  }

  return false;
}

CookieStoreNotificationWatcher*
CookieStoreParent::GetOrCreateNotificationWatcherOnMainThread(
    const OriginAttributes& aOriginAttributes) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mNotificationWatcherOnMainThread) {
    mNotificationWatcherOnMainThread = CookieStoreNotificationWatcher::Create(
        aOriginAttributes.IsPrivateBrowsing());
  }

  return mNotificationWatcherOnMainThread;
}

}  // namespace mozilla::dom
