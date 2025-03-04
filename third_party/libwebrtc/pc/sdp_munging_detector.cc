/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/sdp_munging_detector.h"

#include <cstddef>
#include <string>

#include "absl/algorithm/container.h"
#include "api/jsep.h"
#include "api/uma_metrics.h"
#include "media/base/codec.h"
#include "media/base/media_constants.h"
#include "media/base/stream_params.h"
#include "p2p/base/transport_info.h"
#include "pc/session_description.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {

SdpMungingType DetermineTransportModification(
    const cricket::TransportInfos& last_created_transport_infos,
    const cricket::TransportInfos& transport_infos_to_set) {
  if (last_created_transport_infos.size() != transport_infos_to_set.size()) {
    RTC_LOG(LS_WARNING) << "SDP munging: Number of transport-infos does not "
                           "match last created description.";
    // Number of transports should always match number of contents so this
    // should never happen.
    return SdpMungingType::kNumberOfContents;
  }
  for (size_t i = 0; i < last_created_transport_infos.size(); i++) {
    if (last_created_transport_infos[i].description.ice_ufrag !=
        transport_infos_to_set[i].description.ice_ufrag) {
      RTC_LOG(LS_WARNING)
          << "SDP munging: ice-ufrag does not match last created description.";
      return SdpMungingType::kIceUfrag;
    }
    if (last_created_transport_infos[i].description.ice_pwd !=
        transport_infos_to_set[i].description.ice_pwd) {
      RTC_LOG(LS_WARNING)
          << "SDP munging: ice-pwd does not match last created description.";
      return SdpMungingType::kIcePwd;
    }
    if (last_created_transport_infos[i].description.ice_mode !=
        transport_infos_to_set[i].description.ice_mode) {
      RTC_LOG(LS_WARNING)
          << "SDP munging: ice mode does not match last created description.";
      return SdpMungingType::kIceMode;
    }
    if (last_created_transport_infos[i].description.connection_role !=
        transport_infos_to_set[i].description.connection_role) {
      RTC_LOG(LS_WARNING)
          << "SDP munging: DTLS role does not match last created description.";
      return SdpMungingType::kDtlsSetup;
    }
    if (last_created_transport_infos[i].description.transport_options !=
        transport_infos_to_set[i].description.transport_options) {
      RTC_LOG(LS_WARNING) << "SDP munging: ice_options does not match last "
                             "created description.";
      return SdpMungingType::kIceOptions;
    }
  }
  return SdpMungingType::kNoModification;
}

SdpMungingType DetermineAudioSdpMungingType(
    const cricket::MediaContentDescription* last_created_media_description,
    const cricket::MediaContentDescription* media_description_to_set) {
  RTC_DCHECK(last_created_media_description);
  RTC_DCHECK(media_description_to_set);
  // Removing codecs should be done via setCodecPreferences or negotiation, not
  // munging.
  if (last_created_media_description->codecs().size() >
      media_description_to_set->codecs().size()) {
    RTC_LOG(LS_WARNING) << "SDP munging: audio codecs removed.";
    return SdpMungingType::kAudioCodecsRemoved;
  }
  // Adding audio codecs is measured after the more specific multiopus and L16
  // checks.

  // Opus stereo modification required to enabled stereo playout for opus.
  bool created_opus_stereo =
      absl::c_find_if(last_created_media_description->codecs(),
                      [](const cricket::Codec codec) {
                        std::string value;
                        return codec.name == cricket::kOpusCodecName &&
                               codec.GetParam(cricket::kCodecParamStereo,
                                              &value) &&
                               value == cricket::kParamValueTrue;
                      }) != last_created_media_description->codecs().end();
  bool set_opus_stereo =
      absl::c_find_if(
          media_description_to_set->codecs(), [](const cricket::Codec codec) {
            std::string value;
            return codec.name == cricket::kOpusCodecName &&
                   codec.GetParam(cricket::kCodecParamStereo, &value) &&
                   value == cricket::kParamValueTrue;
          }) != media_description_to_set->codecs().end();
  if (!created_opus_stereo && set_opus_stereo) {
    RTC_LOG(LS_WARNING) << "SDP munging: Opus stereo enabled.";
    return SdpMungingType::kAudioCodecsFmtpOpusStereo;
  }

  // Nonstandard 5.1/7.1 opus variant.
  bool created_multiopus =
      absl::c_find_if(last_created_media_description->codecs(),
                      [](const cricket::Codec codec) {
                        return codec.name == "multiopus";
                      }) != last_created_media_description->codecs().end();
  bool set_multiopus =
      absl::c_find_if(media_description_to_set->codecs(),
                      [](const cricket::Codec codec) {
                        return codec.name == "multiopus";
                      }) != media_description_to_set->codecs().end();
  if (!created_multiopus && set_multiopus) {
    RTC_LOG(LS_WARNING) << "SDP munging: multiopus enabled.";
    return SdpMungingType::kAudioCodecsAddedMultiOpus;
  }

  // L16.
  bool created_l16 =
      absl::c_find_if(last_created_media_description->codecs(),
                      [](const cricket::Codec codec) {
                        return codec.name == cricket::kL16CodecName;
                      }) != last_created_media_description->codecs().end();
  bool set_l16 = absl::c_find_if(media_description_to_set->codecs(),
                                 [](const cricket::Codec codec) {
                                   return codec.name == cricket::kL16CodecName;
                                 }) != media_description_to_set->codecs().end();
  if (!created_l16 && set_l16) {
    RTC_LOG(LS_WARNING) << "SDP munging: L16 enabled.";
    return SdpMungingType::kAudioCodecsAddedL16;
  }

  if (last_created_media_description->codecs().size() <
      media_description_to_set->codecs().size()) {
    RTC_LOG(LS_WARNING) << "SDP munging: audio codecs added.";
    return SdpMungingType::kAudioCodecsAdded;
  }
  return SdpMungingType::kNoModification;
}

