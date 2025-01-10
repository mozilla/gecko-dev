/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SMIL_SMILDISCARDANIMATIONFUNCTION_H_
#define DOM_SMIL_SMILDISCARDANIMATIONFUNCTION_H_

#include "mozilla/Attributes.h"
#include "mozilla/SMILAnimationFunction.h"

namespace mozilla {

//----------------------------------------------------------------------
// SMILDiscardAnimationFunction
//
// Subclass of SMILAnimationFunction that limits the behaviour to that offered
// by a <discard> element.
//
class SMILDiscardAnimationFunction : public SMILAnimationFunction {
 protected:
  bool IsDisallowedAttribute(const nsAtom* aAttribute) const override {
    //
    // A <discard> element doesn't have any animation parameters
    //
    return aAttribute == nsGkAtoms::calcMode ||
           aAttribute == nsGkAtoms::values ||
           aAttribute == nsGkAtoms::keyTimes ||
           aAttribute == nsGkAtoms::keySplines ||
           aAttribute == nsGkAtoms::from || aAttribute == nsGkAtoms::by ||
           aAttribute == nsGkAtoms::to || aAttribute == nsGkAtoms::additive ||
           aAttribute == nsGkAtoms::accumulate;
  }

  bool IsToAnimation() const override { return false; }
  bool IsValueFixedForSimpleDuration() const override { return true; }
  bool WillReplace() const override { return true; }
};

}  // namespace mozilla

#endif  // DOM_SMIL_SMILDISCARDANIMATIONFUNCTION_H_
