/*
 *  Copyright 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCVideoEncoderFactory.h"
#import "RTCMacros.h"

@implementation RTC_OBJC_TYPE (RTCVideoEncoderCodecSupport)

@synthesize isSupported = _isSupported;
@synthesize isPowerEfficient = _isPowerEfficient;

- (instancetype)initWithSupported:(bool)isSupported {
  return [self initWithSupported:isSupported isPowerEfficient:false];
}

- (instancetype)initWithSupported:(bool)isSupported isPowerEfficient:(bool)isPowerEfficient {
  self = [super init];
  if (self) {
    _isSupported = isSupported;
    _isPowerEfficient = isPowerEfficient;
  }
  return self;
}

@end
