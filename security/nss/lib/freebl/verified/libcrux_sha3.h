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

#ifndef __libcrux_sha3_H
#define __libcrux_sha3_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "eurydice_glue.h"
#include "libcrux_core.h"
#include "libcrux_sha3_internal.h"

/**
 A portable SHA3 512 implementation.
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_sha512(Eurydice_slice digest,
                             Eurydice_slice data)
{
    Eurydice_slice buf0[1U] = { data };
    Eurydice_slice buf[1U] = { digest };
    libcrux_sha3_portable_keccakx1_e4(buf0, buf);
}

/**
 A portable SHA3 256 implementation.
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_sha256(Eurydice_slice digest,
                             Eurydice_slice data)
{
    Eurydice_slice buf0[1U] = { data };
    Eurydice_slice buf[1U] = { digest };
    libcrux_sha3_portable_keccakx1_e40(buf0, buf);
}

/**
 A portable SHAKE256 implementation.
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_shake256(
    Eurydice_slice digest, Eurydice_slice data)
{
    Eurydice_slice buf0[1U] = { data };
    Eurydice_slice buf[1U] = { digest };
    libcrux_sha3_portable_keccakx1_e41(buf0, buf);
}

/**
 A portable SHA3 224 implementation.
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_sha224(Eurydice_slice digest,
                             Eurydice_slice data)
{
    Eurydice_slice buf0[1U] = { data };
    Eurydice_slice buf[1U] = { digest };
    libcrux_sha3_portable_keccakx1_e42(buf0, buf);
}

/**
 A portable SHA3 384 implementation.
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_sha384(Eurydice_slice digest,
                             Eurydice_slice data)
{
    Eurydice_slice buf0[1U] = { data };
    Eurydice_slice buf[1U] = { digest };
    libcrux_sha3_portable_keccakx1_e43(buf0, buf);
}

/**
 SHA3 224

 Preconditions:
 - `digest.len() == 28`
*/
static KRML_MUSTINLINE void
libcrux_sha3_sha224_ema(Eurydice_slice digest,
                        Eurydice_slice payload)
{
    libcrux_sha3_portable_sha224(digest, payload);
}

/**
 SHA3 224
*/
static KRML_MUSTINLINE void
libcrux_sha3_sha224(Eurydice_slice data,
                    uint8_t ret[28U])
{
    uint8_t out[28U] = { 0U };
    libcrux_sha3_sha224_ema(Eurydice_array_to_slice((size_t)28U, out, uint8_t),
                            data);
    memcpy(ret, out, (size_t)28U * sizeof(uint8_t));
}

/**
 SHA3 256
*/
static KRML_MUSTINLINE void
libcrux_sha3_sha256_ema(Eurydice_slice digest,
                        Eurydice_slice payload)
{
    libcrux_sha3_portable_sha256(digest, payload);
}

/**
 SHA3 256
*/
static KRML_MUSTINLINE void
libcrux_sha3_sha256(Eurydice_slice data,
                    uint8_t ret[32U])
{
    uint8_t out[32U] = { 0U };
    libcrux_sha3_sha256_ema(Eurydice_array_to_slice((size_t)32U, out, uint8_t),
                            data);
    memcpy(ret, out, (size_t)32U * sizeof(uint8_t));
}

/**
 SHA3 384
*/
static KRML_MUSTINLINE void
libcrux_sha3_sha384_ema(Eurydice_slice digest,
                        Eurydice_slice payload)
{
    libcrux_sha3_portable_sha384(digest, payload);
}

/**
 SHA3 384
*/
static KRML_MUSTINLINE void
libcrux_sha3_sha384(Eurydice_slice data,
                    uint8_t ret[48U])
{
    uint8_t out[48U] = { 0U };
    libcrux_sha3_sha384_ema(Eurydice_array_to_slice((size_t)48U, out, uint8_t),
                            data);
    memcpy(ret, out, (size_t)48U * sizeof(uint8_t));
}

/**
 SHA3 512
*/
static KRML_MUSTINLINE void
libcrux_sha3_sha512_ema(Eurydice_slice digest,
                        Eurydice_slice payload)
{
    libcrux_sha3_portable_sha512(digest, payload);
}

/**
 SHA3 512
*/
static KRML_MUSTINLINE void
libcrux_sha3_sha512(Eurydice_slice data,
                    uint8_t ret[64U])
{
    uint8_t out[64U] = { 0U };
    libcrux_sha3_sha512_ema(Eurydice_array_to_slice((size_t)64U, out, uint8_t),
                            data);
    memcpy(ret, out, (size_t)64U * sizeof(uint8_t));
}

/**
 A portable SHAKE128 implementation.
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_shake128(
    Eurydice_slice digest, Eurydice_slice data)
{
    Eurydice_slice buf0[1U] = { data };
    Eurydice_slice buf[1U] = { digest };
    libcrux_sha3_portable_keccakx1_e44(buf0, buf);
}

/**
 SHAKE 128

 Writes `out.len()` bytes.
*/
static KRML_MUSTINLINE void
libcrux_sha3_shake128_ema(Eurydice_slice out,
                          Eurydice_slice data)
{
    libcrux_sha3_portable_shake128(out, data);
}

/**
 SHAKE 256

 Writes `out.len()` bytes.
*/
static KRML_MUSTINLINE void
libcrux_sha3_shake256_ema(Eurydice_slice out,
                          Eurydice_slice data)
{
    libcrux_sha3_portable_shake256(out, data);
}

#if defined(__cplusplus)
}
#endif

#define __libcrux_sha3_H_DEFINED
#endif
