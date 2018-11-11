/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file contains code to help implement the Conditional Processing
 * section of the SVG specification (i.e. the <switch> element and the
 * requiredFeatures, requiredExtensions and systemLanguage attributes).
 *
 *   http://www.w3.org/TR/SVG11/struct.html#ConditionalProcessing
 */

#include "nsSVGFeatures.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsNameSpaceManager.h"
#include "mozilla/Preferences.h"

using namespace mozilla;

/*static*/ bool
nsSVGFeatures::HasExtension(const nsAString& aExtension, const bool aIsInChrome)
{
#define SVG_SUPPORTED_EXTENSION(str) if (aExtension.EqualsLiteral(str)) return true;
  SVG_SUPPORTED_EXTENSION("http://www.w3.org/1999/xhtml")
  nsNameSpaceManager* nameSpaceManager = nsNameSpaceManager::GetInstance();
  if (aIsInChrome || !nameSpaceManager->mMathMLDisabled) {
    SVG_SUPPORTED_EXTENSION("http://www.w3.org/1998/Math/MathML")
  }
#undef SVG_SUPPORTED_EXTENSION

  return false;
}
