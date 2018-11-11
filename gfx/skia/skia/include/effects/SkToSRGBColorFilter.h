/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkToSRGBColorFilter_DEFINED
#define SkToSRGBColorFilter_DEFINED

#include "SkFlattenable.h"
#include "SkColorFilter.h"
#include "SkRefCnt.h"

class SkColorSpace;
class SkRasterPipeline;

/**
 *  Color filter that converts from supplied color space to sRGB (both gamut and transfer function).
 */
class SK_API SkToSRGBColorFilter : public SkColorFilter {
public:
    static sk_sp<SkColorFilter> Make(sk_sp<SkColorSpace> srcColorSpace);

#if SK_SUPPORT_GPU
    std::unique_ptr<GrFragmentProcessor> asFragmentProcessor(
            GrContext*, const GrColorSpaceInfo&) const override;
#endif

    Factory getFactory() const override { return CreateProc; }

private:
    void flatten(SkWriteBuffer&) const override;
    SkToSRGBColorFilter(sk_sp<SkColorSpace>);
    void onAppendStages(SkRasterPipeline*, SkColorSpace*, SkArenaAlloc*,
                        bool shaderIsOpaque) const override;
    static sk_sp<SkFlattenable> CreateProc(SkReadBuffer&);
    friend class SkFlattenable::PrivateInitializer;

    sk_sp<SkColorSpace> fSrcColorSpace;

    typedef SkColorFilter INHERITED;
};

#endif
