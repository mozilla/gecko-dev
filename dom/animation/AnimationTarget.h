/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AnimationTarget_h
#define mozilla_AnimationTarget_h

#include "mozilla/Attributes.h"     // For MOZ_NON_OWNING_REF
#include "mozilla/HashFunctions.h"  // For HashNumber, AddToHash
#include "mozilla/HashTable.h"      // For DefaultHasher, PointerHasher
#include "mozilla/Maybe.h"
#include "mozilla/PseudoStyleType.h"  // For PseudoStyleRequest
#include "mozilla/RefPtr.h"

class nsAtom;

namespace mozilla {

namespace dom {
class Element;
}  // namespace dom

struct OwningAnimationTarget {
  OwningAnimationTarget() = default;
  OwningAnimationTarget(dom::Element* aElement,
                        const PseudoStyleRequest& aRequest)
      : mElement(aElement), mPseudoRequest(aRequest) {}

  explicit OwningAnimationTarget(dom::Element* aElement) : mElement(aElement) {}

  bool operator==(const OwningAnimationTarget& aOther) const {
    return mElement == aOther.mElement &&
           mPseudoRequest == aOther.mPseudoRequest;
  }

  explicit operator bool() const { return !!mElement; }

  // mElement represents the parent element of a pseudo-element, not the
  // generated content element.
  RefPtr<dom::Element> mElement;
  PseudoStyleRequest mPseudoRequest;
};

struct NonOwningAnimationTarget {
  NonOwningAnimationTarget() = default;
  NonOwningAnimationTarget(dom::Element* aElement,
                           const PseudoStyleRequest& aRequest)
      : mElement(aElement), mPseudoRequest(aRequest) {}

  explicit NonOwningAnimationTarget(const OwningAnimationTarget& aOther)
      : mElement(aOther.mElement), mPseudoRequest(aOther.mPseudoRequest) {}

  bool operator==(const NonOwningAnimationTarget& aOther) const {
    return mElement == aOther.mElement &&
           mPseudoRequest == aOther.mPseudoRequest;
  }

  NonOwningAnimationTarget& operator=(const OwningAnimationTarget& aOther) {
    mElement = aOther.mElement;
    mPseudoRequest = aOther.mPseudoRequest;
    return *this;
  }

  explicit operator bool() const { return !!mElement; }

  // mElement represents the parent element of a pseudo-element, not the
  // generated content element.
  dom::Element* MOZ_NON_OWNING_REF mElement = nullptr;
  PseudoStyleRequest mPseudoRequest;
};

// Helper functions for cycle-collecting Maybe<OwningAnimationTarget>
inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    Maybe<OwningAnimationTarget>& aTarget, const char* aName,
    uint32_t aFlags = 0) {
  if (aTarget) {
    ImplCycleCollectionTraverse(aCallback, aTarget->mElement, aName, aFlags);
  }
}

inline void ImplCycleCollectionUnlink(Maybe<OwningAnimationTarget>& aTarget) {
  if (aTarget) {
    ImplCycleCollectionUnlink(aTarget->mElement);
  }
}

// A DefaultHasher specialization for OwningAnimationTarget.
template <>
struct DefaultHasher<OwningAnimationTarget> {
  using Key = OwningAnimationTarget;
  using Lookup = OwningAnimationTarget;
  using PtrHasher = PointerHasher<dom::Element*>;
  using AtomPtrHasher = DefaultHasher<nsAtom*>;

  static HashNumber hash(const Lookup& aLookup) {
    return AddToHash(
        PtrHasher::hash(aLookup.mElement.get()),
        static_cast<uint8_t>(aLookup.mPseudoRequest.mType),
        AtomPtrHasher::hash(aLookup.mPseudoRequest.mIdentifier.get()));
  }

  static bool match(const Key& aKey, const Lookup& aLookup) {
    return PtrHasher::match(aKey.mElement.get(), aLookup.mElement.get()) &&
           aKey.mPseudoRequest.mType == aLookup.mPseudoRequest.mType &&
           AtomPtrHasher::match(aKey.mPseudoRequest.mIdentifier.get(),
                                aLookup.mPseudoRequest.mIdentifier.get());
  }

  static void rekey(Key& aKey, Key&& aNewKey) { aKey = std::move(aNewKey); }
};

}  // namespace mozilla

#endif  // mozilla_AnimationTarget_h
