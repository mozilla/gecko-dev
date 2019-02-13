/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TrackBuffersManager.h"
#include "SourceBufferResource.h"
#include "SourceBuffer.h"
#include "MediaSourceDemuxer.h"

#ifdef MOZ_FMP4
#include "MP4Demuxer.h"
#endif

#include <limits>

extern PRLogModuleInfo* GetMediaSourceLog();

#define MSE_DEBUG(arg, ...) MOZ_LOG(GetMediaSourceLog(), mozilla::LogLevel::Debug, ("TrackBuffersManager(%p:%s)::%s: " arg, this, mType.get(), __func__, ##__VA_ARGS__))
#define MSE_DEBUGV(arg, ...) MOZ_LOG(GetMediaSourceLog(), mozilla::LogLevel::Verbose, ("TrackBuffersManager(%p:%s)::%s: " arg, this, mType.get(), __func__, ##__VA_ARGS__))

namespace mozilla {

static const char*
AppendStateToStr(TrackBuffersManager::AppendState aState)
{
  switch (aState) {
    case TrackBuffersManager::AppendState::WAITING_FOR_SEGMENT:
      return "WAITING_FOR_SEGMENT";
    case TrackBuffersManager::AppendState::PARSING_INIT_SEGMENT:
      return "PARSING_INIT_SEGMENT";
    case TrackBuffersManager::AppendState::PARSING_MEDIA_SEGMENT:
      return "PARSING_MEDIA_SEGMENT";
    default:
      return "IMPOSSIBLE";
  }
}

static Atomic<uint32_t> sStreamSourceID(0u);

#ifdef MOZ_EME
class DispatchKeyNeededEvent : public nsRunnable {
public:
  DispatchKeyNeededEvent(AbstractMediaDecoder* aDecoder,
                         nsTArray<uint8_t>& aInitData,
                         const nsString& aInitDataType)
    : mDecoder(aDecoder)
    , mInitData(aInitData)
    , mInitDataType(aInitDataType)
  {
  }
  NS_IMETHOD Run() {
    // Note: Null check the owner, as the decoder could have been shutdown
    // since this event was dispatched.
    MediaDecoderOwner* owner = mDecoder->GetOwner();
    if (owner) {
      owner->DispatchEncrypted(mInitData, mInitDataType);
    }
    mDecoder = nullptr;
    return NS_OK;
  }
private:
  nsRefPtr<AbstractMediaDecoder> mDecoder;
  nsTArray<uint8_t> mInitData;
  nsString mInitDataType;
};
#endif // MOZ_EME

TrackBuffersManager::TrackBuffersManager(dom::SourceBuffer* aParent, MediaSourceDecoder* aParentDecoder, const nsACString& aType)
  : mInputBuffer(new MediaByteBuffer)
  , mAppendState(AppendState::WAITING_FOR_SEGMENT)
  , mBufferFull(false)
  , mFirstInitializationSegmentReceived(false)
  , mActiveTrack(false)
  , mType(aType)
  , mParser(ContainerParser::CreateForMIMEType(aType))
  , mProcessedInput(0)
  , mAppendRunning(false)
  , mTaskQueue(aParentDecoder->GetDemuxer()->GetTaskQueue())
  , mParent(new nsMainThreadPtrHolder<dom::SourceBuffer>(aParent, false /* strict */))
  , mParentDecoder(new nsMainThreadPtrHolder<MediaSourceDecoder>(aParentDecoder, false /* strict */))
  , mMediaSourceDemuxer(mParentDecoder->GetDemuxer())
  , mMediaSourceDuration(mTaskQueue, Maybe<double>(), "TrackBuffersManager::mMediaSourceDuration (Mirror)")
  , mAbort(false)
  , mEvictionThreshold(Preferences::GetUint("media.mediasource.eviction_threshold",
                                            100 * (1 << 20)))
  , mEvictionOccurred(false)
  , mMonitor("TrackBuffersManager")
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be instanciated on the main thread");
  nsRefPtr<TrackBuffersManager> self = this;
  nsCOMPtr<nsIRunnable> task =
    NS_NewRunnableFunction([self] () {
      self->mMediaSourceDuration.Connect(self->mParentDecoder->CanonicalExplicitDuration());
    });
  GetTaskQueue()->Dispatch(task.forget());
}

TrackBuffersManager::~TrackBuffersManager()
{
}

bool
TrackBuffersManager::AppendData(MediaByteBuffer* aData,
                                TimeUnit aTimestampOffset)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("Appending %lld bytes", aData->Length());

  mEnded = false;
  nsCOMPtr<nsIRunnable> task =
    NS_NewRunnableMethodWithArg<IncomingBuffer>(
      this, &TrackBuffersManager::AppendIncomingBuffer,
      IncomingBuffer(aData, aTimestampOffset));
  GetTaskQueue()->Dispatch(task.forget());
  return true;
}

void
TrackBuffersManager::AppendIncomingBuffer(IncomingBuffer aData)
{
  MOZ_ASSERT(OnTaskQueue());
  mIncomingBuffers.AppendElement(aData);
  mAbort = false;
}

nsRefPtr<TrackBuffersManager::AppendPromise>
TrackBuffersManager::BufferAppend()
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("");

  return ProxyMediaCall(GetTaskQueue(), this,
                        __func__, &TrackBuffersManager::InitSegmentParserLoop);
}

// Abort any pending AppendData.
// We don't really care about really aborting our inner loop as by spec the
// process is happening asynchronously, as such where and when we would abort is
// non-deterministic. The SourceBuffer also makes sure BufferAppend
// isn't called should the appendBuffer be immediately aborted.
// We do however want to ensure that no new task will be dispatched on our task
// queue and only let the current one finish its job. For this we set mAbort
// to true.
void
TrackBuffersManager::AbortAppendData()
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("");

  mAbort = true;
}

void
TrackBuffersManager::ResetParserState()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mAppendRunning, "AbortAppendData must have been called");
  MSE_DEBUG("");

  // 1. If the append state equals PARSING_MEDIA_SEGMENT and the input buffer contains some complete coded frames, then run the coded frame processing algorithm until all of these complete coded frames have been processed.
  if (mAppendState == AppendState::PARSING_MEDIA_SEGMENT) {
    nsCOMPtr<nsIRunnable> task =
      NS_NewRunnableMethod(this, &TrackBuffersManager::FinishCodedFrameProcessing);
    GetTaskQueue()->Dispatch(task.forget());
  } else {
    nsCOMPtr<nsIRunnable> task =
      NS_NewRunnableMethod(this, &TrackBuffersManager::CompleteResetParserState);
    GetTaskQueue()->Dispatch(task.forget());
  }

  // Our ResetParserState is really asynchronous, the current task has been
  // interrupted and will complete shortly (or has already completed).
  // We must however present to the main thread a stable, reset state.
  // So we run the following operation now in the main thread.
  // 7. Set append state to WAITING_FOR_SEGMENT.
  SetAppendState(AppendState::WAITING_FOR_SEGMENT);
}

nsRefPtr<TrackBuffersManager::RangeRemovalPromise>
TrackBuffersManager::RangeRemoval(TimeUnit aStart, TimeUnit aEnd)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("From %.2f to %.2f", aStart.ToSeconds(), aEnd.ToSeconds());

  mEnded = false;

  return ProxyMediaCall(GetTaskQueue(), this, __func__,
                        &TrackBuffersManager::CodedFrameRemovalWithPromise,
                        TimeInterval(aStart, aEnd));
}

TrackBuffersManager::EvictDataResult
TrackBuffersManager::EvictData(TimeUnit aPlaybackTime,
                               uint32_t aThreshold,
                               TimeUnit* aBufferStartTime)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("");

  int64_t toEvict = GetSize() - aThreshold;
  if (toEvict <= 0) {
    return EvictDataResult::NO_DATA_EVICTED;
  }
  if (toEvict <= 512*1024) {
    // Don't bother evicting less than 512KB.
    return EvictDataResult::CANT_EVICT;
  }

  if (mBufferFull && mEvictionOccurred) {
    return EvictDataResult::BUFFER_FULL;
  }

  MSE_DEBUG("Reaching our size limit, schedule eviction of %lld bytes", toEvict);

  nsCOMPtr<nsIRunnable> task =
    NS_NewRunnableMethodWithArgs<TimeUnit, uint32_t>(
      this, &TrackBuffersManager::DoEvictData,
      aPlaybackTime, toEvict);
  GetTaskQueue()->Dispatch(task.forget());

  return EvictDataResult::NO_DATA_EVICTED;
}

void
TrackBuffersManager::EvictBefore(TimeUnit aTime)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("");

  nsCOMPtr<nsIRunnable> task =
    NS_NewRunnableMethodWithArg<TimeInterval>(
      this, &TrackBuffersManager::CodedFrameRemoval,
      TimeInterval(TimeUnit::FromSeconds(0), aTime));
  GetTaskQueue()->Dispatch(task.forget());
}

media::TimeIntervals
TrackBuffersManager::Buffered()
{
  MSE_DEBUG("");
  MonitorAutoLock mon(mMonitor);
  // http://w3c.github.io/media-source/index.html#widl-SourceBuffer-buffered
  // 2. Let highest end time be the largest track buffer ranges end time across all the track buffers managed by this SourceBuffer object.
  TimeUnit highestEndTime;

  nsTArray<TimeIntervals*> tracks;
  if (HasVideo()) {
    tracks.AppendElement(&mVideoBufferedRanges);
  }
  if (HasAudio()) {
    tracks.AppendElement(&mAudioBufferedRanges);
  }
  for (auto trackRanges : tracks) {
    highestEndTime = std::max(trackRanges->GetEnd(), highestEndTime);
  }

  // 3. Let intersection ranges equal a TimeRange object containing a single range from 0 to highest end time.
  TimeIntervals intersection{TimeInterval(TimeUnit::FromSeconds(0), highestEndTime)};

  // 4. For each track buffer managed by this SourceBuffer, run the following steps:
  //   1. Let track ranges equal the track buffer ranges for the current track buffer.
  for (auto trackRanges : tracks) {
    // 2. If readyState is "ended", then set the end time on the last range in track ranges to highest end time.
    if (mEnded) {
      trackRanges->Add(TimeInterval(trackRanges->GetEnd(), highestEndTime));
    }
    // 3. Let new intersection ranges equal the intersection between the intersection ranges and the track ranges.
    intersection.Intersection(*trackRanges);
  }
  return intersection;
}

