/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkPictureShader.h"

#include "SkArenaAlloc.h"
#include "SkBitmap.h"
#include "SkBitmapProcShader.h"
#include "SkCanvas.h"
#include "SkColorSpaceXformCanvas.h"
#include "SkImage.h"
#include "SkImageShader.h"
#include "SkMatrixUtils.h"
#include "SkPicturePriv.h"
#include "SkPictureImageGenerator.h"
#include "SkReadBuffer.h"
#include "SkResourceCache.h"

#if SK_SUPPORT_GPU
#include "GrCaps.h"
#include "GrColorSpaceInfo.h"
#include "GrContext.h"
#include "GrContextPriv.h"
#include "GrFragmentProcessor.h"
#endif

namespace {
static unsigned gBitmapSkaderKeyNamespaceLabel;

struct BitmapShaderKey : public SkResourceCache::Key {
public:
    BitmapShaderKey(sk_sp<SkColorSpace> colorSpace,
                    uint32_t shaderID,
                    const SkRect& tile,
                    SkShader::TileMode tmx,
                    SkShader::TileMode tmy,
                    const SkSize& scale,
                    bool hasDstColorSpace)
        : fColorSpace(std::move(colorSpace))
        , fTile(tile)
        , fTmx(tmx)
        , fTmy(tmy)
        , fScale(scale)
        , fHasDstColorSpace(hasDstColorSpace ? 1 : 0) {

        static const size_t keySize = sizeof(fColorSpace) +
                                      sizeof(fTile) +
                                      sizeof(fTmx) + sizeof(fTmy) +
                                      sizeof(fScale) +
                                      sizeof(fHasDstColorSpace);
        // This better be packed.
        SkASSERT(sizeof(uint32_t) * (&fEndOfStruct - (uint32_t*)&fColorSpace) == keySize);
        this->init(&gBitmapSkaderKeyNamespaceLabel, MakeSharedID(shaderID), keySize);
    }

    static uint64_t MakeSharedID(uint32_t shaderID) {
        uint64_t sharedID = SkSetFourByteTag('p', 's', 'd', 'r');
        return (sharedID << 32) | shaderID;
    }

private:
    // TODO: there are some fishy things about using CS sk_sps in the key:
    //   - false negatives: keys are memcmp'ed, so we don't detect equivalent CSs
    //     (SkColorspace::Equals)
    //   - we're keeping the CS alive, even when the client releases it
    //
    // Ideally we'd be using unique IDs or some other weak ref + purge mechanism
    // when the CS is deleted.
    sk_sp<SkColorSpace>        fColorSpace;
    SkRect                     fTile;
    SkShader::TileMode         fTmx, fTmy;
    SkSize                     fScale;
    uint32_t                   fHasDstColorSpace;

    SkDEBUGCODE(uint32_t fEndOfStruct;)
};

struct BitmapShaderRec : public SkResourceCache::Rec {
    BitmapShaderRec(const BitmapShaderKey& key, SkShader* tileShader)
        : fKey(key)
        , fShader(SkRef(tileShader)) {}

    BitmapShaderKey fKey;
    sk_sp<SkShader> fShader;

    const Key& getKey() const override { return fKey; }
    size_t bytesUsed() const override {
        // Just the record overhead -- the actual pixels are accounted by SkImage_Lazy.
        return sizeof(fKey) + sizeof(SkImageShader);
    }
    const char* getCategory() const override { return "bitmap-shader"; }
    SkDiscardableMemory* diagnostic_only_getDiscardable() const override { return nullptr; }

    static bool Visitor(const SkResourceCache::Rec& baseRec, void* contextShader) {
        const BitmapShaderRec& rec = static_cast<const BitmapShaderRec&>(baseRec);
        sk_sp<SkShader>* result = reinterpret_cast<sk_sp<SkShader>*>(contextShader);

        *result = rec.fShader;

        // The bitmap shader is backed by an image generator, thus it can always re-generate its
        // pixels if discarded.
        return true;
    }
};

static int32_t gNextID = 1;
uint32_t next_id() {
    int32_t id;
    do {
        id = sk_atomic_inc(&gNextID);
    } while (id == SK_InvalidGenID);
    return static_cast<uint32_t>(id);
}

} // namespace

SkPictureShader::SkPictureShader(sk_sp<SkPicture> picture, TileMode tmx, TileMode tmy,
                                 const SkMatrix* localMatrix, const SkRect* tile,
                                 sk_sp<SkColorSpace> colorSpace)
    : INHERITED(localMatrix)
    , fPicture(std::move(picture))
    , fTile(tile ? *tile : fPicture->cullRect())
    , fTmx(tmx)
    , fTmy(tmy)
    , fColorSpace(std::move(colorSpace))
    , fUniqueID(next_id())
    , fAddedToCache(false) {}

SkPictureShader::~SkPictureShader() {
    if (fAddedToCache.load()) {
        SkResourceCache::PostPurgeSharedID(BitmapShaderKey::MakeSharedID(fUniqueID));
    }
}

