/*
 *  Copyright 2024 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/row.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

#if !defined(LIBYUV_DISABLE_SVE) && defined(__aarch64__)

#define READYUV444_SVE                           \
  "ld1b       {z0.h}, p1/z, [%[src_y]]       \n" \
  "ld1b       {z1.h}, p1/z, [%[src_u]]       \n" \
  "ld1b       {z2.h}, p1/z, [%[src_v]]       \n" \
  "add        %[src_y], %[src_y], %[vl]      \n" \
  "add        %[src_u], %[src_u], %[vl]      \n" \
  "add        %[src_v], %[src_v], %[vl]      \n" \
  "prfm       pldl1keep, [%[src_y], 448]     \n" \
  "prfm       pldl1keep, [%[src_u], 448]     \n" \
  "trn1       z0.b, z0.b, z0.b               \n" \
  "prfm       pldl1keep, [%[src_v], 448]     \n"

#define READYUV422_SVE                           \
  "ld1b       {z0.h}, p1/z, [%[src_y]]       \n" \
  "ld1b       {z1.s}, p1/z, [%[src_u]]       \n" \
  "ld1b       {z2.s}, p1/z, [%[src_v]]       \n" \
  "inch       %[src_y]                       \n" \
  "incw       %[src_u]                       \n" \
  "incw       %[src_v]                       \n" \
  "prfm       pldl1keep, [%[src_y], 448]     \n" \
  "prfm       pldl1keep, [%[src_u], 128]     \n" \
  "prfm       pldl1keep, [%[src_v], 128]     \n" \
  "trn1       z0.b, z0.b, z0.b               \n" \
  "trn1       z1.h, z1.h, z1.h               \n" \
  "trn1       z2.h, z2.h, z2.h               \n"

#define READYUV400_SVE                           \
  "ld1b       {z0.h}, p1/z, [%[src_y]]       \n" \
  "inch       %[src_y]                       \n" \
  "prfm       pldl1keep, [%[src_y], 448]     \n" \
  "trn1       z0.b, z0.b, z0.b               \n"

// We need a different predicate for the UV component to handle the tail.
// If there is a single element remaining then we want to load one Y element
// but two UV elements.
#define READNV_SVE                                                  \
  "ld1b       {z0.h}, p1/z, [%[src_y]]       \n" /* Y0Y0 */         \
  "ld1b       {z1.h}, p2/z, [%[src_uv]]      \n" /* U0V0 or V0U0 */ \
  "inch       %[src_y]                       \n"                    \
  "inch       %[src_uv]                      \n"                    \
  "prfm       pldl1keep, [%[src_y], 448]     \n"                    \
  "prfm       pldl1keep, [%[src_uv], 448]    \n"                    \
  "trn1       z0.b, z0.b, z0.b               \n" /* YYYY */         \
  "tbl        z2.h, {z1.h}, z23.h            \n" /* V0V0 */         \
  "tbl        z1.h, {z1.h}, z22.h            \n" /* U0U0 */

#define READYUY2_SVE                                        \
  "ld1w       {z0.s}, p2/z, [%[src_yuy2]]    \n" /* YUYV */ \
  "incb       %[src_yuy2]                    \n"            \
  "prfm       pldl1keep, [%[src_yuy2], 448]  \n"            \
  "tbl        z2.b, {z0.b}, z23.b            \n" /* V0V0 */ \
  "tbl        z1.b, {z0.b}, z22.b            \n" /* U0U0 */ \
  "trn1       z0.b, z0.b, z0.b               \n" /* YYYY */

#define READUYVY_SVE                                        \
  "ld1w       {z0.s}, p2/z, [%[src_uyvy]]    \n" /* UYVY */ \
  "incb       %[src_uyvy]                    \n"            \
  "prfm       pldl1keep, [%[src_uyvy], 448]  \n"            \
  "tbl        z2.b, {z0.b}, z23.b            \n" /* V0V0 */ \
  "tbl        z1.b, {z0.b}, z22.b            \n" /* U0U0 */ \
  "trn2       z0.b, z0.b, z0.b               \n" /* YYYY */

#define YUVTORGB_SVE_SETUP                          \
  "ld1rb  {z28.b}, p0/z, [%[kUVCoeff], #0]      \n" \
  "ld1rb  {z29.b}, p0/z, [%[kUVCoeff], #1]      \n" \
  "ld1rb  {z30.b}, p0/z, [%[kUVCoeff], #2]      \n" \
  "ld1rb  {z31.b}, p0/z, [%[kUVCoeff], #3]      \n" \
  "ld1rh  {z24.h}, p0/z, [%[kRGBCoeffBias], #0] \n" \
  "ld1rh  {z25.h}, p0/z, [%[kRGBCoeffBias], #2] \n" \
  "ld1rh  {z26.h}, p0/z, [%[kRGBCoeffBias], #4] \n" \
  "ld1rh  {z27.h}, p0/z, [%[kRGBCoeffBias], #6] \n"

#define I4XXTORGB_SVE                                     \
  "umulh      z0.h, z24.h, z0.h              \n" /* Y */  \
  "umullb     z6.h, z30.b, z1.b              \n"          \
  "umullb     z4.h, z28.b, z1.b              \n" /* DB */ \
  "umullb     z5.h, z29.b, z2.b              \n" /* DR */ \
  "umlalb     z6.h, z31.b, z2.b              \n" /* DG */ \
  "add        z17.h, z0.h, z26.h             \n" /* G */  \
  "add        z16.h, z0.h, z4.h              \n" /* B */  \
  "add        z18.h, z0.h, z5.h              \n" /* R */  \
  "uqsub      z17.h, z17.h, z6.h             \n" /* G */  \
  "uqsub      z16.h, z16.h, z25.h            \n" /* B */  \
  "uqsub      z18.h, z18.h, z27.h            \n" /* R */

#define I400TORGB_SVE                                    \
  "umulh      z18.h, z24.h, z0.h             \n" /* Y */ \
  "movprfx    z16, z18                       \n"         \
  "usqadd     z16.h, p0/m, z16.h, z4.h       \n" /* B */ \
  "movprfx    z17, z18                       \n"         \
  "usqadd     z17.h, p0/m, z17.h, z6.h       \n" /* G */ \
  "usqadd     z18.h, p0/m, z18.h, z5.h       \n" /* R */

// Convert from 2.14 fixed point RGB to 8 bit ARGB, interleaving as BG and RA
// pairs to allow us to use ST2 for storing rather than ST4.
#define RGBTOARGB8_SVE                                    \
  /* Inputs: B: z16.h,  G: z17.h,  R: z18.h,  A: z19.b */ \
  "uqshrnb     z16.b, z16.h, #6     \n" /* B0 */          \
  "uqshrnb     z18.b, z18.h, #6     \n" /* R0 */          \
  "uqshrnt     z16.b, z17.h, #6     \n" /* BG */          \
  "trn1        z17.b, z18.b, z19.b  \n" /* RA */

// Convert from 2.14 fixed point RGB to 8 bit RGBA, interleaving as AB and GR
// pairs to allow us to use ST2 for storing rather than ST4.
#define RGBTORGBA8_SVE                                    \
  /* Inputs: B: z16.h,  G: z17.h,  R: z18.h,  A: z19.b */ \
  "uqshrnt     z19.b, z16.h, #6     \n" /* AB */          \
  "uqshrnb     z20.b, z17.h, #6     \n" /* G0 */          \
  "uqshrnt     z20.b, z18.h, #6     \n" /* GR */

