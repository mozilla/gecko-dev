/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_INCLUDE_VIDEO_CAPTURE_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_INCLUDE_VIDEO_CAPTURE_H_

#include "webrtc/modules/interface/module.h"
#include "webrtc/modules/video_capture/include/video_capture_defines.h"

#if defined(ANDROID) && !defined(WEBRTC_GONK)
#include <jni.h>
#endif

namespace webrtc {

#if defined(ANDROID) && !defined(WEBRTC_CHROMIUM_BUILD) && !defined(WEBRTC_GONK)
int32_t SetCaptureAndroidVM(JavaVM* javaVM);
#endif

class VideoCaptureModule: public RefCountedModule {
 public:
  // Interface for receiving information about available camera devices.
  class DeviceInfo {
   public:
    virtual uint32_t NumberOfDevices() = 0;

    // Returns the available capture devices.
    // deviceNumber   - Index of capture device.
    // deviceNameUTF8 - Friendly name of the capture device.
    // deviceUniqueIdUTF8 - Unique name of the capture device if it exist.
    //                      Otherwise same as deviceNameUTF8.
    // productUniqueIdUTF8 - Unique product id if it exist.
    //                       Null terminated otherwise.
    virtual int32_t GetDeviceName(
        uint32_t deviceNumber,
        char* deviceNameUTF8,
        uint32_t deviceNameLength,
        char* deviceUniqueIdUTF8,
        uint32_t deviceUniqueIdUTF8Length,
        char* productUniqueIdUTF8 = 0,
        uint32_t productUniqueIdUTF8Length = 0) = 0;


    // Returns the number of capabilities this device.
    virtual int32_t NumberOfCapabilities(
        const char* deviceUniqueIdUTF8) = 0;

    // Gets the capabilities of the named device.
    virtual int32_t GetCapability(
        const char* deviceUniqueIdUTF8,
        const uint32_t deviceCapabilityNumber,
        VideoCaptureCapability& capability) = 0;

    // Gets clockwise angle the captured frames should be rotated in order
    // to be displayed correctly on a normally rotated display.
    virtual int32_t GetOrientation(
        const char* deviceUniqueIdUTF8,
        VideoCaptureRotation& orientation) = 0;

    // Gets the capability that best matches the requested width, height and
    // frame rate.
    // Returns the deviceCapabilityNumber on success.
    virtual int32_t GetBestMatchedCapability(
        const char* deviceUniqueIdUTF8,
        const VideoCaptureCapability& requested,
        VideoCaptureCapability& resulting) = 0;

     // Display OS /capture device specific settings dialog
    virtual int32_t DisplayCaptureSettingsDialogBox(
        const char* deviceUniqueIdUTF8,
        const char* dialogTitleUTF8,
        void* parentWindow,
        uint32_t positionX,
        uint32_t positionY) = 0;

    virtual ~DeviceInfo() {}
  };

  class VideoCaptureEncodeInterface {
   public:
    virtual int32_t ConfigureEncoder(const VideoCodec& codec,
                                     uint32_t maxPayloadSize) = 0;
    // Inform the encoder about the new target bit rate.
    //  - newBitRate       : New target bit rate in Kbit/s.
    //  - frameRate        : The target frame rate.
    virtual int32_t SetRates(int32_t newBitRate, int32_t frameRate) = 0;
    // Inform the encoder about the packet loss and the round-trip time.
    //   - packetLoss   : Fraction lost
    //                    (loss rate in percent = 100 * packetLoss / 255).
    //   - rtt          : Round-trip time in milliseconds.
    virtual int32_t SetChannelParameters(uint32_t packetLoss, int rtt) = 0;

    // Encode the next frame as key frame.
    virtual int32_t EncodeFrameType(const FrameType type) = 0;
  protected:
    virtual ~VideoCaptureEncodeInterface() {
    }
  };

  //   Register capture data callback
  virtual void RegisterCaptureDataCallback(
      VideoCaptureDataCallback& dataCallback) = 0;

  //  Remove capture data callback
  virtual void DeRegisterCaptureDataCallback() = 0;

  // Register capture callback.
  virtual void RegisterCaptureCallback(VideoCaptureFeedBack& callBack) = 0;

  //  Remove capture callback.
  virtual void DeRegisterCaptureCallback() = 0;

  // Start capture device
  virtual int32_t StartCapture(
      const VideoCaptureCapability& capability) = 0;

  virtual int32_t StopCapture() = 0;

  // Returns the name of the device used by this module.
  virtual const char* CurrentDeviceName() const = 0;

  // Returns true if the capture device is running
  virtual bool CaptureStarted() = 0;

  // Gets the current configuration.
  virtual int32_t CaptureSettings(VideoCaptureCapability& settings) = 0;

  virtual void SetCaptureDelay(int32_t delayMS) = 0;

  // Returns the current CaptureDelay. Only valid when the camera is running.
  virtual int32_t CaptureDelay() = 0;

  // Set the rotation of the captured frames.
  // If the rotation is set to the same as returned by
  // DeviceInfo::GetOrientation the captured frames are
  // displayed correctly if rendered.
  virtual int32_t SetCaptureRotation(VideoCaptureRotation rotation) = 0;

  // Gets a pointer to an encode interface if the capture device supports the
  // requested type and size.  NULL otherwise.
  virtual VideoCaptureEncodeInterface* GetEncodeInterface(
      const VideoCodec& codec) = 0;

  virtual void EnableFrameRateCallback(const bool enable) = 0;
  virtual void EnableNoPictureAlarm(const bool enable) = 0;

protected:
  virtual ~VideoCaptureModule() {};
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_VIDEO_CAPTURE_INCLUDE_VIDEO_CAPTURE_H_
