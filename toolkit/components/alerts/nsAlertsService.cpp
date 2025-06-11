/* -*- Mode: C++; tab-width: 2; indent-tabs-mode:nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIObserverService.h"
#include "xpcpublic.h"
#include "mozilla/AppShutdown.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_alerts.h"
#include "nsServiceManagerUtils.h"
#include "nsXULAlerts.h"

#include "nsAlertsService.h"

#include "nsToolkitCompsCID.h"
#include "nsComponentManagerUtils.h"

#ifdef MOZ_PLACES
#  include "nsIFaviconService.h"
#endif  // MOZ_PLACES

#ifdef XP_WIN
#  include <windows.h>
#  include <shellapi.h>
#endif

using namespace mozilla;

NS_IMPL_ISUPPORTS(nsAlertsService, nsIAlertsService, nsIAlertsDoNotDisturb,
                  nsIObserver)

nsAlertsService::nsAlertsService() : mBackend(nullptr) {
  mBackend = do_GetService(NS_SYSTEMALERTSERVICE_CONTRACTID);
}

nsresult nsAlertsService::Init() {
  if (nsCOMPtr<nsIObserverService> obsServ =
          mozilla::services::GetObserverService()) {
    (void)NS_WARN_IF(
        NS_FAILED(obsServ->AddObserver(this, "last-pb-context-exited", false)));
  }

  // The shutdown callback holds a strong reference and thus makes sure this
  // runs at shutdown.
  //
  // Note that the purpose of this shutdown cleanup is to make the leak checker
  // happy, and an early exit(0) without calling it should not break anything.
  // (See also bug 1606879)
  RunOnShutdown([self = RefPtr{this}]() { self->Teardown(); });

  return NS_OK;
}

nsAlertsService::~nsAlertsService() = default;

bool nsAlertsService::ShouldShowAlert() {
  bool result = true;

#ifdef XP_WIN
  if (!xpc::IsInAutomation()) {
    QUERY_USER_NOTIFICATION_STATE qstate;
    if (SUCCEEDED(SHQueryUserNotificationState(&qstate))) {
      if (qstate != QUNS_ACCEPTS_NOTIFICATIONS) {
        result = false;
      }
    }
  }
#endif

  nsCOMPtr<nsIAlertsDoNotDisturb> alertsDND(GetDNDBackend());
  if (alertsDND) {
    bool suppressForScreenSharing = false;
    nsresult rv =
        alertsDND->GetSuppressForScreenSharing(&suppressForScreenSharing);
    if (NS_SUCCEEDED(rv)) {
      result &= !suppressForScreenSharing;
    }
  }

  return result;
}

bool nsAlertsService::ShouldUseSystemBackend() {
  if (!mBackend) {
    return false;
  }
  return StaticPrefs::alerts_useSystemBackend();
}

NS_IMETHODIMP nsAlertsService::ShowAlertNotification(
    const nsAString& aImageUrl, const nsAString& aAlertTitle,
    const nsAString& aAlertText, bool aAlertTextClickable,
    const nsAString& aAlertCookie, nsIObserver* aAlertListener,
    const nsAString& aAlertName, const nsAString& aBidi, const nsAString& aLang,
    const nsAString& aData, nsIPrincipal* aPrincipal, bool aInPrivateBrowsing,
    bool aRequireInteraction) {
  nsCOMPtr<nsIAlertNotification> alert =
      do_CreateInstance(ALERT_NOTIFICATION_CONTRACTID);
  NS_ENSURE_TRUE(alert, NS_ERROR_FAILURE);
  // vibrate is unused
  nsTArray<uint32_t> vibrate;
  nsresult rv = alert->Init(aAlertName, aImageUrl, aAlertTitle, aAlertText,
                            aAlertTextClickable, aAlertCookie, aBidi, aLang,
                            aData, aPrincipal, aInPrivateBrowsing,
                            aRequireInteraction, false, vibrate);
  NS_ENSURE_SUCCESS(rv, rv);
  return ShowAlert(alert, aAlertListener);
}

static bool ShouldFallBackToXUL() {
#if defined(XP_WIN) || defined(XP_MACOSX)
  // We know we always have system backend on Windows and macOS. Let's not
  // permanently fall back to XUL just because of temporary failure.
  return false;
#else
  // The system may not have the notification library, we should fall back to
  // XUL.
  return true;
#endif
}

NS_IMETHODIMP nsAlertsService::ShowAlert(nsIAlertNotification* aAlert,
                                         nsIObserver* aAlertListener) {
  NS_ENSURE_ARG(aAlert);

  nsAutoString cookie;
  nsresult rv = aAlert->GetCookie(cookie);
  NS_ENSURE_SUCCESS(rv, rv);

  if (AppShutdown::IsInOrBeyond(ShutdownPhase::AppShutdownConfirmed)) {
    // Bailing out without calling alertfinished, because we do not want to
    // propagate an error to observers during shutdown.
    return NS_OK;
  }

  // Check if there is an optional service that handles system-level
  // notifications
  if (ShouldUseSystemBackend()) {
    rv = mBackend->ShowAlert(aAlert, aAlertListener);
    if (NS_SUCCEEDED(rv) || !ShouldFallBackToXUL()) {
      return rv;
    }
    // If the system backend failed to show the alert, clear the backend and
    // retry with XUL notifications. Future alerts will always use XUL.
    mBackend = nullptr;
  }

  if (!ShouldShowAlert()) {
    // Do not display the alert. Instead call alertfinished and get out.
    if (aAlertListener) {
      aAlertListener->Observe(nullptr, "alertfinished", cookie.get());
    }
    return NS_OK;
  }

  // Use XUL notifications as a fallback if above methods have failed.
  nsCOMPtr<nsIAlertsService> xulBackend(nsXULAlerts::GetInstance());
  NS_ENSURE_TRUE(xulBackend, NS_ERROR_FAILURE);
  return xulBackend->ShowAlert(aAlert, aAlertListener);
}

NS_IMETHODIMP nsAlertsService::CloseAlert(const nsAString& aAlertName,
                                          bool aContextClosed) {
  nsresult rv;
  // Try the system notification service.
  if (ShouldUseSystemBackend()) {
    rv = mBackend->CloseAlert(aAlertName, aContextClosed);
    if (NS_WARN_IF(NS_FAILED(rv)) && ShouldFallBackToXUL()) {
      // If the system backend failed to close the alert, fall back to XUL for
      // future alerts.
      mBackend = nullptr;
    }
  } else {
    nsCOMPtr<nsIAlertsService> xulBackend(nsXULAlerts::GetInstance());
    NS_ENSURE_TRUE(xulBackend, NS_ERROR_FAILURE);
    rv = xulBackend->CloseAlert(aAlertName, aContextClosed);
  }
  return rv;
}

NS_IMETHODIMP nsAlertsService::GetHistory(nsTArray<nsString>& aResult) {
  if (!mBackend) {
    return NS_OK;
  }

  return mBackend->GetHistory(aResult);
}

// nsIAlertsDoNotDisturb
NS_IMETHODIMP nsAlertsService::GetManualDoNotDisturb(bool* aRetVal) {
#ifdef MOZ_WIDGET_ANDROID
  return NS_ERROR_NOT_IMPLEMENTED;
#else
  nsCOMPtr<nsIAlertsDoNotDisturb> alertsDND(GetDNDBackend());
  NS_ENSURE_TRUE(alertsDND, NS_ERROR_NOT_IMPLEMENTED);
  return alertsDND->GetManualDoNotDisturb(aRetVal);
#endif
}

NS_IMETHODIMP nsAlertsService::SetManualDoNotDisturb(bool aDoNotDisturb) {
#ifdef MOZ_WIDGET_ANDROID
  return NS_ERROR_NOT_IMPLEMENTED;
#else
  nsCOMPtr<nsIAlertsDoNotDisturb> alertsDND(GetDNDBackend());
  NS_ENSURE_TRUE(alertsDND, NS_ERROR_NOT_IMPLEMENTED);

  return alertsDND->SetManualDoNotDisturb(aDoNotDisturb);
#endif
}

NS_IMETHODIMP nsAlertsService::GetSuppressForScreenSharing(bool* aRetVal) {
#ifdef MOZ_WIDGET_ANDROID
  return NS_ERROR_NOT_IMPLEMENTED;
#else
  nsCOMPtr<nsIAlertsDoNotDisturb> alertsDND(GetDNDBackend());
  NS_ENSURE_TRUE(alertsDND, NS_ERROR_NOT_IMPLEMENTED);
  return alertsDND->GetSuppressForScreenSharing(aRetVal);
#endif
}

NS_IMETHODIMP nsAlertsService::SetSuppressForScreenSharing(bool aSuppress) {
#ifdef MOZ_WIDGET_ANDROID
  return NS_ERROR_NOT_IMPLEMENTED;
#else
  nsCOMPtr<nsIAlertsDoNotDisturb> alertsDND(GetDNDBackend());
  NS_ENSURE_TRUE(alertsDND, NS_ERROR_NOT_IMPLEMENTED);
  return alertsDND->SetSuppressForScreenSharing(aSuppress);
#endif
}

already_AddRefed<nsIAlertsDoNotDisturb> nsAlertsService::GetDNDBackend() {
  nsCOMPtr<nsIAlertsService> backend;
  // Try the system notification service.
  if (ShouldUseSystemBackend()) {
    backend = mBackend;
  }
  if (!backend) {
    backend = nsXULAlerts::GetInstance();
  }

  nsCOMPtr<nsIAlertsDoNotDisturb> alertsDND(do_QueryInterface(backend));
  return alertsDND.forget();
}

NS_IMETHODIMP nsAlertsService::Observe(nsISupports* aSubject,
                                       const char* aTopic,
                                       const char16_t* aData) {
  nsDependentCString topic(aTopic);
  if (topic == "last-pb-context-exited"_ns) {
    return PbmTeardown();
  }
  return NS_OK;
}

NS_IMETHODIMP nsAlertsService::Teardown() {
  nsCOMPtr<nsIAlertsService> backend;
  // Try the system notification service.
  if (ShouldUseSystemBackend()) {
    backend = mBackend;
  }
  if (!backend) {
    // We do not try nsXULAlerts here as it already uses ClearOnShutdown.
    return NS_OK;
  }
  return backend->Teardown();
}

NS_IMETHODIMP nsAlertsService::PbmTeardown() {
  nsCOMPtr<nsIAlertsService> backend;
  // Try the system notification service.
  if (ShouldUseSystemBackend()) {
    backend = mBackend;
  }
  if (!backend) {
    backend = nsXULAlerts::GetInstance();
  }
  return backend->PbmTeardown();
}
