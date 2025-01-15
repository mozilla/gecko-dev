/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Navigation_h___
#define mozilla_dom_Navigation_h___

#include "mozilla/DOMEventTargetHelper.h"

namespace mozilla::dom {

class NavigationActivation;
class NavigationHistoryEntry;
struct NavigationNavigateOptions;
struct NavigationOptions;
class NavigationTransition;
struct NavigationUpdateCurrentEntryOptions;
struct NavigationReloadOptions;
struct NavigationResult;

// See https://bugzilla.mozilla.org/show_bug.cgi?id=1903552.
// https://html.spec.whatwg.org/multipage/browsing-the-web.html#user-navigation-involvement
enum class UserNavigationInvolvement : uint8_t { BrowserUI, Activation, None };

class Navigation final : public DOMEventTargetHelper {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(Navigation, DOMEventTargetHelper)

  void Entries(nsTArray<RefPtr<NavigationHistoryEntry>>& aResult) {}
  already_AddRefed<NavigationHistoryEntry> GetCurrentEntry() { return {}; }
  void UpdateCurrentEntry(JSContext* aCx,
                          const NavigationUpdateCurrentEntryOptions& aOptions,
                          ErrorResult& aRv) {}
  already_AddRefed<NavigationTransition> GetTransition() { return {}; }
  already_AddRefed<NavigationActivation> GetActivation() { return {}; }

  bool CanGoBack() { return {}; }
  bool CanGoForward() { return {}; }

  void Navigate(JSContext* aCx, const nsAString& aUrl,
                const NavigationNavigateOptions& aOptions,
                NavigationResult& aResult) {}
  void Reload(JSContext* aCx, const NavigationReloadOptions& aOptions,
              NavigationResult& aResult) {}

  void TraverseTo(JSContext* aCx, const nsAString& aKey,
                  const NavigationOptions& aOptions,
                  NavigationResult& aResult) {}
  void Back(JSContext* aCx, const NavigationOptions& aOptions,
            NavigationResult& aResult) {}
  void Forward(JSContext* aCx, const NavigationOptions& aOptions,
               NavigationResult& aResult) {}

  IMPL_EVENT_HANDLER(navigate);
  IMPL_EVENT_HANDLER(navigatesuccess);
  IMPL_EVENT_HANDLER(navigateerror);
  IMPL_EVENT_HANDLER(currententrychange);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // The Navigation API is only enabled if both SessionHistoryInParent and
  // the dom.navigation.webidl.enabled pref are set.
  static bool IsAPIEnabled(JSContext* /* unused */, JSObject* /* unused */);

 private:
  ~Navigation() = default;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_Navigation_h___
