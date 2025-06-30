/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NotificationParent.h"

#include "nsThreadUtils.h"
#include "NotificationUtils.h"
#include "mozilla/AlertNotification.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/dom/ClientOpenWindowUtils.h"
#include "mozilla/dom/ServiceWorkerManager.h"
#include "mozilla/ipc/Endpoint.h"
#include "nsComponentManagerUtils.h"
#include "nsIServiceWorkerManager.h"

namespace mozilla::dom::notification {

NS_IMPL_ISUPPORTS0(NotificationParent)

// TODO(krosylight): Would be nice to replace nsIObserver with something like:
//
// nsINotificationManager.NotifyClick(notification.id [, notification.action])
class NotificationObserver final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS

  NotificationObserver(const nsAString& aScope, nsIPrincipal* aPrincipal,
                       IPCNotification aNotification,
                       NotificationParent& aParent)
      : mScope(aScope),
        mPrincipal(aPrincipal),
        mNotification(std::move(aNotification)),
        mActor(&aParent) {}

  NS_IMETHODIMP Observe(nsISupports* aSubject, const char* aTopic,
                        const char16_t* aData) override {
    AlertTopic topic = ToAlertTopic(aTopic);

    // These two never fire any content event directly
    if (topic == AlertTopic::Disable) {
      return RemovePermission(mPrincipal);
    }
    if (topic == AlertTopic::Settings) {
      return OpenSettings(mPrincipal);
    }

    RefPtr<NotificationParent> actor(mActor);

    if (actor && actor->CanSend()) {
      // The actor is alive, call it to ping the content process and/or to make
      // it clean up itself
      actor->HandleAlertTopic(topic);
      if (mScope.IsEmpty()) {
        // The actor covered everything we need.
        return NS_OK;
      }
    } else if (mScope.IsEmpty()) {
      if (topic == AlertTopic::Click) {
        // No actor there, we need to open up a window ourselves
        return OpenWindow();
      }
      // Nothing to do
      return NS_OK;
    }

    // We have a Service Worker to call
    MOZ_ASSERT(!mScope.IsEmpty());
    if (topic == AlertTopic::Show) {
      (void)NS_WARN_IF(NS_FAILED(
          AdjustPushQuota(mPrincipal, NotificationStatusChange::Shown)));
      nsresult rv = PersistNotification(mPrincipal, mNotification, mScope);
      if (NS_FAILED(rv)) {
        NS_WARNING("Could not persist Notification");
      }
      return NS_OK;
    }

    MOZ_ASSERT(topic == AlertTopic::Click || topic == AlertTopic::Finished);

    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (!swm) {
      return NS_ERROR_FAILURE;
    }

    nsAutoCString originSuffix;
    MOZ_TRY(mPrincipal->GetOriginSuffix(originSuffix));

    if (topic == AlertTopic::Click) {
      nsCOMPtr<nsIAlertAction> action = do_QueryInterface(aSubject);
      nsAutoString actionName;
      if (action) {
        MOZ_TRY(action->GetAction(actionName));
      }
      nsresult rv = swm->SendNotificationClickEvent(originSuffix, mScope,
                                                    mNotification, actionName);
      if (NS_FAILED(rv)) {
        // No active service worker, let's do the last resort
        return OpenWindow();
      }
      return NS_OK;
    }

    MOZ_ASSERT(topic == AlertTopic::Finished);
    (void)NS_WARN_IF(NS_FAILED(
        AdjustPushQuota(mPrincipal, NotificationStatusChange::Closed)));
    (void)NS_WARN_IF(
        NS_FAILED(UnpersistNotification(mPrincipal, mNotification.id())));
    (void)swm->SendNotificationCloseEvent(originSuffix, mScope, mNotification);

    return NS_OK;
  }

 private:
  virtual ~NotificationObserver() = default;

