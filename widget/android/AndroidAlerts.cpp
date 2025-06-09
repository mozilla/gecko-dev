/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidAlerts.h"
#include "mozilla/java/GeckoRuntimeWrappers.h"
#include "mozilla/java/WebNotificationWrappers.h"
#include "nsIPrincipal.h"
#include "nsIURI.h"

namespace mozilla {
namespace widget {

NS_IMPL_ISUPPORTS(AndroidAlerts, nsIAlertsService)

struct AndroidNotificationTuple {
  // Can be null if the caller doesn't care about the result.
  nsCOMPtr<nsIObserver> mObserver;

  // The Gecko alert notification.
  nsCOMPtr<nsIAlertNotification> mAlert;

  // The Java represented form of mAlert.
  mozilla::java::WebNotification::GlobalRef mNotificationRef;
};

using NotificationMap = nsTHashMap<nsStringHashKey, AndroidNotificationTuple>;
static StaticAutoPtr<NotificationMap> sNotificationMap;

NS_IMETHODIMP
AndroidAlerts::ShowAlertNotification(
    const nsAString& aImageUrl, const nsAString& aAlertTitle,
    const nsAString& aAlertText, bool aAlertTextClickable,
    const nsAString& aAlertCookie, nsIObserver* aAlertListener,
    const nsAString& aAlertName, const nsAString& aBidi, const nsAString& aLang,
    const nsAString& aData, nsIPrincipal* aPrincipal, bool aInPrivateBrowsing,
    bool aRequireInteraction) {
  MOZ_ASSERT_UNREACHABLE("Should be implemented by nsAlertsService.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
AndroidAlerts::ShowAlert(nsIAlertNotification* aAlert,
                         nsIObserver* aAlertListener) {
  // nsAlertsService disables our alerts backend if we ever return failure
  // here. To keep the backend enabled, we always return NS_OK even if we
  // encounter an error here.
  nsresult rv;

  nsAutoString imageUrl;
  rv = aAlert->GetImageURL(imageUrl);
  NS_ENSURE_SUCCESS(rv, NS_OK);

  nsAutoString title;
  rv = aAlert->GetTitle(title);
  NS_ENSURE_SUCCESS(rv, NS_OK);

  nsAutoString text;
  rv = aAlert->GetText(text);
  NS_ENSURE_SUCCESS(rv, NS_OK);

  nsAutoString cookie;
  rv = aAlert->GetCookie(cookie);
  NS_ENSURE_SUCCESS(rv, NS_OK);

  nsAutoString name;
  rv = aAlert->GetName(name);
  NS_ENSURE_SUCCESS(rv, NS_OK);

  nsAutoString lang;
  rv = aAlert->GetLang(lang);
  NS_ENSURE_SUCCESS(rv, NS_OK);

  nsAutoString dir;
  rv = aAlert->GetDir(dir);
  NS_ENSURE_SUCCESS(rv, NS_OK);

  bool requireInteraction;
  rv = aAlert->GetRequireInteraction(&requireInteraction);
  NS_ENSURE_SUCCESS(rv, NS_OK);

  nsCOMPtr<nsIURI> uri;
  rv = aAlert->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, NS_OK);

  nsCString spec;
  if (uri) {
    rv = uri->GetDisplaySpec(spec);
    NS_ENSURE_SUCCESS(rv, NS_OK);
  }

  bool silent;
  rv = aAlert->GetSilent(&silent);
  NS_ENSURE_SUCCESS(rv, NS_OK);

  bool privateBrowsing;
  rv = aAlert->GetInPrivateBrowsing(&privateBrowsing);
  NS_ENSURE_SUCCESS(rv, NS_OK);

  nsTArray<uint32_t> vibrate;
  rv = aAlert->GetVibrate(vibrate);
  NS_ENSURE_SUCCESS(rv, NS_OK);

  if (!sNotificationMap) {
    sNotificationMap = new NotificationMap();
  } else if (Maybe<AndroidNotificationTuple> tuple =
                 sNotificationMap->Extract(name)) {
    if (tuple->mObserver) {
      tuple->mObserver->Observe(nullptr, "alertfinished", nullptr);
    }
  }

  java::WebNotification::LocalRef notification = notification->New(
      title, name, cookie, text, imageUrl, dir, lang, requireInteraction, spec,
      silent, privateBrowsing, jni::IntArray::From(vibrate));
  AndroidNotificationTuple tuple{
      .mObserver = aAlertListener,
      .mAlert = aAlert,
      .mNotificationRef = notification,
  };
  sNotificationMap->InsertOrUpdate(name, std::move(tuple));

  if (java::GeckoRuntime::LocalRef runtime =
          java::GeckoRuntime::GetInstance()) {
    runtime->NotifyOnShow(notification);
  }

  return NS_OK;
}

NS_IMETHODIMP
AndroidAlerts::CloseAlert(const nsAString& aAlertName, bool aContextClosed) {
  if (!sNotificationMap) {
    return NS_OK;
  }

  Maybe<AndroidNotificationTuple> tuple =
      sNotificationMap->MaybeGet(aAlertName);
  if (!tuple) {
    return NS_OK;
  }

  java::GeckoRuntime::LocalRef runtime = java::GeckoRuntime::GetInstance();
  if (runtime != NULL) {
    runtime->NotifyOnClose(tuple->mNotificationRef);
  }

  return NS_OK;
}

NS_IMETHODIMP AndroidAlerts::Teardown() {
  sNotificationMap = nullptr;
  return NS_OK;
}

NS_IMETHODIMP AndroidAlerts::PbmTeardown() { return NS_ERROR_NOT_IMPLEMENTED; }

void AndroidAlerts::NotifyListener(const nsAString& aName, const char* aTopic,
                                   const char16_t* aCookie) {
  if (!sNotificationMap) {
    return;
  }

  Maybe<AndroidNotificationTuple> tuple = sNotificationMap->MaybeGet(aName);
  if (!tuple) {
    return;
  }

  if (tuple->mObserver) {
    tuple->mObserver->Observe(nullptr, aTopic, nullptr);
  }

  if ("alertfinished"_ns.Equals(aTopic)) {
    sNotificationMap->Remove(aName);
  }
}

}  // namespace widget
}  // namespace mozilla