#define YUVTORGB_SVE_REGS                                                     \
  "z0", "z1", "z2", "z3", "z4", "z5", "z6", "z7", "z16", "z17", "z18", "z19", \
      "z20", "z22", "z23", "z24", "z25", "z26", "z27", "z28", "z29", "z30",   \
      "z31", "p0", "p1"

void I444ToARGBRow_SVE2(const uint8_t* src_y,
                        const uint8_t* src_u,
                        const uint8_t* src_v,
                        uint8_t* dst_argb,
                        const struct YuvConstants* yuvconstants,
                        int width) {
  uint64_t vl;
  asm volatile (
      "cnth     %[vl]                                   \n"
      "ptrue    p0.b                                    \n" YUVTORGB_SVE_SETUP
      "dup      z19.b, #255                             \n" /* A */
      "subs     %w[width], %w[width], %w[vl]            \n"
      "b.lt     2f                                      \n"

      // Run bulk of computation with an all-true predicate to avoid predicate
      // generation overhead.
      "ptrue    p1.h                                    \n"
      "1:                                               \n" READYUV444_SVE
          I4XXTORGB_SVE RGBTOARGB8_SVE
      "subs     %w[width], %w[width], %w[vl]            \n"
      "st2h     {z16.h, z17.h}, p1, [%[dst_argb]]       \n"
      "add      %[dst_argb], %[dst_argb], %[vl], lsl #2 \n"
      "b.ge     1b                                      \n"

      "2:                                               \n"
      "adds     %w[width], %w[width], %w[vl]            \n"
      "b.eq     99f                                     \n"

      // Calculate a predicate for the final iteration to deal with the tail.
      "whilelt  p1.h, wzr, %w[width]                    \n" READYUV444_SVE
          I4XXTORGB_SVE RGBTOARGB8_SVE
      "st2h     {z16.h, z17.h}, p1, [%[dst_argb]]       \n"

      "99:                                              \n"
      : [src_y] "+r"(src_y),                               // %[src_y]
        [src_u] "+r"(src_u),                               // %[src_u]
        [src_v] "+r"(src_v),                               // %[src_v]
        [dst_argb] "+r"(dst_argb),                         // %[dst_argb]
        [width] "+r"(width),                               // %[width]
        [vl] "=&r"(vl)                                     // %[vl]
      : [kUVCoeff] "r"(&yuvconstants->kUVCoeff),           // %[kUVCoeff]
        [kRGBCoeffBias] "r"(&yuvconstants->kRGBCoeffBias)  // %[kRGBCoeffBias]
      : "cc", "memory", YUVTORGB_SVE_REGS);
}

void I400ToARGBRow_SVE2(const uint8_t* src_y,
                        uint8_t* dst_argb,
                        const struct YuvConstants* yuvconstants,
                        int width) {
  uint64_t vl;
  asm volatile (
      "cnth     %[vl]                                   \n"
      "ptrue    p0.b                                    \n"
      "dup      z19.b, #255                             \n"  // A
      YUVTORGB_SVE_SETUP
      "cmp      %w[width], %w[vl]                       \n"
      "mov      z1.h, #128                              \n"  // U/V
      "umullb   z6.h, z30.b, z1.b                       \n"
      "umullb   z4.h, z28.b, z1.b                       \n"  // DB
      "umullb   z5.h, z29.b, z1.b                       \n"  // DR
      "mla      z6.h, p0/m, z31.h, z1.h                 \n"  // DG
      "sub      z4.h, z4.h, z25.h                       \n"
      "sub      z5.h, z5.h, z27.h                       \n"
      "sub      z6.h, z26.h, z6.h                       \n"
      "b.le     2f                                      \n"

      // Run bulk of computation with an all-true predicate to avoid predicate
      // generation overhead.
      "ptrue    p1.h                                    \n"
      "sub      %w[width], %w[width], %w[vl]            \n"
      "1:                                               \n"  //
      READYUV400_SVE I400TORGB_SVE RGBTOARGB8_SVE
      "subs     %w[width], %w[width], %w[vl]            \n"
      "st2h     {z16.h, z17.h}, p1, [%[dst_argb]]       \n"
      "add      %[dst_argb], %[dst_argb], %[vl], lsl #2 \n"
      "b.gt     1b                                      \n"
      "add      %w[width], %w[width], %w[vl]            \n"

      // Calculate a predicate for the final iteration to deal with the tail.
      "2:                                               \n"
      "whilelt  p1.h, wzr, %w[width]                    \n"  //
      READYUV400_SVE I400TORGB_SVE RGBTOARGB8_SVE
      "st2h     {z16.h, z17.h}, p1, [%[dst_argb]]       \n"
      : [src_y] "+r"(src_y),                               // %[src_y]
        [dst_argb] "+r"(dst_argb),                         // %[dst_argb]
        [width] "+r"(width),                               // %[width]
        [vl] "=&r"(vl)                                     // %[vl]
      : [kUVCoeff] "r"(&yuvconstants->kUVCoeff),           // %[kUVCoeff]
        [kRGBCoeffBias] "r"(&yuvconstants->kRGBCoeffBias)  // %[kRGBCoeffBias]
      : "cc", "memory", YUVTORGB_SVE_REGS);
}

void I422ToARGBRow_SVE2(const uint8_t* src_y,
                        const uint8_t* src_u,
                        const uint8_t* src_v,
                        uint8_t* dst_argb,
                        const struct YuvConstants* yuvconstants,
                        int width) {
  uint64_t vl;
  asm volatile (
      "cnth     %[vl]                                   \n"
      "ptrue    p0.b                                    \n" YUVTORGB_SVE_SETUP
      "dup      z19.b, #255                             \n" /* A */
      "subs     %w[width], %w[width], %w[vl]            \n"
      "b.lt     2f                                      \n"

      // Run bulk of computation with an all-true predicate to avoid predicate
      // generation overhead.
      "ptrue    p1.h                                    \n"
      "1:                                               \n" READYUV422_SVE
          I4XXTORGB_SVE RGBTOARGB8_SVE
      "subs     %w[width], %w[width], %w[vl]            \n"
      "st2h     {z16.h, z17.h}, p1, [%[dst_argb]]       \n"
      "add      %[dst_argb], %[dst_argb], %[vl], lsl #2 \n"
      "b.ge     1b                                      \n"

      "2:                                               \n"
      "adds     %w[width], %w[width], %w[vl]            \n"
      "b.eq     99f                                     \n"

      // Calculate a predicate for the final iteration to deal with the tail.
      "whilelt  p1.h, wzr, %w[width]                    \n" READYUV422_SVE
          I4XXTORGB_SVE RGBTOARGB8_SVE
      "st2h     {z16.h, z17.h}, p1, [%[dst_argb]]       \n"

      "99:                                              \n"
      : [src_y] "+r"(src_y),                               // %[src_y]
        [src_u] "+r"(src_u),                               // %[src_u]
        [src_v] "+r"(src_v),                               // %[src_v]
        [dst_argb] "+r"(dst_argb),                         // %[dst_argb]
        [width] "+r"(width),                               // %[width]
        [vl] "=&r"(vl)                                     // %[vl]
      : [kUVCoeff] "r"(&yuvconstants->kUVCoeff),           // %[kUVCoeff]
        [kRGBCoeffBias] "r"(&yuvconstants->kRGBCoeffBias)  // %[kRGBCoeffBias]
      : "cc", "memory", YUVTORGB_SVE_REGS);
}

