/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamGraphImpl.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/unused.h"

#include "AudioSegment.h"
#include "VideoSegment.h"
#include "nsContentUtils.h"
#include "nsIAppShell.h"
#include "nsIObserver.h"
#include "nsPrintfCString.h"
#include "nsServiceManagerUtils.h"
#include "nsWidgetsCID.h"
#include "prerror.h"
#include "prlog.h"
#include "mozilla/Attributes.h"
#include "TrackUnionStream.h"
#include "ImageContainer.h"
#include "AudioChannelService.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "AudioNodeExternalInputStream.h"
#include <algorithm>
#include "DOMMediaStream.h"
#include "GeckoProfiler.h"
#include "mozilla/unused.h"
#ifdef MOZ_WEBRTC
#include "AudioOutputObserver.h"
#endif

using namespace mozilla::layers;
using namespace mozilla::dom;
using namespace mozilla::gfx;

namespace mozilla {

#ifdef PR_LOGGING
PRLogModuleInfo* gMediaStreamGraphLog;
#define STREAM_LOG(type, msg) PR_LOG(gMediaStreamGraphLog, type, msg)
#else
#define STREAM_LOG(type, msg)
#endif

/**
 * The singleton graph instance.
 */
static MediaStreamGraphImpl* gGraph;

MediaStreamGraphImpl::~MediaStreamGraphImpl()
{
  NS_ASSERTION(IsEmpty(),
               "All streams should have been destroyed by messages from the main thread");
  STREAM_LOG(PR_LOG_DEBUG, ("MediaStreamGraph %p destroyed", this));
}


StreamTime
MediaStreamGraphImpl::GetDesiredBufferEnd(MediaStream* aStream)
{
  StreamTime current = mCurrentTime - aStream->mBufferStartTime;
  // When waking up media decoders, we need a longer safety margin, as it can
  // take more time to get new samples. A factor of two seem to work.
  return current +
      2 * MillisecondsToMediaTime(std::max(AUDIO_TARGET_MS, VIDEO_TARGET_MS));
}

void
MediaStreamGraphImpl::FinishStream(MediaStream* aStream)
{
  if (aStream->mFinished)
    return;
  STREAM_LOG(PR_LOG_DEBUG, ("MediaStream %p will finish", aStream));
  aStream->mFinished = true;
  aStream->mBuffer.AdvanceKnownTracksTime(STREAM_TIME_MAX);
  // Force at least one more iteration of the control loop, since we rely
  // on UpdateCurrentTime to notify our listeners once the stream end
  // has been reached.
  EnsureNextIteration();

  SetStreamOrderDirty();
}

void
MediaStreamGraphImpl::AddStream(MediaStream* aStream)
{
  aStream->mBufferStartTime = mCurrentTime;
  mStreams.AppendElement(aStream);
  STREAM_LOG(PR_LOG_DEBUG, ("Adding media stream %p to the graph", aStream));

  SetStreamOrderDirty();
}

void
MediaStreamGraphImpl::RemoveStream(MediaStream* aStream)
{
  // Remove references in mStreamUpdates before we allow aStream to die.
  // Pending updates are not needed (since the main thread has already given
  // up the stream) so we will just drop them.
  {
    MonitorAutoLock lock(mMonitor);
    for (uint32_t i = 0; i < mStreamUpdates.Length(); ++i) {
      if (mStreamUpdates[i].mStream == aStream) {
        mStreamUpdates[i].mStream = nullptr;
      }
    }
  }

  // Ensure that mFirstCycleBreaker and mMixer are updated when necessary.
  SetStreamOrderDirty();

  mStreams.RemoveElement(aStream);
  NS_RELEASE(aStream); // probably destroying it

  STREAM_LOG(PR_LOG_DEBUG, ("Removing media stream %p from the graph", aStream));
}

void
MediaStreamGraphImpl::UpdateConsumptionState(SourceMediaStream* aStream)
{
  MediaStreamListener::Consumption state =
      aStream->mIsConsumed ? MediaStreamListener::CONSUMED
      : MediaStreamListener::NOT_CONSUMED;
  if (state != aStream->mLastConsumptionState) {
    aStream->mLastConsumptionState = state;
    for (uint32_t j = 0; j < aStream->mListeners.Length(); ++j) {
      MediaStreamListener* l = aStream->mListeners[j];
      l->NotifyConsumptionChanged(this, state);
    }
  }
}

void
MediaStreamGraphImpl::ExtractPendingInput(SourceMediaStream* aStream,
                                          GraphTime aDesiredUpToTime,
                                          bool* aEnsureNextIteration)
{
  bool finished;
  {
    MutexAutoLock lock(aStream->mMutex);
    if (aStream->mPullEnabled && !aStream->mFinished &&
        !aStream->mListeners.IsEmpty()) {
      // Compute how much stream time we'll need assuming we don't block
      // the stream at all between mBlockingDecisionsMadeUntilTime and
      // aDesiredUpToTime.
      StreamTime t =
        GraphTimeToStreamTime(aStream, mStateComputedTime) +
        (aDesiredUpToTime - mStateComputedTime);
      STREAM_LOG(PR_LOG_DEBUG+1, ("Calling NotifyPull aStream=%p t=%f current end=%f", aStream,
                                  MediaTimeToSeconds(t),
                                  MediaTimeToSeconds(aStream->mBuffer.GetEnd())));
      if (t > aStream->mBuffer.GetEnd()) {
        *aEnsureNextIteration = true;
#ifdef DEBUG
        if (aStream->mListeners.Length() == 0) {
          STREAM_LOG(PR_LOG_ERROR, ("No listeners in NotifyPull aStream=%p desired=%f current end=%f",
                                    aStream, MediaTimeToSeconds(t),
                                    MediaTimeToSeconds(aStream->mBuffer.GetEnd())));
          aStream->DumpTrackInfo();
        }
#endif
        for (uint32_t j = 0; j < aStream->mListeners.Length(); ++j) {
          MediaStreamListener* l = aStream->mListeners[j];
          {
            MutexAutoUnlock unlock(aStream->mMutex);
            l->NotifyPull(this, t);
          }
        }
      }
    }
    finished = aStream->mUpdateFinished;
    for (int32_t i = aStream->mUpdateTracks.Length() - 1; i >= 0; --i) {
      SourceMediaStream::TrackData* data = &aStream->mUpdateTracks[i];
      aStream->ApplyTrackDisabling(data->mID, data->mData);
      for (uint32_t j = 0; j < aStream->mListeners.Length(); ++j) {
        MediaStreamListener* l = aStream->mListeners[j];
        TrackTicks offset = (data->mCommands & SourceMediaStream::TRACK_CREATE)
            ? data->mStart : aStream->mBuffer.FindTrack(data->mID)->GetSegment()->GetDuration();
        l->NotifyQueuedTrackChanges(this, data->mID, data->mOutputRate,
                                    offset, data->mCommands, *data->mData);
      }
      if (data->mCommands & SourceMediaStream::TRACK_CREATE) {
        MediaSegment* segment = data->mData.forget();
        STREAM_LOG(PR_LOG_DEBUG, ("SourceMediaStream %p creating track %d, rate %d, start %lld, initial end %lld",
                                  aStream, data->mID, data->mOutputRate, int64_t(data->mStart),
                                  int64_t(segment->GetDuration())));

        aStream->mBuffer.AddTrack(data->mID, data->mOutputRate, data->mStart, segment);
        // The track has taken ownership of data->mData, so let's replace
        // data->mData with an empty clone.
        data->mData = segment->CreateEmptyClone();
        data->mCommands &= ~SourceMediaStream::TRACK_CREATE;
      } else if (data->mData->GetDuration() > 0) {
        MediaSegment* dest = aStream->mBuffer.FindTrack(data->mID)->GetSegment();
        STREAM_LOG(PR_LOG_DEBUG+1, ("SourceMediaStream %p track %d, advancing end from %lld to %lld",
                                    aStream, data->mID,
                                    int64_t(dest->GetDuration()),
                                    int64_t(dest->GetDuration() + data->mData->GetDuration())));
        dest->AppendFrom(data->mData);
      }
      if (data->mCommands & SourceMediaStream::TRACK_END) {
        aStream->mBuffer.FindTrack(data->mID)->SetEnded();
        aStream->mUpdateTracks.RemoveElementAt(i);
      }
    }
    if (!aStream->mFinished) {
      aStream->mBuffer.AdvanceKnownTracksTime(aStream->mUpdateKnownTracksTime);
    }
  }
  if (aStream->mBuffer.GetEnd() > 0) {
    aStream->mHasCurrentData = true;
  }
  if (finished) {
    FinishStream(aStream);
  }
}

void
MediaStreamGraphImpl::UpdateBufferSufficiencyState(SourceMediaStream* aStream)
{
  StreamTime desiredEnd = GetDesiredBufferEnd(aStream);
  nsTArray<SourceMediaStream::ThreadAndRunnable> runnables;

  {
    MutexAutoLock lock(aStream->mMutex);
    for (uint32_t i = 0; i < aStream->mUpdateTracks.Length(); ++i) {
      SourceMediaStream::TrackData* data = &aStream->mUpdateTracks[i];
      if (data->mCommands & SourceMediaStream::TRACK_CREATE) {
        // This track hasn't been created yet, so we have no sufficiency
        // data. The track will be created in the next iteration of the
        // control loop and then we'll fire insufficiency notifications
        // if necessary.
        continue;
      }
      if (data->mCommands & SourceMediaStream::TRACK_END) {
        // This track will end, so no point in firing not-enough-data
        // callbacks.
        continue;
      }
      StreamBuffer::Track* track = aStream->mBuffer.FindTrack(data->mID);
      // Note that track->IsEnded() must be false, otherwise we would have
      // removed the track from mUpdateTracks already.
      NS_ASSERTION(!track->IsEnded(), "What is this track doing here?");
      data->mHaveEnough = track->GetEndTimeRoundDown() >= desiredEnd;
      if (!data->mHaveEnough) {
        runnables.MoveElementsFrom(data->mDispatchWhenNotEnough);
      }
    }
  }

  for (uint32_t i = 0; i < runnables.Length(); ++i) {
    runnables[i].mTarget->Dispatch(runnables[i].mRunnable, 0);
  }
}

StreamTime
MediaStreamGraphImpl::GraphTimeToStreamTime(MediaStream* aStream,
                                            GraphTime aTime)
{
  NS_ASSERTION(aTime <= mStateComputedTime,
               "Don't ask about times where we haven't made blocking decisions yet");
  if (aTime <= mCurrentTime) {
    return std::max<StreamTime>(0, aTime - aStream->mBufferStartTime);
  }
  GraphTime t = mCurrentTime;
  StreamTime s = t - aStream->mBufferStartTime;
  while (t < aTime) {
    GraphTime end;
    if (!aStream->mBlocked.GetAt(t, &end)) {
      s += std::min(aTime, end) - t;
    }
    t = end;
  }
  return std::max<StreamTime>(0, s);
}

StreamTime
MediaStreamGraphImpl::GraphTimeToStreamTimeOptimistic(MediaStream* aStream,
                                                      GraphTime aTime)
{
  GraphTime computedUpToTime = std::min(mStateComputedTime, aTime);
  StreamTime s = GraphTimeToStreamTime(aStream, computedUpToTime);
  return s + (aTime - computedUpToTime);
}

GraphTime
MediaStreamGraphImpl::StreamTimeToGraphTime(MediaStream* aStream,
                                            StreamTime aTime, uint32_t aFlags)
{
  if (aTime >= STREAM_TIME_MAX) {
    return GRAPH_TIME_MAX;
  }
  MediaTime bufferElapsedToCurrentTime = mCurrentTime - aStream->mBufferStartTime;
  if (aTime < bufferElapsedToCurrentTime ||
      (aTime == bufferElapsedToCurrentTime && !(aFlags & INCLUDE_TRAILING_BLOCKED_INTERVAL))) {
    return aTime + aStream->mBufferStartTime;
  }

  MediaTime streamAmount = aTime - bufferElapsedToCurrentTime;
  NS_ASSERTION(streamAmount >= 0, "Can't answer queries before current time");

  GraphTime t = mCurrentTime;
  while (t < GRAPH_TIME_MAX) {
    if (!(aFlags & INCLUDE_TRAILING_BLOCKED_INTERVAL) && streamAmount == 0) {
      return t;
    }
    bool blocked;
    GraphTime end;
    if (t < mStateComputedTime) {
      blocked = aStream->mBlocked.GetAt(t, &end);
      end = std::min(end, mStateComputedTime);
    } else {
      blocked = false;
      end = GRAPH_TIME_MAX;
    }
    if (blocked) {
      t = end;
    } else {
      if (streamAmount == 0) {
        // No more stream time to consume at time t, so we're done.
        break;
      }
      MediaTime consume = std::min(end - t, streamAmount);
      streamAmount -= consume;
      t += consume;
    }
  }
  return t;
}

GraphTime
MediaStreamGraphImpl::GetAudioPosition(MediaStream* aStream)
{
  if (aStream->mAudioOutputStreams.IsEmpty()) {
    return mCurrentTime;
  }
  int64_t positionInFrames = aStream->mAudioOutputStreams[0].mStream->GetPositionInFrames();
  if (positionInFrames < 0) {
    return mCurrentTime;
  }
  return aStream->mAudioOutputStreams[0].mAudioPlaybackStartTime +
    RateConvertTicksRoundDown(GraphRate(), mSampleRate, positionInFrames);
}

void
MediaStreamGraphImpl::UpdateCurrentTime()
{
  GraphTime prevCurrentTime, nextCurrentTime;
  if (mRealtime) {
    TimeStamp now = TimeStamp::Now();
    prevCurrentTime = mCurrentTime;
    nextCurrentTime =
      SecondsToMediaTime((now - mInitialTimeStamp).ToSeconds());

    mCurrentTimeStamp = now;
    STREAM_LOG(PR_LOG_DEBUG+1, ("Updating current time to %f (real %f, mStateComputedTime %f)",
               MediaTimeToSeconds(nextCurrentTime),
               (now - mInitialTimeStamp).ToSeconds(),
               MediaTimeToSeconds(mStateComputedTime)));
  } else {
    prevCurrentTime = mCurrentTime;
    nextCurrentTime = mCurrentTime + MillisecondsToMediaTime(MEDIA_GRAPH_TARGET_PERIOD_MS);
    STREAM_LOG(PR_LOG_DEBUG+1, ("Updating offline current time to %f (mStateComputedTime %f)",
               MediaTimeToSeconds(nextCurrentTime),
               MediaTimeToSeconds(mStateComputedTime)));
  }

  if (mStateComputedTime < nextCurrentTime) {
    STREAM_LOG(PR_LOG_WARNING, ("Media graph global underrun detected"));
    if (mRealtime) {
      // Adjust mInitialTimeStamp to remove the missed time.
      mInitialTimeStamp += TimeDuration::
        FromSeconds(MediaTimeToSeconds(nextCurrentTime - mStateComputedTime));
    }
    nextCurrentTime = mStateComputedTime;
  }

  if (prevCurrentTime >= nextCurrentTime) {
    NS_ASSERTION(prevCurrentTime == nextCurrentTime, "Time can't go backwards!");
    // This could happen due to low clock resolution, maybe?
    STREAM_LOG(PR_LOG_DEBUG, ("Time did not advance"));
    // There's not much left to do here, but the code below that notifies
    // listeners that streams have ended still needs to run.
  }

  nsTArray<MediaStream*> streamsReadyToFinish;
  nsAutoTArray<bool,800> streamHasOutput;
  streamHasOutput.SetLength(mStreams.Length());
  for (uint32_t i = 0; i < mStreams.Length(); ++i) {
    MediaStream* stream = mStreams[i];

    // Calculate blocked time and fire Blocked/Unblocked events
    GraphTime blockedTime = 0;
    GraphTime t = prevCurrentTime;
    // include |nextCurrentTime| to ensure NotifyBlockingChanged() is called
    // before NotifyEvent(this, EVENT_FINISHED) when |nextCurrentTime == stream end time|
    while (t <= nextCurrentTime) {
      GraphTime end;
      bool blocked = stream->mBlocked.GetAt(t, &end);
      if (blocked) {
        blockedTime += std::min(end, nextCurrentTime) - t;
      }
      if (blocked != stream->mNotifiedBlocked) {
        for (uint32_t j = 0; j < stream->mListeners.Length(); ++j) {
          MediaStreamListener* l = stream->mListeners[j];
          l->NotifyBlockingChanged(this,
              blocked ? MediaStreamListener::BLOCKED : MediaStreamListener::UNBLOCKED);
        }
        stream->mNotifiedBlocked = blocked;
      }
      t = end;
    }

    stream->AdvanceTimeVaryingValuesToCurrentTime(nextCurrentTime, blockedTime);
    // Advance mBlocked last so that implementations of
    // AdvanceTimeVaryingValuesToCurrentTime can rely on the value of mBlocked.
    stream->mBlocked.AdvanceCurrentTime(nextCurrentTime);

    streamHasOutput[i] = blockedTime < nextCurrentTime - prevCurrentTime;
    // Make this an assertion when bug 957832 is fixed.
    NS_WARN_IF_FALSE(!streamHasOutput[i] || !stream->mNotifiedFinished,
      "Shouldn't have already notified of finish *and* have output!");

    if (stream->mFinished && !stream->mNotifiedFinished) {
      streamsReadyToFinish.AppendElement(stream);
    }
    STREAM_LOG(PR_LOG_DEBUG+1, ("MediaStream %p bufferStartTime=%f blockedTime=%f",
                                stream, MediaTimeToSeconds(stream->mBufferStartTime),
                                MediaTimeToSeconds(blockedTime)));
  }

  mCurrentTime = nextCurrentTime;

  // Do these after setting mCurrentTime so that StreamTimeToGraphTime works properly.
  for (uint32_t i = 0; i < streamHasOutput.Length(); ++i) {
    if (!streamHasOutput[i]) {
      continue;
    }
    MediaStream* stream = mStreams[i];
    for (uint32_t j = 0; j < stream->mListeners.Length(); ++j) {
      MediaStreamListener* l = stream->mListeners[j];
      l->NotifyOutput(this, mCurrentTime);
    }
  }

  for (uint32_t i = 0; i < streamsReadyToFinish.Length(); ++i) {
    MediaStream* stream = streamsReadyToFinish[i];
    // The stream is fully finished when all of its track data has been played
    // out.
    if (mCurrentTime >=
          stream->StreamTimeToGraphTime(stream->GetStreamBuffer().GetAllTracksEnd()))  {
      NS_WARN_IF_FALSE(stream->mNotifiedBlocked,
        "Should've notified blocked=true for a fully finished stream");
      stream->mNotifiedFinished = true;
      stream->mLastPlayedVideoFrame.SetNull();
      SetStreamOrderDirty();
      for (uint32_t j = 0; j < stream->mListeners.Length(); ++j) {
        MediaStreamListener* l = stream->mListeners[j];
        l->NotifyEvent(this, MediaStreamListener::EVENT_FINISHED);
      }
    }
  }
}

bool
MediaStreamGraphImpl::WillUnderrun(MediaStream* aStream, GraphTime aTime,
                                   GraphTime aEndBlockingDecisions, GraphTime* aEnd)
{
  // Finished streams can't underrun. ProcessedMediaStreams also can't cause
  // underrun currently, since we'll always be able to produce data for them
  // unless they block on some other stream.
  if (aStream->mFinished || aStream->AsProcessedStream()) {
    return false;
  }
  GraphTime bufferEnd =
    StreamTimeToGraphTime(aStream, aStream->GetBufferEnd(),
                          INCLUDE_TRAILING_BLOCKED_INTERVAL);
#ifdef DEBUG
  if (bufferEnd < mCurrentTime) {
    STREAM_LOG(PR_LOG_ERROR, ("MediaStream %p underrun, "
                              "bufferEnd %f < mCurrentTime %f (%lld < %lld), Streamtime %lld",
                              aStream, MediaTimeToSeconds(bufferEnd), MediaTimeToSeconds(mCurrentTime),
                              bufferEnd, mCurrentTime, aStream->GetBufferEnd()));
    aStream->DumpTrackInfo();
    NS_ASSERTION(bufferEnd >= mCurrentTime, "Buffer underran");
  }
#endif
  // We should block after bufferEnd.
  if (bufferEnd <= aTime) {
    STREAM_LOG(PR_LOG_DEBUG+1, ("MediaStream %p will block due to data underrun, "
                                "bufferEnd %f",
                                aStream, MediaTimeToSeconds(bufferEnd)));
    return true;
  }
  // We should keep blocking if we're currently blocked and we don't have
  // data all the way through to aEndBlockingDecisions. If we don't have
  // data all the way through to aEndBlockingDecisions, we'll block soon,
  // but we might as well remain unblocked and play the data we've got while
  // we can.
  if (bufferEnd <= aEndBlockingDecisions && aStream->mBlocked.GetBefore(aTime)) {
    STREAM_LOG(PR_LOG_DEBUG+1, ("MediaStream %p will block due to speculative data underrun, "
                                "bufferEnd %f",
                                aStream, MediaTimeToSeconds(bufferEnd)));
    return true;
  }
  // Reconsider decisions at bufferEnd
  *aEnd = std::min(*aEnd, bufferEnd);
  return false;
}

void
MediaStreamGraphImpl::MarkConsumed(MediaStream* aStream)
{
  if (aStream->mIsConsumed) {
    return;
  }
  aStream->mIsConsumed = true;

  ProcessedMediaStream* ps = aStream->AsProcessedStream();
  if (!ps) {
    return;
  }
  // Mark all the inputs to this stream as consumed
  for (uint32_t i = 0; i < ps->mInputs.Length(); ++i) {
    MarkConsumed(ps->mInputs[i]->mSource);
  }
}

static void AudioMixerCallback(AudioDataValue* aMixedBuffer,
                               AudioSampleFormat aFormat,
                               uint32_t aChannels,
                               uint32_t aFrames,
                               uint32_t aSampleRate)
{
  // Need an api to register mixer callbacks, bug 989921
#ifdef MOZ_WEBRTC
  if (aFrames > 0 && aChannels > 0) {
    // XXX need Observer base class and registration API
    if (gFarendObserver) {
      gFarendObserver->InsertFarEnd(aMixedBuffer, aFrames, false,
                                    aSampleRate, aChannels, aFormat);
    }
  }
#endif
}

void
MediaStreamGraphImpl::UpdateStreamOrder()
{
  bool shouldMix = false;
  // Value of mCycleMarker for unvisited streams in cycle detection.
  const uint32_t NOT_VISITED = UINT32_MAX;
  // Value of mCycleMarker for ordered streams in muted cycles.
  const uint32_t IN_MUTED_CYCLE = 1;

  for (uint32_t i = 0; i < mStreams.Length(); ++i) {
    MediaStream* stream = mStreams[i];
    stream->mIsConsumed = false;
    stream->mInBlockingSet = false;
    if (stream->AsSourceStream() &&
        stream->AsSourceStream()->NeedsMixing()) {
      shouldMix = true;
    }
  }

  if (!mMixer && shouldMix) {
    mMixer = new AudioMixer(AudioMixerCallback);
    for (uint32_t i = 0; i < mStreams.Length(); ++i) {
      for (uint32_t j = 0; j < mStreams[i]->mAudioOutputStreams.Length(); ++j) {
        mStreams[i]->mAudioOutputStreams[j].mStream->SetMicrophoneActive(true);
      }
    }
  } else if (mMixer && !shouldMix) {
    mMixer = nullptr;
    for (uint32_t i = 0; i < mStreams.Length(); ++i) {
      for (uint32_t j = 0; j < mStreams[i]->mAudioOutputStreams.Length(); ++j) {
        mStreams[i]->mAudioOutputStreams[j].mStream->SetMicrophoneActive(false);
      }
    }
  }

  // The algorithm for finding cycles is based on Tim Leslie's iterative
  // implementation [1][2] of Pearce's variant [3] of Tarjan's strongly
  // connected components (SCC) algorithm.  There are variations (a) to
  // distinguish whether streams in SCCs of size 1 are in a cycle and (b) to
  // re-run the algorithm over SCCs with breaks at DelayNodes.
  //
  // [1] http://www.timl.id.au/?p=327
  // [2] https://github.com/scipy/scipy/blob/e2c502fca/scipy/sparse/csgraph/_traversal.pyx#L582
  // [3] http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.102.1707
  //
  // There are two stacks.  One for the depth-first search (DFS),
  mozilla::LinkedList<MediaStream> dfsStack;
  // and another for streams popped from the DFS stack, but still being
  // considered as part of SCCs involving streams on the stack.
  mozilla::LinkedList<MediaStream> sccStack;

  // An index into mStreams for the next stream found with no unsatisfied
  // upstream dependencies.
  uint32_t orderedStreamCount = 0;

  for (uint32_t i = 0; i < mStreams.Length(); ++i) {
    MediaStream* s = mStreams[i];
    if (s->IsIntrinsicallyConsumed()) {
      MarkConsumed(s);
    }
    ProcessedMediaStream* ps = s->AsProcessedStream();
    if (ps) {
      // The dfsStack initially contains a list of all processed streams in
      // unchanged order.
      dfsStack.insertBack(s);
      ps->mCycleMarker = NOT_VISITED;
    } else {
      // SourceMediaStreams have no inputs and so can be ordered now.
      mStreams[orderedStreamCount] = s;
      ++orderedStreamCount;
    }
  }

  // mNextStackMarker corresponds to "index" in Tarjan's algorithm.  It is a
  // counter to label mCycleMarker on the next visited stream in the DFS
  // uniquely in the set of visited streams that are still being considered.
  //
  // In this implementation, the counter descends so that the values are
  // strictly greater than the values that mCycleMarker takes when the stream
  // has been ordered (0 or IN_MUTED_CYCLE).
  //
  // Each new stream labelled, as the DFS searches upstream, receives a value
  // less than those used for all other streams being considered.
  uint32_t nextStackMarker = NOT_VISITED - 1;
  // Reset list of DelayNodes in cycles stored at the tail of mStreams.
  mFirstCycleBreaker = mStreams.Length();

  // Rearrange dfsStack order as required to DFS upstream and pop streams
  // in processing order to place in mStreams.
  while (auto ps = static_cast<ProcessedMediaStream*>(dfsStack.getFirst())) {
    const auto& inputs = ps->mInputs;
    MOZ_ASSERT(ps->AsProcessedStream());
    if (ps->mCycleMarker == NOT_VISITED) {
      // Record the position on the visited stack, so that any searches
      // finding this stream again know how much of the stack is in the cycle.
      ps->mCycleMarker = nextStackMarker;
      --nextStackMarker;
      // Not-visited input streams should be processed first.
      // SourceMediaStreams have already been ordered.
      for (uint32_t i = inputs.Length(); i--; ) {
        auto input = inputs[i]->mSource->AsProcessedStream();
        if (input && input->mCycleMarker == NOT_VISITED) {
          input->remove();
          dfsStack.insertFront(input);
        }
      }
      continue;
    }

    // Returning from DFS.  Pop from dfsStack.
    ps->remove();

    // cycleStackMarker keeps track of the highest marker value on any
    // upstream stream, if any, found receiving input, directly or indirectly,
    // from the visited stack (and so from |ps|, making a cycle).  In a
    // variation from Tarjan's SCC algorithm, this does not include |ps|
    // unless it is part of the cycle.
    uint32_t cycleStackMarker = 0;
    for (uint32_t i = inputs.Length(); i--; ) {
      auto input = inputs[i]->mSource->AsProcessedStream();
      if (input) {
        cycleStackMarker = std::max(cycleStackMarker, input->mCycleMarker);
      }
    }

    if (cycleStackMarker <= IN_MUTED_CYCLE) {
      // All inputs have been ordered and their stack markers have been removed.
      // This stream is not part of a cycle.  It can be processed next.
      ps->mCycleMarker = 0;
      mStreams[orderedStreamCount] = ps;
      ++orderedStreamCount;
      continue;
    }

    // A cycle has been found.  Record this stream for ordering when all
    // streams in this SCC have been popped from the DFS stack.
    sccStack.insertFront(ps);

    if (cycleStackMarker > ps->mCycleMarker) {
      // Cycles have been found that involve streams that remain on the stack.
      // Leave mCycleMarker indicating the most downstream (last) stream on
      // the stack known to be part of this SCC.  In this way, any searches on
      // other paths that find |ps| will know (without having to traverse from
      // this stream again) that they are part of this SCC (i.e. part of an
      // intersecting cycle).
      ps->mCycleMarker = cycleStackMarker;
      continue;
    }

    // |ps| is the root of an SCC involving no other streams on dfsStack, the
    // complete SCC has been recorded, and streams in this SCC are part of at
    // least one cycle.
    MOZ_ASSERT(cycleStackMarker == ps->mCycleMarker);
    // If there are DelayNodes in this SCC, then they may break the cycles.
    bool haveDelayNode = false;
    auto next = sccStack.getFirst();
    // Streams in this SCC are identified by mCycleMarker <= cycleStackMarker.
    // (There may be other streams later in sccStack from other incompletely
    // searched SCCs, involving streams still on dfsStack.)
    //
    // DelayNodes in cycles must behave differently from those not in cycles,
    // so all DelayNodes in the SCC must be identified.
    while (next && static_cast<ProcessedMediaStream*>(next)->
           mCycleMarker <= cycleStackMarker) {
      auto ns = next->AsAudioNodeStream();
      // Get next before perhaps removing from list below.
      next = next->getNext();
      if (ns && ns->Engine()->AsDelayNodeEngine()) {
        haveDelayNode = true;
        // DelayNodes break cycles by producing their output in a
        // preprocessing phase; they do not need to be ordered before their
        // consumers.  Order them at the tail of mStreams so that they can be
        // handled specially.  Do so now, so that DFS ignores them.
        ns->remove();
        ns->mCycleMarker = 0;
        --mFirstCycleBreaker;
        mStreams[mFirstCycleBreaker] = ns;
      }
    }
    auto after_scc = next;
    while ((next = sccStack.getFirst()) != after_scc) {
      next->remove();
      auto removed = static_cast<ProcessedMediaStream*>(next);
      if (haveDelayNode) {
        // Return streams to the DFS stack again (to order and detect cycles
        // without delayNodes).  Any of these streams that are still inputs
        // for streams on the visited stack must be returned to the front of
        // the stack to be ordered before their dependents.  We know that none
        // of these streams need input from streams on the visited stack, so
        // they can all be searched and ordered before the current stack head
        // is popped.
        removed->mCycleMarker = NOT_VISITED;
        dfsStack.insertFront(removed);
      } else {
        // Streams in cycles without any DelayNodes must be muted, and so do
        // not need input and can be ordered now.  They must be ordered before
        // their consumers so that their muted output is available.
        removed->mCycleMarker = IN_MUTED_CYCLE;
        mStreams[orderedStreamCount] = removed;
        ++orderedStreamCount;
      }
    }
  }

  MOZ_ASSERT(orderedStreamCount == mFirstCycleBreaker);
}

void
MediaStreamGraphImpl::RecomputeBlocking(GraphTime aEndBlockingDecisions)
{
  bool blockingDecisionsWillChange = false;

  STREAM_LOG(PR_LOG_DEBUG+1, ("Media graph %p computing blocking for time %f",
                              this, MediaTimeToSeconds(mStateComputedTime)));
  for (uint32_t i = 0; i < mStreams.Length(); ++i) {
    MediaStream* stream = mStreams[i];
    if (!stream->mInBlockingSet) {
      // Compute a partition of the streams containing 'stream' such that we can
      // compute the blocking status of each subset independently.
      nsAutoTArray<MediaStream*,10> streamSet;
      AddBlockingRelatedStreamsToSet(&streamSet, stream);

      GraphTime end;
      for (GraphTime t = mStateComputedTime;
           t < aEndBlockingDecisions; t = end) {
        end = GRAPH_TIME_MAX;
        RecomputeBlockingAt(streamSet, t, aEndBlockingDecisions, &end);
        if (end < GRAPH_TIME_MAX) {
          blockingDecisionsWillChange = true;
        }
      }
    }

    GraphTime end;
    stream->mBlocked.GetAt(mCurrentTime, &end);
    if (end < GRAPH_TIME_MAX) {
      blockingDecisionsWillChange = true;
    }
  }
  STREAM_LOG(PR_LOG_DEBUG+1, ("Media graph %p computed blocking for interval %f to %f",
                              this, MediaTimeToSeconds(mStateComputedTime),
                              MediaTimeToSeconds(aEndBlockingDecisions)));
  mStateComputedTime = aEndBlockingDecisions;

  if (blockingDecisionsWillChange) {
    // Make sure we wake up to notify listeners about these changes.
    EnsureNextIteration();
  }
}

void
MediaStreamGraphImpl::AddBlockingRelatedStreamsToSet(nsTArray<MediaStream*>* aStreams,
                                                     MediaStream* aStream)
{
  if (aStream->mInBlockingSet)
    return;
  aStream->mInBlockingSet = true;
  aStreams->AppendElement(aStream);
  for (uint32_t i = 0; i < aStream->mConsumers.Length(); ++i) {
    MediaInputPort* port = aStream->mConsumers[i];
    if (port->mFlags & (MediaInputPort::FLAG_BLOCK_INPUT | MediaInputPort::FLAG_BLOCK_OUTPUT)) {
      AddBlockingRelatedStreamsToSet(aStreams, port->mDest);
    }
  }
  ProcessedMediaStream* ps = aStream->AsProcessedStream();
  if (ps) {
    for (uint32_t i = 0; i < ps->mInputs.Length(); ++i) {
      MediaInputPort* port = ps->mInputs[i];
      if (port->mFlags & (MediaInputPort::FLAG_BLOCK_INPUT | MediaInputPort::FLAG_BLOCK_OUTPUT)) {
        AddBlockingRelatedStreamsToSet(aStreams, port->mSource);
      }
    }
  }
}

void
MediaStreamGraphImpl::MarkStreamBlocking(MediaStream* aStream)
{
  if (aStream->mBlockInThisPhase)
    return;
  aStream->mBlockInThisPhase = true;
  for (uint32_t i = 0; i < aStream->mConsumers.Length(); ++i) {
    MediaInputPort* port = aStream->mConsumers[i];
    if (port->mFlags & MediaInputPort::FLAG_BLOCK_OUTPUT) {
      MarkStreamBlocking(port->mDest);
    }
  }
  ProcessedMediaStream* ps = aStream->AsProcessedStream();
  if (ps) {
    for (uint32_t i = 0; i < ps->mInputs.Length(); ++i) {
      MediaInputPort* port = ps->mInputs[i];
      if (port->mFlags & MediaInputPort::FLAG_BLOCK_INPUT) {
        MarkStreamBlocking(port->mSource);
      }
    }
  }
}

void
MediaStreamGraphImpl::RecomputeBlockingAt(const nsTArray<MediaStream*>& aStreams,
                                          GraphTime aTime,
                                          GraphTime aEndBlockingDecisions,
                                          GraphTime* aEnd)
{
  for (uint32_t i = 0; i < aStreams.Length(); ++i) {
    MediaStream* stream = aStreams[i];
    stream->mBlockInThisPhase = false;
  }

  for (uint32_t i = 0; i < aStreams.Length(); ++i) {
    MediaStream* stream = aStreams[i];

    if (stream->mFinished) {
      GraphTime endTime = StreamTimeToGraphTime(stream,
         stream->GetStreamBuffer().GetAllTracksEnd());
      if (endTime <= aTime) {
        STREAM_LOG(PR_LOG_DEBUG+1, ("MediaStream %p is blocked due to being finished", stream));
        // We'll block indefinitely
        MarkStreamBlocking(stream);
        *aEnd = std::min(*aEnd, aEndBlockingDecisions);
        continue;
      } else {
        STREAM_LOG(PR_LOG_DEBUG+1, ("MediaStream %p is finished, but not blocked yet (end at %f, with blocking at %f)",
                                    stream, MediaTimeToSeconds(stream->GetBufferEnd()),
                                    MediaTimeToSeconds(endTime)));
        *aEnd = std::min(*aEnd, endTime);
      }
    }

    GraphTime end;
    bool explicitBlock = stream->mExplicitBlockerCount.GetAt(aTime, &end) > 0;
    *aEnd = std::min(*aEnd, end);
    if (explicitBlock) {
      STREAM_LOG(PR_LOG_DEBUG+1, ("MediaStream %p is blocked due to explicit blocker", stream));
      MarkStreamBlocking(stream);
      continue;
    }

    bool underrun = WillUnderrun(stream, aTime, aEndBlockingDecisions, aEnd);
    if (underrun) {
      // We'll block indefinitely
      MarkStreamBlocking(stream);
      *aEnd = std::min(*aEnd, aEndBlockingDecisions);
      continue;
    }
  }
  NS_ASSERTION(*aEnd > aTime, "Failed to advance!");

  for (uint32_t i = 0; i < aStreams.Length(); ++i) {
    MediaStream* stream = aStreams[i];
    stream->mBlocked.SetAtAndAfter(aTime, stream->mBlockInThisPhase);
  }
}

void
MediaStreamGraphImpl::NotifyHasCurrentData(MediaStream* aStream)
{
  if (!aStream->mNotifiedHasCurrentData && aStream->mHasCurrentData) {
    for (uint32_t j = 0; j < aStream->mListeners.Length(); ++j) {
      MediaStreamListener* l = aStream->mListeners[j];
      l->NotifyHasCurrentData(this);
    }
    aStream->mNotifiedHasCurrentData = true;
  }
}

void
MediaStreamGraphImpl::CreateOrDestroyAudioStreams(GraphTime aAudioOutputStartTime,
                                                  MediaStream* aStream)
{
  MOZ_ASSERT(mRealtime, "Should only attempt to create audio streams in real-time mode");

  nsAutoTArray<bool,2> audioOutputStreamsFound;
  for (uint32_t i = 0; i < aStream->mAudioOutputStreams.Length(); ++i) {
    audioOutputStreamsFound.AppendElement(false);
  }

  if (!aStream->mAudioOutputs.IsEmpty()) {
    for (StreamBuffer::TrackIter tracks(aStream->GetStreamBuffer(), MediaSegment::AUDIO);
         !tracks.IsEnded(); tracks.Next()) {
      uint32_t i;
      for (i = 0; i < audioOutputStreamsFound.Length(); ++i) {
        if (aStream->mAudioOutputStreams[i].mTrackID == tracks->GetID()) {
          break;
        }
      }
      if (i < audioOutputStreamsFound.Length()) {
        audioOutputStreamsFound[i] = true;
      } else {
        // No output stream created for this track yet. Check if it's time to
        // create one.
        GraphTime startTime =
          StreamTimeToGraphTime(aStream, tracks->GetStartTimeRoundDown(),
                                INCLUDE_TRAILING_BLOCKED_INTERVAL);
        if (startTime >= mStateComputedTime) {
          // The stream wants to play audio, but nothing will play for the forseeable
          // future, so don't create the stream.
          continue;
        }

        // Allocating a AudioStream would be slow, so we finish the Init async
        MediaStream::AudioOutputStream* audioOutputStream =
          aStream->mAudioOutputStreams.AppendElement();
        audioOutputStream->mAudioPlaybackStartTime = aAudioOutputStartTime;
        audioOutputStream->mBlockedAudioTime = 0;
        audioOutputStream->mLastTickWritten = 0;
        audioOutputStream->mStream = new AudioStream();
        // XXX for now, allocate stereo output. But we need to fix this to
        // match the system's ideal channel configuration.
        // NOTE: we presume this is either fast or async-under-the-covers
        audioOutputStream->mStream->Init(2, mSampleRate,
                                         aStream->mAudioChannelType,
                                         AudioStream::LowLatency);
        audioOutputStream->mTrackID = tracks->GetID();

        // If there is a mixer, there is a micrphone active.
        audioOutputStream->mStream->SetMicrophoneActive(mMixer);

        LogLatency(AsyncLatencyLogger::AudioStreamCreate,
                   reinterpret_cast<uint64_t>(aStream),
                   reinterpret_cast<int64_t>(audioOutputStream->mStream.get()));
      }
    }
  }

  for (int32_t i = audioOutputStreamsFound.Length() - 1; i >= 0; --i) {
    if (!audioOutputStreamsFound[i]) {
      aStream->mAudioOutputStreams[i].mStream->Shutdown();
      aStream->mAudioOutputStreams.RemoveElementAt(i);
    }
  }
}

TrackTicks
MediaStreamGraphImpl::PlayAudio(MediaStream* aStream,
                                GraphTime aFrom, GraphTime aTo)
{
  MOZ_ASSERT(mRealtime, "Should only attempt to play audio in realtime mode");

  TrackTicks ticksWritten = 0;
  // We compute the number of needed ticks by converting a difference of graph
  // time rather than by substracting two converted stream time to ensure that
  // the rounding between {Graph,Stream}Time and track ticks is not dependant
  // on the absolute value of the {Graph,Stream}Time, and so that number of
  // ticks to play is the same for each cycle.
  TrackTicks ticksNeeded = TimeToTicksRoundDown(mSampleRate, aTo) - TimeToTicksRoundDown(mSampleRate, aFrom);

  if (aStream->mAudioOutputStreams.IsEmpty()) {
    return 0;
  }

  // When we're playing multiple copies of this stream at the same time, they're
  // perfectly correlated so adding volumes is the right thing to do.
  float volume = 0.0f;
  for (uint32_t i = 0; i < aStream->mAudioOutputs.Length(); ++i) {
    volume += aStream->mAudioOutputs[i].mVolume;
  }

  for (uint32_t i = 0; i < aStream->mAudioOutputStreams.Length(); ++i) {
    MediaStream::AudioOutputStream& audioOutput = aStream->mAudioOutputStreams[i];
    StreamBuffer::Track* track = aStream->mBuffer.FindTrack(audioOutput.mTrackID);
    AudioSegment* audio = track->Get<AudioSegment>();
    AudioSegment output;
    MOZ_ASSERT(track->GetRate() == mSampleRate);

    // offset and audioOutput.mLastTickWritten can differ by at most one sample,
    // because of the rounding issue. We track that to ensure we don't skip a
    // sample. One sample may be played twice, but this should not happen
    // again during an unblocked sequence of track samples.
    TrackTicks offset = track->TimeToTicksRoundDown(GraphTimeToStreamTime(aStream, aFrom));
    if (audioOutput.mLastTickWritten &&
        audioOutput.mLastTickWritten != offset) {
      // If there is a global underrun of the MSG, this property won't hold, and
      // we reset the sample count tracking.
      if (offset - audioOutput.mLastTickWritten == 1) {
        offset = audioOutput.mLastTickWritten;
      }
    }

    // We don't update aStream->mBufferStartTime here to account for
    // time spent blocked. Instead, we'll update it in UpdateCurrentTime after the
    // blocked period has completed. But we do need to make sure we play from the
    // right offsets in the stream buffer, even if we've already written silence for
    // some amount of blocked time after the current time.
    GraphTime t = aFrom;
    while (ticksNeeded) {
      GraphTime end;
      bool blocked = aStream->mBlocked.GetAt(t, &end);
      end = std::min(end, aTo);

      // Check how many ticks of sound we can provide if we are blocked some
      // time in the middle of this cycle.
      TrackTicks toWrite = 0;
      if (end >= aTo) {
        toWrite = ticksNeeded;
      } else {
        toWrite = TimeToTicksRoundDown(mSampleRate, end - t);
      }
      ticksNeeded -= toWrite;

      if (blocked) {
        output.InsertNullDataAtStart(toWrite);
        STREAM_LOG(PR_LOG_DEBUG+1, ("MediaStream %p writing %ld blocking-silence samples for %f to %f (%ld to %ld)\n",
                                    aStream, toWrite, MediaTimeToSeconds(t), MediaTimeToSeconds(end),
                                    offset, offset + toWrite));
      } else {
        TrackTicks endTicksNeeded = offset + toWrite;
        TrackTicks endTicksAvailable = audio->GetDuration();
        STREAM_LOG(PR_LOG_DEBUG+1, ("MediaStream %p writing %ld samples for %f to %f (samples %ld to %ld)\n",
                                     aStream, toWrite, MediaTimeToSeconds(t), MediaTimeToSeconds(end),
                                     offset, endTicksNeeded));

        if (endTicksNeeded <= endTicksAvailable) {
          output.AppendSlice(*audio, offset, endTicksNeeded);
          offset = endTicksNeeded;
        } else {
          MOZ_ASSERT(track->IsEnded(), "Not enough data, and track not ended.");
          // If we are at the end of the track, maybe write the remaining
          // samples, and pad with/output silence.
          if (endTicksNeeded > endTicksAvailable &&
              offset < endTicksAvailable) {
            output.AppendSlice(*audio, offset, endTicksAvailable);
            toWrite -= endTicksAvailable - offset;
            offset = endTicksAvailable;
          }
          output.AppendNullData(toWrite);
        }
        output.ApplyVolume(volume);
      }
      t = end;
    }
    audioOutput.mLastTickWritten = offset;

    // Need unique id for stream & track - and we want it to match the inserter
    output.WriteTo(LATENCY_STREAM_ID(aStream, track->GetID()),
                   audioOutput.mStream, mMixer);
  }
  return ticksWritten;
}

static void
SetImageToBlackPixel(PlanarYCbCrImage* aImage)
{
  uint8_t blackPixel[] = { 0x10, 0x80, 0x80 };

  PlanarYCbCrData data;
  data.mYChannel = blackPixel;
  data.mCbChannel = blackPixel + 1;
  data.mCrChannel = blackPixel + 2;
  data.mYStride = data.mCbCrStride = 1;
  data.mPicSize = data.mYSize = data.mCbCrSize = IntSize(1, 1);
  aImage->SetData(data);
}

void
MediaStreamGraphImpl::PlayVideo(MediaStream* aStream)
{
  MOZ_ASSERT(mRealtime, "Should only attempt to play video in realtime mode");

  if (aStream->mVideoOutputs.IsEmpty())
    return;

  // Display the next frame a bit early. This is better than letting the current
  // frame be displayed for too long.
  GraphTime framePosition = mCurrentTime + MEDIA_GRAPH_TARGET_PERIOD_MS;
  NS_ASSERTION(framePosition >= aStream->mBufferStartTime, "frame position before buffer?");
  StreamTime frameBufferTime = GraphTimeToStreamTime(aStream, framePosition);

  TrackTicks start;
  const VideoFrame* frame = nullptr;
  StreamBuffer::Track* track;
  for (StreamBuffer::TrackIter tracks(aStream->GetStreamBuffer(), MediaSegment::VIDEO);
       !tracks.IsEnded(); tracks.Next()) {
    VideoSegment* segment = tracks->Get<VideoSegment>();
    TrackTicks thisStart;
    const VideoFrame* thisFrame =
      segment->GetFrameAt(tracks->TimeToTicksRoundDown(frameBufferTime), &thisStart);
    if (thisFrame && thisFrame->GetImage()) {
      start = thisStart;
      frame = thisFrame;
      track = tracks.get();
    }
  }
  if (!frame || *frame == aStream->mLastPlayedVideoFrame)
    return;

  STREAM_LOG(PR_LOG_DEBUG+1, ("MediaStream %p writing video frame %p (%dx%d)",
                              aStream, frame->GetImage(), frame->GetIntrinsicSize().width,
                              frame->GetIntrinsicSize().height));
  GraphTime startTime = StreamTimeToGraphTime(aStream,
      track->TicksToTimeRoundDown(start), INCLUDE_TRAILING_BLOCKED_INTERVAL);
  TimeStamp targetTime = mCurrentTimeStamp +
      TimeDuration::FromMilliseconds(double(startTime - mCurrentTime));
  for (uint32_t i = 0; i < aStream->mVideoOutputs.Length(); ++i) {
    VideoFrameContainer* output = aStream->mVideoOutputs[i];

    if (frame->GetForceBlack()) {
      nsRefPtr<Image> image =
        output->GetImageContainer()->CreateImage(ImageFormat::PLANAR_YCBCR);
      if (image) {
        // Sets the image to a single black pixel, which will be scaled to fill
        // the rendered size.
        SetImageToBlackPixel(static_cast<PlanarYCbCrImage*>(image.get()));
      }
      output->SetCurrentFrame(frame->GetIntrinsicSize(), image,
                              targetTime);
    } else {
      output->SetCurrentFrame(frame->GetIntrinsicSize(), frame->GetImage(),
                              targetTime);
    }

    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(output, &VideoFrameContainer::Invalidate);
    NS_DispatchToMainThread(event);
  }
  if (!aStream->mNotifiedFinished) {
    aStream->mLastPlayedVideoFrame = *frame;
  }
}

bool
MediaStreamGraphImpl::ShouldUpdateMainThread()
{
  if (mRealtime) {
    return true;
  }

  TimeStamp now = TimeStamp::Now();
  if ((now - mLastMainThreadUpdate).ToMilliseconds() > MEDIA_GRAPH_TARGET_PERIOD_MS) {
    mLastMainThreadUpdate = now;
    return true;
  }
  return false;
}

void
MediaStreamGraphImpl::PrepareUpdatesToMainThreadState(bool aFinalUpdate)
{
  mMonitor.AssertCurrentThreadOwns();

  // We don't want to frequently update the main thread about timing update
  // when we are not running in realtime.
  if (aFinalUpdate || ShouldUpdateMainThread()) {
    mStreamUpdates.SetCapacity(mStreamUpdates.Length() + mStreams.Length());
    for (uint32_t i = 0; i < mStreams.Length(); ++i) {
      MediaStream* stream = mStreams[i];
      if (!stream->MainThreadNeedsUpdates()) {
        continue;
      }
      StreamUpdate* update = mStreamUpdates.AppendElement();
      update->mGraphUpdateIndex = stream->mGraphUpdateIndices.GetAt(mCurrentTime);
      update->mStream = stream;
      update->mNextMainThreadCurrentTime =
        GraphTimeToStreamTime(stream, mCurrentTime);
      update->mNextMainThreadFinished = stream->mNotifiedFinished;
    }
    if (!mPendingUpdateRunnables.IsEmpty()) {
      mUpdateRunnables.MoveElementsFrom(mPendingUpdateRunnables);
    }
  }

  // Don't send the message to the main thread if it's not going to have
  // any work to do.
  if (aFinalUpdate ||
      !mUpdateRunnables.IsEmpty() ||
      !mStreamUpdates.IsEmpty()) {
    EnsureStableStateEventPosted();
  }
}

void
MediaStreamGraphImpl::EnsureImmediateWakeUpLocked(MonitorAutoLock& aLock)
{
  if (mWaitState == WAITSTATE_WAITING_FOR_NEXT_ITERATION ||
      mWaitState == WAITSTATE_WAITING_INDEFINITELY) {
    mWaitState = WAITSTATE_WAKING_UP;
    aLock.Notify();
  }
}

void
MediaStreamGraphImpl::EnsureNextIteration()
{
  MonitorAutoLock lock(mMonitor);
  EnsureNextIterationLocked(lock);
}

void
MediaStreamGraphImpl::EnsureNextIterationLocked(MonitorAutoLock& aLock)
{
  if (mNeedAnotherIteration)
    return;
  mNeedAnotherIteration = true;
  if (mWaitState == WAITSTATE_WAITING_INDEFINITELY) {
    mWaitState = WAITSTATE_WAKING_UP;
    aLock.Notify();
  }
}

/**
 * Returns smallest value of t such that
 * TimeToTicksRoundUp(aSampleRate, t) is a multiple of WEBAUDIO_BLOCK_SIZE
 * and floor(TimeToTicksRoundUp(aSampleRate, t)/WEBAUDIO_BLOCK_SIZE) >
 * floor(TimeToTicksRoundUp(aSampleRate, aTime)/WEBAUDIO_BLOCK_SIZE).
 */
static GraphTime
RoundUpToNextAudioBlock(TrackRate aSampleRate, GraphTime aTime)
{
  TrackTicks ticks = aTime;
  uint64_t block = ticks >> WEBAUDIO_BLOCK_SIZE_BITS;
  uint64_t nextBlock = block + 1;
  TrackTicks nextTicks = nextBlock << WEBAUDIO_BLOCK_SIZE_BITS;
  return nextTicks;
}

void
MediaStreamGraphImpl::ProduceDataForStreamsBlockByBlock(uint32_t aStreamIndex,
                                                        TrackRate aSampleRate,
                                                        GraphTime aFrom,
                                                        GraphTime aTo)
{
  MOZ_ASSERT(aStreamIndex <= mFirstCycleBreaker,
             "Cycle breaker is not AudioNodeStream?");
  GraphTime t = aFrom;
  while (t < aTo) {
    GraphTime next = RoundUpToNextAudioBlock(aSampleRate, t);
    for (uint32_t i = mFirstCycleBreaker; i < mStreams.Length(); ++i) {
      auto ns = static_cast<AudioNodeStream*>(mStreams[i]);
      MOZ_ASSERT(ns->AsAudioNodeStream());
      ns->ProduceOutputBeforeInput(t);
    }
    for (uint32_t i = aStreamIndex; i < mStreams.Length(); ++i) {
      ProcessedMediaStream* ps = mStreams[i]->AsProcessedStream();
      if (ps) {
        ps->ProcessInput(t, next, (next == aTo) ? ProcessedMediaStream::ALLOW_FINISH : 0);
      }
    }
    t = next;
  }
  NS_ASSERTION(t == aTo, "Something went wrong with rounding to block boundaries");
}

bool
MediaStreamGraphImpl::AllFinishedStreamsNotified()
{
  for (uint32_t i = 0; i < mStreams.Length(); ++i) {
    MediaStream* s = mStreams[i];
    if (s->mFinished && !s->mNotifiedFinished) {
      return false;
    }
  }
  return true;
}

void
MediaStreamGraphImpl::PauseAllAudioOutputs()
{
  for (uint32_t i = 0; i < mStreams.Length(); ++i) {
    MediaStream* s = mStreams[i];
    for (uint32_t j = 0; j < s->mAudioOutputStreams.Length(); ++j) {
      s->mAudioOutputStreams[j].mStream->Pause();
    }
  }
}

void
MediaStreamGraphImpl::ResumeAllAudioOutputs()
{
  for (uint32_t i = 0; i < mStreams.Length(); ++i) {
    MediaStream* s = mStreams[i];
    for (uint32_t j = 0; j < s->mAudioOutputStreams.Length(); ++j) {
      s->mAudioOutputStreams[j].mStream->Resume();
    }
  }
}

struct AutoProfilerUnregisterThread
{
  // The empty ctor is used to silence a pre-4.8.0 GCC unused variable warning.
  AutoProfilerUnregisterThread()
  {
  }

