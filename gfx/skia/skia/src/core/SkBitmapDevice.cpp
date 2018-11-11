/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkBitmapDevice.h"
#include "SkDraw.h"
#include "SkGlyphRun.h"
#include "SkImageFilter.h"
#include "SkImageFilterCache.h"
#include "SkMallocPixelRef.h"
#include "SkMakeUnique.h"
#include "SkMatrix.h"
#include "SkPaint.h"
#include "SkPath.h"
#include "SkPixelRef.h"
#include "SkPixmap.h"
#include "SkRasterClip.h"
#include "SkRasterHandleAllocator.h"
#include "SkShader.h"
#include "SkSpecialImage.h"
#include "SkSurface.h"
#include "SkTLazy.h"
#include "SkVertices.h"

struct Bounder {
    SkRect  fBounds;
    bool    fHasBounds;

    Bounder(const SkRect& r, const SkPaint& paint) {
        if ((fHasBounds = paint.canComputeFastBounds())) {
            fBounds = paint.computeFastBounds(r, &fBounds);
        }
    }

    bool hasBounds() const { return fHasBounds; }
    const SkRect* bounds() const { return fHasBounds ? &fBounds : nullptr; }
    operator const SkRect* () const { return this->bounds(); }
};

class SkDrawTiler {
    enum {
        // 8K is 1 too big, since 8K << supersample == 32768 which is too big for SkFixed
        kMaxDim = 8192 - 1
    };

    SkBitmapDevice* fDevice;
    SkPixmap        fRootPixmap;
    SkIRect         fSrcBounds;

    // Used for tiling and non-tiling
    SkDraw          fDraw;

    // fCurr... are only used if fNeedTiling
    SkMatrix        fTileMatrix;
    SkRasterClip    fTileRC;
    SkIPoint        fOrigin;

    bool            fDone, fNeedsTiling;

public:
    static bool NeedsTiling(SkBitmapDevice* dev) {
        return dev->width() > kMaxDim || dev->height() > kMaxDim;
    }

    SkDrawTiler(SkBitmapDevice* dev, const SkRect* bounds) : fDevice(dev) {
        fDone = false;

        // we need fDst to be set, and if we're actually drawing, to dirty the genID
        if (!dev->accessPixels(&fRootPixmap)) {
            // NoDrawDevice uses us (why?) so we have to catch this case w/ no pixels
            fRootPixmap.reset(dev->imageInfo(), nullptr, 0);
        }

        // do a quick check, so we don't even have to process "bounds" if there is no need
        const SkIRect clipR = dev->fRCStack.rc().getBounds();
        fNeedsTiling = clipR.right() > kMaxDim || clipR.bottom() > kMaxDim;
        if (fNeedsTiling) {
            if (bounds) {
                SkRect devBounds;
                dev->ctm().mapRect(&devBounds, *bounds);
                if (devBounds.intersect(SkRect::Make(clipR))) {
                    fSrcBounds = devBounds.roundOut();
                    // Check again, now that we have computed srcbounds.
                    fNeedsTiling = fSrcBounds.right() > kMaxDim || fSrcBounds.bottom() > kMaxDim;
                } else {
                    fNeedsTiling = false;
                    fDone = true;
                }
            } else {
                fSrcBounds = clipR;
            }
        }

        if (fNeedsTiling) {
            // fDraw.fDst is reset each time in setupTileDraw()
            fDraw.fMatrix = &fTileMatrix;
            fDraw.fRC = &fTileRC;
            // we'll step/increase it before using it
            fOrigin.set(fSrcBounds.fLeft - kMaxDim, fSrcBounds.fTop);
        } else {
            // don't reference fSrcBounds, as it may not have been set
            fDraw.fDst = fRootPixmap;
            fDraw.fMatrix = &dev->ctm();
            fDraw.fRC = &dev->fRCStack.rc();
            fOrigin.set(0, 0);

            fDraw.fCoverage = dev->accessCoverage();
        }
    }

    bool needsTiling() const { return fNeedsTiling; }