SdpMungingType DetermineVideoSdpMungingType(
    const cricket::MediaContentDescription* last_created_media_description,
    const cricket::MediaContentDescription* media_description_to_set) {
  RTC_DCHECK(last_created_media_description);
  RTC_DCHECK(media_description_to_set);
  // Removing codecs should be done via setCodecPreferences or negotiation, not
  // munging.
  if (last_created_media_description->codecs().size() >
      media_description_to_set->codecs().size()) {
    RTC_LOG(LS_WARNING) << "SDP munging: video codecs removed.";
    return SdpMungingType::kVideoCodecsRemoved;
  }
  if (last_created_media_description->codecs().size() <
      media_description_to_set->codecs().size()) {
    RTC_LOG(LS_WARNING) << "SDP munging: video codecs added.";
    return SdpMungingType::kVideoCodecsAdded;
  }

  // Simulcast munging.
  if (last_created_media_description->streams().size() == 1 &&
      media_description_to_set->streams().size() == 1) {
    bool created_sim =
        absl::c_find_if(
            last_created_media_description->streams()[0].ssrc_groups,
            [](const cricket::SsrcGroup group) {
              return group.semantics == cricket::kSimSsrcGroupSemantics;
            }) !=
        last_created_media_description->streams()[0].ssrc_groups.end();
    bool set_sim =
        absl::c_find_if(
            media_description_to_set->streams()[0].ssrc_groups,
            [](const cricket::SsrcGroup group) {
              return group.semantics == cricket::kSimSsrcGroupSemantics;
            }) != media_description_to_set->streams()[0].ssrc_groups.end();
    if (!created_sim && set_sim) {
      RTC_LOG(LS_WARNING) << "SDP munging: legacy simulcast group created.";
      return SdpMungingType::kVideoCodecsLegacySimulcast;
    }
  }

  // sps-pps-idr-in-keyframe.
  bool created_sps_pps_idr_in_keyframe =
      absl::c_find_if(last_created_media_description->codecs(),
                      [](const cricket::Codec codec) {
                        std::string value;
                        return codec.name == cricket::kH264CodecName &&
                               codec.GetParam(
                                   cricket::kH264FmtpSpsPpsIdrInKeyframe,
                                   &value) &&
                               value == cricket::kParamValueTrue;
                      }) != last_created_media_description->codecs().end();
  bool set_sps_pps_idr_in_keyframe =
      absl::c_find_if(
          media_description_to_set->codecs(), [](const cricket::Codec codec) {
            std::string value;
            return codec.name == cricket::kH264CodecName &&
                   codec.GetParam(cricket::kH264FmtpSpsPpsIdrInKeyframe,
                                  &value) &&
                   value == cricket::kParamValueTrue;
          }) != media_description_to_set->codecs().end();
  if (!created_sps_pps_idr_in_keyframe && set_sps_pps_idr_in_keyframe) {
    RTC_LOG(LS_WARNING) << "SDP munging: sps-pps-idr-in-keyframe enabled.";
    return SdpMungingType::kVideoCodecsFmtpH264SpsPpsIdrInKeyframe;
  }

  return SdpMungingType::kNoModification;
}

}  // namespace

