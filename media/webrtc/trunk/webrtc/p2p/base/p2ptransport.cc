/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/base/p2ptransport.h"

#include <string>

#include "webrtc/base/base64.h"
#include "webrtc/base/common.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/p2p/base/p2ptransportchannel.h"

namespace cricket {

P2PTransport::P2PTransport(rtc::Thread* signaling_thread,
                           rtc::Thread* worker_thread,
                           const std::string& content_name,
                           PortAllocator* allocator)
    : Transport(signaling_thread, worker_thread,
                content_name, NS_GINGLE_P2P, allocator) {
}

P2PTransport::~P2PTransport() {
  DestroyAllChannels();
}

TransportChannelImpl* P2PTransport::CreateTransportChannel(int component) {
  return new P2PTransportChannel(content_name(), component, this,
                                 port_allocator());
}

void P2PTransport::DestroyTransportChannel(TransportChannelImpl* channel) {
  delete channel;
}

}  // namespace cricket
