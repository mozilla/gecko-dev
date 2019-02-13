/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkImageDecoder_DEFINED
#define SkImageDecoder_DEFINED

#include "SkBitmap.h"
#include "SkImage.h"
#include "SkRect.h"
#include "SkRefCnt.h"
#include "SkTRegistry.h"
#include "SkTypes.h"

class SkStream;
class SkStreamRewindable;

/** \class SkImageDecoder

    Base class for decoding compressed images into a SkBitmap
*/
class SkImageDecoder : SkNoncopyable {
public:
    virtual ~SkImageDecoder();

    enum Format {
        kUnknown_Format,
        kBMP_Format,
        kGIF_Format,
        kICO_Format,
        kJPEG_Format,
        kPNG_Format,
        kWBMP_Format,
        kWEBP_Format,
        kPKM_Format,
        kKTX_Format,

        kLastKnownFormat = kKTX_Format,
    };

    /** Return the format of image this decoder can decode. If this decoder can decode multiple
        formats, kUnknown_Format will be returned.
    */
    virtual Format getFormat() const;

    /** Return the format of the SkStreamRewindable or kUnknown_Format if it cannot be determined.
        Rewinds the stream before returning.
    */
    static Format GetStreamFormat(SkStreamRewindable*);

    /** Return a readable string of the Format provided.
    */
    static const char* GetFormatName(Format);

    /** Return a readable string of the value returned by getFormat().
    */
    const char* getFormatName() const;

    /** Whether the decoder should skip writing zeroes to output if possible.
    */
    bool getSkipWritingZeroes() const { return fSkipWritingZeroes; }

    /** Set to true if the decoder should skip writing any zeroes when
        creating the output image.
        This is a hint that may not be respected by the decoder.
        It should only be used if it is known that the memory to write
        to has already been set to 0; otherwise the resulting image will
        have garbage.
        This is ideal for images that contain a lot of completely transparent
        pixels, but may be a performance hit for an image that has only a
        few transparent pixels.
        The default is false.
    */
    void setSkipWritingZeroes(bool skip) { fSkipWritingZeroes = skip; }

    /** Returns true if the decoder should try to dither the resulting image.
        The default setting is true.
    */
    bool getDitherImage() const { return fDitherImage; }

    /** Set to true if the the decoder should try to dither the resulting image.
        The default setting is true.
    */
    void setDitherImage(bool dither) { fDitherImage = dither; }

    /** Returns true if the decoder should try to decode the
        resulting image to a higher quality even at the expense of
        the decoding speed.
    */
    bool getPreferQualityOverSpeed() const { return fPreferQualityOverSpeed; }

    /** Set to true if the the decoder should try to decode the
        resulting image to a higher quality even at the expense of
        the decoding speed.
    */
    void setPreferQualityOverSpeed(bool qualityOverSpeed) {
        fPreferQualityOverSpeed = qualityOverSpeed;
    }

    /** Set to true to require the decoder to return a bitmap with unpremultiplied
        colors. The default is false, meaning the resulting bitmap will have its
        colors premultiplied.
        NOTE: Passing true to this function may result in a bitmap which cannot
        be properly used by Skia.
    */
    void setRequireUnpremultipliedColors(bool request) {
        fRequireUnpremultipliedColors = request;
    }

    /** Returns true if the decoder will only return bitmaps with unpremultiplied
        colors.
    */
    bool getRequireUnpremultipliedColors() const { return fRequireUnpremultipliedColors; }

    /** \class Peeker

        Base class for optional callbacks to retrieve meta/chunk data out of
        an image as it is being decoded.
    */
    class Peeker : public SkRefCnt {
    public:
        SK_DECLARE_INST_COUNT(Peeker)

        /** Return true to continue decoding, or false to indicate an error, which
            will cause the decoder to not return the image.
        */
        virtual bool peek(const char tag[], const void* data, size_t length) = 0;
    private:
        typedef SkRefCnt INHERITED;
    };

