/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Stub header for VideoToolbox framework API.
// We include our own copy so we can build on MacOS versions
// where it's not available.

#ifndef mozilla_VideoToolbox_VideoToolbox_h
#define mozilla_VideoToolbox_VideoToolbox_h

// CoreMedia is available starting in OS X 10.7,
// so we need to dlopen it as well to run on 10.6,
// but we can depend on the real framework headers at build time.

#include <CoreFoundation/CoreFoundation.h>
#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CVPixelBuffer.h>

typedef uint32_t VTDecodeFrameFlags;
typedef uint32_t VTDecodeInfoFlags;
enum {
  kVTDecodeInfo_Asynchronous = 1UL << 0,
  kVTDecodeInfo_FrameDropped = 1UL << 1,
};
enum {
  kVTDecodeFrame_EnableAsynchronousDecompression = 1<<0,
  kVTDecodeFrame_DoNotOutputFrame = 1<<1,
  kVTDecodeFrame_1xRealTimePlayback = 1<<2,
  kVTDecodeFrame_EnableTemporalProcessing = 1<<3,
};

typedef CFTypeRef VTSessionRef;
typedef struct OpaqueVTDecompressionSession* VTDecompressionSessionRef;
typedef void (*VTDecompressionOutputCallback)(
    void*,
    void*,
    OSStatus,
    VTDecodeInfoFlags,
    CVImageBufferRef,
    CMTime,
    CMTime
);
typedef struct VTDecompressionOutputCallbackRecord {
    VTDecompressionOutputCallback decompressionOutputCallback;
    void*                         decompressionOutputRefCon;
} VTDecompressionOutputCallbackRecord;

OSStatus
VTDecompressionSessionCreate(
    CFAllocatorRef,
    CMVideoFormatDescriptionRef,
    CFDictionaryRef,
    CFDictionaryRef,
    const VTDecompressionOutputCallbackRecord*,
    VTDecompressionSessionRef*
);

OSStatus
VTDecompressionSessionDecodeFrame(
    VTDecompressionSessionRef,
    CMSampleBufferRef,
    VTDecodeFrameFlags,
    void*,
    VTDecodeInfoFlags*
);

OSStatus
VTDecompressionSessionWaitForAsynchronousFrames(
    VTDecompressionSessionRef
);

void
VTDecompressionSessionInvalidate(
    VTDecompressionSessionRef
);

OSStatus
VTSessionCopyProperty(
    VTSessionRef,
    CFStringRef,
    CFAllocatorRef,
    void*
);

OSStatus
VTSessionCopySupportedPropertyDictionary(
    VTSessionRef,
    CFDictionaryRef*
);

#endif // mozilla_VideoToolbox_VideoToolbox_h
