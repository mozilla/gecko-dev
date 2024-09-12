/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CookieStoreParent.h"
#include "mozilla/Maybe.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/net/CookieCommons.h"
#include "mozilla/Unused.h"
#include "nsICookieManager.h"

using namespace mozilla::ipc;

namespace mozilla::dom {

namespace {

void GetRequestHelper(const nsAString& aDomain,
                      const OriginAttributes& aOriginAttributes,
                      bool aMatchName, const nsAString& aName,
                      const nsACString& aPath, bool aOnlyFirstMatch,
                      nsTArray<CookieData>& aResults) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsICookieManager> service =
      do_GetService(NS_COOKIEMANAGER_CONTRACTID);
  if (!service) {
    return;
  }

  OriginAttributes attrs(aOriginAttributes);
  nsTArray<RefPtr<nsICookie>> results;
  nsresult rv = service->GetCookiesFromHostNative(
      NS_ConvertUTF16toUTF8(aDomain), &attrs, results);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  NS_ConvertUTF16toUTF8 matchName(aName);

  nsTArray<CookieData> list;
  for (nsICookie* cookie : results) {
    bool isHttpOnly;
    rv = cookie->GetIsHttpOnly(&isHttpOnly);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    if (isHttpOnly) {
      continue;
    }

    nsAutoCString name;
    rv = cookie->GetName(name);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    if (aMatchName && !matchName.Equals(name)) {
      continue;
    }

    nsAutoCString cookiePath;
    rv = cookie->GetPath(cookiePath);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    if (!net::CookieCommons::PathMatches(cookiePath, aPath)) {
      continue;
    }

    nsAutoCString value;
    rv = cookie->GetValue(value);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    CookieData* data = list.AppendElement();
    data->name() = NS_ConvertUTF8toUTF16(name);
    data->value() = NS_ConvertUTF8toUTF16(value);

    if (aOnlyFirstMatch) {
      break;
    }
  }

  aResults.SwapElements(list);
}

void SetRequestHelper(const nsAString& aDomain,
                      const OriginAttributes& aOriginAttributes,
                      const nsAString& aName, const nsAString& aValue,
                      bool aSession, int64_t aExpires, const nsAString& aPath,
                      int32_t aSameSite, bool aPartitioned) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsICookieManager> service =
      do_GetService(NS_COOKIEMANAGER_CONTRACTID);
  if (!service) {
    return;
  }

  OriginAttributes attrs(aOriginAttributes);
  nsresult rv = service->AddNative(
      NS_ConvertUTF16toUTF8(aDomain), NS_ConvertUTF16toUTF8(aPath),
      NS_ConvertUTF16toUTF8(aName), NS_ConvertUTF16toUTF8(aValue),
      true,   //  secure
      false,  // mHttpOnly,
      aSession, aSession ? PR_Now() : aExpires, &attrs, aSameSite,
      nsICookie::SCHEME_HTTPS, aPartitioned, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
}

void DeleteRequestHelper(const nsAString& aDomain,
                         const OriginAttributes& aOriginAttributes,
                         const nsAString& aName, const nsAString& aPath,
                         bool aPartitioned) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsICookieManager> service =
      do_GetService(NS_COOKIEMANAGER_CONTRACTID);
  if (!service) {
    return;
  }

  NS_ConvertUTF16toUTF8 domainUtf8(aDomain);

  OriginAttributes attrs(aOriginAttributes);
  nsTArray<RefPtr<nsICookie>> results;
  nsresult rv = service->GetCookiesFromHostNative(domainUtf8, &attrs, results);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  NS_ConvertUTF16toUTF8 matchName(aName);
  NS_ConvertUTF16toUTF8 matchPath(aPath);

  for (nsICookie* cookie : results) {
    MOZ_ASSERT(cookie);

    nsAutoCString name;
    rv = cookie->GetName(name);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    if (!matchName.Equals(name)) {
      continue;
    }

    nsAutoCString path;
    rv = cookie->GetPath(path);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    if (!matchPath.IsEmpty() && !matchPath.Equals(path)) {
      continue;
    }

    bool isPartitioned = false;
    rv = cookie->GetIsPartitioned(&isPartitioned);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    if (isPartitioned != aPartitioned) continue;

    rv = service->RemoveNative(domainUtf8, matchName, path, &attrs, nullptr);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  }
}

}  // namespace

CookieStoreParent::CookieStoreParent() { AssertIsOnBackgroundThread(); }

CookieStoreParent::~CookieStoreParent() { AssertIsOnBackgroundThread(); }

mozilla::ipc::IPCResult CookieStoreParent::RecvGetRequest(
    const nsString& aDomain, const OriginAttributes& aOriginAttributes,
    const bool& aMatchName, const nsString& aName, const nsCString& aPath,
    const bool& aOnlyFirstMatch, GetRequestResolver&& aResolver) {
  InvokeAsync(GetMainThreadSerialEventTarget(), __func__,
              [aDomain, aOriginAttributes, aMatchName, aName, aPath,
               aOnlyFirstMatch]() {
                CopyableTArray<CookieData> results;
                GetRequestHelper(aDomain, aOriginAttributes, aMatchName, aName,
                                 aPath, aOnlyFirstMatch, results);
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
    const nsString& aName, const nsString& aValue, const bool& aSession,
    const int64_t& aExpires, const nsString& aPath, const int32_t& aSameSite,
    const bool& aPartitioned, SetRequestResolver&& aResolver) {
  InvokeAsync(
      GetMainThreadSerialEventTarget(), __func__,
      [aDomain, aOriginAttributes, aName, aValue, aSession, aExpires, aPath,
       aSameSite, aPartitioned]() {
        SetRequestHelper(aDomain, aOriginAttributes, aName, aValue, aSession,
                         aExpires, aPath, aSameSite, aPartitioned);
        return SetDeleteRequestPromise::CreateAndResolve(true, __func__);
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
    const nsString& aName, const nsString& aPath, const bool& aPartitioned,
    DeleteRequestResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  InvokeAsync(GetMainThreadSerialEventTarget(), __func__,
              [aDomain, aOriginAttributes, aName, aPath, aPartitioned]() {
                DeleteRequestHelper(aDomain, aOriginAttributes, aName, aPath,
                                    aPartitioned);
                return SetDeleteRequestPromise::CreateAndResolve(true,
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

}  // namespace mozilla::dom