  static AlertTopic ToAlertTopic(const char* aTopic) {
    if (!strcmp("alertdisablecallback", aTopic)) {
      return AlertTopic::Disable;
    }
    if (!strcmp("alertsettingscallback", aTopic)) {
      return AlertTopic::Settings;
    }
    if (!strcmp("alertclickcallback", aTopic)) {
      return AlertTopic::Click;
    }
    if (!strcmp("alertshow", aTopic)) {
      return AlertTopic::Show;
    }
    if (!strcmp("alertfinished", aTopic)) {
      return AlertTopic::Finished;
    }
    MOZ_ASSERT_UNREACHABLE("Unknown alert topic");
    return AlertTopic::Finished;
  }

  nsresult OpenWindow() {
    nsAutoCString origin;
    MOZ_TRY(mPrincipal->GetOrigin(origin));

    // XXX: We should be able to just pass nsIPrincipal directly
    mozilla::ipc::PrincipalInfo info{};
    MOZ_TRY(PrincipalToPrincipalInfo(mPrincipal, &info));

    (void)ClientOpenWindow(
        nullptr, ClientOpenWindowArgs(info, Nothing(), ""_ns, origin));
    return NS_OK;
  }

  // May want to replace with SWR ID, see bug 1881812
  nsString mScope;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  IPCNotification mNotification;
  WeakPtr<NotificationParent> mActor;
};

NS_IMPL_ISUPPORTS(NotificationObserver, nsIObserver)

nsresult NotificationParent::HandleAlertTopic(AlertTopic aTopic) {
  if (aTopic == AlertTopic::Click) {
    return FireClickEvent();
  }
  if (aTopic == AlertTopic::Show) {
    if (!mResolver) {
#ifdef ANDROID
      // XXX: This can happen as we resolve showNotification() immediately on
      // Android for now and a mock service may still call this.
      return NS_OK;
#else
      MOZ_ASSERT_UNREACHABLE("Are we getting double show events?");
      return NS_ERROR_FAILURE;
#endif
    }
    mResolver.take().value()(CopyableErrorResult());
    return NS_OK;
  }
  if (aTopic == AlertTopic::Finished) {
    if (mResolver) {
      // alertshow happens first before alertfinished, and it should have
      // nullified mResolver. If not it means it failed to show and is bailing
      // out.
      // NOTE(krosylight): The spec does not define what to do when a
      // permission-granted notification fails to open, we throw TypeError here
      // as that's the error for when permission is denied.
      CopyableErrorResult rv;
      rv.ThrowTypeError(
          "Failed to show notification, potentially because the browser did "
          "not have the corresponding OS-level permission."_ns);
      mResolver.take().value()(rv);
    }

    // Unpersisted already and being unregistered already by nsIAlertsService
    mDangling = true;
    Close();

    return NS_OK;
  }

  MOZ_ASSERT_UNREACHABLE("Unknown notification topic");

  return NS_OK;
}

