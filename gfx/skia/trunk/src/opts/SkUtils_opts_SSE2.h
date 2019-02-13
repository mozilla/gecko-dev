/*
 * Copyright 2009 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkUtils_opts_SSE2_DEFINED
#define SkUtils_opts_SSE2_DEFINED

#include "SkTypes.h"

void sk_memset16_SSE2(uint16_t *dst, uint16_t value, int count);
void sk_memset32_SSE2(uint32_t *dst, uint32_t value, int count);
void sk_memcpy32_SSE2(uint32_t *dst, const uint32_t *src, int count);

#endif
