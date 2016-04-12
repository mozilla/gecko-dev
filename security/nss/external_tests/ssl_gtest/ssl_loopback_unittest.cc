/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"
#include <memory>
#include <functional>

extern "C" {
// This is not something that should make you happy.
#include "libssl_internals.h"
}

#include "tls_parser.h"
#include "tls_filter.h"
#include "tls_connect.h"
#include "gtest_utils.h"

namespace nss_test {

uint8_t kBogusClientKeyExchange[] = {
  0x01, 0x00,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

// When we see the ClientKeyExchange from |client|, increment the
// ClientHelloVersion on |server|.
class TlsInspectorClientHelloVersionChanger : public TlsHandshakeFilter {
 public:
  TlsInspectorClientHelloVersionChanger(TlsAgent* server) : server_(server) {}

  virtual PacketFilter::Action FilterHandshake(
      const HandshakeHeader& header,
      const DataBuffer& input, DataBuffer* output) {
    if (header.handshake_type() == kTlsHandshakeClientKeyExchange) {
      EXPECT_EQ(
          SECSuccess,
          SSLInt_IncrementClientHandshakeVersion(server_->ssl_fd()));
    }
    return KEEP;
  }

 private:
  TlsAgent* server_;
};

// Set the version number in the ClientHello.
class TlsInspectorClientHelloVersionSetter : public TlsHandshakeFilter {
 public:
  TlsInspectorClientHelloVersionSetter(uint16_t version) : version_(version) {}

  virtual PacketFilter::Action FilterHandshake(
      const HandshakeHeader& header,
      const DataBuffer& input, DataBuffer* output) {
    if (header.handshake_type() == kTlsHandshakeClientHello) {
      *output = input;
      output->Write(0, version_, 2);
      return CHANGE;
    }
    return KEEP;
  }

 private:
  uint16_t version_;
};

class TlsServerKeyExchangeEcdhe {
 public:
  bool Parse(const DataBuffer& buffer) {
    TlsParser parser(buffer);

    uint8_t curve_type;
    if (!parser.Read(&curve_type)) {
      return false;
    }

    if (curve_type != 3) {  // named_curve
      return false;
    }

    uint32_t named_curve;
    if (!parser.Read(&named_curve, 2)) {
      return false;
    }

    return parser.ReadVariable(&public_key_, 1);
  }

  DataBuffer public_key_;
};

class TlsChaCha20Poly1305Test : public TlsConnectTls12 {
 public:
  void ConnectSendReceive(PRUint32 cipher_suite)
  {
    // Disable all ciphers.
    client_->DisableCiphersByKeyExchange(ssl_kea_rsa);
    client_->DisableCiphersByKeyExchange(ssl_kea_dh);
    client_->DisableCiphersByKeyExchange(ssl_kea_ecdh);

    // Re-enable ChaCha20/Poly1305.
    SECStatus rv = SSL_CipherPrefSet(client_->ssl_fd(), cipher_suite, PR_TRUE);
    EXPECT_EQ(SECSuccess, rv);

    Connect();
    SendReceive();

    // Check that we used the right cipher suite.
    int16_t actual, expected = static_cast<int16_t>(cipher_suite);
    EXPECT_TRUE(client_->cipher_suite(&actual) && actual == expected);
    EXPECT_TRUE(server_->cipher_suite(&actual) && actual == expected);
  }
};

TEST_P(TlsConnectGeneric, SetupOnly) {}

TEST_P(TlsConnectGeneric, Connect) {
  SetExpectedVersion(std::get<1>(GetParam()));
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa);
}

TEST_P(TlsConnectGeneric, ConnectEcdsa) {
  SetExpectedVersion(std::get<1>(GetParam()));
  ResetEcdsa();
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_ecdsa);
}

TEST_P(TlsConnectGenericPre13, ConnectFalseStart) {
  client_->EnableFalseStart();
  Connect();
  SendReceive();
}

TEST_P(TlsConnectGenericPre13, ConnectResumed) {
  ConfigureSessionCache(RESUME_SESSIONID, RESUME_SESSIONID);
  Connect();

  ResetRsa();
  ExpectResumption(RESUME_SESSIONID);
  Connect();
}

TEST_P(TlsConnectGeneric, ConnectClientCacheDisabled) {
  ConfigureSessionCache(RESUME_NONE, RESUME_SESSIONID);
  Connect();
  SendReceive();

  ResetRsa();
  ExpectResumption(RESUME_NONE);
  Connect();
  SendReceive();
}

TEST_P(TlsConnectGeneric, ConnectServerCacheDisabled) {
  ConfigureSessionCache(RESUME_SESSIONID, RESUME_NONE);
  Connect();
  SendReceive();

  ResetRsa();
  ExpectResumption(RESUME_NONE);
  Connect();
  SendReceive();
}

TEST_P(TlsConnectGeneric, ConnectSessionCacheDisabled) {
  ConfigureSessionCache(RESUME_NONE, RESUME_NONE);
  Connect();
  SendReceive();

  ResetRsa();
  ExpectResumption(RESUME_NONE);
  Connect();
  SendReceive();
}

TEST_P(TlsConnectGeneric, ConnectResumeSupportBoth) {
  // This prefers tickets.
  ConfigureSessionCache(RESUME_BOTH, RESUME_BOTH);
  Connect();
  SendReceive();

  ResetRsa();
  ConfigureSessionCache(RESUME_BOTH, RESUME_BOTH);
  ExpectResumption(RESUME_TICKET);
  Connect();
  SendReceive();
}

