/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkPixmap.h"

#include "SkBitmap.h"
#include "SkCanvas.h"
#include "SkColorData.h"
#include "SkConvertPixels.h"
#include "SkData.h"
#include "SkHalf.h"
#include "SkImageInfoPriv.h"
#include "SkImageShader.h"
#include "SkMask.h"
#include "SkNx.h"
#include "SkPixmapPriv.h"
#include "SkPM4f.h"
#include "SkReadPixelsRec.h"
#include "SkSurface.h"
#include "SkTemplates.h"
#include "SkTo.h"
#include "SkUnPreMultiply.h"
#include "SkUtils.h"

#include <utility>

/////////////////////////////////////////////////////////////////////////////////////////////////

void SkPixmap::reset() {
    fPixels = nullptr;
    fRowBytes = 0;
    fInfo = SkImageInfo::MakeUnknown();
}

void SkPixmap::reset(const SkImageInfo& info, const void* addr, size_t rowBytes) {
    if (addr) {
        SkASSERT(info.validRowBytes(rowBytes));
    }
    fPixels = addr;
    fRowBytes = rowBytes;
    fInfo = info;
}

bool SkPixmap::reset(const SkMask& src) {
    if (SkMask::kA8_Format == src.fFormat) {
        this->reset(SkImageInfo::MakeA8(src.fBounds.width(), src.fBounds.height()),
                    src.fImage, src.fRowBytes);
        return true;
    }
    this->reset();
    return false;
}

void SkPixmap::setColorSpace(sk_sp<SkColorSpace> cs) {
    fInfo = fInfo.makeColorSpace(std::move(cs));
}

bool SkPixmap::extractSubset(SkPixmap* result, const SkIRect& subset) const {
    SkIRect srcRect, r;
    srcRect.set(0, 0, this->width(), this->height());
    if (!r.intersect(srcRect, subset)) {
        return false;   // r is empty (i.e. no intersection)
    }

    // If the upper left of the rectangle was outside the bounds of this SkBitmap, we should have
    // exited above.
    SkASSERT(static_cast<unsigned>(r.fLeft) < static_cast<unsigned>(this->width()));
    SkASSERT(static_cast<unsigned>(r.fTop) < static_cast<unsigned>(this->height()));

    const void* pixels = nullptr;
    if (fPixels) {
        const size_t bpp = fInfo.bytesPerPixel();
        pixels = (const uint8_t*)fPixels + r.fTop * fRowBytes + r.fLeft * bpp;
    }
    result->reset(fInfo.makeWH(r.width(), r.height()), pixels, fRowBytes);
    return true;
}

// This is the same as SkPixmap::addr(x,y), but this version gets inlined, while the public
// method does not. Perhaps we could bloat it so it can be inlined, but that would grow code-size
// everywhere, instead of just here (on behalf of getAlphaf()).
static const void* fast_getaddr(const SkPixmap& pm, int x, int y) {
    x <<= SkColorTypeShiftPerPixel(pm.colorType());
    return static_cast<const char*>(pm.addr()) + y * pm.rowBytes() + x;
}

float SkPixmap::getAlphaf(int x, int y) const {
    SkASSERT(this->addr());
    SkASSERT((unsigned)x < (unsigned)this->width());
    SkASSERT((unsigned)y < (unsigned)this->height());

    float value = 0;
    const void* srcPtr = fast_getaddr(*this, x, y);

    switch (this->colorType()) {
        case kUnknown_SkColorType:
            return 0;
        case kGray_8_SkColorType:
        case kRGB_565_SkColorType:
        case kRGB_888x_SkColorType:
        case kRGB_101010x_SkColorType:
            return 1;
        case kAlpha_8_SkColorType:
            value = static_cast<const uint8_t*>(srcPtr)[0] * (1.0f/255);
            break;
        case kARGB_4444_SkColorType: {
            uint16_t u16 = static_cast<const uint16_t*>(srcPtr)[0];
            value = SkGetPackedA4444(u16) * (1.0f/15);
        } break;
        case kRGBA_8888_SkColorType:
        case kBGRA_8888_SkColorType:
            value = static_cast<const uint8_t*>(srcPtr)[3] * (1.0f/255);
            break;
        case kRGBA_1010102_SkColorType: {
            uint32_t u32 = static_cast<const uint32_t*>(srcPtr)[0];
            value = (u32 >> 30) * (1.0f/3);
        } break;
        case kRGBA_F16_SkColorType: {
            uint64_t px;
            memcpy(&px, srcPtr, sizeof(px));
            value = SkHalfToFloat_finite_ftz(px)[3];
        } break;
        case kRGBA_F32_SkColorType:
            value = static_cast<const float*>(srcPtr)[3];
            break;
    }
    return value;
}

