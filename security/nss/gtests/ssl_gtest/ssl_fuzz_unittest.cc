/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <limits>
#include <unordered_set>

#include "blapi.h"
#include "ssl.h"
#include "sslimpl.h"
#include "tls_connect.h"

#include "gtest/gtest.h"

namespace nss_test {

#ifdef UNSAFE_FUZZER_MODE
#define FUZZ_F(c, f) TEST_F(c, Fuzz_##f)
#define FUZZ_P(c, f) TEST_P(c, Fuzz_##f)
#else
#define FUZZ_F(c, f) TEST_F(c, DISABLED_Fuzz_##f)
#define FUZZ_P(c, f) TEST_P(c, DISABLED_Fuzz_##f)
#endif

static std::unordered_set<PRInt32> gFuzzedSslOptions = {
    SSL_SECURITY,             // irrelevant
    SSL_SOCKS,                // irrelevant
    SSL_REQUEST_CERTIFICATE,  // tls_server
    SSL_HANDSHAKE_AS_CLIENT,  // irrelevant
    SSL_HANDSHAKE_AS_SERVER,  // irrelevant
    SSL_ENABLE_SSL2,          // obsolete
    SSL_ENABLE_SSL3,          // obsolete
    SSL_NO_CACHE,             // tls_client, tls_server
    SSL_REQUIRE_CERTIFICATE,  // tls_server
    SSL_ENABLE_FDX,
    SSL_V2_COMPATIBLE_HELLO,  // obsolete
    SSL_ENABLE_TLS,           // obsolete
    SSL_ROLLBACK_DETECTION,
    SSL_NO_STEP_DOWN,            // unsupported
    SSL_BYPASS_PKCS11,           // unsupported
    SSL_NO_LOCKS,                // tls_client, tls_server
    SSL_ENABLE_SESSION_TICKETS,  // tls_client, tls_server
    SSL_ENABLE_DEFLATE,          // tls_client, tls_server
    SSL_ENABLE_RENEGOTIATION,
    SSL_REQUIRE_SAFE_NEGOTIATION,  // tls_client, tls_server
    SSL_ENABLE_FALSE_START,        // tls_client
    SSL_CBC_RANDOM_IV,             // tls_client, tls_server
    SSL_ENABLE_OCSP_STAPLING,      // tls_client
    SSL_ENABLE_NPN,                // defunct
    SSL_ENABLE_ALPN,               // tls_client, tls_server
    SSL_REUSE_SERVER_ECDHE_KEY,
    SSL_ENABLE_FALLBACK_SCSV,  // tls_client, tls_server
    SSL_ENABLE_SERVER_DHE,
    SSL_ENABLE_EXTENDED_MASTER_SECRET,  // tls_client, tls_server
    SSL_ENABLE_SIGNED_CERT_TIMESTAMPS,
    SSL_REQUIRE_DH_NAMED_GROUPS,  // tls_client
    SSL_ENABLE_0RTT_DATA,         // tls_client, tls_server
    SSL_RECORD_SIZE_LIMIT,
    SSL_ENABLE_TLS13_COMPAT_MODE,  // tls_client
    SSL_ENABLE_DTLS_SHORT_HEADER,  // tls_client, tls_server
    SSL_ENABLE_HELLO_DOWNGRADE_CHECK,
    SSL_ENABLE_V2_COMPATIBLE_HELLO,
    SSL_ENABLE_POST_HANDSHAKE_AUTH,    // tls_client
    SSL_ENABLE_DELEGATED_CREDENTIALS,  // tls_client, tls_server
    SSL_SUPPRESS_END_OF_EARLY_DATA,
    SSL_ENABLE_GREASE,                    // tls_client, tls_server
    SSL_ENABLE_CH_EXTENSION_PERMUTATION,  // tls_client
};

const uint8_t kShortEmptyFinished[8] = {0};
const uint8_t kLongEmptyFinished[128] = {0};

class TlsFuzzTest : public TlsConnectGeneric {};

// Record the application data stream.
class TlsApplicationDataRecorder : public TlsRecordFilter {
 public:
  TlsApplicationDataRecorder(const std::shared_ptr<TlsAgent>& a)
      : TlsRecordFilter(a), buffer_() {}

