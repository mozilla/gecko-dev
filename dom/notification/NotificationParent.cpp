/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NotificationParent.h"

#include "NotificationUtils.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/Components.h"
#include "nsComponentManagerUtils.h"
#include "nsIAlertsService.h"

namespace mozilla::dom::notification {

NS_IMPL_ISUPPORTS(NotificationParent, nsIObserver)

NS_IMETHODIMP
NotificationParent::Observe(nsISupports* aSubject, const char* aTopic,
                            const char16_t* aData) {
  if (!strcmp("alertdisablecallback", aTopic)) {
    return RemovePermission(mPrincipal);
  }
  if (!strcmp("alertsettingscallback", aTopic)) {
    return OpenSettings(mPrincipal);
  }
  if (!strcmp("alertshow", aTopic)) {
    (void)NS_WARN_IF(NS_FAILED(
        AdjustPushQuota(mPrincipal, NotificationStatusChange::Shown)));
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
  if (!strcmp("alertfinished", aTopic)) {
    (void)NS_WARN_IF(NS_FAILED(
        AdjustPushQuota(mPrincipal, NotificationStatusChange::Closed)));
    if (mResolver) {
      // alertshow happens first before alertfinished, and it should have If
      // not it means it failed to show and is bailing out.
      // XXX: Apparently XUL manual do not disturb mode does this without firing
      // alertshow at all.
      mResolver.take().value()(CopyableErrorResult(NS_ERROR_FAILURE));
      return NS_OK;
    }
  }
  return NS_OK;
}

// Step 4 of
// https://notifications.spec.whatwg.org/#dom-notification-notification
mozilla::ipc::IPCResult NotificationParent::RecvShow(ShowResolver&& aResolver) {
  mResolver.emplace(std::move(aResolver));

  // Step 4.2: Run the fetch steps for notification. (Will happen in
  // nsIAlertNotification::LoadImage)
  // Step 4.3: Run the show steps for notification.
  nsresult rv = Show();
  if (NS_FAILED(rv)) {
    mResolver.take().value()(CopyableErrorResult(rv));
  }
  // If not failed, the resolver will be called asynchronously by
  // NotificationObserver
  return IPC_OK();
}

nsresult NotificationParent::Show() {
  // Step 4.3 the show steps, which are almost all about processing `tag` and
  // then displaying the notification. Both are handled by
  // nsIAlertsService::ShowAlert/PersistentNotification. The below is all about
  // constructing the observer (for show and close events) right and ultimately
  // call the alerts service function.

  // XXX(krosylight): Non-persistent notifications probably don't need this
  nsAutoString alertName;
  GetAlertName(alertName);
  nsresult rv =
      PersistNotification(mPrincipal, mId, alertName, mOptions, mScope);
  if (NS_FAILED(rv)) {
    NS_WARNING("Could not persist Notification");
  }

  // In the case of IPC, the parent process uses the cookie to map to
  // nsIObserver. Thus the cookie must be unique to differentiate observers.
  // XXX(krosylight): This is about ContentChild::mAlertObserver which is not
  // useful when called by the parent process. This should be removed when we
  // make nsIAlertsService parent process only.
  nsString obsoleteCookie = u"notification:"_ns;

  bool requireInteraction = mOptions.requireInteraction();
  if (!StaticPrefs::dom_webnotifications_requireinteraction_enabled()) {
    requireInteraction = false;
  }

  nsCOMPtr<nsIAlertNotification> alert =
      do_CreateInstance(ALERT_NOTIFICATION_CONTRACTID);
  if (!alert) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  MOZ_TRY(alert->Init(alertName, mOptions.icon(), mOptions.title(),
                      mOptions.body(), true, obsoleteCookie,
                      NS_ConvertASCIItoUTF16(GetEnumString(mOptions.dir())),
                      mOptions.lang(), mOptions.dataSerialized(), mPrincipal,
                      mPrincipal->GetIsInPrivateBrowsing(), requireInteraction,
                      mOptions.silent(), mOptions.vibrate()));

  nsCOMPtr<nsIAlertsService> alertService = components::Alerts::Service();

  JSONStringWriteFunc<nsAutoCString> persistentData;
  JSONWriter w(persistentData);
  w.Start();

  nsAutoString origin;
  GetOrigin(mPrincipal, origin);
  w.StringProperty("origin", NS_ConvertUTF16toUTF8(origin));

  w.StringProperty("id", NS_ConvertUTF16toUTF8(mId));

  nsAutoCString originSuffix;
  mPrincipal->GetOriginSuffix(originSuffix);
  w.StringProperty("originSuffix", originSuffix);

  w.End();

  MOZ_TRY(alertService->ShowPersistentNotification(
      NS_ConvertUTF8toUTF16(persistentData.StringCRef()), alert, this));

#ifdef ANDROID
  // XXX: the Android nsIAlertsService is broken and doesn't send alertshow
  // properly, which means we cannot depend on it to resolve the promise. For
  // now we resolve the promise here.
  // (This now fires onshow event regardless of the actual result, but it should
  // be better than the previous behavior that did not do anything at all)
  mResolver.take().value()(CopyableErrorResult());
#endif

  return NS_OK;
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
