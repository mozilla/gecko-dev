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

#ifndef __internal_libcrux_core_H
#define __internal_libcrux_core_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "../libcrux_core.h"
#include "eurydice_glue.h"

#define CORE_NUM__U32_8__BITS (32U)

static inline uint32_t core_num__u8_6__count_ones(uint8_t x0);

#define LIBCRUX_ML_KEM_CONSTANTS_SHARED_SECRET_SIZE ((size_t)32U)

void libcrux_ml_kem_constant_time_ops_compare_ciphertexts_select_shared_secret_in_constant_time(
    Eurydice_slice lhs_c, Eurydice_slice rhs_c, Eurydice_slice lhs_s,
    Eurydice_slice rhs_s, uint8_t ret[32U]);

#define LIBCRUX_ML_KEM_CONSTANTS_BITS_PER_COEFFICIENT ((size_t)12U)

#define LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT ((size_t)256U)

#define LIBCRUX_ML_KEM_CONSTANTS_BITS_PER_RING_ELEMENT \
    (LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT * (size_t)12U)

#define LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT \
    (LIBCRUX_ML_KEM_CONSTANTS_BITS_PER_RING_ELEMENT / (size_t)8U)

#define LIBCRUX_ML_KEM_CONSTANTS_CPA_PKE_KEY_GENERATION_SEED_SIZE ((size_t)32U)

#define LIBCRUX_ML_KEM_CONSTANTS_H_DIGEST_SIZE ((size_t)32U)

typedef struct libcrux_ml_kem_utils_extraction_helper_Keypair1024_s {
    uint8_t fst[1536U];
    uint8_t snd[1568U];
} libcrux_ml_kem_utils_extraction_helper_Keypair1024;

typedef struct libcrux_ml_kem_utils_extraction_helper_Keypair512_s {
    uint8_t fst[768U];
    uint8_t snd[800U];
} libcrux_ml_kem_utils_extraction_helper_Keypair512;

typedef struct libcrux_ml_kem_utils_extraction_helper_Keypair768_s {
    uint8_t fst[1152U];
    uint8_t snd[1184U];
} libcrux_ml_kem_utils_extraction_helper_Keypair768;

/**
This function found in impl {(core::convert::From<@Array<u8, SIZE>> for
libcrux_ml_kem::types::MlKemPublicKey<SIZE>)#14}
*/
/**
A monomorphic instance of libcrux_ml_kem.types.from_b6
with const generics
- SIZE= 1568
*/
libcrux_ml_kem_types_MlKemPublicKey_1f libcrux_ml_kem_types_from_b6_961(
    uint8_t value[1568U]);

/**
 Create a new [`MlKemKeyPair`] from the secret and public key.
*/
/**
This function found in impl
{libcrux_ml_kem::types::MlKemKeyPair<PRIVATE_KEY_SIZE, PUBLIC_KEY_SIZE>}
*/
/**
A monomorphic instance of libcrux_ml_kem.types.from_17
with const generics
- PRIVATE_KEY_SIZE= 3168
- PUBLIC_KEY_SIZE= 1568
*/
libcrux_ml_kem_mlkem1024_MlKem1024KeyPair libcrux_ml_kem_types_from_17_821(
    libcrux_ml_kem_types_MlKemPrivateKey_95 sk,
    libcrux_ml_kem_types_MlKemPublicKey_1f pk);

/**
This function found in impl {(core::convert::From<@Array<u8, SIZE>> for
libcrux_ml_kem::types::MlKemPrivateKey<SIZE>)#8}
*/
/**
A monomorphic instance of libcrux_ml_kem.types.from_05
with const generics
- SIZE= 3168
*/
libcrux_ml_kem_types_MlKemPrivateKey_95 libcrux_ml_kem_types_from_05_891(
    uint8_t value[3168U]);

/**
This function found in impl {(core::convert::From<@Array<u8, SIZE>> for
libcrux_ml_kem::types::MlKemCiphertext<SIZE>)#2}
*/
/**
A monomorphic instance of libcrux_ml_kem.types.from_01
with const generics
- SIZE= 1568
*/
libcrux_ml_kem_mlkem1024_MlKem1024Ciphertext libcrux_ml_kem_types_from_01_331(
    uint8_t value[1568U]);

