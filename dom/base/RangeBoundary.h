/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RangeBoundary_h
#define mozilla_RangeBoundary_h

#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/Assertions.h"
#include "mozilla/Maybe.h"

class nsRange;

namespace mozilla {
namespace dom {
class CrossShadowBoundaryRange;
}

template <typename T, typename U>
class EditorDOMPointBase;

// This class will maintain a reference to the child immediately
// before the boundary's offset. We try to avoid computing the
// offset as much as possible and just ensure mRef points to the
// correct child.
//
// mParent
//    |
// [child0] [child1] [child2]
//            /      |
//         mRef    mOffset=2
//
// If mOffset == 0, mRef is null.
// For text nodes, mRef will always be null and the offset will
// be kept up-to-date.

template <typename ParentType, typename RefType>
class RangeBoundaryBase;

using RangeBoundary =
    RangeBoundaryBase<nsCOMPtr<nsINode>, nsCOMPtr<nsIContent>>;
using RawRangeBoundary = RangeBoundaryBase<nsINode*, nsIContent*>;
using ConstRawRangeBoundary =
    RangeBoundaryBase<const nsINode*, const nsIContent*>;

/**
 * There are two ways of ensuring that `mRef` points to the correct node.
 * In most cases, the `RangeBoundary` is used by an object that is a
 * `MutationObserver` (i.e. `nsRange`) and replaces its `RangeBoundary`
 * objects when its parent chain changes.
 * However, there are Ranges which are not `MutationObserver`s (i.e.
 * `StaticRange`). `mRef` may become invalid when a DOM mutation happens.
 * Therefore, it needs to be recomputed using `mOffset` before it is being
 * accessed.
 * Because recomputing / validating of `mRef` could be an expensive operation,
 * it should be ensured that `Ref()` is called as few times as possible, i.e.
 * only once per method of `RangeBoundaryBase`.
 *
 * Furthermore, there are special implications when the `RangeBoundary` is not
 * used by an `MutationObserver`:
 * After a DOM mutation, the Boundary may point to something that is not valid
 * anymore, i.e. the `mOffset` is larger than `Container()->Length()`. In this
 * case, `Ref()` and `Get*ChildAtOffset()` return `nullptr` as an indication
 * that this RangeBoundary is not valid anymore. Also, `IsSetAndValid()`
 * returns false. However, `IsSet()` will still return true.
 *
 */
enum class RangeBoundaryIsMutationObserved { No = 0, Yes = 1 };

// This class has two types of specializations, one using reference counting
// pointers and one using raw pointers (both non-const and const versions). The
// latter help us avoid unnecessary AddRef/Release calls.
template <typename ParentType, typename RefType>
class RangeBoundaryBase {
  template <typename T, typename U>
  friend class RangeBoundaryBase;
  template <typename T, typename U>
  friend class EditorDOMPointBase;

  friend nsRange;

  friend class mozilla::dom::CrossShadowBoundaryRange;

  friend void ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback&,
                                          RangeBoundary&, const char*,
                                          uint32_t);
  friend void ImplCycleCollectionUnlink(RangeBoundary&);

  static const uint32_t kFallbackOffset = 0;

  template <typename T, typename Enable = void>
  struct GetNodeType;
  template <typename T>
  struct GetNodeType<T, std::enable_if_t<std::is_pointer_v<T>>> {
    using type = std::remove_pointer_t<T>;
  };
  template <typename T>
  struct GetNodeType<T, std::enable_if_t<!std::is_pointer_v<T>>> {
    using type = typename T::element_type;
  };

 public:
  using RawParentType = typename GetNodeType<ParentType>::type;
  static_assert(std::is_same_v<RawParentType, nsINode> ||
                std::is_same_v<RawParentType, const nsINode>);
  using RawRefType = typename GetNodeType<RefType>::type;
  static_assert(std::is_same_v<RawRefType, nsIContent> ||
                std::is_same_v<RawRefType, const nsIContent>);

  RangeBoundaryBase(RawParentType* aContainer, RawRefType* aRef)
      : mParent(aContainer), mRef(aRef), mIsMutationObserved(true) {
    if (mRef) {
      NS_WARNING_ASSERTION(mRef->GetParentNode() == mParent,
                           "Initializing RangeBoundary with invalid value");
    } else {
      mOffset.emplace(0);
    }
  }

