/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Navigation_h___
#define mozilla_dom_Navigation_h___

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/NavigationBinding.h"

namespace mozilla::dom {

class NavigationActivation;
class NavigationHistoryEntry;
struct NavigationNavigateOptions;
struct NavigationOptions;
class NavigationTransition;
struct NavigationUpdateCurrentEntryOptions;
struct NavigationReloadOptions;
struct NavigationResult;

class SessionHistoryInfo;

class Navigation final : public DOMEventTargetHelper {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(Navigation, DOMEventTargetHelper)

  explicit Navigation(nsPIDOMWindowInner* aWindow);

  // Navigation.webidl
  void Entries(nsTArray<RefPtr<NavigationHistoryEntry>>& aResult) const;
  already_AddRefed<NavigationHistoryEntry> GetCurrentEntry() const;
  MOZ_CAN_RUN_SCRIPT
  void UpdateCurrentEntry(JSContext* aCx,
                          const NavigationUpdateCurrentEntryOptions& aOptions,
                          ErrorResult& aRv);
  already_AddRefed<NavigationTransition> GetTransition() { return {}; }
  already_AddRefed<NavigationActivation> GetActivation() { return {}; }

  bool CanGoBack() {
    return !HasEntriesAndEventsDisabled() && mCurrentEntryIndex &&
           *mCurrentEntryIndex != 0;
  }
  bool CanGoForward() {
    return !HasEntriesAndEventsDisabled() && mCurrentEntryIndex &&
           *mCurrentEntryIndex != mEntries.Length() - 1;
  }

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

  // https://html.spec.whatwg.org/multipage/nav-history-apis.html#initialize-the-navigation-api-entries-for-a-new-document
  void InitializeHistoryEntries(
      mozilla::Span<const SessionHistoryInfo> aNewSHInfos,
      const SessionHistoryInfo* aInitialSHInfo);

  // https://html.spec.whatwg.org/multipage/nav-history-apis.html#update-the-navigation-api-entries-for-reactivation
  MOZ_CAN_RUN_SCRIPT
  void UpdateForReactivation(SessionHistoryInfo* aReactivatedEntry);

  // https://html.spec.whatwg.org/multipage/nav-history-apis.html#update-the-navigation-api-entries-for-a-same-document-navigation
  void UpdateEntriesForSameDocumentNavigation(
      SessionHistoryInfo* aDestinationSHE, NavigationType aNavigationType);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // The Navigation API is only enabled if both SessionHistoryInParent and
  // the dom.navigation.webidl.enabled pref are set.
  static bool IsAPIEnabled(JSContext* /* unused */ = nullptr,
                           JSObject* /* unused */ = nullptr);

 private:
  ~Navigation() = default;

  // https://html.spec.whatwg.org/multipage/nav-history-apis.html#has-entries-and-events-disabled
  bool HasEntriesAndEventsDisabled() const;

  void ScheduleEventsFromNavigation(
      NavigationType aType,
      const RefPtr<NavigationHistoryEntry>& aPreviousEntry,
      nsTArray<RefPtr<NavigationHistoryEntry>>&& aDisposedEntries);

  void LogHistory() const;

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  // https://html.spec.whatwg.org/multipage/nav-history-apis.html#navigation-entry-list
  nsTArray<RefPtr<NavigationHistoryEntry>> mEntries;
  // https://html.spec.whatwg.org/multipage/nav-history-apis.html#navigation-current-entry
  Maybe<uint64_t> mCurrentEntryIndex;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_Navigation_h___
