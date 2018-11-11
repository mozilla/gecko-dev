/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef SkPDFCanon_DEFINED
#define SkPDFCanon_DEFINED

#include <vector>

#include "SkBitmapKey.h"
#include "SkMacros.h"
#include "SkPDFGradientShader.h"
#include "SkPDFGraphicState.h"
#include "SkPDFShader.h"
#include "SkString.h"
#include "SkTHash.h"
#include "SkTypeface.h"

class SkPDFFont;
struct SkAdvancedTypefaceMetrics;

/**
 *  The SkPDFCanon canonicalizes objects across PDF pages
 *  (SkPDFDevices) and across draw calls.
 */
class SkPDFCanon {
public:
    ~SkPDFCanon();
    SkPDFCanon();
    SkPDFCanon(SkPDFCanon&&);
    SkPDFCanon(const SkPDFCanon&) = delete;
    SkPDFCanon& operator=(SkPDFCanon&&);
    SkPDFCanon& operator=(const SkPDFCanon&) = delete;

    SkTHashMap<SkPDFImageShaderKey, sk_sp<SkPDFObject>> fImageShaderMap;

    SkPDFGradientShader::HashMap fGradientPatternMap;

    SkTHashMap<SkBitmapKey, sk_sp<SkPDFObject>> fPDFBitmapMap;

    SkTHashMap<uint32_t, std::unique_ptr<SkAdvancedTypefaceMetrics>> fTypefaceMetrics;
    SkTHashMap<uint32_t, std::vector<SkString>> fType1GlyphNames;
    SkTHashMap<uint32_t, std::vector<SkUnichar>> fToUnicodeMap;
    SkTHashMap<uint32_t, sk_sp<SkPDFDict>> fFontDescriptors;
    SkTHashMap<uint64_t, sk_sp<SkPDFFont>> fFontMap;

    SkTHashMap<SkPDFStrokeGraphicState, sk_sp<SkPDFDict>> fStrokeGSMap;
    SkTHashMap<SkPDFFillGraphicState, sk_sp<SkPDFDict>> fFillGSMap;

    sk_sp<SkPDFStream> fInvertFunction;
    sk_sp<SkPDFDict> fNoSmaskGraphicState;
    sk_sp<SkPDFArray> fRangeObject;
};

#endif  // SkPDFCanon_DEFINED
