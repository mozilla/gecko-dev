/*
 *  Copyright 2013 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Get PSNR for video sequence. Assuming RAW 4:2:0 Y:Cb:Cr format

#ifndef UTIL_PSNR_H_  // NOLINT
#define UTIL_PSNR_H_

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(INT_TYPES_DEFINED) && !defined(UINT8_TYPE_DEFINED)
typedef unsigned char uint8;
#define UINT8_TYPE_DEFINED
#endif

static const double kMaxPSNR = 128.0;

// PSNR formula: psnr = 10 * log10 (Peak Signal^2 * size / sse).
// Returns 128.0 (kMaxPSNR) if sse is 0 (perfect match).
double ComputePSNR(double sse, double size);

// Computer Sum of Squared Error (SSE).
// Pass this to ComputePSNR for final result.
double ComputeSumSquareError(const uint8* org, const uint8* rec, int size);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // UTIL_PSNR_H_  // NOLINT
