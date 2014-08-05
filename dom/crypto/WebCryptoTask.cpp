/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "pk11pub.h"
#include "cryptohi.h"
#include "secerr.h"
#include "ScopedNSSTypes.h"

#include "jsapi.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/AesKeyAlgorithm.h"
#include "mozilla/dom/CryptoBuffer.h"
#include "mozilla/dom/CryptoKey.h"
#include "mozilla/dom/CryptoKeyPair.h"
#include "mozilla/dom/HmacKeyAlgorithm.h"
#include "mozilla/dom/KeyAlgorithm.h"
#include "mozilla/dom/RsaHashedKeyAlgorithm.h"
#include "mozilla/dom/RsaKeyAlgorithm.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/WebCryptoCommon.h"
#include "mozilla/dom/WebCryptoTask.h"

namespace mozilla {
namespace dom {

// Pre-defined identifiers for telemetry histograms

enum TelemetryMethod {
  TM_ENCRYPT      = 0,
  TM_DECRYPT      = 1,
  TM_SIGN         = 2,
  TM_VERIFY       = 3,
  TM_DIGEST       = 4,
  TM_GENERATEKEY  = 5,
  TM_DERIVEKEY    = 6,
  TM_DERIVEBITS   = 7,
  TM_IMPORTKEY    = 8,
  TM_EXPORTKEY    = 9,
  TM_WRAPKEY      = 10,
  TM_UNWRAPKEY    = 11
};

enum TelemetryAlgorithm {
  // Please make additions at the end of the list,
  // to preserve comparability of histograms over time
  TA_UNKNOWN         = 0,
  // encrypt / decrypt
  TA_AES_CBC         = 1,
  TA_AES_CFB         = 2,
  TA_AES_CTR         = 3,
  TA_AES_GCM         = 4,
  TA_RSAES_PKCS1     = 5,
  TA_RSA_OAEP        = 6,
  // sign/verify
  TA_RSASSA_PKCS1    = 7,
  TA_RSA_PSS         = 8,
  TA_HMAC_SHA_1      = 9,
  TA_HMAC_SHA_224    = 10,
  TA_HMAC_SHA_256    = 11,
  TA_HMAC_SHA_384    = 12,
  TA_HMAC_SHA_512    = 13,
  // digest
  TA_SHA_1           = 14,
  TA_SHA_224         = 15,
  TA_SHA_256         = 16,
  TA_SHA_384         = 17,
  TA_SHA_512         = 18,
  // Later additions
  TA_AES_KW          = 19,
};

// Convenience functions for extracting / converting information

// OOM-safe CryptoBuffer initialization, suitable for constructors
#define ATTEMPT_BUFFER_INIT(dst, src) \
  if (!dst.Assign(src)) { \
    mEarlyRv = NS_ERROR_DOM_UNKNOWN_ERR; \
    return; \
  }

// OOM-safe CryptoBuffer-to-SECItem copy, suitable for DoCrypto
#define ATTEMPT_BUFFER_TO_SECITEM(dst, src) \
  dst = src.ToSECItem(); \
  if (!dst) { \
    return NS_ERROR_DOM_UNKNOWN_ERR; \
  }

// OOM-safe CryptoBuffer copy, suitable for DoCrypto
#define ATTEMPT_BUFFER_ASSIGN(dst, src) \
  if (!dst.Assign(src)) { \
    return NS_ERROR_DOM_UNKNOWN_ERR; \
  }

class ClearException
{
public:
  ClearException(JSContext* aCx)
    : mCx(aCx)
  {}

  ~ClearException()
  {
    JS_ClearPendingException(mCx);
  }

private:
  JSContext* mCx;
};

template<class OOS>
static nsresult
GetAlgorithmName(JSContext* aCx, const OOS& aAlgorithm, nsString& aName)
{
  ClearException ce(aCx);

  if (aAlgorithm.IsString()) {
    // If string, then treat as algorithm name
    aName.Assign(aAlgorithm.GetAsString());
  } else {
    // Coerce to algorithm and extract name
    JS::RootedValue value(aCx, JS::ObjectValue(*aAlgorithm.GetAsObject()));
    Algorithm alg;

    if (!alg.Init(aCx, value) || !alg.mName.WasPassed()) {
      return NS_ERROR_DOM_SYNTAX_ERR;
    }

    aName.Assign(alg.mName.Value());
  }

  // Normalize algorithm names.
  if (aName.EqualsIgnoreCase(WEBCRYPTO_ALG_AES_CBC)) {
    aName.AssignLiteral(WEBCRYPTO_ALG_AES_CBC);
  } else if (aName.EqualsIgnoreCase(WEBCRYPTO_ALG_AES_CTR)) {
    aName.AssignLiteral(WEBCRYPTO_ALG_AES_CTR);
  } else if (aName.EqualsIgnoreCase(WEBCRYPTO_ALG_AES_GCM)) {
    aName.AssignLiteral(WEBCRYPTO_ALG_AES_GCM);
  } else if (aName.EqualsIgnoreCase(WEBCRYPTO_ALG_AES_KW)) {
    aName.AssignLiteral(WEBCRYPTO_ALG_AES_KW);
  } else if (aName.EqualsIgnoreCase(WEBCRYPTO_ALG_SHA1)) {
    aName.AssignLiteral(WEBCRYPTO_ALG_SHA1);
  } else if (aName.EqualsIgnoreCase(WEBCRYPTO_ALG_SHA256)) {
    aName.AssignLiteral(WEBCRYPTO_ALG_SHA256);
  } else if (aName.EqualsIgnoreCase(WEBCRYPTO_ALG_SHA384)) {
    aName.AssignLiteral(WEBCRYPTO_ALG_SHA384);
  } else if (aName.EqualsIgnoreCase(WEBCRYPTO_ALG_SHA512)) {
    aName.AssignLiteral(WEBCRYPTO_ALG_SHA512);
  } else if (aName.EqualsIgnoreCase(WEBCRYPTO_ALG_HMAC)) {
    aName.AssignLiteral(WEBCRYPTO_ALG_HMAC);
  } else if (aName.EqualsIgnoreCase(WEBCRYPTO_ALG_PBKDF2)) {
    aName.AssignLiteral(WEBCRYPTO_ALG_PBKDF2);
  } else if (aName.EqualsIgnoreCase(WEBCRYPTO_ALG_RSAES_PKCS1)) {
    aName.AssignLiteral(WEBCRYPTO_ALG_RSAES_PKCS1);
  } else if (aName.EqualsIgnoreCase(WEBCRYPTO_ALG_RSASSA_PKCS1)) {
    aName.AssignLiteral(WEBCRYPTO_ALG_RSASSA_PKCS1);
  } else if (aName.EqualsIgnoreCase(WEBCRYPTO_ALG_RSA_OAEP)) {
    aName.AssignLiteral(WEBCRYPTO_ALG_RSA_OAEP);
  }

  return NS_OK;
}

template<class T, class OOS>
static nsresult
Coerce(JSContext* aCx, T& aTarget, const OOS& aAlgorithm)
{
  ClearException ce(aCx);

  if (!aAlgorithm.IsObject()) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  JS::RootedValue value(aCx, JS::ObjectValue(*aAlgorithm.GetAsObject()));
  if (!aTarget.Init(aCx, value)) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  return NS_OK;
}

inline size_t
MapHashAlgorithmNameToBlockSize(const nsString& aName)
{
  if (aName.EqualsLiteral(WEBCRYPTO_ALG_SHA1) ||
      aName.EqualsLiteral(WEBCRYPTO_ALG_SHA256)) {
    return 512;
  }

  if (aName.EqualsLiteral(WEBCRYPTO_ALG_SHA384) ||
      aName.EqualsLiteral(WEBCRYPTO_ALG_SHA512)) {
    return 1024;
  }

  return 0;
}

inline nsresult
GetKeySizeForAlgorithm(JSContext* aCx, const ObjectOrString& aAlgorithm,
                       size_t& aLength)
{
  aLength = 0;

  // Extract algorithm name
  nsString algName;
  if (NS_FAILED(GetAlgorithmName(aCx, aAlgorithm, algName))) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  // Read AES key length from given algorithm object.
  if (algName.EqualsLiteral(WEBCRYPTO_ALG_AES_CBC) ||
      algName.EqualsLiteral(WEBCRYPTO_ALG_AES_CTR) ||
      algName.EqualsLiteral(WEBCRYPTO_ALG_AES_GCM) ||
      algName.EqualsLiteral(WEBCRYPTO_ALG_AES_KW)) {
    RootedDictionary<AesKeyGenParams> params(aCx);
    if (NS_FAILED(Coerce(aCx, params, aAlgorithm)) ||
        !params.mLength.WasPassed()) {
      return NS_ERROR_DOM_SYNTAX_ERR;
    }

    size_t length = params.mLength.Value();
    if (length != 128 && length != 192 && length != 256) {
      return NS_ERROR_DOM_DATA_ERR;
    }

    aLength = length;
    return NS_OK;
  }

  // Determine HMAC key length as the block size of the given hash.
  if (algName.EqualsLiteral(WEBCRYPTO_ALG_HMAC)) {
    RootedDictionary<HmacImportParams> params(aCx);
    if (NS_FAILED(Coerce(aCx, params, aAlgorithm)) ||
        !params.mHash.WasPassed()) {
      return NS_ERROR_DOM_SYNTAX_ERR;
    }

    nsString hashName;
    if (NS_FAILED(GetAlgorithmName(aCx, params.mHash.Value(), hashName))) {
      return NS_ERROR_DOM_SYNTAX_ERR;
    }

    size_t length = MapHashAlgorithmNameToBlockSize(hashName);
    if (length == 0) {
      return NS_ERROR_DOM_SYNTAX_ERR;
    }

    aLength = length;
    return NS_OK;
  }

  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

// Helper function to clone data from an ArrayBuffer or ArrayBufferView object
inline bool
CloneData(JSContext* aCx, CryptoBuffer& aDst, JS::Handle<JSObject*> aSrc)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Try ArrayBuffer
  RootedTypedArray<ArrayBuffer> ab(aCx);
  if (ab.Init(aSrc)) {
    return !!aDst.Assign(ab);
  }

  // Try ArrayBufferView
  RootedTypedArray<ArrayBufferView> abv(aCx);
  if (abv.Init(aSrc)) {
    return !!aDst.Assign(abv);
  }

  return false;
}


// Implementation of WebCryptoTask methods

void
WebCryptoTask::FailWithError(nsresult aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_RESOLVED, false);

  // Blindly convert nsresult to DOMException
  // Individual tasks must ensure they pass the right values
  mResultPromise->MaybeReject(aRv);
  // Manually release mResultPromise while we're on the main thread
  mResultPromise = nullptr;
  Cleanup();
}

nsresult
WebCryptoTask::CalculateResult()
{
  MOZ_ASSERT(!NS_IsMainThread());

  if (NS_FAILED(mEarlyRv)) {
    return mEarlyRv;
  }

  if (isAlreadyShutDown()) {
    return NS_ERROR_DOM_UNKNOWN_ERR;
  }

  return DoCrypto();
}

void
WebCryptoTask::CallCallback(nsresult rv)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (NS_FAILED(rv)) {
    FailWithError(rv);
    return;
  }

  nsresult rv2 = AfterCrypto();
  if (NS_FAILED(rv2)) {
    FailWithError(rv2);
    return;
  }

  Resolve();
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_RESOLVED, true);

  // Manually release mResultPromise while we're on the main thread
  mResultPromise = nullptr;
  Cleanup();
}

// Some generic utility classes

class FailureTask : public WebCryptoTask
{
public:
  FailureTask(nsresult rv) {
    mEarlyRv = rv;
  }
};

class ReturnArrayBufferViewTask : public WebCryptoTask
{
protected:
  CryptoBuffer mResult;

private:
  // Returns mResult as an ArrayBufferView, or an error
  virtual void Resolve() MOZ_OVERRIDE
  {
    TypedArrayCreator<ArrayBuffer> ret(mResult);
    mResultPromise->MaybeResolve(ret);
  }
};

class DeferredData
{
public:
  template<class T>
  void SetData(const T& aData) {
    mDataIsSet = mData.Assign(aData);
  }

protected:
  DeferredData()
    : mDataIsSet(false)
  {}

  CryptoBuffer mData;
  bool mDataIsSet;
};

