/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_NOTIFICATION_NOTIFICATIONCHILD_H_
#define DOM_NOTIFICATION_NOTIFICATIONCHILD_H_

#include "mozilla/WeakPtr.h"
#include "mozilla/dom/notification/PNotificationChild.h"

namespace mozilla::dom::notification {

class NotificationChild final : public PNotificationChild,
                                public SupportsWeakPtr {
  using IPCResult = mozilla::ipc::IPCResult;

  NS_INLINE_DECL_REFCOUNTING(NotificationChild)

 public:
  explicit NotificationChild();

  IPCResult RecvNotifyClick();

 private:
  ~NotificationChild() = default;
};

}  // namespace mozilla::dom::notification

#endif  // DOM_NOTIFICATION_NOTIFICATIONCHILD_H_
