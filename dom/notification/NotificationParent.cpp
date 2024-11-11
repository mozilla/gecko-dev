/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NotificationParent.h"

#include "NotificationUtils.h"
#include "mozilla/ipc/Endpoint.h"

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

nsresult NotificationParent::BindToMainThread(
    Endpoint<PNotificationParent>&& aParentEndpoint,
    PBackgroundParent::CreateNotificationParentResolver&& aResolver) {
  nsCOMPtr<nsIThread> thread = NS_GetCurrentThread();

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "NotificationParent::BindToMainThread",
      [self = RefPtr(this), endpoint = std::move(aParentEndpoint),
       resolver = std::move(aResolver), thread]() mutable {
        bool result = endpoint.Bind(self);
        thread->Dispatch(NS_NewRunnableFunction(
            "NotificationParent::BindToMainThreadResult",
            [result, resolver = std::move(resolver)]() { resolver(result); }));
      }));

  return NS_OK;
}

void NotificationParent::ActorDestroy(ActorDestroyReason aWhy) {
  nsAutoString alertName;
  GetAlertName(alertName);
  UnregisterNotification(mPrincipal, mId, alertName, CloseMode::InactiveGlobal);
}

void NotificationParent::MaybeInitAlertName() {
  if (!mAlertName.IsEmpty()) {
    return;
  }

  ComputeAlertName(mPrincipal, mOptions.tag(), mId, mAlertName);
}

}  // namespace mozilla::dom::notification
