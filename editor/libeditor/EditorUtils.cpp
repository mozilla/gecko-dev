/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EditorUtils.h"

#include "mozilla/EditorDOMPoint.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/dom/Selection.h"
#include "nsComponentManagerUtils.h"
#include "nsError.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsIDocShell.h"
#include "nsIDocument.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsINode.h"

class nsISupports;
class nsRange;

namespace mozilla {

using namespace dom;

/******************************************************************************
 * some helper classes for iterating the dom tree
 *****************************************************************************/

DOMIterator::DOMIterator(
    nsINode& aNode MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL) {
  MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  mIter = NS_NewContentIterator();
  DebugOnly<nsresult> rv = mIter->Init(&aNode);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

nsresult DOMIterator::Init(nsRange& aRange) {
  mIter = NS_NewContentIterator();
  return mIter->Init(&aRange);
}

DOMIterator::DOMIterator(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM_IN_IMPL) {
  MOZ_GUARD_OBJECT_NOTIFIER_INIT;
}

DOMIterator::~DOMIterator() {}

void DOMIterator::AppendList(
    const BoolDomIterFunctor& functor,
    nsTArray<OwningNonNull<nsINode>>& arrayOfNodes) const {
  // Iterate through dom and build list
  for (; !mIter->IsDone(); mIter->Next()) {
    nsCOMPtr<nsINode> node = mIter->GetCurrentNode();

    if (functor(node)) {
      arrayOfNodes.AppendElement(*node);
    }
  }
}

DOMSubtreeIterator::DOMSubtreeIterator(
    MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM_IN_IMPL)
    : DOMIterator(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM_TO_PARENT) {}

nsresult DOMSubtreeIterator::Init(nsRange& aRange) {
  mIter = NS_NewContentSubtreeIterator();
  return mIter->Init(&aRange);
}

DOMSubtreeIterator::~DOMSubtreeIterator() {}

/******************************************************************************
 * some general purpose editor utils
 *****************************************************************************/

bool EditorUtils::IsDescendantOf(const nsINode& aNode, const nsINode& aParent,
                                 EditorRawDOMPoint* aOutPoint /* = nullptr */) {
  if (aOutPoint) {
    aOutPoint->Clear();
  }

  if (&aNode == &aParent) {
    return false;
  }

  for (const nsINode* node = &aNode; node; node = node->GetParentNode()) {
    if (node->GetParentNode() == &aParent) {
      if (aOutPoint) {
        MOZ_ASSERT(node->IsContent());
        aOutPoint->Set(node->AsContent());
      }
      return true;
    }
  }

  return false;
}

bool EditorUtils::IsDescendantOf(const nsINode& aNode, const nsINode& aParent,
                                 EditorDOMPoint* aOutPoint) {
  MOZ_ASSERT(aOutPoint);
  aOutPoint->Clear();
  if (&aNode == &aParent) {
    return false;
  }

  for (const nsINode* node = &aNode; node; node = node->GetParentNode()) {
    if (node->GetParentNode() == &aParent) {
      MOZ_ASSERT(node->IsContent());
      aOutPoint->Set(node->AsContent());
      return true;
    }
  }

  return false;
}

}  // namespace mozilla
