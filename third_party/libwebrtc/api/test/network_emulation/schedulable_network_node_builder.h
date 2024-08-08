/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_TEST_NETWORK_EMULATION_SCHEDULABLE_NETWORK_NODE_BUILDER_H_
#define API_TEST_NETWORK_EMULATION_SCHEDULABLE_NETWORK_NODE_BUILDER_H_

#include "api/test/network_emulation/network_config_schedule.pb.h"
#include "api/test/network_emulation_manager.h"

namespace webrtc {

class SchedulableNetworkNodeBuilder {
 public:
  SchedulableNetworkNodeBuilder(
      webrtc::NetworkEmulationManager& net,
      network_behaviour::NetworkConfigSchedule schedule);

  webrtc::EmulatedNetworkNode* Build();

 private:
  webrtc::NetworkEmulationManager& net_;
  network_behaviour::NetworkConfigSchedule schedule_;
};

}  // namespace webrtc

#endif  // API_TEST_NETWORK_EMULATION_SCHEDULABLE_NETWORK_NODE_BUILDER_H_
