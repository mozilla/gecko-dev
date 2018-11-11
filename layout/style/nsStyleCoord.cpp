/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of length values in computed style data */

#include "nsStyleCoord.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/PodOperations.h"

nsStyleCoord::nsStyleCoord(nsStyleUnit aUnit)
  : mUnit(aUnit)
{
  NS_ASSERTION(aUnit < eStyleUnit_Percent, "not a valueless unit");
  if (aUnit >= eStyleUnit_Percent) {
    mUnit = eStyleUnit_Null;
  }
  mValue.mInt = 0;
}

nsStyleCoord::nsStyleCoord(int32_t aValue, nsStyleUnit aUnit)
  : mUnit(aUnit)
{
  //if you want to pass in eStyleUnit_Coord, don't. instead, use the
  //constructor just above this one... MMP
  NS_ASSERTION((aUnit == eStyleUnit_Enumerated) ||
               (aUnit == eStyleUnit_Integer), "not an int value");
  if ((aUnit == eStyleUnit_Enumerated) ||
      (aUnit == eStyleUnit_Integer)) {
    mValue.mInt = aValue;
  }
  else {
    mUnit = eStyleUnit_Null;
    mValue.mInt = 0;
  }
}

nsStyleCoord::nsStyleCoord(float aValue, nsStyleUnit aUnit)
  : mUnit(aUnit)
{
  if (aUnit < eStyleUnit_Percent || aUnit >= eStyleUnit_Coord) {
    NS_NOTREACHED("not a float value");
    mUnit = eStyleUnit_Null;
    mValue.mInt = 0;
  } else {
    mValue.mFloat = aValue;
  }
}

bool nsStyleCoord::operator==(const nsStyleCoord& aOther) const
{
  if (mUnit != aOther.mUnit) {
    return false;
  }
  switch (mUnit) {
    case eStyleUnit_Null:
    case eStyleUnit_Normal:
    case eStyleUnit_Auto:
    case eStyleUnit_None:
      return true;
    case eStyleUnit_Percent:
    case eStyleUnit_Factor:
    case eStyleUnit_Degree:
    case eStyleUnit_Grad:
    case eStyleUnit_Radian:
    case eStyleUnit_Turn:
    case eStyleUnit_FlexFraction:
      return mValue.mFloat == aOther.mValue.mFloat;
    case eStyleUnit_Coord:
    case eStyleUnit_Integer:
    case eStyleUnit_Enumerated:
      return mValue.mInt == aOther.mValue.mInt;
    case eStyleUnit_Calc:
      return *this->GetCalcValue() == *aOther.GetCalcValue();
  }
  MOZ_ASSERT(false, "unexpected unit");
  return false;
}

uint32_t nsStyleCoord::HashValue(uint32_t aHash = 0) const
{
  aHash = mozilla::AddToHash(aHash, mUnit);

  switch (mUnit) {
    case eStyleUnit_Null:
    case eStyleUnit_Normal:
    case eStyleUnit_Auto:
    case eStyleUnit_None:
      return mozilla::AddToHash(aHash, true);
    case eStyleUnit_Percent:
    case eStyleUnit_Factor:
    case eStyleUnit_Degree:
    case eStyleUnit_Grad:
    case eStyleUnit_Radian:
    case eStyleUnit_Turn:
    case eStyleUnit_FlexFraction:
      return mozilla::AddToHash(aHash, mValue.mFloat);
    case eStyleUnit_Coord:
    case eStyleUnit_Integer:
    case eStyleUnit_Enumerated:
      return mozilla::AddToHash(aHash, mValue.mInt);
    case eStyleUnit_Calc:
      Calc* calcValue = GetCalcValue();
      aHash = mozilla::AddToHash(aHash, calcValue->mLength);
      if (HasPercent()) {
        return mozilla::AddToHash(aHash, calcValue->mPercent);
      }
      return aHash;
  }
  MOZ_ASSERT(false, "unexpected unit");
  return aHash;
}

void nsStyleCoord::Reset()
{
  Reset(mUnit, mValue);
}

void nsStyleCoord::SetCoordValue(nscoord aValue)
{
  Reset();
  mUnit = eStyleUnit_Coord;
  mValue.mInt = aValue;
}

