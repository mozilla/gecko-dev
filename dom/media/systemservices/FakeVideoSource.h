/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_SYSTEMSERVICES_FAKEVIDEOSOURCE_H_
#define DOM_MEDIA_SYSTEMSERVICES_FAKEVIDEOSOURCE_H_

#include "mozilla/EventTargetCapability.h"
#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"
#include "mozilla/ThreadSafety.h"
#include "MediaEventSource.h"
#include "PerformanceRecorder.h"

class nsITimer;

namespace mozilla {
class TimeStamp;
class TimeDurationValueCalculator;
template <typename T>
class BaseTimeDuration;
typedef BaseTimeDuration<TimeDurationValueCalculator> TimeDuration;
namespace layers {
class Image;
class ImageContainer;
}  // namespace layers

class FakeVideoSource {
 public:
  explicit FakeVideoSource(nsISerialEventTarget* aTarget);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FakeVideoSource)
  int32_t StartCapture(int32_t aWidth, int32_t aHeight,
                       const TimeDuration& aFrameInterval);
  int32_t StopCapture();
  bool CaptureStarted();
  void SetTrackingId(uint32_t aTrackingIdProcId);

  MediaEventSource<RefPtr<layers::Image>>& GeneratedImageEvent() {
    return mGeneratedImageEvent;
  }

 private:
  ~FakeVideoSource();

  /**
   * Called by mTimer when it's time to generate a new image.
   */
  void GenerateImage() MOZ_REQUIRES(mTarget);

  Mutex mMutex{"FakeVideoSource::mMutex"};
  nsCOMPtr<nsITimer> mTimer MOZ_GUARDED_BY(mMutex);
  PerformanceRecorderMulti<CaptureStage> mCaptureRecorder;
  MediaEventProducer<RefPtr<layers::Image>> mGeneratedImageEvent;

  EventTargetCapability<nsISerialEventTarget> mTarget;
  Maybe<TrackingId> mTrackingId MOZ_GUARDED_BY(mTarget);
  RefPtr<layers::ImageContainer> mImageContainer MOZ_GUARDED_BY(mTarget);
  int32_t mWidth MOZ_GUARDED_BY(mTarget) = -1;
  int32_t mHeight MOZ_GUARDED_BY(mTarget) = -1;
  int mCb MOZ_GUARDED_BY(mTarget) = 16;
  int mCr MOZ_GUARDED_BY(mTarget) = 16;
};
}  // namespace mozilla

#endif