class AesTask : public ReturnArrayBufferViewTask,
                public DeferredData
{
public:
  AesTask(JSContext* aCx, const ObjectOrString& aAlgorithm,
          CryptoKey& aKey, bool aEncrypt)
    : mSymKey(aKey.GetSymKey())
    , mEncrypt(aEncrypt)
  {
    Init(aCx, aAlgorithm, aKey, aEncrypt);
  }

  AesTask(JSContext* aCx, const ObjectOrString& aAlgorithm,
          CryptoKey& aKey, const CryptoOperationData& aData,
          bool aEncrypt)
    : mSymKey(aKey.GetSymKey())
    , mEncrypt(aEncrypt)
  {
    Init(aCx, aAlgorithm, aKey, aEncrypt);
    SetData(aData);
  }

  void Init(JSContext* aCx, const ObjectOrString& aAlgorithm,
            CryptoKey& aKey, bool aEncrypt)
  {
    nsString algName;
    mEarlyRv = GetAlgorithmName(aCx, aAlgorithm, algName);
    if (NS_FAILED(mEarlyRv)) {
      return;
    }

    // Check that we got a reasonable key
    if ((mSymKey.Length() != 16) &&
        (mSymKey.Length() != 24) &&
        (mSymKey.Length() != 32))
    {
      mEarlyRv = NS_ERROR_DOM_DATA_ERR;
      return;
    }

    // Cache parameters depending on the specific algorithm
    TelemetryAlgorithm telemetryAlg;
    if (algName.EqualsLiteral(WEBCRYPTO_ALG_AES_CBC)) {
      mMechanism = CKM_AES_CBC_PAD;
      telemetryAlg = TA_AES_CBC;
      AesCbcParams params;
      nsresult rv = Coerce(aCx, params, aAlgorithm);
      if (NS_FAILED(rv) || !params.mIv.WasPassed()) {
        mEarlyRv = NS_ERROR_DOM_INVALID_ACCESS_ERR;
        return;
      }

      ATTEMPT_BUFFER_INIT(mIv, params.mIv.Value())
      if (mIv.Length() != 16) {
        mEarlyRv = NS_ERROR_DOM_DATA_ERR;
        return;
      }
    } else if (algName.EqualsLiteral(WEBCRYPTO_ALG_AES_CTR)) {
      mMechanism = CKM_AES_CTR;
      telemetryAlg = TA_AES_CTR;
      AesCtrParams params;
      nsresult rv = Coerce(aCx, params, aAlgorithm);
      if (NS_FAILED(rv) || !params.mCounter.WasPassed() ||
          !params.mLength.WasPassed()) {
        mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
        return;
      }

      ATTEMPT_BUFFER_INIT(mIv, params.mCounter.Value())
      if (mIv.Length() != 16) {
        mEarlyRv = NS_ERROR_DOM_DATA_ERR;
        return;
      }

      mCounterLength = params.mLength.Value();
    } else if (algName.EqualsLiteral(WEBCRYPTO_ALG_AES_GCM)) {
      mMechanism = CKM_AES_GCM;
      telemetryAlg = TA_AES_GCM;
      AesGcmParams params;
      nsresult rv = Coerce(aCx, params, aAlgorithm);
      if (NS_FAILED(rv) || !params.mIv.WasPassed()) {
        mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
        return;
      }

      ATTEMPT_BUFFER_INIT(mIv, params.mIv.Value())

      if (params.mAdditionalData.WasPassed()) {
        ATTEMPT_BUFFER_INIT(mAad, params.mAdditionalData.Value())
      }

      // 32, 64, 96, 104, 112, 120 or 128
      mTagLength = 128;
      if (params.mTagLength.WasPassed()) {
        mTagLength = params.mTagLength.Value();
        if ((mTagLength > 128) ||
            !(mTagLength == 32 || mTagLength == 64 ||
              (mTagLength >= 96 && mTagLength % 8 == 0))) {
          mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
          return;
        }
      }
    } else {
      mEarlyRv = NS_ERROR_DOM_NOT_SUPPORTED_ERR;
      return;
    }
    Telemetry::Accumulate(Telemetry::WEBCRYPTO_ALG, telemetryAlg);
  }

private:
  CK_MECHANISM_TYPE mMechanism;
  CryptoBuffer mSymKey;
  CryptoBuffer mIv;   // Initialization vector
  CryptoBuffer mAad;  // Additional Authenticated Data
  uint8_t mTagLength;
  uint8_t mCounterLength;
  bool mEncrypt;

  virtual nsresult DoCrypto() MOZ_OVERRIDE
  {
    nsresult rv;

    if (!mDataIsSet) {
      return NS_ERROR_DOM_OPERATION_ERR;
    }

    // Construct the parameters object depending on algorithm
    SECItem param;
    ScopedSECItem cbcParam;
    CK_AES_CTR_PARAMS ctrParams;
    CK_GCM_PARAMS gcmParams;
    switch (mMechanism) {
      case CKM_AES_CBC_PAD:
        ATTEMPT_BUFFER_TO_SECITEM(cbcParam, mIv);
        param = *cbcParam;
        break;
      case CKM_AES_CTR:
        ctrParams.ulCounterBits = mCounterLength;
        MOZ_ASSERT(mIv.Length() == 16);
        memcpy(&ctrParams.cb, mIv.Elements(), 16);
        param.type = siBuffer;
        param.data = (unsigned char*) &ctrParams;
        param.len  = sizeof(ctrParams);
        break;
      case CKM_AES_GCM:
        gcmParams.pIv = mIv.Elements();
        gcmParams.ulIvLen = mIv.Length();
        gcmParams.pAAD = mAad.Elements();
        gcmParams.ulAADLen = mAad.Length();
        gcmParams.ulTagBits = mTagLength;
        param.type = siBuffer;
        param.data = (unsigned char*) &gcmParams;
        param.len  = sizeof(gcmParams);
        break;
      default:
        return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    }

    // Import the key
    ScopedSECItem keyItem;
    ATTEMPT_BUFFER_TO_SECITEM(keyItem, mSymKey);
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    MOZ_ASSERT(slot.get());
    ScopedPK11SymKey symKey(PK11_ImportSymKey(slot, mMechanism, PK11_OriginUnwrap,
                                              CKA_ENCRYPT, keyItem.get(), nullptr));
    if (!symKey) {
      return NS_ERROR_DOM_INVALID_ACCESS_ERR;
    }

    // Initialize the output buffer (enough space for padding / a full tag)
    uint32_t dataLen = mData.Length();
    uint32_t maxLen = dataLen + 16;
    if (!mResult.SetLength(maxLen)) {
      return NS_ERROR_DOM_UNKNOWN_ERR;
    }
    uint32_t outLen = 0;

    // Perform the encryption/decryption
    if (mEncrypt) {
      rv = MapSECStatus(PK11_Encrypt(symKey.get(), mMechanism, &param,
                                     mResult.Elements(), &outLen, maxLen,
                                     mData.Elements(), mData.Length()));
    } else {
      rv = MapSECStatus(PK11_Decrypt(symKey.get(), mMechanism, &param,
                                     mResult.Elements(), &outLen, maxLen,
                                     mData.Elements(), mData.Length()));
    }
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_OPERATION_ERR);

    mResult.SetLength(outLen);
    return rv;
  }
};

// This class looks like an encrypt/decrypt task, like AesTask,
// but it is only exposed to wrapKey/unwrapKey, not encrypt/decrypt
class AesKwTask : public ReturnArrayBufferViewTask,
                  public DeferredData
{
public:
  AesKwTask(JSContext* aCx, const ObjectOrString& aAlgorithm,
            CryptoKey& aKey, bool aEncrypt)
    : mMechanism(CKM_NSS_AES_KEY_WRAP)
    , mSymKey(aKey.GetSymKey())
    , mEncrypt(aEncrypt)
  {
    Init(aCx, aAlgorithm, aKey, aEncrypt);
  }

  AesKwTask(JSContext* aCx, const ObjectOrString& aAlgorithm,
            CryptoKey& aKey, const CryptoOperationData& aData,
            bool aEncrypt)
    : mMechanism(CKM_NSS_AES_KEY_WRAP)
    , mSymKey(aKey.GetSymKey())
    , mEncrypt(aEncrypt)
  {
    Init(aCx, aAlgorithm, aKey, aEncrypt);
    SetData(aData);
  }

  void Init(JSContext* aCx, const ObjectOrString& aAlgorithm,
            CryptoKey& aKey, bool aEncrypt)
  {
    nsString algName;
    mEarlyRv = GetAlgorithmName(aCx, aAlgorithm, algName);
    if (NS_FAILED(mEarlyRv)) {
      return;
    }

    // Check that we got a reasonable key
    if ((mSymKey.Length() != 16) &&
        (mSymKey.Length() != 24) &&
        (mSymKey.Length() != 32))
    {
      mEarlyRv = NS_ERROR_DOM_DATA_ERR;
      return;
    }

    Telemetry::Accumulate(Telemetry::WEBCRYPTO_ALG, TA_AES_KW);
  }

private:
  CK_MECHANISM_TYPE mMechanism;
  CryptoBuffer mSymKey;
  bool mEncrypt;

  virtual nsresult DoCrypto() MOZ_OVERRIDE
  {
    nsresult rv;

    if (!mDataIsSet) {
      return NS_ERROR_DOM_OPERATION_ERR;
    }

    // Check that the input is a multiple of 64 bits long
    if (mData.Length() == 0 || mData.Length() % 8 != 0) {
      return NS_ERROR_DOM_DATA_ERR;
    }

    // Import the key
    ScopedSECItem keyItem;
    ATTEMPT_BUFFER_TO_SECITEM(keyItem, mSymKey);
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    MOZ_ASSERT(slot.get());
    ScopedPK11SymKey symKey(PK11_ImportSymKey(slot, mMechanism, PK11_OriginUnwrap,
                                              CKA_WRAP, keyItem.get(), nullptr));
    if (!symKey) {
      return NS_ERROR_DOM_INVALID_ACCESS_ERR;
    }

    // Import the data to a SECItem
    ScopedSECItem dataItem;
    ATTEMPT_BUFFER_TO_SECITEM(dataItem, mData);

    // Parameters for the fake keys
    CK_MECHANISM_TYPE fakeMechanism = CKM_SHA_1_HMAC;
    CK_ATTRIBUTE_TYPE fakeOperation = CKA_SIGN;

    if (mEncrypt) {
      // Import the data into a fake PK11SymKey structure
      ScopedPK11SymKey keyToWrap(PK11_ImportSymKey(slot, fakeMechanism,
                                                   PK11_OriginUnwrap, fakeOperation,
                                                   dataItem.get(), nullptr));
      if (!keyToWrap) {
        return NS_ERROR_DOM_OPERATION_ERR;
      }

      // Encrypt and return the wrapped key
      // AES-KW encryption results in a wrapped key 64 bits longer
      if (!mResult.SetLength(mData.Length() + 8)) {
        return NS_ERROR_DOM_OPERATION_ERR;
      }
      SECItem resultItem = {siBuffer, mResult.Elements(),
                            (unsigned int) mResult.Length()};
      rv = MapSECStatus(PK11_WrapSymKey(mMechanism, nullptr, symKey.get(),
                                        keyToWrap.get(), &resultItem));
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_OPERATION_ERR);
    } else {
      // Decrypt the ciphertext into a temporary PK11SymKey
      // Unwrapped key should be 64 bits shorter
      int keySize = mData.Length() - 8;
      ScopedPK11SymKey unwrappedKey(PK11_UnwrapSymKey(symKey, mMechanism, nullptr,
                                                 dataItem.get(), fakeMechanism,
                                                 fakeOperation, keySize));
      if (!unwrappedKey) {
        return NS_ERROR_DOM_OPERATION_ERR;
      }

      // Export the key to get the cleartext
      rv = MapSECStatus(PK11_ExtractKeyValue(unwrappedKey));
      if (NS_FAILED(rv)) {
        return NS_ERROR_DOM_UNKNOWN_ERR;
      }
      ATTEMPT_BUFFER_ASSIGN(mResult, PK11_GetKeyData(unwrappedKey));
    }

    return rv;
  }
};

