/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CookieStoreNotifier.h"
#include "CookieStore.h"
#include "CookieChangeEvent.h"
#include "mozilla/net/CookieCommons.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/Unused.h"
#include "nsICookie.h"
#include "nsICookieNotification.h"
#include "nsISerialEventTarget.h"

namespace mozilla::dom {

NS_IMPL_ISUPPORTS(CookieStoreNotifier, nsIObserver);

// static
already_AddRefed<CookieStoreNotifier> CookieStoreNotifier::Create(
    CookieStore* aCookieStore) {
  nsIPrincipal* principal = nullptr;

  if (!NS_IsMainThread()) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);
    principal = workerPrivate->GetPrincipal();
  } else {
    nsCOMPtr<nsPIDOMWindowInner> window = aCookieStore->GetOwnerWindow();
    MOZ_ASSERT(window);

    nsCOMPtr<Document> document = window->GetExtantDoc();
    if (NS_WARN_IF(!document)) {
      return nullptr;
    }

    principal = document->NodePrincipal();
  }

  if (NS_WARN_IF(!principal)) {
    return nullptr;
  }

  nsCString baseDomain;
  if (NS_WARN_IF(NS_FAILED(
          net::CookieCommons::GetBaseDomain(principal, baseDomain)))) {
    return nullptr;
  }

  if (baseDomain.IsEmpty()) {
    return nullptr;
  }

  RefPtr<CookieStoreNotifier> notifier = new CookieStoreNotifier(
      aCookieStore, baseDomain, principal->OriginAttributesRef());

  bool privateBrowsing = principal->OriginAttributesRef().IsPrivateBrowsing();

  notifier->mEventTarget = GetCurrentSerialEventTarget();

  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(
        NS_NewRunnableFunction(__func__, [notifier, privateBrowsing] {
          notifier->AddObserversOnMainThread(privateBrowsing);
        }));
  } else {
    notifier->AddObserversOnMainThread(privateBrowsing);
  }

  return notifier.forget();
}

CookieStoreNotifier::CookieStoreNotifier(
    CookieStore* aCookieStore, const nsACString& aBaseDomain,
    const OriginAttributes& aOriginAttributes)
    : mCookieStore(aCookieStore),
      mBaseDomain(aBaseDomain),
      mOriginAttributes(aOriginAttributes) {
  MOZ_ASSERT(aCookieStore);
}

CookieStoreNotifier::~CookieStoreNotifier() = default;

void CookieStoreNotifier::Disentangle() {
  mCookieStore = nullptr;

  bool privateBrowsing = mOriginAttributes.IsPrivateBrowsing();

  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        __func__, [self = RefPtr(this), privateBrowsing] {
          self->RemoveObserversOnMainThread(privateBrowsing);
        }));
  } else {
    RemoveObserversOnMainThread(privateBrowsing);
  }
}

void CookieStoreNotifier::AddObserversOnMainThread(bool aPrivateBrowsing) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (NS_WARN_IF(!os)) {
    return;
  }

  nsresult rv = os->AddObserver(
      this, aPrivateBrowsing ? "private-cookie-changed" : "cookie-changed",
      false);
  Unused << NS_WARN_IF(NS_FAILED(rv));
}

void CookieStoreNotifier::RemoveObserversOnMainThread(bool aPrivateBrowsing) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (NS_WARN_IF(!os)) {
    return;
  }

  nsresult rv = os->RemoveObserver(
      this, aPrivateBrowsing ? "private-cookie-changed" : "cookie-changed");
  Unused << NS_WARN_IF(NS_FAILED(rv));
}

NS_IMETHODIMP
CookieStoreNotifier::Observe(nsISupports* aSubject, const char* aTopic,
                             const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsICookieNotification> notification = do_QueryInterface(aSubject);
  NS_ENSURE_TRUE(notification, NS_ERROR_FAILURE);

  auto action = notification->GetAction();
  if (action != nsICookieNotification::COOKIE_DELETED &&
      action != nsICookieNotification::COOKIE_ADDED &&
      action != nsICookieNotification::COOKIE_CHANGED) {
    return NS_OK;
  }

  nsAutoCString baseDomain;
  nsresult rv = notification->GetBaseDomain(baseDomain);
  if (NS_WARN_IF(NS_FAILED(rv)) || baseDomain.IsEmpty()) {
    return rv;
  }

  if (baseDomain != mBaseDomain) {
    return NS_OK;
  }

  nsCOMPtr<nsICookie> cookie;
  rv = notification->GetCookie(getter_AddRefs(cookie));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (cookie->OriginAttributesNative() != mOriginAttributes) {
    return NS_OK;
  }

  bool isHttpOnly;
  rv = cookie->GetIsHttpOnly(&isHttpOnly);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (isHttpOnly) {
    return NS_OK;
  }

  CookieListItem item;

  nsAutoCString name;
  rv = cookie->GetName(name);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  item.mName.Construct(NS_ConvertUTF8toUTF16(name));

  if (action != nsICookieNotification::COOKIE_DELETED) {
    nsAutoCString value;
    rv = cookie->GetValue(value);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    item.mValue.Construct(NS_ConvertUTF8toUTF16(value));
  }

  bool deletedEvent = action == nsICookieNotification::COOKIE_DELETED;

  if (mEventTarget->IsOnCurrentThread()) {
    DispatchEvent(item, deletedEvent);
  } else {
    mEventTarget->Dispatch(NS_NewRunnableFunction(
        __func__, [self = RefPtr(this), item, deletedEvent] {
          self->DispatchEvent(item, deletedEvent);
        }));
  }

  return NS_OK;
}

void CookieStoreNotifier::DispatchEvent(const CookieListItem& aItem,
                                        bool aDeletedEvent) {
  MOZ_ASSERT(mEventTarget->IsOnCurrentThread());

  if (!mCookieStore) {
    return;
  }

  RefPtr<Event> event =
      aDeletedEvent
          ? CookieChangeEvent::CreateForDeletedCookie(mCookieStore, aItem)
          : CookieChangeEvent::CreateForChangedCookie(mCookieStore, aItem);

  if (!event) {
    return;
  }

  if (NS_IsMainThread()) {
    nsCOMPtr<nsPIDOMWindowInner> window = mCookieStore->GetOwnerWindow();
    if (!window) {
      return;
    }

    RefPtr<BrowsingContext> bc = window->GetBrowsingContext();
    if (!bc) {
      return;
    }

    if (bc->IsInBFCache() ||
        (window->GetExtantDoc() && window->GetExtantDoc()->GetBFCacheEntry())) {
      mDelayedDOMEvents.AppendElement(event);
      return;
    }
  }

  mCookieStore->DispatchEvent(*event);
}

void CookieStoreNotifier::FireDelayedDOMEvents() {
  MOZ_ASSERT(NS_IsMainThread());

  nsTArray<RefPtr<Event>> delayedDOMEvents;
  delayedDOMEvents.SwapElements(mDelayedDOMEvents);

  for (Event* event : delayedDOMEvents) {
    mCookieStore->DispatchEvent(*event);
  }
}

}  // namespace mozilla::dom