    Peeker* getPeeker() const { return fPeeker; }
    Peeker* setPeeker(Peeker*);

#ifdef SK_SUPPORT_LEGACY_IMAGEDECODER_CHOOSER
    /** \class Chooser

        Base class for optional callbacks to choose an image from a format that
        contains multiple images.
    */
    class Chooser : public SkRefCnt {
    public:
        SK_DECLARE_INST_COUNT(Chooser)

        virtual void begin(int count) {}
        virtual void inspect(int index, SkBitmap::Config config, int width, int height) {}
        /** Return the index of the subimage you want, or -1 to choose none of them.
        */
        virtual int choose() = 0;

    private:
        typedef SkRefCnt INHERITED;
    };

    Chooser* getChooser() const { return fChooser; }
    Chooser* setChooser(Chooser*);
#endif

#ifdef SK_SUPPORT_LEGACY_BITMAP_CONFIG
    /**
     *  Optional table describing the caller's preferred config based on
     *  information about the src data. Each field should be set to the
     *  preferred config for a src described in the name of the field. The
     *  src attributes are described in terms of depth (8-index,
     *  8bit-grayscale, or 8-bits/component) and whether there is per-pixel
     *  alpha (does not apply to grayscale). If the caller has no preference
     *  for a particular src type, its slot should be set to kNo_Config.
     *
     *  NOTE ABOUT PREFERRED CONFIGS:
     *  If a config is preferred, either using a pref table or as a parameter
     *  to some flavor of decode, it is still at the discretion of the codec
     *  as to what output config is actually returned, as it may not be able
     *  to support the caller's preference.
     *
     *  If a bitmap is decoded into SkBitmap::A8_Config, the resulting bitmap
     *  will either be a conversion of the grayscale in the case of a
     *  grayscale source or the alpha channel in the case of a source with
     *  an alpha channel.
     */
    struct PrefConfigTable {
        SkBitmap::Config fPrefFor_8Index_NoAlpha_src;
        SkBitmap::Config fPrefFor_8Index_YesAlpha_src;
        SkBitmap::Config fPrefFor_8Gray_src;
        SkBitmap::Config fPrefFor_8bpc_NoAlpha_src;
        SkBitmap::Config fPrefFor_8bpc_YesAlpha_src;
    };

    /**
     *  Set an optional table for specifying the caller's preferred config
     *  based on information about the src data.
     *
     *  The default is no preference, which will assume the config set by
     *  decode is preferred.
     */
    void setPrefConfigTable(const PrefConfigTable&);

    /**
     *  Do not use a PrefConfigTable to determine the output config. This
     *  is the default, so there is no need to call unless a PrefConfigTable
     *  was previously set.
     */
    void resetPrefConfigTable() { fUsePrefTable = false; }
#endif

    /**
     *  By default, the codec will try to comply with the "pref" colortype
     *  that is passed to decode() or decodeSubset(). However, this can be called
     *  to override that, causing the codec to try to match the src depth instead
     *  (as shown below).
     *
     *      src_8Index  -> kIndex_8_SkColorType
     *      src_8Gray   -> kN32_SkColorType
     *      src_8bpc    -> kN32_SkColorType
     */
    void setPreserveSrcDepth(bool preserve) {
        fPreserveSrcDepth = preserve;
    }

    SkBitmap::Allocator* getAllocator() const { return fAllocator; }
    SkBitmap::Allocator* setAllocator(SkBitmap::Allocator*);

    // sample-size, if set to > 1, tells the decoder to return a smaller than
    // original bitmap, sampling 1 pixel for every size pixels. e.g. if sample
    // size is set to 3, then the returned bitmap will be 1/3 as wide and high,
    // and will contain 1/9 as many pixels as the original.
    // Note: this is a hint, and the codec may choose to ignore this, or only
    // approximate the sample size.
    int getSampleSize() const { return fSampleSize; }
    void setSampleSize(int size);

    /** Reset the sampleSize to its default of 1
     */
    void resetSampleSize() { this->setSampleSize(1); }