int64_t
TrackBuffersManager::GetSize()
{
  return mSizeSourceBuffer;
}

void
TrackBuffersManager::Ended()
{
  mEnded = true;
}

void
TrackBuffersManager::Detach()
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("");

  nsRefPtr<TrackBuffersManager> self = this;
  nsCOMPtr<nsIRunnable> task =
    NS_NewRunnableFunction([self] () {
      // Clear our sourcebuffer
      self->CodedFrameRemoval(TimeInterval(TimeUnit::FromSeconds(0),
                                           TimeUnit::FromInfinity()));
      self->mMediaSourceDuration.DisconnectIfConnected();
    });
  GetTaskQueue()->Dispatch(task.forget());
}

#if defined(DEBUG)
void
TrackBuffersManager::Dump(const char* aPath)
{

}
#endif

void
TrackBuffersManager::FinishCodedFrameProcessing()
{
  MOZ_ASSERT(OnTaskQueue());

  if (mProcessingRequest.Exists()) {
    NS_WARNING("Processing request pending");
    mProcessingRequest.Disconnect();
  }
  // The spec requires us to complete parsing synchronously any outstanding
  // frames in the current media segment. This can't be implemented in a way
  // that makes sense.
  // As such we simply completely ignore the result of any pending input buffer.
  // TODO: Link to W3C bug.

  CompleteResetParserState();
}

void
TrackBuffersManager::CompleteResetParserState()
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(!mAppendRunning);
  MSE_DEBUG("");

  for (auto& track : GetTracksList()) {
    // 2. Unset the last decode timestamp on all track buffers.
    // 3. Unset the last frame duration on all track buffers.
    // 4. Unset the highest end timestamp on all track buffers.
    // 5. Set the need random access point flag on all track buffers to true.
    track->ResetAppendState();

    // if we have been aborted, we may have pending frames that we are going
    // to discard now.
    track->mQueuedSamples.Clear();
  }
  // 6. Remove all bytes from the input buffer.
  mIncomingBuffers.Clear();
  mInputBuffer = nullptr;
  if (mCurrentInputBuffer) {
    mCurrentInputBuffer->EvictAll();
    mCurrentInputBuffer = new SourceBufferResource(mType);
  }

  // We could be left with a demuxer in an unusable state. It needs to be
  // recreated. We store in the InputBuffer an init segment which will be parsed
  // during the next Segment Parser Loop and a new demuxer will be created and
  // initialized.
  if (mFirstInitializationSegmentReceived) {
    MOZ_ASSERT(mInitData && mInitData->Length(), "we must have an init segment");
    // The aim here is really to destroy our current demuxer.
    CreateDemuxerforMIMEType();
    // Recreate our input buffer. We can't directly assign the initData buffer
    // to mInputBuffer as it will get modified in the Segment Parser Loop.
    mInputBuffer = new MediaByteBuffer;
    mInputBuffer->AppendElements(*mInitData);
  }
  RecreateParser();

  // 7. Set append state to WAITING_FOR_SEGMENT.
  SetAppendState(AppendState::WAITING_FOR_SEGMENT);

  // Reject our promise immediately.
  mAppendPromise.RejectIfExists(NS_ERROR_ABORT, __func__);
}

void
TrackBuffersManager::DoEvictData(const TimeUnit& aPlaybackTime,
                                 uint32_t aSizeToEvict)
{
  MOZ_ASSERT(OnTaskQueue());

  // Video is what takes the most space, only evict there if we have video.
  const auto& track = HasVideo() ? mVideoTracks : mAudioTracks;
  const auto& buffer = track.mBuffers.LastElement();
  // Remove any data we've already played, or before the next sample to be
  // demuxed whichever is lowest.
  TimeUnit lowerLimit = std::min(track.mNextSampleTime, aPlaybackTime);
  uint32_t lastKeyFrameIndex = 0;
  int64_t toEvict = aSizeToEvict;
  uint32_t partialEvict = 0;
  for (uint32_t i = 0; i < buffer.Length(); i++) {
    const auto& frame = buffer[i];
    if (frame->mKeyframe) {
      lastKeyFrameIndex = i;
      toEvict -= partialEvict;
      if (toEvict < 0) {
        break;
      }
      partialEvict = 0;
    }
    if (frame->mTime >= lowerLimit.ToMicroseconds()) {
      break;
    }
    partialEvict += sizeof(*frame) + frame->mSize;
  }

  int64_t finalSize = mSizeSourceBuffer - aSizeToEvict;

  if (lastKeyFrameIndex > 0) {
    MSE_DEBUG("Step1. Evicting %u bytes prior currentTime",
              aSizeToEvict - toEvict);
    CodedFrameRemoval(
      TimeInterval(TimeUnit::FromMicroseconds(0),
                   TimeUnit::FromMicroseconds(buffer[lastKeyFrameIndex]->mTime - 1)));
  }

  if (mSizeSourceBuffer <= finalSize) {
    return;
  }

  toEvict = mSizeSourceBuffer - finalSize;

  // Still some to remove. Remove data starting from the end, up to 30s ahead
  // of the later of the playback time or the next sample to be demuxed.
  // 30s is a value chosen as it appears to work with YouTube.
  TimeUnit upperLimit =
    std::max(aPlaybackTime, track.mNextSampleTime) + TimeUnit::FromSeconds(30);
  lastKeyFrameIndex = buffer.Length();
  for (int32_t i = buffer.Length() - 1; i >= 0; i--) {
    const auto& frame = buffer[i];
    if (frame->mKeyframe) {
      lastKeyFrameIndex = i;
      toEvict -= partialEvict;
      if (toEvict < 0) {
        break;
      }
      partialEvict = 0;
    }
    if (frame->mTime <= upperLimit.ToMicroseconds()) {
      break;
    }
    partialEvict += sizeof(*frame) + frame->mSize;
  }
  if (lastKeyFrameIndex < buffer.Length()) {
    MSE_DEBUG("Step2. Evicting %u bytes from trailing data",
              mSizeSourceBuffer - finalSize);
    CodedFrameRemoval(
      TimeInterval(TimeUnit::FromMicroseconds(buffer[lastKeyFrameIndex]->GetEndTime() + 1),
                   TimeUnit::FromInfinity()));
  }
}

nsRefPtr<TrackBuffersManager::RangeRemovalPromise>
TrackBuffersManager::CodedFrameRemovalWithPromise(TimeInterval aInterval)
{
  MOZ_ASSERT(OnTaskQueue());
  bool rv = CodedFrameRemoval(aInterval);
  return RangeRemovalPromise::CreateAndResolve(rv, __func__);
}

