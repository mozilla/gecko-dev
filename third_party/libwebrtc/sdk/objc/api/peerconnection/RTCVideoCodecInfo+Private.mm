/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCVideoCodecInfo+Private.h"

#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/scalability_mode_helper.h"
#import "helpers/NSString+StdString.h"

@implementation RTC_OBJC_TYPE (RTCVideoCodecInfo)
(Private)

    - (instancetype)initWithNativeSdpVideoFormat : (webrtc::SdpVideoFormat)format {
  NSMutableDictionary* params = [NSMutableDictionary dictionary];
  for (const auto& [key, value] : format.parameters) {
    [params setObject:[NSString stringForStdString:value] forKey:[NSString stringForStdString:key]];
  }

  NSMutableArray<NSString*>* scalabilityModes =
      [NSMutableArray arrayWithCapacity:format.scalability_modes.size()];
  for (webrtc::ScalabilityMode mode : format.scalability_modes) {
    [scalabilityModes addObject:[NSString stringForAbslStringView:ScalabilityModeToString(mode)]];
  }

  return [self initWithName:[NSString stringForStdString:format.name]
                 parameters:params
           scalabilityModes:scalabilityModes];
}

- (webrtc::SdpVideoFormat)nativeSdpVideoFormat {
  std::map<std::string, std::string> parameters;
  for (NSString *paramKey in self.parameters.allKeys) {
    std::string key = [NSString stdStringForString:paramKey];
    std::string value = [NSString stdStringForString:self.parameters[paramKey]];
    parameters[key] = value;
  }

  absl::InlinedVector<webrtc::ScalabilityMode, webrtc::kScalabilityModeCount> scalability_modes;
  for (NSString* mode_name in self.scalabilityModes) {
    std::optional<webrtc::ScalabilityMode> mode =
        webrtc::ScalabilityModeStringToEnum([NSString stdStringForString:mode_name]);
    if (mode.has_value()) {
      scalability_modes.push_back(*mode);
    }
  }

  return webrtc::SdpVideoFormat(
      [NSString stdStringForString:self.name], parameters, scalability_modes);
}

@end
