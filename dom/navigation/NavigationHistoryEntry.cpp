/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/NavigationHistoryEntry.h"
#include "mozilla/dom/NavigationHistoryEntryBinding.h"

#include "mozilla/dom/Document.h"
#include "mozilla/dom/SessionHistoryEntry.h"
#include "nsDocShell.h"

extern mozilla::LazyLogModule gNavigationLog;

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(NavigationHistoryEntry,
                                   DOMEventTargetHelper);
NS_IMPL_ADDREF_INHERITED(NavigationHistoryEntry, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(NavigationHistoryEntry, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(NavigationHistoryEntry)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NavigationHistoryEntry::NavigationHistoryEntry(
    nsIGlobalObject* aGlobal, const SessionHistoryInfo* aSHInfo, int64_t aIndex)
    : DOMEventTargetHelper(aGlobal),
      mSHInfo(MakeUnique<SessionHistoryInfo>(*aSHInfo)),
      mIndex(aIndex) {}

NavigationHistoryEntry::~NavigationHistoryEntry() = default;

// https://html.spec.whatwg.org/#dom-navigationhistoryentry-url
void NavigationHistoryEntry::GetUrl(nsAString& aResult) const {
  if (!HasActiveDocument()) {
    return;
  }

  // HasActiveDocument implies that GetCurrentDocument returns non-null.
  MOZ_DIAGNOSTIC_ASSERT(GetCurrentDocument());

  if (!SameDocument()) {
    auto referrerPolicy = GetCurrentDocument()->ReferrerPolicy();
    if (referrerPolicy == ReferrerPolicy::No_referrer ||
        referrerPolicy == ReferrerPolicy::Origin) {
      return;
    }
  }

  MOZ_ASSERT(mSHInfo);
  nsCOMPtr<nsIURI> uri = mSHInfo->GetURI();
  MOZ_ASSERT(uri);
  nsCString uriSpec;
  uri->GetSpec(uriSpec);
  CopyUTF8toUTF16(uriSpec, aResult);
}

// https://html.spec.whatwg.org/#dom-navigationhistoryentry-key
void NavigationHistoryEntry::GetKey(nsAString& aResult) const {
  if (!HasActiveDocument()) {
    return;
  }

  nsIDToCString keyString(mSHInfo->NavigationKey());
  // Omit the curly braces and NUL.
  CopyUTF8toUTF16(Substring(keyString.get() + 1, NSID_LENGTH - 3), aResult);
}

// https://html.spec.whatwg.org/#dom-navigationhistoryentry-id
void NavigationHistoryEntry::GetId(nsAString& aResult) const {
  if (!HasActiveDocument()) {
    return;
  }

  nsIDToCString idString(mSHInfo->NavigationId());
  // Omit the curly braces and NUL.
  CopyUTF8toUTF16(Substring(idString.get() + 1, NSID_LENGTH - 3), aResult);
}

// https://html.spec.whatwg.org/#dom-navigationhistoryentry-index
int64_t NavigationHistoryEntry::Index() const {
  MOZ_ASSERT(mSHInfo);
  if (!HasActiveDocument()) {
    return -1;
  }
  return mIndex;
}

// https://html.spec.whatwg.org/#dom-navigationhistoryentry-samedocument
bool NavigationHistoryEntry::SameDocument() const {
  if (!HasActiveDocument()) {
    return false;
  }

  // HasActiveDocument implies that GetCurrentDocument returns non-null.
  MOZ_DIAGNOSTIC_ASSERT(GetCurrentDocument());

  MOZ_ASSERT(mSHInfo);
  auto* docShell = nsDocShell::Cast(GetCurrentDocument()->GetDocShell());
  return docShell && docShell->IsSameDocumentAsActiveEntry(*mSHInfo);
}

// https://html.spec.whatwg.org/#dom-navigationhistoryentry-getstate
void NavigationHistoryEntry::GetState(JSContext* aCx,
                                      JS::MutableHandle<JS::Value> aResult,
                                      ErrorResult& aRv) const {
  if (!mSHInfo) {
    return;
  }
  RefPtr<nsStructuredCloneContainer> state = mSHInfo->GetNavigationState();
  if (!state) {
    aResult.setUndefined();
    return;
  }

  nsresult rv = state->DeserializeToJsval(aCx, aResult);
  if (NS_FAILED(rv)) {
    // TODO change this to specific exception
    aRv.Throw(rv);
  }
}

void NavigationHistoryEntry::SetState(nsStructuredCloneContainer* aState) {
  if (RefPtr<nsStructuredCloneContainer> state =
          mSHInfo->GetNavigationState()) {
    state->Copy(*aState);
  }
}

bool NavigationHistoryEntry::IsSameEntry(
    const SessionHistoryInfo* aSHInfo) const {
  return mSHInfo->NavigationId() == aSHInfo->NavigationId();
}

bool NavigationHistoryEntry::SharesDocumentWith(
    const SessionHistoryInfo& aSHInfo) const {
  return mSHInfo->SharesDocumentWith(aSHInfo);
}

JSObject* NavigationHistoryEntry::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return NavigationHistoryEntry_Binding::Wrap(aCx, this, aGivenProto);
}

Document* NavigationHistoryEntry::GetCurrentDocument() const {
  return GetDocumentIfCurrent();
}

bool NavigationHistoryEntry::HasActiveDocument() const {
  if (auto* document = GetCurrentDocument()) {
    return document->IsCurrentActiveDocument();
  }

  return false;
}

const nsID& NavigationHistoryEntry::Key() const {
  return mSHInfo->NavigationKey();
}

}  // namespace mozilla::dom