TEST_P(TlsConnectGeneric, ConnectResumeClientTicketServerBoth) {
  // This causes no resumption because the client needs the
  // session cache to resume even with tickets.
  ConfigureSessionCache(RESUME_TICKET, RESUME_BOTH);
  Connect();
  SendReceive();

  ResetRsa();
  ConfigureSessionCache(RESUME_TICKET, RESUME_BOTH);
  ExpectResumption(RESUME_NONE);
  Connect();
  SendReceive();
}

TEST_P(TlsConnectGeneric, ConnectResumeClientBothTicketServerTicket) {
  // This causes a ticket resumption.
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  Connect();
  SendReceive();

  ResetRsa();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  ExpectResumption(RESUME_TICKET);
  Connect();
  SendReceive();
}

TEST_P(TlsConnectGenericPre13, ConnectResumeClientServerTicketOnly) {
  // This causes no resumption because the client needs the
  // session cache to resume even with tickets.
  ConfigureSessionCache(RESUME_TICKET, RESUME_TICKET);
  Connect();
  SendReceive();

  ResetRsa();
  ConfigureSessionCache(RESUME_TICKET, RESUME_TICKET);
  ExpectResumption(RESUME_NONE);
  Connect();
  SendReceive();
}

TEST_P(TlsConnectGenericPre13, ConnectResumeClientBothServerNone) {
  ConfigureSessionCache(RESUME_BOTH, RESUME_NONE);
  Connect();
  SendReceive();

  ResetRsa();
  ConfigureSessionCache(RESUME_BOTH, RESUME_NONE);
  ExpectResumption(RESUME_NONE);
  Connect();
  SendReceive();
}

TEST_P(TlsConnectGenericPre13, ConnectResumeClientNoneServerBoth) {
  ConfigureSessionCache(RESUME_NONE, RESUME_BOTH);
  Connect();
  SendReceive();

  ResetRsa();
  ConfigureSessionCache(RESUME_NONE, RESUME_BOTH);
  ExpectResumption(RESUME_NONE);
  Connect();
  SendReceive();
}

TEST_P(TlsConnectGenericPre13, ConnectResumeWithHigherVersion) {
  EnsureTlsSetup();
  SetExpectedVersion(SSL_LIBRARY_VERSION_TLS_1_1);
  ConfigureSessionCache(RESUME_SESSIONID, RESUME_SESSIONID);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_1);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_1);
  Connect();

  ResetRsa();
  EnsureTlsSetup();
  SetExpectedVersion(SSL_LIBRARY_VERSION_TLS_1_2);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  ExpectResumption(RESUME_NONE);
  Connect();
}

TEST_P(TlsConnectGeneric, ConnectResumeClientBothTicketServerTicketForget) {
  // This causes a ticket resumption.
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  Connect();
  SendReceive();

  ResetRsa();
  ClearServerCache();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  ExpectResumption(RESUME_NONE);
  Connect();
  SendReceive();
}

TEST_P(TlsConnectGeneric, ClientAuth) {
  client_->SetupClientAuth();
  server_->RequestClientAuth(true);
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa);
}

// In TLS 1.3, the client sends its cert rejection on the
// second flight, and since it has already received the
// server's Finished, it transitions to complete and
// then gets an alert from the server. The test harness
// doesn't handle this right yet.
TEST_P(TlsConnectStream, DISABLED_ClientAuthRequiredRejected) {
  server_->RequestClientAuth(true);
  ConnectExpectFail();
}

TEST_P(TlsConnectGeneric, ClientAuthRequestedRejected) {
  server_->RequestClientAuth(false);
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa);
}


TEST_P(TlsConnectGeneric, ClientAuthEcdsa) {
  ResetEcdsa();
  client_->SetupClientAuth();
  server_->RequestClientAuth(true);
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_ecdsa);
}

static const SSLSignatureAndHashAlg SignatureEcdsaSha384[] = {
  {ssl_hash_sha384, ssl_sign_ecdsa}
};
static const SSLSignatureAndHashAlg SignatureEcdsaSha256[] = {
  {ssl_hash_sha256, ssl_sign_ecdsa}
};
static const SSLSignatureAndHashAlg SignatureRsaSha384[] = {
  {ssl_hash_sha384, ssl_sign_rsa}
};
static const SSLSignatureAndHashAlg SignatureRsaSha256[] = {
  {ssl_hash_sha256, ssl_sign_rsa}
};

// When signature algorithms match up, this should connect successfully; even
// for TLS 1.1 and 1.0, where they should be ignored.
TEST_P(TlsConnectGeneric, SignatureAlgorithmServerAuth) {
  client_->SetSignatureAlgorithms(SignatureEcdsaSha384,
                                  PR_ARRAY_SIZE(SignatureEcdsaSha384));
  server_->SetSignatureAlgorithms(SignatureEcdsaSha384,
                                  PR_ARRAY_SIZE(SignatureEcdsaSha384));
  ResetEcdsa();
  Connect();
}

