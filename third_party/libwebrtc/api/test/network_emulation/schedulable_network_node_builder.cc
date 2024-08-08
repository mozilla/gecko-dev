/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/test/network_emulation/schedulable_network_node_builder.h"

#include <memory>
#include <utility>

#include "api/test/network_emulation/network_config_schedule.pb.h"
#include "api/test/network_emulation_manager.h"
#include "test/network/schedulable_network_behavior.h"

namespace webrtc {

SchedulableNetworkNodeBuilder::SchedulableNetworkNodeBuilder(
    webrtc::NetworkEmulationManager& net,
    network_behaviour::NetworkConfigSchedule schedule)
    : net_(net), schedule_(std::move(schedule)) {}

webrtc::EmulatedNetworkNode* SchedulableNetworkNodeBuilder::Build() {
  return net_.CreateEmulatedNode(std::make_unique<SchedulableNetworkBehavior>(
      std::move(schedule_), *net_.time_controller()->GetClock()));
}
}  // namespace webrtc
