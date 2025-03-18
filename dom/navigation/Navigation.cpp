/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Navigation.h"

#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/Logging.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/FeaturePolicy.h"
#include "mozilla/dom/NavigationCurrentEntryChangeEvent.h"
#include "mozilla/dom/NavigationHistoryEntry.h"
#include "mozilla/dom/SessionHistoryEntry.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/NavigationCurrentEntryChangeEvent.h"
#include "mozilla/dom/NavigationHistoryEntry.h"
#include "mozilla/dom/SessionHistoryEntry.h"
#include "nsContentUtils.h"
#include "nsIXULRuntime.h"
#include "nsNetUtil.h"

mozilla::LazyLogModule gNavigationLog("Navigation");

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(Navigation, DOMEventTargetHelper, mEntries);
NS_IMPL_ADDREF_INHERITED(Navigation, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(Navigation, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Navigation)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

Navigation::Navigation(nsPIDOMWindowInner* aWindow) : mWindow(aWindow) {
  MOZ_ASSERT(aWindow);
}

JSObject* Navigation::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return Navigation_Binding::Wrap(aCx, this, aGivenProto);
}

/* static */
bool Navigation::IsAPIEnabled(JSContext* /* unused */, JSObject* /* unused */) {
  return SessionHistoryInParent() &&
         StaticPrefs::dom_navigation_webidl_enabled_DoNotUseDirectly();
}

void Navigation::Entries(
    nsTArray<RefPtr<NavigationHistoryEntry>>& aResult) const {
  aResult = mEntries.Clone();
}

