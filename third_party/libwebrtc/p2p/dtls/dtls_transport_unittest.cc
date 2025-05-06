/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/dtls/dtls_transport.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/crypto/crypto_options.h"
#include "api/dtls_transport_interface.h"
#include "api/scoped_refptr.h"
#include "api/test/rtc_error_matchers.h"
#include "api/units/time_delta.h"
#include "p2p/base/packet_transport_internal.h"
#include "p2p/base/transport_description.h"
#include "p2p/dtls/dtls_transport_internal.h"
#include "p2p/dtls/dtls_utils.h"
#include "p2p/test/fake_ice_transport.h"
#include "rtc_base/buffer.h"
#include "rtc_base/byte_order.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/logging.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/ssl_fingerprint.h"
#include "rtc_base/ssl_identity.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/wait_until.h"

#define MAYBE_SKIP_TEST(feature)                              \
  if (!(rtc::SSLStreamAdapter::feature())) {                  \
    GTEST_SKIP() << #feature " feature disabled... skipping"; \
  }

namespace cricket {

using ::testing::Eq;
using ::testing::IsTrue;

static const size_t kPacketNumOffset = 8;
static const size_t kPacketHeaderLen = 12;
static const int kFakePacketId = 0x1234;
static const int kTimeout = 10000;

const uint8_t kRtpLeadByte = 0x80;

static bool IsRtpLeadByte(uint8_t b) {
  return b == kRtpLeadByte;
}

// `modify_digest` is used to set modified fingerprints that are meant to fail
// validation.
void SetRemoteFingerprintFromCert(
    DtlsTransport* transport,
    const rtc::scoped_refptr<webrtc::RTCCertificate>& cert,
    bool modify_digest = false) {
  std::unique_ptr<rtc::SSLFingerprint> fingerprint =
      rtc::SSLFingerprint::CreateFromCertificate(*cert);
  if (modify_digest) {
    ++fingerprint->digest.MutableData()[0];
  }

  // Even if digest is verified to be incorrect, should fail asynchronously.
  EXPECT_TRUE(
      transport
          ->SetRemoteParameters(
              fingerprint->algorithm,
              reinterpret_cast<const uint8_t*>(fingerprint->digest.data()),
              fingerprint->digest.size(), std::nullopt)
          .ok());
}

class DtlsTestClient : public sigslot::has_slots<> {
 public:
  explicit DtlsTestClient(absl::string_view name) : name_(name) {}
  void CreateCertificate(rtc::KeyType key_type) {
    certificate_ = webrtc::RTCCertificate::Create(
        rtc::SSLIdentity::Create(name_, key_type));
  }
  const rtc::scoped_refptr<webrtc::RTCCertificate>& certificate() {
    return certificate_;
  }
  void SetupMaxProtocolVersion(webrtc::SSLProtocolVersion version) {
    ssl_max_version_ = version;
  }
  void set_async_delay(int async_delay_ms) { async_delay_ms_ = async_delay_ms; }

  // Set up fake ICE transport and real DTLS transport under test.
  void SetupTransports(IceRole role, bool rtt_estimate = true) {
    dtls_transport_ = nullptr;
    fake_ice_transport_ = nullptr;

    fake_ice_transport_.reset(
        new FakeIceTransport(absl::StrCat("fake-", name_), 0));
    if (rtt_estimate) {
      fake_ice_transport_->set_rtt_estimate(
          async_delay_ms_ ? std::optional<int>(async_delay_ms_) : std::nullopt,
          /* async= */ true);
    } else if (async_delay_ms_) {
      fake_ice_transport_->SetAsync(async_delay_ms_);
      fake_ice_transport_->SetAsyncDelay(async_delay_ms_);
    }
    fake_ice_transport_->SetIceRole(role);
    // Hook the raw packets so that we can verify they are encrypted.
    fake_ice_transport_->RegisterReceivedPacketCallback(
        this, [&](rtc::PacketTransportInternal* transport,
                  const rtc::ReceivedPacket& packet) {
          OnFakeIceTransportReadPacket(transport, packet);
        });

    dtls_transport_ = std::make_unique<DtlsTransport>(
        fake_ice_transport_.get(), webrtc::CryptoOptions(),
        /*event_log=*/nullptr, ssl_max_version_);
    // Note: Certificate may be null here if testing passthrough.
    dtls_transport_->SetLocalCertificate(certificate_);
    dtls_transport_->SignalWritableState.connect(
        this, &DtlsTestClient::OnTransportWritableState);
    dtls_transport_->RegisterReceivedPacketCallback(
        this, [&](rtc::PacketTransportInternal* transport,
                  const rtc::ReceivedPacket& packet) {
          OnTransportReadPacket(transport, packet);
        });
    dtls_transport_->SignalSentPacket.connect(
        this, &DtlsTestClient::OnTransportSentPacket);
  }

  FakeIceTransport* fake_ice_transport() {
    return static_cast<FakeIceTransport*>(dtls_transport_->ice_transport());
  }

  DtlsTransport* dtls_transport() { return dtls_transport_.get(); }

  // Simulate fake ICE transports connecting.
  bool Connect(DtlsTestClient* peer, bool asymmetric) {
    fake_ice_transport()->SetDestination(peer->fake_ice_transport(),
                                         asymmetric);
    return true;
  }

  // Connect the fake ICE transports so that packets flows from one to other.
  bool ConnectIceTransport(DtlsTestClient* peer) {
    fake_ice_transport()->SetDestinationNotWritable(peer->fake_ice_transport());
    return true;
  }

  bool SendIcePing() { return fake_ice_transport_->SendIcePing(); }

  bool SendIcePingConf() { return fake_ice_transport_->SendIcePingConf(); }

  int received_dtls_client_hellos() const {
    return received_dtls_client_hellos_;
  }

  int received_dtls_server_hellos() const {
    return received_dtls_server_hellos_;
  }

  std::optional<int> GetVersionBytes() {
    int value;
    if (dtls_transport_->GetSslVersionBytes(&value)) {
      return value;
    }
    return std::nullopt;
  }

  void CheckRole(webrtc::SSLRole role) {
    if (role == webrtc::SSL_CLIENT) {
      ASSERT_EQ(0, received_dtls_client_hellos_);
      ASSERT_GT(received_dtls_server_hellos_, 0);
    } else {
      ASSERT_GT(received_dtls_client_hellos_, 0);
      ASSERT_EQ(0, received_dtls_server_hellos_);
    }
  }

  void CheckSrtp(int expected_crypto_suite) {
    int crypto_suite;
    bool rv = dtls_transport_->GetSrtpCryptoSuite(&crypto_suite);
    if (dtls_transport_->IsDtlsActive() && expected_crypto_suite) {
      ASSERT_TRUE(rv);
      ASSERT_EQ(crypto_suite, expected_crypto_suite);
    } else {
      ASSERT_FALSE(rv);
    }
  }

  void CheckSsl() {
    int cipher;
    bool rv = dtls_transport_->GetSslCipherSuite(&cipher);
    if (dtls_transport_->IsDtlsActive()) {
      ASSERT_TRUE(rv);
      EXPECT_TRUE(webrtc::SSLStreamAdapter::IsAcceptableCipher(
          cipher, rtc::KT_DEFAULT));
    } else {
      ASSERT_FALSE(rv);
    }
  }

