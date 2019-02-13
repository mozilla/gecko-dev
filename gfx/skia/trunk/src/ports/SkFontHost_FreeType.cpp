
/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkAdvancedTypefaceMetrics.h"
#include "SkBitmap.h"
#include "SkCanvas.h"
#include "SkColorPriv.h"
#include "SkDescriptor.h"
#include "SkFDot6.h"
#include "SkFloatingPoint.h"
#include "SkFontHost.h"
#include "SkFontHost_FreeType_common.h"
#include "SkGlyph.h"
#include "SkMask.h"
#include "SkMaskGamma.h"
#include "SkMatrix22.h"
#include "SkOTUtils.h"
#include "SkOnce.h"
#include "SkScalerContext.h"
#include "SkStream.h"
#include "SkString.h"
#include "SkTemplates.h"
#include "SkThread.h"

#if defined(SK_CAN_USE_DLOPEN)
#include <dlfcn.h>
#endif
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_SIZES_H
#include FT_TRUETYPE_TABLES_H
#include FT_TYPE1_TABLES_H
#include FT_BITMAP_H
// In the past, FT_GlyphSlot_Own_Bitmap was defined in this header file.
#include FT_SYNTHESIS_H
#include FT_XFREE86_H
#ifdef FT_LCD_FILTER_H
#include FT_LCD_FILTER_H
#endif

// Defined in FreeType 2.3.8 and later.
// This is a silly build time check, we would need a runtime check if we really cared.
#ifdef   FT_ADVANCES_H
#include FT_ADVANCES_H
#endif

#if 0
// Also include the files by name for build tools which require this.
#include <freetype/freetype.h>
#include <freetype/ftoutln.h>
#include <freetype/ftsizes.h>
#include <freetype/tttables.h>
#include <freetype/ftadvanc.h>
#include <freetype/ftlcdfil.h>
#include <freetype/ftbitmap.h>
#include <freetype/ftsynth.h>
#endif

// FT_LOAD_COLOR and the corresponding FT_Pixel_Mode::FT_PIXEL_MODE_BGRA
// were introduced in FreeType 2.5.0.
// The following may be removed once FreeType 2.5.0 is required to build.
#ifndef FT_LOAD_COLOR
#    define FT_LOAD_COLOR ( 1L << 20 )
#    define FT_PIXEL_MODE_BGRA 7
#endif

// FT_HAS_COLOR and the corresponding FT_FACE_FLAG_COLOR
// were introduced in FreeType 2.5.1
// The following may be removed once FreeType 2.5.1 is required to build.
#ifndef FT_HAS_COLOR
#    define FT_HAS_COLOR(face) false
#endif

//#define ENABLE_GLYPH_SPEW     // for tracing calls
//#define DUMP_STRIKE_CREATION

//#define SK_GAMMA_APPLY_TO_A8

using namespace skia_advanced_typeface_metrics_utils;

static bool isLCD(const SkScalerContext::Rec& rec) {
    switch (rec.fMaskFormat) {
        case SkMask::kLCD16_Format:
        case SkMask::kLCD32_Format:
            return true;
        default:
            return false;
    }
}

//////////////////////////////////////////////////////////////////////////

struct SkFaceRec;

SK_DECLARE_STATIC_MUTEX(gFTMutex);
static int          gFTCount;
static FT_Library   gFTLibrary;
static SkFaceRec*   gFaceRecHead;
static bool         gLCDSupportValid;  // true iff |gLCDSupport| has been set.
static bool         gLCDSupport;  // true iff LCD is supported by the runtime.
static int          gLCDExtra;  // number of extra pixels for filtering.

/////////////////////////////////////////////////////////////////////////

// FT_Library_SetLcdFilterWeights was introduced in FreeType 2.4.0.
// The following platforms provide FreeType of at least 2.4.0.
// Ubuntu >= 11.04 (previous deprecated April 2013)
// Debian >= 6.0 (good)
// OpenSuse >= 11.4 (previous deprecated January 2012 / Nov 2013 for Evergreen 11.2)
// Fedora >= 14 (good)
// Android >= Gingerbread (good)
typedef FT_Error (*FT_Library_SetLcdFilterWeightsProc)(FT_Library, unsigned char*);

// Caller must lock gFTMutex before calling this function.
static bool InitFreetype() {
    FT_Error err = FT_Init_FreeType(&gFTLibrary);
    if (err) {
        return false;
    }

    // Setup LCD filtering. This reduces color fringes for LCD smoothed glyphs.
#ifdef FT_LCD_FILTER_H
    // Use default { 0x10, 0x40, 0x70, 0x40, 0x10 }, as it adds up to 0x110, simulating ink spread.
    // SetLcdFilter must be called before SetLcdFilterWeights.
    err = FT_Library_SetLcdFilter(gFTLibrary, FT_LCD_FILTER_DEFAULT);
    if (0 == err) {
        gLCDSupport = true;
        gLCDExtra = 2; //Using a filter adds one full pixel to each side.

#ifdef SK_FONTHOST_FREETYPE_USE_NORMAL_LCD_FILTER
        // This also adds to 0x110 simulating ink spread, but provides better results than default.
        static unsigned char gGaussianLikeHeavyWeights[] = { 0x1A, 0x43, 0x56, 0x43, 0x1A, };

#if defined(SK_FONTHOST_FREETYPE_RUNTIME_VERSION) && \
            SK_FONTHOST_FREETYPE_RUNTIME_VERSION > 0x020400
        err = FT_Library_SetLcdFilterWeights(gFTLibrary, gGaussianLikeHeavyWeights);
#elif defined(SK_CAN_USE_DLOPEN) && SK_CAN_USE_DLOPEN == 1
        //The FreeType library is already loaded, so symbols are available in process.
        void* self = dlopen(NULL, RTLD_LAZY);
        if (NULL != self) {
            FT_Library_SetLcdFilterWeightsProc setLcdFilterWeights;
            //The following cast is non-standard, but safe for POSIX.
            *reinterpret_cast<void**>(&setLcdFilterWeights) = dlsym(self, "FT_Library_SetLcdFilterWeights");
            dlclose(self);

            if (NULL != setLcdFilterWeights) {
                err = setLcdFilterWeights(gFTLibrary, gGaussianLikeHeavyWeights);
            }
        }
#endif
#endif
    }
#else
    gLCDSupport = false;
#endif
    gLCDSupportValid = true;

    return true;
}

// Called while holding gFTMutex.
static void determine_lcd_support(bool* lcdSupported) {
    if (!gLCDSupportValid) {
        // This will determine LCD support as a side effect.
        InitFreetype();
        FT_Done_FreeType(gFTLibrary);
    }
    SkASSERT(gLCDSupportValid);
    *lcdSupported = gLCDSupport;
}

// Lazy, once, wrapper to ask the FreeType Library if it can support LCD text
static bool is_lcd_supported() {
    static bool lcdSupported = false;
    SkOnce(&gLCDSupportValid, &gFTMutex, determine_lcd_support, &lcdSupported);
    return lcdSupported;
}

class SkScalerContext_FreeType : public SkScalerContext_FreeType_Base {
public:
    SkScalerContext_FreeType(SkTypeface*, const SkDescriptor* desc);
    virtual ~SkScalerContext_FreeType();

    bool success() const {
        return fFaceRec != NULL &&
               fFTSize != NULL &&
               fFace != NULL;
    }

protected:
    virtual unsigned generateGlyphCount() SK_OVERRIDE;
    virtual uint16_t generateCharToGlyph(SkUnichar uni) SK_OVERRIDE;
    virtual void generateAdvance(SkGlyph* glyph) SK_OVERRIDE;
    virtual void generateMetrics(SkGlyph* glyph) SK_OVERRIDE;
    virtual void generateImage(const SkGlyph& glyph) SK_OVERRIDE;
    virtual void generatePath(const SkGlyph& glyph, SkPath* path) SK_OVERRIDE;
    virtual void generateFontMetrics(SkPaint::FontMetrics*) SK_OVERRIDE;
    virtual SkUnichar generateGlyphToChar(uint16_t glyph) SK_OVERRIDE;

private:
    SkFaceRec*  fFaceRec;
    FT_Face     fFace;              // reference to shared face in gFaceRecHead
    FT_Size     fFTSize;            // our own copy
    FT_Int      fStrikeIndex;
    SkFixed     fScaleX, fScaleY;
    FT_Matrix   fMatrix22;
    uint32_t    fLoadGlyphFlags;
    bool        fDoLinearMetrics;
    bool        fLCDIsVert;

