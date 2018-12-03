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

#include "config.h"

#include <assert.h>
#include <limits.h>

#include "common/intops.h"

#include "src/msac.h"

#define EC_MIN_PROB 4  // must be <= (1<<EC_PROB_SHIFT)/16

#define EC_WIN_SIZE (sizeof(ec_win) << 3)

static inline void ctx_refill(MsacContext *s) {
    const uint8_t *buf_pos = s->buf_pos;
    const uint8_t *buf_end = s->buf_end;
    int c = EC_WIN_SIZE - s->cnt - 24;
    ec_win dif = s->dif;
    while (c >= 0 && buf_pos < buf_end) {
        dif ^= ((ec_win)*buf_pos++) << c;
        c -= 8;
    }
    s->dif = dif;
    s->cnt = EC_WIN_SIZE - c - 24;
    s->buf_pos = buf_pos;
}

/* Takes updated dif and range values, renormalizes them so that
 * 32768 <= rng < 65536 (reading more bytes from the stream into dif if
 * necessary), and stores them back in the decoder context.
 * dif: The new value of dif.
 * rng: The new value of the range. */
static inline void ctx_norm(MsacContext *s, ec_win dif, uint32_t rng) {
    const uint16_t d = 15 - (31 ^ clz(rng));
    assert(rng <= 65535U);
    s->cnt -= d;
    s->dif = ((dif + 1) << d) - 1; /* Shift in 1s in the LSBs */
    s->rng = rng << d;
    if (s->cnt < 0)
        ctx_refill(s);
}

/* Decodes a symbol given an inverse cumulative distribution function (CDF)
 * table in Q15. */
unsigned msac_decode_symbol(MsacContext *const s, const uint16_t *const cdf,
                            const unsigned n_symbols)
{
    ec_win u, v = s->rng, r = s->rng >> 8;
    const ec_win c = s->dif >> (EC_WIN_SIZE - 16);
    unsigned ret = 0;

    assert(!cdf[n_symbols - 1]);

    do {
        u = v;
        v = r * (cdf[ret++] >> EC_PROB_SHIFT);
        v >>= 7 - EC_PROB_SHIFT;
        v += EC_MIN_PROB * (n_symbols - ret);
    } while (c < v);

    assert(u <= s->rng);

    ctx_norm(s, s->dif - (v << (EC_WIN_SIZE - 16)), u - v);
    return ret - 1;
}

/* Decode a single binary value.
 * f: The probability that the bit is one
 * Return: The value decoded (0 or 1). */
unsigned msac_decode_bool(MsacContext *const s, const unsigned f) {
    ec_win v, vw, dif = s->dif;
    uint16_t r = s->rng;
    unsigned ret;
    assert((dif >> (EC_WIN_SIZE - 16)) < r);
    v = ((r >> 8) * f >> (7 - EC_PROB_SHIFT)) + EC_MIN_PROB;
    vw   = v << (EC_WIN_SIZE - 16);
    ret  = dif >= vw;
    dif -= ret*vw;
    v   += ret*(r - 2*v);
    ctx_norm(s, dif, v);
    return !ret;
}

unsigned msac_decode_bools(MsacContext *const c, const unsigned l) {
    int v = 0;
    for (int n = (int) l - 1; n >= 0; n--)
        v = (v << 1) | msac_decode_bool(c, EC_BOOL_EPROB);
    return v;
}

int msac_decode_subexp(MsacContext *const c, const int ref,
                       const int n, const unsigned k)
{
    int i = 0;
    int a = 0;
    int b = k;
    while ((2 << b) < n) {
        if (!msac_decode_bool(c, EC_BOOL_EPROB)) break;
        b = k + i++;
        a = (1 << b);
    }
    const unsigned v = msac_decode_bools(c, b) + a;
    return ref * 2 <= n ? inv_recenter(ref, v) :
                          n - 1 - inv_recenter(n - 1 - ref, v);
}

int msac_decode_uniform(MsacContext *const c, const unsigned n) {
    assert(n > 0);
    const int l = ulog2(n) + 1;
    assert(l > 1);
    const unsigned m = (1 << l) - n;
    const unsigned v = msac_decode_bools(c, l - 1);
    return v < m ? v : (v << 1) - m + msac_decode_bool(c, EC_BOOL_EPROB);
}

static void update_cdf(uint16_t *const cdf, const unsigned val,
                       const unsigned n_symbols)
{
    const unsigned count = cdf[n_symbols];
    const int rate = ((count >> 4) | 4) + (n_symbols > 3);
    unsigned i;
    for (i = 0; i < val; i++)
        cdf[i] += (32768 - cdf[i]) >> rate;
    for (; i < n_symbols - 1; i++)
        cdf[i] -= cdf[i] >> rate;
    cdf[n_symbols] = count + (count < 32);
}

unsigned msac_decode_symbol_adapt(MsacContext *const c,
                                  uint16_t *const cdf, const unsigned n_symbols)
{
    const unsigned val = msac_decode_symbol(c, cdf, n_symbols);
    if(c->allow_update_cdf)
        update_cdf(cdf, val, n_symbols);
    return val;
}

unsigned msac_decode_bool_adapt(MsacContext *const c, uint16_t *const cdf) {
    const unsigned bit = msac_decode_bool(c, *cdf >> EC_PROB_SHIFT);

    if(c->allow_update_cdf){
        // update_cdf() specialized for boolean CDFs
        const unsigned count = cdf[1];
        const int rate = (count >> 4) | 4;
        if (bit) {
            cdf[0] += (32768 - cdf[0]) >> rate;
        } else {
            cdf[0] -= cdf[0] >> rate;
        }
        cdf[1] = count + (count < 32);
    }

    return bit;
}

void msac_init(MsacContext *const s, const uint8_t *const data,
               const size_t sz, const int disable_cdf_update_flag)
{
    s->buf_pos = data;
    s->buf_end = data + sz;
    s->dif = ((ec_win)1 << (EC_WIN_SIZE - 1)) - 1;
    s->rng = 0x8000;
    s->cnt = -15;
    s->allow_update_cdf = !disable_cdf_update_flag;
    ctx_refill(s);
}
