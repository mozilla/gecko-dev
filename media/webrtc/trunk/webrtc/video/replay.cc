/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include <map>
#include <sstream>

#include "gflags/gflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/call.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/sleep.h"
#include "webrtc/test/encoder_settings.h"
#include "webrtc/test/null_transport.h"
#include "webrtc/test/rtp_file_reader.h"
#include "webrtc/test/run_loop.h"
#include "webrtc/test/run_test.h"
#include "webrtc/test/video_capturer.h"
#include "webrtc/test/video_renderer.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_decoder.h"

namespace webrtc {
namespace flags {

// TODO(pbos): Multiple receivers.

// Flag for payload type.
static bool ValidatePayloadType(const char* flagname, int32_t payload_type) {
  return payload_type > 0 && payload_type <= 127;
}
DEFINE_int32(payload_type, 0, "Payload type");
static int PayloadType() { return static_cast<int>(FLAGS_payload_type); }
static const bool payload_dummy =
    google::RegisterFlagValidator(&FLAGS_payload_type, &ValidatePayloadType);

// Flag for SSRC.
static bool ValidateSsrc(const char* flagname, uint64_t ssrc) {
  return ssrc > 0 && ssrc <= 0xFFFFFFFFu;
}

DEFINE_uint64(ssrc, 0, "Incoming SSRC");
static uint32_t Ssrc() { return static_cast<uint32_t>(FLAGS_ssrc); }
static const bool ssrc_dummy =
    google::RegisterFlagValidator(&FLAGS_ssrc, &ValidateSsrc);

static bool ValidateOptionalPayloadType(const char* flagname,
                                        int32_t payload_type) {
  return payload_type == -1 || ValidatePayloadType(flagname, payload_type);
}

// Flag for RED payload type.
DEFINE_int32(red_payload_type, -1, "RED payload type");
static int RedPayloadType() {
  return static_cast<int>(FLAGS_red_payload_type);
}
static const bool red_dummy =
    google::RegisterFlagValidator(&FLAGS_red_payload_type,
                                  &ValidateOptionalPayloadType);

// Flag for ULPFEC payload type.
DEFINE_int32(fec_payload_type, -1, "ULPFEC payload type");
static int FecPayloadType() {
  return static_cast<int>(FLAGS_fec_payload_type);
}
static const bool fec_dummy =
    google::RegisterFlagValidator(&FLAGS_fec_payload_type,
                                  &ValidateOptionalPayloadType);

// Flag for abs-send-time id.
static bool ValidateRtpHeaderExtensionId(const char* flagname,
                                         int32_t extension_id) {
  return extension_id >= -1 || extension_id < 15;
}
DEFINE_int32(abs_send_time_id, -1, "RTP extension ID for abs-send-time");
static int AbsSendTimeId() { return static_cast<int>(FLAGS_abs_send_time_id); }
static const bool abs_send_time_dummy =
    google::RegisterFlagValidator(&FLAGS_abs_send_time_id,
                                  &ValidateRtpHeaderExtensionId);

// Flag for transmission-offset id.
DEFINE_int32(transmission_offset_id,
             -1,
             "RTP extension ID for transmission-offset");
static int TransmissionOffsetId() {
  return static_cast<int>(FLAGS_transmission_offset_id);
}
static const bool timestamp_offset_dummy =
    google::RegisterFlagValidator(&FLAGS_transmission_offset_id,
                                  &ValidateRtpHeaderExtensionId);

// Flag for rtpdump input file.
bool ValidateInputFilenameNotEmpty(const char* flagname,
                                   const std::string& string) {
  return string != "";
}
DEFINE_string(input_file, "", "input file");
static std::string InputFile() {
  return static_cast<std::string>(FLAGS_input_file);
}
static const bool input_file_dummy =
    google::RegisterFlagValidator(&FLAGS_input_file,
                                  &ValidateInputFilenameNotEmpty);

// Flag for raw output files.
DEFINE_string(out_base, "", "Basename (excluding .yuv) for raw output");
static std::string OutBase() {
  return static_cast<std::string>(FLAGS_out_base);
}

// Flag for video codec.
DEFINE_string(codec, "VP8", "Video codec");
static std::string Codec() { return static_cast<std::string>(FLAGS_codec); }

}  // namespace flags

static const uint32_t kReceiverLocalSsrc = 0x123456;

class FileRenderPassthrough : public VideoRenderer {
 public:
  FileRenderPassthrough(const std::string& basename, VideoRenderer* renderer)
      : basename_(basename),
        renderer_(renderer),
        file_(NULL),
        count_(0),
        last_width_(0),
        last_height_(0) {}

  ~FileRenderPassthrough() {
    if (file_ != NULL)
      fclose(file_);
  }

