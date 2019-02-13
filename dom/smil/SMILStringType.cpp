/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SMILStringType.h"
#include "nsSMILValue.h"
#include "nsDebug.h"
#include "nsString.h"

namespace mozilla {

void
SMILStringType::Init(nsSMILValue& aValue) const
{
  NS_PRECONDITION(aValue.IsNull(), "Unexpected value type");
  aValue.mU.mPtr = new nsString();
  aValue.mType = this;
}

void
SMILStringType::Destroy(nsSMILValue& aValue) const
{
  NS_PRECONDITION(aValue.mType == this, "Unexpected SMIL value");
  delete static_cast<nsAString*>(aValue.mU.mPtr);
  aValue.mU.mPtr = nullptr;
  aValue.mType = nsSMILNullType::Singleton();
}

nsresult
SMILStringType::Assign(nsSMILValue& aDest, const nsSMILValue& aSrc) const
{
  NS_PRECONDITION(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aDest.mType == this, "Unexpected SMIL value");

  const nsAString* src = static_cast<const nsAString*>(aSrc.mU.mPtr);
  nsAString* dst = static_cast<nsAString*>(aDest.mU.mPtr);
  *dst = *src;
  return NS_OK;
}

bool
SMILStringType::IsEqual(const nsSMILValue& aLeft,
                        const nsSMILValue& aRight) const
{
  NS_PRECONDITION(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aLeft.mType == this, "Unexpected type for SMIL value");

  const nsAString* leftString =
    static_cast<const nsAString*>(aLeft.mU.mPtr);
  const nsAString* rightString =
    static_cast<nsAString*>(aRight.mU.mPtr);
  return *leftString == *rightString;
}

nsresult
SMILStringType::Add(nsSMILValue& aDest, const nsSMILValue& aValueToAdd,
                    uint32_t aCount) const
{
  NS_PRECONDITION(aValueToAdd.mType == aDest.mType,
                  "Trying to add invalid types");
  NS_PRECONDITION(aValueToAdd.mType == this, "Unexpected source type");
  return NS_ERROR_FAILURE; // string values can't be added to each other
}

nsresult
SMILStringType::ComputeDistance(const nsSMILValue& aFrom,
                                const nsSMILValue& aTo,
                                double& aDistance) const
{
  NS_PRECONDITION(aFrom.mType == aTo.mType,"Trying to compare different types");
  NS_PRECONDITION(aFrom.mType == this, "Unexpected source type");
  return NS_ERROR_FAILURE; // there is no concept of distance between string values
}

nsresult
SMILStringType::Interpolate(const nsSMILValue& aStartVal,
                            const nsSMILValue& aEndVal,
                            double aUnitDistance,
                            nsSMILValue& aResult) const
{
  NS_PRECONDITION(aStartVal.mType == aEndVal.mType,
      "Trying to interpolate different types");
  NS_PRECONDITION(aStartVal.mType == this,
      "Unexpected types for interpolation");
  NS_PRECONDITION(aResult.mType   == this, "Unexpected result type");
  return NS_ERROR_FAILURE; // string values do not interpolate
}

} // namespace mozilla
