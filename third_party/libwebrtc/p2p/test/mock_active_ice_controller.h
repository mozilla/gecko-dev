/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_TEST_MOCK_ACTIVE_ICE_CONTROLLER_H_
#define P2P_TEST_MOCK_ACTIVE_ICE_CONTROLLER_H_

#include <memory>

#include "p2p/base/active_ice_controller_factory_interface.h"
#include "p2p/base/active_ice_controller_interface.h"
#include "p2p/base/connection.h"
#include "p2p/base/ice_switch_reason.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/transport_description.h"
#include "test/gmock.h"

namespace webrtc {

class MockActiveIceController : public cricket::ActiveIceControllerInterface {
 public:
  explicit MockActiveIceController(
      const cricket::ActiveIceControllerFactoryArgs& /* args */) {}
  ~MockActiveIceController() override = default;

  MOCK_METHOD(void, SetIceConfig, (const webrtc::IceConfig&), (override));
  MOCK_METHOD(void,
              OnConnectionAdded,
              (const cricket::Connection*),
              (override));
  MOCK_METHOD(void,
              OnConnectionSwitched,
              (const cricket::Connection*),
              (override));
  MOCK_METHOD(void,
              OnConnectionDestroyed,
              (const cricket::Connection*),
              (override));
  MOCK_METHOD(void,
              OnConnectionPinged,
              (const cricket::Connection*),
              (override));
  MOCK_METHOD(void,
              OnConnectionUpdated,
              (const cricket::Connection*),
              (override));
  MOCK_METHOD(bool,
              GetUseCandidateAttribute,
              (const cricket::Connection*,
               webrtc::NominationMode,
               cricket::IceMode),
              (const, override));
  MOCK_METHOD(void,
              OnSortAndSwitchRequest,
              (cricket::IceSwitchReason),
              (override));
  MOCK_METHOD(void,
              OnImmediateSortAndSwitchRequest,
              (cricket::IceSwitchReason),
              (override));
  MOCK_METHOD(bool,
              OnImmediateSwitchRequest,
              (cricket::IceSwitchReason, const cricket::Connection*),
              (override));
  MOCK_METHOD(const cricket::Connection*,
              FindNextPingableConnection,
              (),
              (override));
};

class MockActiveIceControllerFactory
    : public cricket::ActiveIceControllerFactoryInterface {
 public:
  ~MockActiveIceControllerFactory() override = default;

  std::unique_ptr<cricket::ActiveIceControllerInterface> Create(
      const cricket::ActiveIceControllerFactoryArgs& args) {
    RecordActiveIceControllerCreated();
    return std::make_unique<MockActiveIceController>(args);
  }

  MOCK_METHOD(void, RecordActiveIceControllerCreated, ());
};

}  //  namespace webrtc

// Re-export symbols from the webrtc namespace for backwards compatibility.
// TODO(bugs.webrtc.org/4222596): Remove once all references are updated.
namespace cricket {
using ::webrtc::MockActiveIceController;
using ::webrtc::MockActiveIceControllerFactory;
}  // namespace cricket

#endif  // P2P_TEST_MOCK_ACTIVE_ICE_CONTROLLER_H_