  void SendPackets(size_t size, size_t count, bool srtp) {
    std::unique_ptr<char[]> packet(new char[size]);
    size_t sent = 0;
    do {
      // Fill the packet with a known value and a sequence number to check
      // against, and make sure that it doesn't look like DTLS.
      memset(packet.get(), sent & 0xff, size);
      packet[0] = (srtp) ? kRtpLeadByte : 0x00;
      webrtc::SetBE32(packet.get() + kPacketNumOffset,
                      static_cast<uint32_t>(sent));

      // Only set the bypass flag if we've activated DTLS.
      int flags = (certificate_ && srtp) ? PF_SRTP_BYPASS : 0;
      rtc::PacketOptions packet_options;
      packet_options.packet_id = kFakePacketId;
      int rv = dtls_transport_->SendPacket(packet.get(), size, packet_options,
                                           flags);
      ASSERT_GT(rv, 0);
      ASSERT_EQ(size, static_cast<size_t>(rv));
      ++sent;
    } while (sent < count);
  }

  int SendInvalidSrtpPacket(size_t size) {
    std::unique_ptr<char[]> packet(new char[size]);
    // Fill the packet with 0 to form an invalid SRTP packet.
    memset(packet.get(), 0, size);

    rtc::PacketOptions packet_options;
    return dtls_transport_->SendPacket(packet.get(), size, packet_options,
                                       PF_SRTP_BYPASS);
  }

  void ExpectPackets(size_t size) {
    packet_size_ = size;
    received_.clear();
  }

  size_t NumPacketsReceived() { return received_.size(); }

  // Inverse of SendPackets.
  bool VerifyPacket(rtc::ArrayView<const uint8_t> payload, uint32_t* out_num) {
    const uint8_t* data = payload.data();
    size_t size = payload.size();

    if (size != packet_size_ || (data[0] != 0 && (data[0]) != 0x80)) {
      return false;
    }
    uint32_t packet_num = webrtc::GetBE32(data + kPacketNumOffset);
    for (size_t i = kPacketHeaderLen; i < size; ++i) {
      if (data[i] != (packet_num & 0xff)) {
        return false;
      }
    }
    if (out_num) {
      *out_num = packet_num;
    }
    return true;
  }
  bool VerifyEncryptedPacket(const uint8_t* data, size_t size) {
    // This is an encrypted data packet; let's make sure it's mostly random;
    // less than 10% of the bytes should be equal to the cleartext packet.
    if (size <= packet_size_) {
      return false;
    }
    uint32_t packet_num = webrtc::GetBE32(data + kPacketNumOffset);
    int num_matches = 0;
    for (size_t i = kPacketNumOffset; i < size; ++i) {
      if (data[i] == (packet_num & 0xff)) {
        ++num_matches;
      }
    }
    return (num_matches < ((static_cast<int>(size) - 5) / 10));
  }

  // Transport callbacks
  void set_writable_callback(absl::AnyInvocable<void()> func) {
    writable_func_ = std::move(func);
  }
  void OnTransportWritableState(rtc::PacketTransportInternal* transport) {
    RTC_LOG(LS_INFO) << name_ << ": Transport '" << transport->transport_name()
                     << "' is writable";
    if (writable_func_) {
      writable_func_();
    }
  }

  void OnTransportReadPacket(rtc::PacketTransportInternal* /* transport */,
                             const rtc::ReceivedPacket& packet) {
    uint32_t packet_num = 0;
    ASSERT_TRUE(VerifyPacket(packet.payload(), &packet_num));
    received_.insert(packet_num);
    switch (packet.decryption_info()) {
      case rtc::ReceivedPacket::kSrtpEncrypted:
        ASSERT_TRUE(certificate_ && IsRtpLeadByte(packet.payload()[0]));
        break;
      case rtc::ReceivedPacket::kDtlsDecrypted:
        ASSERT_TRUE(certificate_ && !IsRtpLeadByte(packet.payload()[0]));
        break;
      case rtc::ReceivedPacket::kNotDecrypted:
        ASSERT_FALSE(certificate_);
        break;
    }
  }

  void OnTransportSentPacket(rtc::PacketTransportInternal* /* transport */,
                             const rtc::SentPacket& sent_packet) {
    sent_packet_ = sent_packet;
  }

  rtc::SentPacket sent_packet() const { return sent_packet_; }

  // Hook into the raw packet stream to make sure DTLS packets are encrypted.
  void OnFakeIceTransportReadPacket(
      rtc::PacketTransportInternal* /* transport */,
      const rtc::ReceivedPacket& packet) {
    // Packets should not be decrypted on the underlying Transport packets.
    ASSERT_EQ(packet.decryption_info(), rtc::ReceivedPacket::kNotDecrypted);

    // Look at the handshake packets to see what role we played.
    // Check that non-handshake packets are DTLS data or SRTP bypass.
    const uint8_t* data = packet.payload().data();
    if (IsDtlsHandshakePacket(packet.payload())) {
      if (IsDtlsClientHelloPacket(packet.payload())) {
        ++received_dtls_client_hellos_;
      } else if (data[13] == 2) {
        ++received_dtls_server_hellos_;
      }
    } else if (data[0] == 26) {
      RTC_LOG(LS_INFO) << "Found DTLS ACK";
    } else if (dtls_transport_->IsDtlsActive()) {
      if (IsRtpLeadByte(data[0])) {
        ASSERT_TRUE(VerifyPacket(packet.payload(), NULL));
      } else if (packet_size_ && packet.payload().size() >= packet_size_) {
        ASSERT_TRUE(VerifyEncryptedPacket(data, packet.payload().size()));
      }
    }
  }

  absl::string_view name() { return name_; }

 private:
  std::string name_;
  rtc::scoped_refptr<webrtc::RTCCertificate> certificate_;
  std::unique_ptr<FakeIceTransport> fake_ice_transport_;
  std::unique_ptr<DtlsTransport> dtls_transport_;
  size_t packet_size_ = 0u;
  std::set<int> received_;
  webrtc::SSLProtocolVersion ssl_max_version_ = webrtc::SSL_PROTOCOL_DTLS_12;
  int received_dtls_client_hellos_ = 0;
  int received_dtls_server_hellos_ = 0;
  rtc::SentPacket sent_packet_;
  absl::AnyInvocable<void()> writable_func_;
  int async_delay_ms_ = 100;
};

// Base class for DtlsTransportTest and DtlsEventOrderingTest, which
// inherit from different variants of ::testing::Test.
//
// Note that this test always uses a FakeClock, due to the `fake_clock_` member
// variable.
class DtlsTransportTestBase {
 public:
  DtlsTransportTestBase() : client1_("P1"), client2_("P2"), use_dtls_(false) {
    start_time_ns_ = fake_clock_.TimeNanos();
  }

  void SetMaxProtocolVersions(webrtc::SSLProtocolVersion c1,
                              webrtc::SSLProtocolVersion c2) {
    client1_.SetupMaxProtocolVersion(c1);
    client2_.SetupMaxProtocolVersion(c2);
  }
  // If not called, DtlsTransport will be used in SRTP bypass mode.
  void PrepareDtls(rtc::KeyType key_type) {
    client1_.CreateCertificate(key_type);
    client2_.CreateCertificate(key_type);
    use_dtls_ = true;
  }