    const SkDraw* next() {
        if (fDone) {
            return nullptr;
        }
        if (fNeedsTiling) {
            do {
                this->stepAndSetupTileDraw();  // might set the clip to empty and fDone to true
            } while (!fDone && fTileRC.isEmpty());
            // if we exit the loop and we're still empty, we're (past) done
            if (fTileRC.isEmpty()) {
                SkASSERT(fDone);
                return nullptr;
            }
            SkASSERT(!fTileRC.isEmpty());
        } else {
            fDone = true;   // only draw untiled once
        }
        return &fDraw;
    }

private:
    void stepAndSetupTileDraw() {
        SkASSERT(!fDone);
        SkASSERT(fNeedsTiling);

        // We do fRootPixmap.width() - kMaxDim instead of fOrigin.fX + kMaxDim to avoid overflow.
        if (fOrigin.fX >= fSrcBounds.fRight - kMaxDim) {    // too far
            fOrigin.fX = fSrcBounds.fLeft;
            fOrigin.fY += kMaxDim;
        } else {
            fOrigin.fX += kMaxDim;
        }
        // fDone = next origin will be invalid.
        fDone = fOrigin.fX >= fSrcBounds.fRight - kMaxDim &&
                fOrigin.fY >= fSrcBounds.fBottom - kMaxDim;

        SkIRect bounds = SkIRect::MakeXYWH(fOrigin.x(), fOrigin.y(), kMaxDim, kMaxDim);
        SkASSERT(!bounds.isEmpty());
        bool success = fRootPixmap.extractSubset(&fDraw.fDst, bounds);
        SkASSERT_RELEASE(success);
        // now don't use bounds, since fDst has the clipped dimensions.

        fTileMatrix = fDevice->ctm();
        fTileMatrix.postTranslate(SkIntToScalar(-fOrigin.x()), SkIntToScalar(-fOrigin.y()));
        fDevice->fRCStack.rc().translate(-fOrigin.x(), -fOrigin.y(), &fTileRC);
        fTileRC.op(SkIRect::MakeWH(fDraw.fDst.width(), fDraw.fDst.height()),
                   SkRegion::kIntersect_Op);
    }
};

// Passing a bounds allows the tiler to only visit the dst-tiles that might intersect the
// drawing. If null is passed, the tiler has to visit everywhere. The bounds is expected to be
// in local coordinates, as the tiler itself will transform that into device coordinates.
//
#define LOOP_TILER(code, boundsPtr)                         \
    SkDrawTiler priv_tiler(this, boundsPtr);                \
    while (const SkDraw* priv_draw = priv_tiler.next()) {   \
        priv_draw->code;                                    \
    }

// Helper to create an SkDraw from a device
class SkBitmapDevice::BDDraw : public SkDraw {
public:
    BDDraw(SkBitmapDevice* dev) {
        // we need fDst to be set, and if we're actually drawing, to dirty the genID
        if (!dev->accessPixels(&fDst)) {
            // NoDrawDevice uses us (why?) so we have to catch this case w/ no pixels
            fDst.reset(dev->imageInfo(), nullptr, 0);
        }
        fMatrix = &dev->ctm();
        fRC = &dev->fRCStack.rc();
        fCoverage = dev->accessCoverage();
    }
};

static bool valid_for_bitmap_device(const SkImageInfo& info,
                                    SkAlphaType* newAlphaType) {
    if (info.width() < 0 || info.height() < 0) {
        return false;
    }

    // TODO: can we stop supporting kUnknown in SkBitmkapDevice?
    if (kUnknown_SkColorType == info.colorType()) {
        if (newAlphaType) {
            *newAlphaType = kUnknown_SkAlphaType;
        }
        return true;
    }

    SkAlphaType canonicalAlphaType = info.alphaType();

    switch (info.colorType()) {
        case kAlpha_8_SkColorType:
        case kARGB_4444_SkColorType:
        case kRGBA_8888_SkColorType:
        case kBGRA_8888_SkColorType:
        case kRGBA_1010102_SkColorType:
        case kRGBA_F16_SkColorType:
        case kRGBA_F32_SkColorType:
            break;
        case kGray_8_SkColorType:
        case kRGB_565_SkColorType:
        case kRGB_888x_SkColorType:
        case kRGB_101010x_SkColorType:
            canonicalAlphaType = kOpaque_SkAlphaType;
            break;
        default:
            return false;
    }

    if (newAlphaType) {
        *newAlphaType = canonicalAlphaType;
    }
    return true;
}

