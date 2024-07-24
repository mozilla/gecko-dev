/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_NavigateEvent_h___
#define mozilla_dom_NavigateEvent_h___

#include "js/TypeDecls.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/NavigateEventBinding.h"
#include "mozilla/dom/NavigationBinding.h"

namespace mozilla::dom {

class AbortSignal;
class FormData;
class NavigationDestination;
struct NavigationInterceptOptions;

class NavigateEvent final : public Event {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(NavigateEvent, Event)

  static already_AddRefed<NavigateEvent> Constructor(
      const GlobalObject& aGlobal, const nsAString& aType,
      const NavigateEventInit& aEventInitDict);

  enum NavigationType NavigationType() const { return {}; }

  already_AddRefed<NavigationDestination> Destination() const { return {}; }

  bool CanIntercept() const { return {}; }

  bool UserInitiated() const { return {}; }

  bool HashChange() const { return {}; }

  already_AddRefed<AbortSignal> Signal() const { return {}; }

  already_AddRefed<FormData> GetFormData() const { return {}; }

  void GetDownloadRequest(nsString& aRetVal) const {}

  void GetInfo(JSContext* aCx, JS::MutableHandle<JS::Value> aRetVal) const {}

  bool HasUAVisualTransition() const { return {}; }

  void Intercept(const NavigationInterceptOptions& aOptions, ErrorResult& aRv) {
  }

  void Scroll(ErrorResult& aRv) {}

  bool IsTrusted() const { return {}; }

 private:
  ~NavigateEvent() = default;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_NavigateEvent_h___
