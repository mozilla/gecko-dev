/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_AUDIO_CODING_MODULE_IMPL_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_AUDIO_CODING_MODULE_IMPL_H_

#include <vector>

#include "webrtc/common_types.h"
#include "webrtc/engine_configurations.h"
#include "webrtc/modules/audio_coding/main/interface/audio_coding_module.h"
#include "webrtc/modules/audio_coding/main/source/acm_codec_database.h"
#include "webrtc/modules/audio_coding/main/source/acm_neteq.h"
#include "webrtc/modules/audio_coding/main/source/acm_resampler.h"
#include "webrtc/modules/audio_coding/main/acm2/call_statistics.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

struct WebRtcACMAudioBuff;
struct WebRtcACMCodecParams;
class CriticalSectionWrapper;
class RWLockWrapper;
class Clock;

namespace acm2 {
class Nack;
}

namespace acm1 {

class ACMDTMFDetection;
class ACMGenericCodec;

class AudioCodingModuleImpl : public AudioCodingModule {
 public:
  AudioCodingModuleImpl(const int32_t id, Clock* clock);
  ~AudioCodingModuleImpl();

  virtual const char* Version() const;

  // Change the unique identifier of this object.
  virtual int32_t ChangeUniqueId(const int32_t id);

  // Returns the number of milliseconds until the module want a worker thread
  // to call Process.
  int32_t TimeUntilNextProcess();

  // Process any pending tasks such as timeouts.
  int32_t Process();

  /////////////////////////////////////////
  //   Sender
  //

  // Initialize send codec.
  int32_t InitializeSender();

  // Reset send codec.
  int32_t ResetEncoder();

  // Can be called multiple times for Codec, CNG, RED.
  int32_t RegisterSendCodec(const CodecInst& send_codec);

  // Register Secondary codec for dual-streaming. Dual-streaming is activated
  // right after the secondary codec is registered.
  int RegisterSecondarySendCodec(const CodecInst& send_codec);

  // Unregister the secondary codec. Dual-streaming is deactivated right after
  // deregistering secondary codec.
  void UnregisterSecondarySendCodec();

  // Get the secondary codec.
  int SecondarySendCodec(CodecInst* secondary_codec) const;

  // Get current send codec.
  int32_t SendCodec(CodecInst* current_codec) const;

  // Get current send frequency.
  int32_t SendFrequency() const;

  // Get encode bit-rate.
  // Adaptive rate codecs return their current encode target rate, while other
  // codecs return there long-term average or their fixed rate.
  int32_t SendBitrate() const;

  // Set available bandwidth, inform the encoder about the
  // estimated bandwidth received from the remote party.
  virtual int32_t SetReceivedEstimatedBandwidth(const int32_t bw);

  // Register a transport callback which will be
  // called to deliver the encoded buffers.
  int32_t RegisterTransportCallback(AudioPacketizationCallback* transport);

  // Add 10 ms of raw (PCM) audio data to the encoder.
  int32_t Add10MsData(const AudioFrame& audio_frame);

  /////////////////////////////////////////
  // (FEC) Forward Error Correction
  //

  // Configure FEC status i.e on/off.
  int32_t SetFECStatus(const bool enable_fec);

  // Get FEC status.
  bool FECStatus() const;

  /////////////////////////////////////////
  //   (VAD) Voice Activity Detection
  //   and
  //   (CNG) Comfort Noise Generation
  //

  int32_t SetVAD(bool enable_dtx = true,
                 bool enable_vad = false,
                 ACMVADMode mode = VADNormal);

  int32_t VAD(bool* dtx_enabled, bool* vad_enabled, ACMVADMode* mode) const;

  int32_t RegisterVADCallback(ACMVADCallback* vad_callback);

  /////////////////////////////////////////
  //   Receiver
  //

  // Initialize receiver, resets codec database etc.
  int32_t InitializeReceiver();

  // Reset the decoder state.
  int32_t ResetDecoder();