  // This test negotiates DTLS parameters before the underlying transports are
  // writable. DtlsEventOrderingTest is responsible for exercising differerent
  // orderings.
  bool Connect(bool client1_server = true) {
    Negotiate(client1_server);
    EXPECT_TRUE(client1_.Connect(&client2_, false));

    EXPECT_THAT(webrtc::WaitUntil(
                    [&] {
                      return client1_.dtls_transport()->writable() &&
                             client2_.dtls_transport()->writable();
                    },
                    IsTrue(),
                    {.timeout = webrtc::TimeDelta::Millis(kTimeout),
                     .clock = &fake_clock_}),
                webrtc::IsRtcOk());
    if (!client1_.dtls_transport()->writable() ||
        !client2_.dtls_transport()->writable())
      return false;

    // Check that we used the right roles.
    if (use_dtls_) {
      client1_.CheckRole(client1_server ? webrtc::SSL_SERVER
                                        : webrtc::SSL_CLIENT);
      client2_.CheckRole(client1_server ? webrtc::SSL_CLIENT
                                        : webrtc::SSL_SERVER);
    }

    if (use_dtls_) {
      // Check that we negotiated the right ciphers. Since GCM ciphers are not
      // negotiated by default, we should end up with kSrtpAes128CmSha1_80.
      client1_.CheckSrtp(webrtc::kSrtpAes128CmSha1_80);
      client2_.CheckSrtp(webrtc::kSrtpAes128CmSha1_80);
    } else {
      // If DTLS isn't actually being used, GetSrtpCryptoSuite should return
      // false.
      client1_.CheckSrtp(webrtc::kSrtpInvalidCryptoSuite);
      client2_.CheckSrtp(webrtc::kSrtpInvalidCryptoSuite);
    }

    client1_.CheckSsl();
    client2_.CheckSsl();

    return true;
  }

  void Negotiate(bool client1_server = true) {
    client1_.SetupTransports(ICEROLE_CONTROLLING);
    client2_.SetupTransports(ICEROLE_CONTROLLED);
    client1_.dtls_transport()->SetDtlsRole(client1_server ? webrtc::SSL_SERVER
                                                          : webrtc::SSL_CLIENT);
    client2_.dtls_transport()->SetDtlsRole(client1_server ? webrtc::SSL_CLIENT
                                                          : webrtc::SSL_SERVER);
    if (client2_.certificate()) {
      SetRemoteFingerprintFromCert(client1_.dtls_transport(),
                                   client2_.certificate());
    }
    if (client1_.certificate()) {
      SetRemoteFingerprintFromCert(client2_.dtls_transport(),
                                   client1_.certificate());
    }
  }

  void TestTransfer(size_t size, size_t count, bool srtp) {
    RTC_LOG(LS_INFO) << "Expect packets, size=" << size;
    client2_.ExpectPackets(size);
    client1_.SendPackets(size, count, srtp);
    EXPECT_THAT(webrtc::WaitUntil(
                    [&] { return client2_.NumPacketsReceived(); }, Eq(count),
                    {.timeout = webrtc::TimeDelta::Millis(kTimeout),
                     .clock = &fake_clock_}),
                webrtc::IsRtcOk());
  }

  void AddPacketLogging() {
    client1_.fake_ice_transport()->set_packet_recv_filter(
        [&](auto packet, auto timestamp_us) {
          return LogRecv(client1_.name(), packet);
        });
    client2_.fake_ice_transport()->set_packet_recv_filter(
        [&](auto packet, auto timestamp_us) {
          return LogRecv(client2_.name(), packet);
        });
    client1_.set_writable_callback([&]() {});
    client2_.set_writable_callback([&]() {});

    client1_.fake_ice_transport()->set_packet_send_filter(
        [&](auto data, auto len, auto options, auto flags) {
          return LogSend(client1_.name(), /* drop=*/false, data, len);
        });
    client2_.fake_ice_transport()->set_packet_send_filter(
        [&](auto data, auto len, auto options, auto flags) {
          return LogSend(client2_.name(), /* drop=*/false, data, len);
        });
  }

  void ClearPacketFilters() {
    client1_.fake_ice_transport()->set_packet_send_filter(nullptr);
    client2_.fake_ice_transport()->set_packet_send_filter(nullptr);
    client1_.fake_ice_transport()->set_packet_recv_filter(nullptr);
    client2_.fake_ice_transport()->set_packet_recv_filter(nullptr);
  }

  bool LogRecv(absl::string_view name, const rtc::CopyOnWriteBuffer& packet) {
    auto timestamp_ms = (fake_clock_.TimeNanos() - start_time_ns_) / 1000000;
    RTC_LOG(LS_INFO) << "time=" << timestamp_ms << " : " << name
                     << ": ReceivePacket packet len=" << packet.size()
                     << ", data[0]: " << static_cast<uint8_t>(packet.data()[0]);
    return false;
  }

  bool LogSend(absl::string_view name,
               bool drop,
               const char* data,
               size_t len) {
    auto timestamp_ms = (fake_clock_.TimeNanos() - start_time_ns_) / 1000000;
    if (drop) {
      RTC_LOG(LS_INFO) << "time=" << timestamp_ms << " : " << name
                       << ": dropping packet len=" << len
                       << ", data[0]: " << static_cast<uint8_t>(data[0]);
    } else {
      RTC_LOG(LS_INFO) << "time=" << timestamp_ms << " : " << name
                       << ": SendPacket, len=" << len
                       << ", data[0]: " << static_cast<uint8_t>(data[0]);
    }
    return drop;
  }

  template <typename Fn>
  bool WaitUntil(Fn func) {
    return webrtc::WaitUntil(func, IsTrue(),
                             {.timeout = webrtc::TimeDelta::Millis(kTimeout),
                              .clock = &fake_clock_})
        .ok();
  }