  RangeBoundaryBase(RawParentType* aContainer, uint32_t aOffset,
                    RangeBoundaryIsMutationObserved aRangeIsMutationObserver =
                        RangeBoundaryIsMutationObserved::Yes)
      : mParent(aContainer),
        mRef(nullptr),
        mOffset(mozilla::Some(aOffset)),
        mIsMutationObserved(bool(aRangeIsMutationObserver)) {
    if (mIsMutationObserved && mParent && mParent->IsContainerNode()) {
      // Find a reference node
      if (aOffset == mParent->GetChildCount()) {
        mRef = mParent->GetLastChild();
      } else if (aOffset > 0) {
        mRef = mParent->GetChildAt_Deprecated(aOffset - 1);
      }
      NS_WARNING_ASSERTION(mRef || aOffset == 0,
                           "Constructing RangeBoundary with invalid value");
    }
    NS_WARNING_ASSERTION(!mRef || mRef->GetParentNode() == mParent,
                         "Constructing RangeBoundary with invalid value");
  }

  /**
   * Special constructor to create RangeBoundaryBase which stores both mRef and
   * mOffset.  This can make the instance provide both mRef and mOffset without
   * computation, but the creator needs to guarantee that this is valid at least
   * at construction.
   */
  RangeBoundaryBase(RawParentType* aContainer, RawRefType* aRef,
                    uint32_t aOffset,
                    RangeBoundaryIsMutationObserved aRangeIsMutationObserver =
                        RangeBoundaryIsMutationObserved::Yes)
      : mParent(const_cast<nsINode*>(aContainer)),
        mRef(const_cast<nsIContent*>(aRef)),
        mOffset(mozilla::Some(aOffset)),
        mIsMutationObserved(bool(aRangeIsMutationObserver)) {
    MOZ_ASSERT(IsSetAndValid());
  }

  RangeBoundaryBase()
      : mParent(nullptr), mRef(nullptr), mIsMutationObserved(true) {}

  // Convert from RawRangeBoundary or RangeBoundary.
  template <typename PT, typename RT,
            typename = std::enable_if_t<!std::is_const_v<RawParentType> ||
                                        std::is_const_v<PT>>>
  RangeBoundaryBase(const RangeBoundaryBase<PT, RT>& aOther,
                    RangeBoundaryIsMutationObserved aIsMutationObserved)
      : mParent(aOther.mParent),
        mRef(aOther.mRef),
        mOffset(aOther.mOffset),
        mIsMutationObserved(bool(aIsMutationObserved)) {}

  /**
   * This method may return `nullptr` in two cases:
   *  1. `mIsMutationObserved` is true and the boundary points to the first
   *      child of `mParent`.
   *  2. `mIsMutationObserved` is false and `mOffset` is out of bounds for
   *     `mParent`s child list.
   * If `mIsMutationObserved` is false, this method may do some significant
   * computation. Therefore it is advised to call it as seldom as possible.
   * Code inside of this class should call this method exactly one time and
   * afterwards refer to `mRef` directly.
   */
  RawRefType* Ref() const {
    if (mIsMutationObserved) {
      return mRef;
    }
    MOZ_ASSERT(mParent);
    MOZ_ASSERT(mOffset);

    // `mRef` may have become invalid due to some DOM mutation,
    // which is not monitored here. Therefore, we need to validate `mRef`
    // manually.
    if (*mOffset > GetContainer()->Length()) {
      // offset > child count means that the range boundary has become invalid
      // due to a DOM mutation.
      mRef = nullptr;
    } else if (*mOffset == GetContainer()->Length()) {
      mRef = mParent->GetLastChild();
    } else if (*mOffset) {
      // validate and update `mRef`.
      // If `ComputeIndexOf()` returns `Nothing`, then `mRef` is not a child of
      // `mParent` anymore.
      // If the returned index for `mRef` does not match to `mOffset`, `mRef`
      // needs to be updated.
      auto indexOfRefObject = mParent->ComputeIndexOf(mRef);
      if (indexOfRefObject.isNothing() || *mOffset != *indexOfRefObject + 1) {
        mRef = mParent->GetChildAt_Deprecated(*mOffset - 1);
      }
    } else {
      mRef = nullptr;
    }
    return mRef;
  }

  RawParentType* GetContainer() const { return mParent; }

  dom::Document* GetComposedDoc() const {
    return mParent ? mParent->GetComposedDoc() : nullptr;
  }