  ~AutoProfilerUnregisterThread()
  {
    profiler_unregister_thread();
  }
};

void
MediaStreamGraphImpl::RunThread()
{
  nsTArray<MessageBlock> messageQueue;
  {
    MonitorAutoLock lock(mMonitor);
    messageQueue.SwapElements(mMessageQueue);
  }
  NS_ASSERTION(!messageQueue.IsEmpty(),
               "Shouldn't have started a graph with empty message queue!");

  uint32_t ticksProcessed = 0;
  AutoProfilerUnregisterThread autoUnregister;

  for (;;) {
    // Check if a memory report has been requested.
    {
      MonitorAutoLock lock(mMemoryReportMonitor);
      if (mNeedsMemoryReport) {
        mNeedsMemoryReport = false;

        for (uint32_t i = 0; i < mStreams.Length(); ++i) {
          AudioNodeStream* stream = mStreams[i]->AsAudioNodeStream();
          if (stream) {
            AudioNodeSizes usage;
            stream->SizeOfAudioNodesIncludingThis(MallocSizeOf, usage);
            mAudioStreamSizes.AppendElement(usage);
          }
        }

        lock.Notify();
      }
    }

    // Update mCurrentTime to the min of the playing audio times, or using the
    // wall-clock time change if no audio is playing.
    UpdateCurrentTime();

    // Calculate independent action times for each batch of messages (each
    // batch corresponding to an event loop task). This isolates the performance
    // of different scripts to some extent.
    for (uint32_t i = 0; i < messageQueue.Length(); ++i) {
      mProcessingGraphUpdateIndex = messageQueue[i].mGraphUpdateIndex;
      nsTArray<nsAutoPtr<ControlMessage> >& messages = messageQueue[i].mMessages;

      for (uint32_t j = 0; j < messages.Length(); ++j) {
        messages[j]->Run();
      }
    }
    messageQueue.Clear();

    if (mStreamOrderDirty) {
      UpdateStreamOrder();
    }

    GraphTime endBlockingDecisions =
      RoundUpToNextAudioBlock(mSampleRate, mCurrentTime + MillisecondsToMediaTime(AUDIO_TARGET_MS));
    bool ensureNextIteration = false;

    // Grab pending stream input.
    for (uint32_t i = 0; i < mStreams.Length(); ++i) {
      SourceMediaStream* is = mStreams[i]->AsSourceStream();
      if (is) {
        UpdateConsumptionState(is);
        ExtractPendingInput(is, endBlockingDecisions, &ensureNextIteration);
      }
    }

    // The loop is woken up so soon that mCurrentTime barely advances and we
    // end up having endBlockingDecisions == mStateComputedTime.
    // Since stream blocking is computed in the interval of
    // [mStateComputedTime, endBlockingDecisions), it won't be computed at all.
    // We should ensure next iteration so that pending blocking changes will be
    // computed in next loop.
    if (endBlockingDecisions == mStateComputedTime) {
      ensureNextIteration = true;
    }

    // Figure out which streams are blocked and when.
    GraphTime prevComputedTime = mStateComputedTime;
    RecomputeBlocking(endBlockingDecisions);

    // Play stream contents.
    bool allBlockedForever = true;
    // True when we've done ProcessInput for all processed streams.
    bool doneAllProducing = false;
    // This is the number of frame that are written to the AudioStreams, for
    // this cycle.
    TrackTicks ticksPlayed = 0;
    // Figure out what each stream wants to do
    for (uint32_t i = 0; i < mStreams.Length(); ++i) {
      MediaStream* stream = mStreams[i];
      if (!doneAllProducing) {
        ProcessedMediaStream* ps = stream->AsProcessedStream();
        if (ps) {
          AudioNodeStream* n = stream->AsAudioNodeStream();
          if (n) {
#ifdef DEBUG
            // Verify that the sampling rate for all of the following streams is the same
            for (uint32_t j = i + 1; j < mStreams.Length(); ++j) {
              AudioNodeStream* nextStream = mStreams[j]->AsAudioNodeStream();
              if (nextStream) {
                MOZ_ASSERT(n->SampleRate() == nextStream->SampleRate(),
                           "All AudioNodeStreams in the graph must have the same sampling rate");
              }
            }
#endif
            // Since an AudioNodeStream is present, go ahead and
            // produce audio block by block for all the rest of the streams.
            ProduceDataForStreamsBlockByBlock(i, n->SampleRate(), prevComputedTime, mStateComputedTime);
            ticksProcessed += TimeToTicksRoundDown(n->SampleRate(), mStateComputedTime - prevComputedTime);
            doneAllProducing = true;
          } else {
            ps->ProcessInput(prevComputedTime, mStateComputedTime,
                             ProcessedMediaStream::ALLOW_FINISH);
            NS_WARN_IF_FALSE(stream->mBuffer.GetEnd() >=
                             GraphTimeToStreamTime(stream, mStateComputedTime),
                             "Stream did not produce enough data");
          }
        }
      }
      NotifyHasCurrentData(stream);
      if (mRealtime) {
        // Only playback audio and video in real-time mode
        CreateOrDestroyAudioStreams(prevComputedTime, stream);
        TrackTicks ticksPlayedForThisStream = PlayAudio(stream, prevComputedTime, mStateComputedTime);
        if (!ticksPlayed) {
          ticksPlayed = ticksPlayedForThisStream;
        } else {
          MOZ_ASSERT(!ticksPlayedForThisStream || ticksPlayedForThisStream == ticksPlayed,
              "Each stream should have the same number of frame.");
        }
        PlayVideo(stream);
      }
      SourceMediaStream* is = stream->AsSourceStream();
      if (is) {
        UpdateBufferSufficiencyState(is);
      }
      GraphTime end;
      if (!stream->mBlocked.GetAt(mCurrentTime, &end) || end < GRAPH_TIME_MAX) {
        allBlockedForever = false;
      }
    }

    if (mMixer) {
      mMixer->FinishMixing();
    }

    if (ensureNextIteration || !allBlockedForever) {
      EnsureNextIteration();
    }

    // Send updates to the main thread and wait for the next control loop
    // iteration.
    {
      MonitorAutoLock lock(mMonitor);
      bool finalUpdate = mForceShutDown ||
        (mCurrentTime >= mEndTime && AllFinishedStreamsNotified()) ||
        (IsEmpty() && mMessageQueue.IsEmpty());
      PrepareUpdatesToMainThreadState(finalUpdate);
      if (finalUpdate) {
        // Enter shutdown mode. The stable-state handler will detect this
        // and complete shutdown. Destroy any streams immediately.
        STREAM_LOG(PR_LOG_DEBUG, ("MediaStreamGraph %p waiting for main thread cleanup", this));
        // We'll shut down this graph object if it does not get restarted.
        mLifecycleState = LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP;
        // No need to Destroy streams here. The main-thread owner of each
        // stream is responsible for calling Destroy on them.
        return;
      }

      // No need to wait in non-realtime mode, just churn through the input as soon
      // as possible.
      if (mRealtime) {
        PRIntervalTime timeout = PR_INTERVAL_NO_TIMEOUT;
        TimeStamp now = TimeStamp::Now();
        bool pausedOutputs = false;
        if (mNeedAnotherIteration) {
          int64_t timeoutMS = MEDIA_GRAPH_TARGET_PERIOD_MS -
            int64_t((now - mCurrentTimeStamp).ToMilliseconds());
          // Make sure timeoutMS doesn't overflow 32 bits by waking up at
          // least once a minute, if we need to wake up at all
          timeoutMS = std::max<int64_t>(0, std::min<int64_t>(timeoutMS, 60*1000));
          timeout = PR_MillisecondsToInterval(uint32_t(timeoutMS));
          STREAM_LOG(PR_LOG_DEBUG+1, ("Waiting for next iteration; at %f, timeout=%f",
                                     (now - mInitialTimeStamp).ToSeconds(), timeoutMS/1000.0));
          mWaitState = WAITSTATE_WAITING_FOR_NEXT_ITERATION;
        } else {
          mWaitState = WAITSTATE_WAITING_INDEFINITELY;
          PauseAllAudioOutputs();
          pausedOutputs = true;
        }
        if (timeout > 0) {
          mMonitor.Wait(timeout);
          STREAM_LOG(PR_LOG_DEBUG+1, ("Resuming after timeout; at %f, elapsed=%f",
                                     (TimeStamp::Now() - mInitialTimeStamp).ToSeconds(),
                                     (TimeStamp::Now() - now).ToSeconds()));
        }
        if (pausedOutputs) {
          ResumeAllAudioOutputs();
        }
      }
      mWaitState = WAITSTATE_RUNNING;
      mNeedAnotherIteration = false;
      messageQueue.SwapElements(mMessageQueue);
    }
  }
}

void
MediaStreamGraphImpl::ApplyStreamUpdate(StreamUpdate* aUpdate)
{
  mMonitor.AssertCurrentThreadOwns();

  MediaStream* stream = aUpdate->mStream;
  if (!stream)
    return;
  stream->mMainThreadCurrentTime = aUpdate->mNextMainThreadCurrentTime;
  stream->mMainThreadFinished = aUpdate->mNextMainThreadFinished;

  if (stream->mWrapper) {
    stream->mWrapper->NotifyStreamStateChanged();
  }
  for (int32_t i = stream->mMainThreadListeners.Length() - 1; i >= 0; --i) {
    stream->mMainThreadListeners[i]->NotifyMainThreadStateChanged();
  }
}

void
MediaStreamGraphImpl::ShutdownThreads()
{
  NS_ASSERTION(NS_IsMainThread(), "Must be called on main thread");
  // mGraph's thread is not running so it's OK to do whatever here
  STREAM_LOG(PR_LOG_DEBUG, ("Stopping threads for MediaStreamGraph %p", this));

  if (mThread) {
    mThread->Shutdown();
    mThread = nullptr;
  }
}

void
MediaStreamGraphImpl::ForceShutDown()
{
  NS_ASSERTION(NS_IsMainThread(), "Must be called on main thread");
  STREAM_LOG(PR_LOG_DEBUG, ("MediaStreamGraph %p ForceShutdown", this));
  {
    MonitorAutoLock lock(mMonitor);
    mForceShutDown = true;
    EnsureImmediateWakeUpLocked(lock);
  }
}

namespace {

class MediaStreamGraphInitThreadRunnable : public nsRunnable {
public:
  explicit MediaStreamGraphInitThreadRunnable(MediaStreamGraphImpl* aGraph)
    : mGraph(aGraph)
  {
  }
  NS_IMETHOD Run()
  {
    char aLocal;
    profiler_register_thread("MediaStreamGraph", &aLocal);
    mGraph->RunThread();
    return NS_OK;
  }
private:
  MediaStreamGraphImpl* mGraph;
};

class MediaStreamGraphThreadRunnable : public nsRunnable {
public:
  explicit MediaStreamGraphThreadRunnable(MediaStreamGraphImpl* aGraph)
    : mGraph(aGraph)
  {
  }
  NS_IMETHOD Run()
  {
    mGraph->RunThread();
    return NS_OK;
  }
private:
  MediaStreamGraphImpl* mGraph;
};

class MediaStreamGraphShutDownRunnable : public nsRunnable {
public:
  MediaStreamGraphShutDownRunnable(MediaStreamGraphImpl* aGraph) : mGraph(aGraph) {}
  NS_IMETHOD Run()
  {
    NS_ASSERTION(mGraph->mDetectedNotRunning,
                 "We should know the graph thread control loop isn't running!");

    mGraph->ShutdownThreads();

    // mGraph's thread is not running so it's OK to do whatever here
    if (mGraph->IsEmpty()) {
      // mGraph is no longer needed, so delete it.
      mGraph->Destroy();
    } else {
      // The graph is not empty.  We must be in a forced shutdown, or a
      // non-realtime graph that has finished processing.  Some later
      // AppendMessage will detect that the manager has been emptied, and
      // delete it.
      NS_ASSERTION(mGraph->mForceShutDown || !mGraph->mRealtime,
                   "Not in forced shutdown?");
      for (uint32_t i = 0; i < mGraph->mStreams.Length(); ++i) {
        DOMMediaStream* s = mGraph->mStreams[i]->GetWrapper();
        if (s) {
          s->NotifyMediaStreamGraphShutdown();
        }
      }

      mGraph->mLifecycleState =
        MediaStreamGraphImpl::LIFECYCLE_WAITING_FOR_STREAM_DESTRUCTION;
    }
    return NS_OK;
  }
private:
  MediaStreamGraphImpl* mGraph;
};

class MediaStreamGraphStableStateRunnable : public nsRunnable {
public:
  explicit MediaStreamGraphStableStateRunnable(MediaStreamGraphImpl* aGraph)
    : mGraph(aGraph)
  {
  }
  NS_IMETHOD Run()
  {
    if (mGraph) {
      mGraph->RunInStableState();
    }
    return NS_OK;
  }
private:
  MediaStreamGraphImpl* mGraph;
};

/*
 * Control messages forwarded from main thread to graph manager thread
 */
class CreateMessage : public ControlMessage {
public:
  CreateMessage(MediaStream* aStream) : ControlMessage(aStream) {}
  virtual void Run() MOZ_OVERRIDE
  {
    mStream->GraphImpl()->AddStream(mStream);
    mStream->Init();
  }
  virtual void RunDuringShutdown() MOZ_OVERRIDE
  {
    // Make sure to run this message during shutdown too, to make sure
    // that we balance the number of streams registered with the graph
    // as they're destroyed during shutdown.
    Run();
  }
};

class MediaStreamGraphShutdownObserver MOZ_FINAL : public nsIObserver
{
  ~MediaStreamGraphShutdownObserver() {}
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

}

void
MediaStreamGraphImpl::RunInStableState()
{
  NS_ASSERTION(NS_IsMainThread(), "Must be called on main thread");

  nsTArray<nsCOMPtr<nsIRunnable> > runnables;
  // When we're doing a forced shutdown, pending control messages may be
  // run on the main thread via RunDuringShutdown. Those messages must
  // run without the graph monitor being held. So, we collect them here.
  nsTArray<nsAutoPtr<ControlMessage> > controlMessagesToRunDuringShutdown;

  {
    MonitorAutoLock lock(mMonitor);
    mPostedRunInStableStateEvent = false;

    runnables.SwapElements(mUpdateRunnables);
    for (uint32_t i = 0; i < mStreamUpdates.Length(); ++i) {
      StreamUpdate* update = &mStreamUpdates[i];
      if (update->mStream) {
        ApplyStreamUpdate(update);
      }
    }
    mStreamUpdates.Clear();

    // Don't start the thread for a non-realtime graph until it has been
    // explicitly started by StartNonRealtimeProcessing.
    if (mLifecycleState == LIFECYCLE_THREAD_NOT_STARTED &&
        (mRealtime || mNonRealtimeProcessing)) {
      mLifecycleState = LIFECYCLE_RUNNING;
      // Start the thread now. We couldn't start it earlier because
      // the graph might exit immediately on finding it has no streams. The
      // first message for a new graph must create a stream.
      nsCOMPtr<nsIRunnable> event = new MediaStreamGraphInitThreadRunnable(this);
      NS_NewNamedThread("MediaStreamGrph", getter_AddRefs(mThread), event);
    }

    if (mCurrentTaskMessageQueue.IsEmpty()) {
      if (mLifecycleState == LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP && IsEmpty()) {
        // Complete shutdown. First, ensure that this graph is no longer used.
        // A new graph graph will be created if one is needed.
        STREAM_LOG(PR_LOG_DEBUG, ("Disconnecting MediaStreamGraph %p", this));
        if (this == gGraph) {
          // null out gGraph if that's the graph being shut down
          gGraph = nullptr;
        }
        // Asynchronously clean up old graph. We don't want to do this
        // synchronously because it spins the event loop waiting for threads
        // to shut down, and we don't want to do that in a stable state handler.
        mLifecycleState = LIFECYCLE_WAITING_FOR_THREAD_SHUTDOWN;
        nsCOMPtr<nsIRunnable> event = new MediaStreamGraphShutDownRunnable(this);
        NS_DispatchToMainThread(event);
      }
    } else {
      if (mLifecycleState <= LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP) {
        MessageBlock* block = mMessageQueue.AppendElement();
        block->mMessages.SwapElements(mCurrentTaskMessageQueue);
        block->mGraphUpdateIndex = mNextGraphUpdateIndex;
        ++mNextGraphUpdateIndex;
        EnsureNextIterationLocked(lock);
      }

      // If the MediaStreamGraph has more messages going to it, try to revive
      // it to process those messages. Don't do this if we're in a forced
      // shutdown or it's a non-realtime graph that has already terminated
      // processing.
      if (mLifecycleState == LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP &&
          mRealtime && !mForceShutDown) {
        mLifecycleState = LIFECYCLE_RUNNING;
        // Revive the MediaStreamGraph since we have more messages going to it.
        // Note that we need to put messages into its queue before reviving it,
        // or it might exit immediately.
        nsCOMPtr<nsIRunnable> event = new MediaStreamGraphThreadRunnable(this);
        mThread->Dispatch(event, 0);
      }
    }

    if ((mForceShutDown || !mRealtime) &&
        mLifecycleState == LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP) {
      // Defer calls to RunDuringShutdown() to happen while mMonitor is not held.
      for (uint32_t i = 0; i < mMessageQueue.Length(); ++i) {
        MessageBlock& mb = mMessageQueue[i];
        controlMessagesToRunDuringShutdown.MoveElementsFrom(mb.mMessages);
      }
      mMessageQueue.Clear();
      MOZ_ASSERT(mCurrentTaskMessageQueue.IsEmpty());
      // Stop MediaStreamGraph threads. Do not clear gGraph since
      // we have outstanding DOM objects that may need it.
      mLifecycleState = LIFECYCLE_WAITING_FOR_THREAD_SHUTDOWN;
      nsCOMPtr<nsIRunnable> event = new MediaStreamGraphShutDownRunnable(this);
      NS_DispatchToMainThread(event);
    }

    mDetectedNotRunning = mLifecycleState > LIFECYCLE_RUNNING;
  }

  // Make sure we get a new current time in the next event loop task
  mPostedRunInStableState = false;

  for (uint32_t i = 0; i < runnables.Length(); ++i) {
    runnables[i]->Run();
  }
  for (uint32_t i = 0; i < controlMessagesToRunDuringShutdown.Length(); ++i) {
    controlMessagesToRunDuringShutdown[i]->RunDuringShutdown();
  }

#ifdef DEBUG
  mCanRunMessagesSynchronously = mDetectedNotRunning &&
    mLifecycleState >= LIFECYCLE_WAITING_FOR_THREAD_SHUTDOWN;
#endif
}

static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

void
MediaStreamGraphImpl::EnsureRunInStableState()
{
  NS_ASSERTION(NS_IsMainThread(), "main thread only");

  if (mPostedRunInStableState)
    return;
  mPostedRunInStableState = true;
  nsCOMPtr<nsIRunnable> event = new MediaStreamGraphStableStateRunnable(this);
  nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
  if (appShell) {
    appShell->RunInStableState(event);
  } else {
    NS_ERROR("Appshell already destroyed?");
  }
}

void
MediaStreamGraphImpl::EnsureStableStateEventPosted()
{
  mMonitor.AssertCurrentThreadOwns();

  if (mPostedRunInStableStateEvent)
    return;
  mPostedRunInStableStateEvent = true;
  nsCOMPtr<nsIRunnable> event = new MediaStreamGraphStableStateRunnable(this);
  NS_DispatchToMainThread(event);
}

void
MediaStreamGraphImpl::AppendMessage(ControlMessage* aMessage)
{
  NS_ASSERTION(NS_IsMainThread(), "main thread only");
  NS_ASSERTION(!aMessage->GetStream() ||
               !aMessage->GetStream()->IsDestroyed(),
               "Stream already destroyed");

  if (mDetectedNotRunning &&
      mLifecycleState > LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP) {
    // The graph control loop is not running and main thread cleanup has
    // happened. From now on we can't append messages to mCurrentTaskMessageQueue,
    // because that will never be processed again, so just RunDuringShutdown
    // this message.
    // This should only happen during forced shutdown, or after a non-realtime
    // graph has finished processing.
#ifdef DEBUG
    MOZ_ASSERT(mCanRunMessagesSynchronously);
    mCanRunMessagesSynchronously = false;
#endif
    aMessage->RunDuringShutdown();
#ifdef DEBUG
    mCanRunMessagesSynchronously = true;
#endif
    delete aMessage;
    if (IsEmpty() &&
        mLifecycleState >= LIFECYCLE_WAITING_FOR_STREAM_DESTRUCTION) {
      if (gGraph == this) {
        gGraph = nullptr;
      }
      Destroy();
    }
    return;
  }

  mCurrentTaskMessageQueue.AppendElement(aMessage);
  EnsureRunInStableState();
}

MediaStream::MediaStream(DOMMediaStream* aWrapper)
  : mBufferStartTime(0)
  , mExplicitBlockerCount(0)
  , mBlocked(false)
  , mGraphUpdateIndices(0)
  , mFinished(false)
  , mNotifiedFinished(false)
  , mNotifiedBlocked(false)
  , mHasCurrentData(false)
  , mNotifiedHasCurrentData(false)
  , mWrapper(aWrapper)
  , mMainThreadCurrentTime(0)
  , mMainThreadFinished(false)
  , mMainThreadDestroyed(false)
  , mGraph(nullptr)
  , mAudioChannelType(dom::AudioChannel::Normal)
{
  MOZ_COUNT_CTOR(MediaStream);
  // aWrapper should not already be connected to a MediaStream! It needs
  // to be hooked up to this stream, and since this stream is only just
  // being created now, aWrapper must not be connected to anything.
  NS_ASSERTION(!aWrapper || !aWrapper->GetStream(),
               "Wrapper already has another media stream hooked up to it!");
}

size_t
MediaStream::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t amount = 0;

