/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdint>
#include <memory>
#include <optional>
#include <tuple>

#include "api/candidate.h"
#include "api/crypto/crypto_options.h"
#include "api/scoped_refptr.h"
#include "api/test/rtc_error_matchers.h"
#include "api/units/time_delta.h"
#include "p2p/base/basic_packet_socket_factory.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/p2p_transport_channel.h"
#include "p2p/base/port_allocator.h"
#include "p2p/base/transport_description.h"
#include "p2p/client/basic_port_allocator.h"
#include "p2p/dtls/dtls_transport.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/fake_network.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/ssl_fingerprint.h"
#include "rtc_base/ssl_identity.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/thread.h"
#include "rtc_base/virtual_socket_server.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/wait_until.h"

namespace {
constexpr int kDefaultTimeout = 10000;

void SetRemoteFingerprintFromCert(
    cricket::DtlsTransport& transport,
    const rtc::scoped_refptr<rtc::RTCCertificate>& cert) {
  std::unique_ptr<rtc::SSLFingerprint> fingerprint =
      rtc::SSLFingerprint::CreateFromCertificate(*cert);

  transport.SetRemoteParameters(
      fingerprint->algorithm,
      reinterpret_cast<const uint8_t*>(fingerprint->digest.data()),
      fingerprint->digest.size(), std::nullopt);
}

}  // namespace