// Here the client picks a single option, which should work in all versions.
// Defaults on the server include the first option.
TEST_P(TlsConnectGeneric, SignatureAlgorithmClientOnly) {
  const SSLSignatureAndHashAlg clientAlgorithms[] = {
    {ssl_hash_sha384, ssl_sign_ecdsa},
    {ssl_hash_sha384, ssl_sign_rsa}, // supported but unusable
    {ssl_hash_md5, ssl_sign_ecdsa} // unsupported and ignored
  };
  client_->SetSignatureAlgorithms(clientAlgorithms,
                                  PR_ARRAY_SIZE(clientAlgorithms));
  ResetEcdsa();
  Connect();
}

// Here the server picks a single option, which should work in all versions.
// Defaults on the client include the provided option.
TEST_P(TlsConnectGeneric, SignatureAlgorithmServerOnly) {
  server_->SetSignatureAlgorithms(SignatureEcdsaSha384,
                                  PR_ARRAY_SIZE(SignatureEcdsaSha384));
  ResetEcdsa();
  Connect();
}

// There is no need for overlap on signatures; since we don't actually use the
// signatures for static RSA, this should still connect successfully.
// This should also work in TLS 1.0 and 1.1 where the algorithms aren't used.
TEST_P(TlsConnectGenericPre13, SignatureAlgorithmNoOverlapStaticRsa) {
  client_->SetSignatureAlgorithms(SignatureRsaSha384,
                                  PR_ARRAY_SIZE(SignatureRsaSha384));
  server_->SetSignatureAlgorithms(SignatureRsaSha256,
                                  PR_ARRAY_SIZE(SignatureRsaSha256));
  DisableDheAndEcdheCiphers();
  Connect();
  CheckKeys(ssl_kea_rsa, ssl_auth_rsa);
}

TEST_P(TlsConnectGenericPre13, ConnectStaticRSA) {
  DisableDheAndEcdheCiphers();
  Connect();
  CheckKeys(ssl_kea_rsa, ssl_auth_rsa);
}

// Signature algorithms governs both verification and generation of signatures.
// With ECDSA, we need to at least have a common signature algorithm configured.
TEST_P(TlsConnectTls12, SignatureAlgorithmNoOverlapEcdsa) {
  ResetEcdsa();
  client_->SetSignatureAlgorithms(SignatureEcdsaSha384,
                                  PR_ARRAY_SIZE(SignatureEcdsaSha384));
  server_->SetSignatureAlgorithms(SignatureEcdsaSha256,
                                  PR_ARRAY_SIZE(SignatureEcdsaSha256));
  ConnectExpectFail();
}

// Pre 1.2, a mismatch on signature algorithms shouldn't affect anything.
TEST_P(TlsConnectPre12, SignatureAlgorithmNoOverlapEcdsa) {
  ResetEcdsa();
  client_->SetSignatureAlgorithms(SignatureEcdsaSha384,
                                  PR_ARRAY_SIZE(SignatureEcdsaSha384));
  server_->SetSignatureAlgorithms(SignatureEcdsaSha256,
                                  PR_ARRAY_SIZE(SignatureEcdsaSha256));
  Connect();
}

// The server requests client auth but doesn't offer a SHA-256 option.
// This fails because NSS only uses SHA-256 for handshake transcript hashes.
TEST_P(TlsConnectTls12, RequestClientAuthWithoutSha256) {
  server_->SetSignatureAlgorithms(SignatureRsaSha384,
                                  PR_ARRAY_SIZE(SignatureRsaSha384));
  server_->RequestClientAuth(false);
  ConnectExpectFail();
}

TEST_P(TlsConnectGeneric, ConnectAlpn) {
  EnableAlpn();
  Connect();
  client_->CheckAlpn(SSL_NEXT_PROTO_SELECTED, "a");
  server_->CheckAlpn(SSL_NEXT_PROTO_NEGOTIATED, "a");
}

TEST_P(TlsConnectDatagram, ConnectSrtp) {
  EnableSrtp();
  Connect();
  CheckSrtp();
  SendReceive();
}

// This class selectively drops complete writes.  This relies on the fact that
// writes in libssl are on record boundaries.
class SelectiveDropFilter : public PacketFilter, public PollTarget {
 public:
  SelectiveDropFilter(uint32_t pattern)
      : pattern_(pattern),
        counter_(0) {}

 protected:
  virtual Action Filter(const DataBuffer& input, DataBuffer* output) override {
    if (counter_ >= 32) {
      return KEEP;
    }
    return ((1 << counter_++) & pattern_) ? DROP : KEEP;
  }

 private:
  const uint32_t pattern_;
  uint8_t counter_;
};

TEST_P(TlsConnectDatagram, DropClientFirstFlightOnce) {
  client_->SetPacketFilter(new SelectiveDropFilter(0x1));
  Connect();
  SendReceive();
}

TEST_P(TlsConnectDatagram, DropServerFirstFlightOnce) {
  server_->SetPacketFilter(new SelectiveDropFilter(0x1));
  Connect();
  SendReceive();
}

// This drops the first transmission from both the client and server of all
// flights that they send.  Note: In DTLS 1.3, the shorter handshake means that
// this will also drop some application data, so we can't call SendReceive().
TEST_P(TlsConnectDatagram, DropAllFirstTransmissions) {
  client_->SetPacketFilter(new SelectiveDropFilter(0x15));
  server_->SetPacketFilter(new SelectiveDropFilter(0x5));
  Connect();
}

// This drops the server's first flight three times.
TEST_P(TlsConnectDatagram, DropServerFirstFlightThrice) {
  server_->SetPacketFilter(new SelectiveDropFilter(0x7));
  Connect();
}