nsresult NotificationParent::FireClickEvent() {
  if (!mArgs.mScope.IsEmpty()) {
    return NS_OK;
  }
  if (SendNotifyClick()) {
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

// Step 4 of
// https://notifications.spec.whatwg.org/#dom-notification-notification
mozilla::ipc::IPCResult NotificationParent::RecvShow(ShowResolver&& aResolver) {
  MOZ_ASSERT(mId.IsEmpty(), "ID should not be given for a new notification");

  mResolver.emplace(std::move(aResolver));

  // Step 4.1: If the result of getting the notifications permission state is
  // not "granted", then queue a task to fire an event named error on this, and
  // abort these steps.
  NotificationPermission permission = GetNotificationPermission(
      mArgs.mPrincipal, mArgs.mEffectiveStoragePrincipal,
      mArgs.mIsSecureContext, PermissionCheckPurpose::NotificationShow);
  if (permission != NotificationPermission::Granted) {
    CopyableErrorResult rv;
    rv.ThrowTypeError("Permission to show Notification denied.");
    mResolver.take().value()(rv);
    mDangling = true;
    return IPC_OK();
  }

  // Step 4.2: Run the fetch steps for notification. (Will happen in
  // nsIAlertNotification::LoadImage)
  // Step 4.3: Run the show steps for notification.
  nsresult rv = Show();
  // It's possible that we synchronously received a notification while in Show,
  // so mResolver may now be empty.
  if (NS_FAILED(rv) && mResolver) {
    mResolver.take().value()(CopyableErrorResult(rv));
  }
  // If not failed, the resolver will be called asynchronously by
  // NotificationObserver
  return IPC_OK();
}

nsresult NotificationParent::Show() {
  // Step 4.3 the show steps, which are almost all about processing `tag` and
  // then displaying the notification. Both are handled by
  // nsIAlertsService::ShowAlert. The below is all about constructing the
  // observer (for show and close events) right and ultimately call the alerts
  // service function.

  // In the case of IPC, the parent process uses the cookie to map to
  // nsIObserver. Thus the cookie must be unique to differentiate observers.
  // XXX(krosylight): This is about ContentChild::mAlertObserver which is not
  // useful when called by the parent process. This should be removed when we
  // make nsIAlertsService parent process only.
  nsString obsoleteCookie = u"notification:"_ns;

  const IPCNotificationOptions& options = mArgs.mNotification.options();

  bool requireInteraction = options.requireInteraction();
  if (!StaticPrefs::dom_webnotifications_requireinteraction_enabled()) {
    requireInteraction = false;
  }

  nsCOMPtr<nsIAlertNotification> alert =
      do_CreateInstance(ALERT_NOTIFICATION_CONTRACTID);
  if (!alert) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIPrincipal> principal = mArgs.mPrincipal;
  MOZ_TRY(alert->Init(options.tag(), options.icon(), options.title(),
                      options.body(), true, obsoleteCookie,
                      NS_ConvertASCIItoUTF16(GetEnumString(options.dir())),
                      options.lang(), options.dataSerialized(), principal,
                      principal->GetIsInPrivateBrowsing(), requireInteraction,
                      options.silent(), options.vibrate()));

  nsTArray<RefPtr<nsIAlertAction>> actions;
  MOZ_ASSERT(options.actions().Length() <= kMaxActions);
  for (const auto& action : options.actions()) {
    actions.AppendElement(new AlertAction(action.name(), action.title()));
  }

  alert->SetActions(actions);

  MOZ_TRY(alert->GetId(mId));

  RefPtr<NotificationObserver> observer = new NotificationObserver(
      mArgs.mScope, principal, IPCNotification(mId, options), *this);
  MOZ_TRY(ShowAlertWithCleanup(alert, observer));

#ifdef ANDROID
  // XXX: the Android nsIAlertsService is broken and doesn't send alertshow
  // properly, so we call it here manually.
  // (This now fires onshow event regardless of the actual result, but it should
  // be better than the previous behavior that did not do anything at all)
  observer->Observe(nullptr, "alertshow", nullptr);
#endif

  return NS_OK;
}

mozilla::ipc::IPCResult NotificationParent::RecvClose() {
  Unregister();
  Close();
  return IPC_OK();
}

void NotificationParent::Unregister() {
  if (mDangling) {
    // We had no permission, so nothing to clean up.
    return;
  }

  mDangling = true;
  UnregisterNotification(mArgs.mPrincipal, mId);
}

nsresult NotificationParent::CreateOnMainThread(
    NotificationParentArgs&& mArgs,
    Endpoint<PNotificationParent>&& aParentEndpoint,
    PBackgroundParent::CreateNotificationParentResolver&& aResolver) {
  if (mArgs.mNotification.options().actions().Length() > kMaxActions) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIThread> thread = NS_GetCurrentThread();

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "NotificationParent::BindToMainThread",
      [args = std::move(mArgs), endpoint = std::move(aParentEndpoint),
       resolver = std::move(aResolver), thread]() mutable {
        RefPtr<NotificationParent> actor =
            new NotificationParent(std::move(args));
        bool result = endpoint.Bind(actor);
        thread->Dispatch(NS_NewRunnableFunction(
            "NotificationParent::BindToMainThreadResult",
            [result, resolver = std::move(resolver)]() { resolver(result); }));
      }));

  return NS_OK;
}

}  // namespace mozilla::dom::notification
