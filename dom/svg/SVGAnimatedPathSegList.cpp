/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGAnimatedPathSegList.h"

#include <utility>

#include "SVGPathSegListSMILType.h"
#include "mozilla/SMILValue.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/dom/SVGElement.h"

using namespace mozilla::dom;

// See the comments in this file's header!

namespace mozilla {

nsresult SVGAnimatedPathSegList::SetBaseValueString(const nsAString& aValue) {
  // We don't need to call DidChange* here - we're only called by
  // SVGElement::ParseAttribute under Element::SetAttr,
  // which takes care of notifying.
  return mBaseVal.SetValueFromString(NS_ConvertUTF16toUTF8(aValue));
}

void SVGAnimatedPathSegList::ClearBaseValue() {
  mBaseVal.Clear();
  // Caller notifies
}

nsresult SVGAnimatedPathSegList::SetAnimValue(const SVGPathData& aNewAnimValue,
                                              SVGElement* aElement) {
  // Note that a new animation may totally change the number of items in the
  // animVal list, either replacing what was essentially a mirror of the
  // baseVal list, or else replacing and overriding an existing animation.
  // Unfortunately it is not possible for us to reliably distinguish between
  // calls to this method that are setting a new sample for an existing
  // animation, and calls that are setting the first sample of an animation
  // that will override an existing animation.

  if (!mAnimVal) {
    mAnimVal = MakeUnique<SVGPathData>();
  }
  *mAnimVal = aNewAnimValue;
  aElement->DidAnimatePathSegList();
  return NS_OK;
}

void SVGAnimatedPathSegList::ClearAnimValue(SVGElement* aElement) {
  mAnimVal = nullptr;
  aElement->DidAnimatePathSegList();
}

bool SVGAnimatedPathSegList::IsRendered() const {
  return mAnimVal ? !mAnimVal->IsEmpty() : !mBaseVal.IsEmpty();
}

UniquePtr<SMILAttr> SVGAnimatedPathSegList::ToSMILAttr(SVGElement* aElement) {
  return MakeUnique<SMILAnimatedPathSegList>(this, aElement);
}

nsresult SVGAnimatedPathSegList::SMILAnimatedPathSegList::ValueFromString(
    const nsAString& aStr, const dom::SVGAnimationElement* /*aSrcElement*/,
    SMILValue& aValue, bool& aPreventCachingOfSandwich) const {
  SMILValue val(SVGPathSegListSMILType::Singleton());
  SVGPathDataAndInfo* list = static_cast<SVGPathDataAndInfo*>(val.mU.mPtr);
  nsresult rv = list->SetValueFromString(NS_ConvertUTF16toUTF8(aStr));
  if (NS_SUCCEEDED(rv)) {
    list->SetElement(mElement);
    aValue = std::move(val);
  }
  return rv;
}

SMILValue SVGAnimatedPathSegList::SMILAnimatedPathSegList::GetBaseValue()
    const {
  // To benefit from Return Value Optimization and avoid copy constructor calls
  // due to our use of return-by-value, we must return the exact same object
  // from ALL return points. This function must only return THIS variable:
  SMILValue tmp(SVGPathSegListSMILType::Singleton());
  auto* list = static_cast<SVGPathDataAndInfo*>(tmp.mU.mPtr);
  list->CopyFrom(mVal->mBaseVal);
  list->SetElement(mElement);
  return tmp;
}

nsresult SVGAnimatedPathSegList::SMILAnimatedPathSegList::SetAnimValue(
    const SMILValue& aValue) {
  NS_ASSERTION(aValue.mType == SVGPathSegListSMILType::Singleton(),
               "Unexpected type to assign animated value");
  if (aValue.mType == SVGPathSegListSMILType::Singleton()) {
    mVal->SetAnimValue(*static_cast<SVGPathDataAndInfo*>(aValue.mU.mPtr),
                       mElement);
  }
  return NS_OK;
}

void SVGAnimatedPathSegList::SMILAnimatedPathSegList::ClearAnimValue() {
  if (mVal->mAnimVal) {
    mVal->ClearAnimValue(mElement);
  }
}

size_t SVGAnimatedPathSegList::SizeOfExcludingThis(
    MallocSizeOf aMallocSizeOf) const {
  size_t total = mBaseVal.SizeOfExcludingThis(aMallocSizeOf);
  if (mAnimVal) {
    mAnimVal->SizeOfIncludingThis(aMallocSizeOf);
  }
  return total;
}

}  // namespace mozilla