void I422ToRGBARow_SVE2(const uint8_t* src_y,
                        const uint8_t* src_u,
                        const uint8_t* src_v,
                        uint8_t* dst_argb,
                        const struct YuvConstants* yuvconstants,
                        int width) {
  uint64_t vl;
  asm volatile (
      "cnth     %[vl]                                   \n"
      "ptrue    p0.b                                    \n" YUVTORGB_SVE_SETUP
      "dup      z19.b, #255                             \n"  // A
      "subs     %w[width], %w[width], %w[vl]            \n"
      "b.le     2f                                      \n"

      // Run bulk of computation with an all-true predicate to avoid predicate
      // generation overhead.
      "ptrue    p1.h                                    \n"
      "1:                                               \n"  //
      READYUV422_SVE I4XXTORGB_SVE RGBTORGBA8_SVE
      "subs     %w[width], %w[width], %w[vl]            \n"
      "st2h     {z19.h, z20.h}, p1, [%[dst_argb]]       \n"
      "add      %[dst_argb], %[dst_argb], %[vl], lsl #2 \n"
      "b.gt     1b                                      \n"

      // Calculate a predicate for the final iteration to deal with the tail.
      "2:                                               \n"
      "adds    %w[width], %w[width], %w[vl]             \n"
      "b.eq    99f                                      \n"

      "whilelt  p1.h, wzr, %w[width]                    \n"  //
      READYUV422_SVE I4XXTORGB_SVE RGBTORGBA8_SVE
      "st2h     {z19.h, z20.h}, p1, [%[dst_argb]]       \n"

      "99:                                              \n"
      : [src_y] "+r"(src_y),                               // %[src_y]
        [src_u] "+r"(src_u),                               // %[src_u]
        [src_v] "+r"(src_v),                               // %[src_v]
        [dst_argb] "+r"(dst_argb),                         // %[dst_argb]
        [width] "+r"(width),                               // %[width]
        [vl] "=&r"(vl)                                     // %[vl]
      : [kUVCoeff] "r"(&yuvconstants->kUVCoeff),           // %[kUVCoeff]
        [kRGBCoeffBias] "r"(&yuvconstants->kRGBCoeffBias)  // %[kRGBCoeffBias]
      : "cc", "memory", YUVTORGB_SVE_REGS);
}

void I444AlphaToARGBRow_SVE2(const uint8_t* src_y,
                             const uint8_t* src_u,
                             const uint8_t* src_v,
                             const uint8_t* src_a,
                             uint8_t* dst_argb,
                             const struct YuvConstants* yuvconstants,
                             int width) {
  uint64_t vl;
  asm volatile (
      "cnth     %[vl]                                   \n"
      "ptrue    p0.b                                    \n" YUVTORGB_SVE_SETUP
      "subs     %w[width], %w[width], %w[vl]            \n"
      "b.lt     2f                                      \n"

      // Run bulk of computation with an all-true predicate to avoid predicate
      // generation overhead.
      "ptrue    p1.h                                    \n"
      "1:                                               \n" READYUV444_SVE
      "ld1b     {z19.h}, p1/z, [%[src_a]]               \n"
      "add      %[src_a], %[src_a], %[vl]               \n"  // A
      I4XXTORGB_SVE RGBTOARGB8_SVE
      "subs     %w[width], %w[width], %w[vl]            \n"
      "st2h     {z16.h, z17.h}, p1, [%[dst_argb]]       \n"
      "add      %[dst_argb], %[dst_argb], %[vl], lsl #2 \n"
      "b.ge     1b                                      \n"

      "2:                                               \n"
      "adds     %w[width], %w[width], %w[vl]            \n"
      "b.eq     99f                                     \n"

      // Calculate a predicate for the final iteration to deal with the tail.
      "whilelt  p1.h, wzr, %w[width]                    \n" READYUV444_SVE
      "ld1b     {z19.h}, p1/z, [%[src_a]]               \n"  // A
      I4XXTORGB_SVE RGBTOARGB8_SVE
      "st2h     {z16.h, z17.h}, p1, [%[dst_argb]]       \n"

      "99:                                              \n"
      : [src_y] "+r"(src_y),                               // %[src_y]
        [src_u] "+r"(src_u),                               // %[src_u]
        [src_v] "+r"(src_v),                               // %[src_v]
        [src_a] "+r"(src_a),                               // %[src_a]
        [dst_argb] "+r"(dst_argb),                         // %[dst_argb]
        [width] "+r"(width),                               // %[width]
        [vl] "=&r"(vl)                                     // %[vl]
      : [kUVCoeff] "r"(&yuvconstants->kUVCoeff),           // %[kUVCoeff]
        [kRGBCoeffBias] "r"(&yuvconstants->kRGBCoeffBias)  // %[kRGBCoeffBias]
      : "cc", "memory", YUVTORGB_SVE_REGS);
}

void I422AlphaToARGBRow_SVE2(const uint8_t* src_y,
                             const uint8_t* src_u,
                             const uint8_t* src_v,
                             const uint8_t* src_a,
                             uint8_t* dst_argb,
                             const struct YuvConstants* yuvconstants,
                             int width) {
  uint64_t vl;
  asm volatile (
      "cnth     %[vl]                                   \n"
      "ptrue    p0.b                                    \n" YUVTORGB_SVE_SETUP
      "subs     %w[width], %w[width], %w[vl]            \n"
      "b.lt     2f                                      \n"

      // Run bulk of computation with an all-true predicate to avoid predicate
      // generation overhead.
      "ptrue    p1.h                                    \n"
      "1:                                               \n" READYUV422_SVE
      "ld1b     {z19.h}, p1/z, [%[src_a]]               \n"
      "add      %[src_a], %[src_a], %[vl]               \n"  // A
      I4XXTORGB_SVE RGBTOARGB8_SVE
      "subs     %w[width], %w[width], %w[vl]            \n"
      "st2h     {z16.h, z17.h}, p1, [%[dst_argb]]       \n"
      "add      %[dst_argb], %[dst_argb], %[vl], lsl #2 \n"
      "b.ge     1b                                      \n"

      "2:                                               \n"
      "adds     %w[width], %w[width], %w[vl]            \n"
      "b.eq     99f                                     \n"

      // Calculate a predicate for the final iteration to deal with the tail.
      "whilelt  p1.h, wzr, %w[width]                    \n" READYUV422_SVE
      "ld1b     {z19.h}, p1/z, [%[src_a]]               \n"  // A
      I4XXTORGB_SVE RGBTOARGB8_SVE
      "st2h     {z16.h, z17.h}, p1, [%[dst_argb]]       \n"

      "99:                                              \n"
      : [src_y] "+r"(src_y),                               // %[src_y]
        [src_u] "+r"(src_u),                               // %[src_u]
        [src_v] "+r"(src_v),                               // %[src_v]
        [src_a] "+r"(src_a),                               // %[src_a]
        [dst_argb] "+r"(dst_argb),                         // %[dst_argb]
        [width] "+r"(width),                               // %[width]
        [vl] "=&r"(vl)                                     // %[vl]
      : [kUVCoeff] "r"(&yuvconstants->kUVCoeff),           // %[kUVCoeff]
        [kRGBCoeffBias] "r"(&yuvconstants->kRGBCoeffBias)  // %[kRGBCoeffBias]
      : "cc", "memory", YUVTORGB_SVE_REGS);
}