  // Get current receive frequency.
  int32_t ReceiveFrequency() const;

  // Get current playout frequency.
  int32_t PlayoutFrequency() const;

  // Register possible receive codecs, can be called multiple times,
  // for codecs, CNG, DTMF, RED.
  int32_t RegisterReceiveCodec(const CodecInst& receive_codec);

  // Get current received codec.
  int32_t ReceiveCodec(CodecInst* current_codec) const;

  // Incoming packet from network parsed and ready for decode.
  int32_t IncomingPacket(const uint8_t* incoming_payload,
                         const int32_t payload_length,
                         const WebRtcRTPHeader& rtp_info);

  // Incoming payloads, without rtp-info, the rtp-info will be created in ACM.
  // One usage for this API is when pre-encoded files are pushed in ACM.
  int32_t IncomingPayload(const uint8_t* incoming_payload,
                          const int32_t payload_length,
                          const uint8_t payload_type,
                          const uint32_t timestamp = 0);

  // NetEq minimum playout delay (used for lip-sync). The actual target delay
  // is the max of |time_ms| and the required delay dictated by the channel.
  int SetMinimumPlayoutDelay(int time_ms);

  // NetEq maximum playout delay. The actual target delay is the min of
  // |time_ms| and the required delay dictated by the channel.
  int SetMaximumPlayoutDelay(int time_ms);

  // The shortest latency, in milliseconds, required by jitter buffer. This
  // is computed based on inter-arrival times and playout mode of NetEq. The
  // actual delay is the maximum of least-required-delay and the minimum-delay
  // specified by SetMinumumPlayoutDelay() API.
  //
  int LeastRequiredDelayMs() const ;

  // Configure Dtmf playout status i.e on/off playout the incoming outband Dtmf
  // tone.
  int32_t SetDtmfPlayoutStatus(const bool enable);

  // Get Dtmf playout status.
  bool DtmfPlayoutStatus() const;

  // Estimate the Bandwidth based on the incoming stream, needed
  // for one way audio where the RTCP send the BW estimate.
  // This is also done in the RTP module .
  int32_t DecoderEstimatedBandwidth() const;

  // Set playout mode voice, fax.
  int32_t SetPlayoutMode(const AudioPlayoutMode mode);

  // Get playout mode voice, fax.
  AudioPlayoutMode PlayoutMode() const;

  // Get playout timestamp.
  int32_t PlayoutTimestamp(uint32_t* timestamp);

  // Get 10 milliseconds of raw audio data to play out, and
  // automatic resample to the requested frequency if > 0.
  int32_t PlayoutData10Ms(int32_t desired_freq_hz,
                          AudioFrame* audio_frame);

  /////////////////////////////////////////
  //   Statistics
  //

  int32_t NetworkStatistics(ACMNetworkStatistics* statistics);

  void DestructEncoderInst(void* inst);

  int16_t AudioBuffer(WebRtcACMAudioBuff& buffer);

  // GET RED payload for iSAC. The method id called when 'this' ACM is
  // the default ACM.
  int32_t REDPayloadISAC(const int32_t isac_rate,
                         const int16_t isac_bw_estimate,
                         uint8_t* payload,
                         int16_t* length_bytes);

  int16_t SetAudioBuffer(WebRtcACMAudioBuff& buffer);

  uint32_t EarliestTimestamp() const;

  int32_t LastEncodedTimestamp(uint32_t& timestamp) const;

  int32_t ReplaceInternalDTXWithWebRtc(const bool use_webrtc_dtx);

  int32_t IsInternalDTXReplacedWithWebRtc(bool* uses_webrtc_dtx);

  int SetISACMaxRate(int max_bit_per_sec);

  int SetISACMaxPayloadSize(int max_size_bytes);

  int32_t ConfigISACBandwidthEstimator(
      int frame_size_ms,
      int rate_bit_per_sec,
      bool enforce_frame_size = false);

