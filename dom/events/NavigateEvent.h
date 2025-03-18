/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_NavigateEvent_h___
#define mozilla_dom_NavigateEvent_h___

#include "js/RootingAPI.h"
#include "js/Value.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/AbortController.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/NavigateEventBinding.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla::dom {

class AbortController;
class AbortSignal;
class FormData;
class NavigationDestination;
struct NavigationInterceptOptions;

enum class NavigationType : uint8_t;

// https://html.spec.whatwg.org/#the-navigateevent-interface
class NavigateEvent final : public Event {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(NavigateEvent, Event)

  virtual JSObject* WrapObjectInternal(
      JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<NavigateEvent> Constructor(
      const GlobalObject& aGlobal, const nsAString& aType,
      const NavigateEventInit& aEventInitDict);

  static already_AddRefed<NavigateEvent> Constructor(
      EventTarget* aEventTarget, const nsAString& aType,
      const NavigateEventInit& aEventInitDict);

  static already_AddRefed<NavigateEvent> Constructor(
      EventTarget* aEventTarget, const nsAString& aType,
      const NavigateEventInit& aEventInitDict,
      nsIStructuredCloneContainer* aClassicHistoryAPIState,
      AbortController* aAbortController);

  enum NavigationType NavigationType() const;

  already_AddRefed<NavigationDestination> Destination() const;

  bool CanIntercept() const;

  bool UserInitiated() const;

  bool HashChange() const;

  AbortSignal* Signal() const;

  already_AddRefed<FormData> GetFormData() const;

  void GetDownloadRequest(nsAString& aDownloadRequest) const;

  void GetInfo(JSContext* aCx, JS::MutableHandle<JS::Value> aInfo) const;

  bool HasUAVisualTransition() const;

  Element* GetSourceElement() const;

  void Intercept(const NavigationInterceptOptions& aOptions, ErrorResult& aRv);

  void Scroll(ErrorResult& aRv);

  void InitNavigateEvent(const NavigateEventInit& aEventInitDict);

  void SetCanIntercept(bool aCanIntercept);

  enum class InterceptionState : uint8_t {
    None,
    Intercepted,
    Committed,
    Scrolled,
    Finished
  };

  InterceptionState InterceptionState() const;

  void SetInterceptionState(enum InterceptionState aInterceptionState);

  nsIStructuredCloneContainer* ClassicHistoryAPIState() const;

  nsTArray<RefPtr<NavigationInterceptHandler>>& NavigationHandlerList();

  void Finish(bool aDidFulfill);

 private:
  void PotentiallyResetFocus();

  void PotentiallyProcessScrollBehavior();

  void ProcessScrollBehavior();

  explicit NavigateEvent(EventTarget* aOwner);
  ~NavigateEvent();

  enum NavigationType mNavigationType {};
  RefPtr<NavigationDestination> mDestination;
  bool mCanIntercept = false;
  bool mUserInitiated = false;
  bool mHashChange = false;
  RefPtr<AbortSignal> mSignal;
  RefPtr<FormData> mFormData;
  nsString mDownloadRequest;
  JS::Heap<JS::Value> mInfo;
  bool mHasUAVisualTransition = false;
  RefPtr<Element> mSourceElement;

  // https://html.spec.whatwg.org/multipage/nav-history-apis.html#the-navigateevent-interface:navigateevent-2
  enum InterceptionState mInterceptionState = InterceptionState::None;

  // https://html.spec.whatwg.org/multipage/nav-history-apis.html#the-navigateevent-interface:navigateevent-3
  nsTArray<RefPtr<NavigationInterceptHandler>> mNavigationHandlerList;

  // https://html.spec.whatwg.org/multipage/nav-history-apis.html#the-navigateevent-interface:navigateevent-4
  Maybe<NavigationFocusReset> mFocusResetBehavior;

  // https://html.spec.whatwg.org/multipage/nav-history-apis.html#the-navigateevent-interface:navigateevent-5
  Maybe<NavigationScrollBehavior> mScrollBehavior;

  // https://html.spec.whatwg.org/multipage/nav-history-apis.html#the-navigateevent-interface:navigateevent-6
  RefPtr<AbortController> mAbortController;

  // https://html.spec.whatwg.org/multipage/nav-history-apis.html#the-navigateevent-interface:navigateevent-7
  nsCOMPtr<nsIStructuredCloneContainer> mClassicHistoryAPIState;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_NavigateEvent_h___