  /**
   * This method may return `nullptr` if `mIsMutationObserved` is false and
   * `mOffset` is out of bounds.
   */
  RawRefType* GetChildAtOffset() const {
    if (!mParent || !mParent->IsContainerNode()) {
      return nullptr;
    }
    RawRefType* const ref = Ref();
    if (!ref) {
      if (!mIsMutationObserved && *mOffset != 0) {
        // This means that this boundary is invalid.
        // `mOffset` is out of bounds.
        return nullptr;
      }
      MOZ_ASSERT(*Offset(OffsetFilter::kValidOrInvalidOffsets) == 0,
                 "invalid RangeBoundary");
      return mParent->GetFirstChild();
    }
    MOZ_ASSERT(mParent->GetChildAt_Deprecated(
                   *Offset(OffsetFilter::kValidOrInvalidOffsets)) ==
               ref->GetNextSibling());
    return ref->GetNextSibling();
  }

  /**
   * GetNextSiblingOfChildOffset() returns next sibling of a child at offset.
   * If this refers after the last child or the container cannot have children,
   * this returns nullptr with warning.
   */
  RawRefType* GetNextSiblingOfChildAtOffset() const {
    if (NS_WARN_IF(!mParent) || NS_WARN_IF(!mParent->IsContainerNode())) {
      return nullptr;
    }
    RawRefType* const ref = Ref();
    if (!ref) {
      if (!mIsMutationObserved && *mOffset != 0) {
        // This means that this boundary is invalid.
        // `mOffset` is out of bounds.
        return nullptr;
      }
      MOZ_ASSERT(*Offset(OffsetFilter::kValidOffsets) == 0,
                 "invalid RangeBoundary");
      nsIContent* firstChild = mParent->GetFirstChild();
      if (NS_WARN_IF(!firstChild)) {
        // Already referring the end of the container.
        return nullptr;
      }
      return firstChild->GetNextSibling();
    }
    if (NS_WARN_IF(!ref->GetNextSibling())) {
      // Already referring the end of the container.
      return nullptr;
    }
    return ref->GetNextSibling()->GetNextSibling();
  }

  /**
   * GetPreviousSiblingOfChildAtOffset() returns previous sibling of a child
   * at offset.  If this refers the first child or the container cannot have
   * children, this returns nullptr with warning.
   */
  RawRefType* GetPreviousSiblingOfChildAtOffset() const {
    if (NS_WARN_IF(!mParent) || NS_WARN_IF(!mParent->IsContainerNode())) {
      return nullptr;
    }
    RawRefType* const ref = Ref();
    if (NS_WARN_IF(!ref)) {
      // Already referring the start of the container.
      return nullptr;
    }
    return ref;
  }

  /**
   * Return true if this has already computed/set offset.
   */
  [[nodiscard]] bool HasOffset() const { return mOffset.isSome(); }

  enum class OffsetFilter { kValidOffsets, kValidOrInvalidOffsets };

  /**
   * @return maybe an offset, depending on aOffsetFilter. If it is:
   *         kValidOffsets: if the offset is valid, it, Nothing{} otherwise.
   *         kValidOrInvalidOffsets: the internally stored offset, even if
   *                                 invalid, or if not available, a defined
   *                                 default value. That is, always some value.
   */
  Maybe<uint32_t> Offset(const OffsetFilter aOffsetFilter) const {
    switch (aOffsetFilter) {
      case OffsetFilter::kValidOffsets: {
        if (IsSetAndValid()) {
          MOZ_ASSERT_IF(!mIsMutationObserved, mOffset);
          if (!mOffset && mIsMutationObserved) {
            DetermineOffsetFromReference();
          }
        }
        return !mIsMutationObserved && *mOffset > GetContainer()->Length()
                   ? Nothing{}
                   : mOffset;
      }
      case OffsetFilter::kValidOrInvalidOffsets: {
        MOZ_ASSERT_IF(!mIsMutationObserved, mOffset.isSome());
        if (mOffset.isSome()) {
          return mOffset;
        }
        if (mParent && mIsMutationObserved) {
          DetermineOffsetFromReference();
          if (mOffset.isSome()) {
            return mOffset;
          }
        }

        return Some(kFallbackOffset);
      }
    }

    // Needed to calm the compiler. There was deliberately no default case added
    // to the above switch-statement, because it would prevent build-errors when
    // not all enumerators are handled.
    MOZ_ASSERT_UNREACHABLE();
    return Some(kFallbackOffset);
  }

