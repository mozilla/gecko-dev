/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocAccessibleWrap.h"
#include "nsIDocShell.h"
#include "nsLayoutUtils.h"
#include "DocAccessibleChild.h"
#include "nsAccessibilityService.h"
#include "SessionAccessibility.h"

using namespace mozilla::a11y;

const uint32_t kCacheRefreshInterval = 500;

////////////////////////////////////////////////////////////////////////////////
// DocAccessibleWrap
////////////////////////////////////////////////////////////////////////////////

DocAccessibleWrap::DocAccessibleWrap(nsIDocument* aDocument,
                                     nsIPresShell* aPresShell)
  : DocAccessible(aDocument, aPresShell)
{
  nsCOMPtr<nsIDocShellTreeItem> treeItem(aDocument->GetDocShell());

  nsCOMPtr<nsIDocShellTreeItem> parentTreeItem;
  treeItem->GetParent(getter_AddRefs(parentTreeItem));

  if (treeItem->ItemType() == nsIDocShellTreeItem::typeContent &&
      (!parentTreeItem ||
       parentTreeItem->ItemType() == nsIDocShellTreeItem::typeChrome)) {
    // The top-level content document gets this special ID.
    mID = kNoID;
  } else {
    mID = AcquireID();
  }
}

DocAccessibleWrap::~DocAccessibleWrap() {}

AccessibleWrap*
DocAccessibleWrap::GetAccessibleByID(int32_t aID) const
{
  if (AccessibleWrap* acc = mIDToAccessibleMap.Get(aID)) {
    return acc;
  }

  // If the ID is not in the hash table, check the IDs of the child docs.
  for (uint32_t i = 0; i < ChildDocumentCount(); i++) {
    auto childDoc = reinterpret_cast<AccessibleWrap*>(GetChildDocumentAt(i));
    if (childDoc->VirtualViewID() == aID) {
      return childDoc;
    }
  }

  return nullptr;
}

void
DocAccessibleWrap::DoInitialUpdate()
{
  DocAccessible::DoInitialUpdate();
  CacheViewport();
}

nsresult
DocAccessibleWrap::HandleAccEvent(AccEvent* aEvent)
{
  switch(aEvent->GetEventType()) {
    case nsIAccessibleEvent::EVENT_SHOW:
    case nsIAccessibleEvent::EVENT_HIDE:
    case nsIAccessibleEvent::EVENT_SCROLLING_END:
      CacheViewport();
      break;
    default:
      break;
  }

  return DocAccessible::HandleAccEvent(aEvent);
}

void
DocAccessibleWrap::CacheViewportCallback(nsITimer* aTimer, void* aDocAccParam)
{
  RefPtr<DocAccessibleWrap> docAcc(dont_AddRef(
    reinterpret_cast<DocAccessibleWrap*>(aDocAccParam)));
  if (!docAcc) {
    return;
  }

  nsIPresShell *presShell = docAcc->PresShell();
  if (!presShell) {
    return;
  }
  nsIFrame* rootFrame = presShell->GetRootFrame();
  if (!rootFrame) {
    return;
  }

  nsTArray<nsIFrame*> frames;
  nsIScrollableFrame* sf = presShell->GetRootScrollFrameAsScrollable();
  nsRect scrollPort = sf ? sf->GetScrollPortRect() : rootFrame->GetRect();

  nsLayoutUtils::GetFramesForArea(
    presShell->GetRootFrame(),
    scrollPort,
    frames,
    nsLayoutUtils::FrameForPointFlags::ONLY_VISIBLE);
  AccessibleHashtable inViewAccs;
  for (size_t i = 0; i < frames.Length(); i++) {
    nsIContent* content = frames.ElementAt(i)->GetContent();
    if (!content) {
      continue;
    }

    Accessible* visibleAcc = docAcc->GetAccessibleOrContainer(content);
    if (!visibleAcc) {
      continue;
    }

    for (Accessible* acc = visibleAcc; acc && acc != docAcc->Parent(); acc = acc->Parent()) {
      if (inViewAccs.Contains(acc->UniqueID())) {
        break;
      }
      inViewAccs.Put(acc->UniqueID(), acc);
    }
  }

  if (IPCAccessibilityActive()) {
    DocAccessibleChild* ipcDoc = docAcc->IPCDoc();
    nsTArray<BatchData> cacheData(inViewAccs.Count());
    for (auto iter = inViewAccs.Iter(); !iter.Done(); iter.Next()) {
      Accessible* accessible = iter.Data();
      auto uid = accessible->IsDoc() && accessible->AsDoc()->IPCDoc() ? 0
        : reinterpret_cast<uint64_t>(accessible->UniqueID());
      cacheData.AppendElement(BatchData(accessible->Document()->IPCDoc(),
                                        uid,
                                        accessible->State(),
                                        accessible->Bounds()));
    }

    ipcDoc->SendBatch(eBatch_Viewport, cacheData);
  } else if (SessionAccessibility* sessionAcc = SessionAccessibility::GetInstanceFor(docAcc)) {
    nsTArray<AccessibleWrap*> accessibles(inViewAccs.Count());
    for (auto iter = inViewAccs.Iter(); !iter.Done(); iter.Next()) {
      accessibles.AppendElement(static_cast<AccessibleWrap*>(iter.Data().get()));
    }

    sessionAcc->ReplaceViewportCache(accessibles);
  }

  if (docAcc->mCacheRefreshTimer) {
    docAcc->mCacheRefreshTimer = nullptr;
  }
}

void
DocAccessibleWrap::CacheViewport()
{
  if (VirtualViewID() == kNoID && !mCacheRefreshTimer) {
    NS_NewTimerWithFuncCallback(getter_AddRefs(mCacheRefreshTimer),
                                CacheViewportCallback,
                                this,
                                kCacheRefreshInterval,
                                nsITimer::TYPE_ONE_SHOT,
                                "a11y::DocAccessibleWrap::CacheViewport");
    if (mCacheRefreshTimer) {
      NS_ADDREF_THIS(); // Kung fu death grip
    }
  }
}
