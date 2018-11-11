/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.
 */

#ifndef SkGlyphCache_DEFINED
#define SkGlyphCache_DEFINED

#include "SkArenaAlloc.h"
#include "SkDescriptor.h"
#include "SkGlyph.h"
#include "SkGlyphRun.h"
#include "SkPaint.h"
#include "SkTHash.h"
#include "SkScalerContext.h"
#include "SkTemplates.h"
#include <memory>

/** \class SkGlyphCache

    This class represents a strike: a specific combination of typeface, size, matrix, etc., and
    holds the glyphs for that strike. Calling any of the getUnichar.../getGlyphID... methods will
    return the requested glyph, either instantly if it is already cached, or by first generating
    it and then adding it to the strike.

    The strikes are held in a global list, available to all threads. To interact with one, call
    either Find{OrCreate}Exclusive().

    The Find*Exclusive() method returns SkExclusiveStrikePtr, which releases exclusive ownership
    when they go out of scope.
*/
class SkGlyphCache : public SkGlyphCacheInterface {
public:
    SkGlyphCache(const SkDescriptor& desc,
                 std::unique_ptr<SkScalerContext> scaler,
                 const SkPaint::FontMetrics&);
    ~SkGlyphCache() override;

    const SkDescriptor& getDescriptor() const;

    /** Return true if glyph is cached. */
    bool isGlyphCached(SkGlyphID glyphID, SkFixed x, SkFixed y) const;

    /**  Return a glyph that has no information if it is not already filled out. */
    SkGlyph* getRawGlyphByID(SkPackedGlyphID);

    /** Returns a glyph with valid fAdvance and fDevKern fields. The remaining fields may be
        valid, but that is not guaranteed. If you require those, call getUnicharMetrics or
        getGlyphIDMetrics instead.
    */
    const SkGlyph& getUnicharAdvance(SkUnichar);
    const SkGlyph& getGlyphIDAdvance(SkGlyphID);

    /** Returns a glyph with all fields valid except fImage and fPath, which may be null. If they
        are null, call findImage or findPath for those. If they are not null, then they are valid.

        This call is potentially slower than the matching ...Advance call. If you only need the
        fAdvance/fDevKern fields, call those instead.
    */
    const SkGlyph& getUnicharMetrics(SkUnichar);
    const SkGlyph& getGlyphIDMetrics(SkGlyphID);

    /** These are variants that take the device position of the glyph. Call these only if you are
        drawing in subpixel mode. Passing 0, 0 is effectively the same as calling the variants
        w/o the extra params, though a tiny bit slower.
    */
    const SkGlyph& getUnicharMetrics(SkUnichar, SkFixed x, SkFixed y);
    const SkGlyph& getGlyphIDMetrics(uint16_t, SkFixed x, SkFixed y);

    void getAdvances(SkSpan<const SkGlyphID>, SkPoint[]);

    /** Return the glyphID for the specified Unichar. If the char has already been seen, use the
        existing cache entry. If not, ask the scalercontext to compute it for us.
    */
    SkGlyphID unicharToGlyph(SkUnichar);

    /** Map the glyph to its Unicode equivalent. Unmappable glyphs map to a character code of zero.
    */
    SkUnichar glyphToUnichar(SkGlyphID);

    /** Returns the number of glyphs for this strike.
    */
    unsigned getGlyphCount() const;

    /** Return the number of glyphs currently cached. */
    int countCachedGlyphs() const;

    /** Return the image associated with the glyph. If it has not been generated this will
        trigger that.
    */
    const void* findImage(const SkGlyph&);

    /** Initializes the image associated with the glyph with |data|.
     */
    void initializeImage(const volatile void* data, size_t size, SkGlyph*);

    /** If the advance axis intersects the glyph's path, append the positions scaled and offset
        to the array (if non-null), and set the count to the updated array length.
    */
    void findIntercepts(const SkScalar bounds[2], SkScalar scale, SkScalar xPos,
                        bool yAxis, SkGlyph* , SkScalar* array, int* count);

    /** Return the Path associated with the glyph. If it has not been generated this will trigger
        that.
    */
    const SkPath* findPath(const SkGlyph&);

    /** Initializes the path associated with the glyph with |data|. Returns false if
     *  data is invalid.
     */
    bool initializePath(SkGlyph*, const volatile void* data, size_t size);

    /** Fallback glyphs used during font remoting if the original glyph can't be found.
     */
    bool belongsToCache(const SkGlyph* glyph) const;
    /** Find any glyph in this cache with the given ID, regardless of subpixel positioning.
     *  If set and present, skip over the glyph with vetoID.
     */
    const SkGlyph* getCachedGlyphAnySubPix(SkGlyphID,
                                           SkPackedGlyphID vetoID = SkPackedGlyphID()) const;
    void initializeGlyphFromFallback(SkGlyph* glyph, const SkGlyph&);