 protected:
  rtc::AutoThread main_thread_;
  webrtc::ScopedFakeClock fake_clock_;
  DtlsTestClient client1_;
  DtlsTestClient client2_;
  bool use_dtls_;
  uint64_t start_time_ns_;
  webrtc::SSLProtocolVersion ssl_expected_version_;
};

class DtlsTransportTest : public DtlsTransportTestBase,
                          public ::testing::Test {};

// Connect without DTLS, and transfer RTP data.
TEST_F(DtlsTransportTest, TestTransferRtp) {
  ASSERT_TRUE(Connect());
  TestTransfer(1000, 100, /*srtp=*/false);
}

// Test that the SignalSentPacket signal is wired up.
TEST_F(DtlsTransportTest, TestSignalSentPacket) {
  ASSERT_TRUE(Connect());
  // Sanity check default value (-1).
  ASSERT_EQ(client1_.sent_packet().send_time_ms, -1);
  TestTransfer(1000, 100, false);
  // Check that we get the expected fake packet ID, and a time of 0 from the
  // fake clock.
  EXPECT_EQ(kFakePacketId, client1_.sent_packet().packet_id);
  EXPECT_GE(client1_.sent_packet().send_time_ms, 0);
}

// Connect without DTLS, and transfer SRTP data.
TEST_F(DtlsTransportTest, TestTransferSrtp) {
  ASSERT_TRUE(Connect());
  TestTransfer(1000, 100, /*srtp=*/true);
}

// Connect with DTLS, and transfer data over DTLS.
TEST_F(DtlsTransportTest, TestTransferDtls) {
  PrepareDtls(rtc::KT_DEFAULT);
  ASSERT_TRUE(Connect());
  TestTransfer(1000, 100, /*srtp=*/false);
}

// Connect with DTLS, combine multiple DTLS records into one packet.
// Our DTLS implementation doesn't do this, but other implementations may;
// see https://tools.ietf.org/html/rfc6347#section-4.1.1.
// This has caused interoperability problems with ORTCLib in the past.
TEST_F(DtlsTransportTest, TestTransferDtlsCombineRecords) {
  PrepareDtls(rtc::KT_DEFAULT);
  ASSERT_TRUE(Connect());
  // Our DTLS implementation always sends one record per packet, so to simulate
  // an endpoint that sends multiple records per packet, we configure the fake
  // ICE transport to combine every two consecutive packets into a single
  // packet.
  FakeIceTransport* transport = client1_.fake_ice_transport();
  transport->combine_outgoing_packets(true);
  TestTransfer(500, 100, /*srtp=*/false);
}

TEST_F(DtlsTransportTest, KeyingMaterialExporter) {
  PrepareDtls(rtc::KT_DEFAULT);
  ASSERT_TRUE(Connect());

  int crypto_suite;
  EXPECT_TRUE(client1_.dtls_transport()->GetSrtpCryptoSuite(&crypto_suite));
  int key_len;
  int salt_len;
  EXPECT_TRUE(
      webrtc::GetSrtpKeyAndSaltLengths(crypto_suite, &key_len, &salt_len));
  rtc::ZeroOnFreeBuffer<uint8_t> client1_out(2 * (key_len + salt_len));
  rtc::ZeroOnFreeBuffer<uint8_t> client2_out(2 * (key_len + salt_len));
  EXPECT_TRUE(client1_.dtls_transport()->ExportSrtpKeyingMaterial(client1_out));
  EXPECT_TRUE(client2_.dtls_transport()->ExportSrtpKeyingMaterial(client2_out));
  EXPECT_EQ(client1_out, client2_out);
}

enum HandshakeTestEvent {
  EV_CLIENT_SEND = 0,
  EV_SERVER_SEND = 1,
  EV_CLIENT_RECV = 2,
  EV_SERVER_RECV = 3,
  EV_CLIENT_WRITABLE = 4,
  EV_SERVER_WRITABLE = 5,

  EV_CLIENT_SEND_DROPPED = 6,
  EV_SERVER_SEND_DROPPED = 7,
};

static const std::vector<HandshakeTestEvent> dtls_12_handshake_events{
    // Flight 1
    EV_CLIENT_SEND,
    EV_SERVER_RECV,
    EV_SERVER_SEND,
    EV_CLIENT_RECV,

    // Flight 2
    EV_CLIENT_SEND,
    EV_SERVER_RECV,
    EV_SERVER_SEND,
    EV_SERVER_WRITABLE,
    EV_CLIENT_RECV,
    EV_CLIENT_WRITABLE,
};

static const std::vector<HandshakeTestEvent> dtls_13_handshake_events{
    // Flight 1
    EV_CLIENT_SEND,
    EV_SERVER_RECV,
    EV_SERVER_SEND,
    EV_CLIENT_RECV,

    // Flight 2
    EV_CLIENT_SEND,
    EV_CLIENT_WRITABLE,
    EV_SERVER_RECV,
    EV_SERVER_SEND,
    EV_SERVER_WRITABLE,
};

static const struct {
  int version_bytes;
  const std::vector<HandshakeTestEvent>& events;
} kEventsPerVersion[] = {
    {webrtc::kDtls12VersionBytes, dtls_12_handshake_events},
    {webrtc::kDtls13VersionBytes, dtls_13_handshake_events},
};

struct EndpointConfig {
  webrtc::SSLProtocolVersion max_protocol_version;
  bool dtls_in_stun = false;
  std::optional<IceRole> ice_role;
  std::optional<webrtc::SSLRole> ssl_role;

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const EndpointConfig& config) {
    sink.Append("[ dtls: ");
    sink.Append(config.ssl_role == webrtc::SSL_SERVER ? "server/" : "client/");
    switch (config.max_protocol_version) {
      case webrtc::SSL_PROTOCOL_DTLS_10:
        sink.Append("1.0");
        break;
      case webrtc::SSL_PROTOCOL_DTLS_12:
        sink.Append("1.2");
        break;
      case webrtc::SSL_PROTOCOL_DTLS_13:
        sink.Append("1.3");
        break;
      default:
        sink.Append("<unknown>");
        break;
    }
    absl::Format(&sink, " dtls_in_stun: %u ice: ", config.dtls_in_stun);
    sink.Append(config.ice_role == ICEROLE_CONTROLLED ? "controlled"
                                                      : "controlling");
    sink.Append(" ]");
  }
};

class DtlsTransportVersionTest
    : public DtlsTransportTestBase,
      public ::testing::TestWithParam<
          std::tuple<EndpointConfig, EndpointConfig>> {
 public:
  void Prepare(bool rtt_estimate = true) {
    PrepareDtls(rtc::KT_DEFAULT);
    const auto& config1 = std::get<0>(GetParam());
    const auto& config2 = std::get<1>(GetParam());
    SetMaxProtocolVersions(config1.max_protocol_version,
                           config2.max_protocol_version);

    client1_.set_async_delay(50);
    client2_.set_async_delay(50);

    client1_.SetupTransports(config1.ice_role.value_or(ICEROLE_CONTROLLING),
                             rtt_estimate);
    client2_.SetupTransports(config2.ice_role.value_or(ICEROLE_CONTROLLED),
                             rtt_estimate);
    client1_.dtls_transport()->SetDtlsRole(
        config1.ssl_role.value_or(webrtc::SSL_CLIENT));
    client2_.dtls_transport()->SetDtlsRole(
        config2.ssl_role.value_or(webrtc::SSL_SERVER));

    if (config1.dtls_in_stun) {
      auto config = client1_.fake_ice_transport()->config();
      config.dtls_handshake_in_stun = true;
      client1_.fake_ice_transport()->SetIceConfig(config);
    }
    if (config2.dtls_in_stun) {
      auto config = client2_.fake_ice_transport()->config();
      config.dtls_handshake_in_stun = true;
      client2_.fake_ice_transport()->SetIceConfig(config);
    }

    SetRemoteFingerprintFromCert(client1_.dtls_transport(),
                                 client2_.certificate());
    SetRemoteFingerprintFromCert(client2_.dtls_transport(),
                                 client1_.certificate());
  }

  // Run DTLS handshake.
  // - store events in `events`
  // - drop packets as specified in `packets_to_drop`
  std::pair</* dtls_version_bytes*/ int, std::vector<HandshakeTestEvent>>
  RunHandshake(std::set<unsigned> packets_to_drop) {
    std::vector<HandshakeTestEvent> events;
    client1_.fake_ice_transport()->set_packet_recv_filter(
        [&](auto packet, auto timestamp_us) {
          events.push_back(EV_CLIENT_RECV);
          return LogRecv("client", packet);
        });
    client2_.fake_ice_transport()->set_packet_recv_filter(
        [&](auto packet, auto timestamp_us) {
          events.push_back(EV_SERVER_RECV);
          return LogRecv("server", packet);
        });
    client1_.set_writable_callback(
        [&]() { events.push_back(EV_CLIENT_WRITABLE); });
    client2_.set_writable_callback(
        [&]() { events.push_back(EV_SERVER_WRITABLE); });

    unsigned packet_num = 0;
    client1_.fake_ice_transport()->set_packet_send_filter(
        [&](auto data, auto len, auto options, auto flags) {
          auto packet_type = options.info_signaled_after_sent.packet_type;
          if (packet_type == rtc::PacketType::kIceConnectivityCheck ||
              packet_type == rtc::PacketType::kIceConnectivityCheckResponse) {
            // Ignore stun pings for now.
            return LogSend("client-stun", /* drop= */ false, data, len);
          }
          bool drop = packets_to_drop.find(packet_num) != packets_to_drop.end();
          packet_num++;
          if (!drop) {
            events.push_back(EV_CLIENT_SEND);
          } else {
            events.push_back(EV_CLIENT_SEND_DROPPED);
          }
          return LogSend("client", drop, data, len);
        });
    client2_.fake_ice_transport()->set_packet_send_filter(
        [&](auto data, auto len, auto options, auto flags) {
          auto packet_type = options.info_signaled_after_sent.packet_type;
          if (packet_type == rtc::PacketType::kIceConnectivityCheck ||
              packet_type == rtc::PacketType::kIceConnectivityCheckResponse) {
            // Ignore stun pings for now.
            return LogSend("server-stun", /* drop= */ false, data, len);
          }
          bool drop = packets_to_drop.find(packet_num) != packets_to_drop.end();
          packet_num++;
          if (!drop) {
            events.push_back(EV_SERVER_SEND);
          } else {
            events.push_back(EV_SERVER_SEND_DROPPED);
          }
          return LogSend("server", drop, data, len);
        });

    EXPECT_TRUE(client1_.ConnectIceTransport(&client2_));
    client1_.SendIcePing();
    client2_.SendIcePingConf();
    client2_.SendIcePing();
    client1_.SendIcePingConf();

    EXPECT_TRUE(WaitUntil([&] {
      return client1_.dtls_transport()->writable() &&
             client2_.dtls_transport()->writable();
    }));

    ClearPacketFilters();

    auto dtls_version_bytes = client1_.GetVersionBytes();
    EXPECT_EQ(dtls_version_bytes, client2_.GetVersionBytes());
    return std::make_pair(dtls_version_bytes.value_or(0), std::move(events));
  }

  int GetExpectedDtlsVersionBytes() {
    int version = std::min(
        static_cast<int>(std::get<0>(GetParam()).max_protocol_version),
        static_cast<int>(std::get<1>(GetParam()).max_protocol_version));
    if (version == webrtc::SSL_PROTOCOL_DTLS_13) {
      return webrtc::kDtls13VersionBytes;
    } else {
      return webrtc::kDtls12VersionBytes;
    }
  }

  std::vector<HandshakeTestEvent> GetExpectedEvents(int dtls_version_bytes) {
    for (const auto e : kEventsPerVersion) {
      if (e.version_bytes == dtls_version_bytes) {
        return e.events;
      }
    }
    return {};
  }
};

