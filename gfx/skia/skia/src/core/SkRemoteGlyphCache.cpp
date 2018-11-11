/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkRemoteGlyphCache.h"

#include <iterator>
#include <memory>
#include <new>
#include <string>
#include <tuple>

#include "SkDevice.h"
#include "SkDraw.h"
#include "SkGlyphRun.h"
#include "SkGlyphCache.h"
#include "SkRemoteGlyphCacheImpl.h"
#include "SkStrikeCache.h"
#include "SkTraceEvent.h"
#include "SkTypeface_remote.h"

#if SK_SUPPORT_GPU
#include "GrDrawOpAtlas.h"
#include "text/GrTextContext.h"
#endif

static SkDescriptor* auto_descriptor_from_desc(const SkDescriptor* source_desc,
                                               SkFontID font_id,
                                               SkAutoDescriptor* ad) {
    ad->reset(source_desc->getLength());
    auto* desc = ad->getDesc();
    desc->init();

    // Rec.
    {
        uint32_t size;
        auto ptr = source_desc->findEntry(kRec_SkDescriptorTag, &size);
        SkScalerContextRec rec;
        memcpy(&rec, ptr, size);
        rec.fFontID = font_id;
        desc->addEntry(kRec_SkDescriptorTag, sizeof(rec), &rec);
    }

    // Effects.
    {
        uint32_t size;
        auto ptr = source_desc->findEntry(kEffects_SkDescriptorTag, &size);
        if (ptr) { desc->addEntry(kEffects_SkDescriptorTag, size, ptr); }
    }

    desc->computeChecksum();
    return desc;
}

enum DescriptorType : bool { kKey = false, kDevice = true };
static const SkDescriptor* create_descriptor(
        DescriptorType type, const SkPaint& paint, const SkMatrix& m,
        const SkSurfaceProps& props, SkScalerContextFlags flags,
        SkAutoDescriptor* ad, SkScalerContextEffects* effects) {
    SkScalerContextRec deviceRec;
    bool enableTypefaceFiltering = (type == kDevice);
    SkScalerContext::MakeRecAndEffects(
            paint, &props, &m, flags, &deviceRec, effects, enableTypefaceFiltering);
    return SkScalerContext::AutoDescriptorGivenRecAndEffects(deviceRec, *effects, ad);
}

// -- Serializer ----------------------------------------------------------------------------------

size_t pad(size_t size, size_t alignment) { return (size + (alignment - 1)) & ~(alignment - 1); }

class Serializer {
public:
    Serializer(std::vector<uint8_t>* buffer) : fBuffer{buffer} { }

    template <typename T, typename... Args>
    T* emplace(Args&&... args) {
        auto result = allocate(sizeof(T), alignof(T));
        return new (result) T{std::forward<Args>(args)...};
    }

    template <typename T>
    void write(const T& data) {
        T* result = (T*)allocate(sizeof(T), alignof(T));
        memcpy(result, &data, sizeof(T));
    }

    template <typename T>
    T* allocate() {
        T* result = (T*)allocate(sizeof(T), alignof(T));
        return result;
    }

    void writeDescriptor(const SkDescriptor& desc) {
        write(desc.getLength());
        auto result = allocate(desc.getLength(), alignof(SkDescriptor));
        memcpy(result, &desc, desc.getLength());
    }

    void* allocate(size_t size, size_t alignment) {
        size_t aligned = pad(fBuffer->size(), alignment);
        fBuffer->resize(aligned + size);
        return &(*fBuffer)[aligned];
    }

private:
    std::vector<uint8_t>* fBuffer;
};

// -- Deserializer -------------------------------------------------------------------------------
// Note that the Deserializer is reading untrusted data, we need to guard against invalid data.
class Deserializer {
public:
    Deserializer(const volatile char* memory, size_t memorySize)
            : fMemory(memory), fMemorySize(memorySize) {}

    template <typename T>
    bool read(T* val) {
        auto* result = this->ensureAtLeast(sizeof(T), alignof(T));
        if (!result) return false;

        memcpy(val, const_cast<const char*>(result), sizeof(T));
        return true;
    }

    bool readDescriptor(SkAutoDescriptor* ad) {
        uint32_t desc_length = 0u;
        if (!read<uint32_t>(&desc_length)) return false;

        auto* result = this->ensureAtLeast(desc_length, alignof(SkDescriptor));
        if (!result) return false;

        ad->reset(desc_length);
        memcpy(ad->getDesc(), const_cast<const char*>(result), desc_length);
        return true;
    }

