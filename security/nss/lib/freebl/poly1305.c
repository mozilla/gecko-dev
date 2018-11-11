/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This implementation of poly1305 is by Andrew Moon
 * (https://github.com/floodyberry/poly1305-donna) and released as public
 * domain. */

#include <string.h>

#include "poly1305.h"

#if defined(_MSC_VER) && _MSC_VER < 1600
#include "prtypes.h"
typedef PRUint32 uint32_t;
typedef PRUint64 uint64_t;
#else
#include <stdint.h>
#endif

#if defined(NSS_X86) || defined(NSS_X64)
/* We can assume little-endian. */
static uint32_t
U8TO32_LE(const unsigned char *m)
{
    uint32_t r;
    memcpy(&r, m, sizeof(r));
    return r;
}

static void
U32TO8_LE(unsigned char *m, uint32_t v)
{
    memcpy(m, &v, sizeof(v));
}
#else
static uint32_t
U8TO32_LE(const unsigned char *m)
{
    return (uint32_t)m[0] |
           (uint32_t)m[1] << 8 |
           (uint32_t)m[2] << 16 |
           (uint32_t)m[3] << 24;
}

static void
U32TO8_LE(unsigned char *m, uint32_t v)
{
    m[0] = v;
    m[1] = v >> 8;
    m[2] = v >> 16;
    m[3] = v >> 24;
}
#endif

static uint64_t
mul32x32_64(uint32_t a, uint32_t b)
{
    return (uint64_t)a * b;
}

struct poly1305_state_st {
    uint32_t r0, r1, r2, r3, r4;
    uint32_t s1, s2, s3, s4;
    uint32_t h0, h1, h2, h3, h4;
    unsigned char buf[16];
    unsigned int buf_used;
    unsigned char key[16];
};

/* update updates |state| given some amount of input data. This function may
 * only be called with a |len| that is not a multiple of 16 at the end of the
 * data. Otherwise the input must be buffered into 16 byte blocks. */
static void
update(struct poly1305_state_st *state, const unsigned char *in,
       size_t len)
{
    uint32_t t0, t1, t2, t3;
    uint64_t t[5];
    uint32_t b;
    uint64_t c;
    size_t j;
    unsigned char mp[16];

    if (len < 16)
        goto poly1305_donna_atmost15bytes;

poly1305_donna_16bytes:
    t0 = U8TO32_LE(in);
    t1 = U8TO32_LE(in + 4);
    t2 = U8TO32_LE(in + 8);
    t3 = U8TO32_LE(in + 12);

    in += 16;
    len -= 16;

    state->h0 += t0 & 0x3ffffff;
    state->h1 += ((((uint64_t)t1 << 32) | t0) >> 26) & 0x3ffffff;
    state->h2 += ((((uint64_t)t2 << 32) | t1) >> 20) & 0x3ffffff;
    state->h3 += ((((uint64_t)t3 << 32) | t2) >> 14) & 0x3ffffff;
    state->h4 += (t3 >> 8) | (1 << 24);

poly1305_donna_mul:
    t[0] = mul32x32_64(state->h0, state->r0) +
           mul32x32_64(state->h1, state->s4) +
           mul32x32_64(state->h2, state->s3) +
           mul32x32_64(state->h3, state->s2) +
           mul32x32_64(state->h4, state->s1);
    t[1] = mul32x32_64(state->h0, state->r1) +
           mul32x32_64(state->h1, state->r0) +
           mul32x32_64(state->h2, state->s4) +
           mul32x32_64(state->h3, state->s3) +
           mul32x32_64(state->h4, state->s2);
    t[2] = mul32x32_64(state->h0, state->r2) +
           mul32x32_64(state->h1, state->r1) +
           mul32x32_64(state->h2, state->r0) +
           mul32x32_64(state->h3, state->s4) +
           mul32x32_64(state->h4, state->s3);
    t[3] = mul32x32_64(state->h0, state->r3) +
           mul32x32_64(state->h1, state->r2) +
           mul32x32_64(state->h2, state->r1) +
           mul32x32_64(state->h3, state->r0) +
           mul32x32_64(state->h4, state->s4);
    t[4] = mul32x32_64(state->h0, state->r4) +
           mul32x32_64(state->h1, state->r3) +
           mul32x32_64(state->h2, state->r2) +
           mul32x32_64(state->h3, state->r1) +
           mul32x32_64(state->h4, state->r0);

    state->h0 = (uint32_t)t[0] & 0x3ffffff;
    c = (t[0] >> 26);
    t[1] += c;
    state->h1 = (uint32_t)t[1] & 0x3ffffff;
    b = (uint32_t)(t[1] >> 26);
    t[2] += b;
    state->h2 = (uint32_t)t[2] & 0x3ffffff;
    b = (uint32_t)(t[2] >> 26);
    t[3] += b;
    state->h3 = (uint32_t)t[3] & 0x3ffffff;
    b = (uint32_t)(t[3] >> 26);
    t[4] += b;
    state->h4 = (uint32_t)t[4] & 0x3ffffff;
    b = (uint32_t)(t[4] >> 26);
    state->h0 += b * 5;

    if (len >= 16)
        goto poly1305_donna_16bytes;

/* final bytes */
poly1305_donna_atmost15bytes:
    if (!len)
        return;

    for (j = 0; j < len; j++)
        mp[j] = in[j];
    mp[j++] = 1;
    for (; j < 16; j++)
        mp[j] = 0;
    len = 0;

    t0 = U8TO32_LE(mp + 0);
    t1 = U8TO32_LE(mp + 4);
    t2 = U8TO32_LE(mp + 8);
    t3 = U8TO32_LE(mp + 12);

    state->h0 += t0 & 0x3ffffff;
    state->h1 += ((((uint64_t)t1 << 32) | t0) >> 26) & 0x3ffffff;
    state->h2 += ((((uint64_t)t2 << 32) | t1) >> 20) & 0x3ffffff;
    state->h3 += ((((uint64_t)t3 << 32) | t2) >> 14) & 0x3ffffff;
    state->h4 += (t3 >> 8);

    goto poly1305_donna_mul;
}

void
Poly1305Init(poly1305_state *statep, const unsigned char key[32])
{
    struct poly1305_state_st *state = (struct poly1305_state_st *)statep;
    uint32_t t0, t1, t2, t3;

    t0 = U8TO32_LE(key + 0);
    t1 = U8TO32_LE(key + 4);
    t2 = U8TO32_LE(key + 8);
    t3 = U8TO32_LE(key + 12);

    /* precompute multipliers */
    state->r0 = t0 & 0x3ffffff;
    t0 >>= 26;
    t0 |= t1 << 6;
    state->r1 = t0 & 0x3ffff03;
    t1 >>= 20;
    t1 |= t2 << 12;
    state->r2 = t1 & 0x3ffc0ff;
    t2 >>= 14;
    t2 |= t3 << 18;
    state->r3 = t2 & 0x3f03fff;
    t3 >>= 8;
    state->r4 = t3 & 0x00fffff;

    state->s1 = state->r1 * 5;
    state->s2 = state->r2 * 5;
    state->s3 = state->r3 * 5;
    state->s4 = state->r4 * 5;

    /* init state */
    state->h0 = 0;
    state->h1 = 0;
    state->h2 = 0;
    state->h3 = 0;
    state->h4 = 0;

    state->buf_used = 0;
    memcpy(state->key, key + 16, sizeof(state->key));
}

void
Poly1305Update(poly1305_state *statep, const unsigned char *in,
               size_t in_len)
{
    unsigned int i;
    struct poly1305_state_st *state = (struct poly1305_state_st *)statep;

    if (state->buf_used) {
        unsigned int todo = 16 - state->buf_used;
        if (todo > in_len)
            todo = in_len;
        for (i = 0; i < todo; i++)
            state->buf[state->buf_used + i] = in[i];
        state->buf_used += todo;
        in_len -= todo;
        in += todo;

        if (state->buf_used == 16) {
            update(state, state->buf, 16);
            state->buf_used = 0;
        }
    }

    if (in_len >= 16) {
        size_t todo = in_len & ~0xf;
        update(state, in, todo);
        in += todo;
        in_len &= 0xf;
    }

    if (in_len) {
        for (i = 0; i < in_len; i++)
            state->buf[i] = in[i];
        state->buf_used = in_len;
    }
}

void
Poly1305Finish(poly1305_state *statep, unsigned char mac[16])
{
    struct poly1305_state_st *state = (struct poly1305_state_st *)statep;
    uint64_t f0, f1, f2, f3;
    uint32_t g0, g1, g2, g3, g4;
    uint32_t b, nb;

    if (state->buf_used)
        update(state, state->buf, state->buf_used);

    b = state->h0 >> 26;
    state->h0 = state->h0 & 0x3ffffff;
    state->h1 += b;
    b = state->h1 >> 26;
    state->h1 = state->h1 & 0x3ffffff;
    state->h2 += b;
    b = state->h2 >> 26;
    state->h2 = state->h2 & 0x3ffffff;
    state->h3 += b;
    b = state->h3 >> 26;
    state->h3 = state->h3 & 0x3ffffff;
    state->h4 += b;
    b = state->h4 >> 26;
    state->h4 = state->h4 & 0x3ffffff;
    state->h0 += b * 5;

    g0 = state->h0 + 5;
    b = g0 >> 26;
    g0 &= 0x3ffffff;
    g1 = state->h1 + b;
    b = g1 >> 26;
    g1 &= 0x3ffffff;
    g2 = state->h2 + b;
    b = g2 >> 26;
    g2 &= 0x3ffffff;
    g3 = state->h3 + b;
    b = g3 >> 26;
    g3 &= 0x3ffffff;
    g4 = state->h4 + b - (1 << 26);

    b = (g4 >> 31) - 1;
    nb = ~b;
    state->h0 = (state->h0 & nb) | (g0 & b);
    state->h1 = (state->h1 & nb) | (g1 & b);
    state->h2 = (state->h2 & nb) | (g2 & b);
    state->h3 = (state->h3 & nb) | (g3 & b);
    state->h4 = (state->h4 & nb) | (g4 & b);

    f0 = ((state->h0) | (state->h1 << 26)) + (uint64_t)U8TO32_LE(&state->key[0]);
    f1 = ((state->h1 >> 6) | (state->h2 << 20)) + (uint64_t)U8TO32_LE(&state->key[4]);
    f2 = ((state->h2 >> 12) | (state->h3 << 14)) + (uint64_t)U8TO32_LE(&state->key[8]);
    f3 = ((state->h3 >> 18) | (state->h4 << 8)) + (uint64_t)U8TO32_LE(&state->key[12]);

    U32TO8_LE(&mac[0], (uint32_t)f0);
    f1 += (f0 >> 32);
    U32TO8_LE(&mac[4], (uint32_t)f1);
    f2 += (f1 >> 32);
    U32TO8_LE(&mac[8], (uint32_t)f2);
    f3 += (f2 >> 32);
    U32TO8_LE(&mac[12], (uint32_t)f3);
}
