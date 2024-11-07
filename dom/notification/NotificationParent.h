/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_NOTIFICATION_NOTIFICATIONPARENT_H_
#define DOM_NOTIFICATION_NOTIFICATIONPARENT_H_

#include "mozilla/dom/notification/PNotificationParent.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/dom/DOMTypes.h"

namespace mozilla::dom::notification {

class NotificationParent final : public PNotificationParent,
                                 public nsIObserver {
  using IPCResult = mozilla::ipc::IPCResult;

 public:
  // Threadsafe to pass the class from PBackground to main thread
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  IPCResult RecvShow(ShowResolver&& aResolver);
  IPCResult RecvClose();

 private:
  ~NotificationParent() = default;
};

}  // namespace mozilla::dom::notification

#endif  // DOM_NOTIFICATION_NOTIFICATIONPARENT_H_