    const volatile void* read(size_t size, size_t alignment) {
      return this->ensureAtLeast(size, alignment);
    }

private:
    const volatile char* ensureAtLeast(size_t size, size_t alignment) {
        size_t padded = pad(fBytesRead, alignment);

        // Not enough data
        if (padded + size > fMemorySize) return nullptr;

        auto* result = fMemory + padded;
        fBytesRead = padded + size;
        return result;
    }

    // Note that we read each piece of memory only once to guard against TOCTOU violations.
    const volatile char* fMemory;
    size_t fMemorySize;
    size_t fBytesRead = 0u;
};

// Paths use a SkWriter32 which requires 4 byte alignment.
static const size_t kPathAlignment  = 4u;

bool read_path(Deserializer* deserializer, SkGlyph* glyph, SkGlyphCache* cache) {
    uint64_t pathSize = 0u;
    if (!deserializer->read<uint64_t>(&pathSize)) return false;

    if (pathSize == 0u) return true;

    auto* path = deserializer->read(pathSize, kPathAlignment);
    if (!path) return false;

    return cache->initializePath(glyph, path, pathSize);
}

size_t SkDescriptorMapOperators::operator()(const SkDescriptor* key) const {
    return key->getChecksum();
}

bool SkDescriptorMapOperators::operator()(const SkDescriptor* lhs, const SkDescriptor* rhs) const {
    return *lhs == *rhs;
}

struct StrikeSpec {
    StrikeSpec() {}
    StrikeSpec(SkFontID typefaceID_, SkDiscardableHandleId discardableHandleId_)
            : typefaceID{typefaceID_}, discardableHandleId(discardableHandleId_) {}
    SkFontID typefaceID = 0u;
    SkDiscardableHandleId discardableHandleId = 0u;
    /* desc */
    /* n X (glyphs ids) */
};

// -- TrackLayerDevice -----------------------------------------------------------------------------
SkTextBlobCacheDiffCanvas::TrackLayerDevice::TrackLayerDevice(
        const SkIRect& bounds, const SkSurfaceProps& props, SkStrikeServer* server,
        const SkTextBlobCacheDiffCanvas::Settings& settings)
    : SkNoPixelsDevice(bounds, props)
    , fStrikeServer(server)
    , fSettings(settings)
    , fPainter{props, kUnknown_SkColorType, SkScalerContextFlags::kFakeGammaAndBoostContrast} {
    SkASSERT(fStrikeServer);
}

SkBaseDevice* SkTextBlobCacheDiffCanvas::TrackLayerDevice::onCreateDevice(
        const CreateInfo& cinfo, const SkPaint*) {
    const SkSurfaceProps surfaceProps(this->surfaceProps().flags(), cinfo.fPixelGeometry);
    return new TrackLayerDevice(this->getGlobalBounds(), surfaceProps, fStrikeServer, fSettings);
}

void SkTextBlobCacheDiffCanvas::TrackLayerDevice::drawGlyphRunList(
        const SkGlyphRunList& glyphRunList) {
    for (auto& glyphRun : glyphRunList) {
        this->processGlyphRun(glyphRunList.origin(), glyphRun);
    }
}

void SkTextBlobCacheDiffCanvas::TrackLayerDevice::processGlyphRun(
        const SkPoint& origin, const SkGlyphRun& glyphRun) {
    TRACE_EVENT0("skia", "SkTextBlobCacheDiffCanvas::processGlyphRun");

    const SkPaint& runPaint = glyphRun.paint();
    const SkMatrix& runMatrix = this->ctm();

    // If the matrix has perspective, we fall back to using distance field text or paths.
    #if SK_SUPPORT_GPU
    if (this->maybeProcessGlyphRunForDFT(glyphRun, runMatrix, origin)) {
        return;
    } else
    #endif
    if (SkDraw::ShouldDrawTextAsPaths(runPaint, runMatrix)) {
        this->processGlyphRunForPaths(glyphRun, runMatrix, origin);
    } else {
        this->processGlyphRunForMask(glyphRun, runMatrix, origin);
    }
}