namespace cricket {

using ::testing::IsTrue;

class DtlsIceIntegrationTest
    : public ::testing::TestWithParam<
          std::tuple<bool, bool, rtc::SSLProtocolVersion>>,
      public sigslot::has_slots<> {
 public:
  void CandidateC2S(IceTransportInternal*, const Candidate& c) {
    thread_.PostTask([this, c = c]() { server_ice_->AddRemoteCandidate(c); });
  }
  void CandidateS2C(IceTransportInternal*, const Candidate& c) {
    thread_.PostTask([this, c = c]() { client_ice_->AddRemoteCandidate(c); });
  }

 protected:
  DtlsIceIntegrationTest()
      : ss_(std::make_unique<rtc::VirtualSocketServer>()),
        socket_factory_(
            std::make_unique<rtc::BasicPacketSocketFactory>(ss_.get())),
        thread_(ss_.get()),
        client_allocator_(
            std::make_unique<BasicPortAllocator>(&network_manager_,
                                                 socket_factory_.get())),
        server_allocator_(
            std::make_unique<BasicPortAllocator>(&network_manager_,
                                                 socket_factory_.get())),
        client_ice_(
            std::make_unique<P2PTransportChannel>("client_transport",
                                                  0,
                                                  client_allocator_.get())),
        server_ice_(
            std::make_unique<P2PTransportChannel>("server_transport",
                                                  0,
                                                  server_allocator_.get())),
        client_dtls_(client_ice_.get(),
                     webrtc::CryptoOptions(),
                     /*event_log=*/nullptr,
                     std::get<2>(GetParam())),
        server_dtls_(server_ice_.get(),
                     webrtc::CryptoOptions(),
                     /*event_log=*/nullptr,
                     std::get<2>(GetParam())),
        client_ice_parameters_("c_ufrag",
                               "c_icepwd_something_something",
                               false),
        server_ice_parameters_("s_ufrag",
                               "s_icepwd_something_something",
                               false),
        client_dtls_stun_piggyback_(std::get<0>(GetParam())),
        server_dtls_stun_piggyback_(std::get<1>(GetParam())) {
    // Enable(or disable) the dtls_in_stun parameter before
    // DTLS is negotiated.
    cricket::IceConfig client_config;
    client_config.continual_gathering_policy = GATHER_CONTINUALLY;
    client_config.dtls_handshake_in_stun = client_dtls_stun_piggyback_;
    client_ice_->SetIceConfig(client_config);

    cricket::IceConfig server_config;
    server_config.dtls_handshake_in_stun = server_dtls_stun_piggyback_;
    server_config.continual_gathering_policy = GATHER_CONTINUALLY;
    server_ice_->SetIceConfig(server_config);

    // Setup ICE.
    client_ice_->SetIceParameters(client_ice_parameters_);
    client_ice_->SetRemoteIceParameters(server_ice_parameters_);
    client_ice_->SetIceRole(ICEROLE_CONTROLLING);
    client_ice_->SignalCandidateGathered.connect(
        this, &DtlsIceIntegrationTest::CandidateC2S);
    server_ice_->SetIceParameters(server_ice_parameters_);
    server_ice_->SetRemoteIceParameters(client_ice_parameters_);
    server_ice_->SetIceRole(ICEROLE_CONTROLLED);
    server_ice_->SignalCandidateGathered.connect(
        this, &DtlsIceIntegrationTest::CandidateS2C);

    // Setup DTLS.
    auto client_certificate = rtc::RTCCertificate::Create(
        rtc::SSLIdentity::Create("test", rtc::KT_DEFAULT));
    client_dtls_.SetLocalCertificate(client_certificate);
    client_dtls_.SetDtlsRole(rtc::SSL_SERVER);
    auto server_certificate = rtc::RTCCertificate::Create(
        rtc::SSLIdentity::Create("test", rtc::KT_DEFAULT));
    server_dtls_.SetLocalCertificate(server_certificate);
    server_dtls_.SetDtlsRole(rtc::SSL_CLIENT);

    SetRemoteFingerprintFromCert(server_dtls_, client_certificate);
    SetRemoteFingerprintFromCert(client_dtls_, server_certificate);

    // Setup the network.
    network_manager_.AddInterface(rtc::SocketAddress("192.168.1.1", 0));
    client_allocator_->Initialize();
    server_allocator_->Initialize();
  }

  ~DtlsIceIntegrationTest() = default;

  static int CountWritableConnections(IceTransportInternal* ice) {
    IceTransportStats stats;
    ice->GetStats(&stats);
    int count = 0;
    for (const auto& con : stats.connection_infos) {
      if (con.writable) {
        count++;
      }
    }
    return count;
  }

  rtc::FakeNetworkManager network_manager_;
  std::unique_ptr<rtc::VirtualSocketServer> ss_;
  std::unique_ptr<rtc::BasicPacketSocketFactory> socket_factory_;
  rtc::AutoSocketServerThread thread_;

  std::unique_ptr<PortAllocator> client_allocator_;
  std::unique_ptr<PortAllocator> server_allocator_;

  std::unique_ptr<IceTransportInternal> client_ice_;
  std::unique_ptr<IceTransportInternal> server_ice_;

  DtlsTransport client_dtls_;
  DtlsTransport server_dtls_;

  IceParameters client_ice_parameters_;
  IceParameters server_ice_parameters_;

  bool client_dtls_stun_piggyback_;
  bool server_dtls_stun_piggyback_;

  rtc::ScopedFakeClock fake_clock_;
};

TEST_P(DtlsIceIntegrationTest, SmokeTest) {
  client_ice_->MaybeStartGathering();
  server_ice_->MaybeStartGathering();

  // Note: this only reaches the pending piggybacking state.
  EXPECT_THAT(
      webrtc::WaitUntil(
          [&] { return client_dtls_.writable() && server_dtls_.writable(); },
          IsTrue(),
          {.timeout = webrtc::TimeDelta::Millis(kDefaultTimeout),
           .clock = &fake_clock_}),
      webrtc::IsRtcOk());
  EXPECT_EQ(client_dtls_.IsDtlsPiggybackSupportedByPeer(),
            client_dtls_stun_piggyback_ && server_dtls_stun_piggyback_);
  EXPECT_EQ(server_dtls_.IsDtlsPiggybackSupportedByPeer(),
            client_dtls_stun_piggyback_ && server_dtls_stun_piggyback_);

  // Validate that we can add new Connections (that become writable).
  network_manager_.AddInterface(rtc::SocketAddress("192.168.2.1", 0));
  EXPECT_THAT(webrtc::WaitUntil(
                  [&] {
                    return CountWritableConnections(client_ice_.get()) > 1 &&
                           CountWritableConnections(server_ice_.get()) > 1;
                  },
                  IsTrue(),
                  {.timeout = webrtc::TimeDelta::Millis(kDefaultTimeout),
                   .clock = &fake_clock_}),
              webrtc::IsRtcOk());
}

// Test cases are parametrized by
// * client-piggybacking-enabled,
// * server-piggybacking-enabled,
// * maximum DTLS version to use.
INSTANTIATE_TEST_SUITE_P(
    DtlsStunPiggybackingIntegrationTest,
    DtlsIceIntegrationTest,
    ::testing::Values(std::make_tuple(false, false, rtc::SSL_PROTOCOL_DTLS_12),
                      std::make_tuple(true, false, rtc::SSL_PROTOCOL_DTLS_12),
                      std::make_tuple(false, true, rtc::SSL_PROTOCOL_DTLS_12),
                      std::make_tuple(true, true, rtc::SSL_PROTOCOL_DTLS_12),

                      std::make_tuple(false, false, rtc::SSL_PROTOCOL_DTLS_13),
                      std::make_tuple(true, false, rtc::SSL_PROTOCOL_DTLS_13),
                      std::make_tuple(false, true, rtc::SSL_PROTOCOL_DTLS_13),
                      std::make_tuple(true, true, rtc::SSL_PROTOCOL_DTLS_13)));

}  // namespace cricket
