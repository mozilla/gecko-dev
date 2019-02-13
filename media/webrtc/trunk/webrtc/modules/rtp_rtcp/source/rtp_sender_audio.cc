/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_sender_audio.h"

#include <assert.h> //assert
#include <string.h> //memcpy

#include "webrtc/system_wrappers/interface/trace_event.h"

namespace webrtc {
RTPSenderAudio::RTPSenderAudio(const int32_t id, Clock* clock,
                               RTPSender* rtpSender) :
    _id(id),
    _clock(clock),
    _rtpSender(rtpSender),
    _audioFeedbackCritsect(CriticalSectionWrapper::CreateCriticalSection()),
    _audioFeedback(NULL),
    _sendAudioCritsect(CriticalSectionWrapper::CreateCriticalSection()),
    _frequency(8000),
    _packetSizeSamples(160),
    _dtmfEventIsOn(false),
    _dtmfEventFirstPacketSent(false),
    _dtmfPayloadType(-1),
    _dtmfTimestamp(0),
    _dtmfKey(0),
    _dtmfLengthSamples(0),
    _dtmfLevel(0),
    _dtmfTimeLastSent(0),
    _dtmfTimestampLastSent(0),
    _REDPayloadType(-1),
    _inbandVADactive(false),
    _cngNBPayloadType(-1),
    _cngWBPayloadType(-1),
    _cngSWBPayloadType(-1),
    _cngFBPayloadType(-1),
    _lastPayloadType(-1),
    _audioLevel_dBov(0) {
};

RTPSenderAudio::~RTPSenderAudio()
{
    delete _sendAudioCritsect;
    delete _audioFeedbackCritsect;
}

int32_t
RTPSenderAudio::RegisterAudioCallback(RtpAudioFeedback* messagesCallback)
{
    CriticalSectionScoped cs(_audioFeedbackCritsect);
    _audioFeedback = messagesCallback;
    return 0;
}

void
RTPSenderAudio::SetAudioFrequency(const uint32_t f)
{
    CriticalSectionScoped cs(_sendAudioCritsect);
    _frequency = f;
}

int
RTPSenderAudio::AudioFrequency() const
{
    CriticalSectionScoped cs(_sendAudioCritsect);
    return _frequency;
}

