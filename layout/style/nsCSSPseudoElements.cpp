/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* atom list for CSS pseudo-elements */

#include "nsCSSPseudoElements.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/ServoBindings.h"

#include "nsCSSAnonBoxes.h"
#include "nsDOMString.h"
#include "nsGkAtomConsts.h"
#include "nsStaticAtomUtils.h"
#include "nsStringFwd.h"

using namespace mozilla;

// Flags data for each of the pseudo-elements.
/* static */ const uint32_t nsCSSPseudoElements::kPseudoElementFlags[] = {
#define CSS_PSEUDO_ELEMENT(name_, value_, flags_) flags_,
#include "nsCSSPseudoElementList.h"
#undef CSS_PSEUDO_ELEMENT
};

/* static */
nsStaticAtom* nsCSSPseudoElements::GetAtomBase() {
  return const_cast<nsStaticAtom*>(
      nsGkAtoms::GetAtomByIndex(kAtomIndex_PseudoElements));
}

/* static */
nsAtom* nsCSSPseudoElements::GetPseudoAtom(Type aType) {
  MOZ_ASSERT(PseudoStyle::IsPseudoElement(aType), "Unexpected type");
  size_t index = kAtomIndex_PseudoElements + static_cast<size_t>(aType);
  return nsGkAtoms::GetAtomByIndex(index);
}

/* static */
Maybe<PseudoStyleRequest> nsCSSPseudoElements::ParsePseudoElement(
    const nsAString& aPseudoElement, CSSEnabledState aEnabledState) {
  PseudoStyleRequest result;

  // Not a pseudo-element, use default PseudoStyleReqeust.
  if (DOMStringIsNull(aPseudoElement) || aPseudoElement.IsEmpty()) {
    return Some(result);
  }

  // Parse the pseudo-element string.
  if (!Servo_ParsePseudoElement(&aPseudoElement, &result)) {
    return Nothing();
  }

  // Servo_ParsePseudoElement() doesn't do enabled check, so it may return all
  // possible PseudoElements, including tree pseudo-elements and anoymous boxes,
  // so we have to filter out the pseudo-elements not defined in
  // "nsCSSPseudoElementList.h" as well.
  // Also, IsEnabled() checks if this pseudo-element should be enabled here
  // (e.g. preference check and ua stylesheet check).
  if (!PseudoStyle::IsPseudoElement(result.mType) ||
      !IsEnabled(result.mType, aEnabledState)) {
    return Nothing();
  }

  // The universal selector is pre-defined and should not be a valid name for
  // a named view-transition pseudo element (i.e. we accept it only in CSS
  // selectors).
  if (PseudoStyle::IsNamedViewTransitionPseudoElement(result.mType) &&
      result.mIdentifier == nsGkAtoms::_asterisk) {
    return Nothing();
  }

  return Some(result);
}

/* static */
bool nsCSSPseudoElements::PseudoElementSupportsUserActionState(
    const Type aType) {
  return PseudoElementHasFlags(aType,
                               CSS_PSEUDO_ELEMENT_SUPPORTS_USER_ACTION_STATE);
}

/* static */
nsString nsCSSPseudoElements::PseudoRequestAsString(
    const Request& aPseudoRequest) {
  switch (aPseudoRequest.mType) {
    case PseudoStyleType::before:
      return u"::before"_ns;
    case PseudoStyleType::after:
      return u"::after"_ns;
    case PseudoStyleType::marker:
      return u"::marker"_ns;
    case PseudoStyleType::viewTransition:
      return u"::view-transition"_ns;
    case PseudoStyleType::viewTransitionGroup:
      return u"::view-transition-group("_ns +
             nsAtomString(aPseudoRequest.mIdentifier) + u")"_ns;
    case PseudoStyleType::viewTransitionImagePair:
      return u"::view-transition-image-pair("_ns +
             nsAtomString(aPseudoRequest.mIdentifier) + u")"_ns;
    case PseudoStyleType::viewTransitionOld:
      return u"::view-transition-old("_ns +
             nsAtomString(aPseudoRequest.mIdentifier) + u")"_ns;
    case PseudoStyleType::viewTransitionNew:
      return u"::view-transition-new("_ns +
             nsAtomString(aPseudoRequest.mIdentifier) + u")"_ns;
    default:
      MOZ_ASSERT(aPseudoRequest.IsNotPseudo(), "Unexpected pseudo type");
      return u""_ns;
  }
}

#ifdef DEBUG
/* static */
void nsCSSPseudoElements::AssertAtoms() {
  nsStaticAtom* base = GetAtomBase();
#  define CSS_PSEUDO_ELEMENT(name_, value_, flags_)                   \
    {                                                                 \
      RefPtr<nsAtom> atom = NS_Atomize(value_);                       \
      size_t index = static_cast<size_t>(PseudoStyleType::name_);     \
      MOZ_ASSERT(atom == nsGkAtoms::PseudoElement_##name_,            \
                 "Static atom for " #name_ " has incorrect value");   \
      MOZ_ASSERT(atom == &base[index],                                \
                 "Static atom for " #name_ " not at expected index"); \
    }
#  include "nsCSSPseudoElementList.h"
#  undef CSS_PSEUDO_ELEMENT
}
#endif
