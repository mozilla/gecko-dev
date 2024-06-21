/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGPATHSEGUTILS_H_
#define DOM_SVG_SVGPATHSEGUTILS_H_

#include "mozilla/ArrayUtils.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/Span.h"
#include "nsStringFwd.h"

namespace mozilla {
template <typename Angle, typename LP>
struct StyleGenericShapeCommand;

/**
 * Code that works with path segments can use an instance of this class to
 * store/provide information about the start of the current subpath and the
 * last path segment (if any).
 */
struct SVGPathTraversalState {
  using Point = gfx::Point;

  enum TraversalMode { eUpdateAll, eUpdateOnlyStartAndCurrentPos };

  SVGPathTraversalState()
      : start(0.0, 0.0),
        pos(0.0, 0.0),
        cp1(0.0, 0.0),
        cp2(0.0, 0.0),
        length(0.0),
        mode(eUpdateAll) {}

  bool ShouldUpdateLengthAndControlPoints() { return mode == eUpdateAll; }

  Point start;  // start point of current sub path (reset each moveto)

  Point pos;  // current position (end point of previous segment)

  Point cp1;  // quadratic control point - if the previous segment was a
              // quadratic bezier curve then this is set to the absolute
              // position of its control point, otherwise its set to pos

  Point cp2;  // cubic control point - if the previous segment was a cubic
              // bezier curve then this is set to the absolute position of
              // its second control point, otherwise it's set to pos

  float length;  // accumulated path length

  TraversalMode mode;  // indicates what to track while traversing a path
};

/**
 * This class is just a collection of static methods - it doesn't have any data
 * members, and it's not possible to create instances of this class. This class
 * exists purely as a convenient place to gather together a bunch of methods
 * related to manipulating and answering questions about path segments.
 * Internally we represent path segments purely as an array of floats. See the
 * comment documenting SVGPathData for more info on that.
 *
 * The DOM wrapper classes for encoded path segments (data contained in
 * instances of SVGPathData) is DOMSVGPathSeg and its sub-classes. Note that
 * there are multiple different DOM classes for path segs - one for each of the
 * 19 SVG 1.1 segment types.
 */
class SVGPathSegUtils {
 private:
  SVGPathSegUtils() = default;  // private to prevent instances

 public:
  /**
   * Traverse the given path segment and update the SVGPathTraversalState
   * object. This is identical to the above one but accepts StylePathCommand.
   */
  static void TraversePathSegment(
      const StyleGenericShapeCommand<float, float>& aCommand,
      SVGPathTraversalState& aState);
};

/// Detect whether the path represents a rectangle (for both filling AND
/// stroking) and if so returns it.
///
/// This is typically useful for google slides which has many of these rectangle
/// shaped paths. It handles the same scenarios as skia's
/// SkPathPriv::IsRectContour which it is inspried from, including zero-length
/// edges and multiple points on edges of the rectangle, and doesn't attempt to
/// detect flat curves (that could easily be added but the expectation is that
/// since skia doesn't fast path it we're not likely to run into it in
/// practice).
///
/// We could implement something similar for polygons.
Maybe<gfx::Rect> SVGPathToAxisAlignedRect(
    Span<const StyleGenericShapeCommand<float, float>> aPath);

}  // namespace mozilla

#endif  // DOM_SVG_SVGPATHSEGUTILS_H_
