/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include <stdbool.h>

#include "blapi.h"
#include "secerr.h"
#include "secitem.h"

#include "kyber-pqcrystals-ref.h"
#include "kyber.h"
#include "verified/internal/libcrux_core.h"
#include "verified/libcrux_mlkem768_portable.h"
#include "verified/libcrux_mlkem768.h"

/* Consistency check between kyber-pqcrystals-ref.h and kyber.h */
PR_STATIC_ASSERT(KYBER768_PUBLIC_KEY_BYTES == pqcrystals_kyber768_PUBLICKEYBYTES);
PR_STATIC_ASSERT(KYBER768_PRIVATE_KEY_BYTES == pqcrystals_kyber768_SECRETKEYBYTES);
PR_STATIC_ASSERT(KYBER768_CIPHERTEXT_BYTES == pqcrystals_kyber768_CIPHERTEXTBYTES);
PR_STATIC_ASSERT(KYBER_SHARED_SECRET_BYTES == pqcrystals_kyber768_BYTES);
PR_STATIC_ASSERT(KYBER_KEYPAIR_COIN_BYTES == pqcrystals_kyber768_KEYPAIRCOINBYTES);
PR_STATIC_ASSERT(KYBER_ENC_COIN_BYTES == pqcrystals_kyber768_ENCCOINBYTES);

/* Consistency check between libcrux_mlkem768_portable.h and kyber.h */
PR_STATIC_ASSERT(KYBER768_PUBLIC_KEY_BYTES == LIBCRUX_ML_KEM_MLKEM768_CPA_PKE_PUBLIC_KEY_SIZE_768);
PR_STATIC_ASSERT(KYBER768_PRIVATE_KEY_BYTES == LIBCRUX_ML_KEM_MLKEM768_SECRET_KEY_SIZE_768);
PR_STATIC_ASSERT(KYBER768_CIPHERTEXT_BYTES == LIBCRUX_ML_KEM_MLKEM768_CPA_PKE_CIPHERTEXT_SIZE_768);
PR_STATIC_ASSERT(KYBER_SHARED_SECRET_BYTES == LIBCRUX_ML_KEM_CONSTANTS_SHARED_SECRET_SIZE);
PR_STATIC_ASSERT(KYBER_KEYPAIR_COIN_BYTES == 64);
PR_STATIC_ASSERT(KYBER_ENC_COIN_BYTES == 32);

static bool
valid_params(KyberParams params)
{
    switch (params) {
        case params_kyber768_round3:
        case params_kyber768_round3_test_mode:
        case params_ml_kem768:
        case params_ml_kem768_test_mode:
            return true;
        default:
            return false;
    }
}

static bool
valid_pubkey(KyberParams params, const SECItem *pubkey)
{
    switch (params) {
        case params_kyber768_round3:
        case params_kyber768_round3_test_mode:
        case params_ml_kem768:
        case params_ml_kem768_test_mode:
            return pubkey && pubkey->len == KYBER768_PUBLIC_KEY_BYTES;
        default:
            return false;
    }
}

static bool
valid_privkey(KyberParams params, const SECItem *privkey)
{
    switch (params) {
        case params_kyber768_round3:
        case params_kyber768_round3_test_mode:
        case params_ml_kem768:
        case params_ml_kem768_test_mode:
            return privkey && privkey->len == KYBER768_PRIVATE_KEY_BYTES;
        default:
            return false;
    }
}

static bool
valid_ciphertext(KyberParams params, const SECItem *ciphertext)
{
    switch (params) {
        case params_kyber768_round3:
        case params_kyber768_round3_test_mode:
        case params_ml_kem768:
        case params_ml_kem768_test_mode:
            return ciphertext && ciphertext->len == KYBER768_CIPHERTEXT_BYTES;
        default:
            return false;
    }
}

static bool
valid_secret(KyberParams params, const SECItem *secret)
{
    switch (params) {
        case params_kyber768_round3:
        case params_kyber768_round3_test_mode:
        case params_ml_kem768:
        case params_ml_kem768_test_mode:
            return secret && secret->len == KYBER_SHARED_SECRET_BYTES;
        default:
            return false;
    }
}

static bool
valid_keypair_seed(KyberParams params, const SECItem *seed)
{
    switch (params) {
        case params_kyber768_round3:
        case params_kyber768_round3_test_mode:
        case params_ml_kem768:
        case params_ml_kem768_test_mode:
            return !seed || seed->len == KYBER_KEYPAIR_COIN_BYTES;
        default:
            return false;
    }
}

static bool
valid_enc_seed(KyberParams params, const SECItem *seed)
{
    switch (params) {
        case params_kyber768_round3:
        case params_ml_kem768:
            return !seed;
        case params_kyber768_round3_test_mode:
        case params_ml_kem768_test_mode:
            return !seed || seed->len == KYBER_SHARED_SECRET_BYTES;
        default:
            return false;
    }
}