    // Need scalar versions for generateFontMetrics
    SkVector    fScale;
    SkMatrix    fMatrix22Scalar;

    FT_Error setupSize();
    void getBBoxForCurrentGlyph(SkGlyph* glyph, FT_BBox* bbox,
                                bool snapToPixelBoundary = false);
    bool getCBoxForLetter(char letter, FT_BBox* bbox);
    // Caller must lock gFTMutex before calling this function.
    void updateGlyphIfLCD(SkGlyph* glyph);
    // Caller must lock gFTMutex before calling this function.
    // update FreeType2 glyph slot with glyph emboldened
    void emboldenIfNeeded(FT_Face face, FT_GlyphSlot glyph);
};

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

struct SkFaceRec {
    SkFaceRec*      fNext;
    FT_Face         fFace;
    FT_StreamRec    fFTStream;
    SkStream*       fSkStream;
    uint32_t        fRefCnt;
    uint32_t        fFontID;

    // assumes ownership of the stream, will call unref() when its done
    SkFaceRec(SkStream* strm, uint32_t fontID);
    ~SkFaceRec() {
        fSkStream->unref();
    }
};

extern "C" {
    static unsigned long sk_stream_read(FT_Stream       stream,
                                        unsigned long   offset,
                                        unsigned char*  buffer,
                                        unsigned long   count ) {
        SkStream* str = (SkStream*)stream->descriptor.pointer;

        if (count) {
            if (!str->rewind()) {
                return 0;
            } else {
                unsigned long ret;
                if (offset) {
                    ret = str->read(NULL, offset);
                    if (ret != offset) {
                        return 0;
                    }
                }
                ret = str->read(buffer, count);
                if (ret != count) {
                    return 0;
                }
                count = ret;
            }
        }
        return count;
    }

    static void sk_stream_close(FT_Stream) {}
}

SkFaceRec::SkFaceRec(SkStream* strm, uint32_t fontID)
        : fNext(NULL), fSkStream(strm), fRefCnt(1), fFontID(fontID) {
//    SkDEBUGF(("SkFaceRec: opening %s (%p)\n", key.c_str(), strm));

    sk_bzero(&fFTStream, sizeof(fFTStream));
    fFTStream.size = fSkStream->getLength();
    fFTStream.descriptor.pointer = fSkStream;
    fFTStream.read  = sk_stream_read;
    fFTStream.close = sk_stream_close;
}

// Will return 0 on failure
// Caller must lock gFTMutex before calling this function.
static SkFaceRec* ref_ft_face(const SkTypeface* typeface) {
    const SkFontID fontID = typeface->uniqueID();
    SkFaceRec* rec = gFaceRecHead;
    while (rec) {
        if (rec->fFontID == fontID) {
            SkASSERT(rec->fFace);
            rec->fRefCnt += 1;
            return rec;
        }
        rec = rec->fNext;
    }

    int face_index;
    SkStream* strm = typeface->openStream(&face_index);
    if (NULL == strm) {
        return NULL;
    }

    // this passes ownership of strm to the rec
    rec = SkNEW_ARGS(SkFaceRec, (strm, fontID));

    FT_Open_Args    args;
    memset(&args, 0, sizeof(args));
    const void* memoryBase = strm->getMemoryBase();

    if (NULL != memoryBase) {
//printf("mmap(%s)\n", keyString.c_str());
        args.flags = FT_OPEN_MEMORY;
        args.memory_base = (const FT_Byte*)memoryBase;
        args.memory_size = strm->getLength();
    } else {
//printf("fopen(%s)\n", keyString.c_str());
        args.flags = FT_OPEN_STREAM;
        args.stream = &rec->fFTStream;
    }

    FT_Error err = FT_Open_Face(gFTLibrary, &args, face_index, &rec->fFace);
    if (err) {    // bad filename, try the default font
        fprintf(stderr, "ERROR: unable to open font '%x'\n", fontID);
        SkDELETE(rec);
        return NULL;
    } else {
        SkASSERT(rec->fFace);
        //fprintf(stderr, "Opened font '%s'\n", filename.c_str());
        rec->fNext = gFaceRecHead;
        gFaceRecHead = rec;
        return rec;
    }
}

// Caller must lock gFTMutex before calling this function.
static void unref_ft_face(FT_Face face) {
    SkFaceRec*  rec = gFaceRecHead;
    SkFaceRec*  prev = NULL;
    while (rec) {
        SkFaceRec* next = rec->fNext;
        if (rec->fFace == face) {
            if (--rec->fRefCnt == 0) {
                if (prev) {
                    prev->fNext = next;
                } else {
                    gFaceRecHead = next;
                }
                FT_Done_Face(face);
                SkDELETE(rec);
            }
            return;
        }
        prev = rec;
        rec = next;
    }
    SkDEBUGFAIL("shouldn't get here, face not in list");
}

class AutoFTAccess {
public:
    AutoFTAccess(const SkTypeface* tf) : fRec(NULL), fFace(NULL) {
        gFTMutex.acquire();
        if (1 == ++gFTCount) {
            if (!InitFreetype()) {
                sk_throw();
            }
        }
        fRec = ref_ft_face(tf);
        if (fRec) {
            fFace = fRec->fFace;
        }
    }

    ~AutoFTAccess() {
        if (fFace) {
            unref_ft_face(fFace);
        }
        if (0 == --gFTCount) {
            FT_Done_FreeType(gFTLibrary);
        }
        gFTMutex.release();
    }

    SkFaceRec* rec() { return fRec; }
    FT_Face face() { return fFace; }

private:
    SkFaceRec*  fRec;
    FT_Face     fFace;
};

///////////////////////////////////////////////////////////////////////////

// Work around for old versions of freetype.
static FT_Error getAdvances(FT_Face face, FT_UInt start, FT_UInt count,
                           FT_Int32 loadFlags, FT_Fixed* advances) {
#ifdef FT_ADVANCES_H
    return FT_Get_Advances(face, start, count, loadFlags, advances);
#else
    if (!face || start >= face->num_glyphs ||
            start + count > face->num_glyphs || loadFlags != FT_LOAD_NO_SCALE) {
        return 6;  // "Invalid argument."
    }
    if (count == 0)
        return 0;

    for (int i = 0; i < count; i++) {
        FT_Error err = FT_Load_Glyph(face, start + i, FT_LOAD_NO_SCALE);
        if (err)
            return err;
        advances[i] = face->glyph->advance.x;
    }

    return 0;
#endif
}

static bool canEmbed(FT_Face face) {
#ifdef FT_FSTYPE_RESTRICTED_LICENSE_EMBEDDING
    FT_UShort fsType = FT_Get_FSType_Flags(face);
    return (fsType & (FT_FSTYPE_RESTRICTED_LICENSE_EMBEDDING |
                      FT_FSTYPE_BITMAP_EMBEDDING_ONLY)) == 0;
#else
    // No embedding is 0x2 and bitmap embedding only is 0x200.
    TT_OS2* os2_table;
    if ((os2_table = (TT_OS2*)FT_Get_Sfnt_Table(face, ft_sfnt_os2)) != NULL) {
        return (os2_table->fsType & 0x202) == 0;
    }
    return false;  // We tried, fail safe.
#endif
}

static bool canSubset(FT_Face face) {
#ifdef FT_FSTYPE_NO_SUBSETTING
    FT_UShort fsType = FT_Get_FSType_Flags(face);
    return (fsType & FT_FSTYPE_NO_SUBSETTING) == 0;
#else
    // No subset is 0x100.
    TT_OS2* os2_table;
    if ((os2_table = (TT_OS2*)FT_Get_Sfnt_Table(face, ft_sfnt_os2)) != NULL) {
        return (os2_table->fsType & 0x100) == 0;
    }
    return false;  // We tried, fail safe.
#endif
}

static bool GetLetterCBox(FT_Face face, char letter, FT_BBox* bbox) {
    const FT_UInt glyph_id = FT_Get_Char_Index(face, letter);
    if (!glyph_id)
        return false;
    if (FT_Load_Glyph(face, glyph_id, FT_LOAD_NO_SCALE) != 0)
        return false;
    FT_Outline_Get_CBox(&face->glyph->outline, bbox);
    return true;
}

static bool getWidthAdvance(FT_Face face, int gId, int16_t* data) {
    FT_Fixed advance = 0;
    if (getAdvances(face, gId, 1, FT_LOAD_NO_SCALE, &advance)) {
        return false;
    }
    SkASSERT(data);
    *data = advance;
    return true;
}

