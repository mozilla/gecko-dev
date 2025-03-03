/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DocGroup_h
#define DocGroup_h

#include "nsISupportsImpl.h"
#include "nsIPrincipal.h"
#include "nsThreadUtils.h"
#include "nsTHashSet.h"
#include "nsString.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/HTMLSlotElement.h"

namespace mozilla {
class AbstractThread;
namespace dom {

class CustomElementReactionsStack;
class JSExecutionManager;

// DocGroup is the Gecko object for a "Similar-origin Window Agent" (the
// window-global component of an "Agent Cluster").
// https://html.spec.whatwg.org/multipage/webappapis.html#similar-origin-window-agent
//
// A DocGroup is shared between a series of window globals which are reachable
// from one-another (e.g. through `window.opener`, `window.parent` or
// `window.frames`), and are able to synchronously communicate with one-another,
// (either due to being same-origin, or by setting `document.domain`).
//
// NOTE: Similar to how the principal for a global is stored on a Document, the
// DocGroup for a window global is also attached to the corresponding Document
// object. This is required for certain features (such as the ArenaAllocator)
// which require the DocGroup before the nsGlobalWindowInner has been created.
//
// NOTE: DocGroup is not the source of truth for synchronous script access.
// Non-window globals, such as extension globals and system JS, may have
// synchronous access yet not be part of the DocGroup. The DocGroup should,
// however, align with web-visible synchronous script access boundaries.
class DocGroup final {
 public:
  typedef nsTArray<Document*>::iterator Iterator;

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(DocGroup)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(DocGroup)

  static already_AddRefed<DocGroup> Create(
      BrowsingContextGroup* aBrowsingContextGroup, const DocGroupKey& aKey);

  void AssertMatches(const Document* aDocument) const;

  const DocGroupKey& GetKey() const { return mKey; }

  bool IsOriginKeyed() const { return mKey.mOriginKeyed; }

  JSExecutionManager* GetExecutionManager() const { return mExecutionManager; }
  void SetExecutionManager(JSExecutionManager*);

  BrowsingContextGroup* GetBrowsingContextGroup() const {
    return mBrowsingContextGroup;
  }

  mozilla::dom::DOMArena* ArenaAllocator() { return mArena; }

  mozilla::dom::CustomElementReactionsStack* CustomElementReactionsStack();

  // Adding documents to a DocGroup should be done through
  // BrowsingContextGroup::AddDocument (which in turn calls
  // DocGroup::AddDocument).
  void AddDocument(Document* aDocument);

  // Removing documents from a DocGroup should be done through
  // BrowsingContextGroup::RemoveDocument(which in turn calls
  // DocGroup::RemoveDocument).
  void RemoveDocument(Document* aDocument);

  // Iterators for iterating over every document within the DocGroup
  Iterator begin() {
    MOZ_ASSERT(NS_IsMainThread());
    return mDocuments.begin();
  }
  Iterator end() {
    MOZ_ASSERT(NS_IsMainThread());
    return mDocuments.end();
  }

  // Return a pointer that can be continually checked to see if access to this
  // DocGroup is valid. This pointer should live at least as long as the
  // DocGroup.
  bool* GetValidAccessPtr();

  // Append aSlot to the list of signal slot list, and queue a mutation observer
  // microtask.
  void SignalSlotChange(HTMLSlotElement& aSlot);

  nsTArray<RefPtr<HTMLSlotElement>> MoveSignalSlotList();

  // List of DocGroups that has non-empty signal slot list.
  static AutoTArray<RefPtr<DocGroup>, 2>* sPendingDocGroups;

  // Returns true if any of its documents are active but not in the bfcache.
  bool IsActive() const;

  const nsID& AgentClusterId() const { return mAgentClusterId; }

  bool IsEmpty() const { return mDocuments.IsEmpty(); }

 private:
  DocGroup(BrowsingContextGroup* aBrowsingContextGroup,
           const DocGroupKey& aKey);

  ~DocGroup();

  DocGroupKey mKey;

  nsTArray<Document*> mDocuments;
  RefPtr<mozilla::dom::CustomElementReactionsStack> mReactionsStack;
  nsTArray<RefPtr<HTMLSlotElement>> mSignalSlotList;
  RefPtr<BrowsingContextGroup> mBrowsingContextGroup;

  // non-null if the JS execution for this docgroup is regulated with regards
  // to worker threads. This should only be used when we are forcing serialized
  // SAB access.
  RefPtr<JSExecutionManager> mExecutionManager;

  // Each DocGroup has a persisted agent cluster ID.
  const nsID mAgentClusterId;

  RefPtr<mozilla::dom::DOMArena> mArena;
};

}  // namespace dom
}  // namespace mozilla

#endif  // defined(DocGroup_h)