  // Not owned:
  // - mGraph - Not reported here
  // - mConsumers - elements
  // Future:
  // - mWrapper
  // - mVideoOutputs - elements
  // - mLastPlayedVideoFrame
  // - mListeners - elements
  // - mAudioOutputStreams - elements

  amount += mBuffer.SizeOfExcludingThis(aMallocSizeOf);
  amount += mAudioOutputs.SizeOfExcludingThis(aMallocSizeOf);
  amount += mVideoOutputs.SizeOfExcludingThis(aMallocSizeOf);
  amount += mExplicitBlockerCount.SizeOfExcludingThis(aMallocSizeOf);
  amount += mListeners.SizeOfExcludingThis(aMallocSizeOf);
  amount += mMainThreadListeners.SizeOfExcludingThis(aMallocSizeOf);
  amount += mDisabledTrackIDs.SizeOfExcludingThis(aMallocSizeOf);
  amount += mBlocked.SizeOfExcludingThis(aMallocSizeOf);
  amount += mGraphUpdateIndices.SizeOfExcludingThis(aMallocSizeOf);
  amount += mConsumers.SizeOfExcludingThis(aMallocSizeOf);
  amount += mAudioOutputStreams.SizeOfExcludingThis(aMallocSizeOf);
  for (size_t i = 0; i < mAudioOutputStreams.Length(); i++) {
    amount += mAudioOutputStreams[i].SizeOfExcludingThis(aMallocSizeOf);
  }