// This drops the client's second flight three times.
TEST_P(TlsConnectDatagram, DropClientSecondFlightThrice) {
  client_->SetPacketFilter(new SelectiveDropFilter(0xe));
  Connect();
}

// This drops the server's second flight three times.
TEST_P(TlsConnectDatagram, DropServerSecondFlightThrice) {
  server_->SetPacketFilter(new SelectiveDropFilter(0xe));
  Connect();
}

// 1.3 is disabled in the next few tests because we don't
// presently support resumption in 1.3.
TEST_P(TlsConnectStreamPre13, ConnectAndClientRenegotiate) {
  Connect();
  server_->PrepareForRenegotiate();
  client_->StartRenegotiate();
  Handshake();
  CheckConnected();
}

TEST_P(TlsConnectStreamPre13, ConnectAndServerRenegotiate) {
  Connect();
  client_->PrepareForRenegotiate();
  server_->StartRenegotiate();
  Handshake();
  CheckConnected();
}

// TODO implement DHE for 1.3
TEST_P(TlsConnectGenericPre13, ConnectDhe) {
  DisableEcdheCiphers();
  Connect();
  CheckKeys(ssl_kea_dh, ssl_auth_rsa);
}

// Test that a totally bogus EPMS is handled correctly.
// This test is stream so we can catch the bad_record_mac alert.
TEST_P(TlsConnectStreamPre13, ConnectStaticRSABogusCKE) {
  DisableDheAndEcdheCiphers();
  TlsInspectorReplaceHandshakeMessage* i1 =
      new TlsInspectorReplaceHandshakeMessage(kTlsHandshakeClientKeyExchange,
                                              DataBuffer(
                                                  kBogusClientKeyExchange,
                                                  sizeof(kBogusClientKeyExchange)));
  client_->SetPacketFilter(i1);
  auto alert_recorder = new TlsAlertRecorder();
  server_->SetPacketFilter(alert_recorder);
  ConnectExpectFail();
  EXPECT_EQ(kTlsAlertFatal, alert_recorder->level());
  EXPECT_EQ(kTlsAlertBadRecordMac, alert_recorder->description());
}

// Test that a PMS with a bogus version number is handled correctly.
// This test is stream so we can catch the bad_record_mac alert.
TEST_P(TlsConnectStreamPre13, ConnectStaticRSABogusPMSVersionDetect) {
  DisableDheAndEcdheCiphers();
  client_->SetPacketFilter(new TlsInspectorClientHelloVersionChanger(
      server_));
  auto alert_recorder = new TlsAlertRecorder();
  server_->SetPacketFilter(alert_recorder);
  ConnectExpectFail();
  EXPECT_EQ(kTlsAlertFatal, alert_recorder->level());
  EXPECT_EQ(kTlsAlertBadRecordMac, alert_recorder->description());
}

// Test that a PMS with a bogus version number is ignored when
// rollback detection is disabled. This is a positive control for
// ConnectStaticRSABogusPMSVersionDetect.
TEST_P(TlsConnectGenericPre13, ConnectStaticRSABogusPMSVersionIgnore) {
  DisableDheAndEcdheCiphers();
  client_->SetPacketFilter(new TlsInspectorClientHelloVersionChanger(
      server_));
  server_->DisableRollbackDetection();
  Connect();
}

TEST_P(TlsConnectGeneric, ConnectEcdhe) {
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa);
}

// Prior to TLS 1.3, we were not fully ephemeral; though 1.3 fixes that
TEST_P(TlsConnectGenericPre13, ConnectEcdheTwiceReuseKey) {
  TlsInspectorRecordHandshakeMessage* i1 =
      new TlsInspectorRecordHandshakeMessage(kTlsHandshakeServerKeyExchange);
  server_->SetPacketFilter(i1);
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa);
  TlsServerKeyExchangeEcdhe dhe1;
  EXPECT_TRUE(dhe1.Parse(i1->buffer()));

  // Restart
  ResetRsa();
  TlsInspectorRecordHandshakeMessage* i2 =
      new TlsInspectorRecordHandshakeMessage(kTlsHandshakeServerKeyExchange);
  server_->SetPacketFilter(i2);
  ConfigureSessionCache(RESUME_NONE, RESUME_NONE);
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa);

  TlsServerKeyExchangeEcdhe dhe2;
  EXPECT_TRUE(dhe2.Parse(i2->buffer()));

  // Make sure they are the same.
  EXPECT_EQ(dhe1.public_key_.len(), dhe2.public_key_.len());
  EXPECT_TRUE(!memcmp(dhe1.public_key_.data(), dhe2.public_key_.data(),
                      dhe1.public_key_.len()));
}