bool
TrackBuffersManager::CodedFrameRemoval(TimeInterval aInterval)
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(!mAppendRunning, "Logic error: Append in progress");
  MSE_DEBUG("From %.2fs to %.2f",
            aInterval.mStart.ToSeconds(), aInterval.mEnd.ToSeconds());

  if (mMediaSourceDuration.Ref().isNothing() ||
      IsNaN(mMediaSourceDuration.Ref().ref())) {
    MSE_DEBUG("Nothing to remove, aborting");
    return false;
  }
  TimeUnit duration{TimeUnit::FromSeconds(mMediaSourceDuration.Ref().ref())};

  MSE_DEBUG("duration:%.2f", duration.ToSeconds());
  if (HasVideo()) {
    MSE_DEBUG("before video ranges=%s",
              DumpTimeRanges(mVideoTracks.mBufferedRanges).get());
  }
  if (HasAudio()) {
    MSE_DEBUG("before audio ranges=%s",
              DumpTimeRanges(mAudioTracks.mBufferedRanges).get());
  }

  // 1. Let start be the starting presentation timestamp for the removal range.
  TimeUnit start = aInterval.mStart;
  // 2. Let end be the end presentation timestamp for the removal range.
  TimeUnit end = aInterval.mEnd;

  bool dataRemoved = false;

  // 3. For each track buffer in this source buffer, run the following steps:
  for (auto track : GetTracksList()) {
    MSE_DEBUGV("Processing %s track", track->mInfo->mMimeType.get());
    // 1. Let remove end timestamp be the current value of duration
    // See bug: https://www.w3.org/Bugs/Public/show_bug.cgi?id=28727
    TimeUnit removeEndTimestamp = std::max(duration, track->mBufferedRanges.GetEnd());

    // 2. If this track buffer has a random access point timestamp that is greater than or equal to end,
    // then update remove end timestamp to that random access point timestamp.
    if (end < track->mBufferedRanges.GetEnd()) {
      for (auto& frame : track->mBuffers.LastElement()) {
        if (frame->mKeyframe && frame->mTime >= end.ToMicroseconds()) {
          removeEndTimestamp = TimeUnit::FromMicroseconds(frame->mTime);
          break;
        }
      }
    }

    bool removeCurrentCodedFrameGroup = false;

    // 3. Remove all media data, from this track buffer, that contain starting
    // timestamps greater than or equal to start and less than the remove end timestamp.
    TimeInterval removedInterval;
    Maybe<uint32_t> firstRemovedIndex;
    TrackBuffer& data = track->mBuffers.LastElement();
    for (uint32_t i = 0; i < data.Length();) {
      const auto& frame = data[i];
      if (frame->mTime >= start.ToMicroseconds() &&
          frame->mTime < removeEndTimestamp.ToMicroseconds()) {
        if (firstRemovedIndex.isNothing()) {
          removedInterval =
            TimeInterval(TimeUnit::FromMicroseconds(frame->mTime),
                         TimeUnit::FromMicroseconds(frame->mTime + frame->mDuration));
          firstRemovedIndex = Some(i);
        } else {
          removedInterval = removedInterval.Span(
            TimeInterval(TimeUnit::FromMicroseconds(frame->mTime),
                         TimeUnit::FromMicroseconds(frame->mTime + frame->mDuration)));
        }
        track->mSizeBuffer -= sizeof(*frame) + frame->mSize;
        data.RemoveElementAt(i);
        removeCurrentCodedFrameGroup |=
          track->mNextInsertionIndex.isSome() &&
          track->mNextInsertionIndex.ref() == i;
        if (!removeCurrentCodedFrameGroup &&
            track->mNextInsertionIndex.isSome() &&
            track->mNextInsertionIndex.ref() > i) {
          track->mNextInsertionIndex.ref()--;
        }

        if (track->mNextGetSampleIndex.isSome()) {
          if (track->mNextGetSampleIndex.ref() == i) {
            MSE_DEBUG("Next sample to be played got evicted");
            track->mNextGetSampleIndex.reset();
          } else if (track->mNextGetSampleIndex.ref() > i) {
            track->mNextGetSampleIndex.ref()--;
          }
        }
      } else {
        i++;
      }
    }
    // 4. Remove decoding dependencies of the coded frames removed in the previous step:
    // Remove all coded frames between the coded frames removed in the previous step and the next random access point after those removed frames.
    if (firstRemovedIndex.isSome()) {
      uint32_t start = firstRemovedIndex.ref();
      uint32_t end = start;
      for (;end < data.Length(); end++) {
        MediaRawData* sample = data[end].get();
        if (sample->mKeyframe) {
          break;
        }
        removedInterval = removedInterval.Span(
          TimeInterval(TimeUnit::FromMicroseconds(sample->mTime),
                       TimeUnit::FromMicroseconds(sample->GetEndTime())));
        track->mSizeBuffer -= sizeof(*sample) + sample->mSize;
      }
      data.RemoveElementsAt(start, end - start);

      removeCurrentCodedFrameGroup |=
        track->mNextInsertionIndex.isSome() &&
        track->mNextInsertionIndex.ref() >= start &&
        track->mNextInsertionIndex.ref() < end;
      if (!removeCurrentCodedFrameGroup &&
          track->mNextInsertionIndex.isSome() &&
          track->mNextInsertionIndex.ref() >= end) {
        track->mNextInsertionIndex.ref() -= end - start;
      }

      if (track->mNextGetSampleIndex.isSome()) {
        if (track->mNextGetSampleIndex.ref() >= start &&
            track->mNextGetSampleIndex.ref() < end) {
          MSE_DEBUG("Next sample to be played got evicted");
          track->mNextGetSampleIndex.reset();
        } else if (track->mNextGetSampleIndex.ref() >= end) {
          track->mNextGetSampleIndex.ref() -= end - start;
        }
      }

      MSE_DEBUG("Removing undecodable frames from:%u (frames:%d) ([%f, %f))",
                start, end - start,
                removedInterval.mStart.ToSeconds(), removedInterval.mEnd.ToSeconds());
      track->mBufferedRanges -= removedInterval;
      dataRemoved = true;
      if (removeCurrentCodedFrameGroup) {
        track->ResetAppendState();
      }
    }

    // 5. If this object is in activeSourceBuffers, the current playback position
    // is greater than or equal to start and less than the remove end timestamp,
    // and HTMLMediaElement.readyState is greater than HAVE_METADATA, then set the
    // HTMLMediaElement.readyState attribute to HAVE_METADATA and stall playback.
    // This will be done by the MDSM during playback.
    // TODO properly, so it works even if paused.
  }

  {
    MonitorAutoLock mon(mMonitor);
    mVideoBufferedRanges = mVideoTracks.mBufferedRanges;
    mAudioBufferedRanges = mAudioTracks.mBufferedRanges;
  }

  if (HasVideo()) {
    MSE_DEBUG("after video ranges=%s",
              DumpTimeRanges(mVideoTracks.mBufferedRanges).get());
  }
  if (HasAudio()) {
    MSE_DEBUG("after audio ranges=%s",
              DumpTimeRanges(mAudioTracks.mBufferedRanges).get());
  }

  // Update our reported total size.
  mSizeSourceBuffer = mVideoTracks.mSizeBuffer + mAudioTracks.mSizeBuffer;

  // 4. If buffer full flag equals true and this object is ready to accept more bytes, then set the buffer full flag to false.
  if (mBufferFull && mSizeSourceBuffer < mEvictionThreshold) {
    mBufferFull = false;
  }
  mEvictionOccurred = true;

  // Tell our demuxer that data was removed.
  mMediaSourceDemuxer->NotifyTimeRangesChanged();

  return dataRemoved;
}

nsRefPtr<TrackBuffersManager::AppendPromise>
TrackBuffersManager::InitSegmentParserLoop()
{
  MOZ_ASSERT(OnTaskQueue());

  MOZ_ASSERT(mAppendPromise.IsEmpty() && !mAppendRunning);
  nsRefPtr<AppendPromise> p = mAppendPromise.Ensure(__func__);

  AppendIncomingBuffers();
  SegmentParserLoop();

  return p;
}

void
TrackBuffersManager::AppendIncomingBuffers()
{
  MOZ_ASSERT(OnTaskQueue());
  MonitorAutoLock mon(mMonitor);
  for (auto& incomingBuffer : mIncomingBuffers) {
    if (!mInputBuffer) {
      mInputBuffer = incomingBuffer.first();
    } else if (!mInputBuffer->AppendElements(*incomingBuffer.first(), fallible)) {
      RejectAppend(NS_ERROR_OUT_OF_MEMORY, __func__);
    }
    mTimestampOffset = incomingBuffer.second();
    mLastTimestampOffset = mTimestampOffset;
  }
  mIncomingBuffers.Clear();
}

void
TrackBuffersManager::SegmentParserLoop()
{
  MOZ_ASSERT(OnTaskQueue());
  while (true) {
    // 1. If the input buffer is empty, then jump to the need more data step below.
    if (!mInputBuffer || mInputBuffer->IsEmpty()) {
      NeedMoreData();
      return;
    }
    // 2. If the input buffer contains bytes that violate the SourceBuffer
    // byte stream format specification, then run the append error algorithm with
    // the decode error parameter set to true and abort this algorithm.
    // TODO

    // 3. Remove any bytes that the byte stream format specifications say must be
    // ignored from the start of the input buffer.
    // We do not remove bytes from our input buffer. Instead we enforce that
    // our ContainerParser is able to skip over all data that is supposed to be
    // ignored.

    // 4. If the append state equals WAITING_FOR_SEGMENT, then run the following
    // steps:
    if (mAppendState == AppendState::WAITING_FOR_SEGMENT) {
      if (mParser->IsInitSegmentPresent(mInputBuffer)) {
        SetAppendState(AppendState::PARSING_INIT_SEGMENT);
        if (mFirstInitializationSegmentReceived) {
          // This is a new initialization segment. Obsolete the old one.
          mInitData = nullptr;
          RecreateParser();
        }
        continue;
      }
      if (mParser->IsMediaSegmentPresent(mInputBuffer)) {
        SetAppendState(AppendState::PARSING_MEDIA_SEGMENT);
        continue;
      }
      // We have neither an init segment nor a media segment, this is invalid
      // data. We can ignore it.
      MSE_DEBUG("Found invalid data, ignoring.");
      mInputBuffer = nullptr;
      NeedMoreData();
      return;
    }

    int64_t start, end;
    mParser->ParseStartAndEndTimestamps(mInputBuffer, start, end);
    mProcessedInput += mInputBuffer->Length();

    // 5. If the append state equals PARSING_INIT_SEGMENT, then run the
    // following steps:
    if (mAppendState == AppendState::PARSING_INIT_SEGMENT) {
      if (mParser->InitSegmentRange().IsNull()) {
        mInputBuffer = nullptr;
        NeedMoreData();
        return;
      }
      InitializationSegmentReceived();
      return;
    }
    if (mAppendState == AppendState::PARSING_MEDIA_SEGMENT) {
      // 1. If the first initialization segment received flag is false, then run the append error algorithm with the decode error parameter set to true and abort this algorithm.
      if (!mFirstInitializationSegmentReceived) {
        RejectAppend(NS_ERROR_FAILURE, __func__);
        return;
      }
      // 2. If the input buffer does not contain a complete media segment header yet, then jump to the need more data step below.
      if (mParser->MediaHeaderRange().IsNull()) {
        mCurrentInputBuffer->AppendData(mInputBuffer);
        mInputBuffer = nullptr;
        NeedMoreData();
        return;
      }
      // 3. If the input buffer contains one or more complete coded frames, then run the coded frame processing algorithm.
      nsRefPtr<TrackBuffersManager> self = this;
      mProcessingRequest.Begin(CodedFrameProcessing()
          ->Then(GetTaskQueue(), __func__,
                 [self] (bool aNeedMoreData) {
                   self->mProcessingRequest.Complete();
                   if (aNeedMoreData || self->mAbort) {
                     self->NeedMoreData();
                   } else {
                     self->ScheduleSegmentParserLoop();
                   }
                 },
                 [self] (nsresult aRejectValue) {
                   self->mProcessingRequest.Complete();
                   self->RejectAppend(aRejectValue, __func__);
                 }));
      return;
    }
  }
}

void
TrackBuffersManager::NeedMoreData()
{
  MSE_DEBUG("");
  if (!mAbort) {
    RestoreCachedVariables();
  }
  mAppendRunning = false;
  mAppendPromise.ResolveIfExists(mActiveTrack, __func__);
}

