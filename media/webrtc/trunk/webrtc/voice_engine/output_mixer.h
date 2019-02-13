/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_OUTPUT_MIXER_H_
#define WEBRTC_VOICE_ENGINE_OUTPUT_MIXER_H_

#include "webrtc/common_audio/resampler/include/push_resampler.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/audio_conference_mixer/interface/audio_conference_mixer.h"
#include "webrtc/modules/audio_conference_mixer/interface/audio_conference_mixer_defines.h"
#include "webrtc/modules/utility/interface/file_recorder.h"
#include "webrtc/voice_engine/dtmf_inband.h"
#include "webrtc/voice_engine/level_indicator.h"
#include "webrtc/voice_engine/voice_engine_defines.h"

namespace webrtc {

class AudioProcessing;
class CriticalSectionWrapper;
class FileWrapper;
class VoEMediaProcess;

namespace voe {

class Statistics;

class OutputMixer : public AudioMixerOutputReceiver,
                    public AudioMixerStatusReceiver,
                    public FileCallback
{
public:
    static int32_t Create(OutputMixer*& mixer, uint32_t instanceId);

    static void Destroy(OutputMixer*& mixer);

    int32_t SetEngineInformation(Statistics& engineStatistics);

    int32_t SetAudioProcessingModule(
        AudioProcessing* audioProcessingModule);

    // VoEExternalMedia
    int RegisterExternalMediaProcessing(
        VoEMediaProcess& proccess_object);

    int DeRegisterExternalMediaProcessing();

    // VoEDtmf
    int PlayDtmfTone(uint8_t eventCode, int lengthMs, int attenuationDb);

    int32_t MixActiveChannels();

    int32_t DoOperationsOnCombinedSignal(bool feed_data_to_apm);

    int32_t SetMixabilityStatus(MixerParticipant& participant,
                                bool mixable);

    int32_t SetAnonymousMixabilityStatus(MixerParticipant& participant,
                                         bool mixable);

    int GetMixedAudio(int sample_rate_hz, int num_channels,
                      AudioFrame* audioFrame);

    // VoEVolumeControl
    int GetSpeechOutputLevel(uint32_t& level);

    int GetSpeechOutputLevelFullRange(uint32_t& level);

    int SetOutputVolumePan(float left, float right);

    int GetOutputVolumePan(float& left, float& right);

    // VoEFile
    int StartRecordingPlayout(const char* fileName,
                              const CodecInst* codecInst);

    int StartRecordingPlayout(OutStream* stream,
                              const CodecInst* codecInst);
    int StopRecordingPlayout();

    virtual ~OutputMixer();

    // from AudioMixerOutputReceiver
    virtual void NewMixedAudio(
        int32_t id,
        const AudioFrame& generalAudioFrame,
        const AudioFrame** uniqueAudioFrames,
        uint32_t size);

    // from AudioMixerStatusReceiver
    virtual void MixedParticipants(
        int32_t id,
        const ParticipantStatistics* participantStatistics,
        uint32_t size);

    virtual void VADPositiveParticipants(
        int32_t id,
        const ParticipantStatistics* participantStatistics,
        uint32_t size);

    virtual void MixedAudioLevel(int32_t id, uint32_t level);

    // For file recording
    void PlayNotification(int32_t id, uint32_t durationMs);

    void RecordNotification(int32_t id, uint32_t durationMs);

    void PlayFileEnded(int32_t id);
    void RecordFileEnded(int32_t id);

    // so ExternalPlayoutData() can insert far-end audio from the audio drivers
    void APMAnalyzeReverseStream(AudioFrame &audioFrame);

private:
    OutputMixer(uint32_t instanceId);
    int InsertInbandDtmfTone();

    // uses
    Statistics* _engineStatisticsPtr;
    AudioProcessing* _audioProcessingModulePtr;

    // owns
    CriticalSectionWrapper& _callbackCritSect;
    // protect the _outputFileRecorderPtr and _outputFileRecording
    CriticalSectionWrapper& _fileCritSect;
    AudioConferenceMixer& _mixerModule;
    AudioFrame _audioFrame;
    // Converts mixed audio to the audio device output rate.
    PushResampler<int16_t> resampler_;
    // Converts mixed audio to the audio processing rate.
    PushResampler<int16_t> audioproc_resampler_;
    AudioLevel _audioLevel;    // measures audio level for the combined signal
    DtmfInband _dtmfGenerator;
    int _instanceId;
    VoEMediaProcess* _externalMediaCallbackPtr;
    bool _externalMedia;
    float _panLeft;
    float _panRight;
    int _mixingFrequencyHz;
    FileRecorder* _outputFileRecorderPtr;
    bool _outputFileRecording;
};

}  // namespace voe

}  // namespace werbtc

#endif  // VOICE_ENGINE_OUTPUT_MIXER_H_