static const EndpointConfig kEndpointVariants[] = {
    {
        .max_protocol_version = webrtc::SSL_PROTOCOL_DTLS_10,
        .dtls_in_stun = false,
    },
    {
        .max_protocol_version = webrtc::SSL_PROTOCOL_DTLS_12,
        .dtls_in_stun = false,
    },
    {
        .max_protocol_version = webrtc::SSL_PROTOCOL_DTLS_13,
        .dtls_in_stun = false,
    },
    {
        .max_protocol_version = webrtc::SSL_PROTOCOL_DTLS_10,
        .dtls_in_stun = true,
    },
    {
        .max_protocol_version = webrtc::SSL_PROTOCOL_DTLS_12,
        .dtls_in_stun = true,
    },
    {
        .max_protocol_version = webrtc::SSL_PROTOCOL_DTLS_13,
        .dtls_in_stun = true,
    },
};

// Will test every combination of 1.0/1.2/1.3 on the client and server.
// DTLS will negotiate an effective version (the min of client & sewrver).
INSTANTIATE_TEST_SUITE_P(
    DtlsTransportVersionTest,
    DtlsTransportVersionTest,
    ::testing::Combine(testing::ValuesIn(kEndpointVariants),
                       testing::ValuesIn(kEndpointVariants)));

// Test that an acceptable cipher suite is negotiated when different versions
// of DTLS are supported. Note that it's IsAcceptableCipher that does the actual
// work.
TEST_P(DtlsTransportVersionTest, CipherSuiteNegotiation) {
  Prepare();
  ASSERT_TRUE(Connect());
}

TEST_P(DtlsTransportVersionTest, HandshakeFlights) {
  if (std::get<0>(GetParam()).dtls_in_stun ||
      (std::get<0>(GetParam()).dtls_in_stun &&
       std::get<1>(GetParam()).dtls_in_stun)) {
    GTEST_SKIP() << "This test does not support dtls in stun";
  }
  Prepare();
  auto [dtls_version_bytes, events] = RunHandshake({});

  RTC_LOG(LS_INFO) << "Verifying events with ssl version bytes= "
                   << dtls_version_bytes;
  auto expect = GetExpectedEvents(dtls_version_bytes);
  EXPECT_EQ(events, expect);
}

TEST_P(DtlsTransportVersionTest, HandshakeLoseFirstClientPacket) {
  MAYBE_SKIP_TEST(IsBoringSsl);
  if (std::get<0>(GetParam()).dtls_in_stun ||
      (std::get<0>(GetParam()).dtls_in_stun &&
       std::get<1>(GetParam()).dtls_in_stun)) {
    GTEST_SKIP() << "This test does not support dtls in stun";
  }

  Prepare();
  auto [dtls_version_bytes, events] = RunHandshake({/* packet_num= */ 0});

  auto expect = GetExpectedEvents(dtls_version_bytes);

  // If first packet is lost...it is simply retransmitted by client,
  // nothing else changes.
  expect.insert(expect.begin(), EV_CLIENT_SEND_DROPPED);

  EXPECT_EQ(events, expect);
}

TEST_P(DtlsTransportVersionTest, HandshakeLoseSecondClientPacket) {
  MAYBE_SKIP_TEST(IsBoringSsl);
  if (std::get<0>(GetParam()).dtls_in_stun ||
      (std::get<0>(GetParam()).dtls_in_stun &&
       std::get<1>(GetParam()).dtls_in_stun)) {
    GTEST_SKIP() << "This test does not support dtls in stun";
  }

  Prepare();
  auto [dtls_version_bytes, events] = RunHandshake({/* packet_num= */ 2});

  std::vector<HandshakeTestEvent> expect;

  switch (dtls_version_bytes) {
    case webrtc::kDtls12VersionBytes:
      expect = {
          // Flight 1
          EV_CLIENT_SEND,
          EV_SERVER_RECV,
          EV_SERVER_SEND,
          EV_CLIENT_RECV,

          // Flight 2
          EV_CLIENT_SEND_DROPPED,

          // Server retransmit.
          EV_SERVER_SEND,
          // Client retransmit.
          EV_CLIENT_SEND,
          // Client receive retransmit => Do nothing, has already retransmitted.
          EV_CLIENT_RECV,
          // Handshake resume.
          EV_SERVER_RECV,
          EV_SERVER_SEND,
          EV_SERVER_WRITABLE,
          EV_CLIENT_RECV,
          EV_CLIENT_WRITABLE,
      };
      break;
    case webrtc::kDtls13VersionBytes:
      expect = {
          // Flight 1
          EV_CLIENT_SEND,
          EV_SERVER_RECV,
          EV_SERVER_SEND,
          EV_CLIENT_RECV,

          // Flight 2
          EV_CLIENT_SEND_DROPPED,
          // Client doesn't know packet it is dropped, so it becomes writable.
          EV_CLIENT_WRITABLE,

          // Server retransmit.
          EV_SERVER_SEND,
          // Client retransmit.
          EV_CLIENT_SEND,

          // Client receive retransmit => Do nothing, has already retransmitted.
          EV_CLIENT_RECV,
          // Handshake resume.
          EV_SERVER_RECV,
          EV_SERVER_SEND,
          EV_SERVER_WRITABLE,
      };
      break;
    default:
      FAIL() << "Unknown dtls version bytes: " << dtls_version_bytes;
  }
  EXPECT_EQ(events, expect);
}