/**
 A reference to the raw byte slice.
*/
/**
This function found in impl {libcrux_ml_kem::types::MlKemPublicKey<SIZE>#18}
*/
/**
A monomorphic instance of libcrux_ml_kem.types.as_slice_cb
with const generics
- SIZE= 1568
*/
uint8_t *libcrux_ml_kem_types_as_slice_cb_3d1(
    libcrux_ml_kem_types_MlKemPublicKey_1f *self);

/**
This function found in impl {(core::convert::AsRef<@Slice<u8>> for
libcrux_ml_kem::types::MlKemCiphertext<SIZE>)#1}
*/
/**
A monomorphic instance of libcrux_ml_kem.types.as_ref_00
with const generics
- SIZE= 1568
*/
Eurydice_slice libcrux_ml_kem_types_as_ref_00_d81(
    libcrux_ml_kem_mlkem1024_MlKem1024Ciphertext *self);

/**
 Pad the `slice` with `0`s at the end.
*/
/**
A monomorphic instance of libcrux_ml_kem.utils.into_padded_array
with const generics
- LEN= 1600
*/
void libcrux_ml_kem_utils_into_padded_array_6d4(Eurydice_slice slice,
                                                uint8_t ret[1600U]);

/**
This function found in impl {(core::convert::From<@Array<u8, SIZE>> for
libcrux_ml_kem::types::MlKemPublicKey<SIZE>)#14}
*/
/**
A monomorphic instance of libcrux_ml_kem.types.from_b6
with const generics
- SIZE= 1184
*/
libcrux_ml_kem_types_MlKemPublicKey_15 libcrux_ml_kem_types_from_b6_960(
    uint8_t value[1184U]);

/**
 Create a new [`MlKemKeyPair`] from the secret and public key.
*/
/**
This function found in impl
{libcrux_ml_kem::types::MlKemKeyPair<PRIVATE_KEY_SIZE, PUBLIC_KEY_SIZE>}
*/
/**
A monomorphic instance of libcrux_ml_kem.types.from_17
with const generics
- PRIVATE_KEY_SIZE= 2400
- PUBLIC_KEY_SIZE= 1184
*/
libcrux_ml_kem_mlkem768_MlKem768KeyPair libcrux_ml_kem_types_from_17_820(
    libcrux_ml_kem_types_MlKemPrivateKey_55 sk,
    libcrux_ml_kem_types_MlKemPublicKey_15 pk);

/**
This function found in impl {(core::convert::From<@Array<u8, SIZE>> for
libcrux_ml_kem::types::MlKemPrivateKey<SIZE>)#8}
*/
/**
A monomorphic instance of libcrux_ml_kem.types.from_05
with const generics
- SIZE= 2400
*/
libcrux_ml_kem_types_MlKemPrivateKey_55 libcrux_ml_kem_types_from_05_890(
    uint8_t value[2400U]);

/**
This function found in impl {(core::convert::From<@Array<u8, SIZE>> for
libcrux_ml_kem::types::MlKemCiphertext<SIZE>)#2}
*/
/**
A monomorphic instance of libcrux_ml_kem.types.from_01
with const generics
- SIZE= 1088
*/
libcrux_ml_kem_mlkem768_MlKem768Ciphertext libcrux_ml_kem_types_from_01_330(
    uint8_t value[1088U]);

/**
 A reference to the raw byte slice.
*/
/**
This function found in impl {libcrux_ml_kem::types::MlKemPublicKey<SIZE>#18}
*/
/**
A monomorphic instance of libcrux_ml_kem.types.as_slice_cb
with const generics
- SIZE= 1184
*/
uint8_t *libcrux_ml_kem_types_as_slice_cb_3d0(
    libcrux_ml_kem_types_MlKemPublicKey_15 *self);

/**
This function found in impl {(core::convert::AsRef<@Slice<u8>> for
libcrux_ml_kem::types::MlKemCiphertext<SIZE>)#1}
*/
/**
A monomorphic instance of libcrux_ml_kem.types.as_ref_00
with const generics
- SIZE= 1088
*/
Eurydice_slice libcrux_ml_kem_types_as_ref_00_d80(
    libcrux_ml_kem_mlkem768_MlKem768Ciphertext *self);

