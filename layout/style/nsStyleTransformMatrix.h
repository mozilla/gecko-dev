/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A class representing three matrices that can be used for style transforms.
 */

#ifndef nsStyleTransformMatrix_h_
#define nsStyleTransformMatrix_h_

#include "nsCSSValue.h"
#include "gfx3DMatrix.h"

class nsStyleContext;
class nsPresContext;
struct nsRect;

/**
 * A helper to generate gfxMatrixes from css transform functions.
 */
namespace nsStyleTransformMatrix {
  
  /**
   * Return the transform function, as an nsCSSKeyword, for the given
   * nsCSSValue::Array from a transform list.
   */
  nsCSSKeyword TransformFunctionOf(const nsCSSValue::Array* aData);

  float ProcessTranslatePart(const nsCSSValue& aValue,
                             nsStyleContext* aContext,
                             nsPresContext* aPresContext,
                             bool& aCanStoreInRuleTree,
                             nscoord aSize);

  void
  ProcessInterpolateMatrix(gfx3DMatrix& aMatrix,
                            const nsCSSValue::Array* aData,
                            nsStyleContext* aContext,
                            nsPresContext* aPresContext,
                            bool& aCanStoreInRuleTree,
                            nsRect& aBounds);

  /**
   * Given an nsCSSValueList containing -moz-transform functions,
   * returns a matrix containing the value of those functions.
   *
   * @param aData The nsCSSValueList containing the transform functions
   * @param aContext The style context, used for unit conversion.
   * @param aPresContext The presentation context, used for unit conversion.
   * @param aCanStoreInRuleTree Set to false if the result cannot be cached
   *                            in the rule tree, otherwise untouched.
   * @param aBounds The frame's bounding rectangle.
   * @param aAppUnitsPerMatrixUnit The number of app units per device pixel.
   *
   * aContext and aPresContext may be null if all of the (non-percent)
   * length values in aData are already known to have been converted to
   * eCSSUnit_Pixel (as they are in an StyleAnimationValue)
   */
  gfx3DMatrix ReadTransforms(const nsCSSValueList* aList,
                             nsStyleContext* aContext,
                             nsPresContext* aPresContext,
                             bool &aCanStoreInRuleTree,
                             nsRect& aBounds,
                             float aAppUnitsPerMatrixUnit);

} // namespace nsStyleTransformMatrix

#endif
