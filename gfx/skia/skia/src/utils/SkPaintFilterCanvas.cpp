/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkPaintFilterCanvas.h"

#include "SkPaint.h"
#include "SkPixmap.h"
#include "SkSurface.h"
#include "SkTLazy.h"

class SkPaintFilterCanvas::AutoPaintFilter {
public:
    AutoPaintFilter(const SkPaintFilterCanvas* canvas, Type type, const SkPaint* paint)
        : fPaint(paint) {
        fShouldDraw = canvas->onFilter(&fPaint, type);
    }

    AutoPaintFilter(const SkPaintFilterCanvas* canvas, Type type, const SkPaint& paint)
        : AutoPaintFilter(canvas, type, &paint) { }

    const SkPaint* paint() const { return fPaint; }

    bool shouldDraw() const { return fShouldDraw; }

private:
    SkTCopyOnFirstWrite<SkPaint> fPaint;
    bool                         fShouldDraw;
};

SkPaintFilterCanvas::SkPaintFilterCanvas(SkCanvas *canvas)
    : SkCanvasVirtualEnforcer<SkNWayCanvas>(canvas->imageInfo().width(),
                                              canvas->imageInfo().height()) {

    // Transfer matrix & clip state before adding the target canvas.
    this->clipRect(SkRect::Make(canvas->getDeviceClipBounds()));
    this->setMatrix(canvas->getTotalMatrix());

    this->addCanvas(canvas);
}

void SkPaintFilterCanvas::onDrawPaint(const SkPaint& paint) {
    AutoPaintFilter apf(this, kPaint_Type, paint);
    if (apf.shouldDraw()) {
        this->SkNWayCanvas::onDrawPaint(*apf.paint());
    }
}

void SkPaintFilterCanvas::onDrawPoints(PointMode mode, size_t count, const SkPoint pts[],
                                       const SkPaint& paint) {
    AutoPaintFilter apf(this, kPoint_Type, paint);
    if (apf.shouldDraw()) {
        this->SkNWayCanvas::onDrawPoints(mode, count, pts, *apf.paint());
    }
}

void SkPaintFilterCanvas::onDrawRect(const SkRect& rect, const SkPaint& paint) {
    AutoPaintFilter apf(this, kRect_Type, paint);
    if (apf.shouldDraw()) {
        this->SkNWayCanvas::onDrawRect(rect, *apf.paint());
    }
}

void SkPaintFilterCanvas::onDrawRRect(const SkRRect& rrect, const SkPaint& paint) {
    AutoPaintFilter apf(this, kRRect_Type, paint);
    if (apf.shouldDraw()) {
        this->SkNWayCanvas::onDrawRRect(rrect, *apf.paint());
    }
}

void SkPaintFilterCanvas::onDrawDRRect(const SkRRect& outer, const SkRRect& inner,
                                       const SkPaint& paint) {
    AutoPaintFilter apf(this, kDRRect_Type, paint);
    if (apf.shouldDraw()) {
        this->SkNWayCanvas::onDrawDRRect(outer, inner, *apf.paint());
    }
}

void SkPaintFilterCanvas::onDrawRegion(const SkRegion& region, const SkPaint& paint) {
    AutoPaintFilter apf(this, kPath_Type, paint);
    if (apf.shouldDraw()) {
        this->SkNWayCanvas::onDrawRegion(region, *apf.paint());
    }
}

void SkPaintFilterCanvas::onDrawOval(const SkRect& rect, const SkPaint& paint) {
    AutoPaintFilter apf(this, kOval_Type, paint);
    if (apf.shouldDraw()) {
        this->SkNWayCanvas::onDrawOval(rect, *apf.paint());
    }
}

void SkPaintFilterCanvas::onDrawArc(const SkRect& rect, SkScalar startAngle, SkScalar sweepAngle,
                                    bool useCenter, const SkPaint& paint) {
    AutoPaintFilter apf(this, kArc_Type, paint);
    if (apf.shouldDraw()) {
        this->SkNWayCanvas::onDrawArc(rect, startAngle, sweepAngle, useCenter, *apf.paint());
    }
}

void SkPaintFilterCanvas::onDrawPath(const SkPath& path, const SkPaint& paint) {
    AutoPaintFilter apf(this, kPath_Type, paint);
    if (apf.shouldDraw()) {
        this->SkNWayCanvas::onDrawPath(path, *apf.paint());
    }
}

