/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGlobalWindowInner.h"

#include "mozilla/HoldDropJSObjects.h"

#include "mozilla/dom/AbortController.h"
#include "mozilla/dom/NavigateEvent.h"
#include "mozilla/dom/NavigateEventBinding.h"
#include "mozilla/dom/Navigation.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED_WITH_JS_MEMBERS(NavigateEvent, Event,
                                                   (mDestination, mSignal,
                                                    mFormData, mSourceElement,
                                                    mNavigationHandlerList,
                                                    mAbortController),
                                                   (mInfo))

NS_IMPL_ADDREF_INHERITED(NavigateEvent, Event)
NS_IMPL_RELEASE_INHERITED(NavigateEvent, Event)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(NavigateEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

JSObject* NavigateEvent::WrapObjectInternal(JSContext* aCx,
                                            JS::Handle<JSObject*> aGivenProto) {
  return NavigateEvent_Binding::Wrap(aCx, this, aGivenProto);
}

/* static */
already_AddRefed<NavigateEvent> NavigateEvent::Constructor(
    const GlobalObject& aGlobal, const nsAString& aType,
    const NavigateEventInit& aEventInitDict) {
  nsCOMPtr<mozilla::dom::EventTarget> eventTarget =
      do_QueryInterface(aGlobal.GetAsSupports());
  return Constructor(eventTarget, aType, aEventInitDict);
}

/* static */
already_AddRefed<NavigateEvent> NavigateEvent::Constructor(
    EventTarget* aEventTarget, const nsAString& aType,
    const NavigateEventInit& aEventInitDict) {
  RefPtr<NavigateEvent> event = new NavigateEvent(aEventTarget);
  bool trusted = event->Init(aEventTarget);
  event->InitEvent(
      aType, aEventInitDict.mBubbles ? CanBubble::eYes : CanBubble::eNo,
      aEventInitDict.mCancelable ? Cancelable::eYes : Cancelable::eNo,
      aEventInitDict.mComposed ? Composed::eYes : Composed::eNo);
  event->InitNavigateEvent(aEventInitDict);
  event->SetTrusted(trusted);
  return event.forget();
}

/* static */
already_AddRefed<NavigateEvent> NavigateEvent::Constructor(
    EventTarget* aEventTarget, const nsAString& aType,
    const NavigateEventInit& aEventInitDict,
    nsIStructuredCloneContainer* aClassicHistoryAPIState,
    AbortController* aAbortController) {
  RefPtr<NavigateEvent> event =
      Constructor(aEventTarget, aType, aEventInitDict);

  event->mAbortController = aAbortController;
  MOZ_DIAGNOSTIC_ASSERT(event->mSignal == aAbortController->Signal());

  event->mClassicHistoryAPIState = aClassicHistoryAPIState;

  return event.forget();
}

NavigationType NavigateEvent::NavigationType() const { return mNavigationType; }

already_AddRefed<NavigationDestination> NavigateEvent::Destination() const {
  return do_AddRef(mDestination);
}

bool NavigateEvent::CanIntercept() const { return mCanIntercept; }

bool NavigateEvent::UserInitiated() const { return mUserInitiated; }

bool NavigateEvent::HashChange() const { return mHashChange; }

AbortSignal* NavigateEvent::Signal() const { return mSignal; }

already_AddRefed<FormData> NavigateEvent::GetFormData() const {
  return do_AddRef(mFormData);
}

void NavigateEvent::GetDownloadRequest(nsAString& aDownloadRequest) const {
  aDownloadRequest = mDownloadRequest;
}

void NavigateEvent::GetInfo(JSContext* aCx,
                            JS::MutableHandle<JS::Value> aInfo) const {
  aInfo.set(mInfo);
}

bool NavigateEvent::HasUAVisualTransition() const {
  return mHasUAVisualTransition;
}

Element* NavigateEvent::GetSourceElement() const { return mSourceElement; }

void NavigateEvent::Intercept(const NavigationInterceptOptions& aOptions,
                              ErrorResult& aRv) {
  // This will be implemented in Bug 1897439.
}

void NavigateEvent::Scroll(ErrorResult& aRv) {
  // This will be implemented in Bug 1897439.
}

NavigateEvent::NavigateEvent(EventTarget* aOwner)
    : Event(aOwner, nullptr, nullptr) {
  mozilla::HoldJSObjects(this);
}

NavigateEvent::~NavigateEvent() { DropJSObjects(this); }

void NavigateEvent::InitNavigateEvent(const NavigateEventInit& aEventInitDict) {
  mNavigationType = aEventInitDict.mNavigationType;
  mDestination = aEventInitDict.mDestination;
  mCanIntercept = aEventInitDict.mCanIntercept;
  mUserInitiated = aEventInitDict.mUserInitiated;
  mHashChange = aEventInitDict.mHashChange;
  mSignal = aEventInitDict.mSignal;
  mFormData = aEventInitDict.mFormData;
  mDownloadRequest = aEventInitDict.mDownloadRequest;
  mInfo = aEventInitDict.mInfo;
  mHasUAVisualTransition = aEventInitDict.mHasUAVisualTransition;
  mSourceElement = aEventInitDict.mSourceElement;
}

void NavigateEvent::SetCanIntercept(bool aCanIntercept) {
  mCanIntercept = aCanIntercept;
}

enum NavigateEvent::InterceptionState NavigateEvent::InterceptionState() const {
  return mInterceptionState;
}

void NavigateEvent::SetInterceptionState(
    enum InterceptionState aInterceptionState) {
  mInterceptionState = aInterceptionState;
}

nsIStructuredCloneContainer* NavigateEvent::ClassicHistoryAPIState() const {
  return mClassicHistoryAPIState;
}

nsTArray<RefPtr<NavigationInterceptHandler>>&
NavigateEvent::NavigationHandlerList() {
  return mNavigationHandlerList;
}

// https://html.spec.whatwg.org/#navigateevent-finish
void NavigateEvent::Finish(bool aDidFulfill) {
  switch (mInterceptionState) {
    // Step 1
    case InterceptionState::Intercepted:
    case InterceptionState::Finished:
      MOZ_DIAGNOSTIC_ASSERT(false);
      break;
      // Step 2
    case InterceptionState::None:
      return;
    default:
      break;
  }

  // Step 3
  PotentiallyResetFocus();

  // Step 4
  if (aDidFulfill) {
    PotentiallyProcessScrollBehavior();
  }

  // Step 5
  mInterceptionState = InterceptionState::Finished;
}

// https://html.spec.whatwg.org/#potentially-reset-the-focus
void NavigateEvent::PotentiallyResetFocus() {
  // Step 1
  MOZ_DIAGNOSTIC_ASSERT(mInterceptionState == InterceptionState::Committed ||
                        mInterceptionState == InterceptionState::Scrolled);

  // Step 2
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(GetParentObject());

  // If we don't have a window here, there's not much we can do. This could
  // potentially happen in a chrome context, and in the end it's just better to
  // be sure and null check.
  if (NS_WARN_IF(!window)) {
    return;
  }

  Navigation* navigation = window->Navigation();

  // Step 3
  bool focusChanged = navigation->FocusedChangedDuringOngoingNavigation();

  // Step 4
  navigation->SetFocusedChangedDuringOngoingNavigation(false);

  // Step 5
  if (focusChanged) {
    return;
  }

  // Step 6
  if (mFocusResetBehavior &&
      *mFocusResetBehavior == NavigationFocusReset::Manual) {
    return;
  }

  // Step 7
  Document* document = window->GetExtantDoc();

  // If we don't have a document here, there's not much we can do.
  if (NS_WARN_IF(!document)) {
    return;
  }

  // Step 8
  Element* focusTarget = document->GetDocumentElement();
  if (focusTarget) {
    focusTarget =
        focusTarget->GetAutofocusDelegate(mozilla::IsFocusableFlags(0));
  }

  // Step 9
  if (!focusTarget) {
    focusTarget = document->GetBody();
  }

  // Step 10
  if (!focusTarget) {
    focusTarget = document->GetDocumentElement();
  }

  // The remaining steps will be implemented in Bug 1948253.

  // Step 11: Run the focusing steps for focusTarget, with document's viewport
  // as the fallback target.
  // Step 12: Move the sequential focus navigation starting point to
  // focusTarget.
}

// https://html.spec.whatwg.org/#potentially-process-scroll-behavior
void NavigateEvent::PotentiallyProcessScrollBehavior() {
  // Step 1
  MOZ_DIAGNOSTIC_ASSERT(mInterceptionState == InterceptionState::Committed ||
                        mInterceptionState == InterceptionState::Scrolled);

  // Step 2
  if (mInterceptionState == InterceptionState::Scrolled) {
    return;
  }

  // Step 3
  if (mScrollBehavior && *mScrollBehavior == NavigationScrollBehavior::Manual) {
    return;
  }

  // Process 4
  ProcessScrollBehavior();
}

// https://html.spec.whatwg.org/#process-scroll-behavior
void NavigateEvent::ProcessScrollBehavior() {
  // Step 1
  MOZ_DIAGNOSTIC_ASSERT(mInterceptionState == InterceptionState::Committed);

  // Step 2
  mInterceptionState = InterceptionState::Scrolled;

  switch (mNavigationType) {
      // Step 3
    case NavigationType::Traverse:
    case NavigationType::Reload:
      // Restore scroll position data given event's relevant global object's
      // navigable's active session history entry.

      // The remaining steps will be implemented in Bug 1948249.
      return;
    default:
      // Step 4
      break;
  }

  // The remaining steps will be implemented in Bug 1948249.

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(GetParentObject());
  // Step 4.1
  /* Document* document = */ Unused << window->GetExtantDoc();
}
}  // namespace mozilla::dom