// TODO: unify this with the same functionality on SkDraw.
static SkScalerContextFlags scaler_context_flags(const SkBitmap& bitmap)  {
    // If we're doing linear blending, then we can disable the gamma hacks.
    // Otherwise, leave them on. In either case, we still want the contrast boost:
    // TODO: Can we be even smarter about mask gamma based on the dst transfer function?
    if (bitmap.colorSpace() && bitmap.colorSpace()->gammaIsLinear()) {
        return SkScalerContextFlags::kBoostContrast;
    } else {
        return SkScalerContextFlags::kFakeGammaAndBoostContrast;
    }
}

SkBitmapDevice::SkBitmapDevice(const SkBitmap& bitmap)
    : INHERITED(bitmap.info(), SkSurfaceProps(SkSurfaceProps::kLegacyFontHost_InitType))
    , fBitmap(bitmap)
    , fRCStack(bitmap.width(), bitmap.height())
    , fGlyphPainter(this->surfaceProps(), bitmap.colorType(), scaler_context_flags(bitmap))
{
    SkASSERT(valid_for_bitmap_device(bitmap.info(), nullptr));
}

SkBitmapDevice* SkBitmapDevice::Create(const SkImageInfo& info) {
    return Create(info, SkSurfaceProps(SkSurfaceProps::kLegacyFontHost_InitType));
}

SkBitmapDevice::SkBitmapDevice(const SkBitmap& bitmap, const SkSurfaceProps& surfaceProps,
                               SkRasterHandleAllocator::Handle hndl, const SkBitmap* coverage)
    : INHERITED(bitmap.info(), surfaceProps)
    , fBitmap(bitmap)
    , fRasterHandle(hndl)
    , fRCStack(bitmap.width(), bitmap.height())
    , fGlyphPainter(this->surfaceProps(), bitmap.colorType(), scaler_context_flags(bitmap))
{
    SkASSERT(valid_for_bitmap_device(bitmap.info(), nullptr));

    if (coverage) {
        SkASSERT(coverage->width() == bitmap.width());
        SkASSERT(coverage->height() == bitmap.height());
        fCoverage = skstd::make_unique<SkBitmap>(*coverage);
    }
}

SkBitmapDevice* SkBitmapDevice::Create(const SkImageInfo& origInfo,
                                       const SkSurfaceProps& surfaceProps,
                                       bool trackCoverage,
                                       SkRasterHandleAllocator* allocator) {
    SkAlphaType newAT = origInfo.alphaType();
    if (!valid_for_bitmap_device(origInfo, &newAT)) {
        return nullptr;
    }

    SkRasterHandleAllocator::Handle hndl = nullptr;
    const SkImageInfo info = origInfo.makeAlphaType(newAT);
    SkBitmap bitmap;

    if (kUnknown_SkColorType == info.colorType()) {
        if (!bitmap.setInfo(info)) {
            return nullptr;
        }
    } else if (allocator) {
        hndl = allocator->allocBitmap(info, &bitmap);
        if (!hndl) {
            return nullptr;
        }
    } else if (info.isOpaque()) {
        // If this bitmap is opaque, we don't have any sensible default color,
        // so we just return uninitialized pixels.
        if (!bitmap.tryAllocPixels(info)) {
            return nullptr;
        }
    } else {
        // This bitmap has transparency, so we'll zero the pixels (to transparent).
        // We use the flag as a faster alloc-then-eraseColor(SK_ColorTRANSPARENT).
        if (!bitmap.tryAllocPixelsFlags(info, SkBitmap::kZeroPixels_AllocFlag)) {
            return nullptr;
        }
    }

    SkBitmap coverage;
    if (trackCoverage) {
        SkImageInfo ci = SkImageInfo::Make(info.width(), info.height(), kAlpha_8_SkColorType,
                                           kPremul_SkAlphaType);
        if (!coverage.tryAllocPixelsFlags(ci, SkBitmap::kZeroPixels_AllocFlag)) {
            return nullptr;
        }
    }

    return new SkBitmapDevice(bitmap, surfaceProps, hndl, trackCoverage ? &coverage : nullptr);
}