SECStatus
Kyber_NewKey(KyberParams params, const SECItem *keypair_seed, SECItem *privkey, SECItem *pubkey)
{
    if (!valid_params(params)) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        return SECFailure;
    }

    if (!(valid_keypair_seed(params, keypair_seed) && valid_privkey(params, privkey) && valid_pubkey(params, pubkey))) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    uint8_t randbuf[KYBER_KEYPAIR_COIN_BYTES];
    uint8_t *coins;
    if (keypair_seed) {
        coins = keypair_seed->data;
    } else {
        if (RNG_GenerateGlobalRandomBytes(randbuf, sizeof randbuf) != SECSuccess) {
            PORT_SetError(SEC_ERROR_NEED_RANDOM);
            return SECFailure;
        }
        coins = randbuf;
    }
    NSS_CLASSIFY(coins, KYBER_KEYPAIR_COIN_BYTES);
    if (params == params_kyber768_round3 || params == params_kyber768_round3_test_mode) {
        pqcrystals_kyber768_ref_keypair_derand(pubkey->data, privkey->data, coins);
    } else if (params == params_ml_kem768 || params == params_ml_kem768_test_mode) {
        libcrux_ml_kem_mlkem768_MlKem768KeyPair keys = libcrux_ml_kem_mlkem768_portable_generate_key_pair(coins);
        memcpy(pubkey->data, keys.pk.value, KYBER768_PUBLIC_KEY_BYTES);
        memcpy(privkey->data, keys.sk.value, KYBER768_PRIVATE_KEY_BYTES);
    } else {
        /* unreachable */
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    NSS_DECLASSIFY(pubkey->data, pubkey->len);
    return SECSuccess;
}

SECStatus
Kyber_Encapsulate(KyberParams params, const SECItem *enc_seed, const SECItem *pubkey, SECItem *ciphertext, SECItem *secret)
{
    if (!valid_params(params)) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        return SECFailure;
    }

    if (!(valid_enc_seed(params, enc_seed) && valid_pubkey(params, pubkey) && valid_ciphertext(params, ciphertext) && valid_secret(params, secret))) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    uint8_t randbuf[KYBER_ENC_COIN_BYTES];
    uint8_t *coins;
    if (enc_seed) {
        coins = enc_seed->data;
    } else {
        if (RNG_GenerateGlobalRandomBytes(randbuf, sizeof randbuf) != SECSuccess) {
            PORT_SetError(SEC_ERROR_NEED_RANDOM);
            return SECFailure;
        }
        coins = randbuf;
    }
    NSS_CLASSIFY(coins, KYBER_ENC_COIN_BYTES);
    if (params == params_kyber768_round3 || params == params_kyber768_round3_test_mode) {
        pqcrystals_kyber768_ref_enc_derand(ciphertext->data, secret->data, pubkey->data, coins);
    } else if (params == params_ml_kem768 || params == params_ml_kem768_test_mode) {
        libcrux_ml_kem_types_MlKemPublicKey_15 pk_value;
        memcpy(pk_value.value, pubkey->data, KYBER768_PUBLIC_KEY_BYTES);

        bool valid_pk = libcrux_ml_kem_mlkem768_portable_validate_public_key(&pk_value);
        if (!valid_pk) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return SECFailure;
        }

        tuple_3c encap = libcrux_ml_kem_mlkem768_portable_encapsulate(&pk_value, coins);
        memcpy(ciphertext->data, encap.fst.value, KYBER768_CIPHERTEXT_BYTES);
        memcpy(secret->data, encap.snd, KYBER_SHARED_SECRET_BYTES);
    } else {
        /* unreachable */
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    return SECSuccess;
}

SECStatus
Kyber_Decapsulate(KyberParams params, const SECItem *privkey, const SECItem *ciphertext, SECItem *secret)
{
    if (!valid_params(params)) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        return SECFailure;
    }

    if (!(valid_privkey(params, privkey) && valid_ciphertext(params, ciphertext) && valid_secret(params, secret))) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (params == params_kyber768_round3 || params == params_kyber768_round3_test_mode) {
        pqcrystals_kyber768_ref_dec(secret->data, ciphertext->data, privkey->data);
    } else if (params == params_ml_kem768 || params == params_ml_kem768_test_mode) {
        libcrux_ml_kem_types_MlKemPrivateKey_55 private_key;
        memcpy(private_key.value, privkey->data, KYBER768_PRIVATE_KEY_BYTES);

        libcrux_ml_kem_mlkem768_MlKem768Ciphertext cipher_text;
        memcpy(cipher_text.value, ciphertext->data, KYBER768_CIPHERTEXT_BYTES);

        bool valid = libcrux_ml_kem_mlkem768_portable_validate_private_key(&private_key, &cipher_text);
        if (!valid) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return SECFailure;
        }

        libcrux_ml_kem_mlkem768_portable_decapsulate(&private_key, &cipher_text, secret->data);
    } else {
        // unreachable
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    return SECSuccess;
}