  return amount;
}

size_t
MediaStream::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

void
MediaStream::Init()
{
  MediaStreamGraphImpl* graph = GraphImpl();
  mBlocked.SetAtAndAfter(graph->mCurrentTime, true);
  mExplicitBlockerCount.SetAtAndAfter(graph->mCurrentTime, true);
  mExplicitBlockerCount.SetAtAndAfter(graph->mStateComputedTime, false);
}

MediaStreamGraphImpl*
MediaStream::GraphImpl()
{
  return mGraph;
}

MediaStreamGraph*
MediaStream::Graph()
{
  return mGraph;
}

void
MediaStream::SetGraphImpl(MediaStreamGraphImpl* aGraph)
{
  MOZ_ASSERT(!mGraph, "Should only be called once");
  mGraph = aGraph;
  mBuffer.InitGraphRate(aGraph->GraphRate());
}

void
MediaStream::SetGraphImpl(MediaStreamGraph* aGraph)
{
  MediaStreamGraphImpl* graph = static_cast<MediaStreamGraphImpl*>(aGraph);
  SetGraphImpl(graph);
}

StreamTime
MediaStream::GraphTimeToStreamTime(GraphTime aTime)
{
  return GraphImpl()->GraphTimeToStreamTime(this, aTime);
}

StreamTime
MediaStream::GraphTimeToStreamTimeOptimistic(GraphTime aTime)
{
  return GraphImpl()->GraphTimeToStreamTimeOptimistic(this, aTime);
}

GraphTime
MediaStream::StreamTimeToGraphTime(StreamTime aTime)
{
  return GraphImpl()->StreamTimeToGraphTime(this, aTime, 0);
}

void
MediaStream::FinishOnGraphThread()
{
  GraphImpl()->FinishStream(this);
}

int64_t
MediaStream::GetProcessingGraphUpdateIndex()
{
  return GraphImpl()->GetProcessingGraphUpdateIndex();
}

StreamBuffer::Track*
MediaStream::EnsureTrack(TrackID aTrackId, TrackRate aSampleRate)
{
  StreamBuffer::Track* track = mBuffer.FindTrack(aTrackId);
  if (!track) {
    nsAutoPtr<MediaSegment> segment(new AudioSegment());
    for (uint32_t j = 0; j < mListeners.Length(); ++j) {
      MediaStreamListener* l = mListeners[j];
      l->NotifyQueuedTrackChanges(Graph(), aTrackId,
                                  GraphImpl()->AudioSampleRate(), 0,
                                  MediaStreamListener::TRACK_EVENT_CREATED,
                                  *segment);
    }
    track = &mBuffer.AddTrack(aTrackId, aSampleRate, 0, segment.forget());
  }
  return track;
}

void
MediaStream::RemoveAllListenersImpl()
{
  for (int32_t i = mListeners.Length() - 1; i >= 0; --i) {
    nsRefPtr<MediaStreamListener> listener = mListeners[i].forget();
    listener->NotifyEvent(GraphImpl(), MediaStreamListener::EVENT_REMOVED);
  }
  mListeners.Clear();
}

void
MediaStream::DestroyImpl()
{
  for (int32_t i = mConsumers.Length() - 1; i >= 0; --i) {
    mConsumers[i]->Disconnect();
  }
  for (uint32_t i = 0; i < mAudioOutputStreams.Length(); ++i) {
    mAudioOutputStreams[i].mStream->Shutdown();
  }
  mAudioOutputStreams.Clear();
  mGraph = nullptr;
}

void
MediaStream::Destroy()
{
  // Keep this stream alive until we leave this method
  nsRefPtr<MediaStream> kungFuDeathGrip = this;

  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream) : ControlMessage(aStream) {}
    virtual void Run()
    {
      mStream->RemoveAllListenersImpl();
      auto graph = mStream->GraphImpl();
      mStream->DestroyImpl();
      graph->RemoveStream(mStream);
    }
    virtual void RunDuringShutdown()
    { Run(); }
  };
  mWrapper = nullptr;
  GraphImpl()->AppendMessage(new Message(this));
  // Message::RunDuringShutdown may have removed this stream from the graph,
  // but our kungFuDeathGrip above will have kept this stream alive if
  // necessary.
  mMainThreadDestroyed = true;
}

