/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CloseWatcher_h
#define mozilla_dom_CloseWatcher_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/CloseWatcherBinding.h"
#include "mozilla/dom/DebuggerNotificationBinding.h"

namespace mozilla::dom {

class CloseWatcher : public DOMEventTargetHelper, public AbortFollower {
 public:
  NS_DECL_ISUPPORTS_INHERITED

  nsIGlobalObject* GetParentObject() const { return GetOwnerGlobal(); }

  mozilla::Maybe<EventCallbackDebuggerNotificationType>
  GetDebuggerNotificationType() const override {
    return mozilla::Some(EventCallbackDebuggerNotificationType::Closewatcher);
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  IMPL_EVENT_HANDLER(cancel);
  IMPL_EVENT_HANDLER(close);

  static already_AddRefed<CloseWatcher> Constructor(
      const GlobalObject& aGlobal, const CloseWatcherOptions& aOptions,
      ErrorResult& aRv);

  explicit CloseWatcher(nsPIDOMWindowInner* aWindow)
      : DOMEventTargetHelper(aWindow) {}

  // The IDL binding for RequestClose returns void so that the history
  // consumption is not observable.
  MOZ_CAN_RUN_SCRIPT void RequestClose() { RequestToClose(false); }

  // RequestToClose returns a boolean so callers can determine if the action
  // was handled, so that other fallback behaviours can be executed.
  MOZ_CAN_RUN_SCRIPT bool RequestToClose(bool aRequireHistoryActionActivation);

  MOZ_CAN_RUN_SCRIPT void Close();

  void AddToWindowsCloseWatcherManager();

  void Destroy();

  // AbortFollower
  void RunAbortAlgorithm() override;

  bool IsActive() const;

  void SetEnabled(bool aEnabled) { mEnabled = aEnabled; }

  void DisconnectFromOwner() override {
    Destroy();
    DOMEventTargetHelper::DisconnectFromOwner();
  }

 protected:
  virtual ~CloseWatcher() = default;

  bool mIsRunningCancelAction = false;

  // https://html.spec.whatwg.org/multipage/interaction.html#create-close-watcher-getenabledstate
  // HTMLDialogElement can enable/disable close watcher using ClosedBy=none
  bool mEnabled = true;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_CloseWatcher_h