static void populate_glyph_to_unicode(FT_Face& face,
                                      SkTDArray<SkUnichar>* glyphToUnicode) {
    // Check and see if we have Unicode cmaps.
    for (int i = 0; i < face->num_charmaps; ++i) {
        // CMaps known to support Unicode:
        // Platform ID   Encoding ID   Name
        // -----------   -----------   -----------------------------------
        // 0             0,1           Apple Unicode
        // 0             3             Apple Unicode 2.0 (preferred)
        // 3             1             Microsoft Unicode UCS-2
        // 3             10            Microsoft Unicode UCS-4 (preferred)
        //
        // See Apple TrueType Reference Manual
        // http://developer.apple.com/fonts/TTRefMan/RM06/Chap6cmap.html
        // http://developer.apple.com/fonts/TTRefMan/RM06/Chap6name.html#ID
        // Microsoft OpenType Specification
        // http://www.microsoft.com/typography/otspec/cmap.htm

        FT_UShort platformId = face->charmaps[i]->platform_id;
        FT_UShort encodingId = face->charmaps[i]->encoding_id;

        if (platformId != 0 && platformId != 3) {
            continue;
        }
        if (platformId == 3 && encodingId != 1 && encodingId != 10) {
            continue;
        }
        bool preferredMap = ((platformId == 3 && encodingId == 10) ||
                             (platformId == 0 && encodingId == 3));

        FT_Set_Charmap(face, face->charmaps[i]);
        if (glyphToUnicode->isEmpty()) {
            glyphToUnicode->setCount(face->num_glyphs);
            memset(glyphToUnicode->begin(), 0,
                   sizeof(SkUnichar) * face->num_glyphs);
        }

        // Iterate through each cmap entry.
        FT_UInt glyphIndex;
        for (SkUnichar charCode = FT_Get_First_Char(face, &glyphIndex);
             glyphIndex != 0;
             charCode = FT_Get_Next_Char(face, charCode, &glyphIndex)) {
            if (charCode &&
                    ((*glyphToUnicode)[glyphIndex] == 0 || preferredMap)) {
                (*glyphToUnicode)[glyphIndex] = charCode;
            }
        }
    }
}

SkAdvancedTypefaceMetrics* SkTypeface_FreeType::onGetAdvancedTypefaceMetrics(
        SkAdvancedTypefaceMetrics::PerGlyphInfo perGlyphInfo,
        const uint32_t* glyphIDs,
        uint32_t glyphIDsCount) const {
#if defined(SK_BUILD_FOR_MAC)
    return NULL;
#else
    AutoFTAccess fta(this);
    FT_Face face = fta.face();
    if (!face) {
        return NULL;
    }

    SkAdvancedTypefaceMetrics* info = new SkAdvancedTypefaceMetrics;
    info->fFontName.set(FT_Get_Postscript_Name(face));
    info->fFlags = SkAdvancedTypefaceMetrics::kEmpty_FontFlag;
    if (FT_HAS_MULTIPLE_MASTERS(face)) {
        info->fFlags = SkTBitOr<SkAdvancedTypefaceMetrics::FontFlags>(
                info->fFlags, SkAdvancedTypefaceMetrics::kMultiMaster_FontFlag);
    }
    if (!canEmbed(face)) {
        info->fFlags = SkTBitOr<SkAdvancedTypefaceMetrics::FontFlags>(
                info->fFlags,
                SkAdvancedTypefaceMetrics::kNotEmbeddable_FontFlag);
    }
    if (!canSubset(face)) {
        info->fFlags = SkTBitOr<SkAdvancedTypefaceMetrics::FontFlags>(
                info->fFlags,
                SkAdvancedTypefaceMetrics::kNotSubsettable_FontFlag);
    }
    info->fLastGlyphID = face->num_glyphs - 1;
    info->fEmSize = 1000;

    bool cid = false;
    const char* fontType = FT_Get_X11_Font_Format(face);
    if (strcmp(fontType, "Type 1") == 0) {
        info->fType = SkAdvancedTypefaceMetrics::kType1_Font;
    } else if (strcmp(fontType, "CID Type 1") == 0) {
        info->fType = SkAdvancedTypefaceMetrics::kType1CID_Font;
        cid = true;
    } else if (strcmp(fontType, "CFF") == 0) {
        info->fType = SkAdvancedTypefaceMetrics::kCFF_Font;
    } else if (strcmp(fontType, "TrueType") == 0) {
        info->fType = SkAdvancedTypefaceMetrics::kTrueType_Font;
        cid = true;
        TT_Header* ttHeader;
        if ((ttHeader = (TT_Header*)FT_Get_Sfnt_Table(face,
                                                      ft_sfnt_head)) != NULL) {
            info->fEmSize = ttHeader->Units_Per_EM;
        }
    } else {
        info->fType = SkAdvancedTypefaceMetrics::kOther_Font;
    }

    info->fStyle = 0;
    if (FT_IS_FIXED_WIDTH(face))
        info->fStyle |= SkAdvancedTypefaceMetrics::kFixedPitch_Style;
    if (face->style_flags & FT_STYLE_FLAG_ITALIC)
        info->fStyle |= SkAdvancedTypefaceMetrics::kItalic_Style;

    PS_FontInfoRec ps_info;
    TT_Postscript* tt_info;
    if (FT_Get_PS_Font_Info(face, &ps_info) == 0) {
        info->fItalicAngle = ps_info.italic_angle;
    } else if ((tt_info =
                (TT_Postscript*)FT_Get_Sfnt_Table(face,
                                                  ft_sfnt_post)) != NULL) {
        info->fItalicAngle = SkFixedToScalar(tt_info->italicAngle);
    } else {
        info->fItalicAngle = 0;
    }

    info->fAscent = face->ascender;
    info->fDescent = face->descender;

    // Figure out a good guess for StemV - Min width of i, I, !, 1.
    // This probably isn't very good with an italic font.
    int16_t min_width = SHRT_MAX;
    info->fStemV = 0;
    char stem_chars[] = {'i', 'I', '!', '1'};
    for (size_t i = 0; i < SK_ARRAY_COUNT(stem_chars); i++) {
        FT_BBox bbox;
        if (GetLetterCBox(face, stem_chars[i], &bbox)) {
            int16_t width = bbox.xMax - bbox.xMin;
            if (width > 0 && width < min_width) {
                min_width = width;
                info->fStemV = min_width;
            }
        }
    }

    TT_PCLT* pclt_info;
    TT_OS2* os2_table;
    if ((pclt_info = (TT_PCLT*)FT_Get_Sfnt_Table(face, ft_sfnt_pclt)) != NULL) {
        info->fCapHeight = pclt_info->CapHeight;
        uint8_t serif_style = pclt_info->SerifStyle & 0x3F;
        if (serif_style >= 2 && serif_style <= 6)
            info->fStyle |= SkAdvancedTypefaceMetrics::kSerif_Style;
        else if (serif_style >= 9 && serif_style <= 12)
            info->fStyle |= SkAdvancedTypefaceMetrics::kScript_Style;
    } else if (((os2_table = (TT_OS2*)FT_Get_Sfnt_Table(face, ft_sfnt_os2)) != NULL) &&
               // sCapHeight is available only when version 2 or later.
               os2_table->version != 0xFFFF &&
               os2_table->version >= 2) {
        info->fCapHeight = os2_table->sCapHeight;
    } else {
        // Figure out a good guess for CapHeight: average the height of M and X.
        FT_BBox m_bbox, x_bbox;
        bool got_m, got_x;
        got_m = GetLetterCBox(face, 'M', &m_bbox);
        got_x = GetLetterCBox(face, 'X', &x_bbox);
        if (got_m && got_x) {
            info->fCapHeight = (m_bbox.yMax - m_bbox.yMin + x_bbox.yMax -
                    x_bbox.yMin) / 2;
        } else if (got_m && !got_x) {
            info->fCapHeight = m_bbox.yMax - m_bbox.yMin;
        } else if (!got_m && got_x) {
            info->fCapHeight = x_bbox.yMax - x_bbox.yMin;
        } else {
            // Last resort, use the ascent.
            info->fCapHeight = info->fAscent;
        }
    }

    info->fBBox = SkIRect::MakeLTRB(face->bbox.xMin, face->bbox.yMax,
                                    face->bbox.xMax, face->bbox.yMin);

    if (!FT_IS_SCALABLE(face)) {
        perGlyphInfo = SkAdvancedTypefaceMetrics::kNo_PerGlyphInfo;
    }

    if (perGlyphInfo & SkAdvancedTypefaceMetrics::kHAdvance_PerGlyphInfo) {
        if (FT_IS_FIXED_WIDTH(face)) {
            appendRange(&info->fGlyphWidths, 0);
            int16_t advance = face->max_advance_width;
            info->fGlyphWidths->fAdvance.append(1, &advance);
            finishRange(info->fGlyphWidths.get(), 0,
                        SkAdvancedTypefaceMetrics::WidthRange::kDefault);
        } else if (!cid) {
            appendRange(&info->fGlyphWidths, 0);
            // So as to not blow out the stack, get advances in batches.
            for (int gID = 0; gID < face->num_glyphs; gID += 128) {
                FT_Fixed advances[128];
                int advanceCount = 128;
                if (gID + advanceCount > face->num_glyphs)
                    advanceCount = face->num_glyphs - gID;
                getAdvances(face, gID, advanceCount, FT_LOAD_NO_SCALE,
                            advances);
                for (int i = 0; i < advanceCount; i++) {
                    int16_t advance = advances[i];
                    info->fGlyphWidths->fAdvance.append(1, &advance);
                }
            }
            finishRange(info->fGlyphWidths.get(), face->num_glyphs - 1,
                        SkAdvancedTypefaceMetrics::WidthRange::kRange);
        } else {
            info->fGlyphWidths.reset(
                getAdvanceData(face,
                               face->num_glyphs,
                               glyphIDs,
                               glyphIDsCount,
                               &getWidthAdvance));
        }
    }

    if (perGlyphInfo & SkAdvancedTypefaceMetrics::kVAdvance_PerGlyphInfo &&
            FT_HAS_VERTICAL(face)) {
        SkASSERT(false);  // Not implemented yet.
    }

    if (perGlyphInfo & SkAdvancedTypefaceMetrics::kGlyphNames_PerGlyphInfo &&
            info->fType == SkAdvancedTypefaceMetrics::kType1_Font) {
        // Postscript fonts may contain more than 255 glyphs, so we end up
        // using multiple font descriptions with a glyph ordering.  Record
        // the name of each glyph.
        info->fGlyphNames.reset(
                new SkAutoTArray<SkString>(face->num_glyphs));
        for (int gID = 0; gID < face->num_glyphs; gID++) {
            char glyphName[128];  // PS limit for names is 127 bytes.
            FT_Get_Glyph_Name(face, gID, glyphName, 128);
            info->fGlyphNames->get()[gID].set(glyphName);
        }
    }

    if (perGlyphInfo & SkAdvancedTypefaceMetrics::kToUnicode_PerGlyphInfo &&
           info->fType != SkAdvancedTypefaceMetrics::kType1_Font &&
           face->num_charmaps) {
        populate_glyph_to_unicode(face, &(info->fGlyphToUnicode));
    }

    return info;
#endif
}

