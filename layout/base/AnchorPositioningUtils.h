/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AnchorPositioningUtils_h__
#define AnchorPositioningUtils_h__

class nsIFrame;

template <class T>
class nsTArray;

namespace mozilla {

/**
 * AnchorPositioningUtils is a namespace class used for various anchor
 * positioning helper functions that are useful in multiple places.
 * The goal is to avoid code duplication and to avoid having too
 * many helpers in nsLayoutUtils.
 */
struct AnchorPositioningUtils {
  /**
   * Finds the first acceptable frame from the list of possible anchor frames
   * following https://drafts.csswg.org/css-anchor-position-1/#target
   */
  static nsIFrame* FindFirstAcceptableAnchor(
      const nsIFrame* aPositionedFrame,
      const nsTArray<nsIFrame*>& aPossibleAnchorFrames);
};

}  // namespace mozilla

#endif  // AnchorPositioningUtils_h__