void SkTextBlobCacheDiffCanvas::TrackLayerDevice::processGlyphRunForMask(
        const SkGlyphRun& glyphRun, const SkMatrix& runMatrix, SkPoint origin) {
    TRACE_EVENT0("skia", "SkTextBlobCacheDiffCanvas::processGlyphRunForMask");
    const SkPaint& runPaint = glyphRun.paint();

    SkScalerContextEffects effects;
    auto* glyphCacheState = fStrikeServer->getOrCreateCache(
            runPaint, this->surfaceProps(), runMatrix,
            SkScalerContextFlags::kFakeGammaAndBoostContrast, &effects);
    SkASSERT(glyphCacheState);

    auto perGlyph = [glyphCacheState] (const SkGlyph& glyph, SkPoint mappedPt) {
        glyphCacheState->addGlyph(glyph.getPackedID(), false);
    };

    // Glyphs which are too large for the atlas still request images when computing the bounds
    // for the glyph, which is why its necessary to send both. See related code in
    // get_packed_glyph_bounds in GrGlyphCache.cpp and crbug.com/510931.
    auto perPath = [glyphCacheState] (const SkGlyph& glyph, SkPoint mappedPt) {
        glyphCacheState->addGlyph(glyph.getPackedID(), true);
        glyphCacheState->addGlyph(glyph.getPackedID(), false);
    };

    fPainter.drawGlyphRunAsBMPWithPathFallback(
            glyphCacheState, glyphRun, origin, runMatrix, perGlyph, perPath);
}

struct ARGBHelper {
    void operator () (const SkPaint& fallbackPaint, SkSpan<const SkGlyphID> glyphIDs,
                      SkSpan<const SkPoint> positions, SkScalar textScale,
                      const SkMatrix& glyphCacheMatrix,
                      SkGlyphRunListPainter::NeedsTransform needsTransform) {
        TRACE_EVENT0("skia", "argbFallback");

        SkScalerContextEffects effects;
        auto* fallbackCache =
                fStrikeServer->getOrCreateCache(
                        fallbackPaint, fSurfaceProps, fFallbackMatrix,
                        SkScalerContextFlags::kFakeGammaAndBoostContrast, &effects);

        for (auto glyphID : glyphIDs) {
            fallbackCache->addGlyph(SkPackedGlyphID(glyphID, 0, 0), false);
        }
    }

    const SkMatrix& fFallbackMatrix;
    const SkSurfaceProps& fSurfaceProps;
    SkStrikeServer* const fStrikeServer;
};

void SkTextBlobCacheDiffCanvas::TrackLayerDevice::processGlyphRunForPaths(
        const SkGlyphRun& glyphRun, const SkMatrix& runMatrix, SkPoint origin) {
    TRACE_EVENT0("skia", "SkTextBlobCacheDiffCanvas::processGlyphRunForPaths");
    const SkPaint& runPaint = glyphRun.paint();
    SkPaint pathPaint{runPaint};

    SkScalar textScale = pathPaint.setupForAsPaths();

    SkScalerContextEffects effects;
    auto* glyphCacheState = fStrikeServer->getOrCreateCache(
            pathPaint, this->surfaceProps(), SkMatrix::I(),
            SkScalerContextFlags::kFakeGammaAndBoostContrast, &effects);

    auto perPath = [glyphCacheState](const SkGlyph& glyph, SkPoint position) {
        const bool asPath = true;
        glyphCacheState->addGlyph(glyph.getGlyphID(), asPath);
    };

    ARGBHelper argbFallback{runMatrix, surfaceProps(), fStrikeServer};

    fPainter.drawGlyphRunAsPathWithARGBFallback(
            glyphCacheState, glyphRun, origin, runMatrix, textScale, perPath, argbFallback);
}

#if SK_SUPPORT_GPU
bool SkTextBlobCacheDiffCanvas::TrackLayerDevice::maybeProcessGlyphRunForDFT(
        const SkGlyphRun& glyphRun, const SkMatrix& runMatrix, SkPoint origin) {
    TRACE_EVENT0("skia", "SkTextBlobCacheDiffCanvas::maybeProcessGlyphRunForDFT");

    const SkPaint& runPaint = glyphRun.paint();

    GrTextContext::Options options;
    options.fMinDistanceFieldFontSize = fSettings.fMinDistanceFieldFontSize;
    options.fMaxDistanceFieldFontSize = fSettings.fMaxDistanceFieldFontSize;
    GrTextContext::SanitizeOptions(&options);
    if (!GrTextContext::CanDrawAsDistanceFields(runPaint, runMatrix, this->surfaceProps(),
                                                fSettings.fContextSupportsDistanceFieldText,
                                                options)) {
        return false;
    }

    SkScalar textRatio;
    SkPaint dfPaint(runPaint);
    SkScalerContextFlags flags;
    GrTextContext::InitDistanceFieldPaint(nullptr, &dfPaint, runMatrix, options, &textRatio,
                                          &flags);
    SkScalerContextEffects effects;
    auto* sdfCache = fStrikeServer->getOrCreateCache(dfPaint, this->surfaceProps(),
                                                            SkMatrix::I(), flags, &effects);

    ARGBHelper argbFallback{runMatrix, surfaceProps(), fStrikeServer};

    auto perSDF = [sdfCache] (const SkGlyph& glyph, SkPoint position) {
        const bool asPath = false;
        sdfCache->addGlyph(glyph.getGlyphID(), asPath);
    };

    auto perPath = [sdfCache] (const SkGlyph& glyph, SkPoint position) {
        const bool asPath = true;
        sdfCache->addGlyph(glyph.getGlyphID(), asPath);
    };

    fPainter.drawGlyphRunAsSDFWithARGBFallback(
            sdfCache, glyphRun, origin, runMatrix, textRatio,
            perSDF, perPath, argbFallback);

    return true;
}
#endif