void
TrackBuffersManager::RejectAppend(nsresult aRejectValue, const char* aName)
{
  MSE_DEBUG("rv=%d", aRejectValue);
  mAppendRunning = false;
  mAppendPromise.RejectIfExists(aRejectValue, aName);
}

void
TrackBuffersManager::ScheduleSegmentParserLoop()
{
  nsCOMPtr<nsIRunnable> task =
    NS_NewRunnableMethod(this, &TrackBuffersManager::SegmentParserLoop);
  GetTaskQueue()->Dispatch(task.forget());
}

void
TrackBuffersManager::CreateDemuxerforMIMEType()
{
  if (mVideoTracks.mDemuxer) {
    mVideoTracks.mDemuxer->BreakCycles();
    mVideoTracks.mDemuxer = nullptr;
  }
  if (mAudioTracks.mDemuxer) {
    mAudioTracks.mDemuxer->BreakCycles();
    mAudioTracks.mDemuxer = nullptr;
  }
  mInputDemuxer = nullptr;
  if (mType.LowerCaseEqualsLiteral("video/webm") || mType.LowerCaseEqualsLiteral("audio/webm")) {
    NS_WARNING("Waiting on WebMDemuxer");
  // mInputDemuxer = new WebMDemuxer(mCurrentInputBuffer);
    return;
  }

#ifdef MOZ_FMP4
  if (mType.LowerCaseEqualsLiteral("video/mp4") || mType.LowerCaseEqualsLiteral("audio/mp4")) {
    mInputDemuxer = new MP4Demuxer(mCurrentInputBuffer);
    return;
  }
#endif
  NS_WARNING("Not supported (yet)");
  return;
}

void
TrackBuffersManager::InitializationSegmentReceived()
{
  MOZ_ASSERT(mParser->HasCompleteInitData());
  mInitData = mParser->InitData();
  mCurrentInputBuffer = new SourceBufferResource(mType);
  mCurrentInputBuffer->AppendData(mInitData);
  uint32_t length =
    mParser->InitSegmentRange().mEnd - (mProcessedInput - mInputBuffer->Length());
  if (mInputBuffer->Length() == length) {
    mInputBuffer = nullptr;
  } else {
    mInputBuffer->RemoveElementsAt(0, length);
  }
  CreateDemuxerforMIMEType();
  if (!mInputDemuxer) {
    NS_WARNING("TODO type not supported");
    RejectAppend(NS_ERROR_DOM_NOT_SUPPORTED_ERR, __func__);
    return;
  }
  mDemuxerInitRequest.Begin(mInputDemuxer->Init()
                      ->Then(GetTaskQueue(), __func__,
                             this,
                             &TrackBuffersManager::OnDemuxerInitDone,
                             &TrackBuffersManager::OnDemuxerInitFailed));
}

void
TrackBuffersManager::OnDemuxerInitDone(nsresult)
{
  MOZ_ASSERT(OnTaskQueue());
  MSE_DEBUG("mAbort:%d", static_cast<bool>(mAbort));
  mDemuxerInitRequest.Complete();

  if (mAbort) {
    RejectAppend(NS_ERROR_ABORT, __func__);
    return;
  }

  MediaInfo info;

  uint32_t numVideos = mInputDemuxer->GetNumberTracks(TrackInfo::kVideoTrack);
  if (numVideos) {
    // We currently only handle the first video track.
    mVideoTracks.mDemuxer = mInputDemuxer->GetTrackDemuxer(TrackInfo::kVideoTrack, 0);
    MOZ_ASSERT(mVideoTracks.mDemuxer);
    info.mVideo = *mVideoTracks.mDemuxer->GetInfo()->GetAsVideoInfo();
  }

  uint32_t numAudios = mInputDemuxer->GetNumberTracks(TrackInfo::kAudioTrack);
  if (numAudios) {
    // We currently only handle the first audio track.
    mAudioTracks.mDemuxer = mInputDemuxer->GetTrackDemuxer(TrackInfo::kAudioTrack, 0);
    MOZ_ASSERT(mAudioTracks.mDemuxer);
    info.mAudio = *mAudioTracks.mDemuxer->GetInfo()->GetAsAudioInfo();
  }

  int64_t videoDuration = numVideos ? info.mVideo.mDuration : 0;
  int64_t audioDuration = numAudios ? info.mAudio.mDuration : 0;

  int64_t duration = std::max(videoDuration, audioDuration);
  // 1. Update the duration attribute if it currently equals NaN.
  // Those steps are performed by the MediaSourceDecoder::SetInitialDuration
  nsCOMPtr<nsIRunnable> task =
    NS_NewRunnableMethodWithArg<int64_t>(mParentDecoder,
                                         &MediaSourceDecoder::SetInitialDuration,
                                         duration ? duration : -1);
  AbstractThread::MainThread()->Dispatch(task.forget());

  // 2. If the initialization segment has no audio, video, or text tracks, then
  // run the append error algorithm with the decode error parameter set to true
  // and abort these steps.
  if (!numVideos && !numAudios) {
    RejectAppend(NS_ERROR_FAILURE, __func__);
    return;
  }

  // 3. If the first initialization segment received flag is true, then run the following steps:
  if (mFirstInitializationSegmentReceived) {
    if (numVideos != mVideoTracks.mNumTracks ||
        numAudios != mAudioTracks.mNumTracks ||
        (numVideos && info.mVideo.mMimeType != mVideoTracks.mInfo->mMimeType) ||
        (numAudios && info.mAudio.mMimeType != mAudioTracks.mInfo->mMimeType)) {
      RejectAppend(NS_ERROR_FAILURE, __func__);
      return;
    }
    // 1. If more than one track for a single type are present (ie 2 audio tracks),
    // then the Track IDs match the ones in the first initialization segment.
    // TODO
    // 2. Add the appropriate track descriptions from this initialization
    // segment to each of the track buffers.
    // TODO
    // 3. Set the need random access point flag on all track buffers to true.
    mVideoTracks.mNeedRandomAccessPoint = true;
    mAudioTracks.mNeedRandomAccessPoint = true;

    mVideoTracks.mLongestFrameDuration = mVideoTracks.mLastFrameDuration;
    mAudioTracks.mLongestFrameDuration = mAudioTracks.mLastFrameDuration;
  }

  // 4. Let active track flag equal false.
  mActiveTrack = false;

  // Increase our stream id.
  uint32_t streamID = sStreamSourceID++;

  // 5. If the first initialization segment received flag is false, then run the following steps:
  if (!mFirstInitializationSegmentReceived) {
    mAudioTracks.mNumTracks = numAudios;
    // TODO:
    // 1. If the initialization segment contains tracks with codecs the user agent
    // does not support, then run the append error algorithm with the decode
    // error parameter set to true and abort these steps.

    // 2. For each audio track in the initialization segment, run following steps:
    // for (uint32_t i = 0; i < numAudios; i++) {
    if (numAudios) {
      // 1. Let audio byte stream track ID be the Track ID for the current track being processed.
      // 2. Let audio language be a BCP 47 language tag for the language specified in the initialization segment for this track or an empty string if no language info is present.
      // 3. If audio language equals an empty string or the 'und' BCP 47 value, then run the default track language algorithm with byteStreamTrackID set to audio byte stream track ID and type set to "audio" and assign the value returned by the algorithm to audio language.
      // 4. Let audio label be a label specified in the initialization segment for this track or an empty string if no label info is present.
      // 5. If audio label equals an empty string, then run the default track label algorithm with byteStreamTrackID set to audio byte stream track ID and type set to "audio" and assign the value returned by the algorithm to audio label.
      // 6. Let audio kinds be an array of kind strings specified in the initialization segment for this track or an empty array if no kind information is provided.
      // 7. If audio kinds equals an empty array, then run the default track kinds algorithm with byteStreamTrackID set to audio byte stream track ID and type set to "audio" and assign the value returned by the algorithm to audio kinds.
      // 8. For each value in audio kinds, run the following steps:
      //   1. Let current audio kind equal the value from audio kinds for this iteration of the loop.
      //   2. Let new audio track be a new AudioTrack object.
      //   3. Generate a unique ID and assign it to the id property on new audio track.
      //   4. Assign audio language to the language property on new audio track.
      //   5. Assign audio label to the label property on new audio track.
      //   6. Assign current audio kind to the kind property on new audio track.
      //   7. If audioTracks.length equals 0, then run the following steps:
      //     1. Set the enabled property on new audio track to true.
      //     2. Set active track flag to true.
      mActiveTrack = true;
      //   8. Add new audio track to the audioTracks attribute on this SourceBuffer object.
      //   9. Queue a task to fire a trusted event named addtrack, that does not bubble and is not cancelable, and that uses the TrackEvent interface, at the AudioTrackList object referenced by the audioTracks attribute on this SourceBuffer object.
      //   10. Add new audio track to the audioTracks attribute on the HTMLMediaElement.
      //   11. Queue a task to fire a trusted event named addtrack, that does not bubble and is not cancelable, and that uses the TrackEvent interface, at the AudioTrackList object referenced by the audioTracks attribute on the HTMLMediaElement.
      mAudioTracks.mBuffers.AppendElement(TrackBuffer());
      // 10. Add the track description for this track to the track buffer.
      mAudioTracks.mInfo = new SharedTrackInfo(info.mAudio, streamID);
      mAudioTracks.mLastInfo = mAudioTracks.mInfo;
    }

    mVideoTracks.mNumTracks = numVideos;
    // 3. For each video track in the initialization segment, run following steps:
    // for (uint32_t i = 0; i < numVideos; i++) {
    if (numVideos) {
      // 1. Let video byte stream track ID be the Track ID for the current track being processed.
      // 2. Let video language be a BCP 47 language tag for the language specified in the initialization segment for this track or an empty string if no language info is present.
      // 3. If video language equals an empty string or the 'und' BCP 47 value, then run the default track language algorithm with byteStreamTrackID set to video byte stream track ID and type set to "video" and assign the value returned by the algorithm to video language.
      // 4. Let video label be a label specified in the initialization segment for this track or an empty string if no label info is present.
      // 5. If video label equals an empty string, then run the default track label algorithm with byteStreamTrackID set to video byte stream track ID and type set to "video" and assign the value returned by the algorithm to video label.
      // 6. Let video kinds be an array of kind strings specified in the initialization segment for this track or an empty array if no kind information is provided.
      // 7. If video kinds equals an empty array, then run the default track kinds algorithm with byteStreamTrackID set to video byte stream track ID and type set to "video" and assign the value returned by the algorithm to video kinds.
      // 8. For each value in video kinds, run the following steps:
      //   1. Let current video kind equal the value from video kinds for this iteration of the loop.
      //   2. Let new video track be a new VideoTrack object.
      //   3. Generate a unique ID and assign it to the id property on new video track.
      //   4. Assign video language to the language property on new video track.
      //   5. Assign video label to the label property on new video track.
      //   6. Assign current video kind to the kind property on new video track.
      //   7. If videoTracks.length equals 0, then run the following steps:
      //     1. Set the selected property on new video track to true.
      //     2. Set active track flag to true.
      mActiveTrack = true;
      //   8. Add new video track to the videoTracks attribute on this SourceBuffer object.
      //   9. Queue a task to fire a trusted event named addtrack, that does not bubble and is not cancelable, and that uses the TrackEvent interface, at the VideoTrackList object referenced by the videoTracks attribute on this SourceBuffer object.
      //   10. Add new video track to the videoTracks attribute on the HTMLMediaElement.
      //   11. Queue a task to fire a trusted event named addtrack, that does not bubble and is not cancelable, and that uses the TrackEvent interface, at the VideoTrackList object referenced by the videoTracks attribute on the HTMLMediaElement.
      mVideoTracks.mBuffers.AppendElement(TrackBuffer());
      // 10. Add the track description for this track to the track buffer.
      mVideoTracks.mInfo = new SharedTrackInfo(info.mVideo, streamID);
      mVideoTracks.mLastInfo = mVideoTracks.mInfo;
    }
    // 4. For each text track in the initialization segment, run following steps:
    // 5. If active track flag equals true, then run the following steps:
    // This is handled by SourceBuffer once the promise is resolved.

    // 6. Set first initialization segment received flag to true.
    mFirstInitializationSegmentReceived = true;
  } else {
    mAudioTracks.mLastInfo = new SharedTrackInfo(info.mAudio, streamID);
    mVideoTracks.mLastInfo = new SharedTrackInfo(info.mVideo, streamID);
  }

  UniquePtr<EncryptionInfo> crypto = mInputDemuxer->GetCrypto();
  if (crypto && crypto->IsEncrypted()) {
#ifdef MOZ_EME
    // Try and dispatch 'encrypted'. Won't go if ready state still HAVE_NOTHING.
    for (uint32_t i = 0; i < crypto->mInitDatas.Length(); i++) {
      NS_DispatchToMainThread(
        new DispatchKeyNeededEvent(mParentDecoder, crypto->mInitDatas[i].mInitData, NS_LITERAL_STRING("cenc")));
    }
#endif // MOZ_EME
    info.mCrypto = *crypto;
    // We clear our crypto init data array, so the MediaFormatReader will
    // not emit an encrypted event for the same init data again.
    info.mCrypto.mInitDatas.Clear();
    mEncrypted = true;
  }

  {
    MonitorAutoLock mon(mMonitor);
    mInfo = info;
  }

  // 3. Remove the initialization segment bytes from the beginning of the input buffer.
  // This step has already been done in InitializationSegmentReceived when we
  // transferred the content into mCurrentInputBuffer.
  mCurrentInputBuffer->EvictAll();
  RecreateParser();

  // 4. Set append state to WAITING_FOR_SEGMENT.
  SetAppendState(AppendState::WAITING_FOR_SEGMENT);
  // 5. Jump to the loop top step above.
  ScheduleSegmentParserLoop();
}