// This test parses the ServerKeyExchange, which isn't in 1.3
TEST_P(TlsConnectGenericPre13, ConnectEcdheTwiceNewKey) {
  server_->EnsureTlsSetup();
  SECStatus rv =
      SSL_OptionSet(server_->ssl_fd(), SSL_REUSE_SERVER_ECDHE_KEY, PR_FALSE);
  EXPECT_EQ(SECSuccess, rv);
  TlsInspectorRecordHandshakeMessage* i1 =
      new TlsInspectorRecordHandshakeMessage(kTlsHandshakeServerKeyExchange);
  server_->SetPacketFilter(i1);
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa);
  TlsServerKeyExchangeEcdhe dhe1;
  EXPECT_TRUE(dhe1.Parse(i1->buffer()));

  // Restart
  ResetRsa();
  server_->EnsureTlsSetup();
  rv = SSL_OptionSet(server_->ssl_fd(), SSL_REUSE_SERVER_ECDHE_KEY, PR_FALSE);
  EXPECT_EQ(SECSuccess, rv);
  TlsInspectorRecordHandshakeMessage* i2 =
      new TlsInspectorRecordHandshakeMessage(kTlsHandshakeServerKeyExchange);
  server_->SetPacketFilter(i2);
  ConfigureSessionCache(RESUME_NONE, RESUME_NONE);
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa);

  TlsServerKeyExchangeEcdhe dhe2;
  EXPECT_TRUE(dhe2.Parse(i2->buffer()));

  // Make sure they are different.
  EXPECT_FALSE((dhe1.public_key_.len() == dhe2.public_key_.len()) &&
               (!memcmp(dhe1.public_key_.data(), dhe2.public_key_.data(),
                        dhe1.public_key_.len())));
}

TEST_P(TlsConnectGeneric, ConnectSendReceive) {
  Connect();
  SendReceive();
}

TEST_P(TlsChaCha20Poly1305Test, SendReceiveChaCha20Poly1305DheRsa) {
  ConnectSendReceive(TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256);
}

TEST_P(TlsChaCha20Poly1305Test, SendReceiveChaCha20Poly1305EcdheRsa) {
  ConnectSendReceive(TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256);
}

TEST_P(TlsChaCha20Poly1305Test, SendReceiveChaCha20Poly1305EcdheEcdsa) {
  ResetEcdsa();
  ConnectSendReceive(TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256);
}

// The next two tests takes advantage of the fact that we
// automatically read the first 1024 bytes, so if
// we provide 1200 bytes, they overrun the read buffer
// provided by the calling test.

// DTLS should return an error.
TEST_P(TlsConnectDatagram, ShortRead) {
  Connect();
  client_->SetExpectedReadError(true);
  server_->SendData(1200, 1200);
  WAIT_(client_->error_code() == SSL_ERROR_RX_SHORT_DTLS_READ, 2000);
  // Don't call CheckErrorCode() because it requires us to being
  // in state ERROR.
  ASSERT_EQ(SSL_ERROR_RX_SHORT_DTLS_READ, client_->error_code());

  // Now send and receive another packet.
  client_->SetExpectedReadError(false);
  server_->ResetSentBytes(); // Reset the counter.
  SendReceive();
}

// TLS should get the write in two chunks.
TEST_P(TlsConnectStream, ShortRead) {
  // This test behaves oddly with TLS 1.0 because of 1/n+1 splitting,
  // so skip in that case.
  if (version_ < SSL_LIBRARY_VERSION_TLS_1_1)
    return;

  Connect();
  server_->SendData(1200, 1200);
  // Read the first tranche.
  WAIT_(client_->received_bytes() == 1024, 2000);
  ASSERT_EQ(1024U, client_->received_bytes());
  // The second tranche should now immediately be available.
  client_->ReadBytes();
  ASSERT_EQ(1200U, client_->received_bytes());
}

TEST_P(TlsConnectGenericPre13, ConnectExtendedMasterSecret) {
  EnableExtendedMasterSecret();
  Connect();
  ResetRsa();
  ExpectResumption(RESUME_SESSIONID);
  EnableExtendedMasterSecret();
  Connect();
}

TEST_P(TlsConnectGenericPre13, ConnectExtendedMasterSecretStaticRSA) {
  DisableDheAndEcdheCiphers();
  EnableExtendedMasterSecret();
  Connect();
}

// This test is stream so we can catch the bad_record_mac alert.
TEST_P(TlsConnectStreamPre13, ConnectExtendedMasterSecretStaticRSABogusCKE) {
  DisableDheAndEcdheCiphers();
  EnableExtendedMasterSecret();
  TlsInspectorReplaceHandshakeMessage* inspect =
      new TlsInspectorReplaceHandshakeMessage(kTlsHandshakeClientKeyExchange,
                                              DataBuffer(
                                                  kBogusClientKeyExchange,
                                                  sizeof(kBogusClientKeyExchange)));
  client_->SetPacketFilter(inspect);
  auto alert_recorder = new TlsAlertRecorder();
  server_->SetPacketFilter(alert_recorder);
  ConnectExpectFail();
  EXPECT_EQ(kTlsAlertFatal, alert_recorder->level());
  EXPECT_EQ(kTlsAlertBadRecordMac, alert_recorder->description());
}

// This test is stream so we can catch the bad_record_mac alert.
TEST_P(TlsConnectStreamPre13, ConnectExtendedMasterSecretStaticRSABogusPMSVersionDetect) {
  DisableDheAndEcdheCiphers();
  EnableExtendedMasterSecret();
  client_->SetPacketFilter(new TlsInspectorClientHelloVersionChanger(
      server_));
  auto alert_recorder = new TlsAlertRecorder();
  server_->SetPacketFilter(alert_recorder);
  ConnectExpectFail();
  EXPECT_EQ(kTlsAlertFatal, alert_recorder->level());
  EXPECT_EQ(kTlsAlertBadRecordMac, alert_recorder->description());
}