// -- SkTextBlobCacheDiffCanvas -------------------------------------------------------------------
SkTextBlobCacheDiffCanvas::Settings::Settings() = default;

SkTextBlobCacheDiffCanvas::SkTextBlobCacheDiffCanvas(int width, int height,
                                                     const SkMatrix& deviceMatrix,
                                                     const SkSurfaceProps& props,
                                                     SkStrikeServer* strikeServer,
                                                     Settings settings)
        : SkNoDrawCanvas{sk_make_sp<TrackLayerDevice>(SkIRect::MakeWH(width, height), props,
                                                      strikeServer, settings)} {}

SkTextBlobCacheDiffCanvas::SkTextBlobCacheDiffCanvas(
        int width, int height, const SkSurfaceProps& props,
        SkStrikeServer* strikeServer, Settings settings)
    : SkNoDrawCanvas{sk_make_sp<TrackLayerDevice>(
            SkIRect::MakeWH(width, height), props, strikeServer, settings)} {}

SkTextBlobCacheDiffCanvas::~SkTextBlobCacheDiffCanvas() = default;

SkCanvas::SaveLayerStrategy SkTextBlobCacheDiffCanvas::getSaveLayerStrategy(
        const SaveLayerRec& rec) {
    return kFullLayer_SaveLayerStrategy;
}

void SkTextBlobCacheDiffCanvas::onDrawTextBlob(const SkTextBlob* blob, SkScalar x, SkScalar y,
                                               const SkPaint& paint) {
    SkCanvas::onDrawTextBlob(blob, x, y, paint);
}

struct WireTypeface {
    WireTypeface() = default;
    WireTypeface(SkFontID typeface_id, int glyph_count, SkFontStyle style, bool is_fixed)
            : typefaceID(typeface_id), glyphCount(glyph_count), style(style), isFixed(is_fixed) {}

    // std::thread::id thread_id;  // TODO:need to figure a good solution
    SkFontID        typefaceID;
    int             glyphCount;
    SkFontStyle     style;
    bool            isFixed;
};

// SkStrikeServer -----------------------------------------

SkStrikeServer::SkStrikeServer(DiscardableHandleManager* discardableHandleManager)
        : fDiscardableHandleManager(discardableHandleManager) {
    SkASSERT(fDiscardableHandleManager);
}

SkStrikeServer::~SkStrikeServer() = default;

sk_sp<SkData> SkStrikeServer::serializeTypeface(SkTypeface* tf) {
    WireTypeface wire(SkTypeface::UniqueID(tf), tf->countGlyphs(), tf->fontStyle(),
                      tf->isFixedPitch());
    return SkData::MakeWithCopy(&wire, sizeof(wire));
}

void SkStrikeServer::writeStrikeData(std::vector<uint8_t>* memory) {
    if (fLockedDescs.empty() && fTypefacesToSend.empty()) {
        return;
    }

    Serializer serializer(memory);
    serializer.emplace<uint64_t>(fTypefacesToSend.size());
    for (const auto& tf : fTypefacesToSend) serializer.write<WireTypeface>(tf);
    fTypefacesToSend.clear();

    serializer.emplace<uint64_t>(fLockedDescs.size());
    for (const auto* desc : fLockedDescs) {
        auto it = fRemoteGlyphStateMap.find(desc);
        SkASSERT(it != fRemoteGlyphStateMap.end());
        it->second->writePendingGlyphs(&serializer);
    }
    fLockedDescs.clear();
}