void
TrackBuffersManager::OnDemuxerInitFailed(DemuxerFailureReason aFailure)
{
  MOZ_ASSERT(aFailure != DemuxerFailureReason::WAITING_FOR_DATA);
  mDemuxerInitRequest.Complete();

  RejectAppend(NS_ERROR_FAILURE, __func__);
}

nsRefPtr<TrackBuffersManager::CodedFrameProcessingPromise>
TrackBuffersManager::CodedFrameProcessing()
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(mProcessingPromise.IsEmpty());
  nsRefPtr<CodedFrameProcessingPromise> p = mProcessingPromise.Ensure(__func__);

  int64_t offset = mCurrentInputBuffer->GetLength();
  MediaByteRange mediaRange = mParser->MediaSegmentRange();
  uint32_t length;
  if (mediaRange.IsNull()) {
    length = mInputBuffer->Length();
    mCurrentInputBuffer->AppendData(mInputBuffer);
    mInputBuffer = nullptr;
  } else {
    // The mediaRange is offset by the init segment position previously added.
    length = mediaRange.mEnd - (mProcessedInput - mInputBuffer->Length());
    nsRefPtr<MediaByteBuffer> segment = new MediaByteBuffer;
    MOZ_ASSERT(mInputBuffer->Length() >= length);
    if (!segment->AppendElements(mInputBuffer->Elements(), length, fallible)) {
      return CodedFrameProcessingPromise::CreateAndReject(NS_ERROR_OUT_OF_MEMORY, __func__);
    }
    mCurrentInputBuffer->AppendData(segment);
    mInputBuffer->RemoveElementsAt(0, length);
  }
  mInputDemuxer->NotifyDataArrived(length, offset);

  DoDemuxVideo();

  return p;
}

void
TrackBuffersManager::OnDemuxFailed(TrackType aTrack,
                                   DemuxerFailureReason aFailure)
{
  MOZ_ASSERT(OnTaskQueue());
  MSE_DEBUG("Failed to demux %s, failure:%d mAbort:%d",
            aTrack == TrackType::kVideoTrack ? "video" : "audio",
            aFailure, static_cast<bool>(mAbort));
  switch (aFailure) {
    case DemuxerFailureReason::END_OF_STREAM:
    case DemuxerFailureReason::WAITING_FOR_DATA:
      if (aTrack == TrackType::kVideoTrack) {
        DoDemuxAudio();
      } else {
        CompleteCodedFrameProcessing();
      }
      break;
    case DemuxerFailureReason::DEMUXER_ERROR:
      RejectProcessing(NS_ERROR_FAILURE, __func__);
      break;
    case DemuxerFailureReason::CANCELED:
    case DemuxerFailureReason::SHUTDOWN:
      RejectProcessing(NS_ERROR_ABORT, __func__);
      break;
    default:
      MOZ_ASSERT(false);
      break;
  }
}

void
TrackBuffersManager::DoDemuxVideo()
{
  MOZ_ASSERT(OnTaskQueue());
  MSE_DEBUG("mAbort:%d", static_cast<bool>(mAbort));
  if (!HasVideo()) {
    DoDemuxAudio();
    return;
  }
  if (mAbort) {
    RejectProcessing(NS_ERROR_ABORT, __func__);
    return;
  }
  mVideoTracks.mDemuxRequest.Begin(mVideoTracks.mDemuxer->GetSamples(-1)
                             ->Then(GetTaskQueue(), __func__, this,
                                    &TrackBuffersManager::OnVideoDemuxCompleted,
                                    &TrackBuffersManager::OnVideoDemuxFailed));
}

void
TrackBuffersManager::OnVideoDemuxCompleted(nsRefPtr<MediaTrackDemuxer::SamplesHolder> aSamples)
{
  MOZ_ASSERT(OnTaskQueue());
  MSE_DEBUG("%d video samples demuxed", aSamples->mSamples.Length());
  mVideoTracks.mDemuxRequest.Complete();
  mVideoTracks.mQueuedSamples.AppendElements(aSamples->mSamples);
  DoDemuxAudio();
}

void
TrackBuffersManager::DoDemuxAudio()
{
  MOZ_ASSERT(OnTaskQueue());
  MSE_DEBUG("mAbort:%d", static_cast<bool>(mAbort));
  if (!HasAudio()) {
    CompleteCodedFrameProcessing();
    return;
  }
  if (mAbort) {
    RejectProcessing(NS_ERROR_ABORT, __func__);
    return;
  }
  mAudioTracks.mDemuxRequest.Begin(mAudioTracks.mDemuxer->GetSamples(-1)
                             ->Then(GetTaskQueue(), __func__, this,
                                    &TrackBuffersManager::OnAudioDemuxCompleted,
                                    &TrackBuffersManager::OnAudioDemuxFailed));
}

void
TrackBuffersManager::OnAudioDemuxCompleted(nsRefPtr<MediaTrackDemuxer::SamplesHolder> aSamples)
{
  MOZ_ASSERT(OnTaskQueue());
  MSE_DEBUG("%d audio samples demuxed", aSamples->mSamples.Length());
  mAudioTracks.mDemuxRequest.Complete();
  mAudioTracks.mQueuedSamples.AppendElements(aSamples->mSamples);
  CompleteCodedFrameProcessing();
}

