/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef SkColorSpacePriv_DEFINED
#define SkColorSpacePriv_DEFINED

#include <math.h>

#include "SkColorSpace.h"
#include "SkFixed.h"

#define SkColorSpacePrintf(...)

static constexpr float gSRGB_toXYZD50[] {
#ifdef SK_LEGACY_SRGB_GAMUT
    0.4360747f, 0.3850649f, 0.1430804f, // Rx, Gx, Bx
    0.2225045f, 0.7168786f, 0.0606169f, // Ry, Gy, By
    0.0139322f, 0.0971045f, 0.7141733f, // Rz, Gz, Bz
#else
    // These are taken from skcms, and there originally from 16-bit fixed point.
    // For best results, please keep them exactly in sync with skcms.
    0.436065674f, 0.385147095f, 0.143066406f,
    0.222488403f, 0.716873169f, 0.060607910f,
    0.013916016f, 0.097076416f, 0.714096069f,
#endif
};

static constexpr float gAdobeRGB_toXYZD50[] {
    // ICC fixed-point (16.16) repesentation of:
    // 0.60974, 0.20528, 0.14919,
    // 0.31111, 0.62567, 0.06322,
    // 0.01947, 0.06087, 0.74457,
    SkFixedToFloat(0x9c18), SkFixedToFloat(0x348d), SkFixedToFloat(0x2631), // Rx, Gx, Bx
    SkFixedToFloat(0x4fa5), SkFixedToFloat(0xa02c), SkFixedToFloat(0x102f), // Ry, Gy, By
    SkFixedToFloat(0x04fc), SkFixedToFloat(0x0f95), SkFixedToFloat(0xbe9c), // Rz, Gz, Bz
};

static constexpr float gDCIP3_toXYZD50[] {
    0.515102f,   0.291965f,  0.157153f,  // Rx, Gx, Bx
    0.241182f,   0.692236f,  0.0665819f, // Ry, Gy, By
   -0.00104941f, 0.0418818f, 0.784378f,  // Rz, Gz, Bz
};

static constexpr float gRec2020_toXYZD50[] {
    0.673459f,   0.165661f,  0.125100f,  // Rx, Gx, Bx
    0.279033f,   0.675338f,  0.0456288f, // Ry, Gy, By
   -0.00193139f, 0.0299794f, 0.797162f,  // Rz, Gz, Bz
};

// A gamut narrower than sRGB, useful for testing.
static constexpr float gNarrow_toXYZD50[] {
    0.190974f,  0.404865f,  0.368380f,
    0.114746f,  0.582937f,  0.302318f,
    0.032925f,  0.153615f,  0.638669f,
};

// Like gSRGB_toXYZD50, keeping this bitwise exactly the same as skcms makes things fastest.
static constexpr SkColorSpaceTransferFn gSRGB_TransferFn =
#ifdef SK_LEGACY_SRGB_TRANSFER_FUNCTION
        { 2.4f, 1.0f / 1.055f, 0.055f / 1.055f, 1.0f / 12.92f, 0.04045f, 0.0f, 0.0f };
#else
        { 2.4f, (float)(1/1.055), (float)(0.055/1.055), (float)(1/12.92), 0.04045f, 0.0f, 0.0f };
#endif

