/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_NOTIFICATION_NOTIFICATIONCHILD_H_
#define DOM_NOTIFICATION_NOTIFICATIONCHILD_H_

#include "mozilla/GlobalFreezeObserver.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/dom/notification/PNotificationChild.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom {
class Notification;
class WindowGlobalChild;
}  // namespace mozilla::dom

namespace mozilla::dom::notification {

class NotificationChild final : public PNotificationChild,
                                public GlobalFreezeObserver,
                                public SupportsWeakPtr {
  using IPCResult = mozilla::ipc::IPCResult;

  NS_DECL_ISUPPORTS

 public:
  explicit NotificationChild(Notification* aNonPersistentNotification,
                             WindowGlobalChild* aWindow);

  IPCResult RecvNotifyClick();

  void ActorDestroy(ActorDestroyReason aWhy) override;
  void FrozenCallback(nsIGlobalObject* aOwner) override;

 private:
  ~NotificationChild() = default;

  WeakPtr<Notification> mNonPersistentNotification;
  WeakPtr<WindowGlobalChild> mWindow;
};

}  // namespace mozilla::dom::notification

#endif  // DOM_NOTIFICATION_NOTIFICATIONCHILD_H_