void nsStyleCoord::SetIntValue(int32_t aValue, nsStyleUnit aUnit)
{
  NS_ASSERTION((aUnit == eStyleUnit_Enumerated) ||
               (aUnit == eStyleUnit_Integer), "not an int value");
  Reset();
  if ((aUnit == eStyleUnit_Enumerated) ||
      (aUnit == eStyleUnit_Integer)) {
    mUnit = aUnit;
    mValue.mInt = aValue;
  }
}

void nsStyleCoord::SetPercentValue(float aValue)
{
  Reset();
  mUnit = eStyleUnit_Percent;
  mValue.mFloat = aValue;
}

void nsStyleCoord::SetFactorValue(float aValue)
{
  Reset();
  mUnit = eStyleUnit_Factor;
  mValue.mFloat = aValue;
}

void nsStyleCoord::SetAngleValue(float aValue, nsStyleUnit aUnit)
{
  Reset();
  if (aUnit == eStyleUnit_Degree ||
      aUnit == eStyleUnit_Grad ||
      aUnit == eStyleUnit_Radian ||
      aUnit == eStyleUnit_Turn) {
    mUnit = aUnit;
    mValue.mFloat = aValue;
  } else {
    NS_NOTREACHED("not an angle value");
  }
}

void nsStyleCoord::SetFlexFractionValue(float aValue)
{
  Reset();
  mUnit = eStyleUnit_FlexFraction;
  mValue.mFloat = aValue;
}

void nsStyleCoord::SetCalcValue(Calc* aValue)
{
  Reset();
  mUnit = eStyleUnit_Calc;
  mValue.mPointer = aValue;
  aValue->AddRef();
}

void nsStyleCoord::SetNormalValue()
{
  Reset();
  mUnit = eStyleUnit_Normal;
  mValue.mInt = 0;
}

void nsStyleCoord::SetAutoValue()
{
  Reset();
  mUnit = eStyleUnit_Auto;
  mValue.mInt = 0;
}

void nsStyleCoord::SetNoneValue()
{
  Reset();
  mUnit = eStyleUnit_None;
  mValue.mInt = 0;
}

// accessors that are not inlined

double
nsStyleCoord::GetAngleValueInDegrees() const
{
  return GetAngleValueInRadians() * (180.0 / M_PI);
}

double
nsStyleCoord::GetAngleValueInRadians() const
{
  double angle = mValue.mFloat;

  switch (GetUnit()) {
  case eStyleUnit_Radian: return angle;
  case eStyleUnit_Turn:   return angle * 2 * M_PI;
  case eStyleUnit_Degree: return angle * M_PI / 180.0;
  case eStyleUnit_Grad:   return angle * M_PI / 200.0;

  default:
    NS_NOTREACHED("unrecognized angular unit");
    return 0.0;
  }
}

nsStyleSides::nsStyleSides()
{
  NS_FOR_CSS_SIDES(i) {
    mUnits[i] = eStyleUnit_Null;
  }
  mozilla::PodArrayZero(mValues);
}

nsStyleSides::nsStyleSides(const nsStyleSides& aOther)
{
  NS_FOR_CSS_SIDES(i) {
    mUnits[i] = eStyleUnit_Null;
  }
  *this = aOther;
}

nsStyleSides::~nsStyleSides()
{
  Reset();
}

nsStyleSides&
nsStyleSides::operator=(const nsStyleSides& aCopy)
{
  if (this != &aCopy) {
    NS_FOR_CSS_SIDES(i) {
      nsStyleCoord::SetValue(mUnits[i], mValues[i],
                             aCopy.mUnits[i], aCopy.mValues[i]);
    }
  }
  return *this;
}

bool nsStyleSides::operator==(const nsStyleSides& aOther) const
{
  NS_FOR_CSS_SIDES(i) {
    if (nsStyleCoord(mValues[i], (nsStyleUnit)mUnits[i]) !=
        nsStyleCoord(aOther.mValues[i], (nsStyleUnit)aOther.mUnits[i])) {
      return false;
    }
  }
  return true;
}

void nsStyleSides::Reset()
{
  NS_FOR_CSS_SIDES(i) {
    nsStyleCoord::Reset(mUnits[i], mValues[i]);
  }
}

