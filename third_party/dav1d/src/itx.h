/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __DAV1D_SRC_ITX_H__
#define __DAV1D_SRC_ITX_H__

#include <stddef.h>

#include "common/bitdepth.h"

#include "src/levels.h"

#define decl_itx_fn(name) \
void (name)(pixel *dst, ptrdiff_t dst_stride, coef *coeff, int eob)
typedef decl_itx_fn(*itxfm_fn);

typedef struct Dav1dInvTxfmDSPContext {
    itxfm_fn itxfm_add[N_RECT_TX_SIZES][N_TX_TYPES_PLUS_LL];
} Dav1dInvTxfmDSPContext;

void dav1d_itx_dsp_init_8bpc(Dav1dInvTxfmDSPContext *c);
void dav1d_itx_dsp_init_10bpc(Dav1dInvTxfmDSPContext *c);

void dav1d_itx_dsp_init_x86_8bpc(Dav1dInvTxfmDSPContext *c);
void dav1d_itx_dsp_init_x86_10bpc(Dav1dInvTxfmDSPContext *c);

#endif /* __DAV1D_SRC_ITX_H__ */
