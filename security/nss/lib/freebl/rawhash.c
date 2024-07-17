/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "nspr.h"
#include "hasht.h"
#include "blapi.h" /* below the line */
#include "secerr.h"

#define RawHashBase(ctxtype, mmm)                                                        \
    static void *                                                                        \
        RawHash_##mmm##_NewContext(void)                                                 \
    {                                                                                    \
        ctxtype *ctx = mmm##_NewContext();                                               \
        return ctx;                                                                      \
    }                                                                                    \
    static void                                                                          \
        RawHash_##mmm##_Begin(void *vctx)                                                \
    {                                                                                    \
        ctxtype *ctx = vctx;                                                             \
        mmm##_Begin(ctx);                                                                \
    }                                                                                    \
    static void                                                                          \
        RawHash_##mmm##_Update(void *vctx, const unsigned char *input, unsigned int len) \
    {                                                                                    \
        ctxtype *ctx = vctx;                                                             \
        mmm##_Update(ctx, input, len);                                                   \
    }                                                                                    \
    static void                                                                          \
        RawHash_##mmm##_End(void *vctx, unsigned char *digest,                           \
                            unsigned int *len, unsigned int maxLen)                      \
    {                                                                                    \
        ctxtype *ctx = vctx;                                                             \
        mmm##_End(ctx, digest, len, maxLen);                                             \
    }                                                                                    \
    static void                                                                          \
        RawHash_##mmm##_DestroyContext(void *vctx, PRBool freeit)                        \
    {                                                                                    \
        ctxtype *ctx = vctx;                                                             \
        mmm##_DestroyContext(ctx, freeit);                                               \
    }

RawHashBase(MD2Context, MD2);
RawHashBase(MD5Context, MD5);
RawHashBase(SHA1Context, SHA1);
RawHashBase(SHA224Context, SHA224);
RawHashBase(SHA256Context, SHA256);
RawHashBase(SHA384Context, SHA384);
RawHashBase(SHA512Context, SHA512);
RawHashBase(SHA3_224Context, SHA3_224);
RawHashBase(SHA3_256Context, SHA3_256);
RawHashBase(SHA3_384Context, SHA3_384);
RawHashBase(SHA3_512Context, SHA3_512);

#define RawHashExtra(ctxtype, mmm)                                     \
    static void                                                        \
        RawHash_##mmm##_EndRaw(void *vctx, unsigned char *digest,      \
                               unsigned int *len, unsigned int maxLen) \
    {                                                                  \
        ctxtype *ctx = vctx;                                           \
        mmm##_EndRaw(ctx, digest, len, maxLen);                        \
    }

RawHashExtra(MD5Context, MD5);
RawHashExtra(SHA1Context, SHA1);
RawHashExtra(SHA224Context, SHA224);
RawHashExtra(SHA256Context, SHA256);
RawHashExtra(SHA384Context, SHA384);
RawHashExtra(SHA512Context, SHA512);

static void *
null_hash_new_context(void)
{
    return NULL;
}

static void *
null_hash_clone_context(void *v)
{
    PORT_Assert(v == NULL);
    return NULL;
}

static void
null_hash_begin(void *v)
{
}

static void
null_hash_update(void *v, const unsigned char *input, unsigned int length)
{
}

static void
null_hash_end(void *v, unsigned char *output, unsigned int *outLen,
              unsigned int maxOut)
{
    *outLen = 0;
}

static void
null_hash_destroy_context(void *v, PRBool b)
{
    PORT_Assert(v == NULL);
}

