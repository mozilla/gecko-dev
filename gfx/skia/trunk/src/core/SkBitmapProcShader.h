
/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#ifndef SkBitmapProcShader_DEFINED
#define SkBitmapProcShader_DEFINED

#include "SkShader.h"
#include "SkBitmapProcState.h"
#include "SkSmallAllocator.h"

class SkBitmapProcShader : public SkShader {
public:
    SkBitmapProcShader(const SkBitmap& src, TileMode tx, TileMode ty,
                       const SkMatrix* localMatrix = NULL);

    // overrides from SkShader
    virtual bool isOpaque() const SK_OVERRIDE;
    virtual BitmapType asABitmap(SkBitmap*, SkMatrix*, TileMode*) const SK_OVERRIDE;

    virtual size_t contextSize() const SK_OVERRIDE;

    static bool CanDo(const SkBitmap&, TileMode tx, TileMode ty);

    SK_TO_STRING_OVERRIDE()
    SK_DECLARE_PUBLIC_FLATTENABLE_DESERIALIZATION_PROCS(SkBitmapProcShader)


    bool asNewEffect(GrContext*, const SkPaint&, const SkMatrix*, GrColor*, GrEffect**)
            const SK_OVERRIDE;

    class BitmapProcShaderContext : public SkShader::Context {
    public:
        // The context takes ownership of the state. It will call its destructor
        // but will NOT free the memory.
        BitmapProcShaderContext(const SkBitmapProcShader&, const ContextRec&, SkBitmapProcState*);
        virtual ~BitmapProcShaderContext();

        virtual void shadeSpan(int x, int y, SkPMColor dstC[], int count) SK_OVERRIDE;
        virtual ShadeProc asAShadeProc(void** ctx) SK_OVERRIDE;
        virtual void shadeSpan16(int x, int y, uint16_t dstC[], int count) SK_OVERRIDE;

        virtual uint32_t getFlags() const SK_OVERRIDE { return fFlags; }

    private:
        SkBitmapProcState*  fState;
        uint32_t            fFlags;

        typedef SkShader::Context INHERITED;
    };

protected:
    SkBitmapProcShader(SkReadBuffer& );
    virtual void flatten(SkWriteBuffer&) const SK_OVERRIDE;
    virtual Context* onCreateContext(const ContextRec&, void* storage) const SK_OVERRIDE;

    SkBitmap    fRawBitmap;   // experimental for RLE encoding
    uint8_t     fTileModeX, fTileModeY;

private:
    typedef SkShader INHERITED;
};

// Commonly used allocator. It currently is only used to allocate up to 3 objects. The total
// bytes requested is calculated using one of our large shaders, its context size plus the size of
// an Sk3DBlitter in SkDraw.cpp
// Note that some contexts may contain other contexts (e.g. for compose shaders), but we've not
// yet found a situation where the size below isn't big enough.
typedef SkSmallAllocator<3, 768> SkTBlitterAllocator;

// If alloc is non-NULL, it will be used to allocate the returned SkShader, and MUST outlive
// the SkShader.
SkShader* CreateBitmapShader(const SkBitmap& src, SkShader::TileMode, SkShader::TileMode,
                             const SkMatrix* localMatrix, SkTBlitterAllocator* alloc);

#endif