///////////////////////////////////////////////////////////////////////////

static bool bothZero(SkScalar a, SkScalar b) {
    return 0 == a && 0 == b;
}

// returns false if there is any non-90-rotation or skew
static bool isAxisAligned(const SkScalerContext::Rec& rec) {
    return 0 == rec.fPreSkewX &&
           (bothZero(rec.fPost2x2[0][1], rec.fPost2x2[1][0]) ||
            bothZero(rec.fPost2x2[0][0], rec.fPost2x2[1][1]));
}

SkScalerContext* SkTypeface_FreeType::onCreateScalerContext(
                                               const SkDescriptor* desc) const {
    SkScalerContext_FreeType* c = SkNEW_ARGS(SkScalerContext_FreeType,
                                        (const_cast<SkTypeface_FreeType*>(this),
                                         desc));
    if (!c->success()) {
        SkDELETE(c);
        c = NULL;
    }
    return c;
}

void SkTypeface_FreeType::onFilterRec(SkScalerContextRec* rec) const {
    //BOGUS: http://code.google.com/p/chromium/issues/detail?id=121119
    //Cap the requested size as larger sizes give bogus values.
    //Remove when http://code.google.com/p/skia/issues/detail?id=554 is fixed.
    if (rec->fTextSize > SkIntToScalar(1 << 14)) {
        rec->fTextSize = SkIntToScalar(1 << 14);
    }

    if (!is_lcd_supported() && isLCD(*rec)) {
        // If the runtime Freetype library doesn't support LCD mode, we disable
        // it here.
        rec->fMaskFormat = SkMask::kA8_Format;
    }

    SkPaint::Hinting h = rec->getHinting();
    if (SkPaint::kFull_Hinting == h && !isLCD(*rec)) {
        // collapse full->normal hinting if we're not doing LCD
        h = SkPaint::kNormal_Hinting;
    }
    if ((rec->fFlags & SkScalerContext::kSubpixelPositioning_Flag)) {
        if (SkPaint::kNo_Hinting != h) {
            h = SkPaint::kSlight_Hinting;
        }
    }

    // rotated text looks bad with hinting, so we disable it as needed
    if (!isAxisAligned(*rec)) {
        h = SkPaint::kNo_Hinting;
    }
    rec->setHinting(h);

#ifndef SK_GAMMA_APPLY_TO_A8
    if (!isLCD(*rec)) {
      rec->ignorePreBlend();
    }
#endif
}

int SkTypeface_FreeType::onGetUPEM() const {
    AutoFTAccess fta(this);
    FT_Face face = fta.face();
    return face ? face->units_per_EM : 0;
}

bool SkTypeface_FreeType::onGetKerningPairAdjustments(const uint16_t glyphs[],
                                      int count, int32_t adjustments[]) const {
    AutoFTAccess fta(this);
    FT_Face face = fta.face();
    if (!face || !FT_HAS_KERNING(face)) {
        return false;
    }

    for (int i = 0; i < count - 1; ++i) {
        FT_Vector delta;
        FT_Error err = FT_Get_Kerning(face, glyphs[i], glyphs[i+1],
                                      FT_KERNING_UNSCALED, &delta);
        if (err) {
            return false;
        }
        adjustments[i] = delta.x;
    }
    return true;
}

static FT_Int chooseBitmapStrike(FT_Face face, SkFixed scaleY) {
    // early out if face is bad
    if (face == NULL) {
        SkDEBUGF(("chooseBitmapStrike aborted due to NULL face\n"));
        return -1;
    }
    // determine target ppem
    FT_Pos targetPPEM = SkFixedToFDot6(scaleY);
    // find a bitmap strike equal to or just larger than the requested size
    FT_Int chosenStrikeIndex = -1;
    FT_Pos chosenPPEM = 0;
    for (FT_Int strikeIndex = 0; strikeIndex < face->num_fixed_sizes; ++strikeIndex) {
        FT_Pos thisPPEM = face->available_sizes[strikeIndex].y_ppem;
        if (thisPPEM == targetPPEM) {
            // exact match - our search stops here
            chosenPPEM = thisPPEM;
            chosenStrikeIndex = strikeIndex;
            break;
        } else if (chosenPPEM < targetPPEM) {
            // attempt to increase chosenPPEM
            if (thisPPEM > chosenPPEM) {
                chosenPPEM = thisPPEM;
                chosenStrikeIndex = strikeIndex;
            }
        } else {
            // attempt to decrease chosenPPEM, but not below targetPPEM
            if (thisPPEM < chosenPPEM && thisPPEM > targetPPEM) {
                chosenPPEM = thisPPEM;
                chosenStrikeIndex = strikeIndex;
            }
        }
    }
    if (chosenStrikeIndex != -1) {
        // use the chosen strike
        FT_Error err = FT_Select_Size(face, chosenStrikeIndex);
        if (err != 0) {
            SkDEBUGF(("FT_Select_Size(%s, %d) returned 0x%x\n", face->family_name,
                      chosenStrikeIndex, err));
            chosenStrikeIndex = -1;
        }
    }
    return chosenStrikeIndex;
}

