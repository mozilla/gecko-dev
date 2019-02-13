/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "SkIntersections.h"
#include "SkLineParameters.h"
#include "SkPathOpsCubic.h"
#include "SkPathOpsQuad.h"
#include "SkPathOpsTriangle.h"

// from http://blog.gludion.com/2009/08/distance-to-quadratic-bezier-curve.html
// (currently only used by testing)
double SkDQuad::nearestT(const SkDPoint& pt) const {
    SkDVector pos = fPts[0] - pt;
    // search points P of bezier curve with PM.(dP / dt) = 0
    // a calculus leads to a 3d degree equation :
    SkDVector A = fPts[1] - fPts[0];
    SkDVector B = fPts[2] - fPts[1];
    B -= A;
    double a = B.dot(B);
    double b = 3 * A.dot(B);
    double c = 2 * A.dot(A) + pos.dot(B);
    double d = pos.dot(A);
    double ts[3];
    int roots = SkDCubic::RootsValidT(a, b, c, d, ts);
    double d0 = pt.distanceSquared(fPts[0]);
    double d2 = pt.distanceSquared(fPts[2]);
    double distMin = SkTMin(d0, d2);
    int bestIndex = -1;
    for (int index = 0; index < roots; ++index) {
        SkDPoint onQuad = ptAtT(ts[index]);
        double dist = pt.distanceSquared(onQuad);
        if (distMin > dist) {
            distMin = dist;
            bestIndex = index;
        }
    }
    if (bestIndex >= 0) {
        return ts[bestIndex];
    }
    return d0 < d2 ? 0 : 1;
}

bool SkDQuad::pointInHull(const SkDPoint& pt) const {
    return ((const SkDTriangle&) fPts).contains(pt);
}

SkDPoint SkDQuad::top(double startT, double endT) const {
    SkDQuad sub = subDivide(startT, endT);
    SkDPoint topPt = sub[0];
    if (topPt.fY > sub[2].fY || (topPt.fY == sub[2].fY && topPt.fX > sub[2].fX)) {
        topPt = sub[2];
    }
    if (!between(sub[0].fY, sub[1].fY, sub[2].fY)) {
        double extremeT;
        if (FindExtrema(sub[0].fY, sub[1].fY, sub[2].fY, &extremeT)) {
            extremeT = startT + (endT - startT) * extremeT;
            SkDPoint test = ptAtT(extremeT);
            if (topPt.fY > test.fY || (topPt.fY == test.fY && topPt.fX > test.fX)) {
                topPt = test;
            }
        }
    }
    return topPt;
}

int SkDQuad::AddValidTs(double s[], int realRoots, double* t) {
    int foundRoots = 0;
    for (int index = 0; index < realRoots; ++index) {
        double tValue = s[index];
        if (approximately_zero_or_more(tValue) && approximately_one_or_less(tValue)) {
            if (approximately_less_than_zero(tValue)) {
                tValue = 0;
            } else if (approximately_greater_than_one(tValue)) {
                tValue = 1;
            }
            for (int idx2 = 0; idx2 < foundRoots; ++idx2) {
                if (approximately_equal(t[idx2], tValue)) {
                    goto nextRoot;
                }
            }
            t[foundRoots++] = tValue;
        }
nextRoot:
        {}
    }
    return foundRoots;
}

// note: caller expects multiple results to be sorted smaller first
// note: http://en.wikipedia.org/wiki/Loss_of_significance has an interesting
//  analysis of the quadratic equation, suggesting why the following looks at
//  the sign of B -- and further suggesting that the greatest loss of precision
//  is in b squared less two a c
int SkDQuad::RootsValidT(double A, double B, double C, double t[2]) {
    double s[2];
    int realRoots = RootsReal(A, B, C, s);
    int foundRoots = AddValidTs(s, realRoots, t);
    return foundRoots;
}

