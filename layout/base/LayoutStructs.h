/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Helper structs or classes used throughout the Layout module */

#ifndef mozilla_LayoutStructs_h
#define mozilla_LayoutStructs_h

#include "mozilla/AspectRatio.h"
#include "mozilla/ServoStyleConsts.h"

namespace mozilla {

/**
 * A set of StyleSizes used as an input parameter to various functions that
 * compute sizes like nsIFrame::ComputeSize(). If any of the member fields has a
 * value, the function may use the value instead of retrieving it from the
 * frame's style.
 *
 * The logical sizes are assumed to be in the associated frame's writing-mode.
 */
struct StyleSizeOverrides {
  Maybe<StyleSize> mStyleISize;
  Maybe<StyleSize> mStyleBSize;
  Maybe<AspectRatio> mAspectRatio;

  bool HasAnyOverrides() const { return mStyleISize || mStyleBSize; }
  bool HasAnyLengthOverrides() const {
    return (mStyleISize && mStyleISize->ConvertsToLength()) ||
           (mStyleBSize && mStyleBSize->ConvertsToLength());
  }

  // By default, table wrapper frame considers the size overrides applied to
  // itself, so it creates any length size overrides for inner table frame by
  // subtracting the area occupied by the caption and border & padding according
  // to box-sizing.
  //
  // When this flag is true, table wrapper frame is required to apply the size
  // overrides to the inner table frame directly, without any modification,
  // which is useful for flex container to override the inner table frame's
  // preferred main size with 'flex-basis'.
  //
  // Note: if mStyleISize is a LengthPercentage, the inner table frame will
  // comply with the inline-size override without enforcing its min-content
  // inline-size in nsTableFrame::ComputeSize(). This is necessary so that small
  // flex-basis values like 'flex-basis:1%' can be resolved correctly; the
  // flexbox layout algorithm does still explicitly clamp to min-sizes *at a
  // later step*, after the flex-basis has been resolved -- so this flag won't
  // actually produce any user-visible tables whose final inline size is smaller
  // than their min-content inline size.
  bool mApplyOverridesVerbatim = false;
};

}  // namespace mozilla

#endif  // mozilla_LayoutStructs_h