const SECHashObject SECRawHashObjects[] = {
    { 0,
      null_hash_new_context,
      null_hash_clone_context,
      null_hash_destroy_context,
      null_hash_begin,
      null_hash_update,
      null_hash_end,
      0,
      HASH_AlgNULL,
      null_hash_end },
    {
        MD2_LENGTH,
        RawHash_MD2_NewContext,
        null_hash_clone_context,
        RawHash_MD2_DestroyContext,
        RawHash_MD2_Begin,
        RawHash_MD2_Update,
        RawHash_MD2_End,
        MD2_BLOCK_LENGTH,
        HASH_AlgMD2,
        NULL /* end_raw */
    },
    { MD5_LENGTH,
      RawHash_MD5_NewContext,
      null_hash_clone_context,
      RawHash_MD5_DestroyContext,
      RawHash_MD5_Begin,
      RawHash_MD5_Update,
      RawHash_MD5_End,
      MD5_BLOCK_LENGTH,
      HASH_AlgMD5,
      RawHash_MD5_EndRaw },
    { SHA1_LENGTH,
      RawHash_SHA1_NewContext,
      null_hash_clone_context,
      RawHash_SHA1_DestroyContext,
      RawHash_SHA1_Begin,
      RawHash_SHA1_Update,
      RawHash_SHA1_End,
      SHA1_BLOCK_LENGTH,
      HASH_AlgSHA1,
      RawHash_SHA1_EndRaw },
    { SHA256_LENGTH,
      RawHash_SHA256_NewContext,
      null_hash_clone_context,
      RawHash_SHA256_DestroyContext,
      RawHash_SHA256_Begin,
      RawHash_SHA256_Update,
      RawHash_SHA256_End,
      SHA256_BLOCK_LENGTH,
      HASH_AlgSHA256,
      RawHash_SHA256_EndRaw },
    { SHA384_LENGTH,
      RawHash_SHA384_NewContext,
      null_hash_clone_context,
      RawHash_SHA384_DestroyContext,
      RawHash_SHA384_Begin,
      RawHash_SHA384_Update,
      RawHash_SHA384_End,
      SHA384_BLOCK_LENGTH,
      HASH_AlgSHA384,
      RawHash_SHA384_EndRaw },
    { SHA512_LENGTH,
      RawHash_SHA512_NewContext,
      null_hash_clone_context,
      RawHash_SHA512_DestroyContext,
      RawHash_SHA512_Begin,
      RawHash_SHA512_Update,
      RawHash_SHA512_End,
      SHA512_BLOCK_LENGTH,
      HASH_AlgSHA512,
      RawHash_SHA512_EndRaw },
    { SHA224_LENGTH,
      RawHash_SHA224_NewContext,
      null_hash_clone_context,
      RawHash_SHA224_DestroyContext,
      RawHash_SHA224_Begin,
      RawHash_SHA224_Update,
      RawHash_SHA224_End,
      SHA224_BLOCK_LENGTH,
      HASH_AlgSHA224,
      RawHash_SHA224_EndRaw },
    { SHA3_224_LENGTH,
      RawHash_SHA3_224_NewContext,
      null_hash_clone_context,
      RawHash_SHA3_224_DestroyContext,
      RawHash_SHA3_224_Begin,
      RawHash_SHA3_224_Update,
      RawHash_SHA3_224_End,
      SHA3_224_BLOCK_LENGTH,
      HASH_AlgSHA3_224,
      NULL },
    { SHA3_256_LENGTH,
      RawHash_SHA3_256_NewContext,
      null_hash_clone_context,
      RawHash_SHA3_256_DestroyContext,
      RawHash_SHA3_256_Begin,
      RawHash_SHA3_256_Update,
      RawHash_SHA3_256_End,
      SHA3_256_BLOCK_LENGTH,
      HASH_AlgSHA3_256,
      NULL },
    { SHA3_384_LENGTH,
      RawHash_SHA3_384_NewContext,
      null_hash_clone_context,
      RawHash_SHA3_384_DestroyContext,
      RawHash_SHA3_384_Begin,
      RawHash_SHA3_384_Update,
      RawHash_SHA3_384_End,
      SHA3_384_BLOCK_LENGTH,
      HASH_AlgSHA3_384,
      NULL },
    { SHA3_512_LENGTH,
      RawHash_SHA3_512_NewContext,
      null_hash_clone_context,
      RawHash_SHA3_512_DestroyContext,
      RawHash_SHA3_512_Begin,
      RawHash_SHA3_512_Update,
      RawHash_SHA3_512_End,
      SHA3_512_BLOCK_LENGTH,
      HASH_AlgSHA3_512,
      NULL },
};

const SECHashObject *
HASH_GetRawHashObject(HASH_HashType hashType)
{
    if (hashType <= HASH_AlgNULL || hashType >= HASH_AlgTOTAL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }
    return &SECRawHashObjects[hashType];
}