// Connect with DTLS, negotiating DTLS-SRTP, and transfer SRTP using bypass.
TEST_F(DtlsTransportTest, TestTransferDtlsSrtp) {
  PrepareDtls(rtc::KT_DEFAULT);
  ASSERT_TRUE(Connect());
  TestTransfer(1000, 100, /*srtp=*/true);
}

// Connect with DTLS-SRTP, transfer an invalid SRTP packet, and expects -1
// returned.
TEST_F(DtlsTransportTest, TestTransferDtlsInvalidSrtpPacket) {
  PrepareDtls(rtc::KT_DEFAULT);
  ASSERT_TRUE(Connect());
  EXPECT_EQ(-1, client1_.SendInvalidSrtpPacket(100));
}

// Create a single transport with DTLS, and send normal data and SRTP data on
// it.
TEST_F(DtlsTransportTest, TestTransferDtlsSrtpDemux) {
  PrepareDtls(rtc::KT_DEFAULT);
  ASSERT_TRUE(Connect());
  TestTransfer(1000, 100, /*srtp=*/false);
  TestTransfer(1000, 100, /*srtp=*/true);
}

// Test transferring when the "answerer" has the server role.
TEST_F(DtlsTransportTest, TestTransferDtlsSrtpAnswererIsPassive) {
  PrepareDtls(rtc::KT_DEFAULT);
  ASSERT_TRUE(Connect(/*client1_server=*/false));
  TestTransfer(1000, 100, /*srtp=*/true);
}

// Test that renegotiation (setting same role and fingerprint again) can be
// started before the clients become connected in the first negotiation.
TEST_F(DtlsTransportTest, TestRenegotiateBeforeConnect) {
  PrepareDtls(rtc::KT_DEFAULT);
  // Note: This is doing the same thing Connect normally does, minus some
  // additional checks not relevant for this test.
  Negotiate();
  Negotiate();
  EXPECT_TRUE(client1_.Connect(&client2_, false));
  EXPECT_TRUE(WaitUntil([&] {
    return client1_.dtls_transport()->writable() &&
           client2_.dtls_transport()->writable();
  }));
  TestTransfer(1000, 100, true);
}

// Test Certificates state after negotiation but before connection.
TEST_F(DtlsTransportTest, TestCertificatesBeforeConnect) {
  PrepareDtls(rtc::KT_DEFAULT);
  Negotiate();

  // After negotiation, each side has a distinct local certificate, but still no
  // remote certificate, because connection has not yet occurred.
  auto certificate1 = client1_.dtls_transport()->GetLocalCertificate();
  auto certificate2 = client2_.dtls_transport()->GetLocalCertificate();
  ASSERT_NE(certificate1->GetSSLCertificate().ToPEMString(),
            certificate2->GetSSLCertificate().ToPEMString());
  ASSERT_FALSE(client1_.dtls_transport()->GetRemoteSSLCertChain());
  ASSERT_FALSE(client2_.dtls_transport()->GetRemoteSSLCertChain());
}

// Test Certificates state after connection.
TEST_F(DtlsTransportTest, TestCertificatesAfterConnect) {
  PrepareDtls(rtc::KT_DEFAULT);
  ASSERT_TRUE(Connect());

  // After connection, each side has a distinct local certificate.
  auto certificate1 = client1_.dtls_transport()->GetLocalCertificate();
  auto certificate2 = client2_.dtls_transport()->GetLocalCertificate();
  ASSERT_NE(certificate1->GetSSLCertificate().ToPEMString(),
            certificate2->GetSSLCertificate().ToPEMString());

  // Each side's remote certificate is the other side's local certificate.
  std::unique_ptr<rtc::SSLCertChain> remote_cert1 =
      client1_.dtls_transport()->GetRemoteSSLCertChain();
  ASSERT_TRUE(remote_cert1);
  ASSERT_EQ(1u, remote_cert1->GetSize());
  ASSERT_EQ(remote_cert1->Get(0).ToPEMString(),
            certificate2->GetSSLCertificate().ToPEMString());
  std::unique_ptr<rtc::SSLCertChain> remote_cert2 =
      client2_.dtls_transport()->GetRemoteSSLCertChain();
  ASSERT_TRUE(remote_cert2);
  ASSERT_EQ(1u, remote_cert2->GetSize());
  ASSERT_EQ(remote_cert2->Get(0).ToPEMString(),
            certificate1->GetSSLCertificate().ToPEMString());
}

// Test that packets are retransmitted according to the expected schedule.
// Each time a timeout occurs, the retransmission timer should be doubled up to
// 60 seconds. The timer defaults to 1 second, but for WebRTC we should be
// initializing it to 50ms.
TEST_F(DtlsTransportTest, TestRetransmissionSchedule) {
  // We can only change the retransmission schedule with a recently-added
  // BoringSSL API. Skip the test if not built with BoringSSL.
  MAYBE_SKIP_TEST(IsBoringSsl);

  PrepareDtls(rtc::KT_DEFAULT);

  // This test is written with assumption of 0 delay
  // which affect the hard coded schedule below.
  client1_.set_async_delay(0);
  client2_.set_async_delay(0);

  // Exchange fingerprints and set SSL roles.
  Negotiate();

  // Make client2_ writable, but not client1_.
  // This means client1_ will send DTLS client hellos but get no response.
  EXPECT_TRUE(client2_.Connect(&client1_, true));
  EXPECT_TRUE(
      WaitUntil([&] { return client2_.fake_ice_transport()->writable(); }));

  // Wait for the first client hello to be sent.
  EXPECT_TRUE(
      WaitUntil([&] { return client1_.received_dtls_client_hellos(); }));
  EXPECT_FALSE(client1_.fake_ice_transport()->writable());

  static int timeout_schedule_ms[] = {50,   100,  200,   400,   800,   1600,
                                      3200, 6400, 12800, 25600, 51200, 60000};

  int expected_hellos = 1;
  for (size_t i = 0;
       i < (sizeof(timeout_schedule_ms) / sizeof(timeout_schedule_ms[0]));
       ++i) {
    // For each expected retransmission time, advance the fake clock a
    // millisecond before the expected time and verify that no unexpected
    // retransmissions were sent. Then advance it the final millisecond and
    // verify that the expected retransmission was sent.
    fake_clock_.AdvanceTime(
        webrtc::TimeDelta::Millis(timeout_schedule_ms[i] - 1));
    EXPECT_EQ(expected_hellos, client1_.received_dtls_client_hellos());
    fake_clock_.AdvanceTime(webrtc::TimeDelta::Millis(1));
    EXPECT_EQ(++expected_hellos, client1_.received_dtls_client_hellos());
  }
}

