/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Intentionally NO #pragma once... included multiple times.

// This file is included from skcms.cc with some pre-defined macros:
//    N:    depth of all vectors, 1,4,8, or 16
// and inside a namespace, with some types already defined:
//    F:    a vector of N float
//    I32:  a vector of N int32_t
//    U64:  a vector of N uint64_t
//    U32:  a vector of N uint32_t
//    U16:  a vector of N uint16_t
//    U8:   a vector of N uint8_t

#if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
    // TODO(mtklein): this build supports FP16 compute
#endif

#if defined(__GNUC__) && !defined(__clang__)
    // Once again, GCC is kind of weird, not allowing vector = scalar directly.
    static constexpr F F0 = F() + 0.0f,
                       F1 = F() + 1.0f;
#else
    static constexpr F F0 = 0.0f,
                       F1 = 1.0f;
#endif

// Instead of checking __AVX__ below, we'll check USING_AVX.
// This lets skcms.cc set USING_AVX to force us in even if the compiler's not set that way.
// Same deal for __F16C__ and __AVX2__ ~~~> USING_AVX_F16C, USING_AVX2.

#if !defined(USING_AVX)      && N == 8 && defined(__AVX__)
    #define  USING_AVX
#endif
#if !defined(USING_AVX_F16C) && defined(USING_AVX) && defined(__F16C__)
    #define  USING AVX_F16C
#endif
#if !defined(USING_AVX2)     && defined(USING_AVX) && defined(__AVX2__)
    #define  USING_AVX2
#endif

// Similar to the AVX+ features, we define USING_NEON and USING_NEON_F16C.
// This is more for organizational clarity... skcms.cc doesn't force these.
#if N == 4 && defined(__ARM_NEON)
    #define USING_NEON
    #if __ARM_FP & 2
        #define USING_NEON_F16C
    #endif
#endif

// These -Wvector-conversion warnings seem to trigger in very bogus situations,
// like vst3q_f32() expecting a 16x char rather than a 4x float vector.  :/
#if defined(USING_NEON) && defined(__clang__)
    #pragma clang diagnostic ignored "-Wvector-conversion"
#endif

// GCC warns us about returning U64 on x86 because it's larger than a register.
// You'd see warnings like, "using AVX even though AVX is not enabled".
// We stifle these warnings... our helpers that return U64 are always inlined.
#if defined(__SSE__) && defined(__GNUC__) && !defined(__clang__)
    #pragma GCC diagnostic ignored "-Wpsabi"
#endif

// We tag most helper functions as SI, to enforce good code generation
// but also work around what we think is a bug in GCC: when targeting 32-bit
// x86, GCC tends to pass U16 (4x uint16_t vector) function arguments in the
// MMX mm0 register, which seems to mess with unrelated code that later uses
// x87 FP instructions (MMX's mm0 is an alias for x87's st0 register).
//
// It helps codegen to call __builtin_memcpy() when we know the byte count at compile time.
#if defined(__clang__) || defined(__GNUC__)
    #define SI static inline __attribute__((always_inline))
#else
    #define SI static inline
#endif

template <typename T, typename P>
SI T load(const P* ptr) {
    T val;
    small_memcpy(&val, ptr, sizeof(val));
    return val;
}
template <typename T, typename P>
SI void store(P* ptr, const T& val) {
    small_memcpy(ptr, &val, sizeof(val));
}

// (T)v is a cast when N == 1 and a bit-pun when N>1,
// so we use cast<T>(v) to actually cast or bit_pun<T>(v) to bit-pun.
template <typename D, typename S>
SI D cast(const S& v) {
#if N == 1
    return (D)v;
#elif defined(__clang__)
    return __builtin_convertvector(v, D);
#elif N == 4
    return D{v[0],v[1],v[2],v[3]};
#elif N == 8
    return D{v[0],v[1],v[2],v[3], v[4],v[5],v[6],v[7]};
#elif N == 16
    return D{v[0],v[1],v[ 2],v[ 3], v[ 4],v[ 5],v[ 6],v[ 7],
             v[8],v[9],v[10],v[11], v[12],v[13],v[14],v[15]};
#endif
}

template <typename D, typename S>
SI D bit_pun(const S& v) {
    static_assert(sizeof(D) == sizeof(v), "");
    return load<D>(&v);
}

// When we convert from float to fixed point, it's very common to want to round,
// and for some reason compilers generate better code when converting to int32_t.
// To serve both those ends, we use this function to_fixed() instead of direct cast().
SI I32 to_fixed(F f) {  return cast<I32>(f + 0.5f); }

template <typename T>
SI T if_then_else(I32 cond, T t, T e) {
#if N == 1
    return cond ? t : e;
#else
    return bit_pun<T>( ( cond & bit_pun<I32>(t)) |
                       (~cond & bit_pun<I32>(e)) );
#endif
}

SI F F_from_Half(U16 half) {
#if defined(USING_NEON_F16C)
    return vcvt_f32_f16((float16x4_t)half);
#elif defined(__AVX512F__)
    return (F)_mm512_cvtph_ps((__m256i)half);
#elif defined(USING_AVX_F16C)
    typedef int16_t __attribute__((vector_size(16))) I16;
    return __builtin_ia32_vcvtph2ps256((I16)half);
#else
    U32 wide = cast<U32>(half);
    // A half is 1-5-10 sign-exponent-mantissa, with 15 exponent bias.
    U32 s  = wide & 0x8000,
        em = wide ^ s;

    // Constructing the float is easy if the half is not denormalized.
    F norm = bit_pun<F>( (s<<16) + (em<<13) + ((127-15)<<23) );

    // Simply flush all denorm half floats to zero.
    return if_then_else(em < 0x0400, F0, norm);
#endif
}

#if defined(__clang__)
    // The -((127-15)<<10) underflows that side of the math when
    // we pass a denorm half float.  It's harmless... we'll take the 0 side anyway.
    __attribute__((no_sanitize("unsigned-integer-overflow")))
#endif
SI U16 Half_from_F(F f) {
#if defined(USING_NEON_F16C)
    return (U16)vcvt_f16_f32(f);
#elif defined(__AVX512F__)
    return (U16)_mm512_cvtps_ph((__m512 )f, _MM_FROUND_CUR_DIRECTION );
#elif defined(USING_AVX_F16C)
    return (U16)__builtin_ia32_vcvtps2ph256(f, 0x04/*_MM_FROUND_CUR_DIRECTION*/);
#else
    // A float is 1-8-23 sign-exponent-mantissa, with 127 exponent bias.
    U32 sem = bit_pun<U32>(f),
        s   = sem & 0x80000000,
         em = sem ^ s;

    // For simplicity we flush denorm half floats (including all denorm floats) to zero.
    return cast<U16>(if_then_else(em < 0x38800000, (U32)F0
                                                 , (s>>16) + (em>>13) - ((127-15)<<10)));
#endif
}