SkStrikeServer::SkGlyphCacheState* SkStrikeServer::getOrCreateCache(
        const SkPaint& paint,
        const SkSurfaceProps& props,
        const SkMatrix& matrix,
        SkScalerContextFlags flags,
        SkScalerContextEffects* effects) {
    SkAutoDescriptor keyAutoDesc;
    auto keyDesc = create_descriptor(kKey, paint, matrix, props, flags, &keyAutoDesc, effects);

    // In cases where tracing is turned off, make sure not to get an unused function warning.
    // Lambdaize the function.
    TRACE_EVENT1("skia", "RecForDesc", "rec",
            TRACE_STR_COPY(
                    [keyDesc](){
                        auto ptr = keyDesc->findEntry(kRec_SkDescriptorTag, nullptr);
                        SkScalerContextRec rec;
                        std::memcpy(&rec, ptr, sizeof(rec));
                        return rec.dump();
                    }().c_str()
            )
    );

    // Already locked.
    if (fLockedDescs.find(keyDesc) != fLockedDescs.end()) {
        auto it = fRemoteGlyphStateMap.find(keyDesc);
        SkASSERT(it != fRemoteGlyphStateMap.end());
        SkGlyphCacheState* cache = it->second.get();
        cache->setPaint(paint);
        return cache;
    }

    // Try to lock.
    auto it = fRemoteGlyphStateMap.find(keyDesc);
    if (it != fRemoteGlyphStateMap.end()) {
        SkGlyphCacheState* cache = it->second.get();
#ifdef SK_DEBUG
        SkScalerContextEffects deviceEffects;
        SkAutoDescriptor deviceAutoDesc;
        auto deviceDesc = create_descriptor(
                kDevice, paint, matrix, props, flags, &deviceAutoDesc, &deviceEffects);
        SkASSERT(cache->getDeviceDescriptor() == *deviceDesc);
#endif
        bool locked = fDiscardableHandleManager->lockHandle(it->second->discardableHandleId());
        if (locked) {
            fLockedDescs.insert(it->first);
            cache->setPaint(paint);
            return cache;
        }

        // If the lock failed, the entry was deleted on the client. Remove our
        // tracking.
        fRemoteGlyphStateMap.erase(it);
    }

    auto* tf = paint.getTypeface();
    const SkFontID typefaceId = tf->uniqueID();
    if (!fCachedTypefaces.contains(typefaceId)) {
        fCachedTypefaces.add(typefaceId);
        fTypefacesToSend.emplace_back(typefaceId, tf->countGlyphs(), tf->fontStyle(),
                                      tf->isFixedPitch());
    }

    SkScalerContextEffects deviceEffects;
    SkAutoDescriptor deviceAutoDesc;
    auto deviceDesc = create_descriptor(
            kDevice, paint, matrix, props, flags, &deviceAutoDesc, &deviceEffects);

    auto context = tf->createScalerContext(deviceEffects, deviceDesc);

    // Create a new cache state and insert it into the map.
    auto newHandle = fDiscardableHandleManager->createHandle();
    auto cacheState = skstd::make_unique<SkGlyphCacheState>(
            *keyDesc, *deviceDesc, std::move(context), newHandle);

    auto* cacheStatePtr = cacheState.get();

    fLockedDescs.insert(&cacheStatePtr->getKeyDescriptor());
    fRemoteGlyphStateMap[&cacheStatePtr->getKeyDescriptor()] = std::move(cacheState);

    checkForDeletedEntries();
    cacheStatePtr->setPaint(paint);
    return cacheStatePtr;
}

void SkStrikeServer::checkForDeletedEntries() {
    auto it = fRemoteGlyphStateMap.begin();
    while (fRemoteGlyphStateMap.size() > fMaxEntriesInDescriptorMap &&
           it != fRemoteGlyphStateMap.end()) {
        if (fDiscardableHandleManager->isHandleDeleted(it->second->discardableHandleId())) {
            it = fRemoteGlyphStateMap.erase(it);
        } else {
            ++it;
        }
    }
}

// -- SkGlyphCacheState ----------------------------------------------------------------------------
SkStrikeServer::SkGlyphCacheState::SkGlyphCacheState(
        const SkDescriptor& keyDescriptor,
        const SkDescriptor& deviceDescriptor,
        std::unique_ptr<SkScalerContext> context,
        uint32_t discardableHandleId)
        : fKeyDescriptor{keyDescriptor}
        , fDeviceDescriptor{deviceDescriptor}
        , fDiscardableHandleId(discardableHandleId)
        , fIsSubpixel{context->isSubpixel()}
        , fAxisAlignmentForHText{context->computeAxisAlignmentForHText()}
        // N.B. context must come last because it is used above.
        , fContext{std::move(context)} {
    SkASSERT(fKeyDescriptor.getDesc() != nullptr);
    SkASSERT(fDeviceDescriptor.getDesc() != nullptr);
    SkASSERT(fContext != nullptr);
}

