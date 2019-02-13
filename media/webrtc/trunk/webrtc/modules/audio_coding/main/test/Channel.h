/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_TEST_CHANNEL_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_TEST_CHANNEL_H_

#include <stdio.h>

#include "webrtc/modules/audio_coding/main/interface/audio_coding_module.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class CriticalSectionWrapper;

#define MAX_NUM_PAYLOADS   50
#define MAX_NUM_FRAMESIZES  6

// TODO(turajs): Write constructor for this structure.
struct ACMTestFrameSizeStats {
  uint16_t frameSizeSample;
  int16_t maxPayloadLen;
  uint32_t numPackets;
  uint64_t totalPayloadLenByte;
  uint64_t totalEncodedSamples;
  double rateBitPerSec;
  double usageLenSec;
};

// TODO(turajs): Write constructor for this structure.
struct ACMTestPayloadStats {
  bool newPacket;
  int16_t payloadType;
  int16_t lastPayloadLenByte;
  uint32_t lastTimestamp;
  ACMTestFrameSizeStats frameSizeStats[MAX_NUM_FRAMESIZES];
};

class Channel : public AudioPacketizationCallback {
 public:

  Channel(int16_t chID = -1);
  ~Channel();

  virtual int32_t SendData(
      const FrameType frameType, const uint8_t payloadType,
      const uint32_t timeStamp, const uint8_t* payloadData,
      const uint16_t payloadSize,
      const RTPFragmentationHeader* fragmentation) OVERRIDE;

  void RegisterReceiverACM(AudioCodingModule *acm);

  void ResetStats();

  int16_t Stats(CodecInst& codecInst, ACMTestPayloadStats& payloadStats);

  void Stats(uint32_t* numPackets);

  void Stats(uint8_t* payloadLenByte, uint32_t* payloadType);

  void PrintStats(CodecInst& codecInst);

  void SetIsStereo(bool isStereo) {
    _isStereo = isStereo;
  }

  uint32_t LastInTimestamp();

  void SetFECTestWithPacketLoss(bool usePacketLoss) {
    _useFECTestWithPacketLoss = usePacketLoss;
  }

  double BitRate();

  void set_send_timestamp(uint32_t new_send_ts) {
    external_send_timestamp_ = new_send_ts;
  }

  void set_sequence_number(uint16_t new_sequence_number) {
    external_sequence_number_ = new_sequence_number;
  }

  void set_num_packets_to_drop(int new_num_packets_to_drop) {
    num_packets_to_drop_ = new_num_packets_to_drop;
  }

 private:
  void CalcStatistics(WebRtcRTPHeader& rtpInfo, uint16_t payloadSize);

  AudioCodingModule* _receiverACM;
  uint16_t _seqNo;
  // 60msec * 32 sample(max)/msec * 2 description (maybe) * 2 bytes/sample
  uint8_t _payloadData[60 * 32 * 2 * 2];

  CriticalSectionWrapper* _channelCritSect;
  FILE* _bitStreamFile;
  bool _saveBitStream;
  int16_t _lastPayloadType;
  ACMTestPayloadStats _payloadStats[MAX_NUM_PAYLOADS];
  bool _isStereo;
  WebRtcRTPHeader _rtpInfo;
  bool _leftChannel;
  uint32_t _lastInTimestamp;
  // FEC Test variables
  int16_t _packetLoss;
  bool _useFECTestWithPacketLoss;
  uint64_t _beginTime;
  uint64_t _totalBytes;

  // External timing info, defaulted to -1. Only used if they are
  // non-negative.
  int64_t external_send_timestamp_;
  int32_t external_sequence_number_;
  int num_packets_to_drop_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_TEST_CHANNEL_H_
