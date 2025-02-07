/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/dtls/dtls_stun_piggyback_controller.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "api/array_view.h"
#include "api/transport/stun.h"
#include "test/gtest.h"

namespace {
// Extracted from a stock DTLS call using Wireshark.
// Each packet (apart from the last) is truncated to
// the first fragment to keep things short.

// Based on a "server hello done" but with different msg_seq.
const std::vector<uint8_t> dtls_flight1 = {
    0x16, 0xfe, 0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
    0x00, 0x01,                                            // seq=1
    0x00, 0x0c, 0x0e, 0x00, 0x00, 0x00, 0x12, 0x34, 0x00,  // msg_seq=0x1234
    0x00, 0x00, 0x00, 0x00, 0x00};

const std::vector<uint8_t> dtls_flight2 = {
    0x16, 0xfe, 0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
    0x00, 0x02,                                            // seq=2
    0x00, 0x0c, 0x0e, 0x00, 0x00, 0x00, 0x43, 0x21, 0x00,  // msg_seq=0x4321
    0x00, 0x00, 0x00, 0x00, 0x00};

const std::vector<uint8_t> dtls_flight3 = {
    0x16, 0xfe, 0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
    0x00, 0x03,                                            // seq=3
    0x00, 0x0c, 0x0e, 0x00, 0x00, 0x00, 0x44, 0x44, 0x00,  // msg_seq=0x4444
    0x00, 0x00, 0x00, 0x00, 0x00};

const std::vector<uint8_t> dtls_flight4 = {
    0x16, 0xfe, 0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
    0x00, 0x04,                                            // seq=4
    0x00, 0x0c, 0x0e, 0x00, 0x00, 0x00, 0x54, 0x86, 0x00,  // msg_seq=0x5486
    0x00, 0x00, 0x00, 0x00, 0x00};

const std::vector<uint8_t> empty = {};
}  // namespace

namespace cricket {

using State = DtlsStunPiggybackController::State;

class DtlsStunPiggybackControllerTest : public ::testing::Test {
 protected:
  DtlsStunPiggybackControllerTest()
      : client_([](rtc::ArrayView<const uint8_t> data) {}),
        server_([](rtc::ArrayView<const uint8_t> data) {}) {}

  void SendClientToServer(const std::vector<uint8_t> data,
                          StunMessageType type) {
    client_.SetDataToPiggyback(data);
    std::unique_ptr<StunByteStringAttribute> attr_data;
    if (client_.GetDataToPiggyback(type)) {
      attr_data = std::make_unique<StunByteStringAttribute>(
          STUN_ATTR_META_DTLS_IN_STUN, *client_.GetDataToPiggyback(type));
    }
    std::unique_ptr<StunByteStringAttribute> attr_ack;
    if (client_.GetAckToPiggyback(type)) {
      attr_ack = std::make_unique<StunByteStringAttribute>(
          STUN_ATTR_META_DTLS_IN_STUN_ACK, *client_.GetAckToPiggyback(type));
    }
    server_.ReportDataPiggybacked(attr_data.get(), attr_ack.get());
    if (data == dtls_flight3) {
      // When receiving flight 3, server handshake is complete.
      server_.SetDtlsHandshakeComplete(/*is_client=*/false);
    }
  }
  void SendServerToClient(const std::vector<uint8_t> data,
                          StunMessageType type) {
    server_.SetDataToPiggyback(data);
    std::unique_ptr<StunByteStringAttribute> attr_data;
    if (server_.GetDataToPiggyback(type)) {
      attr_data = std::make_unique<StunByteStringAttribute>(
          STUN_ATTR_META_DTLS_IN_STUN, *server_.GetDataToPiggyback(type));
    }
    std::unique_ptr<StunByteStringAttribute> attr_ack;
    if (server_.GetAckToPiggyback(type)) {
      attr_ack = std::make_unique<StunByteStringAttribute>(
          STUN_ATTR_META_DTLS_IN_STUN_ACK, *server_.GetAckToPiggyback(type));
    }
    client_.ReportDataPiggybacked(attr_data.get(), attr_ack.get());
    if (data == dtls_flight4) {
      // When receiving flight 4, client handshake is complete.
      client_.SetDtlsHandshakeComplete(/*is_client=*/true);
    }
  }