  friend std::ostream& operator<<(
      std::ostream& aStream,
      const RangeBoundaryBase<ParentType, RefType>& aRangeBoundary) {
    aStream << "{ mParent=" << aRangeBoundary.GetContainer();
    if (aRangeBoundary.GetContainer()) {
      aStream << " (" << *aRangeBoundary.GetContainer()
              << ", Length()=" << aRangeBoundary.GetContainer()->Length()
              << ")";
    }
    if (aRangeBoundary.mIsMutationObserved) {
      aStream << ", mRef=" << aRangeBoundary.mRef;
      if (aRangeBoundary.mRef) {
        aStream << " (" << *aRangeBoundary.mRef << ")";
      }
    }

    aStream << ", mOffset=" << aRangeBoundary.mOffset;
    aStream << ", mIsMutationObserved="
            << (aRangeBoundary.mIsMutationObserved ? "true" : "false") << " }";
    return aStream;
  }

 private:
  void DetermineOffsetFromReference() const {
    MOZ_ASSERT(mParent);
    MOZ_ASSERT(mRef);
    MOZ_ASSERT(mRef->GetParentNode() == mParent);
    MOZ_ASSERT(mIsMutationObserved);
    MOZ_ASSERT(mOffset.isNothing());

    if (mRef->IsBeingRemoved()) {
      // ComputeIndexOf would return nothing because mRef has already been
      // removed from the child node chain of mParent.
      return;
    }

    const Maybe<uint32_t> index = mParent->ComputeIndexOf(mRef);
    MOZ_ASSERT(*index != UINT32_MAX);
    mOffset.emplace(MOZ_LIKELY(index.isSome()) ? *index + 1u : 0u);
  }

  void InvalidateOffset() {
    MOZ_ASSERT(mParent);
    MOZ_ASSERT(mParent->IsContainerNode(),
               "Range is positioned on a text node!");
    if (!mIsMutationObserved) {
      // RangeBoundaries that are not used in the context of a
      // `MutationObserver` use the offset as main source of truth to compute
      // `mRef`. Therefore, it must not be updated or invalidated.
      return;
    }
    if (!mRef) {
      MOZ_ASSERT(mOffset.isSome() && mOffset.value() == 0,
                 "Invalidating offset of invalid RangeBoundary?");
      return;
    }
    mOffset.reset();
  }

 public:
  void NotifyParentBecomesShadowHost() {
    MOZ_ASSERT(mParent);
    MOZ_ASSERT(mParent->IsContainerNode(),
               "Range is positioned on a text node!");
    if (!StaticPrefs::dom_shadowdom_selection_across_boundary_enabled()) {
      return;
    }

    if (!mIsMutationObserved) {
      // RangeBoundaries that are not used in the context of a
      // `MutationObserver` use the offset as main source of truth to compute
      // `mRef`. Therefore, it must not be updated or invalidated.
      return;
    }

    if (!mRef) {
      MOZ_ASSERT(mOffset.isSome() && mOffset.value() == 0,
                 "Invalidating offset of invalid RangeBoundary?");
      return;
    }

    if (dom::ShadowRoot* shadowRoot = mParent->GetShadowRootForSelection()) {
      mParent = shadowRoot;
    }

    mOffset = Some(0);
  }

  bool IsSet() const { return mParent && (mRef || mOffset.isSome()); }

  [[nodiscard]] bool IsSetAndInComposedDoc() const {
    return IsSet() && mParent->IsInComposedDoc();
  }

  bool IsSetAndValid() const {
    if (!IsSet()) {
      return false;
    }

    if (mIsMutationObserved && Ref()) {
      // XXX mRef refers previous sibling of pointing child.  Therefore, it
      //     seems odd that this becomes invalid due to its removal.  Should we
      //     change RangeBoundaryBase to refer child at offset directly?
      return Ref()->GetParentNode() == GetContainer() &&
             !Ref()->IsBeingRemoved();
    }

    MOZ_ASSERT(mOffset.isSome());
    return *mOffset <= GetContainer()->Length();
  }

  bool IsStartOfContainer() const {
    // We're at the first point in the container if we don't have a reference,
    // and our offset is 0. If we don't have a Ref, we should already have an
    // offset, so we can just directly fetch it.
    return mIsMutationObserved ? !Ref() && mOffset.value() == 0
                               : mOffset.value() == 0;
  }

  bool IsEndOfContainer() const {
    // We're at the last point in the container if Ref is a pointer to the last
    // child in GetContainer(), or our Offset() is the same as the length of our
    // container. If we don't have a Ref, then we should already have an offset,
    // so we can just directly fetch it.
    return mIsMutationObserved && Ref()
               ? !Ref()->GetNextSibling()
               : mOffset.value() == GetContainer()->Length();
  }

