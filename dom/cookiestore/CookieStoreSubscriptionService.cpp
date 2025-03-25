/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CookieStoreSubscriptionService.h"
#include "json/json.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/PCookieStore.h"
#include "mozilla/dom/ServiceWorkerManager.h"
#include "mozilla/dom/ServiceWorkerRegistrar.h"
#include "mozilla/net/Cookie.h"
#include "mozilla/net/CookieCommons.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsICookieNotification.h"

using namespace mozilla::dom;
using namespace mozilla::net;
using mozilla::ipc::PrincipalInfo;

static mozilla::StaticRefPtr<CookieStoreSubscriptionService> gService;

NS_IMPL_ISUPPORTS(CookieStoreSubscriptionService, nsIObserver)

// static
void CookieStoreSubscriptionService::ServiceWorkerLoaded(
    const ServiceWorkerRegistrationData& aData, const nsACString& aValue) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess());

  CookieStoreSubscriptionService* service =
      CookieStoreSubscriptionService::Instance();
  service->Load(aData, aValue);
}

// static
void CookieStoreSubscriptionService::ServiceWorkerUpdated(
    const ServiceWorkerRegistrationData& aData) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess());

  // This is a no-op
}

// static
void CookieStoreSubscriptionService::ServiceWorkerUnregistered(
    const ServiceWorkerRegistrationData& aData) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess());

  CookieStoreSubscriptionService* service =
      CookieStoreSubscriptionService::Instance();
  service->Unregister(aData);
}
// static
void CookieStoreSubscriptionService::ServiceWorkerUnregistered(
    nsIPrincipal* aPrincipal, const nsACString& aScopeURL) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess());

  PrincipalInfo principalInfo;
  nsresult rv = PrincipalToPrincipalInfo(aPrincipal, &principalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  ServiceWorkerRegistrationData tmp;
  tmp.principal() = principalInfo;
  tmp.scope() = aScopeURL;

  CookieStoreSubscriptionService* service =
      CookieStoreSubscriptionService::Instance();
  service->Unregister(tmp);
}

// static
CookieStoreSubscriptionService* CookieStoreSubscriptionService::Instance() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess());

  if (!gService && !PastShutdownPhase(ShutdownPhase::XPCOMShutdownFinal)) {
    gService = new CookieStoreSubscriptionService();
    gService->Initialize();
    ClearOnShutdown(&gService, ShutdownPhase::XPCOMShutdownFinal);
  }

  return gService;
}

CookieStoreSubscriptionService::CookieStoreSubscriptionService() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess());
}

void CookieStoreSubscriptionService::Initialize() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess());

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    DebugOnly<nsresult> rv =
        obs->AddObserver(gService, "private-cookie-changed", false);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    rv = obs->AddObserver(gService, "cookie-changed", false);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }
}

namespace {

bool Equivalent(const ServiceWorkerRegistrationData& aLeft,
                const ServiceWorkerRegistrationData& aRight) {
  MOZ_ASSERT(aLeft.principal().type() == PrincipalInfo::TContentPrincipalInfo);
  MOZ_ASSERT(aRight.principal().type() == PrincipalInfo::TContentPrincipalInfo);

  const auto& leftPrincipal = aLeft.principal().get_ContentPrincipalInfo();
  const auto& rightPrincipal = aRight.principal().get_ContentPrincipalInfo();

  // Only compare the attributes, not the spec part of the principal.
  // The scope comparison above already covers the origin and codebase
  // principals include the full path in their spec which is not what
  // we want here.
  return aLeft.scope() == aRight.scope() &&
         leftPrincipal.attrs() == rightPrincipal.attrs();
}

}  // anonymous namespace

void CookieStoreSubscriptionService::GetSubscriptions(
    const PrincipalInfo& aPrincipalInfo, const nsACString& aScope,
    nsTArray<CookieSubscription>& aSubscriptions) {
  MOZ_ASSERT(NS_IsMainThread());

  ServiceWorkerRegistrationData tmp;
  tmp.principal() = aPrincipalInfo;
  tmp.scope() = aScope;

  for (const RegistrationData& data : mData) {
    if (Equivalent(tmp, data.mRegistration)) {
      aSubscriptions.AppendElements(data.mSubscriptions);
      break;
    }
  }
}

