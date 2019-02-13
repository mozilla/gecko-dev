/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGINNERSVGFRAME_H__
#define __NS_SVGINNERSVGFRAME_H__

#include "mozilla/Attributes.h"
#include "nsSVGContainerFrame.h"
#include "nsISVGSVGFrame.h"

class gfxContext;

typedef nsSVGDisplayContainerFrame nsSVGInnerSVGFrameBase;

class nsSVGInnerSVGFrame : public nsSVGInnerSVGFrameBase,
                           public nsISVGSVGFrame
{
  friend nsIFrame*
  NS_NewSVGInnerSVGFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  explicit nsSVGInnerSVGFrame(nsStyleContext* aContext) :
    nsSVGInnerSVGFrameBase(aContext) {}
  
public:
  NS_DECL_QUERYFRAME_TARGET(nsSVGInnerSVGFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

#ifdef DEBUG
  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;
#endif

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgInnerSVGFrame
   */
  virtual nsIAtom* GetType() const override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGInnerSVG"), aResult);
  }
#endif

  virtual nsresult  AttributeChanged(int32_t         aNameSpaceID,
                                     nsIAtom*        aAttribute,
                                     int32_t         aModType) override;

  // nsISVGChildFrame interface:
  virtual nsresult PaintSVG(gfxContext& aContext,
                            const gfxMatrix& aTransform,
                            const nsIntRect *aDirtyRect = nullptr) override;
  virtual nsRect GetCoveredRegion() override;
  virtual void ReflowSVG() override;
  virtual void NotifySVGChanged(uint32_t aFlags) override;
  virtual nsIFrame* GetFrameForPoint(const gfxPoint& aPoint) override;

  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM() override;

  virtual bool HasChildrenOnlyTransform(Matrix *aTransform) const override;

  // nsISVGSVGFrame interface:
  virtual void NotifyViewportOrTransformChanged(uint32_t aFlags) override;

protected:

  nsAutoPtr<gfxMatrix> mCanvasTM;
};

#endif

