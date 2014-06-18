/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGGenericContainerFrame.h"
#include "nsSVGIntegrationUtils.h"

//----------------------------------------------------------------------
// nsSVGGenericContainerFrame Implementation

nsIFrame*
NS_NewSVGGenericContainerFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGGenericContainerFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGGenericContainerFrame)

//----------------------------------------------------------------------
// nsIFrame methods

nsresult
nsSVGGenericContainerFrame::AttributeChanged(int32_t         aNameSpaceID,
                                             nsIAtom*        aAttribute,
                                             int32_t         aModType)
{
#ifdef DEBUG
    nsAutoString str;
    aAttribute->ToString(str);
    printf("** nsSVGGenericContainerFrame::AttributeChanged(%s)\n",
           NS_LossyConvertUTF16toASCII(str).get());
#endif

  return NS_OK;
}

nsIAtom *
nsSVGGenericContainerFrame::GetType() const
{
  return nsGkAtoms::svgGenericContainerFrame;
}

//----------------------------------------------------------------------
// nsSVGContainerFrame methods:

gfxMatrix
nsSVGGenericContainerFrame::GetCanvasTM(uint32_t aFor,
                                        nsIFrame* aTransformRoot)
{
  if (!(GetStateBits() & NS_FRAME_IS_NONDISPLAY) && !aTransformRoot) {
    if ((aFor == FOR_PAINTING && NS_SVGDisplayListPaintingEnabled()) ||
        (aFor == FOR_HIT_TESTING && NS_SVGDisplayListHitTestingEnabled())) {
      return nsSVGIntegrationUtils::GetCSSPxToDevPxMatrix(this);
    }
  }

  NS_ASSERTION(GetParent(), "null parent");
  
  return static_cast<nsSVGContainerFrame*>(GetParent())->
      GetCanvasTM(aFor, aTransformRoot);
}