  int UnregisterReceiveCodec(uint8_t payload_type);

  std::vector<uint16_t> GetNackList(int round_trip_time_ms) const;

 protected:
  void UnregisterSendCodec();

  int32_t UnregisterReceiveCodecSafe(const int16_t id);

  ACMGenericCodec* CreateCodec(const CodecInst& codec);

  int16_t DecoderParamByPlType(const uint8_t payload_type,
                               WebRtcACMCodecParams& codec_params) const;

  int16_t DecoderListIDByPlName(
      const char* name, const uint16_t frequency = 0) const;

  int32_t InitializeReceiverSafe();

  bool HaveValidEncoder(const char* caller_name) const;

  int32_t RegisterRecCodecMSSafe(const CodecInst& receive_codec,
                                 int16_t codec_id,
                                 int16_t mirror_id,
                                 ACMNetEQ::JitterBuffer jitter_buffer);

  // Set VAD/DTX status. This function does not acquire a lock, and it is
  // created to be called only from inside a critical section.
  int SetVADSafe(bool enable_dtx, bool enable_vad, ACMVADMode mode);

  // Process buffered audio when dual-streaming is not enabled (When RED is
  // enabled still this function is used.)
  int ProcessSingleStream();

  // Process buffered audio when dual-streaming is enabled, i.e. secondary send
  // codec is registered.
  int ProcessDualStream();

  // Preprocessing of input audio, including resampling and down-mixing if
  // required, before pushing audio into encoder's buffer.
  //
  // in_frame: input audio-frame
  // ptr_out: pointer to output audio_frame. If no preprocessing is required
  //          |ptr_out| will be pointing to |in_frame|, otherwise pointing to
  //          |preprocess_frame_|.
  //
  // Return value:
  //   -1: if encountering an error.
  //    0: otherwise.
  int PreprocessToAddData(const AudioFrame& in_frame,
                          const AudioFrame** ptr_out);

  // Set initial playout delay.
  //  -delay_ms: delay in millisecond.
  //
  // Return value:
  //  -1: if cannot set the delay.
  //   0: if delay set successfully.
  int SetInitialPlayoutDelay(int delay_ms);

  // Enable NACK and set the maximum size of the NACK list.
  int EnableNack(size_t max_nack_list_size);

  // Disable NACK.
  void DisableNack();

  void GetDecodingCallStatistics(AudioDecodingCallStats* call_stats) const;

 private:
  // Change required states after starting to receive the codec corresponding
  // to |index|.
  int UpdateUponReceivingCodec(int index);

  // Remove all slaves and initialize a stereo slave with required codecs
  // from the master.
  int InitStereoSlave();

  // Returns true if the codec's |index| is registered with the master and
  // is a stereo codec, RED or CN.
  bool IsCodecForSlave(int index) const;

  int EncodeFragmentation(int fragmentation_index, int payload_type,
                          uint32_t current_timestamp,
                          ACMGenericCodec* encoder,
                          uint8_t* stream);

  void ResetFragmentation(int vector_size);

  bool GetSilence(int desired_sample_rate_hz, AudioFrame* frame);

  // Push a synchronization packet into NetEq. Such packets result in a frame
  // of zeros (not decoded by the corresponding decoder). The size of the frame
  // is the same as last decoding. NetEq has a special payload for this.
  // Call within the scope of ACM critical section.
  int PushSyncPacketSafe();

  // Update the parameters required in initial phase of buffering, when
  // initial playout delay is requested. Call within the scope of ACM critical
  // section.
  void UpdateBufferingSafe(const WebRtcRTPHeader& rtp_info,
                           int payload_len_bytes);

  //
  // Return the timestamp of current time, computed according to sampling rate
  // of the codec identified by |codec_id|.
  //
  uint32_t NowTimestamp(int codec_id);