// Determine if the SDP was modified between createOffer and
// setLocalDescription.
SdpMungingType DetermineSdpMungingType(
    const SessionDescriptionInterface* sdesc,
    const SessionDescriptionInterface* last_created_desc) {
  if (!sdesc || !sdesc->description()) {
    RTC_LOG(LS_WARNING) << "SDP munging: Failed to parse session description.";
    return SdpMungingType::kUnknownModification;
  }

  if (!last_created_desc || !last_created_desc->description()) {
    RTC_LOG(LS_WARNING) << "SDP munging: SetLocalDescription called without "
                           "CreateOffer or CreateAnswer.";
    if (sdesc->GetType() == SdpType::kOffer) {
      return SdpMungingType::kWithoutCreateOffer;
    } else {  // answer or pranswer.
      return SdpMungingType::kWithoutCreateAnswer;
    }
  }

  // TODO: crbug.com/40567530 - we currently allow answer->pranswer
  // so can not check sdesc->GetType() == last_created_desc->GetType().

  SdpMungingType type;

  // TODO: crbug.com/40567530 - change Chromium so that pointer comparison works
  // at least for implicit local description.
  if (sdesc->description() == last_created_desc->description()) {
    return SdpMungingType::kNoModification;
  }

  // Validate contents.
  const auto& last_created_contents =
      last_created_desc->description()->contents();
  const auto& contents_to_set = sdesc->description()->contents();
  if (last_created_contents.size() != contents_to_set.size()) {
    RTC_LOG(LS_WARNING) << "SDP munging: Number of m= sections does not match "
                           "last created description.";
    return SdpMungingType::kNumberOfContents;
  }
  for (size_t i = 0; i < last_created_contents.size(); i++) {
    // TODO: crbug.com/40567530 - more checks are needed here.
    if (last_created_contents[i].mid() != contents_to_set[i].mid()) {
      RTC_LOG(LS_WARNING) << "SDP munging: mid does not match "
                             "last created description.";
      return SdpMungingType::kMid;
    }

    auto* last_created_media_description =
        last_created_contents[i].media_description();
    auto* media_description_to_set = contents_to_set[i].media_description();
    if (!(last_created_media_description && media_description_to_set)) {
      continue;
    }
    // Validate video and audio contents.
    if (last_created_media_description->as_video() != nullptr) {
      type = DetermineVideoSdpMungingType(last_created_media_description,
                                          media_description_to_set);
      if (type != SdpMungingType::kNoModification) {
        return type;
      }
    } else if (last_created_media_description->as_audio() != nullptr) {
      type = DetermineAudioSdpMungingType(last_created_media_description,
                                          media_description_to_set);
      if (type != SdpMungingType::kNoModification) {
        return type;
      }
    }
    // Validate media streams.
    if (last_created_media_description->streams().size() !=
        media_description_to_set->streams().size()) {
      RTC_LOG(LS_WARNING) << "SDP munging: streams size does not match last "
                             "created description.";
      return SdpMungingType::kSsrcs;
    }
    for (size_t i = 0; i < last_created_media_description->streams().size();
         i++) {
      if (last_created_media_description->streams()[i].ssrcs !=
          media_description_to_set->streams()[i].ssrcs) {
        RTC_LOG(LS_WARNING)
            << "SDP munging: SSRCs do not match last created description.";
        return SdpMungingType::kSsrcs;
      }
    }

    // Validate RTP header extensions.
    auto last_created_extensions =
        last_created_media_description->rtp_header_extensions();
    auto extensions_to_set = media_description_to_set->rtp_header_extensions();
    if (last_created_extensions.size() < extensions_to_set.size()) {
      RTC_LOG(LS_WARNING) << "SDP munging: RTP header extension added.";
      return SdpMungingType::kRtpHeaderExtensionAdded;
    }
    if (last_created_extensions.size() > extensions_to_set.size()) {
      RTC_LOG(LS_WARNING) << "SDP munging: RTP header extension removed.";
      return SdpMungingType::kRtpHeaderExtensionRemoved;
    }
    for (size_t i = 0; i < last_created_extensions.size(); i++) {
      if (!(last_created_extensions[i].id == extensions_to_set[i].id)) {
        RTC_LOG(LS_WARNING) << "SDP munging: header extension modified.";
        return SdpMungingType::kRtpHeaderExtensionModified;
      }
    }
  }

  // Validate transport descriptions.
  type = DetermineTransportModification(
      last_created_desc->description()->transport_infos(),
      sdesc->description()->transport_infos());
  if (type != SdpMungingType::kNoModification) {
    return type;
  }

  // TODO: crbug.com/40567530 - this serializes the descriptions back to a SDP
  // string which is very complex and we not should be be forced to rely on
  // string equality.
  std::string serialized_description;
  std::string serialized_last_description;
  if (sdesc->ToString(&serialized_description) &&
      last_created_desc->ToString(&serialized_last_description) &&
      serialized_description == serialized_last_description) {
    return SdpMungingType::kNoModification;
  }
  return SdpMungingType::kUnknownModification;
}

}  // namespace webrtc
