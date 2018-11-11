/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrTextContext_DEFINED
#define GrTextContext_DEFINED

#include "GrDistanceFieldAdjustTable.h"
#include "GrGeometryProcessor.h"
#include "GrTextBlob.h"
#include "GrTextTarget.h"
#include "SkGlyphRun.h"

#if GR_TEST_UTILS
#include "GrDrawOpTest.h"
#endif

class GrDrawOp;
class GrTextBlobCache;
class SkGlyph;

/*
 * Renders text using some kind of an atlas, ie BitmapText or DistanceField text
 */
class GrTextContext {
public:
    struct Options {
        /**
         * Below this size (in device space) distance field text will not be used. Negative means
         * use a default value.
         */
        SkScalar fMinDistanceFieldFontSize = -1.f;
        /**
         * Above this size (in device space) distance field text will not be used and glyphs will
         * be rendered from outline as individual paths. Negative means use a default value.
         */
        SkScalar fMaxDistanceFieldFontSize = -1.f;
        /** Forces all distance field vertices to use 3 components, not just when in perspective. */
        bool fDistanceFieldVerticesAlwaysHaveW = false;
    };

    static std::unique_ptr<GrTextContext> Make(const Options& options);

    void drawGlyphRunList(GrContext*, GrTextTarget*, const GrClip&,
                          const SkMatrix& viewMatrix, const SkSurfaceProps&, const SkGlyphRunList&);

    std::unique_ptr<GrDrawOp> createOp_TestingOnly(GrContext*,
                                                   GrTextContext*,
                                                   GrRenderTargetContext*,
                                                   const SkPaint&,
                                                   const SkMatrix& viewMatrix,
                                                   const char* text,
                                                   int x,
                                                   int y);

    static void SanitizeOptions(Options* options);
    static bool CanDrawAsDistanceFields(const SkPaint& skPaint, const SkMatrix& viewMatrix,
                                        const SkSurfaceProps& props,
                                        bool contextSupportsDistanceFieldText,
                                        const Options& options);
    static void InitDistanceFieldPaint(GrTextBlob* blob,
                                       SkPaint* skPaint,
                                       const SkMatrix& viewMatrix,
                                       const Options& options,
                                       SkScalar* textRatio,
                                       SkScalerContextFlags* flags);

private:
    GrTextContext(const Options& options);

    // sets up the descriptor on the blob and returns a detached cache.  Client must attach
    static SkColor ComputeCanonicalColor(const SkPaint&, bool lcd);
    // Determines if we need to use fake gamma (and contrast boost):
    static SkScalerContextFlags ComputeScalerContextFlags(const GrColorSpaceInfo&);

    void regenerateGlyphRunList(GrTextBlob* bmp,
                            GrGlyphCache*,
                            const GrShaderCaps&,
                            const SkPaint&,
                            GrColor filteredColor,
                            SkScalerContextFlags scalerContextFlags,
                            const SkMatrix& viewMatrix,
                            const SkSurfaceProps&,
                            const SkGlyphRunList& glyphRunList,
                            SkGlyphRunListPainter* glyphPainter);

    static void AppendGlyph(GrTextBlob*, int runIndex,
                            const sk_sp<GrTextStrike>&, const SkGlyph&,
                            GrGlyph::MaskStyle maskStyle, SkScalar sx, SkScalar sy,
                            GrColor color, SkGlyphCache*, SkScalar textRatio,
                            bool needsTransform);


    const GrDistanceFieldAdjustTable* dfAdjustTable() const { return fDistanceAdjustTable.get(); }

    sk_sp<const GrDistanceFieldAdjustTable> fDistanceAdjustTable;

    Options fOptions;

#if GR_TEST_UTILS
    static const SkScalerContextFlags kTextBlobOpScalerContextFlags =
            SkScalerContextFlags::kFakeGammaAndBoostContrast;
    GR_DRAW_OP_TEST_FRIEND(GrAtlasTextOp);
#endif
};

#endif  // GrTextContext_DEFINED
