/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FakeVideoSource.h"

#include "mozilla/SyncRunnable.h"
#include "ImageContainer.h"

#ifdef MOZ_WEBRTC
#  include "common/YuvStamper.h"
#  include "prtime.h"
#endif

using namespace mozilla::gfx;

namespace mozilla {

FakeVideoSource::FakeVideoSource(nsISerialEventTarget* aTarget)
    : mTarget(aTarget) {}

FakeVideoSource::~FakeVideoSource() = default;

int32_t FakeVideoSource::StartCapture(int32_t aWidth, int32_t aHeight,
                                      const TimeDuration& aFrameInterval) {
  MutexAutoLock lock(mMutex);

  mTimer = NS_NewTimer(mTarget.GetEventTarget());
  if (!mTimer) {
    return -1;
  }

  MOZ_ALWAYS_SUCCEEDS(mTarget.Dispatch(NS_NewRunnableFunction(
      "FakeVideoSource::StartCapture",
      [self = RefPtr(this), this, aWidth, aHeight] {
        mTarget.AssertOnCurrentThread();
        if (!mImageContainer) {
          mImageContainer = MakeAndAddRef<layers::ImageContainer>(
              layers::ImageUsageType::Webrtc,
              layers::ImageContainer::ASYNCHRONOUS);
        }
        mWidth = aWidth;
        mHeight = aHeight;
      })));

  // Start timer for subsequent frames
  mTimer->InitHighResolutionWithNamedFuncCallback(
      [](nsITimer* aTimer, void* aClosure) {
        RefPtr<FakeVideoSource> capturer =
            static_cast<FakeVideoSource*>(aClosure);
        capturer->mTarget.AssertOnCurrentThread();
        capturer->GenerateImage();
      },
      this, aFrameInterval, nsITimer::TYPE_REPEATING_PRECISE_CAN_SKIP,
      "FakeVideoSource::GenerateFrame");

  return 0;
}

int32_t FakeVideoSource::StopCapture() {
  MutexAutoLock lock(mMutex);

  if (!mTimer) {
    return 0;
  }

  mTimer->Cancel();
  mTimer = nullptr;

  MOZ_ALWAYS_SUCCEEDS(SyncRunnable::DispatchToThread(
      mTarget.GetEventTarget(),
      NS_NewRunnableFunction(
          "FakeVideoSource::StopCapture", [self = RefPtr(this), this] {
            mTarget.AssertOnCurrentThread();
            if (!mImageContainer) {
              mImageContainer = MakeAndAddRef<layers::ImageContainer>(
                  layers::ImageUsageType::Webrtc,
                  layers::ImageContainer::ASYNCHRONOUS);
            }
          })));

  return 0;
}

bool FakeVideoSource::CaptureStarted() {
  MutexAutoLock lock(mMutex);
  return mTimer;
}

void FakeVideoSource::SetTrackingId(uint32_t aTrackingIdProcId) {
  MOZ_ALWAYS_SUCCEEDS(mTarget.Dispatch(NS_NewRunnableFunction(
      "FakeVideoSource:::SetTrackingId",
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

void FakeVideoSource::GenerateImage() {
  mTarget.AssertOnCurrentThread();

  if (mTrackingId) {
    mCaptureRecorder.Start(0, "FakeVideoSource"_ns, *mTrackingId, mWidth,
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
    return;
  }

#ifdef MOZ_WEBRTC
  uint64_t timestamp = PR_Now();
  YuvStamper::Encode(mWidth, mHeight, mWidth, data.mYChannel,
                     reinterpret_cast<unsigned char*>(&timestamp),
                     sizeof(timestamp), 0, 0);
#endif

  bool setData = NS_SUCCEEDED(ycbcr_image->CopyData(data));
  MOZ_ASSERT(setData);

  // SetData copies data, so we can free the frame
  ReleaseFrame(data);

  if (!setData) {
    return;
  }

  mGeneratedImageEvent.Notify(ycbcr_image);
  mCaptureRecorder.Record(0);
}

}  // namespace mozilla
