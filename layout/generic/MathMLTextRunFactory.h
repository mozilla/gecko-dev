/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MATHMLTEXTRUNFACTORY_H_
#define MATHMLTEXTRUNFACTORY_H_

#include "nsTextRunTransformations.h"

/**
 * Builds textruns that render their text with MathML specific renderings.
 */
class MathMLTextRunFactory : public nsTransformingTextRunFactory {
public:
  MathMLTextRunFactory(nsTransformingTextRunFactory* aInnerTransformingTextRunFactory,
                       uint32_t aFlags, uint8_t aSSTYScriptLevel)
    : mInnerTransformingTextRunFactory(aInnerTransformingTextRunFactory),
      mFlags(aFlags),
      mSSTYScriptLevel(aSSTYScriptLevel) {}

  virtual void RebuildTextRun(nsTransformedTextRun* aTextRun,
                              gfxContext* aRefContext) MOZ_OVERRIDE;
  enum {
    // Style effects which may override single character <mi> behaviour
    MATH_FONT_STYLING_NORMAL   = 0x1, // fontstyle="normal" has been set.
    MATH_FONT_WEIGHT_BOLD      = 0x2, // fontweight="bold" has been set.
  };

protected:
  nsAutoPtr<nsTransformingTextRunFactory> mInnerTransformingTextRunFactory;
  uint32_t mFlags;
  uint8_t mSSTYScriptLevel;
};

#endif /*MATHMLTEXTRUNFACTORY_H_*/
