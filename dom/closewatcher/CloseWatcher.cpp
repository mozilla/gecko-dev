/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CloseWatcher.h"

#include "mozilla/dom/CloseWatcherBinding.h"
#include "mozilla/dom/CloseWatcherManager.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/WindowContext.h"
#include "mozilla/RefPtr.h"
#include "nsGlobalWindowInner.h"

namespace mozilla::dom {

NS_IMPL_ISUPPORTS_INHERITED0(CloseWatcher, DOMEventTargetHelper)

already_AddRefed<CloseWatcher> CloseWatcher::Constructor(
    const GlobalObject& aGlobal, const CloseWatcherOptions& aOptions,
    ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<nsPIDOMWindowInner> window = global->GetAsInnerWindow();
  if (!window || !window->IsFullyActive()) {
    aRv.ThrowInvalidStateError("The document is not fully active.");
    return nullptr;
  }

  RefPtr<CloseWatcher> watcher = new CloseWatcher(window);

  AbortSignal* signal = nullptr;
  if (aOptions.mSignal.WasPassed()) {
    signal = &aOptions.mSignal.Value();
  }
  if (signal && signal->Aborted()) {
    return watcher.forget();
  }
  if (signal) {
    watcher->Follow(signal);
  }

  watcher->AddToWindowsCloseWatcherManager();
  return watcher.forget();
}

JSObject* CloseWatcher::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return CloseWatcher_Binding::Wrap(aCx, this, aGivenProto);
}

// https://html.spec.whatwg.org/multipage/interaction.html#close-watcher-request-close
bool CloseWatcher::RequestToClose(bool aRequireHistoryActionActivation) {
  // 1. If closeWatcher is not active, then return true.
  // 2. If the result of running closeWatcher's get enabled state is false, then
  // return true.
  // 3. If closeWatcher's is running cancel action is true, then return true.
  // 4. Let window be closeWatcher's window.
  // 5. If window's associated Document is not fully active, then return true.
  if (!IsActive() || mIsRunningCancelAction) {
    return true;
  }
  MOZ_ASSERT(GetOwnerWindow());
  RefPtr<WindowContext> winCtx = GetOwnerWindow()->GetWindowContext();
  RefPtr<CloseWatcherManager> manager =
      GetOwnerWindow()->EnsureCloseWatcherManager();
  EventInit init;
  init.mBubbles = false;
  // 6. Let canPreventClose be true if window's close watcher manager's groups's
  // size is less than window's close watcher manager's allowed number of
  // groups, and window has history-action activation; otherwise false.
  init.mCancelable =
      !aRequireHistoryActionActivation ||
      (manager->CanGrow() && winCtx->HasValidHistoryActivation());
  RefPtr<Event> event = Event::Constructor(this, u"cancel"_ns, init);
  event->SetTrusted(true);
  // 7. Set closeWatcher's is running cancel action to true.
  mIsRunningCancelAction = true;
  // 8. Let shouldContinue be the result of running closeWatcher's cancel action
  // given canPreventClose.
  DispatchEvent(*event);
  // 9. Set closeWatcher's is running cancel action to false.
  mIsRunningCancelAction = false;
  // 10. If shouldContinue is false, then:
  if (event->DefaultPrevented()) {
    // 10.2. Consume history-action user activation given window.
    winCtx->ConsumeHistoryActivation();
    // 10.3. Return false.
    return false;
  }
  // 11. Close closeWatcher.
  Close();
  // 12. Return true.
  return true;
}

// https://html.spec.whatwg.org/multipage/interaction.html#close-watcher-close
void CloseWatcher::Close() {
  // 1. If closeWatcher is not active, then return.
  // 2. If the result of running closeWatcher's get enabled state is false, then
  // return true.
  // 3. If closeWatcher's window's associated Document is not fully active, then
  // return.
  if (!IsActive()) {
    return;
  }
  // 4. Destroy closeWatcher.
  Destroy();
  EventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  RefPtr<Event> event = Event::Constructor(this, u"close"_ns, init);
  event->SetTrusted(true);
  DispatchEvent(*event);
}

void CloseWatcher::AddToWindowsCloseWatcherManager() {
  if (auto* window = GetOwnerWindow()) {
    window->EnsureCloseWatcherManager()->Add(*this);
  }
}

void CloseWatcher::Destroy() {
  if (auto* window = GetOwnerWindow()) {
    window->EnsureCloseWatcherManager()->Remove(*this);
  }
}

bool CloseWatcher::IsActive() const {
  if (!mEnabled) {
    return false;
  }
  if (auto* window = GetOwnerWindow()) {
    return window->IsFullyActive() &&
           window->EnsureCloseWatcherManager()->Contains(*this);
  }
  return false;
}

void CloseWatcher::RunAbortAlgorithm() { Destroy(); }

}  // namespace mozilla::dom
