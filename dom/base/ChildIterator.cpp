/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChildIterator.h"
#include "nsContentUtils.h"
#include "mozilla/dom/HTMLSlotElement.h"
#include "mozilla/dom/XBLChildrenElement.h"
#include "mozilla/dom/ShadowRoot.h"
#include "nsIAnonymousContentCreator.h"
#include "nsIFrame.h"
#include "nsCSSAnonBoxes.h"
#include "nsDocument.h"

namespace mozilla {
namespace dom {

ExplicitChildIterator::ExplicitChildIterator(const nsIContent* aParent,
                                             bool aStartAtBeginning)
    : mParent(aParent),
      mChild(nullptr),
      mDefaultChild(nullptr),
      mIsFirst(aStartAtBeginning),
      mIndexInInserted(0) {
  mParentAsSlot = HTMLSlotElement::FromNode(mParent);
}

nsIContent* ExplicitChildIterator::GetNextChild() {
  // If we're already in the inserted-children array, look there first
  if (mIndexInInserted) {
    MOZ_ASSERT(mChild);
    MOZ_ASSERT(!mDefaultChild);

    if (mParentAsSlot) {
      const nsTArray<RefPtr<nsINode>>& assignedNodes =
          mParentAsSlot->AssignedNodes();

      mChild = (mIndexInInserted < assignedNodes.Length())
                   ? assignedNodes[mIndexInInserted++]->AsContent()
                   : nullptr;
      return mChild;
    }

    MOZ_ASSERT(mChild->IsActiveChildrenElement());
    auto* childrenElement = static_cast<XBLChildrenElement*>(mChild);
    if (mIndexInInserted < childrenElement->InsertedChildrenLength()) {
      return childrenElement->InsertedChild(mIndexInInserted++);
    }
    mIndexInInserted = 0;
    mChild = mChild->GetNextSibling();
  } else if (mDefaultChild) {
    // If we're already in default content, check if there are more nodes there
    MOZ_ASSERT(mChild);
    MOZ_ASSERT(mChild->IsActiveChildrenElement());

    mDefaultChild = mDefaultChild->GetNextSibling();
    if (mDefaultChild) {
      return mDefaultChild;
    }

    mChild = mChild->GetNextSibling();
  } else if (mIsFirst) {  // at the beginning of the child list
    // For slot parent, iterate over assigned nodes if not empty, otherwise
    // fall through and iterate over direct children (fallback content).
    if (mParentAsSlot) {
      const nsTArray<RefPtr<nsINode>>& assignedNodes =
          mParentAsSlot->AssignedNodes();
      if (!assignedNodes.IsEmpty()) {
        mIndexInInserted = 1;
        mChild = assignedNodes[0]->AsContent();
        mIsFirst = false;
        return mChild;
      }
    }

    mChild = mParent->GetFirstChild();
    mIsFirst = false;
  } else if (mChild) {  // in the middle of the child list
    mChild = mChild->GetNextSibling();
  }

  // Iterate until we find a non-insertion point, or an insertion point with
  // content.
  while (mChild) {
    if (mChild->IsActiveChildrenElement()) {
      // If the current child being iterated is a content insertion point
      // then the iterator needs to return the nodes distributed into
      // the content insertion point.
      auto* childrenElement = static_cast<XBLChildrenElement*>(mChild);
      if (childrenElement->HasInsertedChildren()) {
        // Iterate through elements projected on insertion point.
        mIndexInInserted = 1;
        return childrenElement->InsertedChild(0);
      }

      // Insertion points inside fallback/default content
      // are considered inactive and do not get assigned nodes.
      mDefaultChild = mChild->GetFirstChild();
      if (mDefaultChild) {
        return mDefaultChild;
      }

      // If we have an insertion point with no assigned nodes and
      // no default content, move on to the next node.
      mChild = mChild->GetNextSibling();
    } else {
      // mChild is not an insertion point, thus it is the next node to
      // return from this iterator.
      break;
    }
  }

  return mChild;
}

void FlattenedChildIterator::Init(bool aIgnoreXBL) {
  if (aIgnoreXBL) {
    mXBLInvolved = Some(false);
    return;
  }

  // TODO(emilio): I think it probably makes sense to only allow constructing
  // FlattenedChildIterators with Element.
  if (mParent->IsElement()) {
    if (ShadowRoot* shadow = mParent->AsElement()->GetShadowRoot()) {
      mParent = shadow;
      mXBLInvolved = Some(true);
      return;
    }
  }

  nsXBLBinding* binding =
      mParent->OwnerDoc()->BindingManager()->GetBindingWithContent(mParent);

  if (binding) {
    MOZ_ASSERT(binding->GetAnonymousContent());
    mParent = binding->GetAnonymousContent();
    mXBLInvolved = Some(true);
  }
}

bool FlattenedChildIterator::ComputeWhetherXBLIsInvolved() const {
  MOZ_ASSERT(mXBLInvolved.isNothing());
  // We set mXBLInvolved to true if either the node we're iterating has a
  // binding with content attached to it (in which case it is handled in Init),
  // the node is generated XBL content and has an <xbl:children> child, or the
  // node is a <slot> element.
  if (!mParent->GetBindingParent()) {
    return false;
  }

  if (mParentAsSlot) {
    return true;
  }

  for (nsIContent* child = mParent->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (child->NodeInfo()->Equals(nsGkAtoms::children, kNameSpaceID_XBL)) {
      MOZ_ASSERT(child->GetBindingParent());
      return true;
    }
  }

  return false;
}

bool ExplicitChildIterator::Seek(const nsIContent* aChildToFind) {
  if (aChildToFind->GetParent() == mParent &&
      !aChildToFind->IsRootOfAnonymousSubtree()) {
    // Fast path: just point ourselves to aChildToFind, which is a
    // normal DOM child of ours.
    mChild = const_cast<nsIContent*>(aChildToFind);
    mIndexInInserted = 0;
    mDefaultChild = nullptr;
    mIsFirst = false;
    MOZ_ASSERT(!mChild->IsActiveChildrenElement());
    return true;
  }

  // Can we add more fast paths here based on whether the parent of aChildToFind
  // is a shadow insertion point or content insertion point?

  // Slow path: just walk all our kids.
  return Seek(aChildToFind, nullptr);
}

nsIContent* ExplicitChildIterator::Get() const {
  MOZ_ASSERT(!mIsFirst);

  // When mParentAsSlot is set, mChild is always set to the current child. It
  // does not matter whether mChild is an assigned node or a fallback content.
  if (mParentAsSlot) {
    return mChild;
  }

  if (mIndexInInserted) {
    MOZ_ASSERT(mChild->IsActiveChildrenElement());
    auto* childrenElement = static_cast<XBLChildrenElement*>(mChild);
    return childrenElement->InsertedChild(mIndexInInserted - 1);
  }

  return mDefaultChild ? mDefaultChild : mChild;
}

nsIContent* ExplicitChildIterator::GetPreviousChild() {
  // If we're already in the inserted-children array, look there first
  if (mIndexInInserted) {
    if (mParentAsSlot) {
      const nsTArray<RefPtr<nsINode>>& assignedNodes =
          mParentAsSlot->AssignedNodes();

      mChild = (--mIndexInInserted)
                   ? assignedNodes[mIndexInInserted - 1]->AsContent()
                   : nullptr;

      if (!mChild) {
        mIsFirst = true;
      }
      return mChild;
    }

    // NB: mIndexInInserted points one past the last returned child so we need
    // to look *two* indices back in order to return the previous child.
    MOZ_ASSERT(mChild->IsActiveChildrenElement());
    auto* childrenElement = static_cast<XBLChildrenElement*>(mChild);
    if (--mIndexInInserted) {
      return childrenElement->InsertedChild(mIndexInInserted - 1);
    }
    mChild = mChild->GetPreviousSibling();
  } else if (mDefaultChild) {
    // If we're already in default content, check if there are more nodes there
    mDefaultChild = mDefaultChild->GetPreviousSibling();
    if (mDefaultChild) {
      return mDefaultChild;
    }

    mChild = mChild->GetPreviousSibling();
  } else if (mIsFirst) {  // at the beginning of the child list
    return nullptr;
  } else if (mChild) {  // in the middle of the child list
    mChild = mChild->GetPreviousSibling();
  } else {  // at the end of the child list
    // For slot parent, iterate over assigned nodes if not empty, otherwise
    // fall through and iterate over direct children (fallback content).
    if (mParentAsSlot) {
      const nsTArray<RefPtr<nsINode>>& assignedNodes =
          mParentAsSlot->AssignedNodes();
      if (!assignedNodes.IsEmpty()) {
        mIndexInInserted = assignedNodes.Length();
        mChild = assignedNodes[mIndexInInserted - 1]->AsContent();
        return mChild;
      }
    }

    mChild = mParent->GetLastChild();
  }

  // Iterate until we find a non-insertion point, or an insertion point with
  // content.
  while (mChild) {
    if (mChild->IsActiveChildrenElement()) {
      // If the current child being iterated is a content insertion point
      // then the iterator needs to return the nodes distributed into
      // the content insertion point.
      auto* childrenElement = static_cast<XBLChildrenElement*>(mChild);
      if (childrenElement->HasInsertedChildren()) {
        mIndexInInserted = childrenElement->InsertedChildrenLength();
        return childrenElement->InsertedChild(mIndexInInserted - 1);
      }

      mDefaultChild = mChild->GetLastChild();
      if (mDefaultChild) {
        return mDefaultChild;
      }

      mChild = mChild->GetPreviousSibling();
    } else {
      // mChild is not an insertion point, thus it is the next node to
      // return from this iterator.
      break;
    }
  }

  if (!mChild) {
    mIsFirst = true;
  }

  return mChild;
}

nsIContent* AllChildrenIterator::Get() const {
  switch (mPhase) {
    case eAtBeforeKid: {
      Element* before = nsLayoutUtils::GetBeforePseudo(mOriginalContent);
      MOZ_ASSERT(before, "No content before frame at eAtBeforeKid phase");
      return before;
    }

    case eAtExplicitKids:
      return ExplicitChildIterator::Get();

    case eAtAnonKids:
      return mAnonKids[mAnonKidsIdx];

    case eAtAfterKid: {
      Element* after = nsLayoutUtils::GetAfterPseudo(mOriginalContent);
      MOZ_ASSERT(after, "No content after frame at eAtAfterKid phase");
      return after;
    }

    default:
      return nullptr;
  }
}

bool AllChildrenIterator::Seek(const nsIContent* aChildToFind) {
  if (mPhase == eAtBegin || mPhase == eAtBeforeKid) {
    mPhase = eAtExplicitKids;
    Element* beforePseudo = nsLayoutUtils::GetBeforePseudo(mOriginalContent);
    if (beforePseudo && beforePseudo == aChildToFind) {
      mPhase = eAtBeforeKid;
      return true;
    }
  }

  if (mPhase == eAtExplicitKids) {
    if (ExplicitChildIterator::Seek(aChildToFind)) {
      return true;
    }
    mPhase = eAtAnonKids;
  }

  nsIContent* child = nullptr;
  do {
    child = GetNextChild();
  } while (child && child != aChildToFind);

  return child == aChildToFind;
}

void AllChildrenIterator::AppendNativeAnonymousChildren() {
  nsContentUtils::AppendNativeAnonymousChildren(mOriginalContent, mAnonKids,
                                                mFlags);
}

nsIContent* AllChildrenIterator::GetNextChild() {
  if (mPhase == eAtBegin) {
    mPhase = eAtExplicitKids;
    Element* beforeContent = nsLayoutUtils::GetBeforePseudo(mOriginalContent);
    if (beforeContent) {
      mPhase = eAtBeforeKid;
      return beforeContent;
    }
  }

  if (mPhase == eAtBeforeKid) {
    // Advance into our explicit kids.
    mPhase = eAtExplicitKids;
  }

  if (mPhase == eAtExplicitKids) {
    nsIContent* kid = ExplicitChildIterator::GetNextChild();
    if (kid) {
      return kid;
    }
    mPhase = eAtAnonKids;
  }

  if (mPhase == eAtAnonKids) {
    if (mAnonKids.IsEmpty()) {
      MOZ_ASSERT(mAnonKidsIdx == UINT32_MAX);
      AppendNativeAnonymousChildren();
      mAnonKidsIdx = 0;
    } else {
      if (mAnonKidsIdx == UINT32_MAX) {
        mAnonKidsIdx = 0;
      } else {
        mAnonKidsIdx++;
      }
    }

    if (mAnonKidsIdx < mAnonKids.Length()) {
      return mAnonKids[mAnonKidsIdx];
    }

    Element* afterContent = nsLayoutUtils::GetAfterPseudo(mOriginalContent);
    if (afterContent) {
      mPhase = eAtAfterKid;
      return afterContent;
    }
  }

  mPhase = eAtEnd;
  return nullptr;
}

nsIContent* AllChildrenIterator::GetPreviousChild() {
  if (mPhase == eAtEnd) {
    MOZ_ASSERT(mAnonKidsIdx == mAnonKids.Length());
    mPhase = eAtAnonKids;
    Element* afterContent = nsLayoutUtils::GetAfterPseudo(mOriginalContent);
    if (afterContent) {
      mPhase = eAtAfterKid;
      return afterContent;
    }
  }

  if (mPhase == eAtAfterKid) {
    mPhase = eAtAnonKids;
  }

  if (mPhase == eAtAnonKids) {
    if (mAnonKids.IsEmpty()) {
      AppendNativeAnonymousChildren();
      mAnonKidsIdx = mAnonKids.Length();
    }

    // If 0 then it turns into UINT32_MAX, which indicates the iterator is
    // before the anonymous children.
    --mAnonKidsIdx;
    if (mAnonKidsIdx < mAnonKids.Length()) {
      return mAnonKids[mAnonKidsIdx];
    }
    mPhase = eAtExplicitKids;
  }

  if (mPhase == eAtExplicitKids) {
    nsIContent* kid = ExplicitChildIterator::GetPreviousChild();
    if (kid) {
      return kid;
    }

    Element* beforeContent = nsLayoutUtils::GetBeforePseudo(mOriginalContent);
    if (beforeContent) {
      mPhase = eAtBeforeKid;
      return beforeContent;
    }
  }

  mPhase = eAtBegin;
  return nullptr;
}

}  // namespace dom
}  // namespace mozilla