SkScalerContext_FreeType::SkScalerContext_FreeType(SkTypeface* typeface,
                                                   const SkDescriptor* desc)
        : SkScalerContext_FreeType_Base(typeface, desc) {
    SkAutoMutexAcquire  ac(gFTMutex);

    if (gFTCount == 0) {
        if (!InitFreetype()) {
            sk_throw();
        }
    }
    ++gFTCount;

    // load the font file
    fStrikeIndex = -1;
    fFTSize = NULL;
    fFace = NULL;
    fFaceRec = ref_ft_face(typeface);
    if (NULL == fFaceRec) {
        return;
    }
    fFace = fFaceRec->fFace;

    // A is the total matrix.
    SkMatrix A;
    fRec.getSingleMatrix(&A);

    SkScalar sx = A.getScaleX();
    SkScalar sy = A.getScaleY();
    fMatrix22Scalar.reset();

    // In GDI, the hinter is aware of the current transformation
    // (the transform is in some sense applied before/with the hinting).
    // The bytecode can then test if it is rotated or stretched and decide
    // to apply instructions or not.
    //
    // FreeType, however, always does the transformation strictly after hinting.
    // It just sets 'rotated' and 'stretched' to false and only applies the
    // size before hinting.
    //
    // Also, FreeType respects the head::flags::IntegerScaling flag,
    // (although this is patched out on most major distros)
    // so it is critical to get the size correct on the request.
    //
    // This also gets us the actual closest size on bitmap fonts as well.
    if (A.getSkewX() || A.getSkewY() || sx < 0 || sy < 0) {
        // h is where A maps the horizontal baseline.
        SkPoint h = SkPoint::Make(SK_Scalar1, 0);
        A.mapPoints(&h, 1);

        // G is the Givens Matrix for A (rotational matrix where GA[0][1] == 0).
        SkMatrix G;
        SkComputeGivensRotation(h, &G);

        // GA is the matrix A with rotation removed.
        SkMatrix GA(G);
        GA.preConcat(A);

        sx = SkScalarAbs(GA.get(SkMatrix::kMScaleX));
        sy = SkScalarAbs(GA.get(SkMatrix::kMScaleY));

        // sA is the total matrix A without the text scale.
        SkMatrix sA(A);
        sA.preScale(SkScalarInvert(sx), SkScalarInvert(sy)); //remove text size

        fMatrix22Scalar.setScaleX(sA.getScaleX());
        fMatrix22Scalar.setSkewX(-sA.getSkewX());
        fMatrix22Scalar.setSkewY(-sA.getSkewY());
        fMatrix22Scalar.setScaleY(sA.getScaleY());
    }
    fScale.set(sx, sy);
    fScaleX = SkScalarToFixed(sx);
    fScaleY = SkScalarToFixed(sy);
    fMatrix22.xx = SkScalarToFixed(fMatrix22Scalar.getScaleX());
    fMatrix22.xy = SkScalarToFixed(fMatrix22Scalar.getSkewX());
    fMatrix22.yx = SkScalarToFixed(fMatrix22Scalar.getSkewY());
    fMatrix22.yy = SkScalarToFixed(fMatrix22Scalar.getScaleY());

    fLCDIsVert = SkToBool(fRec.fFlags & SkScalerContext::kLCD_Vertical_Flag);

    // compute the flags we send to Load_Glyph
    bool linearMetrics = SkToBool(fRec.fFlags & SkScalerContext::kSubpixelPositioning_Flag);
    {
        FT_Int32 loadFlags = FT_LOAD_DEFAULT;

        if (SkMask::kBW_Format == fRec.fMaskFormat) {
            // See http://code.google.com/p/chromium/issues/detail?id=43252#c24
            loadFlags = FT_LOAD_TARGET_MONO;
            if (fRec.getHinting() == SkPaint::kNo_Hinting) {
                loadFlags = FT_LOAD_NO_HINTING;
                linearMetrics = true;
            }
        } else {
            switch (fRec.getHinting()) {
            case SkPaint::kNo_Hinting:
                loadFlags = FT_LOAD_NO_HINTING;
                linearMetrics = true;
                break;
            case SkPaint::kSlight_Hinting:
                loadFlags = FT_LOAD_TARGET_LIGHT;  // This implies FORCE_AUTOHINT
                break;
            case SkPaint::kNormal_Hinting:
                if (fRec.fFlags & SkScalerContext::kForceAutohinting_Flag) {
                    loadFlags = FT_LOAD_FORCE_AUTOHINT;
                }
                break;
            case SkPaint::kFull_Hinting:
                if (fRec.fFlags & SkScalerContext::kForceAutohinting_Flag) {
                    loadFlags = FT_LOAD_FORCE_AUTOHINT;
                    break;
                }
                loadFlags = FT_LOAD_TARGET_NORMAL;
                if (isLCD(fRec)) {
                    if (fLCDIsVert) {
                        loadFlags = FT_LOAD_TARGET_LCD_V;
                    } else {
                        loadFlags = FT_LOAD_TARGET_LCD;
                    }
                }
                break;
            default:
                SkDebugf("---------- UNKNOWN hinting %d\n", fRec.getHinting());
                break;
            }
        }

        if ((fRec.fFlags & SkScalerContext::kEmbeddedBitmapText_Flag) == 0) {
            loadFlags |= FT_LOAD_NO_BITMAP;
        }

        // Always using FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH to get correct
        // advances, as fontconfig and cairo do.
        // See http://code.google.com/p/skia/issues/detail?id=222.
        loadFlags |= FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH;

        // Use vertical layout if requested.
        if (fRec.fFlags & SkScalerContext::kVertical_Flag) {
            loadFlags |= FT_LOAD_VERTICAL_LAYOUT;
        }

        loadFlags |= FT_LOAD_COLOR;

        fLoadGlyphFlags = loadFlags;
    }

    FT_Error err = FT_New_Size(fFace, &fFTSize);
    if (err != 0) {
        SkDEBUGF(("FT_New_Size returned %x for face %s\n", err, fFace->family_name));
        fFace = NULL;
        return;
    }
    err = FT_Activate_Size(fFTSize);
    if (err != 0) {
        SkDEBUGF(("FT_Activate_Size(%08x, 0x%x, 0x%x) returned 0x%x\n", fFace, fScaleX, fScaleY,
                  err));
        fFTSize = NULL;
        return;
    }

    if (FT_IS_SCALABLE(fFace)) {
        err = FT_Set_Char_Size(fFace, SkFixedToFDot6(fScaleX), SkFixedToFDot6(fScaleY), 72, 72);
        if (err != 0) {
            SkDEBUGF(("FT_Set_CharSize(%08x, 0x%x, 0x%x) returned 0x%x\n",
                                    fFace, fScaleX, fScaleY,      err));
            fFace = NULL;
            return;
        }
        FT_Set_Transform(fFace, &fMatrix22, NULL);
    } else if (FT_HAS_FIXED_SIZES(fFace)) {
        fStrikeIndex = chooseBitmapStrike(fFace, fScaleY);
        if (fStrikeIndex == -1) {
            SkDEBUGF(("no glyphs for font \"%s\" size %f?\n",
                            fFace->family_name,       SkFixedToScalar(fScaleY)));
        } else {
            // FreeType does no provide linear metrics for bitmap fonts.
            linearMetrics = false;

            // FreeType documentation says:
            // FT_LOAD_NO_BITMAP -- Ignore bitmap strikes when loading.
            // Bitmap-only fonts ignore this flag.
            //
            // However, in FreeType 2.5.1 color bitmap only fonts do not ignore this flag.
            // Force this flag off for bitmap only fonts.
            fLoadGlyphFlags &= ~FT_LOAD_NO_BITMAP;
        }
    } else {
        SkDEBUGF(("unknown kind of font \"%s\" size %f?\n",
                            fFace->family_name,       SkFixedToScalar(fScaleY)));
    }

    fDoLinearMetrics = linearMetrics;
}

SkScalerContext_FreeType::~SkScalerContext_FreeType() {
    SkAutoMutexAcquire  ac(gFTMutex);

    if (fFTSize != NULL) {
        FT_Done_Size(fFTSize);
    }

    if (fFace != NULL) {
        unref_ft_face(fFace);
    }
    if (--gFTCount == 0) {
        FT_Done_FreeType(gFTLibrary);
        SkDEBUGCODE(gFTLibrary = NULL;)
    }
}

/*  We call this before each use of the fFace, since we may be sharing
    this face with other context (at different sizes).
*/
FT_Error SkScalerContext_FreeType::setupSize() {
    FT_Error err = FT_Activate_Size(fFTSize);
    if (err != 0) {
        SkDEBUGF(("SkScalerContext_FreeType::FT_Activate_Size(%x, 0x%x, 0x%x) returned 0x%x\n",
                  fFaceRec->fFontID, fScaleX, fScaleY, err));
        fFTSize = NULL;
        return err;
    }

    // seems we need to reset this every time (not sure why, but without it
    // I get random italics from some other fFTSize)
    FT_Set_Transform(fFace, &fMatrix22, NULL);
    return 0;
}

unsigned SkScalerContext_FreeType::generateGlyphCount() {
    return fFace->num_glyphs;
}