static inline void NVToARGBRow_SVE2(const uint8_t* src_y,
                                    const uint8_t* src_uv,
                                    uint8_t* dst_argb,
                                    const struct YuvConstants* yuvconstants,
                                    int width,
                                    uint32_t nv_u_start,
                                    uint32_t nv_u_step,
                                    uint32_t nv_v_start,
                                    uint32_t nv_v_step) {
  uint64_t vl;
  asm volatile (
      "cnth %0" : "=r"(vl));
  int width_last_y = width & (vl - 1);
  width_last_y = width_last_y == 0 ? vl : width_last_y;
  int width_last_uv = width_last_y + (width_last_y & 1);
  asm volatile (
      "ptrue    p0.b                                    \n" YUVTORGB_SVE_SETUP
      "index    z22.s, %w[nv_u_start], %w[nv_u_step]    \n"
      "index    z23.s, %w[nv_v_start], %w[nv_v_step]    \n"
      "dup      z19.b, #255                             \n"  // A
      "subs     %w[width], %w[width], %w[vl]            \n"
      "b.le     2f                                      \n"

      // Run bulk of computation with an all-true predicate to avoid predicate
      // generation overhead.
      "ptrue    p1.h                                    \n"
      "ptrue    p2.h                                    \n"
      "1:                                               \n" READNV_SVE
          I4XXTORGB_SVE RGBTOARGB8_SVE
      "subs     %w[width], %w[width], %w[vl]            \n"
      "st2h     {z16.h, z17.h}, p1, [%[dst_argb]]       \n"
      "add      %[dst_argb], %[dst_argb], %[vl], lsl #2 \n"
      "b.gt     1b                                      \n"

      "2:                                               \n"
      "adds     %w[width], %w[width], %w[vl]            \n"
      "b.eq     99f                                     \n"

      // Calculate a predicate for the final iteration to deal with the tail.
      "3:                                               \n"
      "whilelt  p1.h, wzr, %w[width_last_y]             \n"
      "whilelt  p2.h, wzr, %w[width_last_uv]            \n" READNV_SVE
          I4XXTORGB_SVE RGBTOARGB8_SVE
      "st2h     {z16.h, z17.h}, p1, [%[dst_argb]]       \n"

      "99:                                              \n"
      : [src_y] "+r"(src_y),                                // %[src_y]
        [src_uv] "+r"(src_uv),                              // %[src_uv]
        [dst_argb] "+r"(dst_argb),                          // %[dst_argb]
        [width] "+r"(width)                                 // %[width]
      : [vl] "r"(vl),                                       // %[vl]
        [kUVCoeff] "r"(&yuvconstants->kUVCoeff),            // %[kUVCoeff]
        [kRGBCoeffBias] "r"(&yuvconstants->kRGBCoeffBias),  // %[kRGBCoeffBias]
        [nv_u_start] "r"(nv_u_start),                       // %[nv_u_start]
        [nv_u_step] "r"(nv_u_step),                         // %[nv_u_step]
        [nv_v_start] "r"(nv_v_start),                       // %[nv_v_start]
        [nv_v_step] "r"(nv_v_step),                         // %[nv_v_step]
        [width_last_y] "r"(width_last_y),                   // %[width_last_y]
        [width_last_uv] "r"(width_last_uv)                  // %[width_last_uv]
      : "cc", "memory", YUVTORGB_SVE_REGS);
}

void NV12ToARGBRow_SVE2(const uint8_t* src_y,
                        const uint8_t* src_uv,
                        uint8_t* dst_argb,
                        const struct YuvConstants* yuvconstants,
                        int width) {
  uint32_t nv_u_start = 0x0000'0000U;
  uint32_t nv_u_step = 0x0002'0002U;
  uint32_t nv_v_start = 0x0001'0001U;
  uint32_t nv_v_step = 0x0002'0002U;
  NVToARGBRow_SVE2(src_y, src_uv, dst_argb, yuvconstants, width, nv_u_start,
                   nv_u_step, nv_v_start, nv_v_step);
}

void NV21ToARGBRow_SVE2(const uint8_t* src_y,
                        const uint8_t* src_vu,
                        uint8_t* dst_argb,
                        const struct YuvConstants* yuvconstants,
                        int width) {
  uint32_t nv_u_start = 0x0001'0001U;
  uint32_t nv_u_step = 0x0002'0002U;
  uint32_t nv_v_start = 0x0000'0000U;
  uint32_t nv_v_step = 0x0002'0002U;
  NVToARGBRow_SVE2(src_y, src_vu, dst_argb, yuvconstants, width, nv_u_start,
                   nv_u_step, nv_v_start, nv_v_step);
}

// Dot-product constants are stored as four-tuples with the two innermost
// elements flipped to account for the interleaving nature of the widening
// addition instructions.

static const int16_t kARGBToUVCoefficients[] = {
    // UB, -UR, -UG, 0, -VB, VR, -VG, 0
    56, -19, -37, 0, -9, 56, -47, 0,
};

static const int16_t kRGBAToUVCoefficients[] = {
    // 0, -UG, UB, -UR, 0, -VG, -VB, VR
    0, -37, 56, -19, 0, -47, -9, 56,
};

static const int16_t kBGRAToUVCoefficients[] = {
    // 0, -UG, -UR, UB, 0, -VG, VR, -VB
    0, -37, -19, 56, 0, -47, 56, -9,
};

static const int16_t kABGRToUVCoefficients[] = {
    // -UR, UB, -UG, 0, VR, -VB, -VG, 0
    -19, 56, -37, 0, 56, -9, -47, 0,
};

static const int16_t kARGBToUVJCoefficients[] = {
    // UB, -UR, -UG, 0, -VB, VR, -VG, 0
    63, -21, -42, 0, -10, 63, -53, 0,
};

static const int16_t kABGRToUVJCoefficients[] = {
    // -UR, UB, -UG, 0, VR, -VB, -VG, 0
    -21, 63, -42, 0, 63, -10, -53, 0,
};