  void DisableSupport(DtlsStunPiggybackController& client_or_server) {
    ASSERT_EQ(client_or_server.state(), State::TENTATIVE);
    client_or_server.ReportDataPiggybacked(nullptr, nullptr);
    ASSERT_EQ(client_or_server.state(), State::OFF);
  }

  DtlsStunPiggybackController client_;
  DtlsStunPiggybackController server_;
};

TEST_F(DtlsStunPiggybackControllerTest, BasicHandshake) {
  // Flight 1+2
  SendClientToServer(dtls_flight1, STUN_BINDING_REQUEST);
  EXPECT_EQ(server_.state(), State::CONFIRMED);
  SendServerToClient(dtls_flight2, STUN_BINDING_RESPONSE);
  EXPECT_EQ(client_.state(), State::CONFIRMED);

  // Flight 3+4
  SendClientToServer(dtls_flight3, STUN_BINDING_REQUEST);
  SendServerToClient(dtls_flight4, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::PENDING);
  EXPECT_EQ(client_.state(), State::PENDING);

  // Post-handshake ACK
  SendServerToClient(empty, STUN_BINDING_REQUEST);
  SendClientToServer(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::COMPLETE);
  EXPECT_EQ(client_.state(), State::COMPLETE);
}

TEST_F(DtlsStunPiggybackControllerTest, FirstClientPacketLost) {
  // Client to server got lost (or arrives late)
  // Flight 1
  SendServerToClient(empty, STUN_BINDING_REQUEST);
  SendClientToServer(dtls_flight1, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::CONFIRMED);
  EXPECT_EQ(client_.state(), State::CONFIRMED);

  // Flight 2+3
  SendServerToClient(dtls_flight2, STUN_BINDING_REQUEST);
  SendClientToServer(dtls_flight3, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::PENDING);
  EXPECT_EQ(client_.state(), State::CONFIRMED);

  // Flight 4
  SendServerToClient(dtls_flight4, STUN_BINDING_REQUEST);
  SendClientToServer(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::COMPLETE);
  EXPECT_EQ(client_.state(), State::PENDING);

  // Post-handshake ACK
  SendServerToClient(empty, STUN_BINDING_REQUEST);
  EXPECT_EQ(client_.state(), State::COMPLETE);
}

TEST_F(DtlsStunPiggybackControllerTest, NotSupportedByServer) {
  DisableSupport(server_);

  // Flight 1
  SendClientToServer(dtls_flight1, STUN_BINDING_REQUEST);
  SendServerToClient(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(client_.state(), State::OFF);
}

TEST_F(DtlsStunPiggybackControllerTest, NotSupportedByServerClientReceives) {
  DisableSupport(server_);

  // Client to server got lost (or arrives late)
  SendServerToClient(empty, STUN_BINDING_REQUEST);
  EXPECT_EQ(client_.state(), State::OFF);
}

TEST_F(DtlsStunPiggybackControllerTest, NotSupportedByClient) {
  DisableSupport(client_);

  SendServerToClient(empty, STUN_BINDING_REQUEST);
  SendClientToServer(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::OFF);
}

TEST_F(DtlsStunPiggybackControllerTest, SomeRequestsDoNotGoThrough) {
  // Client to server got lost (or arrives late)
  // Flight 1
  SendServerToClient(empty, STUN_BINDING_REQUEST);
  SendClientToServer(dtls_flight1, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::CONFIRMED);
  EXPECT_EQ(client_.state(), State::CONFIRMED);

  // Flight 1+2, server sent request got lost.
  SendClientToServer(dtls_flight1, STUN_BINDING_REQUEST);
  SendServerToClient(dtls_flight2, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::CONFIRMED);
  EXPECT_EQ(client_.state(), State::CONFIRMED);

  // Flight 3+4
  SendClientToServer(dtls_flight3, STUN_BINDING_REQUEST);
  SendServerToClient(dtls_flight4, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::PENDING);
  EXPECT_EQ(client_.state(), State::PENDING);

  // Post-handshake ACK
  SendClientToServer(empty, STUN_BINDING_REQUEST);
  SendServerToClient(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::COMPLETE);
  EXPECT_EQ(client_.state(), State::COMPLETE);
}

TEST_F(DtlsStunPiggybackControllerTest, LossOnPostHandshakeAck) {
  // Flight 1+2
  SendClientToServer(dtls_flight1, STUN_BINDING_REQUEST);
  EXPECT_EQ(server_.state(), State::CONFIRMED);
  SendServerToClient(dtls_flight2, STUN_BINDING_RESPONSE);
  EXPECT_EQ(client_.state(), State::CONFIRMED);

  // Flight 3+4
  SendClientToServer(dtls_flight3, STUN_BINDING_REQUEST);
  SendServerToClient(dtls_flight4, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::PENDING);
  EXPECT_EQ(client_.state(), State::PENDING);

  // Post-handshake ACK. Client to server gets lost
  SendServerToClient(empty, STUN_BINDING_REQUEST);
  SendClientToServer(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::COMPLETE);
  EXPECT_EQ(client_.state(), State::COMPLETE);
}

TEST_F(DtlsStunPiggybackControllerTest,
       UnsupportedStateAfterFallbackHandshakeRemainsOff) {
  DisableSupport(client_);
  DisableSupport(server_);

  // Set DTLS complete after normal handshake.
  client_.SetDtlsHandshakeComplete(true);
  EXPECT_EQ(client_.state(), State::OFF);
  server_.SetDtlsHandshakeComplete(true);
  EXPECT_EQ(server_.state(), State::OFF);
}

TEST_F(DtlsStunPiggybackControllerTest, BasicHandshakeAckData) {
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_RESPONSE), "");
  EXPECT_EQ(client_.GetAckToPiggyback(STUN_BINDING_REQUEST), "");