  // Convenience methods for switching between the two types
  // of RangeBoundary.
  template <typename PT = RawParentType,
            typename = std::enable_if_t<!std::is_const_v<PT>>>
  RawRangeBoundary AsRaw() const {
    return RawRangeBoundary(
        *this, RangeBoundaryIsMutationObserved(mIsMutationObserved));
  }
  ConstRawRangeBoundary AsConstRaw() const {
    return ConstRawRangeBoundary(
        *this, RangeBoundaryIsMutationObserved(mIsMutationObserved));
  }

  template <typename A, typename B>
  RangeBoundaryBase& operator=(const RangeBoundaryBase<A, B>& aOther) = delete;

  template <
      typename PT, typename RT, typename RPT = RawParentType,
      typename = std::enable_if_t<!std::is_const_v<PT> || std::is_const_v<RPT>>>
  RangeBoundaryBase& CopyFrom(
      const RangeBoundaryBase<PT, RT>& aOther,
      RangeBoundaryIsMutationObserved aIsMutationObserved) {
    // mParent and mRef can be strong pointers, so better to try to avoid any
    // extra AddRef/Release calls.
    if (mParent != aOther.mParent) {
      mParent = aOther.mParent;
    }
    if (mRef != aOther.mRef) {
      mRef = aOther.mRef;
    }
    mIsMutationObserved = bool(aIsMutationObserved);
    if (!mIsMutationObserved && aOther.mOffset.isNothing()) {
      // "Fix" the offset from mRef if and only if we won't be updated for
      // further mutations and aOther has not computed the offset of its mRef.
      // XXX What should we do if aOther is not updated for mutations and
      // mOffset has already been invalid?
      mOffset = aOther.Offset(
          RangeBoundaryBase<PT, RT>::OffsetFilter::kValidOrInvalidOffsets);
      MOZ_DIAGNOSTIC_ASSERT(mOffset.isSome());
    } else {
      mOffset = aOther.mOffset;
    }
    // If the mutation will be observed but the other does not have proper
    // mRef for its mOffset, we need to compute mRef like the constructor
    // which takes aOffset.
    if (mIsMutationObserved && !mRef && mParent && mOffset.isSome() &&
        *mOffset) {
      if (*mOffset == mParent->GetChildCount()) {
        mRef = mParent->GetLastChild();
      } else {
        mRef = mParent->GetChildAt_Deprecated(*mOffset - 1);
      }
    }
    return *this;
  }

  bool Equals(const RawParentType* aNode, uint32_t aOffset) const {
    if (mParent != aNode) {
      return false;
    }

    const Maybe<uint32_t> offset = Offset(OffsetFilter::kValidOffsets);
    return offset && (*offset == aOffset);
  }

  template <typename A, typename B>
  [[nodiscard]] bool operator==(const RangeBoundaryBase<A, B>& aOther) const {
    if (!mParent && !aOther.mParent) {
      return true;
    }
    if (mParent != aOther.mParent) {
      return false;
    }
    if (RefIsFixed() && aOther.RefIsFixed()) {
      return mRef == aOther.mRef;
    }
    return Offset(OffsetFilter::kValidOrInvalidOffsets) ==
           aOther.Offset(
               RangeBoundaryBase<A, B>::OffsetFilter::kValidOrInvalidOffsets);
  }

  template <typename A, typename B>
  bool operator!=(const RangeBoundaryBase<A, B>& aOther) const {
    return !(*this == aOther);
  }

 private:
  [[nodiscard]] bool RefIsFixed() const {
    return mParent &&
           (
               // If mutation is observed, mRef is the base of mOffset unless
               // it's not a container node like `Text` node.
               (mIsMutationObserved && (mRef || mParent->IsContainerNode())) ||
               // If offset is not set, we would compute mOffset from mRef.
               // So, mRef is "fixed" for now.
               mOffset.isNothing());
  }

  ParentType mParent;
  mutable RefType mRef;

  mutable mozilla::Maybe<uint32_t> mOffset;
  bool mIsMutationObserved;
};

template <typename ParentType, typename RefType>
const uint32_t RangeBoundaryBase<ParentType, RefType>::kFallbackOffset;

inline void ImplCycleCollectionUnlink(RangeBoundary& aField) {
  ImplCycleCollectionUnlink(aField.mParent);
  ImplCycleCollectionUnlink(aField.mRef);
}

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback, RangeBoundary& aField,
    const char* aName, uint32_t aFlags) {
  ImplCycleCollectionTraverse(aCallback, aField.mParent, "mParent", 0);
  ImplCycleCollectionTraverse(aCallback, aField.mRef, "mRef", 0);
}

}  // namespace mozilla

#endif  // defined(mozilla_RangeBoundary_h)