void ARGBToUVMatrixRow_SVE2(const uint8_t* src_argb,
                            int src_stride_argb,
                            uint8_t* dst_u,
                            uint8_t* dst_v,
                            int width,
                            const int16_t* uvconstants) {
  const uint8_t* src_argb_1 = src_argb + src_stride_argb;
  uint64_t vl;
  asm volatile (
      "ptrue    p0.b                                \n"
      "ld1rd    {z24.d}, p0/z, [%[uvconstants]]     \n"
      "ld1rd    {z25.d}, p0/z, [%[uvconstants], #8] \n"
      "mov      z26.b, #0x80                        \n"

      "cntb     %[vl]                               \n"
      "subs     %w[width], %w[width], %w[vl]        \n"
      "b.lt     2f                                  \n"

      // Process 4x vectors from each input row per iteration.
      // Cannot use predication here due to unrolling.
      "1:                                           \n"  // e.g.
      "ld1b     {z0.b}, p0/z, [%[src0], #0, mul vl] \n"  // bgrabgra
      "ld1b     {z4.b}, p0/z, [%[src1], #0, mul vl] \n"  // bgrabgra
      "ld1b     {z1.b}, p0/z, [%[src0], #1, mul vl] \n"  // bgrabgra
      "ld1b     {z5.b}, p0/z, [%[src1], #1, mul vl] \n"  // bgrabgra
      "ld1b     {z2.b}, p0/z, [%[src0], #2, mul vl] \n"  // bgrabgra
      "ld1b     {z6.b}, p0/z, [%[src1], #2, mul vl] \n"  // bgrabgra
      "ld1b     {z3.b}, p0/z, [%[src0], #3, mul vl] \n"  // bgrabgra
      "ld1b     {z7.b}, p0/z, [%[src1], #3, mul vl] \n"  // bgrabgra
      "incb     %[src0], all, mul #4                \n"
      "incb     %[src1], all, mul #4                \n"

      "uaddlb   z16.h, z0.b, z4.b                   \n"  // brbrbrbr
      "uaddlt   z17.h, z0.b, z4.b                   \n"  // gagagaga
      "uaddlb   z18.h, z1.b, z5.b                   \n"  // brbrbrbr
      "uaddlt   z19.h, z1.b, z5.b                   \n"  // gagagaga
      "uaddlb   z20.h, z2.b, z6.b                   \n"  // brbrbrbr
      "uaddlt   z21.h, z2.b, z6.b                   \n"  // gagagaga
      "uaddlb   z22.h, z3.b, z7.b                   \n"  // brbrbrbr
      "uaddlt   z23.h, z3.b, z7.b                   \n"  // gagagaga

      "trn1     z0.s, z16.s, z17.s                  \n"  // brgabgra
      "trn2     z1.s, z16.s, z17.s                  \n"  // brgabgra
      "trn1     z2.s, z18.s, z19.s                  \n"  // brgabgra
      "trn2     z3.s, z18.s, z19.s                  \n"  // brgabgra
      "trn1     z4.s, z20.s, z21.s                  \n"  // brgabgra
      "trn2     z5.s, z20.s, z21.s                  \n"  // brgabgra
      "trn1     z6.s, z22.s, z23.s                  \n"  // brgabgra
      "trn2     z7.s, z22.s, z23.s                  \n"  // brgabgra

      "subs     %w[width], %w[width], %w[vl]        \n"  // 4*VL per loop

      "urhadd   z0.h, p0/m, z0.h, z1.h              \n"  // brgabrga
      "urhadd   z2.h, p0/m, z2.h, z3.h              \n"  // brgabrga
      "urhadd   z4.h, p0/m, z4.h, z5.h              \n"  // brgabrga
      "urhadd   z6.h, p0/m, z6.h, z7.h              \n"  // brgabrga

      "movi     v16.8h, #0                          \n"
      "movi     v17.8h, #0                          \n"
      "movi     v18.8h, #0                          \n"
      "movi     v19.8h, #0                          \n"

      "movi     v20.8h, #0                          \n"
      "movi     v21.8h, #0                          \n"
      "movi     v22.8h, #0                          \n"
      "movi     v23.8h, #0                          \n"

      "sdot     z16.d, z0.h, z24.h                  \n"  // UUxxxxxx
      "sdot     z17.d, z2.h, z24.h                  \n"  // UUxxxxxx
      "sdot     z18.d, z4.h, z24.h                  \n"  // UUxxxxxx
      "sdot     z19.d, z6.h, z24.h                  \n"  // UUxxxxxx

      "sdot     z20.d, z0.h, z25.h                  \n"  // VVxxxxxx
      "sdot     z21.d, z2.h, z25.h                  \n"  // VVxxxxxx
      "sdot     z22.d, z4.h, z25.h                  \n"  // VVxxxxxx
      "sdot     z23.d, z6.h, z25.h                  \n"  // VVxxxxxx

      "uzp1     z16.s, z16.s, z17.s                 \n"  // UUxx
      "uzp1     z18.s, z18.s, z19.s                 \n"  // UUxx
      "uzp1     z20.s, z20.s, z21.s                 \n"  // VVxx
      "uzp1     z22.s, z22.s, z23.s                 \n"  // VVxx

      "uzp1     z16.h, z16.h, z18.h                 \n"  // UU
      "uzp1     z20.h, z20.h, z22.h                 \n"  // VV

      "addhnb   z16.b, z16.h, z26.h                 \n"  // U
      "addhnb   z20.b, z20.h, z26.h                 \n"  // V

      "st1b     {z16.h}, p0, [%[dst_u]]             \n"  // U
      "st1b     {z20.h}, p0, [%[dst_v]]             \n"  // V
      "inch     %[dst_u]                            \n"
      "inch     %[dst_v]                            \n"

      "b.ge     1b                                  \n"

      "2:                                           \n"
      "adds     %w[width], %w[width], %w[vl]        \n"  // VL per loop
      "b.le     99f                                 \n"

      // Process remaining pixels from each input row.
      // Use predication to do one vector from each input array, so may loop up
      // to three iterations.
      "cntw     %x[vl]                              \n"

      "3:                                           \n"
      "whilelt  p1.s, wzr, %w[width]                \n"
      "ld1d     {z0.d}, p1/z, [%[src0]]             \n"  // bgrabgra
      "ld1d     {z4.d}, p1/z, [%[src1]]             \n"  // bgrabgra
      "incb     %[src0]                             \n"
      "incb     %[src1]                             \n"

      "uaddlb   z16.h, z0.b, z4.b                   \n"  // brbrbrbr
      "uaddlt   z17.h, z0.b, z4.b                   \n"  // gagagaga

      "trn1     z0.s, z16.s, z17.s                  \n"  // brgabgra
      "trn2     z1.s, z16.s, z17.s                  \n"  // brgabgra

      "urhadd   z0.h, p0/m, z0.h, z1.h              \n"  // brgabrga

      "subs     %w[width], %w[width], %w[vl]        \n"  // VL per loop

      "movi     v16.8h, #0                          \n"
      "movi     v20.8h, #0                          \n"

      "sdot     z16.d, z0.h, z24.h                  \n"
      "sdot     z20.d, z0.h, z25.h                  \n"

      "addhnb   z16.b, z16.h, z26.h                 \n"  // U
      "addhnb   z20.b, z20.h, z26.h                 \n"  // V

      "st1b     {z16.d}, p0, [%[dst_u]]             \n"  // U
      "st1b     {z20.d}, p0, [%[dst_v]]             \n"  // V
      "incd     %[dst_u]                            \n"
      "incd     %[dst_v]                            \n"
      "b.gt     3b                                  \n"

      "99:                                          \n"
      : [src0] "+r"(src_argb),    // %[src0]
        [src1] "+r"(src_argb_1),  // %[src1]
        [dst_u] "+r"(dst_u),      // %[dst_u]
        [dst_v] "+r"(dst_v),      // %[dst_v]
        [width] "+r"(width),      // %[width]
        [vl] "=&r"(vl)            // %[vl]
      : [uvconstants] "r"(uvconstants)
      : "cc", "memory", "z0", "z1", "z2", "z3", "z4", "z5", "z6", "z7", "z16",
        "z17", "z18", "z19", "z20", "z21", "z22", "z23", "z24", "z25", "z26",
        "p0");
}