void
TrackBuffersManager::CompleteCodedFrameProcessing()
{
  MOZ_ASSERT(OnTaskQueue());
  MSE_DEBUG("mAbort:%d", static_cast<bool>(mAbort));

  // 1. For each coded frame in the media segment run the following steps:

  for (auto& sample : mVideoTracks.mQueuedSamples) {
    while (true) {
      if (!ProcessFrame(sample, mVideoTracks)) {
        break;
      }
    }
  }
  mVideoTracks.mQueuedSamples.Clear();
#if defined(DEBUG)
  if (HasVideo()) {
    const auto& track = mVideoTracks.mBuffers.LastElement();
    MOZ_ASSERT(track.IsEmpty() || track[0]->mKeyframe);
    for (uint32_t i = 1; i < track.Length(); i++) {
      MOZ_ASSERT((track[i-1]->mTrackInfo->GetID() == track[i]->mTrackInfo->GetID() && track[i-1]->mTimecode < track[i]->mTimecode) ||
                 track[i]->mKeyframe);
    }
  }
#endif

  for (auto& sample : mAudioTracks.mQueuedSamples) {
    while (true) {
      if (!ProcessFrame(sample, mAudioTracks)) {
        break;
      }
    }
  }
  mAudioTracks.mQueuedSamples.Clear();
#if defined(DEBUG)
  if (HasAudio()) {
    const auto& track = mAudioTracks.mBuffers.LastElement();
    MOZ_ASSERT(track.IsEmpty() || track[0]->mKeyframe);
    for (uint32_t i = 1; i < track.Length(); i++) {
      MOZ_ASSERT((track[i-1]->mTrackInfo->GetID() == track[i]->mTrackInfo->GetID() && track[i-1]->mTimecode < track[i]->mTimecode) ||
                 track[i]->mKeyframe);
    }
  }
#endif

  {
    MonitorAutoLock mon(mMonitor);

    // Save our final tracks buffered ranges.
    mVideoBufferedRanges = mVideoTracks.mBufferedRanges;
    mAudioBufferedRanges = mAudioTracks.mBufferedRanges;
    if (HasAudio()) {
      MSE_DEBUG("audio new buffered range = %s",
                DumpTimeRanges(mAudioBufferedRanges).get());
    }
    if (HasVideo()) {
      MSE_DEBUG("video new buffered range = %s",
                DumpTimeRanges(mVideoBufferedRanges).get());
    }
  }

  // Update our reported total size.
  mSizeSourceBuffer = mVideoTracks.mSizeBuffer + mAudioTracks.mSizeBuffer;

  // Return to step 6.4 of Segment Parser Loop algorithm
  // 4. If this SourceBuffer is full and cannot accept more media data, then set the buffer full flag to true.
  if (mSizeSourceBuffer >= mEvictionThreshold) {
    mBufferFull = true;
    mEvictionOccurred = false;
  }

  // 5. If the input buffer does not contain a complete media segment, then jump to the need more data step below.
  if (mParser->MediaSegmentRange().IsNull()) {
    ResolveProcessing(true, __func__);
    return;
  }

  // 6. Remove the media segment bytes from the beginning of the input buffer.
  // Clear our demuxer from any already processed data.
  // As we have handled a complete media segment, it is safe to evict all data
  // from the resource.
  mCurrentInputBuffer->EvictAll();
  mInputDemuxer->NotifyDataRemoved();
  RecreateParser();

  // 7. Set append state to WAITING_FOR_SEGMENT.
  SetAppendState(AppendState::WAITING_FOR_SEGMENT);

  // Tell our demuxer that data was added.
  mMediaSourceDemuxer->NotifyTimeRangesChanged();

  // 8. Jump to the loop top step above.
  ResolveProcessing(false, __func__);
}

void
TrackBuffersManager::RejectProcessing(nsresult aRejectValue, const char* aName)
{
  if (mAbort) {
    // mAppendPromise will be resolved immediately upon mProcessingPromise
    // completing.
    mAppendRunning = false;
  }
  mProcessingPromise.RejectIfExists(aRejectValue, __func__);
}

void
TrackBuffersManager::ResolveProcessing(bool aResolveValue, const char* aName)
{
  if (mAbort) {
    // mAppendPromise will be resolved immediately upon mProcessingPromise
    // completing.
    mAppendRunning = false;
  }
  mProcessingPromise.ResolveIfExists(aResolveValue, __func__);
}

