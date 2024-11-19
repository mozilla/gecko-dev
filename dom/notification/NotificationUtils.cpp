/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NotificationUtils.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/Components.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/dom/NotificationBinding.h"
#include "mozilla/glean/GleanMetrics.h"
#include "nsContentUtils.h"
#include "nsIAlertsService.h"
#include "nsINotificationStorage.h"
#include "nsIPermissionManager.h"
#include "nsIPushService.h"
#include "nsServiceManagerUtils.h"

namespace mozilla::dom::notification {

using GleanLabel = glean::web_notification::ShowOriginLabel;

static void ReportTelemetry(GleanLabel aLabel,
                            PermissionCheckPurpose aPurpose) {
  switch (aPurpose) {
    case PermissionCheckPurpose::PermissionAttribute:
      glean::web_notification::permission_origin
          .EnumGet(static_cast<glean::web_notification::PermissionOriginLabel>(
              aLabel))
          .Add();
      return;
    case PermissionCheckPurpose::PermissionRequest:
      glean::web_notification::request_permission_origin
          .EnumGet(static_cast<
                   glean::web_notification::RequestPermissionOriginLabel>(
              aLabel))
          .Add();
      return;
    case PermissionCheckPurpose::NotificationShow:
      glean::web_notification::show_origin.EnumGet(aLabel).Add();
      return;
    default:
      MOZ_CRASH("Unknown permission checker");
      return;
  }
}

bool IsNotificationAllowedFor(nsIPrincipal* aPrincipal) {
  if (aPrincipal->IsSystemPrincipal()) {
    return true;
  }
  // Allow files to show notifications by default.
  return aPrincipal->SchemeIs("file");
}

bool IsNotificationForbiddenFor(nsIPrincipal* aPrincipal,
                                nsIPrincipal* aEffectiveStoragePrincipal,
                                bool isSecureContext,
                                PermissionCheckPurpose aPurpose,
                                Document* aRequestorDoc) {
  if (aPrincipal->GetIsInPrivateBrowsing() &&
      !StaticPrefs::dom_webnotifications_privateBrowsing_enabled()) {
    return true;
  }

  if (!isSecureContext) {
    if (aRequestorDoc) {
      glean::web_notification::insecure_context_permission_request.Add();
      nsContentUtils::ReportToConsole(
          nsIScriptError::errorFlag, "DOM"_ns, aRequestorDoc,
          nsContentUtils::eDOM_PROPERTIES,
          "NotificationsInsecureRequestIsForbidden");
    }
    return true;
  }

  const nsString& partitionKey =
      aEffectiveStoragePrincipal->OriginAttributesRef().mPartitionKey;

  if (aEffectiveStoragePrincipal->OriginAttributesRef()
          .mPartitionKey.IsEmpty()) {
    // first party
    ReportTelemetry(GleanLabel::eFirstParty, aPurpose);
    return false;
  }
  nsString outScheme;
  nsString outBaseDomain;
  int32_t outPort;
  bool outForeignByAncestorContext;
  OriginAttributes::ParsePartitionKey(partitionKey, outScheme, outBaseDomain,
                                      outPort, outForeignByAncestorContext);
  if (outForeignByAncestorContext) {
    // nested first party
    ReportTelemetry(GleanLabel::eNestedFirstParty, aPurpose);
    return false;
  }

  // third party
  ReportTelemetry(GleanLabel::eThirdParty, aPurpose);
  if (aRequestorDoc) {
    nsContentUtils::ReportToConsole(
        nsIScriptError::errorFlag, "DOM"_ns, aRequestorDoc,
        nsContentUtils::eDOM_PROPERTIES,
        "NotificationsCrossOriginIframeRequestIsForbidden");
  }
  return !StaticPrefs::dom_webnotifications_allowcrossoriginiframe();
}

NotificationPermission GetRawNotificationPermission(nsIPrincipal* aPrincipal) {
  AssertIsOnMainThread();

  uint32_t permission = nsIPermissionManager::UNKNOWN_ACTION;

  nsCOMPtr<nsIPermissionManager> permissionManager =
      components::PermissionManager::Service();
  if (!permissionManager) {
    return NotificationPermission::Default;
  }

  permissionManager->TestExactPermissionFromPrincipal(
      aPrincipal, "desktop-notification"_ns, &permission);

  // Convert the result to one of the enum types.
  switch (permission) {
    case nsIPermissionManager::ALLOW_ACTION:
      return NotificationPermission::Granted;
    case nsIPermissionManager::DENY_ACTION:
      return NotificationPermission::Denied;
    default:
      return NotificationPermission::Default;
  }
}

NotificationPermission GetNotificationPermission(
    nsIPrincipal* aPrincipal, nsIPrincipal* aEffectiveStoragePrincipal,
    bool isSecureContext, PermissionCheckPurpose aPurpose) {
  if (IsNotificationAllowedFor(aPrincipal)) {
    return NotificationPermission::Granted;
  }
  if (IsNotificationForbiddenFor(aPrincipal, aEffectiveStoragePrincipal,
                                 isSecureContext, aPurpose)) {
    return NotificationPermission::Denied;
  }

  return GetRawNotificationPermission(aPrincipal);
}

nsresult GetOrigin(nsIPrincipal* aPrincipal, nsString& aOrigin) {
  if (!aPrincipal) {
    return NS_ERROR_FAILURE;
  }

  MOZ_TRY(
      nsContentUtils::GetWebExposedOriginSerialization(aPrincipal, aOrigin));

  return NS_OK;
}

void ComputeAlertName(nsIPrincipal* aPrincipal, const nsString& aTag,
                      const nsString& aId, nsString& aResult) {
  nsAutoString alertName;
  nsresult rv = GetOrigin(aPrincipal, alertName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  // Get the notification name that is unique per origin + tag/ID.
  // The name of the alert is of the form origin#tag/ID.
  alertName.Append('#');
  if (!aTag.IsEmpty()) {
    alertName.AppendLiteral("tag:");
    alertName.Append(aTag);
  } else {
    alertName.AppendLiteral("notag:");
    alertName.Append(aId);
  }

  aResult = alertName;
}

nsCOMPtr<nsINotificationStorage> GetNotificationStorage(bool isPrivate) {
  return do_GetService(isPrivate ? NS_MEMORY_NOTIFICATION_STORAGE_CONTRACTID
                                 : NS_NOTIFICATION_STORAGE_CONTRACTID);
}

nsresult PersistNotification(nsIPrincipal* aPrincipal, const nsString& aId,
                             const nsString& aAlertName,
                             const IPCNotificationOptions& aOptions,
                             const nsString& aScope) {
  nsCOMPtr<nsINotificationStorage> notificationStorage =
      GetNotificationStorage(aPrincipal->GetIsInPrivateBrowsing());
  if (NS_WARN_IF(!notificationStorage)) {
    return NS_ERROR_UNEXPECTED;
  }

  nsString origin;
  nsresult rv = GetOrigin(aPrincipal, origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoString behavior;
  if (!aOptions.behavior().ToJSON(behavior)) {
    return NS_ERROR_FAILURE;
  }

  rv = notificationStorage->Put(
      origin, aId, aOptions.title(), GetEnumString(aOptions.dir()),
      aOptions.lang(), aOptions.body(), aOptions.tag(), aOptions.icon(),
      aAlertName, aOptions.dataSerialized(), behavior, aScope);

  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

nsresult UnpersistNotification(nsIPrincipal* aPrincipal, const nsString& aId) {
  if (!aPrincipal) {
    return NS_ERROR_FAILURE;
  }
  if (nsCOMPtr<nsINotificationStorage> notificationStorage =
          GetNotificationStorage(aPrincipal->GetIsInPrivateBrowsing())) {
    nsString origin;
    MOZ_TRY(GetOrigin(aPrincipal, origin));
    return notificationStorage->Delete(origin, aId);
  }
  return NS_ERROR_FAILURE;
}

void UnregisterNotification(nsIPrincipal* aPrincipal, const nsString& aId,
                            const nsString& aAlertName, CloseMode aCloseMode) {
  // XXX: unpersist only when explicitly closed, bug 1095073
  UnpersistNotification(aPrincipal, aId);
  if (nsCOMPtr<nsIAlertsService> alertService = components::Alerts::Service()) {
    alertService->CloseAlert(
        aAlertName,
        /* aContextClosed */ aCloseMode == CloseMode::InactiveGlobal);
  }
}

nsresult RemovePermission(nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(XRE_IsParentProcess());
  nsCOMPtr<nsIPermissionManager> permissionManager =
      mozilla::components::PermissionManager::Service();
  if (!permissionManager) {
    return NS_ERROR_FAILURE;
  }
  permissionManager->RemoveFromPrincipal(aPrincipal, "desktop-notification"_ns);
  return NS_OK;
}

nsresult OpenSettings(nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(XRE_IsParentProcess());
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    return NS_ERROR_FAILURE;
  }
  // Notify other observers so they can show settings UI.
  obs->NotifyObservers(aPrincipal, "notifications-open-settings", nullptr);
  return NS_OK;
}

nsresult AdjustPushQuota(nsIPrincipal* aPrincipal,
                         NotificationStatusChange aChange) {
  MOZ_ASSERT(XRE_IsParentProcess());
  nsCOMPtr<nsIPushQuotaManager> pushQuotaManager =
      do_GetService("@mozilla.org/push/Service;1");
  if (!pushQuotaManager) {
    return NS_ERROR_FAILURE;
  }

  nsAutoCString origin;
  MOZ_TRY(aPrincipal->GetOrigin(origin));

  if (aChange == NotificationStatusChange::Shown) {
    return pushQuotaManager->NotificationForOriginShown(origin.get());
  }
  return pushQuotaManager->NotificationForOriginClosed(origin.get());
}

}  // namespace mozilla::dom::notification