void ARGBToUVRow_SVE2(const uint8_t* src_argb,
                      int src_stride_argb,
                      uint8_t* dst_u,
                      uint8_t* dst_v,
                      int width) {
  ARGBToUVMatrixRow_SVE2(src_argb, src_stride_argb, dst_u, dst_v, width,
                         kARGBToUVCoefficients);
}

void ARGBToUVJRow_SVE2(const uint8_t* src_argb,
                       int src_stride_argb,
                       uint8_t* dst_u,
                       uint8_t* dst_v,
                       int width) {
  ARGBToUVMatrixRow_SVE2(src_argb, src_stride_argb, dst_u, dst_v, width,
                         kARGBToUVJCoefficients);
}

void ABGRToUVJRow_SVE2(const uint8_t* src_abgr,
                       int src_stride_abgr,
                       uint8_t* dst_uj,
                       uint8_t* dst_vj,
                       int width) {
  ARGBToUVMatrixRow_SVE2(src_abgr, src_stride_abgr, dst_uj, dst_vj, width,
                         kABGRToUVJCoefficients);
}

void BGRAToUVRow_SVE2(const uint8_t* src_bgra,
                      int src_stride_bgra,
                      uint8_t* dst_u,
                      uint8_t* dst_v,
                      int width) {
  ARGBToUVMatrixRow_SVE2(src_bgra, src_stride_bgra, dst_u, dst_v, width,
                         kBGRAToUVCoefficients);
}

void ABGRToUVRow_SVE2(const uint8_t* src_abgr,
                      int src_stride_abgr,
                      uint8_t* dst_u,
                      uint8_t* dst_v,
                      int width) {
  ARGBToUVMatrixRow_SVE2(src_abgr, src_stride_abgr, dst_u, dst_v, width,
                         kABGRToUVCoefficients);
}

void RGBAToUVRow_SVE2(const uint8_t* src_rgba,
                      int src_stride_rgba,
                      uint8_t* dst_u,
                      uint8_t* dst_v,
                      int width) {
  ARGBToUVMatrixRow_SVE2(src_rgba, src_stride_rgba, dst_u, dst_v, width,
                         kRGBAToUVCoefficients);
}

#define ARGBTORGB565_SVE                    \
  /* Inputs:                                \
   * z0: rrrrrxxxbbbbbxxx                   \
   * z1: xxxxxxxxggggggxx                   \
   * z3: 0000000000000011 (3, 0, 3, 0, ...) \
   * z4: 0000011111100000                   \
   */                                       \
  "lsr     z0.b, p0/m, z0.b, z3.b       \n" \
  "lsl     z1.h, z1.h, #3               \n" \
  "bsl     z1.d, z1.d, z0.d, z4.d       \n"

void ARGBToRGB565Row_SVE2(const uint8_t* src_argb,
                          uint8_t* dst_rgb,
                          int width) {
  unsigned bsl_mask = 0x7e0;
  uint64_t vl;
  width *= 2;
  asm volatile (
      "mov     z3.h, #3                     \n"
      "dup     z4.h, %w[bsl_mask]           \n"

      "cntb    %[vl]                        \n"
      "subs    %w[width], %w[width], %w[vl] \n"
      "b.lt    2f                           \n"

      "ptrue   p0.b                         \n"
      "1:                                   \n"
      "ld2b    {z0.b, z1.b}, p0/z, [%[src]] \n"  // BR, GA
      "incb    %[src], all, mul #2          \n"
      "subs    %w[width], %w[width], %w[vl] \n" ARGBTORGB565_SVE
      "st1b    {z1.b}, p0, [%[dst]]         \n"
      "incb    %[dst]                       \n"
      "b.ge    1b                           \n"

      "2:                                   \n"
      "adds    %w[width], %w[width], %w[vl] \n"
      "b.eq    99f                          \n"

      "whilelt p0.b, wzr, %w[width]         \n"
      "ld2b    {z0.b, z1.b}, p0/z, [%[src]] \n"  // BR, GA
      ARGBTORGB565_SVE
      "st1b    {z1.b}, p0, [%[dst]]         \n"

      "99:                                  \n"
      : [src] "+r"(src_argb),     // %[src]
        [dst] "+r"(dst_rgb),      // %[dst]
        [width] "+r"(width),      // %[width]
        [vl] "=&r"(vl)            // %[vl]
      : [bsl_mask] "r"(bsl_mask)  // %[bsl_mask]
      : "cc", "memory", "z0", "z1", "z3", "z4", "p0");
}

void ARGBToRGB565DitherRow_SVE2(const uint8_t* src_argb,
                                uint8_t* dst_rgb,
                                uint32_t dither4,
                                int width) {
  unsigned bsl_mask = 0x7e0;
  uint64_t vl;
  width *= 2;
  asm volatile (
      "mov     z3.h, #3                     \n"
      "dup     z4.h, %w[bsl_mask]           \n"
      "dup     z2.s, %w[dither4]            \n"
      "zip1    z2.b, z2.b, z2.b             \n"

      "cntb    %[vl]                        \n"
      "subs    %w[width], %w[width], %w[vl] \n"
      "b.lt    2f                           \n"

      "ptrue   p0.b                         \n"
      "1:                                   \n"
      "ld2b    {z0.b, z1.b}, p0/z, [%[src]] \n"  // BR, GA
      "incb    %[src], all, mul #2          \n"
      "uqadd   z0.b, z0.b, z2.b             \n"
      "uqadd   z1.b, z1.b, z2.b             \n"
      "subs    %w[width], %w[width], %w[vl] \n" ARGBTORGB565_SVE
      "st1b    {z1.b}, p0, [%[dst]]         \n"
      "incb    %[dst]                       \n"
      "b.ge    1b                           \n"

      "2:                                   \n"
      "adds    %w[width], %w[width], %w[vl] \n"
      "b.eq    99f                          \n"

      "whilelt p0.b, wzr, %w[width]         \n"
      "ld2b    {z0.b, z1.b}, p0/z, [%[src]] \n"  // BR, GA
      "uqadd   z0.b, z0.b, z2.b             \n"
      "uqadd   z1.b, z1.b, z2.b             \n" ARGBTORGB565_SVE
      "st1b    {z1.b}, p0, [%[dst]]         \n"

      "99:                                  \n"
      : [src] "+r"(src_argb),      // %[src]
        [dst] "+r"(dst_rgb),       // %[dst]
        [width] "+r"(width),       // %[width]
        [vl] "=&r"(vl)             // %[vl]
      : [bsl_mask] "r"(bsl_mask),  // %[bsl_mask]
        [dither4] "r"(dither4)     // %[dither4]
      : "cc", "memory", "z0", "z1", "z3", "z4", "p0");
}

#define ARGB1555TOARGB                                        \
  /* Input: z1/z3.h = arrrrrgggggbbbbb */                     \
  "lsl     z0.h, z1.h, #3          \n" /* rrrgggggbbbbb000 */ \
  "lsl     z2.h, z3.h, #3          \n" /* rrrgggggbbbbb000 */ \
  "asr     z1.h, z1.h, #7          \n" /* aaaaaaaarrrrrggg */ \
  "asr     z3.h, z3.h, #7          \n" /* aaaaaaaarrrrrggg */ \
  "lsl     z0.b, p0/m, z0.b, z4.b  \n" /* ggggg000bbbbb000 */ \
  "lsl     z2.b, p0/m, z2.b, z4.b  \n" /* ggggg000bbbbb000 */ \
  "sri     z1.b, z1.b, #5          \n" /* aaaaaaaarrrrrrrr */ \
  "sri     z3.b, z3.b, #5          \n" /* aaaaaaaarrrrrrrr */ \
  "sri     z0.b, z0.b, #5          \n" /* ggggggggbbbbbbbb */ \
  "sri     z2.b, z2.b, #5          \n" /* ggggggggbbbbbbbb */

