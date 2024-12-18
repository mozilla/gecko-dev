/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_IntrinsicISizesCache_h
#define mozilla_IntrinsicISizesCache_h

#include "nsIFrame.h"

namespace mozilla {

// Some frame classes keep a cache of intrinsic inline sizes. This class
// encapsulates the logic for caching them depending on the IntrinsicSizeInput.
//
// The cache is intended to take as little space as possible
// (max(sizeof(nscoord) * 2, sizeof(void*))), when there are no percentage-size
// dependencies.
struct IntrinsicISizesCache final {
  IntrinsicISizesCache() {
    new (&mInline) InlineCache();
    MOZ_ASSERT(IsInline());
  }

  ~IntrinsicISizesCache() { delete GetOutOfLine(); }

  template <typename Compute>
  nscoord GetOrSet(nsIFrame& aFrame, IntrinsicISizeType aType,
                   const IntrinsicSizeInput& aInput, Compute aCompute) {
    bool dependentOnPercentBSize = aFrame.HasAnyStateBits(
        NS_FRAME_DESCENDANT_INTRINSIC_ISIZE_DEPENDS_ON_BSIZE);
    nscoord value = Get(dependentOnPercentBSize, aType, aInput);
    if (value != kNotFound) {
      return value;
    }
    value = aCompute();
    // Inside of aCompute(), we might have newly discovered that we do have a
    // descendant whose intrinsic isize depends on our bsize; so we check that
    // state bit again before updating the cache.
    dependentOnPercentBSize = aFrame.HasAnyStateBits(
        NS_FRAME_DESCENDANT_INTRINSIC_ISIZE_DEPENDS_ON_BSIZE);
    Set(dependentOnPercentBSize, aType, aInput, value);
    return value;
  }

  void Clear() {
    if (auto* ool = GetOutOfLine()) {
      ool->mCacheWithPercentageBasis.Clear();
      ool->mCacheWithoutPercentageBasis.Clear();
      ool->mLastPercentageBasis.reset();
    } else {
      mInline.Clear();
    }
  }

 private:
  // We use nscoord_MAX rather than NS_INTRINSIC_ISIZE_UNKNOWN as our sentinel
  // value so that our high bit is always free.
  static constexpr nscoord kNotFound = nscoord_MAX;

  nscoord Get(bool aDependentOnPercentBSize, IntrinsicISizeType aType,
              const IntrinsicSizeInput& aInput) const {
    const bool usePercentageAwareCache =
        aDependentOnPercentBSize && aInput.HasSomePercentageBasisForChildren();
    if (!usePercentageAwareCache) {
      if (auto* ool = GetOutOfLine()) {
        return ool->mCacheWithoutPercentageBasis.Get(aType);
      }
      return mInline.Get(aType);
    }
    if (auto* ool = GetOutOfLine()) {
      if (ool->mLastPercentageBasis == aInput.mPercentageBasisForChildren) {
        return ool->mCacheWithPercentageBasis.Get(aType);
      }
    }
    return kNotFound;
  }

  void Set(bool aDependentOnPercentBSize, IntrinsicISizeType aType,
           const IntrinsicSizeInput& aInput, nscoord aValue) {
    // Intrinsic sizes should be nonnegative, so this std::max clamping should
    // rarely be necessary except in cases of integer overflow.  We have to be
    // strict about it, though, because of how we (ab)use the high bit
    // (see kHighBit)
    aValue = std::max(aValue, 0);
    const bool usePercentageAwareCache =
        aDependentOnPercentBSize && aInput.HasSomePercentageBasisForChildren();
    if (usePercentageAwareCache) {
      auto* ool = EnsureOutOfLine();
      ool->mLastPercentageBasis = aInput.mPercentageBasisForChildren;
      ool->mCacheWithPercentageBasis.Set(aType, aValue);
    } else if (auto* ool = GetOutOfLine()) {
      ool->mCacheWithoutPercentageBasis.Set(aType, aValue);
    } else {
      mInline.Set(aType, aValue);
      // No inline value should be able to cause us to misinterpret our
      // representation as out-of-line, because intrinsic isizes should always
      // be non-negative.
      MOZ_DIAGNOSTIC_ASSERT(IsInline());
    }
  }

  struct InlineCache {
    nscoord mCachedMinISize = kNotFound;
    nscoord mCachedPrefISize = kNotFound;

    nscoord Get(IntrinsicISizeType aType) const {
      return aType == IntrinsicISizeType::MinISize ? mCachedMinISize
                                                   : mCachedPrefISize;
    }
    void Set(IntrinsicISizeType aType, nscoord aValue) {
      MOZ_ASSERT(aValue >= 0);
      if (aType == IntrinsicISizeType::MinISize) {
        mCachedMinISize = aValue;
      } else {
        mCachedPrefISize = aValue;
      }
    }

    void Clear() { *this = {}; }
  };

  struct OutOfLineCache {
    InlineCache mCacheWithoutPercentageBasis;
    InlineCache mCacheWithPercentageBasis;
    Maybe<LogicalSize> mLastPercentageBasis;
  };

  // If the high bit of mOutOfLine is 1, then it points to an OutOfLineCache.
  union {
    InlineCache mInline;
    struct {
#ifndef HAVE_64BIT_BUILD
      uintptr_t mPadding = 0;
#endif
      uintptr_t mOutOfLine = 0;
    };
  };

  static constexpr uintptr_t kHighBit = uintptr_t(1)
                                        << (sizeof(void*) * CHAR_BIT - 1);

  bool IsOutOfLine() const {
#ifdef HAVE_64BIT_BUILD
    return mOutOfLine & kHighBit;
#else
    return mPadding & kHighBit;
#endif
  }
  bool IsInline() const { return !IsOutOfLine(); }
  OutOfLineCache* EnsureOutOfLine() {
    if (auto* ool = GetOutOfLine()) {
      return ool;
    }
    auto inlineCache = mInline;
    auto* ool = new OutOfLineCache();
    ool->mCacheWithoutPercentageBasis = inlineCache;
#ifdef HAVE_64BIT_BUILD
    MOZ_ASSERT((reinterpret_cast<uintptr_t>(ool) & kHighBit) == 0);
    mOutOfLine = reinterpret_cast<uintptr_t>(ool) | kHighBit;
#else
    mOutOfLine = reinterpret_cast<uintptr_t>(ool);
    mPadding = kHighBit;
#endif
    MOZ_ASSERT(IsOutOfLine());
    return ool;
  }

  OutOfLineCache* GetOutOfLine() const {
    if (!IsOutOfLine()) {
      return nullptr;
    }
#ifdef HAVE_64BIT_BUILD
    return reinterpret_cast<OutOfLineCache*>(mOutOfLine & ~kHighBit);
#else
    return reinterpret_cast<OutOfLineCache*>(mOutOfLine);
#endif
  }
};

static_assert(sizeof(IntrinsicISizesCache) == 8, "Unexpected cache size");

}  // namespace mozilla

#endif