    /** Decoding is synchronous, but for long decodes, a different thread can
        call this method safely. This sets a state that the decoders will
        periodically check, and if they see it changed to cancel, they will
        cancel. This will result in decode() returning false. However, there is
        no guarantee that the decoder will see the state change in time, so
        it is possible that cancelDecode() will be called, but will be ignored
        and decode() will return true (assuming no other problems were
        encountered).

        This state is automatically reset at the beginning of decode().
     */
    void cancelDecode() {
        // now the subclass must query shouldCancelDecode() to be informed
        // of the request
        fShouldCancelDecode = true;
    }

    /** Passed to the decode method. If kDecodeBounds_Mode is passed, then
        only the bitmap's info need be set. If kDecodePixels_Mode
        is passed, then the bitmap must have pixels or a pixelRef.
    */
    enum Mode {
        kDecodeBounds_Mode, //!< only return info in bitmap
        kDecodePixels_Mode  //!< return entire bitmap (including pixels)
    };

    /** Given a stream, decode it into the specified bitmap.
        If the decoder can decompress the image, it calls bitmap.setInfo(),
        and then if the Mode is kDecodePixels_Mode, call allocPixelRef(),
        which will allocated a pixelRef. To access the pixel memory, the codec
        needs to call lockPixels/unlockPixels on the
        bitmap. It can then set the pixels with the decompressed image.
    *   If the image cannot be decompressed, return false. After the
    *   decoding, the function converts the decoded colortype in bitmap
    *   to pref if possible. Whether a conversion is feasible is
    *   tested by Bitmap::canCopyTo(pref).

        If an SkBitmap::Allocator is installed via setAllocator, it will be
        used to allocate the pixel memory. A clever allocator can be used
        to allocate the memory from a cache, volatile memory, or even from
        an existing bitmap's memory.

        If a Peeker is installed via setPeeker, it may be used to peek into
        meta data during the decode.
    */
    bool decode(SkStream*, SkBitmap* bitmap, SkColorType pref, Mode);
    bool decode(SkStream* stream, SkBitmap* bitmap, Mode mode) {
        return this->decode(stream, bitmap, kUnknown_SkColorType, mode);
    }

    /**
     * Given a stream, build an index for doing tile-based decode.
     * The built index will be saved in the decoder, and the image size will
     * be returned in width and height.
     *
     * Return true for success or false on failure.
     */
    bool buildTileIndex(SkStreamRewindable*, int *width, int *height);

    /**
     * Decode a rectangle subset in the image.
     * The method can only be called after buildTileIndex().
     *
     * Return true for success.
     * Return false if the index is never built or failing in decoding.
     */
    bool decodeSubset(SkBitmap* bm, const SkIRect& subset, SkColorType pref);

    /** Given a stream, this will try to find an appropriate decoder object.
        If none is found, the method returns NULL.
    */
    static SkImageDecoder* Factory(SkStreamRewindable*);

    /** Decode the image stored in the specified file, and store the result
        in bitmap. Return true for success or false on failure.

        @param pref If the PrefConfigTable is not set, prefer this colortype.
                          See NOTE ABOUT PREFERRED CONFIGS.

        @param format On success, if format is non-null, it is set to the format
                      of the decoded file. On failure it is ignored.
    */
    static bool DecodeFile(const char file[], SkBitmap* bitmap, SkColorType pref, Mode,
                           Format* format = NULL);
    static bool DecodeFile(const char file[], SkBitmap* bitmap) {
        return DecodeFile(file, bitmap, kUnknown_SkColorType, kDecodePixels_Mode, NULL);
    }

    /** Decode the image stored in the specified memory buffer, and store the
        result in bitmap. Return true for success or false on failure.

        @param pref If the PrefConfigTable is not set, prefer this colortype.
                          See NOTE ABOUT PREFERRED CONFIGS.

        @param format On success, if format is non-null, it is set to the format
                       of the decoded buffer. On failure it is ignored.
     */
    static bool DecodeMemory(const void* buffer, size_t size, SkBitmap* bitmap, SkColorType pref,
                             Mode, Format* format = NULL);
    static bool DecodeMemory(const void* buffer, size_t size, SkBitmap* bitmap){
        return DecodeMemory(buffer, size, bitmap, kUnknown_SkColorType, kDecodePixels_Mode, NULL);
    }

