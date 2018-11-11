/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SMILVALUE_H_
#define NS_SMILVALUE_H_

#include "nsISMILType.h"
#include "nsSMILNullType.h"

/**
 * Although objects of this type are generally only created on the stack and
 * only exist during the taking of a new time sample, that's not always the
 * case. The nsSMILValue objects obtained from attributes' base values are
 * cached so that the SMIL engine can make certain optimizations during a
 * sample if the base value has not changed since the last sample (potentially
 * avoiding recomposing). These nsSMILValue objects typically live much longer
 * than a single sample.
 */
class nsSMILValue
{
public:
  nsSMILValue() : mU(), mType(nsSMILNullType::Singleton()) { }
  explicit nsSMILValue(const nsISMILType* aType);
  nsSMILValue(const nsSMILValue& aVal);

  ~nsSMILValue()
  {
    mType->Destroy(*this);
  }

  const nsSMILValue& operator=(const nsSMILValue& aVal);

  // Move constructor / reassignment operator:
  nsSMILValue(nsSMILValue&& aVal);
  nsSMILValue& operator=(nsSMILValue&& aVal);

  // Equality operators. These are allowed to be conservative (return false
  // more than you'd expect) - see comment above nsISMILType::IsEqual.
  bool operator==(const nsSMILValue& aVal) const;
  bool operator!=(const nsSMILValue& aVal) const {
    return !(*this == aVal);
  }

  bool IsNull() const
  {
    return (mType == nsSMILNullType::Singleton());
  }

  nsresult Add(const nsSMILValue& aValueToAdd, uint32_t aCount = 1);
  nsresult SandwichAdd(const nsSMILValue& aValueToAdd);
  nsresult ComputeDistance(const nsSMILValue& aTo, double& aDistance) const;
  nsresult Interpolate(const nsSMILValue& aEndVal,
                       double aUnitDistance,
                       nsSMILValue& aResult) const;

  union {
    bool mBool;
    uint64_t mUint;
    int64_t mInt;
    double mDouble;
    struct {
      float mAngle;
      uint16_t mUnit;
      uint16_t mOrientType;
    } mOrient;
    int32_t mIntPair[2];
    float mNumberPair[2];
    void* mPtr;
  } mU;
  const nsISMILType* mType;

protected:
  void InitAndCheckPostcondition(const nsISMILType* aNewType);
  void DestroyAndCheckPostcondition();
  void DestroyAndReinit(const nsISMILType* aNewType);
};

#endif  // NS_SMILVALUE_H_