TEST_P(TlsConnectStreamPre13, ConnectExtendedMasterSecretStaticRSABogusPMSVersionIgnore) {
  DisableDheAndEcdheCiphers();
  EnableExtendedMasterSecret();
  client_->SetPacketFilter(new TlsInspectorClientHelloVersionChanger(
      server_));
  server_->DisableRollbackDetection();
  Connect();
}

TEST_P(TlsConnectGenericPre13, ConnectExtendedMasterSecretECDHE) {
  EnableExtendedMasterSecret();
  Connect();

  ResetRsa();
  EnableExtendedMasterSecret();
  ExpectResumption(RESUME_SESSIONID);
  Connect();
}

TEST_P(TlsConnectGenericPre13, ConnectExtendedMasterSecretTicket) {
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  EnableExtendedMasterSecret();
  Connect();

  ResetRsa();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);

  EnableExtendedMasterSecret();
  ExpectResumption(RESUME_TICKET);
  Connect();
}

TEST_P(TlsConnectGenericPre13,
       ConnectExtendedMasterSecretClientOnly) {
  client_->EnableExtendedMasterSecret();
  ExpectExtendedMasterSecret(false);
  Connect();
}

TEST_P(TlsConnectGenericPre13,
       ConnectExtendedMasterSecretServerOnly) {
  server_->EnableExtendedMasterSecret();
  ExpectExtendedMasterSecret(false);
  Connect();
}

TEST_P(TlsConnectGenericPre13,
       ConnectExtendedMasterSecretResumeWithout) {
  EnableExtendedMasterSecret();
  Connect();

  ResetRsa();
  server_->EnableExtendedMasterSecret();
  auto alert_recorder = new TlsAlertRecorder();
  server_->SetPacketFilter(alert_recorder);
  ConnectExpectFail();
  EXPECT_EQ(kTlsAlertFatal, alert_recorder->level());
  EXPECT_EQ(kTlsAlertHandshakeFailure, alert_recorder->description());
}

TEST_P(TlsConnectGenericPre13,
       ConnectNormalResumeWithExtendedMasterSecret) {
  ConfigureSessionCache(RESUME_SESSIONID, RESUME_SESSIONID);
  ExpectExtendedMasterSecret(false);
  Connect();

  ResetRsa();
  EnableExtendedMasterSecret();
  ExpectResumption(RESUME_NONE);
  Connect();
}

TEST_P(TlsConnectGeneric, ConnectWithCompressionMaybe)
{
  EnsureTlsSetup();
  client_->EnableCompression();
  server_->EnableCompression();
  Connect();
  EXPECT_EQ(client_->version() < SSL_LIBRARY_VERSION_TLS_1_3 &&
            mode_ != DGRAM, client_->is_compressed());
  SendReceive();
}


TEST_P(TlsConnectStream, ServerNegotiateTls10) {
  uint16_t minver, maxver;
  client_->GetVersionRange(&minver, &maxver);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_0,
                           maxver);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_0,
                           SSL_LIBRARY_VERSION_TLS_1_0);
  Connect();
}

TEST_P(TlsConnectGeneric, ServerNegotiateTls11) {
  if (version_ < SSL_LIBRARY_VERSION_TLS_1_1)
    return;

  uint16_t minver, maxver;
  client_->GetVersionRange(&minver, &maxver);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           maxver);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_1);
  Connect();
}

TEST_P(TlsConnectGeneric, ServerNegotiateTls12) {
  if (version_ < SSL_LIBRARY_VERSION_TLS_1_2)
    return;

  uint16_t minver, maxver;
  client_->GetVersionRange(&minver, &maxver);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           maxver);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  Connect();
}

// Test the ServerRandom version hack from
// [draft-ietf-tls-tls13-11 Section 6.3.1.1].
// The first three tests test for active tampering. The next
// two validate that we can also detect fallback using the
// SSL_SetDowngradeCheckVersion() API.
TEST_F(TlsConnectTest, TestDowngradeDetectionToTls11) {
  client_->SetPacketFilter(new TlsInspectorClientHelloVersionSetter
                           (SSL_LIBRARY_VERSION_TLS_1_1));
  ConnectExpectFail();
  ASSERT_EQ(SSL_ERROR_RX_MALFORMED_SERVER_HELLO, client_->error_code());
}

/* Attempt to negotiate the bogus DTLS 1.1 version. */
TEST_F(DtlsConnectTest, TestDtlsVersion11) {
  client_->SetPacketFilter(new TlsInspectorClientHelloVersionSetter(
      ((~0x0101) & 0xffff)));
  ConnectExpectFail();
  // It's kind of surprising that SSL_ERROR_NO_CYPHER_OVERLAP is
  // what is returned here, but this is deliberate in ssl3_HandleAlert().
  EXPECT_EQ(SSL_ERROR_NO_CYPHER_OVERLAP, client_->error_code());
  EXPECT_EQ(SSL_ERROR_UNSUPPORTED_VERSION, server_->error_code());
}

#ifdef NSS_ENABLE_TLS_1_3
TEST_F(TlsConnectTest, TestDowngradeDetectionToTls12) {
  EnsureTlsSetup();
  client_->SetPacketFilter(new TlsInspectorClientHelloVersionSetter
                           (SSL_LIBRARY_VERSION_TLS_1_2));
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  ConnectExpectFail();
  ASSERT_EQ(SSL_ERROR_RX_MALFORMED_SERVER_HELLO, client_->error_code());
}
#endif

