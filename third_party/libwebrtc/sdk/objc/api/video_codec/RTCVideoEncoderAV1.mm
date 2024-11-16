/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#import <Foundation/Foundation.h>

#import "RTCNativeVideoEncoder.h"
#import "RTCNativeVideoEncoderBuilder+Native.h"
#import "RTCVideoEncoderAV1.h"
#import "helpers/NSString+StdString.h"
#import "sdk/objc/base/RTCMacros.h"

#include "api/video_codecs/scalability_mode.h"
#include "modules/video_coding/codecs/av1/av1_svc_config.h"
#include "modules/video_coding/codecs/av1/libaom_av1_encoder.h"

@interface RTC_OBJC_TYPE (RTCVideoEncoderAV1Builder)
    : RTC_OBJC_TYPE(RTCNativeVideoEncoder) <RTC_OBJC_TYPE (RTCNativeVideoEncoderBuilder)>
@end

    @implementation RTC_OBJC_TYPE (RTCVideoEncoderAV1Builder)

    - (std::unique_ptr<webrtc::VideoEncoder>)build:(const webrtc::Environment&)env {
      return webrtc::CreateLibaomAv1Encoder(env);
    }

    @end

    @implementation RTC_OBJC_TYPE (RTCVideoEncoderAV1)

    + (id<RTC_OBJC_TYPE(RTCVideoEncoder)>)av1Encoder {
      return [[RTC_OBJC_TYPE(RTCVideoEncoderAV1Builder) alloc] init];
    }

    + (NSArray<NSString*>*)supportedScalabilityModes {
      // `LibaomAv1EncoderSupportedScalabilityModes` returns an std::vector-like container, but
      // exact type might change, thus use `auto`.
      auto modes = webrtc::LibaomAv1EncoderSupportedScalabilityModes();
      NSMutableArray<NSString*>* result = [NSMutableArray arrayWithCapacity:std::size(modes)];
      for (webrtc::ScalabilityMode mode : modes) {
        [result addObject:[NSString stringForAbslStringView:webrtc::ScalabilityModeToString(mode)]];
      }
      return result;
    }

    + (bool)isSupported {
      return true;
    }

    @end
