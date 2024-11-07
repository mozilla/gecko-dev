/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_NOTIFICATION_NOTIFICATIONUTILS_H_
#define DOM_NOTIFICATION_NOTIFICATIONUTILS_H_

#include <cstdint>

class nsIPrincipal;

namespace mozilla::dom {
enum class NotificationPermission : uint8_t;
class Document;
}  // namespace mozilla::dom

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

}  // namespace mozilla::dom::notification

#endif  // DOM_NOTIFICATION_NOTIFICATIONUTILS_H_