// TLS 1.1 clients do not check the random values, so we should
// instead get a handshake failure alert from the server.
TEST_F(TlsConnectTest, TestDowngradeDetectionToTls10) {
  client_->SetPacketFilter(new TlsInspectorClientHelloVersionSetter
                          (SSL_LIBRARY_VERSION_TLS_1_0));
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_0,
                           SSL_LIBRARY_VERSION_TLS_1_1);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_0,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  ConnectExpectFail();
  ASSERT_EQ(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE, server_->error_code());
  ASSERT_EQ(SSL_ERROR_DECRYPT_ERROR_ALERT, client_->error_code());
}

TEST_F(TlsConnectTest, TestFallbackFromTls12) {
  EnsureTlsSetup();
  client_->SetDowngradeCheckVersion(SSL_LIBRARY_VERSION_TLS_1_2);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_1);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  ConnectExpectFail();
  ASSERT_EQ(SSL_ERROR_RX_MALFORMED_SERVER_HELLO, client_->error_code());
}

#ifdef NSS_ENABLE_TLS_1_3
TEST_F(TlsConnectTest, TestFallbackFromTls13) {
  EnsureTlsSetup();
  client_->SetDowngradeCheckVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  ConnectExpectFail();
  ASSERT_EQ(SSL_ERROR_RX_MALFORMED_SERVER_HELLO, client_->error_code());
}

// Test that two TLS resumptions work and produce the same ticket.
// This will change after bug 1257047 is fixed.
TEST_F(TlsConnectTest, TestTls13ResumptionTwice) {
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  Connect();
  SendReceive(); // Need to read so that we absorb the session ticket.
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa);

  ResetRsa();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  TlsExtensionCapture *c1 =
      new TlsExtensionCapture(kTlsExtensionPreSharedKey);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  ExpectResumption(RESUME_TICKET);
  Connect();
  SendReceive();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa);
  DataBuffer psk1(c1->extension());
  ASSERT_GE(psk1.len(), 0UL);

  ResetRsa();
  ClearStats();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  TlsExtensionCapture *c2 =
      new TlsExtensionCapture(kTlsExtensionPreSharedKey);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  ExpectResumption(RESUME_TICKET);
  Connect();
  SendReceive();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa);
  DataBuffer psk2(c2->extension());
  ASSERT_GE(psk2.len(), 0UL);

  // TODO(ekr@rtfm.com): This will change when we fix bug 1257047.
  ASSERT_EQ(psk1, psk2);
}

#endif

class BeforeFinished : public TlsRecordFilter {
 private:
  enum HandshakeState {
    BEFORE_CCS,
    AFTER_CCS,
    DONE
  };
  typedef std::function<void(void)> VoidFunction;

 public:
  BeforeFinished(TlsAgent* client, TlsAgent* server,
                 VoidFunction before_ccs, VoidFunction before_finished)
      : client_(client),
        server_(server),
        before_ccs_(before_ccs),
        before_finished_(before_finished),
        state_(BEFORE_CCS) {}

 protected:
  virtual PacketFilter::Action FilterRecord(
      const RecordHeader& header, const DataBuffer& body, DataBuffer* out) {
    switch (state_) {
      case BEFORE_CCS:
        // Awaken when we see the CCS.
        if (header.content_type() == kTlsChangeCipherSpecType) {
          before_ccs_();

          // Write the CCS out as a separate write, so that we can make
          // progress. Ordinarily, libssl sends the CCS and Finished together,
          // but that means that they both get processed together.
          DataBuffer ccs;
          header.Write(&ccs, 0, body);
          server_->SendDirect(ccs);
          client_->Handshake();
          state_ = AFTER_CCS;
          // Request that the original record be dropped by the filter.
          return DROP;
        }
        break;

      case AFTER_CCS:
        EXPECT_EQ(kTlsHandshakeType, header.content_type());
        // This could check that data contains a Finished message, but it's
        // encrypted, so that's too much extra work.

        before_finished_();
        state_ = DONE;
        break;

      case DONE:
        break;
    }
    return KEEP;
  }

 private:
  TlsAgent* client_;
  TlsAgent* server_;
  VoidFunction before_ccs_;
  VoidFunction before_finished_;
  HandshakeState state_;
};

TEST_P(TlsConnectGenericPre13, ClientWriteBetweenCCSAndFinishedWithFalseStart) {
  client_->EnableFalseStart();
  server_->SetPacketFilter(new BeforeFinished(client_, server_, [this]() {
        EXPECT_TRUE(client_->can_falsestart_hook_called());
      }, [this]() {
        // Write something, which used to fail: bug 1235366.
        client_->SendData(10);
      }));

  Connect();
  server_->SendData(10);
  Receive(10);
}

TEST_P(TlsConnectGenericPre13, AuthCompleteBeforeFinishedWithFalseStart) {
  client_->EnableFalseStart();
  client_->SetAuthCertificateCallback(
      [](TlsAgent&, PRBool, PRBool) -> SECStatus {
        return SECWouldBlock;
      });
  server_->SetPacketFilter(new BeforeFinished(client_, server_, []() {
        // Do nothing before CCS
      }, [this]() {
        EXPECT_FALSE(client_->can_falsestart_hook_called());
        // AuthComplete before Finished still enables false start.
        EXPECT_EQ(SECSuccess, SSL_AuthCertificateComplete(client_->ssl_fd(), 0));
        EXPECT_TRUE(client_->can_falsestart_hook_called());
        client_->SendData(10);
      }));

  Connect();
  server_->SendData(10);
  Receive(10);
}