void SkPaintFilterCanvas::onDrawBitmap(const SkBitmap& bm, SkScalar left, SkScalar top,
                                       const SkPaint* paint) {
    AutoPaintFilter apf(this, kBitmap_Type, paint);
    if (apf.shouldDraw()) {
        this->SkNWayCanvas::onDrawBitmap(bm, left, top, apf.paint());
    }
}

void SkPaintFilterCanvas::onDrawBitmapRect(const SkBitmap& bm, const SkRect* src, const SkRect& dst,
                                           const SkPaint* paint, SrcRectConstraint constraint) {
    AutoPaintFilter apf(this, kBitmap_Type, paint);
    if (apf.shouldDraw()) {
        this->SkNWayCanvas::onDrawBitmapRect(bm, src, dst, apf.paint(), constraint);
    }
}

void SkPaintFilterCanvas::onDrawBitmapNine(const SkBitmap& bm, const SkIRect& center,
                                           const SkRect& dst, const SkPaint* paint) {
    AutoPaintFilter apf(this, kBitmap_Type, paint);
    if (apf.shouldDraw()) {
        this->SkNWayCanvas::onDrawBitmapNine(bm, center, dst, apf.paint());
    }
}

void SkPaintFilterCanvas::onDrawBitmapLattice(const SkBitmap& bitmap, const Lattice& lattice,
                                              const SkRect& dst, const SkPaint* paint) {
    AutoPaintFilter apf(this, kBitmap_Type, paint);
    if (apf.shouldDraw()) {
        this->SkNWayCanvas::onDrawBitmapLattice(bitmap, lattice, dst, apf.paint());
    }
}

void SkPaintFilterCanvas::onDrawImage(const SkImage* image, SkScalar left, SkScalar top,
                                      const SkPaint* paint) {
    AutoPaintFilter apf(this, kBitmap_Type, paint);
    if (apf.shouldDraw()) {
        this->SkNWayCanvas::onDrawImage(image, left, top, apf.paint());
    }
}

void SkPaintFilterCanvas::onDrawImageRect(const SkImage* image, const SkRect* src,
                                          const SkRect& dst, const SkPaint* paint,
                                          SrcRectConstraint constraint) {
    AutoPaintFilter apf(this, kBitmap_Type, paint);
    if (apf.shouldDraw()) {
        this->SkNWayCanvas::onDrawImageRect(image, src, dst, apf.paint(), constraint);
    }
}

void SkPaintFilterCanvas::onDrawImageNine(const SkImage* image, const SkIRect& center,
                                          const SkRect& dst, const SkPaint* paint) {
    AutoPaintFilter apf(this, kBitmap_Type, paint);
    if (apf.shouldDraw()) {
        this->SkNWayCanvas::onDrawImageNine(image, center, dst, apf.paint());
    }
}

void SkPaintFilterCanvas::onDrawImageLattice(const SkImage* image, const Lattice& lattice,
                                             const SkRect& dst, const SkPaint* paint) {
    AutoPaintFilter apf(this, kBitmap_Type, paint);
    if (apf.shouldDraw()) {
        this->SkNWayCanvas::onDrawImageLattice(image, lattice, dst, apf.paint());
    }
}

void SkPaintFilterCanvas::onDrawVerticesObject(const SkVertices* vertices,
                                               const SkVertices::Bone bones[], int boneCount,
                                               SkBlendMode bmode, const SkPaint& paint) {
    AutoPaintFilter apf(this, kVertices_Type, paint);
    if (apf.shouldDraw()) {
        this->SkNWayCanvas::onDrawVerticesObject(vertices, bones, boneCount, bmode, *apf.paint());
    }
}

void SkPaintFilterCanvas::onDrawPatch(const SkPoint cubics[], const SkColor colors[],
                                      const SkPoint texCoords[], SkBlendMode bmode,
                                      const SkPaint& paint) {
    AutoPaintFilter apf(this, kPatch_Type, paint);
    if (apf.shouldDraw()) {
        this->SkNWayCanvas::onDrawPatch(cubics, colors, texCoords, bmode, *apf.paint());
    }
}

void SkPaintFilterCanvas::onDrawPicture(const SkPicture* picture, const SkMatrix* m,
                                        const SkPaint* paint) {
    AutoPaintFilter apf(this, kPicture_Type, paint);
    if (apf.shouldDraw()) {
        this->SkNWayCanvas::onDrawPicture(picture, m, apf.paint());
    }
}

