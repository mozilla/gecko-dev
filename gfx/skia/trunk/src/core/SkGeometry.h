
/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#ifndef SkGeometry_DEFINED
#define SkGeometry_DEFINED

#include "SkMatrix.h"

/** An XRay is a half-line that runs from the specific point/origin to
    +infinity in the X direction. e.g. XRay(3,5) is the half-line
    (3,5)....(infinity, 5)
 */
typedef SkPoint SkXRay;

/** Given a line segment from pts[0] to pts[1], and an xray, return true if
    they intersect. Optional outgoing "ambiguous" argument indicates
    whether the answer is ambiguous because the query occurred exactly at
    one of the endpoints' y coordinates, indicating that another query y
    coordinate is preferred for robustness.
*/
bool SkXRayCrossesLine(const SkXRay& pt, const SkPoint pts[2],
                       bool* ambiguous = NULL);

/** Given a quadratic equation Ax^2 + Bx + C = 0, return 0, 1, 2 roots for the
    equation.
*/
int SkFindUnitQuadRoots(SkScalar A, SkScalar B, SkScalar C, SkScalar roots[2]);

///////////////////////////////////////////////////////////////////////////////

/** Set pt to the point on the src quadratic specified by t. t must be
    0 <= t <= 1.0
*/
void SkEvalQuadAt(const SkPoint src[3], SkScalar t, SkPoint* pt,
                  SkVector* tangent = NULL);
void SkEvalQuadAtHalf(const SkPoint src[3], SkPoint* pt,
                      SkVector* tangent = NULL);

/** Given a src quadratic bezier, chop it at the specified t value,
    where 0 < t < 1, and return the two new quadratics in dst:
    dst[0..2] and dst[2..4]
*/
void SkChopQuadAt(const SkPoint src[3], SkPoint dst[5], SkScalar t);

/** Given a src quadratic bezier, chop it at the specified t == 1/2,
    The new quads are returned in dst[0..2] and dst[2..4]
*/
void SkChopQuadAtHalf(const SkPoint src[3], SkPoint dst[5]);

/** Given the 3 coefficients for a quadratic bezier (either X or Y values), look
    for extrema, and return the number of t-values that are found that represent
    these extrema. If the quadratic has no extrema betwee (0..1) exclusive, the
    function returns 0.
    Returned count      tValues[]
    0                   ignored
    1                   0 < tValues[0] < 1
*/
int SkFindQuadExtrema(SkScalar a, SkScalar b, SkScalar c, SkScalar tValues[1]);

/** Given 3 points on a quadratic bezier, chop it into 1, 2 beziers such that
    the resulting beziers are monotonic in Y. This is called by the scan converter.
    Depending on what is returned, dst[] is treated as follows
    0   dst[0..2] is the original quad
    1   dst[0..2] and dst[2..4] are the two new quads
*/
int SkChopQuadAtYExtrema(const SkPoint src[3], SkPoint dst[5]);
int SkChopQuadAtXExtrema(const SkPoint src[3], SkPoint dst[5]);

/** Given 3 points on a quadratic bezier, if the point of maximum
    curvature exists on the segment, returns the t value for this
    point along the curve. Otherwise it will return a value of 0.
*/
float SkFindQuadMaxCurvature(const SkPoint src[3]);

/** Given 3 points on a quadratic bezier, divide it into 2 quadratics
    if the point of maximum curvature exists on the quad segment.
    Depending on what is returned, dst[] is treated as follows
    1   dst[0..2] is the original quad
    2   dst[0..2] and dst[2..4] are the two new quads
    If dst == null, it is ignored and only the count is returned.
*/
int SkChopQuadAtMaxCurvature(const SkPoint src[3], SkPoint dst[5]);

/** Given 3 points on a quadratic bezier, use degree elevation to
    convert it into the cubic fitting the same curve. The new cubic
    curve is returned in dst[0..3].
*/
SK_API void SkConvertQuadToCubic(const SkPoint src[3], SkPoint dst[4]);

///////////////////////////////////////////////////////////////////////////////

/** Convert from parametric from (pts) to polynomial coefficients
    coeff[0]*T^3 + coeff[1]*T^2 + coeff[2]*T + coeff[3]
*/
void SkGetCubicCoeff(const SkPoint pts[4], SkScalar cx[4], SkScalar cy[4]);