/**
 Pad the `slice` with `0`s at the end.
*/
/**
A monomorphic instance of libcrux_ml_kem.utils.into_padded_array
with const generics
- LEN= 1120
*/
void libcrux_ml_kem_utils_into_padded_array_6d3(Eurydice_slice slice,
                                                uint8_t ret[1120U]);

/**
This function found in impl {(core::convert::From<@Array<u8, SIZE>> for
libcrux_ml_kem::types::MlKemPublicKey<SIZE>)#14}
*/
/**
A monomorphic instance of libcrux_ml_kem.types.from_b6
with const generics
- SIZE= 800
*/
libcrux_ml_kem_types_MlKemPublicKey_be libcrux_ml_kem_types_from_b6_96(
    uint8_t value[800U]);

/**
 Create a new [`MlKemKeyPair`] from the secret and public key.
*/
/**
This function found in impl
{libcrux_ml_kem::types::MlKemKeyPair<PRIVATE_KEY_SIZE, PUBLIC_KEY_SIZE>}
*/
/**
A monomorphic instance of libcrux_ml_kem.types.from_17
with const generics
- PRIVATE_KEY_SIZE= 1632
- PUBLIC_KEY_SIZE= 800
*/
libcrux_ml_kem_types_MlKemKeyPair_cb libcrux_ml_kem_types_from_17_82(
    libcrux_ml_kem_types_MlKemPrivateKey_5e sk,
    libcrux_ml_kem_types_MlKemPublicKey_be pk);

/**
This function found in impl {(core::convert::From<@Array<u8, SIZE>> for
libcrux_ml_kem::types::MlKemPrivateKey<SIZE>)#8}
*/
/**
A monomorphic instance of libcrux_ml_kem.types.from_05
with const generics
- SIZE= 1632
*/
libcrux_ml_kem_types_MlKemPrivateKey_5e libcrux_ml_kem_types_from_05_89(
    uint8_t value[1632U]);

/**
A monomorphic instance of core.result.Result
with types uint8_t[32size_t], core_array_TryFromSliceError

*/
typedef struct core_result_Result_00_s {
    core_result_Result_86_tags tag;
    union {
        uint8_t case_Ok[32U];
        core_array_TryFromSliceError case_Err;
    } val;
} core_result_Result_00;

/**
This function found in impl {core::result::Result<T, E>}
*/
/**
A monomorphic instance of core.result.unwrap_41
with types uint8_t[32size_t], core_array_TryFromSliceError

*/
void core_result_unwrap_41_33(core_result_Result_00 self, uint8_t ret[32U]);

/**
This function found in impl {(core::convert::From<@Array<u8, SIZE>> for
libcrux_ml_kem::types::MlKemCiphertext<SIZE>)#2}
*/
/**
A monomorphic instance of libcrux_ml_kem.types.from_01
with const generics
- SIZE= 768
*/
libcrux_ml_kem_types_MlKemCiphertext_e8 libcrux_ml_kem_types_from_01_33(
    uint8_t value[768U]);

/**
 A reference to the raw byte slice.
*/
/**
This function found in impl {libcrux_ml_kem::types::MlKemPublicKey<SIZE>#18}
*/
/**
A monomorphic instance of libcrux_ml_kem.types.as_slice_cb
with const generics
- SIZE= 800
*/
uint8_t *libcrux_ml_kem_types_as_slice_cb_3d(
    libcrux_ml_kem_types_MlKemPublicKey_be *self);

/**
 Pad the `slice` with `0`s at the end.
*/
/**
A monomorphic instance of libcrux_ml_kem.utils.into_padded_array
with const generics
- LEN= 33
*/
void libcrux_ml_kem_utils_into_padded_array_6d2(Eurydice_slice slice,
                                                uint8_t ret[33U]);

/**
 Pad the `slice` with `0`s at the end.
*/
/**
A monomorphic instance of libcrux_ml_kem.utils.into_padded_array
with const generics
- LEN= 34
*/
void libcrux_ml_kem_utils_into_padded_array_6d1(Eurydice_slice slice,
                                                uint8_t ret[34U]);