void CookieStoreSubscriptionService::Subscribe(
    const PrincipalInfo& aPrincipalInfo, const nsACString& aScope,
    const nsTArray<CookieSubscription>& aSubscriptions) {
  MOZ_ASSERT(NS_IsMainThread());

  ServiceWorkerRegistrationData tmp;
  tmp.principal() = aPrincipalInfo;
  tmp.scope() = aScope;

  RegistrationData* registrationData = nullptr;

  for (RegistrationData& data : mData) {
    if (Equivalent(tmp, data.mRegistration)) {
      registrationData = &data;
      break;
    }
  }

  if (!registrationData) {
    registrationData = mData.AppendElement();
    registrationData->mRegistration = tmp;
  }

  bool toStore = false;

  for (const CookieSubscription& subscription : aSubscriptions) {
    bool found = false;
    for (const CookieSubscription& existingSubscription :
         registrationData->mSubscriptions) {
      if (existingSubscription.name() == subscription.name() &&
          existingSubscription.url() == subscription.url()) {
        // Nothing to do.
        found = true;
        break;
      }
    }

    if (!found) {
      registrationData->mSubscriptions.AppendElement(subscription);
      toStore = true;
    }
  }

  if (toStore) {
    SerializeAndSave(*registrationData);
  }
}

void CookieStoreSubscriptionService::Unsubscribe(
    const PrincipalInfo& aPrincipalInfo, const nsACString& aScope,
    const nsTArray<CookieSubscription>& aSubscriptions) {
  MOZ_ASSERT(NS_IsMainThread());

  ServiceWorkerRegistrationData tmp;
  tmp.principal() = aPrincipalInfo;
  tmp.scope() = aScope;

  RegistrationData* registrationData = nullptr;
  uint32_t registrationDataId = 0;

  for (; registrationDataId < mData.Length(); ++registrationDataId) {
    RegistrationData& data = mData[registrationDataId];
    if (Equivalent(tmp, data.mRegistration)) {
      registrationData = &data;
      break;
    }
  }

  if (!registrationData) {
    return;
  }

  bool toStore = false;

  for (const CookieSubscription& subscription : aSubscriptions) {
    for (uint32_t i = 0; i < registrationData->mSubscriptions.Length(); ++i) {
      const CookieSubscription& existingSubscription =
          registrationData->mSubscriptions[i];
      if (existingSubscription.name() == subscription.name() &&
          existingSubscription.url() == subscription.url()) {
        registrationData->mSubscriptions.RemoveElementAt(i);
        toStore = true;
        break;
      }
    }
  }

  if (toStore) {
    SerializeAndSave(*registrationData);

    if (registrationData->mSubscriptions.IsEmpty()) {
      mData.RemoveElementAt(registrationDataId);
    }
  }
}

NS_IMETHODIMP
CookieStoreSubscriptionService::Observe(nsISupports* aSubject,
                                        const char* aTopic,
                                        const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsICookieNotification> notification = do_QueryInterface(aSubject);
  NS_ENSURE_TRUE(notification, NS_ERROR_FAILURE);

  auto action = notification->GetAction();
  if (action != nsICookieNotification::COOKIE_DELETED &&
      action != nsICookieNotification::COOKIE_ADDED &&
      action != nsICookieNotification::COOKIE_CHANGED) {
    // Other actions are user specific ones (ALL_COOKIES_CLEARED or
    // COOKIES_BATCH_DELETED) and we don't want to expose them here.
    return NS_OK;
  }

  nsAutoCString baseDomain;
  nsresult rv = notification->GetBaseDomain(baseDomain);
  if (NS_WARN_IF(NS_FAILED(rv)) || baseDomain.IsEmpty()) {
    return rv;
  }

  nsCOMPtr<nsICookie> cookie;
  rv = notification->GetCookie(getter_AddRefs(cookie));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool isHttpOnly;
  rv = cookie->GetIsHttpOnly(&isHttpOnly);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (isHttpOnly) {
    return NS_OK;
  }

  nsAutoCString nameUtf8;
  rv = cookie->GetName(nameUtf8);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  NS_ConvertUTF8toUTF16 name(nameUtf8);

  bool deleteEvent = action == nsICookieNotification::COOKIE_DELETED;

  nsAutoString value;
  if (!deleteEvent) {
    nsAutoCString valueUtf8;
    rv = cookie->GetValue(valueUtf8);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    CopyUTF8toUTF16(valueUtf8, value);
  }

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    return NS_ERROR_FAILURE;
  }

  for (const RegistrationData& data : mData) {
    MOZ_ASSERT(data.mRegistration.principal().type() ==
               PrincipalInfo::TContentPrincipalInfo);

    const auto& principalInfo =
        data.mRegistration.principal().get_ContentPrincipalInfo();

    if (principalInfo.baseDomain() != baseDomain) {
      continue;
    }

    if (cookie->OriginAttributesNative() != principalInfo.attrs()) {
      continue;
    }

    for (const CookieSubscription& subscription : data.mSubscriptions) {
      if (subscription.name().isSome() && subscription.name().value() != name) {
        continue;
      }

      nsCOMPtr<nsIURI> uri;
      rv = NS_NewURI(getter_AddRefs(uri), data.mRegistration.scope());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      nsAutoCString filePath;
      rv = uri->GetFilePath(filePath);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (!CookieCommons::PathMatches(cookie->AsCookie().Path(), filePath)) {
        continue;
      }

      rv = swm->SendCookieChangeEvent(principalInfo.attrs(),
                                      data.mRegistration.scope(), name, value,
                                      deleteEvent);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      break;
    }
  }

  return NS_OK;
}