SkStrikeServer::SkGlyphCacheState::~SkGlyphCacheState() = default;

void SkStrikeServer::SkGlyphCacheState::addGlyph(SkPackedGlyphID glyph, bool asPath) {
    auto* cache = asPath ? &fCachedGlyphPaths : &fCachedGlyphImages;
    auto* pending = asPath ? &fPendingGlyphPaths : &fPendingGlyphImages;

    // Already cached.
    if (cache->contains(glyph)) {
        return;
    }

    // A glyph is going to be sent. Make sure we have a scaler context to send it.
    this->ensureScalerContext();

    // Serialize and cache. Also create the scalar context to use when serializing
    // this glyph.
    cache->add(glyph);
    pending->push_back(glyph);
}

static void writeGlyph(SkGlyph* glyph, Serializer* serializer) {
    serializer->write<SkPackedGlyphID>(glyph->getPackedID());
    serializer->write<float>(glyph->fAdvanceX);
    serializer->write<float>(glyph->fAdvanceY);
    serializer->write<uint16_t>(glyph->fWidth);
    serializer->write<uint16_t>(glyph->fHeight);
    serializer->write<int16_t>(glyph->fTop);
    serializer->write<int16_t>(glyph->fLeft);
    serializer->write<int8_t>(glyph->fForceBW);
    serializer->write<uint8_t>(glyph->fMaskFormat);
}

void SkStrikeServer::SkGlyphCacheState::writePendingGlyphs(Serializer* serializer) {
    // TODO(khushalsagar): Write a strike only if it has any pending glyphs.
    serializer->emplace<bool>(this->hasPendingGlyphs());
    if (!this->hasPendingGlyphs()) {
        fContext.reset();
        fPaint = nullptr;
        return;
    }

    // Write the desc.
    serializer->emplace<StrikeSpec>(fContext->getTypeface()->uniqueID(), fDiscardableHandleId);
    serializer->writeDescriptor(*fKeyDescriptor.getDesc());

    // Write FontMetrics.
    // TODO(khushalsagar): Do we need to re-send each time?
    SkPaint::FontMetrics fontMetrics;
    fContext->getFontMetrics(&fontMetrics);
    serializer->write<SkPaint::FontMetrics>(fontMetrics);

    // Write glyphs images.
    serializer->emplace<uint64_t>(fPendingGlyphImages.size());
    for (const auto& glyphID : fPendingGlyphImages) {
        SkGlyph glyph;
        glyph.initWithGlyphID(glyphID);
        fContext->getMetrics(&glyph);
        writeGlyph(&glyph, serializer);

        auto imageSize = glyph.computeImageSize();
        if (imageSize == 0u) continue;

        glyph.fImage = serializer->allocate(imageSize, glyph.formatAlignment());
        fContext->getImage(glyph);
        // TODO: Generating the image can change the mask format, do we need to update it in the
        // serialized glyph?
    }
    fPendingGlyphImages.clear();

    // Write glyphs paths.
    serializer->emplace<uint64_t>(fPendingGlyphPaths.size());
    for (const auto& glyphID : fPendingGlyphPaths) {
        SkGlyph glyph;
        glyph.initWithGlyphID(glyphID);
        fContext->getMetrics(&glyph);
        writeGlyph(&glyph, serializer);
        writeGlyphPath(glyphID, serializer);
    }
    fPendingGlyphPaths.clear();
    fContext.reset();
    fPaint = nullptr;
}

const SkGlyph& SkStrikeServer::SkGlyphCacheState::findGlyph(SkPackedGlyphID glyphID) {
    auto* glyph = fGlyphMap.find(glyphID);
    if (glyph == nullptr) {
        this->ensureScalerContext();
        glyph = fGlyphMap.set(glyphID, SkGlyph());
        glyph->initWithGlyphID(glyphID);
        fContext->getMetrics(glyph);
    }

    return *glyph;
}

void SkStrikeServer::SkGlyphCacheState::ensureScalerContext() {
    if (fContext == nullptr) {
        SkScalerContextEffects effects{*fPaint};
        auto tf = fPaint->getTypeface();
        fContext = tf->createScalerContext(effects, fDeviceDescriptor.getDesc());
    }
}

void SkStrikeServer::SkGlyphCacheState::setPaint(const SkPaint& paint) {
    fPaint = &paint;
}