bool
TrackBuffersManager::ProcessFrame(MediaRawData* aSample,
                                  TrackData& aTrackData)
{
  TimeUnit presentationTimestamp;
  TimeUnit decodeTimestamp;

  if (!mParent->mGenerateTimestamp) {
    presentationTimestamp = TimeUnit::FromMicroseconds(aSample->mTime);
    decodeTimestamp = TimeUnit::FromMicroseconds(aSample->mTimecode);
  }

  // 2. Let frame duration be a double precision floating point representation of the coded frame's duration in seconds.
  TimeUnit frameDuration{TimeUnit::FromMicroseconds(aSample->mDuration)};

  // 3. If mode equals "sequence" and group start timestamp is set, then run the following steps:
  if (mParent->mAppendMode == SourceBufferAppendMode::Sequence &&
      mGroupStartTimestamp.isSome()) {
    mTimestampOffset = mGroupStartTimestamp.ref();
    mGroupEndTimestamp = mGroupStartTimestamp.ref();
    mVideoTracks.mNeedRandomAccessPoint = true;
    mAudioTracks.mNeedRandomAccessPoint = true;
    mGroupStartTimestamp.reset();
  }

  // 4. If timestampOffset is not 0, then run the following steps:
  if (mTimestampOffset != TimeUnit::FromSeconds(0)) {
    presentationTimestamp += mTimestampOffset;
    decodeTimestamp += mTimestampOffset;
  }

  MSE_DEBUGV("Processing %s frame(pts:%lld end:%lld, dts:%lld, duration:%lld, "
             "kf:%d)",
             aTrackData.mInfo->mMimeType.get(),
             presentationTimestamp.ToMicroseconds(),
             (presentationTimestamp + frameDuration).ToMicroseconds(),
             decodeTimestamp.ToMicroseconds(),
             frameDuration.ToMicroseconds(),
             aSample->mKeyframe);

  // 5. Let track buffer equal the track buffer that the coded frame will be added to.
  auto& trackBuffer = aTrackData;

  // 6. If last decode timestamp for track buffer is set and decode timestamp is less than last decode timestamp:
  // OR
  // If last decode timestamp for track buffer is set and the difference between decode timestamp and last decode timestamp is greater than 2 times last frame duration:

  // TODO: Maybe we should be using TimeStamp and TimeDuration instead?

  // Some MP4 content may exhibit an extremely short frame duration.
  // As such, we can't use the last frame duration as a way to detect
  // discontinuities as required per step 6 above.
  // Instead we use the biggest duration seen so far in this run (init + media
  // segment).
  if ((trackBuffer.mLastDecodeTimestamp.isSome() &&
       decodeTimestamp < trackBuffer.mLastDecodeTimestamp.ref()) ||
      (trackBuffer.mLastDecodeTimestamp.isSome() &&
       decodeTimestamp - trackBuffer.mLastDecodeTimestamp.ref() > 2*trackBuffer.mLongestFrameDuration.ref())) {

    // 1a. If mode equals "segments":
    if (mParent->mAppendMode == SourceBufferAppendMode::Segments) {
      // Set group end timestamp to presentation timestamp.
      mGroupEndTimestamp = presentationTimestamp;
    }
    // 1b. If mode equals "sequence":
    if (mParent->mAppendMode == SourceBufferAppendMode::Sequence) {
      // Set group start timestamp equal to the group end timestamp.
      mGroupStartTimestamp = Some(mGroupEndTimestamp);
    }
    for (auto& track : GetTracksList()) {
      // 2. Unset the last decode timestamp on all track buffers.
      // 3. Unset the last frame duration on all track buffers.
      // 4. Unset the highest end timestamp on all track buffers.
      // 5. Set the need random access point flag on all track buffers to true.
      track->ResetAppendState();
    }

    MSE_DEBUG("Discontinuity detected. Restarting process");
    // 6. Jump to the Loop Top step above to restart processing of the current coded frame.
    return true;
  }

  // 7. Let frame end timestamp equal the sum of presentation timestamp and frame duration.
  TimeUnit frameEndTimestamp = presentationTimestamp + frameDuration;

  // 8. If presentation timestamp is less than appendWindowStart, then set the need random access point flag to true, drop the coded frame, and jump to the top of the loop to start processing the next coded frame.
  // 9. If frame end timestamp is greater than appendWindowEnd, then set the need random access point flag to true, drop the coded frame, and jump to the top of the loop to start processing the next coded frame.

  // We apply a fuzz search += mLongestFrameDuration to get around videos where
  // the start time is negative but close to 0.
  TimeInterval targetWindow{
    TimeInterval(TimeUnit::FromSeconds(mParent->mAppendWindowStart),
                 TimeUnit::FromSeconds(mParent->mAppendWindowEnd),
                 trackBuffer.mLongestFrameDuration.valueOr(frameDuration))};
  TimeInterval frameInterval{presentationTimestamp, frameEndTimestamp};

  if (!targetWindow.Contains(frameInterval)) {
    trackBuffer.mNeedRandomAccessPoint = true;
    return false;
  }

  // 10. If the need random access point flag on track buffer equals true, then run the following steps:
  if (trackBuffer.mNeedRandomAccessPoint) {
    // 1. If the coded frame is not a random access point, then drop the coded frame and jump to the top of the loop to start processing the next coded frame.
    if (!aSample->mKeyframe) {
      return false;
    }
    // 2. Set the need random access point flag on track buffer to false.
    trackBuffer.mNeedRandomAccessPoint = false;
  }

  // TODO: Handle splicing of audio (and text) frames.
  // 11. Let spliced audio frame be an unset variable for holding audio splice information
  // 12. Let spliced timed text frame be an unset variable for holding timed text splice information

  // 13. If last decode timestamp for track buffer is unset and presentation timestamp falls within the presentation interval of a coded frame in track buffer,then run the following steps:
  // For now we only handle replacing existing frames with the new ones. So we
  // skip this step.

  // 14. Remove existing coded frames in track buffer:
  //   a) If highest end timestamp for track buffer is not set:
  //      Remove all coded frames from track buffer that have a presentation timestamp greater than or equal to presentation timestamp and less than frame end timestamp.
  //   b) If highest end timestamp for track buffer is set and less than or equal to presentation timestamp:
  //      Remove all coded frames from track buffer that have a presentation timestamp greater than or equal to highest end timestamp and less than frame end timestamp

  // There is an ambiguity on how to remove frames, which was lodged with:
  // https://www.w3.org/Bugs/Public/show_bug.cgi?id=28710, implementing as per
  // bug description.
  Maybe<uint32_t> firstRemovedIndex;
  TimeInterval removedInterval;
  TrackBuffer& data = trackBuffer.mBuffers.LastElement();
  bool removeCodedFrames =
    trackBuffer.mHighestEndTimestamp.isSome()
      ? trackBuffer.mHighestEndTimestamp.ref() <= presentationTimestamp
      : true;
  if (removeCodedFrames) {
    TimeUnit lowerBound =
      trackBuffer.mHighestEndTimestamp.valueOr(presentationTimestamp);
    if (trackBuffer.mBufferedRanges.ContainsStrict(lowerBound)) {
      for (uint32_t i = 0; i < data.Length();) {
        MediaRawData* sample = data[i].get();
        if (sample->mTime >= lowerBound.ToMicroseconds() &&
            sample->mTime < frameEndTimestamp.ToMicroseconds()) {
          if (firstRemovedIndex.isNothing()) {
            removedInterval =
              TimeInterval(TimeUnit::FromMicroseconds(sample->mTime),
                           TimeUnit::FromMicroseconds(sample->GetEndTime()));
            firstRemovedIndex = Some(i);
          } else {
            removedInterval = removedInterval.Span(
              TimeInterval(TimeUnit::FromMicroseconds(sample->mTime),
                           TimeUnit::FromMicroseconds(sample->GetEndTime())));
          }
          trackBuffer.mSizeBuffer -= sizeof(*sample) + sample->mSize;
          MSE_DEBUGV("Overlapping frame:%u ([%f, %f))",
                    i,
                    TimeUnit::FromMicroseconds(sample->mTime).ToSeconds(),
                    TimeUnit::FromMicroseconds(sample->GetEndTime()).ToSeconds());
          data.RemoveElementAt(i);

          if (trackBuffer.mNextGetSampleIndex.isSome()) {
            if (trackBuffer.mNextGetSampleIndex.ref() == i) {
              MSE_DEBUG("Next sample to be played got evicted");
              trackBuffer.mNextGetSampleIndex.reset();
            } else if (trackBuffer.mNextGetSampleIndex.ref() > i) {
              trackBuffer.mNextGetSampleIndex.ref()--;
            }
          }
        } else {
          i++;
        }
      }
    }
    // 15. Remove decoding dependencies of the coded frames removed in the previous step:
    // Remove all coded frames between the coded frames removed in the previous step and the next random access point after those removed frames.
    if (firstRemovedIndex.isSome()) {
      uint32_t start = firstRemovedIndex.ref();
      uint32_t end = start;
      for (;end < data.Length(); end++) {
        MediaRawData* sample = data[end].get();
        if (sample->mKeyframe) {
          break;
        }
        removedInterval = removedInterval.Span(
          TimeInterval(TimeUnit::FromMicroseconds(sample->mTime),
                       TimeUnit::FromMicroseconds(sample->GetEndTime())));
        trackBuffer.mSizeBuffer -= sizeof(*sample) + sample->mSize;
      }
      data.RemoveElementsAt(start, end - start);

      MSE_DEBUG("Removing undecodable frames from:%u (frames:%u) ([%f, %f))",
                start, end - start,
                removedInterval.mStart.ToSeconds(), removedInterval.mEnd.ToSeconds());

      if (trackBuffer.mNextGetSampleIndex.isSome()) {
        if (trackBuffer.mNextGetSampleIndex.ref() >= start &&
            trackBuffer.mNextGetSampleIndex.ref() < end) {
          MSE_DEBUG("Next sample to be played got evicted");
          trackBuffer.mNextGetSampleIndex.reset();
        } else if (trackBuffer.mNextGetSampleIndex.ref() >= end) {
          trackBuffer.mNextGetSampleIndex.ref() -= end - start;
        }
      }

      // Update our buffered range to exclude the range just removed.
      trackBuffer.mBufferedRanges -= removedInterval;
      MOZ_ASSERT(trackBuffer.mNextInsertionIndex.isNothing() ||
                 trackBuffer.mNextInsertionIndex.ref() <= start);
    }
  }

  // 16. Add the coded frame with the presentation timestamp, decode timestamp, and frame duration to the track buffer.
  aSample->mTime = presentationTimestamp.ToMicroseconds();
  aSample->mTimecode = decodeTimestamp.ToMicroseconds();
  aSample->mTrackInfo = trackBuffer.mLastInfo;

  if (data.IsEmpty()) {
    data.AppendElement(aSample);
    MOZ_ASSERT(aSample->mKeyframe);
    trackBuffer.mNextInsertionIndex = Some(data.Length());
  } else if (trackBuffer.mNextInsertionIndex.isSome()) {
    data.InsertElementAt(trackBuffer.mNextInsertionIndex.ref(), aSample);
    MOZ_ASSERT(trackBuffer.mNextInsertionIndex.ref() == 0 ||
               data[trackBuffer.mNextInsertionIndex.ref()]->mTrackInfo->GetID() == data[trackBuffer.mNextInsertionIndex.ref()-1]->mTrackInfo->GetID() ||
               data[trackBuffer.mNextInsertionIndex.ref()]->mKeyframe);
    trackBuffer.mNextInsertionIndex.ref()++;
  } else if (presentationTimestamp < trackBuffer.mBufferedRanges.GetStart()) {
    data.InsertElementAt(0, aSample);
    MOZ_ASSERT(aSample->mKeyframe);
    trackBuffer.mNextInsertionIndex = Some(size_t(1));
  } else {
    // Find which discontinuity we should insert the frame before.
    TimeInterval target;
    for (const auto& interval : trackBuffer.mBufferedRanges) {
      if (presentationTimestamp < interval.mStart) {
        target = interval;
        break;
      }
    }
    if (target.IsEmpty()) {
      // No existing ranges found after our frame presentation time.
      // Insert frame at the end of array.
      data.AppendElement(aSample);
      MOZ_ASSERT(data.Length() <= 2 ||
                 data[data.Length()-1]->mTrackInfo->GetID() == data[data.Length()-2]->mTrackInfo->GetID() ||
                 data[data.Length()-1]->mKeyframe);
      trackBuffer.mNextInsertionIndex = Some(data.Length());
    }
    for (uint32_t i = 0; i < data.Length(); i++) {
      const nsRefPtr<MediaRawData>& sample = data[i];
      TimeInterval sampleInterval{
        TimeUnit::FromMicroseconds(sample->mTime),
        TimeUnit::FromMicroseconds(sample->GetEndTime())};
      if (target.Intersects(sampleInterval)) {
        data.InsertElementAt(i, aSample);
        MOZ_ASSERT(i != 0 &&
                   (data[i]->mTrackInfo->GetID() == data[i-1]->mTrackInfo->GetID() ||
                    data[i]->mKeyframe));
        trackBuffer.mNextInsertionIndex = Some(size_t(i) + 1);
        break;
      }
    }
    MOZ_ASSERT(aSample->mKeyframe);
  }
  trackBuffer.mSizeBuffer += sizeof(*aSample) + aSample->mSize;

  // 17. Set last decode timestamp for track buffer to decode timestamp.
  trackBuffer.mLastDecodeTimestamp = Some(decodeTimestamp);
  // 18. Set last frame duration for track buffer to frame duration.
  trackBuffer.mLastFrameDuration =
    Some(TimeUnit::FromMicroseconds(aSample->mDuration));

  if (trackBuffer.mLongestFrameDuration.isNothing()) {
    trackBuffer.mLongestFrameDuration = trackBuffer.mLastFrameDuration;
  } else {
    trackBuffer.mLongestFrameDuration =
      Some(std::max(trackBuffer.mLongestFrameDuration.ref(),
               trackBuffer.mLastFrameDuration.ref()));
  }

  // 19. If highest end timestamp for track buffer is unset or frame end timestamp is greater than highest end timestamp, then set highest end timestamp for track buffer to frame end timestamp.
  if (trackBuffer.mHighestEndTimestamp.isNothing() ||
      frameEndTimestamp > trackBuffer.mHighestEndTimestamp.ref()) {
    trackBuffer.mHighestEndTimestamp = Some(frameEndTimestamp);
  }
  // 20. If frame end timestamp is greater than group end timestamp, then set group end timestamp equal to frame end timestamp.
  if (frameEndTimestamp > mGroupEndTimestamp) {
    mGroupEndTimestamp = frameEndTimestamp;
  }
  // 21. If generate timestamps flag equals true, then set timestampOffset equal to frame end timestamp.
  if (mParent->mGenerateTimestamp) {
    mTimestampOffset = frameEndTimestamp;
  }

  // Update our buffered range with new sample interval.
  // We allow a fuzz factor in our interval of half a frame length,
  // as fuzz is +/- value, giving an effective leeway of a full frame
  // length.
  trackBuffer.mBufferedRanges +=
    TimeInterval(presentationTimestamp, frameEndTimestamp,
                 TimeUnit::FromMicroseconds(aSample->mDuration / 2));
  return false;
}

void
TrackBuffersManager::RecreateParser()
{
  MOZ_ASSERT(OnTaskQueue());
  // Recreate our parser for only the data remaining. This is required
  // as it has parsed the entire InputBuffer provided.
  // Once the old TrackBuffer/MediaSource implementation is removed
  // we can optimize this part. TODO
  mParser = ContainerParser::CreateForMIMEType(mType);
  if (mInitData) {
    int64_t start, end;
    mParser->ParseStartAndEndTimestamps(mInitData, start, end);
    mProcessedInput = mInitData->Length();
  } else {
    mProcessedInput = 0;
  }
}