    /** Return the vertical metrics for this strike.
    */
    const SkPaint::FontMetrics& getFontMetrics() const {
        return fFontMetrics;
    }

    SkMask::Format getMaskFormat() const {
        return fScalerContext->getMaskFormat();
    }

    bool isSubpixel() const {
        return fIsSubpixel;
    }

    SkVector rounding() const override;

    const SkGlyph& getGlyphMetrics(SkGlyphID glyphID, SkPoint position) override;

    /** Return the approx RAM usage for this cache. */
    size_t getMemoryUsed() const { return fMemoryUsed; }

    void dump() const;

    SkScalerContext* getScalerContext() const { return fScalerContext.get(); }

#ifdef SK_DEBUG
    void forceValidate() const;
    void validate() const;
#else
    void validate() const {}
#endif

    class AutoValidate : SkNoncopyable {
    public:
        AutoValidate(const SkGlyphCache* cache) : fCache(cache) {
            if (fCache) {
                fCache->validate();
            }
        }
        ~AutoValidate() {
            if (fCache) {
                fCache->validate();
            }
        }
        void forget() {
            fCache = nullptr;
        }
    private:
        const SkGlyphCache* fCache;
    };

private:
    enum MetricsType {
        kNothing_MetricsType,
        kJustAdvance_MetricsType,
        kFull_MetricsType
    };

    enum {
        kHashBits  = 8,
        kHashCount = 1 << kHashBits,
        kHashMask  = kHashCount - 1
    };

    struct CharGlyphRec {
        SkPackedUnicharID fPackedUnicharID;
        SkPackedGlyphID fPackedGlyphID;
    };

    // Return the SkGlyph* associated with MakeID. The id parameter is the
    // combined glyph/x/y id generated by MakeID. If it is just a glyph id
    // then x and y are assumed to be zero.
    SkGlyph* lookupByPackedGlyphID(SkPackedGlyphID packedGlyphID, MetricsType type);

    // Return a SkGlyph* associated with unicode id and position x and y.
    SkGlyph* lookupByChar(SkUnichar id, MetricsType type, SkFixed x = 0, SkFixed y = 0);

    // Return a new SkGlyph for the glyph ID and subpixel position id. Limit the amount
    // of work using type.
    SkGlyph* allocateNewGlyph(SkPackedGlyphID packedGlyphID, MetricsType type);

    // The id arg is a combined id generated by MakeID.
    CharGlyphRec* getCharGlyphRec(SkPackedUnicharID id);

    static void OffsetResults(const SkGlyph::Intercept* intercept, SkScalar scale,
                              SkScalar xPos, SkScalar* array, int* count);
    static void AddInterval(SkScalar val, SkGlyph::Intercept* intercept);
    static void AddPoints(const SkPoint* pts, int ptCount, const SkScalar bounds[2],
                          bool yAxis, SkGlyph::Intercept* intercept);
    static void AddLine(const SkPoint pts[2], SkScalar axis, bool yAxis,
                        SkGlyph::Intercept* intercept);
    static void AddQuad(const SkPoint pts[2], SkScalar axis, bool yAxis,
                        SkGlyph::Intercept* intercept);
    static void AddCubic(const SkPoint pts[3], SkScalar axis, bool yAxis,
                         SkGlyph::Intercept* intercept);
    static const SkGlyph::Intercept* MatchBounds(const SkGlyph* glyph,
                                                 const SkScalar bounds[2]);

    const SkAutoDescriptor fDesc;
    const std::unique_ptr<SkScalerContext> fScalerContext;
    SkPaint::FontMetrics   fFontMetrics;

    // Map from a combined GlyphID and sub-pixel position to a SkGlyph.
    SkTHashTable<SkGlyph, SkPackedGlyphID, SkGlyph::HashTraits> fGlyphMap;

    // so we don't grow our arrays a lot
    static constexpr size_t kMinGlyphCount = 8;
    static constexpr size_t kMinGlyphImageSize = 16 /* height */ * 8 /* width */;
    static constexpr size_t kMinAllocAmount = kMinGlyphImageSize * kMinGlyphCount;

    SkArenaAlloc            fAlloc {kMinAllocAmount};

    std::unique_ptr<CharGlyphRec[]> fPackedUnicharIDToPackedGlyphID;

    // used to track (approx) how much ram is tied-up in this cache
    size_t                  fMemoryUsed;

    const bool              fIsSubpixel;
    const SkAxisAlignment   fAxisAlignment;
};

#endif  // SkGlyphCache_DEFINED
