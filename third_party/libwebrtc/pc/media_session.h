/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Types and classes used in media session descriptions.

#ifndef PC_MEDIA_SESSION_H_
#define PC_MEDIA_SESSION_H_

#include <memory>
#include <string>
#include <vector>

#include "api/media_types.h"
#include "api/rtc_error.h"
#include "media/base/codec_list.h"
#include "media/base/stream_params.h"
#include "p2p/base/ice_credentials_iterator.h"
#include "p2p/base/transport_description.h"
#include "p2p/base/transport_description_factory.h"
#include "p2p/base/transport_info.h"
#include "pc/codec_vendor.h"
#include "pc/media_options.h"
#include "pc/session_description.h"
#include "rtc_base/memory/always_valid_pointer.h"
#include "rtc_base/unique_id_generator.h"

namespace webrtc {

// Forward declaration due to circular dependecy.
class ConnectionContext;

}  // namespace webrtc
namespace cricket {
class MediaEngineInterface;
}  // namespace cricket

namespace webrtc {

// Creates media session descriptions according to the supplied codecs and
// other fields, as well as the supplied per-call options.
// When creating answers, performs the appropriate negotiation
// of the various fields to determine the proper result.
class MediaSessionDescriptionFactory {
 public:
  // This constructor automatically sets up the factory to get its configuration
  // from the specified MediaEngine (when provided).
  // The TransportDescriptionFactory, the UniqueRandomIdGenerator, and the
  // PayloadTypeSuggester are not owned by MediaSessionDescriptionFactory, so
  // they must be kept alive by the user of this class.
  MediaSessionDescriptionFactory(
      cricket::MediaEngineInterface* media_engine,
      bool rtx_enabled,
      UniqueRandomIdGenerator* ssrc_generator,
      const cricket::TransportDescriptionFactory* factory,
      cricket::CodecLookupHelper* codec_lookup_helper);

  cricket::RtpHeaderExtensions filtered_rtp_header_extensions(
      cricket::RtpHeaderExtensions extensions) const;

  void set_enable_encrypted_rtp_header_extensions(bool enable) {
    enable_encrypted_rtp_header_extensions_ = enable;
  }

  void set_is_unified_plan(bool is_unified_plan) {
    is_unified_plan_ = is_unified_plan;
  }

  RTCErrorOr<std::unique_ptr<SessionDescription>> CreateOfferOrError(
      const cricket::MediaSessionOptions& options,
      const SessionDescription* current_description) const;
  RTCErrorOr<std::unique_ptr<SessionDescription>> CreateAnswerOrError(
      const SessionDescription* offer,
      const cricket::MediaSessionOptions& options,
      const SessionDescription* current_description) const;

 private:
  struct AudioVideoRtpHeaderExtensions {
    cricket::RtpHeaderExtensions audio;
    cricket::RtpHeaderExtensions video;
  };

  AudioVideoRtpHeaderExtensions GetOfferedRtpHeaderExtensionsWithIds(
      const std::vector<const ContentInfo*>& current_active_contents,
      bool extmap_allow_mixed,
      const std::vector<cricket::MediaDescriptionOptions>&
          media_description_options) const;
  RTCError AddTransportOffer(
      const std::string& content_name,
      const cricket::TransportOptions& transport_options,
      const SessionDescription* current_desc,
      SessionDescription* offer,
      cricket::IceCredentialsIterator* ice_credentials) const;

  std::unique_ptr<cricket::TransportDescription> CreateTransportAnswer(
      const std::string& content_name,
      const SessionDescription* offer_desc,
      const cricket::TransportOptions& transport_options,
      const SessionDescription* current_desc,
      bool require_transport_attributes,
      cricket::IceCredentialsIterator* ice_credentials) const;

  RTCError AddTransportAnswer(
      const std::string& content_name,
      const cricket::TransportDescription& transport_desc,
      SessionDescription* answer_desc) const;

  // Helpers for adding media contents to the SessionDescription.
  RTCError AddRtpContentForOffer(
      const cricket::MediaDescriptionOptions& media_description_options,
      const cricket::MediaSessionOptions& session_options,
      const ContentInfo* current_content,
      const SessionDescription* current_description,
      const cricket::RtpHeaderExtensions& header_extensions,
      const cricket::CodecList& codecs,
      cricket::StreamParamsVec* current_streams,
      SessionDescription* desc,
      cricket::IceCredentialsIterator* ice_credentials) const;

  RTCError AddDataContentForOffer(
      const cricket::MediaDescriptionOptions& media_description_options,
      const cricket::MediaSessionOptions& session_options,
      const ContentInfo* current_content,
      const SessionDescription* current_description,
      cricket::StreamParamsVec* current_streams,
      SessionDescription* desc,
      cricket::IceCredentialsIterator* ice_credentials) const;

  RTCError AddUnsupportedContentForOffer(
      const cricket::MediaDescriptionOptions& media_description_options,
      const cricket::MediaSessionOptions& session_options,
      const ContentInfo* current_content,
      const SessionDescription* current_description,
      SessionDescription* desc,
      cricket::IceCredentialsIterator* ice_credentials) const;

