/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class for rendering objects that do not have child lists */

#include "nsLeafFrame.h"

#include "mozilla/PresShell.h"
#include "nsPresContext.h"

using namespace mozilla;

nsLeafFrame::~nsLeafFrame() = default;

/* virtual */
void nsLeafFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                   const nsDisplayListSet& aLists) {
  DO_GLOBAL_REFLOW_COUNT_DSP("nsLeafFrame");
  DisplayBorderBackgroundOutline(aBuilder, aLists);
}

nscoord nsLeafFrame::IntrinsicISize(const IntrinsicSizeInput& aInput,
                                    IntrinsicISizeType aType) {
  return GetIntrinsicSize().ISize(GetWritingMode()).valueOr(0);
}

/* virtual */
LogicalSize nsLeafFrame::ComputeAutoSize(
    gfxContext* aRenderingContext, WritingMode aWM, const LogicalSize& aCBSize,
    nscoord aAvailableISize, const LogicalSize& aMargin,
    const LogicalSize& aBorderPadding, const StyleSizeOverrides& aSizeOverrides,
    ComputeSizeFlags aFlags) {
  const WritingMode wm = GetWritingMode();
  IntrinsicSize intrinsicSize = GetIntrinsicSize();
  LogicalSize result(wm, intrinsicSize.ISize(wm).valueOr(0),
                     intrinsicSize.BSize(wm).valueOr(0));
  return result.ConvertTo(aWM, wm);
}