void
MediaStream::AddAudioOutput(void* aKey)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, void* aKey) : ControlMessage(aStream), mKey(aKey) {}
    virtual void Run()
    {
      mStream->AddAudioOutputImpl(mKey);
    }
    void* mKey;
  };
  GraphImpl()->AppendMessage(new Message(this, aKey));
}

void
MediaStream::SetAudioOutputVolumeImpl(void* aKey, float aVolume)
{
  for (uint32_t i = 0; i < mAudioOutputs.Length(); ++i) {
    if (mAudioOutputs[i].mKey == aKey) {
      mAudioOutputs[i].mVolume = aVolume;
      return;
    }
  }
  NS_ERROR("Audio output key not found");
}

void
MediaStream::SetAudioOutputVolume(void* aKey, float aVolume)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, void* aKey, float aVolume) :
      ControlMessage(aStream), mKey(aKey), mVolume(aVolume) {}
    virtual void Run()
    {
      mStream->SetAudioOutputVolumeImpl(mKey, mVolume);
    }
    void* mKey;
    float mVolume;
  };
  GraphImpl()->AppendMessage(new Message(this, aKey, aVolume));
}

void
MediaStream::RemoveAudioOutputImpl(void* aKey)
{
  for (uint32_t i = 0; i < mAudioOutputs.Length(); ++i) {
    if (mAudioOutputs[i].mKey == aKey) {
      mAudioOutputs.RemoveElementAt(i);
      return;
    }
  }
  NS_ERROR("Audio output key not found");
}