/** Set pt to the point on the src cubic specified by t. t must be
    0 <= t <= 1.0
*/
void SkEvalCubicAt(const SkPoint src[4], SkScalar t, SkPoint* locOrNull,
                   SkVector* tangentOrNull, SkVector* curvatureOrNull);

/** Given a src cubic bezier, chop it at the specified t value,
    where 0 < t < 1, and return the two new cubics in dst:
    dst[0..3] and dst[3..6]
*/
void SkChopCubicAt(const SkPoint src[4], SkPoint dst[7], SkScalar t);
/** Given a src cubic bezier, chop it at the specified t values,
    where 0 < t < 1, and return the new cubics in dst:
    dst[0..3],dst[3..6],...,dst[3*t_count..3*(t_count+1)]
*/
void SkChopCubicAt(const SkPoint src[4], SkPoint dst[], const SkScalar t[],
                   int t_count);

/** Given a src cubic bezier, chop it at the specified t == 1/2,
    The new cubics are returned in dst[0..3] and dst[3..6]
*/
void SkChopCubicAtHalf(const SkPoint src[4], SkPoint dst[7]);

/** Given the 4 coefficients for a cubic bezier (either X or Y values), look
    for extrema, and return the number of t-values that are found that represent
    these extrema. If the cubic has no extrema betwee (0..1) exclusive, the
    function returns 0.
    Returned count      tValues[]
    0                   ignored
    1                   0 < tValues[0] < 1
    2                   0 < tValues[0] < tValues[1] < 1
*/
int SkFindCubicExtrema(SkScalar a, SkScalar b, SkScalar c, SkScalar d,
                       SkScalar tValues[2]);

/** Given 4 points on a cubic bezier, chop it into 1, 2, 3 beziers such that
    the resulting beziers are monotonic in Y. This is called by the scan converter.
    Depending on what is returned, dst[] is treated as follows
    0   dst[0..3] is the original cubic
    1   dst[0..3] and dst[3..6] are the two new cubics
    2   dst[0..3], dst[3..6], dst[6..9] are the three new cubics
    If dst == null, it is ignored and only the count is returned.
*/
int SkChopCubicAtYExtrema(const SkPoint src[4], SkPoint dst[10]);
int SkChopCubicAtXExtrema(const SkPoint src[4], SkPoint dst[10]);

/** Given a cubic bezier, return 0, 1, or 2 t-values that represent the
    inflection points.
*/
int SkFindCubicInflections(const SkPoint src[4], SkScalar tValues[2]);

/** Return 1 for no chop, 2 for having chopped the cubic at a single
    inflection point, 3 for having chopped at 2 inflection points.
    dst will hold the resulting 1, 2, or 3 cubics.
*/
int SkChopCubicAtInflections(const SkPoint src[4], SkPoint dst[10]);

int SkFindCubicMaxCurvature(const SkPoint src[4], SkScalar tValues[3]);
int SkChopCubicAtMaxCurvature(const SkPoint src[4], SkPoint dst[13],
                              SkScalar tValues[3] = NULL);

/** Given a monotonic cubic bezier, determine whether an xray intersects the
    cubic.
    By definition the cubic is open at the starting point; in other
    words, if pt.fY is equivalent to cubic[0].fY, and pt.fX is to the
    left of the curve, the line is not considered to cross the curve,
    but if it is equal to cubic[3].fY then it is considered to
    cross.
    Optional outgoing "ambiguous" argument indicates whether the answer is
    ambiguous because the query occurred exactly at one of the endpoints' y
    coordinates, indicating that another query y coordinate is preferred
    for robustness.
 */
bool SkXRayCrossesMonotonicCubic(const SkXRay& pt, const SkPoint cubic[4],
                                 bool* ambiguous = NULL);

/** Given an arbitrary cubic bezier, return the number of times an xray crosses
    the cubic. Valid return values are [0..3]
    By definition the cubic is open at the starting point; in other
    words, if pt.fY is equivalent to cubic[0].fY, and pt.fX is to the
    left of the curve, the line is not considered to cross the curve,
    but if it is equal to cubic[3].fY then it is considered to
    cross.
    Optional outgoing "ambiguous" argument indicates whether the answer is
    ambiguous because the query occurred exactly at one of the endpoints' y
    coordinates or at a tangent point, indicating that another query y
    coordinate is preferred for robustness.
 */
int SkNumXRayCrossingsForCubic(const SkXRay& pt, const SkPoint cubic[4],
                               bool* ambiguous = NULL);