already_AddRefed<NavigationHistoryEntry> Navigation::GetCurrentEntry() const {
  if (HasEntriesAndEventsDisabled()) {
    return nullptr;
  }

  if (!mCurrentEntryIndex) {
    return nullptr;
  }

  MOZ_LOG(gNavigationLog, LogLevel::Debug,
          ("Current Entry: %d; Amount of Entries: %d", int(*mCurrentEntryIndex),
           int(mEntries.Length())));
  MOZ_ASSERT(*mCurrentEntryIndex < mEntries.Length());

  RefPtr entry{mEntries[*mCurrentEntryIndex]};
  return entry.forget();
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#dom-navigation-updatecurrententry
void Navigation::UpdateCurrentEntry(
    JSContext* aCx, const NavigationUpdateCurrentEntryOptions& aOptions,
    ErrorResult& aRv) {
  RefPtr currentEntry(GetCurrentEntry());
  if (!currentEntry) {
    aRv.ThrowInvalidStateError(
        "Can't call updateCurrentEntry without a valid entry.");
    return;
  }

  JS::Rooted<JS::Value> state(aCx, aOptions.mState);
  auto serializedState = MakeRefPtr<nsStructuredCloneContainer>();
  nsresult rv = serializedState->InitFromJSVal(state, aCx);
  if (NS_FAILED(rv)) {
    aRv.ThrowDataCloneError(
        "Failed to serialize value for updateCurrentEntry.");
    return;
  }

  currentEntry->SetState(serializedState);

  NavigationCurrentEntryChangeEventInit init;
  init.mFrom = currentEntry;
  // Leaving the navigation type unspecified means it will be initialized to
  // null.
  RefPtr event = NavigationCurrentEntryChangeEvent::Constructor(
      this, u"currententrychange"_ns, init);
  DispatchEvent(*event);
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#has-entries-and-events-disabled
bool Navigation::HasEntriesAndEventsDisabled() const {
  Document* doc = mWindow->GetDoc();
  return !doc->IsCurrentActiveDocument() ||
         (NS_IsAboutBlankAllowQueryAndFragment(doc->GetDocumentURI()) &&
          doc->IsInitialDocument()) ||
         doc->GetPrincipal()->GetIsNullPrincipal();
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#initialize-the-navigation-api-entries-for-a-new-document
void Navigation::InitializeHistoryEntries(
    mozilla::Span<const SessionHistoryInfo> aNewSHInfos,
    const SessionHistoryInfo* aInitialSHInfo) {
  mEntries.Clear();
  mCurrentEntryIndex.reset();
  if (HasEntriesAndEventsDisabled()) {
    return;
  }

  for (auto i = 0ul; i < aNewSHInfos.Length(); i++) {
    mEntries.AppendElement(
        MakeRefPtr<NavigationHistoryEntry>(mWindow, &aNewSHInfos[i], i));
    if (aNewSHInfos[i].NavigationKey() == aInitialSHInfo->NavigationKey()) {
      mCurrentEntryIndex = Some(i);
    }
  }

  LogHistory();

  nsID key = aInitialSHInfo->NavigationKey();
  nsID id = aInitialSHInfo->NavigationId();
  MOZ_LOG(
      gNavigationLog, LogLevel::Debug,
      ("aInitialSHInfo: %s %s\n", key.ToString().get(), id.ToString().get()));
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#update-the-navigation-api-entries-for-a-same-document-navigation
void Navigation::UpdateEntriesForSameDocumentNavigation(
    SessionHistoryInfo* aDestinationSHE, NavigationType aNavigationType) {
  // Step 1.
  if (HasEntriesAndEventsDisabled()) {
    return;
  }

  MOZ_LOG(gNavigationLog, LogLevel::Debug,
          ("Updating entries for same-document navigation"));

  // Steps 2-7.
  RefPtr<NavigationHistoryEntry> oldCurrentEntry = GetCurrentEntry();
  nsTArray<RefPtr<NavigationHistoryEntry>> disposedEntries;
  switch (aNavigationType) {
    case NavigationType::Traverse:
      MOZ_LOG(gNavigationLog, LogLevel::Debug, ("Traverse navigation"));
      mCurrentEntryIndex.reset();
      for (auto i = 0ul; i < mEntries.Length(); i++) {
        if (mEntries[i]->IsSameEntry(aDestinationSHE)) {
          mCurrentEntryIndex = Some(i);
          break;
        }
      }
      MOZ_ASSERT(mCurrentEntryIndex);
      break;

    case NavigationType::Push:
      MOZ_LOG(gNavigationLog, LogLevel::Debug, ("Push navigation"));
      mCurrentEntryIndex =
          Some(mCurrentEntryIndex ? *mCurrentEntryIndex + 1 : 0);
      while (*mCurrentEntryIndex < mEntries.Length()) {
        disposedEntries.AppendElement(mEntries.PopLastElement());
      }
      mEntries.AppendElement(MakeRefPtr<NavigationHistoryEntry>(
          mWindow, aDestinationSHE, *mCurrentEntryIndex));
      break;

    case NavigationType::Replace:
      MOZ_LOG(gNavigationLog, LogLevel::Debug, ("Replace navigation"));
      disposedEntries.AppendElement(oldCurrentEntry);
      aDestinationSHE->NavigationKey() = oldCurrentEntry->Key();
      mEntries[*mCurrentEntryIndex] = MakeRefPtr<NavigationHistoryEntry>(
          mWindow, aDestinationSHE, *mCurrentEntryIndex);
      break;

    case NavigationType::Reload:
      break;
  }

  // TODO: Step 8.

  // Steps 9-12.
  {
    nsAutoMicroTask mt;
    AutoEntryScript aes(mWindow->AsGlobal(),
                        "UpdateEntriesForSameDocumentNavigation");

    ScheduleEventsFromNavigation(aNavigationType, oldCurrentEntry,
                                 std::move(disposedEntries));
  }
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#update-the-navigation-api-entries-for-reactivation
void Navigation::UpdateForReactivation(SessionHistoryInfo* aReactivatedEntry) {
  // NAV-TODO
}

void Navigation::ScheduleEventsFromNavigation(
    NavigationType aType, const RefPtr<NavigationHistoryEntry>& aPreviousEntry,
    nsTArray<RefPtr<NavigationHistoryEntry>>&& aDisposedEntries) {
  nsContentUtils::AddScriptRunner(NS_NewRunnableFunction(
      "mozilla::dom::Navigation::ScheduleEventsFromNavigation",
      [self = RefPtr(this), previousEntry = RefPtr(aPreviousEntry),
       disposedEntries = std::move(aDisposedEntries), aType]() {
        if (previousEntry) {
          NavigationCurrentEntryChangeEventInit init;
          init.mFrom = previousEntry;
          init.mNavigationType.SetValue(aType);
          RefPtr event = NavigationCurrentEntryChangeEvent::Constructor(
              self, u"currententrychange"_ns, init);
          self->DispatchEvent(*event);
        }

        for (const auto& entry : disposedEntries) {
          RefPtr<Event> event = NS_NewDOMEvent(entry, nullptr, nullptr);
          event->InitEvent(u"dispose"_ns, false, false);
          event->SetTrusted(true);
          event->SetTarget(entry);
          entry->DispatchEvent(*event);
        }
      }));
}

namespace {

void LogEntry(NavigationHistoryEntry* aEntry, uint64_t aIndex, uint64_t aTotal,
              bool aIsCurrent) {
  if (!aEntry) {
    MOZ_LOG(gNavigationLog, LogLevel::Debug,
            (" +- %d NHEntry null\n", int(aIndex)));
    return;
  }

  nsString key, id;
  aEntry->GetKey(key);
  aEntry->GetId(id);
  MOZ_LOG(gNavigationLog, LogLevel::Debug,
          ("%s+- %d NHEntry %p %s %s\n", aIsCurrent ? ">" : " ", int(aIndex),
           aEntry, NS_ConvertUTF16toUTF8(key).get(),
           NS_ConvertUTF16toUTF8(id).get()));

  nsAutoString url;
  aEntry->GetUrl(url);
  MOZ_LOG(gNavigationLog, LogLevel::Debug,
          ("   URL = %s\n", NS_ConvertUTF16toUTF8(url).get()));
}

}  // namespace

void Navigation::LogHistory() const {
  if (!MOZ_LOG_TEST(gNavigationLog, LogLevel::Debug)) {
    return;
  }

  MOZ_LOG(gNavigationLog, LogLevel::Debug,
          ("Navigation %p (current entry index: %d)\n", this,
           mCurrentEntryIndex ? int(*mCurrentEntryIndex) : -1));
  auto length = mEntries.Length();
  for (uint64_t i = 0; i < length; i++) {
    LogEntry(mEntries[i], i, length,
             mCurrentEntryIndex && i == *mCurrentEntryIndex);
  }
}

}  // namespace mozilla::dom