class RsaesPkcs1Task : public ReturnArrayBufferViewTask,
                       public DeferredData
{
public:
  RsaesPkcs1Task(JSContext* aCx, const ObjectOrString& aAlgorithm,
                 CryptoKey& aKey, bool aEncrypt)
    : mPrivKey(aKey.GetPrivateKey())
    , mPubKey(aKey.GetPublicKey())
    , mEncrypt(aEncrypt)
  {
    Init(aCx, aAlgorithm, aKey, aEncrypt);
  }

  RsaesPkcs1Task(JSContext* aCx, const ObjectOrString& aAlgorithm,
                 CryptoKey& aKey, const CryptoOperationData& aData,
                 bool aEncrypt)
    : mPrivKey(aKey.GetPrivateKey())
    , mPubKey(aKey.GetPublicKey())
    , mEncrypt(aEncrypt)
  {
    Init(aCx, aAlgorithm, aKey, aEncrypt);
    SetData(aData);
  }

  void Init(JSContext* aCx, const ObjectOrString& aAlgorithm,
            CryptoKey& aKey, bool aEncrypt)
  {

    Telemetry::Accumulate(Telemetry::WEBCRYPTO_ALG, TA_RSAES_PKCS1);

    if (mEncrypt) {
      if (!mPubKey) {
        mEarlyRv = NS_ERROR_DOM_INVALID_ACCESS_ERR;
        return;
      }
      mStrength = SECKEY_PublicKeyStrength(mPubKey);
    } else {
      if (!mPrivKey) {
        mEarlyRv = NS_ERROR_DOM_INVALID_ACCESS_ERR;
        return;
      }
      mStrength = PK11_GetPrivateModulusLen(mPrivKey);
    }
  }

private:
  ScopedSECKEYPrivateKey mPrivKey;
  ScopedSECKEYPublicKey mPubKey;
  uint32_t mStrength;
  bool mEncrypt;

  virtual nsresult BeforeCrypto() MOZ_OVERRIDE
  {
    if (!mDataIsSet) {
      return NS_ERROR_DOM_OPERATION_ERR;
    }

    // Verify that the data input is not too big
    // (as required by PKCS#1 / RFC 3447, Section 7.2)
    // http://tools.ietf.org/html/rfc3447#section-7.2
    if (mEncrypt && mData.Length() > mStrength - 11) {
      return NS_ERROR_DOM_DATA_ERR;
    }

    return NS_OK;
  }

  virtual nsresult DoCrypto() MOZ_OVERRIDE
  {
    nsresult rv;

    // Ciphertext is an integer mod the modulus, so it will be
    // no longer than mStrength octets
    if (!mResult.SetLength(mStrength)) {
      return NS_ERROR_DOM_UNKNOWN_ERR;
    }

    if (mEncrypt) {
      rv = MapSECStatus(PK11_PubEncryptPKCS1(
              mPubKey.get(), mResult.Elements(),
              mData.Elements(), mData.Length(),
              nullptr));
    } else {
      uint32_t outLen;
      rv = MapSECStatus(PK11_PrivDecryptPKCS1(
              mPrivKey.get(), mResult.Elements(),
              &outLen, mResult.Length(),
              mData.Elements(), mData.Length()));
      mResult.SetLength(outLen);
    }

    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_OPERATION_ERR);
    return NS_OK;
  }
};

class RsaOaepTask : public ReturnArrayBufferViewTask,
                    public DeferredData
{
public:
  RsaOaepTask(JSContext* aCx, const ObjectOrString& aAlgorithm,
              CryptoKey& aKey, bool aEncrypt)
    : mPrivKey(aKey.GetPrivateKey())
    , mPubKey(aKey.GetPublicKey())
    , mEncrypt(aEncrypt)
  {
    Init(aCx, aAlgorithm, aKey, aEncrypt);
  }

  RsaOaepTask(JSContext* aCx, const ObjectOrString& aAlgorithm,
              CryptoKey& aKey, const CryptoOperationData& aData,
              bool aEncrypt)
    : mPrivKey(aKey.GetPrivateKey())
    , mPubKey(aKey.GetPublicKey())
    , mEncrypt(aEncrypt)
  {
    Init(aCx, aAlgorithm, aKey, aEncrypt);
    SetData(aData);
  }

  void Init(JSContext* aCx, const ObjectOrString& aAlgorithm,
            CryptoKey& aKey, bool aEncrypt)
  {
    Telemetry::Accumulate(Telemetry::WEBCRYPTO_ALG, TA_RSA_OAEP);

    if (mEncrypt) {
      if (!mPubKey) {
        mEarlyRv = NS_ERROR_DOM_INVALID_ACCESS_ERR;
        return;
      }
      mStrength = SECKEY_PublicKeyStrength(mPubKey);
    } else {
      if (!mPrivKey) {
        mEarlyRv = NS_ERROR_DOM_INVALID_ACCESS_ERR;
        return;
      }
      mStrength = PK11_GetPrivateModulusLen(mPrivKey);
    }

    RootedDictionary<RsaOaepParams> params(aCx);
    mEarlyRv = Coerce(aCx, params, aAlgorithm);
    if (NS_FAILED(mEarlyRv)) {
      mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
      return;
    }

    if (params.mLabel.WasPassed() && !params.mLabel.Value().IsNull()) {
      ATTEMPT_BUFFER_INIT(mLabel, params.mLabel.Value().Value());
    }
    // Otherwise mLabel remains the empty octet string, as intended

    // Look up the MGF based on the KeyAlgorithm.
    // static_cast is safe because we only get here if the algorithm name
    // is RSA-OAEP, and that only happens if we've constructed
    // an RsaHashedKeyAlgorithm.
    // TODO: Add As* methods to KeyAlgorithm (Bug 1036734)
    nsRefPtr<RsaHashedKeyAlgorithm> rsaAlg =
      static_cast<RsaHashedKeyAlgorithm*>(aKey.Algorithm());
    mHashMechanism = rsaAlg->Hash()->Mechanism();

    switch (mHashMechanism) {
      case CKM_SHA_1:
        mMgfMechanism = CKG_MGF1_SHA1; break;
      case CKM_SHA256:
        mMgfMechanism = CKG_MGF1_SHA256; break;
      case CKM_SHA384:
        mMgfMechanism = CKG_MGF1_SHA384; break;
      case CKM_SHA512:
        mMgfMechanism = CKG_MGF1_SHA512; break;
      default: {
        mEarlyRv = NS_ERROR_DOM_NOT_SUPPORTED_ERR;
        return;
      }
    }
  }

private:
  CK_MECHANISM_TYPE mHashMechanism;
  CK_MECHANISM_TYPE mMgfMechanism;
  ScopedSECKEYPrivateKey mPrivKey;
  ScopedSECKEYPublicKey mPubKey;
  CryptoBuffer mLabel;
  uint32_t mStrength;
  bool mEncrypt;

  virtual nsresult DoCrypto() MOZ_OVERRIDE
  {
    nsresult rv;

    if (!mDataIsSet) {
      return NS_ERROR_DOM_OPERATION_ERR;
    }

    // Ciphertext is an integer mod the modulus, so it will be
    // no longer than mStrength octets
    if (!mResult.SetLength(mStrength)) {
      return NS_ERROR_DOM_UNKNOWN_ERR;
    }

    CK_RSA_PKCS_OAEP_PARAMS oaepParams;
    oaepParams.source = CKZ_DATA_SPECIFIED;

    oaepParams.pSourceData = mLabel.Length() ? mLabel.Elements() : nullptr;
    oaepParams.ulSourceDataLen = mLabel.Length();

    oaepParams.mgf = mMgfMechanism;
    oaepParams.hashAlg = mHashMechanism;

    SECItem param;
    param.type = siBuffer;
    param.data = (unsigned char*) &oaepParams;
    param.len = sizeof(oaepParams);

    uint32_t outLen;
    if (mEncrypt) {
      // PK11_PubEncrypt() checks the plaintext's length and fails if it is too
      // long to encrypt, i.e. if it is longer than (k - 2hLen - 2) with 'k'
      // being the length in octets of the RSA modulus n and 'hLen' being the
      // output length in octets of the chosen hash function.
      // <https://tools.ietf.org/html/rfc3447#section-7.1>
      rv = MapSECStatus(PK11_PubEncrypt(
             mPubKey.get(), CKM_RSA_PKCS_OAEP, &param,
             mResult.Elements(), &outLen, mResult.Length(),
             mData.Elements(), mData.Length(), nullptr));
    } else {
      rv = MapSECStatus(PK11_PrivDecrypt(
             mPrivKey.get(), CKM_RSA_PKCS_OAEP, &param,
             mResult.Elements(), &outLen, mResult.Length(),
             mData.Elements(), mData.Length()));
    }
    mResult.SetLength(outLen);

    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_OPERATION_ERR);
    return NS_OK;
  }
};

class HmacTask : public WebCryptoTask
{
public:
  HmacTask(JSContext* aCx, const ObjectOrString& aAlgorithm,
           CryptoKey& aKey,
           const CryptoOperationData& aSignature,
           const CryptoOperationData& aData,
           bool aSign)
    : mMechanism(aKey.Algorithm()->Mechanism())
    , mSymKey(aKey.GetSymKey())
    , mSign(aSign)
  {
    ATTEMPT_BUFFER_INIT(mData, aData);
    if (!aSign) {
      ATTEMPT_BUFFER_INIT(mSignature, aSignature);
    }

    // Check that we got a symmetric key
    if (mSymKey.Length() == 0) {
      mEarlyRv = NS_ERROR_DOM_DATA_ERR;
      return;
    }

    TelemetryAlgorithm telemetryAlg;
    switch (mMechanism) {
      case CKM_SHA_1_HMAC:  telemetryAlg = TA_HMAC_SHA_1; break;
      case CKM_SHA224_HMAC: telemetryAlg = TA_HMAC_SHA_224; break;
      case CKM_SHA256_HMAC: telemetryAlg = TA_HMAC_SHA_256; break;
      case CKM_SHA384_HMAC: telemetryAlg = TA_HMAC_SHA_384; break;
      case CKM_SHA512_HMAC: telemetryAlg = TA_HMAC_SHA_512; break;
      default:              telemetryAlg = TA_UNKNOWN;
    }
    Telemetry::Accumulate(Telemetry::WEBCRYPTO_ALG, telemetryAlg);
  }

private:
  CK_MECHANISM_TYPE mMechanism;
  CryptoBuffer mSymKey;
  CryptoBuffer mData;
  CryptoBuffer mSignature;
  CryptoBuffer mResult;
  bool mSign;

  virtual nsresult DoCrypto() MOZ_OVERRIDE
  {
    // Initialize the output buffer
    if (!mResult.SetLength(HASH_LENGTH_MAX)) {
      return NS_ERROR_DOM_UNKNOWN_ERR;
    }
    uint32_t outLen;

    // Import the key
    ScopedSECItem keyItem;
    ATTEMPT_BUFFER_TO_SECITEM(keyItem, mSymKey);
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    MOZ_ASSERT(slot.get());
    ScopedPK11SymKey symKey(PK11_ImportSymKey(slot, mMechanism, PK11_OriginUnwrap,
                                              CKA_SIGN, keyItem.get(), nullptr));
    if (!symKey) {
      return NS_ERROR_DOM_INVALID_ACCESS_ERR;
    }

    // Compute the MAC
    SECItem param = { siBuffer, nullptr, 0 };
    ScopedPK11Context ctx(PK11_CreateContextBySymKey(mMechanism, CKA_SIGN,
                                                     symKey.get(), &param));
    if (!ctx.get()) {
      return NS_ERROR_DOM_OPERATION_ERR;
    }
    nsresult rv = MapSECStatus(PK11_DigestBegin(ctx.get()));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_OPERATION_ERR);
    rv = MapSECStatus(PK11_DigestOp(ctx.get(), mData.Elements(), mData.Length()));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_OPERATION_ERR);
    rv = MapSECStatus(PK11_DigestFinal(ctx.get(), mResult.Elements(),
                                       &outLen, HASH_LENGTH_MAX));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_OPERATION_ERR);

    mResult.SetLength(outLen);
    return rv;
  }

  // Returns mResult as an ArrayBufferView, or an error
  virtual void Resolve() MOZ_OVERRIDE
  {
    if (mSign) {
      // Return the computed MAC
      TypedArrayCreator<ArrayBuffer> ret(mResult);
      mResultPromise->MaybeResolve(ret);
    } else {
      // Compare the MAC to the provided signature
      // No truncation allowed
      bool equal = (mResult.Length() == mSignature.Length());
      if (equal) {
        int cmp = NSS_SecureMemcmp(mSignature.Elements(),
                                   mResult.Elements(),
                                   mSignature.Length());
        equal = (cmp == 0);
      }
      mResultPromise->MaybeResolve(equal);
    }
  }
};