nsTArray<TrackBuffersManager::TrackData*>
TrackBuffersManager::GetTracksList()
{
  MOZ_ASSERT(OnTaskQueue());
  nsTArray<TrackData*> tracks;
  if (HasVideo()) {
    tracks.AppendElement(&mVideoTracks);
  }
  if (HasAudio()) {
    tracks.AppendElement(&mAudioTracks);
  }
  return tracks;
}

void
TrackBuffersManager::RestoreCachedVariables()
{
  MOZ_ASSERT(OnTaskQueue());
  if (mTimestampOffset != mLastTimestampOffset) {
    nsRefPtr<TrackBuffersManager> self = this;
    nsCOMPtr<nsIRunnable> task =
      NS_NewRunnableFunction([self] {
        self->mParent->SetTimestampOffset(self->mTimestampOffset);
      });
    AbstractThread::MainThread()->Dispatch(task.forget());
  }
}

void
TrackBuffersManager::SetAppendState(TrackBuffersManager::AppendState aAppendState)
{
  MSE_DEBUG("AppendState changed from %s to %s",
            AppendStateToStr(mAppendState), AppendStateToStr(aAppendState));
  mAppendState = aAppendState;
}

void
TrackBuffersManager::SetGroupStartTimestamp(const TimeUnit& aGroupStartTimestamp)
{
  if (NS_IsMainThread()) {
    nsCOMPtr<nsIRunnable> task =
      NS_NewRunnableMethodWithArg<TimeUnit>(
        this,
        &TrackBuffersManager::SetGroupStartTimestamp,
        aGroupStartTimestamp);
    GetTaskQueue()->Dispatch(task.forget());
    return;
  }
  MOZ_ASSERT(OnTaskQueue());
  mGroupStartTimestamp = Some(aGroupStartTimestamp);
}

void
TrackBuffersManager::RestartGroupStartTimestamp()
{
  if (NS_IsMainThread()) {
    nsCOMPtr<nsIRunnable> task =
      NS_NewRunnableMethod(this, &TrackBuffersManager::RestartGroupStartTimestamp);
    GetTaskQueue()->Dispatch(task.forget());
    return;
  }
  MOZ_ASSERT(OnTaskQueue());
  mGroupStartTimestamp = Some(mGroupEndTimestamp);
}

MediaInfo
TrackBuffersManager::GetMetadata()
{
  MonitorAutoLock mon(mMonitor);
  return mInfo;
}

const TimeIntervals&
TrackBuffersManager::Buffered(TrackInfo::TrackType aTrack)
{
  MOZ_ASSERT(OnTaskQueue());
  return GetTracksData(aTrack).mBufferedRanges;
}

const TrackBuffersManager::TrackBuffer&
TrackBuffersManager::GetTrackBuffer(TrackInfo::TrackType aTrack)
{
  MOZ_ASSERT(OnTaskQueue());
  return GetTracksData(aTrack).mBuffers.LastElement();
}

TimeUnit
TrackBuffersManager::Seek(TrackInfo::TrackType aTrack,
                          const TimeUnit& aTime)
{
  MOZ_ASSERT(OnTaskQueue());
  auto& trackBuffer = GetTracksData(aTrack);
  const TrackBuffersManager::TrackBuffer& track = GetTrackBuffer(aTrack);
  TimeUnit lastKeyFrameTime;
  TimeUnit lastKeyFrameTimecode;
  uint32_t lastKeyFrameIndex = 0;
  for (uint32_t i = 0; i < track.Length(); i++) {
    const nsRefPtr<MediaRawData>& sample = track[i];
    TimeUnit sampleTime = TimeUnit::FromMicroseconds(sample->mTime);
    if (sampleTime > aTime) {
      break;
    }
    if (sample->mKeyframe) {
      lastKeyFrameTimecode = TimeUnit::FromMicroseconds(sample->mTimecode);
      lastKeyFrameTime = sampleTime;
      lastKeyFrameIndex = i;
    }
    if (sampleTime == aTime) {
      break;
    }
  }
  trackBuffer.mNextGetSampleIndex = Some(lastKeyFrameIndex);
  trackBuffer.mNextSampleTimecode = lastKeyFrameTimecode;
  trackBuffer.mNextSampleTime = lastKeyFrameTime;

  return lastKeyFrameTime;
}

uint32_t
TrackBuffersManager::SkipToNextRandomAccessPoint(TrackInfo::TrackType aTrack,
                                                 const TimeUnit& aTimeThreadshold,
                                                 bool& aFound)
{
  MOZ_ASSERT(OnTaskQueue());
  uint32_t parsed = 0;
  auto& trackData = GetTracksData(aTrack);
  const TrackBuffer& track = GetTrackBuffer(aTrack);

  uint32_t nextSampleIndex = trackData.mNextGetSampleIndex.valueOr(0);
  for (uint32_t i = nextSampleIndex; i < track.Length(); i++) {
    const nsRefPtr<MediaRawData>& sample = track[i];
    if (sample->mKeyframe &&
        sample->mTime >= aTimeThreadshold.ToMicroseconds()) {
      trackData.mNextSampleTimecode =
        TimeUnit::FromMicroseconds(sample->mTimecode);
      trackData.mNextSampleTime =
        TimeUnit::FromMicroseconds(sample->mTime);
      trackData.mNextGetSampleIndex = Some(i);
      aFound = true;
      break;
    }
    parsed++;
  }

  return parsed;
}

already_AddRefed<MediaRawData>
TrackBuffersManager::GetSample(TrackInfo::TrackType aTrack,
                               const TimeUnit& aFuzz,
                               bool& aError)
{
  MOZ_ASSERT(OnTaskQueue());
  auto& trackData = GetTracksData(aTrack);
  const TrackBuffer& track = GetTrackBuffer(aTrack);

  aError = false;

  if (!track.Length() ||
      (trackData.mNextGetSampleIndex.isSome() &&
       trackData.mNextGetSampleIndex.ref() >= track.Length())) {
    return nullptr;
  }
  if (trackData.mNextGetSampleIndex.isNothing() &&
      trackData.mNextSampleTimecode == TimeUnit()) {
    // First demux, get first sample.
    trackData.mNextGetSampleIndex = Some(0u);
  }

  if (trackData.mNextGetSampleIndex.isSome()) {
    const nsRefPtr<MediaRawData>& sample =
      track[trackData.mNextGetSampleIndex.ref()];
    if (trackData.mNextGetSampleIndex.ref() &&
        sample->mTimecode > (trackData.mNextSampleTimecode + aFuzz).ToMicroseconds()) {
      // Gap is too big. End of Stream or Waiting for Data.
      return nullptr;
    }

    nsRefPtr<MediaRawData> p = sample->Clone();
    if (!p) {
      aError = true;
      return nullptr;
    }
    trackData.mNextGetSampleIndex.ref()++;
    // Estimate decode timestamp of the next sample.
    trackData.mNextSampleTimecode =
      TimeUnit::FromMicroseconds(sample->mTimecode + sample->mDuration);
    trackData.mNextSampleTime =
      TimeUnit::FromMicroseconds(sample->GetEndTime());
    return p.forget();
  }

  // Our previous index has been overwritten, attempt to find the new one.
  for (uint32_t i = 0; i < track.Length(); i++) {
    const nsRefPtr<MediaRawData>& sample = track[i];
    TimeInterval sampleInterval{
      TimeUnit::FromMicroseconds(sample->mTimecode),
      TimeUnit::FromMicroseconds(sample->mTimecode + sample->mDuration),
      aFuzz};

    if (sampleInterval.ContainsWithStrictEnd(trackData.mNextSampleTimecode)) {
      nsRefPtr<MediaRawData> p = sample->Clone();
      if (!p) {
        // OOM
        aError = true;
        return nullptr;
      }
      trackData.mNextGetSampleIndex = Some(i+1);
      trackData.mNextSampleTimecode = sampleInterval.mEnd;
      trackData.mNextSampleTime =
        TimeUnit::FromMicroseconds(sample->GetEndTime());
      return p.forget();
    }
  }

  // We couldn't find our sample by decode timestamp. Attempt to find it using
  // presentation timestamp. There will likely be small jerkiness.
    for (uint32_t i = 0; i < track.Length(); i++) {
    const nsRefPtr<MediaRawData>& sample = track[i];
    TimeInterval sampleInterval{
      TimeUnit::FromMicroseconds(sample->mTime),
      TimeUnit::FromMicroseconds(sample->GetEndTime()),
      aFuzz};

    if (sampleInterval.ContainsWithStrictEnd(trackData.mNextSampleTimecode)) {
      nsRefPtr<MediaRawData> p = sample->Clone();
      if (!p) {
        // OOM
        aError = true;
        return nullptr;
      }
      trackData.mNextGetSampleIndex = Some(i+1);
      // Estimate decode timestamp of the next sample.
      trackData.mNextSampleTimecode = sampleInterval.mEnd;
      trackData.mNextSampleTime =
        TimeUnit::FromMicroseconds(sample->GetEndTime());
      return p.forget();
    }
  }

  MSE_DEBUG("Couldn't find sample (pts:%lld dts:%lld)",
             trackData.mNextSampleTime.ToMicroseconds(),
            trackData.mNextSampleTimecode.ToMicroseconds());
  return nullptr;
}

TimeUnit
TrackBuffersManager::GetNextRandomAccessPoint(TrackInfo::TrackType aTrack)
{
  auto& trackData = GetTracksData(aTrack);
  MOZ_ASSERT(trackData.mNextGetSampleIndex.isSome());
  const TrackBuffersManager::TrackBuffer& track = GetTrackBuffer(aTrack);

  uint32_t i = trackData.mNextGetSampleIndex.ref();
  for (; i < track.Length(); i++) {
    const nsRefPtr<MediaRawData>& sample = track[i];
    if (sample->mKeyframe) {
      return TimeUnit::FromMicroseconds(sample->mTime);
    }
  }
  return media::TimeUnit::FromInfinity();
}

}
#undef MSE_DEBUG
