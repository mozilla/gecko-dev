/*
 * SPDX-FileCopyrightText: 2024 Cryspen Sarl <info@cryspen.com>
 *
 * SPDX-License-Identifier: MIT or Apache-2.0
 *
 * This code was generated with the following revisions:
 * Charon: b351338f6a84c7a1afc27433eb0ffdc668b3581d
 * Eurydice: 7efec1624422fd5e94388ef06b9c76dfe7a48d46
 * Karamel: c96fb69d15693284644d6aecaa90afa37e4de8f0
 * F*: 58c915a86a2c07c8eca8d9deafd76cb7a91f0eb7
 * Libcrux: 6ff01fb3c57ff29ecb59bc62d9dc7fd231060cfb
 */

#ifndef __libcrux_sha3_internal_H
#define __libcrux_sha3_internal_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "eurydice_glue.h"
#include "libcrux_core.h"

static const uint64_t libcrux_sha3_generic_keccak_ROUNDCONSTANTS[24U] = {
    1ULL,
    32898ULL,
    9223372036854808714ULL,
    9223372039002292224ULL,
    32907ULL,
    2147483649ULL,
    9223372039002292353ULL,
    9223372036854808585ULL,
    138ULL,
    136ULL,
    2147516425ULL,
    2147483658ULL,
    2147516555ULL,
    9223372036854775947ULL,
    9223372036854808713ULL,
    9223372036854808579ULL,
    9223372036854808578ULL,
    9223372036854775936ULL,
    32778ULL,
    9223372039002259466ULL,
    9223372039002292353ULL,
    9223372036854808704ULL,
    2147483649ULL,
    9223372039002292232ULL
};

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_zero_5a(void)
{
    return 0ULL;
}

