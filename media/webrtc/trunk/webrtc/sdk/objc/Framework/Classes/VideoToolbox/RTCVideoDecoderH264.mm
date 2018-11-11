/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#import "WebRTC/RTCVideoCodecH264.h"

#import <VideoToolbox/VideoToolbox.h>

#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/timeutils.h"
#include "sdk/objc/Framework/Classes/VideoToolbox/nalu_rewriter.h"

#import "WebRTC/RTCVideoFrame.h"
#import "WebRTC/RTCVideoFrameBuffer.h"
#import "helpers.h"

#if defined(WEBRTC_IOS)
#import "Common/RTCUIApplicationStatusObserver.h"
#import "WebRTC/UIDevice+RTCDevice.h"
#endif

// Struct that we pass to the decoder per frame to decode. We receive it again
// in the decoder callback.
struct RTCFrameDecodeParams {
  RTCFrameDecodeParams(RTCVideoDecoderCallback cb, int64_t ts) : callback(cb), timestamp(ts) {}
  RTCVideoDecoderCallback callback;
  int64_t timestamp;
};

// This is the callback function that VideoToolbox calls when decode is
// complete.
void decompressionOutputCallback(void *decoder,
                                 void *params,
                                 OSStatus status,
                                 VTDecodeInfoFlags infoFlags,
                                 CVImageBufferRef imageBuffer,
                                 CMTime timestamp,
                                 CMTime duration) {
  std::unique_ptr<RTCFrameDecodeParams> decodeParams(
      reinterpret_cast<RTCFrameDecodeParams *>(params));
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to decode frame. Status: " << status;
    return;
  }
  // TODO(tkchin): Handle CVO properly.
  RTCCVPixelBuffer *frameBuffer = [[RTCCVPixelBuffer alloc] initWithPixelBuffer:imageBuffer];
  RTCVideoFrame *decodedFrame =
      [[RTCVideoFrame alloc] initWithBuffer:frameBuffer
                                   rotation:RTCVideoRotation_0
                                timeStampNs:CMTimeGetSeconds(timestamp) * rtc::kNumNanosecsPerSec];
  decodedFrame.timeStamp = decodeParams->timestamp;
  decodeParams->callback(decodedFrame);
}

// Decoder.
@implementation RTCVideoDecoderH264 {
  CMVideoFormatDescriptionRef _videoFormat;
  VTDecompressionSessionRef _decompressionSession;
  RTCVideoDecoderCallback _callback;
}

- (instancetype)init {
  if (self = [super init]) {
#if defined(WEBRTC_IOS)
    [RTCUIApplicationStatusObserver prepareForUse];
#endif
  }

  return self;
}

- (void)dealloc {
  [self destroyDecompressionSession];
  [self setVideoFormat:nullptr];
}

- (NSInteger)startDecodeWithSettings:(RTCVideoEncoderSettings *)settings
                       numberOfCores:(int)numberOfCores {
  return WEBRTC_VIDEO_CODEC_OK;
}