///////////////////////////////////////////////////////////////////////////////

enum SkRotationDirection {
    kCW_SkRotationDirection,
    kCCW_SkRotationDirection
};

/** Maximum number of points needed in the quadPoints[] parameter for
    SkBuildQuadArc()
*/
#define kSkBuildQuadArcStorage  17

/** Given 2 unit vectors and a rotation direction, fill out the specified
    array of points with quadratic segments. Return is the number of points
    written to, which will be { 0, 3, 5, 7, ... kSkBuildQuadArcStorage }

    matrix, if not null, is appled to the points before they are returned.
*/
int SkBuildQuadArc(const SkVector& unitStart, const SkVector& unitStop,
                   SkRotationDirection, const SkMatrix*, SkPoint quadPoints[]);

// experimental
struct SkConic {
    SkPoint  fPts[3];
    SkScalar fW;

    void set(const SkPoint pts[3], SkScalar w) {
        memcpy(fPts, pts, 3 * sizeof(SkPoint));
        fW = w;
    }

    /**
     *  Given a t-value [0...1] return its position and/or tangent.
     *  If pos is not null, return its position at the t-value.
     *  If tangent is not null, return its tangent at the t-value. NOTE the
     *  tangent value's length is arbitrary, and only its direction should
     *  be used.
     */
    void evalAt(SkScalar t, SkPoint* pos, SkVector* tangent = NULL) const;
    void chopAt(SkScalar t, SkConic dst[2]) const;
    void chop(SkConic dst[2]) const;

    void computeAsQuadError(SkVector* err) const;
    bool asQuadTol(SkScalar tol) const;

    /**
     *  return the power-of-2 number of quads needed to approximate this conic
     *  with a sequence of quads. Will be >= 0.
     */
    int computeQuadPOW2(SkScalar tol) const;

    /**
     *  Chop this conic into N quads, stored continguously in pts[], where
     *  N = 1 << pow2. The amount of storage needed is (1 + 2 * N)
     */
    int chopIntoQuadsPOW2(SkPoint pts[], int pow2) const;

    bool findXExtrema(SkScalar* t) const;
    bool findYExtrema(SkScalar* t) const;
    bool chopAtXExtrema(SkConic dst[2]) const;
    bool chopAtYExtrema(SkConic dst[2]) const;

    void computeTightBounds(SkRect* bounds) const;
    void computeFastBounds(SkRect* bounds) const;

    /** Find the parameter value where the conic takes on its maximum curvature.
     *
     *  @param t   output scalar for max curvature.  Will be unchanged if
     *             max curvature outside 0..1 range.
     *
     *  @return  true if max curvature found inside 0..1 range, false otherwise
     */
    bool findMaxCurvature(SkScalar* t) const;
};

#include "SkTemplates.h"

/**
 *  Help class to allocate storage for approximating a conic with N quads.
 */
class SkAutoConicToQuads {
public:
    SkAutoConicToQuads() : fQuadCount(0) {}

    /**
     *  Given a conic and a tolerance, return the array of points for the
     *  approximating quad(s). Call countQuads() to know the number of quads
     *  represented in these points.
     *
     *  The quads are allocated to share end-points. e.g. if there are 4 quads,
     *  there will be 9 points allocated as follows
     *      quad[0] == pts[0..2]
     *      quad[1] == pts[2..4]
     *      quad[2] == pts[4..6]
     *      quad[3] == pts[6..8]
     */
    const SkPoint* computeQuads(const SkConic& conic, SkScalar tol) {
        int pow2 = conic.computeQuadPOW2(tol);
        fQuadCount = 1 << pow2;
        SkPoint* pts = fStorage.reset(1 + 2 * fQuadCount);
        conic.chopIntoQuadsPOW2(pts, pow2);
        return pts;
    }

    const SkPoint* computeQuads(const SkPoint pts[3], SkScalar weight,
                                SkScalar tol) {
        SkConic conic;
        conic.set(pts, weight);
        return computeQuads(conic, tol);
    }

    int countQuads() const { return fQuadCount; }

private:
    enum {
        kQuadCount = 8, // should handle most conics
        kPointCount = 1 + 2 * kQuadCount,
    };
    SkAutoSTMalloc<kPointCount, SkPoint> fStorage;
    int fQuadCount; // #quads for current usage
};

#endif
