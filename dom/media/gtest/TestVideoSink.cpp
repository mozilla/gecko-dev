/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioDeviceInfo.h"
#include "AudioSink.h"
#include "AudioSinkWrapper.h"
#include "ImageContainer.h"
#include "MockCubeb.h"
#include "MockMediaDecoderOwner.h"
#include "TimeUnits.h"
#include "VideoFrameContainer.h"
#include "VideoSink.h"
#include "gtest/gtest.h"
#include "mozilla/gtest/WaitFor.h"

using namespace mozilla;
using namespace mozilla::layers;

using media::TimeUnit;

TEST(TestVideoSink, FrameThrottling)
{
  MockCubeb* cubeb = new MockCubeb(MockCubeb::RunningMode::Manual);
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  MediaInfo info;
  info.EnableAudio();  // to control the advance of time through MockCubeb
  info.EnableVideo();

  MediaQueue<AudioData> audioQueue;
  auto audioSinkCreator = [&]() {
    return UniquePtr<AudioSink>{new AudioSink(AbstractThread::GetCurrent(),
                                              audioQueue, info.mAudio,
                                              /*resistFingerprinting*/ false)};
  };
  RefPtr wrapper = new AudioSinkWrapper(
      AbstractThread::GetCurrent(), audioQueue, std::move(audioSinkCreator),
      /*initialVolume*/ 1.0, /*playbackRate*/ 1.0, /*preservesPitch*/ true,
      /*sinkDevice*/ nullptr);

  auto owner = std::make_unique<MockMediaDecoderOwner>();
  RefPtr container = new VideoFrameContainer(
      owner.get(),
      MakeAndAddRef<ImageContainer>(ImageUsageType::VideoFrameContainer,
#ifdef MOZ_WIDGET_ANDROID
                                    // Work around bug 1922144
                                    ImageContainer::SYNCHRONOUS
#else
                                    ImageContainer::ASYNCHRONOUS
#endif
                                    ));

  MediaQueue<VideoData> videoQueue;
  RefPtr frameStatistics = new FrameStatistics();
  RefPtr videoSink = new VideoSink(AbstractThread::GetCurrent(), wrapper,
                                   videoQueue, container, *frameStatistics,
                                   /*aVQueueSentToCompositerSize*/ 9999);
  auto initPromise = TakeN(cubeb->StreamInitEvent(), 1);
  videoSink->Start(TimeUnit::Zero(), info);
  auto [stream] = WaitFor(initPromise).unwrap()[0];
  uint32_t audioRate = stream->SampleRate();

  // Enough audio data that it does not underrun, which would stop the clock.
  size_t audioFrameCount = 1000 * info.mAudio.mRate / audioRate;
  AlignedAudioBuffer samples(audioFrameCount * info.mAudio.mChannels);
  RefPtr audioData = new AudioData(
      /*aOffset*/ 0, /*aTime*/ TimeUnit(0, info.mAudio.mRate),
      std::move(samples), info.mAudio.mChannels, info.mAudio.mRate);
  audioQueue.Push(audioData);

  auto image = container->GetImageContainer()->CreatePlanarYCbCrImage();
  static uint8_t pixel[] = {0x00};
  PlanarYCbCrData imageData;
  imageData.mYChannel = imageData.mCbChannel = imageData.mCrChannel = pixel;
  imageData.mYStride = imageData.mCbCrStride = 1;
  imageData.mPictureRect = gfx::IntRect(0, 0, 1, 1);
  image->CopyData(imageData);

  TimeUnit nextFrameTime = TimeUnit(0, audioRate);
  auto PushVideoFrame = [&](const gfx::IntSize& aSize,
                            const TimeUnit& aDuration) {
    static bool isKeyFrame = true;
    RefPtr frame = VideoData::CreateFromImage(aSize, /*aOffset*/ 0,
                                              /*aTime*/ nextFrameTime,
                                              aDuration, image, isKeyFrame,
                                              /*aTimecode*/ nextFrameTime);
    frame->mFrameID = container->NewFrameID();
    videoQueue.Push(frame);
    nextFrameTime = frame->GetEndTime();
    isKeyFrame = false;
  };

  gfx::IntSize size1{1, 1};
  PushVideoFrame(size1, TimeUnit(1, audioRate));
  gfx::IntSize size2{1, 2};
  PushVideoFrame(size2, TimeUnit(1, audioRate));
  // UpdateRenderedVideoFrames() will keep scheduling additional events in
  // antipication of the audio clock advancing for the second frame, so wait
  // for only the initial size from the first frame.
  SpinEventLoopUntil("the intrinsic size receives an initial value"_ns, [&] {
    return container->CurrentIntrinsicSize().isSome();
  });
  EXPECT_EQ(container->CurrentIntrinsicSize().value(), size1);

  // Advance time to expire both frames.
  stream->ManualDataCallback(nextFrameTime.ToTicksAtRate(audioRate) + 1);
  // Run UpdateRenderedVideoFramesByTimer(), which is scheduled on TimeStamp's
  // clock, which we don't control.
  SpinEventLoopUntil(
      "the intrinsic size is updated to that of frame 2"_ns,
      [&] { return container->CurrentIntrinsicSize().value() == size2; });

  // The next frame is overdue but has not yet expired.
  gfx::IntSize size3{1, 3};
  PushVideoFrame(size3, TimeUnit(2, audioRate));
  gfx::IntSize size4{1, 4};
  PushVideoFrame(size4, TimeUnit(1, audioRate));
  // Run UpdateRenderedVideoFrames() via OnVideoQueuePushed().
  NS_ProcessPendingEvents(nullptr);
  EXPECT_EQ(container->CurrentIntrinsicSize().value(), size3);
  EXPECT_EQ(frameStatistics->GetDroppedSinkFrames(), 0u);

  // Advance time to expire the two frames in the queue and the next three.
  stream->ManualDataCallback(static_cast<long>(
      nextFrameTime.ToTicksAtRate(audioRate) + 11 - stream->Position()));
  // This frame has a longer duration and is late.
  gfx::IntSize size5{1, 5};
  PushVideoFrame(size5, TimeUnit(8, audioRate));
  // The most recent frame was late, and so is not rendered yet because it may
  // be dropped.
  //
  // OnVideoQueuePushed() uses TryUpdateRenderedVideoFrames(), which no-ops if
  // an update is already scheduled.  Wait for the update scheduled for
  // frame 4.
  SpinEventLoopUntil(
      "the intrinsic size is updated to that of frame 4"_ns,
      [&] { return container->CurrentIntrinsicSize().value() == size4; });

  // This frame is also late.
  gfx::IntSize size6{1, 6};
  PushVideoFrame(size6, TimeUnit(1, audioRate));
  NS_ProcessPendingEvents(nullptr);
  // One frame was dropped, but the most recent frame was rendered because its
  // lateness was less than the duration of the dropped frame.
  EXPECT_EQ(frameStatistics->GetDroppedSinkFrames(), 1u);
  EXPECT_EQ(container->CurrentIntrinsicSize().value(), size6);

  gfx::IntSize size7{1, 7};
  PushVideoFrame(size7, TimeUnit(1, audioRate));
  NS_ProcessPendingEvents(nullptr);
  // The most recent frame was late, and so is not rendered yet because it may
  // be dropped.
  EXPECT_EQ(container->CurrentIntrinsicSize().value(), size6);

  // On playback pause, the most recent frame is rendered.
  videoSink->SetPlaying(false);
  EXPECT_EQ(container->CurrentIntrinsicSize().value(), size7);
  EXPECT_EQ(frameStatistics->GetDroppedSinkFrames(), 1u);
  videoSink->Stop();
  videoSink->Shutdown();
}