// Swap high and low bytes of 16-bit lanes, converting between big-endian and little-endian.
#if defined(USING_NEON)
    SI U16 swap_endian_16(U16 v) {
        return (U16)vrev16_u8((uint8x8_t) v);
    }
#endif

SI U64 swap_endian_16x4(const U64& rgba) {
    return (rgba & 0x00ff00ff00ff00ff) << 8
         | (rgba & 0xff00ff00ff00ff00) >> 8;
}

#if defined(USING_NEON)
    SI F min_(F x, F y) { return (F)vminq_f32((float32x4_t)x, (float32x4_t)y); }
    SI F max_(F x, F y) { return (F)vmaxq_f32((float32x4_t)x, (float32x4_t)y); }
#else
    SI F min_(F x, F y) { return if_then_else(x > y, y, x); }
    SI F max_(F x, F y) { return if_then_else(x < y, y, x); }
#endif

SI F floor_(F x) {
#if N == 1
    return floorf_(x);
#elif defined(__aarch64__)
    return vrndmq_f32(x);
#elif defined(__AVX512F__)
    return _mm512_floor_ps(x);
#elif defined(USING_AVX)
    return __builtin_ia32_roundps256(x, 0x01/*_MM_FROUND_FLOOR*/);
#elif defined(__SSE4_1__)
    return _mm_floor_ps(x);
#else
    // Round trip through integers with a truncating cast.
    F roundtrip = cast<F>(cast<I32>(x));
    // If x is negative, truncating gives the ceiling instead of the floor.
    return roundtrip - if_then_else(roundtrip > x, F1, F0);

    // This implementation fails for values of x that are outside
    // the range an integer can represent.  We expect most x to be small.
#endif
}

SI F approx_log2(F x) {
    // The first approximation of log2(x) is its exponent 'e', minus 127.
    I32 bits = bit_pun<I32>(x);

    F e = cast<F>(bits) * (1.0f / (1<<23));

    // If we use the mantissa too we can refine the error signficantly.
    F m = bit_pun<F>( (bits & 0x007fffff) | 0x3f000000 );

    return e - 124.225514990f
             -   1.498030302f*m
             -   1.725879990f/(0.3520887068f + m);
}

SI F approx_exp2(F x) {
    F fract = x - floor_(x);

    I32 bits = cast<I32>((1.0f * (1<<23)) * (x + 121.274057500f
                                               -   1.490129070f*fract
                                               +  27.728023300f/(4.84252568f - fract)));
    return bit_pun<F>(bits);
}

SI F approx_pow(F x, float y) {
    return if_then_else((x == F0) | (x == F1), x
                                             , approx_exp2(approx_log2(x) * y));
}

// Return tf(x).
SI F apply_tf(const skcms_TransferFunction* tf, F x) {
    // Peel off the sign bit and set x = |x|.
    U32 bits = bit_pun<U32>(x),
        sign = bits & 0x80000000;
    x = bit_pun<F>(bits ^ sign);

    // The transfer function has a linear part up to d, exponential at d and after.
    F v = if_then_else(x < tf->d,            tf->c*x + tf->f
                                , approx_pow(tf->a*x + tf->b, tf->g) + tf->e);

    // Tack the sign bit back on.
    return bit_pun<F>(sign | bit_pun<U32>(v));
}


// Strided loads and stores of N values, starting from p.
template <typename T, typename P>
SI T load_3(const P* p) {
#if N == 1
    return (T)p[0];
#elif N == 4
    return T{p[ 0],p[ 3],p[ 6],p[ 9]};
#elif N == 8
    return T{p[ 0],p[ 3],p[ 6],p[ 9], p[12],p[15],p[18],p[21]};
#elif N == 16
    return T{p[ 0],p[ 3],p[ 6],p[ 9], p[12],p[15],p[18],p[21],
             p[24],p[27],p[30],p[33], p[36],p[39],p[42],p[45]};
#endif
}

template <typename T, typename P>
SI T load_4(const P* p) {
#if N == 1
    return (T)p[0];
#elif N == 4
    return T{p[ 0],p[ 4],p[ 8],p[12]};
#elif N == 8
    return T{p[ 0],p[ 4],p[ 8],p[12], p[16],p[20],p[24],p[28]};
#elif N == 16
    return T{p[ 0],p[ 4],p[ 8],p[12], p[16],p[20],p[24],p[28],
             p[32],p[36],p[40],p[44], p[48],p[52],p[56],p[60]};
#endif
}

template <typename T, typename P>
SI void store_3(P* p, const T& v) {
#if N == 1
    p[0] = v;
#elif N == 4
    p[ 0] = v[ 0]; p[ 3] = v[ 1]; p[ 6] = v[ 2]; p[ 9] = v[ 3];
#elif N == 8
    p[ 0] = v[ 0]; p[ 3] = v[ 1]; p[ 6] = v[ 2]; p[ 9] = v[ 3];
    p[12] = v[ 4]; p[15] = v[ 5]; p[18] = v[ 6]; p[21] = v[ 7];
#elif N == 16
    p[ 0] = v[ 0]; p[ 3] = v[ 1]; p[ 6] = v[ 2]; p[ 9] = v[ 3];
    p[12] = v[ 4]; p[15] = v[ 5]; p[18] = v[ 6]; p[21] = v[ 7];
    p[24] = v[ 8]; p[27] = v[ 9]; p[30] = v[10]; p[33] = v[11];
    p[36] = v[12]; p[39] = v[13]; p[42] = v[14]; p[45] = v[15];
#endif
}

template <typename T, typename P>
SI void store_4(P* p, const T& v) {
#if N == 1
    p[0] = v;
#elif N == 4
    p[ 0] = v[ 0]; p[ 4] = v[ 1]; p[ 8] = v[ 2]; p[12] = v[ 3];
#elif N == 8
    p[ 0] = v[ 0]; p[ 4] = v[ 1]; p[ 8] = v[ 2]; p[12] = v[ 3];
    p[16] = v[ 4]; p[20] = v[ 5]; p[24] = v[ 6]; p[28] = v[ 7];
#elif N == 16
    p[ 0] = v[ 0]; p[ 4] = v[ 1]; p[ 8] = v[ 2]; p[12] = v[ 3];
    p[16] = v[ 4]; p[20] = v[ 5]; p[24] = v[ 6]; p[28] = v[ 7];
    p[32] = v[ 8]; p[36] = v[ 9]; p[40] = v[10]; p[44] = v[11];
    p[48] = v[12]; p[52] = v[13]; p[56] = v[14]; p[60] = v[15];
#endif
}