// The following events can occur in many different orders:
// 1. Caller receives remote fingerprint.
// 2. Caller is writable.
// 3. Caller receives ClientHello.
// 4. DTLS handshake finishes.
//
// The tests below cover all causally consistent permutations of these events;
// the caller must be writable and receive a ClientHello before the handshake
// finishes, but otherwise any ordering is possible.
//
// For each permutation, the test verifies that a connection is established and
// fingerprint verified without any DTLS packet needing to be retransmitted.
//
// Each permutation is also tested with valid and invalid fingerprints,
// ensuring that the handshake fails with an invalid fingerprint.
enum DtlsTransportEvent {
  CALLER_RECEIVES_FINGERPRINT,
  CALLER_WRITABLE,
  CALLER_RECEIVES_CLIENTHELLO,
  HANDSHAKE_FINISHES
};

class DtlsEventOrderingTest
    : public DtlsTransportTestBase,
      public ::testing::TestWithParam<
          ::testing::tuple<std::vector<DtlsTransportEvent>, bool>> {
 protected:
  // If `valid_fingerprint` is false, the caller will receive a fingerprint
  // that doesn't match the callee's certificate, so the handshake should fail.
  void TestEventOrdering(const std::vector<DtlsTransportEvent>& events,
                         bool valid_fingerprint) {
    // Pre-setup: Set local certificate on both caller and callee, and
    // remote fingerprint on callee, but neither is writable and the caller
    // doesn't have the callee's fingerprint.
    PrepareDtls(rtc::KT_DEFAULT);
    client1_.SetupTransports(ICEROLE_CONTROLLING);
    client2_.SetupTransports(ICEROLE_CONTROLLED);
    // Similar to how NegotiateOrdering works.
    client1_.dtls_transport()->SetDtlsRole(webrtc::SSL_SERVER);
    client2_.dtls_transport()->SetDtlsRole(webrtc::SSL_CLIENT);
    SetRemoteFingerprintFromCert(client2_.dtls_transport(),
                                 client1_.certificate());

    for (DtlsTransportEvent e : events) {
      switch (e) {
        case CALLER_RECEIVES_FINGERPRINT:
          if (valid_fingerprint) {
            SetRemoteFingerprintFromCert(client1_.dtls_transport(),
                                         client2_.certificate());
          } else {
            SetRemoteFingerprintFromCert(client1_.dtls_transport(),
                                         client2_.certificate(),
                                         true /*modify_digest*/);
          }
          break;
        case CALLER_WRITABLE:
          EXPECT_TRUE(client1_.Connect(&client2_, true));
          EXPECT_TRUE(WaitUntil(
              [&] { return client1_.fake_ice_transport()->writable(); }));
          break;
        case CALLER_RECEIVES_CLIENTHELLO:
          // Sanity check that a ClientHello hasn't already been received.
          EXPECT_EQ(0, client1_.received_dtls_client_hellos());
          // Making client2_ writable will cause it to send the ClientHello.
          EXPECT_TRUE(client2_.Connect(&client1_, true));
          EXPECT_TRUE(WaitUntil(
              [&] { return client2_.fake_ice_transport()->writable(); }));
          EXPECT_TRUE(WaitUntil(
              [&] { return client1_.received_dtls_client_hellos() == 1; }));
          break;
        case HANDSHAKE_FINISHES:
          // Sanity check that the handshake hasn't already finished.
          EXPECT_FALSE(client1_.dtls_transport()->IsDtlsConnected() ||
                       client1_.dtls_transport()->dtls_state() ==
                           webrtc::DtlsTransportState::kFailed);
          EXPECT_TRUE(WaitUntil([&] {
            return client1_.dtls_transport()->IsDtlsConnected() ||
                   client1_.dtls_transport()->dtls_state() ==
                       webrtc::DtlsTransportState::kFailed;
          }));
          break;
      }
    }

    webrtc::DtlsTransportState expected_final_state =
        valid_fingerprint ? webrtc::DtlsTransportState::kConnected
                          : webrtc::DtlsTransportState::kFailed;
    EXPECT_TRUE(WaitUntil([&] {
      return client1_.dtls_transport()->dtls_state() == expected_final_state;
    }));
    EXPECT_TRUE(WaitUntil([&] {
      return client2_.dtls_transport()->dtls_state() == expected_final_state;
    }));

    // Transports should be writable iff there was a valid fingerprint.
    EXPECT_EQ(valid_fingerprint, client1_.dtls_transport()->writable());
    EXPECT_EQ(valid_fingerprint, client2_.dtls_transport()->writable());

    // Check that no hello needed to be retransmitted.
    EXPECT_EQ(1, client1_.received_dtls_client_hellos());
    EXPECT_EQ(1, client2_.received_dtls_server_hellos());

    if (valid_fingerprint) {
      TestTransfer(1000, 100, false);
    }
  }
};