uint16_t SkScalerContext_FreeType::generateCharToGlyph(SkUnichar uni) {
    return SkToU16(FT_Get_Char_Index( fFace, uni ));
}

SkUnichar SkScalerContext_FreeType::generateGlyphToChar(uint16_t glyph) {
    // iterate through each cmap entry, looking for matching glyph indices
    FT_UInt glyphIndex;
    SkUnichar charCode = FT_Get_First_Char( fFace, &glyphIndex );

    while (glyphIndex != 0) {
        if (glyphIndex == glyph) {
            return charCode;
        }
        charCode = FT_Get_Next_Char( fFace, charCode, &glyphIndex );
    }

    return 0;
}

void SkScalerContext_FreeType::generateAdvance(SkGlyph* glyph) {
#ifdef FT_ADVANCES_H
   /* unhinted and light hinted text have linearly scaled advances
    * which are very cheap to compute with some font formats...
    */
    if (fDoLinearMetrics) {
        SkAutoMutexAcquire  ac(gFTMutex);

        if (this->setupSize()) {
            glyph->zeroMetrics();
            return;
        }

        FT_Error    error;
        FT_Fixed    advance;

        error = FT_Get_Advance( fFace, glyph->getGlyphID(fBaseGlyphCount),
                                fLoadGlyphFlags | FT_ADVANCE_FLAG_FAST_ONLY,
                                &advance );
        if (0 == error) {
            glyph->fRsbDelta = 0;
            glyph->fLsbDelta = 0;
            glyph->fAdvanceX = SkFixedMul(fMatrix22.xx, advance);
            glyph->fAdvanceY = - SkFixedMul(fMatrix22.yx, advance);
            return;
        }
    }
#endif /* FT_ADVANCES_H */
    /* otherwise, we need to load/hint the glyph, which is slower */
    this->generateMetrics(glyph);
    return;
}

void SkScalerContext_FreeType::getBBoxForCurrentGlyph(SkGlyph* glyph,
                                                      FT_BBox* bbox,
                                                      bool snapToPixelBoundary) {

    FT_Outline_Get_CBox(&fFace->glyph->outline, bbox);

    if (fRec.fFlags & SkScalerContext::kSubpixelPositioning_Flag) {
        int dx = SkFixedToFDot6(glyph->getSubXFixed());
        int dy = SkFixedToFDot6(glyph->getSubYFixed());
        // negate dy since freetype-y-goes-up and skia-y-goes-down
        bbox->xMin += dx;
        bbox->yMin -= dy;
        bbox->xMax += dx;
        bbox->yMax -= dy;
    }

    // outset the box to integral boundaries
    if (snapToPixelBoundary) {
        bbox->xMin &= ~63;
        bbox->yMin &= ~63;
        bbox->xMax  = (bbox->xMax + 63) & ~63;
        bbox->yMax  = (bbox->yMax + 63) & ~63;
    }

    // Must come after snapToPixelBoundary so that the width and height are
    // consistent. Otherwise asserts will fire later on when generating the
    // glyph image.
    if (fRec.fFlags & SkScalerContext::kVertical_Flag) {
        FT_Vector vector;
        vector.x = fFace->glyph->metrics.vertBearingX - fFace->glyph->metrics.horiBearingX;
        vector.y = -fFace->glyph->metrics.vertBearingY - fFace->glyph->metrics.horiBearingY;
        FT_Vector_Transform(&vector, &fMatrix22);
        bbox->xMin += vector.x;
        bbox->xMax += vector.x;
        bbox->yMin += vector.y;
        bbox->yMax += vector.y;
    }
}

bool SkScalerContext_FreeType::getCBoxForLetter(char letter, FT_BBox* bbox) {
    const FT_UInt glyph_id = FT_Get_Char_Index(fFace, letter);
    if (!glyph_id)
        return false;
    if (FT_Load_Glyph(fFace, glyph_id, fLoadGlyphFlags) != 0)
        return false;
    emboldenIfNeeded(fFace, fFace->glyph);
    FT_Outline_Get_CBox(&fFace->glyph->outline, bbox);
    return true;
}

void SkScalerContext_FreeType::updateGlyphIfLCD(SkGlyph* glyph) {
    if (isLCD(fRec)) {
        if (fLCDIsVert) {
            glyph->fHeight += gLCDExtra;
            glyph->fTop -= gLCDExtra >> 1;
        } else {
            glyph->fWidth += gLCDExtra;
            glyph->fLeft -= gLCDExtra >> 1;
        }
    }
}

inline void scaleGlyphMetrics(SkGlyph& glyph, SkScalar scale) {
    glyph.fWidth *= scale;
    glyph.fHeight *= scale;
    glyph.fTop *= scale;
    glyph.fLeft *= scale;

    SkFixed fixedScale = SkScalarToFixed(scale);
    glyph.fAdvanceX = SkFixedMul(glyph.fAdvanceX, fixedScale);
    glyph.fAdvanceY = SkFixedMul(glyph.fAdvanceY, fixedScale);
}

void SkScalerContext_FreeType::generateMetrics(SkGlyph* glyph) {
    SkAutoMutexAcquire  ac(gFTMutex);

    glyph->fRsbDelta = 0;
    glyph->fLsbDelta = 0;

    FT_Error    err;

    if (this->setupSize()) {
        goto ERROR;
    }

    err = FT_Load_Glyph( fFace, glyph->getGlyphID(fBaseGlyphCount), fLoadGlyphFlags );
    if (err != 0) {
#if 0
        SkDEBUGF(("SkScalerContext_FreeType::generateMetrics(%x): FT_Load_Glyph(glyph:%d flags:%x) returned 0x%x\n",
                    fFaceRec->fFontID, glyph->getGlyphID(fBaseGlyphCount), fLoadGlyphFlags, err));
#endif
    ERROR:
        glyph->zeroMetrics();
        return;
    }
    emboldenIfNeeded(fFace, fFace->glyph);

    switch ( fFace->glyph->format ) {
      case FT_GLYPH_FORMAT_OUTLINE:
        if (0 == fFace->glyph->outline.n_contours) {
            glyph->fWidth = 0;
            glyph->fHeight = 0;
            glyph->fTop = 0;
            glyph->fLeft = 0;
        } else {
            FT_BBox bbox;
            getBBoxForCurrentGlyph(glyph, &bbox, true);

            glyph->fWidth   = SkToU16(SkFDot6Floor(bbox.xMax - bbox.xMin));
            glyph->fHeight  = SkToU16(SkFDot6Floor(bbox.yMax - bbox.yMin));
            glyph->fTop     = -SkToS16(SkFDot6Floor(bbox.yMax));
            glyph->fLeft    = SkToS16(SkFDot6Floor(bbox.xMin));

            updateGlyphIfLCD(glyph);
        }
        break;

      case FT_GLYPH_FORMAT_BITMAP:
        if (fRec.fFlags & SkScalerContext::kVertical_Flag) {
            FT_Vector vector;
            vector.x = fFace->glyph->metrics.vertBearingX - fFace->glyph->metrics.horiBearingX;
            vector.y = -fFace->glyph->metrics.vertBearingY - fFace->glyph->metrics.horiBearingY;
            FT_Vector_Transform(&vector, &fMatrix22);
            fFace->glyph->bitmap_left += SkFDot6Floor(vector.x);
            fFace->glyph->bitmap_top  += SkFDot6Floor(vector.y);
        }

        if (fFace->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
            glyph->fMaskFormat = SkMask::kARGB32_Format;
        }

        glyph->fWidth   = SkToU16(fFace->glyph->bitmap.width);
        glyph->fHeight  = SkToU16(fFace->glyph->bitmap.rows);
        glyph->fTop     = -SkToS16(fFace->glyph->bitmap_top);
        glyph->fLeft    = SkToS16(fFace->glyph->bitmap_left);
        break;

      default:
        SkDEBUGFAIL("unknown glyph format");
        goto ERROR;
    }

    if (fRec.fFlags & SkScalerContext::kVertical_Flag) {
        if (fDoLinearMetrics) {
            glyph->fAdvanceX = -SkFixedMul(fMatrix22.xy, fFace->glyph->linearVertAdvance);
            glyph->fAdvanceY = SkFixedMul(fMatrix22.yy, fFace->glyph->linearVertAdvance);
        } else {
            glyph->fAdvanceX = -SkFDot6ToFixed(fFace->glyph->advance.x);
            glyph->fAdvanceY = SkFDot6ToFixed(fFace->glyph->advance.y);
        }
    } else {
        if (fDoLinearMetrics) {
            glyph->fAdvanceX = SkFixedMul(fMatrix22.xx, fFace->glyph->linearHoriAdvance);
            glyph->fAdvanceY = -SkFixedMul(fMatrix22.yx, fFace->glyph->linearHoriAdvance);
        } else {
            glyph->fAdvanceX = SkFDot6ToFixed(fFace->glyph->advance.x);
            glyph->fAdvanceY = -SkFDot6ToFixed(fFace->glyph->advance.y);

            if (fRec.fFlags & kDevKernText_Flag) {
                glyph->fRsbDelta = SkToS8(fFace->glyph->rsb_delta);
                glyph->fLsbDelta = SkToS8(fFace->glyph->lsb_delta);
            }
        }
    }

    if (fFace->glyph->format == FT_GLYPH_FORMAT_BITMAP && fScaleY && fFace->size->metrics.y_ppem) {
        // NOTE: both dimensions are scaled by y_ppem. this is WAI.
        scaleGlyphMetrics(*glyph, SkScalarDiv(SkFixedToScalar(fScaleY),
                                              SkIntToScalar(fFace->size->metrics.y_ppem)));
    }

#ifdef ENABLE_GLYPH_SPEW
    SkDEBUGF(("FT_Set_Char_Size(this:%p sx:%x sy:%x ", this, fScaleX, fScaleY));
    SkDEBUGF(("Metrics(glyph:%d flags:0x%x) w:%d\n", glyph->getGlyphID(fBaseGlyphCount), fLoadGlyphFlags, glyph->fWidth));
#endif
}


