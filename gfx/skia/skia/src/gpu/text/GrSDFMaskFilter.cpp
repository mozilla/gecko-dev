/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrSDFMaskFilter.h"
#include "SkDistanceFieldGen.h"
#include "SkMaskFilterBase.h"
#include "SkReadBuffer.h"
#include "SkSafeMath.h"
#include "SkWriteBuffer.h"
#include "SkString.h"

class SK_API GrSDFMaskFilterImpl : public SkMaskFilterBase {
public:
    GrSDFMaskFilterImpl();

    // overrides from SkMaskFilterBase
    //  This method is not exported to java.
    SkMask::Format getFormat() const override;
    //  This method is not exported to java.
    bool filterMask(SkMask* dst, const SkMask& src, const SkMatrix&,
                    SkIPoint* margin) const override;

    void computeFastBounds(const SkRect&, SkRect*) const override;

    SK_DECLARE_PUBLIC_FLATTENABLE_DESERIALIZATION_PROCS(GrSDFMaskFilterImpl)

protected:

private:
    typedef SkMaskFilter INHERITED;
    friend void gr_register_sdf_maskfilter_createproc();
};

///////////////////////////////////////////////////////////////////////////////

GrSDFMaskFilterImpl::GrSDFMaskFilterImpl() {}

SkMask::Format GrSDFMaskFilterImpl::getFormat() const {
    return SkMask::kSDF_Format;
}

bool GrSDFMaskFilterImpl::filterMask(SkMask* dst, const SkMask& src,
                                     const SkMatrix& matrix, SkIPoint* margin) const {
    if (src.fFormat != SkMask::kA8_Format && src.fFormat != SkMask::kBW_Format) {
        return false;
    }

    *dst = SkMask::PrepareDestination(SK_DistanceFieldPad, SK_DistanceFieldPad, src);
    dst->fFormat = SkMask::kSDF_Format;

    if (margin) {
        margin->set(SK_DistanceFieldPad, SK_DistanceFieldPad);
    }

    if (src.fImage == nullptr) {
        return true;
    }
    if (dst->fImage == nullptr) {
        dst->fBounds.setEmpty();
        return false;
    }

    if (src.fFormat == SkMask::kA8_Format) {
        return SkGenerateDistanceFieldFromA8Image(dst->fImage, src.fImage,
                                                  src.fBounds.width(), src.fBounds.height(),
                                                  src.fRowBytes);

    } else {
        return SkGenerateDistanceFieldFromBWImage(dst->fImage, src.fImage,
                                                  src.fBounds.width(), src.fBounds.height(),
                                                  src.fRowBytes);
    }
}

void GrSDFMaskFilterImpl::computeFastBounds(const SkRect& src,
                                            SkRect* dst) const {
    dst->set(src.fLeft  - SK_DistanceFieldPad, src.fTop    - SK_DistanceFieldPad,
             src.fRight + SK_DistanceFieldPad, src.fBottom + SK_DistanceFieldPad);
}

sk_sp<SkFlattenable> GrSDFMaskFilterImpl::CreateProc(SkReadBuffer& buffer) {
    return GrSDFMaskFilter::Make();
}

void gr_register_sdf_maskfilter_createproc() {
    SK_DEFINE_FLATTENABLE_REGISTRAR_ENTRY(GrSDFMaskFilterImpl)
}

///////////////////////////////////////////////////////////////////////////////

sk_sp<SkMaskFilter> GrSDFMaskFilter::Make() {
    return sk_sp<SkMaskFilter>(new GrSDFMaskFilterImpl());
}