nsStyleCorners::nsStyleCorners()
{
  NS_FOR_CSS_HALF_CORNERS(i) {
    mUnits[i] = eStyleUnit_Null;
  }
  mozilla::PodArrayZero(mValues);
}

nsStyleCorners::nsStyleCorners(const nsStyleCorners& aOther)
{
  NS_FOR_CSS_HALF_CORNERS(i) {
    mUnits[i] = eStyleUnit_Null;
  }
  *this = aOther;
}

nsStyleCorners::~nsStyleCorners()
{
  Reset();
}

nsStyleCorners&
nsStyleCorners::operator=(const nsStyleCorners& aCopy)
{
  if (this != &aCopy) {
    NS_FOR_CSS_HALF_CORNERS(i) {
      nsStyleCoord::SetValue(mUnits[i], mValues[i],
                             aCopy.mUnits[i], aCopy.mValues[i]);
    }
  }
  return *this;
}

bool
nsStyleCorners::operator==(const nsStyleCorners& aOther) const
{
  NS_FOR_CSS_HALF_CORNERS(i) {
    if (nsStyleCoord(mValues[i], (nsStyleUnit)mUnits[i]) !=
        nsStyleCoord(aOther.mValues[i], (nsStyleUnit)aOther.mUnits[i])) {
      return false;
    }
  }
  return true;
}

void nsStyleCorners::Reset()
{
  NS_FOR_CSS_HALF_CORNERS(i) {
    nsStyleCoord::Reset(mUnits[i], mValues[i]);
  }
}

// Validation of NS_SIDE_IS_VERTICAL and NS_HALF_CORNER_IS_X.
#define CASE(side, result)                                                    \
  static_assert(NS_SIDE_IS_VERTICAL(side) == result,                      \
                "NS_SIDE_IS_VERTICAL is wrong")
CASE(NS_SIDE_TOP,    false);
CASE(NS_SIDE_RIGHT,  true);
CASE(NS_SIDE_BOTTOM, false);
CASE(NS_SIDE_LEFT,   true);
#undef CASE

#define CASE(corner, result)                                                  \
  static_assert(NS_HALF_CORNER_IS_X(corner) == result,                    \
                "NS_HALF_CORNER_IS_X is wrong")
CASE(NS_CORNER_TOP_LEFT_X,     true);
CASE(NS_CORNER_TOP_LEFT_Y,     false);
CASE(NS_CORNER_TOP_RIGHT_X,    true);
CASE(NS_CORNER_TOP_RIGHT_Y,    false);
CASE(NS_CORNER_BOTTOM_RIGHT_X, true);
CASE(NS_CORNER_BOTTOM_RIGHT_Y, false);
CASE(NS_CORNER_BOTTOM_LEFT_X,  true);
CASE(NS_CORNER_BOTTOM_LEFT_Y,  false);
#undef CASE

// Validation of NS_HALF_TO_FULL_CORNER.
#define CASE(corner, result)                                                  \
  static_assert(NS_HALF_TO_FULL_CORNER(corner) == result,                 \
                "NS_HALF_TO_FULL_CORNER is wrong")
CASE(NS_CORNER_TOP_LEFT_X,     NS_CORNER_TOP_LEFT);
CASE(NS_CORNER_TOP_LEFT_Y,     NS_CORNER_TOP_LEFT);
CASE(NS_CORNER_TOP_RIGHT_X,    NS_CORNER_TOP_RIGHT);
CASE(NS_CORNER_TOP_RIGHT_Y,    NS_CORNER_TOP_RIGHT);
CASE(NS_CORNER_BOTTOM_RIGHT_X, NS_CORNER_BOTTOM_RIGHT);
CASE(NS_CORNER_BOTTOM_RIGHT_Y, NS_CORNER_BOTTOM_RIGHT);
CASE(NS_CORNER_BOTTOM_LEFT_X,  NS_CORNER_BOTTOM_LEFT);
CASE(NS_CORNER_BOTTOM_LEFT_Y,  NS_CORNER_BOTTOM_LEFT);
#undef CASE

// Validation of NS_FULL_TO_HALF_CORNER.
#define CASE(corner, vert, result)                                            \
  static_assert(NS_FULL_TO_HALF_CORNER(corner, vert) == result,           \
                "NS_FULL_TO_HALF_CORNER is wrong")