/*
Numeric Solutions (5.6) suggests to solve the quadratic by computing
       Q = -1/2(B + sgn(B)Sqrt(B^2 - 4 A C))
and using the roots
      t1 = Q / A
      t2 = C / Q
*/
// this does not discard real roots <= 0 or >= 1
int SkDQuad::RootsReal(const double A, const double B, const double C, double s[2]) {
    const double p = B / (2 * A);
    const double q = C / A;
    if (approximately_zero(A) && (approximately_zero_inverse(p) || approximately_zero_inverse(q))) {
        if (approximately_zero(B)) {
            s[0] = 0;
            return C == 0;
        }
        s[0] = -C / B;
        return 1;
    }
    /* normal form: x^2 + px + q = 0 */
    const double p2 = p * p;
    if (!AlmostDequalUlps(p2, q) && p2 < q) {
        return 0;
    }
    double sqrt_D = 0;
    if (p2 > q) {
        sqrt_D = sqrt(p2 - q);
    }
    s[0] = sqrt_D - p;
    s[1] = -sqrt_D - p;
    return 1 + !AlmostDequalUlps(s[0], s[1]);
}

bool SkDQuad::isLinear(int startIndex, int endIndex) const {
    SkLineParameters lineParameters;
    lineParameters.quadEndPoints(*this, startIndex, endIndex);
    // FIXME: maybe it's possible to avoid this and compare non-normalized
    lineParameters.normalize();
    double distance = lineParameters.controlPtDistance(*this);
    return approximately_zero(distance);
}

SkDCubic SkDQuad::toCubic() const {
    SkDCubic cubic;
    cubic[0] = fPts[0];
    cubic[2] = fPts[1];
    cubic[3] = fPts[2];
    cubic[1].fX = (cubic[0].fX + cubic[2].fX * 2) / 3;
    cubic[1].fY = (cubic[0].fY + cubic[2].fY * 2) / 3;
    cubic[2].fX = (cubic[3].fX + cubic[2].fX * 2) / 3;
    cubic[2].fY = (cubic[3].fY + cubic[2].fY * 2) / 3;
    return cubic;
}

SkDVector SkDQuad::dxdyAtT(double t) const {
    double a = t - 1;
    double b = 1 - 2 * t;
    double c = t;
    SkDVector result = { a * fPts[0].fX + b * fPts[1].fX + c * fPts[2].fX,
            a * fPts[0].fY + b * fPts[1].fY + c * fPts[2].fY };
    return result;
}

// OPTIMIZE: assert if caller passes in t == 0 / t == 1 ?
SkDPoint SkDQuad::ptAtT(double t) const {
    if (0 == t) {
        return fPts[0];
    }
    if (1 == t) {
        return fPts[2];
    }
    double one_t = 1 - t;
    double a = one_t * one_t;
    double b = 2 * one_t * t;
    double c = t * t;
    SkDPoint result = { a * fPts[0].fX + b * fPts[1].fX + c * fPts[2].fX,
            a * fPts[0].fY + b * fPts[1].fY + c * fPts[2].fY };
    return result;
}

/*
Given a quadratic q, t1, and t2, find a small quadratic segment.

The new quadratic is defined by A, B, and C, where
 A = c[0]*(1 - t1)*(1 - t1) + 2*c[1]*t1*(1 - t1) + c[2]*t1*t1
 C = c[3]*(1 - t1)*(1 - t1) + 2*c[2]*t1*(1 - t1) + c[1]*t1*t1

To find B, compute the point halfway between t1 and t2:

q(at (t1 + t2)/2) == D

Next, compute where D must be if we know the value of B:

_12 = A/2 + B/2
12_ = B/2 + C/2
123 = A/4 + B/2 + C/4
    = D

Group the known values on one side:

B   = D*2 - A/2 - C/2
*/

static double interp_quad_coords(const double* src, double t) {
    double ab = SkDInterp(src[0], src[2], t);
    double bc = SkDInterp(src[2], src[4], t);
    double abc = SkDInterp(ab, bc, t);
    return abc;
}

bool SkDQuad::monotonicInY() const {
    return between(fPts[0].fY, fPts[1].fY, fPts[2].fY);
}

SkDQuad SkDQuad::subDivide(double t1, double t2) const {
    SkDQuad dst;
    double ax = dst[0].fX = interp_quad_coords(&fPts[0].fX, t1);
    double ay = dst[0].fY = interp_quad_coords(&fPts[0].fY, t1);
    double dx = interp_quad_coords(&fPts[0].fX, (t1 + t2) / 2);
    double dy = interp_quad_coords(&fPts[0].fY, (t1 + t2) / 2);
    double cx = dst[2].fX = interp_quad_coords(&fPts[0].fX, t2);
    double cy = dst[2].fY = interp_quad_coords(&fPts[0].fY, t2);
    /* bx = */ dst[1].fX = 2*dx - (ax + cx)/2;
    /* by = */ dst[1].fY = 2*dy - (ay + cy)/2;
    return dst;
}