sk_sp<SkShader> SkPictureShader::Make(sk_sp<SkPicture> picture, TileMode tmx, TileMode tmy,
                                      const SkMatrix* localMatrix, const SkRect* tile) {
    if (!picture || picture->cullRect().isEmpty() || (tile && tile->isEmpty())) {
        return SkShader::MakeEmptyShader();
    }
    return sk_sp<SkShader>(new SkPictureShader(std::move(picture), tmx, tmy, localMatrix, tile,
                                               nullptr));
}

sk_sp<SkFlattenable> SkPictureShader::CreateProc(SkReadBuffer& buffer) {
    SkMatrix lm;
    buffer.readMatrix(&lm);
    TileMode mx = (TileMode)buffer.read32();
    TileMode my = (TileMode)buffer.read32();
    SkRect tile;
    buffer.readRect(&tile);

    sk_sp<SkPicture> picture;

    bool didSerialize = buffer.readBool();
    if (didSerialize) {
        picture = SkPicturePriv::MakeFromBuffer(buffer);
    }
    return SkPictureShader::Make(picture, mx, my, &lm, &tile);
}

void SkPictureShader::flatten(SkWriteBuffer& buffer) const {
    buffer.writeMatrix(this->getLocalMatrix());
    buffer.write32(fTmx);
    buffer.write32(fTmy);
    buffer.writeRect(fTile);

    buffer.writeBool(true);
    SkPicturePriv::Flatten(fPicture, buffer);
}

// Returns a cached image shader, which wraps a single picture tile at the given
// CTM/local matrix.  Also adjusts the local matrix for tile scaling.
sk_sp<SkShader> SkPictureShader::refBitmapShader(const SkMatrix& viewMatrix,
                                                 SkTCopyOnFirstWrite<SkMatrix>* localMatrix,
                                                 SkColorSpace* dstColorSpace,
                                                 const int maxTextureSize) const {
    SkASSERT(fPicture && !fPicture->cullRect().isEmpty());

    const SkMatrix m = SkMatrix::Concat(viewMatrix, **localMatrix);

    // Use a rotation-invariant scale
    SkPoint scale;
    //
    // TODO: replace this with decomposeScale() -- but beware LayoutTest rebaselines!
    //
    if (!SkDecomposeUpper2x2(m, nullptr, &scale, nullptr)) {
        // Decomposition failed, use an approximation.
        scale.set(SkScalarSqrt(m.getScaleX() * m.getScaleX() + m.getSkewX() * m.getSkewX()),
                  SkScalarSqrt(m.getScaleY() * m.getScaleY() + m.getSkewY() * m.getSkewY()));
    }
    SkSize scaledSize = SkSize::Make(SkScalarAbs(scale.x() * fTile.width()),
                                     SkScalarAbs(scale.y() * fTile.height()));

    // Clamp the tile size to about 4M pixels
    static const SkScalar kMaxTileArea = 2048 * 2048;
    SkScalar tileArea = scaledSize.width() * scaledSize.height();
    if (tileArea > kMaxTileArea) {
        SkScalar clampScale = SkScalarSqrt(kMaxTileArea / tileArea);
        scaledSize.set(scaledSize.width() * clampScale,
                       scaledSize.height() * clampScale);
    }
#if SK_SUPPORT_GPU
    // Scale down the tile size if larger than maxTextureSize for GPU Path or it should fail on create texture
    if (maxTextureSize) {
        if (scaledSize.width() > maxTextureSize || scaledSize.height() > maxTextureSize) {
            SkScalar downScale = maxTextureSize / SkMaxScalar(scaledSize.width(), scaledSize.height());
            scaledSize.set(SkScalarFloorToScalar(scaledSize.width() * downScale),
                           SkScalarFloorToScalar(scaledSize.height() * downScale));
        }
    }
#endif

    const SkISize tileSize = scaledSize.toCeil();
    if (tileSize.isEmpty()) {
        return SkShader::MakeEmptyShader();
    }

    // The actual scale, compensating for rounding & clamping.
    const SkSize tileScale = SkSize::Make(SkIntToScalar(tileSize.width()) / fTile.width(),
                                          SkIntToScalar(tileSize.height()) / fTile.height());

    // |fColorSpace| will only be set when using an SkColorSpaceXformCanvas to do pre-draw xforms.
    // This canvas is strictly for legacy mode.  A non-null |dstColorSpace| indicates that we
    // should perform color correct rendering and xform at draw time.
    SkASSERT(!fColorSpace || !dstColorSpace);
    sk_sp<SkColorSpace> keyCS = dstColorSpace ? sk_ref_sp(dstColorSpace) : fColorSpace;
    bool hasDstColorSpace = SkToBool(dstColorSpace);

    sk_sp<SkShader> tileShader;
    BitmapShaderKey key(std::move(keyCS),
                        fUniqueID,
                        fTile,
                        fTmx,
                        fTmy,
                        tileScale,
                        hasDstColorSpace);

    if (!SkResourceCache::Find(key, BitmapShaderRec::Visitor, &tileShader)) {
        SkMatrix tileMatrix;
        tileMatrix.setRectToRect(fTile, SkRect::MakeIWH(tileSize.width(), tileSize.height()),
                                 SkMatrix::kFill_ScaleToFit);

        sk_sp<SkImage> tileImage = SkImage::MakeFromGenerator(
                SkPictureImageGenerator::Make(tileSize, fPicture, &tileMatrix, nullptr,
                                              SkImage::BitDepth::kU8, sk_ref_sp(dstColorSpace)));
        if (!tileImage) {
            return nullptr;
        }

        if (fColorSpace) {
            tileImage = tileImage->makeColorSpace(fColorSpace);
        }

        tileShader = tileImage->makeShader(fTmx, fTmy);

        SkResourceCache::Add(new BitmapShaderRec(key, tileShader.get()));
        fAddedToCache.store(true);
    }

    if (tileScale.width() != 1 || tileScale.height() != 1) {
        localMatrix->writable()->preScale(1 / tileScale.width(), 1 / tileScale.height());
    }

    return tileShader;
}

