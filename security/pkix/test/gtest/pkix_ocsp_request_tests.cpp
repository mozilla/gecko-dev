/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2013 Mozilla Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "nssgtest.h"
#include "pkix/pkix.h"
#include "pkixder.h"
#include "prerror.h"
#include "secerr.h"

using namespace mozilla::pkix;
using namespace mozilla::pkix::test;

class pkix_ocsp_request_tests : public NSSTest
{
protected:
  // These SECItems are allocated in arena, and so will be auto-cleaned.
  SECItem* unsupportedLongSerialNumber;
  SECItem* longestRequiredSerialNumber;

  void SetUp()
  {
    static const uint8_t UNSUPPORTED_LEN = 128; // must be larger than 127
    // tag + length + value is 1 + 2 + UNSUPPORTED_LEN
    unsupportedLongSerialNumber = SECITEM_AllocItem(arena.get(), nullptr,
                                                    1 + 2 + UNSUPPORTED_LEN);
    memset(unsupportedLongSerialNumber->data, 0,
           unsupportedLongSerialNumber->len);
    unsupportedLongSerialNumber->data[0] = der::INTEGER;
    // Encoding the length takes two bytes: one byte to indicate that a
    // second byte follows, and the second byte to indicate the length.
    unsupportedLongSerialNumber->data[1] = 0x80 + 1;
    unsupportedLongSerialNumber->data[2] = UNSUPPORTED_LEN;
    unsupportedLongSerialNumber->data[3] = 0x01; // value is 0x010000...00

    static const uint8_t LONGEST_REQUIRED_LEN = 20;
    // tag + length + value is 1 + 1 + LONGEST_REQUIRED_LEN
    longestRequiredSerialNumber = SECITEM_AllocItem(arena.get(), nullptr,
                                    1 + 1 + LONGEST_REQUIRED_LEN);
    memset(longestRequiredSerialNumber->data, 0,
           longestRequiredSerialNumber->len);
    longestRequiredSerialNumber->data[0] = der::INTEGER;
    longestRequiredSerialNumber->data[1] = LONGEST_REQUIRED_LEN;
    longestRequiredSerialNumber->data[2] = 0x01; // value is 0x010000...00
  }

  // The resultant issuerDER is owned by the arena.
  SECStatus MakeIssuerCertIDComponents(const char* issuerASCII,
                                       /*out*/ const SECItem*& issuerDER,
                                       /*out*/ ScopedSECItem& issuerSPKI)
  {
    issuerDER = ASCIIToDERName(arena.get(), issuerASCII);
    if (!issuerDER) {
      return SECFailure;
    }
    ScopedSECKEYPublicKey issuerPublicKey;
    ScopedSECKEYPrivateKey issuerPrivateKey;
    if (GenerateKeyPair(issuerPublicKey, issuerPrivateKey) != SECSuccess) {
      return SECFailure;
    }
    issuerSPKI = SECKEY_EncodeDERSubjectPublicKeyInfo(issuerPublicKey.get());
    if (!issuerSPKI) {
      return SECFailure;
    }

    return SECSuccess;
  }

};

// Test that the large length of the child serial number causes
// CreateEncodedOCSPRequest to fail.
TEST_F(pkix_ocsp_request_tests, ChildCertLongSerialNumberTest)
{
  const SECItem* issuerDER;
  ScopedSECItem issuerSPKI;
  ASSERT_EQ(SECSuccess,
            MakeIssuerCertIDComponents("CN=CA", issuerDER, issuerSPKI));
  ASSERT_FALSE(CreateEncodedOCSPRequest(arena.get(),
                                        CertID(*issuerDER, *issuerSPKI,
                                               *unsupportedLongSerialNumber)));
  ASSERT_EQ(SEC_ERROR_BAD_DATA, PR_GetError());
}

// Test that CreateEncodedOCSPRequest handles the longest serial number that
// it's required to support (i.e. 20 octets).
TEST_F(pkix_ocsp_request_tests, LongestSupportedSerialNumberTest)
{
  const SECItem* issuerDER;
  ScopedSECItem issuerSPKI;
  ASSERT_EQ(SECSuccess,
            MakeIssuerCertIDComponents("CN=CA", issuerDER, issuerSPKI));
  ASSERT_TRUE(CreateEncodedOCSPRequest(arena.get(),
                                        CertID(*issuerDER, *issuerSPKI,
                                               *longestRequiredSerialNumber)));
}
