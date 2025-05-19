/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest_utils.h"
#include "ssl.h"
#include "tls_connect.h"

namespace nss_test {

class SslOptionTest : public ::testing::Test {};

static PRInt32 nextOption(PRInt32 index) {
  switch (++index) {
    case SSL_SOCKS:                // pinned to false
    case 4:                        // not defined
    case SSL_ENABLE_SSL2:          // pinned to false
    case SSL_V2_COMPATIBLE_HELLO:  // pinned to false
    case SSL_ENABLE_TLS:           // depends on other options
    case SSL_NO_STEP_DOWN:         // pinned to false
    case SSL_BYPASS_PKCS11:        // pinned to false
    case SSL_ENABLE_NPN:           // pinned to false
    case SSL_RECORD_SIZE_LIMIT:    // not a boolean
      return nextOption(index);
  }
  return index;
}

TEST_F(SslOptionTest, OptionSetDefault) {
  PRIntn original, modified;
  PRInt32 index = nextOption(0);
  while (SECSuccess == SSL_OptionGetDefault(index, &original)) {
    EXPECT_EQ(SECSuccess, SSL_OptionSetDefault(index, 1 ^ original));
    EXPECT_EQ(SECSuccess, SSL_OptionGetDefault(index, &modified));
    EXPECT_EQ(modified, 1 ^ original);
    EXPECT_EQ(SECSuccess, SSL_OptionSetDefault(index, original));
    index = nextOption(index);
  }

  // Update the expected value here when new options are added.
  EXPECT_EQ(index, SSL_DB_LOAD_CERTIFICATE_CHAIN + 1);
}

TEST_F(TlsConnectStreamTls13, OptionSet) {
  EnsureTlsSetup();
  PRIntn original, modified;
  PRInt32 index = nextOption(0);
  while (SECSuccess == SSL_OptionGetDefault(index, &original)) {
    EXPECT_EQ(SECSuccess,
              SSL_OptionSet(client_->ssl_fd(), index, 1 ^ original));
    EXPECT_EQ(SECSuccess, SSL_OptionGet(client_->ssl_fd(), index, &modified));
    EXPECT_EQ(modified, 1 ^ original);
    EXPECT_EQ(SECSuccess, SSL_OptionSet(client_->ssl_fd(), index, original));
    index = nextOption(index);
  }

  // Update the expected value here when new options are added.
  EXPECT_EQ(index, SSL_DB_LOAD_CERTIFICATE_CHAIN + 1);
  Connect();
}

}  // namespace nss_test