void SkScalerContext_FreeType::generateImage(const SkGlyph& glyph) {
    SkAutoMutexAcquire  ac(gFTMutex);

    FT_Error    err;

    if (this->setupSize()) {
        goto ERROR;
    }

    err = FT_Load_Glyph( fFace, glyph.getGlyphID(fBaseGlyphCount), fLoadGlyphFlags);
    if (err != 0) {
        SkDEBUGF(("SkScalerContext_FreeType::generateImage: FT_Load_Glyph(glyph:%d width:%d height:%d rb:%d flags:%d) returned 0x%x\n",
                    glyph.getGlyphID(fBaseGlyphCount), glyph.fWidth, glyph.fHeight, glyph.rowBytes(), fLoadGlyphFlags, err));
    ERROR:
        memset(glyph.fImage, 0, glyph.rowBytes() * glyph.fHeight);
        return;
    }

    emboldenIfNeeded(fFace, fFace->glyph);
    generateGlyphImage(fFace, glyph);
}


void SkScalerContext_FreeType::generatePath(const SkGlyph& glyph,
                                            SkPath* path) {
    SkAutoMutexAcquire  ac(gFTMutex);

    SkASSERT(path);

    if (this->setupSize()) {
        path->reset();
        return;
    }

    uint32_t flags = fLoadGlyphFlags;
    flags |= FT_LOAD_NO_BITMAP; // ignore embedded bitmaps so we're sure to get the outline
    flags &= ~FT_LOAD_RENDER;   // don't scan convert (we just want the outline)

    FT_Error err = FT_Load_Glyph( fFace, glyph.getGlyphID(fBaseGlyphCount), flags);

    if (err != 0) {
        SkDEBUGF(("SkScalerContext_FreeType::generatePath: FT_Load_Glyph(glyph:%d flags:%d) returned 0x%x\n",
                    glyph.getGlyphID(fBaseGlyphCount), flags, err));
        path->reset();
        return;
    }
    emboldenIfNeeded(fFace, fFace->glyph);

    generateGlyphPath(fFace, path);

    // The path's origin from FreeType is always the horizontal layout origin.
    // Offset the path so that it is relative to the vertical origin if needed.
    if (fRec.fFlags & SkScalerContext::kVertical_Flag) {
        FT_Vector vector;
        vector.x = fFace->glyph->metrics.vertBearingX - fFace->glyph->metrics.horiBearingX;
        vector.y = -fFace->glyph->metrics.vertBearingY - fFace->glyph->metrics.horiBearingY;
        FT_Vector_Transform(&vector, &fMatrix22);
        path->offset(SkFDot6ToScalar(vector.x), -SkFDot6ToScalar(vector.y));
    }
}

void SkScalerContext_FreeType::generateFontMetrics(SkPaint::FontMetrics* metrics) {
    if (NULL == metrics) {
        return;
    }

    SkAutoMutexAcquire ac(gFTMutex);

    if (this->setupSize()) {
        ERROR:
        sk_bzero(metrics, sizeof(*metrics));
        return;
    }

    FT_Face face = fFace;
    SkScalar scaleX = fScale.x();
    SkScalar scaleY = fScale.y();
    SkScalar mxy = fMatrix22Scalar.getSkewX() * scaleY;
    SkScalar myy = fMatrix22Scalar.getScaleY() * scaleY;

    // fetch units/EM from "head" table if needed (ie for bitmap fonts)
    SkScalar upem = SkIntToScalar(face->units_per_EM);
    if (!upem) {
        TT_Header* ttHeader = (TT_Header*)FT_Get_Sfnt_Table(face, ft_sfnt_head);
        if (ttHeader) {
            upem = SkIntToScalar(ttHeader->Units_Per_EM);
        }
    }

    // use the os/2 table as a source of reasonable defaults.
    SkScalar x_height = 0.0f;
    SkScalar avgCharWidth = 0.0f;
    SkScalar cap_height = 0.0f;
    TT_OS2* os2 = (TT_OS2*) FT_Get_Sfnt_Table(face, ft_sfnt_os2);
    if (os2) {
        x_height = scaleX * SkIntToScalar(os2->sxHeight) / upem;
        avgCharWidth = SkIntToScalar(os2->xAvgCharWidth) / upem;
        if (os2->version != 0xFFFF && os2->version >= 2) {
            cap_height = scaleX * SkIntToScalar(os2->sCapHeight) / upem;
        }
    }

    // pull from format-specific metrics as needed
    SkScalar ascent, descent, leading, xmin, xmax, ymin, ymax;
    SkScalar underlineThickness, underlinePosition;
    if (face->face_flags & FT_FACE_FLAG_SCALABLE) { // scalable outline font
        ascent = -SkIntToScalar(face->ascender) / upem;
        descent = -SkIntToScalar(face->descender) / upem;
        leading = SkIntToScalar(face->height + (face->descender - face->ascender)) / upem;
        xmin = SkIntToScalar(face->bbox.xMin) / upem;
        xmax = SkIntToScalar(face->bbox.xMax) / upem;
        ymin = -SkIntToScalar(face->bbox.yMin) / upem;
        ymax = -SkIntToScalar(face->bbox.yMax) / upem;
        underlineThickness = SkIntToScalar(face->underline_thickness) / upem;
        underlinePosition = -SkIntToScalar(face->underline_position +
                                           face->underline_thickness / 2) / upem;

        metrics->fFlags |= SkPaint::FontMetrics::kUnderlineThinknessIsValid_Flag;
        metrics->fFlags |= SkPaint::FontMetrics::kUnderlinePositionIsValid_Flag;

        // we may be able to synthesize x_height and cap_height from outline
        if (!x_height) {
            FT_BBox bbox;
            if (getCBoxForLetter('x', &bbox)) {
                x_height = SkIntToScalar(bbox.yMax) / 64.0f;
            }
        }
        if (!cap_height) {
            FT_BBox bbox;
            if (getCBoxForLetter('H', &bbox)) {
                cap_height = SkIntToScalar(bbox.yMax) / 64.0f;
            }
        }
    } else if (fStrikeIndex != -1) { // bitmap strike metrics
        SkScalar xppem = SkIntToScalar(face->size->metrics.x_ppem);
        SkScalar yppem = SkIntToScalar(face->size->metrics.y_ppem);
        ascent = -SkIntToScalar(face->size->metrics.ascender) / (yppem * 64.0f);
        descent = -SkIntToScalar(face->size->metrics.descender) / (yppem * 64.0f);
        leading = (SkIntToScalar(face->size->metrics.height) / (yppem * 64.0f))
                + ascent - descent;
        xmin = 0.0f;
        xmax = SkIntToScalar(face->available_sizes[fStrikeIndex].width) / xppem;
        ymin = descent + leading;
        ymax = ascent - descent;
        underlineThickness = 0;
        underlinePosition = 0;

        metrics->fFlags &= ~SkPaint::FontMetrics::kUnderlineThinknessIsValid_Flag;
        metrics->fFlags &= ~SkPaint::FontMetrics::kUnderlinePositionIsValid_Flag;
    } else {
        goto ERROR;
    }

    // synthesize elements that were not provided by the os/2 table or format-specific metrics
    if (!x_height) {
        x_height = -ascent;
    }
    if (!avgCharWidth) {
        avgCharWidth = xmax - xmin;
    }
    if (!cap_height) {
      cap_height = -ascent;
    }

    // disallow negative linespacing
    if (leading < 0.0f) {
        leading = 0.0f;
    }

    SkScalar scale = myy;
    if (this->isVertical()) {
        scale = mxy;
    }
    metrics->fTop = ymax * scale;
    metrics->fAscent = ascent * scale;
    metrics->fDescent = descent * scale;
    metrics->fBottom = ymin * scale;
    metrics->fLeading = leading * scale;
    metrics->fAvgCharWidth = avgCharWidth * scale;
    metrics->fXMin = xmin;
    metrics->fXMax = xmax;
    metrics->fXHeight = x_height;
    metrics->fCapHeight = cap_height;
    metrics->fUnderlineThickness = underlineThickness * scale;
    metrics->fUnderlinePosition = underlinePosition * scale;
}