    /**
     *  Struct containing information about a pixel destination.
     */
    struct Target {
        /**
         *  Pre-allocated memory.
         */
        void*  fAddr;

        /**
         *  Rowbytes of the allocated memory.
         */
        size_t fRowBytes;
    };

    /** Decode the image stored in the specified SkStreamRewindable, and store the result
        in bitmap. Return true for success or false on failure.

        @param pref If the PrefConfigTable is not set, prefer this colortype.
                          See NOTE ABOUT PREFERRED CONFIGS.

        @param format On success, if format is non-null, it is set to the format
                      of the decoded stream. On failure it is ignored.
     */
    static bool DecodeStream(SkStreamRewindable* stream, SkBitmap* bitmap, SkColorType pref, Mode,
                             Format* format = NULL);
    static bool DecodeStream(SkStreamRewindable* stream, SkBitmap* bitmap) {
        return DecodeStream(stream, bitmap, kUnknown_SkColorType, kDecodePixels_Mode, NULL);
    }

protected:
    // must be overridden in subclasses. This guy is called by decode(...)
    virtual bool onDecode(SkStream*, SkBitmap* bitmap, Mode) = 0;

    // If the decoder wants to support tiled based decoding,
    // this method must be overridden. This guy is called by buildTileIndex(...)
    virtual bool onBuildTileIndex(SkStreamRewindable*, int *width, int *height) {
        return false;
    }

    // If the decoder wants to support tiled based decoding,
    // this method must be overridden. This guy is called by decodeRegion(...)
    virtual bool onDecodeSubset(SkBitmap* bitmap, const SkIRect& rect) {
        return false;
    }

    /*
     * Crop a rectangle from the src Bitmap to the dest Bitmap. src and dst are
     * both sampled by sampleSize from an original Bitmap.
     *
     * @param dst the destination bitmap.
     * @param src the source bitmap that is sampled by sampleSize from the
     *            original bitmap.
     * @param sampleSize the sample size that src is sampled from the original bitmap.
     * @param (dstX, dstY) the upper-left point of the dest bitmap in terms of
     *                     the coordinate in the original bitmap.
     * @param (width, height) the width and height of the unsampled dst.
     * @param (srcX, srcY) the upper-left point of the src bitmap in terms of
     *                     the coordinate in the original bitmap.
     * @return bool Whether or not it succeeded.
     */
    bool cropBitmap(SkBitmap *dst, SkBitmap *src, int sampleSize,
                    int dstX, int dstY, int width, int height,
                    int srcX, int srcY);

    /**
     *  Copy all fields on this decoder to the other decoder. Used by subclasses
     *  to decode a subimage using a different decoder, but with the same settings.
     */
    void copyFieldsToOther(SkImageDecoder* other);

    /** Can be queried from within onDecode, to see if the user (possibly in
        a different thread) has requested the decode to cancel. If this returns
        true, your onDecode() should stop and return false.
        Each subclass needs to decide how often it can query this, to balance
        responsiveness with performance.

        Calling this outside of onDecode() may return undefined values.
     */

public:
    bool shouldCancelDecode() const { return fShouldCancelDecode; }

protected:
    SkImageDecoder();

    /**
     *  Return the default preference being used by the current or latest call to decode.
     */
    SkColorType getDefaultPref() { return fDefaultPref; }
    
#ifdef SK_SUPPORT_LEGACY_IMAGEDECODER_CHOOSER
    // helper function for decoders to handle the (common) case where there is only
    // once choice available in the image file.
    bool chooseFromOneChoice(SkColorType, int width, int height) const;
#endif

    /*  Helper for subclasses. Call this to allocate the pixel memory given the bitmap's info.
        Returns true on success. This method handles checking for an optional Allocator.
    */
    bool allocPixelRef(SkBitmap*, SkColorTable*) const;

