/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TreeOrderedArrayInlines_h
#define mozilla_dom_TreeOrderedArrayInlines_h

#include "mozilla/dom/TreeOrderedArray.h"
#include "mozilla/BinarySearch.h"
#include "nsContentUtils.h"
#include <type_traits>

namespace mozilla::dom {

template <typename Node>
size_t TreeOrderedArray<Node>::Insert(Node& aNode, nsINode* aCommonAncestor) {
  static_assert(std::is_base_of_v<nsINode, Node>, "Should be a node");

  if (mList.IsEmpty()) {
    mList.AppendElement(&aNode);
    return 0;
  }

  struct PositionComparator {
    Node& mNode;
    nsINode* mCommonAncestor = nullptr;
    mutable nsContentUtils::NodeIndexCache mCache;

    int operator()(void* aNode) const {
      auto* curNode = static_cast<Node*>(aNode);
      MOZ_DIAGNOSTIC_ASSERT(curNode != &mNode,
                            "Tried to insert a node already in the list");
      return nsContentUtils::CompareTreePosition<TreeKind::DOM>(
          &mNode, curNode, mCommonAncestor, &mCache);
    }
  };

  PositionComparator cmp{aNode, aCommonAncestor};
  if (cmp(mList.LastElement()) > 0) {
    // Appending is a really common case, optimize for it.
    auto index = mList.Length();
    mList.AppendElement(&aNode);
    return index;
  }

  size_t idx;
  BinarySearchIf(mList, 0, mList.Length(), cmp, &idx);
  mList.InsertElementAt(idx, &aNode);
  return idx;
}

}  // namespace mozilla::dom
#endif