class RsassaPkcs1Task : public WebCryptoTask
{
public:
  RsassaPkcs1Task(JSContext* aCx, const ObjectOrString& aAlgorithm,
                  CryptoKey& aKey,
                  const CryptoOperationData& aSignature,
                  const CryptoOperationData& aData,
                  bool aSign)
    : mOidTag(SEC_OID_UNKNOWN)
    , mPrivKey(aKey.GetPrivateKey())
    , mPubKey(aKey.GetPublicKey())
    , mSign(aSign)
    , mVerified(false)
  {
    Telemetry::Accumulate(Telemetry::WEBCRYPTO_ALG, TA_RSASSA_PKCS1);

    ATTEMPT_BUFFER_INIT(mData, aData);
    if (!aSign) {
      ATTEMPT_BUFFER_INIT(mSignature, aSignature);
    }

    // Look up the SECOidTag based on the KeyAlgorithm
    // static_cast is safe because we only get here if the algorithm name
    // is RSASSA-PKCS1-v1_5, and that only happens if we've constructed
    // an RsaHashedKeyAlgorithm
    nsRefPtr<RsaHashedKeyAlgorithm> rsaAlg = static_cast<RsaHashedKeyAlgorithm*>(aKey.Algorithm());
    nsRefPtr<KeyAlgorithm> hashAlg = rsaAlg->Hash();

    switch (hashAlg->Mechanism()) {
      case CKM_SHA_1:
        mOidTag = SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION; break;
      case CKM_SHA256:
        mOidTag = SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION; break;
      case CKM_SHA384:
        mOidTag = SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION; break;
      case CKM_SHA512:
        mOidTag = SEC_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION; break;
      default: {
        mEarlyRv = NS_ERROR_DOM_NOT_SUPPORTED_ERR;
        return;
      }
    }

    // Check that we have the appropriate key
    if ((mSign && !mPrivKey) || (!mSign && !mPubKey)) {
      mEarlyRv = NS_ERROR_DOM_INVALID_ACCESS_ERR;
      return;
    }
  }

private:
  SECOidTag mOidTag;
  ScopedSECKEYPrivateKey mPrivKey;
  ScopedSECKEYPublicKey mPubKey;
  CryptoBuffer mSignature;
  CryptoBuffer mData;
  bool mSign;
  bool mVerified;

  virtual nsresult DoCrypto() MOZ_OVERRIDE
  {
    nsresult rv;
    if (mSign) {
      ScopedSECItem signature((SECItem*) PORT_Alloc(sizeof(SECItem)));
      ScopedSGNContext ctx(SGN_NewContext(mOidTag, mPrivKey));
      if (!ctx) {
        return NS_ERROR_DOM_OPERATION_ERR;
      }

      rv = MapSECStatus(SGN_Begin(ctx));
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_OPERATION_ERR);

      rv = MapSECStatus(SGN_Update(ctx, mData.Elements(), mData.Length()));
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_OPERATION_ERR);

      rv = MapSECStatus(SGN_End(ctx, signature));
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_OPERATION_ERR);

      ATTEMPT_BUFFER_ASSIGN(mSignature, signature);
    } else {
      ScopedSECItem signature(mSignature.ToSECItem());
      if (!signature) {
        return NS_ERROR_DOM_UNKNOWN_ERR;
      }

      ScopedVFYContext ctx(VFY_CreateContext(mPubKey, signature,
                                             mOidTag, nullptr));
      if (!ctx) {
        int err = PORT_GetError();
        if (err == SEC_ERROR_BAD_SIGNATURE) {
          mVerified = false;
          return NS_OK;
        }
        return NS_ERROR_DOM_OPERATION_ERR;
      }

      rv = MapSECStatus(VFY_Begin(ctx));
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_OPERATION_ERR);

      rv = MapSECStatus(VFY_Update(ctx, mData.Elements(), mData.Length()));
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_OPERATION_ERR);

      rv = MapSECStatus(VFY_End(ctx));
      mVerified = NS_SUCCEEDED(rv);
    }

    return NS_OK;
  }

  virtual void Resolve() MOZ_OVERRIDE
  {
    if (mSign) {
      TypedArrayCreator<ArrayBuffer> ret(mSignature);
      mResultPromise->MaybeResolve(ret);
    } else {
      mResultPromise->MaybeResolve(mVerified);
    }
  }
};

class DigestTask : public ReturnArrayBufferViewTask
{
public:
  DigestTask(JSContext* aCx,
                   const ObjectOrString& aAlgorithm,
                   const CryptoOperationData& aData)
  {
    ATTEMPT_BUFFER_INIT(mData, aData);

    nsString algName;
    mEarlyRv = GetAlgorithmName(aCx, aAlgorithm, algName);
    if (NS_FAILED(mEarlyRv)) {
      mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
      return;
    }

    TelemetryAlgorithm telemetryAlg;
    if (algName.EqualsLiteral(WEBCRYPTO_ALG_SHA1))   {
      mOidTag = SEC_OID_SHA1;
      telemetryAlg = TA_SHA_1;
    } else if (algName.EqualsLiteral(WEBCRYPTO_ALG_SHA256)) {
      mOidTag = SEC_OID_SHA256;
      telemetryAlg = TA_SHA_224;
    } else if (algName.EqualsLiteral(WEBCRYPTO_ALG_SHA384)) {
      mOidTag = SEC_OID_SHA384;
      telemetryAlg = TA_SHA_256;
    } else if (algName.EqualsLiteral(WEBCRYPTO_ALG_SHA512)) {
      mOidTag = SEC_OID_SHA512;
      telemetryAlg = TA_SHA_384;
    } else {
      mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
      return;
    }
    Telemetry::Accumulate(Telemetry::WEBCRYPTO_ALG, telemetryAlg);
  }

private:
  SECOidTag mOidTag;
  CryptoBuffer mData;

  virtual nsresult DoCrypto() MOZ_OVERRIDE
  {
    // Resize the result buffer
    uint32_t hashLen = HASH_ResultLenByOidTag(mOidTag);
    if (!mResult.SetLength(hashLen)) {
      return NS_ERROR_DOM_UNKNOWN_ERR;
    }

    // Compute the hash
    nsresult rv = MapSECStatus(PK11_HashBuf(mOidTag, mResult.Elements(),
                                            mData.Elements(), mData.Length()));
    if (NS_FAILED(rv)) {
      return NS_ERROR_DOM_UNKNOWN_ERR;
    }

    return rv;
  }
};

class ImportKeyTask : public WebCryptoTask
{
public:
  void Init(JSContext* aCx,
      const nsAString& aFormat,
      const ObjectOrString& aAlgorithm, bool aExtractable,
      const Sequence<nsString>& aKeyUsages)
  {
    mFormat = aFormat;
    mDataIsSet = false;

    // Get the current global object from the context
    nsIGlobalObject *global = xpc::GetNativeForGlobal(JS::CurrentGlobalOrNull(aCx));
    if (!global) {
      mEarlyRv = NS_ERROR_DOM_UNKNOWN_ERR;
      return;
    }

    // This stuff pretty much always happens, so we'll do it here
    mKey = new CryptoKey(global);
    mKey->SetExtractable(aExtractable);
    mKey->ClearUsages();
    for (uint32_t i = 0; i < aKeyUsages.Length(); ++i) {
      mEarlyRv = mKey->AddUsage(aKeyUsages[i]);
      if (NS_FAILED(mEarlyRv)) {
        return;
      }
    }

    mEarlyRv = GetAlgorithmName(aCx, aAlgorithm, mAlgName);
    if (NS_FAILED(mEarlyRv)) {
      mEarlyRv = NS_ERROR_DOM_DATA_ERR;
      return;
    }
  }

  static bool JwkCompatible(const JsonWebKey& aJwk, const CryptoKey* aKey)
  {
    // Check 'ext'
    if (aKey->Extractable() &&
        aJwk.mExt.WasPassed() && !aJwk.mExt.Value()) {
      return false;
    }

    // Check 'alg'
    if (aJwk.mAlg.WasPassed() &&
        aJwk.mAlg.Value() != aKey->Algorithm()->ToJwkAlg()) {
      return false;
    }

    // Check 'key_ops'
    if (aJwk.mKey_ops.WasPassed()) {
      nsTArray<nsString> usages;
      aKey->GetUsages(usages);
      for (size_t i = 0; i < usages.Length(); ++i) {
        if (!aJwk.mKey_ops.Value().Contains(usages[i])) {
          return false;
        }
      }
    }

    // Individual algorithms may still have to check 'use'
    return true;
  }

  void SetKeyData(JSContext* aCx, JS::Handle<JSObject*> aKeyData) {
    // First try to treat as ArrayBuffer/ABV,
    // and if that fails, try to initialize a JWK
    if (CloneData(aCx, mKeyData, aKeyData)) {
      mDataIsJwk = false;

      if (mFormat.EqualsLiteral(WEBCRYPTO_KEY_FORMAT_JWK)) {
        SetJwkFromKeyData();
      }
    } else {
      JS::RootedValue value(aCx, JS::ObjectValue(*aKeyData));
      if (!mJwk.Init(aCx, value)) {
        return;
      }
      mDataIsJwk = true;
    }
  }

  void SetKeyData(const CryptoBuffer& aKeyData)
  {
    mKeyData = aKeyData;
    mDataIsJwk = false;

    if (mFormat.EqualsLiteral(WEBCRYPTO_KEY_FORMAT_JWK)) {
      SetJwkFromKeyData();
    }
  }

  void SetJwkFromKeyData()
  {
    nsDependentCSubstring utf8((const char*) mKeyData.Elements(),
                               (const char*) (mKeyData.Elements() +
                                              mKeyData.Length()));
    if (!IsUTF8(utf8)) {
      mEarlyRv = NS_ERROR_DOM_DATA_ERR;
      return;
    }

    nsString json = NS_ConvertUTF8toUTF16(utf8);
    if (!mJwk.Init(json)) {
      mEarlyRv = NS_ERROR_DOM_DATA_ERR;
      return;
    }
    mDataIsJwk = true;
  }

protected:
  nsString mFormat;
  nsRefPtr<CryptoKey> mKey;
  CryptoBuffer mKeyData;
  bool mDataIsSet;
  bool mDataIsJwk;
  JsonWebKey mJwk;
  nsString mAlgName;

private:
  virtual void Resolve() MOZ_OVERRIDE
  {
    mResultPromise->MaybeResolve(mKey);
  }

  virtual void Cleanup() MOZ_OVERRIDE
  {
    mKey = nullptr;
  }
};


class ImportSymmetricKeyTask : public ImportKeyTask
{
public:
  ImportSymmetricKeyTask(JSContext* aCx,
      const nsAString& aFormat,
      const ObjectOrString& aAlgorithm, bool aExtractable,
      const Sequence<nsString>& aKeyUsages)
  {
    Init(aCx, aFormat, aAlgorithm, aExtractable, aKeyUsages);
  }

  ImportSymmetricKeyTask(JSContext* aCx,
      const nsAString& aFormat, const JS::Handle<JSObject*> aKeyData,
      const ObjectOrString& aAlgorithm, bool aExtractable,
      const Sequence<nsString>& aKeyUsages)
  {
    Init(aCx, aFormat, aAlgorithm, aExtractable, aKeyUsages);
    if (NS_FAILED(mEarlyRv)) {
      return;
    }

    SetKeyData(aCx, aKeyData);
  }

  void Init(JSContext* aCx,
      const nsAString& aFormat,
      const ObjectOrString& aAlgorithm, bool aExtractable,
      const Sequence<nsString>& aKeyUsages)
  {
    ImportKeyTask::Init(aCx, aFormat, aAlgorithm, aExtractable, aKeyUsages);
    if (NS_FAILED(mEarlyRv)) {
      return;
    }

    // If this is an HMAC key, import the hash name
    if (mAlgName.EqualsLiteral(WEBCRYPTO_ALG_HMAC)) {
      RootedDictionary<HmacImportParams> params(aCx);
      mEarlyRv = Coerce(aCx, params, aAlgorithm);
      if (NS_FAILED(mEarlyRv) || !params.mHash.WasPassed()) {
        mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
        return;
      }
      mEarlyRv = GetAlgorithmName(aCx, params.mHash.Value(), mHashName);
      if (NS_FAILED(mEarlyRv)) {
        mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
        return;
      }
    }
  }

