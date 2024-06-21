/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGPathSegListSMILType.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/SMILValue.h"
#include "SVGPathData.h"
#include "SVGPathSegUtils.h"

namespace mozilla {

//----------------------------------------------------------------------
// nsISMILType implementation

void SVGPathSegListSMILType::Init(SMILValue& aValue) const {
  MOZ_ASSERT(aValue.IsNull(), "Unexpected value type");
  aValue.mU.mPtr = new SVGPathDataAndInfo();
  aValue.mType = this;
}

void SVGPathSegListSMILType::Destroy(SMILValue& aValue) const {
  MOZ_ASSERT(aValue.mType == this, "Unexpected SMIL value type");
  delete static_cast<SVGPathDataAndInfo*>(aValue.mU.mPtr);
  aValue.mU.mPtr = nullptr;
  aValue.mType = SMILNullType::Singleton();
}

nsresult SVGPathSegListSMILType::Assign(SMILValue& aDest,
                                        const SMILValue& aSrc) const {
  MOZ_ASSERT(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aDest.mType == this, "Unexpected SMIL value");

  const SVGPathDataAndInfo* src =
      static_cast<const SVGPathDataAndInfo*>(aSrc.mU.mPtr);
  SVGPathDataAndInfo* dest = static_cast<SVGPathDataAndInfo*>(aDest.mU.mPtr);
  dest->CopyFrom(*src);
  return NS_OK;
}

bool SVGPathSegListSMILType::IsEqual(const SMILValue& aLeft,
                                     const SMILValue& aRight) const {
  MOZ_ASSERT(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aLeft.mType == this, "Unexpected type for SMIL value");

  return *static_cast<const SVGPathDataAndInfo*>(aLeft.mU.mPtr) ==
         *static_cast<const SVGPathDataAndInfo*>(aRight.mU.mPtr);
}

nsresult SVGPathSegListSMILType::Add(SMILValue& aDest,
                                     const SMILValue& aValueToAdd,
                                     uint32_t aCount) const {
  MOZ_ASSERT(aDest.mType == this, "Unexpected SMIL type");
  MOZ_ASSERT(aValueToAdd.mType == this, "Incompatible SMIL type");

  SVGPathDataAndInfo& dest = *static_cast<SVGPathDataAndInfo*>(aDest.mU.mPtr);
  const SVGPathDataAndInfo& valueToAdd =
      *static_cast<const SVGPathDataAndInfo*>(aValueToAdd.mU.mPtr);

  if (valueToAdd.IsIdentity()) {  // Adding identity value - no-op
    return NS_OK;
  }

  if (dest.IsIdentity()) {
    dest.CopyFrom(valueToAdd);
    aCount--;
  }

  if (aCount &&
      !Servo_SVGPathData_Add(&dest.RawData(), &valueToAdd.RawData(), aCount)) {
    // SVGContentUtils::ReportToConsole - can't add path segment lists with
    // different numbers of segments, with arcs that have different flag
    // values, or with incompatible segment types.
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult SVGPathSegListSMILType::ComputeDistance(const SMILValue& aFrom,
                                                 const SMILValue& aTo,
                                                 double& aDistance) const {
  MOZ_ASSERT(aFrom.mType == this, "Unexpected SMIL type");
  MOZ_ASSERT(aTo.mType == this, "Incompatible SMIL type");

  // See https://bugzilla.mozilla.org/show_bug.cgi?id=522306#c18

  // SVGContentUtils::ReportToConsole
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult SVGPathSegListSMILType::Interpolate(const SMILValue& aStartVal,
                                             const SMILValue& aEndVal,
                                             double aUnitDistance,
                                             SMILValue& aResult) const {
  MOZ_ASSERT(aStartVal.mType == aEndVal.mType,
             "Trying to interpolate different types");
  MOZ_ASSERT(aStartVal.mType == this, "Unexpected types for interpolation");
  MOZ_ASSERT(aResult.mType == this, "Unexpected result type");

  const SVGPathDataAndInfo& start =
      *static_cast<const SVGPathDataAndInfo*>(aStartVal.mU.mPtr);
  const SVGPathDataAndInfo& end =
      *static_cast<const SVGPathDataAndInfo*>(aEndVal.mU.mPtr);
  SVGPathDataAndInfo& result =
      *static_cast<SVGPathDataAndInfo*>(aResult.mU.mPtr);
  MOZ_ASSERT(result.IsIdentity(),
             "expecting outparam to start out as identity");
  result.SetElement(end.Element());
  if (!Servo_SVGPathData_Interpolate(
          start.IsIdentity() ? nullptr : &start.RawData(), &end.RawData(),
          aUnitDistance, &result.RawData())) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

}  // namespace mozilla
