/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkCanvasVirtualEnforcer_DEFINED
#define SkCanvasVirtualEnforcer_DEFINED

#include "SkCanvas.h"

// If you would ordinarily want to inherit from Base (eg SkCanvas, SkNWayCanvas), instead
// inherit from SkCanvasVirtualEnforcer<Base>, which will make the build fail if you forget
// to override one of SkCanvas' key virtual hooks.
template <typename Base>
class SkCanvasVirtualEnforcer : public Base {
public:
    using Base::Base;

protected:
    void onDrawPaint(const SkPaint& paint) override = 0;
    void onDrawRect(const SkRect& rect, const SkPaint& paint) override = 0;
    void onDrawRRect(const SkRRect& rrect, const SkPaint& paint) override = 0;
    void onDrawDRRect(const SkRRect& outer, const SkRRect& inner,
                      const SkPaint& paint) override = 0;
    void onDrawOval(const SkRect& rect, const SkPaint& paint) override = 0;
    void onDrawArc(const SkRect& rect, SkScalar startAngle, SkScalar sweepAngle, bool useCenter,
                   const SkPaint& paint) override = 0;
    void onDrawPath(const SkPath& path, const SkPaint& paint) override = 0;
    void onDrawRegion(const SkRegion& region, const SkPaint& paint) override = 0;

    void onDrawText(const void* text, size_t byteLength, SkScalar x, SkScalar y,
                    const SkPaint& paint) override = 0;
    void onDrawPosText(const void* text, size_t byteLength, const SkPoint pos[],
                       const SkPaint& paint) override = 0;
    void onDrawPosTextH(const void* text, size_t byteLength, const SkScalar xpos[],
                        SkScalar constY, const SkPaint& paint) override = 0;
    void onDrawTextRSXform(const void* text, size_t byteLength, const SkRSXform xform[],
                           const SkRect* cullRect, const SkPaint& paint) override = 0;
    void onDrawTextBlob(const SkTextBlob* blob, SkScalar x, SkScalar y,
                        const SkPaint& paint) override = 0;

    void onDrawPatch(const SkPoint cubics[12], const SkColor colors[4],
                     const SkPoint texCoords[4], SkBlendMode mode,
                     const SkPaint& paint) override = 0;
    void onDrawPoints(SkCanvas::PointMode mode, size_t count, const SkPoint pts[],
                      const SkPaint& paint) override = 0;
    void onDrawVerticesObject(const SkVertices*, const SkVertices::Bone bones[], int boneCount,
                              SkBlendMode, const SkPaint&) override = 0;

    void onDrawImage(const SkImage* image, SkScalar dx, SkScalar dy,
                     const SkPaint* paint) override = 0;
    void onDrawImageRect(const SkImage* image, const SkRect* src, const SkRect& dst,
                         const SkPaint* paint, SkCanvas::SrcRectConstraint constraint) override = 0;
    void onDrawImageNine(const SkImage* image, const SkIRect& center, const SkRect& dst,
                         const SkPaint* paint) override = 0;
    void onDrawImageLattice(const SkImage* image, const SkCanvas::Lattice& lattice,
                            const SkRect& dst, const SkPaint* paint) override = 0;

    void onDrawBitmap(const SkBitmap& bitmap, SkScalar dx, SkScalar dy,
                      const SkPaint* paint) override = 0;
    void onDrawBitmapRect(const SkBitmap& bitmap, const SkRect* src, const SkRect& dst,
                          const SkPaint* paint,
                          SkCanvas::SrcRectConstraint constraint) override = 0;
    void onDrawBitmapNine(const SkBitmap& bitmap, const SkIRect& center, const SkRect& dst,
                          const SkPaint* paint) override = 0;
    void onDrawBitmapLattice(const SkBitmap& bitmap, const SkCanvas::Lattice& lattice,
                             const SkRect& dst, const SkPaint* paint) override = 0;

    void onDrawAtlas(const SkImage* atlas, const SkRSXform xform[], const SkRect rect[],
                     const SkColor colors[], int count, SkBlendMode mode, const SkRect* cull,
                     const SkPaint* paint) override = 0;

    void onDrawAnnotation(const SkRect& rect, const char key[], SkData* value) override = 0;
    void onDrawShadowRec(const SkPath&, const SkDrawShadowRec&) override = 0;

    void onDrawDrawable(SkDrawable* drawable, const SkMatrix* matrix) override = 0;
    void onDrawPicture(const SkPicture* picture, const SkMatrix* matrix,
                       const SkPaint* paint) override = 0;
};

#endif