- (NSInteger)decode:(RTCEncodedImage *)inputImage
          missingFrames:(BOOL)missingFrames
    fragmentationHeader:(RTCRtpFragmentationHeader *)fragmentationHeader
      codecSpecificInfo:(__nullable id<RTCCodecSpecificInfo>)info
           renderTimeMs:(int64_t)renderTimeMs {
  RTC_DCHECK(inputImage.buffer);

#if defined(WEBRTC_IOS)
  if (![[RTCUIApplicationStatusObserver sharedInstance] isApplicationActive]) {
    // Ignore all decode requests when app isn't active. In this state, the
    // hardware decoder has been invalidated by the OS.
    // Reset video format so that we won't process frames until the next
    // keyframe.
    [self setVideoFormat:nullptr];
    return WEBRTC_VIDEO_CODEC_NO_OUTPUT;
  }
#endif
  CMVideoFormatDescriptionRef inputFormat = nullptr;
  if (webrtc::H264AnnexBBufferHasVideoFormatDescription((uint8_t *)inputImage.buffer.bytes,
                                                        inputImage.buffer.length)) {
    inputFormat = webrtc::CreateVideoFormatDescription((uint8_t *)inputImage.buffer.bytes,
                                                       inputImage.buffer.length);
    if (inputFormat) {
      // Check if the video format has changed, and reinitialize decoder if
      // needed.
      if (!CMFormatDescriptionEqual(inputFormat, _videoFormat)) {
        [self setVideoFormat:inputFormat];
        [self resetDecompressionSession];
      }
      CFRelease(inputFormat);
    }
  }
  if (!_videoFormat) {
    // We received a frame but we don't have format information so we can't
    // decode it.
    // This can happen after backgrounding. We need to wait for the next
    // sps/pps before we can resume so we request a keyframe by returning an
    // error.
    RTC_LOG(LS_WARNING) << "Missing video format. Frame with sps/pps required.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  CMSampleBufferRef sampleBuffer = nullptr;
  if (!webrtc::H264AnnexBBufferToCMSampleBuffer((uint8_t *)inputImage.buffer.bytes,
                                                inputImage.buffer.length,
                                                _videoFormat,
                                                &sampleBuffer)) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  RTC_DCHECK(sampleBuffer);
  VTDecodeFrameFlags decodeFlags = kVTDecodeFrame_EnableAsynchronousDecompression;
  std::unique_ptr<RTCFrameDecodeParams> frameDecodeParams;
  frameDecodeParams.reset(new RTCFrameDecodeParams(_callback, inputImage.timeStamp));
  OSStatus status = VTDecompressionSessionDecodeFrame(
      _decompressionSession, sampleBuffer, decodeFlags, frameDecodeParams.release(), nullptr);
#if defined(WEBRTC_IOS)
  // Re-initialize the decoder if we have an invalid session while the app is
  // active and retry the decode request.
  if (status == kVTInvalidSessionErr && [self resetDecompressionSession] == WEBRTC_VIDEO_CODEC_OK) {
    frameDecodeParams.reset(new RTCFrameDecodeParams(_callback, inputImage.timeStamp));
    status = VTDecompressionSessionDecodeFrame(
        _decompressionSession, sampleBuffer, decodeFlags, frameDecodeParams.release(), nullptr);
  }
#endif
  CFRelease(sampleBuffer);
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to decode frame with code: " << status;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

- (void)setCallback:(RTCVideoDecoderCallback)callback {
  _callback = callback;
}

- (NSInteger)releaseDecoder {
  // Need to invalidate the session so that callbacks no longer occur and it
  // is safe to null out the callback.
  [self destroyDecompressionSession];
  [self setVideoFormat:nullptr];
  _callback = nullptr;
  return WEBRTC_VIDEO_CODEC_OK;
}

#pragma mark - Private

- (int)resetDecompressionSession {
  [self destroyDecompressionSession];

  // Need to wait for the first SPS to initialize decoder.
  if (!_videoFormat) {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  // Set keys for OpenGL and IOSurface compatibilty, which makes the encoder
  // create pixel buffers with GPU backed memory. The intent here is to pass
  // the pixel buffers directly so we avoid a texture upload later during
  // rendering. This currently is moot because we are converting back to an
  // I420 frame after decode, but eventually we will be able to plumb
  // CVPixelBuffers directly to the renderer.
  // TODO(tkchin): Maybe only set OpenGL/IOSurface keys if we know that that
  // we can pass CVPixelBuffers as native handles in decoder output.
  static size_t const attributesSize = 3;
  CFTypeRef keys[attributesSize] = {
#if defined(WEBRTC_IOS)
    kCVPixelBufferOpenGLESCompatibilityKey,
#elif defined(WEBRTC_MAC)
    kCVPixelBufferOpenGLCompatibilityKey,
#endif
    kCVPixelBufferIOSurfacePropertiesKey,
    kCVPixelBufferPixelFormatTypeKey
  };
  CFDictionaryRef ioSurfaceValue = CreateCFTypeDictionary(nullptr, nullptr, 0);
  int64_t nv12type = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
  CFNumberRef pixelFormat = CFNumberCreate(nullptr, kCFNumberLongType, &nv12type);
  CFTypeRef values[attributesSize] = {kCFBooleanTrue, ioSurfaceValue, pixelFormat};
  CFDictionaryRef attributes = CreateCFTypeDictionary(keys, values, attributesSize);
  if (ioSurfaceValue) {
    CFRelease(ioSurfaceValue);
    ioSurfaceValue = nullptr;
  }
  if (pixelFormat) {
    CFRelease(pixelFormat);
    pixelFormat = nullptr;
  }
  VTDecompressionOutputCallbackRecord record = {
      decompressionOutputCallback, nullptr,
  };
  OSStatus status = VTDecompressionSessionCreate(
      nullptr, _videoFormat, nullptr, attributes, &record, &_decompressionSession);
  CFRelease(attributes);
  if (status != noErr) {
    [self destroyDecompressionSession];
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  [self configureDecompressionSession];

  return WEBRTC_VIDEO_CODEC_OK;
}

- (void)configureDecompressionSession {
  RTC_DCHECK(_decompressionSession);
#if defined(WEBRTC_IOS)
  VTSessionSetProperty(_decompressionSession, kVTDecompressionPropertyKey_RealTime, kCFBooleanTrue);
#endif
}

- (void)destroyDecompressionSession {
  if (_decompressionSession) {
#if defined(WEBRTC_IOS)
    if ([UIDevice isIOS11OrLater]) {
      VTDecompressionSessionWaitForAsynchronousFrames(_decompressionSession);
    }
#endif
    VTDecompressionSessionInvalidate(_decompressionSession);
    CFRelease(_decompressionSession);
    _decompressionSession = nullptr;
  }
}

- (void)setVideoFormat:(CMVideoFormatDescriptionRef)videoFormat {
  if (_videoFormat == videoFormat) {
    return;
  }
  if (_videoFormat) {
    CFRelease(_videoFormat);
  }
  _videoFormat = videoFormat;
  if (_videoFormat) {
    CFRetain(_videoFormat);
  }
}

- (NSString *)implementationName {
  return @"VideoToolbox";
}

@end
