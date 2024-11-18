/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGARCCONVERTER_H_
#define DOM_SVG_SVGARCCONVERTER_H_

#include "mozilla/gfx/Point.h"

namespace mozilla {

class MOZ_STACK_CLASS SVGArcConverter {
  using Point = mozilla::gfx::Point;

 public:
  SVGArcConverter(const Point& from, const Point& to, const Point& radii,
                  double angle, bool largeArcFlag, bool sweepFlag);
  bool GetNextSegment(Point* cp1, Point* cp2, Point* to);

 protected:
  int32_t mNumSegs, mSegIndex;
  double mTheta, mDelta, mT;
  double mSinPhi, mCosPhi;
  double mRx, mRy;
  Point mFrom, mC;
};

}  // namespace mozilla

#endif  // DOM_SVG_SVGARCCONVERTER_H_
