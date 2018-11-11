/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/base/constants.h"

#include <string>

namespace cricket {

const char CN_AUDIO[] = "audio";
const char CN_VIDEO[] = "video";
const char CN_DATA[] = "data";
const char CN_OTHER[] = "main";

const char GROUP_TYPE_BUNDLE[] = "BUNDLE";

const char NS_JINGLE_ICE_UDP[] = "urn:xmpp:jingle:transports:ice-udp:1";
const char NS_GINGLE_P2P[] = "http://www.google.com/transport/p2p";
const char NS_GINGLE_RAW[] = "http://www.google.com/transport/raw-udp";

const char ICE_OPTION_GICE[] = "google-ice";

// Minimum ufrag length is 4 characters as per RFC5245. We chose 16 because
// some internal systems expect username to be 16 bytes.
const int ICE_UFRAG_LENGTH = 16;
// Minimum password length of 22 characters as per RFC5245. We chose 24 because
// some internal systems expect password to be multiple of 4.
const int ICE_PWD_LENGTH = 24;
const size_t ICE_UFRAG_MIN_LENGTH = 4;
const size_t ICE_PWD_MIN_LENGTH = 22;
const size_t ICE_UFRAG_MAX_LENGTH = 255;
const size_t ICE_PWD_MAX_LENGTH = 256;
const size_t GICE_UFRAG_MAX_LENGTH = 16;

// TODO: This is media-specific, so might belong
// somewhere like media/base/constants.h
const int ICE_CANDIDATE_COMPONENT_RTP = 1;
const int ICE_CANDIDATE_COMPONENT_RTCP = 2;
const int ICE_CANDIDATE_COMPONENT_DEFAULT = 1;

const char NS_JINGLE_RTP[] = "urn:xmpp:jingle:apps:rtp:1";
const char NS_JINGLE_DRAFT_SCTP[] = "google:jingle:sctp";

const char GICE_CHANNEL_NAME_RTP[] = "rtp";
const char GICE_CHANNEL_NAME_RTCP[] = "rtcp";
const char GICE_CHANNEL_NAME_VIDEO_RTP[] = "video_rtp";
const char GICE_CHANNEL_NAME_VIDEO_RTCP[] = "video_rtcp";
const char GICE_CHANNEL_NAME_DATA_RTP[] = "data_rtp";
const char GICE_CHANNEL_NAME_DATA_RTCP[] = "data_rtcp";


// From RFC 4145, SDP setup attribute values.
const char CONNECTIONROLE_ACTIVE_STR[] = "active";
const char CONNECTIONROLE_PASSIVE_STR[] = "passive";
const char CONNECTIONROLE_ACTPASS_STR[] = "actpass";
const char CONNECTIONROLE_HOLDCONN_STR[] = "holdconn";

}  // namespace cricket
