/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NotificationUtils.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/Components.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/dom/NotificationBinding.h"
#include "nsIPermissionManager.h"

namespace mozilla::dom::notification {

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
    return false;
  }

  // third party
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

}  // namespace mozilla::dom::notification
