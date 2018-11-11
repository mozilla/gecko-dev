/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCVideoCodec.h"

#import "NSString+StdString.h"
#import "RTCVideoCodec+Private.h"
#import "WebRTC/RTCVideoCodecFactory.h"

@implementation RTCVideoEncoderSettings

@synthesize name = _name;
@synthesize width = _width;
@synthesize height = _height;
@synthesize startBitrate = _startBitrate;
@synthesize maxBitrate = _maxBitrate;
@synthesize minBitrate = _minBitrate;
@synthesize targetBitrate = _targetBitrate;
@synthesize maxFramerate = _maxFramerate;
@synthesize qpMax = _qpMax;
@synthesize mode = _mode;

- (instancetype)initWithNativeVideoCodec:(const webrtc::VideoCodec *)videoCodec {
  if (self = [super init]) {
    if (videoCodec) {
      const char *codecName = CodecTypeToPayloadString(videoCodec->codecType);
      _name = [NSString stringWithUTF8String:codecName];

      _width = videoCodec->width;
      _height = videoCodec->height;
      _startBitrate = videoCodec->startBitrate;
      _maxBitrate = videoCodec->maxBitrate;
      _minBitrate = videoCodec->minBitrate;
      _targetBitrate = videoCodec->targetBitrate;
      _maxFramerate = videoCodec->maxFramerate;
      _qpMax = videoCodec->qpMax;
      _mode = (RTCVideoCodecMode)videoCodec->mode;
    }
  }

  return self;
}

- (webrtc::VideoCodec)nativeVideoCodec {
  webrtc::VideoCodec videoCodec;
  videoCodec.width = _width;
  videoCodec.height = _height;
  videoCodec.startBitrate = _startBitrate;
  videoCodec.maxBitrate = _maxBitrate;
  videoCodec.minBitrate = _minBitrate;
  videoCodec.targetBitrate = _targetBitrate;
  videoCodec.maxBitrate = _maxBitrate;
  videoCodec.qpMax = _qpMax;
  videoCodec.mode = (webrtc::VideoCodecMode)_mode;

  return videoCodec;
}

@end
