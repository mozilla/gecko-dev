/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_TEST_MOCK_ICE_TRANSPORT_H_
#define P2P_TEST_MOCK_ICE_TRANSPORT_H_

#include <cstddef>
#include <optional>
#include <string>

#include "api/candidate.h"
#include "api/transport/enums.h"
#include "p2p/base/candidate_pair_interface.h"
#include "p2p/base/connection.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/transport_description.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/socket.h"
#include "test/gmock.h"

namespace webrtc {

// Used in Chromium/remoting/protocol/channel_socket_adapter_unittest.cc
class MockIceTransport : public IceTransportInternal {
 public:
  MockIceTransport() {
    SignalReadyToSend(this);
    SignalWritableState(this);
  }

  MOCK_METHOD(int,
              SendPacket,
              (const char* data,
               size_t len,
               const rtc::PacketOptions& options,
               int flags),
              (override));
  MOCK_METHOD(int, SetOption, (rtc::Socket::Option opt, int value), (override));
  MOCK_METHOD(int, GetError, (), (override));
  MOCK_METHOD(cricket::IceRole, GetIceRole, (), (const, override));
  MOCK_METHOD(bool,
              GetStats,
              (webrtc::IceTransportStats * ice_transport_stats),
              (override));

  cricket::IceTransportState GetState() const override {
    return cricket::IceTransportState::STATE_INIT;
  }
  IceTransportState GetIceTransportState() const override {
    return IceTransportState::kNew;
  }

  const std::string& transport_name() const override { return transport_name_; }
  int component() const override { return 0; }
  void SetIceRole(cricket::IceRole /* role */) override {}
  // The ufrag and pwd in `ice_params` must be set
  // before candidate gathering can start.
  void SetIceParameters(
      const cricket::IceParameters& /* ice_params */) override {}
  void SetRemoteIceParameters(
      const cricket::IceParameters& /* ice_params */) override {}
  void SetRemoteIceMode(cricket::IceMode /* mode */) override {}
  void SetIceConfig(const IceConfig& config) override { ice_config_ = config; }
  const IceConfig& config() const override { return ice_config_; }
  std::optional<int> GetRttEstimate() override { return std::nullopt; }
  const cricket::Connection* selected_connection() const override {
    return nullptr;
  }
  std::optional<const cricket::CandidatePair> GetSelectedCandidatePair()
      const override {
    return std::nullopt;
  }
  void MaybeStartGathering() override {}
  void AddRemoteCandidate(const cricket::Candidate& /* candidate */) override {}
  void RemoveRemoteCandidate(
      const cricket::Candidate& /* candidate */) override {}
  void RemoveAllRemoteCandidates() override {}
  IceGatheringState gathering_state() const override {
    return IceGatheringState::kIceGatheringComplete;
  }

  bool receiving() const override { return true; }
  bool writable() const override { return true; }

 private:
  std::string transport_name_;
  IceConfig ice_config_;
};

}  //  namespace webrtc

// Re-export symbols from the webrtc namespace for backwards compatibility.
// TODO(bugs.webrtc.org/4222596): Remove once all references are updated.
namespace cricket {
using ::webrtc::MockIceTransport;
}  // namespace cricket

#endif  // P2P_TEST_MOCK_ICE_TRANSPORT_H_
