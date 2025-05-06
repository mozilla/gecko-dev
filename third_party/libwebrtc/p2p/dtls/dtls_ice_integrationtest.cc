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
#include "api/test/create_network_emulation_manager.h"
#include "api/test/create_time_controller.h"
#include "api/test/network_emulation_manager.h"
#include "api/test/rtc_error_matchers.h"
#include "api/test/simulated_network.h"
#include "api/units/data_rate.h"
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
constexpr int kDefaultTimeout = 30000;

void SetRemoteFingerprintFromCert(
    cricket::DtlsTransport& transport,
    const rtc::scoped_refptr<webrtc::RTCCertificate>& cert) {
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
using ::webrtc::BuiltInNetworkBehaviorConfig;
using ::webrtc::EmulatedEndpoint;
using ::webrtc::EmulatedEndpointConfig;
using ::webrtc::EmulatedNetworkManagerInterface;
using ::webrtc::EmulatedNetworkNode;
using ::webrtc::NetworkEmulationManager;

class DtlsIceIntegrationTest : public ::testing::TestWithParam<std::tuple<
                                   /* client_piggyback= */ bool,
                                   /* server_piggyback= */ bool,
                                   webrtc::SSLProtocolVersion,
                                   /* client_dtls_is_ice_controlling= */ bool>>,
                               public sigslot::has_slots<> {
 public:
  void CandidateC2S(IceTransportInternal*, const Candidate& c) {
    server_thread()->PostTask(
        [this, c = c]() { server_.ice->AddRemoteCandidate(c); });
  }
  void CandidateS2C(IceTransportInternal*, const Candidate& c) {
    client_thread()->PostTask(
        [this, c = c]() { client_.ice->AddRemoteCandidate(c); });
  }

 private:
  struct Endpoint {
    webrtc::EmulatedNetworkManagerInterface* emulated_network_manager = nullptr;
    std::unique_ptr<rtc::NetworkManager> network_manager;
    std::unique_ptr<rtc::BasicPacketSocketFactory> packet_socket_factory;
    std::unique_ptr<PortAllocator> allocator;
    std::unique_ptr<IceTransportInternal> ice;
    std::unique_ptr<DtlsTransport> dtls;
    std::unique_ptr<DtlsTransport> server_dtls_;
  };

 protected:
  DtlsIceIntegrationTest()
      : ss_(std::make_unique<rtc::VirtualSocketServer>()),
        socket_factory_(
            std::make_unique<rtc::BasicPacketSocketFactory>(ss_.get())),
        client_ice_parameters_("c_ufrag",
                               "c_icepwd_something_something",
                               false),
        server_ice_parameters_("s_ufrag",
                               "s_icepwd_something_something",
                               false),
        client_dtls_stun_piggyback_(std::get<0>(GetParam())),
        server_dtls_stun_piggyback_(std::get<1>(GetParam())) {}

  void ConfigureEmulatedNetwork() {
    network_emulation_manager_ = webrtc::CreateNetworkEmulationManager(
        {.time_mode = webrtc::TimeMode::kSimulated});

    BuiltInNetworkBehaviorConfig networkBehavior;
    networkBehavior.link_capacity = webrtc::DataRate::KilobitsPerSec(200);
    // TODO (webrtc:383141571) : Investigate why this testcase fails for
    // DTLS 1.3 delay if networkBehavior.queue_delay_ms = 100ms.
    // - unless both peers support dtls in stun, in which case it passes.
    // - note: only for dtls1.3, it works for dtls1.2!
    networkBehavior.queue_delay_ms = 50;
    networkBehavior.queue_length_packets = 30;
    networkBehavior.loss_percent = 50;
    auto pair = network_emulation_manager_->CreateEndpointPairWithTwoWayRoutes(
        networkBehavior);

    client_.emulated_network_manager = pair.first;
    server_.emulated_network_manager = pair.second;
  }

  void SetupEndpoint(
      Endpoint& ep,
      bool client,
      const rtc::scoped_refptr<webrtc::RTCCertificate> client_certificate,
      const rtc::scoped_refptr<webrtc::RTCCertificate> server_certificate) {
    thread(ep)->BlockingCall([&]() {
      if (network_emulation_manager_ == nullptr) {
        ep.allocator = std::make_unique<BasicPortAllocator>(
            &network_manager_, socket_factory_.get());
      } else {
        ep.network_manager =
            ep.emulated_network_manager->ReleaseNetworkManager();
        ep.packet_socket_factory =
            std::make_unique<rtc::BasicPacketSocketFactory>(
                ep.emulated_network_manager->socket_factory());
        ep.allocator = std::make_unique<BasicPortAllocator>(
            ep.network_manager.get(), ep.packet_socket_factory.get());
      }
      ep.allocator->set_flags(ep.allocator->flags() |
                              cricket::PORTALLOCATOR_DISABLE_TCP);
      ep.ice = std::make_unique<P2PTransportChannel>(
          client ? "client_transport" : "server_transport", 0,
          ep.allocator.get());
      ep.dtls = std::make_unique<DtlsTransport>(
          ep.ice.get(), webrtc::CryptoOptions(),
          /*event_log=*/nullptr, std::get<2>(GetParam()));

      // Enable(or disable) the dtls_in_stun parameter before
      // DTLS is negotiated.
      cricket::IceConfig config;
      config.continual_gathering_policy = GATHER_CONTINUALLY;
      config.dtls_handshake_in_stun =
          client ? client_dtls_stun_piggyback_ : server_dtls_stun_piggyback_;
      ep.ice->SetIceConfig(config);

      // Setup ICE.
      ep.ice->SetIceParameters(client ? client_ice_parameters_
                                      : server_ice_parameters_);
      ep.ice->SetRemoteIceParameters(client ? server_ice_parameters_
                                            : client_ice_parameters_);
      if (client) {
        ep.ice->SetIceRole(std::get<3>(GetParam()) ? ICEROLE_CONTROLLED
                                                   : ICEROLE_CONTROLLING);
      } else {
        ep.ice->SetIceRole(std::get<3>(GetParam()) ? ICEROLE_CONTROLLING
                                                   : ICEROLE_CONTROLLED);
      }
      if (client) {
        ep.ice->SignalCandidateGathered.connect(
            this, &DtlsIceIntegrationTest::CandidateC2S);
      } else {
        ep.ice->SignalCandidateGathered.connect(
            this, &DtlsIceIntegrationTest::CandidateS2C);
      }

      // Setup DTLS.
      ep.dtls->SetLocalCertificate(client ? client_certificate
                                          : server_certificate);
      ep.dtls->SetDtlsRole(client ? webrtc::SSL_SERVER : webrtc::SSL_CLIENT);
      SetRemoteFingerprintFromCert(
          *ep.dtls.get(), client ? server_certificate : client_certificate);
    });
  }

  void Prepare() {
    auto client_certificate = webrtc::RTCCertificate::Create(
        rtc::SSLIdentity::Create("test", rtc::KT_DEFAULT));
    auto server_certificate = webrtc::RTCCertificate::Create(
        rtc::SSLIdentity::Create("test", rtc::KT_DEFAULT));

    if (network_emulation_manager_ == nullptr) {
      thread_ = std::make_unique<rtc::AutoSocketServerThread>(ss_.get());
    }

    client_thread()->BlockingCall([&]() {
      SetupEndpoint(client_, /* client= */ true, client_certificate,
                    server_certificate);
    });

    server_thread()->BlockingCall([&]() {
      SetupEndpoint(server_, /* client= */ false, client_certificate,
                    server_certificate);
    });

    // Setup the network.
    if (network_emulation_manager_ == nullptr) {
      network_manager_.AddInterface(webrtc::SocketAddress("192.168.1.1", 0));
    }

    client_thread()->BlockingCall([&]() { client_.allocator->Initialize(); });
    server_thread()->BlockingCall([&]() { server_.allocator->Initialize(); });
  }

  void TearDown() {
    client_thread()->BlockingCall([&]() {
      client_.dtls.reset();
      client_.ice.reset();
      client_.allocator.reset();
    });

    server_thread()->BlockingCall([&]() {
      server_.dtls.reset();
      server_.ice.reset();
      server_.allocator.reset();
    });
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

  webrtc::WaitUntilSettings wait_until_settings() {
    if (network_emulation_manager_ == nullptr) {
      return {
          .timeout = webrtc::TimeDelta::Millis(kDefaultTimeout),
          .clock = &fake_clock_,
      };
    } else {
      return {
          .timeout = webrtc::TimeDelta::Millis(kDefaultTimeout),
          .clock = network_emulation_manager_->time_controller(),
      };
    }
  }

  rtc::Thread* thread(Endpoint& ep) {
    if (ep.emulated_network_manager == nullptr) {
      return thread_.get();
    } else {
      return ep.emulated_network_manager->network_thread();
    }
  }

  rtc::Thread* client_thread() { return thread(client_); }

  rtc::Thread* server_thread() { return thread(server_); }

  rtc::ScopedFakeClock fake_clock_;
  rtc::FakeNetworkManager network_manager_;
  std::unique_ptr<rtc::VirtualSocketServer> ss_;
  std::unique_ptr<rtc::BasicPacketSocketFactory> socket_factory_;
  std::unique_ptr<webrtc::NetworkEmulationManager> network_emulation_manager_;
  std::unique_ptr<rtc::AutoSocketServerThread> thread_;

  Endpoint client_;
  Endpoint server_;

  IceParameters client_ice_parameters_;
  IceParameters server_ice_parameters_;

  bool client_dtls_stun_piggyback_;
  bool server_dtls_stun_piggyback_;
};

TEST_P(DtlsIceIntegrationTest, SmokeTest) {
  Prepare();
  client_.ice->MaybeStartGathering();
  server_.ice->MaybeStartGathering();

  // Note: this only reaches the pending piggybacking state.
  EXPECT_THAT(
      webrtc::WaitUntil(
          [&] { return client_.dtls->writable() && server_.dtls->writable(); },
          IsTrue(), wait_until_settings()),
      webrtc::IsRtcOk());
  EXPECT_EQ(client_.dtls->IsDtlsPiggybackSupportedByPeer(),
            client_dtls_stun_piggyback_ && server_dtls_stun_piggyback_);
  EXPECT_EQ(server_.dtls->IsDtlsPiggybackSupportedByPeer(),
            client_dtls_stun_piggyback_ && server_dtls_stun_piggyback_);

  // Validate that we can add new Connections (that become writable).
  network_manager_.AddInterface(webrtc::SocketAddress("192.168.2.1", 0));
  EXPECT_THAT(webrtc::WaitUntil(
                  [&] {
                    return CountWritableConnections(client_.ice.get()) > 1 &&
                           CountWritableConnections(server_.ice.get()) > 1;
                  },
                  IsTrue(), wait_until_settings()),
              webrtc::IsRtcOk());
}

TEST_P(DtlsIceIntegrationTest, TestWithPacketLoss) {
  ConfigureEmulatedNetwork();
  Prepare();

  client_thread()->PostTask([&]() { client_.ice->MaybeStartGathering(); });

  server_thread()->PostTask([&]() { server_.ice->MaybeStartGathering(); });

  EXPECT_THAT(webrtc::WaitUntil(
                  [&] {
                    return client_thread()->BlockingCall([&]() {
                      return client_.dtls->writable();
                    }) && server_thread()->BlockingCall([&]() {
                      return server_.dtls->writable();
                    });
                  },
                  IsTrue(), wait_until_settings()),
              webrtc::IsRtcOk());

  EXPECT_EQ(client_thread()->BlockingCall([&]() {
    return client_.dtls->IsDtlsPiggybackSupportedByPeer();
  }),
            client_dtls_stun_piggyback_ && server_dtls_stun_piggyback_);
  EXPECT_EQ(server_thread()->BlockingCall([&]() {
    return server_.dtls->IsDtlsPiggybackSupportedByPeer();
  }),
            client_dtls_stun_piggyback_ && server_dtls_stun_piggyback_);
}

// Test cases are parametrized by
// * client-piggybacking-enabled,
// * server-piggybacking-enabled,
// * maximum DTLS version to use.
INSTANTIATE_TEST_SUITE_P(
    DtlsStunPiggybackingIntegrationTest,
    DtlsIceIntegrationTest,
    ::testing::Combine(testing::Bool(),
                       testing::Bool(),
                       testing::Values(webrtc::SSL_PROTOCOL_DTLS_12,
                                       webrtc::SSL_PROTOCOL_DTLS_13),
                       testing::Bool()));

}  // namespace cricket