void
MediaStream::RemoveAudioOutput(void* aKey)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, void* aKey) :
      ControlMessage(aStream), mKey(aKey) {}
    virtual void Run()
    {
      mStream->RemoveAudioOutputImpl(mKey);
    }
    void* mKey;
  };
  GraphImpl()->AppendMessage(new Message(this, aKey));
}

void
MediaStream::AddVideoOutput(VideoFrameContainer* aContainer)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, VideoFrameContainer* aContainer) :
      ControlMessage(aStream), mContainer(aContainer) {}
    virtual void Run()
    {
      mStream->AddVideoOutputImpl(mContainer.forget());
    }
    nsRefPtr<VideoFrameContainer> mContainer;
  };
  GraphImpl()->AppendMessage(new Message(this, aContainer));
}

void
MediaStream::RemoveVideoOutput(VideoFrameContainer* aContainer)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, VideoFrameContainer* aContainer) :
      ControlMessage(aStream), mContainer(aContainer) {}
    virtual void Run()
    {
      mStream->RemoveVideoOutputImpl(mContainer);
    }
    nsRefPtr<VideoFrameContainer> mContainer;
  };
  GraphImpl()->AppendMessage(new Message(this, aContainer));
}

void
MediaStream::ChangeExplicitBlockerCount(int32_t aDelta)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, int32_t aDelta) :
      ControlMessage(aStream), mDelta(aDelta) {}
    virtual void Run()
    {
      mStream->ChangeExplicitBlockerCountImpl(
          mStream->GraphImpl()->mStateComputedTime, mDelta);
    }
    int32_t mDelta;
  };

  // This can happen if this method has been called asynchronously, and the
  // stream has been destroyed since then.
  if (mMainThreadDestroyed) {
    return;
  }
  GraphImpl()->AppendMessage(new Message(this, aDelta));
}