SI U8 gather_8(const uint8_t* p, I32 ix) {
#if N == 1
    U8 v = p[ix];
#elif N == 4
    U8 v = { p[ix[0]], p[ix[1]], p[ix[2]], p[ix[3]] };
#elif N == 8
    U8 v = { p[ix[0]], p[ix[1]], p[ix[2]], p[ix[3]],
             p[ix[4]], p[ix[5]], p[ix[6]], p[ix[7]] };
#elif N == 16
    U8 v = { p[ix[ 0]], p[ix[ 1]], p[ix[ 2]], p[ix[ 3]],
             p[ix[ 4]], p[ix[ 5]], p[ix[ 6]], p[ix[ 7]],
             p[ix[ 8]], p[ix[ 9]], p[ix[10]], p[ix[11]],
             p[ix[12]], p[ix[13]], p[ix[14]], p[ix[15]] };
#endif
    return v;
}

// Helper for gather_16(), loading the ix'th 16-bit value from p.
SI uint16_t load_16(const uint8_t* p, int ix) {
    return load<uint16_t>(p + 2*ix);
}

SI U16 gather_16(const uint8_t* p, I32 ix) {
#if N == 1
    U16 v = load_16(p,ix);
#elif N == 4
    U16 v = { load_16(p,ix[0]), load_16(p,ix[1]), load_16(p,ix[2]), load_16(p,ix[3]) };
#elif N == 8
    U16 v = { load_16(p,ix[0]), load_16(p,ix[1]), load_16(p,ix[2]), load_16(p,ix[3]),
              load_16(p,ix[4]), load_16(p,ix[5]), load_16(p,ix[6]), load_16(p,ix[7]) };
#elif N == 16
    U16 v = { load_16(p,ix[ 0]), load_16(p,ix[ 1]), load_16(p,ix[ 2]), load_16(p,ix[ 3]),
              load_16(p,ix[ 4]), load_16(p,ix[ 5]), load_16(p,ix[ 6]), load_16(p,ix[ 7]),
              load_16(p,ix[ 8]), load_16(p,ix[ 9]), load_16(p,ix[10]), load_16(p,ix[11]),
              load_16(p,ix[12]), load_16(p,ix[13]), load_16(p,ix[14]), load_16(p,ix[15]) };
#endif
    return v;
}

#if !defined(USING_AVX2)
    // Helpers for gather_24/48(), loading the ix'th 24/48-bit value from p, and 1/2 extra bytes.
    SI uint32_t load_24_32(const uint8_t* p, int ix) {
        return load<uint32_t>(p + 3*ix);
    }
    SI uint64_t load_48_64(const uint8_t* p, int ix) {
        return load<uint64_t>(p + 6*ix);
    }
#endif

SI U32 gather_24(const uint8_t* p, I32 ix) {
    // First, back up a byte.  Any place we're gathering from has a safe junk byte to read
    // in front of it, either a previous table value, or some tag metadata.
    p -= 1;

    // Now load multiples of 4 bytes (a junk byte, then r,g,b).
#if N == 1
    U32 v = load_24_32(p,ix);
#elif N == 4
    U32 v = { load_24_32(p,ix[0]), load_24_32(p,ix[1]), load_24_32(p,ix[2]), load_24_32(p,ix[3]) };
#elif N == 8 && !defined(USING_AVX2)
    U32 v = { load_24_32(p,ix[0]), load_24_32(p,ix[1]), load_24_32(p,ix[2]), load_24_32(p,ix[3]),
              load_24_32(p,ix[4]), load_24_32(p,ix[5]), load_24_32(p,ix[6]), load_24_32(p,ix[7]) };
#elif N == 8
    // The gather instruction here doesn't need any particular alignment,
    // but the intrinsic takes a const int*.
    const int* p4 = bit_pun<const int*>(p);
    I32 zero = { 0, 0, 0, 0,  0, 0, 0, 0},
        mask = {-1,-1,-1,-1, -1,-1,-1,-1};
    #if defined(__clang__)
        U32 v = (U32)__builtin_ia32_gatherd_d256(zero, p4, 3*ix, mask, 1);
    #elif defined(__GNUC__)
        U32 v = (U32)__builtin_ia32_gathersiv8si(zero, p4, 3*ix, mask, 1);
    #endif
#elif N == 16
    // The intrinsic is supposed to take const void* now, but it takes const int*, just like AVX2.
    // And AVX-512 swapped the order of arguments.  :/
    const int* p4 = bit_pun<const int*>(p);
    U32 v = (U32)_mm512_i32gather_epi32((__m512i)(3*ix), p4, 1);
#endif

    // Shift off the junk byte, leaving r,g,b in low 24 bits (and zero in the top 8).
    return v >> 8;
}

#if !defined(__arm__)
    SI void gather_48(const uint8_t* p, I32 ix, U64* v) {
        // As in gather_24(), with everything doubled.
        p -= 2;

    #if N == 1
        *v = load_48_64(p,ix);
    #elif N == 4
        *v = U64{
            load_48_64(p,ix[0]), load_48_64(p,ix[1]), load_48_64(p,ix[2]), load_48_64(p,ix[3]),
        };
    #elif N == 8 && !defined(USING_AVX2)
        *v = U64{
            load_48_64(p,ix[0]), load_48_64(p,ix[1]), load_48_64(p,ix[2]), load_48_64(p,ix[3]),
            load_48_64(p,ix[4]), load_48_64(p,ix[5]), load_48_64(p,ix[6]), load_48_64(p,ix[7]),
        };
    #elif N == 8
        typedef int32_t   __attribute__((vector_size(16))) Half_I32;
        typedef long long __attribute__((vector_size(32))) Half_I64;

        // The gather instruction here doesn't need any particular alignment,
        // but the intrinsic takes a const long long*.
        const long long int* p8 = bit_pun<const long long int*>(p);

        Half_I64 zero = { 0, 0, 0, 0},
                 mask = {-1,-1,-1,-1};

        ix *= 6;
        Half_I32 ix_lo = { ix[0], ix[1], ix[2], ix[3] },
                 ix_hi = { ix[4], ix[5], ix[6], ix[7] };

        #if defined(__clang__)
            Half_I64 lo = (Half_I64)__builtin_ia32_gatherd_q256(zero, p8, ix_lo, mask, 1),
                     hi = (Half_I64)__builtin_ia32_gatherd_q256(zero, p8, ix_hi, mask, 1);
        #elif defined(__GNUC__)
            Half_I64 lo = (Half_I64)__builtin_ia32_gathersiv4di(zero, p8, ix_lo, mask, 1),
                     hi = (Half_I64)__builtin_ia32_gathersiv4di(zero, p8, ix_hi, mask, 1);
        #endif
        store((char*)v +  0, lo);
        store((char*)v + 32, hi);
    #elif N == 16
        const long long int* p8 = bit_pun<const long long int*>(p);
        __m512i lo = _mm512_i32gather_epi64(_mm512_extracti32x8_epi32((__m512i)(6*ix), 0), p8, 1),
                hi = _mm512_i32gather_epi64(_mm512_extracti32x8_epi32((__m512i)(6*ix), 1), p8, 1);
        store((char*)v +  0, lo);
        store((char*)v + 64, hi);
    #endif

        *v >>= 16;
    }
#endif