void SkScalerContext_FreeType::emboldenIfNeeded(FT_Face face, FT_GlyphSlot glyph)
{
    // check to see if the embolden bit is set
    if (0 == (fRec.fFlags & SkScalerContext::kEmbolden_Flag)) {
        return;
    }

#if defined(SK_BUILD_FOR_ANDROID_FRAMEWORK)
    // Android doesn't want to embolden a font that is already bold.
    if ((fFace->style_flags & FT_STYLE_FLAG_BOLD)) {
        return;
    }
#endif

    switch (glyph->format) {
        case FT_GLYPH_FORMAT_OUTLINE:
            FT_Pos strength;
            strength = FT_MulFix(face->units_per_EM, face->size->metrics.y_scale) / 24;
            FT_Outline_Embolden(&glyph->outline, strength);
            break;
        case FT_GLYPH_FORMAT_BITMAP:
            FT_GlyphSlot_Own_Bitmap(glyph);
            FT_Bitmap_Embolden(glyph->library, &glyph->bitmap, kBitmapEmboldenStrength, 0);
            break;
        default:
            SkDEBUGFAIL("unknown glyph format");
    }
}

///////////////////////////////////////////////////////////////////////////////

#include "SkUtils.h"

static SkUnichar next_utf8(const void** chars) {
    return SkUTF8_NextUnichar((const char**)chars);
}

static SkUnichar next_utf16(const void** chars) {
    return SkUTF16_NextUnichar((const uint16_t**)chars);
}

static SkUnichar next_utf32(const void** chars) {
    const SkUnichar** uniChars = (const SkUnichar**)chars;
    SkUnichar uni = **uniChars;
    *uniChars += 1;
    return uni;
}

typedef SkUnichar (*EncodingProc)(const void**);

static EncodingProc find_encoding_proc(SkTypeface::Encoding enc) {
    static const EncodingProc gProcs[] = {
        next_utf8, next_utf16, next_utf32
    };
    SkASSERT((size_t)enc < SK_ARRAY_COUNT(gProcs));
    return gProcs[enc];
}

int SkTypeface_FreeType::onCharsToGlyphs(const void* chars, Encoding encoding,
                                      uint16_t glyphs[], int glyphCount) const {
    AutoFTAccess fta(this);
    FT_Face face = fta.face();
    if (!face) {
        if (glyphs) {
            sk_bzero(glyphs, glyphCount * sizeof(glyphs[0]));
        }
        return 0;
    }

    EncodingProc next_uni_proc = find_encoding_proc(encoding);

    if (NULL == glyphs) {
        for (int i = 0; i < glyphCount; ++i) {
            if (0 == FT_Get_Char_Index(face, next_uni_proc(&chars))) {
                return i;
            }
        }
        return glyphCount;
    } else {
        int first = glyphCount;
        for (int i = 0; i < glyphCount; ++i) {
            unsigned id = FT_Get_Char_Index(face, next_uni_proc(&chars));
            glyphs[i] = SkToU16(id);
            if (0 == id && i < first) {
                first = i;
            }
        }
        return first;
    }
}

int SkTypeface_FreeType::onCountGlyphs() const {
    // we cache this value, using -1 as a sentinel for "not computed"
    if (fGlyphCount < 0) {
        AutoFTAccess fta(this);
        FT_Face face = fta.face();
        // if the face failed, we still assign a non-negative value
        fGlyphCount = face ? face->num_glyphs : 0;
    }
    return fGlyphCount;
}

SkTypeface::LocalizedStrings* SkTypeface_FreeType::onCreateFamilyNameIterator() const {
    SkTypeface::LocalizedStrings* nameIter =
        SkOTUtils::LocalizedStrings_NameTable::CreateForFamilyNames(*this);
    if (NULL == nameIter) {
        SkString familyName;
        this->getFamilyName(&familyName);
        SkString language("und"); //undetermined
        nameIter = new SkOTUtils::LocalizedStrings_SingleName(familyName, language);
    }
    return nameIter;
}

int SkTypeface_FreeType::onGetTableTags(SkFontTableTag tags[]) const {
    AutoFTAccess fta(this);
    FT_Face face = fta.face();

    FT_ULong tableCount = 0;
    FT_Error error;

    // When 'tag' is NULL, returns number of tables in 'length'.
    error = FT_Sfnt_Table_Info(face, 0, NULL, &tableCount);
    if (error) {
        return 0;
    }

    if (tags) {
        for (FT_ULong tableIndex = 0; tableIndex < tableCount; ++tableIndex) {
            FT_ULong tableTag;
            FT_ULong tablelength;
            error = FT_Sfnt_Table_Info(face, tableIndex, &tableTag, &tablelength);
            if (error) {
                return 0;
            }
            tags[tableIndex] = static_cast<SkFontTableTag>(tableTag);
        }
    }
    return tableCount;
}

size_t SkTypeface_FreeType::onGetTableData(SkFontTableTag tag, size_t offset,
                                           size_t length, void* data) const
{
    AutoFTAccess fta(this);
    FT_Face face = fta.face();

    FT_ULong tableLength = 0;
    FT_Error error;

    // When 'length' is 0 it is overwritten with the full table length; 'offset' is ignored.
    error = FT_Load_Sfnt_Table(face, tag, 0, NULL, &tableLength);
    if (error) {
        return 0;
    }

    if (offset > tableLength) {
        return 0;
    }
    FT_ULong size = SkTMin((FT_ULong)length, tableLength - (FT_ULong)offset);
    if (NULL != data) {
        error = FT_Load_Sfnt_Table(face, tag, offset, reinterpret_cast<FT_Byte*>(data), &size);
        if (error) {
            return 0;
        }
    }

    return size;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

/*static*/ bool SkTypeface_FreeType::ScanFont(
    SkStream* stream, int ttcIndex, SkString* name, SkTypeface::Style* style, bool* isFixedPitch)
{
    FT_Library  library;
    if (FT_Init_FreeType(&library)) {
        return false;
    }

    FT_Open_Args    args;
    memset(&args, 0, sizeof(args));

    const void* memoryBase = stream->getMemoryBase();
    FT_StreamRec    streamRec;

    if (NULL != memoryBase) {
        args.flags = FT_OPEN_MEMORY;
        args.memory_base = (const FT_Byte*)memoryBase;
        args.memory_size = stream->getLength();
    } else {
        memset(&streamRec, 0, sizeof(streamRec));
        streamRec.size = stream->getLength();
        streamRec.descriptor.pointer = stream;
        streamRec.read  = sk_stream_read;
        streamRec.close = sk_stream_close;

        args.flags = FT_OPEN_STREAM;
        args.stream = &streamRec;
    }

    FT_Face face;
    if (FT_Open_Face(library, &args, ttcIndex, &face)) {
        FT_Done_FreeType(library);
        return false;
    }

    int tempStyle = SkTypeface::kNormal;
    if (face->style_flags & FT_STYLE_FLAG_BOLD) {
        tempStyle |= SkTypeface::kBold;
    }
    if (face->style_flags & FT_STYLE_FLAG_ITALIC) {
        tempStyle |= SkTypeface::kItalic;
    }

    if (name) {
        name->set(face->family_name);
    }
    if (style) {
        *style = (SkTypeface::Style) tempStyle;
    }
    if (isFixedPitch) {
        *isFixedPitch = FT_IS_FIXED_WIDTH(face);
    }

    FT_Done_Face(face);
    FT_Done_FreeType(library);
    return true;
}
