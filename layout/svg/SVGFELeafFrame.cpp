/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Keep in (case-insensitive) order:
#include "nsContainerFrame.h"
#include "nsFrame.h"
#include "nsGkAtoms.h"
#include "nsSVGEffects.h"
#include "nsSVGFilters.h"

typedef nsFrame SVGFELeafFrameBase;

/*
 * This frame is used by filter primitive elements that don't
 * have special child elements that provide parameters.
 */
class SVGFELeafFrame : public SVGFELeafFrameBase
{
  friend nsIFrame*
  NS_NewSVGFELeafFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  explicit SVGFELeafFrame(nsStyleContext* aContext)
    : SVGFELeafFrameBase(aContext)
  {
    AddStateBits(NS_FRAME_SVG_LAYOUT | NS_FRAME_IS_NONDISPLAY);
  }

public:
  NS_DECL_FRAMEARENA_HELPERS

#ifdef DEBUG
  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;
#endif

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return SVGFELeafFrameBase::IsFrameOfType(aFlags & ~(nsIFrame::eSVG));
  }

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGFELeaf"), aResult);
  }
#endif

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgFELeafFrame
   */
  virtual nsIAtom* GetType() const override;

  virtual nsresult AttributeChanged(int32_t  aNameSpaceID,
                                    nsIAtom* aAttribute,
                                    int32_t  aModType) override;

  virtual bool UpdateOverflow() override {
    // We don't maintain a visual overflow rect
    return false;
  }
};

nsIFrame*
NS_NewSVGFELeafFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) SVGFELeafFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(SVGFELeafFrame)

#ifdef DEBUG
void
SVGFELeafFrame::Init(nsIContent*       aContent,
                     nsContainerFrame* aParent,
                     nsIFrame*         aPrevInFlow)
{
  NS_ASSERTION(aContent->IsNodeOfType(nsINode::eFILTER),
               "Trying to construct an SVGFELeafFrame for a "
               "content element that doesn't support the right interfaces");

  SVGFELeafFrameBase::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

nsIAtom *
SVGFELeafFrame::GetType() const
{
  return nsGkAtoms::svgFELeafFrame;
}

nsresult
SVGFELeafFrame::AttributeChanged(int32_t  aNameSpaceID,
                                 nsIAtom* aAttribute,
                                 int32_t  aModType)
{
  nsSVGFE *element = static_cast<nsSVGFE*>(mContent);
  if (element->AttributeAffectsRendering(aNameSpaceID, aAttribute)) {
    MOZ_ASSERT(GetParent()->GetType() == nsGkAtoms::svgFilterFrame,
               "Observers observe the filter, so that's what we must invalidate");
    nsSVGEffects::InvalidateDirectRenderingObservers(GetParent());
  }

  return SVGFELeafFrameBase::AttributeChanged(aNameSpaceID,
                                              aAttribute, aModType);
}