  virtual nsresult BeforeCrypto() MOZ_OVERRIDE
  {
    nsresult rv;

    // If we're doing a JWK import, import the key data
    if (mDataIsJwk) {
      if (!mJwk.mK.WasPassed()) {
        return NS_ERROR_DOM_DATA_ERR;
      }

      // Import the key material
      rv = mKeyData.FromJwkBase64(mJwk.mK.Value());
      if (NS_FAILED(rv)) {
        return NS_ERROR_DOM_DATA_ERR;
      }
    }

    // Check that we have valid key data.
    if (mKeyData.Length() == 0) {
      return NS_ERROR_DOM_DATA_ERR;
    }

    // Construct an appropriate KeyAlorithm,
    // and verify that usages are appropriate
    nsRefPtr<KeyAlgorithm> algorithm;
    nsIGlobalObject* global = mKey->GetParentObject();
    uint32_t length = 8 * mKeyData.Length(); // bytes to bits
    if (mAlgName.EqualsLiteral(WEBCRYPTO_ALG_AES_CBC) ||
        mAlgName.EqualsLiteral(WEBCRYPTO_ALG_AES_CTR) ||
        mAlgName.EqualsLiteral(WEBCRYPTO_ALG_AES_GCM) ||
        mAlgName.EqualsLiteral(WEBCRYPTO_ALG_AES_KW)) {
      if (mKey->HasUsageOtherThan(CryptoKey::ENCRYPT | CryptoKey::DECRYPT |
                                  CryptoKey::WRAPKEY | CryptoKey::UNWRAPKEY)) {
        return NS_ERROR_DOM_DATA_ERR;
      }

      if (mAlgName.EqualsLiteral(WEBCRYPTO_ALG_AES_KW) &&
          mKey->HasUsageOtherThan(CryptoKey::WRAPKEY | CryptoKey::UNWRAPKEY)) {
        return NS_ERROR_DOM_DATA_ERR;
      }

      if ( (length != 128) && (length != 192) && (length != 256) ) {
        return NS_ERROR_DOM_DATA_ERR;
      }
      algorithm = new AesKeyAlgorithm(global, mAlgName, length);

      if (mDataIsJwk && mJwk.mUse.WasPassed() &&
          !mJwk.mUse.Value().EqualsLiteral(JWK_USE_ENC)) {
        return NS_ERROR_DOM_DATA_ERR;
      }
    } else if (mAlgName.EqualsLiteral(WEBCRYPTO_ALG_PBKDF2)) {
      if (mKey->HasUsageOtherThan(CryptoKey::DERIVEKEY)) {
        return NS_ERROR_DOM_DATA_ERR;
      }
      algorithm = new BasicSymmetricKeyAlgorithm(global, mAlgName, length);

      if (mDataIsJwk && mJwk.mUse.WasPassed()) {
        // There is not a 'use' value consistent with PBKDF
        return NS_ERROR_DOM_DATA_ERR;
      };
    } else if (mAlgName.EqualsLiteral(WEBCRYPTO_ALG_HMAC)) {
      if (mKey->HasUsageOtherThan(CryptoKey::SIGN | CryptoKey::VERIFY)) {
        return NS_ERROR_DOM_DATA_ERR;
      }

      algorithm = new HmacKeyAlgorithm(global, mAlgName, length, mHashName);
      if (algorithm->Mechanism() == UNKNOWN_CK_MECHANISM) {
        return NS_ERROR_DOM_SYNTAX_ERR;
      }

      if (mDataIsJwk && mJwk.mUse.WasPassed() &&
          !mJwk.mUse.Value().EqualsLiteral(JWK_USE_SIG)) {
        return NS_ERROR_DOM_DATA_ERR;
      }
    } else {
      return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    }

    mKey->SetAlgorithm(algorithm);
    mKey->SetSymKey(mKeyData);
    mKey->SetType(CryptoKey::SECRET);
    mEarlyComplete = true;
    return NS_OK;
  }

  nsresult AfterCrypto() MOZ_OVERRIDE
  {
    if (mDataIsJwk && !JwkCompatible(mJwk, mKey)) {
      return NS_ERROR_DOM_DATA_ERR;
    }
    return NS_OK;
  }

private:
  nsString mHashName;
};

class ImportRsaKeyTask : public ImportKeyTask
{
public:
  ImportRsaKeyTask(JSContext* aCx,
      const nsAString& aFormat,
      const ObjectOrString& aAlgorithm, bool aExtractable,
      const Sequence<nsString>& aKeyUsages)
  {
    Init(aCx, aFormat, aAlgorithm, aExtractable, aKeyUsages);
  }

  ImportRsaKeyTask(JSContext* aCx,
      const nsAString& aFormat, JS::Handle<JSObject*> aKeyData,
      const ObjectOrString& aAlgorithm, bool aExtractable,
      const Sequence<nsString>& aKeyUsages)
  {
    Init(aCx, aFormat, aAlgorithm, aExtractable, aKeyUsages);
    if (NS_FAILED(mEarlyRv)) {
      return;
    }

    SetKeyData(aCx, aKeyData);
  }

  void Init(JSContext* aCx,
      const nsAString& aFormat,
      const ObjectOrString& aAlgorithm, bool aExtractable,
      const Sequence<nsString>& aKeyUsages)
  {
    ImportKeyTask::Init(aCx, aFormat, aAlgorithm, aExtractable, aKeyUsages);
    if (NS_FAILED(mEarlyRv)) {
      return;
    }

    // If this is RSA with a hash, cache the hash name
    if (mAlgName.EqualsLiteral(WEBCRYPTO_ALG_RSASSA_PKCS1) ||
        mAlgName.EqualsLiteral(WEBCRYPTO_ALG_RSA_OAEP)) {
      RootedDictionary<RsaHashedImportParams> params(aCx);
      mEarlyRv = Coerce(aCx, params, aAlgorithm);
      if (NS_FAILED(mEarlyRv) || !params.mHash.WasPassed()) {
        mEarlyRv = NS_ERROR_DOM_DATA_ERR;
        return;
      }

      mEarlyRv = GetAlgorithmName(aCx, params.mHash.Value(), mHashName);
      if (NS_FAILED(mEarlyRv)) {
        mEarlyRv = NS_ERROR_DOM_DATA_ERR;
        return;
      }
    }
  }

private:
  nsString mHashName;
  uint32_t mModulusLength;
  CryptoBuffer mPublicExponent;

  virtual nsresult DoCrypto() MOZ_OVERRIDE
  {
    nsNSSShutDownPreventionLock locker;

    // Import the key data itself
    ScopedSECKEYPublicKey pubKey;
    ScopedSECKEYPrivateKey privKey;
    if (mFormat.EqualsLiteral(WEBCRYPTO_KEY_FORMAT_SPKI) ||
        (mFormat.EqualsLiteral(WEBCRYPTO_KEY_FORMAT_JWK) &&
         !mJwk.mD.WasPassed())) {
      // Public key import
      if (mFormat.EqualsLiteral(WEBCRYPTO_KEY_FORMAT_SPKI)) {
        pubKey = CryptoKey::PublicKeyFromSpki(mKeyData, locker);
      } else {
        pubKey = CryptoKey::PublicKeyFromJwk(mJwk, locker);
      }

      if (!pubKey) {
        return NS_ERROR_DOM_DATA_ERR;
      }

      mKey->SetPublicKey(pubKey.get());
      mKey->SetType(CryptoKey::PUBLIC);
    } else if (mFormat.EqualsLiteral(WEBCRYPTO_KEY_FORMAT_PKCS8) ||
        (mFormat.EqualsLiteral(WEBCRYPTO_KEY_FORMAT_JWK) &&
         mJwk.mD.WasPassed())) {
      // Private key import
      if (mFormat.EqualsLiteral(WEBCRYPTO_KEY_FORMAT_PKCS8)) {
        privKey = CryptoKey::PrivateKeyFromPkcs8(mKeyData, locker);
      } else {
        privKey = CryptoKey::PrivateKeyFromJwk(mJwk, locker);
      }

      if (!privKey) {
        return NS_ERROR_DOM_DATA_ERR;
      }

      mKey->SetPrivateKey(privKey.get());
      mKey->SetType(CryptoKey::PRIVATE);
      pubKey = SECKEY_ConvertToPublicKey(privKey.get());
      if (!pubKey) {
        return NS_ERROR_DOM_UNKNOWN_ERR;
      }
    } else {
      // Invalid key format
      return NS_ERROR_DOM_SYNTAX_ERR;
    }

    // Extract relevant information from the public key
    mModulusLength = 8 * pubKey->u.rsa.modulus.len;
    mPublicExponent.Assign(&pubKey->u.rsa.publicExponent);

    return NS_OK;
  }

  virtual nsresult AfterCrypto() MOZ_OVERRIDE
  {
    // Check permissions for the requested operation
    nsIGlobalObject* global = mKey->GetParentObject();
    if (mAlgName.EqualsLiteral(WEBCRYPTO_ALG_RSAES_PKCS1) ||
        mAlgName.EqualsLiteral(WEBCRYPTO_ALG_RSA_OAEP)) {
      if ((mKey->GetKeyType() == CryptoKey::PUBLIC &&
           mKey->HasUsageOtherThan(CryptoKey::ENCRYPT | CryptoKey::WRAPKEY)) ||
          (mKey->GetKeyType() == CryptoKey::PRIVATE &&
           mKey->HasUsageOtherThan(CryptoKey::DECRYPT | CryptoKey::UNWRAPKEY))) {
        return NS_ERROR_DOM_DATA_ERR;
      }
    } else if (mAlgName.EqualsLiteral(WEBCRYPTO_ALG_RSASSA_PKCS1)) {
      if ((mKey->GetKeyType() == CryptoKey::PUBLIC &&
           mKey->HasUsageOtherThan(CryptoKey::VERIFY)) ||
          (mKey->GetKeyType() == CryptoKey::PRIVATE &&
           mKey->HasUsageOtherThan(CryptoKey::SIGN))) {
        return NS_ERROR_DOM_DATA_ERR;
      }
    }

    // Construct an appropriate KeyAlgorithm
    if (mAlgName.EqualsLiteral(WEBCRYPTO_ALG_RSAES_PKCS1)) {
      mKey->SetAlgorithm(new RsaKeyAlgorithm(global, mAlgName, mModulusLength, mPublicExponent));
    } else if (mAlgName.EqualsLiteral(WEBCRYPTO_ALG_RSASSA_PKCS1) ||
               mAlgName.EqualsLiteral(WEBCRYPTO_ALG_RSA_OAEP)) {
      nsRefPtr<RsaHashedKeyAlgorithm> algorithm =
        new RsaHashedKeyAlgorithm(global, mAlgName,
                                  mModulusLength, mPublicExponent, mHashName);
      if (algorithm->Mechanism() == UNKNOWN_CK_MECHANISM) {
        return NS_ERROR_DOM_SYNTAX_ERR;
      }

      if (algorithm->Hash()->Mechanism() == UNKNOWN_CK_MECHANISM) {
        return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
      }
      mKey->SetAlgorithm(algorithm);
    }

    if (mDataIsJwk && !JwkCompatible(mJwk, mKey)) {
      return NS_ERROR_DOM_DATA_ERR;
    }

    return NS_OK;
  }
};

class ExportKeyTask : public WebCryptoTask
{
public:
  ExportKeyTask(const nsAString& aFormat, CryptoKey& aKey)
    : mFormat(aFormat)
    , mSymKey(aKey.GetSymKey())
    , mPrivateKey(aKey.GetPrivateKey())
    , mPublicKey(aKey.GetPublicKey())
    , mKeyType(aKey.GetKeyType())
    , mExtractable(aKey.Extractable())
    , mAlg(aKey.Algorithm()->ToJwkAlg())
  {
    if (!aKey.Extractable()) {
      mEarlyRv = NS_ERROR_DOM_INVALID_ACCESS_ERR;
      return;
    }

    aKey.GetUsages(mKeyUsages);
  }


protected:
  nsString mFormat;
  CryptoBuffer mSymKey;
  ScopedSECKEYPrivateKey mPrivateKey;
  ScopedSECKEYPublicKey mPublicKey;
  CryptoKey::KeyType mKeyType;
  bool mExtractable;
  nsString mAlg;
  nsTArray<nsString> mKeyUsages;
  CryptoBuffer mResult;
  JsonWebKey mJwk;

private:
  virtual void ReleaseNSSResources() MOZ_OVERRIDE
  {
    mPrivateKey.dispose();
    mPublicKey.dispose();
  }

