/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSSVGGFRAME_H
#define NSSVGGFRAME_H

#include "mozilla/Attributes.h"
#include "gfxMatrix.h"
#include "nsSVGContainerFrame.h"

typedef nsSVGDisplayContainerFrame nsSVGGFrameBase;

class nsSVGGFrame : public nsSVGGFrameBase
{
  friend nsIFrame*
  NS_NewSVGGFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  nsSVGGFrame(nsStyleContext* aContext) :
    nsSVGGFrameBase(aContext) {}

public:
  NS_DECL_FRAMEARENA_HELPERS

#ifdef DEBUG
  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) MOZ_OVERRIDE;
#endif

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgGFrame
   */
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const MOZ_OVERRIDE
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGG"), aResult);
  }
#endif

  // nsIFrame interface:
  virtual nsresult AttributeChanged(int32_t         aNameSpaceID,
                                    nsIAtom*        aAttribute,
                                    int32_t         aModType) MOZ_OVERRIDE;

  // nsISVGChildFrame interface:
  virtual void NotifySVGChanged(uint32_t aFlags) MOZ_OVERRIDE;

  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM(uint32_t aFor,
                                nsIFrame* aTransformRoot = nullptr) MOZ_OVERRIDE;

  nsAutoPtr<gfxMatrix> mCanvasTM;
};

#endif