void
MediaStream::AddListenerImpl(already_AddRefed<MediaStreamListener> aListener)
{
  MediaStreamListener* listener = *mListeners.AppendElement() = aListener;
  listener->NotifyBlockingChanged(GraphImpl(),
    mNotifiedBlocked ? MediaStreamListener::BLOCKED : MediaStreamListener::UNBLOCKED);
  if (mNotifiedFinished) {
    listener->NotifyEvent(GraphImpl(), MediaStreamListener::EVENT_FINISHED);
  }
  if (mNotifiedHasCurrentData) {
    listener->NotifyHasCurrentData(GraphImpl());
  }
}

void
MediaStream::AddListener(MediaStreamListener* aListener)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, MediaStreamListener* aListener) :
      ControlMessage(aStream), mListener(aListener) {}
    virtual void Run()
    {
      mStream->AddListenerImpl(mListener.forget());
    }
    nsRefPtr<MediaStreamListener> mListener;
  };
  GraphImpl()->AppendMessage(new Message(this, aListener));
}

void
MediaStream::RemoveListenerImpl(MediaStreamListener* aListener)
{
  // wouldn't need this if we could do it in the opposite order
  nsRefPtr<MediaStreamListener> listener(aListener);
  mListeners.RemoveElement(aListener);
  listener->NotifyEvent(GraphImpl(), MediaStreamListener::EVENT_REMOVED);
}

void
MediaStream::RemoveListener(MediaStreamListener* aListener)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, MediaStreamListener* aListener) :
      ControlMessage(aStream), mListener(aListener) {}
    virtual void Run()
    {
      mStream->RemoveListenerImpl(mListener);
    }
    nsRefPtr<MediaStreamListener> mListener;
  };
  // If the stream is destroyed the Listeners have or will be
  // removed.
  if (!IsDestroyed()) {
    GraphImpl()->AppendMessage(new Message(this, aListener));
  }
}

void
MediaStream::RunAfterPendingUpdates(nsRefPtr<nsIRunnable> aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MediaStreamGraphImpl* graph = GraphImpl();

  // Special case when a non-realtime graph has not started, to ensure the
  // runnable will run in finite time.
  if (!(graph->mRealtime || graph->mNonRealtimeProcessing)) {
    aRunnable->Run();
  }

  class Message : public ControlMessage {
  public:
    explicit Message(MediaStream* aStream,
                     already_AddRefed<nsIRunnable> aRunnable)
      : ControlMessage(aStream)
      , mRunnable(aRunnable) {}
    virtual void Run() MOZ_OVERRIDE
    {
      mStream->Graph()->
        DispatchToMainThreadAfterStreamStateUpdate(mRunnable.forget());
    }
    virtual void RunDuringShutdown() MOZ_OVERRIDE
    {
      // Don't run mRunnable now as it may call AppendMessage() which would
      // assume that there are no remaining controlMessagesToRunDuringShutdown.
      MOZ_ASSERT(NS_IsMainThread());
      NS_DispatchToCurrentThread(mRunnable);
    }
  private:
    nsRefPtr<nsIRunnable> mRunnable;
  };

  graph->AppendMessage(new Message(this, aRunnable.forget()));
}

void
MediaStream::SetTrackEnabledImpl(TrackID aTrackID, bool aEnabled)
{
  if (aEnabled) {
    mDisabledTrackIDs.RemoveElement(aTrackID);
  } else {
    if (!mDisabledTrackIDs.Contains(aTrackID)) {
      mDisabledTrackIDs.AppendElement(aTrackID);
    }
  }
}

void
MediaStream::SetTrackEnabled(TrackID aTrackID, bool aEnabled)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, TrackID aTrackID, bool aEnabled) :
      ControlMessage(aStream), mTrackID(aTrackID), mEnabled(aEnabled) {}
    virtual void Run()
    {
      mStream->SetTrackEnabledImpl(mTrackID, mEnabled);
    }
    TrackID mTrackID;
    bool mEnabled;
  };
  GraphImpl()->AppendMessage(new Message(this, aTrackID, aEnabled));
}

void
MediaStream::ApplyTrackDisabling(TrackID aTrackID, MediaSegment* aSegment, MediaSegment* aRawSegment)
{
  // mMutex must be owned here if this is a SourceMediaStream
  if (!mDisabledTrackIDs.Contains(aTrackID)) {
    return;
  }
  aSegment->ReplaceWithDisabled();
  if (aRawSegment) {
    aRawSegment->ReplaceWithDisabled();
  }
}

void
SourceMediaStream::DestroyImpl()
{
  // Hold mMutex while mGraph is reset so that other threads holding mMutex
  // can null-check know that the graph will not destroyed.
  MutexAutoLock lock(mMutex);
  MediaStream::DestroyImpl();
}

void
SourceMediaStream::SetPullEnabled(bool aEnabled)
{
  MutexAutoLock lock(mMutex);
  mPullEnabled = aEnabled;
  if (mPullEnabled && GraphImpl()) {
    GraphImpl()->EnsureNextIteration();
  }
}

void
SourceMediaStream::AddTrack(TrackID aID, TrackRate aRate, TrackTicks aStart,
                            MediaSegment* aSegment)
{
  MutexAutoLock lock(mMutex);
  TrackData* data = mUpdateTracks.AppendElement();
  data->mID = aID;
  data->mInputRate = aRate;
  // We resample all audio input tracks to the sample rate of the audio mixer.
  data->mOutputRate = aSegment->GetType() == MediaSegment::AUDIO ?
                      GraphImpl()->AudioSampleRate() : aRate;
  data->mStart = aStart;
  data->mCommands = TRACK_CREATE;
  data->mData = aSegment;
  data->mHaveEnough = false;
  if (auto graph = GraphImpl()) {
    graph->EnsureNextIteration();
  }
}

void
SourceMediaStream::ResampleAudioToGraphSampleRate(TrackData* aTrackData, MediaSegment* aSegment)
{
  if (aSegment->GetType() != MediaSegment::AUDIO ||
      aTrackData->mInputRate == GraphImpl()->AudioSampleRate()) {
    return;
  }
  AudioSegment* segment = static_cast<AudioSegment*>(aSegment);
  int channels = segment->ChannelCount();

  // If this segment is just silence, we delay instanciating the resampler.
  if (channels) {
    if (aTrackData->mResampler) {
      MOZ_ASSERT(aTrackData->mResamplerChannelCount == segment->ChannelCount());
    } else {
      SpeexResamplerState* state = speex_resampler_init(channels,
                                                        aTrackData->mInputRate,
                                                        GraphImpl()->AudioSampleRate(),
                                                        SPEEX_RESAMPLER_QUALITY_DEFAULT,
                                                        nullptr);
      if (!state) {
        return;
      }
      aTrackData->mResampler.own(state);
#ifdef DEBUG
      aTrackData->mResamplerChannelCount = channels;
#endif
    }
  }
  segment->ResampleChunks(aTrackData->mResampler, aTrackData->mInputRate, GraphImpl()->AudioSampleRate());
}

bool
SourceMediaStream::AppendToTrack(TrackID aID, MediaSegment* aSegment, MediaSegment *aRawSegment)
{
  MutexAutoLock lock(mMutex);
  // ::EndAllTrackAndFinished() can end these before the sources notice
  bool appended = false;
  auto graph = GraphImpl();
  if (!mFinished && graph) {
    TrackData *track = FindDataForTrack(aID);
    if (track) {
      // Data goes into mData, and on the next iteration of the MSG moves
      // into the track's segment after NotifyQueuedTrackChanges().  This adds
      // 0-10ms of delay before data gets to direct listeners.
      // Indirect listeners (via subsequent TrackUnion nodes) are synced to
      // playout time, and so can be delayed by buffering.

      // Apply track disabling before notifying any consumers directly
      // or inserting into the graph
      ApplyTrackDisabling(aID, aSegment, aRawSegment);

      ResampleAudioToGraphSampleRate(track, aSegment);

      // Must notify first, since AppendFrom() will empty out aSegment
      NotifyDirectConsumers(track, aRawSegment ? aRawSegment : aSegment);
      track->mData->AppendFrom(aSegment); // note: aSegment is now dead
      appended = true;
      graph->EnsureNextIteration();
    } else {
      aSegment->Clear();
    }
  }
  return appended;
}

void
SourceMediaStream::NotifyDirectConsumers(TrackData *aTrack,
                                         MediaSegment *aSegment)
{
  // Call with mMutex locked
  MOZ_ASSERT(aTrack);

  for (uint32_t j = 0; j < mDirectListeners.Length(); ++j) {
    MediaStreamDirectListener* l = mDirectListeners[j];
    TrackTicks offset = 0; // FIX! need a separate TrackTicks.... or the end of the internal buffer
    l->NotifyRealtimeData(static_cast<MediaStreamGraph*>(GraphImpl()), aTrack->mID, aTrack->mOutputRate,
                          offset, aTrack->mCommands, *aSegment);
  }
}

void
SourceMediaStream::AddDirectListener(MediaStreamDirectListener* aListener)
{
  bool wasEmpty;
  {
    MutexAutoLock lock(mMutex);
    wasEmpty = mDirectListeners.IsEmpty();
    mDirectListeners.AppendElement(aListener);
  }

  if (wasEmpty) {
    for (uint32_t j = 0; j < mListeners.Length(); ++j) {
      MediaStreamListener* l = mListeners[j];
      l->NotifyEvent(GraphImpl(), MediaStreamListener::EVENT_HAS_DIRECT_LISTENERS);
    }
  }
}

void
SourceMediaStream::RemoveDirectListener(MediaStreamDirectListener* aListener)
{
  bool isEmpty;
  {
    MutexAutoLock lock(mMutex);
    mDirectListeners.RemoveElement(aListener);
    isEmpty = mDirectListeners.IsEmpty();
  }

  if (isEmpty) {
    for (uint32_t j = 0; j < mListeners.Length(); ++j) {
      MediaStreamListener* l = mListeners[j];
      l->NotifyEvent(GraphImpl(), MediaStreamListener::EVENT_HAS_NO_DIRECT_LISTENERS);
    }
  }
}

bool
SourceMediaStream::HaveEnoughBuffered(TrackID aID)
{
  MutexAutoLock lock(mMutex);
  TrackData *track = FindDataForTrack(aID);
  if (track) {
    return track->mHaveEnough;
  }
  return false;
}

void
SourceMediaStream::DispatchWhenNotEnoughBuffered(TrackID aID,
    nsIEventTarget* aSignalThread, nsIRunnable* aSignalRunnable)
{
  MutexAutoLock lock(mMutex);
  TrackData* data = FindDataForTrack(aID);
  if (!data) {
    aSignalThread->Dispatch(aSignalRunnable, 0);
    return;
  }

  if (data->mHaveEnough) {
    if (data->mDispatchWhenNotEnough.IsEmpty()) {
      data->mDispatchWhenNotEnough.AppendElement()->Init(aSignalThread, aSignalRunnable);
    }
  } else {
    aSignalThread->Dispatch(aSignalRunnable, 0);
  }
}

void
SourceMediaStream::EndTrack(TrackID aID)
{
  MutexAutoLock lock(mMutex);
  // ::EndAllTrackAndFinished() can end these before the sources call this
  if (!mFinished) {
    TrackData *track = FindDataForTrack(aID);
    if (track) {
      track->mCommands |= TRACK_END;
    }
  }
  if (auto graph = GraphImpl()) {
    graph->EnsureNextIteration();
  }
}

void
SourceMediaStream::AdvanceKnownTracksTime(StreamTime aKnownTime)
{
  MutexAutoLock lock(mMutex);
  MOZ_ASSERT(aKnownTime >= mUpdateKnownTracksTime);
  mUpdateKnownTracksTime = aKnownTime;
  if (auto graph = GraphImpl()) {
    graph->EnsureNextIteration();
  }
}

void
SourceMediaStream::FinishWithLockHeld()
{
  mMutex.AssertCurrentThreadOwns();
  mUpdateFinished = true;
  if (auto graph = GraphImpl()) {
    graph->EnsureNextIteration();
  }
}

void
SourceMediaStream::EndAllTrackAndFinish()
{
  MutexAutoLock lock(mMutex);
  for (uint32_t i = 0; i < mUpdateTracks.Length(); ++i) {
    SourceMediaStream::TrackData* data = &mUpdateTracks[i];
    data->mCommands |= TRACK_END;
  }
  FinishWithLockHeld();
  // we will call NotifyEvent() to let GetUserMedia know
}

TrackTicks
SourceMediaStream::GetBufferedTicks(TrackID aID)
{
  StreamBuffer::Track* track  = mBuffer.FindTrack(aID);
  if (track) {
    MediaSegment* segment = track->GetSegment();
    if (segment) {
      return segment->GetDuration() -
        track->TimeToTicksRoundDown(
          GraphTimeToStreamTime(GraphImpl()->mStateComputedTime));
    }
  }
  return 0;
}

void
SourceMediaStream::RegisterForAudioMixing()
{
  MutexAutoLock lock(mMutex);
  mNeedsMixing = true;
}

bool
SourceMediaStream::NeedsMixing()
{
  MutexAutoLock lock(mMutex);
  return mNeedsMixing;
}

void
MediaInputPort::Init()
{
  STREAM_LOG(PR_LOG_DEBUG, ("Adding MediaInputPort %p (from %p to %p) to the graph",
             this, mSource, mDest));
  mSource->AddConsumer(this);
  mDest->AddInput(this);
  // mPortCount decremented via MediaInputPort::Destroy's message
  ++mDest->GraphImpl()->mPortCount;
}

void
MediaInputPort::Disconnect()
{
  NS_ASSERTION(!mSource == !mDest,
               "mSource must either both be null or both non-null");
  if (!mSource)
    return;

  mSource->RemoveConsumer(this);
  mSource = nullptr;
  mDest->RemoveInput(this);
  mDest = nullptr;

  GraphImpl()->SetStreamOrderDirty();
}

