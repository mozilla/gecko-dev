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

#include "internal/libcrux_mlkem_portable.h"

#include "internal/libcrux_core.h"
#include "internal/libcrux_sha3_internal.h"

KRML_MUSTINLINE void
libcrux_ml_kem_hash_functions_portable_G(
    Eurydice_slice input, uint8_t ret[64U])
{
    uint8_t digest[64U] = { 0U };
    libcrux_sha3_portable_sha512(
        Eurydice_array_to_slice((size_t)64U, digest, uint8_t), input);
    memcpy(ret, digest, (size_t)64U * sizeof(uint8_t));
}

KRML_MUSTINLINE void
libcrux_ml_kem_hash_functions_portable_H(
    Eurydice_slice input, uint8_t ret[32U])
{
    uint8_t digest[32U] = { 0U };
    libcrux_sha3_portable_sha256(
        Eurydice_array_to_slice((size_t)32U, digest, uint8_t), input);
    memcpy(ret, digest, (size_t)32U * sizeof(uint8_t));
}

const int16_t libcrux_ml_kem_polynomial_ZETAS_TIMES_MONTGOMERY_R[128U] = {
    (int16_t)-1044, (int16_t)-758, (int16_t)-359, (int16_t)-1517,
    (int16_t)1493, (int16_t)1422, (int16_t)287, (int16_t)202,
    (int16_t)-171, (int16_t)622, (int16_t)1577, (int16_t)182,
    (int16_t)962, (int16_t)-1202, (int16_t)-1474, (int16_t)1468,
    (int16_t)573, (int16_t)-1325, (int16_t)264, (int16_t)383,
    (int16_t)-829, (int16_t)1458, (int16_t)-1602, (int16_t)-130,
    (int16_t)-681, (int16_t)1017, (int16_t)732, (int16_t)608,
    (int16_t)-1542, (int16_t)411, (int16_t)-205, (int16_t)-1571,
    (int16_t)1223, (int16_t)652, (int16_t)-552, (int16_t)1015,
    (int16_t)-1293, (int16_t)1491, (int16_t)-282, (int16_t)-1544,
    (int16_t)516, (int16_t)-8, (int16_t)-320, (int16_t)-666,
    (int16_t)-1618, (int16_t)-1162, (int16_t)126, (int16_t)1469,
    (int16_t)-853, (int16_t)-90, (int16_t)-271, (int16_t)830,
    (int16_t)107, (int16_t)-1421, (int16_t)-247, (int16_t)-951,
    (int16_t)-398, (int16_t)961, (int16_t)-1508, (int16_t)-725,
    (int16_t)448, (int16_t)-1065, (int16_t)677, (int16_t)-1275,
    (int16_t)-1103, (int16_t)430, (int16_t)555, (int16_t)843,
    (int16_t)-1251, (int16_t)871, (int16_t)1550, (int16_t)105,
    (int16_t)422, (int16_t)587, (int16_t)177, (int16_t)-235,
    (int16_t)-291, (int16_t)-460, (int16_t)1574, (int16_t)1653,
    (int16_t)-246, (int16_t)778, (int16_t)1159, (int16_t)-147,
    (int16_t)-777, (int16_t)1483, (int16_t)-602, (int16_t)1119,
    (int16_t)-1590, (int16_t)644, (int16_t)-872, (int16_t)349,
    (int16_t)418, (int16_t)329, (int16_t)-156, (int16_t)-75,
    (int16_t)817, (int16_t)1097, (int16_t)603, (int16_t)610,
    (int16_t)1322, (int16_t)-1285, (int16_t)-1465, (int16_t)384,
    (int16_t)-1215, (int16_t)-136, (int16_t)1218, (int16_t)-1335,
    (int16_t)-874, (int16_t)220, (int16_t)-1187, (int16_t)-1659,
    (int16_t)-1185, (int16_t)-1530, (int16_t)-1278, (int16_t)794,
    (int16_t)-1510, (int16_t)-854, (int16_t)-870, (int16_t)478,
    (int16_t)-108, (int16_t)-308, (int16_t)996, (int16_t)991,
    (int16_t)958, (int16_t)-1460, (int16_t)1522, (int16_t)1628
};

KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_vector_type_from_i16_array(
    Eurydice_slice array)
{
    libcrux_ml_kem_vector_portable_vector_type_PortableVector lit;
    int16_t ret[16U];
    core_result_Result_c0 dst;
    Eurydice_slice_to_array2(
        &dst, Eurydice_slice_subslice2(array, (size_t)0U, (size_t)16U, int16_t),
        Eurydice_slice, int16_t[16U]);
    core_result_unwrap_41_30(dst, ret);
    memcpy(lit.elements, ret, (size_t)16U * sizeof(int16_t));
    return lit;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_from_i16_array_0d(Eurydice_slice array)
{
    return libcrux_ml_kem_vector_portable_vector_type_from_i16_array(array);
}

KRML_MUSTINLINE uint8_t_x11
libcrux_ml_kem_vector_portable_serialize_serialize_11_int(Eurydice_slice v)
{
    uint8_t r0 = (uint8_t)Eurydice_slice_index(v, (size_t)0U, int16_t, int16_t *);
    uint8_t r1 = (uint32_t)(uint8_t)(Eurydice_slice_index(v, (size_t)1U, int16_t,
                                                          int16_t *) &
                                     (int16_t)31)
                     << 3U |
                 (uint32_t)(uint8_t)(Eurydice_slice_index(v, (size_t)0U, int16_t,
                                                          int16_t *) >>
                                     8U);
    uint8_t r2 = (uint32_t)(uint8_t)(Eurydice_slice_index(v, (size_t)2U, int16_t,
                                                          int16_t *) &
                                     (int16_t)3)
                     << 6U |
                 (uint32_t)(uint8_t)(Eurydice_slice_index(v, (size_t)1U, int16_t,
                                                          int16_t *) >>
                                     5U);
    uint8_t r3 =
        (uint8_t)(Eurydice_slice_index(v, (size_t)2U, int16_t, int16_t *) >> 2U &
                  (int16_t)255);
    uint8_t r4 = (uint32_t)(uint8_t)(Eurydice_slice_index(v, (size_t)3U, int16_t,
                                                          int16_t *) &
                                     (int16_t)127)
                     << 1U |
                 (uint32_t)(uint8_t)(Eurydice_slice_index(v, (size_t)2U, int16_t,
                                                          int16_t *) >>
                                     10U);
    uint8_t r5 = (uint32_t)(uint8_t)(Eurydice_slice_index(v, (size_t)4U, int16_t,
                                                          int16_t *) &
                                     (int16_t)15)
                     << 4U |
                 (uint32_t)(uint8_t)(Eurydice_slice_index(v, (size_t)3U, int16_t,
                                                          int16_t *) >>
                                     7U);
    uint8_t r6 = (uint32_t)(uint8_t)(Eurydice_slice_index(v, (size_t)5U, int16_t,
                                                          int16_t *) &
                                     (int16_t)1)
                     << 7U |
                 (uint32_t)(uint8_t)(Eurydice_slice_index(v, (size_t)4U, int16_t,
                                                          int16_t *) >>
                                     4U);
    uint8_t r7 =
        (uint8_t)(Eurydice_slice_index(v, (size_t)5U, int16_t, int16_t *) >> 1U &
                  (int16_t)255);
    uint8_t r8 = (uint32_t)(uint8_t)(Eurydice_slice_index(v, (size_t)6U, int16_t,
                                                          int16_t *) &
                                     (int16_t)63)
                     << 2U |
                 (uint32_t)(uint8_t)(Eurydice_slice_index(v, (size_t)5U, int16_t,
                                                          int16_t *) >>
                                     9U);
    uint8_t r9 = (uint32_t)(uint8_t)(Eurydice_slice_index(v, (size_t)7U, int16_t,
                                                          int16_t *) &
                                     (int16_t)7)
                     << 5U |
                 (uint32_t)(uint8_t)(Eurydice_slice_index(v, (size_t)6U, int16_t,
                                                          int16_t *) >>
                                     6U);
    uint8_t r10 =
        (uint8_t)(Eurydice_slice_index(v, (size_t)7U, int16_t, int16_t *) >> 3U);
    return (CLITERAL(uint8_t_x11){ .fst = r0,
                                   .snd = r1,
                                   .thd = r2,
                                   .f3 = r3,
                                   .f4 = r4,
                                   .f5 = r5,
                                   .f6 = r6,
                                   .f7 = r7,
                                   .f8 = r8,
                                   .f9 = r9,
                                   .f10 = r10 });
}

KRML_MUSTINLINE void
libcrux_ml_kem_vector_portable_serialize_serialize_11(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v,
    uint8_t ret[22U])
{
    uint8_t_x11 r0_10 = libcrux_ml_kem_vector_portable_serialize_serialize_11_int(
        Eurydice_array_to_subslice2(v.elements, (size_t)0U, (size_t)8U, int16_t));
    uint8_t_x11 r11_21 =
        libcrux_ml_kem_vector_portable_serialize_serialize_11_int(
            Eurydice_array_to_subslice2(v.elements, (size_t)8U, (size_t)16U,
                                        int16_t));
    uint8_t result[22U] = { 0U };
    result[0U] = r0_10.fst;
    result[1U] = r0_10.snd;
    result[2U] = r0_10.thd;
    result[3U] = r0_10.f3;
    result[4U] = r0_10.f4;
    result[5U] = r0_10.f5;
    result[6U] = r0_10.f6;
    result[7U] = r0_10.f7;
    result[8U] = r0_10.f8;
    result[9U] = r0_10.f9;
    result[10U] = r0_10.f10;
    result[11U] = r11_21.fst;
    result[12U] = r11_21.snd;
    result[13U] = r11_21.thd;
    result[14U] = r11_21.f3;
    result[15U] = r11_21.f4;
    result[16U] = r11_21.f5;
    result[17U] = r11_21.f6;
    result[18U] = r11_21.f7;
    result[19U] = r11_21.f8;
    result[20U] = r11_21.f9;
    result[21U] = r11_21.f10;
    memcpy(ret, result, (size_t)22U * sizeof(uint8_t));
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
void
libcrux_ml_kem_vector_portable_serialize_11_0d(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a,
    uint8_t ret[22U])
{
    libcrux_ml_kem_vector_portable_serialize_serialize_11(a, ret);
}

KRML_MUSTINLINE int16_t_x8
libcrux_ml_kem_vector_portable_serialize_deserialize_11_int(
    Eurydice_slice bytes)
{
    int16_t r0 =
        ((int16_t)Eurydice_slice_index(bytes, (size_t)1U, uint8_t, uint8_t *) &
         (int16_t)7)
            << 8U |
        (int16_t)Eurydice_slice_index(bytes, (size_t)0U, uint8_t, uint8_t *);
    int16_t r1 =
        ((int16_t)Eurydice_slice_index(bytes, (size_t)2U, uint8_t, uint8_t *) &
         (int16_t)63)
            << 5U |
        (int16_t)Eurydice_slice_index(bytes, (size_t)1U, uint8_t, uint8_t *) >>
            3U;
    int16_t r2 =
        (((int16_t)Eurydice_slice_index(bytes, (size_t)4U, uint8_t, uint8_t *) &
          (int16_t)1)
             << 10U |
         (int16_t)Eurydice_slice_index(bytes, (size_t)3U, uint8_t, uint8_t *)
             << 2U) |
        (int16_t)Eurydice_slice_index(bytes, (size_t)2U, uint8_t, uint8_t *) >>
            6U;
    int16_t r3 =
        ((int16_t)Eurydice_slice_index(bytes, (size_t)5U, uint8_t, uint8_t *) &
         (int16_t)15)
            << 7U |
        (int16_t)Eurydice_slice_index(bytes, (size_t)4U, uint8_t, uint8_t *) >>
            1U;
    int16_t r4 =
        ((int16_t)Eurydice_slice_index(bytes, (size_t)6U, uint8_t, uint8_t *) &
         (int16_t)127)
            << 4U |
        (int16_t)Eurydice_slice_index(bytes, (size_t)5U, uint8_t, uint8_t *) >>
            4U;
    int16_t r5 =
        (((int16_t)Eurydice_slice_index(bytes, (size_t)8U, uint8_t, uint8_t *) &
          (int16_t)3)
             << 9U |
         (int16_t)Eurydice_slice_index(bytes, (size_t)7U, uint8_t, uint8_t *)
             << 1U) |
        (int16_t)Eurydice_slice_index(bytes, (size_t)6U, uint8_t, uint8_t *) >>
            7U;
    int16_t r6 =
        ((int16_t)Eurydice_slice_index(bytes, (size_t)9U, uint8_t, uint8_t *) &
         (int16_t)31)
            << 6U |
        (int16_t)Eurydice_slice_index(bytes, (size_t)8U, uint8_t, uint8_t *) >>
            2U;
    int16_t r7 =
        (int16_t)Eurydice_slice_index(bytes, (size_t)10U, uint8_t, uint8_t *)
            << 3U |
        (int16_t)Eurydice_slice_index(bytes, (size_t)9U, uint8_t, uint8_t *) >>
            5U;
    return (CLITERAL(int16_t_x8){ .fst = r0,
                                  .snd = r1,
                                  .thd = r2,
                                  .f3 = r3,
                                  .f4 = r4,
                                  .f5 = r5,
                                  .f6 = r6,
                                  .f7 = r7 });
}

KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_vector_type_zero(void)
{
    libcrux_ml_kem_vector_portable_vector_type_PortableVector lit;
    lit.elements[0U] = (int16_t)0;
    lit.elements[1U] = (int16_t)0;
    lit.elements[2U] = (int16_t)0;
    lit.elements[3U] = (int16_t)0;
    lit.elements[4U] = (int16_t)0;
    lit.elements[5U] = (int16_t)0;
    lit.elements[6U] = (int16_t)0;
    lit.elements[7U] = (int16_t)0;
    lit.elements[8U] = (int16_t)0;
    lit.elements[9U] = (int16_t)0;
    lit.elements[10U] = (int16_t)0;
    lit.elements[11U] = (int16_t)0;
    lit.elements[12U] = (int16_t)0;
    lit.elements[13U] = (int16_t)0;
    lit.elements[14U] = (int16_t)0;
    lit.elements[15U] = (int16_t)0;
    return lit;
}

KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_serialize_deserialize_11(Eurydice_slice bytes)
{
    int16_t_x8 v0_7 = libcrux_ml_kem_vector_portable_serialize_deserialize_11_int(
        Eurydice_slice_subslice2(bytes, (size_t)0U, (size_t)11U, uint8_t));
    int16_t_x8 v8_15 =
        libcrux_ml_kem_vector_portable_serialize_deserialize_11_int(
            Eurydice_slice_subslice2(bytes, (size_t)11U, (size_t)22U, uint8_t));
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v =
        libcrux_ml_kem_vector_portable_vector_type_zero();
    v.elements[0U] = v0_7.fst;
    v.elements[1U] = v0_7.snd;
    v.elements[2U] = v0_7.thd;
    v.elements[3U] = v0_7.f3;
    v.elements[4U] = v0_7.f4;
    v.elements[5U] = v0_7.f5;
    v.elements[6U] = v0_7.f6;
    v.elements[7U] = v0_7.f7;
    v.elements[8U] = v8_15.fst;
    v.elements[9U] = v8_15.snd;
    v.elements[10U] = v8_15.thd;
    v.elements[11U] = v8_15.f3;
    v.elements[12U] = v8_15.f4;
    v.elements[13U] = v8_15.f5;
    v.elements[14U] = v8_15.f6;
    v.elements[15U] = v8_15.f7;
    return v;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_deserialize_11_0d(Eurydice_slice a)
{
    return libcrux_ml_kem_vector_portable_serialize_deserialize_11(a);
}

KRML_MUSTINLINE void
libcrux_ml_kem_vector_portable_vector_type_to_i16_array(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector x,
    int16_t ret[16U])
{
    memcpy(ret, x.elements, (size_t)16U * sizeof(int16_t));
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
void
libcrux_ml_kem_vector_portable_to_i16_array_0d(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector x,
    int16_t ret[16U])
{
    libcrux_ml_kem_vector_portable_vector_type_to_i16_array(x, ret);
}

const uint8_t
    libcrux_ml_kem_vector_rej_sample_table_REJECTION_SAMPLE_SHUFFLE_TABLE
        [256U][16U] = { { 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 4U, 5U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 6U, 7U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 6U, 7U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 6U, 7U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 6U, 7U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 4U, 5U, 6U, 7U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 6U, 7U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 6U, 7U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 8U, 9U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 8U, 9U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 8U, 9U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 8U, 9U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 4U, 5U, 8U, 9U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 8U, 9U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 8U, 9U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 8U, 9U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 6U, 7U, 8U, 9U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 6U, 7U, 8U, 9U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 6U, 7U, 8U, 9U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 6U, 7U, 8U, 9U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 4U, 5U, 6U, 7U, 8U, 9U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 6U, 7U, 8U, 9U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 10U, 11U, 255U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 10U, 11U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 10U, 11U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 10U, 11U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 4U, 5U, 10U, 11U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 10U, 11U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 10U, 11U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 10U, 11U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 6U, 7U, 10U, 11U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 6U, 7U, 10U, 11U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 6U, 7U, 10U, 11U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 6U, 7U, 10U, 11U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 4U, 5U, 6U, 7U, 10U, 11U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 6U, 7U, 10U, 11U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 6U, 7U, 10U, 11U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 10U, 11U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 8U, 9U, 10U, 11U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 8U, 9U, 10U, 11U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 8U, 9U, 10U, 11U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 8U, 9U, 10U, 11U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 4U, 5U, 8U, 9U, 10U, 11U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 8U, 9U, 10U, 11U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 8U, 9U, 10U, 11U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 8U, 9U, 10U, 11U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 6U, 7U, 8U, 9U, 10U, 11U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 6U, 7U, 8U, 9U, 10U, 11U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 6U, 7U, 8U, 9U, 10U, 11U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 6U, 7U, 8U, 9U, 10U, 11U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 255U,
                          255U, 255U, 255U },
                        { 12U, 13U, 255U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 12U, 13U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 12U, 13U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 12U, 13U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 4U, 5U, 12U, 13U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 12U, 13U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 12U, 13U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 12U, 13U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 6U, 7U, 12U, 13U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 6U, 7U, 12U, 13U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 6U, 7U, 12U, 13U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 6U, 7U, 12U, 13U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 4U, 5U, 6U, 7U, 12U, 13U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 6U, 7U, 12U, 13U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 6U, 7U, 12U, 13U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 12U, 13U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 8U, 9U, 12U, 13U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 8U, 9U, 12U, 13U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 8U, 9U, 12U, 13U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 8U, 9U, 12U, 13U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 4U, 5U, 8U, 9U, 12U, 13U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 8U, 9U, 12U, 13U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 8U, 9U, 12U, 13U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 8U, 9U, 12U, 13U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 6U, 7U, 8U, 9U, 12U, 13U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 6U, 7U, 8U, 9U, 12U, 13U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 6U, 7U, 8U, 9U, 12U, 13U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 6U, 7U, 8U, 9U, 12U, 13U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 4U, 5U, 6U, 7U, 8U, 9U, 12U, 13U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 6U, 7U, 8U, 9U, 12U, 13U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 12U, 13U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 12U, 13U, 255U,
                          255U, 255U, 255U },
                        { 10U, 11U, 12U, 13U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 10U, 11U, 12U, 13U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 10U, 11U, 12U, 13U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 10U, 11U, 12U, 13U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 4U, 5U, 10U, 11U, 12U, 13U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 10U, 11U, 12U, 13U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 10U, 11U, 12U, 13U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 10U, 11U, 12U, 13U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 6U, 7U, 10U, 11U, 12U, 13U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 6U, 7U, 10U, 11U, 12U, 13U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 6U, 7U, 10U, 11U, 12U, 13U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 6U, 7U, 10U, 11U, 12U, 13U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 4U, 5U, 6U, 7U, 10U, 11U, 12U, 13U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 6U, 7U, 10U, 11U, 12U, 13U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 6U, 7U, 10U, 11U, 12U, 13U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 10U, 11U, 12U, 13U,
                          255U, 255U, 255U, 255U },
                        { 8U, 9U, 10U, 11U, 12U, 13U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 8U, 9U, 10U, 11U, 12U, 13U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 8U, 9U, 10U, 11U, 12U, 13U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 8U, 9U, 10U, 11U, 12U, 13U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 4U, 5U, 8U, 9U, 10U, 11U, 12U, 13U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 8U, 9U, 10U, 11U, 12U, 13U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 8U, 9U, 10U, 11U, 12U, 13U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 8U, 9U, 10U, 11U, 12U, 13U,
                          255U, 255U, 255U, 255U },
                        { 6U, 7U, 8U, 9U, 10U, 11U, 12U, 13U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 6U, 7U, 8U, 9U, 10U, 11U, 12U, 13U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 2U, 3U, 6U, 7U, 8U, 9U, 10U, 11U, 12U, 13U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 6U, 7U, 8U, 9U, 10U, 11U, 12U, 13U,
                          255U, 255U, 255U, 255U },
                        { 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U, 13U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U, 13U,
                          255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U, 13U,
                          255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U,
                          13U, 255U, 255U },
                        { 14U, 15U, 255U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 14U, 15U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 14U, 15U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 14U, 15U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 4U, 5U, 14U, 15U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 14U, 15U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 14U, 15U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 6U, 7U, 14U, 15U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 6U, 7U, 14U, 15U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 6U, 7U, 14U, 15U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 6U, 7U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 4U, 5U, 6U, 7U, 14U, 15U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 6U, 7U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 6U, 7U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 14U, 15U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 8U, 9U, 14U, 15U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 8U, 9U, 14U, 15U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 8U, 9U, 14U, 15U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 8U, 9U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 4U, 5U, 8U, 9U, 14U, 15U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 8U, 9U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 8U, 9U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 8U, 9U, 14U, 15U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 6U, 7U, 8U, 9U, 14U, 15U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 6U, 7U, 8U, 9U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 6U, 7U, 8U, 9U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 6U, 7U, 8U, 9U, 14U, 15U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 4U, 5U, 6U, 7U, 8U, 9U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 6U, 7U, 8U, 9U, 14U, 15U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 14U, 15U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 14U, 15U, 255U,
                          255U, 255U, 255U },
                        { 10U, 11U, 14U, 15U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 10U, 11U, 14U, 15U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 10U, 11U, 14U, 15U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 10U, 11U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 4U, 5U, 10U, 11U, 14U, 15U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 10U, 11U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 10U, 11U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 10U, 11U, 14U, 15U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 6U, 7U, 10U, 11U, 14U, 15U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 6U, 7U, 10U, 11U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 6U, 7U, 10U, 11U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 6U, 7U, 10U, 11U, 14U, 15U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 4U, 5U, 6U, 7U, 10U, 11U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 6U, 7U, 10U, 11U, 14U, 15U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 6U, 7U, 10U, 11U, 14U, 15U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 10U, 11U, 14U, 15U,
                          255U, 255U, 255U, 255U },
                        { 8U, 9U, 10U, 11U, 14U, 15U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 8U, 9U, 10U, 11U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 8U, 9U, 10U, 11U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 8U, 9U, 10U, 11U, 14U, 15U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 4U, 5U, 8U, 9U, 10U, 11U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 8U, 9U, 10U, 11U, 14U, 15U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 8U, 9U, 10U, 11U, 14U, 15U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 8U, 9U, 10U, 11U, 14U, 15U,
                          255U, 255U, 255U, 255U },
                        { 6U, 7U, 8U, 9U, 10U, 11U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 6U, 7U, 8U, 9U, 10U, 11U, 14U, 15U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 2U, 3U, 6U, 7U, 8U, 9U, 10U, 11U, 14U, 15U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 6U, 7U, 8U, 9U, 10U, 11U, 14U, 15U,
                          255U, 255U, 255U, 255U },
                        { 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 14U, 15U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 14U, 15U,
                          255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 14U, 15U,
                          255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 14U,
                          15U, 255U, 255U },
                        { 12U, 13U, 14U, 15U, 255U, 255U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 12U, 13U, 14U, 15U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 12U, 13U, 14U, 15U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 12U, 13U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 4U, 5U, 12U, 13U, 14U, 15U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 12U, 13U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 12U, 13U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 12U, 13U, 14U, 15U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 6U, 7U, 12U, 13U, 14U, 15U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 6U, 7U, 12U, 13U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 6U, 7U, 12U, 13U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 6U, 7U, 12U, 13U, 14U, 15U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 4U, 5U, 6U, 7U, 12U, 13U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 6U, 7U, 12U, 13U, 14U, 15U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 6U, 7U, 12U, 13U, 14U, 15U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 12U, 13U, 14U, 15U,
                          255U, 255U, 255U, 255U },
                        { 8U, 9U, 12U, 13U, 14U, 15U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 8U, 9U, 12U, 13U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 8U, 9U, 12U, 13U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 8U, 9U, 12U, 13U, 14U, 15U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 4U, 5U, 8U, 9U, 12U, 13U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 8U, 9U, 12U, 13U, 14U, 15U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 8U, 9U, 12U, 13U, 14U, 15U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 8U, 9U, 12U, 13U, 14U, 15U,
                          255U, 255U, 255U, 255U },
                        { 6U, 7U, 8U, 9U, 12U, 13U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 6U, 7U, 8U, 9U, 12U, 13U, 14U, 15U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 2U, 3U, 6U, 7U, 8U, 9U, 12U, 13U, 14U, 15U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 6U, 7U, 8U, 9U, 12U, 13U, 14U, 15U,
                          255U, 255U, 255U, 255U },
                        { 4U, 5U, 6U, 7U, 8U, 9U, 12U, 13U, 14U, 15U, 255U, 255U,
                          255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 6U, 7U, 8U, 9U, 12U, 13U, 14U, 15U,
                          255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 12U, 13U, 14U, 15U,
                          255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 12U, 13U, 14U,
                          15U, 255U, 255U },
                        { 10U, 11U, 12U, 13U, 14U, 15U, 255U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 10U, 11U, 12U, 13U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 10U, 11U, 12U, 13U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 10U, 11U, 12U, 13U, 14U, 15U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 4U, 5U, 10U, 11U, 12U, 13U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 10U, 11U, 12U, 13U, 14U, 15U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 10U, 11U, 12U, 13U, 14U, 15U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 10U, 11U, 12U, 13U, 14U, 15U,
                          255U, 255U, 255U, 255U },
                        { 6U, 7U, 10U, 11U, 12U, 13U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 6U, 7U, 10U, 11U, 12U, 13U, 14U, 15U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 6U, 7U, 10U, 11U, 12U, 13U, 14U, 15U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 6U, 7U, 10U, 11U, 12U, 13U, 14U, 15U,
                          255U, 255U, 255U, 255U },
                        { 4U, 5U, 6U, 7U, 10U, 11U, 12U, 13U, 14U, 15U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 6U, 7U, 10U, 11U, 12U, 13U, 14U, 15U,
                          255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 6U, 7U, 10U, 11U, 12U, 13U, 14U, 15U,
                          255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 10U, 11U, 12U, 13U, 14U,
                          15U, 255U, 255U },
                        { 8U, 9U, 10U, 11U, 12U, 13U, 14U, 15U, 255U, 255U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 8U, 9U, 10U, 11U, 12U, 13U, 14U, 15U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 2U, 3U, 8U, 9U, 10U, 11U, 12U, 13U, 14U, 15U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 8U, 9U, 10U, 11U, 12U, 13U, 14U, 15U,
                          255U, 255U, 255U, 255U },
                        { 4U, 5U, 8U, 9U, 10U, 11U, 12U, 13U, 14U, 15U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 8U, 9U, 10U, 11U, 12U, 13U, 14U, 15U,
                          255U, 255U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 8U, 9U, 10U, 11U, 12U, 13U, 14U, 15U,
                          255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 8U, 9U, 10U, 11U, 12U, 13U, 14U,
                          15U, 255U, 255U },
                        { 6U, 7U, 8U, 9U, 10U, 11U, 12U, 13U, 14U, 15U, 255U,
                          255U, 255U, 255U, 255U, 255U },
                        { 0U, 1U, 6U, 7U, 8U, 9U, 10U, 11U, 12U, 13U, 14U, 15U,
                          255U, 255U, 255U, 255U },
                        { 2U, 3U, 6U, 7U, 8U, 9U, 10U, 11U, 12U, 13U, 14U, 15U,
                          255U, 255U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 6U, 7U, 8U, 9U, 10U, 11U, 12U, 13U, 14U,
                          15U, 255U, 255U },
                        { 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U, 13U, 14U, 15U,
                          255U, 255U, 255U, 255U },
                        { 0U, 1U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U, 13U, 14U,
                          15U, 255U, 255U },
                        { 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U, 13U, 14U,
                          15U, 255U, 255U },
                        { 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U,
                          13U, 14U, 15U } };

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_ZERO_0d(void)
{
    return libcrux_ml_kem_vector_portable_vector_type_zero();
}

KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_arithmetic_add(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector lhs,
    libcrux_ml_kem_vector_portable_vector_type_PortableVector *rhs)
{
    for (size_t i = (size_t)0U;
         i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
        size_t i0 = i;
        size_t uu____0 = i0;
        lhs.elements[uu____0] = lhs.elements[uu____0] + rhs->elements[i0];
    }
    return lhs;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_add_0d(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector lhs,
    libcrux_ml_kem_vector_portable_vector_type_PortableVector *rhs)
{
    return libcrux_ml_kem_vector_portable_arithmetic_add(lhs, rhs);
}

KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_arithmetic_sub(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector lhs,
    libcrux_ml_kem_vector_portable_vector_type_PortableVector *rhs)
{
    for (size_t i = (size_t)0U;
         i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
        size_t i0 = i;
        size_t uu____0 = i0;
        lhs.elements[uu____0] = lhs.elements[uu____0] - rhs->elements[i0];
    }
    return lhs;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_sub_0d(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector lhs,
    libcrux_ml_kem_vector_portable_vector_type_PortableVector *rhs)
{
    return libcrux_ml_kem_vector_portable_arithmetic_sub(lhs, rhs);
}

KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_arithmetic_multiply_by_constant(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v, int16_t c)
{
    for (size_t i = (size_t)0U;
         i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
        size_t i0 = i;
        size_t uu____0 = i0;
        v.elements[uu____0] = v.elements[uu____0] * c;
    }
    return v;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_multiply_by_constant_0d(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v, int16_t c)
{
    return libcrux_ml_kem_vector_portable_arithmetic_multiply_by_constant(v, c);
}

KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_arithmetic_bitwise_and_with_constant(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v, int16_t c)
{
    for (size_t i = (size_t)0U;
         i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
        size_t i0 = i;
        size_t uu____0 = i0;
        v.elements[uu____0] = v.elements[uu____0] & c;
    }
    return v;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_bitwise_and_with_constant_0d(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v, int16_t c)
{
    return libcrux_ml_kem_vector_portable_arithmetic_bitwise_and_with_constant(v,
                                                                               c);
}

KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_arithmetic_cond_subtract_3329(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v)
{
    core_ops_range_Range_b3 iter =
        core_iter_traits_collect___core__iter__traits__collect__IntoIterator_for_I__1__into_iter(
            (CLITERAL(core_ops_range_Range_b3){
                .start = (size_t)0U,
                .end = LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR }),
            core_ops_range_Range_b3, core_ops_range_Range_b3);
    while (true) {
        core_option_Option_b3 uu____0 =
            core_iter_range___core__iter__traits__iterator__Iterator_for_core__ops__range__Range_A___6__next(
                &iter, size_t, core_option_Option_b3);
        if (!(uu____0.tag == core_option_None)) {
            size_t i = uu____0.f0;
            if (v.elements[i] >= (int16_t)3329) {
                size_t uu____1 = i;
                v.elements[uu____1] = v.elements[uu____1] - (int16_t)3329;
            }
            continue;
        }
        return v;
    }
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_cond_subtract_3329_0d(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v)
{
    return libcrux_ml_kem_vector_portable_arithmetic_cond_subtract_3329(v);
}

/**
 Signed Barrett Reduction

 Given an input `value`, `barrett_reduce` outputs a representative `result`
 such that:

 - result ≡ value (mod FIELD_MODULUS)
 - the absolute value of `result` is bound as follows:

 `|result| ≤ FIELD_MODULUS / 2 · (|value|/BARRETT_R + 1)

 In particular, if `|value| < BARRETT_R`, then `|result| < FIELD_MODULUS`.
*/
int16_t
libcrux_ml_kem_vector_portable_arithmetic_barrett_reduce_element(
    int16_t value)
{
    int32_t t = (int32_t)value *
                    LIBCRUX_ML_KEM_VECTOR_PORTABLE_ARITHMETIC_BARRETT_MULTIPLIER +
                (LIBCRUX_ML_KEM_VECTOR_PORTABLE_ARITHMETIC_BARRETT_R >> 1U);
    int16_t quotient =
        (int16_t)(t >>
                  (uint32_t)
                      LIBCRUX_ML_KEM_VECTOR_PORTABLE_ARITHMETIC_BARRETT_SHIFT);
    return value - quotient * LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_MODULUS;
}

KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_arithmetic_barrett_reduce(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v)
{
    for (size_t i = (size_t)0U;
         i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
        size_t i0 = i;
        v.elements[i0] =
            libcrux_ml_kem_vector_portable_arithmetic_barrett_reduce_element(
                v.elements[i0]);
    }
    return v;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_barrett_reduce_0d(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v)
{
    return libcrux_ml_kem_vector_portable_arithmetic_barrett_reduce(v);
}

/**
 Signed Montgomery Reduction

 Given an input `value`, `montgomery_reduce` outputs a representative `o`
 such that:

 - o ≡ value · MONTGOMERY_R^(-1) (mod FIELD_MODULUS)
 - the absolute value of `o` is bound as follows:

 `|result| ≤ (|value| / MONTGOMERY_R) + (FIELD_MODULUS / 2)

 In particular, if `|value| ≤ FIELD_MODULUS * MONTGOMERY_R`, then `|o| < (3 ·
 FIELD_MODULUS) / 2`.
*/
int16_t
libcrux_ml_kem_vector_portable_arithmetic_montgomery_reduce_element(
    int32_t value)
{
    int32_t k =
        (int32_t)(int16_t)value *
        (int32_t)LIBCRUX_ML_KEM_VECTOR_TRAITS_INVERSE_OF_MODULUS_MOD_MONTGOMERY_R;
    int32_t k_times_modulus =
        (int32_t)(int16_t)k * (int32_t)LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_MODULUS;
    int16_t c =
        (int16_t)(k_times_modulus >>
                  (uint32_t)
                      LIBCRUX_ML_KEM_VECTOR_PORTABLE_ARITHMETIC_MONTGOMERY_SHIFT);
    int16_t value_high =
        (int16_t)(value >>
                  (uint32_t)
                      LIBCRUX_ML_KEM_VECTOR_PORTABLE_ARITHMETIC_MONTGOMERY_SHIFT);
    return value_high - c;
}

/**
 If `fe` is some field element 'x' of the Kyber field and `fer` is congruent to
 `y · MONTGOMERY_R`, this procedure outputs a value that is congruent to
 `x · y`, as follows:

    `fe · fer ≡ x · y · MONTGOMERY_R (mod FIELD_MODULUS)`

 `montgomery_reduce` takes the value `x · y · MONTGOMERY_R` and outputs a
 representative `x · y · MONTGOMERY_R * MONTGOMERY_R^{-1} ≡ x · y (mod
 FIELD_MODULUS)`.
*/
KRML_MUSTINLINE int16_t
libcrux_ml_kem_vector_portable_arithmetic_montgomery_multiply_fe_by_fer(
    int16_t fe, int16_t fer)
{
    return libcrux_ml_kem_vector_portable_arithmetic_montgomery_reduce_element(
        (int32_t)fe * (int32_t)fer);
}

KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_arithmetic_montgomery_multiply_by_constant(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v, int16_t c)
{
    for (size_t i = (size_t)0U;
         i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
        size_t i0 = i;
        v.elements[i0] =
            libcrux_ml_kem_vector_portable_arithmetic_montgomery_multiply_fe_by_fer(
                v.elements[i0], c);
    }
    return v;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_montgomery_multiply_by_constant_0d(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v, int16_t r)
{
    return libcrux_ml_kem_vector_portable_arithmetic_montgomery_multiply_by_constant(
        v, r);
}

/**
 The `compress_*` functions implement the `Compress` function specified in the
 NIST FIPS 203 standard (Page 18, Expression 4.5), which is defined as:

 ```plaintext
 Compress_d: ℤq -> ℤ_{2ᵈ}
 Compress_d(x) = ⌈(2ᵈ/q)·x⌋
 ```

 Since `⌈x⌋ = ⌊x + 1/2⌋` we have:

 ```plaintext
 Compress_d(x) = ⌊(2ᵈ/q)·x + 1/2⌋
               = ⌊(2^{d+1}·x + q) / 2q⌋
 ```

 For further information about the function implementations, consult the
 `implementation_notes.pdf` document in this directory.

 The NIST FIPS 203 standard can be found at
 <https://csrc.nist.gov/pubs/fips/203/ipd>.
*/
uint8_t
libcrux_ml_kem_vector_portable_compress_compress_message_coefficient(
    uint16_t fe)
{
    int16_t shifted = (int16_t)1664 - (int16_t)fe;
    int16_t mask = shifted >> 15U;
    int16_t shifted_to_positive = mask ^ shifted;
    int16_t shifted_positive_in_range = shifted_to_positive - (int16_t)832;
    return (uint8_t)(shifted_positive_in_range >> 15U & (int16_t)1);
}

KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_compress_compress_1(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v)
{
    for (size_t i = (size_t)0U;
         i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
        size_t i0 = i;
        v.elements[i0] = (int16_t)
            libcrux_ml_kem_vector_portable_compress_compress_message_coefficient(
                (uint16_t)v.elements[i0]);
    }
    return v;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_compress_1_0d(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v)
{
    return libcrux_ml_kem_vector_portable_compress_compress_1(v);
}

KRML_MUSTINLINE uint32_t
libcrux_ml_kem_vector_portable_arithmetic_get_n_least_significant_bits(
    uint8_t n, uint32_t value)
{
    return value & ((1U << (uint32_t)n) - 1U);
}

int16_t
libcrux_ml_kem_vector_portable_compress_compress_ciphertext_coefficient(
    uint8_t coefficient_bits, uint16_t fe)
{
    uint64_t compressed = (uint64_t)fe << (uint32_t)coefficient_bits;
    compressed = compressed + 1664ULL;
    compressed = compressed * 10321340ULL;
    compressed = compressed >> 35U;
    return (int16_t)
        libcrux_ml_kem_vector_portable_arithmetic_get_n_least_significant_bits(
            coefficient_bits, (uint32_t)compressed);
}

KRML_MUSTINLINE void
libcrux_ml_kem_vector_portable_ntt_ntt_step(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector *v, int16_t zeta,
    size_t i, size_t j)
{
    int16_t t =
        libcrux_ml_kem_vector_portable_arithmetic_montgomery_multiply_fe_by_fer(
            v->elements[j], zeta);
    v->elements[j] = v->elements[i] - t;
    v->elements[i] = v->elements[i] + t;
}

KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_ntt_ntt_layer_1_step(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v, int16_t zeta0,
    int16_t zeta1, int16_t zeta2, int16_t zeta3)
{
    libcrux_ml_kem_vector_portable_ntt_ntt_step(&v, zeta0, (size_t)0U,
                                                (size_t)2U);
    libcrux_ml_kem_vector_portable_ntt_ntt_step(&v, zeta0, (size_t)1U,
                                                (size_t)3U);
    libcrux_ml_kem_vector_portable_ntt_ntt_step(&v, zeta1, (size_t)4U,
                                                (size_t)6U);
    libcrux_ml_kem_vector_portable_ntt_ntt_step(&v, zeta1, (size_t)5U,
                                                (size_t)7U);
    libcrux_ml_kem_vector_portable_ntt_ntt_step(&v, zeta2, (size_t)8U,
                                                (size_t)10U);
    libcrux_ml_kem_vector_portable_ntt_ntt_step(&v, zeta2, (size_t)9U,
                                                (size_t)11U);
    libcrux_ml_kem_vector_portable_ntt_ntt_step(&v, zeta3, (size_t)12U,
                                                (size_t)14U);
    libcrux_ml_kem_vector_portable_ntt_ntt_step(&v, zeta3, (size_t)13U,
                                                (size_t)15U);
    return v;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_ntt_layer_1_step_0d(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a, int16_t zeta0,
    int16_t zeta1, int16_t zeta2, int16_t zeta3)
{
    return libcrux_ml_kem_vector_portable_ntt_ntt_layer_1_step(a, zeta0, zeta1,
                                                               zeta2, zeta3);
}

KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_ntt_ntt_layer_2_step(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v, int16_t zeta0,
    int16_t zeta1)
{
    libcrux_ml_kem_vector_portable_ntt_ntt_step(&v, zeta0, (size_t)0U,
                                                (size_t)4U);
    libcrux_ml_kem_vector_portable_ntt_ntt_step(&v, zeta0, (size_t)1U,
                                                (size_t)5U);
    libcrux_ml_kem_vector_portable_ntt_ntt_step(&v, zeta0, (size_t)2U,
                                                (size_t)6U);
    libcrux_ml_kem_vector_portable_ntt_ntt_step(&v, zeta0, (size_t)3U,
                                                (size_t)7U);
    libcrux_ml_kem_vector_portable_ntt_ntt_step(&v, zeta1, (size_t)8U,
                                                (size_t)12U);
    libcrux_ml_kem_vector_portable_ntt_ntt_step(&v, zeta1, (size_t)9U,
                                                (size_t)13U);
    libcrux_ml_kem_vector_portable_ntt_ntt_step(&v, zeta1, (size_t)10U,
                                                (size_t)14U);
    libcrux_ml_kem_vector_portable_ntt_ntt_step(&v, zeta1, (size_t)11U,
                                                (size_t)15U);
    return v;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_ntt_layer_2_step_0d(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a, int16_t zeta0,
    int16_t zeta1)
{
    return libcrux_ml_kem_vector_portable_ntt_ntt_layer_2_step(a, zeta0, zeta1);
}

KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_ntt_ntt_layer_3_step(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v, int16_t zeta)
{
    libcrux_ml_kem_vector_portable_ntt_ntt_step(&v, zeta, (size_t)0U, (size_t)8U);
    libcrux_ml_kem_vector_portable_ntt_ntt_step(&v, zeta, (size_t)1U, (size_t)9U);
    libcrux_ml_kem_vector_portable_ntt_ntt_step(&v, zeta, (size_t)2U,
                                                (size_t)10U);
    libcrux_ml_kem_vector_portable_ntt_ntt_step(&v, zeta, (size_t)3U,
                                                (size_t)11U);
    libcrux_ml_kem_vector_portable_ntt_ntt_step(&v, zeta, (size_t)4U,
                                                (size_t)12U);
    libcrux_ml_kem_vector_portable_ntt_ntt_step(&v, zeta, (size_t)5U,
                                                (size_t)13U);
    libcrux_ml_kem_vector_portable_ntt_ntt_step(&v, zeta, (size_t)6U,
                                                (size_t)14U);
    libcrux_ml_kem_vector_portable_ntt_ntt_step(&v, zeta, (size_t)7U,
                                                (size_t)15U);
    return v;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_ntt_layer_3_step_0d(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a, int16_t zeta)
{
    return libcrux_ml_kem_vector_portable_ntt_ntt_layer_3_step(a, zeta);
}

KRML_MUSTINLINE void
libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector *v, int16_t zeta,
    size_t i, size_t j)
{
    int16_t a_minus_b = v->elements[j] - v->elements[i];
    v->elements[i] =
        libcrux_ml_kem_vector_portable_arithmetic_barrett_reduce_element(
            v->elements[i] + v->elements[j]);
    v->elements[j] =
        libcrux_ml_kem_vector_portable_arithmetic_montgomery_multiply_fe_by_fer(
            a_minus_b, zeta);
}

KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_ntt_inv_ntt_layer_1_step(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v, int16_t zeta0,
    int16_t zeta1, int16_t zeta2, int16_t zeta3)
{
    libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&v, zeta0, (size_t)0U,
                                                    (size_t)2U);
    libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&v, zeta0, (size_t)1U,
                                                    (size_t)3U);
    libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&v, zeta1, (size_t)4U,
                                                    (size_t)6U);
    libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&v, zeta1, (size_t)5U,
                                                    (size_t)7U);
    libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&v, zeta2, (size_t)8U,
                                                    (size_t)10U);
    libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&v, zeta2, (size_t)9U,
                                                    (size_t)11U);
    libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&v, zeta3, (size_t)12U,
                                                    (size_t)14U);
    libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&v, zeta3, (size_t)13U,
                                                    (size_t)15U);
    return v;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_inv_ntt_layer_1_step_0d(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a, int16_t zeta0,
    int16_t zeta1, int16_t zeta2, int16_t zeta3)
{
    return libcrux_ml_kem_vector_portable_ntt_inv_ntt_layer_1_step(
        a, zeta0, zeta1, zeta2, zeta3);
}

KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_ntt_inv_ntt_layer_2_step(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v, int16_t zeta0,
    int16_t zeta1)
{
    libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&v, zeta0, (size_t)0U,
                                                    (size_t)4U);
    libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&v, zeta0, (size_t)1U,
                                                    (size_t)5U);
    libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&v, zeta0, (size_t)2U,
                                                    (size_t)6U);
    libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&v, zeta0, (size_t)3U,
                                                    (size_t)7U);
    libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&v, zeta1, (size_t)8U,
                                                    (size_t)12U);
    libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&v, zeta1, (size_t)9U,
                                                    (size_t)13U);
    libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&v, zeta1, (size_t)10U,
                                                    (size_t)14U);
    libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&v, zeta1, (size_t)11U,
                                                    (size_t)15U);
    return v;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_inv_ntt_layer_2_step_0d(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a, int16_t zeta0,
    int16_t zeta1)
{
    return libcrux_ml_kem_vector_portable_ntt_inv_ntt_layer_2_step(a, zeta0,
                                                                   zeta1);
}

KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_ntt_inv_ntt_layer_3_step(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v, int16_t zeta)
{
    libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&v, zeta, (size_t)0U,
                                                    (size_t)8U);
    libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&v, zeta, (size_t)1U,
                                                    (size_t)9U);
    libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&v, zeta, (size_t)2U,
                                                    (size_t)10U);
    libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&v, zeta, (size_t)3U,
                                                    (size_t)11U);
    libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&v, zeta, (size_t)4U,
                                                    (size_t)12U);
    libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&v, zeta, (size_t)5U,
                                                    (size_t)13U);
    libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&v, zeta, (size_t)6U,
                                                    (size_t)14U);
    libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&v, zeta, (size_t)7U,
                                                    (size_t)15U);
    return v;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_inv_ntt_layer_3_step_0d(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a, int16_t zeta)
{
    return libcrux_ml_kem_vector_portable_ntt_inv_ntt_layer_3_step(a, zeta);
}

/**
 Compute the product of two Kyber binomials with respect to the
 modulus `X² - zeta`.

 This function almost implements <strong>Algorithm 11</strong> of the
 NIST FIPS 203 standard, which is reproduced below:

 ```plaintext
 Input:  a₀, a₁, b₀, b₁ ∈ ℤq.
 Input: γ ∈ ℤq.
 Output: c₀, c₁ ∈ ℤq.

 c₀ ← a₀·b₀ + a₁·b₁·γ
 c₁ ← a₀·b₁ + a₁·b₀
 return c₀, c₁
 ```
 We say "almost" because the coefficients output by this function are in
 the Montgomery domain (unlike in the specification).

 The NIST FIPS 203 standard can be found at
 <https://csrc.nist.gov/pubs/fips/203/ipd>.
*/
KRML_MUSTINLINE void
libcrux_ml_kem_vector_portable_ntt_ntt_multiply_binomials(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector *a,
    libcrux_ml_kem_vector_portable_vector_type_PortableVector *b, int16_t zeta,
    size_t i, size_t j,
    libcrux_ml_kem_vector_portable_vector_type_PortableVector *out)
{
    int16_t o0 = libcrux_ml_kem_vector_portable_arithmetic_montgomery_reduce_element(
        (int32_t)a->elements[i] * (int32_t)b->elements[i] +
        (int32_t)
                libcrux_ml_kem_vector_portable_arithmetic_montgomery_reduce_element(
                    (int32_t)a->elements[j] * (int32_t)b->elements[j]) *
            (int32_t)zeta);
    int16_t o1 =
        libcrux_ml_kem_vector_portable_arithmetic_montgomery_reduce_element(
            (int32_t)a->elements[i] * (int32_t)b->elements[j] +
            (int32_t)a->elements[j] * (int32_t)b->elements[i]);
    out->elements[i] = o0;
    out->elements[j] = o1;
}

KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_ntt_ntt_multiply(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector *lhs,
    libcrux_ml_kem_vector_portable_vector_type_PortableVector *rhs,
    int16_t zeta0, int16_t zeta1, int16_t zeta2, int16_t zeta3)
{
    libcrux_ml_kem_vector_portable_vector_type_PortableVector out =
        libcrux_ml_kem_vector_portable_vector_type_zero();
    libcrux_ml_kem_vector_portable_ntt_ntt_multiply_binomials(
        lhs, rhs, zeta0, (size_t)0U, (size_t)1U, &out);
    libcrux_ml_kem_vector_portable_ntt_ntt_multiply_binomials(
        lhs, rhs, -zeta0, (size_t)2U, (size_t)3U, &out);
    libcrux_ml_kem_vector_portable_ntt_ntt_multiply_binomials(
        lhs, rhs, zeta1, (size_t)4U, (size_t)5U, &out);
    libcrux_ml_kem_vector_portable_ntt_ntt_multiply_binomials(
        lhs, rhs, -zeta1, (size_t)6U, (size_t)7U, &out);
    libcrux_ml_kem_vector_portable_ntt_ntt_multiply_binomials(
        lhs, rhs, zeta2, (size_t)8U, (size_t)9U, &out);
    libcrux_ml_kem_vector_portable_ntt_ntt_multiply_binomials(
        lhs, rhs, -zeta2, (size_t)10U, (size_t)11U, &out);
    libcrux_ml_kem_vector_portable_ntt_ntt_multiply_binomials(
        lhs, rhs, zeta3, (size_t)12U, (size_t)13U, &out);
    libcrux_ml_kem_vector_portable_ntt_ntt_multiply_binomials(
        lhs, rhs, -zeta3, (size_t)14U, (size_t)15U, &out);
    return out;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_ntt_multiply_0d(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector *lhs,
    libcrux_ml_kem_vector_portable_vector_type_PortableVector *rhs,
    int16_t zeta0, int16_t zeta1, int16_t zeta2, int16_t zeta3)
{
    return libcrux_ml_kem_vector_portable_ntt_ntt_multiply(lhs, rhs, zeta0, zeta1,
                                                           zeta2, zeta3);
}

KRML_MUSTINLINE void
libcrux_ml_kem_vector_portable_serialize_serialize_1(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v,
    uint8_t ret[2U])
{
    uint8_t result[2U] = { 0U };
    KRML_MAYBE_FOR8(
        i, (size_t)0U, (size_t)8U, (size_t)1U, size_t i0 = i;
        size_t uu____0 = (size_t)0U;
        result[uu____0] = (uint32_t)result[uu____0] |
                          (uint32_t)(uint8_t)v.elements[i0] << (uint32_t)i0;);
    KRML_MAYBE_FOR8(i, (size_t)8U, (size_t)16U, (size_t)1U, size_t i0 = i;
                    size_t uu____1 = (size_t)1U;
                    result[uu____1] = (uint32_t)result[uu____1] |
                                      (uint32_t)(uint8_t)v.elements[i0]
                                          << (uint32_t)(i0 - (size_t)8U););
    memcpy(ret, result, (size_t)2U * sizeof(uint8_t));
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
void
libcrux_ml_kem_vector_portable_serialize_1_0d(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a,
    uint8_t ret[2U])
{
    libcrux_ml_kem_vector_portable_serialize_serialize_1(a, ret);
}

KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_serialize_deserialize_1(Eurydice_slice v)
{
    libcrux_ml_kem_vector_portable_vector_type_PortableVector result =
        libcrux_ml_kem_vector_portable_vector_type_zero();
    KRML_MAYBE_FOR8(
        i, (size_t)0U, (size_t)8U, (size_t)1U, size_t i0 = i;
        result.elements[i0] = (int16_t)((uint32_t)Eurydice_slice_index(
                                            v, (size_t)0U, uint8_t, uint8_t *) >>
                                            (uint32_t)i0 &
                                        1U););
    for (size_t i = (size_t)8U;
         i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
        size_t i0 = i;
        result.elements[i0] = (int16_t)((uint32_t)Eurydice_slice_index(
                                            v, (size_t)1U, uint8_t, uint8_t *) >>
                                            (uint32_t)(i0 - (size_t)8U) &
                                        1U);
    }
    return result;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_deserialize_1_0d(Eurydice_slice a)
{
    return libcrux_ml_kem_vector_portable_serialize_deserialize_1(a);
}

KRML_MUSTINLINE uint8_t_x4
libcrux_ml_kem_vector_portable_serialize_serialize_4_int(Eurydice_slice v)
{
    uint8_t result0 =
        (uint32_t)(uint8_t)Eurydice_slice_index(v, (size_t)1U, int16_t, int16_t *)
            << 4U |
        (uint32_t)(uint8_t)Eurydice_slice_index(v, (size_t)0U, int16_t,
                                                int16_t *);
    uint8_t result1 =
        (uint32_t)(uint8_t)Eurydice_slice_index(v, (size_t)3U, int16_t, int16_t *)
            << 4U |
        (uint32_t)(uint8_t)Eurydice_slice_index(v, (size_t)2U, int16_t,
                                                int16_t *);
    uint8_t result2 =
        (uint32_t)(uint8_t)Eurydice_slice_index(v, (size_t)5U, int16_t, int16_t *)
            << 4U |
        (uint32_t)(uint8_t)Eurydice_slice_index(v, (size_t)4U, int16_t,
                                                int16_t *);
    uint8_t result3 =
        (uint32_t)(uint8_t)Eurydice_slice_index(v, (size_t)7U, int16_t, int16_t *)
            << 4U |
        (uint32_t)(uint8_t)Eurydice_slice_index(v, (size_t)6U, int16_t,
                                                int16_t *);
    return (CLITERAL(uint8_t_x4){
        .fst = result0, .snd = result1, .thd = result2, .f3 = result3 });
}

KRML_MUSTINLINE void
libcrux_ml_kem_vector_portable_serialize_serialize_4(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v,
    uint8_t ret[8U])
{
    uint8_t_x4 result0_3 =
        libcrux_ml_kem_vector_portable_serialize_serialize_4_int(
            Eurydice_array_to_subslice2(v.elements, (size_t)0U, (size_t)8U,
                                        int16_t));
    uint8_t_x4 result4_7 =
        libcrux_ml_kem_vector_portable_serialize_serialize_4_int(
            Eurydice_array_to_subslice2(v.elements, (size_t)8U, (size_t)16U,
                                        int16_t));
    uint8_t result[8U] = { 0U };
    result[0U] = result0_3.fst;
    result[1U] = result0_3.snd;
    result[2U] = result0_3.thd;
    result[3U] = result0_3.f3;
    result[4U] = result4_7.fst;
    result[5U] = result4_7.snd;
    result[6U] = result4_7.thd;
    result[7U] = result4_7.f3;
    memcpy(ret, result, (size_t)8U * sizeof(uint8_t));
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
void
libcrux_ml_kem_vector_portable_serialize_4_0d(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a,
    uint8_t ret[8U])
{
    libcrux_ml_kem_vector_portable_serialize_serialize_4(a, ret);
}

KRML_MUSTINLINE int16_t_x8
libcrux_ml_kem_vector_portable_serialize_deserialize_4_int(
    Eurydice_slice bytes)
{
    int16_t v0 = (int16_t)((uint32_t)Eurydice_slice_index(bytes, (size_t)0U,
                                                          uint8_t, uint8_t *) &
                           15U);
    int16_t v1 = (int16_t)((uint32_t)Eurydice_slice_index(bytes, (size_t)0U,
                                                          uint8_t, uint8_t *) >>
                               4U &
                           15U);
    int16_t v2 = (int16_t)((uint32_t)Eurydice_slice_index(bytes, (size_t)1U,
                                                          uint8_t, uint8_t *) &
                           15U);
    int16_t v3 = (int16_t)((uint32_t)Eurydice_slice_index(bytes, (size_t)1U,
                                                          uint8_t, uint8_t *) >>
                               4U &
                           15U);
    int16_t v4 = (int16_t)((uint32_t)Eurydice_slice_index(bytes, (size_t)2U,
                                                          uint8_t, uint8_t *) &
                           15U);
    int16_t v5 = (int16_t)((uint32_t)Eurydice_slice_index(bytes, (size_t)2U,
                                                          uint8_t, uint8_t *) >>
                               4U &
                           15U);
    int16_t v6 = (int16_t)((uint32_t)Eurydice_slice_index(bytes, (size_t)3U,
                                                          uint8_t, uint8_t *) &
                           15U);
    int16_t v7 = (int16_t)((uint32_t)Eurydice_slice_index(bytes, (size_t)3U,
                                                          uint8_t, uint8_t *) >>
                               4U &
                           15U);
    return (CLITERAL(int16_t_x8){ .fst = v0,
                                  .snd = v1,
                                  .thd = v2,
                                  .f3 = v3,
                                  .f4 = v4,
                                  .f5 = v5,
                                  .f6 = v6,
                                  .f7 = v7 });
}

KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_serialize_deserialize_4(Eurydice_slice bytes)
{
    int16_t_x8 v0_7 = libcrux_ml_kem_vector_portable_serialize_deserialize_4_int(
        Eurydice_slice_subslice2(bytes, (size_t)0U, (size_t)4U, uint8_t));
    int16_t_x8 v8_15 = libcrux_ml_kem_vector_portable_serialize_deserialize_4_int(
        Eurydice_slice_subslice2(bytes, (size_t)4U, (size_t)8U, uint8_t));
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v =
        libcrux_ml_kem_vector_portable_vector_type_zero();
    v.elements[0U] = v0_7.fst;
    v.elements[1U] = v0_7.snd;
    v.elements[2U] = v0_7.thd;
    v.elements[3U] = v0_7.f3;
    v.elements[4U] = v0_7.f4;
    v.elements[5U] = v0_7.f5;
    v.elements[6U] = v0_7.f6;
    v.elements[7U] = v0_7.f7;
    v.elements[8U] = v8_15.fst;
    v.elements[9U] = v8_15.snd;
    v.elements[10U] = v8_15.thd;
    v.elements[11U] = v8_15.f3;
    v.elements[12U] = v8_15.f4;
    v.elements[13U] = v8_15.f5;
    v.elements[14U] = v8_15.f6;
    v.elements[15U] = v8_15.f7;
    return v;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_deserialize_4_0d(Eurydice_slice a)
{
    return libcrux_ml_kem_vector_portable_serialize_deserialize_4(a);
}

KRML_MUSTINLINE uint8_t_x5
libcrux_ml_kem_vector_portable_serialize_serialize_5_int(Eurydice_slice v)
{
    uint8_t r0 =
        (uint8_t)(Eurydice_slice_index(v, (size_t)0U, int16_t, int16_t *) |
                  Eurydice_slice_index(v, (size_t)1U, int16_t, int16_t *) << 5U);
    uint8_t r1 =
        (uint8_t)((Eurydice_slice_index(v, (size_t)1U, int16_t, int16_t *) >> 3U |
                   Eurydice_slice_index(v, (size_t)2U, int16_t, int16_t *)
                       << 2U) |
                  Eurydice_slice_index(v, (size_t)3U, int16_t, int16_t *) << 7U);
    uint8_t r2 =
        (uint8_t)(Eurydice_slice_index(v, (size_t)3U, int16_t, int16_t *) >> 1U |
                  Eurydice_slice_index(v, (size_t)4U, int16_t, int16_t *) << 4U);
    uint8_t r3 =
        (uint8_t)((Eurydice_slice_index(v, (size_t)4U, int16_t, int16_t *) >> 4U |
                   Eurydice_slice_index(v, (size_t)5U, int16_t, int16_t *)
                       << 1U) |
                  Eurydice_slice_index(v, (size_t)6U, int16_t, int16_t *) << 6U);
    uint8_t r4 =
        (uint8_t)(Eurydice_slice_index(v, (size_t)6U, int16_t, int16_t *) >> 2U |
                  Eurydice_slice_index(v, (size_t)7U, int16_t, int16_t *) << 3U);
    return (CLITERAL(uint8_t_x5){
        .fst = r0, .snd = r1, .thd = r2, .f3 = r3, .f4 = r4 });
}

KRML_MUSTINLINE void
libcrux_ml_kem_vector_portable_serialize_serialize_5(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v,
    uint8_t ret[10U])
{
    uint8_t_x5 r0_4 = libcrux_ml_kem_vector_portable_serialize_serialize_5_int(
        Eurydice_array_to_subslice2(v.elements, (size_t)0U, (size_t)8U, int16_t));
    uint8_t_x5 r5_9 = libcrux_ml_kem_vector_portable_serialize_serialize_5_int(
        Eurydice_array_to_subslice2(v.elements, (size_t)8U, (size_t)16U,
                                    int16_t));
    uint8_t result[10U] = { 0U };
    result[0U] = r0_4.fst;
    result[1U] = r0_4.snd;
    result[2U] = r0_4.thd;
    result[3U] = r0_4.f3;
    result[4U] = r0_4.f4;
    result[5U] = r5_9.fst;
    result[6U] = r5_9.snd;
    result[7U] = r5_9.thd;
    result[8U] = r5_9.f3;
    result[9U] = r5_9.f4;
    memcpy(ret, result, (size_t)10U * sizeof(uint8_t));
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
void
libcrux_ml_kem_vector_portable_serialize_5_0d(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a,
    uint8_t ret[10U])
{
    libcrux_ml_kem_vector_portable_serialize_serialize_5(a, ret);
}

KRML_MUSTINLINE int16_t_x8
libcrux_ml_kem_vector_portable_serialize_deserialize_5_int(
    Eurydice_slice bytes)
{
    int16_t v0 = (int16_t)((uint32_t)Eurydice_slice_index(bytes, (size_t)0U,
                                                          uint8_t, uint8_t *) &
                           31U);
    int16_t v1 = (int16_t)(((uint32_t)Eurydice_slice_index(bytes, (size_t)1U,
                                                           uint8_t, uint8_t *) &
                            3U) << 3U |
                           (uint32_t)Eurydice_slice_index(bytes, (size_t)0U,
                                                          uint8_t, uint8_t *) >>
                               5U);
    int16_t v2 = (int16_t)((uint32_t)Eurydice_slice_index(bytes, (size_t)1U,
                                                          uint8_t, uint8_t *) >>
                               2U &
                           31U);
    int16_t v3 = (int16_t)(((uint32_t)Eurydice_slice_index(bytes, (size_t)2U,
                                                           uint8_t, uint8_t *) &
                            15U)
                               << 1U |
                           (uint32_t)Eurydice_slice_index(bytes, (size_t)1U,
                                                          uint8_t, uint8_t *) >>
                               7U);
    int16_t v4 = (int16_t)(((uint32_t)Eurydice_slice_index(bytes, (size_t)3U,
                                                           uint8_t, uint8_t *) &
                            1U) << 4U |
                           (uint32_t)Eurydice_slice_index(bytes, (size_t)2U,
                                                          uint8_t, uint8_t *) >>
                               4U);
    int16_t v5 = (int16_t)((uint32_t)Eurydice_slice_index(bytes, (size_t)3U,
                                                          uint8_t, uint8_t *) >>
                               1U &
                           31U);
    int16_t v6 = (int16_t)(((uint32_t)Eurydice_slice_index(bytes, (size_t)4U,
                                                           uint8_t, uint8_t *) &
                            7U) << 2U |
                           (uint32_t)Eurydice_slice_index(bytes, (size_t)3U,
                                                          uint8_t, uint8_t *) >>
                               6U);
    int16_t v7 = (int16_t)((uint32_t)Eurydice_slice_index(bytes, (size_t)4U,
                                                          uint8_t, uint8_t *) >>
                           3U);
    return (CLITERAL(int16_t_x8){ .fst = v0,
                                  .snd = v1,
                                  .thd = v2,
                                  .f3 = v3,
                                  .f4 = v4,
                                  .f5 = v5,
                                  .f6 = v6,
                                  .f7 = v7 });
}

KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_serialize_deserialize_5(Eurydice_slice bytes)
{
    int16_t_x8 v0_7 = libcrux_ml_kem_vector_portable_serialize_deserialize_5_int(
        Eurydice_slice_subslice2(bytes, (size_t)0U, (size_t)5U, uint8_t));
    int16_t_x8 v8_15 = libcrux_ml_kem_vector_portable_serialize_deserialize_5_int(
        Eurydice_slice_subslice2(bytes, (size_t)5U, (size_t)10U, uint8_t));
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v =
        libcrux_ml_kem_vector_portable_vector_type_zero();
    v.elements[0U] = v0_7.fst;
    v.elements[1U] = v0_7.snd;
    v.elements[2U] = v0_7.thd;
    v.elements[3U] = v0_7.f3;
    v.elements[4U] = v0_7.f4;
    v.elements[5U] = v0_7.f5;
    v.elements[6U] = v0_7.f6;
    v.elements[7U] = v0_7.f7;
    v.elements[8U] = v8_15.fst;
    v.elements[9U] = v8_15.snd;
    v.elements[10U] = v8_15.thd;
    v.elements[11U] = v8_15.f3;
    v.elements[12U] = v8_15.f4;
    v.elements[13U] = v8_15.f5;
    v.elements[14U] = v8_15.f6;
    v.elements[15U] = v8_15.f7;
    return v;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_deserialize_5_0d(Eurydice_slice a)
{
    return libcrux_ml_kem_vector_portable_serialize_deserialize_5(a);
}

KRML_MUSTINLINE uint8_t_x5
libcrux_ml_kem_vector_portable_serialize_serialize_10_int(Eurydice_slice v)
{
    uint8_t r0 =
        (uint8_t)(Eurydice_slice_index(v, (size_t)0U, int16_t, int16_t *) &
                  (int16_t)255);
    uint8_t r1 = (uint32_t)(uint8_t)(Eurydice_slice_index(v, (size_t)1U, int16_t,
                                                          int16_t *) &
                                     (int16_t)63)
                     << 2U |
                 (uint32_t)(uint8_t)(Eurydice_slice_index(v, (size_t)0U, int16_t,
                                                          int16_t *) >>
                                         8U &
                                     (int16_t)3);
    uint8_t r2 = (uint32_t)(uint8_t)(Eurydice_slice_index(v, (size_t)2U, int16_t,
                                                          int16_t *) &
                                     (int16_t)15)
                     << 4U |
                 (uint32_t)(uint8_t)(Eurydice_slice_index(v, (size_t)1U, int16_t,
                                                          int16_t *) >>
                                         6U &
                                     (int16_t)15);
    uint8_t r3 = (uint32_t)(uint8_t)(Eurydice_slice_index(v, (size_t)3U, int16_t,
                                                          int16_t *) &
                                     (int16_t)3)
                     << 6U |
                 (uint32_t)(uint8_t)(Eurydice_slice_index(v, (size_t)2U, int16_t,
                                                          int16_t *) >>
                                         4U &
                                     (int16_t)63);
    uint8_t r4 =
        (uint8_t)(Eurydice_slice_index(v, (size_t)3U, int16_t, int16_t *) >> 2U &
                  (int16_t)255);
    return (CLITERAL(uint8_t_x5){
        .fst = r0, .snd = r1, .thd = r2, .f3 = r3, .f4 = r4 });
}

KRML_MUSTINLINE void
libcrux_ml_kem_vector_portable_serialize_serialize_10(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v,
    uint8_t ret[20U])
{
    uint8_t_x5 r0_4 = libcrux_ml_kem_vector_portable_serialize_serialize_10_int(
        Eurydice_array_to_subslice2(v.elements, (size_t)0U, (size_t)4U, int16_t));
    uint8_t_x5 r5_9 = libcrux_ml_kem_vector_portable_serialize_serialize_10_int(
        Eurydice_array_to_subslice2(v.elements, (size_t)4U, (size_t)8U, int16_t));
    uint8_t_x5 r10_14 = libcrux_ml_kem_vector_portable_serialize_serialize_10_int(
        Eurydice_array_to_subslice2(v.elements, (size_t)8U, (size_t)12U,
                                    int16_t));
    uint8_t_x5 r15_19 = libcrux_ml_kem_vector_portable_serialize_serialize_10_int(
        Eurydice_array_to_subslice2(v.elements, (size_t)12U, (size_t)16U,
                                    int16_t));
    uint8_t result[20U] = { 0U };
    result[0U] = r0_4.fst;
    result[1U] = r0_4.snd;
    result[2U] = r0_4.thd;
    result[3U] = r0_4.f3;
    result[4U] = r0_4.f4;
    result[5U] = r5_9.fst;
    result[6U] = r5_9.snd;
    result[7U] = r5_9.thd;
    result[8U] = r5_9.f3;
    result[9U] = r5_9.f4;
    result[10U] = r10_14.fst;
    result[11U] = r10_14.snd;
    result[12U] = r10_14.thd;
    result[13U] = r10_14.f3;
    result[14U] = r10_14.f4;
    result[15U] = r15_19.fst;
    result[16U] = r15_19.snd;
    result[17U] = r15_19.thd;
    result[18U] = r15_19.f3;
    result[19U] = r15_19.f4;
    memcpy(ret, result, (size_t)20U * sizeof(uint8_t));
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
void
libcrux_ml_kem_vector_portable_serialize_10_0d(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a,
    uint8_t ret[20U])
{
    libcrux_ml_kem_vector_portable_serialize_serialize_10(a, ret);
}

KRML_MUSTINLINE int16_t_x8
libcrux_ml_kem_vector_portable_serialize_deserialize_10_int(
    Eurydice_slice bytes)
{
    int16_t r0 =
        ((int16_t)Eurydice_slice_index(bytes, (size_t)1U, uint8_t, uint8_t *) &
         (int16_t)3)
            << 8U |
        ((int16_t)Eurydice_slice_index(bytes, (size_t)0U, uint8_t, uint8_t *) &
         (int16_t)255);
    int16_t r1 =
        ((int16_t)Eurydice_slice_index(bytes, (size_t)2U, uint8_t, uint8_t *) &
         (int16_t)15)
            << 6U |
        (int16_t)Eurydice_slice_index(bytes, (size_t)1U, uint8_t, uint8_t *) >>
            2U;
    int16_t r2 =
        ((int16_t)Eurydice_slice_index(bytes, (size_t)3U, uint8_t, uint8_t *) &
         (int16_t)63)
            << 4U |
        (int16_t)Eurydice_slice_index(bytes, (size_t)2U, uint8_t, uint8_t *) >>
            4U;
    int16_t r3 =
        (int16_t)Eurydice_slice_index(bytes, (size_t)4U, uint8_t, uint8_t *)
            << 2U |
        (int16_t)Eurydice_slice_index(bytes, (size_t)3U, uint8_t, uint8_t *) >>
            6U;
    int16_t r4 =
        ((int16_t)Eurydice_slice_index(bytes, (size_t)6U, uint8_t, uint8_t *) &
         (int16_t)3)
            << 8U |
        ((int16_t)Eurydice_slice_index(bytes, (size_t)5U, uint8_t, uint8_t *) &
         (int16_t)255);
    int16_t r5 =
        ((int16_t)Eurydice_slice_index(bytes, (size_t)7U, uint8_t, uint8_t *) &
         (int16_t)15)
            << 6U |
        (int16_t)Eurydice_slice_index(bytes, (size_t)6U, uint8_t, uint8_t *) >>
            2U;
    int16_t r6 =
        ((int16_t)Eurydice_slice_index(bytes, (size_t)8U, uint8_t, uint8_t *) &
         (int16_t)63)
            << 4U |
        (int16_t)Eurydice_slice_index(bytes, (size_t)7U, uint8_t, uint8_t *) >>
            4U;
    int16_t r7 =
        (int16_t)Eurydice_slice_index(bytes, (size_t)9U, uint8_t, uint8_t *)
            << 2U |
        (int16_t)Eurydice_slice_index(bytes, (size_t)8U, uint8_t, uint8_t *) >>
            6U;
    return (CLITERAL(int16_t_x8){ .fst = r0,
                                  .snd = r1,
                                  .thd = r2,
                                  .f3 = r3,
                                  .f4 = r4,
                                  .f5 = r5,
                                  .f6 = r6,
                                  .f7 = r7 });
}

KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_serialize_deserialize_10(Eurydice_slice bytes)
{
    int16_t_x8 v0_7 = libcrux_ml_kem_vector_portable_serialize_deserialize_10_int(
        Eurydice_slice_subslice2(bytes, (size_t)0U, (size_t)10U, uint8_t));
    int16_t_x8 v8_15 =
        libcrux_ml_kem_vector_portable_serialize_deserialize_10_int(
            Eurydice_slice_subslice2(bytes, (size_t)10U, (size_t)20U, uint8_t));
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v =
        libcrux_ml_kem_vector_portable_vector_type_zero();
    v.elements[0U] = v0_7.fst;
    v.elements[1U] = v0_7.snd;
    v.elements[2U] = v0_7.thd;
    v.elements[3U] = v0_7.f3;
    v.elements[4U] = v0_7.f4;
    v.elements[5U] = v0_7.f5;
    v.elements[6U] = v0_7.f6;
    v.elements[7U] = v0_7.f7;
    v.elements[8U] = v8_15.fst;
    v.elements[9U] = v8_15.snd;
    v.elements[10U] = v8_15.thd;
    v.elements[11U] = v8_15.f3;
    v.elements[12U] = v8_15.f4;
    v.elements[13U] = v8_15.f5;
    v.elements[14U] = v8_15.f6;
    v.elements[15U] = v8_15.f7;
    return v;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_deserialize_10_0d(Eurydice_slice a)
{
    return libcrux_ml_kem_vector_portable_serialize_deserialize_10(a);
}

KRML_MUSTINLINE uint8_t_x3
libcrux_ml_kem_vector_portable_serialize_serialize_12_int(Eurydice_slice v)
{
    uint8_t r0 =
        (uint8_t)(Eurydice_slice_index(v, (size_t)0U, int16_t, int16_t *) &
                  (int16_t)255);
    uint8_t r1 =
        (uint8_t)(Eurydice_slice_index(v, (size_t)0U, int16_t, int16_t *) >> 8U |
                  (Eurydice_slice_index(v, (size_t)1U, int16_t, int16_t *) &
                   (int16_t)15)
                      << 4U);
    uint8_t r2 =
        (uint8_t)(Eurydice_slice_index(v, (size_t)1U, int16_t, int16_t *) >> 4U &
                  (int16_t)255);
    return (CLITERAL(uint8_t_x3){ .fst = r0, .snd = r1, .thd = r2 });
}

KRML_MUSTINLINE void
libcrux_ml_kem_vector_portable_serialize_serialize_12(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v,
    uint8_t ret[24U])
{
    uint8_t_x3 r0_2 = libcrux_ml_kem_vector_portable_serialize_serialize_12_int(
        Eurydice_array_to_subslice2(v.elements, (size_t)0U, (size_t)2U, int16_t));
    uint8_t_x3 r3_5 = libcrux_ml_kem_vector_portable_serialize_serialize_12_int(
        Eurydice_array_to_subslice2(v.elements, (size_t)2U, (size_t)4U, int16_t));
    uint8_t_x3 r6_8 = libcrux_ml_kem_vector_portable_serialize_serialize_12_int(
        Eurydice_array_to_subslice2(v.elements, (size_t)4U, (size_t)6U, int16_t));
    uint8_t_x3 r9_11 = libcrux_ml_kem_vector_portable_serialize_serialize_12_int(
        Eurydice_array_to_subslice2(v.elements, (size_t)6U, (size_t)8U, int16_t));
    uint8_t_x3 r12_14 = libcrux_ml_kem_vector_portable_serialize_serialize_12_int(
        Eurydice_array_to_subslice2(v.elements, (size_t)8U, (size_t)10U,
                                    int16_t));
    uint8_t_x3 r15_17 = libcrux_ml_kem_vector_portable_serialize_serialize_12_int(
        Eurydice_array_to_subslice2(v.elements, (size_t)10U, (size_t)12U,
                                    int16_t));
    uint8_t_x3 r18_20 = libcrux_ml_kem_vector_portable_serialize_serialize_12_int(
        Eurydice_array_to_subslice2(v.elements, (size_t)12U, (size_t)14U,
                                    int16_t));
    uint8_t_x3 r21_23 = libcrux_ml_kem_vector_portable_serialize_serialize_12_int(
        Eurydice_array_to_subslice2(v.elements, (size_t)14U, (size_t)16U,
                                    int16_t));
    uint8_t result[24U] = { 0U };
    result[0U] = r0_2.fst;
    result[1U] = r0_2.snd;
    result[2U] = r0_2.thd;
    result[3U] = r3_5.fst;
    result[4U] = r3_5.snd;
    result[5U] = r3_5.thd;
    result[6U] = r6_8.fst;
    result[7U] = r6_8.snd;
    result[8U] = r6_8.thd;
    result[9U] = r9_11.fst;
    result[10U] = r9_11.snd;
    result[11U] = r9_11.thd;
    result[12U] = r12_14.fst;
    result[13U] = r12_14.snd;
    result[14U] = r12_14.thd;
    result[15U] = r15_17.fst;
    result[16U] = r15_17.snd;
    result[17U] = r15_17.thd;
    result[18U] = r18_20.fst;
    result[19U] = r18_20.snd;
    result[20U] = r18_20.thd;
    result[21U] = r21_23.fst;
    result[22U] = r21_23.snd;
    result[23U] = r21_23.thd;
    memcpy(ret, result, (size_t)24U * sizeof(uint8_t));
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
void
libcrux_ml_kem_vector_portable_serialize_12_0d(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a,
    uint8_t ret[24U])
{
    libcrux_ml_kem_vector_portable_serialize_serialize_12(a, ret);
}

KRML_MUSTINLINE int16_t_x2
libcrux_ml_kem_vector_portable_serialize_deserialize_12_int(
    Eurydice_slice bytes)
{
    int16_t byte0 =
        (int16_t)Eurydice_slice_index(bytes, (size_t)0U, uint8_t, uint8_t *);
    int16_t byte1 =
        (int16_t)Eurydice_slice_index(bytes, (size_t)1U, uint8_t, uint8_t *);
    int16_t byte2 =
        (int16_t)Eurydice_slice_index(bytes, (size_t)2U, uint8_t, uint8_t *);
    int16_t r0 = (byte1 & (int16_t)15) << 8U | (byte0 & (int16_t)255);
    int16_t r1 = byte2 << 4U | (byte1 >> 4U & (int16_t)15);
    return (CLITERAL(int16_t_x2){ .fst = r0, .snd = r1 });
}

KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_serialize_deserialize_12(Eurydice_slice bytes)
{
    int16_t_x2 v0_1 = libcrux_ml_kem_vector_portable_serialize_deserialize_12_int(
        Eurydice_slice_subslice2(bytes, (size_t)0U, (size_t)3U, uint8_t));
    int16_t_x2 v2_3 = libcrux_ml_kem_vector_portable_serialize_deserialize_12_int(
        Eurydice_slice_subslice2(bytes, (size_t)3U, (size_t)6U, uint8_t));
    int16_t_x2 v4_5 = libcrux_ml_kem_vector_portable_serialize_deserialize_12_int(
        Eurydice_slice_subslice2(bytes, (size_t)6U, (size_t)9U, uint8_t));
    int16_t_x2 v6_7 = libcrux_ml_kem_vector_portable_serialize_deserialize_12_int(
        Eurydice_slice_subslice2(bytes, (size_t)9U, (size_t)12U, uint8_t));
    int16_t_x2 v8_9 = libcrux_ml_kem_vector_portable_serialize_deserialize_12_int(
        Eurydice_slice_subslice2(bytes, (size_t)12U, (size_t)15U, uint8_t));
    int16_t_x2 v10_11 =
        libcrux_ml_kem_vector_portable_serialize_deserialize_12_int(
            Eurydice_slice_subslice2(bytes, (size_t)15U, (size_t)18U, uint8_t));
    int16_t_x2 v12_13 =
        libcrux_ml_kem_vector_portable_serialize_deserialize_12_int(
            Eurydice_slice_subslice2(bytes, (size_t)18U, (size_t)21U, uint8_t));
    int16_t_x2 v14_15 =
        libcrux_ml_kem_vector_portable_serialize_deserialize_12_int(
            Eurydice_slice_subslice2(bytes, (size_t)21U, (size_t)24U, uint8_t));
    libcrux_ml_kem_vector_portable_vector_type_PortableVector re =
        libcrux_ml_kem_vector_portable_vector_type_zero();
    re.elements[0U] = v0_1.fst;
    re.elements[1U] = v0_1.snd;
    re.elements[2U] = v2_3.fst;
    re.elements[3U] = v2_3.snd;
    re.elements[4U] = v4_5.fst;
    re.elements[5U] = v4_5.snd;
    re.elements[6U] = v6_7.fst;
    re.elements[7U] = v6_7.snd;
    re.elements[8U] = v8_9.fst;
    re.elements[9U] = v8_9.snd;
    re.elements[10U] = v10_11.fst;
    re.elements[11U] = v10_11.snd;
    re.elements[12U] = v12_13.fst;
    re.elements[13U] = v12_13.snd;
    re.elements[14U] = v14_15.fst;
    re.elements[15U] = v14_15.snd;
    return re;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_deserialize_12_0d(Eurydice_slice a)
{
    return libcrux_ml_kem_vector_portable_serialize_deserialize_12(a);
}

KRML_MUSTINLINE size_t
libcrux_ml_kem_vector_portable_sampling_rej_sample(
    Eurydice_slice a, Eurydice_slice result)
{
    size_t sampled = (size_t)0U;
    for (size_t i = (size_t)0U; i < Eurydice_slice_len(a, uint8_t) / (size_t)3U;
         i++) {
        size_t i0 = i;
        int16_t b1 = (int16_t)Eurydice_slice_index(a, i0 * (size_t)3U + (size_t)0U,
                                                   uint8_t, uint8_t *);
        int16_t b2 = (int16_t)Eurydice_slice_index(a, i0 * (size_t)3U + (size_t)1U,
                                                   uint8_t, uint8_t *);
        int16_t b3 = (int16_t)Eurydice_slice_index(a, i0 * (size_t)3U + (size_t)2U,
                                                   uint8_t, uint8_t *);
        int16_t d1 = (b2 & (int16_t)15) << 8U | b1;
        int16_t d2 = b3 << 4U | b2 >> 4U;
        bool uu____0;
        int16_t uu____1;
        bool uu____2;
        size_t uu____3;
        int16_t uu____4;
        size_t uu____5;
        int16_t uu____6;
        if (d1 < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_MODULUS) {
            if (sampled < (size_t)16U) {
                Eurydice_slice_index(result, sampled, int16_t, int16_t *) = d1;
                sampled++;
                uu____1 = d2;
                uu____6 = LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_MODULUS;
                uu____0 = uu____1 < uu____6;
                if (uu____0) {
                    uu____3 = sampled;
                    uu____2 = uu____3 < (size_t)16U;
                    if (uu____2) {
                        uu____4 = d2;
                        uu____5 = sampled;
                        Eurydice_slice_index(result, uu____5, int16_t, int16_t *) = uu____4;
                        sampled++;
                        continue;
                    }
                }
                continue;
            }
        }
        uu____1 = d2;
        uu____6 = LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_MODULUS;
        uu____0 = uu____1 < uu____6;
        if (uu____0) {
            uu____3 = sampled;
            uu____2 = uu____3 < (size_t)16U;
            if (uu____2) {
                uu____4 = d2;
                uu____5 = sampled;
                Eurydice_slice_index(result, uu____5, int16_t, int16_t *) = uu____4;
                sampled++;
                continue;
            }
        }
    }
    return sampled;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
size_t
libcrux_ml_kem_vector_portable_rej_sample_0d(Eurydice_slice a,
                                             Eurydice_slice out)
{
    return libcrux_ml_kem_vector_portable_sampling_rej_sample(a, out);
}

/**
This function found in impl {(core::clone::Clone for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_vector_type_clone_3b(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector *self)
{
    return self[0U];
}

/**
This function found in impl
{libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0]}
*/
/**
A monomorphic instance of libcrux_ml_kem.polynomial.ZERO_89
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static libcrux_ml_kem_polynomial_PolynomialRingElement_f0
ZERO_89_c3(void)
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 lit;
    lit.coefficients[0U] = libcrux_ml_kem_vector_portable_ZERO_0d();
    lit.coefficients[1U] = libcrux_ml_kem_vector_portable_ZERO_0d();
    lit.coefficients[2U] = libcrux_ml_kem_vector_portable_ZERO_0d();
    lit.coefficients[3U] = libcrux_ml_kem_vector_portable_ZERO_0d();
    lit.coefficients[4U] = libcrux_ml_kem_vector_portable_ZERO_0d();
    lit.coefficients[5U] = libcrux_ml_kem_vector_portable_ZERO_0d();
    lit.coefficients[6U] = libcrux_ml_kem_vector_portable_ZERO_0d();
    lit.coefficients[7U] = libcrux_ml_kem_vector_portable_ZERO_0d();
    lit.coefficients[8U] = libcrux_ml_kem_vector_portable_ZERO_0d();
    lit.coefficients[9U] = libcrux_ml_kem_vector_portable_ZERO_0d();
    lit.coefficients[10U] = libcrux_ml_kem_vector_portable_ZERO_0d();
    lit.coefficients[11U] = libcrux_ml_kem_vector_portable_ZERO_0d();
    lit.coefficients[12U] = libcrux_ml_kem_vector_portable_ZERO_0d();
    lit.coefficients[13U] = libcrux_ml_kem_vector_portable_ZERO_0d();
    lit.coefficients[14U] = libcrux_ml_kem_vector_portable_ZERO_0d();
    lit.coefficients[15U] = libcrux_ml_kem_vector_portable_ZERO_0d();
    return lit;
}

/**
 Only use with public values.

 This MUST NOT be used with secret inputs, like its caller
 `deserialize_ring_elements_reduced`.
*/
/**
A monomorphic instance of
libcrux_ml_kem.serialize.deserialize_to_reduced_ring_element with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics

*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_f0
deserialize_to_reduced_ring_element_45(Eurydice_slice serialized)
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 re = ZERO_89_c3();
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(serialized, uint8_t) / (size_t)24U; i++) {
        size_t i0 = i;
        Eurydice_slice bytes = Eurydice_slice_subslice2(
            serialized, i0 * (size_t)24U, i0 * (size_t)24U + (size_t)24U, uint8_t);
        libcrux_ml_kem_vector_portable_vector_type_PortableVector coefficient =
            libcrux_ml_kem_vector_portable_deserialize_12_0d(bytes);
        libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
            libcrux_ml_kem_vector_portable_cond_subtract_3329_0d(coefficient);
        re.coefficients[i0] = uu____0;
    }
    return re;
}

/**
 This function deserializes ring elements and reduces the result by the field
 modulus.

 This function MUST NOT be used on secret inputs.
*/
/**
A monomorphic instance of
libcrux_ml_kem.serialize.deserialize_ring_elements_reduced with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- PUBLIC_KEY_SIZE= 1568
- K= 4
*/
static KRML_MUSTINLINE void
deserialize_ring_elements_reduced_4f4(
    Eurydice_slice public_key,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret[4U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 deserialized_pk[4U];
    KRML_MAYBE_FOR4(i, (size_t)0U, (size_t)4U, (size_t)1U,
                    deserialized_pk[i] = ZERO_89_c3(););
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(public_key, uint8_t) /
                 LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT;
         i++) {
        size_t i0 = i;
        Eurydice_slice ring_element = Eurydice_slice_subslice2(
            public_key, i0 * LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
            i0 * LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT +
                LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
            uint8_t);
        libcrux_ml_kem_polynomial_PolynomialRingElement_f0 uu____0 =
            deserialize_to_reduced_ring_element_45(ring_element);
        deserialized_pk[i0] = uu____0;
    }
    memcpy(
        ret, deserialized_pk,
        (size_t)4U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
}

/**
A monomorphic instance of libcrux_ml_kem.vector.portable.arithmetic.shift_right
with const generics
- SHIFT_BY= 15
*/
static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
shift_right_b0(libcrux_ml_kem_vector_portable_vector_type_PortableVector v)
{
    for (size_t i = (size_t)0U;
         i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
        size_t i0 = i;
        v.elements[i0] = v.elements[i0] >> (uint32_t)(int32_t)15;
    }
    return v;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
/**
A monomorphic instance of libcrux_ml_kem.vector.portable.shift_right_0d
with const generics
- SHIFT_BY= 15
*/
static libcrux_ml_kem_vector_portable_vector_type_PortableVector
shift_right_0d_01(libcrux_ml_kem_vector_portable_vector_type_PortableVector v)
{
    return shift_right_b0(v);
}

/**
A monomorphic instance of
libcrux_ml_kem.vector.traits.to_unsigned_representative with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics

*/
static libcrux_ml_kem_vector_portable_vector_type_PortableVector
to_unsigned_representative_84(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a)
{
    libcrux_ml_kem_vector_portable_vector_type_PortableVector t =
        shift_right_0d_01(a);
    libcrux_ml_kem_vector_portable_vector_type_PortableVector fm =
        libcrux_ml_kem_vector_portable_bitwise_and_with_constant_0d(
            t, LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_MODULUS);
    return libcrux_ml_kem_vector_portable_add_0d(a, &fm);
}

/**
A monomorphic instance of
libcrux_ml_kem.serialize.serialize_uncompressed_ring_element with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics

*/
static KRML_MUSTINLINE void
serialize_uncompressed_ring_element_3c(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *re, uint8_t ret[384U])
{
    uint8_t serialized[384U] = { 0U };
    for (size_t i = (size_t)0U;
         i < LIBCRUX_ML_KEM_POLYNOMIAL_VECTORS_IN_RING_ELEMENT; i++) {
        size_t i0 = i;
        libcrux_ml_kem_vector_portable_vector_type_PortableVector coefficient =
            to_unsigned_representative_84(re->coefficients[i0]);
        uint8_t bytes[24U];
        libcrux_ml_kem_vector_portable_serialize_12_0d(coefficient, bytes);
        Eurydice_slice uu____0 = Eurydice_array_to_subslice2(
            serialized, (size_t)24U * i0, (size_t)24U * i0 + (size_t)24U, uint8_t);
        Eurydice_slice_copy(
            uu____0, Eurydice_array_to_slice((size_t)24U, bytes, uint8_t), uint8_t);
    }
    memcpy(ret, serialized, (size_t)384U * sizeof(uint8_t));
}

/**
 Call [`serialize_uncompressed_ring_element`] for each ring element.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.serialize_secret_key
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 4
- OUT_LEN= 1536
*/
static KRML_MUSTINLINE void
serialize_secret_key_a31(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *key,
    uint8_t ret[1536U])
{
    uint8_t out[1536U] = { 0U };
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(
                 Eurydice_array_to_slice(
                     (size_t)4U, key,
                     libcrux_ml_kem_polynomial_PolynomialRingElement_f0),
                 libcrux_ml_kem_polynomial_PolynomialRingElement_f0);
         i++) {
        size_t i0 = i;
        libcrux_ml_kem_polynomial_PolynomialRingElement_f0 re = key[i0];
        Eurydice_slice uu____0 = Eurydice_array_to_subslice2(
            out, i0 * LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
            (i0 + (size_t)1U) * LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
            uint8_t);
        uint8_t ret0[384U];
        serialize_uncompressed_ring_element_3c(&re, ret0);
        Eurydice_slice_copy(
            uu____0, Eurydice_array_to_slice((size_t)384U, ret0, uint8_t), uint8_t);
    }
    memcpy(ret, out, (size_t)1536U * sizeof(uint8_t));
}

/**
 Concatenate `t` and `ρ` into the public key.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.serialize_public_key
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 4
- RANKED_BYTES_PER_RING_ELEMENT= 1536
- PUBLIC_KEY_SIZE= 1568
*/
static KRML_MUSTINLINE void
serialize_public_key_671(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *t_as_ntt,
    Eurydice_slice seed_for_a, uint8_t ret[1568U])
{
    uint8_t public_key_serialized[1568U] = { 0U };
    Eurydice_slice uu____0 = Eurydice_array_to_subslice2(
        public_key_serialized, (size_t)0U, (size_t)1536U, uint8_t);
    uint8_t ret0[1536U];
    serialize_secret_key_a31(t_as_ntt, ret0);
    Eurydice_slice_copy(
        uu____0, Eurydice_array_to_slice((size_t)1536U, ret0, uint8_t), uint8_t);
    Eurydice_slice_copy(
        Eurydice_array_to_subslice_from((size_t)1568U, public_key_serialized,
                                        (size_t)1536U, uint8_t, size_t),
        seed_for_a, uint8_t);
    memcpy(ret, public_key_serialized, (size_t)1568U * sizeof(uint8_t));
}

/**
 Validate an ML-KEM public key.

 This implements the Modulus check in 7.2 2.
 Note that the size check in 7.2 1 is covered by the `PUBLIC_KEY_SIZE` in the
 `public_key` type.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cca.validate_public_key
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 4
- RANKED_BYTES_PER_RING_ELEMENT= 1536
- PUBLIC_KEY_SIZE= 1568
*/
bool
libcrux_ml_kem_ind_cca_validate_public_key_b71(uint8_t *public_key)
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 deserialized_pk[4U];
    deserialize_ring_elements_reduced_4f4(
        Eurydice_array_to_subslice_to((size_t)1568U, public_key, (size_t)1536U,
                                      uint8_t, size_t),
        deserialized_pk);
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *uu____0 = deserialized_pk;
    uint8_t public_key_serialized[1568U];
    serialize_public_key_671(
        uu____0,
        Eurydice_array_to_subslice_from((size_t)1568U, public_key, (size_t)1536U,
                                        uint8_t, size_t),
        public_key_serialized);
    return core_array_equality___core__cmp__PartialEq__Array_U__N___for__Array_T__N____eq(
        (size_t)1568U, public_key, public_key_serialized, uint8_t, uint8_t, bool);
}

/**
This function found in impl {(libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>)}
*/
/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.H_f1
with const generics
- K= 4
*/
static KRML_MUSTINLINE void
H_f1_191(Eurydice_slice input, uint8_t ret[32U])
{
    libcrux_ml_kem_hash_functions_portable_H(input, ret);
}

/**
 Validate an ML-KEM private key.

 This implements the Hash check in 7.3 3.
 Note that the size checks in 7.2 1 and 2 are covered by the `SECRET_KEY_SIZE`
 and `CIPHERTEXT_SIZE` in the `private_key` and `ciphertext` types.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cca.validate_private_key
with types libcrux_ml_kem_hash_functions_portable_PortableHash[[$4size_t]]
with const generics
- K= 4
- SECRET_KEY_SIZE= 3168
- CIPHERTEXT_SIZE= 1568
*/
bool
libcrux_ml_kem_ind_cca_validate_private_key_dc(
    libcrux_ml_kem_types_MlKemPrivateKey_95 *private_key,
    libcrux_ml_kem_mlkem1024_MlKem1024Ciphertext *_ciphertext)
{
    uint8_t t[32U];
    H_f1_191(Eurydice_array_to_subslice2(
                 private_key->value, (size_t)384U * (size_t)4U,
                 (size_t)768U * (size_t)4U + (size_t)32U, uint8_t),
             t);
    Eurydice_slice expected = Eurydice_array_to_subslice2(
        private_key->value, (size_t)768U * (size_t)4U + (size_t)32U,
        (size_t)768U * (size_t)4U + (size_t)64U, uint8_t);
    return core_array_equality___core__cmp__PartialEq__0___Slice_U____for__Array_T__N___3__eq(
        (size_t)32U, t, &expected, uint8_t, uint8_t, bool);
}

/**
This function found in impl {(libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>)}
*/
/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.G_f1
with const generics
- K= 4
*/
static KRML_MUSTINLINE void
G_f1_381(Eurydice_slice input, uint8_t ret[64U])
{
    libcrux_ml_kem_hash_functions_portable_G(input, ret);
}

/**
This function found in impl {(libcrux_ml_kem::variant::Variant for
libcrux_ml_kem::variant::MlKem)}
*/
/**
A monomorphic instance of libcrux_ml_kem.variant.cpa_keygen_seed_d8
with types libcrux_ml_kem_hash_functions_portable_PortableHash[[$4size_t]]
with const generics
- K= 4
*/
static KRML_MUSTINLINE void
cpa_keygen_seed_d8_72(
    Eurydice_slice key_generation_seed, uint8_t ret[64U])
{
    uint8_t seed[33U] = { 0U };
    Eurydice_slice_copy(
        Eurydice_array_to_subslice2(
            seed, (size_t)0U,
            LIBCRUX_ML_KEM_CONSTANTS_CPA_PKE_KEY_GENERATION_SEED_SIZE, uint8_t),
        key_generation_seed, uint8_t);
    seed[LIBCRUX_ML_KEM_CONSTANTS_CPA_PKE_KEY_GENERATION_SEED_SIZE] =
        (uint8_t)(size_t)4U;
    uint8_t ret0[64U];
    G_f1_381(Eurydice_array_to_slice((size_t)33U, seed, uint8_t), ret0);
    memcpy(ret, ret0, (size_t)64U * sizeof(uint8_t));
}

/**
A monomorphic instance of libcrux_ml_kem.matrix.sample_matrix_A.closure
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$4size_t]] with const
generics
- K= 4
*/
static void
closure_fc1(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret[4U])
{
    KRML_MAYBE_FOR4(i, (size_t)0U, (size_t)4U, (size_t)1U,
                    ret[i] = ZERO_89_c3(););
}

/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.PortableHash
with const generics
- $4size_t
*/
typedef struct PortableHash_d1_s {
    libcrux_sha3_generic_keccak_KeccakState_48 shake128_state[4U];
} PortableHash_d1;

/**
A monomorphic instance of
libcrux_ml_kem.hash_functions.portable.shake128_init_absorb with const generics
- K= 4
*/
static KRML_MUSTINLINE PortableHash_d1
shake128_init_absorb_791(uint8_t input[4U][34U])
{
    libcrux_sha3_generic_keccak_KeccakState_48 shake128_state[4U];
    KRML_MAYBE_FOR4(
        i, (size_t)0U, (size_t)4U, (size_t)1U,
        shake128_state[i] = libcrux_sha3_portable_incremental_shake128_init(););
    KRML_MAYBE_FOR4(
        i, (size_t)0U, (size_t)4U, (size_t)1U, size_t i0 = i;
        libcrux_sha3_portable_incremental_shake128_absorb_final(
            &shake128_state[i0],
            Eurydice_array_to_slice((size_t)34U, input[i0], uint8_t)););
    /* Passing arrays by value in Rust generates a copy in C */
    libcrux_sha3_generic_keccak_KeccakState_48 copy_of_shake128_state[4U];
    memcpy(copy_of_shake128_state, shake128_state,
           (size_t)4U * sizeof(libcrux_sha3_generic_keccak_KeccakState_48));
    PortableHash_d1 lit;
    memcpy(lit.shake128_state, copy_of_shake128_state,
           (size_t)4U * sizeof(libcrux_sha3_generic_keccak_KeccakState_48));
    return lit;
}

/**
This function found in impl {(libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>)}
*/
/**
A monomorphic instance of
libcrux_ml_kem.hash_functions.portable.shake128_init_absorb_f1 with const
generics
- K= 4
*/
static KRML_MUSTINLINE PortableHash_d1
shake128_init_absorb_f1_771(uint8_t input[4U][34U])
{
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_input[4U][34U];
    memcpy(copy_of_input, input, (size_t)4U * sizeof(uint8_t[34U]));
    return shake128_init_absorb_791(copy_of_input);
}

/**
A monomorphic instance of
libcrux_ml_kem.hash_functions.portable.shake128_squeeze_three_blocks with const
generics
- K= 4
*/
static KRML_MUSTINLINE void
shake128_squeeze_three_blocks_eb1(
    PortableHash_d1 *st, uint8_t ret[4U][504U])
{
    uint8_t out[4U][504U] = { { 0U } };
    KRML_MAYBE_FOR4(
        i, (size_t)0U, (size_t)4U, (size_t)1U, size_t i0 = i;
        libcrux_sha3_portable_incremental_shake128_squeeze_first_three_blocks(
            &st->shake128_state[i0],
            Eurydice_array_to_slice((size_t)504U, out[i0], uint8_t)););
    memcpy(ret, out, (size_t)4U * sizeof(uint8_t[504U]));
}

/**
This function found in impl {(libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>)}
*/
/**
A monomorphic instance of
libcrux_ml_kem.hash_functions.portable.shake128_squeeze_three_blocks_f1 with
const generics
- K= 4
*/
static KRML_MUSTINLINE void
shake128_squeeze_three_blocks_f1_841(
    PortableHash_d1 *self, uint8_t ret[4U][504U])
{
    shake128_squeeze_three_blocks_eb1(self, ret);
}

/**
 If `bytes` contains a set of uniformly random bytes, this function
 uniformly samples a ring element `â` that is treated as being the NTT
 representation of the corresponding polynomial `a`.

 Since rejection sampling is used, it is possible the supplied bytes are
 not enough to sample the element, in which case an `Err` is returned and the
 caller must try again with a fresh set of bytes.

 This function <strong>partially</strong> implements <strong>Algorithm
 6</strong> of the NIST FIPS 203 standard, We say "partially" because this
 implementation only accepts a finite set of bytes as input and returns an error
 if the set is not enough; Algorithm 6 of the FIPS 203 standard on the other
 hand samples from an infinite stream of bytes until the ring element is filled.
 Algorithm 6 is reproduced below:

 ```plaintext
 Input: byte stream B ∈ 𝔹*.
 Output: array â ∈ ℤ₂₅₆.

 i ← 0
 j ← 0
 while j < 256 do
     d₁ ← B[i] + 256·(B[i+1] mod 16)
     d₂ ← ⌊B[i+1]/16⌋ + 16·B[i+2]
     if d₁ < q then
         â[j] ← d₁
         j ← j + 1
     end if
     if d₂ < q and j < 256 then
         â[j] ← d₂
         j ← j + 1
     end if
     i ← i + 3
 end while
 return â
 ```

 The NIST FIPS 203 standard can be found at
 <https://csrc.nist.gov/pubs/fips/203/ipd>.
*/
/**
A monomorphic instance of
libcrux_ml_kem.sampling.sample_from_uniform_distribution_next with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- K= 4
- N= 504
*/
static KRML_MUSTINLINE bool
sample_from_uniform_distribution_next_173(
    uint8_t randomness[4U][504U], size_t *sampled_coefficients,
    int16_t (*out)[272U])
{
    KRML_MAYBE_FOR4(
        i0, (size_t)0U, (size_t)4U, (size_t)1U, size_t i1 = i0;
        for (size_t i = (size_t)0U; i < (size_t)504U / (size_t)24U; i++) {
            size_t r = i;
            if (sampled_coefficients[i1] <
                LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT) {
                Eurydice_slice uu____0 = Eurydice_array_to_subslice2(
                    randomness[i1], r * (size_t)24U, r * (size_t)24U + (size_t)24U,
                    uint8_t);
                size_t sampled = libcrux_ml_kem_vector_portable_rej_sample_0d(
                    uu____0, Eurydice_array_to_subslice2(
                                 out[i1], sampled_coefficients[i1],
                                 sampled_coefficients[i1] + (size_t)16U, int16_t));
                size_t uu____1 = i1;
                sampled_coefficients[uu____1] =
                    sampled_coefficients[uu____1] + sampled;
            }
        });
    bool done = true;
    KRML_MAYBE_FOR4(
        i, (size_t)0U, (size_t)4U, (size_t)1U, size_t i0 = i;
        if (sampled_coefficients[i0] >=
            LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT) {
            sampled_coefficients[i0] =
                LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT;
        } else { done = false; });
    return done;
}

/**
A monomorphic instance of
libcrux_ml_kem.hash_functions.portable.shake128_squeeze_block with const
generics
- K= 4
*/
static KRML_MUSTINLINE void
shake128_squeeze_block_3b1(PortableHash_d1 *st,
                           uint8_t ret[4U][168U])
{
    uint8_t out[4U][168U] = { { 0U } };
    KRML_MAYBE_FOR4(
        i, (size_t)0U, (size_t)4U, (size_t)1U, size_t i0 = i;
        libcrux_sha3_portable_incremental_shake128_squeeze_next_block(
            &st->shake128_state[i0],
            Eurydice_array_to_slice((size_t)168U, out[i0], uint8_t)););
    memcpy(ret, out, (size_t)4U * sizeof(uint8_t[168U]));
}

/**
This function found in impl {(libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>)}
*/
/**
A monomorphic instance of
libcrux_ml_kem.hash_functions.portable.shake128_squeeze_block_f1 with const
generics
- K= 4
*/
static KRML_MUSTINLINE void
shake128_squeeze_block_f1_8e1(
    PortableHash_d1 *self, uint8_t ret[4U][168U])
{
    shake128_squeeze_block_3b1(self, ret);
}

/**
 If `bytes` contains a set of uniformly random bytes, this function
 uniformly samples a ring element `â` that is treated as being the NTT
 representation of the corresponding polynomial `a`.

 Since rejection sampling is used, it is possible the supplied bytes are
 not enough to sample the element, in which case an `Err` is returned and the
 caller must try again with a fresh set of bytes.

 This function <strong>partially</strong> implements <strong>Algorithm
 6</strong> of the NIST FIPS 203 standard, We say "partially" because this
 implementation only accepts a finite set of bytes as input and returns an error
 if the set is not enough; Algorithm 6 of the FIPS 203 standard on the other
 hand samples from an infinite stream of bytes until the ring element is filled.
 Algorithm 6 is reproduced below:

 ```plaintext
 Input: byte stream B ∈ 𝔹*.
 Output: array â ∈ ℤ₂₅₆.

 i ← 0
 j ← 0
 while j < 256 do
     d₁ ← B[i] + 256·(B[i+1] mod 16)
     d₂ ← ⌊B[i+1]/16⌋ + 16·B[i+2]
     if d₁ < q then
         â[j] ← d₁
         j ← j + 1
     end if
     if d₂ < q and j < 256 then
         â[j] ← d₂
         j ← j + 1
     end if
     i ← i + 3
 end while
 return â
 ```

 The NIST FIPS 203 standard can be found at
 <https://csrc.nist.gov/pubs/fips/203/ipd>.
*/
/**
A monomorphic instance of
libcrux_ml_kem.sampling.sample_from_uniform_distribution_next with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- K= 4
- N= 168
*/
static KRML_MUSTINLINE bool
sample_from_uniform_distribution_next_174(
    uint8_t randomness[4U][168U], size_t *sampled_coefficients,
    int16_t (*out)[272U])
{
    KRML_MAYBE_FOR4(
        i0, (size_t)0U, (size_t)4U, (size_t)1U, size_t i1 = i0;
        for (size_t i = (size_t)0U; i < (size_t)168U / (size_t)24U; i++) {
            size_t r = i;
            if (sampled_coefficients[i1] <
                LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT) {
                Eurydice_slice uu____0 = Eurydice_array_to_subslice2(
                    randomness[i1], r * (size_t)24U, r * (size_t)24U + (size_t)24U,
                    uint8_t);
                size_t sampled = libcrux_ml_kem_vector_portable_rej_sample_0d(
                    uu____0, Eurydice_array_to_subslice2(
                                 out[i1], sampled_coefficients[i1],
                                 sampled_coefficients[i1] + (size_t)16U, int16_t));
                size_t uu____1 = i1;
                sampled_coefficients[uu____1] =
                    sampled_coefficients[uu____1] + sampled;
            }
        });
    bool done = true;
    KRML_MAYBE_FOR4(
        i, (size_t)0U, (size_t)4U, (size_t)1U, size_t i0 = i;
        if (sampled_coefficients[i0] >=
            LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT) {
            sampled_coefficients[i0] =
                LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT;
        } else { done = false; });
    return done;
}

/**
This function found in impl
{libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0]}
*/
/**
A monomorphic instance of libcrux_ml_kem.polynomial.from_i16_array_89
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_f0
from_i16_array_89_33(Eurydice_slice a)
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 result = ZERO_89_c3();
    for (size_t i = (size_t)0U;
         i < LIBCRUX_ML_KEM_POLYNOMIAL_VECTORS_IN_RING_ELEMENT; i++) {
        size_t i0 = i;
        libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
            libcrux_ml_kem_vector_portable_from_i16_array_0d(
                Eurydice_slice_subslice2(a, i0 * (size_t)16U,
                                         (i0 + (size_t)1U) * (size_t)16U, int16_t));
        result.coefficients[i0] = uu____0;
    }
    return result;
}

/**
A monomorphic instance of libcrux_ml_kem.sampling.sample_from_xof.closure
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$4size_t]] with const
generics
- K= 4
*/
static libcrux_ml_kem_polynomial_PolynomialRingElement_f0
closure_e41(
    int16_t s[272U])
{
    return from_i16_array_89_33(
        Eurydice_array_to_subslice2(s, (size_t)0U, (size_t)256U, int16_t));
}

/**
A monomorphic instance of libcrux_ml_kem.sampling.sample_from_xof
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$4size_t]] with const
generics
- K= 4
*/
static KRML_MUSTINLINE void
sample_from_xof_461(
    uint8_t seeds[4U][34U],
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret[4U])
{
    size_t sampled_coefficients[4U] = { 0U };
    int16_t out[4U][272U] = { { 0U } };
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_seeds[4U][34U];
    memcpy(copy_of_seeds, seeds, (size_t)4U * sizeof(uint8_t[34U]));
    PortableHash_d1 xof_state = shake128_init_absorb_f1_771(copy_of_seeds);
    uint8_t randomness0[4U][504U];
    shake128_squeeze_three_blocks_f1_841(&xof_state, randomness0);
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_randomness0[4U][504U];
    memcpy(copy_of_randomness0, randomness0, (size_t)4U * sizeof(uint8_t[504U]));
    bool done = sample_from_uniform_distribution_next_173(
        copy_of_randomness0, sampled_coefficients, out);
    while (true) {
        if (done) {
            break;
        } else {
            uint8_t randomness[4U][168U];
            shake128_squeeze_block_f1_8e1(&xof_state, randomness);
            /* Passing arrays by value in Rust generates a copy in C */
            uint8_t copy_of_randomness[4U][168U];
            memcpy(copy_of_randomness, randomness,
                   (size_t)4U * sizeof(uint8_t[168U]));
            done = sample_from_uniform_distribution_next_174(
                copy_of_randomness, sampled_coefficients, out);
        }
    }
    /* Passing arrays by value in Rust generates a copy in C */
    int16_t copy_of_out[4U][272U];
    memcpy(copy_of_out, out, (size_t)4U * sizeof(int16_t[272U]));
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret0[4U];
    KRML_MAYBE_FOR4(i, (size_t)0U, (size_t)4U, (size_t)1U,
                    ret0[i] = closure_e41(copy_of_out[i]););
    memcpy(
        ret, ret0,
        (size_t)4U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
}

/**
A monomorphic instance of libcrux_ml_kem.matrix.sample_matrix_A
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$4size_t]] with const
generics
- K= 4
*/
static KRML_MUSTINLINE void
sample_matrix_A_e71(
    uint8_t seed[34U], bool transpose,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret[4U][4U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 A_transpose[4U][4U];
    KRML_MAYBE_FOR4(i, (size_t)0U, (size_t)4U, (size_t)1U,
                    closure_fc1(A_transpose[i]););
    KRML_MAYBE_FOR4(
        i0, (size_t)0U, (size_t)4U, (size_t)1U, size_t i1 = i0;
        /* Passing arrays by value in Rust generates a copy in C */
        uint8_t copy_of_seed[34U];
        memcpy(copy_of_seed, seed, (size_t)34U * sizeof(uint8_t));
        uint8_t seeds[4U][34U]; KRML_MAYBE_FOR4(
            i, (size_t)0U, (size_t)4U, (size_t)1U,
            memcpy(seeds[i], copy_of_seed, (size_t)34U * sizeof(uint8_t)););
        KRML_MAYBE_FOR4(i, (size_t)0U, (size_t)4U, (size_t)1U, size_t j = i;
                        seeds[j][32U] = (uint8_t)i1; seeds[j][33U] = (uint8_t)j;);
        /* Passing arrays by value in Rust generates a copy in C */
        uint8_t copy_of_seeds[4U][34U];
        memcpy(copy_of_seeds, seeds, (size_t)4U * sizeof(uint8_t[34U]));
        libcrux_ml_kem_polynomial_PolynomialRingElement_f0 sampled[4U];
        sample_from_xof_461(copy_of_seeds, sampled);
        for (size_t i = (size_t)0U;
             i < Eurydice_slice_len(
                     Eurydice_array_to_slice(
                         (size_t)4U, sampled,
                         libcrux_ml_kem_polynomial_PolynomialRingElement_f0),
                     libcrux_ml_kem_polynomial_PolynomialRingElement_f0);
             i++) {
            size_t j = i;
            libcrux_ml_kem_polynomial_PolynomialRingElement_f0 sample = sampled[j];
            if (transpose) {
                A_transpose[j][i1] = sample;
            } else {
                A_transpose[i1][j] = sample;
            }
        }

    );
    memcpy(ret, A_transpose,
           (size_t)4U *
               sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0[4U]));
}

/**
A monomorphic instance of K.
with types libcrux_ml_kem_polynomial_PolynomialRingElement
libcrux_ml_kem_vector_portable_vector_type_PortableVector[4size_t], uint8_t

*/
typedef struct tuple_710_s {
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 fst[4U];
    uint8_t snd;
} tuple_710;

/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.PRFxN
with const generics
- K= 4
- LEN= 128
*/
static KRML_MUSTINLINE void
PRFxN_1c2(uint8_t (*input)[33U],
          uint8_t ret[4U][128U])
{
    uint8_t out[4U][128U] = { { 0U } };
    KRML_MAYBE_FOR4(
        i, (size_t)0U, (size_t)4U, (size_t)1U, size_t i0 = i;
        libcrux_sha3_portable_shake256(
            Eurydice_array_to_slice((size_t)128U, out[i0], uint8_t),
            Eurydice_array_to_slice((size_t)33U, input[i0], uint8_t)););
    memcpy(ret, out, (size_t)4U * sizeof(uint8_t[128U]));
}

/**
This function found in impl {(libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>)}
*/
/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.PRFxN_f1
with const generics
- K= 4
- LEN= 128
*/
static KRML_MUSTINLINE void
PRFxN_f1_d52(uint8_t (*input)[33U],
             uint8_t ret[4U][128U])
{
    PRFxN_1c2(input, ret);
}

/**
 Given a series of uniformly random bytes in `randomness`, for some number
 `eta`, the `sample_from_binomial_distribution_{eta}` functions sample a ring
 element from a binomial distribution centered at 0 that uses two sets of `eta`
 coin flips. If, for example, `eta = ETA`, each ring coefficient is a value `v`
 such such that `v ∈ {-ETA, -ETA + 1, ..., 0, ..., ETA + 1, ETA}` and:

 ```plaintext
 - If v < 0, Pr[v] = Pr[-v]
 - If v >= 0, Pr[v] = BINOMIAL_COEFFICIENT(2 * ETA; ETA - v) / 2 ^ (2 * ETA)
 ```

 The values `v < 0` are mapped to the appropriate `KyberFieldElement`.

 The expected value is:

 ```plaintext
 E[X] = (-ETA)Pr[-ETA] + (-(ETA - 1))Pr[-(ETA - 1)] + ... + (ETA - 1)Pr[ETA - 1]
 + (ETA)Pr[ETA] = 0 since Pr[-v] = Pr[v] when v < 0.
 ```

 And the variance is:

 ```plaintext
 Var(X) = E[(X - E[X])^2]
        = E[X^2]
        = sum_(v=-ETA to ETA)v^2 * (BINOMIAL_COEFFICIENT(2 * ETA; ETA - v) /
 2^(2 * ETA)) = ETA / 2
 ```

 This function implements <strong>Algorithm 7</strong> of the NIST FIPS 203
 standard, which is reproduced below:

 ```plaintext
 Input: byte array B ∈ 𝔹^{64η}.
 Output: array f ∈ ℤ₂₅₆.

 b ← BytesToBits(B)
 for (i ← 0; i < 256; i++)
     x ← ∑(j=0 to η - 1) b[2iη + j]
     y ← ∑(j=0 to η - 1) b[2iη + η + j]
     f[i] ← x−y mod q
 end for
 return f
 ```

 The NIST FIPS 203 standard can be found at
 <https://csrc.nist.gov/pubs/fips/203/ipd>.
*/
/**
A monomorphic instance of
libcrux_ml_kem.sampling.sample_from_binomial_distribution_2 with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics

*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_f0
sample_from_binomial_distribution_2_7b(Eurydice_slice randomness)
{
    int16_t sampled_i16s[256U] = { 0U };
    for (size_t i0 = (size_t)0U;
         i0 < Eurydice_slice_len(randomness, uint8_t) / (size_t)4U; i0++) {
        size_t chunk_number = i0;
        Eurydice_slice byte_chunk = Eurydice_slice_subslice2(
            randomness, chunk_number * (size_t)4U,
            chunk_number * (size_t)4U + (size_t)4U, uint8_t);
        uint32_t random_bits_as_u32 =
            (((uint32_t)Eurydice_slice_index(byte_chunk, (size_t)0U, uint8_t,
                                             uint8_t *) |
              (uint32_t)Eurydice_slice_index(byte_chunk, (size_t)1U, uint8_t,
                                             uint8_t *)
                  << 8U) |
             (uint32_t)Eurydice_slice_index(byte_chunk, (size_t)2U, uint8_t,
                                            uint8_t *)
                 << 16U) |
            (uint32_t)Eurydice_slice_index(byte_chunk, (size_t)3U, uint8_t,
                                           uint8_t *)
                << 24U;
        uint32_t even_bits = random_bits_as_u32 & 1431655765U;
        uint32_t odd_bits = random_bits_as_u32 >> 1U & 1431655765U;
        uint32_t coin_toss_outcomes = even_bits + odd_bits;
        for (uint32_t i = 0U; i < CORE_NUM__U32_8__BITS / 4U; i++) {
            uint32_t outcome_set = i;
            uint32_t outcome_set0 = outcome_set * 4U;
            int16_t outcome_1 =
                (int16_t)(coin_toss_outcomes >> (uint32_t)outcome_set0 & 3U);
            int16_t outcome_2 =
                (int16_t)(coin_toss_outcomes >> (uint32_t)(outcome_set0 + 2U) & 3U);
            size_t offset = (size_t)(outcome_set0 >> 2U);
            sampled_i16s[(size_t)8U * chunk_number + offset] = outcome_1 - outcome_2;
        }
    }
    return from_i16_array_89_33(
        Eurydice_array_to_slice((size_t)256U, sampled_i16s, int16_t));
}

/**
A monomorphic instance of
libcrux_ml_kem.sampling.sample_from_binomial_distribution_3 with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics

*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_f0
sample_from_binomial_distribution_3_14(Eurydice_slice randomness)
{
    int16_t sampled_i16s[256U] = { 0U };
    for (size_t i0 = (size_t)0U;
         i0 < Eurydice_slice_len(randomness, uint8_t) / (size_t)3U; i0++) {
        size_t chunk_number = i0;
        Eurydice_slice byte_chunk = Eurydice_slice_subslice2(
            randomness, chunk_number * (size_t)3U,
            chunk_number * (size_t)3U + (size_t)3U, uint8_t);
        uint32_t random_bits_as_u24 =
            ((uint32_t)Eurydice_slice_index(byte_chunk, (size_t)0U, uint8_t,
                                            uint8_t *) |
             (uint32_t)Eurydice_slice_index(byte_chunk, (size_t)1U, uint8_t,
                                            uint8_t *)
                 << 8U) |
            (uint32_t)Eurydice_slice_index(byte_chunk, (size_t)2U, uint8_t,
                                           uint8_t *)
                << 16U;
        uint32_t first_bits = random_bits_as_u24 & 2396745U;
        uint32_t second_bits = random_bits_as_u24 >> 1U & 2396745U;
        uint32_t third_bits = random_bits_as_u24 >> 2U & 2396745U;
        uint32_t coin_toss_outcomes = first_bits + second_bits + third_bits;
        for (int32_t i = (int32_t)0; i < (int32_t)24 / (int32_t)6; i++) {
            int32_t outcome_set = i;
            int32_t outcome_set0 = outcome_set * (int32_t)6;
            int16_t outcome_1 =
                (int16_t)(coin_toss_outcomes >> (uint32_t)outcome_set0 & 7U);
            int16_t outcome_2 = (int16_t)(coin_toss_outcomes >>
                                              (uint32_t)(outcome_set0 + (int32_t)3) &
                                          7U);
            size_t offset = (size_t)(outcome_set0 / (int32_t)6);
            sampled_i16s[(size_t)4U * chunk_number + offset] = outcome_1 - outcome_2;
        }
    }
    return from_i16_array_89_33(
        Eurydice_array_to_slice((size_t)256U, sampled_i16s, int16_t));
}

/**
A monomorphic instance of
libcrux_ml_kem.sampling.sample_from_binomial_distribution with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- ETA= 2
*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_f0
sample_from_binomial_distribution_ca(Eurydice_slice randomness)
{
    return sample_from_binomial_distribution_2_7b(randomness);
}

/**
A monomorphic instance of libcrux_ml_kem.ntt.ntt_at_layer_7
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void
ntt_at_layer_7_60(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *re)
{
    size_t step = LIBCRUX_ML_KEM_POLYNOMIAL_VECTORS_IN_RING_ELEMENT / (size_t)2U;
    for (size_t i = (size_t)0U; i < step; i++) {
        size_t j = i;
        libcrux_ml_kem_vector_portable_vector_type_PortableVector t =
            libcrux_ml_kem_vector_portable_multiply_by_constant_0d(
                re->coefficients[j + step], (int16_t)-1600);
        re->coefficients[j + step] =
            libcrux_ml_kem_vector_portable_sub_0d(re->coefficients[j], &t);
        libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____1 =
            libcrux_ml_kem_vector_portable_add_0d(re->coefficients[j], &t);
        re->coefficients[j] = uu____1;
    }
}

typedef struct libcrux_ml_kem_vector_portable_vector_type_PortableVector_x2_s {
    libcrux_ml_kem_vector_portable_vector_type_PortableVector fst;
    libcrux_ml_kem_vector_portable_vector_type_PortableVector snd;
} libcrux_ml_kem_vector_portable_vector_type_PortableVector_x2;

/**
A monomorphic instance of libcrux_ml_kem.vector.traits.montgomery_multiply_fe
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static libcrux_ml_kem_vector_portable_vector_type_PortableVector
montgomery_multiply_fe_05(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v, int16_t fer)
{
    return libcrux_ml_kem_vector_portable_montgomery_multiply_by_constant_0d(v,
                                                                             fer);
}

/**
A monomorphic instance of libcrux_ml_kem.ntt.ntt_layer_int_vec_step
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE
    libcrux_ml_kem_vector_portable_vector_type_PortableVector_x2
    ntt_layer_int_vec_step_88(
        libcrux_ml_kem_vector_portable_vector_type_PortableVector a,
        libcrux_ml_kem_vector_portable_vector_type_PortableVector b,
        int16_t zeta_r)
{
    libcrux_ml_kem_vector_portable_vector_type_PortableVector t =
        montgomery_multiply_fe_05(b, zeta_r);
    b = libcrux_ml_kem_vector_portable_sub_0d(a, &t);
    a = libcrux_ml_kem_vector_portable_add_0d(a, &t);
    return (
        CLITERAL(libcrux_ml_kem_vector_portable_vector_type_PortableVector_x2){
            .fst = a, .snd = b });
}

/**
A monomorphic instance of libcrux_ml_kem.ntt.ntt_at_layer_4_plus
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void
ntt_at_layer_4_plus_8c(
    size_t *zeta_i, libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *re,
    size_t layer)
{
    size_t step = (size_t)1U << (uint32_t)layer;
    for (size_t i0 = (size_t)0U; i0 < (size_t)128U >> (uint32_t)layer; i0++) {
        size_t round = i0;
        zeta_i[0U] = zeta_i[0U] + (size_t)1U;
        size_t offset = round * step * (size_t)2U;
        size_t offset_vec = offset / (size_t)16U;
        size_t step_vec = step / (size_t)16U;
        for (size_t i = offset_vec; i < offset_vec + step_vec; i++) {
            size_t j = i;
            libcrux_ml_kem_vector_portable_vector_type_PortableVector_x2 uu____0 =
                ntt_layer_int_vec_step_88(
                    re->coefficients[j], re->coefficients[j + step_vec],
                    libcrux_ml_kem_polynomial_ZETAS_TIMES_MONTGOMERY_R[zeta_i[0U]]);
            libcrux_ml_kem_vector_portable_vector_type_PortableVector x = uu____0.fst;
            libcrux_ml_kem_vector_portable_vector_type_PortableVector y = uu____0.snd;
            re->coefficients[j] = x;
            re->coefficients[j + step_vec] = y;
        }
    }
}

/**
A monomorphic instance of libcrux_ml_kem.ntt.ntt_at_layer_3
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void
ntt_at_layer_3_34(
    size_t *zeta_i, libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *re)
{
    KRML_MAYBE_FOR16(
        i, (size_t)0U, (size_t)16U, (size_t)1U, size_t round = i;
        zeta_i[0U] = zeta_i[0U] + (size_t)1U;
        libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
            libcrux_ml_kem_vector_portable_ntt_layer_3_step_0d(
                re->coefficients[round],
                libcrux_ml_kem_polynomial_ZETAS_TIMES_MONTGOMERY_R[zeta_i[0U]]);
        re->coefficients[round] = uu____0;);
}

/**
A monomorphic instance of libcrux_ml_kem.ntt.ntt_at_layer_2
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void
ntt_at_layer_2_26(
    size_t *zeta_i, libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *re)
{
    KRML_MAYBE_FOR16(
        i, (size_t)0U, (size_t)16U, (size_t)1U, size_t round = i;
        zeta_i[0U] = zeta_i[0U] + (size_t)1U;
        re->coefficients[round] =
            libcrux_ml_kem_vector_portable_ntt_layer_2_step_0d(
                re->coefficients[round],
                libcrux_ml_kem_polynomial_ZETAS_TIMES_MONTGOMERY_R[zeta_i[0U]],
                libcrux_ml_kem_polynomial_ZETAS_TIMES_MONTGOMERY_R[zeta_i[0U] +
                                                                   (size_t)1U]);
        zeta_i[0U] = zeta_i[0U] + (size_t)1U;);
}

/**
A monomorphic instance of libcrux_ml_kem.ntt.ntt_at_layer_1
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void
ntt_at_layer_1_3c(
    size_t *zeta_i, libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *re)
{
    KRML_MAYBE_FOR16(
        i, (size_t)0U, (size_t)16U, (size_t)1U, size_t round = i;
        zeta_i[0U] = zeta_i[0U] + (size_t)1U;
        re->coefficients[round] =
            libcrux_ml_kem_vector_portable_ntt_layer_1_step_0d(
                re->coefficients[round],
                libcrux_ml_kem_polynomial_ZETAS_TIMES_MONTGOMERY_R[zeta_i[0U]],
                libcrux_ml_kem_polynomial_ZETAS_TIMES_MONTGOMERY_R[zeta_i[0U] +
                                                                   (size_t)1U],
                libcrux_ml_kem_polynomial_ZETAS_TIMES_MONTGOMERY_R[zeta_i[0U] +
                                                                   (size_t)2U],
                libcrux_ml_kem_polynomial_ZETAS_TIMES_MONTGOMERY_R[zeta_i[0U] +
                                                                   (size_t)3U]);
        zeta_i[0U] = zeta_i[0U] + (size_t)3U;);
}

/**
This function found in impl
{libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0]}
*/
/**
A monomorphic instance of libcrux_ml_kem.polynomial.poly_barrett_reduce_89
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void
poly_barrett_reduce_89_d8(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *self)
{
    for (size_t i = (size_t)0U;
         i < LIBCRUX_ML_KEM_POLYNOMIAL_VECTORS_IN_RING_ELEMENT; i++) {
        size_t i0 = i;
        libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
            libcrux_ml_kem_vector_portable_barrett_reduce_0d(
                self->coefficients[i0]);
        self->coefficients[i0] = uu____0;
    }
}

/**
A monomorphic instance of libcrux_ml_kem.ntt.ntt_binomially_sampled_ring_element
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void
ntt_binomially_sampled_ring_element_63(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *re)
{
    ntt_at_layer_7_60(re);
    size_t zeta_i = (size_t)1U;
    ntt_at_layer_4_plus_8c(&zeta_i, re, (size_t)6U);
    ntt_at_layer_4_plus_8c(&zeta_i, re, (size_t)5U);
    ntt_at_layer_4_plus_8c(&zeta_i, re, (size_t)4U);
    ntt_at_layer_3_34(&zeta_i, re);
    ntt_at_layer_2_26(&zeta_i, re);
    ntt_at_layer_1_3c(&zeta_i, re);
    poly_barrett_reduce_89_d8(re);
}

/**
 Sample a vector of ring elements from a centered binomial distribution and
 convert them into their NTT representations.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.sample_vector_cbd_then_ntt
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$4size_t]] with const
generics
- K= 4
- ETA= 2
- ETA_RANDOMNESS_SIZE= 128
*/
static KRML_MUSTINLINE tuple_710
sample_vector_cbd_then_ntt_781(
    uint8_t prf_input[33U], uint8_t domain_separator)
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 re_as_ntt[4U];
    KRML_MAYBE_FOR4(i, (size_t)0U, (size_t)4U, (size_t)1U,
                    re_as_ntt[i] = ZERO_89_c3(););
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_prf_input[33U];
    memcpy(copy_of_prf_input, prf_input, (size_t)33U * sizeof(uint8_t));
    uint8_t prf_inputs[4U][33U];
    KRML_MAYBE_FOR4(
        i, (size_t)0U, (size_t)4U, (size_t)1U,
        memcpy(prf_inputs[i], copy_of_prf_input, (size_t)33U * sizeof(uint8_t)););
    KRML_MAYBE_FOR4(i, (size_t)0U, (size_t)4U, (size_t)1U, size_t i0 = i;
                    prf_inputs[i0][32U] = domain_separator;
                    domain_separator = (uint32_t)domain_separator + 1U;);
    uint8_t prf_outputs[4U][128U];
    PRFxN_f1_d52(prf_inputs, prf_outputs);
    KRML_MAYBE_FOR4(
        i, (size_t)0U, (size_t)4U, (size_t)1U, size_t i0 = i;
        re_as_ntt[i0] = sample_from_binomial_distribution_ca(
            Eurydice_array_to_slice((size_t)128U, prf_outputs[i0], uint8_t));
        ntt_binomially_sampled_ring_element_63(&re_as_ntt[i0]););
    /* Passing arrays by value in Rust generates a copy in C */
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 copy_of_re_as_ntt[4U];
    memcpy(
        copy_of_re_as_ntt, re_as_ntt,
        (size_t)4U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    tuple_710 lit;
    memcpy(
        lit.fst, copy_of_re_as_ntt,
        (size_t)4U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    lit.snd = domain_separator;
    return lit;
}

/**
 Given two `KyberPolynomialRingElement`s in their NTT representations,
 compute their product. Given two polynomials in the NTT domain `f^` and `ĵ`,
 the `iᵗʰ` coefficient of the product `k̂` is determined by the calculation:

 ```plaintext
 ĥ[2·i] + ĥ[2·i + 1]X = (f^[2·i] + f^[2·i + 1]X)·(ĝ[2·i] + ĝ[2·i + 1]X) mod (X²
 - ζ^(2·BitRev₇(i) + 1))
 ```

 This function almost implements <strong>Algorithm 10</strong> of the
 NIST FIPS 203 standard, which is reproduced below:

 ```plaintext
 Input: Two arrays fˆ ∈ ℤ₂₅₆ and ĝ ∈ ℤ₂₅₆.
 Output: An array ĥ ∈ ℤq.

 for(i ← 0; i < 128; i++)
     (ĥ[2i], ĥ[2i+1]) ← BaseCaseMultiply(fˆ[2i], fˆ[2i+1], ĝ[2i], ĝ[2i+1],
 ζ^(2·BitRev₇(i) + 1)) end for return ĥ
 ```
 We say "almost" because the coefficients of the ring element output by
 this function are in the Montgomery domain.

 The NIST FIPS 203 standard can be found at
 <https://csrc.nist.gov/pubs/fips/203/ipd>.
*/
/**
This function found in impl
{libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0]}
*/
/**
A monomorphic instance of libcrux_ml_kem.polynomial.ntt_multiply_89
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_f0
ntt_multiply_89_3b(libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *self,
                   libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *rhs)
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 out = ZERO_89_c3();
    for (size_t i = (size_t)0U;
         i < LIBCRUX_ML_KEM_POLYNOMIAL_VECTORS_IN_RING_ELEMENT; i++) {
        size_t i0 = i;
        libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
            libcrux_ml_kem_vector_portable_ntt_multiply_0d(
                &self->coefficients[i0], &rhs->coefficients[i0],
                libcrux_ml_kem_polynomial_ZETAS_TIMES_MONTGOMERY_R[(size_t)64U +
                                                                   (size_t)4U * i0],
                libcrux_ml_kem_polynomial_ZETAS_TIMES_MONTGOMERY_R[(size_t)64U +
                                                                   (size_t)4U * i0 +
                                                                   (size_t)1U],
                libcrux_ml_kem_polynomial_ZETAS_TIMES_MONTGOMERY_R[(size_t)64U +
                                                                   (size_t)4U * i0 +
                                                                   (size_t)2U],
                libcrux_ml_kem_polynomial_ZETAS_TIMES_MONTGOMERY_R[(size_t)64U +
                                                                   (size_t)4U * i0 +
                                                                   (size_t)3U]);
        out.coefficients[i0] = uu____0;
    }
    return out;
}

/**
 Given two polynomial ring elements `lhs` and `rhs`, compute the pointwise
 sum of their constituent coefficients.
*/
/**
This function found in impl
{libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0]}
*/
/**
A monomorphic instance of libcrux_ml_kem.polynomial.add_to_ring_element_89
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 4
*/
static KRML_MUSTINLINE void
add_to_ring_element_89_1e1(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *self,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *rhs)
{
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(
                 Eurydice_array_to_slice(
                     (size_t)16U, self->coefficients,
                     libcrux_ml_kem_vector_portable_vector_type_PortableVector),
                 libcrux_ml_kem_vector_portable_vector_type_PortableVector);
         i++) {
        size_t i0 = i;
        libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
            libcrux_ml_kem_vector_portable_add_0d(self->coefficients[i0],
                                                  &rhs->coefficients[i0]);
        self->coefficients[i0] = uu____0;
    }
}

/**
A monomorphic instance of libcrux_ml_kem.vector.traits.to_standard_domain
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static libcrux_ml_kem_vector_portable_vector_type_PortableVector
to_standard_domain_21(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v)
{
    return libcrux_ml_kem_vector_portable_montgomery_multiply_by_constant_0d(
        v, LIBCRUX_ML_KEM_VECTOR_TRAITS_MONTGOMERY_R_SQUARED_MOD_FIELD_MODULUS);
}

/**
This function found in impl
{libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0]}
*/
/**
A monomorphic instance of libcrux_ml_kem.polynomial.add_standard_error_reduce_89
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void
add_standard_error_reduce_89_64(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *self,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *error)
{
    for (size_t i = (size_t)0U;
         i < LIBCRUX_ML_KEM_POLYNOMIAL_VECTORS_IN_RING_ELEMENT; i++) {
        size_t j = i;
        libcrux_ml_kem_vector_portable_vector_type_PortableVector
            coefficient_normal_form = to_standard_domain_21(self->coefficients[j]);
        libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
            libcrux_ml_kem_vector_portable_barrett_reduce_0d(
                libcrux_ml_kem_vector_portable_add_0d(coefficient_normal_form,
                                                      &error->coefficients[j]));
        self->coefficients[j] = uu____0;
    }
}

/**
 Compute Â ◦ ŝ + ê
*/
/**
A monomorphic instance of libcrux_ml_kem.matrix.compute_As_plus_e
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 4
*/
static KRML_MUSTINLINE void
compute_As_plus_e_c71(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 (*matrix_A)[4U],
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *s_as_ntt,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *error_as_ntt,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret[4U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 result[4U];
    KRML_MAYBE_FOR4(i, (size_t)0U, (size_t)4U, (size_t)1U,
                    result[i] = ZERO_89_c3(););
    for (size_t i0 = (size_t)0U;
         i0 < Eurydice_slice_len(
                  Eurydice_array_to_slice(
                      (size_t)4U, matrix_A,
                      libcrux_ml_kem_polynomial_PolynomialRingElement_f0[4U]),
                  libcrux_ml_kem_polynomial_PolynomialRingElement_f0[4U]);
         i0++) {
        size_t i1 = i0;
        libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *row = matrix_A[i1];
        for (size_t i = (size_t)0U;
             i < Eurydice_slice_len(
                     Eurydice_array_to_slice(
                         (size_t)4U, row,
                         libcrux_ml_kem_polynomial_PolynomialRingElement_f0),
                     libcrux_ml_kem_polynomial_PolynomialRingElement_f0);
             i++) {
            size_t j = i;
            libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *matrix_element =
                &row[j];
            libcrux_ml_kem_polynomial_PolynomialRingElement_f0 product =
                ntt_multiply_89_3b(matrix_element, &s_as_ntt[j]);
            add_to_ring_element_89_1e1(&result[i1], &product);
        }
        add_standard_error_reduce_89_64(&result[i1], &error_as_ntt[i1]);
    }
    memcpy(
        ret, result,
        (size_t)4U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
}

/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.generate_keypair
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$4size_t]],
libcrux_ml_kem_variant_MlKem with const generics
- K= 4
- PRIVATE_KEY_SIZE= 1536
- PUBLIC_KEY_SIZE= 1568
- RANKED_BYTES_PER_RING_ELEMENT= 1536
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
*/
static libcrux_ml_kem_utils_extraction_helper_Keypair1024
generate_keypair_a21(
    Eurydice_slice key_generation_seed)
{
    uint8_t hashed[64U];
    cpa_keygen_seed_d8_72(key_generation_seed, hashed);
    Eurydice_slice_uint8_t_x2 uu____0 = Eurydice_slice_split_at(
        Eurydice_array_to_slice((size_t)64U, hashed, uint8_t), (size_t)32U,
        uint8_t, Eurydice_slice_uint8_t_x2);
    Eurydice_slice seed_for_A0 = uu____0.fst;
    Eurydice_slice seed_for_secret_and_error = uu____0.snd;
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 A_transpose[4U][4U];
    uint8_t ret[34U];
    libcrux_ml_kem_utils_into_padded_array_6d1(seed_for_A0, ret);
    sample_matrix_A_e71(ret, true, A_transpose);
    uint8_t prf_input[33U];
    libcrux_ml_kem_utils_into_padded_array_6d2(seed_for_secret_and_error,
                                               prf_input);
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_prf_input0[33U];
    memcpy(copy_of_prf_input0, prf_input, (size_t)33U * sizeof(uint8_t));
    tuple_710 uu____2 = sample_vector_cbd_then_ntt_781(copy_of_prf_input0, 0U);
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 secret_as_ntt[4U];
    memcpy(
        secret_as_ntt, uu____2.fst,
        (size_t)4U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    uint8_t domain_separator = uu____2.snd;
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_prf_input[33U];
    memcpy(copy_of_prf_input, prf_input, (size_t)33U * sizeof(uint8_t));
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 error_as_ntt[4U];
    memcpy(
        error_as_ntt,
        sample_vector_cbd_then_ntt_781(copy_of_prf_input, domain_separator).fst,
        (size_t)4U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 t_as_ntt[4U];
    compute_As_plus_e_c71(A_transpose, secret_as_ntt, error_as_ntt, t_as_ntt);
    uint8_t seed_for_A[32U];
    core_result_Result_00 dst;
    Eurydice_slice_to_array2(&dst, seed_for_A0, Eurydice_slice, uint8_t[32U]);
    core_result_unwrap_41_33(dst, seed_for_A);
    uint8_t public_key_serialized[1568U];
    serialize_public_key_671(
        t_as_ntt, Eurydice_array_to_slice((size_t)32U, seed_for_A, uint8_t),
        public_key_serialized);
    uint8_t secret_key_serialized[1536U];
    serialize_secret_key_a31(secret_as_ntt, secret_key_serialized);
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_secret_key_serialized[1536U];
    memcpy(copy_of_secret_key_serialized, secret_key_serialized,
           (size_t)1536U * sizeof(uint8_t));
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_public_key_serialized[1568U];
    memcpy(copy_of_public_key_serialized, public_key_serialized,
           (size_t)1568U * sizeof(uint8_t));
    libcrux_ml_kem_utils_extraction_helper_Keypair1024 lit;
    memcpy(lit.fst, copy_of_secret_key_serialized,
           (size_t)1536U * sizeof(uint8_t));
    memcpy(lit.snd, copy_of_public_key_serialized,
           (size_t)1568U * sizeof(uint8_t));
    return lit;
}

/**
 Serialize the secret key.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cca.serialize_kem_secret_key
with types libcrux_ml_kem_hash_functions_portable_PortableHash[[$4size_t]]
with const generics
- K= 4
- SERIALIZED_KEY_LEN= 3168
*/
static KRML_MUSTINLINE void
serialize_kem_secret_key_7d(
    Eurydice_slice private_key, Eurydice_slice public_key,
    Eurydice_slice implicit_rejection_value, uint8_t ret[3168U])
{
    uint8_t out[3168U] = { 0U };
    size_t pointer = (size_t)0U;
    uint8_t *uu____0 = out;
    size_t uu____1 = pointer;
    size_t uu____2 = pointer;
    Eurydice_slice_copy(
        Eurydice_array_to_subslice2(
            uu____0, uu____1, uu____2 + Eurydice_slice_len(private_key, uint8_t),
            uint8_t),
        private_key, uint8_t);
    pointer = pointer + Eurydice_slice_len(private_key, uint8_t);
    uint8_t *uu____3 = out;
    size_t uu____4 = pointer;
    size_t uu____5 = pointer;
    Eurydice_slice_copy(
        Eurydice_array_to_subslice2(
            uu____3, uu____4, uu____5 + Eurydice_slice_len(public_key, uint8_t),
            uint8_t),
        public_key, uint8_t);
    pointer = pointer + Eurydice_slice_len(public_key, uint8_t);
    Eurydice_slice uu____6 = Eurydice_array_to_subslice2(
        out, pointer, pointer + LIBCRUX_ML_KEM_CONSTANTS_H_DIGEST_SIZE, uint8_t);
    uint8_t ret0[32U];
    H_f1_191(public_key, ret0);
    Eurydice_slice_copy(
        uu____6, Eurydice_array_to_slice((size_t)32U, ret0, uint8_t), uint8_t);
    pointer = pointer + LIBCRUX_ML_KEM_CONSTANTS_H_DIGEST_SIZE;
    uint8_t *uu____7 = out;
    size_t uu____8 = pointer;
    size_t uu____9 = pointer;
    Eurydice_slice_copy(
        Eurydice_array_to_subslice2(
            uu____7, uu____8,
            uu____9 + Eurydice_slice_len(implicit_rejection_value, uint8_t),
            uint8_t),
        implicit_rejection_value, uint8_t);
    memcpy(ret, out, (size_t)3168U * sizeof(uint8_t));
}

/**
 Packed API

 Generate a key pair.

 Depending on the `Vector` and `Hasher` used, this requires different hardware
 features
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cca.generate_keypair
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$4size_t]],
libcrux_ml_kem_variant_MlKem with const generics
- K= 4
- CPA_PRIVATE_KEY_SIZE= 1536
- PRIVATE_KEY_SIZE= 3168
- PUBLIC_KEY_SIZE= 1568
- BYTES_PER_RING_ELEMENT= 1536
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
*/
libcrux_ml_kem_mlkem1024_MlKem1024KeyPair
libcrux_ml_kem_ind_cca_generate_keypair_f61(uint8_t randomness[64U])
{
    Eurydice_slice ind_cpa_keypair_randomness = Eurydice_array_to_subslice2(
        randomness, (size_t)0U,
        LIBCRUX_ML_KEM_CONSTANTS_CPA_PKE_KEY_GENERATION_SEED_SIZE, uint8_t);
    Eurydice_slice implicit_rejection_value = Eurydice_array_to_subslice_from(
        (size_t)64U, randomness,
        LIBCRUX_ML_KEM_CONSTANTS_CPA_PKE_KEY_GENERATION_SEED_SIZE, uint8_t,
        size_t);
    libcrux_ml_kem_utils_extraction_helper_Keypair1024 uu____0 =
        generate_keypair_a21(ind_cpa_keypair_randomness);
    uint8_t ind_cpa_private_key[1536U];
    memcpy(ind_cpa_private_key, uu____0.fst, (size_t)1536U * sizeof(uint8_t));
    uint8_t public_key[1568U];
    memcpy(public_key, uu____0.snd, (size_t)1568U * sizeof(uint8_t));
    uint8_t secret_key_serialized[3168U];
    serialize_kem_secret_key_7d(
        Eurydice_array_to_slice((size_t)1536U, ind_cpa_private_key, uint8_t),
        Eurydice_array_to_slice((size_t)1568U, public_key, uint8_t),
        implicit_rejection_value, secret_key_serialized);
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_secret_key_serialized[3168U];
    memcpy(copy_of_secret_key_serialized, secret_key_serialized,
           (size_t)3168U * sizeof(uint8_t));
    libcrux_ml_kem_types_MlKemPrivateKey_95 private_key =
        libcrux_ml_kem_types_from_05_891(copy_of_secret_key_serialized);
    libcrux_ml_kem_types_MlKemPrivateKey_95 uu____2 = private_key;
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_public_key[1568U];
    memcpy(copy_of_public_key, public_key, (size_t)1568U * sizeof(uint8_t));
    return libcrux_ml_kem_types_from_17_821(
        uu____2, libcrux_ml_kem_types_from_b6_961(copy_of_public_key));
}

/**
This function found in impl {(libcrux_ml_kem::variant::Variant for
libcrux_ml_kem::variant::MlKem)}
*/
/**
A monomorphic instance of libcrux_ml_kem.variant.entropy_preprocess_d8
with types libcrux_ml_kem_hash_functions_portable_PortableHash[[$4size_t]]
with const generics
- K= 4
*/
static KRML_MUSTINLINE void
entropy_preprocess_d8_d1(Eurydice_slice randomness,
                         uint8_t ret[32U])
{
    uint8_t out[32U] = { 0U };
    Eurydice_slice_copy(Eurydice_array_to_slice((size_t)32U, out, uint8_t),
                        randomness, uint8_t);
    memcpy(ret, out, (size_t)32U * sizeof(uint8_t));
}

/**
 This function deserializes ring elements and reduces the result by the field
 modulus.

 This function MUST NOT be used on secret inputs.
*/
/**
A monomorphic instance of
libcrux_ml_kem.serialize.deserialize_ring_elements_reduced with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- PUBLIC_KEY_SIZE= 1536
- K= 4
*/
static KRML_MUSTINLINE void
deserialize_ring_elements_reduced_4f3(
    Eurydice_slice public_key,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret[4U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 deserialized_pk[4U];
    KRML_MAYBE_FOR4(i, (size_t)0U, (size_t)4U, (size_t)1U,
                    deserialized_pk[i] = ZERO_89_c3(););
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(public_key, uint8_t) /
                 LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT;
         i++) {
        size_t i0 = i;
        Eurydice_slice ring_element = Eurydice_slice_subslice2(
            public_key, i0 * LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
            i0 * LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT +
                LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
            uint8_t);
        libcrux_ml_kem_polynomial_PolynomialRingElement_f0 uu____0 =
            deserialize_to_reduced_ring_element_45(ring_element);
        deserialized_pk[i0] = uu____0;
    }
    memcpy(
        ret, deserialized_pk,
        (size_t)4U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
}

/**
 Sample a vector of ring elements from a centered binomial distribution.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.sample_ring_element_cbd
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$4size_t]] with const
generics
- K= 4
- ETA2_RANDOMNESS_SIZE= 128
- ETA2= 2
*/
static KRML_MUSTINLINE tuple_710
sample_ring_element_cbd_a81(uint8_t prf_input[33U], uint8_t domain_separator)
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 error_1[4U];
    KRML_MAYBE_FOR4(i, (size_t)0U, (size_t)4U, (size_t)1U,
                    error_1[i] = ZERO_89_c3(););
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_prf_input[33U];
    memcpy(copy_of_prf_input, prf_input, (size_t)33U * sizeof(uint8_t));
    uint8_t prf_inputs[4U][33U];
    KRML_MAYBE_FOR4(
        i, (size_t)0U, (size_t)4U, (size_t)1U,
        memcpy(prf_inputs[i], copy_of_prf_input, (size_t)33U * sizeof(uint8_t)););
    KRML_MAYBE_FOR4(i, (size_t)0U, (size_t)4U, (size_t)1U, size_t i0 = i;
                    prf_inputs[i0][32U] = domain_separator;
                    domain_separator = (uint32_t)domain_separator + 1U;);
    uint8_t prf_outputs[4U][128U];
    PRFxN_f1_d52(prf_inputs, prf_outputs);
    KRML_MAYBE_FOR4(
        i, (size_t)0U, (size_t)4U, (size_t)1U, size_t i0 = i;
        libcrux_ml_kem_polynomial_PolynomialRingElement_f0 uu____1 =
            sample_from_binomial_distribution_ca(
                Eurydice_array_to_slice((size_t)128U, prf_outputs[i0], uint8_t));
        error_1[i0] = uu____1;);
    /* Passing arrays by value in Rust generates a copy in C */
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 copy_of_error_1[4U];
    memcpy(
        copy_of_error_1, error_1,
        (size_t)4U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    tuple_710 lit;
    memcpy(
        lit.fst, copy_of_error_1,
        (size_t)4U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    lit.snd = domain_separator;
    return lit;
}

/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.PRF
with const generics
- LEN= 128
*/
static KRML_MUSTINLINE void
PRF_7c0(Eurydice_slice input, uint8_t ret[128U])
{
    uint8_t digest[128U] = { 0U };
    libcrux_sha3_portable_shake256(
        Eurydice_array_to_slice((size_t)128U, digest, uint8_t), input);
    memcpy(ret, digest, (size_t)128U * sizeof(uint8_t));
}

/**
This function found in impl {(libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>)}
*/
/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.PRF_f1
with const generics
- K= 4
- LEN= 128
*/
static KRML_MUSTINLINE void
PRF_f1_2e4(Eurydice_slice input,
           uint8_t ret[128U])
{
    PRF_7c0(input, ret);
}

/**
A monomorphic instance of libcrux_ml_kem.invert_ntt.invert_ntt_at_layer_1
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void
invert_ntt_at_layer_1_4b(
    size_t *zeta_i, libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *re)
{
    KRML_MAYBE_FOR16(
        i, (size_t)0U, (size_t)16U, (size_t)1U, size_t round = i;
        zeta_i[0U] = zeta_i[0U] - (size_t)1U;
        re->coefficients[round] =
            libcrux_ml_kem_vector_portable_inv_ntt_layer_1_step_0d(
                re->coefficients[round],
                libcrux_ml_kem_polynomial_ZETAS_TIMES_MONTGOMERY_R[zeta_i[0U]],
                libcrux_ml_kem_polynomial_ZETAS_TIMES_MONTGOMERY_R[zeta_i[0U] -
                                                                   (size_t)1U],
                libcrux_ml_kem_polynomial_ZETAS_TIMES_MONTGOMERY_R[zeta_i[0U] -
                                                                   (size_t)2U],
                libcrux_ml_kem_polynomial_ZETAS_TIMES_MONTGOMERY_R[zeta_i[0U] -
                                                                   (size_t)3U]);
        zeta_i[0U] = zeta_i[0U] - (size_t)3U;);
}

/**
A monomorphic instance of libcrux_ml_kem.invert_ntt.invert_ntt_at_layer_2
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void
invert_ntt_at_layer_2_2b(
    size_t *zeta_i, libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *re)
{
    KRML_MAYBE_FOR16(
        i, (size_t)0U, (size_t)16U, (size_t)1U, size_t round = i;
        zeta_i[0U] = zeta_i[0U] - (size_t)1U;
        re->coefficients[round] =
            libcrux_ml_kem_vector_portable_inv_ntt_layer_2_step_0d(
                re->coefficients[round],
                libcrux_ml_kem_polynomial_ZETAS_TIMES_MONTGOMERY_R[zeta_i[0U]],
                libcrux_ml_kem_polynomial_ZETAS_TIMES_MONTGOMERY_R[zeta_i[0U] -
                                                                   (size_t)1U]);
        zeta_i[0U] = zeta_i[0U] - (size_t)1U;);
}

/**
A monomorphic instance of libcrux_ml_kem.invert_ntt.invert_ntt_at_layer_3
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void
invert_ntt_at_layer_3_97(
    size_t *zeta_i, libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *re)
{
    KRML_MAYBE_FOR16(
        i, (size_t)0U, (size_t)16U, (size_t)1U, size_t round = i;
        zeta_i[0U] = zeta_i[0U] - (size_t)1U;
        libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
            libcrux_ml_kem_vector_portable_inv_ntt_layer_3_step_0d(
                re->coefficients[round],
                libcrux_ml_kem_polynomial_ZETAS_TIMES_MONTGOMERY_R[zeta_i[0U]]);
        re->coefficients[round] = uu____0;);
}

/**
A monomorphic instance of
libcrux_ml_kem.invert_ntt.inv_ntt_layer_int_vec_step_reduce with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics

*/
static KRML_MUSTINLINE
    libcrux_ml_kem_vector_portable_vector_type_PortableVector_x2
    inv_ntt_layer_int_vec_step_reduce_aa(
        libcrux_ml_kem_vector_portable_vector_type_PortableVector a,
        libcrux_ml_kem_vector_portable_vector_type_PortableVector b,
        int16_t zeta_r)
{
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a_minus_b =
        libcrux_ml_kem_vector_portable_sub_0d(b, &a);
    a = libcrux_ml_kem_vector_portable_barrett_reduce_0d(
        libcrux_ml_kem_vector_portable_add_0d(a, &b));
    b = montgomery_multiply_fe_05(a_minus_b, zeta_r);
    return (
        CLITERAL(libcrux_ml_kem_vector_portable_vector_type_PortableVector_x2){
            .fst = a, .snd = b });
}

/**
A monomorphic instance of libcrux_ml_kem.invert_ntt.invert_ntt_at_layer_4_plus
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void
invert_ntt_at_layer_4_plus_04(
    size_t *zeta_i, libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *re,
    size_t layer)
{
    size_t step = (size_t)1U << (uint32_t)layer;
    for (size_t i0 = (size_t)0U; i0 < (size_t)128U >> (uint32_t)layer; i0++) {
        size_t round = i0;
        zeta_i[0U] = zeta_i[0U] - (size_t)1U;
        size_t offset = round * step * (size_t)2U;
        size_t offset_vec =
            offset / LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR;
        size_t step_vec =
            step / LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR;
        for (size_t i = offset_vec; i < offset_vec + step_vec; i++) {
            size_t j = i;
            libcrux_ml_kem_vector_portable_vector_type_PortableVector_x2 uu____0 =
                inv_ntt_layer_int_vec_step_reduce_aa(
                    re->coefficients[j], re->coefficients[j + step_vec],
                    libcrux_ml_kem_polynomial_ZETAS_TIMES_MONTGOMERY_R[zeta_i[0U]]);
            libcrux_ml_kem_vector_portable_vector_type_PortableVector x = uu____0.fst;
            libcrux_ml_kem_vector_portable_vector_type_PortableVector y = uu____0.snd;
            re->coefficients[j] = x;
            re->coefficients[j + step_vec] = y;
        }
    }
}

/**
A monomorphic instance of libcrux_ml_kem.invert_ntt.invert_ntt_montgomery
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 4
*/
static KRML_MUSTINLINE void
invert_ntt_montgomery_c91(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *re)
{
    size_t zeta_i =
        LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT / (size_t)2U;
    invert_ntt_at_layer_1_4b(&zeta_i, re);
    invert_ntt_at_layer_2_2b(&zeta_i, re);
    invert_ntt_at_layer_3_97(&zeta_i, re);
    invert_ntt_at_layer_4_plus_04(&zeta_i, re, (size_t)4U);
    invert_ntt_at_layer_4_plus_04(&zeta_i, re, (size_t)5U);
    invert_ntt_at_layer_4_plus_04(&zeta_i, re, (size_t)6U);
    invert_ntt_at_layer_4_plus_04(&zeta_i, re, (size_t)7U);
    poly_barrett_reduce_89_d8(re);
}

/**
This function found in impl
{libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0]}
*/
/**
A monomorphic instance of libcrux_ml_kem.polynomial.add_error_reduce_89
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void
add_error_reduce_89_5d(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *self,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *error)
{
    for (size_t i = (size_t)0U;
         i < LIBCRUX_ML_KEM_POLYNOMIAL_VECTORS_IN_RING_ELEMENT; i++) {
        size_t j = i;
        libcrux_ml_kem_vector_portable_vector_type_PortableVector
            coefficient_normal_form =
                libcrux_ml_kem_vector_portable_montgomery_multiply_by_constant_0d(
                    self->coefficients[j], (int16_t)1441);
        libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
            libcrux_ml_kem_vector_portable_barrett_reduce_0d(
                libcrux_ml_kem_vector_portable_add_0d(coefficient_normal_form,
                                                      &error->coefficients[j]));
        self->coefficients[j] = uu____0;
    }
}

/**
 Compute u := InvertNTT(Aᵀ ◦ r̂) + e₁
*/
/**
A monomorphic instance of libcrux_ml_kem.matrix.compute_vector_u
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 4
*/
static KRML_MUSTINLINE void
compute_vector_u_931(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 (*a_as_ntt)[4U],
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *r_as_ntt,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *error_1,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret[4U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 result[4U];
    KRML_MAYBE_FOR4(i, (size_t)0U, (size_t)4U, (size_t)1U,
                    result[i] = ZERO_89_c3(););
    for (size_t i0 = (size_t)0U;
         i0 < Eurydice_slice_len(
                  Eurydice_array_to_slice(
                      (size_t)4U, a_as_ntt,
                      libcrux_ml_kem_polynomial_PolynomialRingElement_f0[4U]),
                  libcrux_ml_kem_polynomial_PolynomialRingElement_f0[4U]);
         i0++) {
        size_t i1 = i0;
        libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *row = a_as_ntt[i1];
        for (size_t i = (size_t)0U;
             i < Eurydice_slice_len(
                     Eurydice_array_to_slice(
                         (size_t)4U, row,
                         libcrux_ml_kem_polynomial_PolynomialRingElement_f0),
                     libcrux_ml_kem_polynomial_PolynomialRingElement_f0);
             i++) {
            size_t j = i;
            libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *a_element = &row[j];
            libcrux_ml_kem_polynomial_PolynomialRingElement_f0 product =
                ntt_multiply_89_3b(a_element, &r_as_ntt[j]);
            add_to_ring_element_89_1e1(&result[i1], &product);
        }
        invert_ntt_montgomery_c91(&result[i1]);
        add_error_reduce_89_5d(&result[i1], &error_1[i1]);
    }
    memcpy(
        ret, result,
        (size_t)4U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
}

/**
A monomorphic instance of libcrux_ml_kem.vector.traits.decompress_1
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static libcrux_ml_kem_vector_portable_vector_type_PortableVector
decompress_1_3e(libcrux_ml_kem_vector_portable_vector_type_PortableVector v)
{
    libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
        libcrux_ml_kem_vector_portable_ZERO_0d();
    return libcrux_ml_kem_vector_portable_bitwise_and_with_constant_0d(
        libcrux_ml_kem_vector_portable_sub_0d(uu____0, &v), (int16_t)1665);
}

/**
A monomorphic instance of
libcrux_ml_kem.serialize.deserialize_then_decompress_message with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics

*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_f0
deserialize_then_decompress_message_f7(uint8_t serialized[32U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 re = ZERO_89_c3();
    KRML_MAYBE_FOR16(
        i, (size_t)0U, (size_t)16U, (size_t)1U, size_t i0 = i;
        libcrux_ml_kem_vector_portable_vector_type_PortableVector
            coefficient_compressed =
                libcrux_ml_kem_vector_portable_deserialize_1_0d(
                    Eurydice_array_to_subslice2(serialized, (size_t)2U * i0,
                                                (size_t)2U * i0 + (size_t)2U,
                                                uint8_t));
        libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
            decompress_1_3e(coefficient_compressed);
        re.coefficients[i0] = uu____0;);
    return re;
}

/**
This function found in impl
{libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0]}
*/
/**
A monomorphic instance of libcrux_ml_kem.polynomial.add_message_error_reduce_89
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_f0
add_message_error_reduce_89_c4(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *self,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *message,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 result)
{
    for (size_t i = (size_t)0U;
         i < LIBCRUX_ML_KEM_POLYNOMIAL_VECTORS_IN_RING_ELEMENT; i++) {
        size_t i0 = i;
        libcrux_ml_kem_vector_portable_vector_type_PortableVector
            coefficient_normal_form =
                libcrux_ml_kem_vector_portable_montgomery_multiply_by_constant_0d(
                    result.coefficients[i0], (int16_t)1441);
        libcrux_ml_kem_vector_portable_vector_type_PortableVector tmp =
            libcrux_ml_kem_vector_portable_add_0d(self->coefficients[i0],
                                                  &message->coefficients[i0]);
        libcrux_ml_kem_vector_portable_vector_type_PortableVector tmp0 =
            libcrux_ml_kem_vector_portable_add_0d(coefficient_normal_form, &tmp);
        libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
            libcrux_ml_kem_vector_portable_barrett_reduce_0d(tmp0);
        result.coefficients[i0] = uu____0;
    }
    return result;
}

/**
 Compute InverseNTT(tᵀ ◦ r̂) + e₂ + message
*/
/**
A monomorphic instance of libcrux_ml_kem.matrix.compute_ring_element_v
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 4
*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_f0
compute_ring_element_v_541(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *t_as_ntt,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *r_as_ntt,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *error_2,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *message)
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 result = ZERO_89_c3();
    KRML_MAYBE_FOR4(i, (size_t)0U, (size_t)4U, (size_t)1U, size_t i0 = i;
                    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 product =
                        ntt_multiply_89_3b(&t_as_ntt[i0], &r_as_ntt[i0]);
                    add_to_ring_element_89_1e1(&result, &product););
    invert_ntt_montgomery_c91(&result);
    result = add_message_error_reduce_89_c4(error_2, message, result);
    return result;
}

/**
A monomorphic instance of libcrux_ml_kem.vector.portable.compress.compress
with const generics
- COEFFICIENT_BITS= 10
*/
static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
compress_dc(libcrux_ml_kem_vector_portable_vector_type_PortableVector v)
{
    for (size_t i = (size_t)0U;
         i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
        size_t i0 = i;
        int16_t uu____0 =
            libcrux_ml_kem_vector_portable_compress_compress_ciphertext_coefficient(
                (uint8_t)(int32_t)10, (uint16_t)v.elements[i0]);
        v.elements[i0] = uu____0;
    }
    return v;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
/**
A monomorphic instance of libcrux_ml_kem.vector.portable.compress_0d
with const generics
- COEFFICIENT_BITS= 10
*/
static libcrux_ml_kem_vector_portable_vector_type_PortableVector
compress_0d_96(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v)
{
    return compress_dc(v);
}

/**
A monomorphic instance of libcrux_ml_kem.vector.portable.compress.compress
with const generics
- COEFFICIENT_BITS= 11
*/
static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
compress_dc0(libcrux_ml_kem_vector_portable_vector_type_PortableVector v)
{
    for (size_t i = (size_t)0U;
         i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
        size_t i0 = i;
        int16_t uu____0 =
            libcrux_ml_kem_vector_portable_compress_compress_ciphertext_coefficient(
                (uint8_t)(int32_t)11, (uint16_t)v.elements[i0]);
        v.elements[i0] = uu____0;
    }
    return v;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
/**
A monomorphic instance of libcrux_ml_kem.vector.portable.compress_0d
with const generics
- COEFFICIENT_BITS= 11
*/
static libcrux_ml_kem_vector_portable_vector_type_PortableVector
compress_0d_960(libcrux_ml_kem_vector_portable_vector_type_PortableVector v)
{
    return compress_dc0(v);
}

/**
A monomorphic instance of libcrux_ml_kem.serialize.compress_then_serialize_11
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- OUT_LEN= 352
*/
static KRML_MUSTINLINE void
compress_then_serialize_11_ef0(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *re, uint8_t ret[352U])
{
    uint8_t serialized[352U] = { 0U };
    for (size_t i = (size_t)0U;
         i < LIBCRUX_ML_KEM_POLYNOMIAL_VECTORS_IN_RING_ELEMENT; i++) {
        size_t i0 = i;
        libcrux_ml_kem_vector_portable_vector_type_PortableVector coefficient =
            compress_0d_960(to_unsigned_representative_84(re->coefficients[i0]));
        uint8_t bytes[22U];
        libcrux_ml_kem_vector_portable_serialize_11_0d(coefficient, bytes);
        Eurydice_slice uu____0 = Eurydice_array_to_subslice2(
            serialized, (size_t)22U * i0, (size_t)22U * i0 + (size_t)22U, uint8_t);
        Eurydice_slice_copy(
            uu____0, Eurydice_array_to_slice((size_t)22U, bytes, uint8_t), uint8_t);
    }
    memcpy(ret, serialized, (size_t)352U * sizeof(uint8_t));
}

/**
A monomorphic instance of
libcrux_ml_kem.serialize.compress_then_serialize_ring_element_u with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- COMPRESSION_FACTOR= 11
- OUT_LEN= 352
*/
static KRML_MUSTINLINE void
compress_then_serialize_ring_element_u_2e0(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *re, uint8_t ret[352U])
{
    uint8_t uu____0[352U];
    compress_then_serialize_11_ef0(re, uu____0);
    memcpy(ret, uu____0, (size_t)352U * sizeof(uint8_t));
}

/**
 Call [`compress_then_serialize_ring_element_u`] on each ring element.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.compress_then_serialize_u
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 4
- OUT_LEN= 1408
- COMPRESSION_FACTOR= 11
- BLOCK_LEN= 352
*/
static void
compress_then_serialize_u_281(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 input[4U],
    Eurydice_slice out)
{
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(
                 Eurydice_array_to_slice(
                     (size_t)4U, input,
                     libcrux_ml_kem_polynomial_PolynomialRingElement_f0),
                 libcrux_ml_kem_polynomial_PolynomialRingElement_f0);
         i++) {
        size_t i0 = i;
        libcrux_ml_kem_polynomial_PolynomialRingElement_f0 re = input[i0];
        Eurydice_slice uu____0 = Eurydice_slice_subslice2(
            out, i0 * ((size_t)1408U / (size_t)4U),
            (i0 + (size_t)1U) * ((size_t)1408U / (size_t)4U), uint8_t);
        uint8_t ret[352U];
        compress_then_serialize_ring_element_u_2e0(&re, ret);
        Eurydice_slice_copy(
            uu____0, Eurydice_array_to_slice((size_t)352U, ret, uint8_t), uint8_t);
    }
}

/**
A monomorphic instance of libcrux_ml_kem.vector.portable.compress.compress
with const generics
- COEFFICIENT_BITS= 4
*/
static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
compress_dc1(libcrux_ml_kem_vector_portable_vector_type_PortableVector v)
{
    for (size_t i = (size_t)0U;
         i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
        size_t i0 = i;
        int16_t uu____0 =
            libcrux_ml_kem_vector_portable_compress_compress_ciphertext_coefficient(
                (uint8_t)(int32_t)4, (uint16_t)v.elements[i0]);
        v.elements[i0] = uu____0;
    }
    return v;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
/**
A monomorphic instance of libcrux_ml_kem.vector.portable.compress_0d
with const generics
- COEFFICIENT_BITS= 4
*/
static libcrux_ml_kem_vector_portable_vector_type_PortableVector
compress_0d_961(libcrux_ml_kem_vector_portable_vector_type_PortableVector v)
{
    return compress_dc1(v);
}

/**
A monomorphic instance of libcrux_ml_kem.serialize.compress_then_serialize_4
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void
compress_then_serialize_4_80(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 re,
    Eurydice_slice serialized)
{
    for (size_t i = (size_t)0U;
         i < LIBCRUX_ML_KEM_POLYNOMIAL_VECTORS_IN_RING_ELEMENT; i++) {
        size_t i0 = i;
        libcrux_ml_kem_vector_portable_vector_type_PortableVector coefficient =
            compress_0d_961(to_unsigned_representative_84(re.coefficients[i0]));
        uint8_t bytes[8U];
        libcrux_ml_kem_vector_portable_serialize_4_0d(coefficient, bytes);
        Eurydice_slice_copy(
            Eurydice_slice_subslice2(serialized, (size_t)8U * i0,
                                     (size_t)8U * i0 + (size_t)8U, uint8_t),
            Eurydice_array_to_slice((size_t)8U, bytes, uint8_t), uint8_t);
    }
}

/**
A monomorphic instance of libcrux_ml_kem.vector.portable.compress.compress
with const generics
- COEFFICIENT_BITS= 5
*/
static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
compress_dc2(libcrux_ml_kem_vector_portable_vector_type_PortableVector v)
{
    for (size_t i = (size_t)0U;
         i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
        size_t i0 = i;
        int16_t uu____0 =
            libcrux_ml_kem_vector_portable_compress_compress_ciphertext_coefficient(
                (uint8_t)(int32_t)5, (uint16_t)v.elements[i0]);
        v.elements[i0] = uu____0;
    }
    return v;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
/**
A monomorphic instance of libcrux_ml_kem.vector.portable.compress_0d
with const generics
- COEFFICIENT_BITS= 5
*/
static libcrux_ml_kem_vector_portable_vector_type_PortableVector
compress_0d_962(libcrux_ml_kem_vector_portable_vector_type_PortableVector v)
{
    return compress_dc2(v);
}

/**
A monomorphic instance of libcrux_ml_kem.serialize.compress_then_serialize_5
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void
compress_then_serialize_5_3c(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 re,
    Eurydice_slice serialized)
{
    for (size_t i = (size_t)0U;
         i < LIBCRUX_ML_KEM_POLYNOMIAL_VECTORS_IN_RING_ELEMENT; i++) {
        size_t i0 = i;
        libcrux_ml_kem_vector_portable_vector_type_PortableVector coefficients =
            compress_0d_962(to_unsigned_representative_84(re.coefficients[i0]));
        uint8_t bytes[10U];
        libcrux_ml_kem_vector_portable_serialize_5_0d(coefficients, bytes);
        Eurydice_slice_copy(
            Eurydice_slice_subslice2(serialized, (size_t)10U * i0,
                                     (size_t)10U * i0 + (size_t)10U, uint8_t),
            Eurydice_array_to_slice((size_t)10U, bytes, uint8_t), uint8_t);
    }
}

/**
A monomorphic instance of
libcrux_ml_kem.serialize.compress_then_serialize_ring_element_v with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- COMPRESSION_FACTOR= 5
- OUT_LEN= 160
*/
static KRML_MUSTINLINE void
compress_then_serialize_ring_element_v_650(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 re, Eurydice_slice out)
{
    compress_then_serialize_5_3c(re, out);
}

/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.encrypt
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$4size_t]] with const
generics
- K= 4
- CIPHERTEXT_SIZE= 1568
- T_AS_NTT_ENCODED_SIZE= 1536
- C1_LEN= 1408
- C2_LEN= 160
- U_COMPRESSION_FACTOR= 11
- V_COMPRESSION_FACTOR= 5
- BLOCK_LEN= 352
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
- ETA2= 2
- ETA2_RANDOMNESS_SIZE= 128
*/
static void
encrypt_7b1(Eurydice_slice public_key, uint8_t message[32U],
            Eurydice_slice randomness, uint8_t ret[1568U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 t_as_ntt[4U];
    deserialize_ring_elements_reduced_4f3(
        Eurydice_slice_subslice_to(public_key, (size_t)1536U, uint8_t, size_t),
        t_as_ntt);
    Eurydice_slice seed =
        Eurydice_slice_subslice_from(public_key, (size_t)1536U, uint8_t, size_t);
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 A[4U][4U];
    uint8_t ret0[34U];
    libcrux_ml_kem_utils_into_padded_array_6d1(seed, ret0);
    sample_matrix_A_e71(ret0, false, A);
    uint8_t prf_input[33U];
    libcrux_ml_kem_utils_into_padded_array_6d2(randomness, prf_input);
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_prf_input0[33U];
    memcpy(copy_of_prf_input0, prf_input, (size_t)33U * sizeof(uint8_t));
    tuple_710 uu____1 = sample_vector_cbd_then_ntt_781(copy_of_prf_input0, 0U);
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 r_as_ntt[4U];
    memcpy(
        r_as_ntt, uu____1.fst,
        (size_t)4U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    uint8_t domain_separator0 = uu____1.snd;
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_prf_input[33U];
    memcpy(copy_of_prf_input, prf_input, (size_t)33U * sizeof(uint8_t));
    tuple_710 uu____3 =
        sample_ring_element_cbd_a81(copy_of_prf_input, domain_separator0);
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 error_1[4U];
    memcpy(
        error_1, uu____3.fst,
        (size_t)4U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    uint8_t domain_separator = uu____3.snd;
    prf_input[32U] = domain_separator;
    uint8_t prf_output[128U];
    PRF_f1_2e4(Eurydice_array_to_slice((size_t)33U, prf_input, uint8_t),
               prf_output);
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 error_2 =
        sample_from_binomial_distribution_ca(
            Eurydice_array_to_slice((size_t)128U, prf_output, uint8_t));
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 u[4U];
    compute_vector_u_931(A, r_as_ntt, error_1, u);
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_message[32U];
    memcpy(copy_of_message, message, (size_t)32U * sizeof(uint8_t));
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 message_as_ring_element =
        deserialize_then_decompress_message_f7(copy_of_message);
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 v =
        compute_ring_element_v_541(t_as_ntt, r_as_ntt, &error_2,
                                   &message_as_ring_element);
    uint8_t ciphertext[1568U] = { 0U };
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 uu____5[4U];
    memcpy(
        uu____5, u,
        (size_t)4U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    compress_then_serialize_u_281(
        uu____5, Eurydice_array_to_subslice2(ciphertext, (size_t)0U,
                                             (size_t)1408U, uint8_t));
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 uu____6 = v;
    compress_then_serialize_ring_element_v_650(
        uu____6, Eurydice_array_to_subslice_from((size_t)1568U, ciphertext,
                                                 (size_t)1408U, uint8_t, size_t));
    memcpy(ret, ciphertext, (size_t)1568U * sizeof(uint8_t));
}

/**
This function found in impl {(libcrux_ml_kem::variant::Variant for
libcrux_ml_kem::variant::MlKem)}
*/
/**
A monomorphic instance of libcrux_ml_kem.variant.kdf_d8
with types libcrux_ml_kem_hash_functions_portable_PortableHash[[$4size_t]]
with const generics
- K= 4
- CIPHERTEXT_SIZE= 1568
*/
static KRML_MUSTINLINE void
kdf_d8_93(Eurydice_slice shared_secret,
          uint8_t ret[32U])
{
    uint8_t out[32U] = { 0U };
    Eurydice_slice_copy(Eurydice_array_to_slice((size_t)32U, out, uint8_t),
                        shared_secret, uint8_t);
    memcpy(ret, out, (size_t)32U * sizeof(uint8_t));
}

/**
A monomorphic instance of libcrux_ml_kem.ind_cca.encapsulate
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$4size_t]],
libcrux_ml_kem_variant_MlKem with const generics
- K= 4
- CIPHERTEXT_SIZE= 1568
- PUBLIC_KEY_SIZE= 1568
- T_AS_NTT_ENCODED_SIZE= 1536
- C1_SIZE= 1408
- C2_SIZE= 160
- VECTOR_U_COMPRESSION_FACTOR= 11
- VECTOR_V_COMPRESSION_FACTOR= 5
- VECTOR_U_BLOCK_LEN= 352
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
- ETA2= 2
- ETA2_RANDOMNESS_SIZE= 128
*/
tuple_21
libcrux_ml_kem_ind_cca_encapsulate_eb1(
    libcrux_ml_kem_types_MlKemPublicKey_1f *public_key,
    uint8_t randomness[32U])
{
    uint8_t randomness0[32U];
    entropy_preprocess_d8_d1(
        Eurydice_array_to_slice((size_t)32U, randomness, uint8_t), randomness0);
    uint8_t to_hash[64U];
    libcrux_ml_kem_utils_into_padded_array_6d(
        Eurydice_array_to_slice((size_t)32U, randomness0, uint8_t), to_hash);
    Eurydice_slice uu____0 = Eurydice_array_to_subslice_from(
        (size_t)64U, to_hash, LIBCRUX_ML_KEM_CONSTANTS_H_DIGEST_SIZE, uint8_t,
        size_t);
    uint8_t ret[32U];
    H_f1_191(Eurydice_array_to_slice(
                 (size_t)1568U, libcrux_ml_kem_types_as_slice_cb_3d1(public_key),
                 uint8_t),
             ret);
    Eurydice_slice_copy(
        uu____0, Eurydice_array_to_slice((size_t)32U, ret, uint8_t), uint8_t);
    uint8_t hashed[64U];
    G_f1_381(Eurydice_array_to_slice((size_t)64U, to_hash, uint8_t), hashed);
    Eurydice_slice_uint8_t_x2 uu____1 = Eurydice_slice_split_at(
        Eurydice_array_to_slice((size_t)64U, hashed, uint8_t),
        LIBCRUX_ML_KEM_CONSTANTS_SHARED_SECRET_SIZE, uint8_t,
        Eurydice_slice_uint8_t_x2);
    Eurydice_slice shared_secret = uu____1.fst;
    Eurydice_slice pseudorandomness = uu____1.snd;
    Eurydice_slice uu____2 = Eurydice_array_to_slice(
        (size_t)1568U, libcrux_ml_kem_types_as_slice_cb_3d1(public_key), uint8_t);
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_randomness[32U];
    memcpy(copy_of_randomness, randomness0, (size_t)32U * sizeof(uint8_t));
    uint8_t ciphertext[1568U];
    encrypt_7b1(uu____2, copy_of_randomness, pseudorandomness, ciphertext);
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_ciphertext[1568U];
    memcpy(copy_of_ciphertext, ciphertext, (size_t)1568U * sizeof(uint8_t));
    libcrux_ml_kem_mlkem1024_MlKem1024Ciphertext ciphertext0 =
        libcrux_ml_kem_types_from_01_331(copy_of_ciphertext);
    uint8_t shared_secret_array[32U];
    kdf_d8_93(shared_secret, shared_secret_array);
    libcrux_ml_kem_mlkem1024_MlKem1024Ciphertext uu____5 = ciphertext0;
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_shared_secret_array[32U];
    memcpy(copy_of_shared_secret_array, shared_secret_array,
           (size_t)32U * sizeof(uint8_t));
    tuple_21 lit;
    lit.fst = uu____5;
    memcpy(lit.snd, copy_of_shared_secret_array, (size_t)32U * sizeof(uint8_t));
    return lit;
}

/**
A monomorphic instance of
libcrux_ml_kem.serialize.deserialize_to_uncompressed_ring_element with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics

*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_f0
deserialize_to_uncompressed_ring_element_27(Eurydice_slice serialized)
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 re = ZERO_89_c3();
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(serialized, uint8_t) / (size_t)24U; i++) {
        size_t i0 = i;
        Eurydice_slice bytes = Eurydice_slice_subslice2(
            serialized, i0 * (size_t)24U, i0 * (size_t)24U + (size_t)24U, uint8_t);
        libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
            libcrux_ml_kem_vector_portable_deserialize_12_0d(bytes);
        re.coefficients[i0] = uu____0;
    }
    return re;
}

/**
 Call [`deserialize_to_uncompressed_ring_element`] for each ring element.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.deserialize_secret_key
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 4
*/
static KRML_MUSTINLINE void
deserialize_secret_key_7b1(
    Eurydice_slice secret_key,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret[4U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 secret_as_ntt[4U];
    KRML_MAYBE_FOR4(i, (size_t)0U, (size_t)4U, (size_t)1U,
                    secret_as_ntt[i] = ZERO_89_c3(););
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(secret_key, uint8_t) /
                 LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT;
         i++) {
        size_t i0 = i;
        Eurydice_slice secret_bytes = Eurydice_slice_subslice2(
            secret_key, i0 * LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
            i0 * LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT +
                LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
            uint8_t);
        libcrux_ml_kem_polynomial_PolynomialRingElement_f0 uu____0 =
            deserialize_to_uncompressed_ring_element_27(secret_bytes);
        secret_as_ntt[i0] = uu____0;
    }
    memcpy(
        ret, secret_as_ntt,
        (size_t)4U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
}

/**
A monomorphic instance of
libcrux_ml_kem.ind_cpa.unpacked.IndCpaPrivateKeyUnpacked with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- $4size_t
*/
typedef struct IndCpaPrivateKeyUnpacked_42_s {
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 secret_as_ntt[4U];
} IndCpaPrivateKeyUnpacked_42;

/**
A monomorphic instance of
libcrux_ml_kem.vector.portable.compress.decompress_ciphertext_coefficient with
const generics
- COEFFICIENT_BITS= 10
*/
static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
decompress_ciphertext_coefficient_84(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v)
{
    for (size_t i = (size_t)0U;
         i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
        size_t i0 = i;
        int32_t decompressed = (int32_t)v.elements[i0] *
                               (int32_t)LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_MODULUS;
        decompressed = (decompressed << 1U) + ((int32_t)1 << (uint32_t)(int32_t)10);
        decompressed = decompressed >> (uint32_t)((int32_t)10 + (int32_t)1);
        v.elements[i0] = (int16_t)decompressed;
    }
    return v;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
/**
A monomorphic instance of
libcrux_ml_kem.vector.portable.decompress_ciphertext_coefficient_0d with const
generics
- COEFFICIENT_BITS= 10
*/
static libcrux_ml_kem_vector_portable_vector_type_PortableVector
decompress_ciphertext_coefficient_0d_b3(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v)
{
    return decompress_ciphertext_coefficient_84(v);
}

/**
A monomorphic instance of
libcrux_ml_kem.serialize.deserialize_then_decompress_10 with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics

*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_f0
deserialize_then_decompress_10_50(Eurydice_slice serialized)
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 re = ZERO_89_c3();
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(serialized, uint8_t) / (size_t)20U; i++) {
        size_t i0 = i;
        Eurydice_slice bytes = Eurydice_slice_subslice2(
            serialized, i0 * (size_t)20U, i0 * (size_t)20U + (size_t)20U, uint8_t);
        libcrux_ml_kem_vector_portable_vector_type_PortableVector coefficient =
            libcrux_ml_kem_vector_portable_deserialize_10_0d(bytes);
        libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
            decompress_ciphertext_coefficient_0d_b3(coefficient);
        re.coefficients[i0] = uu____0;
    }
    return re;
}

/**
A monomorphic instance of
libcrux_ml_kem.vector.portable.compress.decompress_ciphertext_coefficient with
const generics
- COEFFICIENT_BITS= 11
*/
static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
decompress_ciphertext_coefficient_840(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v)
{
    for (size_t i = (size_t)0U;
         i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
        size_t i0 = i;
        int32_t decompressed = (int32_t)v.elements[i0] *
                               (int32_t)LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_MODULUS;
        decompressed = (decompressed << 1U) + ((int32_t)1 << (uint32_t)(int32_t)11);
        decompressed = decompressed >> (uint32_t)((int32_t)11 + (int32_t)1);
        v.elements[i0] = (int16_t)decompressed;
    }
    return v;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
/**
A monomorphic instance of
libcrux_ml_kem.vector.portable.decompress_ciphertext_coefficient_0d with const
generics
- COEFFICIENT_BITS= 11
*/
static libcrux_ml_kem_vector_portable_vector_type_PortableVector
decompress_ciphertext_coefficient_0d_b30(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v)
{
    return decompress_ciphertext_coefficient_840(v);
}

/**
A monomorphic instance of
libcrux_ml_kem.serialize.deserialize_then_decompress_11 with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics

*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_f0
deserialize_then_decompress_11_1f(Eurydice_slice serialized)
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 re = ZERO_89_c3();
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(serialized, uint8_t) / (size_t)22U; i++) {
        size_t i0 = i;
        Eurydice_slice bytes = Eurydice_slice_subslice2(
            serialized, i0 * (size_t)22U, i0 * (size_t)22U + (size_t)22U, uint8_t);
        libcrux_ml_kem_vector_portable_vector_type_PortableVector coefficient =
            libcrux_ml_kem_vector_portable_deserialize_11_0d(bytes);
        libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
            decompress_ciphertext_coefficient_0d_b30(coefficient);
        re.coefficients[i0] = uu____0;
    }
    return re;
}

/**
A monomorphic instance of
libcrux_ml_kem.serialize.deserialize_then_decompress_ring_element_u with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- COMPRESSION_FACTOR= 11
*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_f0
deserialize_then_decompress_ring_element_u_c20(Eurydice_slice serialized)
{
    return deserialize_then_decompress_11_1f(serialized);
}

/**
A monomorphic instance of libcrux_ml_kem.ntt.ntt_vector_u
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- VECTOR_U_COMPRESSION_FACTOR= 11
*/
static KRML_MUSTINLINE void
ntt_vector_u_6f0(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *re)
{
    size_t zeta_i = (size_t)0U;
    ntt_at_layer_4_plus_8c(&zeta_i, re, (size_t)7U);
    ntt_at_layer_4_plus_8c(&zeta_i, re, (size_t)6U);
    ntt_at_layer_4_plus_8c(&zeta_i, re, (size_t)5U);
    ntt_at_layer_4_plus_8c(&zeta_i, re, (size_t)4U);
    ntt_at_layer_3_34(&zeta_i, re);
    ntt_at_layer_2_26(&zeta_i, re);
    ntt_at_layer_1_3c(&zeta_i, re);
    poly_barrett_reduce_89_d8(re);
}

/**
 Call [`deserialize_then_decompress_ring_element_u`] on each ring element
 in the `ciphertext`.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.deserialize_then_decompress_u
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 4
- CIPHERTEXT_SIZE= 1568
- U_COMPRESSION_FACTOR= 11
*/
static KRML_MUSTINLINE void
deserialize_then_decompress_u_1a1(
    uint8_t *ciphertext,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret[4U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 u_as_ntt[4U];
    KRML_MAYBE_FOR4(i, (size_t)0U, (size_t)4U, (size_t)1U,
                    u_as_ntt[i] = ZERO_89_c3(););
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(
                 Eurydice_array_to_slice((size_t)1568U, ciphertext, uint8_t),
                 uint8_t) /
                 (LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT *
                  (size_t)11U / (size_t)8U);
         i++) {
        size_t i0 = i;
        Eurydice_slice u_bytes = Eurydice_array_to_subslice2(
            ciphertext,
            i0 * (LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT *
                  (size_t)11U / (size_t)8U),
            i0 * (LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT *
                  (size_t)11U / (size_t)8U) +
                LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT *
                    (size_t)11U / (size_t)8U,
            uint8_t);
        u_as_ntt[i0] = deserialize_then_decompress_ring_element_u_c20(u_bytes);
        ntt_vector_u_6f0(&u_as_ntt[i0]);
    }
    memcpy(
        ret, u_as_ntt,
        (size_t)4U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
}

/**
A monomorphic instance of
libcrux_ml_kem.vector.portable.compress.decompress_ciphertext_coefficient with
const generics
- COEFFICIENT_BITS= 4
*/
static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
decompress_ciphertext_coefficient_841(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v)
{
    for (size_t i = (size_t)0U;
         i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
        size_t i0 = i;
        int32_t decompressed = (int32_t)v.elements[i0] *
                               (int32_t)LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_MODULUS;
        decompressed = (decompressed << 1U) + ((int32_t)1 << (uint32_t)(int32_t)4);
        decompressed = decompressed >> (uint32_t)((int32_t)4 + (int32_t)1);
        v.elements[i0] = (int16_t)decompressed;
    }
    return v;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
/**
A monomorphic instance of
libcrux_ml_kem.vector.portable.decompress_ciphertext_coefficient_0d with const
generics
- COEFFICIENT_BITS= 4
*/
static libcrux_ml_kem_vector_portable_vector_type_PortableVector
decompress_ciphertext_coefficient_0d_b31(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v)
{
    return decompress_ciphertext_coefficient_841(v);
}

/**
A monomorphic instance of libcrux_ml_kem.serialize.deserialize_then_decompress_4
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_f0
deserialize_then_decompress_4_94(Eurydice_slice serialized)
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 re = ZERO_89_c3();
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(serialized, uint8_t) / (size_t)8U; i++) {
        size_t i0 = i;
        Eurydice_slice bytes = Eurydice_slice_subslice2(
            serialized, i0 * (size_t)8U, i0 * (size_t)8U + (size_t)8U, uint8_t);
        libcrux_ml_kem_vector_portable_vector_type_PortableVector coefficient =
            libcrux_ml_kem_vector_portable_deserialize_4_0d(bytes);
        libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
            decompress_ciphertext_coefficient_0d_b31(coefficient);
        re.coefficients[i0] = uu____0;
    }
    return re;
}

/**
A monomorphic instance of
libcrux_ml_kem.vector.portable.compress.decompress_ciphertext_coefficient with
const generics
- COEFFICIENT_BITS= 5
*/
static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
decompress_ciphertext_coefficient_842(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v)
{
    for (size_t i = (size_t)0U;
         i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
        size_t i0 = i;
        int32_t decompressed = (int32_t)v.elements[i0] *
                               (int32_t)LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_MODULUS;
        decompressed = (decompressed << 1U) + ((int32_t)1 << (uint32_t)(int32_t)5);
        decompressed = decompressed >> (uint32_t)((int32_t)5 + (int32_t)1);
        v.elements[i0] = (int16_t)decompressed;
    }
    return v;
}

/**
This function found in impl {(libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector)}
*/
/**
A monomorphic instance of
libcrux_ml_kem.vector.portable.decompress_ciphertext_coefficient_0d with const
generics
- COEFFICIENT_BITS= 5
*/
static libcrux_ml_kem_vector_portable_vector_type_PortableVector
decompress_ciphertext_coefficient_0d_b32(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v)
{
    return decompress_ciphertext_coefficient_842(v);
}

/**
A monomorphic instance of libcrux_ml_kem.serialize.deserialize_then_decompress_5
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_f0
deserialize_then_decompress_5_100(Eurydice_slice serialized)
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 re = ZERO_89_c3();
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(serialized, uint8_t) / (size_t)10U; i++) {
        size_t i0 = i;
        Eurydice_slice bytes = Eurydice_slice_subslice2(
            serialized, i0 * (size_t)10U, i0 * (size_t)10U + (size_t)10U, uint8_t);
        re.coefficients[i0] =
            libcrux_ml_kem_vector_portable_deserialize_5_0d(bytes);
        libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____1 =
            decompress_ciphertext_coefficient_0d_b32(re.coefficients[i0]);
        re.coefficients[i0] = uu____1;
    }
    return re;
}

/**
A monomorphic instance of
libcrux_ml_kem.serialize.deserialize_then_decompress_ring_element_v with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- COMPRESSION_FACTOR= 5
*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_f0
deserialize_then_decompress_ring_element_v_930(Eurydice_slice serialized)
{
    return deserialize_then_decompress_5_100(serialized);
}

/**
This function found in impl
{libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0]}
*/
/**
A monomorphic instance of libcrux_ml_kem.polynomial.subtract_reduce_89
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_f0
subtract_reduce_89_61(libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *self,
                      libcrux_ml_kem_polynomial_PolynomialRingElement_f0 b)
{
    for (size_t i = (size_t)0U;
         i < LIBCRUX_ML_KEM_POLYNOMIAL_VECTORS_IN_RING_ELEMENT; i++) {
        size_t i0 = i;
        libcrux_ml_kem_vector_portable_vector_type_PortableVector
            coefficient_normal_form =
                libcrux_ml_kem_vector_portable_montgomery_multiply_by_constant_0d(
                    b.coefficients[i0], (int16_t)1441);
        libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
            libcrux_ml_kem_vector_portable_barrett_reduce_0d(
                libcrux_ml_kem_vector_portable_sub_0d(self->coefficients[i0],
                                                      &coefficient_normal_form));
        b.coefficients[i0] = uu____0;
    }
    return b;
}

/**
 The following functions compute various expressions involving
 vectors and matrices. The computation of these expressions has been
 abstracted away into these functions in order to save on loop iterations.
 Compute v − InverseNTT(sᵀ ◦ NTT(u))
*/
/**
A monomorphic instance of libcrux_ml_kem.matrix.compute_message
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 4
*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_f0
compute_message_c91(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *v,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *secret_as_ntt,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *u_as_ntt)
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 result = ZERO_89_c3();
    KRML_MAYBE_FOR4(i, (size_t)0U, (size_t)4U, (size_t)1U, size_t i0 = i;
                    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 product =
                        ntt_multiply_89_3b(&secret_as_ntt[i0], &u_as_ntt[i0]);
                    add_to_ring_element_89_1e1(&result, &product););
    invert_ntt_montgomery_c91(&result);
    result = subtract_reduce_89_61(v, result);
    return result;
}

/**
A monomorphic instance of
libcrux_ml_kem.serialize.compress_then_serialize_message with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics

*/
static KRML_MUSTINLINE void
compress_then_serialize_message_d4(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 re, uint8_t ret[32U])
{
    uint8_t serialized[32U] = { 0U };
    KRML_MAYBE_FOR16(
        i, (size_t)0U, (size_t)16U, (size_t)1U, size_t i0 = i;
        libcrux_ml_kem_vector_portable_vector_type_PortableVector coefficient =
            to_unsigned_representative_84(re.coefficients[i0]);
        libcrux_ml_kem_vector_portable_vector_type_PortableVector
            coefficient_compressed =
                libcrux_ml_kem_vector_portable_compress_1_0d(coefficient);
        uint8_t bytes[2U]; libcrux_ml_kem_vector_portable_serialize_1_0d(
            coefficient_compressed, bytes);
        Eurydice_slice uu____0 = Eurydice_array_to_subslice2(
            serialized, (size_t)2U * i0, (size_t)2U * i0 + (size_t)2U, uint8_t);
        Eurydice_slice_copy(uu____0,
                            Eurydice_array_to_slice((size_t)2U, bytes, uint8_t),
                            uint8_t););
    memcpy(ret, serialized, (size_t)32U * sizeof(uint8_t));
}

/**
 This function implements <strong>Algorithm 14</strong> of the
 NIST FIPS 203 specification; this is the Kyber CPA-PKE decryption algorithm.

 Algorithm 14 is reproduced below:

 ```plaintext
 Input: decryption key dkₚₖₑ ∈ 𝔹^{384k}.
 Input: ciphertext c ∈ 𝔹^{32(dᵤk + dᵥ)}.
 Output: message m ∈ 𝔹^{32}.

 c₁ ← c[0 : 32dᵤk]
 c₂ ← c[32dᵤk : 32(dᵤk + dᵥ)]
 u ← Decompress_{dᵤ}(ByteDecode_{dᵤ}(c₁))
 v ← Decompress_{dᵥ}(ByteDecode_{dᵥ}(c₂))
 ŝ ← ByteDecode₁₂(dkₚₖₑ)
 w ← v - NTT-¹(ŝᵀ ◦ NTT(u))
 m ← ByteEncode₁(Compress₁(w))
 return m
 ```

 The NIST FIPS 203 standard can be found at
 <https://csrc.nist.gov/pubs/fips/203/ipd>.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.decrypt_unpacked
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 4
- CIPHERTEXT_SIZE= 1568
- VECTOR_U_ENCODED_SIZE= 1408
- U_COMPRESSION_FACTOR= 11
- V_COMPRESSION_FACTOR= 5
*/
static void
decrypt_unpacked_6b1(IndCpaPrivateKeyUnpacked_42 *secret_key,
                     uint8_t *ciphertext, uint8_t ret[32U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 u_as_ntt[4U];
    deserialize_then_decompress_u_1a1(ciphertext, u_as_ntt);
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 v =
        deserialize_then_decompress_ring_element_v_930(
            Eurydice_array_to_subslice_from((size_t)1568U, ciphertext,
                                            (size_t)1408U, uint8_t, size_t));
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 message =
        compute_message_c91(&v, secret_key->secret_as_ntt, u_as_ntt);
    uint8_t ret0[32U];
    compress_then_serialize_message_d4(message, ret0);
    memcpy(ret, ret0, (size_t)32U * sizeof(uint8_t));
}

/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.decrypt
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 4
- CIPHERTEXT_SIZE= 1568
- VECTOR_U_ENCODED_SIZE= 1408
- U_COMPRESSION_FACTOR= 11
- V_COMPRESSION_FACTOR= 5
*/
static void
decrypt_121(Eurydice_slice secret_key, uint8_t *ciphertext,
            uint8_t ret[32U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 secret_as_ntt[4U];
    deserialize_secret_key_7b1(secret_key, secret_as_ntt);
    /* Passing arrays by value in Rust generates a copy in C */
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 copy_of_secret_as_ntt[4U];
    memcpy(
        copy_of_secret_as_ntt, secret_as_ntt,
        (size_t)4U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    IndCpaPrivateKeyUnpacked_42 secret_key_unpacked;
    memcpy(
        secret_key_unpacked.secret_as_ntt, copy_of_secret_as_ntt,
        (size_t)4U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    uint8_t ret0[32U];
    decrypt_unpacked_6b1(&secret_key_unpacked, ciphertext, ret0);
    memcpy(ret, ret0, (size_t)32U * sizeof(uint8_t));
}

/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.PRF
with const generics
- LEN= 32
*/
static KRML_MUSTINLINE void
PRF_7c(Eurydice_slice input, uint8_t ret[32U])
{
    uint8_t digest[32U] = { 0U };
    libcrux_sha3_portable_shake256(
        Eurydice_array_to_slice((size_t)32U, digest, uint8_t), input);
    memcpy(ret, digest, (size_t)32U * sizeof(uint8_t));
}

/**
This function found in impl {(libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>)}
*/
/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.PRF_f1
with const generics
- K= 4
- LEN= 32
*/
static KRML_MUSTINLINE void
PRF_f1_2e3(Eurydice_slice input, uint8_t ret[32U])
{
    PRF_7c(input, ret);
}

/**
A monomorphic instance of libcrux_ml_kem.ind_cca.decapsulate
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$4size_t]],
libcrux_ml_kem_variant_MlKem with const generics
- K= 4
- SECRET_KEY_SIZE= 3168
- CPA_SECRET_KEY_SIZE= 1536
- PUBLIC_KEY_SIZE= 1568
- CIPHERTEXT_SIZE= 1568
- T_AS_NTT_ENCODED_SIZE= 1536
- C1_SIZE= 1408
- C2_SIZE= 160
- VECTOR_U_COMPRESSION_FACTOR= 11
- VECTOR_V_COMPRESSION_FACTOR= 5
- C1_BLOCK_SIZE= 352
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
- ETA2= 2
- ETA2_RANDOMNESS_SIZE= 128
- IMPLICIT_REJECTION_HASH_INPUT_SIZE= 1600
*/
void
libcrux_ml_kem_ind_cca_decapsulate_1f1(
    libcrux_ml_kem_types_MlKemPrivateKey_95 *private_key,
    libcrux_ml_kem_mlkem1024_MlKem1024Ciphertext *ciphertext,
    uint8_t ret[32U])
{
    Eurydice_slice_uint8_t_x2 uu____0 = Eurydice_slice_split_at(
        Eurydice_array_to_slice((size_t)3168U, private_key->value, uint8_t),
        (size_t)1536U, uint8_t, Eurydice_slice_uint8_t_x2);
    Eurydice_slice ind_cpa_secret_key = uu____0.fst;
    Eurydice_slice secret_key0 = uu____0.snd;
    Eurydice_slice_uint8_t_x2 uu____1 = Eurydice_slice_split_at(
        secret_key0, (size_t)1568U, uint8_t, Eurydice_slice_uint8_t_x2);
    Eurydice_slice ind_cpa_public_key = uu____1.fst;
    Eurydice_slice secret_key = uu____1.snd;
    Eurydice_slice_uint8_t_x2 uu____2 = Eurydice_slice_split_at(
        secret_key, LIBCRUX_ML_KEM_CONSTANTS_H_DIGEST_SIZE, uint8_t,
        Eurydice_slice_uint8_t_x2);
    Eurydice_slice ind_cpa_public_key_hash = uu____2.fst;
    Eurydice_slice implicit_rejection_value = uu____2.snd;
    uint8_t decrypted[32U];
    decrypt_121(ind_cpa_secret_key, ciphertext->value, decrypted);
    uint8_t to_hash0[64U];
    libcrux_ml_kem_utils_into_padded_array_6d(
        Eurydice_array_to_slice((size_t)32U, decrypted, uint8_t), to_hash0);
    Eurydice_slice_copy(
        Eurydice_array_to_subslice_from(
            (size_t)64U, to_hash0, LIBCRUX_ML_KEM_CONSTANTS_SHARED_SECRET_SIZE,
            uint8_t, size_t),
        ind_cpa_public_key_hash, uint8_t);
    uint8_t hashed[64U];
    G_f1_381(Eurydice_array_to_slice((size_t)64U, to_hash0, uint8_t), hashed);
    Eurydice_slice_uint8_t_x2 uu____3 = Eurydice_slice_split_at(
        Eurydice_array_to_slice((size_t)64U, hashed, uint8_t),
        LIBCRUX_ML_KEM_CONSTANTS_SHARED_SECRET_SIZE, uint8_t,
        Eurydice_slice_uint8_t_x2);
    Eurydice_slice shared_secret0 = uu____3.fst;
    Eurydice_slice pseudorandomness = uu____3.snd;
    uint8_t to_hash[1600U];
    libcrux_ml_kem_utils_into_padded_array_6d4(implicit_rejection_value, to_hash);
    Eurydice_slice uu____4 = Eurydice_array_to_subslice_from(
        (size_t)1600U, to_hash, LIBCRUX_ML_KEM_CONSTANTS_SHARED_SECRET_SIZE,
        uint8_t, size_t);
    Eurydice_slice_copy(uu____4, libcrux_ml_kem_types_as_ref_00_d81(ciphertext),
                        uint8_t);
    uint8_t implicit_rejection_shared_secret0[32U];
    PRF_f1_2e3(Eurydice_array_to_slice((size_t)1600U, to_hash, uint8_t),
               implicit_rejection_shared_secret0);
    Eurydice_slice uu____5 = ind_cpa_public_key;
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_decrypted[32U];
    memcpy(copy_of_decrypted, decrypted, (size_t)32U * sizeof(uint8_t));
    uint8_t expected_ciphertext[1568U];
    encrypt_7b1(uu____5, copy_of_decrypted, pseudorandomness,
                expected_ciphertext);
    uint8_t implicit_rejection_shared_secret[32U];
    kdf_d8_93(Eurydice_array_to_slice((size_t)32U,
                                      implicit_rejection_shared_secret0, uint8_t),
              implicit_rejection_shared_secret);
    uint8_t shared_secret[32U];
    kdf_d8_93(shared_secret0, shared_secret);
    uint8_t ret0[32U];
    libcrux_ml_kem_constant_time_ops_compare_ciphertexts_select_shared_secret_in_constant_time(
        libcrux_ml_kem_types_as_ref_00_d81(ciphertext),
        Eurydice_array_to_slice((size_t)1568U, expected_ciphertext, uint8_t),
        Eurydice_array_to_slice((size_t)32U, shared_secret, uint8_t),
        Eurydice_array_to_slice((size_t)32U, implicit_rejection_shared_secret,
                                uint8_t),
        ret0);
    memcpy(ret, ret0, (size_t)32U * sizeof(uint8_t));
}

/**
 This function deserializes ring elements and reduces the result by the field
 modulus.

 This function MUST NOT be used on secret inputs.
*/
/**
A monomorphic instance of
libcrux_ml_kem.serialize.deserialize_ring_elements_reduced with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- PUBLIC_KEY_SIZE= 800
- K= 2
*/
static KRML_MUSTINLINE void
deserialize_ring_elements_reduced_4f2(
    Eurydice_slice public_key,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret[2U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 deserialized_pk[2U];
    KRML_MAYBE_FOR2(i, (size_t)0U, (size_t)2U, (size_t)1U,
                    deserialized_pk[i] = ZERO_89_c3(););
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(public_key, uint8_t) /
                 LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT;
         i++) {
        size_t i0 = i;
        Eurydice_slice ring_element = Eurydice_slice_subslice2(
            public_key, i0 * LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
            i0 * LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT +
                LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
            uint8_t);
        libcrux_ml_kem_polynomial_PolynomialRingElement_f0 uu____0 =
            deserialize_to_reduced_ring_element_45(ring_element);
        deserialized_pk[i0] = uu____0;
    }
    memcpy(
        ret, deserialized_pk,
        (size_t)2U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
}

/**
 Call [`serialize_uncompressed_ring_element`] for each ring element.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.serialize_secret_key
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 2
- OUT_LEN= 768
*/
static KRML_MUSTINLINE void
serialize_secret_key_a30(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *key,
    uint8_t ret[768U])
{
    uint8_t out[768U] = { 0U };
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(
                 Eurydice_array_to_slice(
                     (size_t)2U, key,
                     libcrux_ml_kem_polynomial_PolynomialRingElement_f0),
                 libcrux_ml_kem_polynomial_PolynomialRingElement_f0);
         i++) {
        size_t i0 = i;
        libcrux_ml_kem_polynomial_PolynomialRingElement_f0 re = key[i0];
        Eurydice_slice uu____0 = Eurydice_array_to_subslice2(
            out, i0 * LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
            (i0 + (size_t)1U) * LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
            uint8_t);
        uint8_t ret0[384U];
        serialize_uncompressed_ring_element_3c(&re, ret0);
        Eurydice_slice_copy(
            uu____0, Eurydice_array_to_slice((size_t)384U, ret0, uint8_t), uint8_t);
    }
    memcpy(ret, out, (size_t)768U * sizeof(uint8_t));
}

/**
 Concatenate `t` and `ρ` into the public key.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.serialize_public_key
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 2
- RANKED_BYTES_PER_RING_ELEMENT= 768
- PUBLIC_KEY_SIZE= 800
*/
static KRML_MUSTINLINE void
serialize_public_key_670(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *t_as_ntt,
    Eurydice_slice seed_for_a, uint8_t ret[800U])
{
    uint8_t public_key_serialized[800U] = { 0U };
    Eurydice_slice uu____0 = Eurydice_array_to_subslice2(
        public_key_serialized, (size_t)0U, (size_t)768U, uint8_t);
    uint8_t ret0[768U];
    serialize_secret_key_a30(t_as_ntt, ret0);
    Eurydice_slice_copy(
        uu____0, Eurydice_array_to_slice((size_t)768U, ret0, uint8_t), uint8_t);
    Eurydice_slice_copy(
        Eurydice_array_to_subslice_from((size_t)800U, public_key_serialized,
                                        (size_t)768U, uint8_t, size_t),
        seed_for_a, uint8_t);
    memcpy(ret, public_key_serialized, (size_t)800U * sizeof(uint8_t));
}

/**
 Validate an ML-KEM public key.

 This implements the Modulus check in 7.2 2.
 Note that the size check in 7.2 1 is covered by the `PUBLIC_KEY_SIZE` in the
 `public_key` type.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cca.validate_public_key
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 2
- RANKED_BYTES_PER_RING_ELEMENT= 768
- PUBLIC_KEY_SIZE= 800
*/
bool
libcrux_ml_kem_ind_cca_validate_public_key_b70(uint8_t *public_key)
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 deserialized_pk[2U];
    deserialize_ring_elements_reduced_4f2(
        Eurydice_array_to_subslice_to((size_t)800U, public_key, (size_t)768U,
                                      uint8_t, size_t),
        deserialized_pk);
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *uu____0 = deserialized_pk;
    uint8_t public_key_serialized[800U];
    serialize_public_key_670(
        uu____0,
        Eurydice_array_to_subslice_from((size_t)800U, public_key, (size_t)768U,
                                        uint8_t, size_t),
        public_key_serialized);
    return core_array_equality___core__cmp__PartialEq__Array_U__N___for__Array_T__N____eq(
        (size_t)800U, public_key, public_key_serialized, uint8_t, uint8_t, bool);
}

/**
This function found in impl {(libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>)}
*/
/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.H_f1
with const generics
- K= 2
*/
static KRML_MUSTINLINE void
H_f1_190(Eurydice_slice input, uint8_t ret[32U])
{
    libcrux_ml_kem_hash_functions_portable_H(input, ret);
}

/**
 Validate an ML-KEM private key.

 This implements the Hash check in 7.3 3.
 Note that the size checks in 7.2 1 and 2 are covered by the `SECRET_KEY_SIZE`
 and `CIPHERTEXT_SIZE` in the `private_key` and `ciphertext` types.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cca.validate_private_key
with types libcrux_ml_kem_hash_functions_portable_PortableHash[[$2size_t]]
with const generics
- K= 2
- SECRET_KEY_SIZE= 1632
- CIPHERTEXT_SIZE= 768
*/
bool
libcrux_ml_kem_ind_cca_validate_private_key_03(
    libcrux_ml_kem_types_MlKemPrivateKey_5e *private_key,
    libcrux_ml_kem_types_MlKemCiphertext_e8 *_ciphertext)
{
    uint8_t t[32U];
    H_f1_190(Eurydice_array_to_subslice2(
                 private_key->value, (size_t)384U * (size_t)2U,
                 (size_t)768U * (size_t)2U + (size_t)32U, uint8_t),
             t);
    Eurydice_slice expected = Eurydice_array_to_subslice2(
        private_key->value, (size_t)768U * (size_t)2U + (size_t)32U,
        (size_t)768U * (size_t)2U + (size_t)64U, uint8_t);
    return core_array_equality___core__cmp__PartialEq__0___Slice_U____for__Array_T__N___3__eq(
        (size_t)32U, t, &expected, uint8_t, uint8_t, bool);
}

/**
This function found in impl {(libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>)}
*/
/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.G_f1
with const generics
- K= 2
*/
static KRML_MUSTINLINE void
G_f1_380(Eurydice_slice input, uint8_t ret[64U])
{
    libcrux_ml_kem_hash_functions_portable_G(input, ret);
}

/**
This function found in impl {(libcrux_ml_kem::variant::Variant for
libcrux_ml_kem::variant::MlKem)}
*/
/**
A monomorphic instance of libcrux_ml_kem.variant.cpa_keygen_seed_d8
with types libcrux_ml_kem_hash_functions_portable_PortableHash[[$2size_t]]
with const generics
- K= 2
*/
static KRML_MUSTINLINE void
cpa_keygen_seed_d8_4d(
    Eurydice_slice key_generation_seed, uint8_t ret[64U])
{
    uint8_t seed[33U] = { 0U };
    Eurydice_slice_copy(
        Eurydice_array_to_subslice2(
            seed, (size_t)0U,
            LIBCRUX_ML_KEM_CONSTANTS_CPA_PKE_KEY_GENERATION_SEED_SIZE, uint8_t),
        key_generation_seed, uint8_t);
    seed[LIBCRUX_ML_KEM_CONSTANTS_CPA_PKE_KEY_GENERATION_SEED_SIZE] =
        (uint8_t)(size_t)2U;
    uint8_t ret0[64U];
    G_f1_380(Eurydice_array_to_slice((size_t)33U, seed, uint8_t), ret0);
    memcpy(ret, ret0, (size_t)64U * sizeof(uint8_t));
}

/**
A monomorphic instance of libcrux_ml_kem.matrix.sample_matrix_A.closure
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$2size_t]] with const
generics
- K= 2
*/
static void
closure_fc0(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret[2U])
{
    KRML_MAYBE_FOR2(i, (size_t)0U, (size_t)2U, (size_t)1U,
                    ret[i] = ZERO_89_c3(););
}

/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.PortableHash
with const generics
- $2size_t
*/
typedef struct PortableHash_8b_s {
    libcrux_sha3_generic_keccak_KeccakState_48 shake128_state[2U];
} PortableHash_8b;

/**
A monomorphic instance of
libcrux_ml_kem.hash_functions.portable.shake128_init_absorb with const generics
- K= 2
*/
static KRML_MUSTINLINE PortableHash_8b
shake128_init_absorb_790(uint8_t input[2U][34U])
{
    libcrux_sha3_generic_keccak_KeccakState_48 shake128_state[2U];
    KRML_MAYBE_FOR2(
        i, (size_t)0U, (size_t)2U, (size_t)1U,
        shake128_state[i] = libcrux_sha3_portable_incremental_shake128_init(););
    KRML_MAYBE_FOR2(
        i, (size_t)0U, (size_t)2U, (size_t)1U, size_t i0 = i;
        libcrux_sha3_portable_incremental_shake128_absorb_final(
            &shake128_state[i0],
            Eurydice_array_to_slice((size_t)34U, input[i0], uint8_t)););
    /* Passing arrays by value in Rust generates a copy in C */
    libcrux_sha3_generic_keccak_KeccakState_48 copy_of_shake128_state[2U];
    memcpy(copy_of_shake128_state, shake128_state,
           (size_t)2U * sizeof(libcrux_sha3_generic_keccak_KeccakState_48));
    PortableHash_8b lit;
    memcpy(lit.shake128_state, copy_of_shake128_state,
           (size_t)2U * sizeof(libcrux_sha3_generic_keccak_KeccakState_48));
    return lit;
}

/**
This function found in impl {(libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>)}
*/
/**
A monomorphic instance of
libcrux_ml_kem.hash_functions.portable.shake128_init_absorb_f1 with const
generics
- K= 2
*/
static KRML_MUSTINLINE PortableHash_8b
shake128_init_absorb_f1_770(uint8_t input[2U][34U])
{
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_input[2U][34U];
    memcpy(copy_of_input, input, (size_t)2U * sizeof(uint8_t[34U]));
    return shake128_init_absorb_790(copy_of_input);
}

/**
A monomorphic instance of
libcrux_ml_kem.hash_functions.portable.shake128_squeeze_three_blocks with const
generics
- K= 2
*/
static KRML_MUSTINLINE void
shake128_squeeze_three_blocks_eb0(
    PortableHash_8b *st, uint8_t ret[2U][504U])
{
    uint8_t out[2U][504U] = { { 0U } };
    KRML_MAYBE_FOR2(
        i, (size_t)0U, (size_t)2U, (size_t)1U, size_t i0 = i;
        libcrux_sha3_portable_incremental_shake128_squeeze_first_three_blocks(
            &st->shake128_state[i0],
            Eurydice_array_to_slice((size_t)504U, out[i0], uint8_t)););
    memcpy(ret, out, (size_t)2U * sizeof(uint8_t[504U]));
}

/**
This function found in impl {(libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>)}
*/
/**
A monomorphic instance of
libcrux_ml_kem.hash_functions.portable.shake128_squeeze_three_blocks_f1 with
const generics
- K= 2
*/
static KRML_MUSTINLINE void
shake128_squeeze_three_blocks_f1_840(
    PortableHash_8b *self, uint8_t ret[2U][504U])
{
    shake128_squeeze_three_blocks_eb0(self, ret);
}

/**
 If `bytes` contains a set of uniformly random bytes, this function
 uniformly samples a ring element `â` that is treated as being the NTT
 representation of the corresponding polynomial `a`.

 Since rejection sampling is used, it is possible the supplied bytes are
 not enough to sample the element, in which case an `Err` is returned and the
 caller must try again with a fresh set of bytes.

 This function <strong>partially</strong> implements <strong>Algorithm
 6</strong> of the NIST FIPS 203 standard, We say "partially" because this
 implementation only accepts a finite set of bytes as input and returns an error
 if the set is not enough; Algorithm 6 of the FIPS 203 standard on the other
 hand samples from an infinite stream of bytes until the ring element is filled.
 Algorithm 6 is reproduced below:

 ```plaintext
 Input: byte stream B ∈ 𝔹*.
 Output: array â ∈ ℤ₂₅₆.

 i ← 0
 j ← 0
 while j < 256 do
     d₁ ← B[i] + 256·(B[i+1] mod 16)
     d₂ ← ⌊B[i+1]/16⌋ + 16·B[i+2]
     if d₁ < q then
         â[j] ← d₁
         j ← j + 1
     end if
     if d₂ < q and j < 256 then
         â[j] ← d₂
         j ← j + 1
     end if
     i ← i + 3
 end while
 return â
 ```

 The NIST FIPS 203 standard can be found at
 <https://csrc.nist.gov/pubs/fips/203/ipd>.
*/
/**
A monomorphic instance of
libcrux_ml_kem.sampling.sample_from_uniform_distribution_next with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- K= 2
- N= 504
*/
static KRML_MUSTINLINE bool
sample_from_uniform_distribution_next_171(
    uint8_t randomness[2U][504U], size_t *sampled_coefficients,
    int16_t (*out)[272U])
{
    KRML_MAYBE_FOR2(
        i0, (size_t)0U, (size_t)2U, (size_t)1U, size_t i1 = i0;
        for (size_t i = (size_t)0U; i < (size_t)504U / (size_t)24U; i++) {
            size_t r = i;
            if (sampled_coefficients[i1] <
                LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT) {
                Eurydice_slice uu____0 = Eurydice_array_to_subslice2(
                    randomness[i1], r * (size_t)24U, r * (size_t)24U + (size_t)24U,
                    uint8_t);
                size_t sampled = libcrux_ml_kem_vector_portable_rej_sample_0d(
                    uu____0, Eurydice_array_to_subslice2(
                                 out[i1], sampled_coefficients[i1],
                                 sampled_coefficients[i1] + (size_t)16U, int16_t));
                size_t uu____1 = i1;
                sampled_coefficients[uu____1] =
                    sampled_coefficients[uu____1] + sampled;
            }
        });
    bool done = true;
    KRML_MAYBE_FOR2(
        i, (size_t)0U, (size_t)2U, (size_t)1U, size_t i0 = i;
        if (sampled_coefficients[i0] >=
            LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT) {
            sampled_coefficients[i0] =
                LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT;
        } else { done = false; });
    return done;
}

/**
A monomorphic instance of
libcrux_ml_kem.hash_functions.portable.shake128_squeeze_block with const
generics
- K= 2
*/
static KRML_MUSTINLINE void
shake128_squeeze_block_3b0(PortableHash_8b *st,
                           uint8_t ret[2U][168U])
{
    uint8_t out[2U][168U] = { { 0U } };
    KRML_MAYBE_FOR2(
        i, (size_t)0U, (size_t)2U, (size_t)1U, size_t i0 = i;
        libcrux_sha3_portable_incremental_shake128_squeeze_next_block(
            &st->shake128_state[i0],
            Eurydice_array_to_slice((size_t)168U, out[i0], uint8_t)););
    memcpy(ret, out, (size_t)2U * sizeof(uint8_t[168U]));
}

/**
This function found in impl {(libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>)}
*/
/**
A monomorphic instance of
libcrux_ml_kem.hash_functions.portable.shake128_squeeze_block_f1 with const
generics
- K= 2
*/
static KRML_MUSTINLINE void
shake128_squeeze_block_f1_8e0(
    PortableHash_8b *self, uint8_t ret[2U][168U])
{
    shake128_squeeze_block_3b0(self, ret);
}

/**
 If `bytes` contains a set of uniformly random bytes, this function
 uniformly samples a ring element `â` that is treated as being the NTT
 representation of the corresponding polynomial `a`.

 Since rejection sampling is used, it is possible the supplied bytes are
 not enough to sample the element, in which case an `Err` is returned and the
 caller must try again with a fresh set of bytes.

 This function <strong>partially</strong> implements <strong>Algorithm
 6</strong> of the NIST FIPS 203 standard, We say "partially" because this
 implementation only accepts a finite set of bytes as input and returns an error
 if the set is not enough; Algorithm 6 of the FIPS 203 standard on the other
 hand samples from an infinite stream of bytes until the ring element is filled.
 Algorithm 6 is reproduced below:

 ```plaintext
 Input: byte stream B ∈ 𝔹*.
 Output: array â ∈ ℤ₂₅₆.

 i ← 0
 j ← 0
 while j < 256 do
     d₁ ← B[i] + 256·(B[i+1] mod 16)
     d₂ ← ⌊B[i+1]/16⌋ + 16·B[i+2]
     if d₁ < q then
         â[j] ← d₁
         j ← j + 1
     end if
     if d₂ < q and j < 256 then
         â[j] ← d₂
         j ← j + 1
     end if
     i ← i + 3
 end while
 return â
 ```

 The NIST FIPS 203 standard can be found at
 <https://csrc.nist.gov/pubs/fips/203/ipd>.
*/
/**
A monomorphic instance of
libcrux_ml_kem.sampling.sample_from_uniform_distribution_next with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- K= 2
- N= 168
*/
static KRML_MUSTINLINE bool
sample_from_uniform_distribution_next_172(
    uint8_t randomness[2U][168U], size_t *sampled_coefficients,
    int16_t (*out)[272U])
{
    KRML_MAYBE_FOR2(
        i0, (size_t)0U, (size_t)2U, (size_t)1U, size_t i1 = i0;
        for (size_t i = (size_t)0U; i < (size_t)168U / (size_t)24U; i++) {
            size_t r = i;
            if (sampled_coefficients[i1] <
                LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT) {
                Eurydice_slice uu____0 = Eurydice_array_to_subslice2(
                    randomness[i1], r * (size_t)24U, r * (size_t)24U + (size_t)24U,
                    uint8_t);
                size_t sampled = libcrux_ml_kem_vector_portable_rej_sample_0d(
                    uu____0, Eurydice_array_to_subslice2(
                                 out[i1], sampled_coefficients[i1],
                                 sampled_coefficients[i1] + (size_t)16U, int16_t));
                size_t uu____1 = i1;
                sampled_coefficients[uu____1] =
                    sampled_coefficients[uu____1] + sampled;
            }
        });
    bool done = true;
    KRML_MAYBE_FOR2(
        i, (size_t)0U, (size_t)2U, (size_t)1U, size_t i0 = i;
        if (sampled_coefficients[i0] >=
            LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT) {
            sampled_coefficients[i0] =
                LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT;
        } else { done = false; });
    return done;
}

/**
A monomorphic instance of libcrux_ml_kem.sampling.sample_from_xof.closure
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$2size_t]] with const
generics
- K= 2
*/
static libcrux_ml_kem_polynomial_PolynomialRingElement_f0
closure_e40(
    int16_t s[272U])
{
    return from_i16_array_89_33(
        Eurydice_array_to_subslice2(s, (size_t)0U, (size_t)256U, int16_t));
}

/**
A monomorphic instance of libcrux_ml_kem.sampling.sample_from_xof
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$2size_t]] with const
generics
- K= 2
*/
static KRML_MUSTINLINE void
sample_from_xof_460(
    uint8_t seeds[2U][34U],
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret[2U])
{
    size_t sampled_coefficients[2U] = { 0U };
    int16_t out[2U][272U] = { { 0U } };
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_seeds[2U][34U];
    memcpy(copy_of_seeds, seeds, (size_t)2U * sizeof(uint8_t[34U]));
    PortableHash_8b xof_state = shake128_init_absorb_f1_770(copy_of_seeds);
    uint8_t randomness0[2U][504U];
    shake128_squeeze_three_blocks_f1_840(&xof_state, randomness0);
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_randomness0[2U][504U];
    memcpy(copy_of_randomness0, randomness0, (size_t)2U * sizeof(uint8_t[504U]));
    bool done = sample_from_uniform_distribution_next_171(
        copy_of_randomness0, sampled_coefficients, out);
    while (true) {
        if (done) {
            break;
        } else {
            uint8_t randomness[2U][168U];
            shake128_squeeze_block_f1_8e0(&xof_state, randomness);
            /* Passing arrays by value in Rust generates a copy in C */
            uint8_t copy_of_randomness[2U][168U];
            memcpy(copy_of_randomness, randomness,
                   (size_t)2U * sizeof(uint8_t[168U]));
            done = sample_from_uniform_distribution_next_172(
                copy_of_randomness, sampled_coefficients, out);
        }
    }
    /* Passing arrays by value in Rust generates a copy in C */
    int16_t copy_of_out[2U][272U];
    memcpy(copy_of_out, out, (size_t)2U * sizeof(int16_t[272U]));
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret0[2U];
    KRML_MAYBE_FOR2(i, (size_t)0U, (size_t)2U, (size_t)1U,
                    ret0[i] = closure_e40(copy_of_out[i]););
    memcpy(
        ret, ret0,
        (size_t)2U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
}

/**
A monomorphic instance of libcrux_ml_kem.matrix.sample_matrix_A
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$2size_t]] with const
generics
- K= 2
*/
static KRML_MUSTINLINE void
sample_matrix_A_e70(
    uint8_t seed[34U], bool transpose,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret[2U][2U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 A_transpose[2U][2U];
    KRML_MAYBE_FOR2(i, (size_t)0U, (size_t)2U, (size_t)1U,
                    closure_fc0(A_transpose[i]););
    KRML_MAYBE_FOR2(
        i0, (size_t)0U, (size_t)2U, (size_t)1U, size_t i1 = i0;
        /* Passing arrays by value in Rust generates a copy in C */
        uint8_t copy_of_seed[34U];
        memcpy(copy_of_seed, seed, (size_t)34U * sizeof(uint8_t));
        uint8_t seeds[2U][34U]; KRML_MAYBE_FOR2(
            i, (size_t)0U, (size_t)2U, (size_t)1U,
            memcpy(seeds[i], copy_of_seed, (size_t)34U * sizeof(uint8_t)););
        KRML_MAYBE_FOR2(i, (size_t)0U, (size_t)2U, (size_t)1U, size_t j = i;
                        seeds[j][32U] = (uint8_t)i1; seeds[j][33U] = (uint8_t)j;);
        /* Passing arrays by value in Rust generates a copy in C */
        uint8_t copy_of_seeds[2U][34U];
        memcpy(copy_of_seeds, seeds, (size_t)2U * sizeof(uint8_t[34U]));
        libcrux_ml_kem_polynomial_PolynomialRingElement_f0 sampled[2U];
        sample_from_xof_460(copy_of_seeds, sampled);
        for (size_t i = (size_t)0U;
             i < Eurydice_slice_len(
                     Eurydice_array_to_slice(
                         (size_t)2U, sampled,
                         libcrux_ml_kem_polynomial_PolynomialRingElement_f0),
                     libcrux_ml_kem_polynomial_PolynomialRingElement_f0);
             i++) {
            size_t j = i;
            libcrux_ml_kem_polynomial_PolynomialRingElement_f0 sample = sampled[j];
            if (transpose) {
                A_transpose[j][i1] = sample;
            } else {
                A_transpose[i1][j] = sample;
            }
        }

    );
    memcpy(ret, A_transpose,
           (size_t)2U *
               sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0[2U]));
}

/**
A monomorphic instance of K.
with types libcrux_ml_kem_polynomial_PolynomialRingElement
libcrux_ml_kem_vector_portable_vector_type_PortableVector[2size_t], uint8_t

*/
typedef struct tuple_740_s {
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 fst[2U];
    uint8_t snd;
} tuple_740;

/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.PRFxN
with const generics
- K= 2
- LEN= 192
*/
static KRML_MUSTINLINE void
PRFxN_1c0(uint8_t (*input)[33U],
          uint8_t ret[2U][192U])
{
    uint8_t out[2U][192U] = { { 0U } };
    KRML_MAYBE_FOR2(
        i, (size_t)0U, (size_t)2U, (size_t)1U, size_t i0 = i;
        libcrux_sha3_portable_shake256(
            Eurydice_array_to_slice((size_t)192U, out[i0], uint8_t),
            Eurydice_array_to_slice((size_t)33U, input[i0], uint8_t)););
    memcpy(ret, out, (size_t)2U * sizeof(uint8_t[192U]));
}

/**
This function found in impl {(libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>)}
*/
/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.PRFxN_f1
with const generics
- K= 2
- LEN= 192
*/
static KRML_MUSTINLINE void
PRFxN_f1_d50(uint8_t (*input)[33U],
             uint8_t ret[2U][192U])
{
    PRFxN_1c0(input, ret);
}

/**
A monomorphic instance of
libcrux_ml_kem.sampling.sample_from_binomial_distribution with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- ETA= 3
*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_f0
sample_from_binomial_distribution_ca0(Eurydice_slice randomness)
{
    return sample_from_binomial_distribution_3_14(randomness);
}

/**
 Sample a vector of ring elements from a centered binomial distribution and
 convert them into their NTT representations.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.sample_vector_cbd_then_ntt
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$2size_t]] with const
generics
- K= 2
- ETA= 3
- ETA_RANDOMNESS_SIZE= 192
*/
static KRML_MUSTINLINE tuple_740
sample_vector_cbd_then_ntt_780(
    uint8_t prf_input[33U], uint8_t domain_separator)
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 re_as_ntt[2U];
    KRML_MAYBE_FOR2(i, (size_t)0U, (size_t)2U, (size_t)1U,
                    re_as_ntt[i] = ZERO_89_c3(););
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_prf_input[33U];
    memcpy(copy_of_prf_input, prf_input, (size_t)33U * sizeof(uint8_t));
    uint8_t prf_inputs[2U][33U];
    KRML_MAYBE_FOR2(
        i, (size_t)0U, (size_t)2U, (size_t)1U,
        memcpy(prf_inputs[i], copy_of_prf_input, (size_t)33U * sizeof(uint8_t)););
    KRML_MAYBE_FOR2(i, (size_t)0U, (size_t)2U, (size_t)1U, size_t i0 = i;
                    prf_inputs[i0][32U] = domain_separator;
                    domain_separator = (uint32_t)domain_separator + 1U;);
    uint8_t prf_outputs[2U][192U];
    PRFxN_f1_d50(prf_inputs, prf_outputs);
    KRML_MAYBE_FOR2(
        i, (size_t)0U, (size_t)2U, (size_t)1U, size_t i0 = i;
        re_as_ntt[i0] = sample_from_binomial_distribution_ca0(
            Eurydice_array_to_slice((size_t)192U, prf_outputs[i0], uint8_t));
        ntt_binomially_sampled_ring_element_63(&re_as_ntt[i0]););
    /* Passing arrays by value in Rust generates a copy in C */
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 copy_of_re_as_ntt[2U];
    memcpy(
        copy_of_re_as_ntt, re_as_ntt,
        (size_t)2U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    tuple_740 lit;
    memcpy(
        lit.fst, copy_of_re_as_ntt,
        (size_t)2U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    lit.snd = domain_separator;
    return lit;
}

/**
 Given two polynomial ring elements `lhs` and `rhs`, compute the pointwise
 sum of their constituent coefficients.
*/
/**
This function found in impl
{libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0]}
*/
/**
A monomorphic instance of libcrux_ml_kem.polynomial.add_to_ring_element_89
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 2
*/
static KRML_MUSTINLINE void
add_to_ring_element_89_1e0(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *self,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *rhs)
{
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(
                 Eurydice_array_to_slice(
                     (size_t)16U, self->coefficients,
                     libcrux_ml_kem_vector_portable_vector_type_PortableVector),
                 libcrux_ml_kem_vector_portable_vector_type_PortableVector);
         i++) {
        size_t i0 = i;
        libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
            libcrux_ml_kem_vector_portable_add_0d(self->coefficients[i0],
                                                  &rhs->coefficients[i0]);
        self->coefficients[i0] = uu____0;
    }
}

/**
 Compute Â ◦ ŝ + ê
*/
/**
A monomorphic instance of libcrux_ml_kem.matrix.compute_As_plus_e
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 2
*/
static KRML_MUSTINLINE void
compute_As_plus_e_c70(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 (*matrix_A)[2U],
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *s_as_ntt,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *error_as_ntt,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret[2U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 result[2U];
    KRML_MAYBE_FOR2(i, (size_t)0U, (size_t)2U, (size_t)1U,
                    result[i] = ZERO_89_c3(););
    for (size_t i0 = (size_t)0U;
         i0 < Eurydice_slice_len(
                  Eurydice_array_to_slice(
                      (size_t)2U, matrix_A,
                      libcrux_ml_kem_polynomial_PolynomialRingElement_f0[2U]),
                  libcrux_ml_kem_polynomial_PolynomialRingElement_f0[2U]);
         i0++) {
        size_t i1 = i0;
        libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *row = matrix_A[i1];
        for (size_t i = (size_t)0U;
             i < Eurydice_slice_len(
                     Eurydice_array_to_slice(
                         (size_t)2U, row,
                         libcrux_ml_kem_polynomial_PolynomialRingElement_f0),
                     libcrux_ml_kem_polynomial_PolynomialRingElement_f0);
             i++) {
            size_t j = i;
            libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *matrix_element =
                &row[j];
            libcrux_ml_kem_polynomial_PolynomialRingElement_f0 product =
                ntt_multiply_89_3b(matrix_element, &s_as_ntt[j]);
            add_to_ring_element_89_1e0(&result[i1], &product);
        }
        add_standard_error_reduce_89_64(&result[i1], &error_as_ntt[i1]);
    }
    memcpy(
        ret, result,
        (size_t)2U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
}

/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.generate_keypair
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$2size_t]],
libcrux_ml_kem_variant_MlKem with const generics
- K= 2
- PRIVATE_KEY_SIZE= 768
- PUBLIC_KEY_SIZE= 800
- RANKED_BYTES_PER_RING_ELEMENT= 768
- ETA1= 3
- ETA1_RANDOMNESS_SIZE= 192
*/
static libcrux_ml_kem_utils_extraction_helper_Keypair512
generate_keypair_a20(
    Eurydice_slice key_generation_seed)
{
    uint8_t hashed[64U];
    cpa_keygen_seed_d8_4d(key_generation_seed, hashed);
    Eurydice_slice_uint8_t_x2 uu____0 = Eurydice_slice_split_at(
        Eurydice_array_to_slice((size_t)64U, hashed, uint8_t), (size_t)32U,
        uint8_t, Eurydice_slice_uint8_t_x2);
    Eurydice_slice seed_for_A0 = uu____0.fst;
    Eurydice_slice seed_for_secret_and_error = uu____0.snd;
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 A_transpose[2U][2U];
    uint8_t ret[34U];
    libcrux_ml_kem_utils_into_padded_array_6d1(seed_for_A0, ret);
    sample_matrix_A_e70(ret, true, A_transpose);
    uint8_t prf_input[33U];
    libcrux_ml_kem_utils_into_padded_array_6d2(seed_for_secret_and_error,
                                               prf_input);
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_prf_input0[33U];
    memcpy(copy_of_prf_input0, prf_input, (size_t)33U * sizeof(uint8_t));
    tuple_740 uu____2 = sample_vector_cbd_then_ntt_780(copy_of_prf_input0, 0U);
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 secret_as_ntt[2U];
    memcpy(
        secret_as_ntt, uu____2.fst,
        (size_t)2U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    uint8_t domain_separator = uu____2.snd;
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_prf_input[33U];
    memcpy(copy_of_prf_input, prf_input, (size_t)33U * sizeof(uint8_t));
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 error_as_ntt[2U];
    memcpy(
        error_as_ntt,
        sample_vector_cbd_then_ntt_780(copy_of_prf_input, domain_separator).fst,
        (size_t)2U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 t_as_ntt[2U];
    compute_As_plus_e_c70(A_transpose, secret_as_ntt, error_as_ntt, t_as_ntt);
    uint8_t seed_for_A[32U];
    core_result_Result_00 dst;
    Eurydice_slice_to_array2(&dst, seed_for_A0, Eurydice_slice, uint8_t[32U]);
    core_result_unwrap_41_33(dst, seed_for_A);
    uint8_t public_key_serialized[800U];
    serialize_public_key_670(
        t_as_ntt, Eurydice_array_to_slice((size_t)32U, seed_for_A, uint8_t),
        public_key_serialized);
    uint8_t secret_key_serialized[768U];
    serialize_secret_key_a30(secret_as_ntt, secret_key_serialized);
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_secret_key_serialized[768U];
    memcpy(copy_of_secret_key_serialized, secret_key_serialized,
           (size_t)768U * sizeof(uint8_t));
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_public_key_serialized[800U];
    memcpy(copy_of_public_key_serialized, public_key_serialized,
           (size_t)800U * sizeof(uint8_t));
    libcrux_ml_kem_utils_extraction_helper_Keypair512 lit;
    memcpy(lit.fst, copy_of_secret_key_serialized,
           (size_t)768U * sizeof(uint8_t));
    memcpy(lit.snd, copy_of_public_key_serialized,
           (size_t)800U * sizeof(uint8_t));
    return lit;
}

/**
 Serialize the secret key.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cca.serialize_kem_secret_key
with types libcrux_ml_kem_hash_functions_portable_PortableHash[[$2size_t]]
with const generics
- K= 2
- SERIALIZED_KEY_LEN= 1632
*/
static KRML_MUSTINLINE void
serialize_kem_secret_key_19(
    Eurydice_slice private_key, Eurydice_slice public_key,
    Eurydice_slice implicit_rejection_value, uint8_t ret[1632U])
{
    uint8_t out[1632U] = { 0U };
    size_t pointer = (size_t)0U;
    uint8_t *uu____0 = out;
    size_t uu____1 = pointer;
    size_t uu____2 = pointer;
    Eurydice_slice_copy(
        Eurydice_array_to_subslice2(
            uu____0, uu____1, uu____2 + Eurydice_slice_len(private_key, uint8_t),
            uint8_t),
        private_key, uint8_t);
    pointer = pointer + Eurydice_slice_len(private_key, uint8_t);
    uint8_t *uu____3 = out;
    size_t uu____4 = pointer;
    size_t uu____5 = pointer;
    Eurydice_slice_copy(
        Eurydice_array_to_subslice2(
            uu____3, uu____4, uu____5 + Eurydice_slice_len(public_key, uint8_t),
            uint8_t),
        public_key, uint8_t);
    pointer = pointer + Eurydice_slice_len(public_key, uint8_t);
    Eurydice_slice uu____6 = Eurydice_array_to_subslice2(
        out, pointer, pointer + LIBCRUX_ML_KEM_CONSTANTS_H_DIGEST_SIZE, uint8_t);
    uint8_t ret0[32U];
    H_f1_190(public_key, ret0);
    Eurydice_slice_copy(
        uu____6, Eurydice_array_to_slice((size_t)32U, ret0, uint8_t), uint8_t);
    pointer = pointer + LIBCRUX_ML_KEM_CONSTANTS_H_DIGEST_SIZE;
    uint8_t *uu____7 = out;
    size_t uu____8 = pointer;
    size_t uu____9 = pointer;
    Eurydice_slice_copy(
        Eurydice_array_to_subslice2(
            uu____7, uu____8,
            uu____9 + Eurydice_slice_len(implicit_rejection_value, uint8_t),
            uint8_t),
        implicit_rejection_value, uint8_t);
    memcpy(ret, out, (size_t)1632U * sizeof(uint8_t));
}

/**
 Packed API

 Generate a key pair.

 Depending on the `Vector` and `Hasher` used, this requires different hardware
 features
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cca.generate_keypair
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$2size_t]],
libcrux_ml_kem_variant_MlKem with const generics
- K= 2
- CPA_PRIVATE_KEY_SIZE= 768
- PRIVATE_KEY_SIZE= 1632
- PUBLIC_KEY_SIZE= 800
- BYTES_PER_RING_ELEMENT= 768
- ETA1= 3
- ETA1_RANDOMNESS_SIZE= 192
*/
libcrux_ml_kem_types_MlKemKeyPair_cb
libcrux_ml_kem_ind_cca_generate_keypair_f60(uint8_t randomness[64U])
{
    Eurydice_slice ind_cpa_keypair_randomness = Eurydice_array_to_subslice2(
        randomness, (size_t)0U,
        LIBCRUX_ML_KEM_CONSTANTS_CPA_PKE_KEY_GENERATION_SEED_SIZE, uint8_t);
    Eurydice_slice implicit_rejection_value = Eurydice_array_to_subslice_from(
        (size_t)64U, randomness,
        LIBCRUX_ML_KEM_CONSTANTS_CPA_PKE_KEY_GENERATION_SEED_SIZE, uint8_t,
        size_t);
    libcrux_ml_kem_utils_extraction_helper_Keypair512 uu____0 =
        generate_keypair_a20(ind_cpa_keypair_randomness);
    uint8_t ind_cpa_private_key[768U];
    memcpy(ind_cpa_private_key, uu____0.fst, (size_t)768U * sizeof(uint8_t));
    uint8_t public_key[800U];
    memcpy(public_key, uu____0.snd, (size_t)800U * sizeof(uint8_t));
    uint8_t secret_key_serialized[1632U];
    serialize_kem_secret_key_19(
        Eurydice_array_to_slice((size_t)768U, ind_cpa_private_key, uint8_t),
        Eurydice_array_to_slice((size_t)800U, public_key, uint8_t),
        implicit_rejection_value, secret_key_serialized);
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_secret_key_serialized[1632U];
    memcpy(copy_of_secret_key_serialized, secret_key_serialized,
           (size_t)1632U * sizeof(uint8_t));
    libcrux_ml_kem_types_MlKemPrivateKey_5e private_key =
        libcrux_ml_kem_types_from_05_89(copy_of_secret_key_serialized);
    libcrux_ml_kem_types_MlKemPrivateKey_5e uu____2 = private_key;
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_public_key[800U];
    memcpy(copy_of_public_key, public_key, (size_t)800U * sizeof(uint8_t));
    return libcrux_ml_kem_types_from_17_82(
        uu____2, libcrux_ml_kem_types_from_b6_96(copy_of_public_key));
}

/**
This function found in impl {(libcrux_ml_kem::variant::Variant for
libcrux_ml_kem::variant::MlKem)}
*/
/**
A monomorphic instance of libcrux_ml_kem.variant.entropy_preprocess_d8
with types libcrux_ml_kem_hash_functions_portable_PortableHash[[$2size_t]]
with const generics
- K= 2
*/
static KRML_MUSTINLINE void
entropy_preprocess_d8_6c(Eurydice_slice randomness,
                         uint8_t ret[32U])
{
    uint8_t out[32U] = { 0U };
    Eurydice_slice_copy(Eurydice_array_to_slice((size_t)32U, out, uint8_t),
                        randomness, uint8_t);
    memcpy(ret, out, (size_t)32U * sizeof(uint8_t));
}

/**
 This function deserializes ring elements and reduces the result by the field
 modulus.

 This function MUST NOT be used on secret inputs.
*/
/**
A monomorphic instance of
libcrux_ml_kem.serialize.deserialize_ring_elements_reduced with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- PUBLIC_KEY_SIZE= 768
- K= 2
*/
static KRML_MUSTINLINE void
deserialize_ring_elements_reduced_4f1(
    Eurydice_slice public_key,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret[2U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 deserialized_pk[2U];
    KRML_MAYBE_FOR2(i, (size_t)0U, (size_t)2U, (size_t)1U,
                    deserialized_pk[i] = ZERO_89_c3(););
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(public_key, uint8_t) /
                 LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT;
         i++) {
        size_t i0 = i;
        Eurydice_slice ring_element = Eurydice_slice_subslice2(
            public_key, i0 * LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
            i0 * LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT +
                LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
            uint8_t);
        libcrux_ml_kem_polynomial_PolynomialRingElement_f0 uu____0 =
            deserialize_to_reduced_ring_element_45(ring_element);
        deserialized_pk[i0] = uu____0;
    }
    memcpy(
        ret, deserialized_pk,
        (size_t)2U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
}

/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.PRFxN
with const generics
- K= 2
- LEN= 128
*/
static KRML_MUSTINLINE void
PRFxN_1c1(uint8_t (*input)[33U],
          uint8_t ret[2U][128U])
{
    uint8_t out[2U][128U] = { { 0U } };
    KRML_MAYBE_FOR2(
        i, (size_t)0U, (size_t)2U, (size_t)1U, size_t i0 = i;
        libcrux_sha3_portable_shake256(
            Eurydice_array_to_slice((size_t)128U, out[i0], uint8_t),
            Eurydice_array_to_slice((size_t)33U, input[i0], uint8_t)););
    memcpy(ret, out, (size_t)2U * sizeof(uint8_t[128U]));
}

/**
This function found in impl {(libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>)}
*/
/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.PRFxN_f1
with const generics
- K= 2
- LEN= 128
*/
static KRML_MUSTINLINE void
PRFxN_f1_d51(uint8_t (*input)[33U],
             uint8_t ret[2U][128U])
{
    PRFxN_1c1(input, ret);
}

/**
 Sample a vector of ring elements from a centered binomial distribution.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.sample_ring_element_cbd
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$2size_t]] with const
generics
- K= 2
- ETA2_RANDOMNESS_SIZE= 128
- ETA2= 2
*/
static KRML_MUSTINLINE tuple_740
sample_ring_element_cbd_a80(uint8_t prf_input[33U], uint8_t domain_separator)
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 error_1[2U];
    KRML_MAYBE_FOR2(i, (size_t)0U, (size_t)2U, (size_t)1U,
                    error_1[i] = ZERO_89_c3(););
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_prf_input[33U];
    memcpy(copy_of_prf_input, prf_input, (size_t)33U * sizeof(uint8_t));
    uint8_t prf_inputs[2U][33U];
    KRML_MAYBE_FOR2(
        i, (size_t)0U, (size_t)2U, (size_t)1U,
        memcpy(prf_inputs[i], copy_of_prf_input, (size_t)33U * sizeof(uint8_t)););
    KRML_MAYBE_FOR2(i, (size_t)0U, (size_t)2U, (size_t)1U, size_t i0 = i;
                    prf_inputs[i0][32U] = domain_separator;
                    domain_separator = (uint32_t)domain_separator + 1U;);
    uint8_t prf_outputs[2U][128U];
    PRFxN_f1_d51(prf_inputs, prf_outputs);
    KRML_MAYBE_FOR2(
        i, (size_t)0U, (size_t)2U, (size_t)1U, size_t i0 = i;
        libcrux_ml_kem_polynomial_PolynomialRingElement_f0 uu____1 =
            sample_from_binomial_distribution_ca(
                Eurydice_array_to_slice((size_t)128U, prf_outputs[i0], uint8_t));
        error_1[i0] = uu____1;);
    /* Passing arrays by value in Rust generates a copy in C */
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 copy_of_error_1[2U];
    memcpy(
        copy_of_error_1, error_1,
        (size_t)2U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    tuple_740 lit;
    memcpy(
        lit.fst, copy_of_error_1,
        (size_t)2U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    lit.snd = domain_separator;
    return lit;
}

/**
This function found in impl {(libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>)}
*/
/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.PRF_f1
with const generics
- K= 2
- LEN= 128
*/
static KRML_MUSTINLINE void
PRF_f1_2e2(Eurydice_slice input,
           uint8_t ret[128U])
{
    PRF_7c0(input, ret);
}

/**
A monomorphic instance of libcrux_ml_kem.invert_ntt.invert_ntt_montgomery
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 2
*/
static KRML_MUSTINLINE void
invert_ntt_montgomery_c90(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *re)
{
    size_t zeta_i =
        LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT / (size_t)2U;
    invert_ntt_at_layer_1_4b(&zeta_i, re);
    invert_ntt_at_layer_2_2b(&zeta_i, re);
    invert_ntt_at_layer_3_97(&zeta_i, re);
    invert_ntt_at_layer_4_plus_04(&zeta_i, re, (size_t)4U);
    invert_ntt_at_layer_4_plus_04(&zeta_i, re, (size_t)5U);
    invert_ntt_at_layer_4_plus_04(&zeta_i, re, (size_t)6U);
    invert_ntt_at_layer_4_plus_04(&zeta_i, re, (size_t)7U);
    poly_barrett_reduce_89_d8(re);
}

/**
 Compute u := InvertNTT(Aᵀ ◦ r̂) + e₁
*/
/**
A monomorphic instance of libcrux_ml_kem.matrix.compute_vector_u
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 2
*/
static KRML_MUSTINLINE void
compute_vector_u_930(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 (*a_as_ntt)[2U],
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *r_as_ntt,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *error_1,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret[2U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 result[2U];
    KRML_MAYBE_FOR2(i, (size_t)0U, (size_t)2U, (size_t)1U,
                    result[i] = ZERO_89_c3(););
    for (size_t i0 = (size_t)0U;
         i0 < Eurydice_slice_len(
                  Eurydice_array_to_slice(
                      (size_t)2U, a_as_ntt,
                      libcrux_ml_kem_polynomial_PolynomialRingElement_f0[2U]),
                  libcrux_ml_kem_polynomial_PolynomialRingElement_f0[2U]);
         i0++) {
        size_t i1 = i0;
        libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *row = a_as_ntt[i1];
        for (size_t i = (size_t)0U;
             i < Eurydice_slice_len(
                     Eurydice_array_to_slice(
                         (size_t)2U, row,
                         libcrux_ml_kem_polynomial_PolynomialRingElement_f0),
                     libcrux_ml_kem_polynomial_PolynomialRingElement_f0);
             i++) {
            size_t j = i;
            libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *a_element = &row[j];
            libcrux_ml_kem_polynomial_PolynomialRingElement_f0 product =
                ntt_multiply_89_3b(a_element, &r_as_ntt[j]);
            add_to_ring_element_89_1e0(&result[i1], &product);
        }
        invert_ntt_montgomery_c90(&result[i1]);
        add_error_reduce_89_5d(&result[i1], &error_1[i1]);
    }
    memcpy(
        ret, result,
        (size_t)2U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
}

/**
 Compute InverseNTT(tᵀ ◦ r̂) + e₂ + message
*/
/**
A monomorphic instance of libcrux_ml_kem.matrix.compute_ring_element_v
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 2
*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_f0
compute_ring_element_v_540(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *t_as_ntt,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *r_as_ntt,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *error_2,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *message)
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 result = ZERO_89_c3();
    KRML_MAYBE_FOR2(i, (size_t)0U, (size_t)2U, (size_t)1U, size_t i0 = i;
                    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 product =
                        ntt_multiply_89_3b(&t_as_ntt[i0], &r_as_ntt[i0]);
                    add_to_ring_element_89_1e0(&result, &product););
    invert_ntt_montgomery_c90(&result);
    result = add_message_error_reduce_89_c4(error_2, message, result);
    return result;
}

/**
A monomorphic instance of libcrux_ml_kem.serialize.compress_then_serialize_10
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- OUT_LEN= 320
*/
static KRML_MUSTINLINE void
compress_then_serialize_10_88(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *re, uint8_t ret[320U])
{
    uint8_t serialized[320U] = { 0U };
    for (size_t i = (size_t)0U;
         i < LIBCRUX_ML_KEM_POLYNOMIAL_VECTORS_IN_RING_ELEMENT; i++) {
        size_t i0 = i;
        libcrux_ml_kem_vector_portable_vector_type_PortableVector coefficient =
            compress_0d_96(to_unsigned_representative_84(re->coefficients[i0]));
        uint8_t bytes[20U];
        libcrux_ml_kem_vector_portable_serialize_10_0d(coefficient, bytes);
        Eurydice_slice uu____0 = Eurydice_array_to_subslice2(
            serialized, (size_t)20U * i0, (size_t)20U * i0 + (size_t)20U, uint8_t);
        Eurydice_slice_copy(
            uu____0, Eurydice_array_to_slice((size_t)20U, bytes, uint8_t), uint8_t);
    }
    memcpy(ret, serialized, (size_t)320U * sizeof(uint8_t));
}

/**
A monomorphic instance of
libcrux_ml_kem.serialize.compress_then_serialize_ring_element_u with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- COMPRESSION_FACTOR= 10
- OUT_LEN= 320
*/
static KRML_MUSTINLINE void
compress_then_serialize_ring_element_u_2e(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *re, uint8_t ret[320U])
{
    uint8_t uu____0[320U];
    compress_then_serialize_10_88(re, uu____0);
    memcpy(ret, uu____0, (size_t)320U * sizeof(uint8_t));
}

/**
 Call [`compress_then_serialize_ring_element_u`] on each ring element.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.compress_then_serialize_u
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 2
- OUT_LEN= 640
- COMPRESSION_FACTOR= 10
- BLOCK_LEN= 320
*/
static void
compress_then_serialize_u_280(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 input[2U],
    Eurydice_slice out)
{
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(
                 Eurydice_array_to_slice(
                     (size_t)2U, input,
                     libcrux_ml_kem_polynomial_PolynomialRingElement_f0),
                 libcrux_ml_kem_polynomial_PolynomialRingElement_f0);
         i++) {
        size_t i0 = i;
        libcrux_ml_kem_polynomial_PolynomialRingElement_f0 re = input[i0];
        Eurydice_slice uu____0 = Eurydice_slice_subslice2(
            out, i0 * ((size_t)640U / (size_t)2U),
            (i0 + (size_t)1U) * ((size_t)640U / (size_t)2U), uint8_t);
        uint8_t ret[320U];
        compress_then_serialize_ring_element_u_2e(&re, ret);
        Eurydice_slice_copy(
            uu____0, Eurydice_array_to_slice((size_t)320U, ret, uint8_t), uint8_t);
    }
}

/**
A monomorphic instance of
libcrux_ml_kem.serialize.compress_then_serialize_ring_element_v with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- COMPRESSION_FACTOR= 4
- OUT_LEN= 128
*/
static KRML_MUSTINLINE void
compress_then_serialize_ring_element_v_65(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 re, Eurydice_slice out)
{
    compress_then_serialize_4_80(re, out);
}

/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.encrypt
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$2size_t]] with const
generics
- K= 2
- CIPHERTEXT_SIZE= 768
- T_AS_NTT_ENCODED_SIZE= 768
- C1_LEN= 640
- C2_LEN= 128
- U_COMPRESSION_FACTOR= 10
- V_COMPRESSION_FACTOR= 4
- BLOCK_LEN= 320
- ETA1= 3
- ETA1_RANDOMNESS_SIZE= 192
- ETA2= 2
- ETA2_RANDOMNESS_SIZE= 128
*/
static void
encrypt_7b0(Eurydice_slice public_key, uint8_t message[32U],
            Eurydice_slice randomness, uint8_t ret[768U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 t_as_ntt[2U];
    deserialize_ring_elements_reduced_4f1(
        Eurydice_slice_subslice_to(public_key, (size_t)768U, uint8_t, size_t),
        t_as_ntt);
    Eurydice_slice seed =
        Eurydice_slice_subslice_from(public_key, (size_t)768U, uint8_t, size_t);
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 A[2U][2U];
    uint8_t ret0[34U];
    libcrux_ml_kem_utils_into_padded_array_6d1(seed, ret0);
    sample_matrix_A_e70(ret0, false, A);
    uint8_t prf_input[33U];
    libcrux_ml_kem_utils_into_padded_array_6d2(randomness, prf_input);
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_prf_input0[33U];
    memcpy(copy_of_prf_input0, prf_input, (size_t)33U * sizeof(uint8_t));
    tuple_740 uu____1 = sample_vector_cbd_then_ntt_780(copy_of_prf_input0, 0U);
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 r_as_ntt[2U];
    memcpy(
        r_as_ntt, uu____1.fst,
        (size_t)2U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    uint8_t domain_separator0 = uu____1.snd;
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_prf_input[33U];
    memcpy(copy_of_prf_input, prf_input, (size_t)33U * sizeof(uint8_t));
    tuple_740 uu____3 =
        sample_ring_element_cbd_a80(copy_of_prf_input, domain_separator0);
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 error_1[2U];
    memcpy(
        error_1, uu____3.fst,
        (size_t)2U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    uint8_t domain_separator = uu____3.snd;
    prf_input[32U] = domain_separator;
    uint8_t prf_output[128U];
    PRF_f1_2e2(Eurydice_array_to_slice((size_t)33U, prf_input, uint8_t),
               prf_output);
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 error_2 =
        sample_from_binomial_distribution_ca(
            Eurydice_array_to_slice((size_t)128U, prf_output, uint8_t));
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 u[2U];
    compute_vector_u_930(A, r_as_ntt, error_1, u);
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_message[32U];
    memcpy(copy_of_message, message, (size_t)32U * sizeof(uint8_t));
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 message_as_ring_element =
        deserialize_then_decompress_message_f7(copy_of_message);
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 v =
        compute_ring_element_v_540(t_as_ntt, r_as_ntt, &error_2,
                                   &message_as_ring_element);
    uint8_t ciphertext[768U] = { 0U };
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 uu____5[2U];
    memcpy(
        uu____5, u,
        (size_t)2U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    compress_then_serialize_u_280(
        uu____5, Eurydice_array_to_subslice2(ciphertext, (size_t)0U, (size_t)640U,
                                             uint8_t));
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 uu____6 = v;
    compress_then_serialize_ring_element_v_65(
        uu____6, Eurydice_array_to_subslice_from((size_t)768U, ciphertext,
                                                 (size_t)640U, uint8_t, size_t));
    memcpy(ret, ciphertext, (size_t)768U * sizeof(uint8_t));
}

/**
This function found in impl {(libcrux_ml_kem::variant::Variant for
libcrux_ml_kem::variant::MlKem)}
*/
/**
A monomorphic instance of libcrux_ml_kem.variant.kdf_d8
with types libcrux_ml_kem_hash_functions_portable_PortableHash[[$2size_t]]
with const generics
- K= 2
- CIPHERTEXT_SIZE= 768
*/
static KRML_MUSTINLINE void
kdf_d8_32(Eurydice_slice shared_secret,
          uint8_t ret[32U])
{
    uint8_t out[32U] = { 0U };
    Eurydice_slice_copy(Eurydice_array_to_slice((size_t)32U, out, uint8_t),
                        shared_secret, uint8_t);
    memcpy(ret, out, (size_t)32U * sizeof(uint8_t));
}

/**
A monomorphic instance of libcrux_ml_kem.ind_cca.encapsulate
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$2size_t]],
libcrux_ml_kem_variant_MlKem with const generics
- K= 2
- CIPHERTEXT_SIZE= 768
- PUBLIC_KEY_SIZE= 800
- T_AS_NTT_ENCODED_SIZE= 768
- C1_SIZE= 640
- C2_SIZE= 128
- VECTOR_U_COMPRESSION_FACTOR= 10
- VECTOR_V_COMPRESSION_FACTOR= 4
- VECTOR_U_BLOCK_LEN= 320
- ETA1= 3
- ETA1_RANDOMNESS_SIZE= 192
- ETA2= 2
- ETA2_RANDOMNESS_SIZE= 128
*/
tuple_ec
libcrux_ml_kem_ind_cca_encapsulate_eb0(
    libcrux_ml_kem_types_MlKemPublicKey_be *public_key,
    uint8_t randomness[32U])
{
    uint8_t randomness0[32U];
    entropy_preprocess_d8_6c(
        Eurydice_array_to_slice((size_t)32U, randomness, uint8_t), randomness0);
    uint8_t to_hash[64U];
    libcrux_ml_kem_utils_into_padded_array_6d(
        Eurydice_array_to_slice((size_t)32U, randomness0, uint8_t), to_hash);
    Eurydice_slice uu____0 = Eurydice_array_to_subslice_from(
        (size_t)64U, to_hash, LIBCRUX_ML_KEM_CONSTANTS_H_DIGEST_SIZE, uint8_t,
        size_t);
    uint8_t ret[32U];
    H_f1_190(Eurydice_array_to_slice(
                 (size_t)800U, libcrux_ml_kem_types_as_slice_cb_3d(public_key),
                 uint8_t),
             ret);
    Eurydice_slice_copy(
        uu____0, Eurydice_array_to_slice((size_t)32U, ret, uint8_t), uint8_t);
    uint8_t hashed[64U];
    G_f1_380(Eurydice_array_to_slice((size_t)64U, to_hash, uint8_t), hashed);
    Eurydice_slice_uint8_t_x2 uu____1 = Eurydice_slice_split_at(
        Eurydice_array_to_slice((size_t)64U, hashed, uint8_t),
        LIBCRUX_ML_KEM_CONSTANTS_SHARED_SECRET_SIZE, uint8_t,
        Eurydice_slice_uint8_t_x2);
    Eurydice_slice shared_secret = uu____1.fst;
    Eurydice_slice pseudorandomness = uu____1.snd;
    Eurydice_slice uu____2 = Eurydice_array_to_slice(
        (size_t)800U, libcrux_ml_kem_types_as_slice_cb_3d(public_key), uint8_t);
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_randomness[32U];
    memcpy(copy_of_randomness, randomness0, (size_t)32U * sizeof(uint8_t));
    uint8_t ciphertext[768U];
    encrypt_7b0(uu____2, copy_of_randomness, pseudorandomness, ciphertext);
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_ciphertext[768U];
    memcpy(copy_of_ciphertext, ciphertext, (size_t)768U * sizeof(uint8_t));
    libcrux_ml_kem_types_MlKemCiphertext_e8 ciphertext0 =
        libcrux_ml_kem_types_from_01_33(copy_of_ciphertext);
    uint8_t shared_secret_array[32U];
    kdf_d8_32(shared_secret, shared_secret_array);
    libcrux_ml_kem_types_MlKemCiphertext_e8 uu____5 = ciphertext0;
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_shared_secret_array[32U];
    memcpy(copy_of_shared_secret_array, shared_secret_array,
           (size_t)32U * sizeof(uint8_t));
    tuple_ec lit;
    lit.fst = uu____5;
    memcpy(lit.snd, copy_of_shared_secret_array, (size_t)32U * sizeof(uint8_t));
    return lit;
}

/**
 Call [`deserialize_to_uncompressed_ring_element`] for each ring element.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.deserialize_secret_key
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 2
*/
static KRML_MUSTINLINE void
deserialize_secret_key_7b0(
    Eurydice_slice secret_key,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret[2U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 secret_as_ntt[2U];
    KRML_MAYBE_FOR2(i, (size_t)0U, (size_t)2U, (size_t)1U,
                    secret_as_ntt[i] = ZERO_89_c3(););
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(secret_key, uint8_t) /
                 LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT;
         i++) {
        size_t i0 = i;
        Eurydice_slice secret_bytes = Eurydice_slice_subslice2(
            secret_key, i0 * LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
            i0 * LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT +
                LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
            uint8_t);
        libcrux_ml_kem_polynomial_PolynomialRingElement_f0 uu____0 =
            deserialize_to_uncompressed_ring_element_27(secret_bytes);
        secret_as_ntt[i0] = uu____0;
    }
    memcpy(
        ret, secret_as_ntt,
        (size_t)2U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
}

/**
A monomorphic instance of
libcrux_ml_kem.ind_cpa.unpacked.IndCpaPrivateKeyUnpacked with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- $2size_t
*/
typedef struct IndCpaPrivateKeyUnpacked_ae_s {
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 secret_as_ntt[2U];
} IndCpaPrivateKeyUnpacked_ae;

/**
A monomorphic instance of
libcrux_ml_kem.serialize.deserialize_then_decompress_ring_element_u with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- COMPRESSION_FACTOR= 10
*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_f0
deserialize_then_decompress_ring_element_u_c2(Eurydice_slice serialized)
{
    return deserialize_then_decompress_10_50(serialized);
}

/**
A monomorphic instance of libcrux_ml_kem.ntt.ntt_vector_u
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- VECTOR_U_COMPRESSION_FACTOR= 10
*/
static KRML_MUSTINLINE void
ntt_vector_u_6f(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *re)
{
    size_t zeta_i = (size_t)0U;
    ntt_at_layer_4_plus_8c(&zeta_i, re, (size_t)7U);
    ntt_at_layer_4_plus_8c(&zeta_i, re, (size_t)6U);
    ntt_at_layer_4_plus_8c(&zeta_i, re, (size_t)5U);
    ntt_at_layer_4_plus_8c(&zeta_i, re, (size_t)4U);
    ntt_at_layer_3_34(&zeta_i, re);
    ntt_at_layer_2_26(&zeta_i, re);
    ntt_at_layer_1_3c(&zeta_i, re);
    poly_barrett_reduce_89_d8(re);
}

/**
 Call [`deserialize_then_decompress_ring_element_u`] on each ring element
 in the `ciphertext`.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.deserialize_then_decompress_u
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 2
- CIPHERTEXT_SIZE= 768
- U_COMPRESSION_FACTOR= 10
*/
static KRML_MUSTINLINE void
deserialize_then_decompress_u_1a0(
    uint8_t *ciphertext,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret[2U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 u_as_ntt[2U];
    KRML_MAYBE_FOR2(i, (size_t)0U, (size_t)2U, (size_t)1U,
                    u_as_ntt[i] = ZERO_89_c3(););
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(
                 Eurydice_array_to_slice((size_t)768U, ciphertext, uint8_t),
                 uint8_t) /
                 (LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT *
                  (size_t)10U / (size_t)8U);
         i++) {
        size_t i0 = i;
        Eurydice_slice u_bytes = Eurydice_array_to_subslice2(
            ciphertext,
            i0 * (LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT *
                  (size_t)10U / (size_t)8U),
            i0 * (LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT *
                  (size_t)10U / (size_t)8U) +
                LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT *
                    (size_t)10U / (size_t)8U,
            uint8_t);
        u_as_ntt[i0] = deserialize_then_decompress_ring_element_u_c2(u_bytes);
        ntt_vector_u_6f(&u_as_ntt[i0]);
    }
    memcpy(
        ret, u_as_ntt,
        (size_t)2U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
}

/**
A monomorphic instance of
libcrux_ml_kem.serialize.deserialize_then_decompress_ring_element_v with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- COMPRESSION_FACTOR= 4
*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_f0
deserialize_then_decompress_ring_element_v_93(Eurydice_slice serialized)
{
    return deserialize_then_decompress_4_94(serialized);
}

/**
 The following functions compute various expressions involving
 vectors and matrices. The computation of these expressions has been
 abstracted away into these functions in order to save on loop iterations.
 Compute v − InverseNTT(sᵀ ◦ NTT(u))
*/
/**
A monomorphic instance of libcrux_ml_kem.matrix.compute_message
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 2
*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_f0
compute_message_c90(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *v,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *secret_as_ntt,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *u_as_ntt)
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 result = ZERO_89_c3();
    KRML_MAYBE_FOR2(i, (size_t)0U, (size_t)2U, (size_t)1U, size_t i0 = i;
                    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 product =
                        ntt_multiply_89_3b(&secret_as_ntt[i0], &u_as_ntt[i0]);
                    add_to_ring_element_89_1e0(&result, &product););
    invert_ntt_montgomery_c90(&result);
    result = subtract_reduce_89_61(v, result);
    return result;
}

/**
 This function implements <strong>Algorithm 14</strong> of the
 NIST FIPS 203 specification; this is the Kyber CPA-PKE decryption algorithm.

 Algorithm 14 is reproduced below:

 ```plaintext
 Input: decryption key dkₚₖₑ ∈ 𝔹^{384k}.
 Input: ciphertext c ∈ 𝔹^{32(dᵤk + dᵥ)}.
 Output: message m ∈ 𝔹^{32}.

 c₁ ← c[0 : 32dᵤk]
 c₂ ← c[32dᵤk : 32(dᵤk + dᵥ)]
 u ← Decompress_{dᵤ}(ByteDecode_{dᵤ}(c₁))
 v ← Decompress_{dᵥ}(ByteDecode_{dᵥ}(c₂))
 ŝ ← ByteDecode₁₂(dkₚₖₑ)
 w ← v - NTT-¹(ŝᵀ ◦ NTT(u))
 m ← ByteEncode₁(Compress₁(w))
 return m
 ```

 The NIST FIPS 203 standard can be found at
 <https://csrc.nist.gov/pubs/fips/203/ipd>.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.decrypt_unpacked
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 2
- CIPHERTEXT_SIZE= 768
- VECTOR_U_ENCODED_SIZE= 640
- U_COMPRESSION_FACTOR= 10
- V_COMPRESSION_FACTOR= 4
*/
static void
decrypt_unpacked_6b0(IndCpaPrivateKeyUnpacked_ae *secret_key,
                     uint8_t *ciphertext, uint8_t ret[32U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 u_as_ntt[2U];
    deserialize_then_decompress_u_1a0(ciphertext, u_as_ntt);
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 v =
        deserialize_then_decompress_ring_element_v_93(
            Eurydice_array_to_subslice_from((size_t)768U, ciphertext,
                                            (size_t)640U, uint8_t, size_t));
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 message =
        compute_message_c90(&v, secret_key->secret_as_ntt, u_as_ntt);
    uint8_t ret0[32U];
    compress_then_serialize_message_d4(message, ret0);
    memcpy(ret, ret0, (size_t)32U * sizeof(uint8_t));
}

/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.decrypt
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 2
- CIPHERTEXT_SIZE= 768
- VECTOR_U_ENCODED_SIZE= 640
- U_COMPRESSION_FACTOR= 10
- V_COMPRESSION_FACTOR= 4
*/
static void
decrypt_120(Eurydice_slice secret_key, uint8_t *ciphertext,
            uint8_t ret[32U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 secret_as_ntt[2U];
    deserialize_secret_key_7b0(secret_key, secret_as_ntt);
    /* Passing arrays by value in Rust generates a copy in C */
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 copy_of_secret_as_ntt[2U];
    memcpy(
        copy_of_secret_as_ntt, secret_as_ntt,
        (size_t)2U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    IndCpaPrivateKeyUnpacked_ae secret_key_unpacked;
    memcpy(
        secret_key_unpacked.secret_as_ntt, copy_of_secret_as_ntt,
        (size_t)2U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    uint8_t ret0[32U];
    decrypt_unpacked_6b0(&secret_key_unpacked, ciphertext, ret0);
    memcpy(ret, ret0, (size_t)32U * sizeof(uint8_t));
}

/**
This function found in impl {(libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>)}
*/
/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.PRF_f1
with const generics
- K= 2
- LEN= 32
*/
static KRML_MUSTINLINE void
PRF_f1_2e1(Eurydice_slice input, uint8_t ret[32U])
{
    PRF_7c(input, ret);
}

/**
A monomorphic instance of libcrux_ml_kem.ind_cca.decapsulate
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$2size_t]],
libcrux_ml_kem_variant_MlKem with const generics
- K= 2
- SECRET_KEY_SIZE= 1632
- CPA_SECRET_KEY_SIZE= 768
- PUBLIC_KEY_SIZE= 800
- CIPHERTEXT_SIZE= 768
- T_AS_NTT_ENCODED_SIZE= 768
- C1_SIZE= 640
- C2_SIZE= 128
- VECTOR_U_COMPRESSION_FACTOR= 10
- VECTOR_V_COMPRESSION_FACTOR= 4
- C1_BLOCK_SIZE= 320
- ETA1= 3
- ETA1_RANDOMNESS_SIZE= 192
- ETA2= 2
- ETA2_RANDOMNESS_SIZE= 128
- IMPLICIT_REJECTION_HASH_INPUT_SIZE= 800
*/
void
libcrux_ml_kem_ind_cca_decapsulate_1f0(
    libcrux_ml_kem_types_MlKemPrivateKey_5e *private_key,
    libcrux_ml_kem_types_MlKemCiphertext_e8 *ciphertext, uint8_t ret[32U])
{
    Eurydice_slice_uint8_t_x2 uu____0 = Eurydice_slice_split_at(
        Eurydice_array_to_slice((size_t)1632U, private_key->value, uint8_t),
        (size_t)768U, uint8_t, Eurydice_slice_uint8_t_x2);
    Eurydice_slice ind_cpa_secret_key = uu____0.fst;
    Eurydice_slice secret_key0 = uu____0.snd;
    Eurydice_slice_uint8_t_x2 uu____1 = Eurydice_slice_split_at(
        secret_key0, (size_t)800U, uint8_t, Eurydice_slice_uint8_t_x2);
    Eurydice_slice ind_cpa_public_key = uu____1.fst;
    Eurydice_slice secret_key = uu____1.snd;
    Eurydice_slice_uint8_t_x2 uu____2 = Eurydice_slice_split_at(
        secret_key, LIBCRUX_ML_KEM_CONSTANTS_H_DIGEST_SIZE, uint8_t,
        Eurydice_slice_uint8_t_x2);
    Eurydice_slice ind_cpa_public_key_hash = uu____2.fst;
    Eurydice_slice implicit_rejection_value = uu____2.snd;
    uint8_t decrypted[32U];
    decrypt_120(ind_cpa_secret_key, ciphertext->value, decrypted);
    uint8_t to_hash0[64U];
    libcrux_ml_kem_utils_into_padded_array_6d(
        Eurydice_array_to_slice((size_t)32U, decrypted, uint8_t), to_hash0);
    Eurydice_slice_copy(
        Eurydice_array_to_subslice_from(
            (size_t)64U, to_hash0, LIBCRUX_ML_KEM_CONSTANTS_SHARED_SECRET_SIZE,
            uint8_t, size_t),
        ind_cpa_public_key_hash, uint8_t);
    uint8_t hashed[64U];
    G_f1_380(Eurydice_array_to_slice((size_t)64U, to_hash0, uint8_t), hashed);
    Eurydice_slice_uint8_t_x2 uu____3 = Eurydice_slice_split_at(
        Eurydice_array_to_slice((size_t)64U, hashed, uint8_t),
        LIBCRUX_ML_KEM_CONSTANTS_SHARED_SECRET_SIZE, uint8_t,
        Eurydice_slice_uint8_t_x2);
    Eurydice_slice shared_secret0 = uu____3.fst;
    Eurydice_slice pseudorandomness = uu____3.snd;
    uint8_t to_hash[800U];
    libcrux_ml_kem_utils_into_padded_array_6d0(implicit_rejection_value, to_hash);
    Eurydice_slice uu____4 = Eurydice_array_to_subslice_from(
        (size_t)800U, to_hash, LIBCRUX_ML_KEM_CONSTANTS_SHARED_SECRET_SIZE,
        uint8_t, size_t);
    Eurydice_slice_copy(uu____4, libcrux_ml_kem_types_as_ref_00_d8(ciphertext),
                        uint8_t);
    uint8_t implicit_rejection_shared_secret0[32U];
    PRF_f1_2e1(Eurydice_array_to_slice((size_t)800U, to_hash, uint8_t),
               implicit_rejection_shared_secret0);
    Eurydice_slice uu____5 = ind_cpa_public_key;
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_decrypted[32U];
    memcpy(copy_of_decrypted, decrypted, (size_t)32U * sizeof(uint8_t));
    uint8_t expected_ciphertext[768U];
    encrypt_7b0(uu____5, copy_of_decrypted, pseudorandomness,
                expected_ciphertext);
    uint8_t implicit_rejection_shared_secret[32U];
    kdf_d8_32(Eurydice_array_to_slice((size_t)32U,
                                      implicit_rejection_shared_secret0, uint8_t),
              implicit_rejection_shared_secret);
    uint8_t shared_secret[32U];
    kdf_d8_32(shared_secret0, shared_secret);
    uint8_t ret0[32U];
    libcrux_ml_kem_constant_time_ops_compare_ciphertexts_select_shared_secret_in_constant_time(
        libcrux_ml_kem_types_as_ref_00_d8(ciphertext),
        Eurydice_array_to_slice((size_t)768U, expected_ciphertext, uint8_t),
        Eurydice_array_to_slice((size_t)32U, shared_secret, uint8_t),
        Eurydice_array_to_slice((size_t)32U, implicit_rejection_shared_secret,
                                uint8_t),
        ret0);
    memcpy(ret, ret0, (size_t)32U * sizeof(uint8_t));
}

/**
 This function deserializes ring elements and reduces the result by the field
 modulus.

 This function MUST NOT be used on secret inputs.
*/
/**
A monomorphic instance of
libcrux_ml_kem.serialize.deserialize_ring_elements_reduced with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- PUBLIC_KEY_SIZE= 1184
- K= 3
*/
static KRML_MUSTINLINE void
deserialize_ring_elements_reduced_4f0(
    Eurydice_slice public_key,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret[3U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 deserialized_pk[3U];
    KRML_MAYBE_FOR3(i, (size_t)0U, (size_t)3U, (size_t)1U,
                    deserialized_pk[i] = ZERO_89_c3(););
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(public_key, uint8_t) /
                 LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT;
         i++) {
        size_t i0 = i;
        Eurydice_slice ring_element = Eurydice_slice_subslice2(
            public_key, i0 * LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
            i0 * LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT +
                LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
            uint8_t);
        libcrux_ml_kem_polynomial_PolynomialRingElement_f0 uu____0 =
            deserialize_to_reduced_ring_element_45(ring_element);
        deserialized_pk[i0] = uu____0;
    }
    memcpy(
        ret, deserialized_pk,
        (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
}

/**
 Call [`serialize_uncompressed_ring_element`] for each ring element.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.serialize_secret_key
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
- OUT_LEN= 1152
*/
static KRML_MUSTINLINE void
serialize_secret_key_a3(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *key,
    uint8_t ret[1152U])
{
    uint8_t out[1152U] = { 0U };
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(
                 Eurydice_array_to_slice(
                     (size_t)3U, key,
                     libcrux_ml_kem_polynomial_PolynomialRingElement_f0),
                 libcrux_ml_kem_polynomial_PolynomialRingElement_f0);
         i++) {
        size_t i0 = i;
        libcrux_ml_kem_polynomial_PolynomialRingElement_f0 re = key[i0];
        Eurydice_slice uu____0 = Eurydice_array_to_subslice2(
            out, i0 * LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
            (i0 + (size_t)1U) * LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
            uint8_t);
        uint8_t ret0[384U];
        serialize_uncompressed_ring_element_3c(&re, ret0);
        Eurydice_slice_copy(
            uu____0, Eurydice_array_to_slice((size_t)384U, ret0, uint8_t), uint8_t);
    }
    memcpy(ret, out, (size_t)1152U * sizeof(uint8_t));
}

/**
 Concatenate `t` and `ρ` into the public key.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.serialize_public_key
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
- RANKED_BYTES_PER_RING_ELEMENT= 1152
- PUBLIC_KEY_SIZE= 1184
*/
static KRML_MUSTINLINE void
serialize_public_key_67(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *t_as_ntt,
    Eurydice_slice seed_for_a, uint8_t ret[1184U])
{
    uint8_t public_key_serialized[1184U] = { 0U };
    Eurydice_slice uu____0 = Eurydice_array_to_subslice2(
        public_key_serialized, (size_t)0U, (size_t)1152U, uint8_t);
    uint8_t ret0[1152U];
    serialize_secret_key_a3(t_as_ntt, ret0);
    Eurydice_slice_copy(
        uu____0, Eurydice_array_to_slice((size_t)1152U, ret0, uint8_t), uint8_t);
    Eurydice_slice_copy(
        Eurydice_array_to_subslice_from((size_t)1184U, public_key_serialized,
                                        (size_t)1152U, uint8_t, size_t),
        seed_for_a, uint8_t);
    memcpy(ret, public_key_serialized, (size_t)1184U * sizeof(uint8_t));
}

/**
 Validate an ML-KEM public key.

 This implements the Modulus check in 7.2 2.
 Note that the size check in 7.2 1 is covered by the `PUBLIC_KEY_SIZE` in the
 `public_key` type.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cca.validate_public_key
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
- RANKED_BYTES_PER_RING_ELEMENT= 1152
- PUBLIC_KEY_SIZE= 1184
*/
bool
libcrux_ml_kem_ind_cca_validate_public_key_b7(uint8_t *public_key)
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 deserialized_pk[3U];
    deserialize_ring_elements_reduced_4f0(
        Eurydice_array_to_subslice_to((size_t)1184U, public_key, (size_t)1152U,
                                      uint8_t, size_t),
        deserialized_pk);
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *uu____0 = deserialized_pk;
    uint8_t public_key_serialized[1184U];
    serialize_public_key_67(
        uu____0,
        Eurydice_array_to_subslice_from((size_t)1184U, public_key, (size_t)1152U,
                                        uint8_t, size_t),
        public_key_serialized);
    return core_array_equality___core__cmp__PartialEq__Array_U__N___for__Array_T__N____eq(
        (size_t)1184U, public_key, public_key_serialized, uint8_t, uint8_t, bool);
}

/**
This function found in impl {(libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>)}
*/
/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.H_f1
with const generics
- K= 3
*/
static KRML_MUSTINLINE void
H_f1_19(Eurydice_slice input, uint8_t ret[32U])
{
    libcrux_ml_kem_hash_functions_portable_H(input, ret);
}

/**
 Validate an ML-KEM private key.

 This implements the Hash check in 7.3 3.
 Note that the size checks in 7.2 1 and 2 are covered by the `SECRET_KEY_SIZE`
 and `CIPHERTEXT_SIZE` in the `private_key` and `ciphertext` types.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cca.validate_private_key
with types libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]]
with const generics
- K= 3
- SECRET_KEY_SIZE= 2400
- CIPHERTEXT_SIZE= 1088
*/
bool
libcrux_ml_kem_ind_cca_validate_private_key_05(
    libcrux_ml_kem_types_MlKemPrivateKey_55 *private_key,
    libcrux_ml_kem_mlkem768_MlKem768Ciphertext *_ciphertext)
{
    uint8_t t[32U];
    H_f1_19(Eurydice_array_to_subslice2(
                private_key->value, (size_t)384U * (size_t)3U,
                (size_t)768U * (size_t)3U + (size_t)32U, uint8_t),
            t);
    Eurydice_slice expected = Eurydice_array_to_subslice2(
        private_key->value, (size_t)768U * (size_t)3U + (size_t)32U,
        (size_t)768U * (size_t)3U + (size_t)64U, uint8_t);
    return core_array_equality___core__cmp__PartialEq__0___Slice_U____for__Array_T__N___3__eq(
        (size_t)32U, t, &expected, uint8_t, uint8_t, bool);
}

/**
This function found in impl {(libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>)}
*/
/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.G_f1
with const generics
- K= 3
*/
static KRML_MUSTINLINE void
G_f1_38(Eurydice_slice input, uint8_t ret[64U])
{
    libcrux_ml_kem_hash_functions_portable_G(input, ret);
}

/**
This function found in impl {(libcrux_ml_kem::variant::Variant for
libcrux_ml_kem::variant::MlKem)}
*/
/**
A monomorphic instance of libcrux_ml_kem.variant.cpa_keygen_seed_d8
with types libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]]
with const generics
- K= 3
*/
static KRML_MUSTINLINE void
cpa_keygen_seed_d8_b0(
    Eurydice_slice key_generation_seed, uint8_t ret[64U])
{
    uint8_t seed[33U] = { 0U };
    Eurydice_slice_copy(
        Eurydice_array_to_subslice2(
            seed, (size_t)0U,
            LIBCRUX_ML_KEM_CONSTANTS_CPA_PKE_KEY_GENERATION_SEED_SIZE, uint8_t),
        key_generation_seed, uint8_t);
    seed[LIBCRUX_ML_KEM_CONSTANTS_CPA_PKE_KEY_GENERATION_SEED_SIZE] =
        (uint8_t)(size_t)3U;
    uint8_t ret0[64U];
    G_f1_38(Eurydice_array_to_slice((size_t)33U, seed, uint8_t), ret0);
    memcpy(ret, ret0, (size_t)64U * sizeof(uint8_t));
}

/**
A monomorphic instance of libcrux_ml_kem.matrix.sample_matrix_A.closure
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]] with const
generics
- K= 3
*/
static void
closure_fc(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret[3U])
{
    KRML_MAYBE_FOR3(i, (size_t)0U, (size_t)3U, (size_t)1U,
                    ret[i] = ZERO_89_c3(););
}

/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.PortableHash
with const generics
- $3size_t
*/
typedef struct PortableHash_58_s {
    libcrux_sha3_generic_keccak_KeccakState_48 shake128_state[3U];
} PortableHash_58;

/**
A monomorphic instance of
libcrux_ml_kem.hash_functions.portable.shake128_init_absorb with const generics
- K= 3
*/
static KRML_MUSTINLINE PortableHash_58
shake128_init_absorb_79(uint8_t input[3U][34U])
{
    libcrux_sha3_generic_keccak_KeccakState_48 shake128_state[3U];
    KRML_MAYBE_FOR3(
        i, (size_t)0U, (size_t)3U, (size_t)1U,
        shake128_state[i] = libcrux_sha3_portable_incremental_shake128_init(););
    KRML_MAYBE_FOR3(
        i, (size_t)0U, (size_t)3U, (size_t)1U, size_t i0 = i;
        libcrux_sha3_portable_incremental_shake128_absorb_final(
            &shake128_state[i0],
            Eurydice_array_to_slice((size_t)34U, input[i0], uint8_t)););
    /* Passing arrays by value in Rust generates a copy in C */
    libcrux_sha3_generic_keccak_KeccakState_48 copy_of_shake128_state[3U];
    memcpy(copy_of_shake128_state, shake128_state,
           (size_t)3U * sizeof(libcrux_sha3_generic_keccak_KeccakState_48));
    PortableHash_58 lit;
    memcpy(lit.shake128_state, copy_of_shake128_state,
           (size_t)3U * sizeof(libcrux_sha3_generic_keccak_KeccakState_48));
    return lit;
}

/**
This function found in impl {(libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>)}
*/
/**
A monomorphic instance of
libcrux_ml_kem.hash_functions.portable.shake128_init_absorb_f1 with const
generics
- K= 3
*/
static KRML_MUSTINLINE PortableHash_58
shake128_init_absorb_f1_77(uint8_t input[3U][34U])
{
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_input[3U][34U];
    memcpy(copy_of_input, input, (size_t)3U * sizeof(uint8_t[34U]));
    return shake128_init_absorb_79(copy_of_input);
}

/**
A monomorphic instance of
libcrux_ml_kem.hash_functions.portable.shake128_squeeze_three_blocks with const
generics
- K= 3
*/
static KRML_MUSTINLINE void
shake128_squeeze_three_blocks_eb(
    PortableHash_58 *st, uint8_t ret[3U][504U])
{
    uint8_t out[3U][504U] = { { 0U } };
    KRML_MAYBE_FOR3(
        i, (size_t)0U, (size_t)3U, (size_t)1U, size_t i0 = i;
        libcrux_sha3_portable_incremental_shake128_squeeze_first_three_blocks(
            &st->shake128_state[i0],
            Eurydice_array_to_slice((size_t)504U, out[i0], uint8_t)););
    memcpy(ret, out, (size_t)3U * sizeof(uint8_t[504U]));
}

/**
This function found in impl {(libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>)}
*/
/**
A monomorphic instance of
libcrux_ml_kem.hash_functions.portable.shake128_squeeze_three_blocks_f1 with
const generics
- K= 3
*/
static KRML_MUSTINLINE void
shake128_squeeze_three_blocks_f1_84(
    PortableHash_58 *self, uint8_t ret[3U][504U])
{
    shake128_squeeze_three_blocks_eb(self, ret);
}

/**
 If `bytes` contains a set of uniformly random bytes, this function
 uniformly samples a ring element `â` that is treated as being the NTT
 representation of the corresponding polynomial `a`.

 Since rejection sampling is used, it is possible the supplied bytes are
 not enough to sample the element, in which case an `Err` is returned and the
 caller must try again with a fresh set of bytes.

 This function <strong>partially</strong> implements <strong>Algorithm
 6</strong> of the NIST FIPS 203 standard, We say "partially" because this
 implementation only accepts a finite set of bytes as input and returns an error
 if the set is not enough; Algorithm 6 of the FIPS 203 standard on the other
 hand samples from an infinite stream of bytes until the ring element is filled.
 Algorithm 6 is reproduced below:

 ```plaintext
 Input: byte stream B ∈ 𝔹*.
 Output: array â ∈ ℤ₂₅₆.

 i ← 0
 j ← 0
 while j < 256 do
     d₁ ← B[i] + 256·(B[i+1] mod 16)
     d₂ ← ⌊B[i+1]/16⌋ + 16·B[i+2]
     if d₁ < q then
         â[j] ← d₁
         j ← j + 1
     end if
     if d₂ < q and j < 256 then
         â[j] ← d₂
         j ← j + 1
     end if
     i ← i + 3
 end while
 return â
 ```

 The NIST FIPS 203 standard can be found at
 <https://csrc.nist.gov/pubs/fips/203/ipd>.
*/
/**
A monomorphic instance of
libcrux_ml_kem.sampling.sample_from_uniform_distribution_next with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- K= 3
- N= 504
*/
static KRML_MUSTINLINE bool
sample_from_uniform_distribution_next_17(
    uint8_t randomness[3U][504U], size_t *sampled_coefficients,
    int16_t (*out)[272U])
{
    KRML_MAYBE_FOR3(
        i0, (size_t)0U, (size_t)3U, (size_t)1U, size_t i1 = i0;
        for (size_t i = (size_t)0U; i < (size_t)504U / (size_t)24U; i++) {
            size_t r = i;
            if (sampled_coefficients[i1] <
                LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT) {
                Eurydice_slice uu____0 = Eurydice_array_to_subslice2(
                    randomness[i1], r * (size_t)24U, r * (size_t)24U + (size_t)24U,
                    uint8_t);
                size_t sampled = libcrux_ml_kem_vector_portable_rej_sample_0d(
                    uu____0, Eurydice_array_to_subslice2(
                                 out[i1], sampled_coefficients[i1],
                                 sampled_coefficients[i1] + (size_t)16U, int16_t));
                size_t uu____1 = i1;
                sampled_coefficients[uu____1] =
                    sampled_coefficients[uu____1] + sampled;
            }
        });
    bool done = true;
    KRML_MAYBE_FOR3(
        i, (size_t)0U, (size_t)3U, (size_t)1U, size_t i0 = i;
        if (sampled_coefficients[i0] >=
            LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT) {
            sampled_coefficients[i0] =
                LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT;
        } else { done = false; });
    return done;
}

/**
A monomorphic instance of
libcrux_ml_kem.hash_functions.portable.shake128_squeeze_block with const
generics
- K= 3
*/
static KRML_MUSTINLINE void
shake128_squeeze_block_3b(PortableHash_58 *st,
                          uint8_t ret[3U][168U])
{
    uint8_t out[3U][168U] = { { 0U } };
    KRML_MAYBE_FOR3(
        i, (size_t)0U, (size_t)3U, (size_t)1U, size_t i0 = i;
        libcrux_sha3_portable_incremental_shake128_squeeze_next_block(
            &st->shake128_state[i0],
            Eurydice_array_to_slice((size_t)168U, out[i0], uint8_t)););
    memcpy(ret, out, (size_t)3U * sizeof(uint8_t[168U]));
}

/**
This function found in impl {(libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>)}
*/
/**
A monomorphic instance of
libcrux_ml_kem.hash_functions.portable.shake128_squeeze_block_f1 with const
generics
- K= 3
*/
static KRML_MUSTINLINE void
shake128_squeeze_block_f1_8e(
    PortableHash_58 *self, uint8_t ret[3U][168U])
{
    shake128_squeeze_block_3b(self, ret);
}

/**
 If `bytes` contains a set of uniformly random bytes, this function
 uniformly samples a ring element `â` that is treated as being the NTT
 representation of the corresponding polynomial `a`.

 Since rejection sampling is used, it is possible the supplied bytes are
 not enough to sample the element, in which case an `Err` is returned and the
 caller must try again with a fresh set of bytes.

 This function <strong>partially</strong> implements <strong>Algorithm
 6</strong> of the NIST FIPS 203 standard, We say "partially" because this
 implementation only accepts a finite set of bytes as input and returns an error
 if the set is not enough; Algorithm 6 of the FIPS 203 standard on the other
 hand samples from an infinite stream of bytes until the ring element is filled.
 Algorithm 6 is reproduced below:

 ```plaintext
 Input: byte stream B ∈ 𝔹*.
 Output: array â ∈ ℤ₂₅₆.

 i ← 0
 j ← 0
 while j < 256 do
     d₁ ← B[i] + 256·(B[i+1] mod 16)
     d₂ ← ⌊B[i+1]/16⌋ + 16·B[i+2]
     if d₁ < q then
         â[j] ← d₁
         j ← j + 1
     end if
     if d₂ < q and j < 256 then
         â[j] ← d₂
         j ← j + 1
     end if
     i ← i + 3
 end while
 return â
 ```

 The NIST FIPS 203 standard can be found at
 <https://csrc.nist.gov/pubs/fips/203/ipd>.
*/
/**
A monomorphic instance of
libcrux_ml_kem.sampling.sample_from_uniform_distribution_next with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- K= 3
- N= 168
*/
static KRML_MUSTINLINE bool
sample_from_uniform_distribution_next_170(
    uint8_t randomness[3U][168U], size_t *sampled_coefficients,
    int16_t (*out)[272U])
{
    KRML_MAYBE_FOR3(
        i0, (size_t)0U, (size_t)3U, (size_t)1U, size_t i1 = i0;
        for (size_t i = (size_t)0U; i < (size_t)168U / (size_t)24U; i++) {
            size_t r = i;
            if (sampled_coefficients[i1] <
                LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT) {
                Eurydice_slice uu____0 = Eurydice_array_to_subslice2(
                    randomness[i1], r * (size_t)24U, r * (size_t)24U + (size_t)24U,
                    uint8_t);
                size_t sampled = libcrux_ml_kem_vector_portable_rej_sample_0d(
                    uu____0, Eurydice_array_to_subslice2(
                                 out[i1], sampled_coefficients[i1],
                                 sampled_coefficients[i1] + (size_t)16U, int16_t));
                size_t uu____1 = i1;
                sampled_coefficients[uu____1] =
                    sampled_coefficients[uu____1] + sampled;
            }
        });
    bool done = true;
    KRML_MAYBE_FOR3(
        i, (size_t)0U, (size_t)3U, (size_t)1U, size_t i0 = i;
        if (sampled_coefficients[i0] >=
            LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT) {
            sampled_coefficients[i0] =
                LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT;
        } else { done = false; });
    return done;
}

/**
A monomorphic instance of libcrux_ml_kem.sampling.sample_from_xof.closure
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]] with const
generics
- K= 3
*/
static libcrux_ml_kem_polynomial_PolynomialRingElement_f0
closure_e4(
    int16_t s[272U])
{
    return from_i16_array_89_33(
        Eurydice_array_to_subslice2(s, (size_t)0U, (size_t)256U, int16_t));
}

/**
A monomorphic instance of libcrux_ml_kem.sampling.sample_from_xof
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]] with const
generics
- K= 3
*/
static KRML_MUSTINLINE void
sample_from_xof_46(
    uint8_t seeds[3U][34U],
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret[3U])
{
    size_t sampled_coefficients[3U] = { 0U };
    int16_t out[3U][272U] = { { 0U } };
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_seeds[3U][34U];
    memcpy(copy_of_seeds, seeds, (size_t)3U * sizeof(uint8_t[34U]));
    PortableHash_58 xof_state = shake128_init_absorb_f1_77(copy_of_seeds);
    uint8_t randomness0[3U][504U];
    shake128_squeeze_three_blocks_f1_84(&xof_state, randomness0);
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_randomness0[3U][504U];
    memcpy(copy_of_randomness0, randomness0, (size_t)3U * sizeof(uint8_t[504U]));
    bool done = sample_from_uniform_distribution_next_17(
        copy_of_randomness0, sampled_coefficients, out);
    while (true) {
        if (done) {
            break;
        } else {
            uint8_t randomness[3U][168U];
            shake128_squeeze_block_f1_8e(&xof_state, randomness);
            /* Passing arrays by value in Rust generates a copy in C */
            uint8_t copy_of_randomness[3U][168U];
            memcpy(copy_of_randomness, randomness,
                   (size_t)3U * sizeof(uint8_t[168U]));
            done = sample_from_uniform_distribution_next_170(
                copy_of_randomness, sampled_coefficients, out);
        }
    }
    /* Passing arrays by value in Rust generates a copy in C */
    int16_t copy_of_out[3U][272U];
    memcpy(copy_of_out, out, (size_t)3U * sizeof(int16_t[272U]));
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret0[3U];
    KRML_MAYBE_FOR3(i, (size_t)0U, (size_t)3U, (size_t)1U,
                    ret0[i] = closure_e4(copy_of_out[i]););
    memcpy(
        ret, ret0,
        (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
}

/**
A monomorphic instance of libcrux_ml_kem.matrix.sample_matrix_A
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]] with const
generics
- K= 3
*/
static KRML_MUSTINLINE void
sample_matrix_A_e7(
    uint8_t seed[34U], bool transpose,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret[3U][3U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 A_transpose[3U][3U];
    KRML_MAYBE_FOR3(i, (size_t)0U, (size_t)3U, (size_t)1U,
                    closure_fc(A_transpose[i]););
    KRML_MAYBE_FOR3(
        i0, (size_t)0U, (size_t)3U, (size_t)1U, size_t i1 = i0;
        /* Passing arrays by value in Rust generates a copy in C */
        uint8_t copy_of_seed[34U];
        memcpy(copy_of_seed, seed, (size_t)34U * sizeof(uint8_t));
        uint8_t seeds[3U][34U]; KRML_MAYBE_FOR3(
            i, (size_t)0U, (size_t)3U, (size_t)1U,
            memcpy(seeds[i], copy_of_seed, (size_t)34U * sizeof(uint8_t)););
        KRML_MAYBE_FOR3(i, (size_t)0U, (size_t)3U, (size_t)1U, size_t j = i;
                        seeds[j][32U] = (uint8_t)i1; seeds[j][33U] = (uint8_t)j;);
        /* Passing arrays by value in Rust generates a copy in C */
        uint8_t copy_of_seeds[3U][34U];
        memcpy(copy_of_seeds, seeds, (size_t)3U * sizeof(uint8_t[34U]));
        libcrux_ml_kem_polynomial_PolynomialRingElement_f0 sampled[3U];
        sample_from_xof_46(copy_of_seeds, sampled);
        for (size_t i = (size_t)0U;
             i < Eurydice_slice_len(
                     Eurydice_array_to_slice(
                         (size_t)3U, sampled,
                         libcrux_ml_kem_polynomial_PolynomialRingElement_f0),
                     libcrux_ml_kem_polynomial_PolynomialRingElement_f0);
             i++) {
            size_t j = i;
            libcrux_ml_kem_polynomial_PolynomialRingElement_f0 sample = sampled[j];
            if (transpose) {
                A_transpose[j][i1] = sample;
            } else {
                A_transpose[i1][j] = sample;
            }
        }

    );
    memcpy(ret, A_transpose,
           (size_t)3U *
               sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0[3U]));
}

/**
A monomorphic instance of K.
with types libcrux_ml_kem_polynomial_PolynomialRingElement
libcrux_ml_kem_vector_portable_vector_type_PortableVector[3size_t], uint8_t

*/
typedef struct tuple_b0_s {
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 fst[3U];
    uint8_t snd;
} tuple_b0;

/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.PRFxN
with const generics
- K= 3
- LEN= 128
*/
static KRML_MUSTINLINE void
PRFxN_1c(uint8_t (*input)[33U],
         uint8_t ret[3U][128U])
{
    uint8_t out[3U][128U] = { { 0U } };
    KRML_MAYBE_FOR3(
        i, (size_t)0U, (size_t)3U, (size_t)1U, size_t i0 = i;
        libcrux_sha3_portable_shake256(
            Eurydice_array_to_slice((size_t)128U, out[i0], uint8_t),
            Eurydice_array_to_slice((size_t)33U, input[i0], uint8_t)););
    memcpy(ret, out, (size_t)3U * sizeof(uint8_t[128U]));
}

/**
This function found in impl {(libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>)}
*/
/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.PRFxN_f1
with const generics
- K= 3
- LEN= 128
*/
static KRML_MUSTINLINE void
PRFxN_f1_d5(uint8_t (*input)[33U],
            uint8_t ret[3U][128U])
{
    PRFxN_1c(input, ret);
}

/**
 Sample a vector of ring elements from a centered binomial distribution and
 convert them into their NTT representations.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.sample_vector_cbd_then_ntt
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]] with const
generics
- K= 3
- ETA= 2
- ETA_RANDOMNESS_SIZE= 128
*/
static KRML_MUSTINLINE tuple_b0
sample_vector_cbd_then_ntt_78(
    uint8_t prf_input[33U], uint8_t domain_separator)
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 re_as_ntt[3U];
    KRML_MAYBE_FOR3(i, (size_t)0U, (size_t)3U, (size_t)1U,
                    re_as_ntt[i] = ZERO_89_c3(););
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_prf_input[33U];
    memcpy(copy_of_prf_input, prf_input, (size_t)33U * sizeof(uint8_t));
    uint8_t prf_inputs[3U][33U];
    KRML_MAYBE_FOR3(
        i, (size_t)0U, (size_t)3U, (size_t)1U,
        memcpy(prf_inputs[i], copy_of_prf_input, (size_t)33U * sizeof(uint8_t)););
    KRML_MAYBE_FOR3(i, (size_t)0U, (size_t)3U, (size_t)1U, size_t i0 = i;
                    prf_inputs[i0][32U] = domain_separator;
                    domain_separator = (uint32_t)domain_separator + 1U;);
    uint8_t prf_outputs[3U][128U];
    PRFxN_f1_d5(prf_inputs, prf_outputs);
    KRML_MAYBE_FOR3(
        i, (size_t)0U, (size_t)3U, (size_t)1U, size_t i0 = i;
        re_as_ntt[i0] = sample_from_binomial_distribution_ca(
            Eurydice_array_to_slice((size_t)128U, prf_outputs[i0], uint8_t));
        ntt_binomially_sampled_ring_element_63(&re_as_ntt[i0]););
    /* Passing arrays by value in Rust generates a copy in C */
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 copy_of_re_as_ntt[3U];
    memcpy(
        copy_of_re_as_ntt, re_as_ntt,
        (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    tuple_b0 lit;
    memcpy(
        lit.fst, copy_of_re_as_ntt,
        (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    lit.snd = domain_separator;
    return lit;
}

/**
 Given two polynomial ring elements `lhs` and `rhs`, compute the pointwise
 sum of their constituent coefficients.
*/
/**
This function found in impl
{libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0]}
*/
/**
A monomorphic instance of libcrux_ml_kem.polynomial.add_to_ring_element_89
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
*/
static KRML_MUSTINLINE void
add_to_ring_element_89_1e(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *self,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *rhs)
{
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(
                 Eurydice_array_to_slice(
                     (size_t)16U, self->coefficients,
                     libcrux_ml_kem_vector_portable_vector_type_PortableVector),
                 libcrux_ml_kem_vector_portable_vector_type_PortableVector);
         i++) {
        size_t i0 = i;
        libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
            libcrux_ml_kem_vector_portable_add_0d(self->coefficients[i0],
                                                  &rhs->coefficients[i0]);
        self->coefficients[i0] = uu____0;
    }
}

/**
 Compute Â ◦ ŝ + ê
*/
/**
A monomorphic instance of libcrux_ml_kem.matrix.compute_As_plus_e
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
*/
static KRML_MUSTINLINE void
compute_As_plus_e_c7(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 (*matrix_A)[3U],
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *s_as_ntt,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *error_as_ntt,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret[3U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 result[3U];
    KRML_MAYBE_FOR3(i, (size_t)0U, (size_t)3U, (size_t)1U,
                    result[i] = ZERO_89_c3(););
    for (size_t i0 = (size_t)0U;
         i0 < Eurydice_slice_len(
                  Eurydice_array_to_slice(
                      (size_t)3U, matrix_A,
                      libcrux_ml_kem_polynomial_PolynomialRingElement_f0[3U]),
                  libcrux_ml_kem_polynomial_PolynomialRingElement_f0[3U]);
         i0++) {
        size_t i1 = i0;
        libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *row = matrix_A[i1];
        for (size_t i = (size_t)0U;
             i < Eurydice_slice_len(
                     Eurydice_array_to_slice(
                         (size_t)3U, row,
                         libcrux_ml_kem_polynomial_PolynomialRingElement_f0),
                     libcrux_ml_kem_polynomial_PolynomialRingElement_f0);
             i++) {
            size_t j = i;
            libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *matrix_element =
                &row[j];
            libcrux_ml_kem_polynomial_PolynomialRingElement_f0 product =
                ntt_multiply_89_3b(matrix_element, &s_as_ntt[j]);
            add_to_ring_element_89_1e(&result[i1], &product);
        }
        add_standard_error_reduce_89_64(&result[i1], &error_as_ntt[i1]);
    }
    memcpy(
        ret, result,
        (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
}

/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.generate_keypair
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]],
libcrux_ml_kem_variant_MlKem with const generics
- K= 3
- PRIVATE_KEY_SIZE= 1152
- PUBLIC_KEY_SIZE= 1184
- RANKED_BYTES_PER_RING_ELEMENT= 1152
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
*/
static libcrux_ml_kem_utils_extraction_helper_Keypair768
generate_keypair_a2(
    Eurydice_slice key_generation_seed)
{
    uint8_t hashed[64U];
    cpa_keygen_seed_d8_b0(key_generation_seed, hashed);
    Eurydice_slice_uint8_t_x2 uu____0 = Eurydice_slice_split_at(
        Eurydice_array_to_slice((size_t)64U, hashed, uint8_t), (size_t)32U,
        uint8_t, Eurydice_slice_uint8_t_x2);
    Eurydice_slice seed_for_A0 = uu____0.fst;
    Eurydice_slice seed_for_secret_and_error = uu____0.snd;
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 A_transpose[3U][3U];
    uint8_t ret[34U];
    libcrux_ml_kem_utils_into_padded_array_6d1(seed_for_A0, ret);
    sample_matrix_A_e7(ret, true, A_transpose);
    uint8_t prf_input[33U];
    libcrux_ml_kem_utils_into_padded_array_6d2(seed_for_secret_and_error,
                                               prf_input);
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_prf_input0[33U];
    memcpy(copy_of_prf_input0, prf_input, (size_t)33U * sizeof(uint8_t));
    tuple_b0 uu____2 = sample_vector_cbd_then_ntt_78(copy_of_prf_input0, 0U);
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 secret_as_ntt[3U];
    memcpy(
        secret_as_ntt, uu____2.fst,
        (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    uint8_t domain_separator = uu____2.snd;
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_prf_input[33U];
    memcpy(copy_of_prf_input, prf_input, (size_t)33U * sizeof(uint8_t));
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 error_as_ntt[3U];
    memcpy(
        error_as_ntt,
        sample_vector_cbd_then_ntt_78(copy_of_prf_input, domain_separator).fst,
        (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 t_as_ntt[3U];
    compute_As_plus_e_c7(A_transpose, secret_as_ntt, error_as_ntt, t_as_ntt);
    uint8_t seed_for_A[32U];
    core_result_Result_00 dst;
    Eurydice_slice_to_array2(&dst, seed_for_A0, Eurydice_slice, uint8_t[32U]);
    core_result_unwrap_41_33(dst, seed_for_A);
    uint8_t public_key_serialized[1184U];
    serialize_public_key_67(
        t_as_ntt, Eurydice_array_to_slice((size_t)32U, seed_for_A, uint8_t),
        public_key_serialized);
    uint8_t secret_key_serialized[1152U];
    serialize_secret_key_a3(secret_as_ntt, secret_key_serialized);
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_secret_key_serialized[1152U];
    memcpy(copy_of_secret_key_serialized, secret_key_serialized,
           (size_t)1152U * sizeof(uint8_t));
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_public_key_serialized[1184U];
    memcpy(copy_of_public_key_serialized, public_key_serialized,
           (size_t)1184U * sizeof(uint8_t));
    libcrux_ml_kem_utils_extraction_helper_Keypair768 lit;
    memcpy(lit.fst, copy_of_secret_key_serialized,
           (size_t)1152U * sizeof(uint8_t));
    memcpy(lit.snd, copy_of_public_key_serialized,
           (size_t)1184U * sizeof(uint8_t));
    return lit;
}

/**
 Serialize the secret key.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cca.serialize_kem_secret_key
with types libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]]
with const generics
- K= 3
- SERIALIZED_KEY_LEN= 2400
*/
static KRML_MUSTINLINE void
serialize_kem_secret_key_5d(
    Eurydice_slice private_key, Eurydice_slice public_key,
    Eurydice_slice implicit_rejection_value, uint8_t ret[2400U])
{
    uint8_t out[2400U] = { 0U };
    size_t pointer = (size_t)0U;
    uint8_t *uu____0 = out;
    size_t uu____1 = pointer;
    size_t uu____2 = pointer;
    Eurydice_slice_copy(
        Eurydice_array_to_subslice2(
            uu____0, uu____1, uu____2 + Eurydice_slice_len(private_key, uint8_t),
            uint8_t),
        private_key, uint8_t);
    pointer = pointer + Eurydice_slice_len(private_key, uint8_t);
    uint8_t *uu____3 = out;
    size_t uu____4 = pointer;
    size_t uu____5 = pointer;
    Eurydice_slice_copy(
        Eurydice_array_to_subslice2(
            uu____3, uu____4, uu____5 + Eurydice_slice_len(public_key, uint8_t),
            uint8_t),
        public_key, uint8_t);
    pointer = pointer + Eurydice_slice_len(public_key, uint8_t);
    Eurydice_slice uu____6 = Eurydice_array_to_subslice2(
        out, pointer, pointer + LIBCRUX_ML_KEM_CONSTANTS_H_DIGEST_SIZE, uint8_t);
    uint8_t ret0[32U];
    H_f1_19(public_key, ret0);
    Eurydice_slice_copy(
        uu____6, Eurydice_array_to_slice((size_t)32U, ret0, uint8_t), uint8_t);
    pointer = pointer + LIBCRUX_ML_KEM_CONSTANTS_H_DIGEST_SIZE;
    uint8_t *uu____7 = out;
    size_t uu____8 = pointer;
    size_t uu____9 = pointer;
    Eurydice_slice_copy(
        Eurydice_array_to_subslice2(
            uu____7, uu____8,
            uu____9 + Eurydice_slice_len(implicit_rejection_value, uint8_t),
            uint8_t),
        implicit_rejection_value, uint8_t);
    memcpy(ret, out, (size_t)2400U * sizeof(uint8_t));
}

/**
 Packed API

 Generate a key pair.

 Depending on the `Vector` and `Hasher` used, this requires different hardware
 features
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cca.generate_keypair
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]],
libcrux_ml_kem_variant_MlKem with const generics
- K= 3
- CPA_PRIVATE_KEY_SIZE= 1152
- PRIVATE_KEY_SIZE= 2400
- PUBLIC_KEY_SIZE= 1184
- BYTES_PER_RING_ELEMENT= 1152
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
*/
libcrux_ml_kem_mlkem768_MlKem768KeyPair
libcrux_ml_kem_ind_cca_generate_keypair_f6(uint8_t randomness[64U])
{
    Eurydice_slice ind_cpa_keypair_randomness = Eurydice_array_to_subslice2(
        randomness, (size_t)0U,
        LIBCRUX_ML_KEM_CONSTANTS_CPA_PKE_KEY_GENERATION_SEED_SIZE, uint8_t);
    Eurydice_slice implicit_rejection_value = Eurydice_array_to_subslice_from(
        (size_t)64U, randomness,
        LIBCRUX_ML_KEM_CONSTANTS_CPA_PKE_KEY_GENERATION_SEED_SIZE, uint8_t,
        size_t);
    libcrux_ml_kem_utils_extraction_helper_Keypair768 uu____0 =
        generate_keypair_a2(ind_cpa_keypair_randomness);
    uint8_t ind_cpa_private_key[1152U];
    memcpy(ind_cpa_private_key, uu____0.fst, (size_t)1152U * sizeof(uint8_t));
    uint8_t public_key[1184U];
    memcpy(public_key, uu____0.snd, (size_t)1184U * sizeof(uint8_t));
    uint8_t secret_key_serialized[2400U];
    serialize_kem_secret_key_5d(
        Eurydice_array_to_slice((size_t)1152U, ind_cpa_private_key, uint8_t),
        Eurydice_array_to_slice((size_t)1184U, public_key, uint8_t),
        implicit_rejection_value, secret_key_serialized);
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_secret_key_serialized[2400U];
    memcpy(copy_of_secret_key_serialized, secret_key_serialized,
           (size_t)2400U * sizeof(uint8_t));
    libcrux_ml_kem_types_MlKemPrivateKey_55 private_key =
        libcrux_ml_kem_types_from_05_890(copy_of_secret_key_serialized);
    libcrux_ml_kem_types_MlKemPrivateKey_55 uu____2 = private_key;
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_public_key[1184U];
    memcpy(copy_of_public_key, public_key, (size_t)1184U * sizeof(uint8_t));
    return libcrux_ml_kem_types_from_17_820(
        uu____2, libcrux_ml_kem_types_from_b6_960(copy_of_public_key));
}

/**
This function found in impl {(libcrux_ml_kem::variant::Variant for
libcrux_ml_kem::variant::MlKem)}
*/
/**
A monomorphic instance of libcrux_ml_kem.variant.entropy_preprocess_d8
with types libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]]
with const generics
- K= 3
*/
static KRML_MUSTINLINE void
entropy_preprocess_d8_9f(Eurydice_slice randomness,
                         uint8_t ret[32U])
{
    uint8_t out[32U] = { 0U };
    Eurydice_slice_copy(Eurydice_array_to_slice((size_t)32U, out, uint8_t),
                        randomness, uint8_t);
    memcpy(ret, out, (size_t)32U * sizeof(uint8_t));
}

/**
 This function deserializes ring elements and reduces the result by the field
 modulus.

 This function MUST NOT be used on secret inputs.
*/
/**
A monomorphic instance of
libcrux_ml_kem.serialize.deserialize_ring_elements_reduced with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- PUBLIC_KEY_SIZE= 1152
- K= 3
*/
static KRML_MUSTINLINE void
deserialize_ring_elements_reduced_4f(
    Eurydice_slice public_key,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret[3U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 deserialized_pk[3U];
    KRML_MAYBE_FOR3(i, (size_t)0U, (size_t)3U, (size_t)1U,
                    deserialized_pk[i] = ZERO_89_c3(););
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(public_key, uint8_t) /
                 LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT;
         i++) {
        size_t i0 = i;
        Eurydice_slice ring_element = Eurydice_slice_subslice2(
            public_key, i0 * LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
            i0 * LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT +
                LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
            uint8_t);
        libcrux_ml_kem_polynomial_PolynomialRingElement_f0 uu____0 =
            deserialize_to_reduced_ring_element_45(ring_element);
        deserialized_pk[i0] = uu____0;
    }
    memcpy(
        ret, deserialized_pk,
        (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
}

/**
 Sample a vector of ring elements from a centered binomial distribution.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.sample_ring_element_cbd
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]] with const
generics
- K= 3
- ETA2_RANDOMNESS_SIZE= 128
- ETA2= 2
*/
static KRML_MUSTINLINE tuple_b0
sample_ring_element_cbd_a8(uint8_t prf_input[33U], uint8_t domain_separator)
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 error_1[3U];
    KRML_MAYBE_FOR3(i, (size_t)0U, (size_t)3U, (size_t)1U,
                    error_1[i] = ZERO_89_c3(););
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_prf_input[33U];
    memcpy(copy_of_prf_input, prf_input, (size_t)33U * sizeof(uint8_t));
    uint8_t prf_inputs[3U][33U];
    KRML_MAYBE_FOR3(
        i, (size_t)0U, (size_t)3U, (size_t)1U,
        memcpy(prf_inputs[i], copy_of_prf_input, (size_t)33U * sizeof(uint8_t)););
    KRML_MAYBE_FOR3(i, (size_t)0U, (size_t)3U, (size_t)1U, size_t i0 = i;
                    prf_inputs[i0][32U] = domain_separator;
                    domain_separator = (uint32_t)domain_separator + 1U;);
    uint8_t prf_outputs[3U][128U];
    PRFxN_f1_d5(prf_inputs, prf_outputs);
    KRML_MAYBE_FOR3(
        i, (size_t)0U, (size_t)3U, (size_t)1U, size_t i0 = i;
        libcrux_ml_kem_polynomial_PolynomialRingElement_f0 uu____1 =
            sample_from_binomial_distribution_ca(
                Eurydice_array_to_slice((size_t)128U, prf_outputs[i0], uint8_t));
        error_1[i0] = uu____1;);
    /* Passing arrays by value in Rust generates a copy in C */
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 copy_of_error_1[3U];
    memcpy(
        copy_of_error_1, error_1,
        (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    tuple_b0 lit;
    memcpy(
        lit.fst, copy_of_error_1,
        (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    lit.snd = domain_separator;
    return lit;
}

/**
This function found in impl {(libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>)}
*/
/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.PRF_f1
with const generics
- K= 3
- LEN= 128
*/
static KRML_MUSTINLINE void
PRF_f1_2e0(Eurydice_slice input,
           uint8_t ret[128U])
{
    PRF_7c0(input, ret);
}

/**
A monomorphic instance of libcrux_ml_kem.invert_ntt.invert_ntt_montgomery
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
*/
static KRML_MUSTINLINE void
invert_ntt_montgomery_c9(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *re)
{
    size_t zeta_i =
        LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT / (size_t)2U;
    invert_ntt_at_layer_1_4b(&zeta_i, re);
    invert_ntt_at_layer_2_2b(&zeta_i, re);
    invert_ntt_at_layer_3_97(&zeta_i, re);
    invert_ntt_at_layer_4_plus_04(&zeta_i, re, (size_t)4U);
    invert_ntt_at_layer_4_plus_04(&zeta_i, re, (size_t)5U);
    invert_ntt_at_layer_4_plus_04(&zeta_i, re, (size_t)6U);
    invert_ntt_at_layer_4_plus_04(&zeta_i, re, (size_t)7U);
    poly_barrett_reduce_89_d8(re);
}

/**
 Compute u := InvertNTT(Aᵀ ◦ r̂) + e₁
*/
/**
A monomorphic instance of libcrux_ml_kem.matrix.compute_vector_u
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
*/
static KRML_MUSTINLINE void
compute_vector_u_93(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 (*a_as_ntt)[3U],
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *r_as_ntt,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *error_1,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret[3U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 result[3U];
    KRML_MAYBE_FOR3(i, (size_t)0U, (size_t)3U, (size_t)1U,
                    result[i] = ZERO_89_c3(););
    for (size_t i0 = (size_t)0U;
         i0 < Eurydice_slice_len(
                  Eurydice_array_to_slice(
                      (size_t)3U, a_as_ntt,
                      libcrux_ml_kem_polynomial_PolynomialRingElement_f0[3U]),
                  libcrux_ml_kem_polynomial_PolynomialRingElement_f0[3U]);
         i0++) {
        size_t i1 = i0;
        libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *row = a_as_ntt[i1];
        for (size_t i = (size_t)0U;
             i < Eurydice_slice_len(
                     Eurydice_array_to_slice(
                         (size_t)3U, row,
                         libcrux_ml_kem_polynomial_PolynomialRingElement_f0),
                     libcrux_ml_kem_polynomial_PolynomialRingElement_f0);
             i++) {
            size_t j = i;
            libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *a_element = &row[j];
            libcrux_ml_kem_polynomial_PolynomialRingElement_f0 product =
                ntt_multiply_89_3b(a_element, &r_as_ntt[j]);
            add_to_ring_element_89_1e(&result[i1], &product);
        }
        invert_ntt_montgomery_c9(&result[i1]);
        add_error_reduce_89_5d(&result[i1], &error_1[i1]);
    }
    memcpy(
        ret, result,
        (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
}

/**
 Compute InverseNTT(tᵀ ◦ r̂) + e₂ + message
*/
/**
A monomorphic instance of libcrux_ml_kem.matrix.compute_ring_element_v
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_f0
compute_ring_element_v_54(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *t_as_ntt,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *r_as_ntt,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *error_2,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *message)
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 result = ZERO_89_c3();
    KRML_MAYBE_FOR3(i, (size_t)0U, (size_t)3U, (size_t)1U, size_t i0 = i;
                    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 product =
                        ntt_multiply_89_3b(&t_as_ntt[i0], &r_as_ntt[i0]);
                    add_to_ring_element_89_1e(&result, &product););
    invert_ntt_montgomery_c9(&result);
    result = add_message_error_reduce_89_c4(error_2, message, result);
    return result;
}

/**
 Call [`compress_then_serialize_ring_element_u`] on each ring element.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.compress_then_serialize_u
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
- OUT_LEN= 960
- COMPRESSION_FACTOR= 10
- BLOCK_LEN= 320
*/
static void
compress_then_serialize_u_28(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 input[3U],
    Eurydice_slice out)
{
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(
                 Eurydice_array_to_slice(
                     (size_t)3U, input,
                     libcrux_ml_kem_polynomial_PolynomialRingElement_f0),
                 libcrux_ml_kem_polynomial_PolynomialRingElement_f0);
         i++) {
        size_t i0 = i;
        libcrux_ml_kem_polynomial_PolynomialRingElement_f0 re = input[i0];
        Eurydice_slice uu____0 = Eurydice_slice_subslice2(
            out, i0 * ((size_t)960U / (size_t)3U),
            (i0 + (size_t)1U) * ((size_t)960U / (size_t)3U), uint8_t);
        uint8_t ret[320U];
        compress_then_serialize_ring_element_u_2e(&re, ret);
        Eurydice_slice_copy(
            uu____0, Eurydice_array_to_slice((size_t)320U, ret, uint8_t), uint8_t);
    }
}

/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.encrypt
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]] with const
generics
- K= 3
- CIPHERTEXT_SIZE= 1088
- T_AS_NTT_ENCODED_SIZE= 1152
- C1_LEN= 960
- C2_LEN= 128
- U_COMPRESSION_FACTOR= 10
- V_COMPRESSION_FACTOR= 4
- BLOCK_LEN= 320
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
- ETA2= 2
- ETA2_RANDOMNESS_SIZE= 128
*/
static void
encrypt_7b(Eurydice_slice public_key, uint8_t message[32U],
           Eurydice_slice randomness, uint8_t ret[1088U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 t_as_ntt[3U];
    deserialize_ring_elements_reduced_4f(
        Eurydice_slice_subslice_to(public_key, (size_t)1152U, uint8_t, size_t),
        t_as_ntt);
    Eurydice_slice seed =
        Eurydice_slice_subslice_from(public_key, (size_t)1152U, uint8_t, size_t);
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 A[3U][3U];
    uint8_t ret0[34U];
    libcrux_ml_kem_utils_into_padded_array_6d1(seed, ret0);
    sample_matrix_A_e7(ret0, false, A);
    uint8_t prf_input[33U];
    libcrux_ml_kem_utils_into_padded_array_6d2(randomness, prf_input);
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_prf_input0[33U];
    memcpy(copy_of_prf_input0, prf_input, (size_t)33U * sizeof(uint8_t));
    tuple_b0 uu____1 = sample_vector_cbd_then_ntt_78(copy_of_prf_input0, 0U);
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 r_as_ntt[3U];
    memcpy(
        r_as_ntt, uu____1.fst,
        (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    uint8_t domain_separator0 = uu____1.snd;
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_prf_input[33U];
    memcpy(copy_of_prf_input, prf_input, (size_t)33U * sizeof(uint8_t));
    tuple_b0 uu____3 =
        sample_ring_element_cbd_a8(copy_of_prf_input, domain_separator0);
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 error_1[3U];
    memcpy(
        error_1, uu____3.fst,
        (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    uint8_t domain_separator = uu____3.snd;
    prf_input[32U] = domain_separator;
    uint8_t prf_output[128U];
    PRF_f1_2e0(Eurydice_array_to_slice((size_t)33U, prf_input, uint8_t),
               prf_output);
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 error_2 =
        sample_from_binomial_distribution_ca(
            Eurydice_array_to_slice((size_t)128U, prf_output, uint8_t));
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 u[3U];
    compute_vector_u_93(A, r_as_ntt, error_1, u);
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_message[32U];
    memcpy(copy_of_message, message, (size_t)32U * sizeof(uint8_t));
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 message_as_ring_element =
        deserialize_then_decompress_message_f7(copy_of_message);
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 v =
        compute_ring_element_v_54(t_as_ntt, r_as_ntt, &error_2,
                                  &message_as_ring_element);
    uint8_t ciphertext[1088U] = { 0U };
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 uu____5[3U];
    memcpy(
        uu____5, u,
        (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    compress_then_serialize_u_28(
        uu____5, Eurydice_array_to_subslice2(ciphertext, (size_t)0U, (size_t)960U,
                                             uint8_t));
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 uu____6 = v;
    compress_then_serialize_ring_element_v_65(
        uu____6, Eurydice_array_to_subslice_from((size_t)1088U, ciphertext,
                                                 (size_t)960U, uint8_t, size_t));
    memcpy(ret, ciphertext, (size_t)1088U * sizeof(uint8_t));
}

/**
This function found in impl {(libcrux_ml_kem::variant::Variant for
libcrux_ml_kem::variant::MlKem)}
*/
/**
A monomorphic instance of libcrux_ml_kem.variant.kdf_d8
with types libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]]
with const generics
- K= 3
- CIPHERTEXT_SIZE= 1088
*/
static KRML_MUSTINLINE void
kdf_d8_c5(Eurydice_slice shared_secret,
          uint8_t ret[32U])
{
    uint8_t out[32U] = { 0U };
    Eurydice_slice_copy(Eurydice_array_to_slice((size_t)32U, out, uint8_t),
                        shared_secret, uint8_t);
    memcpy(ret, out, (size_t)32U * sizeof(uint8_t));
}

/**
A monomorphic instance of libcrux_ml_kem.ind_cca.encapsulate
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]],
libcrux_ml_kem_variant_MlKem with const generics
- K= 3
- CIPHERTEXT_SIZE= 1088
- PUBLIC_KEY_SIZE= 1184
- T_AS_NTT_ENCODED_SIZE= 1152
- C1_SIZE= 960
- C2_SIZE= 128
- VECTOR_U_COMPRESSION_FACTOR= 10
- VECTOR_V_COMPRESSION_FACTOR= 4
- VECTOR_U_BLOCK_LEN= 320
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
- ETA2= 2
- ETA2_RANDOMNESS_SIZE= 128
*/
tuple_3c
libcrux_ml_kem_ind_cca_encapsulate_eb(
    libcrux_ml_kem_types_MlKemPublicKey_15 *public_key,
    uint8_t randomness[32U])
{
    uint8_t randomness0[32U];
    entropy_preprocess_d8_9f(
        Eurydice_array_to_slice((size_t)32U, randomness, uint8_t), randomness0);
    uint8_t to_hash[64U];
    libcrux_ml_kem_utils_into_padded_array_6d(
        Eurydice_array_to_slice((size_t)32U, randomness0, uint8_t), to_hash);
    Eurydice_slice uu____0 = Eurydice_array_to_subslice_from(
        (size_t)64U, to_hash, LIBCRUX_ML_KEM_CONSTANTS_H_DIGEST_SIZE, uint8_t,
        size_t);
    uint8_t ret[32U];
    H_f1_19(Eurydice_array_to_slice(
                (size_t)1184U, libcrux_ml_kem_types_as_slice_cb_3d0(public_key),
                uint8_t),
            ret);
    Eurydice_slice_copy(
        uu____0, Eurydice_array_to_slice((size_t)32U, ret, uint8_t), uint8_t);
    uint8_t hashed[64U];
    G_f1_38(Eurydice_array_to_slice((size_t)64U, to_hash, uint8_t), hashed);
    Eurydice_slice_uint8_t_x2 uu____1 = Eurydice_slice_split_at(
        Eurydice_array_to_slice((size_t)64U, hashed, uint8_t),
        LIBCRUX_ML_KEM_CONSTANTS_SHARED_SECRET_SIZE, uint8_t,
        Eurydice_slice_uint8_t_x2);
    Eurydice_slice shared_secret = uu____1.fst;
    Eurydice_slice pseudorandomness = uu____1.snd;
    Eurydice_slice uu____2 = Eurydice_array_to_slice(
        (size_t)1184U, libcrux_ml_kem_types_as_slice_cb_3d0(public_key), uint8_t);
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_randomness[32U];
    memcpy(copy_of_randomness, randomness0, (size_t)32U * sizeof(uint8_t));
    uint8_t ciphertext[1088U];
    encrypt_7b(uu____2, copy_of_randomness, pseudorandomness, ciphertext);
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_ciphertext[1088U];
    memcpy(copy_of_ciphertext, ciphertext, (size_t)1088U * sizeof(uint8_t));
    libcrux_ml_kem_mlkem768_MlKem768Ciphertext ciphertext0 =
        libcrux_ml_kem_types_from_01_330(copy_of_ciphertext);
    uint8_t shared_secret_array[32U];
    kdf_d8_c5(shared_secret, shared_secret_array);
    libcrux_ml_kem_mlkem768_MlKem768Ciphertext uu____5 = ciphertext0;
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_shared_secret_array[32U];
    memcpy(copy_of_shared_secret_array, shared_secret_array,
           (size_t)32U * sizeof(uint8_t));
    tuple_3c lit;
    lit.fst = uu____5;
    memcpy(lit.snd, copy_of_shared_secret_array, (size_t)32U * sizeof(uint8_t));
    return lit;
}

/**
 Call [`deserialize_to_uncompressed_ring_element`] for each ring element.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.deserialize_secret_key
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
*/
static KRML_MUSTINLINE void
deserialize_secret_key_7b(
    Eurydice_slice secret_key,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret[3U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 secret_as_ntt[3U];
    KRML_MAYBE_FOR3(i, (size_t)0U, (size_t)3U, (size_t)1U,
                    secret_as_ntt[i] = ZERO_89_c3(););
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(secret_key, uint8_t) /
                 LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT;
         i++) {
        size_t i0 = i;
        Eurydice_slice secret_bytes = Eurydice_slice_subslice2(
            secret_key, i0 * LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
            i0 * LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT +
                LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
            uint8_t);
        libcrux_ml_kem_polynomial_PolynomialRingElement_f0 uu____0 =
            deserialize_to_uncompressed_ring_element_27(secret_bytes);
        secret_as_ntt[i0] = uu____0;
    }
    memcpy(
        ret, secret_as_ntt,
        (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
}

/**
A monomorphic instance of
libcrux_ml_kem.ind_cpa.unpacked.IndCpaPrivateKeyUnpacked with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- $3size_t
*/
typedef struct IndCpaPrivateKeyUnpacked_f8_s {
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 secret_as_ntt[3U];
} IndCpaPrivateKeyUnpacked_f8;

/**
 Call [`deserialize_then_decompress_ring_element_u`] on each ring element
 in the `ciphertext`.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.deserialize_then_decompress_u
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
- CIPHERTEXT_SIZE= 1088
- U_COMPRESSION_FACTOR= 10
*/
static KRML_MUSTINLINE void
deserialize_then_decompress_u_1a(
    uint8_t *ciphertext,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ret[3U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 u_as_ntt[3U];
    KRML_MAYBE_FOR3(i, (size_t)0U, (size_t)3U, (size_t)1U,
                    u_as_ntt[i] = ZERO_89_c3(););
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(
                 Eurydice_array_to_slice((size_t)1088U, ciphertext, uint8_t),
                 uint8_t) /
                 (LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT *
                  (size_t)10U / (size_t)8U);
         i++) {
        size_t i0 = i;
        Eurydice_slice u_bytes = Eurydice_array_to_subslice2(
            ciphertext,
            i0 * (LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT *
                  (size_t)10U / (size_t)8U),
            i0 * (LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT *
                  (size_t)10U / (size_t)8U) +
                LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT *
                    (size_t)10U / (size_t)8U,
            uint8_t);
        u_as_ntt[i0] = deserialize_then_decompress_ring_element_u_c2(u_bytes);
        ntt_vector_u_6f(&u_as_ntt[i0]);
    }
    memcpy(
        ret, u_as_ntt,
        (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
}

/**
 The following functions compute various expressions involving
 vectors and matrices. The computation of these expressions has been
 abstracted away into these functions in order to save on loop iterations.
 Compute v − InverseNTT(sᵀ ◦ NTT(u))
*/
/**
A monomorphic instance of libcrux_ml_kem.matrix.compute_message
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_f0
compute_message_c9(
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *v,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *secret_as_ntt,
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 *u_as_ntt)
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 result = ZERO_89_c3();
    KRML_MAYBE_FOR3(i, (size_t)0U, (size_t)3U, (size_t)1U, size_t i0 = i;
                    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 product =
                        ntt_multiply_89_3b(&secret_as_ntt[i0], &u_as_ntt[i0]);
                    add_to_ring_element_89_1e(&result, &product););
    invert_ntt_montgomery_c9(&result);
    result = subtract_reduce_89_61(v, result);
    return result;
}

/**
 This function implements <strong>Algorithm 14</strong> of the
 NIST FIPS 203 specification; this is the Kyber CPA-PKE decryption algorithm.

 Algorithm 14 is reproduced below:

 ```plaintext
 Input: decryption key dkₚₖₑ ∈ 𝔹^{384k}.
 Input: ciphertext c ∈ 𝔹^{32(dᵤk + dᵥ)}.
 Output: message m ∈ 𝔹^{32}.

 c₁ ← c[0 : 32dᵤk]
 c₂ ← c[32dᵤk : 32(dᵤk + dᵥ)]
 u ← Decompress_{dᵤ}(ByteDecode_{dᵤ}(c₁))
 v ← Decompress_{dᵥ}(ByteDecode_{dᵥ}(c₂))
 ŝ ← ByteDecode₁₂(dkₚₖₑ)
 w ← v - NTT-¹(ŝᵀ ◦ NTT(u))
 m ← ByteEncode₁(Compress₁(w))
 return m
 ```

 The NIST FIPS 203 standard can be found at
 <https://csrc.nist.gov/pubs/fips/203/ipd>.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.decrypt_unpacked
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
- CIPHERTEXT_SIZE= 1088
- VECTOR_U_ENCODED_SIZE= 960
- U_COMPRESSION_FACTOR= 10
- V_COMPRESSION_FACTOR= 4
*/
static void
decrypt_unpacked_6b(IndCpaPrivateKeyUnpacked_f8 *secret_key,
                    uint8_t *ciphertext, uint8_t ret[32U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 u_as_ntt[3U];
    deserialize_then_decompress_u_1a(ciphertext, u_as_ntt);
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 v =
        deserialize_then_decompress_ring_element_v_93(
            Eurydice_array_to_subslice_from((size_t)1088U, ciphertext,
                                            (size_t)960U, uint8_t, size_t));
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 message =
        compute_message_c9(&v, secret_key->secret_as_ntt, u_as_ntt);
    uint8_t ret0[32U];
    compress_then_serialize_message_d4(message, ret0);
    memcpy(ret, ret0, (size_t)32U * sizeof(uint8_t));
}

/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.decrypt
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
- CIPHERTEXT_SIZE= 1088
- VECTOR_U_ENCODED_SIZE= 960
- U_COMPRESSION_FACTOR= 10
- V_COMPRESSION_FACTOR= 4
*/
static void
decrypt_12(Eurydice_slice secret_key, uint8_t *ciphertext,
           uint8_t ret[32U])
{
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 secret_as_ntt[3U];
    deserialize_secret_key_7b(secret_key, secret_as_ntt);
    /* Passing arrays by value in Rust generates a copy in C */
    libcrux_ml_kem_polynomial_PolynomialRingElement_f0 copy_of_secret_as_ntt[3U];
    memcpy(
        copy_of_secret_as_ntt, secret_as_ntt,
        (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    IndCpaPrivateKeyUnpacked_f8 secret_key_unpacked;
    memcpy(
        secret_key_unpacked.secret_as_ntt, copy_of_secret_as_ntt,
        (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0));
    uint8_t ret0[32U];
    decrypt_unpacked_6b(&secret_key_unpacked, ciphertext, ret0);
    memcpy(ret, ret0, (size_t)32U * sizeof(uint8_t));
}

/**
This function found in impl {(libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>)}
*/
/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.PRF_f1
with const generics
- K= 3
- LEN= 32
*/
static KRML_MUSTINLINE void
PRF_f1_2e(Eurydice_slice input, uint8_t ret[32U])
{
    PRF_7c(input, ret);
}

/**
A monomorphic instance of libcrux_ml_kem.ind_cca.decapsulate
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]],
libcrux_ml_kem_variant_MlKem with const generics
- K= 3
- SECRET_KEY_SIZE= 2400
- CPA_SECRET_KEY_SIZE= 1152
- PUBLIC_KEY_SIZE= 1184
- CIPHERTEXT_SIZE= 1088
- T_AS_NTT_ENCODED_SIZE= 1152
- C1_SIZE= 960
- C2_SIZE= 128
- VECTOR_U_COMPRESSION_FACTOR= 10
- VECTOR_V_COMPRESSION_FACTOR= 4
- C1_BLOCK_SIZE= 320
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
- ETA2= 2
- ETA2_RANDOMNESS_SIZE= 128
- IMPLICIT_REJECTION_HASH_INPUT_SIZE= 1120
*/
void
libcrux_ml_kem_ind_cca_decapsulate_1f(
    libcrux_ml_kem_types_MlKemPrivateKey_55 *private_key,
    libcrux_ml_kem_mlkem768_MlKem768Ciphertext *ciphertext, uint8_t ret[32U])
{
    Eurydice_slice_uint8_t_x2 uu____0 = Eurydice_slice_split_at(
        Eurydice_array_to_slice((size_t)2400U, private_key->value, uint8_t),
        (size_t)1152U, uint8_t, Eurydice_slice_uint8_t_x2);
    Eurydice_slice ind_cpa_secret_key = uu____0.fst;
    Eurydice_slice secret_key0 = uu____0.snd;
    Eurydice_slice_uint8_t_x2 uu____1 = Eurydice_slice_split_at(
        secret_key0, (size_t)1184U, uint8_t, Eurydice_slice_uint8_t_x2);
    Eurydice_slice ind_cpa_public_key = uu____1.fst;
    Eurydice_slice secret_key = uu____1.snd;
    Eurydice_slice_uint8_t_x2 uu____2 = Eurydice_slice_split_at(
        secret_key, LIBCRUX_ML_KEM_CONSTANTS_H_DIGEST_SIZE, uint8_t,
        Eurydice_slice_uint8_t_x2);
    Eurydice_slice ind_cpa_public_key_hash = uu____2.fst;
    Eurydice_slice implicit_rejection_value = uu____2.snd;
    uint8_t decrypted[32U];
    decrypt_12(ind_cpa_secret_key, ciphertext->value, decrypted);
    uint8_t to_hash0[64U];
    libcrux_ml_kem_utils_into_padded_array_6d(
        Eurydice_array_to_slice((size_t)32U, decrypted, uint8_t), to_hash0);
    Eurydice_slice_copy(
        Eurydice_array_to_subslice_from(
            (size_t)64U, to_hash0, LIBCRUX_ML_KEM_CONSTANTS_SHARED_SECRET_SIZE,
            uint8_t, size_t),
        ind_cpa_public_key_hash, uint8_t);
    uint8_t hashed[64U];
    G_f1_38(Eurydice_array_to_slice((size_t)64U, to_hash0, uint8_t), hashed);
    Eurydice_slice_uint8_t_x2 uu____3 = Eurydice_slice_split_at(
        Eurydice_array_to_slice((size_t)64U, hashed, uint8_t),
        LIBCRUX_ML_KEM_CONSTANTS_SHARED_SECRET_SIZE, uint8_t,
        Eurydice_slice_uint8_t_x2);
    Eurydice_slice shared_secret0 = uu____3.fst;
    Eurydice_slice pseudorandomness = uu____3.snd;
    uint8_t to_hash[1120U];
    libcrux_ml_kem_utils_into_padded_array_6d3(implicit_rejection_value, to_hash);
    Eurydice_slice uu____4 = Eurydice_array_to_subslice_from(
        (size_t)1120U, to_hash, LIBCRUX_ML_KEM_CONSTANTS_SHARED_SECRET_SIZE,
        uint8_t, size_t);
    Eurydice_slice_copy(uu____4, libcrux_ml_kem_types_as_ref_00_d80(ciphertext),
                        uint8_t);
    uint8_t implicit_rejection_shared_secret0[32U];
    PRF_f1_2e(Eurydice_array_to_slice((size_t)1120U, to_hash, uint8_t),
              implicit_rejection_shared_secret0);
    Eurydice_slice uu____5 = ind_cpa_public_key;
    /* Passing arrays by value in Rust generates a copy in C */
    uint8_t copy_of_decrypted[32U];
    memcpy(copy_of_decrypted, decrypted, (size_t)32U * sizeof(uint8_t));
    uint8_t expected_ciphertext[1088U];
    encrypt_7b(uu____5, copy_of_decrypted, pseudorandomness, expected_ciphertext);
    uint8_t implicit_rejection_shared_secret[32U];
    kdf_d8_c5(Eurydice_array_to_slice((size_t)32U,
                                      implicit_rejection_shared_secret0, uint8_t),
              implicit_rejection_shared_secret);
    uint8_t shared_secret[32U];
    kdf_d8_c5(shared_secret0, shared_secret);
    uint8_t ret0[32U];
    libcrux_ml_kem_constant_time_ops_compare_ciphertexts_select_shared_secret_in_constant_time(
        libcrux_ml_kem_types_as_ref_00_d80(ciphertext),
        Eurydice_array_to_slice((size_t)1088U, expected_ciphertext, uint8_t),
        Eurydice_array_to_slice((size_t)32U, shared_secret, uint8_t),
        Eurydice_array_to_slice((size_t)32U, implicit_rejection_shared_secret,
                                uint8_t),
        ret0);
    memcpy(ret, ret0, (size_t)32U * sizeof(uint8_t));
}
