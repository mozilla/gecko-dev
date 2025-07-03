/* riscv_init.c - RISC-V Vector optimized filter functions
 *
 * Copyright (c) 2023 Google LLC
 * Written by Dragoș Tiselice <dtiselice@google.com>, May 2023.
 *            Filip Wasil     <f.wasil@samsung.com>, March 2025.
 * This code is released under the libpng license.
 * For conditions of distribution and use, see the disclaimer
 * and license in png.h
 */

#include "../pngpriv.h"

#ifdef PNG_READ_SUPPORTED

#if PNG_RISCV_RVV_OPT > 0

#include <riscv_vector.h>

#include <signal.h>

#ifndef PNG_RISCV_RVV_FILE
#  if defined(__linux__)
#    define PNG_RISCV_RVV_FILE "contrib/riscv-rvv/linux.c"
#  else
#    error "No support for run-time RISC-V Vector checking; use compile-time options"
#  endif
#endif

static int png_have_rvv();
#ifdef PNG_RISCV_RVV_FILE
#  include PNG_RISCV_RVV_FILE
#endif

#ifndef PNG_ALIGNED_MEMORY_SUPPORTED
#  error "ALIGNED_MEMORY is required; set: -DPNG_ALIGNED_MEMORY_SUPPORTED"
#endif

void
png_init_filter_functions_rvv(png_structp pp, unsigned int bpp)
{
   png_debug(1, "in png_init_filter_functions_rvv");

   static volatile sig_atomic_t no_rvv = -1; /* not checked */

   if (no_rvv < 0)
      no_rvv = !png_have_rvv();

   if (no_rvv)
      return;

   pp->read_filter[PNG_FILTER_VALUE_UP-1] = png_read_filter_row_up_rvv;

   if (bpp == 3)
   {
      pp->read_filter[PNG_FILTER_VALUE_AVG-1] = png_read_filter_row_avg3_rvv;
      pp->read_filter[PNG_FILTER_VALUE_PAETH-1] = png_read_filter_row_paeth3_rvv;
      pp->read_filter[PNG_FILTER_VALUE_SUB-1] = png_read_filter_row_sub3_rvv;
   }
   else if (bpp == 4)
   {
      pp->read_filter[PNG_FILTER_VALUE_AVG-1] = png_read_filter_row_avg4_rvv;
      pp->read_filter[PNG_FILTER_VALUE_PAETH-1] = png_read_filter_row_paeth4_rvv;
      pp->read_filter[PNG_FILTER_VALUE_SUB-1] = png_read_filter_row_sub4_rvv;
   }
}

#endif /* PNG_RISCV_RVV_OPT > 0 */
#endif /* PNG_READ_SUPPORTED */