    /**
     *  The raw data of the src image.
     */
    enum SrcDepth {
        // Color-indexed.
        kIndex_SrcDepth,
        // Grayscale in 8 bits.
        k8BitGray_SrcDepth,
        // 8 bits per component. Used for 24 bit if there is no alpha.
        k32Bit_SrcDepth,
    };
    /** The subclass, inside onDecode(), calls this to determine the colorType of
        the returned bitmap. SrcDepth and hasAlpha reflect the raw data of the
        src image. This routine returns the caller's preference given
        srcDepth and hasAlpha, or kUnknown_SkColorType if there is no preference.
     */
    SkColorType getPrefColorType(SrcDepth, bool hasAlpha) const;

private:
    Peeker*                 fPeeker;
#ifdef SK_SUPPORT_LEGACY_IMAGEDECODER_CHOOSER
    Chooser*                fChooser;
#endif
    SkBitmap::Allocator*    fAllocator;
    int                     fSampleSize;
    SkColorType             fDefaultPref;   // use if fUsePrefTable is false
#ifdef SK_SUPPORT_LEGACY_BITMAP_CONFIG
    PrefConfigTable         fPrefTable;     // use if fUsePrefTable is true
    bool                    fUsePrefTable;
#endif
    bool                    fPreserveSrcDepth;
    bool                    fDitherImage;
    bool                    fSkipWritingZeroes;
    mutable bool            fShouldCancelDecode;
    bool                    fPreferQualityOverSpeed;
    bool                    fRequireUnpremultipliedColors;
};

/** Calling newDecoder with a stream returns a new matching imagedecoder
    instance, or NULL if none can be found. The caller must manage its ownership
    of the stream as usual, calling unref() when it is done, as the returned
    decoder may have called ref() (and if so, the decoder is responsible for
    balancing its ownership when it is destroyed).
 */
class SkImageDecoderFactory : public SkRefCnt {
public:
    SK_DECLARE_INST_COUNT(SkImageDecoderFactory)

    virtual SkImageDecoder* newDecoder(SkStreamRewindable*) = 0;

private:
    typedef SkRefCnt INHERITED;
};

class SkDefaultImageDecoderFactory : SkImageDecoderFactory {
public:
    // calls SkImageDecoder::Factory(stream)
    virtual SkImageDecoder* newDecoder(SkStreamRewindable* stream) {
        return SkImageDecoder::Factory(stream);
    }
};

// This macro declares a global (i.e., non-class owned) creation entry point
// for each decoder (e.g., CreateJPEGImageDecoder)
#define DECLARE_DECODER_CREATOR(codec)          \
    SkImageDecoder *Create ## codec ();

// This macro defines the global creation entry point for each decoder. Each
// decoder implementation that registers with the decoder factory must call it.
#define DEFINE_DECODER_CREATOR(codec)           \
    SkImageDecoder *Create ## codec () {        \
        return SkNEW( Sk ## codec );            \
    }

// All the decoders known by Skia. Note that, depending on the compiler settings,
// not all of these will be available
DECLARE_DECODER_CREATOR(BMPImageDecoder);
DECLARE_DECODER_CREATOR(GIFImageDecoder);
DECLARE_DECODER_CREATOR(ICOImageDecoder);
DECLARE_DECODER_CREATOR(JPEGImageDecoder);
DECLARE_DECODER_CREATOR(PNGImageDecoder);
DECLARE_DECODER_CREATOR(WBMPImageDecoder);
DECLARE_DECODER_CREATOR(WEBPImageDecoder);
DECLARE_DECODER_CREATOR(PKMImageDecoder);
DECLARE_DECODER_CREATOR(KTXImageDecoder);

// Typedefs to make registering decoder and formatter callbacks easier.
// These have to be defined outside SkImageDecoder. :(
typedef SkTRegistry<SkImageDecoder*(*)(SkStreamRewindable*)>        SkImageDecoder_DecodeReg;
typedef SkTRegistry<SkImageDecoder::Format(*)(SkStreamRewindable*)> SkImageDecoder_FormatReg;

#endif
