/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* struct containing the output from nsIFrame::Reflow */

#include "mozilla/ReflowOutput.h"
#include "mozilla/ReflowInput.h"
#include "mozilla/WritingModes.h"

namespace mozilla {

static bool IsValidOverflowRect(const nsRect& aRect) {
  // The reason we can't simply use `nsRect::IsEmpty` is that any one dimension
  // being zero is considered empty by it - On the other hand, an overflow rect
  // is valid if it has non-negative dimensions and at least one of them is
  // non-zero.
  return aRect.Size() != nsSize{0, 0} && aRect.Width() >= 0 &&
         aRect.Height() >= 0;
}

/* static */
nsRect OverflowAreas::GetOverflowClipRect(const nsRect& aRectToClip,
                                          const nsRect& aBounds,
                                          PhysicalAxes aClipAxes,
                                          const nsSize& aOverflowMargin) {
  auto inflatedBounds = aBounds;
  inflatedBounds.Inflate(aOverflowMargin);
  auto clip = aRectToClip;
  if (aClipAxes.contains(PhysicalAxis::Vertical)) {
    clip.y = inflatedBounds.y;
    clip.height = inflatedBounds.height;
  }
  if (aClipAxes.contains(PhysicalAxis::Horizontal)) {
    clip.x = inflatedBounds.x;
    clip.width = inflatedBounds.width;
  }
  return clip;
}

/* static */
void OverflowAreas::ApplyOverflowClippingOnRect(nsRect& aOverflowRect,
                                                const nsRect& aBounds,
                                                PhysicalAxes aClipAxes,
                                                const nsSize& aOverflowMargin) {
  aOverflowRect = aOverflowRect.Intersect(
      GetOverflowClipRect(aOverflowRect, aBounds, aClipAxes, aOverflowMargin));
}

void OverflowAreas::UnionWith(const OverflowAreas& aOther) {
  if (IsValidOverflowRect(aOther.InkOverflow())) {
    InkOverflow().UnionRect(InkOverflow(), aOther.InkOverflow());
  }
  if (IsValidOverflowRect(aOther.ScrollableOverflow())) {
    ScrollableOverflow().UnionRect(ScrollableOverflow(),
                                   aOther.ScrollableOverflow());
  }
}

void OverflowAreas::UnionAllWith(const nsRect& aRect) {
  if (!IsValidOverflowRect(aRect)) {
    // Same as `UnionWith()` - avoid losing information.
    return;
  }
  InkOverflow().UnionRect(InkOverflow(), aRect);
  ScrollableOverflow().UnionRect(ScrollableOverflow(), aRect);
}

void OverflowAreas::SetAllTo(const nsRect& aRect) {
  InkOverflow() = aRect;
  ScrollableOverflow() = aRect;
}

ReflowOutput::ReflowOutput(const ReflowInput& aReflowInput)
    : ReflowOutput(aReflowInput.GetWritingMode()) {}

void ReflowOutput::SetOverflowAreasToDesiredBounds() {
  mOverflowAreas.SetAllTo(nsRect(0, 0, Width(), Height()));
}

void ReflowOutput::UnionOverflowAreasWithDesiredBounds() {
  mOverflowAreas.UnionAllWith(nsRect(0, 0, Width(), Height()));
}

}  // namespace mozilla