  virtual nsresult DoCrypto() MOZ_OVERRIDE
  {
    nsNSSShutDownPreventionLock locker;

    if (mFormat.EqualsLiteral(WEBCRYPTO_KEY_FORMAT_RAW)) {
      mResult = mSymKey;
      if (mResult.Length() == 0) {
        return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
      }

      return NS_OK;
    } else if (mFormat.EqualsLiteral(WEBCRYPTO_KEY_FORMAT_PKCS8)) {
      if (!mPrivateKey) {
        return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
      }

      switch (mPrivateKey->keyType) {
        case rsaKey:
          CryptoKey::PrivateKeyToPkcs8(mPrivateKey.get(), mResult, locker);
          return NS_OK;
        default:
          return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
      }
    } else if (mFormat.EqualsLiteral(WEBCRYPTO_KEY_FORMAT_SPKI)) {
      if (!mPublicKey) {
        return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
      }

      return CryptoKey::PublicKeyToSpki(mPublicKey.get(), mResult, locker);
    } else if (mFormat.EqualsLiteral(WEBCRYPTO_KEY_FORMAT_JWK)) {
      if (mKeyType == CryptoKey::SECRET) {
        nsString k;
        nsresult rv = mSymKey.ToJwkBase64(k);
        if (NS_FAILED(rv)) {
          return NS_ERROR_DOM_OPERATION_ERR;
        }
        mJwk.mK.Construct(k);
        mJwk.mKty.Construct(NS_LITERAL_STRING(JWK_TYPE_SYMMETRIC));
      } else if (mKeyType == CryptoKey::PUBLIC) {
        if (!mPublicKey) {
          return NS_ERROR_DOM_UNKNOWN_ERR;
        }

        nsresult rv = CryptoKey::PublicKeyToJwk(mPublicKey, mJwk, locker);
        if (NS_FAILED(rv)) {
          return NS_ERROR_DOM_OPERATION_ERR;
        }
      } else if (mKeyType == CryptoKey::PRIVATE) {
        if (!mPrivateKey) {
          return NS_ERROR_DOM_UNKNOWN_ERR;
        }

        nsresult rv = CryptoKey::PrivateKeyToJwk(mPrivateKey, mJwk, locker);
        if (NS_FAILED(rv)) {
          return NS_ERROR_DOM_OPERATION_ERR;
        }
      }

      if (!mAlg.IsEmpty()) {
        mJwk.mAlg.Construct(mAlg);
      }

      mJwk.mExt.Construct(mExtractable);

      if (!mKeyUsages.IsEmpty()) {
        mJwk.mKey_ops.Construct();
        mJwk.mKey_ops.Value().AppendElements(mKeyUsages);
      }

      return NS_OK;
    }

    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  // Returns mResult as an ArrayBufferView or JWK, as appropriate
  virtual void Resolve() MOZ_OVERRIDE
  {
    if (mFormat.EqualsLiteral(WEBCRYPTO_KEY_FORMAT_JWK)) {
      mResultPromise->MaybeResolve(mJwk);
      return;
    }

    TypedArrayCreator<ArrayBuffer> ret(mResult);
    mResultPromise->MaybeResolve(ret);
  }
};

class GenerateSymmetricKeyTask : public WebCryptoTask
{
public:
  GenerateSymmetricKeyTask(JSContext* aCx,
      const ObjectOrString& aAlgorithm, bool aExtractable,
      const Sequence<nsString>& aKeyUsages)
  {
    nsIGlobalObject* global = xpc::GetNativeForGlobal(JS::CurrentGlobalOrNull(aCx));
    if (!global) {
      mEarlyRv = NS_ERROR_DOM_UNKNOWN_ERR;
      return;
    }

    // Create an empty key and set easy attributes
    mKey = new CryptoKey(global);
    mKey->SetExtractable(aExtractable);
    mKey->SetType(CryptoKey::SECRET);

    // Extract algorithm name
    nsString algName;
    mEarlyRv = GetAlgorithmName(aCx, aAlgorithm, algName);
    if (NS_FAILED(mEarlyRv)) {
      mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
      return;
    }

    // Construct an appropriate KeyAlorithm
    nsRefPtr<KeyAlgorithm> algorithm;
    uint32_t allowedUsages = 0;
    if (algName.EqualsLiteral(WEBCRYPTO_ALG_AES_CBC) ||
        algName.EqualsLiteral(WEBCRYPTO_ALG_AES_CTR) ||
        algName.EqualsLiteral(WEBCRYPTO_ALG_AES_GCM) ||
        algName.EqualsLiteral(WEBCRYPTO_ALG_AES_KW)) {
      mEarlyRv = GetKeySizeForAlgorithm(aCx, aAlgorithm, mLength);
      if (NS_FAILED(mEarlyRv)) {
        return;
      }
      algorithm = new AesKeyAlgorithm(global, algName, mLength);
      allowedUsages = CryptoKey::ENCRYPT | CryptoKey::DECRYPT |
                      CryptoKey::WRAPKEY | CryptoKey::UNWRAPKEY;
    } else if (algName.EqualsLiteral(WEBCRYPTO_ALG_HMAC)) {
      RootedDictionary<HmacKeyGenParams> params(aCx);
      mEarlyRv = Coerce(aCx, params, aAlgorithm);
      if (NS_FAILED(mEarlyRv) || !params.mHash.WasPassed()) {
        mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
        return;
      }

      nsString hashName;
      mEarlyRv = GetAlgorithmName(aCx, params.mHash.Value(), hashName);
      if (NS_FAILED(mEarlyRv)) {
        mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
        return;
      }

      if (params.mLength.WasPassed()) {
        mLength = params.mLength.Value();
      } else {
        mLength = MapHashAlgorithmNameToBlockSize(hashName);
      }

      if (mLength == 0) {
        mEarlyRv = NS_ERROR_DOM_DATA_ERR;
        return;
      }

      algorithm = new HmacKeyAlgorithm(global, algName, mLength, hashName);
      allowedUsages = CryptoKey::SIGN | CryptoKey::VERIFY;
    } else {
      mEarlyRv = NS_ERROR_DOM_NOT_SUPPORTED_ERR;
      return;
    }

    // Add key usages
    mKey->ClearUsages();
    for (uint32_t i = 0; i < aKeyUsages.Length(); ++i) {
      mEarlyRv = mKey->AddUsageIntersecting(aKeyUsages[i], allowedUsages);
      if (NS_FAILED(mEarlyRv)) {
        return;
      }
    }

    mLength = mLength >> 3; // bits to bytes
    mMechanism = algorithm->Mechanism();
    mKey->SetAlgorithm(algorithm);
    // SetSymKey done in Resolve, after we've done the keygen
  }

private:
  nsRefPtr<CryptoKey> mKey;
  size_t mLength;
  CK_MECHANISM_TYPE mMechanism;
  CryptoBuffer mKeyData;

  virtual nsresult DoCrypto() MOZ_OVERRIDE
  {
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    MOZ_ASSERT(slot.get());

    ScopedPK11SymKey symKey(PK11_KeyGen(slot.get(), mMechanism, nullptr,
                                        mLength, nullptr));
    if (!symKey) {
      return NS_ERROR_DOM_UNKNOWN_ERR;
    }

    nsresult rv = MapSECStatus(PK11_ExtractKeyValue(symKey));
    if (NS_FAILED(rv)) {
      return NS_ERROR_DOM_UNKNOWN_ERR;
    }

    // This doesn't leak, because the SECItem* returned by PK11_GetKeyData
    // just refers to a buffer managed by symKey.  The assignment copies the
    // data, so mKeyData manages one copy, while symKey manages another.
    ATTEMPT_BUFFER_ASSIGN(mKeyData, PK11_GetKeyData(symKey));
    return NS_OK;
  }

  virtual void Resolve() {
    mKey->SetSymKey(mKeyData);
    mResultPromise->MaybeResolve(mKey);
  }

  virtual void Cleanup() {
    mKey = nullptr;
  }
};

class GenerateAsymmetricKeyTask : public WebCryptoTask
{
public:
  GenerateAsymmetricKeyTask(JSContext* aCx,
      const ObjectOrString& aAlgorithm, bool aExtractable,
      const Sequence<nsString>& aKeyUsages)
  {
    nsIGlobalObject* global = xpc::GetNativeForGlobal(JS::CurrentGlobalOrNull(aCx));
    if (!global) {
      mEarlyRv = NS_ERROR_DOM_UNKNOWN_ERR;
      return;
    }

    // Create an empty key and set easy attributes
    mKeyPair = new CryptoKeyPair(global);

    // Extract algorithm name
    nsString algName;
    mEarlyRv = GetAlgorithmName(aCx, aAlgorithm, algName);
    if (NS_FAILED(mEarlyRv)) {
      mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
      return;
    }

    // Construct an appropriate KeyAlorithm
    KeyAlgorithm* algorithm;
    uint32_t privateAllowedUsages = 0, publicAllowedUsages = 0;
    if (algName.EqualsLiteral(WEBCRYPTO_ALG_RSASSA_PKCS1) ||
        algName.EqualsLiteral(WEBCRYPTO_ALG_RSA_OAEP)) {
      RootedDictionary<RsaHashedKeyGenParams> params(aCx);
      mEarlyRv = Coerce(aCx, params, aAlgorithm);
      if (NS_FAILED(mEarlyRv) || !params.mModulusLength.WasPassed() ||
          !params.mPublicExponent.WasPassed() ||
          !params.mHash.WasPassed()) {
        mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
        return;
      }

      // Pull relevant info
      uint32_t modulusLength = params.mModulusLength.Value();
      CryptoBuffer publicExponent;
      ATTEMPT_BUFFER_INIT(publicExponent, params.mPublicExponent.Value());
      nsString hashName;
      mEarlyRv = GetAlgorithmName(aCx, params.mHash.Value(), hashName);
      if (NS_FAILED(mEarlyRv)) {
        mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
        return;
      }

      // Create algorithm
      algorithm = new RsaHashedKeyAlgorithm(global, algName, modulusLength,
                                            publicExponent, hashName);
      mKeyPair->PublicKey()->SetAlgorithm(algorithm);
      mKeyPair->PrivateKey()->SetAlgorithm(algorithm);
      mMechanism = CKM_RSA_PKCS_KEY_PAIR_GEN;

      // Set up params struct
      mRsaParams.keySizeInBits = modulusLength;
      bool converted = publicExponent.GetBigIntValue(mRsaParams.pe);
      if (!converted) {
        mEarlyRv = NS_ERROR_DOM_INVALID_ACCESS_ERR;
        return;
      }
    } else if (algName.EqualsLiteral(WEBCRYPTO_ALG_RSAES_PKCS1)) {
      RootedDictionary<RsaKeyGenParams> params(aCx);
      mEarlyRv = Coerce(aCx, params, aAlgorithm);
      if (NS_FAILED(mEarlyRv) || !params.mModulusLength.WasPassed() ||
          !params.mPublicExponent.WasPassed()) {
        mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
        return;
      }

      // Pull relevant info
      uint32_t modulusLength = params.mModulusLength.Value();
      CryptoBuffer publicExponent;
      ATTEMPT_BUFFER_INIT(publicExponent, params.mPublicExponent.Value());

      // Create algorithm and note the mechanism
      algorithm = new RsaKeyAlgorithm(global, algName, modulusLength,
                                      publicExponent);
      mKeyPair->PublicKey()->SetAlgorithm(algorithm);
      mKeyPair->PrivateKey()->SetAlgorithm(algorithm);
      mMechanism = CKM_RSA_PKCS_KEY_PAIR_GEN;

      // Set up params struct
      mRsaParams.keySizeInBits = modulusLength;
      bool converted = publicExponent.GetBigIntValue(mRsaParams.pe);
      if (!converted) {
        mEarlyRv = NS_ERROR_DOM_INVALID_ACCESS_ERR;
        return;
      }
    } else {
      mEarlyRv = NS_ERROR_DOM_NOT_SUPPORTED_ERR;
      return;
    }

    // Set key usages.
    if (algName.EqualsLiteral(WEBCRYPTO_ALG_RSASSA_PKCS1)) {
      privateAllowedUsages = CryptoKey::SIGN;
      publicAllowedUsages = CryptoKey::VERIFY;
    } else if (algName.EqualsLiteral(WEBCRYPTO_ALG_RSAES_PKCS1) ||
               algName.EqualsLiteral(WEBCRYPTO_ALG_RSA_OAEP)) {
      privateAllowedUsages = CryptoKey::DECRYPT | CryptoKey::UNWRAPKEY;
      publicAllowedUsages = CryptoKey::ENCRYPT | CryptoKey::WRAPKEY;
    }

    mKeyPair->PrivateKey()->SetExtractable(aExtractable);
    mKeyPair->PrivateKey()->SetType(CryptoKey::PRIVATE);

    mKeyPair->PublicKey()->SetExtractable(true);
    mKeyPair->PublicKey()->SetType(CryptoKey::PUBLIC);

    mKeyPair->PrivateKey()->ClearUsages();
    mKeyPair->PublicKey()->ClearUsages();
    for (uint32_t i=0; i < aKeyUsages.Length(); ++i) {
      mEarlyRv = mKeyPair->PrivateKey()->AddUsageIntersecting(aKeyUsages[i],
                                                              privateAllowedUsages);
      if (NS_FAILED(mEarlyRv)) {
        return;
      }

      mEarlyRv = mKeyPair->PublicKey()->AddUsageIntersecting(aKeyUsages[i],
                                                             publicAllowedUsages);
      if (NS_FAILED(mEarlyRv)) {
        return;
      }
    }
  }

private:
  nsRefPtr<CryptoKeyPair> mKeyPair;
  CK_MECHANISM_TYPE mMechanism;
  PK11RSAGenParams mRsaParams;
  ScopedSECKEYPublicKey mPublicKey;
  ScopedSECKEYPrivateKey mPrivateKey;