CASE(NS_CORNER_TOP_LEFT,     false, NS_CORNER_TOP_LEFT_X);
CASE(NS_CORNER_TOP_LEFT,     true,  NS_CORNER_TOP_LEFT_Y);
CASE(NS_CORNER_TOP_RIGHT,    false, NS_CORNER_TOP_RIGHT_X);
CASE(NS_CORNER_TOP_RIGHT,    true,  NS_CORNER_TOP_RIGHT_Y);
CASE(NS_CORNER_BOTTOM_RIGHT, false, NS_CORNER_BOTTOM_RIGHT_X);
CASE(NS_CORNER_BOTTOM_RIGHT, true,  NS_CORNER_BOTTOM_RIGHT_Y);
CASE(NS_CORNER_BOTTOM_LEFT,  false, NS_CORNER_BOTTOM_LEFT_X);
CASE(NS_CORNER_BOTTOM_LEFT,  true,  NS_CORNER_BOTTOM_LEFT_Y);
#undef CASE

// Validation of NS_SIDE_TO_{FULL,HALF}_CORNER.
#define CASE(side, second, result)                                            \
  static_assert(NS_SIDE_TO_FULL_CORNER(side, second) == result,           \
                "NS_SIDE_TO_FULL_CORNER is wrong")
CASE(NS_SIDE_TOP,    false, NS_CORNER_TOP_LEFT);
CASE(NS_SIDE_TOP,    true,  NS_CORNER_TOP_RIGHT);

CASE(NS_SIDE_RIGHT,  false, NS_CORNER_TOP_RIGHT);
CASE(NS_SIDE_RIGHT,  true,  NS_CORNER_BOTTOM_RIGHT);

CASE(NS_SIDE_BOTTOM, false, NS_CORNER_BOTTOM_RIGHT);
CASE(NS_SIDE_BOTTOM, true,  NS_CORNER_BOTTOM_LEFT);

CASE(NS_SIDE_LEFT,   false, NS_CORNER_BOTTOM_LEFT);
CASE(NS_SIDE_LEFT,   true,  NS_CORNER_TOP_LEFT);
#undef CASE

#define CASE(side, second, parallel, result)                                  \
  static_assert(NS_SIDE_TO_HALF_CORNER(side, second, parallel) == result, \
                "NS_SIDE_TO_HALF_CORNER is wrong")
CASE(NS_SIDE_TOP,    false, true,  NS_CORNER_TOP_LEFT_X);
CASE(NS_SIDE_TOP,    false, false, NS_CORNER_TOP_LEFT_Y);
CASE(NS_SIDE_TOP,    true,  true,  NS_CORNER_TOP_RIGHT_X);
CASE(NS_SIDE_TOP,    true,  false, NS_CORNER_TOP_RIGHT_Y);

CASE(NS_SIDE_RIGHT,  false, false, NS_CORNER_TOP_RIGHT_X);
CASE(NS_SIDE_RIGHT,  false, true,  NS_CORNER_TOP_RIGHT_Y);
CASE(NS_SIDE_RIGHT,  true,  false, NS_CORNER_BOTTOM_RIGHT_X);
CASE(NS_SIDE_RIGHT,  true,  true,  NS_CORNER_BOTTOM_RIGHT_Y);

CASE(NS_SIDE_BOTTOM, false, true,  NS_CORNER_BOTTOM_RIGHT_X);
CASE(NS_SIDE_BOTTOM, false, false, NS_CORNER_BOTTOM_RIGHT_Y);
CASE(NS_SIDE_BOTTOM, true,  true,  NS_CORNER_BOTTOM_LEFT_X);
CASE(NS_SIDE_BOTTOM, true,  false, NS_CORNER_BOTTOM_LEFT_Y);

CASE(NS_SIDE_LEFT,   false, false, NS_CORNER_BOTTOM_LEFT_X);
CASE(NS_SIDE_LEFT,   false, true,  NS_CORNER_BOTTOM_LEFT_Y);
CASE(NS_SIDE_LEFT,   true,  false, NS_CORNER_TOP_LEFT_X);
CASE(NS_SIDE_LEFT,   true,  true,  NS_CORNER_TOP_LEFT_Y);
#undef CASE
