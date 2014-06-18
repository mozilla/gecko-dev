/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/neteq4/interface/audio_decoder.h"

#include <assert.h>

#include "webrtc/modules/audio_coding/neteq4/audio_decoder_impl.h"

namespace webrtc {

int AudioDecoder::DecodeRedundant(const uint8_t* encoded,
                                  size_t encoded_len,
                                  int16_t* decoded,
                                  SpeechType* speech_type) {
  return Decode(encoded, encoded_len, decoded, speech_type);
}

bool AudioDecoder::HasDecodePlc() const { return false; }

int AudioDecoder::DecodePlc(int num_frames, int16_t* decoded) { return -1; }

int AudioDecoder::IncomingPacket(const uint8_t* payload,
                                 size_t payload_len,
                                 uint16_t rtp_sequence_number,
                                 uint32_t rtp_timestamp,
                                 uint32_t arrival_timestamp) {
  return 0;
}

int AudioDecoder::ErrorCode() { return 0; }

int AudioDecoder::PacketDuration(const uint8_t* encoded, size_t encoded_len) {
  return kNotImplemented;
}

NetEqDecoder AudioDecoder::codec_type() const { return codec_type_; }

bool AudioDecoder::CodecSupported(NetEqDecoder codec_type) {
  switch (codec_type) {
    case kDecoderPCMu:
    case kDecoderPCMa:
    case kDecoderPCMu_2ch:
    case kDecoderPCMa_2ch:
#ifdef WEBRTC_CODEC_ILBC
    case kDecoderILBC:
#endif
#if defined(WEBRTC_CODEC_ISACFX) || defined(WEBRTC_CODEC_ISAC)
    case kDecoderISAC:
#endif
#ifdef WEBRTC_CODEC_ISAC
    case kDecoderISACswb:
    case kDecoderISACfb:
#endif
#ifdef WEBRTC_CODEC_PCM16
    case kDecoderPCM16B:
    case kDecoderPCM16Bwb:
    case kDecoderPCM16Bswb32kHz:
    case kDecoderPCM16Bswb48kHz:
    case kDecoderPCM16B_2ch:
    case kDecoderPCM16Bwb_2ch:
    case kDecoderPCM16Bswb32kHz_2ch:
    case kDecoderPCM16Bswb48kHz_2ch:
    case kDecoderPCM16B_5ch:
#endif
#ifdef WEBRTC_CODEC_G722
    case kDecoderG722:
    case kDecoderG722_2ch:
#endif
#ifdef WEBRTC_CODEC_CELT
    case kDecoderCELT_32:
    case kDecoderCELT_32_2ch:
#endif
#ifdef WEBRTC_CODEC_OPUS
    case kDecoderOpus:
    case kDecoderOpus_2ch:
#endif
    case kDecoderRED:
    case kDecoderAVT:
    case kDecoderCNGnb:
    case kDecoderCNGwb:
    case kDecoderCNGswb32kHz:
    case kDecoderCNGswb48kHz:
    case kDecoderArbitrary: {
      return true;
    }
    default: {
      return false;
    }
  }
}

int AudioDecoder::CodecSampleRateHz(NetEqDecoder codec_type) {
  switch (codec_type) {
    case kDecoderPCMu:
    case kDecoderPCMa:
    case kDecoderPCMu_2ch:
    case kDecoderPCMa_2ch:
#ifdef WEBRTC_CODEC_ILBC
    case kDecoderILBC:
#endif
#ifdef WEBRTC_CODEC_PCM16
    case kDecoderPCM16B:
    case kDecoderPCM16B_2ch:
    case kDecoderPCM16B_5ch:
#endif
    case kDecoderCNGnb: {
      return 8000;
    }
#if defined(WEBRTC_CODEC_ISACFX) || defined(WEBRTC_CODEC_ISAC)
    case kDecoderISAC:
#endif
#ifdef WEBRTC_CODEC_PCM16
    case kDecoderPCM16Bwb:
    case kDecoderPCM16Bwb_2ch:
#endif
#ifdef WEBRTC_CODEC_G722
    case kDecoderG722:
    case kDecoderG722_2ch:
#endif
    case kDecoderCNGwb: {
      return 16000;
    }
#ifdef WEBRTC_CODEC_ISAC
    case kDecoderISACswb:
    case kDecoderISACfb:
#endif
#ifdef WEBRTC_CODEC_PCM16
    case kDecoderPCM16Bswb32kHz:
    case kDecoderPCM16Bswb32kHz_2ch:
#endif
#ifdef WEBRTC_CODEC_CELT
    case kDecoderCELT_32:
    case kDecoderCELT_32_2ch:
#endif
    case kDecoderCNGswb32kHz: {
      return 32000;
    }
#ifdef WEBRTC_CODEC_PCM16
    case kDecoderPCM16Bswb48kHz:
    case kDecoderPCM16Bswb48kHz_2ch: {
      return 48000;
    }
#endif
#ifdef WEBRTC_CODEC_OPUS
    case kDecoderOpus:
    case kDecoderOpus_2ch: {
      return 32000;
    }
#endif
    case kDecoderCNGswb48kHz: {
      // TODO(tlegrand): Remove limitation once ACM has full 48 kHz support.
      return 32000;
    }
    default: {
      return -1;  // Undefined sample rate.
    }
  }
}

AudioDecoder* AudioDecoder::CreateAudioDecoder(NetEqDecoder codec_type) {
  if (!CodecSupported(codec_type)) {
    return NULL;
  }
  switch (codec_type) {
    case kDecoderPCMu:
      return new AudioDecoderPcmU;
    case kDecoderPCMa:
      return new AudioDecoderPcmA;
    case kDecoderPCMu_2ch:
      return new AudioDecoderPcmUMultiCh(2);
    case kDecoderPCMa_2ch:
      return new AudioDecoderPcmAMultiCh(2);
#ifdef WEBRTC_CODEC_ILBC
    case kDecoderILBC:
      return new AudioDecoderIlbc;
#endif
#if defined(WEBRTC_CODEC_ISACFX)
    case kDecoderISAC:
      return new AudioDecoderIsacFix;
#elif defined(WEBRTC_CODEC_ISAC)
    case kDecoderISAC:
      return new AudioDecoderIsac;
#endif
#ifdef WEBRTC_CODEC_ISAC
    case kDecoderISACswb:
      return new AudioDecoderIsacSwb;
    case kDecoderISACfb:
      return new AudioDecoderIsacFb;
#endif
#ifdef WEBRTC_CODEC_PCM16
    case kDecoderPCM16B:
    case kDecoderPCM16Bwb:
    case kDecoderPCM16Bswb32kHz:
    case kDecoderPCM16Bswb48kHz:
      return new AudioDecoderPcm16B(codec_type);
    case kDecoderPCM16B_2ch:
    case kDecoderPCM16Bwb_2ch:
    case kDecoderPCM16Bswb32kHz_2ch:
    case kDecoderPCM16Bswb48kHz_2ch:
    case kDecoderPCM16B_5ch:
      return new AudioDecoderPcm16BMultiCh(codec_type);
#endif
#ifdef WEBRTC_CODEC_G722
    case kDecoderG722:
      return new AudioDecoderG722;
    case kDecoderG722_2ch:
      return new AudioDecoderG722Stereo;
#endif
#ifdef WEBRTC_CODEC_CELT
    case kDecoderCELT_32:
    case kDecoderCELT_32_2ch:
      return new AudioDecoderCelt(codec_type);
#endif
#ifdef WEBRTC_CODEC_OPUS
    case kDecoderOpus:
    case kDecoderOpus_2ch:
      return new AudioDecoderOpus(codec_type);
#endif
    case kDecoderCNGnb:
    case kDecoderCNGwb:
    case kDecoderCNGswb32kHz:
    case kDecoderCNGswb48kHz:
      return new AudioDecoderCng(codec_type);
    case kDecoderRED:
    case kDecoderAVT:
    case kDecoderArbitrary:
    default: {
      return NULL;
    }
  }
}

AudioDecoder::SpeechType AudioDecoder::ConvertSpeechType(int16_t type) {
  switch (type) {
    case 0:  // TODO(hlundin): Both iSAC and Opus return 0 for speech.
    case 1:
      return kSpeech;
    case 2:
      return kComfortNoise;
    default:
      assert(false);
      return kSpeech;
  }
}

}  // namespace webrtc