SI F F_from_U8(U8 v) {
    return cast<F>(v) * (1/255.0f);
}

SI F F_from_U16_BE(U16 v) {
    // All 16-bit ICC values are big-endian, so we byte swap before converting to float.
    // MSVC catches the "loss" of data here in the portable path, so we also make sure to mask.
    v = (U16)( ((v<<8)|(v>>8)) & 0xffff );
    return cast<F>(v) * (1/65535.0f);
}

SI F minus_1_ulp(F v) {
    return bit_pun<F>( bit_pun<I32>(v) - 1 );
}

SI F table_8(const skcms_Curve* curve, F v) {
    // Clamp the input to [0,1], then scale to a table index.
    F ix = max_(F0, min_(v, F1)) * (float)(curve->table_entries - 1);

    // We'll look up (equal or adjacent) entries at lo and hi, then lerp by t between the two.
    I32 lo = cast<I32>(            ix      ),
        hi = cast<I32>(minus_1_ulp(ix+1.0f));
    F t = ix - cast<F>(lo);  // i.e. the fractional part of ix.

    // TODO: can we load l and h simultaneously?  Each entry in 'h' is either
    // the same as in 'l' or adjacent.  We have a rough idea that's it'd always be safe
    // to read adjacent entries and perhaps underflow the table by a byte or two
    // (it'd be junk, but always safe to read).  Not sure how to lerp yet.
    F l = F_from_U8(gather_8(curve->table_8, lo)),
      h = F_from_U8(gather_8(curve->table_8, hi));
    return l + (h-l)*t;
}

SI F table_16(const skcms_Curve* curve, F v) {
    // All just as in table_8() until the gathers.
    F ix = max_(F0, min_(v, F1)) * (float)(curve->table_entries - 1);

    I32 lo = cast<I32>(            ix      ),
        hi = cast<I32>(minus_1_ulp(ix+1.0f));
    F t = ix - cast<F>(lo);

    // TODO: as above, load l and h simultaneously?
    // Here we could even use AVX2-style 32-bit gathers.
    F l = F_from_U16_BE(gather_16(curve->table_16, lo)),
      h = F_from_U16_BE(gather_16(curve->table_16, hi));
    return l + (h-l)*t;
}

// Color lookup tables, by input dimension and bit depth.
SI void clut_0_8(const skcms_A2B* a2b, I32 ix, I32 stride, F* r, F* g, F* b, F a) {
    U32 rgb = gather_24(a2b->grid_8, ix);

    *r = cast<F>((rgb >>  0) & 0xff) * (1/255.0f);
    *g = cast<F>((rgb >>  8) & 0xff) * (1/255.0f);
    *b = cast<F>((rgb >> 16) & 0xff) * (1/255.0f);

    (void)a;
    (void)stride;
}
SI void clut_0_16(const skcms_A2B* a2b, I32 ix, I32 stride, F* r, F* g, F* b, F a) {
    #if defined(__arm__)
        // This is up to 2x faster on 32-bit ARM than the #else-case fast path.
        *r = F_from_U16_BE(gather_16(a2b->grid_16, 3*ix+0));
        *g = F_from_U16_BE(gather_16(a2b->grid_16, 3*ix+1));
        *b = F_from_U16_BE(gather_16(a2b->grid_16, 3*ix+2));
    #else
        // This strategy is much faster for 64-bit builds, and fine for 32-bit x86 too.
        U64 rgb;
        gather_48(a2b->grid_16, ix, &rgb);
        rgb = swap_endian_16x4(rgb);

        *r = cast<F>((rgb >>  0) & 0xffff) * (1/65535.0f);
        *g = cast<F>((rgb >> 16) & 0xffff) * (1/65535.0f);
        *b = cast<F>((rgb >> 32) & 0xffff) * (1/65535.0f);
    #endif
    (void)a;
    (void)stride;
}

// __attribute__((always_inline)) hits some pathological case in GCC that makes
// compilation way too slow for my patience.
#if defined(__clang__)
    #define MAYBE_SI SI
#else
    #define MAYBE_SI static inline
#endif

// These are all the same basic approach: handle one dimension, then the rest recursively.
// We let "I" be the current dimension, and "J" the previous dimension, I-1.  "B" is the bit depth.
#define DEF_CLUT(I,J,B)                                                                    \
    MAYBE_SI \
    void clut_##I##_##B(const skcms_A2B* a2b, I32 ix, I32 stride, F* r, F* g, F* b, F a) { \
        I32 limit = cast<I32>(F0);                                                         \
        limit += a2b->grid_points[I-1];                                                    \
                                                                                           \
        const F* srcs[] = { r,g,b,&a };                                                    \
        F src = *srcs[I-1];                                                                \
                                                                                           \
        F x = max_(F0, min_(src, F1)) * cast<F>(limit - 1);                                \
                                                                                           \
        I32 lo = cast<I32>(            x      ),                                           \
            hi = cast<I32>(minus_1_ulp(x+1.0f));                                           \
        F lr = *r, lg = *g, lb = *b,                                                       \
          hr = *r, hg = *g, hb = *b;                                                       \
        clut_##J##_##B(a2b, stride*lo + ix, stride*limit, &lr,&lg,&lb,a);                  \
        clut_##J##_##B(a2b, stride*hi + ix, stride*limit, &hr,&hg,&hb,a);                  \
                                                                                           \
        F t = x - cast<F>(lo);                                                             \
        *r = lr + (hr-lr)*t;                                                               \
        *g = lg + (hg-lg)*t;                                                               \
        *b = lb + (hb-lb)*t;                                                               \
    }

DEF_CLUT(1,0,8)
DEF_CLUT(2,1,8)
DEF_CLUT(3,2,8)
DEF_CLUT(4,3,8)

DEF_CLUT(1,0,16)
DEF_CLUT(2,1,16)
DEF_CLUT(3,2,16)
DEF_CLUT(4,3,16)