void ARGB1555ToARGBRow_SVE2(const uint8_t* src_argb1555,
                            uint8_t* dst_argb,
                            int width) {
  uint64_t vl;
  asm volatile (
      "mov     z4.h, #0x0300                           \n"
      "ptrue   p0.b                                    \n"

      "cnth    %x[vl]                                  \n"
      "subs    %w[width], %w[width], %w[vl], lsl #1    \n"
      "b.lt    2f                                      \n"

      "1:                                              \n"
      "ld1h    {z1.h}, p0/z, [%[src]]                  \n"
      "ld1h    {z3.h}, p0/z, [%[src], #1, mul vl]      \n"
      "incb    %[src], all, mul #2                     \n" ARGB1555TOARGB
      "subs    %w[width], %w[width], %w[vl], lsl #1    \n"
      "st2h    {z0.h, z1.h}, p0, [%[dst]]              \n"
      "st2h    {z2.h, z3.h}, p0, [%[dst], #2, mul vl]  \n"
      "incb    %[dst], all, mul #4                     \n"
      "b.ge    1b                                      \n"

      "2:                                              \n"
      "adds    %w[width], %w[width], %w[vl], lsl #1    \n"
      "b.eq    99f                                     \n"

      "whilelt p1.h, wzr, %w[width]                    \n"
      "whilelt p2.h, %w[vl], %w[width]                 \n"
      "ld1h    {z1.h}, p1/z, [%[src]]                  \n"
      "ld1h    {z3.h}, p2/z, [%[src], #1, mul vl]      \n" ARGB1555TOARGB
      "st2h    {z0.h, z1.h}, p1, [%[dst]]              \n"
      "st2h    {z2.h, z3.h}, p2, [%[dst], #2, mul vl]  \n"

      "99:                                             \n"
      : [src] "+r"(src_argb1555),  // %[src]
        [dst] "+r"(dst_argb),      // %[dst]
        [width] "+r"(width),       // %[width]
        [vl] "=&r"(vl)             // %[vl]
      :
      : "cc", "memory", "z0", "z1", "z2", "z3", "z4", "p0", "p1", "p2");
}

// clang-format off
#define AYUVTOUV_SVE(zU0, zV0, zU1, zV1)                   /* e.g. */          \
  "ld2h     {z0.h, z1.h}, p0/z, [%[src0]]              \n" /* VUVU.. YAYA.. */ \
  "ld2h     {z1.h, z2.h}, p1/z, [%[src0], #2, mul vl]  \n" /* VUVU.. YAYA.. */ \
  "ld2h     {z2.h, z3.h}, p0/z, [%[src1]]              \n" /* VUVU.. YAYA.. */ \
  "ld2h     {z3.h, z4.h}, p1/z, [%[src1], #2, mul vl]  \n" /* VUVU.. YAYA.. */ \
  "incb     %[src0], all, mul #4                       \n"                     \
  "incb     %[src1], all, mul #4                       \n"                     \
  "uaddlb   z4.h, z0.b, z2.b                           \n" /* V */             \
  "uaddlt   z5.h, z0.b, z2.b                           \n" /* U */             \
  "uaddlb   z6.h, z1.b, z3.b                           \n" /* V */             \
  "uaddlt   z7.h, z1.b, z3.b                           \n" /* U */             \
  "addp   " #zU0 ".h, p0/m, " #zU0 ".h, " #zV0 ".h     \n" /* UV */            \
  "addp   " #zU1 ".h, p1/m, " #zU1 ".h, " #zV1 ".h     \n" /* UV */            \
  "subs     %w[width], %w[width], %w[vl]               \n"                     \
  "urshr  " #zU0 ".h, p0/m, " #zU0 ".h, #2             \n" /* U0V0 */          \
  "urshr  " #zU1 ".h, p1/m, " #zU1 ".h, #2             \n" /* U0V0 */          \
  "st1b     {" #zU0 ".h}, p0, [%[dst]]                 \n"                     \
  "st1b     {" #zU1 ".h}, p1, [%[dst], #1, mul vl]     \n"                     \
  "incb     %[dst]                                     \n"
// clang-format on

// Filter 2 rows of AYUV UV's (444) into UV (420).
// AYUV is VUYA in memory.  UV for NV12 is UV order in memory.
void AYUVToUVRow_SVE2(const uint8_t* src_ayuv,
                      int src_stride_ayuv,
                      uint8_t* dst_uv,
                      int width) {
  // Output a row of UV values, filtering 2x2 rows of AYUV.
  const uint8_t* src_ayuv1 = src_ayuv + src_stride_ayuv;
  int vl;
  asm volatile (
      "cntb    %x[vl]                            \n"
      "subs    %w[width], %w[width], %w[vl]      \n"
      "b.lt    2f                                \n"

      "ptrue   p0.h                              \n"
      "ptrue   p1.h                              \n"
      "1:                                        \n"
      AYUVTOUV_SVE(z5, z4, z7, z6)
      "b.ge    1b                                \n"

      "2:                                        \n"
      "adds    %w[width], %w[width], %w[vl]      \n"
      "b.eq    99f                               \n"

      "cnth    %x[vl]                            \n"
      "whilelt p0.h, wzr, %w[width]              \n" // first row
      "whilelt p1.h, %w[vl], %w[width]           \n" // second row
      AYUVTOUV_SVE(z5, z4, z7, z6)

      "99:                                       \n"
      : [src0]"+r"(src_ayuv),   // %[src0]
        [src1]"+r"(src_ayuv1),  // %[src1]
        [dst]"+r"(dst_uv),      // %[dst]
        [width]"+r"(width),     // %[width]
        [vl]"=&r"(vl)           // %[vl]
      :
      : "cc", "memory", "z0", "z1", "z2", "z3", "z4", "z5", "z6", "z7", "p0",
        "p1");
}

// Filter 2 rows of AYUV UV's (444) into VU (420).
void AYUVToVURow_SVE2(const uint8_t* src_ayuv,
                      int src_stride_ayuv,
                      uint8_t* dst_vu,
                      int width) {
  // Output a row of VU values, filtering 2x2 rows of AYUV.
  const uint8_t* src_ayuv1 = src_ayuv + src_stride_ayuv;
  int vl;
  asm volatile (
      "cntb    %x[vl]                            \n"
      "cmp     %w[width], %w[vl]                 \n"
      "subs    %w[width], %w[width], %w[vl]      \n"
      "b.lt    2f                                \n"

      "ptrue   p0.h                              \n"
      "ptrue   p1.h                              \n"
      "1:                                        \n"
      AYUVTOUV_SVE(z4, z5, z6, z7)
      "b.ge    1b                                \n"

      "2:                                        \n"
      "adds    %w[width], %w[width], %w[vl]      \n"
      "b.eq    99f                               \n"

      "cnth    %x[vl]                            \n"
      "whilelt p0.h, wzr, %w[width]              \n" // first row
      "whilelt p1.h, %w[vl], %w[width]           \n" // second row
      AYUVTOUV_SVE(z4, z5, z6, z7)

      "99:                                       \n"
      : [src0]"+r"(src_ayuv),   // %[src0]
        [src1]"+r"(src_ayuv1),  // %[src1]
        [dst]"+r"(dst_vu),      // %[dst]
        [width]"+r"(width),     // %[width]
        [vl]"=&r"(vl)           // %[vl]
      :
      : "cc", "memory", "z0", "z1", "z2", "z3", "z4", "z5", "z6", "z7", "p0",
        "p1");
}