  virtual PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                            const DataBuffer& input,
                                            DataBuffer* output) {
    if (header.content_type() == ssl_ct_application_data) {
      buffer_.Append(input);
    }

    return KEEP;
  }

  const DataBuffer& buffer() const { return buffer_; }

 private:
  DataBuffer buffer_;
};

// Check that due to the deterministic PRNG we derive
// the same master secret in two consecutive TLS sessions.
FUZZ_P(TlsFuzzTest, DeterministicExporter) {
  const char kLabel[] = "label";
  std::vector<unsigned char> out1(32), out2(32);

  // Make sure we have RSA blinding params.
  Connect();

  Reset();
  ConfigureSessionCache(RESUME_NONE, RESUME_NONE);

  // Reset the RNG state.
  EXPECT_EQ(SECSuccess, RNG_RandomUpdate(NULL, 0));
  Connect();

  // Export a key derived from the MS and nonces.
  SECStatus rv =
      SSL_ExportKeyingMaterial(client_->ssl_fd(), kLabel, strlen(kLabel), false,
                               NULL, 0, out1.data(), out1.size());
  EXPECT_EQ(SECSuccess, rv);

  Reset();
  ConfigureSessionCache(RESUME_NONE, RESUME_NONE);

  // Reset the RNG state.
  EXPECT_EQ(SECSuccess, RNG_RandomUpdate(NULL, 0));
  Connect();

  // Export another key derived from the MS and nonces.
  rv = SSL_ExportKeyingMaterial(client_->ssl_fd(), kLabel, strlen(kLabel),
                                false, NULL, 0, out2.data(), out2.size());
  EXPECT_EQ(SECSuccess, rv);

  // The two exported keys should be the same.
  EXPECT_EQ(out1, out2);
}

// Check that due to the deterministic RNG two consecutive
// TLS sessions will have the exact same transcript.
FUZZ_P(TlsFuzzTest, DeterministicTranscript) {
  // Make sure we have RSA blinding params.
  Connect();

  // Connect a few times and compare the transcripts byte-by-byte.
  DataBuffer last;
  for (size_t i = 0; i < 5; i++) {
    Reset();
    ConfigureSessionCache(RESUME_NONE, RESUME_NONE);

    DataBuffer buffer;
    MakeTlsFilter<TlsConversationRecorder>(client_, buffer);
    MakeTlsFilter<TlsConversationRecorder>(server_, buffer);

    // Reset the RNG state.
    EXPECT_EQ(SECSuccess, RNG_RandomUpdate(NULL, 0));
    Connect();

    // Ensure the filters go away before |buffer| does.
    client_->ClearFilter();
    server_->ClearFilter();

    if (last.len() > 0) {
      EXPECT_EQ(last, buffer);
    }

    last = buffer;
  }
}

// Check that we can establish and use a connection
// with all supported TLS versions, STREAM and DGRAM.
// Check that records are NOT encrypted.
// Check that records don't have a MAC.
FUZZ_P(TlsFuzzTest, ConnectSendReceive_NullCipher) {
  // Set up app data filters.
  auto client_recorder = MakeTlsFilter<TlsApplicationDataRecorder>(client_);
  auto server_recorder = MakeTlsFilter<TlsApplicationDataRecorder>(server_);

  Connect();

  // Construct the plaintext.
  DataBuffer buf;
  buf.Allocate(50);
  for (size_t i = 0; i < buf.len(); ++i) {
    buf.data()[i] = i & 0xff;
  }

  // Send/Receive data.
  client_->SendBuffer(buf);
  server_->SendBuffer(buf);
  Receive(buf.len());

  // Check for plaintext on the wire.
  EXPECT_EQ(buf, client_recorder->buffer());
  EXPECT_EQ(buf, server_recorder->buffer());
}

// Check that an invalid Finished message doesn't abort the connection.
FUZZ_P(TlsFuzzTest, BogusClientFinished) {
  EnsureTlsSetup();

  MakeTlsFilter<TlsInspectorReplaceHandshakeMessage>(
      client_, kTlsHandshakeFinished,
      DataBuffer(kShortEmptyFinished, sizeof(kShortEmptyFinished)));
  Connect();
  SendReceive();
}