static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak__veor5q_u64(
    uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e)
{
    uint64_t ab = a ^ b;
    uint64_t cd = c ^ d;
    uint64_t abcd = ab ^ cd;
    return abcd ^ e;
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_xor5_5a(
    uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e)
{
    return libcrux_sha3_portable_keccak__veor5q_u64(a, b, c, d, e);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.rotate_left
with const generics
- LEFT= 1
- RIGHT= 63
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_rotate_left_d6(uint64_t x)
{
    return x << (uint32_t)(int32_t)1 | x >> (uint32_t)(int32_t)63;
}

static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak__vrax1q_u64(uint64_t a, uint64_t b)
{
    uint64_t uu____0 = a;
    return uu____0 ^ libcrux_sha3_portable_keccak_rotate_left_d6(b);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_rotate_left1_and_xor_5a(uint64_t a, uint64_t b)
{
    return libcrux_sha3_portable_keccak__vrax1q_u64(a, b);
}

static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak__vbcaxq_u64(uint64_t a, uint64_t b, uint64_t c)
{
    return a ^ (b & ~c);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_and_not_xor_5a(
    uint64_t a, uint64_t b, uint64_t c)
{
    return libcrux_sha3_portable_keccak__vbcaxq_u64(a, b, c);
}

static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak__veorq_n_u64(uint64_t a, uint64_t c)
{
    return a ^ c;
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_xor_constant_5a(uint64_t a, uint64_t c)
{
    return libcrux_sha3_portable_keccak__veorq_n_u64(a, c);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_xor_5a(uint64_t a, uint64_t b)
{
    return a ^ b;
}

static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_slice_1(
    Eurydice_slice a[1U], size_t start, size_t len, Eurydice_slice ret[1U])
{
    ret[0U] = Eurydice_slice_subslice2(a[0U], start, start + len, uint8_t);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_slice_n_5a(
    Eurydice_slice a[1U], size_t start, size_t len, Eurydice_slice ret[1U])
{
    /* Passing arrays by value in Rust generates a copy in C */
    Eurydice_slice copy_of_a[1U];
    memcpy(copy_of_a, a, (size_t)1U * sizeof(Eurydice_slice));
    Eurydice_slice ret0[1U];
    libcrux_sha3_portable_keccak_slice_1(copy_of_a, start, len, ret0);
    memcpy(ret, ret0, (size_t)1U * sizeof(Eurydice_slice));
}

static KRML_MUSTINLINE Eurydice_slice_uint8_t_1size_t__x2
libcrux_sha3_portable_keccak_split_at_mut_1(Eurydice_slice out[1U],
                                            size_t mid)
{
    Eurydice_slice_uint8_t_x2 uu____0 = Eurydice_slice_split_at_mut(
        out[0U], mid, uint8_t, Eurydice_slice_uint8_t_x2);
    Eurydice_slice out00 = uu____0.fst;
    Eurydice_slice out01 = uu____0.snd;
    Eurydice_slice_uint8_t_1size_t__x2 lit;
    lit.fst[0U] = out00;
    lit.snd[0U] = out01;
    return lit;
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
static KRML_MUSTINLINE Eurydice_slice_uint8_t_1size_t__x2
libcrux_sha3_portable_keccak_split_at_mut_n_5a(Eurydice_slice a[1U],
                                               size_t mid)
{
    return libcrux_sha3_portable_keccak_split_at_mut_1(a, mid);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.KeccakState
with types uint64_t
with const generics
- $1size_t
*/
typedef struct libcrux_sha3_generic_keccak_KeccakState_48_s {
    uint64_t st[5U][5U];
} libcrux_sha3_generic_keccak_KeccakState_48;

/**
 Create a new Shake128 x4 state.
*/
/**
This function found in impl {libcrux_sha3::generic_keccak::KeccakState<T,
N>[TraitClause@0]#1}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.new_1e
with types uint64_t
with const generics
- N= 1
*/
static KRML_MUSTINLINE libcrux_sha3_generic_keccak_KeccakState_48
libcrux_sha3_generic_keccak_new_1e_cf(void)
{
    libcrux_sha3_generic_keccak_KeccakState_48 lit;
    lit.st[0U][0U] = libcrux_sha3_portable_keccak_zero_5a();
    lit.st[0U][1U] = libcrux_sha3_portable_keccak_zero_5a();
    lit.st[0U][2U] = libcrux_sha3_portable_keccak_zero_5a();
    lit.st[0U][3U] = libcrux_sha3_portable_keccak_zero_5a();
    lit.st[0U][4U] = libcrux_sha3_portable_keccak_zero_5a();
    lit.st[1U][0U] = libcrux_sha3_portable_keccak_zero_5a();
    lit.st[1U][1U] = libcrux_sha3_portable_keccak_zero_5a();
    lit.st[1U][2U] = libcrux_sha3_portable_keccak_zero_5a();
    lit.st[1U][3U] = libcrux_sha3_portable_keccak_zero_5a();
    lit.st[1U][4U] = libcrux_sha3_portable_keccak_zero_5a();
    lit.st[2U][0U] = libcrux_sha3_portable_keccak_zero_5a();
    lit.st[2U][1U] = libcrux_sha3_portable_keccak_zero_5a();
    lit.st[2U][2U] = libcrux_sha3_portable_keccak_zero_5a();
    lit.st[2U][3U] = libcrux_sha3_portable_keccak_zero_5a();
    lit.st[2U][4U] = libcrux_sha3_portable_keccak_zero_5a();
    lit.st[3U][0U] = libcrux_sha3_portable_keccak_zero_5a();
    lit.st[3U][1U] = libcrux_sha3_portable_keccak_zero_5a();
    lit.st[3U][2U] = libcrux_sha3_portable_keccak_zero_5a();
    lit.st[3U][3U] = libcrux_sha3_portable_keccak_zero_5a();
    lit.st[3U][4U] = libcrux_sha3_portable_keccak_zero_5a();
    lit.st[4U][0U] = libcrux_sha3_portable_keccak_zero_5a();
    lit.st[4U][1U] = libcrux_sha3_portable_keccak_zero_5a();
    lit.st[4U][2U] = libcrux_sha3_portable_keccak_zero_5a();
    lit.st[4U][3U] = libcrux_sha3_portable_keccak_zero_5a();
    lit.st[4U][4U] = libcrux_sha3_portable_keccak_zero_5a();
    return lit;
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.load_block
with const generics
- RATE= 168
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_load_block_65(
    uint64_t (*s)[5U], Eurydice_slice blocks[1U])
{
    for (size_t i = (size_t)0U; i < (size_t)168U / (size_t)8U; i++) {
        size_t i0 = i;
        uint8_t uu____0[8U];
        core_result_Result_56 dst;
        Eurydice_slice_to_array2(
            &dst,
            Eurydice_slice_subslice2(blocks[0U], (size_t)8U * i0,
                                     (size_t)8U * i0 + (size_t)8U, uint8_t),
            Eurydice_slice, uint8_t[8U]);
        core_result_unwrap_41_0e(dst, uu____0);
        size_t uu____1 = i0 / (size_t)5U;
        size_t uu____2 = i0 % (size_t)5U;
        s[uu____1][uu____2] =
            s[uu____1][uu____2] ^ core_num__u64_9__from_le_bytes(uu____0);
    }
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.load_block_full
with const generics
- RATE= 168
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_load_block_full_d4(
    uint64_t (*s)[5U], uint8_t blocks[1U][200U])
{
    Eurydice_slice buf[1U] = {
        Eurydice_array_to_slice((size_t)200U, blocks[0U], uint8_t)
    };
    libcrux_sha3_portable_keccak_load_block_65(s, buf);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.load_block_full_5a
with const generics
- RATE= 168
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_load_block_full_5a_05(
    uint64_t (*a)[5U], uint8_t b[1U][200U])
{
    uint64_t(*uu____0)[5U] = a;
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_b[1U][200U];
    memcpy(copy_of_b, b, (size_t)1U * sizeof(uint8_t[200U]));
    libcrux_sha3_portable_keccak_load_block_full_d4(uu____0, copy_of_b);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.rotate_left
with const generics
- LEFT= 36
- RIGHT= 28
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_rotate_left_d60(uint64_t x)
{
    return x << (uint32_t)(int32_t)36 | x >> (uint32_t)(int32_t)28;
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak._vxarq_u64
with const generics
- LEFT= 36
- RIGHT= 28
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak__vxarq_u64_74(uint64_t a, uint64_t b)
{
    uint64_t ab = a ^ b;
    return libcrux_sha3_portable_keccak_rotate_left_d60(ab);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.xor_and_rotate_5a
with const generics
- LEFT= 36
- RIGHT= 28
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_xor_and_rotate_5a_03(uint64_t a, uint64_t b)
{
    return libcrux_sha3_portable_keccak__vxarq_u64_74(a, b);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.rotate_left
with const generics
- LEFT= 3
- RIGHT= 61
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_rotate_left_d61(uint64_t x)
{
    return x << (uint32_t)(int32_t)3 | x >> (uint32_t)(int32_t)61;
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak._vxarq_u64
with const generics
- LEFT= 3
- RIGHT= 61
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak__vxarq_u64_740(uint64_t a, uint64_t b)
{
    uint64_t ab = a ^ b;
    return libcrux_sha3_portable_keccak_rotate_left_d61(ab);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.xor_and_rotate_5a
with const generics
- LEFT= 3
- RIGHT= 61
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_xor_and_rotate_5a_030(uint64_t a, uint64_t b)
{
    return libcrux_sha3_portable_keccak__vxarq_u64_740(a, b);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.rotate_left
with const generics
- LEFT= 41
- RIGHT= 23
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_rotate_left_d62(uint64_t x)
{
    return x << (uint32_t)(int32_t)41 | x >> (uint32_t)(int32_t)23;
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak._vxarq_u64
with const generics
- LEFT= 41
- RIGHT= 23
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak__vxarq_u64_741(uint64_t a, uint64_t b)
{
    uint64_t ab = a ^ b;
    return libcrux_sha3_portable_keccak_rotate_left_d62(ab);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.xor_and_rotate_5a
with const generics
- LEFT= 41
- RIGHT= 23
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_xor_and_rotate_5a_031(uint64_t a, uint64_t b)
{
    return libcrux_sha3_portable_keccak__vxarq_u64_741(a, b);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.rotate_left
with const generics
- LEFT= 18
- RIGHT= 46
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_rotate_left_d63(uint64_t x)
{
    return x << (uint32_t)(int32_t)18 | x >> (uint32_t)(int32_t)46;
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak._vxarq_u64
with const generics
- LEFT= 18
- RIGHT= 46
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak__vxarq_u64_742(uint64_t a, uint64_t b)
{
    uint64_t ab = a ^ b;
    return libcrux_sha3_portable_keccak_rotate_left_d63(ab);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.xor_and_rotate_5a
with const generics
- LEFT= 18
- RIGHT= 46
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_xor_and_rotate_5a_032(uint64_t a, uint64_t b)
{
    return libcrux_sha3_portable_keccak__vxarq_u64_742(a, b);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak._vxarq_u64
with const generics
- LEFT= 1
- RIGHT= 63
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak__vxarq_u64_743(uint64_t a, uint64_t b)
{
    uint64_t ab = a ^ b;
    return libcrux_sha3_portable_keccak_rotate_left_d6(ab);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.xor_and_rotate_5a
with const generics
- LEFT= 1
- RIGHT= 63
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_xor_and_rotate_5a_033(uint64_t a, uint64_t b)
{
    return libcrux_sha3_portable_keccak__vxarq_u64_743(a, b);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.rotate_left
with const generics
- LEFT= 44
- RIGHT= 20
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_rotate_left_d64(uint64_t x)
{
    return x << (uint32_t)(int32_t)44 | x >> (uint32_t)(int32_t)20;
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak._vxarq_u64
with const generics
- LEFT= 44
- RIGHT= 20
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak__vxarq_u64_744(uint64_t a, uint64_t b)
{
    uint64_t ab = a ^ b;
    return libcrux_sha3_portable_keccak_rotate_left_d64(ab);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.xor_and_rotate_5a
with const generics
- LEFT= 44
- RIGHT= 20
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_xor_and_rotate_5a_034(uint64_t a, uint64_t b)
{
    return libcrux_sha3_portable_keccak__vxarq_u64_744(a, b);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.rotate_left
with const generics
- LEFT= 10
- RIGHT= 54
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_rotate_left_d65(uint64_t x)
{
    return x << (uint32_t)(int32_t)10 | x >> (uint32_t)(int32_t)54;
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak._vxarq_u64
with const generics
- LEFT= 10
- RIGHT= 54
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak__vxarq_u64_745(uint64_t a, uint64_t b)
{
    uint64_t ab = a ^ b;
    return libcrux_sha3_portable_keccak_rotate_left_d65(ab);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.xor_and_rotate_5a
with const generics
- LEFT= 10
- RIGHT= 54
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_xor_and_rotate_5a_035(uint64_t a, uint64_t b)
{
    return libcrux_sha3_portable_keccak__vxarq_u64_745(a, b);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.rotate_left
with const generics
- LEFT= 45
- RIGHT= 19
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_rotate_left_d66(uint64_t x)
{
    return x << (uint32_t)(int32_t)45 | x >> (uint32_t)(int32_t)19;
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak._vxarq_u64
with const generics
- LEFT= 45
- RIGHT= 19
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak__vxarq_u64_746(uint64_t a, uint64_t b)
{
    uint64_t ab = a ^ b;
    return libcrux_sha3_portable_keccak_rotate_left_d66(ab);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.xor_and_rotate_5a
with const generics
- LEFT= 45
- RIGHT= 19
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_xor_and_rotate_5a_036(uint64_t a, uint64_t b)
{
    return libcrux_sha3_portable_keccak__vxarq_u64_746(a, b);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.rotate_left
with const generics
- LEFT= 2
- RIGHT= 62
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_rotate_left_d67(uint64_t x)
{
    return x << (uint32_t)(int32_t)2 | x >> (uint32_t)(int32_t)62;
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak._vxarq_u64
with const generics
- LEFT= 2
- RIGHT= 62
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak__vxarq_u64_747(uint64_t a, uint64_t b)
{
    uint64_t ab = a ^ b;
    return libcrux_sha3_portable_keccak_rotate_left_d67(ab);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.xor_and_rotate_5a
with const generics
- LEFT= 2
- RIGHT= 62
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_xor_and_rotate_5a_037(uint64_t a, uint64_t b)
{
    return libcrux_sha3_portable_keccak__vxarq_u64_747(a, b);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.rotate_left
with const generics
- LEFT= 62
- RIGHT= 2
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_rotate_left_d68(uint64_t x)
{
    return x << (uint32_t)(int32_t)62 | x >> (uint32_t)(int32_t)2;
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak._vxarq_u64
with const generics
- LEFT= 62
- RIGHT= 2
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak__vxarq_u64_748(uint64_t a, uint64_t b)
{
    uint64_t ab = a ^ b;
    return libcrux_sha3_portable_keccak_rotate_left_d68(ab);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.xor_and_rotate_5a
with const generics
- LEFT= 62
- RIGHT= 2
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_xor_and_rotate_5a_038(uint64_t a, uint64_t b)
{
    return libcrux_sha3_portable_keccak__vxarq_u64_748(a, b);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.rotate_left
with const generics
- LEFT= 6
- RIGHT= 58
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_rotate_left_d69(uint64_t x)
{
    return x << (uint32_t)(int32_t)6 | x >> (uint32_t)(int32_t)58;
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak._vxarq_u64
with const generics
- LEFT= 6
- RIGHT= 58
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak__vxarq_u64_749(uint64_t a, uint64_t b)
{
    uint64_t ab = a ^ b;
    return libcrux_sha3_portable_keccak_rotate_left_d69(ab);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.xor_and_rotate_5a
with const generics
- LEFT= 6
- RIGHT= 58
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_xor_and_rotate_5a_039(uint64_t a, uint64_t b)
{
    return libcrux_sha3_portable_keccak__vxarq_u64_749(a, b);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.rotate_left
with const generics
- LEFT= 43
- RIGHT= 21
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_rotate_left_d610(uint64_t x)
{
    return x << (uint32_t)(int32_t)43 | x >> (uint32_t)(int32_t)21;
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak._vxarq_u64
with const generics
- LEFT= 43
- RIGHT= 21
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak__vxarq_u64_7410(uint64_t a, uint64_t b)
{
    uint64_t ab = a ^ b;
    return libcrux_sha3_portable_keccak_rotate_left_d610(ab);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.xor_and_rotate_5a
with const generics
- LEFT= 43
- RIGHT= 21
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_xor_and_rotate_5a_0310(uint64_t a, uint64_t b)
{
    return libcrux_sha3_portable_keccak__vxarq_u64_7410(a, b);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.rotate_left
with const generics
- LEFT= 15
- RIGHT= 49
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_rotate_left_d611(uint64_t x)
{
    return x << (uint32_t)(int32_t)15 | x >> (uint32_t)(int32_t)49;
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak._vxarq_u64
with const generics
- LEFT= 15
- RIGHT= 49
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak__vxarq_u64_7411(uint64_t a, uint64_t b)
{
    uint64_t ab = a ^ b;
    return libcrux_sha3_portable_keccak_rotate_left_d611(ab);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.xor_and_rotate_5a
with const generics
- LEFT= 15
- RIGHT= 49
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_xor_and_rotate_5a_0311(uint64_t a, uint64_t b)
{
    return libcrux_sha3_portable_keccak__vxarq_u64_7411(a, b);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.rotate_left
with const generics
- LEFT= 61
- RIGHT= 3
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_rotate_left_d612(uint64_t x)
{
    return x << (uint32_t)(int32_t)61 | x >> (uint32_t)(int32_t)3;
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak._vxarq_u64
with const generics
- LEFT= 61
- RIGHT= 3
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak__vxarq_u64_7412(uint64_t a, uint64_t b)
{
    uint64_t ab = a ^ b;
    return libcrux_sha3_portable_keccak_rotate_left_d612(ab);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.xor_and_rotate_5a
with const generics
- LEFT= 61
- RIGHT= 3
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_xor_and_rotate_5a_0312(uint64_t a, uint64_t b)
{
    return libcrux_sha3_portable_keccak__vxarq_u64_7412(a, b);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.rotate_left
with const generics
- LEFT= 28
- RIGHT= 36
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_rotate_left_d613(uint64_t x)
{
    return x << (uint32_t)(int32_t)28 | x >> (uint32_t)(int32_t)36;
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak._vxarq_u64
with const generics
- LEFT= 28
- RIGHT= 36
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak__vxarq_u64_7413(uint64_t a, uint64_t b)
{
    uint64_t ab = a ^ b;
    return libcrux_sha3_portable_keccak_rotate_left_d613(ab);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.xor_and_rotate_5a
with const generics
- LEFT= 28
- RIGHT= 36
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_xor_and_rotate_5a_0313(uint64_t a, uint64_t b)
{
    return libcrux_sha3_portable_keccak__vxarq_u64_7413(a, b);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.rotate_left
with const generics
- LEFT= 55
- RIGHT= 9
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_rotate_left_d614(uint64_t x)
{
    return x << (uint32_t)(int32_t)55 | x >> (uint32_t)(int32_t)9;
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak._vxarq_u64
with const generics
- LEFT= 55
- RIGHT= 9
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak__vxarq_u64_7414(uint64_t a, uint64_t b)
{
    uint64_t ab = a ^ b;
    return libcrux_sha3_portable_keccak_rotate_left_d614(ab);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.xor_and_rotate_5a
with const generics
- LEFT= 55
- RIGHT= 9
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_xor_and_rotate_5a_0314(uint64_t a, uint64_t b)
{
    return libcrux_sha3_portable_keccak__vxarq_u64_7414(a, b);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.rotate_left
with const generics
- LEFT= 25
- RIGHT= 39
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_rotate_left_d615(uint64_t x)
{
    return x << (uint32_t)(int32_t)25 | x >> (uint32_t)(int32_t)39;
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak._vxarq_u64
with const generics
- LEFT= 25
- RIGHT= 39
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak__vxarq_u64_7415(uint64_t a, uint64_t b)
{
    uint64_t ab = a ^ b;
    return libcrux_sha3_portable_keccak_rotate_left_d615(ab);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.xor_and_rotate_5a
with const generics
- LEFT= 25
- RIGHT= 39
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_xor_and_rotate_5a_0315(uint64_t a, uint64_t b)
{
    return libcrux_sha3_portable_keccak__vxarq_u64_7415(a, b);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.rotate_left
with const generics
- LEFT= 21
- RIGHT= 43
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_rotate_left_d616(uint64_t x)
{
    return x << (uint32_t)(int32_t)21 | x >> (uint32_t)(int32_t)43;
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak._vxarq_u64
with const generics
- LEFT= 21
- RIGHT= 43
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak__vxarq_u64_7416(uint64_t a, uint64_t b)
{
    uint64_t ab = a ^ b;
    return libcrux_sha3_portable_keccak_rotate_left_d616(ab);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.xor_and_rotate_5a
with const generics
- LEFT= 21
- RIGHT= 43
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_xor_and_rotate_5a_0316(uint64_t a, uint64_t b)
{
    return libcrux_sha3_portable_keccak__vxarq_u64_7416(a, b);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.rotate_left
with const generics
- LEFT= 56
- RIGHT= 8
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_rotate_left_d617(uint64_t x)
{
    return x << (uint32_t)(int32_t)56 | x >> (uint32_t)(int32_t)8;
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak._vxarq_u64
with const generics
- LEFT= 56
- RIGHT= 8
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak__vxarq_u64_7417(uint64_t a, uint64_t b)
{
    uint64_t ab = a ^ b;
    return libcrux_sha3_portable_keccak_rotate_left_d617(ab);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.xor_and_rotate_5a
with const generics
- LEFT= 56
- RIGHT= 8
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_xor_and_rotate_5a_0317(uint64_t a, uint64_t b)
{
    return libcrux_sha3_portable_keccak__vxarq_u64_7417(a, b);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.rotate_left
with const generics
- LEFT= 27
- RIGHT= 37
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_rotate_left_d618(uint64_t x)
{
    return x << (uint32_t)(int32_t)27 | x >> (uint32_t)(int32_t)37;
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak._vxarq_u64
with const generics
- LEFT= 27
- RIGHT= 37
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak__vxarq_u64_7418(uint64_t a, uint64_t b)
{
    uint64_t ab = a ^ b;
    return libcrux_sha3_portable_keccak_rotate_left_d618(ab);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.xor_and_rotate_5a
with const generics
- LEFT= 27
- RIGHT= 37
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_xor_and_rotate_5a_0318(uint64_t a, uint64_t b)
{
    return libcrux_sha3_portable_keccak__vxarq_u64_7418(a, b);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.rotate_left
with const generics
- LEFT= 20
- RIGHT= 44
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_rotate_left_d619(uint64_t x)
{
    return x << (uint32_t)(int32_t)20 | x >> (uint32_t)(int32_t)44;
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak._vxarq_u64
with const generics
- LEFT= 20
- RIGHT= 44
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak__vxarq_u64_7419(uint64_t a, uint64_t b)
{
    uint64_t ab = a ^ b;
    return libcrux_sha3_portable_keccak_rotate_left_d619(ab);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.xor_and_rotate_5a
with const generics
- LEFT= 20
- RIGHT= 44
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_xor_and_rotate_5a_0319(uint64_t a, uint64_t b)
{
    return libcrux_sha3_portable_keccak__vxarq_u64_7419(a, b);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.rotate_left
with const generics
- LEFT= 39
- RIGHT= 25
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_rotate_left_d620(uint64_t x)
{
    return x << (uint32_t)(int32_t)39 | x >> (uint32_t)(int32_t)25;
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak._vxarq_u64
with const generics
- LEFT= 39
- RIGHT= 25
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak__vxarq_u64_7420(uint64_t a, uint64_t b)
{
    uint64_t ab = a ^ b;
    return libcrux_sha3_portable_keccak_rotate_left_d620(ab);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.xor_and_rotate_5a
with const generics
- LEFT= 39
- RIGHT= 25
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_xor_and_rotate_5a_0320(uint64_t a, uint64_t b)
{
    return libcrux_sha3_portable_keccak__vxarq_u64_7420(a, b);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.rotate_left
with const generics
- LEFT= 8
- RIGHT= 56
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_rotate_left_d621(uint64_t x)
{
    return x << (uint32_t)(int32_t)8 | x >> (uint32_t)(int32_t)56;
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak._vxarq_u64
with const generics
- LEFT= 8
- RIGHT= 56
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak__vxarq_u64_7421(uint64_t a, uint64_t b)
{
    uint64_t ab = a ^ b;
    return libcrux_sha3_portable_keccak_rotate_left_d621(ab);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.xor_and_rotate_5a
with const generics
- LEFT= 8
- RIGHT= 56
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_xor_and_rotate_5a_0321(uint64_t a, uint64_t b)
{
    return libcrux_sha3_portable_keccak__vxarq_u64_7421(a, b);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.rotate_left
with const generics
- LEFT= 14
- RIGHT= 50
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_rotate_left_d622(uint64_t x)
{
    return x << (uint32_t)(int32_t)14 | x >> (uint32_t)(int32_t)50;
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak._vxarq_u64
with const generics
- LEFT= 14
- RIGHT= 50
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak__vxarq_u64_7422(uint64_t a, uint64_t b)
{
    uint64_t ab = a ^ b;
    return libcrux_sha3_portable_keccak_rotate_left_d622(ab);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.xor_and_rotate_5a
with const generics
- LEFT= 14
- RIGHT= 50
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_portable_keccak_xor_and_rotate_5a_0322(uint64_t a, uint64_t b)
{
    return libcrux_sha3_portable_keccak__vxarq_u64_7422(a, b);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.theta_rho
with types uint64_t
with const generics
- N= 1
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_theta_rho_a7(
    libcrux_sha3_generic_keccak_KeccakState_48 *s)
{
    uint64_t c[5U] = {
        libcrux_sha3_portable_keccak_xor5_5a(s->st[0U][0U], s->st[1U][0U],
                                             s->st[2U][0U], s->st[3U][0U],
                                             s->st[4U][0U]),
        libcrux_sha3_portable_keccak_xor5_5a(s->st[0U][1U], s->st[1U][1U],
                                             s->st[2U][1U], s->st[3U][1U],
                                             s->st[4U][1U]),
        libcrux_sha3_portable_keccak_xor5_5a(s->st[0U][2U], s->st[1U][2U],
                                             s->st[2U][2U], s->st[3U][2U],
                                             s->st[4U][2U]),
        libcrux_sha3_portable_keccak_xor5_5a(s->st[0U][3U], s->st[1U][3U],
                                             s->st[2U][3U], s->st[3U][3U],
                                             s->st[4U][3U]),
        libcrux_sha3_portable_keccak_xor5_5a(s->st[0U][4U], s->st[1U][4U],
                                             s->st[2U][4U], s->st[3U][4U],
                                             s->st[4U][4U])
    };
    uint64_t uu____0 = libcrux_sha3_portable_keccak_rotate_left1_and_xor_5a(
        c[((size_t)0U + (size_t)4U) % (size_t)5U],
        c[((size_t)0U + (size_t)1U) % (size_t)5U]);
    uint64_t uu____1 = libcrux_sha3_portable_keccak_rotate_left1_and_xor_5a(
        c[((size_t)1U + (size_t)4U) % (size_t)5U],
        c[((size_t)1U + (size_t)1U) % (size_t)5U]);
    uint64_t uu____2 = libcrux_sha3_portable_keccak_rotate_left1_and_xor_5a(
        c[((size_t)2U + (size_t)4U) % (size_t)5U],
        c[((size_t)2U + (size_t)1U) % (size_t)5U]);
    uint64_t uu____3 = libcrux_sha3_portable_keccak_rotate_left1_and_xor_5a(
        c[((size_t)3U + (size_t)4U) % (size_t)5U],
        c[((size_t)3U + (size_t)1U) % (size_t)5U]);
    uint64_t t[5U] = { uu____0, uu____1, uu____2, uu____3,
                       libcrux_sha3_portable_keccak_rotate_left1_and_xor_5a(
                           c[((size_t)4U + (size_t)4U) % (size_t)5U],
                           c[((size_t)4U + (size_t)1U) % (size_t)5U]) };
    s->st[0U][0U] = libcrux_sha3_portable_keccak_xor_5a(s->st[0U][0U], t[0U]);
    s->st[1U][0U] =
        libcrux_sha3_portable_keccak_xor_and_rotate_5a_03(s->st[1U][0U], t[0U]);
    s->st[2U][0U] =
        libcrux_sha3_portable_keccak_xor_and_rotate_5a_030(s->st[2U][0U], t[0U]);
    s->st[3U][0U] =
        libcrux_sha3_portable_keccak_xor_and_rotate_5a_031(s->st[3U][0U], t[0U]);
    s->st[4U][0U] =
        libcrux_sha3_portable_keccak_xor_and_rotate_5a_032(s->st[4U][0U], t[0U]);
    s->st[0U][1U] =
        libcrux_sha3_portable_keccak_xor_and_rotate_5a_033(s->st[0U][1U], t[1U]);
    s->st[1U][1U] =
        libcrux_sha3_portable_keccak_xor_and_rotate_5a_034(s->st[1U][1U], t[1U]);
    s->st[2U][1U] =
        libcrux_sha3_portable_keccak_xor_and_rotate_5a_035(s->st[2U][1U], t[1U]);
    s->st[3U][1U] =
        libcrux_sha3_portable_keccak_xor_and_rotate_5a_036(s->st[3U][1U], t[1U]);
    s->st[4U][1U] =
        libcrux_sha3_portable_keccak_xor_and_rotate_5a_037(s->st[4U][1U], t[1U]);
    s->st[0U][2U] =
        libcrux_sha3_portable_keccak_xor_and_rotate_5a_038(s->st[0U][2U], t[2U]);
    s->st[1U][2U] =
        libcrux_sha3_portable_keccak_xor_and_rotate_5a_039(s->st[1U][2U], t[2U]);
    s->st[2U][2U] =
        libcrux_sha3_portable_keccak_xor_and_rotate_5a_0310(s->st[2U][2U], t[2U]);
    s->st[3U][2U] =
        libcrux_sha3_portable_keccak_xor_and_rotate_5a_0311(s->st[3U][2U], t[2U]);
    s->st[4U][2U] =
        libcrux_sha3_portable_keccak_xor_and_rotate_5a_0312(s->st[4U][2U], t[2U]);
    s->st[0U][3U] =
        libcrux_sha3_portable_keccak_xor_and_rotate_5a_0313(s->st[0U][3U], t[3U]);
    s->st[1U][3U] =
        libcrux_sha3_portable_keccak_xor_and_rotate_5a_0314(s->st[1U][3U], t[3U]);
    s->st[2U][3U] =
        libcrux_sha3_portable_keccak_xor_and_rotate_5a_0315(s->st[2U][3U], t[3U]);
    s->st[3U][3U] =
        libcrux_sha3_portable_keccak_xor_and_rotate_5a_0316(s->st[3U][3U], t[3U]);
    s->st[4U][3U] =
        libcrux_sha3_portable_keccak_xor_and_rotate_5a_0317(s->st[4U][3U], t[3U]);
    s->st[0U][4U] =
        libcrux_sha3_portable_keccak_xor_and_rotate_5a_0318(s->st[0U][4U], t[4U]);
    s->st[1U][4U] =
        libcrux_sha3_portable_keccak_xor_and_rotate_5a_0319(s->st[1U][4U], t[4U]);
    s->st[2U][4U] =
        libcrux_sha3_portable_keccak_xor_and_rotate_5a_0320(s->st[2U][4U], t[4U]);
    s->st[3U][4U] =
        libcrux_sha3_portable_keccak_xor_and_rotate_5a_0321(s->st[3U][4U], t[4U]);
    uint64_t uu____27 =
        libcrux_sha3_portable_keccak_xor_and_rotate_5a_0322(s->st[4U][4U], t[4U]);
    s->st[4U][4U] = uu____27;
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.pi
with types uint64_t
with const generics
- N= 1
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_pi_d5(
    libcrux_sha3_generic_keccak_KeccakState_48 *s)
{
    uint64_t old[5U][5U];
    memcpy(old, s->st, (size_t)5U * sizeof(uint64_t[5U]));
    s->st[0U][1U] = old[1U][1U];
    s->st[0U][2U] = old[2U][2U];
    s->st[0U][3U] = old[3U][3U];
    s->st[0U][4U] = old[4U][4U];
    s->st[1U][0U] = old[0U][3U];
    s->st[1U][1U] = old[1U][4U];
    s->st[1U][2U] = old[2U][0U];
    s->st[1U][3U] = old[3U][1U];
    s->st[1U][4U] = old[4U][2U];
    s->st[2U][0U] = old[0U][1U];
    s->st[2U][1U] = old[1U][2U];
    s->st[2U][2U] = old[2U][3U];
    s->st[2U][3U] = old[3U][4U];
    s->st[2U][4U] = old[4U][0U];
    s->st[3U][0U] = old[0U][4U];
    s->st[3U][1U] = old[1U][0U];
    s->st[3U][2U] = old[2U][1U];
    s->st[3U][3U] = old[3U][2U];
    s->st[3U][4U] = old[4U][3U];
    s->st[4U][0U] = old[0U][2U];
    s->st[4U][1U] = old[1U][3U];
    s->st[4U][2U] = old[2U][4U];
    s->st[4U][3U] = old[3U][0U];
    s->st[4U][4U] = old[4U][1U];
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.chi
with types uint64_t
with const generics
- N= 1
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_chi_3e(
    libcrux_sha3_generic_keccak_KeccakState_48 *s)
{
    uint64_t old[5U][5U];
    memcpy(old, s->st, (size_t)5U * sizeof(uint64_t[5U]));
    KRML_MAYBE_FOR5(
        i0, (size_t)0U, (size_t)5U, (size_t)1U, size_t i1 = i0; KRML_MAYBE_FOR5(
            i, (size_t)0U, (size_t)5U, (size_t)1U, size_t j = i;
            s->st[i1][j] = libcrux_sha3_portable_keccak_and_not_xor_5a(
                s->st[i1][j], old[i1][(j + (size_t)2U) % (size_t)5U],
                old[i1][(j + (size_t)1U) % (size_t)5U]);););
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.iota
with types uint64_t
with const generics
- N= 1
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_iota_00(
    libcrux_sha3_generic_keccak_KeccakState_48 *s, size_t i)
{
    s->st[0U][0U] = libcrux_sha3_portable_keccak_xor_constant_5a(
        s->st[0U][0U], libcrux_sha3_generic_keccak_ROUNDCONSTANTS[i]);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.keccakf1600
with types uint64_t
with const generics
- N= 1
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_keccakf1600_b8(
    libcrux_sha3_generic_keccak_KeccakState_48 *s)
{
    for (size_t i = (size_t)0U; i < (size_t)24U; i++) {
        size_t i0 = i;
        libcrux_sha3_generic_keccak_theta_rho_a7(s);
        libcrux_sha3_generic_keccak_pi_d5(s);
        libcrux_sha3_generic_keccak_chi_3e(s);
        libcrux_sha3_generic_keccak_iota_00(s, i0);
    }
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.absorb_final
with types uint64_t
with const generics
- N= 1
- RATE= 168
- DELIM= 31
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_absorb_final_40(
    libcrux_sha3_generic_keccak_KeccakState_48 *s, Eurydice_slice last[1U])
{
    size_t last_len = Eurydice_slice_len(last[0U], uint8_t);
    uint8_t blocks[1U][200U] = { { 0U } };
    {
        size_t i = (size_t)0U;
        if (last_len > (size_t)0U) {
            Eurydice_slice uu____0 =
                Eurydice_array_to_subslice2(blocks[i], (size_t)0U, last_len, uint8_t);
            Eurydice_slice_copy(uu____0, last[i], uint8_t);
        }
        blocks[i][last_len] = 31U;
        size_t uu____1 = i;
        size_t uu____2 = (size_t)168U - (size_t)1U;
        blocks[uu____1][uu____2] = (uint32_t)blocks[uu____1][uu____2] | 128U;
    }
    uint64_t(*uu____3)[5U] = s->st;
    uint8_t uu____4[1U][200U];
    memcpy(uu____4, blocks, (size_t)1U * sizeof(uint8_t[200U]));
    libcrux_sha3_portable_keccak_load_block_full_5a_05(uu____3, uu____4);
    libcrux_sha3_generic_keccak_keccakf1600_b8(s);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.store_block
with const generics
- RATE= 168
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_store_block_9b(
    uint64_t (*s)[5U], Eurydice_slice out[1U])
{
    for (size_t i = (size_t)0U; i < (size_t)168U / (size_t)8U; i++) {
        size_t i0 = i;
        Eurydice_slice uu____0 = Eurydice_slice_subslice2(
            out[0U], (size_t)8U * i0, (size_t)8U * i0 + (size_t)8U, uint8_t);
        uint8_t ret[8U];
        core_num__u64_9__to_le_bytes(s[i0 / (size_t)5U][i0 % (size_t)5U], ret);
        Eurydice_slice_copy(
            uu____0, Eurydice_array_to_slice((size_t)8U, ret, uint8_t), uint8_t);
    }
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.store_block_5a
with const generics
- RATE= 168
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_store_block_5a_49(
    uint64_t (*a)[5U], Eurydice_slice b[1U])
{
    libcrux_sha3_portable_keccak_store_block_9b(a, b);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.squeeze_next_block
with types uint64_t
with const generics
- N= 1
- RATE= 168
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_squeeze_next_block_c2(
    libcrux_sha3_generic_keccak_KeccakState_48 *s, Eurydice_slice out[1U])
{
    libcrux_sha3_generic_keccak_keccakf1600_b8(s);
    libcrux_sha3_portable_keccak_store_block_5a_49(s->st, out);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.squeeze_first_block
with types uint64_t
with const generics
- N= 1
- RATE= 168
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_squeeze_first_block_7b(
    libcrux_sha3_generic_keccak_KeccakState_48 *s, Eurydice_slice out[1U])
{
    libcrux_sha3_portable_keccak_store_block_5a_49(s->st, out);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.load_block
with const generics
- RATE= 136
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_load_block_650(
    uint64_t (*s)[5U], Eurydice_slice blocks[1U])
{
    for (size_t i = (size_t)0U; i < (size_t)136U / (size_t)8U; i++) {
        size_t i0 = i;
        uint8_t uu____0[8U];
        core_result_Result_56 dst;
        Eurydice_slice_to_array2(
            &dst,
            Eurydice_slice_subslice2(blocks[0U], (size_t)8U * i0,
                                     (size_t)8U * i0 + (size_t)8U, uint8_t),
            Eurydice_slice, uint8_t[8U]);
        core_result_unwrap_41_0e(dst, uu____0);
        size_t uu____1 = i0 / (size_t)5U;
        size_t uu____2 = i0 % (size_t)5U;
        s[uu____1][uu____2] =
            s[uu____1][uu____2] ^ core_num__u64_9__from_le_bytes(uu____0);
    }
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.load_block_full
with const generics
- RATE= 136
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_load_block_full_d40(
    uint64_t (*s)[5U], uint8_t blocks[1U][200U])
{
    Eurydice_slice buf[1U] = {
        Eurydice_array_to_slice((size_t)200U, blocks[0U], uint8_t)
    };
    libcrux_sha3_portable_keccak_load_block_650(s, buf);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.load_block_full_5a
with const generics
- RATE= 136
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_load_block_full_5a_050(
    uint64_t (*a)[5U], uint8_t b[1U][200U])
{
    uint64_t(*uu____0)[5U] = a;
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_b[1U][200U];
    memcpy(copy_of_b, b, (size_t)1U * sizeof(uint8_t[200U]));
    libcrux_sha3_portable_keccak_load_block_full_d40(uu____0, copy_of_b);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.absorb_final
with types uint64_t
with const generics
- N= 1
- RATE= 136
- DELIM= 31
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_absorb_final_400(
    libcrux_sha3_generic_keccak_KeccakState_48 *s, Eurydice_slice last[1U])
{
    size_t last_len = Eurydice_slice_len(last[0U], uint8_t);
    uint8_t blocks[1U][200U] = { { 0U } };
    {
        size_t i = (size_t)0U;
        if (last_len > (size_t)0U) {
            Eurydice_slice uu____0 =
                Eurydice_array_to_subslice2(blocks[i], (size_t)0U, last_len, uint8_t);
            Eurydice_slice_copy(uu____0, last[i], uint8_t);
        }
        blocks[i][last_len] = 31U;
        size_t uu____1 = i;
        size_t uu____2 = (size_t)136U - (size_t)1U;
        blocks[uu____1][uu____2] = (uint32_t)blocks[uu____1][uu____2] | 128U;
    }
    uint64_t(*uu____3)[5U] = s->st;
    uint8_t uu____4[1U][200U];
    memcpy(uu____4, blocks, (size_t)1U * sizeof(uint8_t[200U]));
    libcrux_sha3_portable_keccak_load_block_full_5a_050(uu____3, uu____4);
    libcrux_sha3_generic_keccak_keccakf1600_b8(s);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.store_block
with const generics
- RATE= 136
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_store_block_9b0(
    uint64_t (*s)[5U], Eurydice_slice out[1U])
{
    for (size_t i = (size_t)0U; i < (size_t)136U / (size_t)8U; i++) {
        size_t i0 = i;
        Eurydice_slice uu____0 = Eurydice_slice_subslice2(
            out[0U], (size_t)8U * i0, (size_t)8U * i0 + (size_t)8U, uint8_t);
        uint8_t ret[8U];
        core_num__u64_9__to_le_bytes(s[i0 / (size_t)5U][i0 % (size_t)5U], ret);
        Eurydice_slice_copy(
            uu____0, Eurydice_array_to_slice((size_t)8U, ret, uint8_t), uint8_t);
    }
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.store_block_5a
with const generics
- RATE= 136
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_store_block_5a_490(
    uint64_t (*a)[5U], Eurydice_slice b[1U])
{
    libcrux_sha3_portable_keccak_store_block_9b0(a, b);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.squeeze_first_block
with types uint64_t
with const generics
- N= 1
- RATE= 136
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_squeeze_first_block_7b0(
    libcrux_sha3_generic_keccak_KeccakState_48 *s, Eurydice_slice out[1U])
{
    libcrux_sha3_portable_keccak_store_block_5a_490(s->st, out);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.squeeze_next_block
with types uint64_t
with const generics
- N= 1
- RATE= 136
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_squeeze_next_block_c20(
    libcrux_sha3_generic_keccak_KeccakState_48 *s, Eurydice_slice out[1U])
{
    libcrux_sha3_generic_keccak_keccakf1600_b8(s);
    libcrux_sha3_portable_keccak_store_block_5a_490(s->st, out);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.load_block_5a
with const generics
- RATE= 136
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_load_block_5a_35(
    uint64_t (*a)[5U], Eurydice_slice b[1U])
{
    uint64_t(*uu____0)[5U] = a;
    /* Passing arrays by value in Rust generates a copy in C */
    Eurydice_slice copy_of_b[1U];
    memcpy(copy_of_b, b, (size_t)1U * sizeof(Eurydice_slice));
    libcrux_sha3_portable_keccak_load_block_650(uu____0, copy_of_b);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.load_block_5a
with const generics
- RATE= 168
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_load_block_5a_350(
    uint64_t (*a)[5U], Eurydice_slice b[1U])
{
    uint64_t(*uu____0)[5U] = a;
    /* Passing arrays by value in Rust generates a copy in C */
    Eurydice_slice copy_of_b[1U];
    memcpy(copy_of_b, b, (size_t)1U * sizeof(Eurydice_slice));
    libcrux_sha3_portable_keccak_load_block_65(uu____0, copy_of_b);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.absorb_block
with types uint64_t
with const generics
- N= 1
- RATE= 168
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_absorb_block_403(
    libcrux_sha3_generic_keccak_KeccakState_48 *s, Eurydice_slice blocks[1U])
{
    uint64_t(*uu____0)[5U] = s->st;
    Eurydice_slice uu____1[1U];
    memcpy(uu____1, blocks, (size_t)1U * sizeof(Eurydice_slice));
    libcrux_sha3_portable_keccak_load_block_5a_350(uu____0, uu____1);
    libcrux_sha3_generic_keccak_keccakf1600_b8(s);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.store_block_full
with const generics
- RATE= 168
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_store_block_full_7e3(
    uint64_t (*s)[5U], uint8_t ret[1U][200U])
{
    uint8_t out[200U] = { 0U };
    Eurydice_slice buf[1U] = {
        Eurydice_array_to_slice((size_t)200U, out, uint8_t)
    };
    libcrux_sha3_portable_keccak_store_block_9b(s, buf);
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_out[200U];
    memcpy(copy_of_out, out, (size_t)200U * sizeof(uint8_t));
    memcpy(ret[0U], copy_of_out, (size_t)200U * sizeof(uint8_t));
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.store_block_full_5a
with const generics
- RATE= 168
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_store_block_full_5a_273(uint64_t (*a)[5U],
                                                     uint8_t ret[1U][200U])
{
    libcrux_sha3_portable_keccak_store_block_full_7e3(a, ret);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.squeeze_first_and_last
with types uint64_t
with const generics
- N= 1
- RATE= 168
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_squeeze_first_and_last_883(
    libcrux_sha3_generic_keccak_KeccakState_48 *s, Eurydice_slice out[1U])
{
    uint8_t b[1U][200U];
    libcrux_sha3_portable_keccak_store_block_full_5a_273(s->st, b);
    {
        size_t i = (size_t)0U;
        Eurydice_slice uu____0 = out[i];
        uint8_t *uu____1 = b[i];
        core_ops_range_Range_b3 lit;
        lit.start = (size_t)0U;
        lit.end = Eurydice_slice_len(out[i], uint8_t);
        Eurydice_slice_copy(
            uu____0,
            Eurydice_array_to_subslice((size_t)200U, uu____1, lit, uint8_t,
                                       core_ops_range_Range_b3),
            uint8_t);
    }
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.squeeze_last
with types uint64_t
with const generics
- N= 1
- RATE= 168
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_squeeze_last_ca3(
    libcrux_sha3_generic_keccak_KeccakState_48 s, Eurydice_slice out[1U])
{
    libcrux_sha3_generic_keccak_keccakf1600_b8(&s);
    uint8_t b[1U][200U];
    libcrux_sha3_portable_keccak_store_block_full_5a_273(s.st, b);
    {
        size_t i = (size_t)0U;
        Eurydice_slice uu____0 = out[i];
        uint8_t *uu____1 = b[i];
        core_ops_range_Range_b3 lit;
        lit.start = (size_t)0U;
        lit.end = Eurydice_slice_len(out[i], uint8_t);
        Eurydice_slice_copy(
            uu____0,
            Eurydice_array_to_subslice((size_t)200U, uu____1, lit, uint8_t,
                                       core_ops_range_Range_b3),
            uint8_t);
    }
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.keccak
with types uint64_t
with const generics
- N= 1
- RATE= 168
- DELIM= 31
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_keccak_064(
    Eurydice_slice data[1U], Eurydice_slice out[1U])
{
    libcrux_sha3_generic_keccak_KeccakState_48 s =
        libcrux_sha3_generic_keccak_new_1e_cf();
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(data[0U], uint8_t) / (size_t)168U; i++) {
        size_t i0 = i;
        libcrux_sha3_generic_keccak_KeccakState_48 *uu____0 = &s;
        /* Passing arrays by value in Rust generates a copy in C */
        Eurydice_slice copy_of_data[1U];
        memcpy(copy_of_data, data, (size_t)1U * sizeof(Eurydice_slice));
        Eurydice_slice ret[1U];
        libcrux_sha3_portable_keccak_slice_n_5a(copy_of_data, i0 * (size_t)168U,
                                                (size_t)168U, ret);
        libcrux_sha3_generic_keccak_absorb_block_403(uu____0, ret);
    }
    size_t rem = Eurydice_slice_len(data[0U], uint8_t) % (size_t)168U;
    libcrux_sha3_generic_keccak_KeccakState_48 *uu____2 = &s;
    /* Passing arrays by value in Rust generates a copy in C */
    Eurydice_slice copy_of_data[1U];
    memcpy(copy_of_data, data, (size_t)1U * sizeof(Eurydice_slice));
    Eurydice_slice ret[1U];
    libcrux_sha3_portable_keccak_slice_n_5a(
        copy_of_data, Eurydice_slice_len(data[0U], uint8_t) - rem, rem, ret);
    libcrux_sha3_generic_keccak_absorb_final_40(uu____2, ret);
    size_t outlen = Eurydice_slice_len(out[0U], uint8_t);
    size_t blocks = outlen / (size_t)168U;
    size_t last = outlen - outlen % (size_t)168U;
    if (blocks == (size_t)0U) {
        libcrux_sha3_generic_keccak_squeeze_first_and_last_883(&s, out);
    } else {
        Eurydice_slice_uint8_t_1size_t__x2 uu____4 =
            libcrux_sha3_portable_keccak_split_at_mut_n_5a(out, (size_t)168U);
        Eurydice_slice o0[1U];
        memcpy(o0, uu____4.fst, (size_t)1U * sizeof(Eurydice_slice));
        Eurydice_slice o1[1U];
        memcpy(o1, uu____4.snd, (size_t)1U * sizeof(Eurydice_slice));
        libcrux_sha3_generic_keccak_squeeze_first_block_7b(&s, o0);
        core_ops_range_Range_b3 iter =
            core_iter_traits_collect___core__iter__traits__collect__IntoIterator_for_I__1__into_iter(
                (CLITERAL(core_ops_range_Range_b3){ .start = (size_t)1U,
                                                    .end = blocks }),
                core_ops_range_Range_b3, core_ops_range_Range_b3);
        while (true) {
            if (core_iter_range___core__iter__traits__iterator__Iterator_for_core__ops__range__Range_A___6__next(
                    &iter, size_t, core_option_Option_b3)
                    .tag == core_option_None) {
                break;
            } else {
                Eurydice_slice_uint8_t_1size_t__x2 uu____5 =
                    libcrux_sha3_portable_keccak_split_at_mut_n_5a(o1, (size_t)168U);
                Eurydice_slice o[1U];
                memcpy(o, uu____5.fst, (size_t)1U * sizeof(Eurydice_slice));
                Eurydice_slice orest[1U];
                memcpy(orest, uu____5.snd, (size_t)1U * sizeof(Eurydice_slice));
                libcrux_sha3_generic_keccak_squeeze_next_block_c2(&s, o);
                memcpy(o1, orest, (size_t)1U * sizeof(Eurydice_slice));
            }
        }
        if (last < outlen) {
            libcrux_sha3_generic_keccak_squeeze_last_ca3(s, o1);
        }
    }
}

/**
A monomorphic instance of libcrux_sha3.portable.keccakx1
with const generics
- RATE= 168
- DELIM= 31
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccakx1_e44(
    Eurydice_slice data[1U], Eurydice_slice out[1U])
{
    /* Passing arrays by value in Rust generates a copy in C */
    Eurydice_slice copy_of_data[1U];
    memcpy(copy_of_data, data, (size_t)1U * sizeof(Eurydice_slice));
    libcrux_sha3_generic_keccak_keccak_064(copy_of_data, out);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.load_block
with const generics
- RATE= 104
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_load_block_653(
    uint64_t (*s)[5U], Eurydice_slice blocks[1U])
{
    for (size_t i = (size_t)0U; i < (size_t)104U / (size_t)8U; i++) {
        size_t i0 = i;
        uint8_t uu____0[8U];
        core_result_Result_56 dst;
        Eurydice_slice_to_array2(
            &dst,
            Eurydice_slice_subslice2(blocks[0U], (size_t)8U * i0,
                                     (size_t)8U * i0 + (size_t)8U, uint8_t),
            Eurydice_slice, uint8_t[8U]);
        core_result_unwrap_41_0e(dst, uu____0);
        size_t uu____1 = i0 / (size_t)5U;
        size_t uu____2 = i0 % (size_t)5U;
        s[uu____1][uu____2] =
            s[uu____1][uu____2] ^ core_num__u64_9__from_le_bytes(uu____0);
    }
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.load_block_5a
with const generics
- RATE= 104
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_load_block_5a_353(
    uint64_t (*a)[5U], Eurydice_slice b[1U])
{
    uint64_t(*uu____0)[5U] = a;
    /* Passing arrays by value in Rust generates a copy in C */
    Eurydice_slice copy_of_b[1U];
    memcpy(copy_of_b, b, (size_t)1U * sizeof(Eurydice_slice));
    libcrux_sha3_portable_keccak_load_block_653(uu____0, copy_of_b);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.absorb_block
with types uint64_t
with const generics
- N= 1
- RATE= 104
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_absorb_block_402(
    libcrux_sha3_generic_keccak_KeccakState_48 *s, Eurydice_slice blocks[1U])
{
    uint64_t(*uu____0)[5U] = s->st;
    Eurydice_slice uu____1[1U];
    memcpy(uu____1, blocks, (size_t)1U * sizeof(Eurydice_slice));
    libcrux_sha3_portable_keccak_load_block_5a_353(uu____0, uu____1);
    libcrux_sha3_generic_keccak_keccakf1600_b8(s);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.load_block_full
with const generics
- RATE= 104
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_load_block_full_d43(
    uint64_t (*s)[5U], uint8_t blocks[1U][200U])
{
    Eurydice_slice buf[1U] = {
        Eurydice_array_to_slice((size_t)200U, blocks[0U], uint8_t)
    };
    libcrux_sha3_portable_keccak_load_block_653(s, buf);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.load_block_full_5a
with const generics
- RATE= 104
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_load_block_full_5a_053(
    uint64_t (*a)[5U], uint8_t b[1U][200U])
{
    uint64_t(*uu____0)[5U] = a;
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_b[1U][200U];
    memcpy(copy_of_b, b, (size_t)1U * sizeof(uint8_t[200U]));
    libcrux_sha3_portable_keccak_load_block_full_d43(uu____0, copy_of_b);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.absorb_final
with types uint64_t
with const generics
- N= 1
- RATE= 104
- DELIM= 6
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_absorb_final_404(
    libcrux_sha3_generic_keccak_KeccakState_48 *s, Eurydice_slice last[1U])
{
    size_t last_len = Eurydice_slice_len(last[0U], uint8_t);
    uint8_t blocks[1U][200U] = { { 0U } };
    {
        size_t i = (size_t)0U;
        if (last_len > (size_t)0U) {
            Eurydice_slice uu____0 =
                Eurydice_array_to_subslice2(blocks[i], (size_t)0U, last_len, uint8_t);
            Eurydice_slice_copy(uu____0, last[i], uint8_t);
        }
        blocks[i][last_len] = 6U;
        size_t uu____1 = i;
        size_t uu____2 = (size_t)104U - (size_t)1U;
        blocks[uu____1][uu____2] = (uint32_t)blocks[uu____1][uu____2] | 128U;
    }
    uint64_t(*uu____3)[5U] = s->st;
    uint8_t uu____4[1U][200U];
    memcpy(uu____4, blocks, (size_t)1U * sizeof(uint8_t[200U]));
    libcrux_sha3_portable_keccak_load_block_full_5a_053(uu____3, uu____4);
    libcrux_sha3_generic_keccak_keccakf1600_b8(s);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.store_block
with const generics
- RATE= 104
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_store_block_9b3(
    uint64_t (*s)[5U], Eurydice_slice out[1U])
{
    for (size_t i = (size_t)0U; i < (size_t)104U / (size_t)8U; i++) {
        size_t i0 = i;
        Eurydice_slice uu____0 = Eurydice_slice_subslice2(
            out[0U], (size_t)8U * i0, (size_t)8U * i0 + (size_t)8U, uint8_t);
        uint8_t ret[8U];
        core_num__u64_9__to_le_bytes(s[i0 / (size_t)5U][i0 % (size_t)5U], ret);
        Eurydice_slice_copy(
            uu____0, Eurydice_array_to_slice((size_t)8U, ret, uint8_t), uint8_t);
    }
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.store_block_full
with const generics
- RATE= 104
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_store_block_full_7e2(
    uint64_t (*s)[5U], uint8_t ret[1U][200U])
{
    uint8_t out[200U] = { 0U };
    Eurydice_slice buf[1U] = {
        Eurydice_array_to_slice((size_t)200U, out, uint8_t)
    };
    libcrux_sha3_portable_keccak_store_block_9b3(s, buf);
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_out[200U];
    memcpy(copy_of_out, out, (size_t)200U * sizeof(uint8_t));
    memcpy(ret[0U], copy_of_out, (size_t)200U * sizeof(uint8_t));
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.store_block_full_5a
with const generics
- RATE= 104
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_store_block_full_5a_272(uint64_t (*a)[5U],
                                                     uint8_t ret[1U][200U])
{
    libcrux_sha3_portable_keccak_store_block_full_7e2(a, ret);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.squeeze_first_and_last
with types uint64_t
with const generics
- N= 1
- RATE= 104
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_squeeze_first_and_last_882(
    libcrux_sha3_generic_keccak_KeccakState_48 *s, Eurydice_slice out[1U])
{
    uint8_t b[1U][200U];
    libcrux_sha3_portable_keccak_store_block_full_5a_272(s->st, b);
    {
        size_t i = (size_t)0U;
        Eurydice_slice uu____0 = out[i];
        uint8_t *uu____1 = b[i];
        core_ops_range_Range_b3 lit;
        lit.start = (size_t)0U;
        lit.end = Eurydice_slice_len(out[i], uint8_t);
        Eurydice_slice_copy(
            uu____0,
            Eurydice_array_to_subslice((size_t)200U, uu____1, lit, uint8_t,
                                       core_ops_range_Range_b3),
            uint8_t);
    }
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.store_block_5a
with const generics
- RATE= 104
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_store_block_5a_493(
    uint64_t (*a)[5U], Eurydice_slice b[1U])
{
    libcrux_sha3_portable_keccak_store_block_9b3(a, b);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.squeeze_first_block
with types uint64_t
with const generics
- N= 1
- RATE= 104
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_squeeze_first_block_7b3(
    libcrux_sha3_generic_keccak_KeccakState_48 *s, Eurydice_slice out[1U])
{
    libcrux_sha3_portable_keccak_store_block_5a_493(s->st, out);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.squeeze_next_block
with types uint64_t
with const generics
- N= 1
- RATE= 104
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_squeeze_next_block_c23(
    libcrux_sha3_generic_keccak_KeccakState_48 *s, Eurydice_slice out[1U])
{
    libcrux_sha3_generic_keccak_keccakf1600_b8(s);
    libcrux_sha3_portable_keccak_store_block_5a_493(s->st, out);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.squeeze_last
with types uint64_t
with const generics
- N= 1
- RATE= 104
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_squeeze_last_ca2(
    libcrux_sha3_generic_keccak_KeccakState_48 s, Eurydice_slice out[1U])
{
    libcrux_sha3_generic_keccak_keccakf1600_b8(&s);
    uint8_t b[1U][200U];
    libcrux_sha3_portable_keccak_store_block_full_5a_272(s.st, b);
    {
        size_t i = (size_t)0U;
        Eurydice_slice uu____0 = out[i];
        uint8_t *uu____1 = b[i];
        core_ops_range_Range_b3 lit;
        lit.start = (size_t)0U;
        lit.end = Eurydice_slice_len(out[i], uint8_t);
        Eurydice_slice_copy(
            uu____0,
            Eurydice_array_to_subslice((size_t)200U, uu____1, lit, uint8_t,
                                       core_ops_range_Range_b3),
            uint8_t);
    }
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.keccak
with types uint64_t
with const generics
- N= 1
- RATE= 104
- DELIM= 6
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_keccak_063(
    Eurydice_slice data[1U], Eurydice_slice out[1U])
{
    libcrux_sha3_generic_keccak_KeccakState_48 s =
        libcrux_sha3_generic_keccak_new_1e_cf();
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(data[0U], uint8_t) / (size_t)104U; i++) {
        size_t i0 = i;
        libcrux_sha3_generic_keccak_KeccakState_48 *uu____0 = &s;
        /* Passing arrays by value in Rust generates a copy in C */
        Eurydice_slice copy_of_data[1U];
        memcpy(copy_of_data, data, (size_t)1U * sizeof(Eurydice_slice));
        Eurydice_slice ret[1U];
        libcrux_sha3_portable_keccak_slice_n_5a(copy_of_data, i0 * (size_t)104U,
                                                (size_t)104U, ret);
        libcrux_sha3_generic_keccak_absorb_block_402(uu____0, ret);
    }
    size_t rem = Eurydice_slice_len(data[0U], uint8_t) % (size_t)104U;
    libcrux_sha3_generic_keccak_KeccakState_48 *uu____2 = &s;
    /* Passing arrays by value in Rust generates a copy in C */
    Eurydice_slice copy_of_data[1U];
    memcpy(copy_of_data, data, (size_t)1U * sizeof(Eurydice_slice));
    Eurydice_slice ret[1U];
    libcrux_sha3_portable_keccak_slice_n_5a(
        copy_of_data, Eurydice_slice_len(data[0U], uint8_t) - rem, rem, ret);
    libcrux_sha3_generic_keccak_absorb_final_404(uu____2, ret);
    size_t outlen = Eurydice_slice_len(out[0U], uint8_t);
    size_t blocks = outlen / (size_t)104U;
    size_t last = outlen - outlen % (size_t)104U;
    if (blocks == (size_t)0U) {
        libcrux_sha3_generic_keccak_squeeze_first_and_last_882(&s, out);
    } else {
        Eurydice_slice_uint8_t_1size_t__x2 uu____4 =
            libcrux_sha3_portable_keccak_split_at_mut_n_5a(out, (size_t)104U);
        Eurydice_slice o0[1U];
        memcpy(o0, uu____4.fst, (size_t)1U * sizeof(Eurydice_slice));
        Eurydice_slice o1[1U];
        memcpy(o1, uu____4.snd, (size_t)1U * sizeof(Eurydice_slice));
        libcrux_sha3_generic_keccak_squeeze_first_block_7b3(&s, o0);
        core_ops_range_Range_b3 iter =
            core_iter_traits_collect___core__iter__traits__collect__IntoIterator_for_I__1__into_iter(
                (CLITERAL(core_ops_range_Range_b3){ .start = (size_t)1U,
                                                    .end = blocks }),
                core_ops_range_Range_b3, core_ops_range_Range_b3);
        while (true) {
            if (core_iter_range___core__iter__traits__iterator__Iterator_for_core__ops__range__Range_A___6__next(
                    &iter, size_t, core_option_Option_b3)
                    .tag == core_option_None) {
                break;
            } else {
                Eurydice_slice_uint8_t_1size_t__x2 uu____5 =
                    libcrux_sha3_portable_keccak_split_at_mut_n_5a(o1, (size_t)104U);
                Eurydice_slice o[1U];
                memcpy(o, uu____5.fst, (size_t)1U * sizeof(Eurydice_slice));
                Eurydice_slice orest[1U];
                memcpy(orest, uu____5.snd, (size_t)1U * sizeof(Eurydice_slice));
                libcrux_sha3_generic_keccak_squeeze_next_block_c23(&s, o);
                memcpy(o1, orest, (size_t)1U * sizeof(Eurydice_slice));
            }
        }
        if (last < outlen) {
            libcrux_sha3_generic_keccak_squeeze_last_ca2(s, o1);
        }
    }
}

/**
A monomorphic instance of libcrux_sha3.portable.keccakx1
with const generics
- RATE= 104
- DELIM= 6
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccakx1_e43(
    Eurydice_slice data[1U], Eurydice_slice out[1U])
{
    /* Passing arrays by value in Rust generates a copy in C */
    Eurydice_slice copy_of_data[1U];
    memcpy(copy_of_data, data, (size_t)1U * sizeof(Eurydice_slice));
    libcrux_sha3_generic_keccak_keccak_063(copy_of_data, out);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.load_block
with const generics
- RATE= 144
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_load_block_652(
    uint64_t (*s)[5U], Eurydice_slice blocks[1U])
{
    for (size_t i = (size_t)0U; i < (size_t)144U / (size_t)8U; i++) {
        size_t i0 = i;
        uint8_t uu____0[8U];
        core_result_Result_56 dst;
        Eurydice_slice_to_array2(
            &dst,
            Eurydice_slice_subslice2(blocks[0U], (size_t)8U * i0,
                                     (size_t)8U * i0 + (size_t)8U, uint8_t),
            Eurydice_slice, uint8_t[8U]);
        core_result_unwrap_41_0e(dst, uu____0);
        size_t uu____1 = i0 / (size_t)5U;
        size_t uu____2 = i0 % (size_t)5U;
        s[uu____1][uu____2] =
            s[uu____1][uu____2] ^ core_num__u64_9__from_le_bytes(uu____0);
    }
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.load_block_5a
with const generics
- RATE= 144
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_load_block_5a_352(
    uint64_t (*a)[5U], Eurydice_slice b[1U])
{
    uint64_t(*uu____0)[5U] = a;
    /* Passing arrays by value in Rust generates a copy in C */
    Eurydice_slice copy_of_b[1U];
    memcpy(copy_of_b, b, (size_t)1U * sizeof(Eurydice_slice));
    libcrux_sha3_portable_keccak_load_block_652(uu____0, copy_of_b);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.absorb_block
with types uint64_t
with const generics
- N= 1
- RATE= 144
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_absorb_block_401(
    libcrux_sha3_generic_keccak_KeccakState_48 *s, Eurydice_slice blocks[1U])
{
    uint64_t(*uu____0)[5U] = s->st;
    Eurydice_slice uu____1[1U];
    memcpy(uu____1, blocks, (size_t)1U * sizeof(Eurydice_slice));
    libcrux_sha3_portable_keccak_load_block_5a_352(uu____0, uu____1);
    libcrux_sha3_generic_keccak_keccakf1600_b8(s);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.load_block_full
with const generics
- RATE= 144
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_load_block_full_d42(
    uint64_t (*s)[5U], uint8_t blocks[1U][200U])
{
    Eurydice_slice buf[1U] = {
        Eurydice_array_to_slice((size_t)200U, blocks[0U], uint8_t)
    };
    libcrux_sha3_portable_keccak_load_block_652(s, buf);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.load_block_full_5a
with const generics
- RATE= 144
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_load_block_full_5a_052(
    uint64_t (*a)[5U], uint8_t b[1U][200U])
{
    uint64_t(*uu____0)[5U] = a;
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_b[1U][200U];
    memcpy(copy_of_b, b, (size_t)1U * sizeof(uint8_t[200U]));
    libcrux_sha3_portable_keccak_load_block_full_d42(uu____0, copy_of_b);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.absorb_final
with types uint64_t
with const generics
- N= 1
- RATE= 144
- DELIM= 6
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_absorb_final_403(
    libcrux_sha3_generic_keccak_KeccakState_48 *s, Eurydice_slice last[1U])
{
    size_t last_len = Eurydice_slice_len(last[0U], uint8_t);
    uint8_t blocks[1U][200U] = { { 0U } };
    {
        size_t i = (size_t)0U;
        if (last_len > (size_t)0U) {
            Eurydice_slice uu____0 =
                Eurydice_array_to_subslice2(blocks[i], (size_t)0U, last_len, uint8_t);
            Eurydice_slice_copy(uu____0, last[i], uint8_t);
        }
        blocks[i][last_len] = 6U;
        size_t uu____1 = i;
        size_t uu____2 = (size_t)144U - (size_t)1U;
        blocks[uu____1][uu____2] = (uint32_t)blocks[uu____1][uu____2] | 128U;
    }
    uint64_t(*uu____3)[5U] = s->st;
    uint8_t uu____4[1U][200U];
    memcpy(uu____4, blocks, (size_t)1U * sizeof(uint8_t[200U]));
    libcrux_sha3_portable_keccak_load_block_full_5a_052(uu____3, uu____4);
    libcrux_sha3_generic_keccak_keccakf1600_b8(s);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.store_block
with const generics
- RATE= 144
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_store_block_9b2(
    uint64_t (*s)[5U], Eurydice_slice out[1U])
{
    for (size_t i = (size_t)0U; i < (size_t)144U / (size_t)8U; i++) {
        size_t i0 = i;
        Eurydice_slice uu____0 = Eurydice_slice_subslice2(
            out[0U], (size_t)8U * i0, (size_t)8U * i0 + (size_t)8U, uint8_t);
        uint8_t ret[8U];
        core_num__u64_9__to_le_bytes(s[i0 / (size_t)5U][i0 % (size_t)5U], ret);
        Eurydice_slice_copy(
            uu____0, Eurydice_array_to_slice((size_t)8U, ret, uint8_t), uint8_t);
    }
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.store_block_full
with const generics
- RATE= 144
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_store_block_full_7e1(
    uint64_t (*s)[5U], uint8_t ret[1U][200U])
{
    uint8_t out[200U] = { 0U };
    Eurydice_slice buf[1U] = {
        Eurydice_array_to_slice((size_t)200U, out, uint8_t)
    };
    libcrux_sha3_portable_keccak_store_block_9b2(s, buf);
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_out[200U];
    memcpy(copy_of_out, out, (size_t)200U * sizeof(uint8_t));
    memcpy(ret[0U], copy_of_out, (size_t)200U * sizeof(uint8_t));
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.store_block_full_5a
with const generics
- RATE= 144
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_store_block_full_5a_271(uint64_t (*a)[5U],
                                                     uint8_t ret[1U][200U])
{
    libcrux_sha3_portable_keccak_store_block_full_7e1(a, ret);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.squeeze_first_and_last
with types uint64_t
with const generics
- N= 1
- RATE= 144
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_squeeze_first_and_last_881(
    libcrux_sha3_generic_keccak_KeccakState_48 *s, Eurydice_slice out[1U])
{
    uint8_t b[1U][200U];
    libcrux_sha3_portable_keccak_store_block_full_5a_271(s->st, b);
    {
        size_t i = (size_t)0U;
        Eurydice_slice uu____0 = out[i];
        uint8_t *uu____1 = b[i];
        core_ops_range_Range_b3 lit;
        lit.start = (size_t)0U;
        lit.end = Eurydice_slice_len(out[i], uint8_t);
        Eurydice_slice_copy(
            uu____0,
            Eurydice_array_to_subslice((size_t)200U, uu____1, lit, uint8_t,
                                       core_ops_range_Range_b3),
            uint8_t);
    }
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.store_block_5a
with const generics
- RATE= 144
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_store_block_5a_492(
    uint64_t (*a)[5U], Eurydice_slice b[1U])
{
    libcrux_sha3_portable_keccak_store_block_9b2(a, b);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.squeeze_first_block
with types uint64_t
with const generics
- N= 1
- RATE= 144
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_squeeze_first_block_7b2(
    libcrux_sha3_generic_keccak_KeccakState_48 *s, Eurydice_slice out[1U])
{
    libcrux_sha3_portable_keccak_store_block_5a_492(s->st, out);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.squeeze_next_block
with types uint64_t
with const generics
- N= 1
- RATE= 144
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_squeeze_next_block_c22(
    libcrux_sha3_generic_keccak_KeccakState_48 *s, Eurydice_slice out[1U])
{
    libcrux_sha3_generic_keccak_keccakf1600_b8(s);
    libcrux_sha3_portable_keccak_store_block_5a_492(s->st, out);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.squeeze_last
with types uint64_t
with const generics
- N= 1
- RATE= 144
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_squeeze_last_ca1(
    libcrux_sha3_generic_keccak_KeccakState_48 s, Eurydice_slice out[1U])
{
    libcrux_sha3_generic_keccak_keccakf1600_b8(&s);
    uint8_t b[1U][200U];
    libcrux_sha3_portable_keccak_store_block_full_5a_271(s.st, b);
    {
        size_t i = (size_t)0U;
        Eurydice_slice uu____0 = out[i];
        uint8_t *uu____1 = b[i];
        core_ops_range_Range_b3 lit;
        lit.start = (size_t)0U;
        lit.end = Eurydice_slice_len(out[i], uint8_t);
        Eurydice_slice_copy(
            uu____0,
            Eurydice_array_to_subslice((size_t)200U, uu____1, lit, uint8_t,
                                       core_ops_range_Range_b3),
            uint8_t);
    }
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.keccak
with types uint64_t
with const generics
- N= 1
- RATE= 144
- DELIM= 6
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_keccak_062(
    Eurydice_slice data[1U], Eurydice_slice out[1U])
{
    libcrux_sha3_generic_keccak_KeccakState_48 s =
        libcrux_sha3_generic_keccak_new_1e_cf();
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(data[0U], uint8_t) / (size_t)144U; i++) {
        size_t i0 = i;
        libcrux_sha3_generic_keccak_KeccakState_48 *uu____0 = &s;
        /* Passing arrays by value in Rust generates a copy in C */
        Eurydice_slice copy_of_data[1U];
        memcpy(copy_of_data, data, (size_t)1U * sizeof(Eurydice_slice));
        Eurydice_slice ret[1U];
        libcrux_sha3_portable_keccak_slice_n_5a(copy_of_data, i0 * (size_t)144U,
                                                (size_t)144U, ret);
        libcrux_sha3_generic_keccak_absorb_block_401(uu____0, ret);
    }
    size_t rem = Eurydice_slice_len(data[0U], uint8_t) % (size_t)144U;
    libcrux_sha3_generic_keccak_KeccakState_48 *uu____2 = &s;
    /* Passing arrays by value in Rust generates a copy in C */
    Eurydice_slice copy_of_data[1U];
    memcpy(copy_of_data, data, (size_t)1U * sizeof(Eurydice_slice));
    Eurydice_slice ret[1U];
    libcrux_sha3_portable_keccak_slice_n_5a(
        copy_of_data, Eurydice_slice_len(data[0U], uint8_t) - rem, rem, ret);
    libcrux_sha3_generic_keccak_absorb_final_403(uu____2, ret);
    size_t outlen = Eurydice_slice_len(out[0U], uint8_t);
    size_t blocks = outlen / (size_t)144U;
    size_t last = outlen - outlen % (size_t)144U;
    if (blocks == (size_t)0U) {
        libcrux_sha3_generic_keccak_squeeze_first_and_last_881(&s, out);
    } else {
        Eurydice_slice_uint8_t_1size_t__x2 uu____4 =
            libcrux_sha3_portable_keccak_split_at_mut_n_5a(out, (size_t)144U);
        Eurydice_slice o0[1U];
        memcpy(o0, uu____4.fst, (size_t)1U * sizeof(Eurydice_slice));
        Eurydice_slice o1[1U];
        memcpy(o1, uu____4.snd, (size_t)1U * sizeof(Eurydice_slice));
        libcrux_sha3_generic_keccak_squeeze_first_block_7b2(&s, o0);
        core_ops_range_Range_b3 iter =
            core_iter_traits_collect___core__iter__traits__collect__IntoIterator_for_I__1__into_iter(
                (CLITERAL(core_ops_range_Range_b3){ .start = (size_t)1U,
                                                    .end = blocks }),
                core_ops_range_Range_b3, core_ops_range_Range_b3);
        while (true) {
            if (core_iter_range___core__iter__traits__iterator__Iterator_for_core__ops__range__Range_A___6__next(
                    &iter, size_t, core_option_Option_b3)
                    .tag == core_option_None) {
                break;
            } else {
                Eurydice_slice_uint8_t_1size_t__x2 uu____5 =
                    libcrux_sha3_portable_keccak_split_at_mut_n_5a(o1, (size_t)144U);
                Eurydice_slice o[1U];
                memcpy(o, uu____5.fst, (size_t)1U * sizeof(Eurydice_slice));
                Eurydice_slice orest[1U];
                memcpy(orest, uu____5.snd, (size_t)1U * sizeof(Eurydice_slice));
                libcrux_sha3_generic_keccak_squeeze_next_block_c22(&s, o);
                memcpy(o1, orest, (size_t)1U * sizeof(Eurydice_slice));
            }
        }
        if (last < outlen) {
            libcrux_sha3_generic_keccak_squeeze_last_ca1(s, o1);
        }
    }
}

/**
A monomorphic instance of libcrux_sha3.portable.keccakx1
with const generics
- RATE= 144
- DELIM= 6
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccakx1_e42(
    Eurydice_slice data[1U], Eurydice_slice out[1U])
{
    /* Passing arrays by value in Rust generates a copy in C */
    Eurydice_slice copy_of_data[1U];
    memcpy(copy_of_data, data, (size_t)1U * sizeof(Eurydice_slice));
    libcrux_sha3_generic_keccak_keccak_062(copy_of_data, out);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.absorb_block
with types uint64_t
with const generics
- N= 1
- RATE= 136
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_absorb_block_400(
    libcrux_sha3_generic_keccak_KeccakState_48 *s, Eurydice_slice blocks[1U])
{
    uint64_t(*uu____0)[5U] = s->st;
    Eurydice_slice uu____1[1U];
    memcpy(uu____1, blocks, (size_t)1U * sizeof(Eurydice_slice));
    libcrux_sha3_portable_keccak_load_block_5a_35(uu____0, uu____1);
    libcrux_sha3_generic_keccak_keccakf1600_b8(s);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.store_block_full
with const generics
- RATE= 136
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_store_block_full_7e0(
    uint64_t (*s)[5U], uint8_t ret[1U][200U])
{
    uint8_t out[200U] = { 0U };
    Eurydice_slice buf[1U] = {
        Eurydice_array_to_slice((size_t)200U, out, uint8_t)
    };
    libcrux_sha3_portable_keccak_store_block_9b0(s, buf);
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_out[200U];
    memcpy(copy_of_out, out, (size_t)200U * sizeof(uint8_t));
    memcpy(ret[0U], copy_of_out, (size_t)200U * sizeof(uint8_t));
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.store_block_full_5a
with const generics
- RATE= 136
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_store_block_full_5a_270(uint64_t (*a)[5U],
                                                     uint8_t ret[1U][200U])
{
    libcrux_sha3_portable_keccak_store_block_full_7e0(a, ret);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.squeeze_first_and_last
with types uint64_t
with const generics
- N= 1
- RATE= 136
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_squeeze_first_and_last_880(
    libcrux_sha3_generic_keccak_KeccakState_48 *s, Eurydice_slice out[1U])
{
    uint8_t b[1U][200U];
    libcrux_sha3_portable_keccak_store_block_full_5a_270(s->st, b);
    {
        size_t i = (size_t)0U;
        Eurydice_slice uu____0 = out[i];
        uint8_t *uu____1 = b[i];
        core_ops_range_Range_b3 lit;
        lit.start = (size_t)0U;
        lit.end = Eurydice_slice_len(out[i], uint8_t);
        Eurydice_slice_copy(
            uu____0,
            Eurydice_array_to_subslice((size_t)200U, uu____1, lit, uint8_t,
                                       core_ops_range_Range_b3),
            uint8_t);
    }
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.squeeze_last
with types uint64_t
with const generics
- N= 1
- RATE= 136
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_squeeze_last_ca0(
    libcrux_sha3_generic_keccak_KeccakState_48 s, Eurydice_slice out[1U])
{
    libcrux_sha3_generic_keccak_keccakf1600_b8(&s);
    uint8_t b[1U][200U];
    libcrux_sha3_portable_keccak_store_block_full_5a_270(s.st, b);
    {
        size_t i = (size_t)0U;
        Eurydice_slice uu____0 = out[i];
        uint8_t *uu____1 = b[i];
        core_ops_range_Range_b3 lit;
        lit.start = (size_t)0U;
        lit.end = Eurydice_slice_len(out[i], uint8_t);
        Eurydice_slice_copy(
            uu____0,
            Eurydice_array_to_subslice((size_t)200U, uu____1, lit, uint8_t,
                                       core_ops_range_Range_b3),
            uint8_t);
    }
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.keccak
with types uint64_t
with const generics
- N= 1
- RATE= 136
- DELIM= 31
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_keccak_061(
    Eurydice_slice data[1U], Eurydice_slice out[1U])
{
    libcrux_sha3_generic_keccak_KeccakState_48 s =
        libcrux_sha3_generic_keccak_new_1e_cf();
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(data[0U], uint8_t) / (size_t)136U; i++) {
        size_t i0 = i;
        libcrux_sha3_generic_keccak_KeccakState_48 *uu____0 = &s;
        /* Passing arrays by value in Rust generates a copy in C */
        Eurydice_slice copy_of_data[1U];
        memcpy(copy_of_data, data, (size_t)1U * sizeof(Eurydice_slice));
        Eurydice_slice ret[1U];
        libcrux_sha3_portable_keccak_slice_n_5a(copy_of_data, i0 * (size_t)136U,
                                                (size_t)136U, ret);
        libcrux_sha3_generic_keccak_absorb_block_400(uu____0, ret);
    }
    size_t rem = Eurydice_slice_len(data[0U], uint8_t) % (size_t)136U;
    libcrux_sha3_generic_keccak_KeccakState_48 *uu____2 = &s;
    /* Passing arrays by value in Rust generates a copy in C */
    Eurydice_slice copy_of_data[1U];
    memcpy(copy_of_data, data, (size_t)1U * sizeof(Eurydice_slice));
    Eurydice_slice ret[1U];
    libcrux_sha3_portable_keccak_slice_n_5a(
        copy_of_data, Eurydice_slice_len(data[0U], uint8_t) - rem, rem, ret);
    libcrux_sha3_generic_keccak_absorb_final_400(uu____2, ret);
    size_t outlen = Eurydice_slice_len(out[0U], uint8_t);
    size_t blocks = outlen / (size_t)136U;
    size_t last = outlen - outlen % (size_t)136U;
    if (blocks == (size_t)0U) {
        libcrux_sha3_generic_keccak_squeeze_first_and_last_880(&s, out);
    } else {
        Eurydice_slice_uint8_t_1size_t__x2 uu____4 =
            libcrux_sha3_portable_keccak_split_at_mut_n_5a(out, (size_t)136U);
        Eurydice_slice o0[1U];
        memcpy(o0, uu____4.fst, (size_t)1U * sizeof(Eurydice_slice));
        Eurydice_slice o1[1U];
        memcpy(o1, uu____4.snd, (size_t)1U * sizeof(Eurydice_slice));
        libcrux_sha3_generic_keccak_squeeze_first_block_7b0(&s, o0);
        core_ops_range_Range_b3 iter =
            core_iter_traits_collect___core__iter__traits__collect__IntoIterator_for_I__1__into_iter(
                (CLITERAL(core_ops_range_Range_b3){ .start = (size_t)1U,
                                                    .end = blocks }),
                core_ops_range_Range_b3, core_ops_range_Range_b3);
        while (true) {
            if (core_iter_range___core__iter__traits__iterator__Iterator_for_core__ops__range__Range_A___6__next(
                    &iter, size_t, core_option_Option_b3)
                    .tag == core_option_None) {
                break;
            } else {
                Eurydice_slice_uint8_t_1size_t__x2 uu____5 =
                    libcrux_sha3_portable_keccak_split_at_mut_n_5a(o1, (size_t)136U);
                Eurydice_slice o[1U];
                memcpy(o, uu____5.fst, (size_t)1U * sizeof(Eurydice_slice));
                Eurydice_slice orest[1U];
                memcpy(orest, uu____5.snd, (size_t)1U * sizeof(Eurydice_slice));
                libcrux_sha3_generic_keccak_squeeze_next_block_c20(&s, o);
                memcpy(o1, orest, (size_t)1U * sizeof(Eurydice_slice));
            }
        }
        if (last < outlen) {
            libcrux_sha3_generic_keccak_squeeze_last_ca0(s, o1);
        }
    }
}

/**
A monomorphic instance of libcrux_sha3.portable.keccakx1
with const generics
- RATE= 136
- DELIM= 31
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccakx1_e41(
    Eurydice_slice data[1U], Eurydice_slice out[1U])
{
    /* Passing arrays by value in Rust generates a copy in C */
    Eurydice_slice copy_of_data[1U];
    memcpy(copy_of_data, data, (size_t)1U * sizeof(Eurydice_slice));
    libcrux_sha3_generic_keccak_keccak_061(copy_of_data, out);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.absorb_final
with types uint64_t
with const generics
- N= 1
- RATE= 136
- DELIM= 6
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_absorb_final_402(
    libcrux_sha3_generic_keccak_KeccakState_48 *s, Eurydice_slice last[1U])
{
    size_t last_len = Eurydice_slice_len(last[0U], uint8_t);
    uint8_t blocks[1U][200U] = { { 0U } };
    {
        size_t i = (size_t)0U;
        if (last_len > (size_t)0U) {
            Eurydice_slice uu____0 =
                Eurydice_array_to_subslice2(blocks[i], (size_t)0U, last_len, uint8_t);
            Eurydice_slice_copy(uu____0, last[i], uint8_t);
        }
        blocks[i][last_len] = 6U;
        size_t uu____1 = i;
        size_t uu____2 = (size_t)136U - (size_t)1U;
        blocks[uu____1][uu____2] = (uint32_t)blocks[uu____1][uu____2] | 128U;
    }
    uint64_t(*uu____3)[5U] = s->st;
    uint8_t uu____4[1U][200U];
    memcpy(uu____4, blocks, (size_t)1U * sizeof(uint8_t[200U]));
    libcrux_sha3_portable_keccak_load_block_full_5a_050(uu____3, uu____4);
    libcrux_sha3_generic_keccak_keccakf1600_b8(s);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.keccak
with types uint64_t
with const generics
- N= 1
- RATE= 136
- DELIM= 6
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_keccak_060(
    Eurydice_slice data[1U], Eurydice_slice out[1U])
{
    libcrux_sha3_generic_keccak_KeccakState_48 s =
        libcrux_sha3_generic_keccak_new_1e_cf();
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(data[0U], uint8_t) / (size_t)136U; i++) {
        size_t i0 = i;
        libcrux_sha3_generic_keccak_KeccakState_48 *uu____0 = &s;
        /* Passing arrays by value in Rust generates a copy in C */
        Eurydice_slice copy_of_data[1U];
        memcpy(copy_of_data, data, (size_t)1U * sizeof(Eurydice_slice));
        Eurydice_slice ret[1U];
        libcrux_sha3_portable_keccak_slice_n_5a(copy_of_data, i0 * (size_t)136U,
                                                (size_t)136U, ret);
        libcrux_sha3_generic_keccak_absorb_block_400(uu____0, ret);
    }
    size_t rem = Eurydice_slice_len(data[0U], uint8_t) % (size_t)136U;
    libcrux_sha3_generic_keccak_KeccakState_48 *uu____2 = &s;
    /* Passing arrays by value in Rust generates a copy in C */
    Eurydice_slice copy_of_data[1U];
    memcpy(copy_of_data, data, (size_t)1U * sizeof(Eurydice_slice));
    Eurydice_slice ret[1U];
    libcrux_sha3_portable_keccak_slice_n_5a(
        copy_of_data, Eurydice_slice_len(data[0U], uint8_t) - rem, rem, ret);
    libcrux_sha3_generic_keccak_absorb_final_402(uu____2, ret);
    size_t outlen = Eurydice_slice_len(out[0U], uint8_t);
    size_t blocks = outlen / (size_t)136U;
    size_t last = outlen - outlen % (size_t)136U;
    if (blocks == (size_t)0U) {
        libcrux_sha3_generic_keccak_squeeze_first_and_last_880(&s, out);
    } else {
        Eurydice_slice_uint8_t_1size_t__x2 uu____4 =
            libcrux_sha3_portable_keccak_split_at_mut_n_5a(out, (size_t)136U);
        Eurydice_slice o0[1U];
        memcpy(o0, uu____4.fst, (size_t)1U * sizeof(Eurydice_slice));
        Eurydice_slice o1[1U];
        memcpy(o1, uu____4.snd, (size_t)1U * sizeof(Eurydice_slice));
        libcrux_sha3_generic_keccak_squeeze_first_block_7b0(&s, o0);
        core_ops_range_Range_b3 iter =
            core_iter_traits_collect___core__iter__traits__collect__IntoIterator_for_I__1__into_iter(
                (CLITERAL(core_ops_range_Range_b3){ .start = (size_t)1U,
                                                    .end = blocks }),
                core_ops_range_Range_b3, core_ops_range_Range_b3);
        while (true) {
            if (core_iter_range___core__iter__traits__iterator__Iterator_for_core__ops__range__Range_A___6__next(
                    &iter, size_t, core_option_Option_b3)
                    .tag == core_option_None) {
                break;
            } else {
                Eurydice_slice_uint8_t_1size_t__x2 uu____5 =
                    libcrux_sha3_portable_keccak_split_at_mut_n_5a(o1, (size_t)136U);
                Eurydice_slice o[1U];
                memcpy(o, uu____5.fst, (size_t)1U * sizeof(Eurydice_slice));
                Eurydice_slice orest[1U];
                memcpy(orest, uu____5.snd, (size_t)1U * sizeof(Eurydice_slice));
                libcrux_sha3_generic_keccak_squeeze_next_block_c20(&s, o);
                memcpy(o1, orest, (size_t)1U * sizeof(Eurydice_slice));
            }
        }
        if (last < outlen) {
            libcrux_sha3_generic_keccak_squeeze_last_ca0(s, o1);
        }
    }
}

/**
A monomorphic instance of libcrux_sha3.portable.keccakx1
with const generics
- RATE= 136
- DELIM= 6
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccakx1_e40(
    Eurydice_slice data[1U], Eurydice_slice out[1U])
{
    /* Passing arrays by value in Rust generates a copy in C */
    Eurydice_slice copy_of_data[1U];
    memcpy(copy_of_data, data, (size_t)1U * sizeof(Eurydice_slice));
    libcrux_sha3_generic_keccak_keccak_060(copy_of_data, out);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.load_block
with const generics
- RATE= 72
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_load_block_651(
    uint64_t (*s)[5U], Eurydice_slice blocks[1U])
{
    for (size_t i = (size_t)0U; i < (size_t)72U / (size_t)8U; i++) {
        size_t i0 = i;
        uint8_t uu____0[8U];
        core_result_Result_56 dst;
        Eurydice_slice_to_array2(
            &dst,
            Eurydice_slice_subslice2(blocks[0U], (size_t)8U * i0,
                                     (size_t)8U * i0 + (size_t)8U, uint8_t),
            Eurydice_slice, uint8_t[8U]);
        core_result_unwrap_41_0e(dst, uu____0);
        size_t uu____1 = i0 / (size_t)5U;
        size_t uu____2 = i0 % (size_t)5U;
        s[uu____1][uu____2] =
            s[uu____1][uu____2] ^ core_num__u64_9__from_le_bytes(uu____0);
    }
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.load_block_5a
with const generics
- RATE= 72
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_load_block_5a_351(
    uint64_t (*a)[5U], Eurydice_slice b[1U])
{
    uint64_t(*uu____0)[5U] = a;
    /* Passing arrays by value in Rust generates a copy in C */
    Eurydice_slice copy_of_b[1U];
    memcpy(copy_of_b, b, (size_t)1U * sizeof(Eurydice_slice));
    libcrux_sha3_portable_keccak_load_block_651(uu____0, copy_of_b);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.absorb_block
with types uint64_t
with const generics
- N= 1
- RATE= 72
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_absorb_block_40(
    libcrux_sha3_generic_keccak_KeccakState_48 *s, Eurydice_slice blocks[1U])
{
    uint64_t(*uu____0)[5U] = s->st;
    Eurydice_slice uu____1[1U];
    memcpy(uu____1, blocks, (size_t)1U * sizeof(Eurydice_slice));
    libcrux_sha3_portable_keccak_load_block_5a_351(uu____0, uu____1);
    libcrux_sha3_generic_keccak_keccakf1600_b8(s);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.load_block_full
with const generics
- RATE= 72
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_load_block_full_d41(
    uint64_t (*s)[5U], uint8_t blocks[1U][200U])
{
    Eurydice_slice buf[1U] = {
        Eurydice_array_to_slice((size_t)200U, blocks[0U], uint8_t)
    };
    libcrux_sha3_portable_keccak_load_block_651(s, buf);
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.load_block_full_5a
with const generics
- RATE= 72
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_load_block_full_5a_051(
    uint64_t (*a)[5U], uint8_t b[1U][200U])
{
    uint64_t(*uu____0)[5U] = a;
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_b[1U][200U];
    memcpy(copy_of_b, b, (size_t)1U * sizeof(uint8_t[200U]));
    libcrux_sha3_portable_keccak_load_block_full_d41(uu____0, copy_of_b);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.absorb_final
with types uint64_t
with const generics
- N= 1
- RATE= 72
- DELIM= 6
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_absorb_final_401(
    libcrux_sha3_generic_keccak_KeccakState_48 *s, Eurydice_slice last[1U])
{
    size_t last_len = Eurydice_slice_len(last[0U], uint8_t);
    uint8_t blocks[1U][200U] = { { 0U } };
    {
        size_t i = (size_t)0U;
        if (last_len > (size_t)0U) {
            Eurydice_slice uu____0 =
                Eurydice_array_to_subslice2(blocks[i], (size_t)0U, last_len, uint8_t);
            Eurydice_slice_copy(uu____0, last[i], uint8_t);
        }
        blocks[i][last_len] = 6U;
        size_t uu____1 = i;
        size_t uu____2 = (size_t)72U - (size_t)1U;
        blocks[uu____1][uu____2] = (uint32_t)blocks[uu____1][uu____2] | 128U;
    }
    uint64_t(*uu____3)[5U] = s->st;
    uint8_t uu____4[1U][200U];
    memcpy(uu____4, blocks, (size_t)1U * sizeof(uint8_t[200U]));
    libcrux_sha3_portable_keccak_load_block_full_5a_051(uu____3, uu____4);
    libcrux_sha3_generic_keccak_keccakf1600_b8(s);
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.store_block
with const generics
- RATE= 72
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_store_block_9b1(
    uint64_t (*s)[5U], Eurydice_slice out[1U])
{
    for (size_t i = (size_t)0U; i < (size_t)72U / (size_t)8U; i++) {
        size_t i0 = i;
        Eurydice_slice uu____0 = Eurydice_slice_subslice2(
            out[0U], (size_t)8U * i0, (size_t)8U * i0 + (size_t)8U, uint8_t);
        uint8_t ret[8U];
        core_num__u64_9__to_le_bytes(s[i0 / (size_t)5U][i0 % (size_t)5U], ret);
        Eurydice_slice_copy(
            uu____0, Eurydice_array_to_slice((size_t)8U, ret, uint8_t), uint8_t);
    }
}

/**
A monomorphic instance of libcrux_sha3.portable_keccak.store_block_full
with const generics
- RATE= 72
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_store_block_full_7e(
    uint64_t (*s)[5U], uint8_t ret[1U][200U])
{
    uint8_t out[200U] = { 0U };
    Eurydice_slice buf[1U] = {
        Eurydice_array_to_slice((size_t)200U, out, uint8_t)
    };
    libcrux_sha3_portable_keccak_store_block_9b1(s, buf);
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_out[200U];
    memcpy(copy_of_out, out, (size_t)200U * sizeof(uint8_t));
    memcpy(ret[0U], copy_of_out, (size_t)200U * sizeof(uint8_t));
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.store_block_full_5a
with const generics
- RATE= 72
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_store_block_full_5a_27(
    uint64_t (*a)[5U], uint8_t ret[1U][200U])
{
    libcrux_sha3_portable_keccak_store_block_full_7e(a, ret);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.squeeze_first_and_last
with types uint64_t
with const generics
- N= 1
- RATE= 72
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_squeeze_first_and_last_88(
    libcrux_sha3_generic_keccak_KeccakState_48 *s, Eurydice_slice out[1U])
{
    uint8_t b[1U][200U];
    libcrux_sha3_portable_keccak_store_block_full_5a_27(s->st, b);
    {
        size_t i = (size_t)0U;
        Eurydice_slice uu____0 = out[i];
        uint8_t *uu____1 = b[i];
        core_ops_range_Range_b3 lit;
        lit.start = (size_t)0U;
        lit.end = Eurydice_slice_len(out[i], uint8_t);
        Eurydice_slice_copy(
            uu____0,
            Eurydice_array_to_subslice((size_t)200U, uu____1, lit, uint8_t,
                                       core_ops_range_Range_b3),
            uint8_t);
    }
}

/**
This function found in impl {(libcrux_sha3::traits::internal::KeccakItem<1:
usize> for u64)}
*/
/**
A monomorphic instance of libcrux_sha3.portable_keccak.store_block_5a
with const generics
- RATE= 72
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccak_store_block_5a_491(
    uint64_t (*a)[5U], Eurydice_slice b[1U])
{
    libcrux_sha3_portable_keccak_store_block_9b1(a, b);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.squeeze_first_block
with types uint64_t
with const generics
- N= 1
- RATE= 72
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_squeeze_first_block_7b1(
    libcrux_sha3_generic_keccak_KeccakState_48 *s, Eurydice_slice out[1U])
{
    libcrux_sha3_portable_keccak_store_block_5a_491(s->st, out);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.squeeze_next_block
with types uint64_t
with const generics
- N= 1
- RATE= 72
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_squeeze_next_block_c21(
    libcrux_sha3_generic_keccak_KeccakState_48 *s, Eurydice_slice out[1U])
{
    libcrux_sha3_generic_keccak_keccakf1600_b8(s);
    libcrux_sha3_portable_keccak_store_block_5a_491(s->st, out);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.squeeze_last
with types uint64_t
with const generics
- N= 1
- RATE= 72
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_squeeze_last_ca(
    libcrux_sha3_generic_keccak_KeccakState_48 s, Eurydice_slice out[1U])
{
    libcrux_sha3_generic_keccak_keccakf1600_b8(&s);
    uint8_t b[1U][200U];
    libcrux_sha3_portable_keccak_store_block_full_5a_27(s.st, b);
    {
        size_t i = (size_t)0U;
        Eurydice_slice uu____0 = out[i];
        uint8_t *uu____1 = b[i];
        core_ops_range_Range_b3 lit;
        lit.start = (size_t)0U;
        lit.end = Eurydice_slice_len(out[i], uint8_t);
        Eurydice_slice_copy(
            uu____0,
            Eurydice_array_to_subslice((size_t)200U, uu____1, lit, uint8_t,
                                       core_ops_range_Range_b3),
            uint8_t);
    }
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.keccak
with types uint64_t
with const generics
- N= 1
- RATE= 72
- DELIM= 6
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_keccak_06(
    Eurydice_slice data[1U], Eurydice_slice out[1U])
{
    libcrux_sha3_generic_keccak_KeccakState_48 s =
        libcrux_sha3_generic_keccak_new_1e_cf();
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(data[0U], uint8_t) / (size_t)72U; i++) {
        size_t i0 = i;
        libcrux_sha3_generic_keccak_KeccakState_48 *uu____0 = &s;
        /* Passing arrays by value in Rust generates a copy in C */
        Eurydice_slice copy_of_data[1U];
        memcpy(copy_of_data, data, (size_t)1U * sizeof(Eurydice_slice));
        Eurydice_slice ret[1U];
        libcrux_sha3_portable_keccak_slice_n_5a(copy_of_data, i0 * (size_t)72U,
                                                (size_t)72U, ret);
        libcrux_sha3_generic_keccak_absorb_block_40(uu____0, ret);
    }
    size_t rem = Eurydice_slice_len(data[0U], uint8_t) % (size_t)72U;
    libcrux_sha3_generic_keccak_KeccakState_48 *uu____2 = &s;
    /* Passing arrays by value in Rust generates a copy in C */
    Eurydice_slice copy_of_data[1U];
    memcpy(copy_of_data, data, (size_t)1U * sizeof(Eurydice_slice));
    Eurydice_slice ret[1U];
    libcrux_sha3_portable_keccak_slice_n_5a(
        copy_of_data, Eurydice_slice_len(data[0U], uint8_t) - rem, rem, ret);
    libcrux_sha3_generic_keccak_absorb_final_401(uu____2, ret);
    size_t outlen = Eurydice_slice_len(out[0U], uint8_t);
    size_t blocks = outlen / (size_t)72U;
    size_t last = outlen - outlen % (size_t)72U;
    if (blocks == (size_t)0U) {
        libcrux_sha3_generic_keccak_squeeze_first_and_last_88(&s, out);
    } else {
        Eurydice_slice_uint8_t_1size_t__x2 uu____4 =
            libcrux_sha3_portable_keccak_split_at_mut_n_5a(out, (size_t)72U);
        Eurydice_slice o0[1U];
        memcpy(o0, uu____4.fst, (size_t)1U * sizeof(Eurydice_slice));
        Eurydice_slice o1[1U];
        memcpy(o1, uu____4.snd, (size_t)1U * sizeof(Eurydice_slice));
        libcrux_sha3_generic_keccak_squeeze_first_block_7b1(&s, o0);
        core_ops_range_Range_b3 iter =
            core_iter_traits_collect___core__iter__traits__collect__IntoIterator_for_I__1__into_iter(
                (CLITERAL(core_ops_range_Range_b3){ .start = (size_t)1U,
                                                    .end = blocks }),
                core_ops_range_Range_b3, core_ops_range_Range_b3);
        while (true) {
            if (core_iter_range___core__iter__traits__iterator__Iterator_for_core__ops__range__Range_A___6__next(
                    &iter, size_t, core_option_Option_b3)
                    .tag == core_option_None) {
                break;
            } else {
                Eurydice_slice_uint8_t_1size_t__x2 uu____5 =
                    libcrux_sha3_portable_keccak_split_at_mut_n_5a(o1, (size_t)72U);
                Eurydice_slice o[1U];
                memcpy(o, uu____5.fst, (size_t)1U * sizeof(Eurydice_slice));
                Eurydice_slice orest[1U];
                memcpy(orest, uu____5.snd, (size_t)1U * sizeof(Eurydice_slice));
                libcrux_sha3_generic_keccak_squeeze_next_block_c21(&s, o);
                memcpy(o1, orest, (size_t)1U * sizeof(Eurydice_slice));
            }
        }
        if (last < outlen) {
            libcrux_sha3_generic_keccak_squeeze_last_ca(s, o1);
        }
    }
}

/**
A monomorphic instance of libcrux_sha3.portable.keccakx1
with const generics
- RATE= 72
- DELIM= 6
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_keccakx1_e4(
    Eurydice_slice data[1U], Eurydice_slice out[1U])
{
    /* Passing arrays by value in Rust generates a copy in C */
    Eurydice_slice copy_of_data[1U];
    memcpy(copy_of_data, data, (size_t)1U * sizeof(Eurydice_slice));
    libcrux_sha3_generic_keccak_keccak_06(copy_of_data, out);
}

#if defined(__cplusplus)
}
#endif

#define __libcrux_sha3_internal_H_DEFINED
#endif
