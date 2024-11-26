/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "media/base/codec_comparators.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "api/rtp_parameters.h"
#include "api/video_codecs/av1_profile.h"
#include "api/video_codecs/h264_profile_level_id.h"
#ifdef RTC_ENABLE_H265
#include "api/video_codecs/h265_profile_tier_level.h"
#endif
#include "api/video_codecs/vp9_profile.h"
#include "media/base/codec.h"
#include "media/base/media_constants.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/string_encode.h"

namespace webrtc {

namespace {

using cricket::Codec;

// TODO(bugs.webrtc.org/15847): remove code duplication of IsSameCodecSpecific
// in api/video_codecs/sdp_video_format.cc
std::string GetFmtpParameterOrDefault(const CodecParameterMap& params,
                                      const std::string& name,
                                      const std::string& default_value) {
  const auto it = params.find(name);
  if (it != params.end()) {
    return it->second;
  }
  return default_value;
}

std::string H264GetPacketizationModeOrDefault(const CodecParameterMap& params) {
  // If packetization-mode is not present, default to "0".
  // https://tools.ietf.org/html/rfc6184#section-6.2
  return GetFmtpParameterOrDefault(params, cricket::kH264FmtpPacketizationMode,
                                   "0");
}

bool H264IsSamePacketizationMode(const CodecParameterMap& left,
                                 const CodecParameterMap& right) {
  return H264GetPacketizationModeOrDefault(left) ==
         H264GetPacketizationModeOrDefault(right);
}

std::string AV1GetTierOrDefault(const CodecParameterMap& params) {
  // If the parameter is not present, the tier MUST be inferred to be 0.
  // https://aomediacodec.github.io/av1-rtp-spec/#72-sdp-parameters
  return GetFmtpParameterOrDefault(params, cricket::kAv1FmtpTier, "0");
}

bool AV1IsSameTier(const CodecParameterMap& left,
                   const CodecParameterMap& right) {
  return AV1GetTierOrDefault(left) == AV1GetTierOrDefault(right);
}

std::string AV1GetLevelIdxOrDefault(const CodecParameterMap& params) {
  // If the parameter is not present, it MUST be inferred to be 5 (level 3.1).
  // https://aomediacodec.github.io/av1-rtp-spec/#72-sdp-parameters
  return GetFmtpParameterOrDefault(params, cricket::kAv1FmtpLevelIdx, "5");
}

bool AV1IsSameLevelIdx(const CodecParameterMap& left,
                       const CodecParameterMap& right) {
  return AV1GetLevelIdxOrDefault(left) == AV1GetLevelIdxOrDefault(right);
}

#ifdef RTC_ENABLE_H265
std::string GetH265TxModeOrDefault(const CodecParameterMap& params) {
  // If TxMode is not present, a value of "SRST" must be inferred.
  // https://tools.ietf.org/html/rfc7798@section-7.1
  return GetFmtpParameterOrDefault(params, cricket::kH265FmtpTxMode, "SRST");
}

bool IsSameH265TxMode(const CodecParameterMap& left,
                      const CodecParameterMap& right) {
  return absl::EqualsIgnoreCase(GetH265TxModeOrDefault(left),
                                GetH265TxModeOrDefault(right));
}
#endif

// Some (video) codecs are actually families of codecs and rely on parameters
// to distinguish different incompatible family members.
bool IsSameCodecSpecific(const std::string& name1,
                         const CodecParameterMap& params1,
                         const std::string& name2,
                         const CodecParameterMap& params2) {
  // The names might not necessarily match, so check both.
  auto either_name_matches = [&](const std::string name) {
    return absl::EqualsIgnoreCase(name, name1) ||
           absl::EqualsIgnoreCase(name, name2);
  };
  if (either_name_matches(cricket::kH264CodecName))
    return H264IsSameProfile(params1, params2) &&
           H264IsSamePacketizationMode(params1, params2);
  if (either_name_matches(cricket::kVp9CodecName))
    return VP9IsSameProfile(params1, params2);
  if (either_name_matches(cricket::kAv1CodecName))
    return AV1IsSameProfile(params1, params2) &&
           AV1IsSameTier(params1, params2) &&
           AV1IsSameLevelIdx(params1, params2);
#ifdef RTC_ENABLE_H265
  if (either_name_matches(cricket::kH265CodecName)) {
    return H265IsSameProfile(params1, params2) &&
           H265IsSameTier(params1, params2) &&
           IsSameH265TxMode(params1, params2);
  }
#endif
  return true;
}

bool ReferencedCodecsMatch(const std::vector<Codec>& codecs1,
                           const int codec1_id,
                           const std::vector<Codec>& codecs2,
                           const int codec2_id) {
  const Codec* codec1 = FindCodecById(codecs1, codec1_id);
  const Codec* codec2 = FindCodecById(codecs2, codec2_id);
  return codec1 != nullptr && codec2 != nullptr && codec1->Matches(*codec2);
}

}  // namespace

bool MatchesWithCodecRules(const Codec& left_codec, const Codec& right_codec) {
  // Match the codec id/name based on the typical static/dynamic name rules.
  // Matching is case-insensitive.

  // We support the ranges [96, 127] and more recently [35, 65].
  // https://www.iana.org/assignments/rtp-parameters/rtp-parameters.xhtml#rtp-parameters-1
  // Within those ranges we match by codec name, outside by codec id.
  // We also match by name if either ID is unassigned.
  // Since no codecs are assigned an id in the range [66, 95] by us, these will
  // never match.
  const int kLowerDynamicRangeMin = 35;
  const int kLowerDynamicRangeMax = 65;
  const int kUpperDynamicRangeMin = 96;
  const int kUpperDynamicRangeMax = 127;
  const bool is_id_in_dynamic_range =
      (left_codec.id >= kLowerDynamicRangeMin &&
       left_codec.id <= kLowerDynamicRangeMax) ||
      (left_codec.id >= kUpperDynamicRangeMin &&
       left_codec.id <= kUpperDynamicRangeMax);
  const bool is_codec_id_in_dynamic_range =
      (right_codec.id >= kLowerDynamicRangeMin &&
       right_codec.id <= kLowerDynamicRangeMax) ||
      (right_codec.id >= kUpperDynamicRangeMin &&
       right_codec.id <= kUpperDynamicRangeMax);
  bool matches_id;
  if ((is_id_in_dynamic_range && is_codec_id_in_dynamic_range) ||
      left_codec.id == Codec::kIdNotSet || right_codec.id == Codec::kIdNotSet) {
    matches_id = absl::EqualsIgnoreCase(left_codec.name, right_codec.name);
  } else {
    matches_id = (left_codec.id == right_codec.id);
  }

  auto matches_type_specific = [&]() {
    switch (left_codec.type) {
      case Codec::Type::kAudio:
        // If a nonzero clockrate is specified, it must match the actual
        // clockrate. If a nonzero bitrate is specified, it must match the
        // actual bitrate, unless the codec is VBR (0), where we just force the
        // supplied value. The number of channels must match exactly, with the
        // exception that channels=0 is treated synonymously as channels=1, per
        // RFC 4566 section 6: " [The channels] parameter is OPTIONAL and may be
        // omitted if the number of channels is one."
        // Preference is ignored.
        // TODO(juberti): Treat a zero clockrate as 8000Hz, the RTP default
        // clockrate.
        return ((right_codec.clockrate == 0 /*&& clockrate == 8000*/) ||
                left_codec.clockrate == right_codec.clockrate) &&
               (right_codec.bitrate == 0 || left_codec.bitrate <= 0 ||
                left_codec.bitrate == right_codec.bitrate) &&
               ((right_codec.channels < 2 && left_codec.channels < 2) ||
                left_codec.channels == right_codec.channels);

      case Codec::Type::kVideo:
        return IsSameCodecSpecific(left_codec.name, left_codec.params,
                                   right_codec.name, right_codec.params);
    }
  };

  return matches_id && matches_type_specific();
}

// Finds a codec in `codecs2` that matches `codec_to_match`, which is
// a member of `codecs1`. If `codec_to_match` is an RED or RTX codec, both
// the codecs themselves and their associated codecs must match.
std::optional<Codec> FindMatchingCodec(const std::vector<Codec>& codecs1,
                                       const std::vector<Codec>& codecs2,
                                       const Codec& codec_to_match) {
  // `codec_to_match` should be a member of `codecs1`, in order to look up
  // RED/RTX codecs' associated codecs correctly. If not, that's a programming
  // error.
  RTC_DCHECK(absl::c_any_of(codecs1, [&codec_to_match](const Codec& codec) {
    return &codec == &codec_to_match;
  }));
  for (const Codec& potential_match : codecs2) {
    if (potential_match.Matches(codec_to_match)) {
      if (codec_to_match.GetResiliencyType() == Codec::ResiliencyType::kRtx) {
        int apt_value_1 = 0;
        int apt_value_2 = 0;
        if (!codec_to_match.GetParam(cricket::kCodecParamAssociatedPayloadType,
                                     &apt_value_1) ||
            !potential_match.GetParam(cricket::kCodecParamAssociatedPayloadType,
                                      &apt_value_2)) {
          RTC_LOG(LS_WARNING) << "RTX missing associated payload type.";
          continue;
        }
        if (!ReferencedCodecsMatch(codecs1, apt_value_1, codecs2,
                                   apt_value_2)) {
          continue;
        }
      } else if (codec_to_match.GetResiliencyType() ==
                 Codec::ResiliencyType::kRed) {
        auto red_parameters_1 = codec_to_match.params.find(
            cricket::kCodecParamNotInNameValueFormat);
        auto red_parameters_2 = potential_match.params.find(
            cricket::kCodecParamNotInNameValueFormat);
        bool has_parameters_1 = red_parameters_1 != codec_to_match.params.end();
        bool has_parameters_2 =
            red_parameters_2 != potential_match.params.end();
        // If codec_to_match has unassigned PT and no parameter,
        // we assume that it'll be assigned later and return a match.
        if (potential_match.id == Codec::kIdNotSet && !has_parameters_2) {
          return potential_match;
        }
        if (codec_to_match.id == Codec::kIdNotSet && !has_parameters_1) {
          return potential_match;
        }
        if (has_parameters_1 && has_parameters_2) {
          // Mixed reference codecs (i.e. 111/112) are not supported.
          // Different levels of redundancy between offer and answer are
          // since RED is considered to be declarative.
          std::vector<absl::string_view> redundant_payloads_1 =
              rtc::split(red_parameters_1->second, '/');
          std::vector<absl::string_view> redundant_payloads_2 =
              rtc::split(red_parameters_2->second, '/');
          if (redundant_payloads_1.size() > 0 &&
              redundant_payloads_2.size() > 0) {
            bool consistent = true;
            for (size_t i = 1; i < redundant_payloads_1.size(); i++) {
              if (redundant_payloads_1[i] != redundant_payloads_1[0]) {
                consistent = false;
                break;
              }
            }
            for (size_t i = 1; i < redundant_payloads_2.size(); i++) {
              if (redundant_payloads_2[i] != redundant_payloads_2[0]) {
                consistent = false;
                break;
              }
            }
            if (!consistent) {
              continue;
            }

            int red_value_1;
            int red_value_2;
            if (rtc::FromString(redundant_payloads_1[0], &red_value_1) &&
                rtc::FromString(redundant_payloads_2[0], &red_value_2)) {
              if (!ReferencedCodecsMatch(codecs1, red_value_1, codecs2,
                                         red_value_2)) {
                continue;
              }
            }
          }
        } else if (has_parameters_1 != has_parameters_2) {
          continue;
        }
      }
      return potential_match;
    }
  }
  return std::nullopt;
}

}  // namespace webrtc