  virtual void ReleaseNSSResources() MOZ_OVERRIDE
  {
    mPublicKey.dispose();
    mPrivateKey.dispose();
  }

  virtual nsresult DoCrypto() MOZ_OVERRIDE
  {
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    MOZ_ASSERT(slot.get());

    void* param;
    switch (mMechanism) {
      case CKM_RSA_PKCS_KEY_PAIR_GEN: param = &mRsaParams; break;
      default: return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    }

    SECKEYPublicKey* pubKey = nullptr;
    mPrivateKey = PK11_GenerateKeyPair(slot.get(), mMechanism, param, &pubKey,
                                       PR_FALSE, PR_FALSE, nullptr);
    mPublicKey = pubKey;
    if (!mPrivateKey.get() || !mPublicKey.get()) {
      return NS_ERROR_DOM_UNKNOWN_ERR;
    }

    mKeyPair->PrivateKey()->SetPrivateKey(mPrivateKey);
    mKeyPair->PublicKey()->SetPublicKey(mPublicKey);
    return NS_OK;
  }

  virtual void Resolve() MOZ_OVERRIDE
  {
    mResultPromise->MaybeResolve(mKeyPair);
  }

  virtual void Cleanup() MOZ_OVERRIDE
  {
    mKeyPair = nullptr;
  }
};

class DerivePbkdfBitsTask : public ReturnArrayBufferViewTask
{
public:
  DerivePbkdfBitsTask(JSContext* aCx,
      const ObjectOrString& aAlgorithm, CryptoKey& aKey, uint32_t aLength)
    : mSymKey(aKey.GetSymKey())
  {
    Init(aCx, aAlgorithm, aKey, aLength);
  }

  DerivePbkdfBitsTask(JSContext* aCx, const ObjectOrString& aAlgorithm,
                      CryptoKey& aKey, const ObjectOrString& aTargetAlgorithm)
    : mSymKey(aKey.GetSymKey())
  {
    size_t length;
    mEarlyRv = GetKeySizeForAlgorithm(aCx, aTargetAlgorithm, length);

    if (NS_SUCCEEDED(mEarlyRv)) {
      Init(aCx, aAlgorithm, aKey, length);
    }
  }

  void Init(JSContext* aCx, const ObjectOrString& aAlgorithm, CryptoKey& aKey,
            uint32_t aLength)
  {
    // Check that we got a symmetric key
    if (mSymKey.Length() == 0) {
      mEarlyRv = NS_ERROR_DOM_INVALID_ACCESS_ERR;
      return;
    }

    RootedDictionary<Pbkdf2Params> params(aCx);
    mEarlyRv = Coerce(aCx, params, aAlgorithm);
    if (NS_FAILED(mEarlyRv) || !params.mHash.WasPassed() ||
        !params.mIterations.WasPassed() || !params.mSalt.WasPassed()) {
      mEarlyRv = NS_ERROR_DOM_SYNTAX_ERR;
      return;
    }

    // length must be a multiple of 8 bigger than zero.
    if (aLength == 0 || aLength % 8) {
      mEarlyRv = NS_ERROR_DOM_DATA_ERR;
      return;
    }

    // Extract the hash algorithm.
    nsString hashName;
    mEarlyRv = GetAlgorithmName(aCx, params.mHash.Value(), hashName);
    if (NS_FAILED(mEarlyRv)) {
      return;
    }

    // Check the given hash algorithm.
    switch (MapAlgorithmNameToMechanism(hashName)) {
      case CKM_SHA_1: mHashOidTag = SEC_OID_HMAC_SHA1; break;
      case CKM_SHA256: mHashOidTag = SEC_OID_HMAC_SHA256; break;
      case CKM_SHA384: mHashOidTag = SEC_OID_HMAC_SHA384; break;
      case CKM_SHA512: mHashOidTag = SEC_OID_HMAC_SHA512; break;
      default: {
        mEarlyRv = NS_ERROR_DOM_NOT_SUPPORTED_ERR;
        return;
      }
    }

    ATTEMPT_BUFFER_INIT(mSalt, params.mSalt.Value())
    mLength = aLength >> 3; // bits to bytes
    mIterations = params.mIterations.Value();
  }

private:
  size_t mLength;
  size_t mIterations;
  CryptoBuffer mSalt;
  CryptoBuffer mSymKey;
  SECOidTag mHashOidTag;

  virtual nsresult DoCrypto() MOZ_OVERRIDE
  {
    ScopedSECItem salt;
    ATTEMPT_BUFFER_TO_SECITEM(salt, mSalt);

    // Always pass in cipherAlg=SEC_OID_HMAC_SHA1 (i.e. PBMAC1) as this
    // parameter is unused for key generation. It is currently only used
    // for PBKDF2 authentication or key (un)wrapping when specifying an
    // encryption algorithm (PBES2).
    ScopedSECAlgorithmID alg_id(PK11_CreatePBEV2AlgorithmID(
      SEC_OID_PKCS5_PBKDF2, SEC_OID_HMAC_SHA1, mHashOidTag,
      mLength, mIterations, salt));

    if (!alg_id.get()) {
      return NS_ERROR_DOM_OPERATION_ERR;
    }

    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    if (!slot.get()) {
      return NS_ERROR_DOM_OPERATION_ERR;
    }

    ScopedSECItem keyItem;
    ATTEMPT_BUFFER_TO_SECITEM(keyItem, mSymKey);

    ScopedPK11SymKey symKey(PK11_PBEKeyGen(slot, alg_id, keyItem, false, nullptr));
    if (!symKey.get()) {
      return NS_ERROR_DOM_OPERATION_ERR;
    }

    nsresult rv = MapSECStatus(PK11_ExtractKeyValue(symKey));
    if (NS_FAILED(rv)) {
      return NS_ERROR_DOM_OPERATION_ERR;
    }

    // This doesn't leak, because the SECItem* returned by PK11_GetKeyData
    // just refers to a buffer managed by symKey. The assignment copies the
    // data, so mResult manages one copy, while symKey manages another.
    ATTEMPT_BUFFER_ASSIGN(mResult, PK11_GetKeyData(symKey));
    return NS_OK;
  }
};

class DerivePbkdfKeyTask : public DerivePbkdfBitsTask
{
public:
  DerivePbkdfKeyTask(JSContext* aCx,
                     const ObjectOrString& aAlgorithm, CryptoKey& aBaseKey,
                     const ObjectOrString& aDerivedKeyType, bool aExtractable,
                     const Sequence<nsString>& aKeyUsages)
    : DerivePbkdfBitsTask(aCx, aAlgorithm, aBaseKey, aDerivedKeyType)
    , mResolved(false)
  {
    if (NS_FAILED(mEarlyRv)) {
      return;
    }

    NS_NAMED_LITERAL_STRING(format, WEBCRYPTO_KEY_FORMAT_RAW);
    mTask = new ImportSymmetricKeyTask(aCx, format, aDerivedKeyType,
                                       aExtractable, aKeyUsages);
  }

protected:
  nsRefPtr<ImportSymmetricKeyTask> mTask;
  bool mResolved;

private:
  virtual void Resolve() MOZ_OVERRIDE {
    mTask->SetKeyData(mResult);
    mTask->DispatchWithPromise(mResultPromise);
    mResolved = true;
  }

  virtual void Cleanup() MOZ_OVERRIDE
  {
    if (mTask && !mResolved) {
      mTask->Skip();
    }
    mTask = nullptr;
  }
};

template<class KeyEncryptTask>
class WrapKeyTask : public ExportKeyTask
{
public:
  WrapKeyTask(JSContext* aCx,
              const nsAString& aFormat,
              CryptoKey& aKey,
              CryptoKey& aWrappingKey,
              const ObjectOrString& aWrapAlgorithm)
    : ExportKeyTask(aFormat, aKey)
    , mResolved(false)
  {
    if (NS_FAILED(mEarlyRv)) {
      return;
    }

    mTask = new KeyEncryptTask(aCx, aWrapAlgorithm, aWrappingKey, true);
  }

private:
  nsRefPtr<KeyEncryptTask> mTask;
  bool mResolved;

  virtual nsresult AfterCrypto() MOZ_OVERRIDE {
    // If wrapping JWK, stringify the JSON
    if (mFormat.EqualsLiteral(WEBCRYPTO_KEY_FORMAT_JWK)) {
      nsAutoString json;
      if (!mJwk.ToJSON(json)) {
        return NS_ERROR_DOM_OPERATION_ERR;
      }

      NS_ConvertUTF16toUTF8 utf8(json);
      mResult.Assign((const uint8_t*) utf8.BeginReading(), utf8.Length());
    }

    return NS_OK;
  }

  virtual void Resolve() MOZ_OVERRIDE {
    mTask->SetData(mResult);
    mTask->DispatchWithPromise(mResultPromise);
    mResolved = true;
  }

  virtual void Cleanup() MOZ_OVERRIDE
  {
    if (mTask && !mResolved) {
      mTask->Skip();
    }
    mTask = nullptr;
  }
};

template<class KeyEncryptTask>
class UnwrapKeyTask : public KeyEncryptTask
{
public:
  UnwrapKeyTask(JSContext* aCx,
                const ArrayBufferViewOrArrayBuffer& aWrappedKey,
                CryptoKey& aUnwrappingKey,
                const ObjectOrString& aUnwrapAlgorithm,
                ImportKeyTask* aTask)
    : KeyEncryptTask(aCx, aUnwrapAlgorithm, aUnwrappingKey, aWrappedKey, false)
    , mTask(aTask)
    , mResolved(false)
  {}

private:
  nsRefPtr<ImportKeyTask> mTask;
  bool mResolved;

  virtual void Resolve() MOZ_OVERRIDE {
    mTask->SetKeyData(KeyEncryptTask::mResult);
    mTask->DispatchWithPromise(KeyEncryptTask::mResultPromise);
    mResolved = true;
  }

  virtual void Cleanup() MOZ_OVERRIDE
  {
    if (mTask && !mResolved) {
      mTask->Skip();
    }
    mTask = nullptr;
  }
};

// Task creation methods for WebCryptoTask

WebCryptoTask*
WebCryptoTask::CreateEncryptDecryptTask(JSContext* aCx,
                                  const ObjectOrString& aAlgorithm,
                                  CryptoKey& aKey,
                                  const CryptoOperationData& aData,
                                  bool aEncrypt)
{
  TelemetryMethod method = (aEncrypt)? TM_ENCRYPT : TM_DECRYPT;
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_METHOD, method);
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_EXTRACTABLE_ENC, aKey.Extractable());

  nsString algName;
  nsresult rv = GetAlgorithmName(aCx, aAlgorithm, algName);
  if (NS_FAILED(rv)) {
    return new FailureTask(rv);
  }

  // Ensure key is usable for this operation
  if ((aEncrypt  && !aKey.HasUsage(CryptoKey::ENCRYPT)) ||
      (!aEncrypt && !aKey.HasUsage(CryptoKey::DECRYPT))) {
    return new FailureTask(NS_ERROR_DOM_INVALID_ACCESS_ERR);
  }

  if (algName.EqualsLiteral(WEBCRYPTO_ALG_AES_CBC) ||
      algName.EqualsLiteral(WEBCRYPTO_ALG_AES_CTR) ||
      algName.EqualsLiteral(WEBCRYPTO_ALG_AES_GCM)) {
    return new AesTask(aCx, aAlgorithm, aKey, aData, aEncrypt);
  } else if (algName.EqualsLiteral(WEBCRYPTO_ALG_RSAES_PKCS1)) {
    return new RsaesPkcs1Task(aCx, aAlgorithm, aKey, aData, aEncrypt);
  } else if (algName.EqualsLiteral(WEBCRYPTO_ALG_RSA_OAEP)) {
    return new RsaOaepTask(aCx, aAlgorithm, aKey, aData, aEncrypt);
  }

  return new FailureTask(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
}