SkVector SkStrikeServer::SkGlyphCacheState::rounding() const {
    return SkGlyphCacheCommon::PixelRounding(fIsSubpixel, fAxisAlignmentForHText);
}

const SkGlyph& SkStrikeServer::SkGlyphCacheState::getGlyphMetrics(
        SkGlyphID glyphID, SkPoint position) {
    SkIPoint lookupPoint = SkGlyphCacheCommon::SubpixelLookup(fAxisAlignmentForHText, position);
    SkPackedGlyphID packedGlyphID = fIsSubpixel ? SkPackedGlyphID{glyphID, lookupPoint}
                                                : SkPackedGlyphID{glyphID};

    return this->findGlyph(packedGlyphID);
}

void SkStrikeServer::SkGlyphCacheState::writeGlyphPath(const SkPackedGlyphID& glyphID,
                                                       Serializer* serializer) const {
    SkPath path;
    if (!fContext->getPath(glyphID, &path)) {
        serializer->write<uint64_t>(0u);
        return;
    }

    size_t pathSize = path.writeToMemory(nullptr);
    serializer->write<uint64_t>(pathSize);
    path.writeToMemory(serializer->allocate(pathSize, kPathAlignment));
}

// SkStrikeClient -----------------------------------------
class SkStrikeClient::DiscardableStrikePinner : public SkStrikePinner {
public:
    DiscardableStrikePinner(SkDiscardableHandleId discardableHandleId,
                            sk_sp<DiscardableHandleManager> manager)
            : fDiscardableHandleId(discardableHandleId), fManager(std::move(manager)) {}

    ~DiscardableStrikePinner() override = default;
    bool canDelete() override { return fManager->deleteHandle(fDiscardableHandleId); }

private:
    const SkDiscardableHandleId fDiscardableHandleId;
    sk_sp<DiscardableHandleManager> fManager;
};

SkStrikeClient::SkStrikeClient(sk_sp<DiscardableHandleManager> discardableManager,
                               bool isLogging,
                               SkStrikeCache* strikeCache)
        : fDiscardableHandleManager(std::move(discardableManager))
        , fStrikeCache{strikeCache ? strikeCache : SkStrikeCache::GlobalStrikeCache()}
        , fIsLogging{isLogging} {}

SkStrikeClient::~SkStrikeClient() = default;

#define READ_FAILURE                      \
    {                                     \
        SkDEBUGFAIL("Bad serialization"); \
        return false;                     \
    }

static bool readGlyph(SkGlyph* glyph, Deserializer* deserializer) {
    SkPackedGlyphID glyphID;
    if (!deserializer->read<SkPackedGlyphID>(&glyphID)) return false;
    glyph->initWithGlyphID(glyphID);
    if (!deserializer->read<float>(&glyph->fAdvanceX)) return false;
    if (!deserializer->read<float>(&glyph->fAdvanceY)) return false;
    if (!deserializer->read<uint16_t>(&glyph->fWidth)) return false;
    if (!deserializer->read<uint16_t>(&glyph->fHeight)) return false;
    if (!deserializer->read<int16_t>(&glyph->fTop)) return false;
    if (!deserializer->read<int16_t>(&glyph->fLeft)) return false;
    if (!deserializer->read<int8_t>(&glyph->fForceBW)) return false;
    if (!deserializer->read<uint8_t>(&glyph->fMaskFormat)) return false;
    return true;
}