// Running code after the client has started processing the encrypted part of
// the server's first flight, but before the Finished is processed is very hard
// in TLS 1.3.  These encrypted messages are sent in a single encrypted blob.
// The following test uses DTLS to make it possible to force the client to
// process the handshake in pieces.
//
// The first encrypted message from the server is dropped, and the MTU is
// reduced to just below the original message size so that the server sends two
// messages.  The Finished message is then processed with the sec.
class BeforeFinished13 : public PacketFilter {
 private:
  enum HandshakeState {
    INIT,
    BEFORE_FIRST_FRAGMENT,
    BEFORE_SECOND_FRAGMENT,
    DONE
  };
  typedef std::function<void(void)> VoidFunction;

 public:
  BeforeFinished13(TlsAgent* client, TlsAgent *server,
                   VoidFunction before_finished)
      : client_(client),
        server_(server),
        before_finished_(before_finished),
        records_(0) {}

 protected:
  virtual PacketFilter::Action Filter(const DataBuffer& input, DataBuffer* output) {
    switch (++records_) {
      case 1:
        SSLInt_SetMTU(server_->ssl_fd(), input.len() - 1);
        return DROP;

      case 3:
        client_->Handshake();
        before_finished_();
        break;

      default:
        break;
    }
    return KEEP;
  }

 private:
  TlsAgent *client_;
  TlsAgent *server_;
  VoidFunction before_finished_;
  size_t records_;
};

TEST_F(TlsConnectDatagram13, AuthCompleteBeforeFinished) {
  client_->SetAuthCertificateCallback(
      [](TlsAgent&, PRBool, PRBool) -> SECStatus {
        return SECWouldBlock;
      });
  server_->SetPacketFilter(new BeforeFinished13(client_, server_, [this]() {
        EXPECT_EQ(SECSuccess, SSL_AuthCertificateComplete(client_->ssl_fd(), 0));
      }));
  Connect();
}

static void TriggerAuthComplete(PollTarget *target, Event event) {
  std::cerr << "client: call SSL_AuthCertificateComplete" << std::endl;
  EXPECT_EQ(TIMER_EVENT, event);
  TlsAgent* client = static_cast<TlsAgent*>(target);
  EXPECT_EQ(SECSuccess, SSL_AuthCertificateComplete(client->ssl_fd(), 0));
}

TEST_F(TlsConnectDatagram13, AuthCompleteAfterFinished) {
  client_->SetAuthCertificateCallback(
      [this](TlsAgent&, PRBool, PRBool) -> SECStatus {
        Poller::Timer *timer_handle;
        // This is really just to unroll the stack.
        Poller::Instance()->SetTimer(1U, client_, TriggerAuthComplete,
                                     &timer_handle);
        return SECWouldBlock;
      });
  Connect();
}

INSTANTIATE_TEST_CASE_P(GenericStream, TlsConnectGeneric,
                        ::testing::Combine(
                          TlsConnectTestBase::kTlsModesStream,
                          TlsConnectTestBase::kTlsVAll));
INSTANTIATE_TEST_CASE_P(GenericDatagram, TlsConnectGeneric,
                        ::testing::Combine(
                          TlsConnectTestBase::kTlsModesDatagram,
                          TlsConnectTestBase::kTlsV11Plus));

INSTANTIATE_TEST_CASE_P(StreamOnly, TlsConnectStream,
                        TlsConnectTestBase::kTlsVAll);
INSTANTIATE_TEST_CASE_P(DatagramOnly, TlsConnectDatagram,
                        TlsConnectTestBase::kTlsV11Plus);

INSTANTIATE_TEST_CASE_P(ChaCha20, TlsChaCha20Poly1305Test,
                        TlsConnectTestBase::kTlsModesAll);

INSTANTIATE_TEST_CASE_P(Pre12Stream, TlsConnectPre12,
                        ::testing::Combine(
                          TlsConnectTestBase::kTlsModesStream,
                          TlsConnectTestBase::kTlsV10V11));
INSTANTIATE_TEST_CASE_P(Pre12Datagram, TlsConnectPre12,
                        ::testing::Combine(
                          TlsConnectTestBase::kTlsModesDatagram,
                          TlsConnectTestBase::kTlsV11));

INSTANTIATE_TEST_CASE_P(Version12Only, TlsConnectTls12,
                        TlsConnectTestBase::kTlsModesAll);

INSTANTIATE_TEST_CASE_P(Pre13Stream, TlsConnectGenericPre13,
                        ::testing::Combine(
                          TlsConnectTestBase::kTlsModesStream,
                          TlsConnectTestBase::kTlsV10To12));
INSTANTIATE_TEST_CASE_P(Pre13Datagram, TlsConnectGenericPre13,
                        ::testing::Combine(
                             TlsConnectTestBase::kTlsModesDatagram,
                             TlsConnectTestBase::kTlsV11V12));
INSTANTIATE_TEST_CASE_P(Pre13StreamOnly, TlsConnectStreamPre13,
                        TlsConnectTestBase::kTlsV10To12);
}  // namespace nspr_test