TEST_P(DtlsEventOrderingTest, TestEventOrdering) {
  TestEventOrdering(::testing::get<0>(GetParam()),
                    ::testing::get<1>(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    TestEventOrdering,
    DtlsEventOrderingTest,
    ::testing::Combine(
        ::testing::Values(
            std::vector<DtlsTransportEvent>{
                CALLER_RECEIVES_FINGERPRINT, CALLER_WRITABLE,
                CALLER_RECEIVES_CLIENTHELLO, HANDSHAKE_FINISHES},
            std::vector<DtlsTransportEvent>{
                CALLER_WRITABLE, CALLER_RECEIVES_FINGERPRINT,
                CALLER_RECEIVES_CLIENTHELLO, HANDSHAKE_FINISHES},
            std::vector<DtlsTransportEvent>{
                CALLER_WRITABLE, CALLER_RECEIVES_CLIENTHELLO,
                CALLER_RECEIVES_FINGERPRINT, HANDSHAKE_FINISHES},
            std::vector<DtlsTransportEvent>{
                CALLER_WRITABLE, CALLER_RECEIVES_CLIENTHELLO,
                HANDSHAKE_FINISHES, CALLER_RECEIVES_FINGERPRINT},
            std::vector<DtlsTransportEvent>{
                CALLER_RECEIVES_FINGERPRINT, CALLER_RECEIVES_CLIENTHELLO,
                CALLER_WRITABLE, HANDSHAKE_FINISHES},
            std::vector<DtlsTransportEvent>{
                CALLER_RECEIVES_CLIENTHELLO, CALLER_RECEIVES_FINGERPRINT,
                CALLER_WRITABLE, HANDSHAKE_FINISHES},
            std::vector<DtlsTransportEvent>{
                CALLER_RECEIVES_CLIENTHELLO, CALLER_WRITABLE,
                CALLER_RECEIVES_FINGERPRINT, HANDSHAKE_FINISHES},
            std::vector<DtlsTransportEvent>{CALLER_RECEIVES_CLIENTHELLO,
                                            CALLER_WRITABLE, HANDSHAKE_FINISHES,
                                            CALLER_RECEIVES_FINGERPRINT}),
        ::testing::Bool()));

class DtlsTransportDtlsInStunTest : public DtlsTransportVersionTest {
 public:
  DtlsTransportDtlsInStunTest() {}
};

std::vector<std::tuple<EndpointConfig, EndpointConfig>> AllEndpointVariants() {
  std::vector<std::tuple<EndpointConfig, EndpointConfig>> v;
  for (auto ice_role : {ICEROLE_CONTROLLING, ICEROLE_CONTROLLED}) {
    for (auto ssl_role : {webrtc::SSL_CLIENT, webrtc::SSL_SERVER}) {
      for (auto version1 : {
               webrtc::SSL_PROTOCOL_DTLS_12,
               webrtc::SSL_PROTOCOL_DTLS_13,
           }) {
        for (auto version2 : {
                 webrtc::SSL_PROTOCOL_DTLS_12,
                 webrtc::SSL_PROTOCOL_DTLS_13,
             }) {
          for (auto dtls_in_stun1 : {false, true}) {
            for (auto dtls_in_stun2 : {false, true}) {
              v.push_back(std::make_tuple(
                  EndpointConfig{
                      .max_protocol_version = version1,
                      .dtls_in_stun = dtls_in_stun1,
                      .ice_role = ice_role,
                      .ssl_role = ssl_role,
                  },
                  EndpointConfig{
                      .max_protocol_version = version2,
                      .dtls_in_stun = dtls_in_stun2,
                      .ice_role = ice_role == ICEROLE_CONTROLLING
                                      ? ICEROLE_CONTROLLED
                                      : ICEROLE_CONTROLLING,
                      .ssl_role = ssl_role == webrtc::SSL_CLIENT
                                      ? webrtc::SSL_SERVER
                                      : webrtc::SSL_CLIENT,
                  }));
            }
          }
        }
      }
    }
  }
  return v;
}

TEST_P(DtlsTransportDtlsInStunTest, Handshake1) {
  Prepare(/* rtt_estimate= */ false);
  AddPacketLogging();

  RTC_LOG(LS_INFO) << "client1: " << std::get<0>(GetParam());
  RTC_LOG(LS_INFO) << "client2: " << std::get<1>(GetParam());

  ASSERT_TRUE(client1_.ConnectIceTransport(&client2_));

  for (int i = 1; i < 3; i++) {
    client1_.SendIcePing();
    ASSERT_TRUE(WaitUntil([&] {
      return client2_.fake_ice_transport()->GetCountOfReceivedStunMessages(
                 STUN_BINDING_REQUEST) == i;
    }));
    client2_.SendIcePingConf();
    ASSERT_TRUE(WaitUntil([&] {
      return client1_.fake_ice_transport()->GetCountOfReceivedStunMessages(
                 STUN_BINDING_RESPONSE) == i;
    }));
    client2_.SendIcePing();
    ASSERT_TRUE(WaitUntil([&] {
      return client1_.fake_ice_transport()->GetCountOfReceivedStunMessages(
                 STUN_BINDING_REQUEST) == i;
    }));
    client1_.SendIcePingConf();
    ASSERT_TRUE(WaitUntil([&] {
      return client2_.fake_ice_transport()->GetCountOfReceivedStunMessages(
                 STUN_BINDING_RESPONSE) == i;
    }));
    if (client1_.dtls_transport()->writable() &&
        client2_.dtls_transport()->writable()) {
      break;
    }
  }

  EXPECT_TRUE(WaitUntil([&] {
    return client1_.dtls_transport()->writable() &&
           client2_.dtls_transport()->writable();
  }));

  EXPECT_TRUE(client1_.dtls_transport()->writable());
  EXPECT_TRUE(client2_.dtls_transport()->writable());

  ClearPacketFilters();
}

TEST_P(DtlsTransportDtlsInStunTest, Handshake2) {
  Prepare(/* rtt_estimate= */ false);
  AddPacketLogging();

  RTC_LOG(LS_INFO) << "client1: " << std::get<0>(GetParam());
  RTC_LOG(LS_INFO) << "client2: " << std::get<1>(GetParam());

  ASSERT_TRUE(client1_.ConnectIceTransport(&client2_));

  for (int i = 1; i < 3; i++) {
    client1_.SendIcePing();
    client2_.SendIcePing();
    ASSERT_TRUE(WaitUntil([&] {
      return client1_.fake_ice_transport()->GetCountOfReceivedStunMessages(
                 STUN_BINDING_REQUEST) == i;
    }));
    ASSERT_TRUE(WaitUntil([&] {
      return client2_.fake_ice_transport()->GetCountOfReceivedStunMessages(
                 STUN_BINDING_REQUEST) == i;
    }));
    client1_.SendIcePingConf();
    client2_.SendIcePingConf();

    ASSERT_TRUE(WaitUntil([&] {
      return client1_.fake_ice_transport()->GetCountOfReceivedStunMessages(
                 STUN_BINDING_RESPONSE) == i;
    }));
    ASSERT_TRUE(WaitUntil([&] {
      return client2_.fake_ice_transport()->GetCountOfReceivedStunMessages(
                 STUN_BINDING_RESPONSE) == i;
    }));
    if (client1_.dtls_transport()->writable() &&
        client2_.dtls_transport()->writable()) {
      break;
    }
  }

  EXPECT_TRUE(WaitUntil([&] {
    return client1_.dtls_transport()->writable() &&
           client2_.dtls_transport()->writable();
  }));

  EXPECT_TRUE(client1_.dtls_transport()->writable());
  EXPECT_TRUE(client2_.dtls_transport()->writable());

  ClearPacketFilters();
}

// Test scenario where DTLS is partially transferred with
// STUN and the "rest" of the handshake is transported
// by DtlsTransport.
TEST_P(DtlsTransportDtlsInStunTest, PartiallyPiggybacked) {
  Prepare(/* rtt_estimate= */ false);
  AddPacketLogging();

  RTC_LOG(LS_INFO) << "client1: " << std::get<0>(GetParam());
  RTC_LOG(LS_INFO) << "client2: " << std::get<1>(GetParam());

  ASSERT_TRUE(client1_.ConnectIceTransport(&client2_));

  for (int i = 1; i < 2; i++) {
    client1_.SendIcePing();
    client2_.SendIcePing();
    ASSERT_TRUE(WaitUntil([&] {
      return client1_.fake_ice_transport()->GetCountOfReceivedStunMessages(
                 STUN_BINDING_REQUEST) == i;
    }));
    ASSERT_TRUE(WaitUntil([&] {
      return client2_.fake_ice_transport()->GetCountOfReceivedStunMessages(
                 STUN_BINDING_REQUEST) == i;
    }));
    client1_.SendIcePingConf();
    client2_.SendIcePingConf();

    ASSERT_TRUE(WaitUntil([&] {
      return client1_.fake_ice_transport()->GetCountOfReceivedStunMessages(
                 STUN_BINDING_RESPONSE) == i;
    }));
    ASSERT_TRUE(WaitUntil([&] {
      return client2_.fake_ice_transport()->GetCountOfReceivedStunMessages(
                 STUN_BINDING_RESPONSE) == i;
    }));
    if (client1_.dtls_transport()->writable() &&
        client2_.dtls_transport()->writable()) {
      break;
    }
  }

  EXPECT_FALSE(client1_.dtls_transport()->writable() &&
               client2_.dtls_transport()->writable());

  EXPECT_TRUE(WaitUntil([&] {
    return client1_.dtls_transport()->writable() &&
           client2_.dtls_transport()->writable();
  }));

  EXPECT_TRUE(client1_.dtls_transport()->writable());
  EXPECT_TRUE(client2_.dtls_transport()->writable());

  ClearPacketFilters();
}

INSTANTIATE_TEST_SUITE_P(DtlsTransportDtlsInStunTest,
                         DtlsTransportDtlsInStunTest,
                         testing::ValuesIn(AllEndpointVariants()));

}  // namespace cricket
