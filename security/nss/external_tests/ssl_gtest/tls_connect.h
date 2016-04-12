/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef tls_connect_h_
#define tls_connect_h_

#include <tuple>

#include "sslt.h"

#include "tls_agent.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

namespace nss_test {

// A generic TLS connection test base.
class TlsConnectTestBase : public ::testing::Test {
 public:
  static ::testing::internal::ParamGenerator<std::string> kTlsModesStream;
  static ::testing::internal::ParamGenerator<std::string> kTlsModesDatagram;
  static ::testing::internal::ParamGenerator<std::string> kTlsModesAll;
  static ::testing::internal::ParamGenerator<uint16_t> kTlsV10;
  static ::testing::internal::ParamGenerator<uint16_t> kTlsV11;
  static ::testing::internal::ParamGenerator<uint16_t> kTlsV10V11;
  static ::testing::internal::ParamGenerator<uint16_t> kTlsV11V12;
  static ::testing::internal::ParamGenerator<uint16_t> kTlsV10To12;
  static ::testing::internal::ParamGenerator<uint16_t> kTlsV13;
  static ::testing::internal::ParamGenerator<uint16_t> kTlsV11Plus;
  static ::testing::internal::ParamGenerator<uint16_t> kTlsV12Plus;
  static ::testing::internal::ParamGenerator<uint16_t> kTlsVAll;

  static inline Mode ToMode(const std::string& str) {
    return str == "TLS" ? STREAM : DGRAM;
  }

  TlsConnectTestBase(Mode mode, uint16_t version);
  virtual ~TlsConnectTestBase();

  void SetUp();
  void TearDown();

  // Initialize client and server.
  void Init();
  // Clear the statistics.
  void ClearStats();
  // Clear the server session cache.
  void ClearServerCache();
  // Re-initialize client and server with the default RSA cert.
  void ResetRsa();
  // Re-initialize client and server with an ECDSA cert on the server
  // and some ECDHE suites.
  void ResetEcdsa();
  // Make sure TLS is configured for a connection.
  void EnsureTlsSetup();

  // Run the handshake.
  void Handshake();
  // Connect and check that it works.
  void Connect();
  // Check that the connection was successfully established.
  void CheckConnected();
  // Connect and expect it to fail.
  void ConnectExpectFail();
  void CheckKeys(SSLKEAType keyType, SSLAuthType authType) const;

  void SetExpectedVersion(uint16_t version);
  // Expect resumption of a particular type.
  void ExpectResumption(SessionResumptionMode expected);
  void DisableDheAndEcdheCiphers();
  void DisableDheCiphers();
  void DisableEcdheCiphers();
  void EnableExtendedMasterSecret();
  void ConfigureSessionCache(SessionResumptionMode client,
                             SessionResumptionMode server);
  void EnableAlpn();
  void EnableSrtp();
  void CheckSrtp() const;
  void SendReceive();
  void Receive(size_t amount);
  void ExpectExtendedMasterSecret(bool expected);

 protected:
  Mode mode_;
  TlsAgent* client_;
  TlsAgent* server_;
  uint16_t version_;
  SessionResumptionMode expected_resumption_mode_;
  std::vector<std::vector<uint8_t>> session_ids_;

 private:
  void Reset(const std::string& server_name, SSLKEAType kea);
  void CheckResumption(SessionResumptionMode expected);
  void CheckExtendedMasterSecret();

  bool expect_extended_master_secret_;
};

// A non-parametrized TLS test base.
class TlsConnectTest : public TlsConnectTestBase {
 public:
 TlsConnectTest() : TlsConnectTestBase(STREAM, 0) {}
};

// A non-parametrized DTLS-only test base.
class DtlsConnectTest : public TlsConnectTestBase {
 public:
  DtlsConnectTest() : TlsConnectTestBase(DGRAM, 0) {}
};

// A TLS-only test base.
class TlsConnectStream : public TlsConnectTestBase,
                         public ::testing::WithParamInterface<uint16_t> {
 public:
  TlsConnectStream() : TlsConnectTestBase(STREAM, GetParam()) {}
};

// A TLS-only test base for tests before 1.3
class TlsConnectStreamPre13 : public TlsConnectStream {
};

// A DTLS-only test base.
class TlsConnectDatagram : public TlsConnectTestBase,
                           public ::testing::WithParamInterface<uint16_t> {
 public:
  TlsConnectDatagram() : TlsConnectTestBase(DGRAM, GetParam()) {}
};

// A generic test class that can be either STREAM or DGRAM and a single version
// of TLS.  This is configured in ssl_loopback_unittest.cc.  All uses of this
// should use TEST_P().
class TlsConnectGeneric
  : public TlsConnectTestBase,
    public ::testing::WithParamInterface<std::tuple<std::string, uint16_t>> {
 public:
  TlsConnectGeneric();
};

// A Pre TLS 1.2 generic test.
class TlsConnectPre12
  : public TlsConnectTestBase,
    public ::testing::WithParamInterface<std::tuple<std::string, uint16_t>> {
 public:
  TlsConnectPre12();
};

// A TLS 1.2 only generic test.
class TlsConnectTls12
  : public TlsConnectTestBase,
    public ::testing::WithParamInterface<std::string> {
 public:
  TlsConnectTls12();
};

// A TLS 1.3 only generic test.
class TlsConnectDatagram13
  : public TlsConnectTestBase {
 public:
  TlsConnectDatagram13()
      : TlsConnectTestBase(DGRAM, SSL_LIBRARY_VERSION_TLS_1_3) {}
};

// A variant that is used only with Pre13.
class TlsConnectGenericPre13 : public TlsConnectGeneric {
};

} // namespace nss_test

#endif