  // Flight 1+2
  SendClientToServer(dtls_flight1, STUN_BINDING_REQUEST);
  SendServerToClient(dtls_flight2, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_REQUEST),
            std::string("\x12\x34", 2));
  EXPECT_EQ(client_.GetAckToPiggyback(STUN_BINDING_RESPONSE),
            std::string("\x43\x21", 2));

  // Flight 3+4
  SendClientToServer(dtls_flight3, STUN_BINDING_REQUEST);
  SendServerToClient(dtls_flight4, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_RESPONSE),
            std::string("\x12\x34\x44\x44", 4));
  EXPECT_EQ(client_.GetAckToPiggyback(STUN_BINDING_REQUEST),
            std::string("\x43\x21\x54\x86", 4));

  // Post-handshake ACK
  SendServerToClient(empty, STUN_BINDING_REQUEST);
  SendClientToServer(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::COMPLETE);
  EXPECT_EQ(client_.state(), State::COMPLETE);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_RESPONSE), std::nullopt);
  EXPECT_EQ(client_.GetAckToPiggyback(STUN_BINDING_REQUEST), std::nullopt);
}

TEST_F(DtlsStunPiggybackControllerTest, AckDataNoDuplicates) {
  // Flight 1+2
  SendClientToServer(dtls_flight1, STUN_BINDING_REQUEST);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_REQUEST),
            std::string("\x12\x34", 2));
  SendClientToServer(dtls_flight3, STUN_BINDING_REQUEST);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_REQUEST),
            std::string("\x12\x34\x44\x44", 4));

  // Receive Flight 1 again, no change expected.
  SendClientToServer(dtls_flight1, STUN_BINDING_REQUEST);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_REQUEST),
            std::string("\x12\x34\x44\x44", 4));
}

}  // namespace cricket