    // set audio packet size, used to determine when it's time to send a DTMF packet in silence (CNG)
int32_t
RTPSenderAudio::SetAudioPacketSize(const uint16_t packetSizeSamples)
{
    CriticalSectionScoped cs(_sendAudioCritsect);

    _packetSizeSamples = packetSizeSamples;
    return 0;
}

int32_t RTPSenderAudio::RegisterAudioPayload(
    const char payloadName[RTP_PAYLOAD_NAME_SIZE],
    const int8_t payloadType,
    const uint32_t frequency,
    const uint8_t channels,
    const uint32_t rate,
    RtpUtility::Payload*& payload) {
  CriticalSectionScoped cs(_sendAudioCritsect);

  if (RtpUtility::StringCompare(payloadName, "cn", 2)) {
    //  we can have multiple CNG payload types
    if (frequency == 8000) {
      _cngNBPayloadType = payloadType;

    } else if (frequency == 16000) {
      _cngWBPayloadType = payloadType;

    } else if (frequency == 32000) {
      _cngSWBPayloadType = payloadType;

    } else if (frequency == 48000) {
      _cngFBPayloadType = payloadType;

    } else {
      return -1;
    }
  }
  if (RtpUtility::StringCompare(payloadName, "telephone-event", 15)) {
    // Don't add it to the list
    // we dont want to allow send with a DTMF payloadtype
    _dtmfPayloadType = payloadType;
    return 0;
    // The default timestamp rate is 8000 Hz, but other rates may be defined.
  }
  payload = new RtpUtility::Payload;
  payload->typeSpecific.Audio.frequency = frequency;
  payload->typeSpecific.Audio.channels = channels;
  payload->typeSpecific.Audio.rate = rate;
  payload->audio = true;
  payload->name[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
  strncpy(payload->name, payloadName, RTP_PAYLOAD_NAME_SIZE - 1);
  return 0;
}

bool
RTPSenderAudio::MarkerBit(const FrameType frameType,
                          const int8_t payloadType)
{
    CriticalSectionScoped cs(_sendAudioCritsect);

    // for audio true for first packet in a speech burst
    bool markerBit = false;
    if(_lastPayloadType != payloadType)
    {
        if(_cngNBPayloadType != -1)
        {
            // we have configured NB CNG
            if(_cngNBPayloadType == payloadType)
            {
                // only set a marker bit when we change payload type to a non CNG
                return false;
            }
        }
        if(_cngWBPayloadType != -1)
        {
            // we have configured WB CNG
            if(_cngWBPayloadType == payloadType)
            {
                // only set a marker bit when we change payload type to a non CNG
                return false;
            }
        }
        if(_cngSWBPayloadType != -1)
        {
            // we have configured SWB CNG
            if(_cngSWBPayloadType == payloadType)
            {
                // only set a marker bit when we change payload type to a non CNG
                return false;
            }
        }
        if(_cngFBPayloadType != -1)
        {
            // we have configured SWB CNG
            if(_cngFBPayloadType == payloadType)
            {
                // only set a marker bit when we change payload type to a non CNG
                return false;
            }
        }
        // payloadType differ
        if(_lastPayloadType == -1)
        {
            if(frameType != kAudioFrameCN)
            {
                // first packet and NOT CNG
                return true;

            }else
            {
                // first packet and CNG
                _inbandVADactive = true;
                return false;
            }
        }
        // not first packet AND
        // not CNG AND
        // payloadType changed

        // set a marker bit when we change payload type
        markerBit = true;
    }

    // For G.723 G.729, AMR etc we can have inband VAD
    if(frameType == kAudioFrameCN)
    {
        _inbandVADactive = true;

    } else if(_inbandVADactive)
    {
        _inbandVADactive = false;
        markerBit = true;
    }
    return markerBit;
}

bool
RTPSenderAudio::SendTelephoneEventActive(int8_t& telephoneEvent) const
{
    if(_dtmfEventIsOn)
    {
        telephoneEvent = _dtmfKey;
        return true;
    }
    int64_t delaySinceLastDTMF = _clock->TimeInMilliseconds() -
        _dtmfTimeLastSent;
    if(delaySinceLastDTMF < 100)
    {
        telephoneEvent = _dtmfKey;
        return true;
    }
    telephoneEvent = -1;
    return false;
}

int32_t RTPSenderAudio::SendAudio(
    const FrameType frameType,
    const int8_t payloadType,
    const uint32_t captureTimeStamp,
    const uint8_t* payloadData,
    const uint32_t dataSize,
    const RTPFragmentationHeader* fragmentation) {
  // TODO(pwestin) Breakup function in smaller functions.
  uint16_t payloadSize = static_cast<uint16_t>(dataSize);
  uint16_t maxPayloadLength = _rtpSender->MaxPayloadLength();
  bool dtmfToneStarted = false;
  uint16_t dtmfLengthMS = 0;
  uint8_t key = 0;

  // Check if we have pending DTMFs to send
  if (!_dtmfEventIsOn && PendingDTMF()) {
    CriticalSectionScoped cs(_sendAudioCritsect);

    int64_t delaySinceLastDTMF = _clock->TimeInMilliseconds() -
        _dtmfTimeLastSent;

    if (delaySinceLastDTMF > 100) {
      // New tone to play
      _dtmfTimestamp = captureTimeStamp;
      if (NextDTMF(&key, &dtmfLengthMS, &_dtmfLevel) >= 0) {
        _dtmfEventFirstPacketSent = false;
        _dtmfKey = key;
        _dtmfLengthSamples = (_frequency / 1000) * dtmfLengthMS;
        dtmfToneStarted = true;
        _dtmfEventIsOn = true;
      }
    }
  }
  if (dtmfToneStarted) {
    CriticalSectionScoped cs(_audioFeedbackCritsect);
    if (_audioFeedback) {
      _audioFeedback->OnPlayTelephoneEvent(_id, key, dtmfLengthMS, _dtmfLevel);
    }
  }

  // A source MAY send events and coded audio packets for the same time
  // but we don't support it
  {
    _sendAudioCritsect->Enter();

    if (_dtmfEventIsOn) {
      if (frameType == kFrameEmpty) {
        // kFrameEmpty is used to drive the DTMF when in CN mode
        // it can be triggered more frequently than we want to send the
        // DTMF packets.
        if (_packetSizeSamples > (captureTimeStamp - _dtmfTimestampLastSent)) {
          // not time to send yet
          _sendAudioCritsect->Leave();
          return 0;
        }
      }
      _dtmfTimestampLastSent = captureTimeStamp;
      uint32_t dtmfDurationSamples = captureTimeStamp - _dtmfTimestamp;
      bool ended = false;
      bool send = true;

      if (_dtmfLengthSamples > dtmfDurationSamples) {
        if (dtmfDurationSamples <= 0) {
          // Skip send packet at start, since we shouldn't use duration 0
          send = false;
        }
      } else {
        ended = true;
        _dtmfEventIsOn = false;
        _dtmfTimeLastSent = _clock->TimeInMilliseconds();
      }
      // don't hold the critsect while calling SendTelephoneEventPacket
      _sendAudioCritsect->Leave();
      if (send) {
        if (dtmfDurationSamples > 0xffff) {
          // RFC 4733 2.5.2.3 Long-Duration Events
          SendTelephoneEventPacket(ended, _dtmfTimestamp,
                                   static_cast<uint16_t>(0xffff), false);

          // set new timestap for this segment
          _dtmfTimestamp = captureTimeStamp;
          dtmfDurationSamples -= 0xffff;
          _dtmfLengthSamples -= 0xffff;

          return SendTelephoneEventPacket(
              ended,
              _dtmfTimestamp,
              static_cast<uint16_t>(dtmfDurationSamples),
              false);
        } else {
          if (SendTelephoneEventPacket(
                  ended,
                  _dtmfTimestamp,
                  static_cast<uint16_t>(dtmfDurationSamples),
                  !_dtmfEventFirstPacketSent) != 0) {
            return -1;
          }
          _dtmfEventFirstPacketSent = true;
          return 0;
        }
      }
      return 0;
    }
    _sendAudioCritsect->Leave();
  }
  if (payloadSize == 0 || payloadData == NULL) {
    if (frameType == kFrameEmpty) {
      // we don't send empty audio RTP packets
      // no error since we use it to drive DTMF when we use VAD
      return 0;
    }
    return -1;
  }
  uint8_t dataBuffer[IP_PACKET_SIZE];
  bool markerBit = MarkerBit(frameType, payloadType);

  int32_t rtpHeaderLength = 0;
  uint16_t timestampOffset = 0;

  if (_REDPayloadType >= 0 && fragmentation && !markerBit &&
      fragmentation->fragmentationVectorSize > 1) {
    // have we configured RED? use its payload type
    // we need to get the current timestamp to calc the diff
    uint32_t oldTimeStamp = _rtpSender->Timestamp();
    rtpHeaderLength = _rtpSender->BuildRTPheader(dataBuffer, _REDPayloadType,
                                                 markerBit, captureTimeStamp,
                                                 _clock->TimeInMilliseconds());

    timestampOffset = uint16_t(_rtpSender->Timestamp() - oldTimeStamp);
  } else {
    rtpHeaderLength = _rtpSender->BuildRTPheader(dataBuffer, payloadType,
                                                 markerBit, captureTimeStamp,
                                                 _clock->TimeInMilliseconds());
  }
  if (rtpHeaderLength <= 0) {
    return -1;
  }
  if (maxPayloadLength < (rtpHeaderLength + payloadSize)) {
    // Too large payload buffer.
    return -1;
  }
  {
    CriticalSectionScoped cs(_sendAudioCritsect);
    if (_REDPayloadType >= 0 &&  // Have we configured RED?
        fragmentation &&
        fragmentation->fragmentationVectorSize > 1 &&
        !markerBit) {
      if (timestampOffset <= 0x3fff) {
        if(fragmentation->fragmentationVectorSize != 2) {
          // we only support 2 codecs when using RED
          return -1;
        }
        // only 0x80 if we have multiple blocks
        dataBuffer[rtpHeaderLength++] = 0x80 +
            fragmentation->fragmentationPlType[1];
        uint32_t blockLength = fragmentation->fragmentationLength[1];

        // sanity blockLength
        if(blockLength > 0x3ff) {  // block length 10 bits 1023 bytes
          return -1;
        }
        uint32_t REDheader = (timestampOffset << 10) + blockLength;
        RtpUtility::AssignUWord24ToBuffer(dataBuffer + rtpHeaderLength,
                                          REDheader);
        rtpHeaderLength += 3;

        dataBuffer[rtpHeaderLength++] = fragmentation->fragmentationPlType[0];
        // copy the RED data
        memcpy(dataBuffer+rtpHeaderLength,
               payloadData + fragmentation->fragmentationOffset[1],
               fragmentation->fragmentationLength[1]);

        // copy the normal data
        memcpy(dataBuffer+rtpHeaderLength +
               fragmentation->fragmentationLength[1],
               payloadData + fragmentation->fragmentationOffset[0],
               fragmentation->fragmentationLength[0]);

        payloadSize = static_cast<uint16_t>(
            fragmentation->fragmentationLength[0] +
            fragmentation->fragmentationLength[1]);
      } else {
        // silence for too long send only new data
        dataBuffer[rtpHeaderLength++] = fragmentation->fragmentationPlType[0];
        memcpy(dataBuffer+rtpHeaderLength,
               payloadData + fragmentation->fragmentationOffset[0],
               fragmentation->fragmentationLength[0]);

        payloadSize = static_cast<uint16_t>(
            fragmentation->fragmentationLength[0]);
      }
    } else {
      if (fragmentation && fragmentation->fragmentationVectorSize > 0) {
        // use the fragment info if we have one
        dataBuffer[rtpHeaderLength++] = fragmentation->fragmentationPlType[0];
        memcpy( dataBuffer+rtpHeaderLength,
                payloadData + fragmentation->fragmentationOffset[0],
                fragmentation->fragmentationLength[0]);

        payloadSize = static_cast<uint16_t>(
            fragmentation->fragmentationLength[0]);
      } else {
        memcpy(dataBuffer+rtpHeaderLength, payloadData, payloadSize);
      }
    }
    _lastPayloadType = payloadType;

    // Update audio level extension, if included.
    {
      uint16_t packetSize = payloadSize + rtpHeaderLength;
      RtpUtility::RtpHeaderParser rtp_parser(dataBuffer, packetSize);
      RTPHeader rtp_header;
      rtp_parser.Parse(rtp_header);
      _rtpSender->UpdateAudioLevel(dataBuffer, packetSize, rtp_header,
                                   (frameType == kAudioFrameSpeech),
                                   _audioLevel_dBov);
    }
  }  // end critical section
  TRACE_EVENT_ASYNC_END2("webrtc", "Audio", captureTimeStamp,
                         "timestamp", _rtpSender->Timestamp(),
                         "seqnum", _rtpSender->SequenceNumber());
  return _rtpSender->SendToNetwork(dataBuffer,
                                   payloadSize,
                                   static_cast<uint16_t>(rtpHeaderLength),
                                   -1,
                                   kAllowRetransmission,
                                   PacedSender::kHighPriority);
}

    // Audio level magnitude and voice activity flag are set for each RTP packet
int32_t
RTPSenderAudio::SetAudioLevel(const uint8_t level_dBov)
{
    if (level_dBov > 127)
    {
        return -1;
    }
    CriticalSectionScoped cs(_sendAudioCritsect);
    _audioLevel_dBov = level_dBov;
    return 0;
}

    // Set payload type for Redundant Audio Data RFC 2198
int32_t
RTPSenderAudio::SetRED(const int8_t payloadType)
{
    if(payloadType < -1 )
    {
        return -1;
    }
    _REDPayloadType = payloadType;
    return 0;
}

    // Get payload type for Redundant Audio Data RFC 2198
int32_t
RTPSenderAudio::RED(int8_t& payloadType) const
{
    if(_REDPayloadType == -1)
    {
        // not configured
        return -1;
    }
    payloadType = _REDPayloadType;
    return 0;
}

// Send a TelephoneEvent tone using RFC 2833 (4733)
int32_t
RTPSenderAudio::SendTelephoneEvent(const uint8_t key,
                                   const uint16_t time_ms,
                                   const uint8_t level)
{
    // DTMF is protected by its own critsect
    if(_dtmfPayloadType < 0)
    {
        // TelephoneEvent payloadtype not configured
        return -1;
    }
    return AddDTMF(key, time_ms, level);
}

int32_t
RTPSenderAudio::SendTelephoneEventPacket(const bool ended,
                                         const uint32_t dtmfTimeStamp,
                                         const uint16_t duration,
                                         const bool markerBit)
{
    uint8_t dtmfbuffer[IP_PACKET_SIZE];
    uint8_t sendCount = 1;
    int32_t retVal = 0;

    if(ended)
    {
        // resend last packet in an event 3 times
        sendCount = 3;
    }
    do
    {
        _sendAudioCritsect->Enter();

        //Send DTMF data
        _rtpSender->BuildRTPheader(dtmfbuffer, _dtmfPayloadType, markerBit,
                                   dtmfTimeStamp, _clock->TimeInMilliseconds());

        // reset CSRC and X bit
        dtmfbuffer[0] &= 0xe0;

        //Create DTMF data
        /*    From RFC 2833:

         0                   1                   2                   3
         0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        |     event     |E|R| volume    |          duration             |
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        */
        // R bit always cleared
        uint8_t R = 0x00;
        uint8_t volume = _dtmfLevel;

        // First packet un-ended
          uint8_t E = 0x00;

        if(ended)
        {
            E = 0x80;
        }

        // First byte is Event number, equals key number
        dtmfbuffer[12] = _dtmfKey;
        dtmfbuffer[13] = E|R|volume;
        RtpUtility::AssignUWord16ToBuffer(dtmfbuffer + 14, duration);

        _sendAudioCritsect->Leave();
        TRACE_EVENT_INSTANT2("webrtc_rtp",
                             "Audio::SendTelephoneEvent",
                             "timestamp", dtmfTimeStamp,
                             "seqnum", _rtpSender->SequenceNumber());
        retVal = _rtpSender->SendToNetwork(dtmfbuffer, 4, 12, -1,
                                           kAllowRetransmission,
                                           PacedSender::kHighPriority);
        sendCount--;

    }while (sendCount > 0 && retVal == 0);

    return retVal;
}
}  // namespace webrtc