// Check that an invalid Finished message doesn't abort the connection.
FUZZ_P(TlsFuzzTest, BogusServerFinished) {
  EnsureTlsSetup();

  MakeTlsFilter<TlsInspectorReplaceHandshakeMessage>(
      server_, kTlsHandshakeFinished,
      DataBuffer(kLongEmptyFinished, sizeof(kLongEmptyFinished)));
  Connect();
  SendReceive();
}

// Check that an invalid server auth signature doesn't abort the connection.
FUZZ_P(TlsFuzzTest, BogusServerAuthSignature) {
  EnsureTlsSetup();
  uint8_t msg_type = version_ == SSL_LIBRARY_VERSION_TLS_1_3
                         ? kTlsHandshakeCertificateVerify
                         : kTlsHandshakeServerKeyExchange;
  MakeTlsFilter<TlsLastByteDamager>(server_, msg_type);
  Connect();
  SendReceive();
}

// Check that an invalid client auth signature doesn't abort the connection.
FUZZ_P(TlsFuzzTest, BogusClientAuthSignature) {
  EnsureTlsSetup();
  client_->SetupClientAuth();
  server_->RequestClientAuth(true);
  MakeTlsFilter<TlsLastByteDamager>(client_, kTlsHandshakeCertificateVerify);
  Connect();
}

// Check that session ticket resumption works.
FUZZ_P(TlsFuzzTest, SessionTicketResumption) {
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  Connect();
  SendReceive();

  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  ExpectResumption(RESUME_TICKET);
  Connect();
  SendReceive();
}

// Check that session tickets are not encrypted.
FUZZ_P(TlsFuzzTest, UnencryptedSessionTickets) {
  ConfigureSessionCache(RESUME_TICKET, RESUME_TICKET);

  auto filter = MakeTlsFilter<TlsHandshakeRecorder>(
      server_, kTlsHandshakeNewSessionTicket);
  Connect();

  std::cerr << "ticket" << filter->buffer() << std::endl;
  size_t offset = 4;  // Skip lifetime.

  if (version_ == SSL_LIBRARY_VERSION_TLS_1_3) {
    offset += 4;  // Skip ticket_age_add.
    uint32_t nonce_len = 0;
    EXPECT_TRUE(filter->buffer().Read(offset, 1, &nonce_len));
    offset += 1 + nonce_len;
  }

  offset += 2;  // Skip the ticket length.

  // This bit parses the contents of the ticket, which would ordinarily be
  // encrypted.  Start by checking that we have the right version.  This needs
  // to be updated every time that TLS_EX_SESS_TICKET_VERSION is changed.  But
  // we don't use the #define.  That way, any time that code is updated, this
  // test will fail unless it is manually checked.
  uint32_t ticket_version;
  EXPECT_TRUE(filter->buffer().Read(offset, 2, &ticket_version));
  EXPECT_EQ(0x010aU, ticket_version);
  offset += 2;

  // Check the protocol version number.
  uint32_t tls_version = 0;
  EXPECT_TRUE(filter->buffer().Read(offset, sizeof(version_), &tls_version));
  EXPECT_EQ(version_, static_cast<decltype(version_)>(tls_version));
  offset += sizeof(version_);

  // Check the cipher suite.
  uint32_t suite = 0;
  EXPECT_TRUE(filter->buffer().Read(offset, 2, &suite));
  client_->CheckCipherSuite(static_cast<uint16_t>(suite));
}

class MiscFuzzTest : public ::testing::Test {};

FUZZ_F(MiscFuzzTest, UnfuzzedSslOption) {
  PRIntn val;
  SECStatus rv;

  for (PRInt32 option = 0; option < std::numeric_limits<PRUint8>::max();
       ++option) {
    rv = SSL_OptionGetDefault(option, &val);
    // The return value should either be  a failure (=> there is no such
    // option) or the the option should be in the fuzzed options.
    EXPECT_TRUE(rv == SECFailure || gFuzzedSslOptions.count(option));
  }
}

INSTANTIATE_TEST_SUITE_P(
    FuzzStream, TlsFuzzTest,
    ::testing::Combine(TlsConnectTestBase::kTlsVariantsStream,
                       TlsConnectTestBase::kTlsVAll));
INSTANTIATE_TEST_SUITE_P(
    FuzzDatagram, TlsFuzzTest,
    ::testing::Combine(TlsConnectTestBase::kTlsVariantsDatagram,
                       TlsConnectTestBase::kTlsV11Plus));
}  // namespace nss_test