void YUY2ToARGBRow_SVE2(const uint8_t* src_yuy2,
                        uint8_t* dst_argb,
                        const struct YuvConstants* yuvconstants,
                        int width) {
  uint32_t nv_u_start = 0x0001'0001U;
  uint32_t nv_u_step = 0x0004'0004U;
  uint32_t nv_v_start = 0x0003'0003U;
  uint32_t nv_v_step = 0x0004'0004U;
  uint64_t vl;
  asm volatile (
      "cnth %0" : "=r"(vl));
  int width_last_y = width & (vl - 1);
  int width_last_uv = width_last_y + (width_last_y & 1);
  asm volatile (
      "ptrue    p0.b                                    \n"
      "index    z22.s, %w[nv_u_start], %w[nv_u_step]    \n"
      "index    z23.s, %w[nv_v_start], %w[nv_v_step]    \n"
      "dup      z19.b, #255                             \n"  // A
      YUVTORGB_SVE_SETUP
      "subs     %w[width], %w[width], %w[vl]            \n"
      "b.lt     2f                                      \n"

      // Run bulk of computation with an all-true predicate to avoid predicate
      // generation overhead.
      "ptrue    p1.h                                    \n"
      "ptrue    p2.h                                    \n"
      "1:                                               \n"  //
      READYUY2_SVE I4XXTORGB_SVE RGBTOARGB8_SVE
      "subs     %w[width], %w[width], %w[vl]            \n"
      "st2h     {z16.h, z17.h}, p1, [%[dst_argb]]       \n"
      "add      %[dst_argb], %[dst_argb], %[vl], lsl #2 \n"
      "b.ge     1b                                      \n"

      "2:                                               \n"
      "adds     %w[width], %w[width], %w[vl]            \n"
      "b.eq     99f                                     \n"

      // Calculate a predicate for the final iteration to deal with the tail.
      "whilelt  p1.h, wzr, %w[width_last_y]             \n"
      "whilelt  p2.h, wzr, %w[width_last_uv]            \n"  //
      READYUY2_SVE I4XXTORGB_SVE RGBTOARGB8_SVE
      "st2h     {z16.h, z17.h}, p1, [%[dst_argb]]       \n"

      "99:                                              \n"
      : [src_yuy2] "+r"(src_yuy2),                          // %[src_yuy2]
        [dst_argb] "+r"(dst_argb),                          // %[dst_argb]
        [width] "+r"(width)                                 // %[width]
      : [vl] "r"(vl),                                       // %[vl]
        [kUVCoeff] "r"(&yuvconstants->kUVCoeff),            // %[kUVCoeff]
        [kRGBCoeffBias] "r"(&yuvconstants->kRGBCoeffBias),  // %[kRGBCoeffBias]
        [nv_u_start] "r"(nv_u_start),                       // %[nv_u_start]
        [nv_u_step] "r"(nv_u_step),                         // %[nv_u_step]
        [nv_v_start] "r"(nv_v_start),                       // %[nv_v_start]
        [nv_v_step] "r"(nv_v_step),                         // %[nv_v_step]
        [width_last_y] "r"(width_last_y),                   // %[width_last_y]
        [width_last_uv] "r"(width_last_uv)                  // %[width_last_uv]
      : "cc", "memory", YUVTORGB_SVE_REGS, "p2");
}

void UYVYToARGBRow_SVE2(const uint8_t* src_uyvy,
                        uint8_t* dst_argb,
                        const struct YuvConstants* yuvconstants,
                        int width) {
  uint32_t nv_u_start = 0x0000'0000U;
  uint32_t nv_u_step = 0x0004'0004U;
  uint32_t nv_v_start = 0x0002'0002U;
  uint32_t nv_v_step = 0x0004'0004U;
  uint64_t vl;
  asm volatile (
      "cnth %0" : "=r"(vl));
  int width_last_y = width & (vl - 1);
  int width_last_uv = width_last_y + (width_last_y & 1);
  asm volatile (
      "ptrue    p0.b                                    \n"
      "index    z22.s, %w[nv_u_start], %w[nv_u_step]    \n"
      "index    z23.s, %w[nv_v_start], %w[nv_v_step]    \n"
      "dup      z19.b, #255                             \n"  // A
      YUVTORGB_SVE_SETUP
      "subs     %w[width], %w[width], %w[vl]            \n"
      "b.lt     2f                                      \n"

      // Run bulk of computation with an all-true predicate to avoid predicate
      // generation overhead.
      "ptrue    p1.h                                    \n"
      "ptrue    p2.h                                    \n"
      "1:                                               \n"  //
      READUYVY_SVE I4XXTORGB_SVE RGBTOARGB8_SVE
      "subs     %w[width], %w[width], %w[vl]            \n"
      "st2h     {z16.h, z17.h}, p1, [%[dst_argb]]       \n"
      "add      %[dst_argb], %[dst_argb], %[vl], lsl #2 \n"
      "b.ge     1b                                      \n"

      "2:                                               \n"
      "adds     %w[width], %w[width], %w[vl]            \n"
      "b.eq     99f                                     \n"

      // Calculate a predicate for the final iteration to deal with the tail.
      "2:                                               \n"
      "whilelt  p1.h, wzr, %w[width_last_y]             \n"
      "whilelt  p2.h, wzr, %w[width_last_uv]            \n"  //
      READUYVY_SVE I4XXTORGB_SVE RGBTOARGB8_SVE
      "st2h     {z16.h, z17.h}, p1, [%[dst_argb]]       \n"

      "99:                                              \n"
      : [src_uyvy] "+r"(src_uyvy),                          // %[src_yuy2]
        [dst_argb] "+r"(dst_argb),                          // %[dst_argb]
        [width] "+r"(width)                                 // %[width]
      : [vl] "r"(vl),                                       // %[vl]
        [kUVCoeff] "r"(&yuvconstants->kUVCoeff),            // %[kUVCoeff]
        [kRGBCoeffBias] "r"(&yuvconstants->kRGBCoeffBias),  // %[kRGBCoeffBias]
        [nv_u_start] "r"(nv_u_start),                       // %[nv_u_start]
        [nv_u_step] "r"(nv_u_step),                         // %[nv_u_step]
        [nv_v_start] "r"(nv_v_start),                       // %[nv_v_start]
        [nv_v_step] "r"(nv_v_step),                         // %[nv_v_step]
        [width_last_y] "r"(width_last_y),                   // %[width_last_y]
        [width_last_uv] "r"(width_last_uv)                  // %[width_last_uv]
      : "cc", "memory", YUVTORGB_SVE_REGS, "p2");
}

#endif  // !defined(LIBYUV_DISABLE_SVE) && defined(__aarch64__)

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