static constexpr SkColorSpaceTransferFn g2Dot2_TransferFn =
        { 2.2f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

static constexpr SkColorSpaceTransferFn gLinear_TransferFn =
        { 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

static constexpr SkColorSpaceTransferFn gDCIP3_TransferFn =
    { 2.399994f, 0.947998047f, 0.0520019531f, 0.0769958496f, 0.0390014648f, 0.0f, 0.0f };

static inline void to_xyz_d50(SkMatrix44* toXYZD50, SkColorSpace::Gamut gamut) {
    switch (gamut) {
        case SkColorSpace::kSRGB_Gamut:
            toXYZD50->set3x3RowMajorf(gSRGB_toXYZD50);
            break;
        case SkColorSpace::kAdobeRGB_Gamut:
            toXYZD50->set3x3RowMajorf(gAdobeRGB_toXYZD50);
            break;
        case SkColorSpace::kDCIP3_D65_Gamut:
            toXYZD50->set3x3RowMajorf(gDCIP3_toXYZD50);
            break;
        case SkColorSpace::kRec2020_Gamut:
            toXYZD50->set3x3RowMajorf(gRec2020_toXYZD50);
            break;
    }
}

static inline bool color_space_almost_equal(float a, float b) {
    return SkTAbs(a - b) < 0.01f;
}

// Let's use a stricter version for transfer functions.  Worst case, these are encoded
// in ICC format, which offers 16-bits of fractional precision.
static inline bool transfer_fn_almost_equal(float a, float b) {
    return SkTAbs(a - b) < 0.001f;
}

static inline bool is_zero_to_one(float v) {
    // Because we allow a value just barely larger than 1, the client can use an
    // entirely linear transfer function.
    return (0.0f <= v) && (v <= nextafterf(1.0f, 2.0f));
}

static inline bool is_valid_transfer_fn(const SkColorSpaceTransferFn& coeffs) {
    if (SkScalarIsNaN(coeffs.fA) || SkScalarIsNaN(coeffs.fB) ||
        SkScalarIsNaN(coeffs.fC) || SkScalarIsNaN(coeffs.fD) ||
        SkScalarIsNaN(coeffs.fE) || SkScalarIsNaN(coeffs.fF) ||
        SkScalarIsNaN(coeffs.fG))
    {
        return false;
    }

    if (coeffs.fD < 0.0f) {
        return false;
    }

    if (coeffs.fD == 0.0f) {
        // Y = (aX + b)^g + e  for always
        if (0.0f == coeffs.fA || 0.0f == coeffs.fG) {
            SkColorSpacePrintf("A or G is zero, constant transfer function "
                               "is nonsense");
            return false;
        }
    }

    if (coeffs.fD >= 1.0f) {
        // Y = cX + f          for always
        if (0.0f == coeffs.fC) {
            SkColorSpacePrintf("C is zero, constant transfer function is "
                               "nonsense");
            return false;
        }
    }

    if ((0.0f == coeffs.fA || 0.0f == coeffs.fG) && 0.0f == coeffs.fC) {
        SkColorSpacePrintf("A or G, and C are zero, constant transfer function "
                           "is nonsense");
        return false;
    }

    if (coeffs.fC < 0.0f) {
        SkColorSpacePrintf("Transfer function must be increasing");
        return false;
    }

    if (coeffs.fA < 0.0f || coeffs.fG < 0.0f) {
        SkColorSpacePrintf("Transfer function must be positive or increasing");
        return false;
    }

    return true;
}

static inline bool is_almost_srgb(const SkColorSpaceTransferFn& coeffs) {
    return transfer_fn_almost_equal(gSRGB_TransferFn.fA, coeffs.fA) &&
           transfer_fn_almost_equal(gSRGB_TransferFn.fB, coeffs.fB) &&
           transfer_fn_almost_equal(gSRGB_TransferFn.fC, coeffs.fC) &&
           transfer_fn_almost_equal(gSRGB_TransferFn.fD, coeffs.fD) &&
           transfer_fn_almost_equal(gSRGB_TransferFn.fE, coeffs.fE) &&
           transfer_fn_almost_equal(gSRGB_TransferFn.fF, coeffs.fF) &&
           transfer_fn_almost_equal(gSRGB_TransferFn.fG, coeffs.fG);
}

static inline bool is_almost_2dot2(const SkColorSpaceTransferFn& coeffs) {
    return transfer_fn_almost_equal(1.0f, coeffs.fA) &&
           transfer_fn_almost_equal(0.0f, coeffs.fB) &&
           transfer_fn_almost_equal(0.0f, coeffs.fE) &&
           transfer_fn_almost_equal(2.2f, coeffs.fG) &&
           coeffs.fD <= 0.0f;
}

static inline bool is_almost_linear(const SkColorSpaceTransferFn& coeffs) {
    // OutputVal = InputVal ^ 1.0f
    const bool linearExp =
            transfer_fn_almost_equal(1.0f, coeffs.fA) &&
            transfer_fn_almost_equal(0.0f, coeffs.fB) &&
            transfer_fn_almost_equal(0.0f, coeffs.fE) &&
            transfer_fn_almost_equal(1.0f, coeffs.fG) &&
            coeffs.fD <= 0.0f;

    // OutputVal = 1.0f * InputVal
    const bool linearFn =
            transfer_fn_almost_equal(1.0f, coeffs.fC) &&
            transfer_fn_almost_equal(0.0f, coeffs.fF) &&
            coeffs.fD >= 1.0f;

    return linearExp || linearFn;
}

// Return raw pointers to commonly used SkColorSpaces.
// No need to ref/unref these, but if you do, do it in pairs.
SkColorSpace* sk_srgb_singleton();
SkColorSpace* sk_srgb_linear_singleton();

#endif  // SkColorSpacePriv_DEFINED
