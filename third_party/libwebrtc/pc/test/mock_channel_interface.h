/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_MOCK_CHANNEL_INTERFACE_H_
#define PC_TEST_MOCK_CHANNEL_INTERFACE_H_

#include <functional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/jsep.h"
#include "api/media_types.h"
#include "media/base/media_channel.h"
#include "media/base/stream_params.h"
#include "pc/channel_interface.h"
#include "pc/rtp_transport_internal.h"
#include "test/gmock.h"

namespace webrtc {

// Mock class for BaseChannel.
// Use this class in unit tests to avoid dependecy on a specific
// implementation of BaseChannel.
class MockChannelInterface : public cricket::ChannelInterface {
 public:
  MOCK_METHOD(cricket::MediaType, media_type, (), (const, override));
  MOCK_METHOD(cricket::VideoChannel*, AsVideoChannel, (), (override));
  MOCK_METHOD(cricket::VoiceChannel*, AsVoiceChannel, (), (override));
  MOCK_METHOD(cricket::MediaSendChannelInterface*,
              media_send_channel,
              (),
              (override));
  MOCK_METHOD(cricket::VoiceMediaSendChannelInterface*,
              voice_media_send_channel,
              (),
              (override));
  MOCK_METHOD(cricket::VideoMediaSendChannelInterface*,
              video_media_send_channel,
              (),
              (override));
  MOCK_METHOD(cricket::MediaReceiveChannelInterface*,
              media_receive_channel,
              (),
              (override));
  MOCK_METHOD(cricket::VoiceMediaReceiveChannelInterface*,
              voice_media_receive_channel,
              (),
              (override));
  MOCK_METHOD(cricket::VideoMediaReceiveChannelInterface*,
              video_media_receive_channel,
              (),
              (override));
  MOCK_METHOD(absl::string_view, transport_name, (), (const, override));
  MOCK_METHOD(const std::string&, mid, (), (const, override));
  MOCK_METHOD(void, Enable, (bool), (override));
  MOCK_METHOD(void,
              SetFirstPacketReceivedCallback,
              (std::function<void()>),
              (override));
  MOCK_METHOD(void,
              SetFirstPacketSentCallback,
              (std::function<void()>),
              (override));
  MOCK_METHOD(bool,
              SetLocalContent,
              (const cricket::MediaContentDescription*, SdpType, std::string&),
              (override));
  MOCK_METHOD(bool,
              SetRemoteContent,
              (const cricket::MediaContentDescription*, SdpType, std::string&),
              (override));
  MOCK_METHOD(bool, SetPayloadTypeDemuxingEnabled, (bool), (override));
  MOCK_METHOD(const std::vector<cricket::StreamParams>&,
              local_streams,
              (),
              (const, override));
  MOCK_METHOD(const std::vector<cricket::StreamParams>&,
              remote_streams,
              (),
              (const, override));
  MOCK_METHOD(bool, SetRtpTransport, (RtpTransportInternal*), (override));
};

}  //  namespace webrtc

// Re-export symbols from the webrtc namespace for backwards compatibility.
// TODO(bugs.webrtc.org/4222596): Remove once all references are updated.
namespace cricket {
using ::webrtc::MockChannelInterface;
}  // namespace cricket

#endif  // PC_TEST_MOCK_CHANNEL_INTERFACE_H_