  AudioPacketizationCallback* packetization_callback_;
  int32_t id_;
  uint32_t last_timestamp_;
  uint32_t last_in_timestamp_;
  CodecInst send_codec_inst_;
  uint8_t cng_nb_pltype_;
  uint8_t cng_wb_pltype_;
  uint8_t cng_swb_pltype_;
  uint8_t cng_fb_pltype_;
  uint8_t red_pltype_;
  bool vad_enabled_;
  bool dtx_enabled_;
  ACMVADMode vad_mode_;
  ACMGenericCodec* codecs_[ACMCodecDB::kMaxNumCodecs];
  ACMGenericCodec* slave_codecs_[ACMCodecDB::kMaxNumCodecs];
  int16_t mirror_codec_idx_[ACMCodecDB::kMaxNumCodecs];
  bool stereo_receive_[ACMCodecDB::kMaxNumCodecs];
  bool stereo_receive_registered_;
  bool stereo_send_;
  int prev_received_channel_;
  int expected_channels_;
  int32_t current_send_codec_idx_;
  int current_receive_codec_idx_;
  bool send_codec_registered_;
  ACMResampler input_resampler_;
  ACMResampler output_resampler_;
  ACMNetEQ neteq_;
  CriticalSectionWrapper* acm_crit_sect_;
  ACMVADCallback* vad_callback_;
  uint8_t last_recv_audio_codec_pltype_;

  // RED/FEC.
  bool is_first_red_;
  bool fec_enabled_;
  // TODO(turajs): |red_buffer_| is allocated in constructor, why having them
  // as pointers and not an array. If concerned about the memory, then make a
  // set-up function to allocate them only when they are going to be used, i.e.
  // FEC or Dual-streaming is enabled.
  uint8_t* red_buffer_;
  // TODO(turajs): we actually don't need |fragmentation_| as a member variable.
  // It is sufficient to keep the length & payload type of previous payload in
  // member variables.
  RTPFragmentationHeader fragmentation_;
  uint32_t last_fec_timestamp_;
  // If no RED is registered as receive codec this
  // will have an invalid value.
  uint8_t receive_red_pltype_;

  // This is to keep track of CN instances where we can send DTMFs.
  uint8_t previous_pltype_;

  // This keeps track of payload types associated with codecs_[].
  // We define it as signed variable and initialize with -1 to indicate
  // unused elements.
  int16_t registered_pltypes_[ACMCodecDB::kMaxNumCodecs];

  // Used when payloads are pushed into ACM without any RTP info
  // One example is when pre-encoded bit-stream is pushed from
  // a file.
  WebRtcRTPHeader* dummy_rtp_header_;
  uint16_t recv_pl_frame_size_smpls_;

  bool receiver_initialized_;
  ACMDTMFDetection* dtmf_detector_;

  AudioCodingFeedback* dtmf_callback_;
  int16_t last_detected_tone_;
  CriticalSectionWrapper* callback_crit_sect_;

  AudioFrame audio_frame_;
  AudioFrame preprocess_frame_;
  CodecInst secondary_send_codec_inst_;
  scoped_ptr<ACMGenericCodec> secondary_encoder_;

  // Initial delay.
  int initial_delay_ms_;
  int num_packets_accumulated_;
  int num_bytes_accumulated_;
  int accumulated_audio_ms_;
  int first_payload_received_;
  uint32_t last_incoming_send_timestamp_;
  bool track_neteq_buffer_;
  uint32_t playout_ts_;

  // AV-sync is enabled. In AV-sync mode, sync packet pushed during long packet
  // losses.
  bool av_sync_;

  // Latest send timestamp difference of two consecutive packets.
  uint32_t last_timestamp_diff_;
  uint16_t last_sequence_number_;
  uint32_t last_ssrc_;
  bool last_packet_was_sync_;
  int64_t last_receive_timestamp_;

  Clock* clock_;
  scoped_ptr<acm2::Nack> nack_;
  bool nack_enabled_;

  acm2::CallStatistics call_stats_;
};

}  // namespace acm1

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_AUDIO_CODING_MODULE_IMPL_H_