bool SkPixmap::readPixels(const SkImageInfo& dstInfo, void* dstPixels, size_t dstRB,
                          int x, int y) const {
    if (!SkImageInfoValidConversion(dstInfo, fInfo)) {
        return false;
    }

    SkReadPixelsRec rec(dstInfo, dstPixels, dstRB, x, y);
    if (!rec.trim(fInfo.width(), fInfo.height())) {
        return false;
    }

    const void* srcPixels = this->addr(rec.fX, rec.fY);
    const SkImageInfo srcInfo = fInfo.makeWH(rec.fInfo.width(), rec.fInfo.height());
    SkConvertPixels(rec.fInfo, rec.fPixels, rec.fRowBytes, srcInfo, srcPixels, this->rowBytes());
    return true;
}

static uint16_t pack_8888_to_4444(unsigned a, unsigned r, unsigned g, unsigned b) {
    unsigned pixel = (SkA32To4444(a) << SK_A4444_SHIFT) |
    (SkR32To4444(r) << SK_R4444_SHIFT) |
    (SkG32To4444(g) << SK_G4444_SHIFT) |
    (SkB32To4444(b) << SK_B4444_SHIFT);
    return SkToU16(pixel);
}

bool SkPixmap::erase(SkColor color, const SkIRect& inArea) const {
    if (nullptr == fPixels) {
        return false;
    }
    SkIRect area;
    if (!area.intersect(this->bounds(), inArea)) {
        return false;
    }

    U8CPU a = SkColorGetA(color);
    U8CPU r = SkColorGetR(color);
    U8CPU g = SkColorGetG(color);
    U8CPU b = SkColorGetB(color);

    int height = area.height();
    const int width = area.width();
    const int rowBytes = this->rowBytes();

    if (color == 0
          && width == this->rowBytesAsPixels()
          && inArea == this->bounds()) {
        // All formats represent SkColor(0) as byte 0.
        memset(this->writable_addr(), 0, (int64_t)height * rowBytes);
        return true;
    }

    switch (this->colorType()) {
        case kGray_8_SkColorType: {
            if (255 != a) {
                r = SkMulDiv255Round(r, a);
                g = SkMulDiv255Round(g, a);
                b = SkMulDiv255Round(b, a);
            }
            int gray = SkComputeLuminance(r, g, b);
            uint8_t* p = this->writable_addr8(area.fLeft, area.fTop);
            while (--height >= 0) {
                memset(p, gray, width);
                p += rowBytes;
            }
            break;
        }
        case kAlpha_8_SkColorType: {
            uint8_t* p = this->writable_addr8(area.fLeft, area.fTop);
            while (--height >= 0) {
                memset(p, a, width);
                p += rowBytes;
            }
            break;
        }

        case kARGB_4444_SkColorType:
        case kRGB_565_SkColorType: {
            uint16_t* p = this->writable_addr16(area.fLeft, area.fTop);
            uint16_t v;

            // make rgb premultiplied
            if (255 != a) {
                r = SkMulDiv255Round(r, a);
                g = SkMulDiv255Round(g, a);
                b = SkMulDiv255Round(b, a);
            }

            if (kARGB_4444_SkColorType == this->colorType()) {
                v = pack_8888_to_4444(a, r, g, b);
            } else {
                v = SkPackRGB16(r >> (8 - SK_R16_BITS),
                                g >> (8 - SK_G16_BITS),
                                b >> (8 - SK_B16_BITS));
            }
            while (--height >= 0) {
                sk_memset16(p, v, width);
                p = (uint16_t*)((char*)p + rowBytes);
            }
            break;
        }

        case kRGB_888x_SkColorType:
            a = 255; // then fallthrough to 8888
        case kRGBA_8888_SkColorType:
        case kBGRA_8888_SkColorType: {
            uint32_t* p = this->writable_addr32(area.fLeft, area.fTop);

            if (255 != a && kPremul_SkAlphaType == this->alphaType()) {
                r = SkMulDiv255Round(r, a);
                g = SkMulDiv255Round(g, a);
                b = SkMulDiv255Round(b, a);
            }
            uint32_t v = kBGRA_8888_SkColorType == this->colorType()
                             ? SkPackARGB_as_BGRA(a, r, g, b)   // bgra 8888
                             : SkPackARGB_as_RGBA(a, r, g, b);  // rgba 8888 or rgb 888

            while (--height >= 0) {
                sk_memset32(p, v, width);
                p = (uint32_t*)((char*)p + rowBytes);
            }
            break;
        }

        case kRGB_101010x_SkColorType:
            a = 255;  // then fallthrough to 1010102
        case kRGBA_1010102_SkColorType: {
            uint32_t* p = this->writable_addr32(area.fLeft, area.fTop);

            float R = r * (1/255.0f),
                  G = g * (1/255.0f),
                  B = b * (1/255.0f),
                  A = a * (1/255.0f);
            if (a != 255 && this->alphaType() == kPremul_SkAlphaType) {
                R *= A;
                G *= A;
                B *= A;
            }
            uint32_t v = (uint32_t)(R * 1023.0f) <<  0
                       | (uint32_t)(G * 1023.0f) << 10
                       | (uint32_t)(B * 1023.0f) << 20
                       | (uint32_t)(A *    3.0f) << 30;
            while (--height >= 0) {
                sk_memset32(p, v, width);
                p = (uint32_t*)((char*)p + rowBytes);
            }
            break;
        }

        case kRGBA_F16_SkColorType:
        case kRGBA_F32_SkColorType:
            // The colorspace is unspecified, so assume linear just like getColor().
            this->erase(SkColor4f{(1 / 255.0f) * r,
                                  (1 / 255.0f) * g,
                                  (1 / 255.0f) * b,
                                  (1 / 255.0f) * a}, &area);
            break;
        default:
            return false; // no change, so don't call notifyPixelsChanged()
    }
    return true;
}