bool SkStrikeClient::readStrikeData(const volatile void* memory, size_t memorySize) {
    SkASSERT(memorySize != 0u);
    Deserializer deserializer(static_cast<const volatile char*>(memory), memorySize);

    uint64_t typefaceSize = 0u;
    if (!deserializer.read<uint64_t>(&typefaceSize)) READ_FAILURE

    for (size_t i = 0; i < typefaceSize; ++i) {
        WireTypeface wire;
        if (!deserializer.read<WireTypeface>(&wire)) READ_FAILURE

        // TODO(khushalsagar): The typeface no longer needs a reference to the
        // SkStrikeClient, since all needed glyphs must have been pushed before
        // raster.
        addTypeface(wire);
    }

    uint64_t strikeCount = 0u;
    if (!deserializer.read<uint64_t>(&strikeCount)) READ_FAILURE

    for (size_t i = 0; i < strikeCount; ++i) {
        bool has_glyphs = false;
        if (!deserializer.read<bool>(&has_glyphs)) READ_FAILURE

        if (!has_glyphs) continue;

        StrikeSpec spec;
        if (!deserializer.read<StrikeSpec>(&spec)) READ_FAILURE

        SkAutoDescriptor sourceAd;
        if (!deserializer.readDescriptor(&sourceAd)) READ_FAILURE

        SkPaint::FontMetrics fontMetrics;
        if (!deserializer.read<SkPaint::FontMetrics>(&fontMetrics)) READ_FAILURE

        // Get the local typeface from remote fontID.
        auto* tf = fRemoteFontIdToTypeface.find(spec.typefaceID)->get();
        // Received strikes for a typeface which doesn't exist.
        if (!tf) READ_FAILURE

        // Replace the ContextRec in the desc from the server to create the client
        // side descriptor.
        // TODO: Can we do this in-place and re-compute checksum? Instead of a complete copy.
        SkAutoDescriptor ad;
        auto* client_desc = auto_descriptor_from_desc(sourceAd.getDesc(), tf->uniqueID(), &ad);

        auto strike = fStrikeCache->findStrikeExclusive(*client_desc);
        if (strike == nullptr) {
            // Note that we don't need to deserialize the effects since we won't be generating any
            // glyphs here anyway, and the desc is still correct since it includes the serialized
            // effects.
            SkScalerContextEffects effects;
            auto scaler = SkStrikeCache::CreateScalerContext(*client_desc, effects, *tf);
            strike = fStrikeCache->createStrikeExclusive(
                    *client_desc, std::move(scaler), &fontMetrics,
                    skstd::make_unique<DiscardableStrikePinner>(spec.discardableHandleId,
                                                                fDiscardableHandleManager));
            auto proxyContext = static_cast<SkScalerContextProxy*>(strike->getScalerContext());
            proxyContext->initCache(strike.get(), fStrikeCache);
        }

        uint64_t glyphImagesCount = 0u;
        if (!deserializer.read<uint64_t>(&glyphImagesCount)) READ_FAILURE
        for (size_t j = 0; j < glyphImagesCount; j++) {
            SkGlyph glyph;
            if (!readGlyph(&glyph, &deserializer)) READ_FAILURE

            SkGlyph* allocatedGlyph = strike->getRawGlyphByID(glyph.getPackedID());

            // Update the glyph unless it's already got an image (from fallback),
            // preserving any path that might be present.
            if (allocatedGlyph->fImage == nullptr) {
                auto* glyphPath = allocatedGlyph->fPathData;
                *allocatedGlyph = glyph;
                allocatedGlyph->fPathData = glyphPath;
            }

            auto imageSize = glyph.computeImageSize();
            if (imageSize == 0u) continue;

            auto* image = deserializer.read(imageSize, allocatedGlyph->formatAlignment());
            if (!image) READ_FAILURE
            strike->initializeImage(image, imageSize, allocatedGlyph);
        }

        uint64_t glyphPathsCount = 0u;
        if (!deserializer.read<uint64_t>(&glyphPathsCount)) READ_FAILURE
        for (size_t j = 0; j < glyphPathsCount; j++) {
            SkGlyph glyph;
            if (!readGlyph(&glyph, &deserializer)) READ_FAILURE

            SkGlyph* allocatedGlyph = strike->getRawGlyphByID(glyph.getPackedID());

            // Update the glyph unless it's already got a path (from fallback),
            // preserving any image that might be present.
            if (allocatedGlyph->fPathData == nullptr) {
                auto* glyphImage = allocatedGlyph->fImage;
                *allocatedGlyph = glyph;
                allocatedGlyph->fImage = glyphImage;
            }

            if (!read_path(&deserializer, allocatedGlyph, strike.get())) READ_FAILURE
        }
    }

    return true;
}

sk_sp<SkTypeface> SkStrikeClient::deserializeTypeface(const void* buf, size_t len) {
    WireTypeface wire;
    if (len != sizeof(wire)) return nullptr;
    memcpy(&wire, buf, sizeof(wire));
    return this->addTypeface(wire);
}

sk_sp<SkTypeface> SkStrikeClient::addTypeface(const WireTypeface& wire) {
    auto* typeface = fRemoteFontIdToTypeface.find(wire.typefaceID);
    if (typeface) return *typeface;

    auto newTypeface = sk_make_sp<SkTypefaceProxy>(
            wire.typefaceID, wire.glyphCount, wire.style, wire.isFixed,
            fDiscardableHandleManager, fIsLogging);
    fRemoteFontIdToTypeface.set(wire.typefaceID, newTypeface);
    return std::move(newTypeface);
}
