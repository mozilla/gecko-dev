/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_NOTIFICATION_NOTIFICATIONUTILS_H_
#define DOM_NOTIFICATION_NOTIFICATIONUTILS_H_

#include <cstdint>
#include "nsCOMPtr.h"
#include "nsStringFwd.h"

enum class nsresult : uint32_t;
class nsIPrincipal;
class nsINotificationStorage;
namespace mozilla::dom {
enum class NotificationPermission : uint8_t;
class Document;
}  // namespace mozilla::dom

namespace mozilla::dom {
class IPCNotificationOptions;
}

namespace mozilla::dom::notification {

/**
 * Retrieves raw notification permission directly from PermissionManager.
 */
NotificationPermission GetRawNotificationPermission(nsIPrincipal* aPrincipal);

enum class PermissionCheckPurpose : uint8_t {
  PermissionRequest,
  PermissionAttribute,
  NotificationShow,
};

/**
 * Returns true if the current principal must be given notification
 * permission, regardless of the permission status. This one should be dominant
 * compared to FobbiddenFor below.
 */
bool IsNotificationAllowedFor(nsIPrincipal* aPrincipal);

/**
 * Returns true if the current principal must not be given notification
 * permission, regardless of the permission status.
 *
 * @param aRequestorDoc The Document object from the page requesting permission.
 *                      Pass only when this is for requestNotification().
 */
bool IsNotificationForbiddenFor(nsIPrincipal* aPrincipal,
                                nsIPrincipal* aEffectiveStoragePrincipal,
                                bool isSecureContext,
                                PermissionCheckPurpose aPurpose,
                                Document* aRequestorDoc = nullptr);

/**
 * Retrieves notification permission based on the context.
 */
NotificationPermission GetNotificationPermission(
    nsIPrincipal* aPrincipal, nsIPrincipal* aEffectiveStoragePrincipal,
    bool isSecureContext, PermissionCheckPurpose aPurpose);

nsCOMPtr<nsINotificationStorage> GetNotificationStorage(bool isPrivate);

nsresult GetOrigin(nsIPrincipal* aPrincipal, nsString& aOrigin);

void ComputeAlertName(nsIPrincipal* aPrincipal, const nsString& aTag,
                      const nsString& aId, nsString& aResult);

nsresult PersistNotification(nsIPrincipal* aPrincipal, const nsString& aId,
                             const nsString& aAlertName,
                             const IPCNotificationOptions& aOptions,
                             const nsString& aScope);
nsresult UnpersistNotification(nsIPrincipal* aPrincipal, const nsString& aId);

enum class CloseMode {
  CloseMethod,
  // Either on global teardown or freeze
  InactiveGlobal,
};
void UnregisterNotification(nsIPrincipal* aPrincipal, const nsString& aId,
                            const nsString& aAlertName, CloseMode aCloseMode);

nsresult RemovePermission(nsIPrincipal* aPrincipal);
nsresult OpenSettings(nsIPrincipal* aPrincipal);

enum class NotificationStatusChange { Shown, Closed };
nsresult AdjustPushQuota(nsIPrincipal* aPrincipal,
                         NotificationStatusChange aChange);

}  // namespace mozilla::dom::notification

#endif  // DOM_NOTIFICATION_NOTIFICATIONUTILS_H_