void SkBitmapDevice::replaceBitmapBackendForRasterSurface(const SkBitmap& bm) {
    SkASSERT(bm.width() == fBitmap.width());
    SkASSERT(bm.height() == fBitmap.height());
    fBitmap = bm;   // intent is to use bm's pixelRef (and rowbytes/config)
    this->privateResize(fBitmap.info().width(), fBitmap.info().height());
}

SkBaseDevice* SkBitmapDevice::onCreateDevice(const CreateInfo& cinfo, const SkPaint*) {
    const SkSurfaceProps surfaceProps(this->surfaceProps().flags(), cinfo.fPixelGeometry);
    return SkBitmapDevice::Create(cinfo.fInfo, surfaceProps, cinfo.fTrackCoverage,
                                  cinfo.fAllocator);
}

bool SkBitmapDevice::onAccessPixels(SkPixmap* pmap) {
    if (this->onPeekPixels(pmap)) {
        fBitmap.notifyPixelsChanged();
        return true;
    }
    return false;
}

bool SkBitmapDevice::onPeekPixels(SkPixmap* pmap) {
    const SkImageInfo info = fBitmap.info();
    if (fBitmap.getPixels() && (kUnknown_SkColorType != info.colorType())) {
        pmap->reset(fBitmap.info(), fBitmap.getPixels(), fBitmap.rowBytes());
        return true;
    }
    return false;
}

bool SkBitmapDevice::onWritePixels(const SkPixmap& pm, int x, int y) {
    // since we don't stop creating un-pixeled devices yet, check for no pixels here
    if (nullptr == fBitmap.getPixels()) {
        return false;
    }

    if (fBitmap.writePixels(pm, x, y)) {
        fBitmap.notifyPixelsChanged();
        return true;
    }
    return false;
}

bool SkBitmapDevice::onReadPixels(const SkPixmap& pm, int x, int y) {
    return fBitmap.readPixels(pm, x, y);
}

///////////////////////////////////////////////////////////////////////////////

void SkBitmapDevice::drawPaint(const SkPaint& paint) {
    BDDraw(this).drawPaint(paint);
}

void SkBitmapDevice::drawPoints(SkCanvas::PointMode mode, size_t count,
                                const SkPoint pts[], const SkPaint& paint) {
    LOOP_TILER( drawPoints(mode, count, pts, paint, nullptr), nullptr)
}

void SkBitmapDevice::drawRect(const SkRect& r, const SkPaint& paint) {
    LOOP_TILER( drawRect(r, paint), Bounder(r, paint))
}

void SkBitmapDevice::drawOval(const SkRect& oval, const SkPaint& paint) {
    SkPath path;
    path.addOval(oval);
    // call the VIRTUAL version, so any subclasses who do handle drawPath aren't
    // required to override drawOval.
    this->drawPath(path, paint, true);
}

void SkBitmapDevice::drawRRect(const SkRRect& rrect, const SkPaint& paint) {
#ifdef SK_IGNORE_BLURRED_RRECT_OPT
    SkPath  path;

    path.addRRect(rrect);
    // call the VIRTUAL version, so any subclasses who do handle drawPath aren't
    // required to override drawRRect.
    this->drawPath(path, paint, true);
#else
    LOOP_TILER( drawRRect(rrect, paint), Bounder(rrect.getBounds(), paint))
#endif
}

void SkBitmapDevice::drawPath(const SkPath& path,
                              const SkPaint& paint,
                              bool pathIsMutable) {
    const SkRect* bounds = nullptr;
    if (SkDrawTiler::NeedsTiling(this) && !path.isInverseFillType()) {
        bounds = &path.getBounds();
    }
    SkDrawTiler tiler(this, bounds ? Bounder(*bounds, paint).bounds() : nullptr);
    if (tiler.needsTiling()) {
        pathIsMutable = false;
    }
    while (const SkDraw* draw = tiler.next()) {
        draw->drawPath(path, paint, nullptr, pathIsMutable);
    }
}

void SkBitmapDevice::drawBitmap(const SkBitmap& bitmap, SkScalar x, SkScalar y,
                                const SkPaint& paint) {
    SkMatrix matrix = SkMatrix::MakeTrans(x, y);
    LogDrawScaleFactor(SkMatrix::Concat(this->ctm(), matrix), paint.getFilterQuality());
    this->drawBitmap(bitmap, matrix, nullptr, paint);
}