  RTCError AddRtpContentForAnswer(
      const cricket::MediaDescriptionOptions& media_description_options,
      const cricket::MediaSessionOptions& session_options,
      const ContentInfo* offer_content,
      const SessionDescription* offer_description,
      const ContentInfo* current_content,
      const SessionDescription* current_description,
      const cricket::TransportInfo* bundle_transport,
      const cricket::CodecList& codecs,
      const cricket::RtpHeaderExtensions& header_extensions,
      cricket::StreamParamsVec* current_streams,
      SessionDescription* answer,
      cricket::IceCredentialsIterator* ice_credentials) const;

  RTCError AddDataContentForAnswer(
      const cricket::MediaDescriptionOptions& media_description_options,
      const cricket::MediaSessionOptions& session_options,
      const ContentInfo* offer_content,
      const SessionDescription* offer_description,
      const ContentInfo* current_content,
      const SessionDescription* current_description,
      const cricket::TransportInfo* bundle_transport,
      cricket::StreamParamsVec* current_streams,
      SessionDescription* answer,
      cricket::IceCredentialsIterator* ice_credentials) const;

  RTCError AddUnsupportedContentForAnswer(
      const cricket::MediaDescriptionOptions& media_description_options,
      const cricket::MediaSessionOptions& session_options,
      const ContentInfo* offer_content,
      const SessionDescription* offer_description,
      const ContentInfo* current_content,
      const SessionDescription* current_description,
      const cricket::TransportInfo* bundle_transport,
      SessionDescription* answer,
      cricket::IceCredentialsIterator* ice_credentials) const;

  UniqueRandomIdGenerator* ssrc_generator() const {
    return ssrc_generator_.get();
  }

  bool is_unified_plan_ = false;
  // This object may or may not be owned by this class.
  AlwaysValidPointer<UniqueRandomIdGenerator> const ssrc_generator_;
  bool enable_encrypted_rtp_header_extensions_ = true;
  const cricket::TransportDescriptionFactory* transport_desc_factory_;
  cricket::CodecLookupHelper* codec_lookup_helper_;
  bool payload_types_in_transport_trial_enabled_;
};

// Convenience functions.
bool IsMediaContent(const ContentInfo* content);
bool IsAudioContent(const ContentInfo* content);
bool IsVideoContent(const ContentInfo* content);
bool IsDataContent(const ContentInfo* content);
bool IsUnsupportedContent(const ContentInfo* content);
const ContentInfo* GetFirstMediaContent(const cricket::ContentInfos& contents,
                                        webrtc::MediaType media_type);
const ContentInfo* GetFirstAudioContent(const cricket::ContentInfos& contents);
const ContentInfo* GetFirstVideoContent(const cricket::ContentInfos& contents);
const ContentInfo* GetFirstDataContent(const cricket::ContentInfos& contents);
const ContentInfo* GetFirstMediaContent(const SessionDescription* sdesc,
                                        webrtc::MediaType media_type);
const ContentInfo* GetFirstAudioContent(const SessionDescription* sdesc);
const ContentInfo* GetFirstVideoContent(const SessionDescription* sdesc);
const ContentInfo* GetFirstDataContent(const SessionDescription* sdesc);
const AudioContentDescription* GetFirstAudioContentDescription(
    const SessionDescription* sdesc);
const VideoContentDescription* GetFirstVideoContentDescription(
    const SessionDescription* sdesc);
const SctpDataContentDescription* GetFirstSctpDataContentDescription(
    const SessionDescription* sdesc);
// Non-const versions of the above functions.
// Useful when modifying an existing description.
ContentInfo* GetFirstMediaContent(cricket::ContentInfos* contents,
                                  webrtc::MediaType media_type);
ContentInfo* GetFirstAudioContent(cricket::ContentInfos* contents);
ContentInfo* GetFirstVideoContent(cricket::ContentInfos* contents);
ContentInfo* GetFirstDataContent(cricket::ContentInfos* contents);
ContentInfo* GetFirstMediaContent(SessionDescription* sdesc,
                                  webrtc::MediaType media_type);
ContentInfo* GetFirstAudioContent(SessionDescription* sdesc);
ContentInfo* GetFirstVideoContent(SessionDescription* sdesc);
ContentInfo* GetFirstDataContent(SessionDescription* sdesc);
AudioContentDescription* GetFirstAudioContentDescription(
    SessionDescription* sdesc);
VideoContentDescription* GetFirstVideoContentDescription(
    SessionDescription* sdesc);
SctpDataContentDescription* GetFirstSctpDataContentDescription(
    SessionDescription* sdesc);

}  //  namespace webrtc

// Re-export symbols from the webrtc namespace for backwards compatibility.
// TODO(bugs.webrtc.org/4222596): Remove once all references are updated.
namespace cricket {
using ::webrtc::GetFirstAudioContent;
using ::webrtc::GetFirstAudioContentDescription;
using ::webrtc::GetFirstDataContent;
using ::webrtc::GetFirstMediaContent;
using ::webrtc::GetFirstSctpDataContentDescription;
using ::webrtc::GetFirstVideoContent;
using ::webrtc::GetFirstVideoContentDescription;
using ::webrtc::IsAudioContent;
using ::webrtc::IsDataContent;
using ::webrtc::IsMediaContent;
using ::webrtc::IsUnsupportedContent;
using ::webrtc::IsVideoContent;
using ::webrtc::MediaSessionDescriptionFactory;
}  // namespace cricket

#endif  // PC_MEDIA_SESSION_H_
