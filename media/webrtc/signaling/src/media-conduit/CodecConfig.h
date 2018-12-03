
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CODEC_CONFIG_H_
#define CODEC_CONFIG_H_

#include <string>
#include <vector>

#include "signaling/src/common/EncodingConstraints.h"

namespace mozilla {

/**
 * Minimalistic Audio Codec Config Params
 */
struct AudioCodecConfig {
  /*
   * The data-types for these properties mimic the
   * corresponding webrtc::CodecInst data-types.
   */
  int mType;
  std::string mName;
  int mFreq;
  int mChannels;

  bool mFECEnabled;
  bool mDtmfEnabled;

  // OPUS-specific
  int mMaxPlaybackRate;

  AudioCodecConfig(int type, std::string name, int freq, int channels,
                   bool FECEnabled)
      : mType(type),
        mName(name),
        mFreq(freq),
        mChannels(channels),
        mFECEnabled(FECEnabled),
        mDtmfEnabled(false),
        mMaxPlaybackRate(0) {}
};

/*
 * Minimalistic video codec configuration
 * More to be added later depending on the use-case
 */

#define MAX_SPROP_LEN 128

// used for holding SDP negotiation results
struct VideoCodecConfigH264 {
  char sprop_parameter_sets[MAX_SPROP_LEN];
  int packetization_mode;
  int profile_level_id;
  int tias_bw;
};

// class so the std::strings can get freed more easily/reliably
class VideoCodecConfig {
 public:
  /*
   * The data-types for these properties mimic the
   * corresponding webrtc::VideoCodec data-types.
   */
  int mType;  // payload type
  std::string mName;

  std::vector<std::string> mAckFbTypes;
  std::vector<std::string> mNackFbTypes;
  std::vector<std::string> mCcmFbTypes;
  // Don't pass mOtherFbTypes from JsepVideoCodecDescription because we'd have
  // to drag SdpRtcpFbAttributeList::Feedback along too.
  bool mRembFbSet;
  bool mFECFbSet;

  int mULPFECPayloadType;
  int mREDPayloadType;
  int mREDRTXPayloadType;

  uint32_t mTias;
  EncodingConstraints mEncodingConstraints;
  struct SimulcastEncoding {
    std::string rid;
    EncodingConstraints constraints;
    bool operator==(const SimulcastEncoding& aOther) const {
      return rid == aOther.rid && constraints == aOther.constraints;
    }
  };
  std::vector<SimulcastEncoding> mSimulcastEncodings;
  std::string mSpropParameterSets;
  uint8_t mProfile;
  uint8_t mConstraints;
  uint8_t mLevel;
  uint8_t mPacketizationMode;
  // TODO: add external negotiated SPS/PPS

  bool operator==(const VideoCodecConfig& aRhs) const {
    if (mType != aRhs.mType || mName != aRhs.mName ||
        mAckFbTypes != aRhs.mAckFbTypes || mNackFbTypes != aRhs.mNackFbTypes ||
        mCcmFbTypes != aRhs.mCcmFbTypes || mRembFbSet != aRhs.mRembFbSet ||
        mFECFbSet != aRhs.mFECFbSet ||
        mULPFECPayloadType != aRhs.mULPFECPayloadType ||
        mREDPayloadType != aRhs.mREDPayloadType ||
        mREDRTXPayloadType != aRhs.mREDRTXPayloadType || mTias != aRhs.mTias ||
        !(mEncodingConstraints == aRhs.mEncodingConstraints) ||
        !(mSimulcastEncodings == aRhs.mSimulcastEncodings) ||
        mSpropParameterSets != aRhs.mSpropParameterSets ||
        mProfile != aRhs.mProfile || mConstraints != aRhs.mConstraints ||
        mLevel != aRhs.mLevel ||
        mPacketizationMode != aRhs.mPacketizationMode) {
      return false;
    }

    return true;
  }

  VideoCodecConfig(int type, std::string name,
                   const EncodingConstraints& constraints,
                   const struct VideoCodecConfigH264* h264 = nullptr)
      : mType(type),
        mName(name),
        mRembFbSet(false),
        mFECFbSet(false),
        mULPFECPayloadType(123),
        mREDPayloadType(122),
        mREDRTXPayloadType(-1),
        mTias(0),
        mEncodingConstraints(constraints),
        mProfile(0x42),
        mConstraints(0xE0),
        mLevel(0x0C),
        mPacketizationMode(1) {
    if (h264) {
      mProfile = (h264->profile_level_id & 0x00FF0000) >> 16;
      mConstraints = (h264->profile_level_id & 0x0000FF00) >> 8;
      mLevel = (h264->profile_level_id & 0x000000FF);
      mPacketizationMode = h264->packetization_mode;
      mSpropParameterSets = h264->sprop_parameter_sets;
    }
  }

  bool ResolutionEquals(const VideoCodecConfig& aConfig) const {
    if (mSimulcastEncodings.size() != aConfig.mSimulcastEncodings.size()) {
      return false;
    }
    for (size_t i = 0; i < mSimulcastEncodings.size(); ++i) {
      if (!mSimulcastEncodings[i].constraints.ResolutionEquals(
              aConfig.mSimulcastEncodings[i].constraints)) {
        return false;
      }
    }
    return true;
  }

  // Nothing seems to use this right now. Do we intend to support this
  // someday?
  bool RtcpFbAckIsSet(const std::string& type) const {
    for (auto i = mAckFbTypes.begin(); i != mAckFbTypes.end(); ++i) {
      if (*i == type) {
        return true;
      }
    }
    return false;
  }

  bool RtcpFbNackIsSet(const std::string& type) const {
    for (auto i = mNackFbTypes.begin(); i != mNackFbTypes.end(); ++i) {
      if (*i == type) {
        return true;
      }
    }
    return false;
  }

  bool RtcpFbCcmIsSet(const std::string& type) const {
    for (auto i = mCcmFbTypes.begin(); i != mCcmFbTypes.end(); ++i) {
      if (*i == type) {
        return true;
      }
    }
    return false;
  }

  bool RtcpFbRembIsSet() const { return mRembFbSet; }

  bool RtcpFbFECIsSet() const { return mFECFbSet; }
};
}  // namespace mozilla
#endif