void SkBitmapDevice::drawBitmap(const SkBitmap& bitmap, const SkMatrix& matrix,
                                const SkRect* dstOrNull, const SkPaint& paint) {
    const SkRect* bounds = dstOrNull;
    SkRect storage;
    if (!bounds && SkDrawTiler::NeedsTiling(this)) {
        matrix.mapRect(&storage, SkRect::MakeIWH(bitmap.width(), bitmap.height()));
        Bounder b(storage, paint);
        if (b.hasBounds()) {
            storage = *b.bounds();
            bounds = &storage;
        }
    }
    LOOP_TILER(drawBitmap(bitmap, matrix, dstOrNull, paint), bounds);
}

static inline bool CanApplyDstMatrixAsCTM(const SkMatrix& m, const SkPaint& paint) {
    if (!paint.getMaskFilter()) {
        return true;
    }

    // Some mask filters parameters (sigma) depend on the CTM/scale.
    return m.getType() <= SkMatrix::kTranslate_Mask;
}

void SkBitmapDevice::drawBitmapRect(const SkBitmap& bitmap,
                                    const SkRect* src, const SkRect& dst,
                                    const SkPaint& paint, SkCanvas::SrcRectConstraint constraint) {
    SkASSERT(dst.isFinite());
    SkASSERT(dst.isSorted());

    SkMatrix    matrix;
    SkRect      bitmapBounds, tmpSrc, tmpDst;
    SkBitmap    tmpBitmap;

    bitmapBounds.isetWH(bitmap.width(), bitmap.height());

    // Compute matrix from the two rectangles
    if (src) {
        tmpSrc = *src;
    } else {
        tmpSrc = bitmapBounds;
    }
    matrix.setRectToRect(tmpSrc, dst, SkMatrix::kFill_ScaleToFit);

    LogDrawScaleFactor(SkMatrix::Concat(this->ctm(), matrix), paint.getFilterQuality());

    const SkRect* dstPtr = &dst;
    const SkBitmap* bitmapPtr = &bitmap;

    // clip the tmpSrc to the bounds of the bitmap, and recompute dstRect if
    // needed (if the src was clipped). No check needed if src==null.
    if (src) {
        if (!bitmapBounds.contains(*src)) {
            if (!tmpSrc.intersect(bitmapBounds)) {
                return; // nothing to draw
            }
            // recompute dst, based on the smaller tmpSrc
            matrix.mapRect(&tmpDst, tmpSrc);
            if (!tmpDst.isFinite()) {
                return;
            }
            dstPtr = &tmpDst;
        }
    }

    if (src && !src->contains(bitmapBounds) &&
        SkCanvas::kFast_SrcRectConstraint == constraint &&
        paint.getFilterQuality() != kNone_SkFilterQuality) {
        // src is smaller than the bounds of the bitmap, and we are filtering, so we don't know
        // how much more of the bitmap we need, so we can't use extractSubset or drawBitmap,
        // but we must use a shader w/ dst bounds (which can access all of the bitmap needed).
        goto USE_SHADER;
    }

    if (src) {
        // since we may need to clamp to the borders of the src rect within
        // the bitmap, we extract a subset.
        const SkIRect srcIR = tmpSrc.roundOut();
        if (!bitmap.extractSubset(&tmpBitmap, srcIR)) {
            return;
        }
        bitmapPtr = &tmpBitmap;

        // Since we did an extract, we need to adjust the matrix accordingly
        SkScalar dx = 0, dy = 0;
        if (srcIR.fLeft > 0) {
            dx = SkIntToScalar(srcIR.fLeft);
        }
        if (srcIR.fTop > 0) {
            dy = SkIntToScalar(srcIR.fTop);
        }
        if (dx || dy) {
            matrix.preTranslate(dx, dy);
        }

#ifdef SK_DRAWBITMAPRECT_FAST_OFFSET
        SkRect extractedBitmapBounds = SkRect::MakeXYWH(dx, dy,
                                                        SkIntToScalar(bitmapPtr->width()),
                                                        SkIntToScalar(bitmapPtr->height()));
#else
        SkRect extractedBitmapBounds;
        extractedBitmapBounds.isetWH(bitmapPtr->width(), bitmapPtr->height());
#endif
        if (extractedBitmapBounds == tmpSrc) {
            // no fractional part in src, we can just call drawBitmap
            goto USE_DRAWBITMAP;
        }
    } else {
        USE_DRAWBITMAP:
        // We can go faster by just calling drawBitmap, which will concat the
        // matrix with the CTM, and try to call drawSprite if it can. If not,
        // it will make a shader and call drawRect, as we do below.
        if (CanApplyDstMatrixAsCTM(matrix, paint)) {
            this->drawBitmap(*bitmapPtr, matrix, dstPtr, paint);
            return;
        }
    }

    USE_SHADER:

    // TODO(herb): Move this over to SkArenaAlloc when arena alloc has a facility to return sk_sps.
    // Since the shader need only live for our stack-frame, pass in a custom allocator. This
    // can save malloc calls, and signals to SkMakeBitmapShader to not try to copy the bitmap
    // if its mutable, since that precaution is not needed (give the short lifetime of the shader).

    // construct a shader, so we can call drawRect with the dst
    auto s = SkMakeBitmapShader(*bitmapPtr, SkShader::kClamp_TileMode, SkShader::kClamp_TileMode,
                                &matrix, kNever_SkCopyPixelsMode);
    if (!s) {
        return;
    }

    SkPaint paintWithShader(paint);
    paintWithShader.setStyle(SkPaint::kFill_Style);
    paintWithShader.setShader(s);

    // Call ourself, in case the subclass wanted to share this setup code
    // but handle the drawRect code themselves.
    this->drawRect(*dstPtr, paintWithShader);
}

