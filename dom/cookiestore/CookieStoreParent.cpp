/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CookieStoreParent.h"
#include "CookieStoreNotificationWatcher.h"

#include "mozilla/Maybe.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/net/Cookie.h"
#include "mozilla/net/CookieCommons.h"
#include "mozilla/net/CookieServiceParent.h"
#include "mozilla/net/NeckoParent.h"
#include "mozilla/Unused.h"
#include "nsICookieManager.h"
#include "nsICookieService.h"
#include "nsProxyRelease.h"

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
    const nsString& aDomain, const OriginAttributes& aOriginAttributes,
    const Maybe<OriginAttributes>& aPartitionedOriginAttributes,
    const bool& aThirdPartyContext, const bool& aPartitionForeign,
    const bool& aUsingStorageAccess, const bool& aMatchName,
    const nsString& aName, const nsCString& aPath, const bool& aOnlyFirstMatch,
    GetRequestResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  InvokeAsync(
      GetMainThreadSerialEventTarget(), __func__,
      [self = RefPtr(this), aDomain, aOriginAttributes,
       aPartitionedOriginAttributes, aThirdPartyContext, aPartitionForeign,
       aUsingStorageAccess, aMatchName, aName, aPath, aOnlyFirstMatch]() {
        CopyableTArray<CookieData> results;
        self->GetRequestOnMainThread(
            aDomain, aOriginAttributes, aPartitionedOriginAttributes,
            aThirdPartyContext, aPartitionForeign, aUsingStorageAccess,
            aMatchName, aName, aPath, aOnlyFirstMatch, results);
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
    const nsString& aDomain, const OriginAttributes& aOriginAttributes,
    const bool& aThirdPartyContext, const bool& aPartitionForeign,
    const bool& aUsingStorageAccess, const nsString& aName,
    const nsString& aValue, const bool& aSession, const int64_t& aExpires,
    const nsString& aPath, const int32_t& aSameSite, const bool& aPartitioned,
    const nsID& aOperationID, SetRequestResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  RefPtr<ThreadsafeContentParentHandle> parent =
      BackgroundParent::GetContentParentHandle(Manager());

  InvokeAsync(GetMainThreadSerialEventTarget(), __func__,
              [self = RefPtr(this), parent = RefPtr(parent), aDomain,
               aOriginAttributes, aThirdPartyContext, aPartitionForeign,
               aUsingStorageAccess, aName, aValue, aSession, aExpires, aPath,
               aSameSite, aPartitioned, aOperationID]() {
                bool waitForNotification = self->SetRequestOnMainThread(
                    parent, aDomain, aOriginAttributes, aThirdPartyContext,
                    aPartitionForeign, aUsingStorageAccess, aName, aValue,
                    aSession, aExpires, aPath, aSameSite, aPartitioned,
                    aOperationID);
                return SetDeleteRequestPromise::CreateAndResolve(
                    waitForNotification, __func__);
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
    const nsString& aDomain, const OriginAttributes& aOriginAttributes,
    const bool& aThirdPartyContext, const bool& aPartitionForeign,
    const bool& aUsingStorageAccess, const nsString& aName,
    const nsString& aPath, const bool& aPartitioned, const nsID& aOperationID,
    DeleteRequestResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  RefPtr<ThreadsafeContentParentHandle> parent =
      BackgroundParent::GetContentParentHandle(Manager());

  InvokeAsync(
      GetMainThreadSerialEventTarget(), __func__,
      [self = RefPtr(this), parent = RefPtr(parent), aDomain, aOriginAttributes,
       aThirdPartyContext, aPartitionForeign, aUsingStorageAccess, aName, aPath,
       aPartitioned, aOperationID]() {
        bool waitForNotification = self->DeleteRequestOnMainThread(
            parent, aDomain, aOriginAttributes, aThirdPartyContext,
            aPartitionForeign, aUsingStorageAccess, aName, aPath, aPartitioned,
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

mozilla::ipc::IPCResult CookieStoreParent::RecvClose() {
  AssertIsOnBackgroundThread();

  Unused << Send__delete__(this);
  return IPC_OK();
}

void CookieStoreParent::GetRequestOnMainThread(
    const nsAString& aDomain, const OriginAttributes& aOriginAttributes,
    const Maybe<OriginAttributes>& aPartitionedOriginAttributes,
    bool aThirdPartyContext, bool aPartitionForeign, bool aUsingStorageAccess,
    bool aMatchName, const nsAString& aName, const nsACString& aPath,
    bool aOnlyFirstMatch, nsTArray<CookieData>& aResults) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsICookieService> service =
      do_GetService(NS_COOKIESERVICE_CONTRACTID);
  if (!service) {
    return;
  }

  NS_ConvertUTF16toUTF8 matchName(aName);

  nsTArray<OriginAttributes> attrsList;
  attrsList.AppendElement(aOriginAttributes);

  if (aPartitionedOriginAttributes) {
    attrsList.AppendElement(aPartitionedOriginAttributes.value());
  }

  nsTArray<CookieData> list;

  for (const OriginAttributes& attrs : attrsList) {
    nsTArray<RefPtr<Cookie>> cookies;
    service->GetCookiesFromHost(NS_ConvertUTF16toUTF8(aDomain), attrs, cookies);

    for (Cookie* cookie : cookies) {
      if (cookie->IsHttpOnly()) {
        continue;
      }

      if (aThirdPartyContext &&
          !CookieCommons::ShouldIncludeCrossSiteCookie(
              cookie, aPartitionForeign, attrs.IsPrivateBrowsing(),
              aUsingStorageAccess)) {
        continue;
      }

      if (aMatchName && !matchName.Equals(cookie->Name())) {
        continue;
      }

      if (!net::CookieCommons::PathMatches(cookie->Path(), aPath)) {
        continue;
      }

      CookieData* data = list.AppendElement();
      data->name() = NS_ConvertUTF8toUTF16(cookie->Name());
      data->value() = NS_ConvertUTF8toUTF16(cookie->Value());

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
    ThreadsafeContentParentHandle* aParent, const nsAString& aDomain,
    const OriginAttributes& aOriginAttributes, bool aThirdPartyContext,
    bool aPartitionForeign, bool aUsingStorageAccess, const nsAString& aName,
    const nsAString& aValue, bool aSession, int64_t aExpires,
    const nsAString& aPath, int32_t aSameSite, bool aPartitioned,
    const nsID& aOperationID) {
  MOZ_ASSERT(NS_IsMainThread());

  NS_ConvertUTF16toUTF8 domain(aDomain);

  if (!CheckContentProcessSecurity(aParent, domain, aOriginAttributes)) {
    return false;
  }

  if (aThirdPartyContext &&
      !CookieCommons::ShouldIncludeCrossSiteCookie(
          aSameSite, aPartitioned && !aOriginAttributes.mPartitionKey.IsEmpty(),
          aPartitionForeign, aOriginAttributes.IsPrivateBrowsing(),
          aUsingStorageAccess)) {
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
  nsresult rv = service->AddNative(
      domain, NS_ConvertUTF16toUTF8(aPath), NS_ConvertUTF16toUTF8(aName),
      NS_ConvertUTF16toUTF8(aValue),
      true,   //  secure
      false,  // mHttpOnly,
      aSession, aSession ? PR_Now() : aExpires, &attrs, aSameSite,
      nsICookie::SCHEME_HTTPS, aPartitioned, &aOperationID);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  notificationWatcher->ForgetOperationID(aOperationID);

  return notified;
}

bool CookieStoreParent::DeleteRequestOnMainThread(
    ThreadsafeContentParentHandle* aParent, const nsAString& aDomain,
    const OriginAttributes& aOriginAttributes, bool aThirdPartyContext,
    bool aPartitionForeign, bool aUsingStorageAccess, const nsAString& aName,
    const nsAString& aPath, bool aPartitioned, const nsID& aOperationID) {
  MOZ_ASSERT(NS_IsMainThread());

  NS_ConvertUTF16toUTF8 domain(aDomain);

  if (!CheckContentProcessSecurity(aParent, domain, aOriginAttributes)) {
    return false;
  }

  nsCOMPtr<nsICookieManager> service =
      do_GetService(NS_COOKIEMANAGER_CONTRACTID);
  if (!service) {
    return false;
  }

  OriginAttributes attrs(aOriginAttributes);
  nsTArray<RefPtr<nsICookie>> results;
  nsresult rv =
      service->GetCookiesFromHostNative(domain, &attrs, false, results);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  NS_ConvertUTF16toUTF8 matchName(aName);
  NS_ConvertUTF16toUTF8 matchPath(aPath);

  for (nsICookie* cookie : results) {
    MOZ_ASSERT(cookie);

    nsAutoCString name;
    rv = cookie->GetName(name);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }

    if (!matchName.Equals(name)) {
      continue;
    }

    nsAutoCString path;
    rv = cookie->GetPath(path);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }

    if (!matchPath.IsEmpty() && !matchPath.Equals(path)) {
      continue;
    }

    bool isPartitioned = false;
    rv = cookie->GetIsPartitioned(&isPartitioned);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }

    if (isPartitioned != aPartitioned) continue;

    if (aThirdPartyContext) {
      int32_t sameSiteAttr = 0;
      rv = cookie->GetSameSite(&sameSiteAttr);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return false;
      }

      if (!CookieCommons::ShouldIncludeCrossSiteCookie(
              sameSiteAttr,
              isPartitioned && !aOriginAttributes.mPartitionKey.IsEmpty(),
              aPartitionForeign, attrs.IsPrivateBrowsing(),
              aUsingStorageAccess)) {
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

    rv = service->RemoveNative(domain, matchName, path, &attrs, &aOperationID);
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
