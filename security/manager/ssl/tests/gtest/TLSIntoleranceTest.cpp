/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSIOLayer.h"
#include "sslproto.h"

#include "gtest/gtest.h"

NS_NAMED_LITERAL_CSTRING(HOST, "example.org");
const int16_t PORT = 443;

class TLSIntoleranceTest : public ::testing::Test
{
protected:
  nsSSLIOLayerHelpers helpers;
};

TEST_F(TLSIntoleranceTest, Test_1_2_through_3_0)
{
  // No adjustment made when there is no entry for the site.
  {
    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };
    helpers.adjustForTLSIntolerance(HOST, PORT, range);
    ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, range.max);

    ASSERT_TRUE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                    range.min, range.max));
  }

  {
    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };
    helpers.adjustForTLSIntolerance(HOST, PORT, range);
    ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_1, range.max);

    ASSERT_TRUE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                    range.min, range.max));
  }

  {
    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };
    helpers.adjustForTLSIntolerance(HOST, PORT, range);
    ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_0, range.max);

    ASSERT_TRUE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                    range.min, range.max));
  }

  {
    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };

    helpers.adjustForTLSIntolerance(HOST, PORT, range);
    ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
    ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.max);

    // false because we reached the floor set by range.min
    ASSERT_FALSE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                     range.min, range.max));
  }

  {
    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };
    helpers.adjustForTLSIntolerance(HOST, PORT, range);
    ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
    // When rememberIntolerantAtVersion returns false, it also resets the
    // intolerance information for the server.
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, range.max);
  }
}

TEST_F(TLSIntoleranceTest, Test_Tolerant_Overrides_Intolerant_1)
{
  ASSERT_TRUE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                  SSL_LIBRARY_VERSION_3_0,
                                                  SSL_LIBRARY_VERSION_TLS_1_0));
  helpers.rememberTolerantAtVersion(HOST, PORT, SSL_LIBRARY_VERSION_TLS_1_0);
  SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                            SSL_LIBRARY_VERSION_TLS_1_2 };
  helpers.adjustForTLSIntolerance(HOST, PORT, range);
  ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
  ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_0, range.max);
}

TEST_F(TLSIntoleranceTest, Test_Tolerant_Overrides_Intolerant_2)
{
  ASSERT_TRUE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                  SSL_LIBRARY_VERSION_3_0,
                                                  SSL_LIBRARY_VERSION_TLS_1_0));
  helpers.rememberTolerantAtVersion(HOST, PORT, SSL_LIBRARY_VERSION_TLS_1_1);
  SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                            SSL_LIBRARY_VERSION_TLS_1_2 };
  helpers.adjustForTLSIntolerance(HOST, PORT, range);
  ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
  ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_1, range.max);
}

TEST_F(TLSIntoleranceTest, Test_Intolerant_Does_Not_Override_Tolerant)
{
  // No adjustment made when there is no entry for the site.
  helpers.rememberTolerantAtVersion(HOST, PORT, SSL_LIBRARY_VERSION_TLS_1_0);
  // false because we reached the floor set by rememberTolerantAtVersion.
  ASSERT_FALSE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                   SSL_LIBRARY_VERSION_3_0,
                                                   SSL_LIBRARY_VERSION_TLS_1_0));
  SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                            SSL_LIBRARY_VERSION_TLS_1_2 };
  helpers.adjustForTLSIntolerance(HOST, PORT, range);
  ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
  ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, range.max);
}

TEST_F(TLSIntoleranceTest, Test_Port_Is_Relevant)
{
  helpers.rememberTolerantAtVersion(HOST, 1, SSL_LIBRARY_VERSION_TLS_1_2);
  ASSERT_FALSE(helpers.rememberIntolerantAtVersion(HOST, 1,
                                                   SSL_LIBRARY_VERSION_3_0,
                                                   SSL_LIBRARY_VERSION_TLS_1_2));
  ASSERT_TRUE(helpers.rememberIntolerantAtVersion(HOST, 2,
                                                  SSL_LIBRARY_VERSION_3_0,
                                                  SSL_LIBRARY_VERSION_TLS_1_2));

  {
    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };
    helpers.adjustForTLSIntolerance(HOST, 1, range);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, range.max);
  }

  {
    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };
    helpers.adjustForTLSIntolerance(HOST, 2, range);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_1, range.max);
  }
}