void SkBitmapDevice::drawSprite(const SkBitmap& bitmap, int x, int y, const SkPaint& paint) {
    BDDraw(this).drawSprite(bitmap, x, y, paint);
}

void SkBitmapDevice::drawGlyphRunList(const SkGlyphRunList& glyphRunList) {
#if defined(SK_SUPPORT_LEGACY_TEXT_BLOB)
    auto blob = glyphRunList.blob();

    if (blob == nullptr) {
        glyphRunList.temporaryShuntToDrawPosText(this, SkPoint::Make(0, 0));
    } else {
        auto origin = glyphRunList.origin();
        auto paint = glyphRunList.paint();
        this->drawTextBlob(blob, origin.x(), origin.y(), paint);
    }
#else
    LOOP_TILER( drawGlyphRunList(glyphRunList, &fGlyphPainter), nullptr )
#endif
}

void SkBitmapDevice::drawVertices(const SkVertices* vertices, const SkVertices::Bone bones[],
                                  int boneCount, SkBlendMode bmode, const SkPaint& paint) {
    BDDraw(this).drawVertices(vertices->mode(), vertices->vertexCount(), vertices->positions(),
                              vertices->texCoords(), vertices->colors(), vertices->boneIndices(),
                              vertices->boneWeights(), bmode, vertices->indices(),
                              vertices->indexCount(), paint, bones, boneCount);
}

void SkBitmapDevice::drawDevice(SkBaseDevice* device, int x, int y, const SkPaint& origPaint) {
    SkASSERT(!origPaint.getImageFilter());

    // todo: can we unify with similar adjustment in SkGpuDevice?
    SkTCopyOnFirstWrite<SkPaint> paint(origPaint);
    if (paint->getMaskFilter()) {
        paint.writable()->setMaskFilter(paint->getMaskFilter()->makeWithMatrix(this->ctm()));
    }

    // hack to test coverage
    SkBitmapDevice* src = static_cast<SkBitmapDevice*>(device);
    if (src->fCoverage) {
        SkDraw draw;
        draw.fDst = fBitmap.pixmap();
        draw.fMatrix = &SkMatrix::I();
        draw.fRC = &fRCStack.rc();
        SkPaint paint(origPaint);
        paint.setShader(SkShader::MakeBitmapShader(src->fBitmap, SkShader::kClamp_TileMode,
                                                   SkShader::kClamp_TileMode, nullptr));
        draw.drawBitmap(*src->fCoverage.get(),
                        SkMatrix::MakeTrans(SkIntToScalar(x),SkIntToScalar(y)), nullptr, paint);
    } else {
        this->drawSprite(src->fBitmap, x, y, *paint);
    }
}

///////////////////////////////////////////////////////////////////////////////

