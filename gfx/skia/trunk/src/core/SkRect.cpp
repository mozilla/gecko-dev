
/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include "SkRect.h"

void SkIRect::join(int32_t left, int32_t top, int32_t right, int32_t bottom) {
    // do nothing if the params are empty
    if (left >= right || top >= bottom) {
        return;
    }

    // if we are empty, just assign
    if (fLeft >= fRight || fTop >= fBottom) {
        this->set(left, top, right, bottom);
    } else {
        if (left < fLeft) fLeft = left;
        if (top < fTop) fTop = top;
        if (right > fRight) fRight = right;
        if (bottom > fBottom) fBottom = bottom;
    }
}

void SkIRect::sort() {
    if (fLeft > fRight) {
        SkTSwap<int32_t>(fLeft, fRight);
    }
    if (fTop > fBottom) {
        SkTSwap<int32_t>(fTop, fBottom);
    }
}

/////////////////////////////////////////////////////////////////////////////

void SkRect::sort() {
    if (fLeft > fRight) {
        SkTSwap<SkScalar>(fLeft, fRight);
    }
    if (fTop > fBottom) {
        SkTSwap<SkScalar>(fTop, fBottom);
    }
}

void SkRect::toQuad(SkPoint quad[4]) const {
    SkASSERT(quad);

    quad[0].set(fLeft, fTop);
    quad[1].set(fRight, fTop);
    quad[2].set(fRight, fBottom);
    quad[3].set(fLeft, fBottom);
}

bool SkRect::setBoundsCheck(const SkPoint pts[], int count) {
    SkASSERT((pts && count > 0) || count == 0);

    bool isFinite = true;

    if (count <= 0) {
        sk_bzero(this, sizeof(SkRect));
    } else {
        SkScalar    l, t, r, b;

        l = r = pts[0].fX;
        t = b = pts[0].fY;

        // If all of the points are finite, accum should stay 0. If we encounter
        // a NaN or infinity, then accum should become NaN.
        float accum = 0;
        accum *= l; accum *= t;

        for (int i = 1; i < count; i++) {
            SkScalar x = pts[i].fX;
            SkScalar y = pts[i].fY;

            accum *= x; accum *= y;

            // we use if instead of if/else, so we can generate min/max
            // float instructions (at least on SSE)
            if (x < l) l = x;
            if (x > r) r = x;

            if (y < t) t = y;
            if (y > b) b = y;
        }

        SkASSERT(!accum || !SkScalarIsFinite(accum));
        if (accum) {
            l = t = r = b = 0;
            isFinite = false;
        }
        this->set(l, t, r, b);
    }

    return isFinite;
}

bool SkRect::intersect(SkScalar left, SkScalar top, SkScalar right,
                       SkScalar bottom) {
    if (left < right && top < bottom && !this->isEmpty() && // check for empties
        fLeft < right && left < fRight && fTop < bottom && top < fBottom)
    {
        if (fLeft < left) fLeft = left;
        if (fTop < top) fTop = top;
        if (fRight > right) fRight = right;
        if (fBottom > bottom) fBottom = bottom;
        return true;
    }
    return false;
}

bool SkRect::intersect(const SkRect& r) {
    return this->intersect(r.fLeft, r.fTop, r.fRight, r.fBottom);
}

bool SkRect::intersect2(const SkRect& r) {
    SkScalar L = SkMaxScalar(fLeft, r.fLeft);
    SkScalar R = SkMinScalar(fRight, r.fRight);
    if (L >= R) {
        return false;
    }
    SkScalar T = SkMaxScalar(fTop, r.fTop);
    SkScalar B = SkMinScalar(fBottom, r.fBottom);
    if (T >= B) {
        return false;
    }
    this->set(L, T, R, B);
    return true;
}

bool SkRect::intersect(const SkRect& a, const SkRect& b) {

    if (!a.isEmpty() && !b.isEmpty() &&
        a.fLeft < b.fRight && b.fLeft < a.fRight &&
        a.fTop < b.fBottom && b.fTop < a.fBottom) {
        fLeft   = SkMaxScalar(a.fLeft,   b.fLeft);
        fTop    = SkMaxScalar(a.fTop,    b.fTop);
        fRight  = SkMinScalar(a.fRight,  b.fRight);
        fBottom = SkMinScalar(a.fBottom, b.fBottom);
        return true;
    }
    return false;
}

void SkRect::join(SkScalar left, SkScalar top, SkScalar right,
                  SkScalar bottom) {
    // do nothing if the params are empty
    if (left >= right || top >= bottom) {
        return;
    }

    // if we are empty, just assign
    if (fLeft >= fRight || fTop >= fBottom) {
        this->set(left, top, right, bottom);
    } else {
        if (left < fLeft) fLeft = left;
        if (top < fTop) fTop = top;
        if (right > fRight) fRight = right;
        if (bottom > fBottom) fBottom = bottom;
    }
}
