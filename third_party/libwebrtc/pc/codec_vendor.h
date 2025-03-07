/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_CODEC_VENDOR_H_
#define PC_CODEC_VENDOR_H_

#include <vector>

#include "api/rtc_error.h"
#include "api/rtp_transceiver_direction.h"
#include "media/base/codec.h"
#include "media/base/codec_list.h"
#include "media/base/media_engine.h"
#include "pc/media_options.h"
#include "pc/session_description.h"

namespace cricket {

// This class vends codecs of a specific type only.
// It is intended to eventually be owned by the RtpSender and RtpReceiver
// objects.
class TypedCodecVendor {
 public:
  // Constructor for the case where media engine is not provided. The resulting
  // vendor will always return an empty codec list.
  TypedCodecVendor() {}
  TypedCodecVendor(MediaEngineInterface* media_engine,
                   MediaType type,
                   bool is_sender,
                   bool rtx_enabled);
  const CodecList& codecs() const { return codecs_; }
  void set_codecs(const CodecList& codecs) { codecs_ = codecs; }
  // For easy initialization, copying is allowed.
  TypedCodecVendor(const TypedCodecVendor& from) = default;
  TypedCodecVendor& operator=(const TypedCodecVendor& from) = default;

 private:
  CodecList codecs_;
};

// This class contains the functions required to compute the list of codecs
// for SDP offer/answer. It is exposed to MediaSessionDescriptionFactory
// for the construction of offers and answers.

// TODO: bugs.webrtc.org/360058654 - complete the architectural changes
// The list of things to be done:
// - Make as much as possible private.
// - Split object usage into four objects: sender/receiver/audio/video.
// - Remove audio/video from the call names, merge code where possible.
// - Make the class instances owned by transceivers, so that codec
//   lists can differ per transceiver.
// For cleanliness:
// - Thread guard
class CodecVendor {
 public:
  CodecVendor(MediaEngineInterface* media_engine, bool rtx_enabled);

 public:
  void GetCodecsForOffer(
      const std::vector<const ContentInfo*>& current_active_contents,
      Codecs* audio_codecs,
      Codecs* video_codecs) const;
  void GetCodecsForAnswer(
      const std::vector<const ContentInfo*>& current_active_contents,
      const SessionDescription& remote_offer,
      Codecs* audio_codecs,
      Codecs* video_codecs) const;

  webrtc::RTCErrorOr<std::vector<Codec>> GetNegotiatedCodecsForOffer(
      const MediaDescriptionOptions& media_description_options,
      const MediaSessionOptions& session_options,
      const ContentInfo* current_content,
      const CodecList& codecs);

  webrtc::RTCErrorOr<Codecs> GetNegotiatedCodecsForAnswer(
      const MediaDescriptionOptions& media_description_options,
      const MediaSessionOptions& session_options,
      webrtc::RtpTransceiverDirection offer_rtd,
      webrtc::RtpTransceiverDirection answer_rtd,
      const ContentInfo* current_content,
      std::vector<Codec> codecs_from_offer,
      const CodecList& codecs);

  // Functions exposed for testing
  void set_audio_codecs(const CodecList& send_codecs,
                        const CodecList& recv_codecs);
  void set_audio_codecs(const std::vector<Codec>& send_codecs,
                        const std::vector<Codec>& recv_codecs) {
    set_audio_codecs(CodecList(send_codecs), CodecList(recv_codecs));
  }
  void set_video_codecs(const CodecList& send_codecs,
                        const CodecList& recv_codecs);
  void set_video_codecs(const std::vector<Codec>& send_codecs,
                        const std::vector<Codec>& recv_codecs) {
    set_video_codecs(CodecList(send_codecs), CodecList(recv_codecs));
  }
  CodecList audio_sendrecv_codecs() const;
  const CodecList& audio_send_codecs() const;
  const CodecList& audio_recv_codecs() const;
  CodecList video_sendrecv_codecs() const;
  const CodecList& video_send_codecs() const;
  const CodecList& video_recv_codecs() const;

 private:
  CodecList GetAudioCodecsForOffer(
      const webrtc::RtpTransceiverDirection& direction) const;
  CodecList GetAudioCodecsForAnswer(
      const webrtc::RtpTransceiverDirection& offer,
      const webrtc::RtpTransceiverDirection& answer) const;
  CodecList GetVideoCodecsForOffer(
      const webrtc::RtpTransceiverDirection& direction) const;
  CodecList GetVideoCodecsForAnswer(
      const webrtc::RtpTransceiverDirection& offer,
      const webrtc::RtpTransceiverDirection& answer) const;

  CodecList all_video_codecs() const;
  CodecList all_audio_codecs() const;

  TypedCodecVendor audio_send_codecs_;
  TypedCodecVendor audio_recv_codecs_;

  TypedCodecVendor video_send_codecs_;
  TypedCodecVendor video_recv_codecs_;
};

}  // namespace cricket

#endif  // PC_CODEC_VENDOR_H_