namespace {

class SkAutoDeviceClipRestore {
public:
    SkAutoDeviceClipRestore(SkBaseDevice* device, const SkIRect& clip)
        : fDevice(device)
        , fPrevCTM(device->ctm()) {
        fDevice->save();
        fDevice->setCTM(SkMatrix::I());
        fDevice->clipRect(SkRect::Make(clip), SkClipOp::kIntersect, false);
        fDevice->setCTM(fPrevCTM);
    }

    ~SkAutoDeviceClipRestore() {
        fDevice->restore(fPrevCTM);
    }

private:
    SkBaseDevice*  fDevice;
    const SkMatrix fPrevCTM;
};

}  // anonymous ns

void SkBitmapDevice::drawSpecial(SkSpecialImage* src, int x, int y, const SkPaint& origPaint,
                                 SkImage* clipImage, const SkMatrix& clipMatrix) {
    SkASSERT(!src->isTextureBacked());

    sk_sp<SkSpecialImage> filteredImage;
    SkTCopyOnFirstWrite<SkPaint> paint(origPaint);

    if (SkImageFilter* filter = paint->getImageFilter()) {
        SkIPoint offset = SkIPoint::Make(0, 0);
        const SkMatrix matrix = SkMatrix::Concat(
            SkMatrix::MakeTrans(SkIntToScalar(-x), SkIntToScalar(-y)), this->ctm());
        const SkIRect clipBounds = fRCStack.rc().getBounds().makeOffset(-x, -y);
        sk_sp<SkImageFilterCache> cache(this->getImageFilterCache());
        SkImageFilter::OutputProperties outputProperties(fBitmap.colorType(), fBitmap.colorSpace());
        SkImageFilter::Context ctx(matrix, clipBounds, cache.get(), outputProperties);

        filteredImage = filter->filterImage(src, ctx, &offset);
        if (!filteredImage) {
            return;
        }

        src = filteredImage.get();
        paint.writable()->setImageFilter(nullptr);
        x += offset.x();
        y += offset.y();
    }

    if (paint->getMaskFilter()) {
        paint.writable()->setMaskFilter(paint->getMaskFilter()->makeWithMatrix(this->ctm()));
    }

    if (!clipImage) {
        SkBitmap resultBM;
        if (src->getROPixels(&resultBM)) {
            this->drawSprite(resultBM, x, y, *paint);
        }
        return;
    }

    // Clip image case.
    sk_sp<SkImage> srcImage(src->asImage());
    if (!srcImage) {
        return;
    }

    const SkMatrix totalMatrix = SkMatrix::Concat(this->ctm(), clipMatrix);
    SkRect clipBounds;
    totalMatrix.mapRect(&clipBounds, SkRect::Make(clipImage->bounds()));
    const SkIRect srcBounds = srcImage->bounds().makeOffset(x, y);

    SkIRect maskBounds = fRCStack.rc().getBounds();
    if (!maskBounds.intersect(clipBounds.roundOut()) || !maskBounds.intersect(srcBounds)) {
        return;
    }

    sk_sp<SkImage> mask;
    SkMatrix maskMatrix, shaderMatrix;
    SkTLazy<SkAutoDeviceClipRestore> autoClipRestore;

    SkMatrix totalInverse;
    if (clipImage->isAlphaOnly() && totalMatrix.invert(&totalInverse)) {
        // If the mask is already in A8 format, we can draw it directly
        // (while compensating in the shader matrix).
        mask = sk_ref_sp(clipImage);
        maskMatrix = totalMatrix;
        shaderMatrix = SkMatrix::Concat(totalInverse, SkMatrix::MakeTrans(x, y));

        // If the mask is not fully contained within the src layer, we must clip.
        if (!srcBounds.contains(clipBounds)) {
            autoClipRestore.init(this, srcBounds);
        }

        maskBounds.offsetTo(0, 0);
    } else {
        // Otherwise, we convert the mask to A8 explicitly.
        sk_sp<SkSurface> surf = SkSurface::MakeRaster(SkImageInfo::MakeA8(maskBounds.width(),
                                                                          maskBounds.height()));
        SkCanvas* canvas = surf->getCanvas();
        canvas->translate(-maskBounds.x(), -maskBounds.y());
        canvas->concat(totalMatrix);
        canvas->drawImage(clipImage, 0, 0);

        mask = surf->makeImageSnapshot();
        maskMatrix = SkMatrix::I();
        shaderMatrix = SkMatrix::MakeTrans(x - maskBounds.x(), y - maskBounds.y());
    }

    SkAutoDeviceCTMRestore adctmr(this, maskMatrix);
    paint.writable()->setShader(srcImage->makeShader(&shaderMatrix));
    this->drawImage(mask.get(), maskBounds.x(), maskBounds.y(), *paint);
}

