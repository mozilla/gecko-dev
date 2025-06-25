/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "video_capture_fake.h"

#include "common/YuvStamper.h"
#include "device_info_fake.h"
#include "libwebrtcglue/WebrtcImageBuffer.h"
#include "mozilla/SyncRunnable.h"
#include "prtime.h"
#include "ImageContainer.h"

using namespace mozilla;
using namespace mozilla::gfx;

namespace webrtc::videocapturemodule {
rtc::scoped_refptr<webrtc::VideoCaptureModule> VideoCaptureFake::Create(
    nsCOMPtr<nsISerialEventTarget>&& aTarget) {
  return rtc::make_ref_counted<VideoCaptureFake>(std::move(aTarget));
}

VideoCaptureFake::VideoCaptureFake(nsCOMPtr<nsISerialEventTarget>&& aTarget)
    : mTarget(aTarget.get()) {
  size_t len = strlen(DeviceInfoFake::kId);
  _deviceUniqueId = new (std::nothrow) char[len + 1];
  if (_deviceUniqueId) {
    memcpy(_deviceUniqueId, DeviceInfoFake::kId, len + 1);
  }
}

int32_t VideoCaptureFake::StartCapture(
    const VideoCaptureCapability& aCapability) {
  MutexLock lock(&api_lock_);

  mCapability = aCapability;

  mTimer = NS_NewTimer(mTarget.GetEventTarget());
  if (!mTimer) {
    return -1;
  }

  MOZ_ALWAYS_SUCCEEDS(mTarget.Dispatch(NS_NewRunnableFunction(
      "VideoCaptureFake::StartCapture",
      [self = RefPtr(this), this, w = aCapability.width,
       h = aCapability.height] {
        mTarget.AssertOnCurrentThread();
        if (!mImageContainer) {
          mImageContainer = MakeAndAddRef<layers::ImageContainer>(
              layers::ImageUsageType::Webrtc,
              layers::ImageContainer::ASYNCHRONOUS);
        }
        mWidth = w;
        mHeight = h;
      })));

  // Start timer for subsequent frames
  const uint32_t interval = 1000 / mCapability.maxFPS;
  mTimer->InitWithNamedFuncCallback(
      [](nsITimer* aTimer, void* aClosure) {
        RefPtr<VideoCaptureFake> capturer =
            static_cast<VideoCaptureFake*>(aClosure);
        capturer->mTarget.AssertOnCurrentThread();
        capturer->GenerateFrame();
      },
      this, interval, nsITimer::TYPE_REPEATING_PRECISE_CAN_SKIP,
      "VideoCaptureFake::GenerateFrame");

  return 0;
}

// Stops capturing synchronously. Idempotent.
int32_t VideoCaptureFake::StopCapture() {
  MutexLock lock(&api_lock_);

  if (!mTimer) {
    return 0;
  }

  mTimer->Cancel();
  mTimer = nullptr;

  MOZ_ALWAYS_SUCCEEDS(SyncRunnable::DispatchToThread(
      mTarget.GetEventTarget(),
      NS_NewRunnableFunction(
          "VideoCaptureFake::StopCapture", [self = RefPtr(this), this] {
            mTarget.AssertOnCurrentThread();
            if (!mImageContainer) {
              mImageContainer = MakeAndAddRef<layers::ImageContainer>(
                  layers::ImageUsageType::Webrtc,
                  layers::ImageContainer::ASYNCHRONOUS);
            }
          })));

  return 0;
}

bool VideoCaptureFake::CaptureStarted() {
  MutexLock lock(&api_lock_);
  return mTimer;
}

int32_t VideoCaptureFake::CaptureSettings(VideoCaptureCapability& aSettings) {
  return {};
}

void VideoCaptureFake::SetTrackingId(uint32_t aTrackingIdProcId) {
  MOZ_ALWAYS_SUCCEEDS(mTarget.Dispatch(NS_NewRunnableFunction(
      "VideoCaptureFake:::SetTrackingId",
      [self = RefPtr(this), this, aTrackingIdProcId] {
        mTarget.AssertOnCurrentThread();
        if (NS_WARN_IF(mTrackingId.isSome())) {
          // This capture instance must be shared across multiple camera
          // requests. For now ignore other requests than the first.
          return;
        }
        mTrackingId.emplace(TrackingId::Source::Camera, aTrackingIdProcId);
      })));
}

static bool AllocateSolidColorFrame(layers::PlanarYCbCrData& aData, int aWidth,
                                    int aHeight, int aY, int aCb, int aCr) {
  MOZ_ASSERT(!(aWidth & 1));
  MOZ_ASSERT(!(aHeight & 1));
  // Allocate a single frame with a solid color
  int yLen = aWidth * aHeight;
  int cbLen = yLen >> 2;
  int crLen = cbLen;
  uint8_t* frame = (uint8_t*)malloc(yLen + cbLen + crLen);
  if (!frame) {
    return false;
  }
  memset(frame, aY, yLen);
  memset(frame + yLen, aCb, cbLen);
  memset(frame + yLen + cbLen, aCr, crLen);

  aData.mYChannel = frame;
  aData.mYStride = aWidth;
  aData.mCbCrStride = aWidth >> 1;
  aData.mCbChannel = frame + yLen;
  aData.mCrChannel = aData.mCbChannel + cbLen;
  aData.mPictureRect = IntRect(0, 0, aWidth, aHeight);
  aData.mStereoMode = StereoMode::MONO;
  aData.mYUVColorSpace = gfx::YUVColorSpace::BT601;
  aData.mChromaSubsampling = gfx::ChromaSubsampling::HALF_WIDTH_AND_HEIGHT;
  return true;
}

static void ReleaseFrame(layers::PlanarYCbCrData& aData) {
  free(aData.mYChannel);
}

int32_t VideoCaptureFake::GenerateFrame() {
  mTarget.AssertOnCurrentThread();

  if (mTrackingId) {
    mCaptureRecorder.Start(0, "VideoCaptureFake"_ns, *mTrackingId, mWidth,
                           mHeight, CaptureStage::ImageType::I420);
  }

  // Update the target color
  if (mCr <= 16) {
    if (mCb < 240) {
      mCb++;
    } else {
      mCr++;
    }
  } else if (mCb >= 240) {
    if (mCr < 240) {
      mCr++;
    } else {
      mCb--;
    }
  } else if (mCr >= 240) {
    if (mCb > 16) {
      mCb--;
    } else {
      mCr--;
    }
  } else {
    mCr--;
  }

  // Allocate a single solid color image
  RefPtr<layers::PlanarYCbCrImage> ycbcr_image =
      mImageContainer->CreatePlanarYCbCrImage();
  layers::PlanarYCbCrData data;
  if (NS_WARN_IF(
          !AllocateSolidColorFrame(data, mWidth, mHeight, 0x80, mCb, mCr))) {
    return -1;
  }

  uint64_t timestamp = PR_Now();
  YuvStamper::Encode(mWidth, mHeight, mWidth, data.mYChannel,
                     reinterpret_cast<unsigned char*>(&timestamp),
                     sizeof(timestamp), 0, 0);

  bool setData = NS_SUCCEEDED(ycbcr_image->CopyData(data));
  MOZ_ASSERT(setData);

  // SetData copies data, so we can free the frame
  ReleaseFrame(data);

  if (!setData) {
    return -1;
  }

  auto frame = webrtc::VideoFrame::Builder()
                   .set_video_frame_buffer(rtc::make_ref_counted<ImageBuffer>(
                       std::move(ycbcr_image)))
                   .set_timestamp_us(-1)
                   .build();

  MutexLock lock(&api_lock_);
  int32_t rv = DeliverCapturedFrame(frame);
  mCaptureRecorder.Record(0);
  return rv;
}

}  // namespace webrtc::videocapturemodule
