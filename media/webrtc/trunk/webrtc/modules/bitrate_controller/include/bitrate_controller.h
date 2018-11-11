/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 *  Usage: this class will register multiple RtcpBitrateObserver's one at each
 *  RTCP module. It will aggregate the results and run one bandwidth estimation
 *  and push the result to the encoders via BitrateObserver(s).
 */

#ifndef WEBRTC_MODULES_BITRATE_CONTROLLER_INCLUDE_BITRATE_CONTROLLER_H_
#define WEBRTC_MODULES_BITRATE_CONTROLLER_INCLUDE_BITRATE_CONTROLLER_H_

#include <map>

#include "webrtc/modules/interface/module.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"

namespace webrtc {

class CriticalSectionWrapper;

class BitrateObserver {
  // Observer class for bitrate changes announced due to change in bandwidth
  // estimate or due to bitrate allocation changes. Fraction loss and rtt is
  // also part of this callback to allow the obsevrer to optimize its settings
  // for different types of network environments. The bitrate does not include
  // packet headers and is measured in bits per second.
 public:
  virtual void OnNetworkChanged(uint32_t bitrate_bps,
                                uint8_t fraction_loss,  // 0 - 255.
                                int64_t rtt_ms) = 0;

  virtual ~BitrateObserver() {}
};

class BitrateController : public Module {
  // This class collects feedback from all streams sent to a peer (via
  // RTCPBandwidthObservers). It does one  aggregated send side bandwidth
  // estimation and divide the available bitrate between all its registered
  // BitrateObservers.
 public:
  static const int kDefaultStartBitrateKbps = 300;

  static BitrateController* CreateBitrateController(Clock* clock,
                                                    BitrateObserver* observer);
  virtual ~BitrateController() {}

  virtual RtcpBandwidthObserver* CreateRtcpBandwidthObserver() = 0;

  virtual void SetStartBitrate(int start_bitrate_bps) = 0;
  virtual void SetMinMaxBitrate(int min_bitrate_bps, int max_bitrate_bps) = 0;

  // Gets the available payload bandwidth in bits per second. Note that
  // this bandwidth excludes packet headers.
  virtual bool AvailableBandwidth(uint32_t* bandwidth) const = 0;

  virtual void SetReservedBitrate(uint32_t reserved_bitrate_bps) = 0;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_BITRATE_CONTROLLER_INCLUDE_BITRATE_CONTROLLER_H_
