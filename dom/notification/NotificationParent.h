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

enum class CloseMode;

class NotificationParent final : public PNotificationParent,
                                 public nsIObserver {
  using IPCResult = mozilla::ipc::IPCResult;

 public:
  // Threadsafe to pass the class from PBackground to main thread
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  NotificationParent(NotNull<nsIPrincipal*> aPrincipal,
                     NotNull<nsIPrincipal*> aEffectiveStoragePrincipal,
                     bool aIsSecureContext, const nsAString& aId,
                     const nsAString& aScope,
                     const IPCNotificationOptions& aOptions)
      : mPrincipal(aPrincipal),
        mEffectiveStoragePrincipal(aEffectiveStoragePrincipal),
        mIsSecureContext(aIsSecureContext),
        mId(aId),
        mScope(aScope),
        mOptions(aOptions) {};

  IPCResult RecvShow(ShowResolver&& aResolver);
  IPCResult RecvClose();

  nsresult BindToMainThread(
      Endpoint<PNotificationParent>&& aParentEndpoint,
      PBackgroundParent::CreateNotificationParentResolver&& aResolver);

  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  ~NotificationParent() = default;

  nsresult Show();
  nsresult FireClickEvent();
  nsresult FireCloseEvent();

  void Unregister(CloseMode aCloseMode);

  void GetAlertName(nsAString& aRetval) {
    if (mAlertName.IsEmpty()) {
      MaybeInitAlertName();
    }
    aRetval = mAlertName;
  }
  void MaybeInitAlertName();

  Maybe<NotificationParent::ShowResolver> mResolver;

  NotNull<nsCOMPtr<nsIPrincipal>> mPrincipal;
  NotNull<nsCOMPtr<nsIPrincipal>> mEffectiveStoragePrincipal;
  bool mIsSecureContext;
  nsString mId;
  nsString mScope;
  IPCNotificationOptions mOptions;

  nsString mAlertName;

  // Whether it's now a dangling actor without corresponding OS notification,
  // either because it's closed or denied permission. We don't have to call
  // CloseAlert if this is the case.
  bool mDangling = false;
};

}  // namespace mozilla::dom::notification

#endif  // DOM_NOTIFICATION_NOTIFICATIONPARENT_H_