/**
This function found in impl {(core::convert::AsRef<@Slice<u8>> for
libcrux_ml_kem::types::MlKemCiphertext<SIZE>)#1}
*/
/**
A monomorphic instance of libcrux_ml_kem.types.as_ref_00
with const generics
- SIZE= 768
*/
Eurydice_slice libcrux_ml_kem_types_as_ref_00_d8(
    libcrux_ml_kem_types_MlKemCiphertext_e8 *self);

/**
 Pad the `slice` with `0`s at the end.
*/
/**
A monomorphic instance of libcrux_ml_kem.utils.into_padded_array
with const generics
- LEN= 800
*/
void libcrux_ml_kem_utils_into_padded_array_6d0(Eurydice_slice slice,
                                                uint8_t ret[800U]);

/**
 Pad the `slice` with `0`s at the end.
*/
/**
A monomorphic instance of libcrux_ml_kem.utils.into_padded_array
with const generics
- LEN= 64
*/
void libcrux_ml_kem_utils_into_padded_array_6d(Eurydice_slice slice,
                                               uint8_t ret[64U]);

/**
A monomorphic instance of core.result.Result
with types uint8_t[24size_t], core_array_TryFromSliceError

*/
typedef struct core_result_Result_6f_s {
    core_result_Result_86_tags tag;
    union {
        uint8_t case_Ok[24U];
        core_array_TryFromSliceError case_Err;
    } val;
} core_result_Result_6f;

/**
This function found in impl {core::result::Result<T, E>}
*/
/**
A monomorphic instance of core.result.unwrap_41
with types uint8_t[24size_t], core_array_TryFromSliceError

*/
void core_result_unwrap_41_76(core_result_Result_6f self, uint8_t ret[24U]);

/**
A monomorphic instance of core.result.Result
with types uint8_t[20size_t], core_array_TryFromSliceError

*/
typedef struct core_result_Result_7a_s {
    core_result_Result_86_tags tag;
    union {
        uint8_t case_Ok[20U];
        core_array_TryFromSliceError case_Err;
    } val;
} core_result_Result_7a;

/**
This function found in impl {core::result::Result<T, E>}
*/
/**
A monomorphic instance of core.result.unwrap_41
with types uint8_t[20size_t], core_array_TryFromSliceError

*/
void core_result_unwrap_41_ea(core_result_Result_7a self, uint8_t ret[20U]);

/**
A monomorphic instance of core.result.Result
with types uint8_t[10size_t], core_array_TryFromSliceError

*/
typedef struct core_result_Result_cd_s {
    core_result_Result_86_tags tag;
    union {
        uint8_t case_Ok[10U];
        core_array_TryFromSliceError case_Err;
    } val;
} core_result_Result_cd;

/**
This function found in impl {core::result::Result<T, E>}
*/
/**
A monomorphic instance of core.result.unwrap_41
with types uint8_t[10size_t], core_array_TryFromSliceError

*/
void core_result_unwrap_41_07(core_result_Result_cd self, uint8_t ret[10U]);

/**
A monomorphic instance of core.result.Result
with types int16_t[16size_t], core_array_TryFromSliceError

*/
typedef struct core_result_Result_c0_s {
    core_result_Result_86_tags tag;
    union {
        int16_t case_Ok[16U];
        core_array_TryFromSliceError case_Err;
    } val;
} core_result_Result_c0;

/**
This function found in impl {core::result::Result<T, E>}
*/
/**
A monomorphic instance of core.result.unwrap_41
with types int16_t[16size_t], core_array_TryFromSliceError

*/
void core_result_unwrap_41_30(core_result_Result_c0 self, int16_t ret[16U]);

typedef struct Eurydice_slice_uint8_t_4size_t__x2_s {
    Eurydice_slice fst[4U];
    Eurydice_slice snd[4U];
} Eurydice_slice_uint8_t_4size_t__x2;

#if defined(__cplusplus)
}
#endif

#define __internal_libcrux_core_H_DEFINED
#endif
