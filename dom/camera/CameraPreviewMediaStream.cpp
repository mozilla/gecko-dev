/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CameraPreviewMediaStream.h"
#include "CameraCommon.h"

/**
 * Maximum number of outstanding invalidates before we start to drop frames;
 * if we hit this threshold, it is an indicator that the main thread is
 * either very busy or the device is busy elsewhere (e.g. encoding or
 * persisting video data).
 */
#define MAX_INVALIDATE_PENDING 4

using namespace mozilla::layers;
using namespace mozilla::dom;

namespace mozilla {

static const TrackID TRACK_VIDEO = 2;

void
FakeMediaStreamGraph::DispatchToMainThreadAfterStreamStateUpdate(already_AddRefed<nsIRunnable> aRunnable)
{
  nsCOMPtr<nsIRunnable> task = aRunnable;
  NS_DispatchToMainThread(task);
}

CameraPreviewMediaStream::CameraPreviewMediaStream(DOMMediaStream* aWrapper)
  : MediaStream(aWrapper)
  , mMutex("mozilla::camera::CameraPreviewMediaStream")
  , mInvalidatePending(0)
  , mDiscardedFrames(0)
  , mRateLimit(false)
  , mTrackCreated(false)
{
  SetGraphImpl(MediaStreamGraph::GetInstance());
  mFakeMediaStreamGraph = new FakeMediaStreamGraph();
  mIsConsumed = false;
}

void
CameraPreviewMediaStream::AddAudioOutput(void* aKey)
{
}

void
CameraPreviewMediaStream::SetAudioOutputVolume(void* aKey, float aVolume)
{
}

void
CameraPreviewMediaStream::RemoveAudioOutput(void* aKey)
{
}

void
CameraPreviewMediaStream::AddVideoOutput(VideoFrameContainer* aContainer)
{
  MutexAutoLock lock(mMutex);
  nsRefPtr<VideoFrameContainer> container = aContainer;
  AddVideoOutputImpl(container.forget());

  if (mVideoOutputs.Length() > 1) {
    return;
  }
  mIsConsumed = true;
  for (uint32_t j = 0; j < mListeners.Length(); ++j) {
    MediaStreamListener* l = mListeners[j];
    l->NotifyConsumptionChanged(mFakeMediaStreamGraph, MediaStreamListener::CONSUMED);
  }
}

void
CameraPreviewMediaStream::RemoveVideoOutput(VideoFrameContainer* aContainer)
{
  MutexAutoLock lock(mMutex);
  RemoveVideoOutputImpl(aContainer);

  if (!mVideoOutputs.IsEmpty()) {
    return;
  }
  mIsConsumed = false;
  for (uint32_t j = 0; j < mListeners.Length(); ++j) {
    MediaStreamListener* l = mListeners[j];
    l->NotifyConsumptionChanged(mFakeMediaStreamGraph, MediaStreamListener::NOT_CONSUMED);
  }
}

void
CameraPreviewMediaStream::ChangeExplicitBlockerCount(int32_t aDelta)
{
}

void
CameraPreviewMediaStream::AddListener(MediaStreamListener* aListener)
{
  MutexAutoLock lock(mMutex);

  MediaStreamListener* listener = *mListeners.AppendElement() = aListener;
  listener->NotifyBlockingChanged(mFakeMediaStreamGraph, MediaStreamListener::UNBLOCKED);
  listener->NotifyHasCurrentData(mFakeMediaStreamGraph);
}

void
CameraPreviewMediaStream::RemoveListener(MediaStreamListener* aListener)
{
  MutexAutoLock lock(mMutex);

  nsRefPtr<MediaStreamListener> listener(aListener);
  mListeners.RemoveElement(aListener);
  listener->NotifyEvent(mFakeMediaStreamGraph, MediaStreamListener::EVENT_REMOVED);
}

void
CameraPreviewMediaStream::OnPreviewStateChange(bool aActive)
{
  if (aActive) {
    MutexAutoLock lock(mMutex);
    if (!mTrackCreated) {
      mTrackCreated = true;
      VideoSegment tmpSegment;
      for (uint32_t j = 0; j < mListeners.Length(); ++j) {
        MediaStreamListener* l = mListeners[j];
        l->NotifyQueuedTrackChanges(mFakeMediaStreamGraph, TRACK_VIDEO, 0,
                                    MediaStreamListener::TRACK_EVENT_CREATED,
                                    tmpSegment);
        l->NotifyFinishedTrackCreation(mFakeMediaStreamGraph);
      }
    }
  }
}

void
CameraPreviewMediaStream::Destroy()
{
  MutexAutoLock lock(mMutex);
  mMainThreadDestroyed = true;
  DestroyImpl();
}

void
CameraPreviewMediaStream::Invalidate()
{
  MutexAutoLock lock(mMutex);
  --mInvalidatePending;
  for (nsTArray<nsRefPtr<VideoFrameContainer> >::size_type i = 0; i < mVideoOutputs.Length(); ++i) {
    VideoFrameContainer* output = mVideoOutputs[i];
    output->Invalidate();
  }
}

void
CameraPreviewMediaStream::RateLimit(bool aLimit)
{
  mRateLimit = aLimit;
}

void
CameraPreviewMediaStream::SetCurrentFrame(const gfxIntSize& aIntrinsicSize, Image* aImage)
{
  {
    MutexAutoLock lock(mMutex);

    if (mInvalidatePending > 0) {
      if (mRateLimit || mInvalidatePending > MAX_INVALIDATE_PENDING) {
        ++mDiscardedFrames;
        DOM_CAMERA_LOGW("Discard preview frame %d, %d invalidation(s) pending",
          mDiscardedFrames, mInvalidatePending);
        return;
      }

      DOM_CAMERA_LOGI("Update preview frame, %d invalidation(s) pending",
        mInvalidatePending);
    }
    mDiscardedFrames = 0;

    TimeStamp now = TimeStamp::Now();
    for (nsTArray<nsRefPtr<VideoFrameContainer> >::size_type i = 0; i < mVideoOutputs.Length(); ++i) {
      VideoFrameContainer* output = mVideoOutputs[i];
      output->SetCurrentFrame(aIntrinsicSize, aImage, now);
    }

    ++mInvalidatePending;
  }

  nsCOMPtr<nsIRunnable> event =
    NS_NewRunnableMethod(this, &CameraPreviewMediaStream::Invalidate);
  NS_DispatchToMainThread(event);
}

void
CameraPreviewMediaStream::ClearCurrentFrame()
{
  MutexAutoLock lock(mMutex);

  for (nsTArray<nsRefPtr<VideoFrameContainer> >::size_type i = 0; i < mVideoOutputs.Length(); ++i) {
    VideoFrameContainer* output = mVideoOutputs[i];
    output->ClearCurrentFrame();
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(output, &VideoFrameContainer::Invalidate);
    NS_DispatchToMainThread(event);
  }
}

}
