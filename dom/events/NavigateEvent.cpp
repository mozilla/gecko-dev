/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGlobalWindowInner.h"
#include "nsDocShell.h"

#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/PresShell.h"

#include "mozilla/dom/AbortController.h"
#include "mozilla/dom/NavigateEvent.h"
#include "mozilla/dom/NavigateEventBinding.h"
#include "mozilla/dom/Navigation.h"
#include "mozilla/dom/SessionHistoryEntry.h"

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
    class AbortController* aAbortController) {
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

template <typename OptionEnum>
static void MaybeReportWarningToConsole(Document* aDocument,
                                        const nsString& aOption,
                                        OptionEnum aPrevious, OptionEnum aNew) {
  if (!aDocument) {
    return;
  }

  nsTArray<nsString> params = {aOption,
                               NS_ConvertUTF8toUTF16(GetEnumString(aNew)),
                               NS_ConvertUTF8toUTF16(GetEnumString(aPrevious))};
  nsContentUtils::ReportToConsole(
      nsIScriptError::warningFlag, "DOM"_ns, aDocument,
      nsContentUtils::eDOM_PROPERTIES,
      "PreviousInterceptCallOptionOverriddenWarning", params);
}

void NavigateEvent::Intercept(const NavigationInterceptOptions& aOptions,
                              ErrorResult& aRv) {
  // Step 1
  if (PerformSharedChecks(aRv); aRv.Failed()) {
    return;
  }

  // Step 2
  if (!mCanIntercept) {
    aRv.ThrowSecurityError("Event's canIntercept was initialized to false");
    return;
  }

  // Step 3
  if (!HasBeenDispatched()) {
    aRv.ThrowInvalidStateError("Event has never been dispatched");
    return;
  }

  // Step 4
  MOZ_DIAGNOSTIC_ASSERT(mInterceptionState == InterceptionState::None ||
                        mInterceptionState == InterceptionState::Intercepted);

  // Step 5
  mInterceptionState = InterceptionState::Intercepted;

  // Step 6
  if (aOptions.mHandler.WasPassed()) {
    mNavigationHandlerList.AppendElement(
        aOptions.mHandler.InternalValue().get());
  }

  // Step 7
  if (aOptions.mFocusReset.WasPassed()) {
    // Step 7.1
    if (mFocusResetBehavior &&
        *mFocusResetBehavior != aOptions.mFocusReset.Value()) {
      RefPtr<Document> document = GetDocument();
      MaybeReportWarningToConsole(document, u"focusReset"_ns,
                                  *mFocusResetBehavior,
                                  aOptions.mFocusReset.Value());
    }

    // Step 7.2
    mFocusResetBehavior.emplace(aOptions.mFocusReset.Value());
  }

  // Step 8
  if (aOptions.mScroll.WasPassed()) {
    // Step 8.1
    if (mScrollBehavior && *mScrollBehavior != aOptions.mScroll.Value()) {
      RefPtr<Document> document = GetDocument();
      MaybeReportWarningToConsole(document, u"scroll"_ns, *mScrollBehavior,
                                  aOptions.mScroll.Value());
    }

    // Step 8.2
    mScrollBehavior.emplace(aOptions.mScroll.Value());
  }
}

// https://html.spec.whatwg.org/#dom-navigateevent-scroll
void NavigateEvent::Scroll(ErrorResult& aRv) {
  // Step 1
  if (PerformSharedChecks(aRv); aRv.Failed()) {
    return;
  }

  // Step 2
  if (mInterceptionState != InterceptionState::Committed) {
    aRv.ThrowInvalidStateError("NavigateEvent was not committed");
    return;
  }

  // Step 3
  ProcessScrollBehavior();
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

AbortController* NavigateEvent::AbortController() const {
  return mAbortController;
}

bool NavigateEvent::HasBeenDispatched() const {
  return mEvent->mFlags.mDispatchedAtLeastOnce;
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

// https://html.spec.whatwg.org/#navigateevent-perform-shared-checks
void NavigateEvent::PerformSharedChecks(ErrorResult& aRv) {
  // Step 1
  if (RefPtr document = GetDocument();
      !document || !document->IsFullyActive()) {
    aRv.ThrowInvalidStateError("Document isn't fully active");
    return;
  }

  // Step 2
  if (!IsTrusted()) {
    aRv.ThrowSecurityError("Event is untrusted");
    return;
  }

  // Step 3
  if (DefaultPrevented()) {
    aRv.ThrowInvalidStateError("Event was canceled");
  }
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

// Here we want to scroll to the beginning of the document, as described in
// https://drafts.csswg.org/cssom-view/#scroll-to-the-beginning-of-the-document
MOZ_CAN_RUN_SCRIPT
static void ScrollToBeginningOfDocument(Document& aDocument) {
  RefPtr<PresShell> presShell = aDocument.GetPresShell();
  if (!presShell) {
    return;
  }

  RefPtr<Element> rootElement = aDocument.GetRootElement();
  ScrollAxis vertical(WhereToScroll::Start, WhenToScroll::Always);
  presShell->ScrollContentIntoView(rootElement, vertical, ScrollAxis(),
                                   ScrollFlags::TriggeredByScript);
}

// https://html.spec.whatwg.org/#restore-scroll-position-data
static void RestoreScrollPositionData(Document* aDocument) {
  if (!aDocument || aDocument->HasBeenScrolled()) {
    return;
  }

  // This will be implemented in Bug 1955947. Make sure to move this to
  // `SessionHistoryEntry`/`SessionHistoryInfo`.
}

// https://html.spec.whatwg.org/#process-scroll-behavior
void NavigateEvent::ProcessScrollBehavior() {
  // Step 1
  MOZ_DIAGNOSTIC_ASSERT(mInterceptionState == InterceptionState::Committed);

  // Step 2
  mInterceptionState = InterceptionState::Scrolled;

  // Step 3
  if (mNavigationType == NavigationType::Traverse ||
      mNavigationType == NavigationType::Reload) {
    RefPtr<Document> document = GetDocument();
    RestoreScrollPositionData(document);
    return;
  }

  // Step 4.1
  RefPtr<Document> document = GetDocument();
  // If there is no document there's not much to do.
  if (!document) {
    return;
  }

  // Step 4.2
  nsAutoCString ref;
  if (nsIURI* uri = document->GetDocumentURI();
      NS_SUCCEEDED(uri->GetRef(ref)) &&
      !nsContentUtils::GetTargetElement(document, NS_ConvertUTF8toUTF16(ref))) {
    ScrollToBeginningOfDocument(*document);
    return;
  }

  // Step 4.3
  document->ScrollToRef();
}
}  // namespace mozilla::dom