MediaInputPort::InputInterval
MediaInputPort::GetNextInputInterval(GraphTime aTime)
{
  InputInterval result = { GRAPH_TIME_MAX, GRAPH_TIME_MAX, false };
  GraphTime t = aTime;
  GraphTime end;
  for (;;) {
    if (!mDest->mBlocked.GetAt(t, &end))
      break;
    if (end >= GRAPH_TIME_MAX)
      return result;
    t = end;
  }
  result.mStart = t;
  GraphTime sourceEnd;
  result.mInputIsBlocked = mSource->mBlocked.GetAt(t, &sourceEnd);
  result.mEnd = std::min(end, sourceEnd);
  return result;
}

void
MediaInputPort::Destroy()
{
  class Message : public ControlMessage {
  public:
    Message(MediaInputPort* aPort)
      : ControlMessage(nullptr), mPort(aPort) {}
    virtual void Run()
    {
      mPort->Disconnect();
      --mPort->GraphImpl()->mPortCount;
      mPort->SetGraphImpl(nullptr);
      NS_RELEASE(mPort);
    }
    virtual void RunDuringShutdown()
    {
      Run();
    }
    MediaInputPort* mPort;
  };
  GraphImpl()->AppendMessage(new Message(this));
}

MediaStreamGraphImpl*
MediaInputPort::GraphImpl()
{
  return mGraph;
}

MediaStreamGraph*
MediaInputPort::Graph()
{
  return mGraph;
}

void
MediaInputPort::SetGraphImpl(MediaStreamGraphImpl* aGraph)
{
  MOZ_ASSERT(!mGraph || !aGraph, "Should only be set once");
  mGraph = aGraph;
}

already_AddRefed<MediaInputPort>
ProcessedMediaStream::AllocateInputPort(MediaStream* aStream, uint32_t aFlags,
                                        uint16_t aInputNumber, uint16_t aOutputNumber)
{
  // This method creates two references to the MediaInputPort: one for
  // the main thread, and one for the MediaStreamGraph.
  class Message : public ControlMessage {
  public:
    Message(MediaInputPort* aPort)
      : ControlMessage(aPort->GetDestination()),
        mPort(aPort) {}
    virtual void Run()
    {
      mPort->Init();
      // The graph holds its reference implicitly
      mPort->GraphImpl()->SetStreamOrderDirty();
      unused << mPort.forget();
    }
    virtual void RunDuringShutdown()
    {
      Run();
    }
    nsRefPtr<MediaInputPort> mPort;
  };
  nsRefPtr<MediaInputPort> port = new MediaInputPort(aStream, this, aFlags,
                                                     aInputNumber, aOutputNumber);
  port->SetGraphImpl(GraphImpl());
  GraphImpl()->AppendMessage(new Message(port));
  return port.forget();
}

void
ProcessedMediaStream::Finish()
{
  class Message : public ControlMessage {
  public:
    Message(ProcessedMediaStream* aStream)
      : ControlMessage(aStream) {}
    virtual void Run()
    {
      mStream->GraphImpl()->FinishStream(mStream);
    }
  };
  GraphImpl()->AppendMessage(new Message(this));
}

void
ProcessedMediaStream::SetAutofinish(bool aAutofinish)
{
  class Message : public ControlMessage {
  public:
    Message(ProcessedMediaStream* aStream, bool aAutofinish)
      : ControlMessage(aStream), mAutofinish(aAutofinish) {}
    virtual void Run()
    {
      static_cast<ProcessedMediaStream*>(mStream)->SetAutofinishImpl(mAutofinish);
    }
    bool mAutofinish;
  };
  GraphImpl()->AppendMessage(new Message(this, aAutofinish));
}

void
ProcessedMediaStream::DestroyImpl()
{
  for (int32_t i = mInputs.Length() - 1; i >= 0; --i) {
    mInputs[i]->Disconnect();
  }
  MediaStream::DestroyImpl();
  // The stream order is only important if there are connections, in which
  // case MediaInputPort::Disconnect() called SetStreamOrderDirty().
  // MediaStreamGraphImpl::RemoveStream() will also call
  // SetStreamOrderDirty(), for other reasons.
}

MediaStreamGraphImpl::MediaStreamGraphImpl(bool aRealtime, TrackRate aSampleRate)
  : mCurrentTime(0)
  , mStateComputedTime(0)
  , mProcessingGraphUpdateIndex(0)
  , mPortCount(0)
  , mMonitor("MediaStreamGraphImpl")
  , mLifecycleState(LIFECYCLE_THREAD_NOT_STARTED)
  , mWaitState(WAITSTATE_RUNNING)
  , mEndTime(GRAPH_TIME_MAX)
  , mSampleRate(aSampleRate)
  , mNeedAnotherIteration(false)
  , mForceShutDown(false)
  , mPostedRunInStableStateEvent(false)
  , mDetectedNotRunning(false)
  , mPostedRunInStableState(false)
  , mRealtime(aRealtime)
  , mNonRealtimeProcessing(false)
  , mStreamOrderDirty(false)
  , mLatencyLog(AsyncLatencyLogger::Get())
  , mMixer(nullptr)
  , mMemoryReportMonitor("MSGIMemory")
  , mSelfRef(MOZ_THIS_IN_INITIALIZER_LIST())
  , mAudioStreamSizes()
  , mNeedsMemoryReport(false)
#ifdef DEBUG
  , mCanRunMessagesSynchronously(false)
#endif
{
#ifdef PR_LOGGING
  if (!gMediaStreamGraphLog) {
    gMediaStreamGraphLog = PR_NewLogModule("MediaStreamGraph");
  }
#endif

  mCurrentTimeStamp = mInitialTimeStamp = mLastMainThreadUpdate = TimeStamp::Now();

  RegisterWeakMemoryReporter(this);
}

void
MediaStreamGraphImpl::Destroy()
{
  // First unregister from memory reporting.
  UnregisterWeakMemoryReporter(this);

  // Clear the self reference which will destroy this instance.
  mSelfRef = nullptr;
}

NS_IMPL_ISUPPORTS(MediaStreamGraphShutdownObserver, nsIObserver)

static bool gShutdownObserverRegistered = false;

NS_IMETHODIMP
MediaStreamGraphShutdownObserver::Observe(nsISupports *aSubject,
                                          const char *aTopic,
                                          const char16_t *aData)
{
  if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    if (gGraph) {
      gGraph->ForceShutDown();
    }
    nsContentUtils::UnregisterShutdownObserver(this);
    gShutdownObserverRegistered = false;
  }
  return NS_OK;
}

MediaStreamGraph*
MediaStreamGraph::GetInstance()
{
  NS_ASSERTION(NS_IsMainThread(), "Main thread only");

  if (!gGraph) {
    if (!gShutdownObserverRegistered) {
      gShutdownObserverRegistered = true;
      nsContentUtils::RegisterShutdownObserver(new MediaStreamGraphShutdownObserver());
    }

    AudioStream::InitPreferredSampleRate();

    gGraph = new MediaStreamGraphImpl(true, AudioStream::PreferredSampleRate());

    STREAM_LOG(PR_LOG_DEBUG, ("Starting up MediaStreamGraph %p", gGraph));
  }

  return gGraph;
}

MediaStreamGraph*
MediaStreamGraph::CreateNonRealtimeInstance(TrackRate aSampleRate)
{
  NS_ASSERTION(NS_IsMainThread(), "Main thread only");

  MediaStreamGraphImpl* graph = new MediaStreamGraphImpl(false, aSampleRate);

  return graph;
}

void
MediaStreamGraph::DestroyNonRealtimeInstance(MediaStreamGraph* aGraph)
{
  NS_ASSERTION(NS_IsMainThread(), "Main thread only");
  MOZ_ASSERT(aGraph->IsNonRealtime(), "Should not destroy the global graph here");

  MediaStreamGraphImpl* graph = static_cast<MediaStreamGraphImpl*>(aGraph);
  if (graph->mForceShutDown)
    return; // already done

  if (!graph->mNonRealtimeProcessing) {
    // Start the graph, but don't produce anything
    graph->StartNonRealtimeProcessing(1, 0);
  }
  graph->ForceShutDown();
}

NS_IMPL_ISUPPORTS(MediaStreamGraphImpl, nsIMemoryReporter)

struct ArrayClearer
{
  ArrayClearer(nsTArray<AudioNodeSizes>& aArray) : mArray(aArray) {}
  ~ArrayClearer() { mArray.Clear(); }
  nsTArray<AudioNodeSizes>& mArray;
};

NS_IMETHODIMP
MediaStreamGraphImpl::CollectReports(nsIHandleReportCallback* aHandleReport,
                                     nsISupports* aData, bool aAnonymize)
{
  // Clears out the report array after we're done with it.
  ArrayClearer reportCleanup(mAudioStreamSizes);

  {
    MonitorAutoLock memoryReportLock(mMemoryReportMonitor);
    mNeedsMemoryReport = true;

    {
      // Wake up the MSG thread.
      MonitorAutoLock monitorLock(mMonitor);
      EnsureImmediateWakeUpLocked(monitorLock);
    }

    if (mLifecycleState >= LIFECYCLE_WAITING_FOR_THREAD_SHUTDOWN) {
      // Shutting down, nothing to report.
      return NS_OK;
    }

    // Wait for up to one second for the report to complete.
    nsresult rv;
    const PRIntervalTime kMaxWait = PR_SecondsToInterval(1);
    while ((rv = memoryReportLock.Wait(kMaxWait)) != NS_OK) {
      if (PR_GetError() != PR_PENDING_INTERRUPT_ERROR) {
        return rv;
      }
    }
  }

#define REPORT(_path, _amount, _desc)                                       \
  do {                                                                      \
    nsresult rv;                                                            \
    rv = aHandleReport->Callback(EmptyCString(), _path,                     \
                                 KIND_HEAP, UNITS_BYTES, _amount,           \
                                 NS_LITERAL_CSTRING(_desc), aData);         \
    NS_ENSURE_SUCCESS(rv, rv);                                              \
  } while (0)

  for (size_t i = 0; i < mAudioStreamSizes.Length(); i++) {
    const AudioNodeSizes& usage = mAudioStreamSizes[i];
    const char* const nodeType =  usage.mNodeType.IsEmpty() ?
                                  "<unknown>" : usage.mNodeType.get();

    nsPrintfCString domNodePath("explicit/webaudio/audio-node/%s/dom-nodes",
                                nodeType);
    REPORT(domNodePath, usage.mDomNode,
           "Memory used by AudioNode DOM objects (Web Audio).");

    nsPrintfCString enginePath("explicit/webaudio/audio-node/%s/engine-objects",
                               nodeType);
    REPORT(enginePath, usage.mEngine,
           "Memory used by AudioNode engine objects (Web Audio).");

    nsPrintfCString streamPath("explicit/webaudio/audio-node/%s/stream-objects",
                               nodeType);
    REPORT(streamPath, usage.mStream,
           "Memory used by AudioNode stream objects (Web Audio).");

  }

#undef REPORT

  return NS_OK;
}

SourceMediaStream*
MediaStreamGraph::CreateSourceStream(DOMMediaStream* aWrapper)
{
  SourceMediaStream* stream = new SourceMediaStream(aWrapper);
  NS_ADDREF(stream);
  MediaStreamGraphImpl* graph = static_cast<MediaStreamGraphImpl*>(this);
  stream->SetGraphImpl(graph);
  graph->AppendMessage(new CreateMessage(stream));
  return stream;
}

ProcessedMediaStream*
MediaStreamGraph::CreateTrackUnionStream(DOMMediaStream* aWrapper)
{
  TrackUnionStream* stream = new TrackUnionStream(aWrapper);
  NS_ADDREF(stream);
  MediaStreamGraphImpl* graph = static_cast<MediaStreamGraphImpl*>(this);
  stream->SetGraphImpl(graph);
  graph->AppendMessage(new CreateMessage(stream));
  return stream;
}

AudioNodeExternalInputStream*
MediaStreamGraph::CreateAudioNodeExternalInputStream(AudioNodeEngine* aEngine, TrackRate aSampleRate)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!aSampleRate) {
    aSampleRate = aEngine->NodeMainThread()->Context()->SampleRate();
  }
  AudioNodeExternalInputStream* stream = new AudioNodeExternalInputStream(aEngine, aSampleRate);
  NS_ADDREF(stream);
  MediaStreamGraphImpl* graph = static_cast<MediaStreamGraphImpl*>(this);
  stream->SetGraphImpl(graph);
  graph->AppendMessage(new CreateMessage(stream));
  return stream;
}

AudioNodeStream*
MediaStreamGraph::CreateAudioNodeStream(AudioNodeEngine* aEngine,
                                        AudioNodeStreamKind aKind,
                                        TrackRate aSampleRate)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!aSampleRate) {
    aSampleRate = aEngine->NodeMainThread()->Context()->SampleRate();
  }
  AudioNodeStream* stream = new AudioNodeStream(aEngine, aKind, aSampleRate);
  NS_ADDREF(stream);
  MediaStreamGraphImpl* graph = static_cast<MediaStreamGraphImpl*>(this);
  stream->SetGraphImpl(graph);
  if (aEngine->HasNode()) {
    stream->SetChannelMixingParametersImpl(aEngine->NodeMainThread()->ChannelCount(),
                                           aEngine->NodeMainThread()->ChannelCountModeValue(),
                                           aEngine->NodeMainThread()->ChannelInterpretationValue());
  }
  graph->AppendMessage(new CreateMessage(stream));
  return stream;
}

bool
MediaStreamGraph::IsNonRealtime() const
{
  return this != gGraph;
}

void
MediaStreamGraph::StartNonRealtimeProcessing(TrackRate aRate, uint32_t aTicksToProcess)
{
  NS_ASSERTION(NS_IsMainThread(), "main thread only");

  MediaStreamGraphImpl* graph = static_cast<MediaStreamGraphImpl*>(this);
  NS_ASSERTION(!graph->mRealtime, "non-realtime only");

  if (graph->mNonRealtimeProcessing)
    return;
  graph->mEndTime = graph->mCurrentTime +
    RateConvertTicksRoundUp(graph->GraphRate(), aRate, aTicksToProcess);
  graph->mNonRealtimeProcessing = true;
  graph->EnsureRunInStableState();
}

void
ProcessedMediaStream::AddInput(MediaInputPort* aPort)
{
  mInputs.AppendElement(aPort);
  GraphImpl()->SetStreamOrderDirty();
}

}
