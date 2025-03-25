/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CookieStoreSubscriptionService_h
#define mozilla_dom_CookieStoreSubscriptionService_h

#include "nsIObserver.h"
#include "mozilla/dom/ServiceWorkerRegistrarTypes.h"

namespace mozilla::dom {

class CookieSubscription;

class CookieStoreSubscriptionService final : public nsIObserver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static CookieStoreSubscriptionService* Instance();

  // ServiceWorkerRegistrar expandos callbacks:
  // This callback is executed when something is retrieved from the disk. The
  // `aValue` is a JSON string containing the cookie subscriptions for the
  // `aData` ServiceWorker.
  static void ServiceWorkerLoaded(const ServiceWorkerRegistrationData& aData,
                                  const nsACString& aValue);
  // When a ServiceWorker registration is updated, this callback is triggered.
  // For the CookieStore implementation, this is noop.
  static void ServiceWorkerUpdated(const ServiceWorkerRegistrationData& aData);
  // When a ServiceWorker is unregistered, this callback is executed.
  static void ServiceWorkerUnregistered(
      const ServiceWorkerRegistrationData& aData);
  // Similar to the previous callback but with different arguments.
  static void ServiceWorkerUnregistered(nsIPrincipal* aPrincipal,
                                        const nsACString& aScopeURL);

  void GetSubscriptions(const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
                        const nsACString& aScope,
                        nsTArray<CookieSubscription>& aSubscriptions);

  void Subscribe(const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
                 const nsACString& aScope,
                 const nsTArray<CookieSubscription>& aSubscriptions);

  void Unsubscribe(const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
                   const nsACString& aScope,
                   const nsTArray<CookieSubscription>& aSubscriptions);

  void Load(const ServiceWorkerRegistrationData& aData,
            const nsACString& aValue);

  void Unregister(const ServiceWorkerRegistrationData& aData);

 private:
  CookieStoreSubscriptionService();
  ~CookieStoreSubscriptionService() = default;

  void Initialize();

  struct RegistrationData {
    ServiceWorkerRegistrationData mRegistration;
    nsTArray<CookieSubscription> mSubscriptions;
  };

  void ParseAndAddSubscription(RegistrationData& aData,
                               const nsACString& aValue);

  void SerializeAndSave(const RegistrationData& aData);

  nsTArray<RegistrationData> mData;
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_CookieStoreSubscriptionService_h */