bool SkPixmap::erase(const SkColor4f& origColor, const SkIRect* subset) const {
    SkPixmap pm;
    if (subset) {
        if (!this->extractSubset(&pm, *subset)) {
            return false;
        }
    } else {
        pm = *this;
    }

    const SkColor4f color = origColor.pin();

    if (pm.colorType() == kRGBA_F16_SkColorType) {
        uint64_t half4;
        SkFloatToHalf_finite_ftz(Sk4f::Load(color.premul().vec())).store(&half4);
        for (int y = 0; y < pm.height(); ++y) {
            sk_memset64(pm.writable_addr64(0, y), half4, pm.width());
        }
        return true;
    }

    if (pm.colorType() == kRGBA_F32_SkColorType) {
        const SkPMColor4f rgba = color.premul();
        for (int y = 0; y < pm.height(); ++y) {
            auto row = (float*)pm.writable_addr(0, y);
            for (int x = 0; x < pm.width(); ++x) {
                row[4*x+0] = rgba.fR;
                row[4*x+1] = rgba.fG;
                row[4*x+2] = rgba.fB;
                row[4*x+3] = rgba.fA;
            }
        }
        return true;
    }

    return pm.erase(color.toSkColor());
}

bool SkPixmap::scalePixels(const SkPixmap& actualDst, SkFilterQuality quality) const {
    // We may need to tweak how we interpret these just a little below, so we make copies.
    SkPixmap src = *this,
             dst = actualDst;

    // Can't do anthing with empty src or dst
    if (src.width() <= 0 || src.height() <= 0 ||
        dst.width() <= 0 || dst.height() <= 0) {
        return false;
    }

    // no scaling involved?
    if (src.width() == dst.width() && src.height() == dst.height()) {
        return src.readPixels(dst);
    }

    // If src and dst are both unpremul, we'll fake the source out to appear as if premul,
    // and mark the destination as opaque.  This odd combination allows us to scale unpremul
    // pixels without ever premultiplying them (perhaps losing information in the color channels).
    // This is an idiosyncratic feature of scalePixels(), and is tested by scalepixels_unpremul GM.
    bool clampAsIfUnpremul = false;
    if (src.alphaType() == kUnpremul_SkAlphaType &&
        dst.alphaType() == kUnpremul_SkAlphaType) {
        src.reset(src.info().makeAlphaType(kPremul_SkAlphaType), src.addr(), src.rowBytes());
        dst.reset(dst.info().makeAlphaType(kOpaque_SkAlphaType), dst.addr(), dst.rowBytes());

        // We'll need to tell the image shader to clamp to [0,1] instead of the
        // usual [0,a] when using a bicubic scaling (kHigh_SkFilterQuality).
        clampAsIfUnpremul = true;
    }

    SkBitmap bitmap;
    if (!bitmap.installPixels(src)) {
        return false;
    }
    bitmap.setImmutable();        // Don't copy when we create an image.
    bitmap.setIsVolatile(true);   // Disable any caching.

    SkMatrix scale = SkMatrix::MakeRectToRect(SkRect::Make(src.bounds()),
                                              SkRect::Make(dst.bounds()),
                                              SkMatrix::kFill_ScaleToFit);

    // We'll create a shader to do this draw so we have control over the bicubic clamp.
    sk_sp<SkShader> shader = SkImageShader::Make(SkImage::MakeFromBitmap(bitmap),
                                                 SkShader::kClamp_TileMode,
                                                 SkShader::kClamp_TileMode,
                                                 &scale,
                                                 clampAsIfUnpremul);

    sk_sp<SkSurface> surface = SkSurface::MakeRasterDirect(dst.info(),
                                                           dst.writable_addr(),
                                                           dst.rowBytes());
    if (!shader || !surface) {
        return false;
    }

    SkPaint paint;
    paint.setBlendMode(SkBlendMode::kSrc);
    paint.setFilterQuality(quality);
    paint.setShader(std::move(shader));
    surface->getCanvas()->drawPaint(paint);
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

SkColor SkPixmap::getColor(int x, int y) const {
    SkASSERT(this->addr());
    SkASSERT((unsigned)x < (unsigned)this->width());
    SkASSERT((unsigned)y < (unsigned)this->height());

    const bool needsUnpremul = (kPremul_SkAlphaType == fInfo.alphaType());
    auto toColor = [needsUnpremul](uint32_t maybePremulColor) {
        return needsUnpremul ? SkUnPreMultiply::PMColorToColor(maybePremulColor)
                             : SkSwizzle_BGRA_to_PMColor(maybePremulColor);
    };

    switch (this->colorType()) {
        case kGray_8_SkColorType: {
            uint8_t value = *this->addr8(x, y);
            return SkColorSetRGB(value, value, value);
        }
        case kAlpha_8_SkColorType: {
            return SkColorSetA(0, *this->addr8(x, y));
        }
        case kRGB_565_SkColorType: {
            return SkPixel16ToColor(*this->addr16(x, y));
        }
        case kARGB_4444_SkColorType: {
            uint16_t value = *this->addr16(x, y);
            SkPMColor c = SkPixel4444ToPixel32(value);
            return toColor(c);
        }
        case kRGB_888x_SkColorType: {
            uint32_t value = *this->addr32(x, y);
            return SkSwizzle_RB(value | 0xff000000);
        }
        case kBGRA_8888_SkColorType: {
            uint32_t value = *this->addr32(x, y);
            SkPMColor c = SkSwizzle_BGRA_to_PMColor(value);
            return toColor(c);
        }
        case kRGBA_8888_SkColorType: {
            uint32_t value = *this->addr32(x, y);
            SkPMColor c = SkSwizzle_RGBA_to_PMColor(value);
            return toColor(c);
        }
        case kRGB_101010x_SkColorType: {
            uint32_t value = *this->addr32(x, y);
            // Convert 10-bit rgb to 8-bit bgr, and mask in 0xff alpha at the top.
            return (uint32_t)( ((value >>  0) & 0x3ff) * (255/1023.0f) ) << 16
                 | (uint32_t)( ((value >> 10) & 0x3ff) * (255/1023.0f) ) <<  8
                 | (uint32_t)( ((value >> 20) & 0x3ff) * (255/1023.0f) ) <<  0
                 | 0xff000000;
        }
        case kRGBA_1010102_SkColorType: {
            uint32_t value = *this->addr32(x, y);

            float r = ((value >>  0) & 0x3ff) * (1/1023.0f),
                  g = ((value >> 10) & 0x3ff) * (1/1023.0f),
                  b = ((value >> 20) & 0x3ff) * (1/1023.0f),
                  a = ((value >> 30) & 0x3  ) * (1/   3.0f);
            if (a != 0 && needsUnpremul) {
                r *= (1.0f/a);
                g *= (1.0f/a);
                b *= (1.0f/a);
            }
            return (uint32_t)( r * 255.0f ) << 16
                 | (uint32_t)( g * 255.0f ) <<  8
                 | (uint32_t)( b * 255.0f ) <<  0
                 | (uint32_t)( a * 255.0f ) << 24;
        }
        case kRGBA_F16_SkColorType: {
            const uint64_t* addr =
                (const uint64_t*)fPixels + y * (fRowBytes >> 3) + x;
            Sk4f p4 = SkHalfToFloat_finite_ftz(*addr);
            if (p4[3] && needsUnpremul) {
                float inva = 1 / p4[3];
                p4 = p4 * Sk4f(inva, inva, inva, 1);
            }
            SkColor c;
            SkNx_cast<uint8_t>(p4 * Sk4f(255) + Sk4f(0.5f)).store(&c);
            // p4 is RGBA, but we want BGRA, so we need to swap next
            return SkSwizzle_RB(c);
        }
        case kRGBA_F32_SkColorType: {
            const float* rgba =
                (const float*)fPixels + 4*y*(fRowBytes >> 4) + 4*x;
            Sk4f p4 = Sk4f::Load(rgba);
            // From here on, just like F16:
            if (p4[3] && needsUnpremul) {
                float inva = 1 / p4[3];
                p4 = p4 * Sk4f(inva, inva, inva, 1);
            }
            SkColor c;
            SkNx_cast<uint8_t>(p4 * Sk4f(255) + Sk4f(0.5f)).store(&c);
            // p4 is RGBA, but we want BGRA, so we need to swap next
            return SkSwizzle_RB(c);
        }
        default:
            SkDEBUGFAIL("");
            return SkColorSetARGB(0, 0, 0, 0);
    }
}

bool SkPixmap::computeIsOpaque() const {
    const int height = this->height();
    const int width = this->width();

    switch (this->colorType()) {
        case kAlpha_8_SkColorType: {
            unsigned a = 0xFF;
            for (int y = 0; y < height; ++y) {
                const uint8_t* row = this->addr8(0, y);
                for (int x = 0; x < width; ++x) {
                    a &= row[x];
                }
                if (0xFF != a) {
                    return false;
                }
            }
            return true;
        } break;
        case kRGB_565_SkColorType:
        case kGray_8_SkColorType:
            return true;
            break;
        case kARGB_4444_SkColorType: {
            unsigned c = 0xFFFF;
            for (int y = 0; y < height; ++y) {
                const SkPMColor16* row = this->addr16(0, y);
                for (int x = 0; x < width; ++x) {
                    c &= row[x];
                }
                if (0xF != SkGetPackedA4444(c)) {
                    return false;
                }
            }
            return true;
        } break;
        case kBGRA_8888_SkColorType:
        case kRGBA_8888_SkColorType: {
            SkPMColor c = (SkPMColor)~0;
            for (int y = 0; y < height; ++y) {
                const SkPMColor* row = this->addr32(0, y);
                for (int x = 0; x < width; ++x) {
                    c &= row[x];
                }
                if (0xFF != SkGetPackedA32(c)) {
                    return false;
                }
            }
            return true;
        }
        case kRGBA_F16_SkColorType: {
            const SkHalf* row = (const SkHalf*)this->addr();
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    if (row[4 * x + 3] < SK_Half1) {
                        return false;
                    }
                }
                row += this->rowBytes() >> 1;
            }
            return true;
        }
        default:
            break;
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

static bool draw_orientation(const SkPixmap& dst, const SkPixmap& src, unsigned flags) {
    auto surf = SkSurface::MakeRasterDirect(dst.info(), dst.writable_addr(), dst.rowBytes());
    if (!surf) {
        return false;
    }

    SkBitmap bm;
    bm.installPixels(src);

    SkMatrix m;
    m.setIdentity();

    SkScalar W = SkIntToScalar(src.width());
    SkScalar H = SkIntToScalar(src.height());
    if (flags & SkPixmapPriv::kSwapXY) {
        SkMatrix s;
        s.setAll(0, 1, 0, 1, 0, 0, 0, 0, 1);
        m.postConcat(s);
        using std::swap;
        swap(W, H);
    }
    if (flags & SkPixmapPriv::kMirrorX) {
        m.postScale(-1, 1);
        m.postTranslate(W, 0);
    }
    if (flags & SkPixmapPriv::kMirrorY) {
        m.postScale(1, -1);
        m.postTranslate(0, H);
    }
    SkPaint p;
    p.setBlendMode(SkBlendMode::kSrc);
    surf->getCanvas()->concat(m);
    surf->getCanvas()->drawBitmap(bm, 0, 0, &p);
    return true;
}

bool SkPixmapPriv::Orient(const SkPixmap& dst, const SkPixmap& src, OrientFlags flags) {
    SkASSERT((flags & ~(kMirrorX | kMirrorY | kSwapXY)) == 0);
    if (src.colorType() != dst.colorType()) {
        return false;
    }
    // note: we just ignore alphaType and colorSpace for this transformation

    int w = src.width();
    int h = src.height();
    if (flags & kSwapXY) {
        using std::swap;
        swap(w, h);
    }
    if (dst.width() != w || dst.height() != h) {
        return false;
    }
    if (w == 0 || h == 0) {
        return true;
    }

    // check for aliasing to self
    if (src.addr() == dst.addr()) {
        return flags == 0;
    }
    return draw_orientation(dst, src, flags);
}

#define kMirrorX    SkPixmapPriv::kMirrorX
#define kMirrorY    SkPixmapPriv::kMirrorY
#define kSwapXY     SkPixmapPriv::kSwapXY

static constexpr uint8_t gOrientationFlags[] = {
    0,                              // kTopLeft_SkEncodedOrigin
    kMirrorX,                       // kTopRight_SkEncodedOrigin
    kMirrorX | kMirrorY,            // kBottomRight_SkEncodedOrigin
               kMirrorY,            // kBottomLeft_SkEncodedOrigin
                          kSwapXY,  // kLeftTop_SkEncodedOrigin
    kMirrorX            | kSwapXY,  // kRightTop_SkEncodedOrigin
    kMirrorX | kMirrorY | kSwapXY,  // kRightBottom_SkEncodedOrigin
               kMirrorY | kSwapXY,  // kLeftBottom_SkEncodedOrigin
};

SkPixmapPriv::OrientFlags SkPixmapPriv::OriginToOrient(SkEncodedOrigin o) {
    unsigned io = static_cast<int>(o) - 1;
    SkASSERT(io < SK_ARRAY_COUNT(gOrientationFlags));
    return static_cast<SkPixmapPriv::OrientFlags>(gOrientationFlags[io]);
}

bool SkPixmapPriv::ShouldSwapWidthHeight(SkEncodedOrigin o) {
    return SkToBool(OriginToOrient(o) & kSwapXY);
}

SkImageInfo SkPixmapPriv::SwapWidthHeight(const SkImageInfo& info) {
    return info.makeWH(info.height(), info.width());
}