void SkDQuad::align(int endIndex, SkDPoint* dstPt) const {
    if (fPts[endIndex].fX == fPts[1].fX) {
        dstPt->fX = fPts[endIndex].fX;
    }
    if (fPts[endIndex].fY == fPts[1].fY) {
        dstPt->fY = fPts[endIndex].fY;
    }
}

SkDPoint SkDQuad::subDivide(const SkDPoint& a, const SkDPoint& c, double t1, double t2) const {
    SkASSERT(t1 != t2);
    SkDPoint b;
#if 0
    // this approach assumes that the control point computed directly is accurate enough
    double dx = interp_quad_coords(&fPts[0].fX, (t1 + t2) / 2);
    double dy = interp_quad_coords(&fPts[0].fY, (t1 + t2) / 2);
    b.fX = 2 * dx - (a.fX + c.fX) / 2;
    b.fY = 2 * dy - (a.fY + c.fY) / 2;
#else
    SkDQuad sub = subDivide(t1, t2);
    SkDLine b0 = {{a, sub[1] + (a - sub[0])}};
    SkDLine b1 = {{c, sub[1] + (c - sub[2])}};
    SkIntersections i;
    i.intersectRay(b0, b1);
    if (i.used() == 1 && i[0][0] >= 0 && i[1][0] >= 0) {
        b = i.pt(0);
    } else {
        SkASSERT(i.used() <= 2);
        b = SkDPoint::Mid(b0[1], b1[1]);
    }
#endif
    if (t1 == 0 || t2 == 0) {
        align(0, &b);
    }
    if (t1 == 1 || t2 == 1) {
        align(2, &b);
    }
    if (AlmostBequalUlps(b.fX, a.fX)) {
        b.fX = a.fX;
    } else if (AlmostBequalUlps(b.fX, c.fX)) {
        b.fX = c.fX;
    }
    if (AlmostBequalUlps(b.fY, a.fY)) {
        b.fY = a.fY;
    } else if (AlmostBequalUlps(b.fY, c.fY)) {
        b.fY = c.fY;
    }
    return b;
}

/* classic one t subdivision */
static void interp_quad_coords(const double* src, double* dst, double t) {
    double ab = SkDInterp(src[0], src[2], t);
    double bc = SkDInterp(src[2], src[4], t);
    dst[0] = src[0];
    dst[2] = ab;
    dst[4] = SkDInterp(ab, bc, t);
    dst[6] = bc;
    dst[8] = src[4];
}

SkDQuadPair SkDQuad::chopAt(double t) const
{
    SkDQuadPair dst;
    interp_quad_coords(&fPts[0].fX, &dst.pts[0].fX, t);
    interp_quad_coords(&fPts[0].fY, &dst.pts[0].fY, t);
    return dst;
}

static int valid_unit_divide(double numer, double denom, double* ratio)
{
    if (numer < 0) {
        numer = -numer;
        denom = -denom;
    }
    if (denom == 0 || numer == 0 || numer >= denom) {
        return 0;
    }
    double r = numer / denom;
    if (r == 0) {  // catch underflow if numer <<<< denom
        return 0;
    }
    *ratio = r;
    return 1;
}

/** Quad'(t) = At + B, where
    A = 2(a - 2b + c)
    B = 2(b - a)
    Solve for t, only if it fits between 0 < t < 1
*/
int SkDQuad::FindExtrema(double a, double b, double c, double tValue[1]) {
    /*  At + B == 0
        t = -B / A
    */
    return valid_unit_divide(a - b, a - b - b + c, tValue);
}

/* Parameterization form, given A*t*t + 2*B*t*(1-t) + C*(1-t)*(1-t)
 *
 * a = A - 2*B +   C
 * b =     2*B - 2*C
 * c =             C
 */
void SkDQuad::SetABC(const double* quad, double* a, double* b, double* c) {
    *a = quad[0];      // a = A
    *b = 2 * quad[2];  // b =     2*B
    *c = quad[4];      // c =             C
    *b -= *c;          // b =     2*B -   C
    *a -= *b;          // a = A - 2*B +   C
    *b -= *c;          // b =     2*B - 2*C
}
