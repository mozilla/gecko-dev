/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NotificationParent.h"

namespace mozilla::dom::notification {

NS_IMPL_ISUPPORTS(NotificationParent, nsIObserver)

NS_IMETHODIMP
NotificationParent::Observe(nsISupports* aSubject, const char* aTopic,
                            const char16_t* aData) {
  return NS_OK;
}

mozilla::ipc::IPCResult NotificationParent::RecvShow(ShowResolver&& aResolver) {
  return IPC_OK();
}

}  // namespace mozilla::dom::notification