bool SkPictureShader::onAppendStages(const StageRec& rec) const {
    auto lm = this->totalLocalMatrix(rec.fLocalM);

    // Keep bitmapShader alive by using alloc instead of stack memory
    auto& bitmapShader = *rec.fAlloc->make<sk_sp<SkShader>>();
    bitmapShader = this->refBitmapShader(rec.fCTM, &lm, rec.fDstCS);

    if (!bitmapShader) {
        return false;
    }

    StageRec localRec = rec;
    localRec.fLocalM = lm->isIdentity() ? nullptr : lm.get();

    return as_SB(bitmapShader)->appendStages(localRec);
}

/////////////////////////////////////////////////////////////////////////////////////////
SkShaderBase::Context* SkPictureShader::onMakeContext(const ContextRec& rec, SkArenaAlloc* alloc)
const {
    auto lm = this->totalLocalMatrix(rec.fLocalMatrix);
    sk_sp<SkShader> bitmapShader = this->refBitmapShader(*rec.fMatrix, &lm, rec.fDstColorSpace);
    if (!bitmapShader) {
        return nullptr;
    }

    ContextRec localRec = rec;
    localRec.fLocalMatrix = lm->isIdentity() ? nullptr : lm.get();

    PictureShaderContext* ctx =
        alloc->make<PictureShaderContext>(*this, localRec, std::move(bitmapShader), alloc);
    if (nullptr == ctx->fBitmapShaderContext) {
        ctx = nullptr;
    }
    return ctx;
}

sk_sp<SkShader> SkPictureShader::onMakeColorSpace(SkColorSpaceXformer* xformer) const {
    sk_sp<SkColorSpace> dstCS = xformer->dst();
    if (SkColorSpace::Equals(dstCS.get(), fColorSpace.get())) {
        return sk_ref_sp(const_cast<SkPictureShader*>(this));
    }

    return sk_sp<SkPictureShader>(new SkPictureShader(fPicture, fTmx, fTmy, &this->getLocalMatrix(),
                                                      &fTile, std::move(dstCS)));
}

/////////////////////////////////////////////////////////////////////////////////////////

SkPictureShader::PictureShaderContext::PictureShaderContext(
        const SkPictureShader& shader, const ContextRec& rec, sk_sp<SkShader> bitmapShader,
        SkArenaAlloc* alloc)
    : INHERITED(shader, rec)
    , fBitmapShader(std::move(bitmapShader))
{
    fBitmapShaderContext = as_SB(fBitmapShader)->makeContext(rec, alloc);
    //if fBitmapShaderContext is null, we are invalid
}

uint32_t SkPictureShader::PictureShaderContext::getFlags() const {
    SkASSERT(fBitmapShaderContext);
    return fBitmapShaderContext->getFlags();
}

void SkPictureShader::PictureShaderContext::shadeSpan(int x, int y, SkPMColor dstC[], int count) {
    SkASSERT(fBitmapShaderContext);
    fBitmapShaderContext->shadeSpan(x, y, dstC, count);
}

#if SK_SUPPORT_GPU
std::unique_ptr<GrFragmentProcessor> SkPictureShader::asFragmentProcessor(
        const GrFPArgs& args) const {
    int maxTextureSize = 0;
    if (args.fContext) {
        maxTextureSize = args.fContext->contextPriv().caps()->maxTextureSize();
    }

    auto lm = this->totalLocalMatrix(args.fPreLocalMatrix, args.fPostLocalMatrix);
    sk_sp<SkShader> bitmapShader(this->refBitmapShader(*args.fViewMatrix, &lm,
                                                       args.fDstColorSpaceInfo->colorSpace(),
                                                       maxTextureSize));
    if (!bitmapShader) {
        return nullptr;
    }

    // We want to *reset* args.fPreLocalMatrix, not compose it.
    GrFPArgs newArgs(args.fContext, args.fViewMatrix, args.fFilterQuality, args.fDstColorSpaceInfo);
    newArgs.fPreLocalMatrix = lm.get();

    return as_SB(bitmapShader)->asFragmentProcessor(newArgs);
}
#endif