void SkPaintFilterCanvas::onDrawDrawable(SkDrawable* drawable, const SkMatrix* matrix) {
    // There is no paint to filter in this case, but we can still filter on type.
    // Subclasses need to unroll the drawable explicity (by overriding this method) in
    // order to actually filter nested content.
    AutoPaintFilter apf(this, kDrawable_Type, nullptr);
    if (apf.shouldDraw()) {
        this->SkNWayCanvas::onDrawDrawable(drawable, matrix);
    }
}

void SkPaintFilterCanvas::onDrawText(const void* text, size_t byteLength, SkScalar x, SkScalar y,
                                     const SkPaint& paint) {
    AutoPaintFilter apf(this, kText_Type, paint);
    if (apf.shouldDraw()) {
        this->SkNWayCanvas::onDrawText(text, byteLength, x, y, *apf.paint());
    }
}

void SkPaintFilterCanvas::onDrawPosText(const void* text, size_t byteLength, const SkPoint pos[],
                                        const SkPaint& paint) {
    AutoPaintFilter apf(this, kText_Type, paint);
    if (apf.shouldDraw()) {
        this->SkNWayCanvas::onDrawPosText(text, byteLength, pos, *apf.paint());
    }
}

void SkPaintFilterCanvas::onDrawPosTextH(const void* text, size_t byteLength, const SkScalar xpos[],
                                         SkScalar constY, const SkPaint& paint) {
    AutoPaintFilter apf(this, kText_Type, paint);
    if (apf.shouldDraw()) {
        this->SkNWayCanvas::onDrawPosTextH(text, byteLength, xpos, constY, *apf.paint());
    }
}

void SkPaintFilterCanvas::onDrawTextRSXform(const void* text, size_t byteLength,
                                            const SkRSXform xform[], const SkRect* cull,
                                            const SkPaint& paint) {
    AutoPaintFilter apf(this, kText_Type, paint);
    if (apf.shouldDraw()) {
        this->SkNWayCanvas::onDrawTextRSXform(text, byteLength, xform, cull, *apf.paint());
    }
}

void SkPaintFilterCanvas::onDrawTextBlob(const SkTextBlob* blob, SkScalar x, SkScalar y,
                                         const SkPaint& paint) {
    AutoPaintFilter apf(this, kTextBlob_Type, paint);
    if (apf.shouldDraw()) {
        this->SkNWayCanvas::onDrawTextBlob(blob, x, y, *apf.paint());
    }
}

void SkPaintFilterCanvas::onDrawAtlas(const SkImage* image, const SkRSXform xform[],
                                      const SkRect tex[], const SkColor colors[], int count,
                                      SkBlendMode bmode, const SkRect* cull, const SkPaint* paint) {
    AutoPaintFilter apf(this, kBitmap_Type, paint);
    if (apf.shouldDraw()) {
        this->SkNWayCanvas::onDrawAtlas(image, xform, tex, colors, count, bmode, cull, apf.paint());
    }
}

void SkPaintFilterCanvas::onDrawAnnotation(const SkRect& rect, const char key[], SkData* value) {
    this->SkNWayCanvas::onDrawAnnotation(rect, key, value);
}

void SkPaintFilterCanvas::onDrawShadowRec(const SkPath& path, const SkDrawShadowRec& rec) {
    this->SkNWayCanvas::onDrawShadowRec(path, rec);
}

sk_sp<SkSurface> SkPaintFilterCanvas::onNewSurface(const SkImageInfo& info,
                                                   const SkSurfaceProps& props) {
    return proxy()->makeSurface(info, &props);
}

bool SkPaintFilterCanvas::onPeekPixels(SkPixmap* pixmap) {
    return proxy()->peekPixels(pixmap);
}

bool SkPaintFilterCanvas::onAccessTopLayerPixels(SkPixmap* pixmap) {
    SkImageInfo info;
    size_t rowBytes;

    void* addr = proxy()->accessTopLayerPixels(&info, &rowBytes);
    if (!addr) {
        return false;
    }

    pixmap->reset(info, addr, rowBytes);
    return true;
}

SkImageInfo SkPaintFilterCanvas::onImageInfo() const {
    return proxy()->imageInfo();
}

bool SkPaintFilterCanvas::onGetProps(SkSurfaceProps* props) const {
    return proxy()->getProps(props);
}