WebCryptoTask*
WebCryptoTask::CreateSignVerifyTask(JSContext* aCx,
                              const ObjectOrString& aAlgorithm,
                              CryptoKey& aKey,
                              const CryptoOperationData& aSignature,
                              const CryptoOperationData& aData,
                              bool aSign)
{
  TelemetryMethod method = (aSign)? TM_SIGN : TM_VERIFY;
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_METHOD, method);
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_EXTRACTABLE_SIG, aKey.Extractable());

  nsString algName;
  nsresult rv = GetAlgorithmName(aCx, aAlgorithm, algName);
  if (NS_FAILED(rv)) {
    return new FailureTask(rv);
  }

  // Ensure key is usable for this operation
  if ((aSign  && !aKey.HasUsage(CryptoKey::SIGN)) ||
      (!aSign && !aKey.HasUsage(CryptoKey::VERIFY))) {
    return new FailureTask(NS_ERROR_DOM_INVALID_ACCESS_ERR);
  }

  if (algName.EqualsLiteral(WEBCRYPTO_ALG_HMAC)) {
    return new HmacTask(aCx, aAlgorithm, aKey, aSignature, aData, aSign);
  } else if (algName.EqualsLiteral(WEBCRYPTO_ALG_RSASSA_PKCS1)) {
    return new RsassaPkcs1Task(aCx, aAlgorithm, aKey, aSignature, aData, aSign);
  }

  return new FailureTask(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
}

WebCryptoTask*
WebCryptoTask::CreateDigestTask(JSContext* aCx,
                          const ObjectOrString& aAlgorithm,
                          const CryptoOperationData& aData)
{
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_METHOD, TM_DIGEST);
  return new DigestTask(aCx, aAlgorithm, aData);
}

WebCryptoTask*
WebCryptoTask::CreateImportKeyTask(JSContext* aCx,
                             const nsAString& aFormat,
                             JS::Handle<JSObject*> aKeyData,
                             const ObjectOrString& aAlgorithm,
                             bool aExtractable,
                             const Sequence<nsString>& aKeyUsages)
{
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_METHOD, TM_IMPORTKEY);
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_EXTRACTABLE_IMPORT, aExtractable);

  nsString algName;
  nsresult rv = GetAlgorithmName(aCx, aAlgorithm, algName);
  if (NS_FAILED(rv)) {
    return new FailureTask(rv);
  }

  if (algName.EqualsLiteral(WEBCRYPTO_ALG_AES_CBC) ||
      algName.EqualsLiteral(WEBCRYPTO_ALG_AES_CTR) ||
      algName.EqualsLiteral(WEBCRYPTO_ALG_AES_GCM) ||
      algName.EqualsLiteral(WEBCRYPTO_ALG_AES_KW) ||
      algName.EqualsLiteral(WEBCRYPTO_ALG_PBKDF2) ||
      algName.EqualsLiteral(WEBCRYPTO_ALG_HMAC)) {
    return new ImportSymmetricKeyTask(aCx, aFormat, aKeyData, aAlgorithm,
                                      aExtractable, aKeyUsages);
  } else if (algName.EqualsLiteral(WEBCRYPTO_ALG_RSAES_PKCS1) ||
             algName.EqualsLiteral(WEBCRYPTO_ALG_RSASSA_PKCS1) ||
             algName.EqualsLiteral(WEBCRYPTO_ALG_RSA_OAEP)) {
    return new ImportRsaKeyTask(aCx, aFormat, aKeyData, aAlgorithm,
                                aExtractable, aKeyUsages);
  } else {
    return new FailureTask(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  }
}

WebCryptoTask*
WebCryptoTask::CreateExportKeyTask(const nsAString& aFormat,
                             CryptoKey& aKey)
{
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_METHOD, TM_EXPORTKEY);

  return new ExportKeyTask(aFormat, aKey);
}

WebCryptoTask*
WebCryptoTask::CreateGenerateKeyTask(JSContext* aCx,
                               const ObjectOrString& aAlgorithm,
                               bool aExtractable,
                               const Sequence<nsString>& aKeyUsages)
{
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_METHOD, TM_GENERATEKEY);
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_EXTRACTABLE_GENERATE, aExtractable);

  nsString algName;
  nsresult rv = GetAlgorithmName(aCx, aAlgorithm, algName);
  if (NS_FAILED(rv)) {
    return new FailureTask(rv);
  }

  if (algName.EqualsASCII(WEBCRYPTO_ALG_AES_CBC) ||
      algName.EqualsASCII(WEBCRYPTO_ALG_AES_CTR) ||
      algName.EqualsASCII(WEBCRYPTO_ALG_AES_GCM) ||
      algName.EqualsASCII(WEBCRYPTO_ALG_AES_KW) ||
      algName.EqualsASCII(WEBCRYPTO_ALG_HMAC)) {
    return new GenerateSymmetricKeyTask(aCx, aAlgorithm, aExtractable, aKeyUsages);
  } else if (algName.EqualsASCII(WEBCRYPTO_ALG_RSAES_PKCS1) ||
             algName.EqualsASCII(WEBCRYPTO_ALG_RSASSA_PKCS1) ||
             algName.EqualsASCII(WEBCRYPTO_ALG_RSA_OAEP)) {
    return new GenerateAsymmetricKeyTask(aCx, aAlgorithm, aExtractable, aKeyUsages);
  } else {
    return new FailureTask(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  }
}

WebCryptoTask*
WebCryptoTask::CreateDeriveKeyTask(JSContext* aCx,
                             const ObjectOrString& aAlgorithm,
                             CryptoKey& aBaseKey,
                             const ObjectOrString& aDerivedKeyType,
                             bool aExtractable,
                             const Sequence<nsString>& aKeyUsages)
{
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_METHOD, TM_DERIVEKEY);

  nsString algName;
  nsresult rv = GetAlgorithmName(aCx, aAlgorithm, algName);
  if (NS_FAILED(rv)) {
    return new FailureTask(rv);
  }

  if (algName.EqualsASCII(WEBCRYPTO_ALG_PBKDF2)) {
    return new DerivePbkdfKeyTask(aCx, aAlgorithm, aBaseKey, aDerivedKeyType,
                                  aExtractable, aKeyUsages);
  }

  return new FailureTask(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
}

WebCryptoTask*
WebCryptoTask::CreateDeriveBitsTask(JSContext* aCx,
                              const ObjectOrString& aAlgorithm,
                              CryptoKey& aKey,
                              uint32_t aLength)
{
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_METHOD, TM_DERIVEBITS);

  nsString algName;
  nsresult rv = GetAlgorithmName(aCx, aAlgorithm, algName);
  if (NS_FAILED(rv)) {
    return new FailureTask(rv);
  }

  if (algName.EqualsASCII(WEBCRYPTO_ALG_PBKDF2)) {
    return new DerivePbkdfBitsTask(aCx, aAlgorithm, aKey, aLength);
  }

  return new FailureTask(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
}

WebCryptoTask*
WebCryptoTask::CreateWrapKeyTask(JSContext* aCx,
                             const nsAString& aFormat,
                           CryptoKey& aKey,
                           CryptoKey& aWrappingKey,
                           const ObjectOrString& aWrapAlgorithm)
{
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_METHOD, TM_WRAPKEY);

  // Ensure key is usable for this operation
  if (!aWrappingKey.HasUsage(CryptoKey::WRAPKEY)) {
    return new FailureTask(NS_ERROR_DOM_INVALID_ACCESS_ERR);
  }

  nsString wrapAlgName;
  nsresult rv = GetAlgorithmName(aCx, aWrapAlgorithm, wrapAlgName);
  if (NS_FAILED(rv)) {
    return new FailureTask(rv);
  }

  if (wrapAlgName.EqualsLiteral(WEBCRYPTO_ALG_AES_CBC) ||
      wrapAlgName.EqualsLiteral(WEBCRYPTO_ALG_AES_CTR) ||
      wrapAlgName.EqualsLiteral(WEBCRYPTO_ALG_AES_GCM)) {
    return new WrapKeyTask<AesTask>(aCx, aFormat, aKey,
                                    aWrappingKey, aWrapAlgorithm);
  } else if (wrapAlgName.EqualsLiteral(WEBCRYPTO_ALG_AES_KW)) {
    return new WrapKeyTask<AesKwTask>(aCx, aFormat, aKey,
                                    aWrappingKey, aWrapAlgorithm);
  } else if (wrapAlgName.EqualsLiteral(WEBCRYPTO_ALG_RSAES_PKCS1)) {
    return new WrapKeyTask<RsaesPkcs1Task>(aCx, aFormat, aKey,
                                           aWrappingKey, aWrapAlgorithm);
  } else if (wrapAlgName.EqualsLiteral(WEBCRYPTO_ALG_RSA_OAEP)) {
    return new WrapKeyTask<RsaOaepTask>(aCx, aFormat, aKey,
                                        aWrappingKey, aWrapAlgorithm);
  }

  return new FailureTask(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
}

WebCryptoTask*
WebCryptoTask::CreateUnwrapKeyTask(JSContext* aCx,
                             const nsAString& aFormat,
                             const ArrayBufferViewOrArrayBuffer& aWrappedKey,
                             CryptoKey& aUnwrappingKey,
                             const ObjectOrString& aUnwrapAlgorithm,
                             const ObjectOrString& aUnwrappedKeyAlgorithm,
                             bool aExtractable,
                             const Sequence<nsString>& aKeyUsages)
{
  Telemetry::Accumulate(Telemetry::WEBCRYPTO_METHOD, TM_UNWRAPKEY);

  // Ensure key is usable for this operation
  if (!aUnwrappingKey.HasUsage(CryptoKey::UNWRAPKEY)) {
    return new FailureTask(NS_ERROR_DOM_INVALID_ACCESS_ERR);
  }

  nsString keyAlgName;
  nsresult rv = GetAlgorithmName(aCx, aUnwrappedKeyAlgorithm, keyAlgName);
  if (NS_FAILED(rv)) {
    return new FailureTask(rv);
  }

  CryptoOperationData dummy;
  nsRefPtr<ImportKeyTask> importTask;
  if (keyAlgName.EqualsASCII(WEBCRYPTO_ALG_AES_CBC) ||
      keyAlgName.EqualsASCII(WEBCRYPTO_ALG_AES_CTR) ||
      keyAlgName.EqualsASCII(WEBCRYPTO_ALG_AES_GCM) ||
      keyAlgName.EqualsASCII(WEBCRYPTO_ALG_HMAC)) {
    importTask = new ImportSymmetricKeyTask(aCx, aFormat,
                                            aUnwrappedKeyAlgorithm,
                                            aExtractable, aKeyUsages);
  } else if (keyAlgName.EqualsASCII(WEBCRYPTO_ALG_RSAES_PKCS1) ||
             keyAlgName.EqualsASCII(WEBCRYPTO_ALG_RSASSA_PKCS1) ||
             keyAlgName.EqualsASCII(WEBCRYPTO_ALG_RSA_OAEP)) {
    importTask = new ImportRsaKeyTask(aCx, aFormat,
                                      aUnwrappedKeyAlgorithm,
                                      aExtractable, aKeyUsages);
  } else {
    return new FailureTask(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  }

  nsString unwrapAlgName;
  rv = GetAlgorithmName(aCx, aUnwrapAlgorithm, unwrapAlgName);
  if (NS_FAILED(rv)) {
    return new FailureTask(rv);
  }
  if (unwrapAlgName.EqualsLiteral(WEBCRYPTO_ALG_AES_CBC) ||
      unwrapAlgName.EqualsLiteral(WEBCRYPTO_ALG_AES_CTR) ||
      unwrapAlgName.EqualsLiteral(WEBCRYPTO_ALG_AES_GCM)) {
    return new UnwrapKeyTask<AesTask>(aCx, aWrappedKey,
                                      aUnwrappingKey, aUnwrapAlgorithm,
                                      importTask);
  } else if (unwrapAlgName.EqualsLiteral(WEBCRYPTO_ALG_AES_KW)) {
    return new UnwrapKeyTask<AesKwTask>(aCx, aWrappedKey,
                                      aUnwrappingKey, aUnwrapAlgorithm,
                                      importTask);
  } else if (unwrapAlgName.EqualsLiteral(WEBCRYPTO_ALG_RSAES_PKCS1)) {
    return new UnwrapKeyTask<RsaesPkcs1Task>(aCx, aWrappedKey,
                                      aUnwrappingKey, aUnwrapAlgorithm,
                                      importTask);
  } else if (unwrapAlgName.EqualsLiteral(WEBCRYPTO_ALG_RSA_OAEP)) {
    return new UnwrapKeyTask<RsaOaepTask>(aCx, aWrappedKey,
                                      aUnwrappingKey, aUnwrapAlgorithm,
                                      importTask);
  }

  return new FailureTask(NS_ERROR_DOM_NOT_SUPPORTED_ERR);

}

} // namespace dom
} // namespace mozilla
