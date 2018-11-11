/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_PC_AUDIO_JNI_H_
#define SDK_ANDROID_SRC_JNI_PC_AUDIO_JNI_H_

// Adding 'nogncheck' to disable the gn include headers check.
// We don't want this target depend on audio related targets
#include "api/audio_codecs/audio_decoder_factory.h"  // nogncheck
#include "api/audio_codecs/audio_encoder_factory.h"  // nogncheck
#include "modules/audio_processing/include/audio_processing.h"  // nogncheck
#include "rtc_base/scoped_ref_ptr.h"

namespace webrtc {
namespace jni {

rtc::scoped_refptr<AudioDecoderFactory> CreateAudioDecoderFactory();

rtc::scoped_refptr<AudioEncoderFactory> CreateAudioEncoderFactory();

rtc::scoped_refptr<AudioProcessing> CreateAudioProcessing();

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_PC_AUDIO_JNI_H_
