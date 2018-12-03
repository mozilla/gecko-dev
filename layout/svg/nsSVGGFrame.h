/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSSVGGFRAME_H
#define NSSVGGFRAME_H

#include "mozilla/Attributes.h"
#include "gfxMatrix.h"
#include "nsAutoPtr.h"
#include "nsSVGContainerFrame.h"

class nsSVGGFrame : public nsSVGDisplayContainerFrame {
  friend nsIFrame* NS_NewSVGGFrame(nsIPresShell* aPresShell,
                                   ComputedStyle* aStyle);
  explicit nsSVGGFrame(ComputedStyle* aStyle) : nsSVGGFrame(aStyle, kClassID) {}

 protected:
  nsSVGGFrame(ComputedStyle* aStyle, nsIFrame::ClassID aID)
      : nsSVGDisplayContainerFrame(aStyle, aID) {}

 public:
  NS_DECL_FRAMEARENA_HELPERS(nsSVGGFrame)

#ifdef DEBUG
  virtual void Init(nsIContent* aContent, nsContainerFrame* aParent,
                    nsIFrame* aPrevInFlow) override;
#endif

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(NS_LITERAL_STRING("SVGG"), aResult);
  }
#endif

  // nsIFrame interface:
  virtual nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                    int32_t aModType) override;
};

#endif
