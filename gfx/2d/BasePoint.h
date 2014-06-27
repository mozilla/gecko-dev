/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_BASEPOINT_H_
#define MOZILLA_GFX_BASEPOINT_H_

#include <cmath>
#include "mozilla/Attributes.h"

namespace mozilla {
namespace gfx {

/**
 * Do not use this class directly. Subclass it, pass that subclass as the
 * Sub parameter, and only use that subclass. This allows methods to safely
 * cast 'this' to 'Sub*'.
 */
template <class T, class Sub>
struct BasePoint {
  T x, y;

  // Constructors
  MOZ_CONSTEXPR BasePoint() : x(0), y(0) {}
  MOZ_CONSTEXPR BasePoint(T aX, T aY) : x(aX), y(aY) {}

  void MoveTo(T aX, T aY) { x = aX; y = aY; }
  void MoveBy(T aDx, T aDy) { x += aDx; y += aDy; }

  // Note that '=' isn't defined so we'll get the
  // compiler generated default assignment operator

  bool operator==(const Sub& aPoint) const {
    return x == aPoint.x && y == aPoint.y;
  }
  bool operator!=(const Sub& aPoint) const {
    return x != aPoint.x || y != aPoint.y;
  }

  Sub operator+(const Sub& aPoint) const {
    return Sub(x + aPoint.x, y + aPoint.y);
  }
  Sub operator-(const Sub& aPoint) const {
    return Sub(x - aPoint.x, y - aPoint.y);
  }
  Sub& operator+=(const Sub& aPoint) {
    x += aPoint.x;
    y += aPoint.y;
    return *static_cast<Sub*>(this);
  }
  Sub& operator-=(const Sub& aPoint) {
    x -= aPoint.x;
    y -= aPoint.y;
    return *static_cast<Sub*>(this);
  }

  Sub operator*(T aScale) const {
    return Sub(x * aScale, y * aScale);
  }
  Sub operator/(T aScale) const {
    return Sub(x / aScale, y / aScale);
  }

  Sub operator-() const {
    return Sub(-x, -y);
  }

  T Length() const {
    return hypot(x, y);
  }

  // Round() is *not* rounding to nearest integer if the values are negative.
  // They are always rounding as floor(n + 0.5).
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=410748#c14
  Sub& Round() {
    x = static_cast<T>(floor(x + 0.5));
    y = static_cast<T>(floor(y + 0.5));
    return *static_cast<Sub*>(this);
  }

};

}
}

#endif /* MOZILLA_GFX_BASEPOINT_H_ */