void CookieStoreSubscriptionService::Load(
    const ServiceWorkerRegistrationData& aData, const nsACString& aValue) {
  MOZ_ASSERT(NS_IsMainThread());

  for (RegistrationData& data : mData) {
    if (Equivalent(aData, data.mRegistration)) {
      ParseAndAddSubscription(data, aValue);
      return;
    }
  }

  RegistrationData* data = mData.AppendElement();
  data->mRegistration = aData;
  ParseAndAddSubscription(*data, aValue);
}

void CookieStoreSubscriptionService::Unregister(
    const ServiceWorkerRegistrationData& aData) {
  MOZ_ASSERT(NS_IsMainThread());

  for (uint32_t i = 0; i < mData.Length(); ++i) {
    if (Equivalent(aData, mData[i].mRegistration)) {
      mData.RemoveElementAt(i);
      return;
    }
  }
}

void CookieStoreSubscriptionService::ParseAndAddSubscription(
    RegistrationData& aData, const nsACString& aValue) {
  MOZ_ASSERT(NS_IsMainThread());

  Json::Value value;
  Json::Reader jsonReader;

  MOZ_ASSERT(jsonReader.parse(aValue.BeginReading(), value, false));
  MOZ_ASSERT(value.isObject());

  for (Json::ValueConstIterator iter = value.begin(); iter != value.end();
       ++iter) {
    CookieSubscription* subscription = aData.mSubscriptions.AppendElement();

    for (Json::Value::const_iterator itr = iter->begin(); itr != iter->end();
         itr++) {
      MOZ_ASSERT(iter.key().isString());
      MOZ_ASSERT(iter->isString());
      if (itr.key().asString().compare("name") == 0) {
        subscription->name() =
            Some(NS_ConvertUTF8toUTF16(iter->asString().c_str()));
      } else if (itr.key().asString().compare("url") == 0) {
        subscription->url() = NS_ConvertUTF8toUTF16(iter->asString().c_str());
      }
    }
  }
}

void CookieStoreSubscriptionService::SerializeAndSave(
    const RegistrationData& aData) {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<ServiceWorkerRegistrar> swr = ServiceWorkerRegistrar::Get();
  MOZ_ASSERT(swr);

  if (aData.mSubscriptions.IsEmpty()) {
    swr->UnstoreServiceWorkerExpandoOnMainThread(
        aData.mRegistration.principal(), aData.mRegistration.scope(),
        nsCString("cookie-store"));
    return;
  }

  Json::Value root;
  Json::StreamWriterBuilder builder;
  builder["indentation"] = "";
  const std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());

  for (uint32_t i = 0; i < aData.mSubscriptions.Length(); ++i) {
    Json::Value entry;

    if (aData.mSubscriptions[i].name().isSome()) {
      entry["name"] =
          NS_ConvertUTF16toUTF8(aData.mSubscriptions[i].name().value()).get();
    }

    entry["url"] = NS_ConvertUTF16toUTF8(aData.mSubscriptions[i].url()).get();
    root[i] = entry;
  }

  std::string document = Json::writeString(builder, root);

  swr->StoreServiceWorkerExpandoOnMainThread(
      aData.mRegistration.principal(), aData.mRegistration.scope(),
      nsCString("cookie-store"), nsCString(document));
}
