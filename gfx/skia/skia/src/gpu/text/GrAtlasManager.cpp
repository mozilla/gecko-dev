/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrAtlasManager.h"

#include "GrGlyph.h"
#include "GrGlyphCache.h"

GrAtlasManager::GrAtlasManager(GrProxyProvider* proxyProvider, GrGlyphCache* glyphCache,
                               size_t maxTextureBytes,
                               GrDrawOpAtlas::AllowMultitexturing allowMultitexturing)
            : fAllowMultitexturing{allowMultitexturing}
            , fProxyProvider{proxyProvider}
            , fCaps{fProxyProvider->refCaps()}
            , fGlyphCache{glyphCache}
            , fAtlasConfigs{fCaps->maxTextureSize(), maxTextureBytes} { }

GrAtlasManager::~GrAtlasManager() = default;

static GrPixelConfig mask_format_to_pixel_config(GrMaskFormat format) {
    switch (format) {
        case kA8_GrMaskFormat:
            return kAlpha_8_GrPixelConfig;
        case kA565_GrMaskFormat:
            return kRGB_565_GrPixelConfig;
        case kARGB_GrMaskFormat:
            return kRGBA_8888_GrPixelConfig;
        default:
            SkDEBUGFAIL("unsupported GrMaskFormat");
            return kAlpha_8_GrPixelConfig;
    }
}

void GrAtlasManager::freeAll() {
    for (int i = 0; i < kMaskFormatCount; ++i) {
        fAtlases[i] = nullptr;
    }
}

bool GrAtlasManager::hasGlyph(GrGlyph* glyph) {
    SkASSERT(glyph);
    return this->getAtlas(glyph->fMaskFormat)->hasID(glyph->fID);
}

// add to texture atlas that matches this format
GrDrawOpAtlas::ErrorCode GrAtlasManager::addToAtlas(
                                GrResourceProvider* resourceProvider,
                                GrGlyphCache* glyphCache,
                                GrTextStrike* strike, GrDrawOpAtlas::AtlasID* id,
                                GrDeferredUploadTarget* target, GrMaskFormat format,
                                int width, int height, const void* image, SkIPoint16* loc) {
    glyphCache->setStrikeToPreserve(strike);
    return this->getAtlas(format)->addToAtlas(resourceProvider, id, target, width, height,
                                              image, loc);
}

void GrAtlasManager::addGlyphToBulkAndSetUseToken(GrDrawOpAtlas::BulkUseTokenUpdater* updater,
                                                  GrGlyph* glyph,
                                                  GrDeferredUploadToken token) {
    SkASSERT(glyph);
    updater->add(glyph->fID);
    this->getAtlas(glyph->fMaskFormat)->setLastUseToken(glyph->fID, token);
}

#ifdef SK_DEBUG
#include "GrContextPriv.h"
#include "GrSurfaceProxy.h"
#include "GrSurfaceContext.h"
#include "GrTextureProxy.h"

#include "SkBitmap.h"
#include "SkImageEncoder.h"
#include "SkStream.h"
#include <stdio.h>

/**
  * Write the contents of the surface proxy to a PNG. Returns true if successful.
  * @param filename      Full path to desired file
  */
static bool save_pixels(GrContext* context, GrSurfaceProxy* sProxy, const char* filename) {
    if (!sProxy) {
        return false;
    }

    SkImageInfo ii = SkImageInfo::Make(sProxy->width(), sProxy->height(),
                                       kRGBA_8888_SkColorType, kPremul_SkAlphaType);
    SkBitmap bm;
    if (!bm.tryAllocPixels(ii)) {
        return false;
    }

    sk_sp<GrSurfaceContext> sContext(context->contextPriv().makeWrappedSurfaceContext(
                                                                                sk_ref_sp(sProxy)));
    if (!sContext || !sContext->asTextureProxy()) {
        return false;
    }

    bool result = sContext->readPixels(ii, bm.getPixels(), bm.rowBytes(), 0, 0);
    if (!result) {
        SkDebugf("------ failed to read pixels for %s\n", filename);
        return false;
    }

    // remove any previous version of this file
    remove(filename);

    SkFILEWStream file(filename);
    if (!file.isValid()) {
        SkDebugf("------ failed to create file: %s\n", filename);
        remove(filename);   // remove any partial file
        return false;
    }

    if (!SkEncodeImage(&file, bm, SkEncodedImageFormat::kPNG, 100)) {
        SkDebugf("------ failed to encode %s\n", filename);
        remove(filename);   // remove any partial file
        return false;
    }

    return true;
}

void GrAtlasManager::dump(GrContext* context) const {
    static int gDumpCount = 0;
    for (int i = 0; i < kMaskFormatCount; ++i) {
        if (fAtlases[i]) {
            const sk_sp<GrTextureProxy>* proxies = fAtlases[i]->getProxies();
            for (uint32_t pageIdx = 0; pageIdx < fAtlases[i]->numActivePages(); ++pageIdx) {
                SkASSERT(proxies[pageIdx]);
                SkString filename;
#ifdef SK_BUILD_FOR_ANDROID
                filename.printf("/sdcard/fontcache_%d%d%d.png", gDumpCount, i, pageIdx);
#else
                filename.printf("fontcache_%d%d%d.png", gDumpCount, i, pageIdx);
#endif

                save_pixels(context, proxies[pageIdx].get(), filename.c_str());
            }
        }
    }
    ++gDumpCount;
}
#endif

void GrAtlasManager::setAtlasSizesToMinimum_ForTesting() {
    // Delete any old atlases.
    // This should be safe to do as long as we are not in the middle of a flush.
    for (int i = 0; i < kMaskFormatCount; i++) {
        fAtlases[i] = nullptr;
    }

    // Set all the atlas sizes to 1x1 plot each.
    new (&fAtlasConfigs) GrDrawOpAtlasConfig{};
}

bool GrAtlasManager::initAtlas(GrMaskFormat format) {
    int index = MaskFormatToAtlasIndex(format);
    if (fAtlases[index] == nullptr) {
        GrPixelConfig config = mask_format_to_pixel_config(format);
        SkISize atlasDimensions = fAtlasConfigs.atlasDimensions(format);
        SkISize numPlots = fAtlasConfigs.numPlots(format);

        fAtlases[index] = GrDrawOpAtlas::Make(
                fProxyProvider, config, atlasDimensions.width(), atlasDimensions.height(),
                numPlots.width(), numPlots.height(), fAllowMultitexturing,
                &GrGlyphCache::HandleEviction, fGlyphCache);
        if (!fAtlases[index]) {
            return false;
        }
    }
    return true;
}
