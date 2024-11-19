/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NotificationChild.h"

#include "WindowGlobalChild.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/Notification.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "nsFocusManager.h"

namespace mozilla::dom::notification {

using IPCResult = mozilla::ipc::IPCResult;

NS_IMPL_ISUPPORTS(NotificationChild, nsISupports);

NotificationChild::NotificationChild(Notification* aNonPersistentNotification,
                                     WindowGlobalChild* aWindow)
    : mNonPersistentNotification(aNonPersistentNotification), mWindow(aWindow) {
  if (mWindow) {
    BindToOwner(mWindow->GetWindowGlobal()->AsGlobal());
    return;
  }
}

class FocusWindowRunnable : public WorkerMainThreadRunnable {
 public:
  explicit FocusWindowRunnable(WorkerPrivate* aWorkerPrivate)
      : WorkerMainThreadRunnable(aWorkerPrivate,
                                 "Notification :: FocusWindowRunnable"_ns) {}

 protected:
  // Runnables don't support MOZ_CAN_RUN_SCRIPT, bug 1535398
  MOZ_CAN_RUN_SCRIPT_BOUNDARY bool MainThreadRun() override {
    RefPtr<nsPIDOMWindowInner> inner = mWorkerRef->Private()->GetWindow();
    if (inner->IsCurrentInnerWindow()) {
      nsCOMPtr<nsPIDOMWindowOuter> outer = inner->GetOuterWindow();
      nsFocusManager::FocusWindow(outer, CallerType::System);
    }
    return true;
  }
};

// Step 2 of https://notifications.spec.whatwg.org/#activating-a-notification
// MOZ_CAN_RUN_SCRIPT_BOUNDARY because of DispatchEvent (boundary for now, bug
// 1748910) and FocusWindow.
// Bug 1539864 for IPDL not able to handle MOZ_CAN_RUN_SCRIPT.
//
// Note that FrozenCallback below makes sure we don't do anything here on
// bfcached page.
MOZ_CAN_RUN_SCRIPT_BOUNDARY IPCResult NotificationChild::RecvNotifyClick() {
  // Step 2.1: Let intoFocus be the result of firing an event named click on the
  // Notification object representing notification, with its cancelable
  // attribute initialized to true.
  bool intoFocus = true;
  if (mNonPersistentNotification) {
    RefPtr<Event> event =
        NS_NewDOMEvent(mNonPersistentNotification, nullptr, nullptr);
    event->InitEvent(u"click"_ns, /* canBubble */ false, /* cancelable */ true);
    event->SetTrusted(true);
    WantsPopupControlCheck popupControlCheck(event);
    intoFocus = mNonPersistentNotification->DispatchEvent(
        *event, CallerType::System, IgnoreErrors());
  }

  if (!intoFocus) {
    return IPC_OK();
  }

  // Step 2.2: If intoFocus is true, then the user agent should bring the
  // notification’s related browsing context’s viewport into focus.
  if (mWindow) {
    if (RefPtr<nsGlobalWindowInner> inner = mWindow->GetWindowGlobal()) {
      if (inner->IsCurrentInnerWindow()) {
        nsCOMPtr<nsPIDOMWindowOuter> outer = inner->GetOuterWindow();
        nsFocusManager::FocusWindow(outer, CallerType::System);
      }
    }
  } else if (WorkerPrivate* wp = GetCurrentThreadWorkerPrivate()) {
    if (!wp->IsDedicatedWorker()) {
      // Only dedicated worker has a window to focus.
      return IPC_OK();
    }

    RefPtr<FocusWindowRunnable> runnable =
        new FocusWindowRunnable(wp->GetTopLevelWorker());
    runnable->Dispatch(wp, Canceling, IgnoreErrors());
  }
  return IPC_OK();
}

void NotificationChild::ActorDestroy(ActorDestroyReason aWhy) {
  if (RefPtr<Notification> notification = mNonPersistentNotification.get()) {
    // We are being closed because the parent actor is gone, and that means the
    // notification is closed
    notification->MaybeNotifyClose();
  }
}

void NotificationChild::FrozenCallback(nsIGlobalObject* aOwner) {
  // Make sure the closure below won't dispatch close event and still allow
  // explicit close() call.
  mNonPersistentNotification = nullptr;
  // Closing on FrozenCallback makes sure that clicking the notification opens a
  // new tab instead of pinging an inactive tab
  Close();
  DisconnectFreezeObserver();
}

}  // namespace mozilla::dom::notification