static void exec_ops(const Op* ops, const void** args,
                     const char* src, char* dst, int i) {
    F r = F0, g = F0, b = F0, a = F1;
    while (true) {
        switch (*ops++) {
            case Op_load_a8:{
                a = F_from_U8(load<U8>(src + 1*i));
            } break;

            case Op_load_g8:{
                r = g = b = F_from_U8(load<U8>(src + 1*i));
            } break;

            case Op_load_4444:{
                U16 abgr = load<U16>(src + 2*i);

                r = cast<F>((abgr >> 12) & 0xf) * (1/15.0f);
                g = cast<F>((abgr >>  8) & 0xf) * (1/15.0f);
                b = cast<F>((abgr >>  4) & 0xf) * (1/15.0f);
                a = cast<F>((abgr >>  0) & 0xf) * (1/15.0f);
            } break;

            case Op_load_565:{
                U16 rgb = load<U16>(src + 2*i);

                r = cast<F>(rgb & (uint16_t)(31<< 0)) * (1.0f / (31<< 0));
                g = cast<F>(rgb & (uint16_t)(63<< 5)) * (1.0f / (63<< 5));
                b = cast<F>(rgb & (uint16_t)(31<<11)) * (1.0f / (31<<11));
            } break;

            case Op_load_888:{
                const uint8_t* rgb = (const uint8_t*)(src + 3*i);
            #if defined(USING_NEON)
                // There's no uint8x4x3_t or vld3 load for it, so we'll load each rgb pixel one at
                // a time.  Since we're doing that, we might as well load them into 16-bit lanes.
                // (We'd even load into 32-bit lanes, but that's not possible on ARMv7.)
                uint8x8x3_t v = {{ vdup_n_u8(0), vdup_n_u8(0), vdup_n_u8(0) }};
                v = vld3_lane_u8(rgb+0, v, 0);
                v = vld3_lane_u8(rgb+3, v, 2);
                v = vld3_lane_u8(rgb+6, v, 4);
                v = vld3_lane_u8(rgb+9, v, 6);

                // Now if we squint, those 3 uint8x8_t we constructed are really U16s, easy to
                // convert to F.  (Again, U32 would be even better here if drop ARMv7 or split
                // ARMv7 and ARMv8 impls.)
                r = cast<F>((U16)v.val[0]) * (1/255.0f);
                g = cast<F>((U16)v.val[1]) * (1/255.0f);
                b = cast<F>((U16)v.val[2]) * (1/255.0f);
            #else
                r = cast<F>(load_3<U32>(rgb+0) ) * (1/255.0f);
                g = cast<F>(load_3<U32>(rgb+1) ) * (1/255.0f);
                b = cast<F>(load_3<U32>(rgb+2) ) * (1/255.0f);
            #endif
            } break;

            case Op_load_8888:{
                U32 rgba = load<U32>(src + 4*i);

                r = cast<F>((rgba >>  0) & 0xff) * (1/255.0f);
                g = cast<F>((rgba >>  8) & 0xff) * (1/255.0f);
                b = cast<F>((rgba >> 16) & 0xff) * (1/255.0f);
                a = cast<F>((rgba >> 24) & 0xff) * (1/255.0f);
            } break;

            case Op_load_1010102:{
                U32 rgba = load<U32>(src + 4*i);

                r = cast<F>((rgba >>  0) & 0x3ff) * (1/1023.0f);
                g = cast<F>((rgba >> 10) & 0x3ff) * (1/1023.0f);
                b = cast<F>((rgba >> 20) & 0x3ff) * (1/1023.0f);
                a = cast<F>((rgba >> 30) & 0x3  ) * (1/   3.0f);
            } break;

            case Op_load_161616LE:{
                uintptr_t ptr = (uintptr_t)(src + 6*i);
                assert( (ptr & 1) == 0 );                   // src must be 2-byte aligned for this
                const uint16_t* rgb = (const uint16_t*)ptr; // cast to const uint16_t* to be safe.
            #if defined(USING_NEON)
                uint16x4x3_t v = vld3_u16(rgb);
                r = cast<F>((U16)v.val[0]) * (1/65535.0f);
                g = cast<F>((U16)v.val[1]) * (1/65535.0f);
                b = cast<F>((U16)v.val[2]) * (1/65535.0f);
            #else
                r = cast<F>(load_3<U32>(rgb+0)) * (1/65535.0f);
                g = cast<F>(load_3<U32>(rgb+1)) * (1/65535.0f);
                b = cast<F>(load_3<U32>(rgb+2)) * (1/65535.0f);
            #endif
            } break;

            case Op_load_16161616LE:{
                uintptr_t ptr = (uintptr_t)(src + 8*i);
                assert( (ptr & 1) == 0 );                    // src must be 2-byte aligned for this
                const uint16_t* rgba = (const uint16_t*)ptr; // cast to const uint16_t* to be safe.
            #if defined(USING_NEON)
                uint16x4x4_t v = vld4_u16(rgba);
                r = cast<F>((U16)v.val[0]) * (1/65535.0f);
                g = cast<F>((U16)v.val[1]) * (1/65535.0f);
                b = cast<F>((U16)v.val[2]) * (1/65535.0f);
                a = cast<F>((U16)v.val[3]) * (1/65535.0f);
            #else
                U64 px = load<U64>(rgba);

                r = cast<F>((px >>  0) & 0xffff) * (1/65535.0f);
                g = cast<F>((px >> 16) & 0xffff) * (1/65535.0f);
                b = cast<F>((px >> 32) & 0xffff) * (1/65535.0f);
                a = cast<F>((px >> 48) & 0xffff) * (1/65535.0f);
            #endif
            } break;

            case Op_load_161616BE:{
                uintptr_t ptr = (uintptr_t)(src + 6*i);
                assert( (ptr & 1) == 0 );                   // src must be 2-byte aligned for this
                const uint16_t* rgb = (const uint16_t*)ptr; // cast to const uint16_t* to be safe.
            #if defined(USING_NEON)
                uint16x4x3_t v = vld3_u16(rgb);
                r = cast<F>(swap_endian_16((U16)v.val[0])) * (1/65535.0f);
                g = cast<F>(swap_endian_16((U16)v.val[1])) * (1/65535.0f);
                b = cast<F>(swap_endian_16((U16)v.val[2])) * (1/65535.0f);
            #else
                U32 R = load_3<U32>(rgb+0),
                    G = load_3<U32>(rgb+1),
                    B = load_3<U32>(rgb+2);
                // R,G,B are big-endian 16-bit, so byte swap them before converting to float.
                r = cast<F>((R & 0x00ff)<<8 | (R & 0xff00)>>8) * (1/65535.0f);
                g = cast<F>((G & 0x00ff)<<8 | (G & 0xff00)>>8) * (1/65535.0f);
                b = cast<F>((B & 0x00ff)<<8 | (B & 0xff00)>>8) * (1/65535.0f);
            #endif
            } break;

            case Op_load_16161616BE:{
                uintptr_t ptr = (uintptr_t)(src + 8*i);
                assert( (ptr & 1) == 0 );                    // src must be 2-byte aligned for this
                const uint16_t* rgba = (const uint16_t*)ptr; // cast to const uint16_t* to be safe.
            #if defined(USING_NEON)
                uint16x4x4_t v = vld4_u16(rgba);
                r = cast<F>(swap_endian_16((U16)v.val[0])) * (1/65535.0f);
                g = cast<F>(swap_endian_16((U16)v.val[1])) * (1/65535.0f);
                b = cast<F>(swap_endian_16((U16)v.val[2])) * (1/65535.0f);
                a = cast<F>(swap_endian_16((U16)v.val[3])) * (1/65535.0f);
            #else
                U64 px = swap_endian_16x4(load<U64>(rgba));

                r = cast<F>((px >>  0) & 0xffff) * (1/65535.0f);
                g = cast<F>((px >> 16) & 0xffff) * (1/65535.0f);
                b = cast<F>((px >> 32) & 0xffff) * (1/65535.0f);
                a = cast<F>((px >> 48) & 0xffff) * (1/65535.0f);
            #endif
            } break;

            case Op_load_hhh:{
                uintptr_t ptr = (uintptr_t)(src + 6*i);
                assert( (ptr & 1) == 0 );                   // src must be 2-byte aligned for this
                const uint16_t* rgb = (const uint16_t*)ptr; // cast to const uint16_t* to be safe.
            #if defined(USING_NEON)
                uint16x4x3_t v = vld3_u16(rgb);
                U16 R = (U16)v.val[0],
                    G = (U16)v.val[1],
                    B = (U16)v.val[2];
            #else
                U16 R = load_3<U16>(rgb+0),
                    G = load_3<U16>(rgb+1),
                    B = load_3<U16>(rgb+2);
            #endif
                r = F_from_Half(R);
                g = F_from_Half(G);
                b = F_from_Half(B);
            } break;

            case Op_load_hhhh:{
                uintptr_t ptr = (uintptr_t)(src + 8*i);
                assert( (ptr & 1) == 0 );                    // src must be 2-byte aligned for this
                const uint16_t* rgba = (const uint16_t*)ptr; // cast to const uint16_t* to be safe.
            #if defined(USING_NEON)
                uint16x4x4_t v = vld4_u16(rgba);
                U16 R = (U16)v.val[0],
                    G = (U16)v.val[1],
                    B = (U16)v.val[2],
                    A = (U16)v.val[3];
            #else
                U64 px = load<U64>(rgba);
                U16 R = cast<U16>((px >>  0) & 0xffff),
                    G = cast<U16>((px >> 16) & 0xffff),
                    B = cast<U16>((px >> 32) & 0xffff),
                    A = cast<U16>((px >> 48) & 0xffff);
            #endif
                r = F_from_Half(R);
                g = F_from_Half(G);
                b = F_from_Half(B);
                a = F_from_Half(A);
            } break;

            case Op_load_fff:{
                uintptr_t ptr = (uintptr_t)(src + 12*i);
                assert( (ptr & 3) == 0 );                   // src must be 4-byte aligned for this
                const float* rgb = (const float*)ptr;       // cast to const float* to be safe.
            #if defined(USING_NEON)
                float32x4x3_t v = vld3q_f32(rgb);
                r = (F)v.val[0];
                g = (F)v.val[1];
                b = (F)v.val[2];
            #else
                r = load_3<F>(rgb+0);
                g = load_3<F>(rgb+1);
                b = load_3<F>(rgb+2);
            #endif
            } break;

            case Op_load_ffff:{
                uintptr_t ptr = (uintptr_t)(src + 16*i);
                assert( (ptr & 3) == 0 );                   // src must be 4-byte aligned for this
                const float* rgba = (const float*)ptr;      // cast to const float* to be safe.
            #if defined(USING_NEON)
                float32x4x4_t v = vld4q_f32(rgba);
                r = (F)v.val[0];
                g = (F)v.val[1];
                b = (F)v.val[2];
                a = (F)v.val[3];
            #else
                r = load_4<F>(rgba+0);
                g = load_4<F>(rgba+1);
                b = load_4<F>(rgba+2);
                a = load_4<F>(rgba+3);
            #endif
            } break;

            case Op_swap_rb:{
                F t = r;
                r = b;
                b = t;
            } break;

            case Op_clamp:{
                r = max_(F0, min_(r, F1));
                g = max_(F0, min_(g, F1));
                b = max_(F0, min_(b, F1));
                a = max_(F0, min_(a, F1));
            } break;

            case Op_invert:{
                r = F1 - r;
                g = F1 - g;
                b = F1 - b;
                a = F1 - a;
            } break;

            case Op_force_opaque:{
                a = F1;
            } break;

            case Op_premul:{
                r *= a;
                g *= a;
                b *= a;
            } break;

            case Op_unpremul:{
                F scale = if_then_else(F1 / a < INFINITY_, F1 / a, F0);
                r *= scale;
                g *= scale;
                b *= scale;
            } break;

            case Op_matrix_3x3:{
                const skcms_Matrix3x3* matrix = (const skcms_Matrix3x3*) *args++;
                const float* m = &matrix->vals[0][0];

                F R = m[0]*r + m[1]*g + m[2]*b,
                  G = m[3]*r + m[4]*g + m[5]*b,
                  B = m[6]*r + m[7]*g + m[8]*b;

                r = R;
                g = G;
                b = B;
            } break;

            case Op_matrix_3x4:{
                const skcms_Matrix3x4* matrix = (const skcms_Matrix3x4*) *args++;
                const float* m = &matrix->vals[0][0];

                F R = m[0]*r + m[1]*g + m[ 2]*b + m[ 3],
                  G = m[4]*r + m[5]*g + m[ 6]*b + m[ 7],
                  B = m[8]*r + m[9]*g + m[10]*b + m[11];

                r = R;
                g = G;
                b = B;
            } break;

            case Op_lab_to_xyz:{
                // The L*a*b values are in r,g,b, but normalized to [0,1].  Reconstruct them:
                F L = r * 100.0f,
                  A = g * 255.0f - 128.0f,
                  B = b * 255.0f - 128.0f;

                // Convert to CIE XYZ.
                F Y = (L + 16.0f) * (1/116.0f),
                  X = Y + A*(1/500.0f),
                  Z = Y - B*(1/200.0f);

                X = if_then_else(X*X*X > 0.008856f, X*X*X, (X - (16/116.0f)) * (1/7.787f));
                Y = if_then_else(Y*Y*Y > 0.008856f, Y*Y*Y, (Y - (16/116.0f)) * (1/7.787f));
                Z = if_then_else(Z*Z*Z > 0.008856f, Z*Z*Z, (Z - (16/116.0f)) * (1/7.787f));

                // Adjust to XYZD50 illuminant, and stuff back into r,g,b for the next op.
                r = X * 0.9642f;
                g = Y          ;
                b = Z * 0.8249f;
            } break;

            case Op_tf_r:{ r = apply_tf((const skcms_TransferFunction*)*args++, r); } break;
            case Op_tf_g:{ g = apply_tf((const skcms_TransferFunction*)*args++, g); } break;
            case Op_tf_b:{ b = apply_tf((const skcms_TransferFunction*)*args++, b); } break;
            case Op_tf_a:{ a = apply_tf((const skcms_TransferFunction*)*args++, a); } break;

            case Op_table_8_r: { r = table_8((const skcms_Curve*)*args++, r); } break;
            case Op_table_8_g: { g = table_8((const skcms_Curve*)*args++, g); } break;
            case Op_table_8_b: { b = table_8((const skcms_Curve*)*args++, b); } break;
            case Op_table_8_a: { a = table_8((const skcms_Curve*)*args++, a); } break;

            case Op_table_16_r:{ r = table_16((const skcms_Curve*)*args++, r); } break;
            case Op_table_16_g:{ g = table_16((const skcms_Curve*)*args++, g); } break;
            case Op_table_16_b:{ b = table_16((const skcms_Curve*)*args++, b); } break;
            case Op_table_16_a:{ a = table_16((const skcms_Curve*)*args++, a); } break;

            case Op_clut_1D_8:{
                const skcms_A2B* a2b = (const skcms_A2B*) *args++;
                clut_1_8(a2b, cast<I32>(F0),cast<I32>(F1), &r,&g,&b,a);
            } break;

            case Op_clut_1D_16:{
                const skcms_A2B* a2b = (const skcms_A2B*) *args++;
                clut_1_16(a2b, cast<I32>(F0),cast<I32>(F1), &r,&g,&b,a);
            } break;

            case Op_clut_2D_8:{
                const skcms_A2B* a2b = (const skcms_A2B*) *args++;
                clut_2_8(a2b, cast<I32>(F0),cast<I32>(F1), &r,&g,&b,a);
            } break;

            case Op_clut_2D_16:{
                const skcms_A2B* a2b = (const skcms_A2B*) *args++;
                clut_2_16(a2b, cast<I32>(F0),cast<I32>(F1), &r,&g,&b,a);
            } break;

            case Op_clut_3D_8:{
                const skcms_A2B* a2b = (const skcms_A2B*) *args++;
                clut_3_8(a2b, cast<I32>(F0),cast<I32>(F1), &r,&g,&b,a);
            } break;

            case Op_clut_3D_16:{
                const skcms_A2B* a2b = (const skcms_A2B*) *args++;
                clut_3_16(a2b, cast<I32>(F0),cast<I32>(F1), &r,&g,&b,a);
            } break;

            case Op_clut_4D_8:{
                const skcms_A2B* a2b = (const skcms_A2B*) *args++;
                clut_4_8(a2b, cast<I32>(F0),cast<I32>(F1), &r,&g,&b,a);
                // 'a' was really a CMYK K, so our output is actually opaque.
                a = F1;
            } break;

            case Op_clut_4D_16:{
                const skcms_A2B* a2b = (const skcms_A2B*) *args++;
                clut_4_16(a2b, cast<I32>(F0),cast<I32>(F1), &r,&g,&b,a);
                // 'a' was really a CMYK K, so our output is actually opaque.
                a = F1;
            } break;

    // Notice, from here on down the store_ ops all return, ending the loop.

            case Op_store_a8: {
                store(dst + 1*i, cast<U8>(to_fixed(a * 255)));
            } return;

            case Op_store_g8: {
                // g should be holding luminance (Y) (r,g,b ~~~> X,Y,Z)
                store(dst + 1*i, cast<U8>(to_fixed(g * 255)));
            } return;

            case Op_store_4444: {
                store<U16>(dst + 2*i, cast<U16>(to_fixed(r * 15) << 12)
                                    | cast<U16>(to_fixed(g * 15) <<  8)
                                    | cast<U16>(to_fixed(b * 15) <<  4)
                                    | cast<U16>(to_fixed(a * 15) <<  0));
            } return;

            case Op_store_565: {
                store<U16>(dst + 2*i, cast<U16>(to_fixed(r * 31) <<  0 )
                                    | cast<U16>(to_fixed(g * 63) <<  5 )
                                    | cast<U16>(to_fixed(b * 31) << 11 ));
            } return;

            case Op_store_888: {
                uint8_t* rgb = (uint8_t*)dst + 3*i;
            #if defined(USING_NEON)
                // Same deal as load_888 but in reverse... we'll store using uint8x8x3_t, but
                // get there via U16 to save some instructions converting to float.  And just
                // like load_888, we'd prefer to go via U32 but for ARMv7 support.
                U16 R = cast<U16>(to_fixed(r * 255)),
                    G = cast<U16>(to_fixed(g * 255)),
                    B = cast<U16>(to_fixed(b * 255));

                uint8x8x3_t v = {{ (uint8x8_t)R, (uint8x8_t)G, (uint8x8_t)B }};
                vst3_lane_u8(rgb+0, v, 0);
                vst3_lane_u8(rgb+3, v, 2);
                vst3_lane_u8(rgb+6, v, 4);
                vst3_lane_u8(rgb+9, v, 6);
            #else
                store_3(rgb+0, cast<U8>(to_fixed(r * 255)) );
                store_3(rgb+1, cast<U8>(to_fixed(g * 255)) );
                store_3(rgb+2, cast<U8>(to_fixed(b * 255)) );
            #endif
            } return;

            case Op_store_8888: {
                store(dst + 4*i, cast<U32>(to_fixed(r * 255) <<  0)
                               | cast<U32>(to_fixed(g * 255) <<  8)
                               | cast<U32>(to_fixed(b * 255) << 16)
                               | cast<U32>(to_fixed(a * 255) << 24));
            } return;

            case Op_store_1010102: {
                store(dst + 4*i, cast<U32>(to_fixed(r * 1023) <<  0)
                               | cast<U32>(to_fixed(g * 1023) << 10)
                               | cast<U32>(to_fixed(b * 1023) << 20)
                               | cast<U32>(to_fixed(a *    3) << 30));
            } return;

            case Op_store_161616LE: {
                uintptr_t ptr = (uintptr_t)(dst + 6*i);
                assert( (ptr & 1) == 0 );                // The dst pointer must be 2-byte aligned
                uint16_t* rgb = (uint16_t*)ptr;          // for this cast to uint16_t* to be safe.
            #if defined(USING_NEON)
                uint16x4x3_t v = {{
                    (uint16x4_t)cast<U16>(to_fixed(r * 65535)),
                    (uint16x4_t)cast<U16>(to_fixed(g * 65535)),
                    (uint16x4_t)cast<U16>(to_fixed(b * 65535)),
                }};
                vst3_u16(rgb, v);
            #else
                store_3(rgb+0, cast<U16>(to_fixed(r * 65535)));
                store_3(rgb+1, cast<U16>(to_fixed(g * 65535)));
                store_3(rgb+2, cast<U16>(to_fixed(b * 65535)));
            #endif

            } return;

            case Op_store_16161616LE: {
                uintptr_t ptr = (uintptr_t)(dst + 8*i);
                assert( (ptr & 1) == 0 );               // The dst pointer must be 2-byte aligned
                uint16_t* rgba = (uint16_t*)ptr;        // for this cast to uint16_t* to be safe.
            #if defined(USING_NEON)
                uint16x4x4_t v = {{
                    (uint16x4_t)cast<U16>(to_fixed(r * 65535)),
                    (uint16x4_t)cast<U16>(to_fixed(g * 65535)),
                    (uint16x4_t)cast<U16>(to_fixed(b * 65535)),
                    (uint16x4_t)cast<U16>(to_fixed(a * 65535)),
                }};
                vst4_u16(rgba, v);
            #else
                U64 px = cast<U64>(to_fixed(r * 65535)) <<  0
                       | cast<U64>(to_fixed(g * 65535)) << 16
                       | cast<U64>(to_fixed(b * 65535)) << 32
                       | cast<U64>(to_fixed(a * 65535)) << 48;
                store(rgba, px);
            #endif
            } return;

            case Op_store_161616BE: {
                uintptr_t ptr = (uintptr_t)(dst + 6*i);
                assert( (ptr & 1) == 0 );                // The dst pointer must be 2-byte aligned
                uint16_t* rgb = (uint16_t*)ptr;          // for this cast to uint16_t* to be safe.
            #if defined(USING_NEON)
                uint16x4x3_t v = {{
                    (uint16x4_t)swap_endian_16(cast<U16>(to_fixed(r * 65535))),
                    (uint16x4_t)swap_endian_16(cast<U16>(to_fixed(g * 65535))),
                    (uint16x4_t)swap_endian_16(cast<U16>(to_fixed(b * 65535))),
                }};
                vst3_u16(rgb, v);
            #else
                I32 R = to_fixed(r * 65535),
                    G = to_fixed(g * 65535),
                    B = to_fixed(b * 65535);
                store_3(rgb+0, cast<U16>((R & 0x00ff) << 8 | (R & 0xff00) >> 8) );
                store_3(rgb+1, cast<U16>((G & 0x00ff) << 8 | (G & 0xff00) >> 8) );
                store_3(rgb+2, cast<U16>((B & 0x00ff) << 8 | (B & 0xff00) >> 8) );
            #endif

            } return;

            case Op_store_16161616BE: {
                uintptr_t ptr = (uintptr_t)(dst + 8*i);
                assert( (ptr & 1) == 0 );               // The dst pointer must be 2-byte aligned
                uint16_t* rgba = (uint16_t*)ptr;        // for this cast to uint16_t* to be safe.
            #if defined(USING_NEON)
                uint16x4x4_t v = {{
                    (uint16x4_t)swap_endian_16(cast<U16>(to_fixed(r * 65535))),
                    (uint16x4_t)swap_endian_16(cast<U16>(to_fixed(g * 65535))),
                    (uint16x4_t)swap_endian_16(cast<U16>(to_fixed(b * 65535))),
                    (uint16x4_t)swap_endian_16(cast<U16>(to_fixed(a * 65535))),
                }};
                vst4_u16(rgba, v);
            #else
                U64 px = cast<U64>(to_fixed(r * 65535)) <<  0
                       | cast<U64>(to_fixed(g * 65535)) << 16
                       | cast<U64>(to_fixed(b * 65535)) << 32
                       | cast<U64>(to_fixed(a * 65535)) << 48;
                store(rgba, swap_endian_16x4(px));
            #endif
            } return;

            case Op_store_hhh: {
                uintptr_t ptr = (uintptr_t)(dst + 6*i);
                assert( (ptr & 1) == 0 );                // The dst pointer must be 2-byte aligned
                uint16_t* rgb = (uint16_t*)ptr;          // for this cast to uint16_t* to be safe.

                U16 R = Half_from_F(r),
                    G = Half_from_F(g),
                    B = Half_from_F(b);
            #if defined(USING_NEON)
                uint16x4x3_t v = {{
                    (uint16x4_t)R,
                    (uint16x4_t)G,
                    (uint16x4_t)B,
                }};
                vst3_u16(rgb, v);
            #else
                store_3(rgb+0, R);
                store_3(rgb+1, G);
                store_3(rgb+2, B);
            #endif
            } return;

            case Op_store_hhhh: {
                uintptr_t ptr = (uintptr_t)(dst + 8*i);
                assert( (ptr & 1) == 0 );                // The dst pointer must be 2-byte aligned
                uint16_t* rgba = (uint16_t*)ptr;         // for this cast to uint16_t* to be safe.

                U16 R = Half_from_F(r),
                    G = Half_from_F(g),
                    B = Half_from_F(b),
                    A = Half_from_F(a);
            #if defined(USING_NEON)
                uint16x4x4_t v = {{
                    (uint16x4_t)R,
                    (uint16x4_t)G,
                    (uint16x4_t)B,
                    (uint16x4_t)A,
                }};
                vst4_u16(rgba, v);
            #else
                store(rgba, cast<U64>(R) <<  0
                          | cast<U64>(G) << 16
                          | cast<U64>(B) << 32
                          | cast<U64>(A) << 48);
            #endif

            } return;

            case Op_store_fff: {
                uintptr_t ptr = (uintptr_t)(dst + 12*i);
                assert( (ptr & 3) == 0 );                // The dst pointer must be 4-byte aligned
                float* rgb = (float*)ptr;                // for this cast to float* to be safe.
            #if defined(USING_NEON)
                float32x4x3_t v = {{
                    (float32x4_t)r,
                    (float32x4_t)g,
                    (float32x4_t)b,
                }};
                vst3q_f32(rgb, v);
            #else
                store_3(rgb+0, r);
                store_3(rgb+1, g);
                store_3(rgb+2, b);
            #endif
            } return;

            case Op_store_ffff: {
                uintptr_t ptr = (uintptr_t)(dst + 16*i);
                assert( (ptr & 3) == 0 );                // The dst pointer must be 4-byte aligned
                float* rgba = (float*)ptr;               // for this cast to float* to be safe.
            #if defined(USING_NEON)
                float32x4x4_t v = {{
                    (float32x4_t)r,
                    (float32x4_t)g,
                    (float32x4_t)b,
                    (float32x4_t)a,
                }};
                vst4q_f32(rgba, v);
            #else
                store_4(rgba+0, r);
                store_4(rgba+1, g);
                store_4(rgba+2, b);
                store_4(rgba+3, a);
            #endif
            } return;
        }
    }
}


static void run_program(const Op* program, const void** arguments,
                        const char* src, char* dst, int n,
                        const size_t src_bpp, const size_t dst_bpp) {
    int i = 0;
    while (n >= N) {
        exec_ops(program, arguments, src, dst, i);
        i += N;
        n -= N;
    }
    if (n > 0) {
        char tmp_src[4*4*N] = {0},
             tmp_dst[4*4*N] = {0};

        memcpy(tmp_src, (const char*)src + (size_t)i*src_bpp, (size_t)n*src_bpp);
        exec_ops(program, arguments, tmp_src, tmp_dst, 0);
        memcpy((char*)dst + (size_t)i*dst_bpp, tmp_dst, (size_t)n*dst_bpp);
    }
}

// Clean up any #defines we may have set so that we can be #included again.
#if defined(USING_AVX)
    #undef  USING_AVX
#endif
#if defined(USING_AVX_F16C)
    #undef  USING_AVX_F16C
#endif
#if defined(USING_AVX2)
    #undef  USING_AVX2
#endif

#if defined(USING_NEON)
    #undef  USING_NEON
#endif
#if defined(USING_NEON_F16C)
    #undef  USING_NEON_F16C
#endif
