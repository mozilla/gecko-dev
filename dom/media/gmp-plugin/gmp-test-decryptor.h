/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FAKE_DECRYPTOR_H__
#define FAKE_DECRYPTOR_H__

#include "gmp-decryption.h"
#include "gmp-async-shutdown.h"
#include <string>
#include "mozilla/Attributes.h"

class FakeDecryptor : public GMPDecryptor {
public:

  explicit FakeDecryptor(GMPDecryptorHost* aHost);

  virtual void Init(GMPDecryptorCallback* aCallback) override {
    mCallback = aCallback;
  }

  virtual void CreateSession(uint32_t aCreateSessionToken,
                             uint32_t aPromiseId,
                             const char* aInitDataType,
                             uint32_t aInitDataTypeSize,
                             const uint8_t* aInitData,
                             uint32_t aInitDataSize,
                             GMPSessionType aSessionType) override
  {
  }

  virtual void LoadSession(uint32_t aPromiseId,
                           const char* aSessionId,
                           uint32_t aSessionIdLength) override
  {
  }

  virtual void UpdateSession(uint32_t aPromiseId,
                             const char* aSessionId,
                             uint32_t aSessionIdLength,
                             const uint8_t* aResponse,
                             uint32_t aResponseSize) override;

  virtual void CloseSession(uint32_t aPromiseId,
                            const char* aSessionId,
                            uint32_t aSessionIdLength) override
  {
  }

  virtual void RemoveSession(uint32_t aPromiseId,
                             const char* aSessionId,
                             uint32_t aSessionIdLength) override
  {
  }

  virtual void SetServerCertificate(uint32_t aPromiseId,
                                    const uint8_t* aServerCert,
                                    uint32_t aServerCertSize) override
  {
  }

  virtual void Decrypt(GMPBuffer* aBuffer,
                       GMPEncryptedBufferMetadata* aMetadata) override
  {
  }

  virtual void DecryptingComplete() override;

  static void Message(const std::string& aMessage);

  void ProcessRecordNames(GMPRecordIterator* aRecordIterator,
                          GMPErr aStatus);

private:

  virtual ~FakeDecryptor() {}
  static FakeDecryptor* sInstance;

  void TestStorage();

  GMPDecryptorCallback* mCallback;
  GMPDecryptorHost* mHost;
};

class TestAsyncShutdown : public GMPAsyncShutdown {
public:
  explicit TestAsyncShutdown(GMPAsyncShutdownHost* aHost)
    : mHost(aHost)
  {
  }
  virtual void BeginShutdown() override;
private:
  GMPAsyncShutdownHost* mHost;
};

#endif
