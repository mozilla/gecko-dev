/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// The functions in this file are called from native code. They can still be
// accessed even though they are declared private.

package org.webrtc.voiceengine;

import android.content.Context;
import android.content.pm.PackageManager;
import android.media.AudioManager;
import android.util.Log;

import java.lang.reflect.Field;
import java.lang.reflect.Method;

import org.mozilla.gecko.annotation.WebRTCJNITarget;

@WebRTCJNITarget
class AudioManagerAndroid {
  // Most of Google lead devices use 44.1K as the default sampling rate, 44.1K
  // is also widely used on other android devices.
  private static final int DEFAULT_SAMPLING_RATE = 44100;
  // Randomly picked up frame size which is close to return value on N4.
  // Return this default value when
  // getProperty(PROPERTY_OUTPUT_FRAMES_PER_BUFFER) fails.
  private static final int DEFAULT_FRAMES_PER_BUFFER = 256;

  private int mNativeOutputSampleRate;
  private boolean mAudioLowLatencySupported;
  private int mAudioLowLatencyOutputFrameSize;


  @SuppressWarnings("unused")
  private AudioManagerAndroid(Context context) {
    AudioManager audioManager = (AudioManager)
        context.getSystemService(Context.AUDIO_SERVICE);

    mNativeOutputSampleRate = DEFAULT_SAMPLING_RATE;
    mAudioLowLatencyOutputFrameSize = DEFAULT_FRAMES_PER_BUFFER;
    mAudioLowLatencySupported = context.getPackageManager().hasSystemFeature(
      PackageManager.FEATURE_AUDIO_LOW_LATENCY);
    if (android.os.Build.VERSION.SDK_INT >=
        17 /*android.os.Build.VERSION_CODES.JELLY_BEAN_MR1*/) {
      try {
        Method getProperty = AudioManager.class.getMethod("getProperty", String.class);
        Field sampleRateField = AudioManager.class.getField("PROPERTY_OUTPUT_SAMPLE_RATE");
        Field framePerBufferField = AudioManager.class.getField("PROPERTY_OUTPUT_FRAMES_PER_BUFFER");
        String sampleRateKey = (String)sampleRateField.get(null);
        String framePerBufferKey = (String)framePerBufferField.get(null);
        String sampleRateString = (String)getProperty.invoke(audioManager, sampleRateKey);
        if (sampleRateString != null) {
          mNativeOutputSampleRate = Integer.parseInt(sampleRateString);
        }
        String framesPerBuffer = (String)getProperty.invoke(audioManager, sampleRateKey);
        if (framesPerBuffer != null) {
          mAudioLowLatencyOutputFrameSize = Integer.parseInt(framesPerBuffer);
        }
      } catch (Exception ex) {
        Log.w("WebRTC", "error getting low latency params", ex);
      }
    }
  }

    @SuppressWarnings("unused")
    private int getNativeOutputSampleRate() {
      return mNativeOutputSampleRate;
    }

    @SuppressWarnings("unused")
    private boolean isAudioLowLatencySupported() {
        return mAudioLowLatencySupported;
    }

    @SuppressWarnings("unused")
    private int getAudioLowLatencyOutputFrameSize() {
        return mAudioLowLatencyOutputFrameSize;
    }
}
