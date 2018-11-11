/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 */

#ifdef _MSC_VER

#include <intrin.h>

#else



#endif

//
//
//

#include "util.h"

//
//
//

bool
is_pow2_u32(uint32_t n)
{
  return (n & (n-1)) == 0;
}

//
//
//

uint32_t
pow2_ru_u32(uint32_t n)
{
  n--;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  n++;

  return n;
}

//
//
//

uint32_t
pow2_rd_u32(uint32_t n)
{
  return 1u << msb_idx_u32(n);
}

//
// ASSUMES NON-ZERO
//

uint32_t
msb_idx_u32(uint32_t n)
{
#if defined( _MSC_VER )

  uint32_t index;

  _BitScanReverse((unsigned long *)&index,n);

  return index;

#elif defined( __GNUC__ )

  return __builtin_clz(n) ^ 31;

#else

#error "No msb_index()"

#endif
}

//
//
//
