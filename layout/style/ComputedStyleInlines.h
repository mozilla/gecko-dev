/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Inlined methods for ComputedStyle. Will just redirect to
 * GeckoComputedStyle methods when compiled without stylo, but will do
 * virtual dispatch (by checking which kind of container it is)
 * in stylo mode.
 */

#ifndef ComputedStyleInlines_h
#define ComputedStyleInlines_h

#include "mozilla/ComputedStyle.h"
#include "mozilla/ServoComputedDataInlines.h"
#include "mozilla/ServoUtils.h"

namespace mozilla {

#define STYLE_STRUCT(name_)                                       \
  const nsStyle##name_* ComputedStyle::Style##name_() {           \
    return DoGetStyle##name_<true>();                             \
  }                                                               \
  const nsStyle##name_* ComputedStyle::ThreadsafeStyle##name_() { \
    if (mozilla::IsInServoTraversal()) {                          \
      return ComputedData()->GetStyle##name_();                   \
    }                                                             \
    return Style##name_();                                        \
  }                                                               \
  const nsStyle##name_* ComputedStyle::PeekStyle##name_() {       \
    return DoGetStyle##name_<false>();                            \
  }
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

// Helper functions for GetStyle* and PeekStyle*
#define STYLE_STRUCT(name_)                                                  \
  template <bool aComputeData>                                               \
  const nsStyle##name_* ComputedStyle::DoGetStyle##name_() {                 \
    const auto kStructID = StyleStructID::name_;                             \
    const bool needToCompute = !HasRequestedStruct(kStructID);               \
    if (!aComputeData && needToCompute) {                                    \
      return nullptr;                                                        \
    }                                                                        \
    const nsStyle##name_* data = ComputedData()->GetStyle##name_();          \
    /* perform any remaining main thread work on the struct */               \
    if (needToCompute) {                                                     \
      MOZ_ASSERT(NS_IsMainThread());                                         \
      MOZ_ASSERT(!mozilla::IsInServoTraversal());                            \
      const_cast<nsStyle##name_*>(data)->FinishStyle(mPresContext, nullptr); \
      /* the ComputedStyle owns the struct */                                \
      SetRequestedStruct(kStructID);                                         \
    }                                                                        \
    return data;                                                             \
  }
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

void ComputedStyle::StartBackgroundImageLoads() {
  // Just get our background struct; that should do the trick
  StyleBackground();
}

void ComputedStyle::ResolveSameStructsAs(const ComputedStyle* aOther) {
  // Only resolve structs that are not already resolved in this struct.
  auto newBits = aOther->mRequestedStructs & ~mRequestedStructs;

#define STYLE_STRUCT(name_)                                                    \
  if (nsStyle##name_::kHasFinishStyle &&                                       \
      (newBits & StyleStructConstants::BitFor(StyleStructID::name_))) {        \
    const nsStyle##name_* data = ComputedData()->GetStyle##name_();            \
    const nsStyle##name_* oldData = aOther->ComputedData()->GetStyle##name_(); \
    const_cast<nsStyle##name_*>(data)->FinishStyle(mPresContext, oldData);     \
  }
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

  mRequestedStructs |= newBits;
}

}  // namespace mozilla

#endif  // ComputedStyleInlines_h
