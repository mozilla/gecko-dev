/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGMASKFRAME_H__
#define __NS_SVGMASKFRAME_H__

#include "mozilla/Attributes.h"
#include "gfxPattern.h"
#include "gfxMatrix.h"
#include "nsSVGContainerFrame.h"
#include "nsSVGUtils.h"

class gfxContext;
class nsRenderingContext;

typedef nsSVGContainerFrame nsSVGMaskFrameBase;

class nsSVGMaskFrame : public nsSVGMaskFrameBase
{
  friend nsIFrame*
  NS_NewSVGMaskFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  nsSVGMaskFrame(nsStyleContext* aContext)
    : nsSVGMaskFrameBase(aContext)
    , mInUse(false)
  {
    AddStateBits(NS_FRAME_IS_NONDISPLAY);
  }

public:
  NS_DECL_FRAMEARENA_HELPERS

  // nsSVGMaskFrame method:
  already_AddRefed<gfxPattern> ComputeMaskAlpha(nsRenderingContext *aContext,
                                                nsIFrame* aParent,
                                                const gfxMatrix &aMatrix,
                                                float aOpacity = 1.0f);

  virtual nsresult AttributeChanged(int32_t         aNameSpaceID,
                                    nsIAtom*        aAttribute,
                                    int32_t         aModType) MOZ_OVERRIDE;

#ifdef DEBUG
  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) MOZ_OVERRIDE;
#endif

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE {}

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgMaskFrame
   */
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const MOZ_OVERRIDE
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGMask"), aResult);
  }
#endif

private:
  // A helper class to allow us to paint masks safely. The helper
  // automatically sets and clears the mInUse flag on the mask frame
  // (to prevent nasty reference loops). It's easy to mess this up
  // and break things, so this helper makes the code far more robust.
  class MOZ_STACK_CLASS AutoMaskReferencer
  {
  public:
    AutoMaskReferencer(nsSVGMaskFrame *aFrame
                       MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
       : mFrame(aFrame) {
      MOZ_GUARD_OBJECT_NOTIFIER_INIT;
      NS_ASSERTION(!mFrame->mInUse, "reference loop!");
      mFrame->mInUse = true;
    }
    ~AutoMaskReferencer() {
      mFrame->mInUse = false;
    }
  private:
    nsSVGMaskFrame *mFrame;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  };

  nsIFrame *mMaskParent;
  nsAutoPtr<gfxMatrix> mMaskParentMatrix;
  // recursion prevention flag
  bool mInUse;

  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM(uint32_t aFor,
                                nsIFrame* aTransformRoot = nullptr) MOZ_OVERRIDE;
};

#endif
