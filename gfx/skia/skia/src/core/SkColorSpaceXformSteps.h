/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkColorSpaceXformSteps_DEFINED
#define SkColorSpaceXformSteps_DEFINED

#include "SkColorSpace.h"
#include "SkImageInfo.h"

class SkRasterPipeline;

struct SkColorSpaceXformSteps {
    struct Flags {
        bool unpremul         = false;
        bool linearize        = false;
        bool gamut_transform  = false;
        bool encode           = false;
        bool premul           = false;

        uint32_t mask() const {
            return (unpremul        ?  1 : 0)
                 | (linearize       ?  2 : 0)
                 | (gamut_transform ?  4 : 0)
                 | (encode          ?  8 : 0)
                 | (premul          ? 16 : 0);
        }
    };

    SkColorSpaceXformSteps(SkColorSpace* src, SkAlphaType srcAT,
                           SkColorSpace* dst, SkAlphaType dstAT);

    void apply(float rgba[4]) const;
    void apply(SkRasterPipeline*) const;

    Flags flags;

    bool srcTF_is_sRGB,
         dstTF_is_sRGB;
    SkColorSpaceTransferFn srcTF,     // Apply for linearize.
                           dstTFInv;  // Apply for encode.
    float src_to_dst_matrix[9];       // Apply this 3x3 column-major matrix for gamut_transform.
};

#endif//SkColorSpaceXformSteps_DEFINED