 private:
  virtual void RenderFrame(const I420VideoFrame& video_frame,
                           int time_to_render_ms) OVERRIDE {
    if (renderer_ != NULL)
      renderer_->RenderFrame(video_frame, time_to_render_ms);
    if (basename_ == "")
      return;
    if (last_width_ != video_frame.width() ||
        last_height_ != video_frame.height()) {
      if (file_ != NULL)
        fclose(file_);
      std::stringstream filename;
      filename << basename_;
      if (++count_ > 1)
        filename << '-' << count_;
      filename << '_' << video_frame.width() << 'x' << video_frame.height()
               << ".yuv";
      file_ = fopen(filename.str().c_str(), "wb");
      if (file_ == NULL) {
        fprintf(stderr,
                "Couldn't open file for writing: %s\n",
                filename.str().c_str());
      }
    }
    last_width_ = video_frame.width();
    last_height_ = video_frame.height();
    if (file_ == NULL)
      return;
    PrintI420VideoFrame(video_frame, file_);
  }

  const std::string basename_;
  VideoRenderer* const renderer_;
  FILE* file_;
  size_t count_;
  int last_width_;
  int last_height_;
};

void RtpReplay() {
  scoped_ptr<test::VideoRenderer> playback_video(test::VideoRenderer::Create(
      "Playback Video", 640, 480));
  FileRenderPassthrough file_passthrough(flags::OutBase(),
                                         playback_video.get());

  // TODO(pbos): Might be good to have a transport that prints keyframe requests
  //             etc.
  test::NullTransport transport;
  Call::Config call_config(&transport);
  scoped_ptr<Call> call(Call::Create(call_config));

  VideoReceiveStream::Config receive_config;
  receive_config.rtp.remote_ssrc = flags::Ssrc();
  receive_config.rtp.local_ssrc = kReceiverLocalSsrc;
  receive_config.rtp.fec.ulpfec_payload_type = flags::FecPayloadType();
  receive_config.rtp.fec.red_payload_type = flags::RedPayloadType();
  receive_config.rtp.nack.rtp_history_ms = 1000;
  if (flags::TransmissionOffsetId() != -1) {
    receive_config.rtp.extensions.push_back(
        RtpExtension(RtpExtension::kTOffset, flags::TransmissionOffsetId()));
  }
  if (flags::AbsSendTimeId() != -1) {
    receive_config.rtp.extensions.push_back(
        RtpExtension(RtpExtension::kAbsSendTime, flags::AbsSendTimeId()));
  }
  receive_config.renderer = &file_passthrough;

  VideoSendStream::Config::EncoderSettings encoder_settings;
  encoder_settings.payload_name = flags::Codec();
  encoder_settings.payload_type = flags::PayloadType();
  VideoReceiveStream::Decoder decoder =
      test::CreateMatchingDecoder(encoder_settings);
  receive_config.decoders.push_back(decoder);

  VideoReceiveStream* receive_stream =
      call->CreateVideoReceiveStream(receive_config);

  scoped_ptr<test::RtpFileReader> rtp_reader(test::RtpFileReader::Create(
      test::RtpFileReader::kRtpDump, flags::InputFile()));
  if (rtp_reader.get() == NULL) {
    rtp_reader.reset(test::RtpFileReader::Create(test::RtpFileReader::kPcap,
                                                 flags::InputFile()));
    if (rtp_reader.get() == NULL) {
      fprintf(stderr,
              "Couldn't open input file as either a rtpdump or .pcap. Note "
              "that .pcapng is not supported.\n");
      return;
    }
  }
  receive_stream->Start();

  uint32_t last_time_ms = 0;
  int num_packets = 0;
  std::map<uint32_t, int> unknown_packets;
  while (true) {
    test::RtpFileReader::Packet packet;
    if (!rtp_reader->NextPacket(&packet))
      break;
    ++num_packets;
    switch (call->Receiver()->DeliverPacket(packet.data, packet.length)) {
      case PacketReceiver::DELIVERY_OK:
        break;
      case PacketReceiver::DELIVERY_UNKNOWN_SSRC: {
        RTPHeader header;
        scoped_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
        parser->Parse(packet.data, packet.length, &header);
        if (unknown_packets[header.ssrc] == 0)
          fprintf(stderr, "Unknown SSRC: %u!\n", header.ssrc);
        ++unknown_packets[header.ssrc];
        break;
      }
      case PacketReceiver::DELIVERY_PACKET_ERROR:
        fprintf(stderr, "Packet error, corrupt packets or incorrect setup?\n");
        break;
    }
    if (last_time_ms != 0 && last_time_ms != packet.time_ms) {
      SleepMs(packet.time_ms - last_time_ms);
    }
    last_time_ms = packet.time_ms;
  }
  fprintf(stderr, "num_packets: %d\n", num_packets);

  for (std::map<uint32_t, int>::const_iterator it = unknown_packets.begin();
       it != unknown_packets.end();
       ++it) {
    fprintf(
        stderr, "Packets for unknown ssrc '%u': %d\n", it->first, it->second);
  }

  call->DestroyVideoReceiveStream(receive_stream);

  delete decoder.decoder;
}
}  // namespace webrtc

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  google::ParseCommandLineFlags(&argc, &argv, true);

  webrtc::test::RunTest(webrtc::RtpReplay);
  return 0;
}