sk_sp<SkSpecialImage> SkBitmapDevice::makeSpecial(const SkBitmap& bitmap) {
    return SkSpecialImage::MakeFromRaster(bitmap.bounds(), bitmap);
}

sk_sp<SkSpecialImage> SkBitmapDevice::makeSpecial(const SkImage* image) {
    return SkSpecialImage::MakeFromImage(SkIRect::MakeWH(image->width(), image->height()),
                                         image->makeNonTextureImage(), fBitmap.colorSpace());
}

sk_sp<SkSpecialImage> SkBitmapDevice::snapSpecial() {
    return this->makeSpecial(fBitmap);
}

///////////////////////////////////////////////////////////////////////////////

sk_sp<SkSurface> SkBitmapDevice::makeSurface(const SkImageInfo& info, const SkSurfaceProps& props) {
    return SkSurface::MakeRaster(info, &props);
}

SkImageFilterCache* SkBitmapDevice::getImageFilterCache() {
    SkImageFilterCache* cache = SkImageFilterCache::Get();
    cache->ref();
    return cache;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void SkBitmapDevice::onSave() {
    fRCStack.save();
}

void SkBitmapDevice::onRestore() {
    fRCStack.restore();
}

void SkBitmapDevice::onClipRect(const SkRect& rect, SkClipOp op, bool aa) {
    fRCStack.clipRect(this->ctm(), rect, op, aa);
}

void SkBitmapDevice::onClipRRect(const SkRRect& rrect, SkClipOp op, bool aa) {
    fRCStack.clipRRect(this->ctm(), rrect, op, aa);
}

void SkBitmapDevice::onClipPath(const SkPath& path, SkClipOp op, bool aa) {
    fRCStack.clipPath(this->ctm(), path, op, aa);
}

void SkBitmapDevice::onClipRegion(const SkRegion& rgn, SkClipOp op) {
    SkIPoint origin = this->getOrigin();
    SkRegion tmp;
    const SkRegion* ptr = &rgn;
    if (origin.fX | origin.fY) {
        // translate from "global/canvas" coordinates to relative to this device
        rgn.translate(-origin.fX, -origin.fY, &tmp);
        ptr = &tmp;
    }
    fRCStack.clipRegion(*ptr, op);
}

void SkBitmapDevice::onSetDeviceClipRestriction(SkIRect* mutableClipRestriction) {
    fRCStack.setDeviceClipRestriction(mutableClipRestriction);
    if (!mutableClipRestriction->isEmpty()) {
        SkRegion rgn(*mutableClipRestriction);
        fRCStack.clipRegion(rgn, SkClipOp::kIntersect);
    }
}

bool SkBitmapDevice::onClipIsAA() const {
    const SkRasterClip& rc = fRCStack.rc();
    return !rc.isEmpty() && rc.isAA();
}

void SkBitmapDevice::onAsRgnClip(SkRegion* rgn) const {
    const SkRasterClip& rc = fRCStack.rc();
    if (rc.isAA()) {
        rgn->setRect(rc.getBounds());
    } else {
        *rgn = rc.bwRgn();
    }
}

void SkBitmapDevice::validateDevBounds(const SkIRect& drawClipBounds) {
#ifdef SK_DEBUG
    const SkIRect& stackBounds = fRCStack.rc().getBounds();
    SkASSERT(drawClipBounds == stackBounds);
#endif
}

SkBaseDevice::ClipType SkBitmapDevice::onGetClipType() const {
    const SkRasterClip& rc = fRCStack.rc();
    if (rc.isEmpty()) {
        return kEmpty_ClipType;
    } else if (rc.isRect()) {
        return kRect_ClipType;
    } else {
        return kComplex_ClipType;
    }
}
