/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamGraphImpl.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Unused.h"

#include "AudioSegment.h"
#include "VideoSegment.h"
#include "nsContentUtils.h"
#include "nsIObserver.h"
#include "nsPrintfCString.h"
#include "nsServiceManagerUtils.h"
#include "prerror.h"
#include "mozilla/Logging.h"
#include "mozilla/Attributes.h"
#include "TrackUnionStream.h"
#include "ImageContainer.h"
#include "AudioCaptureStream.h"
#include "AudioNodeStream.h"
#include "AudioNodeExternalInputStream.h"
#include "MediaStreamListener.h"
#include "MediaStreamVideoSink.h"
#include "mozilla/dom/BaseAudioContextBinding.h"
#include "mozilla/media/MediaUtils.h"
#include <algorithm>
#include "GeckoProfiler.h"
#include "VideoFrameContainer.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/Unused.h"
#include "mtransport/runnable_utils.h"
#include "VideoUtils.h"
#include "Tracing.h"

#include "webaudio/blink/DenormalDisabler.h"
#include "webaudio/blink/HRTFDatabaseLoader.h"

using namespace mozilla::layers;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::media;

mozilla::AsyncLogger gMSGTraceLogger("MSGTracing");

namespace mozilla {

LazyLogModule gMediaStreamGraphLog("MediaStreamGraph");
#ifdef LOG
#undef LOG
#endif // LOG
#define LOG(type, msg) MOZ_LOG(gMediaStreamGraphLog, type, msg)

enum SourceMediaStream::TrackCommands : uint32_t {
  TRACK_CREATE = TrackEventCommand::TRACK_EVENT_CREATED,
  TRACK_END = TrackEventCommand::TRACK_EVENT_ENDED,
  TRACK_UNUSED = TrackEventCommand::TRACK_EVENT_UNUSED,
};

/**
 * A hash table containing the graph instances, one per document.
 *
 * The key is a hash of nsPIDOMWindowInner, see `WindowToHash`.
 */
static nsDataHashtable<nsUint32HashKey, MediaStreamGraphImpl*> gGraphs;

MediaStreamGraphImpl::~MediaStreamGraphImpl()
{
  MOZ_ASSERT(mStreams.IsEmpty() && mSuspendedStreams.IsEmpty(),
             "All streams should have been destroyed by messages from the main thread");
  LOG(LogLevel::Debug, ("MediaStreamGraph %p destroyed", this));
  LOG(LogLevel::Debug, ("MediaStreamGraphImpl::~MediaStreamGraphImpl"));

#ifdef TRACING
  gMSGTraceLogger.Stop();
#endif
}

void
MediaStreamGraphImpl::AddStreamGraphThread(MediaStream* aStream)
{
  MOZ_ASSERT(OnGraphThreadOrNotRunning());
  aStream->mTracksStartTime = mProcessedTime;

  if (aStream->AsSourceStream()) {
    SourceMediaStream* source = aStream->AsSourceStream();
    TimeStamp currentTimeStamp = CurrentDriver()->GetCurrentTimeStamp();
    TimeStamp processedTimeStamp = currentTimeStamp +
      TimeDuration::FromSeconds(MediaTimeToSeconds(mProcessedTime - IterationEnd()));
    source->SetStreamTracksStartTimeStamp(processedTimeStamp);
  }

  if (aStream->IsSuspended()) {
    mSuspendedStreams.AppendElement(aStream);
    LOG(LogLevel::Debug,
        ("%p: Adding media stream %p, in the suspended stream array",
         this, aStream));
  } else {
    mStreams.AppendElement(aStream);
    LOG(LogLevel::Debug,
        ("%p:  Adding media stream %p, count %zu",
         this,
         aStream,
         mStreams.Length()));
  }

  SetStreamOrderDirty();
}

void
MediaStreamGraphImpl::RemoveStreamGraphThread(MediaStream* aStream)
{
  MOZ_ASSERT(OnGraphThreadOrNotRunning());
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

  if (aStream->IsSuspended()) {
    mSuspendedStreams.RemoveElement(aStream);
  } else {
    mStreams.RemoveElement(aStream);
  }

  LOG(LogLevel::Debug,
      ("%p: Removed media stream %p, count %zu",
       this,
       aStream,
       mStreams.Length()));

  NS_RELEASE(aStream); // probably destroying it
}

StreamTime
MediaStreamGraphImpl::GraphTimeToStreamTimeWithBlocking(const MediaStream* aStream,
                                                        GraphTime aTime) const
{
  MOZ_ASSERT(aTime <= mStateComputedTime,
             "Don't ask about times where we haven't made blocking decisions yet");
  return std::max<StreamTime>(0,
      std::min(aTime, aStream->mStartBlocking) - aStream->mTracksStartTime);
}

GraphTime
MediaStreamGraphImpl::IterationEnd() const
{
  MOZ_ASSERT(OnGraphThreadOrNotRunning());
  return CurrentDriver()->IterationEnd();
}

void
MediaStreamGraphImpl::UpdateCurrentTimeForStreams(GraphTime aPrevCurrentTime)
{
  MOZ_ASSERT(OnGraphThread());
  for (MediaStream* stream : AllStreams()) {
    bool isAnyBlocked = stream->mStartBlocking < mStateComputedTime;
    bool isAnyUnblocked = stream->mStartBlocking > aPrevCurrentTime;

    // Calculate blocked time and fire Blocked/Unblocked events
    GraphTime blockedTime = mStateComputedTime - stream->mStartBlocking;
    NS_ASSERTION(blockedTime >= 0, "Error in blocking time");
    stream->AdvanceTimeVaryingValuesToCurrentTime(mStateComputedTime,
                                                  blockedTime);
    LOG(LogLevel::Verbose,
        ("%p: MediaStream %p bufferStartTime=%f blockedTime=%f",
         this,
         stream,
         MediaTimeToSeconds(stream->mTracksStartTime),
         MediaTimeToSeconds(blockedTime)));
    stream->mStartBlocking = mStateComputedTime;

    if (isAnyUnblocked && stream->mNotifiedBlocked) {
      for (uint32_t j = 0; j < stream->mListeners.Length(); ++j) {
        MediaStreamListener* l = stream->mListeners[j];
        l->NotifyBlockingChanged(this, MediaStreamListener::UNBLOCKED);
      }
      stream->mNotifiedBlocked = false;
    }
    if (isAnyBlocked && !stream->mNotifiedBlocked) {
      for (uint32_t j = 0; j < stream->mListeners.Length(); ++j) {
        MediaStreamListener* l = stream->mListeners[j];
        l->NotifyBlockingChanged(this, MediaStreamListener::BLOCKED);
      }
      stream->mNotifiedBlocked = true;
    }

    if (isAnyUnblocked) {
      NS_ASSERTION(!stream->mNotifiedFinished,
        "Shouldn't have already notified of finish *and* have output!");
      for (uint32_t j = 0; j < stream->mListeners.Length(); ++j) {
        MediaStreamListener* l = stream->mListeners[j];
        l->NotifyOutput(this, mProcessedTime);
      }
    }

    // The stream is fully finished when all of its track data has been played
    // out.
    if (stream->mFinished && !stream->mNotifiedFinished &&
        mProcessedTime >=
          stream->StreamTimeToGraphTime(stream->GetStreamTracks().GetAllTracksEnd())) {
      stream->mNotifiedFinished = true;
      SetStreamOrderDirty();
      for (uint32_t j = 0; j < stream->mListeners.Length(); ++j) {
        MediaStreamListener* l = stream->mListeners[j];
        l->NotifyEvent(this, MediaStreamGraphEvent::EVENT_FINISHED);
      }
    }
  }
}

template<typename C, typename Chunk>
void
MediaStreamGraphImpl::ProcessChunkMetadataForInterval(MediaStream* aStream,
                                                      TrackID aTrackID,
                                                      C& aSegment,
                                                      StreamTime aStart,
                                                      StreamTime aEnd)
{
  MOZ_ASSERT(OnGraphThreadOrNotRunning());
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(IsTrackIDExplicit(aTrackID));

  StreamTime offset = 0;
  for (typename C::ConstChunkIterator chunk(aSegment);
         !chunk.IsEnded(); chunk.Next()) {
    if (offset >= aEnd) {
      break;
    }
    offset += chunk->GetDuration();
    if (chunk->IsNull() || offset < aStart) {
      continue;
    }
    const PrincipalHandle& principalHandle = chunk->GetPrincipalHandle();
    if (principalHandle != aSegment.GetLastPrincipalHandle()) {
      aSegment.SetLastPrincipalHandle(principalHandle);
      LOG(LogLevel::Debug,
          ("%p: MediaStream %p track %d, principalHandle "
           "changed in %sChunk with duration %lld",
           this,
           aStream,
           aTrackID,
           aSegment.GetType() == MediaSegment::AUDIO ? "Audio" : "Video",
           (long long)chunk->GetDuration()));
      for (const TrackBound<MediaStreamTrackListener>& listener :
           aStream->mTrackListeners) {
        if (listener.mTrackID == aTrackID) {
          listener.mListener->NotifyPrincipalHandleChanged(this, principalHandle);
        }
      }
    }
  }
}

void
MediaStreamGraphImpl::ProcessChunkMetadata(GraphTime aPrevCurrentTime)
{
  MOZ_ASSERT(OnGraphThreadOrNotRunning());
  for (MediaStream* stream : AllStreams()) {
    StreamTime iterationStart = stream->GraphTimeToStreamTime(aPrevCurrentTime);
    StreamTime iterationEnd = stream->GraphTimeToStreamTime(mProcessedTime);
    for (StreamTracks::TrackIter tracks(stream->mTracks);
            !tracks.IsEnded(); tracks.Next()) {
      MediaSegment* segment = tracks->GetSegment();
      if (!segment) {
        continue;
      }
      if (tracks->GetType() == MediaSegment::AUDIO) {
        AudioSegment* audio = static_cast<AudioSegment*>(segment);
        ProcessChunkMetadataForInterval<AudioSegment, AudioChunk>(
            stream, tracks->GetID(), *audio, iterationStart, iterationEnd);
      } else if (tracks->GetType() == MediaSegment::VIDEO) {
        VideoSegment* video = static_cast<VideoSegment*>(segment);
        ProcessChunkMetadataForInterval<VideoSegment, VideoChunk>(
            stream, tracks->GetID(), *video, iterationStart, iterationEnd);
      } else {
        MOZ_CRASH("Unknown track type");
      }
    }
  }
}

GraphTime
MediaStreamGraphImpl::WillUnderrun(MediaStream* aStream,
                                   GraphTime aEndBlockingDecisions)
{
  // Finished streams can't underrun. ProcessedMediaStreams also can't cause
  // underrun currently, since we'll always be able to produce data for them
  // unless they block on some other stream.
  if (aStream->mFinished || aStream->AsProcessedStream()) {
    return aEndBlockingDecisions;
  }
  // This stream isn't finished or suspended. We don't need to call
  // StreamTimeToGraphTime since an underrun is the only thing that can block
  // it.
  GraphTime bufferEnd = aStream->GetTracksEnd() + aStream->mTracksStartTime;
#ifdef DEBUG
  if (bufferEnd < mProcessedTime) {
    LOG(LogLevel::Error,
        ("%p: MediaStream %p underrun, "
         "bufferEnd %f < mProcessedTime %f (%" PRId64 " < %" PRId64
         "), Streamtime %" PRId64,
         this,
         aStream,
         MediaTimeToSeconds(bufferEnd),
         MediaTimeToSeconds(mProcessedTime),
         bufferEnd,
         mProcessedTime,
         aStream->GetTracksEnd()));
    aStream->DumpTrackInfo();
    NS_ASSERTION(bufferEnd >= mProcessedTime, "Buffer underran");
  }
#endif
  return std::min(bufferEnd, aEndBlockingDecisions);
}

namespace {
  // Value of mCycleMarker for unvisited streams in cycle detection.
  const uint32_t NOT_VISITED = UINT32_MAX;
  // Value of mCycleMarker for ordered streams in muted cycles.
  const uint32_t IN_MUTED_CYCLE = 1;
} // namespace

bool
MediaStreamGraphImpl::AudioTrackPresent()
{
  MOZ_ASSERT(OnGraphThreadOrNotRunning());

  bool audioTrackPresent = false;
  for (MediaStream* stream : mStreams) {
    if (stream->AsAudioNodeStream()) {
      audioTrackPresent = true;
      break;
    }

    if (!StreamTracks::TrackIter(
            stream->GetStreamTracks(),
            MediaSegment::AUDIO
          ).IsEnded()) {
      audioTrackPresent = true;
      break;
    }

    if (SourceMediaStream* source = stream->AsSourceStream()) {
      audioTrackPresent = source->HasPendingAudioTrack();
    }

    if (audioTrackPresent) {
      break;
    }
  }

  // XXX For some reason, there are race conditions when starting an audio input where
  // we find no active audio tracks.  In any case, if we have an active audio input we
  // should not allow a switch back to a SystemClockDriver
  if (!audioTrackPresent && mInputDeviceUsers.Count() != 0) {
    NS_WARNING("No audio tracks, but full-duplex audio is enabled!!!!!");
    audioTrackPresent = true;
  }

  return audioTrackPresent;
}

void
MediaStreamGraphImpl::UpdateStreamOrder()
{
  MOZ_ASSERT(OnGraphThread());
  bool audioTrackPresent = AudioTrackPresent();

  // Note that this looks for any audio streams, input or output, and switches to a
  // SystemClockDriver if there are none.  However, if another is already pending, let that
  // switch happen.

  if (!audioTrackPresent && mRealtime &&
      CurrentDriver()->AsAudioCallbackDriver()) {
    MonitorAutoLock mon(mMonitor);
    if (CurrentDriver()->AsAudioCallbackDriver()->IsStarted() &&
        !(CurrentDriver()->Switching())) {
      if (LifecycleStateRef() == LIFECYCLE_RUNNING) {
        SystemClockDriver* driver = new SystemClockDriver(this);
        CurrentDriver()->SwitchAtNextIteration(driver);
      }
    }
  }

  bool switching = false;
  {
    MonitorAutoLock mon(mMonitor);
    switching = CurrentDriver()->Switching();
  }

  if (audioTrackPresent && mRealtime &&
      !CurrentDriver()->AsAudioCallbackDriver() &&
      !switching) {
    MonitorAutoLock mon(mMonitor);
    if (LifecycleStateRef() == LIFECYCLE_RUNNING) {
      AudioCallbackDriver* driver = new AudioCallbackDriver(this, AudioInputChannelCount());
      CurrentDriver()->SwitchAtNextIteration(driver);
    }
  }

  if (!mStreamOrderDirty) {
    return;
  }

  mStreamOrderDirty = false;

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
        if (inputs[i]->mSource->IsSuspended()) {
          continue;
        }
        auto input = inputs[i]->mSource->AsProcessedStream();
        if (input && input->mCycleMarker == NOT_VISITED) {
          // It can be that this stream has an input which is from a suspended
          // AudioContext.
          if (input->isInList()) {
            input->remove();
            dfsStack.insertFront(input);
          }
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
      if (inputs[i]->mSource->IsSuspended()) {
        continue;
      }
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
MediaStreamGraphImpl::CreateOrDestroyAudioStreams(MediaStream* aStream)
{
  MOZ_ASSERT(OnGraphThread());
  MOZ_ASSERT(mRealtime, "Should only attempt to create audio streams in real-time mode");

  if (aStream->mAudioOutputs.IsEmpty()) {
    aStream->mAudioOutputStreams.Clear();
    return;
  }

  if (!aStream->GetStreamTracks().GetAndResetTracksDirty() &&
      !aStream->mAudioOutputStreams.IsEmpty()) {
    return;
  }

  LOG(LogLevel::Debug,
      ("%p: Updating AudioOutputStreams for MediaStream %p", this, aStream));

  AutoTArray<bool,2> audioOutputStreamsFound;
  for (uint32_t i = 0; i < aStream->mAudioOutputStreams.Length(); ++i) {
    audioOutputStreamsFound.AppendElement(false);
  }

  for (StreamTracks::TrackIter tracks(aStream->GetStreamTracks(), MediaSegment::AUDIO);
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
      MediaStream::AudioOutputStream* audioOutputStream =
        aStream->mAudioOutputStreams.AppendElement();
      audioOutputStream->mAudioPlaybackStartTime = mProcessedTime;
      audioOutputStream->mBlockedAudioTime = 0;
      audioOutputStream->mLastTickWritten = 0;
      audioOutputStream->mTrackID = tracks->GetID();

      bool switching = false;

      {
        MonitorAutoLock lock(mMonitor);
        switching = CurrentDriver()->Switching();
      }

      if (!CurrentDriver()->AsAudioCallbackDriver() &&
          !switching) {
        MonitorAutoLock mon(mMonitor);
        if (LifecycleStateRef() == LIFECYCLE_RUNNING) {
          AudioCallbackDriver* driver = new AudioCallbackDriver(this, AudioInputChannelCount());
          CurrentDriver()->SwitchAtNextIteration(driver);
        }
      }
    }
  }

  for (int32_t i = audioOutputStreamsFound.Length() - 1; i >= 0; --i) {
    if (!audioOutputStreamsFound[i]) {
      aStream->mAudioOutputStreams.RemoveElementAt(i);
    }
  }
}

StreamTime
MediaStreamGraphImpl::PlayAudio(MediaStream* aStream)
{
  MOZ_ASSERT(OnGraphThread());
  MOZ_ASSERT(mRealtime, "Should only attempt to play audio in realtime mode");

  float volume = 0.0f;
  for (uint32_t i = 0; i < aStream->mAudioOutputs.Length(); ++i) {
    volume += aStream->mAudioOutputs[i].mVolume * mGlobalVolume;
  }

  StreamTime ticksWritten = 0;

  for (uint32_t i = 0; i < aStream->mAudioOutputStreams.Length(); ++i) {
    ticksWritten = 0;

    MediaStream::AudioOutputStream& audioOutput = aStream->mAudioOutputStreams[i];
    StreamTracks::Track* track = aStream->mTracks.FindTrack(audioOutput.mTrackID);
    AudioSegment* audio = track->Get<AudioSegment>();
    AudioSegment output;

    StreamTime offset = aStream->GraphTimeToStreamTime(mProcessedTime);

    // We don't update aStream->mTracksStartTime here to account for time spent
    // blocked. Instead, we'll update it in UpdateCurrentTimeForStreams after
    // the blocked period has completed. But we do need to make sure we play
    // from the right offsets in the stream buffer, even if we've already
    // written silence for some amount of blocked time after the current time.
    GraphTime t = mProcessedTime;
    while (t < mStateComputedTime) {
      bool blocked = t >= aStream->mStartBlocking;
      GraphTime end = blocked ? mStateComputedTime : aStream->mStartBlocking;
      NS_ASSERTION(end <= mStateComputedTime, "mStartBlocking is wrong!");

      // Check how many ticks of sound we can provide if we are blocked some
      // time in the middle of this cycle.
      StreamTime toWrite = end - t;

      if (blocked) {
        output.InsertNullDataAtStart(toWrite);
        ticksWritten += toWrite;
        LOG(LogLevel::Verbose,
            ("%p: MediaStream %p writing %" PRId64 " blocking-silence samples for "
             "%f to %f (%" PRId64 " to %" PRId64 ")",
             this,
             aStream,
             toWrite,
             MediaTimeToSeconds(t),
             MediaTimeToSeconds(end),
             offset,
             offset + toWrite));
      } else {
        StreamTime endTicksNeeded = offset + toWrite;
        StreamTime endTicksAvailable = audio->GetDuration();

        if (endTicksNeeded <= endTicksAvailable) {
          LOG(LogLevel::Verbose,
              ("%p: MediaStream %p writing %" PRId64 " samples for %f to %f "
               "(samples %" PRId64 " to %" PRId64 ")",
               this,
               aStream,
               toWrite,
               MediaTimeToSeconds(t),
               MediaTimeToSeconds(end),
               offset,
               endTicksNeeded));
          output.AppendSlice(*audio, offset, endTicksNeeded);
          ticksWritten += toWrite;
          offset = endTicksNeeded;
        } else {
          // MOZ_ASSERT(track->IsEnded(), "Not enough data, and track not ended.");
          // If we are at the end of the track, maybe write the remaining
          // samples, and pad with/output silence.
          if (endTicksNeeded > endTicksAvailable &&
              offset < endTicksAvailable) {
            output.AppendSlice(*audio, offset, endTicksAvailable);
            LOG(LogLevel::Verbose,
                ("%p: MediaStream %p writing %" PRId64 " samples for %f to %f "
                 "(samples %" PRId64 " to %" PRId64 ")",
                 this,
                 aStream,
                 toWrite,
                 MediaTimeToSeconds(t),
                 MediaTimeToSeconds(end),
                 offset,
                 endTicksNeeded));
            uint32_t available = endTicksAvailable - offset;
            ticksWritten += available;
            toWrite -= available;
            offset = endTicksAvailable;
          }
          output.AppendNullData(toWrite);
          LOG(LogLevel::Verbose,
              ("%p MediaStream %p writing %" PRId64 " padding slsamples for %f to "
               "%f (samples %" PRId64 " to %" PRId64 ")",
               this,
               aStream,
               toWrite,
               MediaTimeToSeconds(t),
               MediaTimeToSeconds(end),
               offset,
               endTicksNeeded));
          ticksWritten += toWrite;
        }
        output.ApplyVolume(volume);
      }
      t = end;
    }
    audioOutput.mLastTickWritten = offset;

    output.WriteTo(mMixer, AudioOutputChannelCount(), mSampleRate);
  }
  return ticksWritten;
}

void
MediaStreamGraphImpl::OpenAudioInputImpl(CubebUtils::AudioDeviceID aID,
                                         AudioDataListener* aListener)
{
  MOZ_ASSERT(OnGraphThread());
  // Only allow one device per MSG (hence, per document), but allow opening a
  // device multiple times
  nsTArray<RefPtr<AudioDataListener>>& listeners = mInputDeviceUsers.GetOrInsert(aID);
  if (listeners.IsEmpty() && mInputDeviceUsers.Count() > 1) {
    // We don't support opening multiple input device in a graph for now.
    listeners.RemoveElement(aID);
    return;
  }

  MOZ_ASSERT(!listeners.Contains(aListener), "Don't add a listener twice.");

  listeners.AppendElement(aListener);

  if (listeners.Length() == 1) { // first open for this device
    mInputDeviceID = aID;
    // Switch Drivers since we're adding input (to input-only or full-duplex)
    MonitorAutoLock mon(mMonitor);
    if (LifecycleStateRef() == LIFECYCLE_RUNNING) {
      AudioCallbackDriver* driver = new AudioCallbackDriver(this, AudioInputChannelCount());
      LOG(
        LogLevel::Debug,
        ("%p OpenAudioInput: starting new AudioCallbackDriver(input) %p", this, driver));
      CurrentDriver()->SwitchAtNextIteration(driver);
   } else {
     LOG(LogLevel::Error, ("OpenAudioInput in shutdown!"));
     MOZ_ASSERT_UNREACHABLE("Can't open cubeb inputs in shutdown");
    }
  }
}

nsresult
MediaStreamGraphImpl::OpenAudioInput(CubebUtils::AudioDeviceID aID,
                                     AudioDataListener* aListener)
{
  // So, so, so annoying.  Can't AppendMessage except on Mainthread
  if (!NS_IsMainThread()) {
    RefPtr<nsIRunnable> runnable =
      WrapRunnable(this,
                   &MediaStreamGraphImpl::OpenAudioInput,
                   aID,
                   RefPtr<AudioDataListener>(aListener));
    mAbstractMainThread->Dispatch(runnable.forget());
    return NS_OK;
  }
  class Message : public ControlMessage {
  public:
    Message(MediaStreamGraphImpl *aGraph, CubebUtils::AudioDeviceID aID,
            AudioDataListener* aListener) :
      ControlMessage(nullptr), mGraph(aGraph), mID(aID), mListener(aListener) {}
    void Run() override
    {
      mGraph->OpenAudioInputImpl(mID, mListener);
    }
    MediaStreamGraphImpl *mGraph;
    CubebUtils::AudioDeviceID mID;
    RefPtr<AudioDataListener> mListener;
  };
  // XXX Check not destroyed!
  this->AppendMessage(MakeUnique<Message>(this, aID, aListener));
  return NS_OK;
}

void
MediaStreamGraphImpl::CloseAudioInputImpl(Maybe<CubebUtils::AudioDeviceID>& aID, AudioDataListener* aListener)
{
  MOZ_ASSERT(OnGraphThreadOrNotRunning());
  // It is possible to not know the ID here, find it first.
  if (aID.isNothing()) {
    for (auto iter = mInputDeviceUsers.Iter(); !iter.Done(); iter.Next()) {
      if (iter.Data().Contains(aListener)) {
        aID = Some(iter.Key());
      }
    }
    MOZ_ASSERT(aID.isSome(), "Closing an audio input that was not opened.");
  }

  nsTArray<RefPtr<AudioDataListener>>* listeners = mInputDeviceUsers.GetValue(aID.value());

  MOZ_ASSERT(listeners);
  DebugOnly<bool> wasPresent = listeners->RemoveElement(aListener);
  MOZ_ASSERT(wasPresent);

  // Breaks the cycle between the MSG and the listener.
  aListener->Disconnect(this);

  if (!listeners->IsEmpty()) {
    // There is still a consumer for this audio input device
    return;
  }

  mInputDeviceID = nullptr; // reset to default
  mInputDeviceUsers.Remove(aID.value());

  // Switch Drivers since we're adding or removing an input (to nothing/system or output only)
  bool audioTrackPresent = AudioTrackPresent();

  MonitorAutoLock mon(mMonitor);
  if (LifecycleStateRef() == LIFECYCLE_RUNNING) {
    GraphDriver* driver;
    if (audioTrackPresent) {
      // We still have audio output
      LOG(LogLevel::Debug, ("%p: CloseInput: output present (AudioCallback)", this));

      driver = new AudioCallbackDriver(this, AudioInputChannelCount());
      CurrentDriver()->SwitchAtNextIteration(driver);
    } else if (CurrentDriver()->AsAudioCallbackDriver()) {
      LOG(LogLevel::Debug,
          ("%p: CloseInput: no output present (SystemClockCallback)", this));

      driver = new SystemClockDriver(this);
      CurrentDriver()->SwitchAtNextIteration(driver);
    } // else SystemClockDriver->SystemClockDriver, no switch
  }
}

void
MediaStreamGraphImpl::CloseAudioInput(Maybe<CubebUtils::AudioDeviceID>& aID, AudioDataListener* aListener)
{
  // So, so, so annoying.  Can't AppendMessage except on Mainthread
  if (!NS_IsMainThread()) {
    RefPtr<nsIRunnable> runnable =
      WrapRunnable(this,
                   &MediaStreamGraphImpl::CloseAudioInput,
                   aID,
                   RefPtr<AudioDataListener>(aListener));
    mAbstractMainThread->Dispatch(runnable.forget());
    return;
  }
  class Message : public ControlMessage {
  public:
    Message(MediaStreamGraphImpl *aGraph,
            Maybe<CubebUtils::AudioDeviceID>& aID,
            AudioDataListener* aListener)
      : ControlMessage(nullptr),
        mGraph(aGraph),
        mID(aID),
        mListener(aListener)
    {}
    void Run() override
    {
      mGraph->CloseAudioInputImpl(mID, mListener);
    }
    MediaStreamGraphImpl *mGraph;
    Maybe<CubebUtils::AudioDeviceID> mID;
    RefPtr<AudioDataListener> mListener;
  };
  this->AppendMessage(MakeUnique<Message>(this, aID, aListener));
}

// All AudioInput listeners get the same speaker data (at least for now).
void
MediaStreamGraphImpl::NotifyOutputData(AudioDataValue* aBuffer, size_t aFrames,
                                       TrackRate aRate, uint32_t aChannels)
{
#ifdef ANDROID
  // On Android, mInputDeviceID is always null and represents the default
  // device.
  // The absence of an input consumer is enough to know we need to bail out
  // here.
  if (!mInputDeviceUsers.GetValue(mInputDeviceID)) {
    return;
  }
#else
  if (!mInputDeviceID) {
    return;
  }
#endif
  // When/if we decide to support multiple input devices per graph, this needs
  // to loop over them.
  nsTArray<RefPtr<AudioDataListener>>* listeners = mInputDeviceUsers.GetValue(mInputDeviceID);
  MOZ_ASSERT(listeners);
  for (auto& listener : *listeners) {
    listener->NotifyOutputData(this, aBuffer, aFrames, aRate, aChannels);
  }
}

void
MediaStreamGraphImpl::NotifyInputData(const AudioDataValue* aBuffer, size_t aFrames,
                                      TrackRate aRate, uint32_t aChannels)
{
#ifdef ANDROID
  if (!mInputDeviceUsers.GetValue(mInputDeviceID)) {
    return;
  }
#else
#ifdef DEBUG
  {
    MonitorAutoLock lock(mMonitor);
    // Either we have an audio input device, or we just removed the audio input
    // this iteration, and we're switching back to an output-only driver next
    // iteration.
    MOZ_ASSERT(mInputDeviceID || CurrentDriver()->Switching());
  }
#endif
  if (!mInputDeviceID) {
    return;
  }
#endif
  nsTArray<RefPtr<AudioDataListener>>* listeners = mInputDeviceUsers.GetValue(mInputDeviceID);
  MOZ_ASSERT(listeners);
  for (auto& listener : *listeners) {
    listener->NotifyInputData(this, aBuffer, aFrames, aRate, aChannels);
  }
}

void MediaStreamGraphImpl::DeviceChangedImpl()
{
  MOZ_ASSERT(OnGraphThread());

#ifdef ANDROID
  if (!mInputDeviceUsers.GetValue(mInputDeviceID)) {
    return;
  }
#else
  if (!mInputDeviceID) {
    return;
  }
#endif

  nsTArray<RefPtr<AudioDataListener>>* listeners =
    mInputDeviceUsers.GetValue(mInputDeviceID);
  for (auto& listener : *listeners) {
    listener->DeviceChanged(this);
  }
}

void MediaStreamGraphImpl::DeviceChanged()
{
  // This is safe to be called from any thread: this message comes from an
  // underlying platform API, and we don't have much guarantees. If it is not
  // called from the main thread (and it probably will rarely be), it will post
  // itself to the main thread, and the actual device change message will be ran
  // and acted upon on the graph thread.
  if (!NS_IsMainThread()) {
    RefPtr<nsIRunnable> runnable =
      WrapRunnable(this,
                   &MediaStreamGraphImpl::DeviceChanged);
    mAbstractMainThread->Dispatch(runnable.forget());
    return;
  }

  class Message : public ControlMessage {
  public:
    explicit Message(MediaStreamGraph* aGraph)
      : ControlMessage(nullptr)
      , mGraphImpl(static_cast<MediaStreamGraphImpl*>(aGraph))
    {}
    void Run() override
    {
      mGraphImpl->DeviceChangedImpl();
    }
    // We know that this is valid, because the graph can't shutdown if it has
    // messages.
    MediaStreamGraphImpl* mGraphImpl;
  };

  AppendMessage(MakeUnique<Message>(this));
}

void MediaStreamGraphImpl::ReevaluateInputDevice()
{
  MOZ_ASSERT(OnGraphThread());
  bool needToSwitch = false;

  if (CurrentDriver()->AsAudioCallbackDriver()) {
    AudioCallbackDriver* audioCallbackDriver = CurrentDriver()->AsAudioCallbackDriver();
    if (audioCallbackDriver->InputChannelCount() != AudioInputChannelCount()) {
      needToSwitch = true;
    }
  } else {
    // We're already in the process of switching to a audio callback driver,
    // which will happen at the next iteration.
    // However, maybe it's not the correct number of channels. Re-query the
    // correct channel amount at this time.
#ifdef DEBUG
    MonitorAutoLock lock(mMonitor);
    MOZ_ASSERT(CurrentDriver()->Switching());
#endif
    needToSwitch = true;
  }
  if (needToSwitch) {
    AudioCallbackDriver* newDriver = new AudioCallbackDriver(this, AudioInputChannelCount());
    {
      MonitorAutoLock lock(mMonitor);
      CurrentDriver()->SwitchAtNextIteration(newDriver);
    }
  }
}

bool
MediaStreamGraph::OnGraphThreadOrNotRunning() const
{
  // either we're on the right thread (and calling CurrentDriver() is safe),
  // or we're going to fail the assert anyway, so don't cross-check
  // via CurrentDriver().
  MediaStreamGraphImpl const * graph =
    static_cast<MediaStreamGraphImpl const *>(this);
  return graph->mDetectedNotRunning ?
    NS_IsMainThread() : graph->mDriver->OnThread();
}

bool
MediaStreamGraph::OnGraphThread() const
{
  // we're on the right thread (and calling mDriver is safe),
  MediaStreamGraphImpl const * graph =
    static_cast<MediaStreamGraphImpl const *>(this);
  MOZ_ASSERT(graph->mDriver);
  return graph->mDriver->OnThread();
}

bool
MediaStreamGraphImpl::ShouldUpdateMainThread()
{
  MOZ_ASSERT(OnGraphThreadOrNotRunning());
  if (mRealtime) {
    return true;
  }

  TimeStamp now = TimeStamp::Now();
  // For offline graphs, update now if there is no pending iteration or if it
  // has been long enough since the last update.
  if (!mNeedAnotherIteration ||
      ((now - mLastMainThreadUpdate).ToMilliseconds() >
       CurrentDriver()->IterationDuration())) {
    mLastMainThreadUpdate = now;
    return true;
  }
  return false;
}

void
MediaStreamGraphImpl::PrepareUpdatesToMainThreadState(bool aFinalUpdate)
{
  MOZ_ASSERT(OnGraphThreadOrNotRunning());
  mMonitor.AssertCurrentThreadOwns();

  // We don't want to frequently update the main thread about timing update
  // when we are not running in realtime.
  if (aFinalUpdate || ShouldUpdateMainThread()) {
    // Strip updates that will be obsoleted below, so as to keep the length of
    // mStreamUpdates sane.
    size_t keptUpdateCount = 0;
    for (size_t i = 0; i < mStreamUpdates.Length(); ++i) {
      MediaStream* stream = mStreamUpdates[i].mStream;
      // RemoveStreamGraphThread() clears mStream in updates for
      // streams that are removed from the graph.
      MOZ_ASSERT(!stream || stream->GraphImpl() == this);
      if (!stream || stream->MainThreadNeedsUpdates()) {
        // Discard this update as it has either been cleared when the stream
        // was destroyed or there will be a newer update below.
        continue;
      }
      if (keptUpdateCount != i) {
        mStreamUpdates[keptUpdateCount] = std::move(mStreamUpdates[i]);
        MOZ_ASSERT(!mStreamUpdates[i].mStream);
      }
      ++keptUpdateCount;
    }
    mStreamUpdates.TruncateLength(keptUpdateCount);

    mStreamUpdates.SetCapacity(mStreamUpdates.Length() + mStreams.Length() +
        mSuspendedStreams.Length());
    for (MediaStream* stream : AllStreams()) {
      if (!stream->MainThreadNeedsUpdates()) {
        continue;
      }
      StreamUpdate* update = mStreamUpdates.AppendElement();
      update->mStream = stream;
      // No blocking to worry about here, since we've passed
      // UpdateCurrentTimeForStreams.
      update->mNextMainThreadCurrentTime =
        stream->GraphTimeToStreamTime(mProcessedTime);
      update->mNextMainThreadFinished = stream->mNotifiedFinished;
    }
    if (!mPendingUpdateRunnables.IsEmpty()) {
      mUpdateRunnables.AppendElements(std::move(mPendingUpdateRunnables));
    }
  }

  // If this is the final update, then a stable state event will soon be
  // posted just before this thread finishes, and so there is no need to also
  // post here.
  if (!aFinalUpdate &&
      // Don't send the message to the main thread if it's not going to have
      // any work to do.
      !(mUpdateRunnables.IsEmpty() && mStreamUpdates.IsEmpty())) {
    EnsureStableStateEventPosted();
  }
}

GraphTime
MediaStreamGraphImpl::RoundUpToEndOfAudioBlock(GraphTime aTime)
{
  if (aTime % WEBAUDIO_BLOCK_SIZE == 0) {
    return aTime;
  }
  return RoundUpToNextAudioBlock(aTime);
}

GraphTime
MediaStreamGraphImpl::RoundUpToNextAudioBlock(GraphTime aTime)
{
  uint64_t block = aTime >> WEBAUDIO_BLOCK_SIZE_BITS;
  uint64_t nextBlock = block + 1;
  GraphTime nextTime = nextBlock << WEBAUDIO_BLOCK_SIZE_BITS;
  return nextTime;
}

void
MediaStreamGraphImpl::ProduceDataForStreamsBlockByBlock(uint32_t aStreamIndex,
                                                        TrackRate aSampleRate)
{
  MOZ_ASSERT(OnGraphThread());
  MOZ_ASSERT(aStreamIndex <= mFirstCycleBreaker,
             "Cycle breaker is not AudioNodeStream?");
  GraphTime t = mProcessedTime;
  while (t < mStateComputedTime) {
    GraphTime next = RoundUpToNextAudioBlock(t);
    for (uint32_t i = mFirstCycleBreaker; i < mStreams.Length(); ++i) {
      auto ns = static_cast<AudioNodeStream*>(mStreams[i]);
      MOZ_ASSERT(ns->AsAudioNodeStream());
      ns->ProduceOutputBeforeInput(t);
    }
    for (uint32_t i = aStreamIndex; i < mStreams.Length(); ++i) {
      ProcessedMediaStream* ps = mStreams[i]->AsProcessedStream();
      if (ps) {
        ps->ProcessInput(t, next,
            (next == mStateComputedTime) ? ProcessedMediaStream::ALLOW_FINISH : 0);
      }
    }
    t = next;
  }
  NS_ASSERTION(t == mStateComputedTime,
               "Something went wrong with rounding to block boundaries");
}

void
MediaStreamGraphImpl::RunMessageAfterProcessing(UniquePtr<ControlMessage> aMessage)
{
  MOZ_ASSERT(OnGraphThread());

  if (mFrontMessageQueue.IsEmpty()) {
    mFrontMessageQueue.AppendElement();
  }

  // Only one block is used for messages from the graph thread.
  MOZ_ASSERT(mFrontMessageQueue.Length() == 1);
  mFrontMessageQueue[0].mMessages.AppendElement(std::move(aMessage));
}

void
MediaStreamGraphImpl::RunMessagesInQueue()
{
  TRACE_AUDIO_CALLBACK();
  MOZ_ASSERT(OnGraphThread());
  // Calculate independent action times for each batch of messages (each
  // batch corresponding to an event loop task). This isolates the performance
  // of different scripts to some extent.
  for (uint32_t i = 0; i < mFrontMessageQueue.Length(); ++i) {
    nsTArray<UniquePtr<ControlMessage>>& messages = mFrontMessageQueue[i].mMessages;

    for (uint32_t j = 0; j < messages.Length(); ++j) {
      messages[j]->Run();
    }
  }
  mFrontMessageQueue.Clear();
}

void
MediaStreamGraphImpl::UpdateGraph(GraphTime aEndBlockingDecisions)
{
  TRACE_AUDIO_CALLBACK();
  MOZ_ASSERT(OnGraphThread());
  MOZ_ASSERT(aEndBlockingDecisions >= mProcessedTime);
  // The next state computed time can be the same as the previous: it
  // means the driver would have been blocking indefinitly, but the graph has
  // been woken up right after having been to sleep.
  MOZ_ASSERT(aEndBlockingDecisions >= mStateComputedTime);

  UpdateStreamOrder();

  bool ensureNextIteration = false;

  for (MediaStream* stream : mStreams) {
    if (SourceMediaStream* is = stream->AsSourceStream()) {
      ensureNextIteration |= is->PullNewData(aEndBlockingDecisions);
      is->ExtractPendingInput();
    }
    if (stream->mFinished) {
      // The stream's not suspended, and since it's finished, underruns won't
      // stop it playing out. So there's no blocking other than what we impose
      // here.
      GraphTime endTime = stream->GetStreamTracks().GetAllTracksEnd() +
          stream->mTracksStartTime;
      if (endTime <= mStateComputedTime) {
        LOG(LogLevel::Verbose,
            ("%p: MediaStream %p is blocked due to being finished", this, stream));
        stream->mStartBlocking = mStateComputedTime;
      } else {
        LOG(LogLevel::Verbose,
            ("%p: MediaStream %p is finished, but not blocked yet (end at %f, with "
             "blocking at %f)",
             this,
             stream,
             MediaTimeToSeconds(stream->GetTracksEnd()),
             MediaTimeToSeconds(endTime)));
        // Data can't be added to a finished stream, so underruns are irrelevant.
        stream->mStartBlocking = std::min(endTime, aEndBlockingDecisions);
      }
    } else {
      stream->mStartBlocking = WillUnderrun(stream, aEndBlockingDecisions);

      SourceMediaStream* s = stream->AsSourceStream();
      if (s && s->mPullEnabled) {
        for (StreamTracks::TrackIter i(s->mTracks); !i.IsEnded(); i.Next()) {
          if (i->IsEnded()) {
            continue;
          }
          if (i->GetEnd() < stream->GraphTimeToStreamTime(aEndBlockingDecisions)) {
            LOG(LogLevel::Error,
                ("%p: SourceMediaStream %p track %u (%s) is live and pulled, but wasn't fed "
                 "enough data. Listeners=%zu. Track-end=%f, Iteration-end=%f",
                 this,
                 stream,
                 i->GetID(),
                 (i->GetType() == MediaSegment::AUDIO ? "audio" : "video"),
                 stream->mListeners.Length(),
                 MediaTimeToSeconds(i->GetEnd()),
                 MediaTimeToSeconds(stream->GraphTimeToStreamTime(aEndBlockingDecisions))));
            MOZ_DIAGNOSTIC_ASSERT(false,
                                  "A non-finished SourceMediaStream wasn't fed "
                                  "enough data by NotifyPull");
          }
        }
      }
    }
  }

  for (MediaStream* stream : mSuspendedStreams) {
    stream->mStartBlocking = mStateComputedTime;
  }

  // If the loop is woken up so soon that IterationEnd() barely advances or
  // if an offline graph is not currently rendering, we end up having
  // aEndBlockingDecisions == mStateComputedTime.
  // Since the process interval [mStateComputedTime, aEndBlockingDecision) is
  // empty, Process() will not find any unblocked stream and so will not
  // ensure another iteration.  If the graph should be rendering, then ensure
  // another iteration to render.
  if (ensureNextIteration ||
      (aEndBlockingDecisions == mStateComputedTime &&
       mStateComputedTime < mEndTime)) {
    EnsureNextIteration();
  }
}

void
MediaStreamGraphImpl::Process()
{
  TRACE_AUDIO_CALLBACK();
  MOZ_ASSERT(OnGraphThread());
  // Play stream contents.
  bool allBlockedForever = true;
  // True when we've done ProcessInput for all processed streams.
  bool doneAllProducing = false;
  // This is the number of frame that are written to the AudioStreams, for
  // this cycle.
  StreamTime ticksPlayed = 0;

  mMixer.StartMixing();

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
          ProduceDataForStreamsBlockByBlock(i, n->SampleRate());
          doneAllProducing = true;
        } else {
          ps->ProcessInput(mProcessedTime, mStateComputedTime,
                           ProcessedMediaStream::ALLOW_FINISH);
          NS_ASSERTION(stream->mTracks.GetEnd() >=
                       GraphTimeToStreamTimeWithBlocking(stream, mStateComputedTime),
                       "Stream did not produce enough data");
        }
      }
    }
    NotifyHasCurrentData(stream);
    // Only playback audio and video in real-time mode
    if (mRealtime) {
      CreateOrDestroyAudioStreams(stream);
      if (CurrentDriver()->AsAudioCallbackDriver()) {
        StreamTime ticksPlayedForThisStream = PlayAudio(stream);
        if (!ticksPlayed) {
          ticksPlayed = ticksPlayedForThisStream;
        } else {
          MOZ_ASSERT(!ticksPlayedForThisStream || ticksPlayedForThisStream == ticksPlayed,
              "Each stream should have the same number of frame.");
        }
      }
    }
    if (stream->mStartBlocking > mProcessedTime) {
      allBlockedForever = false;
    }
  }

  if (CurrentDriver()->AsAudioCallbackDriver()) {
    if (!ticksPlayed) {
      // Nothing was played, so the mixer doesn't know how many frames were
      // processed. We still tell it so AudioCallbackDriver knows how much has
      // been processed. (bug 1406027)
      mMixer.Mix(nullptr,
                 CurrentDriver()->AsAudioCallbackDriver()->OutputChannelCount(),
                 mStateComputedTime - mProcessedTime,
                 mSampleRate);
    }
    mMixer.FinishMixing();
  }

  if (!allBlockedForever) {
    EnsureNextIteration();
  }
}

bool
MediaStreamGraphImpl::UpdateMainThreadState()
{
  MOZ_ASSERT(OnGraphThread());
  MonitorAutoLock lock(mMonitor);
  bool finalUpdate = mForceShutDown ||
    (IsEmpty() && mBackMessageQueue.IsEmpty());
  PrepareUpdatesToMainThreadState(finalUpdate);
  if (finalUpdate) {
    // Enter shutdown mode when this iteration is completed.
    // No need to Destroy streams here. The main-thread owner of each
    // stream is responsible for calling Destroy on them.
    return false;
  }

  CurrentDriver()->WaitForNextIteration();

  SwapMessageQueues();
  return true;
}

bool
MediaStreamGraphImpl::OneIteration(GraphTime aStateEnd)
{
  TRACE_AUDIO_CALLBACK();
  // Changes to LIFECYCLE_RUNNING occur before starting or reviving the graph
  // thread, and so the monitor need not be held to check mLifecycleState.
  // LIFECYCLE_THREAD_NOT_STARTED is possible when shutting down offline
  // graphs that have not started.
  MOZ_DIAGNOSTIC_ASSERT(mLifecycleState <= LIFECYCLE_RUNNING);
  MOZ_ASSERT(OnGraphThread());
  WebCore::DenormalDisabler disabler;

  // Process graph message from the main thread for this iteration.
  RunMessagesInQueue();

  GraphTime stateEnd = std::min(aStateEnd, GraphTime(mEndTime));
  UpdateGraph(stateEnd);

  mStateComputedTime = stateEnd;

  Process();

  GraphTime oldProcessedTime = mProcessedTime;
  mProcessedTime = stateEnd;

  UpdateCurrentTimeForStreams(oldProcessedTime);

  ProcessChunkMetadata(oldProcessedTime);

  // Process graph messages queued from RunMessageAfterProcessing() on this
  // thread during the iteration.
  RunMessagesInQueue();

  return UpdateMainThreadState();
}

void
MediaStreamGraphImpl::ApplyStreamUpdate(StreamUpdate* aUpdate)
{
  MOZ_ASSERT(NS_IsMainThread());
  mMonitor.AssertCurrentThreadOwns();

  MediaStream* stream = aUpdate->mStream;
  if (!stream)
    return;
  stream->mMainThreadCurrentTime = aUpdate->mNextMainThreadCurrentTime;
  stream->mMainThreadFinished = aUpdate->mNextMainThreadFinished;

  if (stream->ShouldNotifyStreamFinished()) {
    stream->NotifyMainThreadListeners();
  }
}

void
MediaStreamGraphImpl::ForceShutDown(media::ShutdownTicket* aShutdownTicket)
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be called on main thread");
  LOG(LogLevel::Debug, ("%p: MediaStreamGraph::ForceShutdown", this));

  if (aShutdownTicket) {
    MOZ_ASSERT(!mForceShutdownTicket);
    // Avoid waiting forever for a graph to shut down
    // synchronously.  Reports are that some 3rd-party audio drivers
    // occasionally hang in shutdown (both for us and Chrome).
    NS_NewTimerWithCallback(getter_AddRefs(mShutdownTimer),
                            this,
                            MediaStreamGraph::AUDIO_CALLBACK_DRIVER_SHUTDOWN_TIMEOUT,
                            nsITimer::TYPE_ONE_SHOT);
  }
  mForceShutdownTicket = aShutdownTicket;
  MonitorAutoLock lock(mMonitor);
  mForceShutDown = true;
  if (LifecycleStateRef() == LIFECYCLE_THREAD_NOT_STARTED) {
    // We *could* have just sent this a message to start up, so don't
    // yank the rug out from under it.  Tell it to startup and let it
    // shut down.
    RefPtr<GraphDriver> driver = CurrentDriver();
    MonitorAutoUnlock unlock(mMonitor);
    driver->Start();
  }
  EnsureNextIterationLocked();
}

NS_IMETHODIMP
MediaStreamGraphImpl::Notify(nsITimer* aTimer)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ASSERTION(!mForceShutdownTicket, "MediaStreamGraph took too long to shut down!");
  // Sigh, graph took too long to shut down.  Stop blocking system
  // shutdown and hope all is well.
  mForceShutdownTicket = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
MediaStreamGraphImpl::GetName(nsACString& aName)
{
  aName.AssignLiteral("MediaStreamGraphImpl");
  return NS_OK;
}

/* static */ StaticRefPtr<nsIAsyncShutdownBlocker> gMediaStreamGraphShutdownBlocker;

namespace {

class MediaStreamGraphShutDownRunnable : public Runnable {
public:
  explicit MediaStreamGraphShutDownRunnable(MediaStreamGraphImpl* aGraph)
    : Runnable("MediaStreamGraphShutDownRunnable")
    , mGraph(aGraph)
  {}
  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mGraph->mDetectedNotRunning && mGraph->mDriver,
                 "We should know the graph thread control loop isn't running!");

    LOG(LogLevel::Debug, ("%p: Shutting down graph", mGraph.get()));

    // We've asserted the graph isn't running.  Use mDriver instead of CurrentDriver
    // to avoid thread-safety checks
#if 0 // AudioCallbackDrivers are released asynchronously anyways
    // XXX a better test would be have setting mDetectedNotRunning make sure
    // any current callback has finished and block future ones -- or just
    // handle it all in Shutdown()!
    if (mGraph->mDriver->AsAudioCallbackDriver()) {
      MOZ_ASSERT(!mGraph->mDriver->AsAudioCallbackDriver()->InCallback());
    }
#endif

    mGraph->mDriver->Shutdown(); // This will wait until it's shutdown since
                                 // we'll start tearing down the graph after this

    // Release the driver now so that an AudioCallbackDriver will release its
    // SharedThreadPool reference.  Each SharedThreadPool reference must be
    // released before SharedThreadPool::SpinUntilEmpty() runs on
    // xpcom-shutdown-threads.  Don't wait for GC/CC to release references to
    // objects owning streams, or for expiration of mGraph->mShutdownTimer,
    // which won't otherwise release its reference on the graph until
    // nsTimerImpl::Shutdown(), which runs after xpcom-shutdown-threads.
    {
      MonitorAutoLock mon(mGraph->mMonitor);
      mGraph->SetCurrentDriver(nullptr);
    }

    // Safe to access these without the monitor since the graph isn't running.
    // We may be one of several graphs. Drop ticket to eventually unblock shutdown.
    if (mGraph->mShutdownTimer && !mGraph->mForceShutdownTicket) {
      MOZ_ASSERT(false,
        "AudioCallbackDriver took too long to shut down and we let shutdown"
        " continue - freezing and leaking");

      // The timer fired, so we may be deeper in shutdown now.  Block any further
      // teardown and just leak, for safety.
      return NS_OK;
    }

    // mGraph's thread is not running so it's OK to do whatever here
    for (MediaStream* stream : mGraph->AllStreams()) {
      // Clean up all MediaSegments since we cannot release Images too
      // late during shutdown. Also notify listeners that they were removed
      // so they can clean up any gfx resources.
      if (SourceMediaStream* source = stream->AsSourceStream()) {
        // Finishing a SourceStream prevents new data from being appended.
        source->FinishOnGraphThread();
      }
      stream->GetStreamTracks().Clear();
      stream->RemoveAllListenersImpl();
    }

    mGraph->mForceShutdownTicket = nullptr;

    // We can't block past the final LIFECYCLE_WAITING_FOR_STREAM_DESTRUCTION
    // stage, since completion of that stage requires all streams to be freed,
    // which requires shutdown to proceed.

    if (mGraph->IsEmpty()) {
      // mGraph is no longer needed, so delete it.
      mGraph->Destroy();
    } else {
      // The graph is not empty.  We must be in a forced shutdown, or a
      // non-realtime graph that has finished processing. Some later
      // AppendMessage will detect that the graph has been emptied, and
      // delete it.
      NS_ASSERTION(mGraph->mForceShutDown || !mGraph->mRealtime,
                   "Not in forced shutdown?");
      mGraph->LifecycleStateRef() =
        MediaStreamGraphImpl::LIFECYCLE_WAITING_FOR_STREAM_DESTRUCTION;
    }
    return NS_OK;
  }
private:
  RefPtr<MediaStreamGraphImpl> mGraph;
};

class MediaStreamGraphStableStateRunnable : public Runnable {
public:
  explicit MediaStreamGraphStableStateRunnable(MediaStreamGraphImpl* aGraph,
                                               bool aSourceIsMSG)
    : Runnable("MediaStreamGraphStableStateRunnable")
    , mGraph(aGraph)
    , mSourceIsMSG(aSourceIsMSG)
  {
  }
  NS_IMETHOD Run() override
  {
    TRACE();
    if (mGraph) {
      mGraph->RunInStableState(mSourceIsMSG);
    }
    return NS_OK;
  }
private:
  RefPtr<MediaStreamGraphImpl> mGraph;
  bool mSourceIsMSG;
};

/*
 * Control messages forwarded from main thread to graph manager thread
 */
class CreateMessage : public ControlMessage {
public:
  explicit CreateMessage(MediaStream* aStream) : ControlMessage(aStream) {}
  void Run() override
  {
    mStream->GraphImpl()->AddStreamGraphThread(mStream);
  }
  void RunDuringShutdown() override
  {
    // Make sure to run this message during shutdown too, to make sure
    // that we balance the number of streams registered with the graph
    // as they're destroyed during shutdown.
    Run();
  }
};

} // namespace

void
MediaStreamGraphImpl::RunInStableState(bool aSourceIsMSG)
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be called on main thread");

  nsTArray<nsCOMPtr<nsIRunnable> > runnables;
  // When we're doing a forced shutdown, pending control messages may be
  // run on the main thread via RunDuringShutdown. Those messages must
  // run without the graph monitor being held. So, we collect them here.
  nsTArray<UniquePtr<ControlMessage>> controlMessagesToRunDuringShutdown;

  {
    MonitorAutoLock lock(mMonitor);
    if (aSourceIsMSG) {
      MOZ_ASSERT(mPostedRunInStableStateEvent);
      mPostedRunInStableStateEvent = false;
    }

    // This should be kept in sync with the LifecycleState enum in
    // MediaStreamGraphImpl.h
    const char* LifecycleState_str[] = {
      "LIFECYCLE_THREAD_NOT_STARTED",
      "LIFECYCLE_RUNNING",
      "LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP",
      "LIFECYCLE_WAITING_FOR_THREAD_SHUTDOWN",
      "LIFECYCLE_WAITING_FOR_STREAM_DESTRUCTION"
    };

    if (LifecycleStateRef() != LIFECYCLE_RUNNING) {
      LOG(LogLevel::Debug,
          ("%p: Running stable state callback. Current state: %s",
           this,
           LifecycleState_str[LifecycleStateRef()]));
    }

    runnables.SwapElements(mUpdateRunnables);
    for (uint32_t i = 0; i < mStreamUpdates.Length(); ++i) {
      StreamUpdate* update = &mStreamUpdates[i];
      if (update->mStream) {
        ApplyStreamUpdate(update);
      }
    }
    mStreamUpdates.Clear();

    if (mCurrentTaskMessageQueue.IsEmpty()) {
      if (LifecycleStateRef() == LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP && IsEmpty()) {
        // Complete shutdown. First, ensure that this graph is no longer used.
        // A new graph graph will be created if one is needed.
        // Asynchronously clean up old graph. We don't want to do this
        // synchronously because it spins the event loop waiting for threads
        // to shut down, and we don't want to do that in a stable state handler.
        LifecycleStateRef() = LIFECYCLE_WAITING_FOR_THREAD_SHUTDOWN;
        LOG(LogLevel::Debug,
            ("%p: Sending MediaStreamGraphShutDownRunnable", this));
        nsCOMPtr<nsIRunnable> event = new MediaStreamGraphShutDownRunnable(this );
        mAbstractMainThread->Dispatch(event.forget());

        LOG(LogLevel::Debug, ("%p: Disconnecting MediaStreamGraph", this));

        // Find the graph in the hash table and remove it.
        for (auto iter = gGraphs.Iter(); !iter.Done(); iter.Next()) {
          if (iter.UserData() == this) {
            iter.Remove();
            break;
          }
        }
      }
    } else {
      if (LifecycleStateRef() <= LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP) {
        MessageBlock* block = mBackMessageQueue.AppendElement();
        block->mMessages.SwapElements(mCurrentTaskMessageQueue);
        EnsureNextIterationLocked();
      }

      // If the MediaStreamGraph has more messages going to it, try to revive
      // it to process those messages. Don't do this if we're in a forced
      // shutdown or it's a non-realtime graph that has already terminated
      // processing.
      if (LifecycleStateRef() == LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP &&
          mRealtime && !mForceShutDown) {
        LifecycleStateRef() = LIFECYCLE_RUNNING;
        // Revive the MediaStreamGraph since we have more messages going to it.
        // Note that we need to put messages into its queue before reviving it,
        // or it might exit immediately.
        {
          LOG(LogLevel::Debug,
              ("%p: Reviving this graph! %s",
               this,
               CurrentDriver()->AsAudioCallbackDriver() ? "AudioCallbackDriver"
                                                        : "SystemClockDriver"));
          RefPtr<GraphDriver> driver = CurrentDriver();
          MonitorAutoUnlock unlock(mMonitor);
          driver->Revive();
        }
      }
    }

    if (LifecycleStateRef() == LIFECYCLE_THREAD_NOT_STARTED) {
      LifecycleStateRef() = LIFECYCLE_RUNNING;
      // Start the thread now. We couldn't start it earlier because
      // the graph might exit immediately on finding it has no streams. The
      // first message for a new graph must create a stream.
      {
        // We should exit the monitor for now, because starting a stream might
        // take locks, and we don't want to deadlock.
        LOG(LogLevel::Debug,
            ("%p: Starting a graph with a %s",
             this,
             CurrentDriver()->AsAudioCallbackDriver() ? "AudioCallbackDriver"
                                                      : "SystemClockDriver"));
        RefPtr<GraphDriver> driver = CurrentDriver();
        MonitorAutoUnlock unlock(mMonitor);
        driver->Start();
        // It's not safe to Shutdown() a thread from StableState, and
        // releasing this may shutdown a SystemClockDriver thread.
        // Proxy the release to outside of StableState.
        NS_ReleaseOnMainThreadSystemGroup(
          "MediaStreamGraphImpl::CurrentDriver", driver.forget(),
          true); // always proxy
      }
    }

    if ((mForceShutDown || !mRealtime) &&
        LifecycleStateRef() == LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP) {
      // Defer calls to RunDuringShutdown() to happen while mMonitor is not held.
      for (uint32_t i = 0; i < mBackMessageQueue.Length(); ++i) {
        MessageBlock& mb = mBackMessageQueue[i];
        controlMessagesToRunDuringShutdown.AppendElements(std::move(mb.mMessages));
      }
      mBackMessageQueue.Clear();
      MOZ_ASSERT(mCurrentTaskMessageQueue.IsEmpty());
      // Stop MediaStreamGraph threads. Do not clear gGraph since
      // we have outstanding DOM objects that may need it.
      LifecycleStateRef() = LIFECYCLE_WAITING_FOR_THREAD_SHUTDOWN;
      nsCOMPtr<nsIRunnable> event = new MediaStreamGraphShutDownRunnable(this);
      mAbstractMainThread->Dispatch(event.forget());
    }

    mDetectedNotRunning = LifecycleStateRef() > LIFECYCLE_RUNNING;
  }

  // Make sure we get a new current time in the next event loop task
  if (!aSourceIsMSG) {
    MOZ_ASSERT(mPostedRunInStableState);
    mPostedRunInStableState = false;
  }

  for (uint32_t i = 0; i < controlMessagesToRunDuringShutdown.Length(); ++i) {
    controlMessagesToRunDuringShutdown[i]->RunDuringShutdown();
  }

#ifdef DEBUG
  mCanRunMessagesSynchronously = mDetectedNotRunning &&
    LifecycleStateRef() >= LIFECYCLE_WAITING_FOR_THREAD_SHUTDOWN;
#endif

  for (uint32_t i = 0; i < runnables.Length(); ++i) {
    runnables[i]->Run();
  }
}


void
MediaStreamGraphImpl::EnsureRunInStableState()
{
  MOZ_ASSERT(NS_IsMainThread(), "main thread only");

  if (mPostedRunInStableState)
    return;
  mPostedRunInStableState = true;
  nsCOMPtr<nsIRunnable> event = new MediaStreamGraphStableStateRunnable(this, false);
  nsContentUtils::RunInStableState(event.forget());
}

void
MediaStreamGraphImpl::EnsureStableStateEventPosted()
{
  MOZ_ASSERT(OnGraphThread());
  mMonitor.AssertCurrentThreadOwns();

  if (mPostedRunInStableStateEvent)
    return;
  mPostedRunInStableStateEvent = true;
  nsCOMPtr<nsIRunnable> event = new MediaStreamGraphStableStateRunnable(this, true);
  mAbstractMainThread->Dispatch(event.forget());
}

void
MediaStreamGraphImpl::SignalMainThreadCleanup()
{
  MOZ_ASSERT(mDriver->OnThread());

  MonitorAutoLock lock(mMonitor);
  // LIFECYCLE_THREAD_NOT_STARTED is possible when shutting down offline
  // graphs that have not started.
  MOZ_DIAGNOSTIC_ASSERT(mLifecycleState <= LIFECYCLE_RUNNING);
  LOG(LogLevel::Debug,
      ("%p: MediaStreamGraph waiting for main thread cleanup", this));
  LifecycleStateRef() =
    MediaStreamGraphImpl::LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP;
  EnsureStableStateEventPosted();
}

void
MediaStreamGraphImpl::AppendMessage(UniquePtr<ControlMessage> aMessage)
{
  MOZ_ASSERT(NS_IsMainThread(), "main thread only");
  MOZ_ASSERT(!aMessage->GetStream() ||
             !aMessage->GetStream()->IsDestroyed(),
             "Stream already destroyed");

  if (mDetectedNotRunning &&
      LifecycleStateRef() > LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP) {
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
    if (IsEmpty() &&
        LifecycleStateRef() >= LIFECYCLE_WAITING_FOR_STREAM_DESTRUCTION) {

      // Find the graph in the hash table and remove it.
      for (auto iter = gGraphs.Iter(); !iter.Done(); iter.Next()) {
        if (iter.UserData() == this) {
          iter.Remove();
          break;
        }
      }

      Destroy();
    }
    return;
  }

  mCurrentTaskMessageQueue.AppendElement(std::move(aMessage));
  EnsureRunInStableState();
}

void
MediaStreamGraphImpl::Dispatch(already_AddRefed<nsIRunnable>&& aRunnable)
{
  mAbstractMainThread->Dispatch(std::move(aRunnable));
}

MediaStream::MediaStream()
  : mTracksStartTime(0)
  , mStartBlocking(GRAPH_TIME_MAX)
  , mSuspendedCount(0)
  , mFinished(false)
  , mNotifiedFinished(false)
  , mNotifiedBlocked(false)
  , mHasCurrentData(false)
  , mNotifiedHasCurrentData(false)
  , mMainThreadCurrentTime(0)
  , mMainThreadFinished(false)
  , mFinishedNotificationSent(false)
  , mMainThreadDestroyed(false)
  , mNrOfMainThreadUsers(0)
  , mGraph(nullptr)
{
  MOZ_COUNT_CTOR(MediaStream);
}

MediaStream::~MediaStream()
{
  MOZ_COUNT_DTOR(MediaStream);
  NS_ASSERTION(mMainThreadDestroyed, "Should have been destroyed already");
  NS_ASSERTION(mMainThreadListeners.IsEmpty(),
               "All main thread listeners should have been removed");
}

size_t
MediaStream::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t amount = 0;

  // Not owned:
  // - mGraph - Not reported here
  // - mConsumers - elements
  // Future:
  // - mVideoOutputs - elements
  // - mLastPlayedVideoFrame
  // - mListeners - elements
  // - mAudioOutputStream - elements

  amount += mTracks.SizeOfExcludingThis(aMallocSizeOf);
  amount += mAudioOutputs.ShallowSizeOfExcludingThis(aMallocSizeOf);
  amount += mVideoOutputs.ShallowSizeOfExcludingThis(aMallocSizeOf);
  amount += mListeners.ShallowSizeOfExcludingThis(aMallocSizeOf);
  amount += mMainThreadListeners.ShallowSizeOfExcludingThis(aMallocSizeOf);
  amount += mDisabledTracks.ShallowSizeOfExcludingThis(aMallocSizeOf);
  amount += mConsumers.ShallowSizeOfExcludingThis(aMallocSizeOf);

  return amount;
}

size_t
MediaStream::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

void
MediaStream::IncrementSuspendCount()
{
  ++mSuspendedCount;
  if (mSuspendedCount == 1) {
    for (uint32_t i = 0; i < mConsumers.Length(); ++i) {
      mConsumers[i]->Suspended();
    }
  }
}

void
MediaStream::DecrementSuspendCount()
{
    NS_ASSERTION(mSuspendedCount > 0, "Suspend count underrun");
    --mSuspendedCount;

  if (mSuspendedCount == 0) {
    for (uint32_t i = 0; i < mConsumers.Length(); ++i) {
      mConsumers[i]->Resumed();
    }
  }
}

MediaStreamGraphImpl*
MediaStream::GraphImpl()
{
  return mGraph;
}

const MediaStreamGraphImpl*
MediaStream::GraphImpl() const
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
  mTracks.InitGraphRate(aGraph->GraphRate());
}

void
MediaStream::SetGraphImpl(MediaStreamGraph* aGraph)
{
  MediaStreamGraphImpl* graph = static_cast<MediaStreamGraphImpl*>(aGraph);
  SetGraphImpl(graph);
}

StreamTime
MediaStream::GraphTimeToStreamTime(GraphTime aTime) const
{
  NS_ASSERTION(mStartBlocking == GraphImpl()->mStateComputedTime ||
               aTime <= mStartBlocking,
               "Incorrectly ignoring blocking!");
  return aTime - mTracksStartTime;
}

GraphTime
MediaStream::StreamTimeToGraphTime(StreamTime aTime) const
{
  NS_ASSERTION(mStartBlocking == GraphImpl()->mStateComputedTime ||
               aTime + mTracksStartTime <= mStartBlocking,
               "Incorrectly ignoring blocking!");
  return aTime + mTracksStartTime;
}

StreamTime
MediaStream::GraphTimeToStreamTimeWithBlocking(GraphTime aTime) const
{
  return GraphImpl()->GraphTimeToStreamTimeWithBlocking(this, aTime);
}

void
MediaStream::FinishOnGraphThread()
{
  if (mFinished) {
    return;
  }
  LOG(LogLevel::Debug, ("MediaStream %p will finish", this));
#ifdef DEBUG
  if (!mGraph->mForceShutDown) {
    // All tracks must be ended by the source before the stream finishes.
    // The exception is in forced shutdown, where we finish all streams as is.
    for (StreamTracks::TrackIter track(mTracks); !track.IsEnded(); track.Next()) {
      if (!track->IsEnded()) {
        LOG(LogLevel::Error,
            ("MediaStream %p will finish, but track %d has not ended.",
             this,
             track->GetID()));
        NS_ASSERTION(false, "Finished stream cannot contain live track");
      }
    }
  }
#endif
  mFinished = true;
  mTracks.AdvanceKnownTracksTime(STREAM_TIME_MAX);

  // Let the MSG knows that this stream can be destroyed if necessary to avoid
  // unnecessarily processing it in the future.
  GraphImpl()->SetStreamOrderDirty();
}

StreamTracks::Track*
MediaStream::FindTrack(TrackID aID) const
{
  return mTracks.FindTrack(aID);
}

StreamTracks::Track*
MediaStream::EnsureTrack(TrackID aTrackId)
{
  StreamTracks::Track* track = mTracks.FindTrack(aTrackId);
  if (!track) {
    nsAutoPtr<MediaSegment> segment(new AudioSegment());
    for (uint32_t j = 0; j < mListeners.Length(); ++j) {
      MediaStreamListener* l = mListeners[j];
      l->NotifyQueuedTrackChanges(Graph(), aTrackId, 0,
                                  TrackEventCommand::TRACK_EVENT_CREATED,
                                  *segment);
      // TODO If we ever need to ensure several tracks at once, we will have to
      // change this.
      l->NotifyFinishedTrackCreation(Graph());
    }
    track = &mTracks.AddTrack(aTrackId, 0, segment.forget());
  }
  return track;
}

void
MediaStream::RemoveAllListenersImpl()
{
  GraphImpl()->AssertOnGraphThreadOrNotRunning();

  auto streamListeners(mListeners);
  for (auto& l : streamListeners) {
    l->NotifyEvent(GraphImpl(), MediaStreamGraphEvent::EVENT_REMOVED);
  }
  mListeners.Clear();

  auto trackListeners(mTrackListeners);
  for (auto& l : trackListeners) {
    l.mListener->NotifyRemoved();
  }
  mTrackListeners.Clear();

  RemoveAllDirectListenersImpl();

  auto videoOutputs(mVideoOutputs);
  for (auto& l : videoOutputs) {
    l.mListener->NotifyRemoved();
  }
  mVideoOutputs.Clear();
}

void
MediaStream::DestroyImpl()
{
  for (int32_t i = mConsumers.Length() - 1; i >= 0; --i) {
    mConsumers[i]->Disconnect();
  }
  mTracks.Clear();
  mGraph = nullptr;
}

void
MediaStream::Destroy()
{
  NS_ASSERTION(mNrOfMainThreadUsers == 0,
               "Do not mix Destroy() and RegisterUser()/UnregisterUser()");
  // Keep this stream alive until we leave this method
  RefPtr<MediaStream> kungFuDeathGrip = this;

  class Message : public ControlMessage {
  public:
    explicit Message(MediaStream* aStream) : ControlMessage(aStream) {}
    void Run() override
    {
      mStream->RemoveAllListenersImpl();
      auto graph = mStream->GraphImpl();
      mStream->DestroyImpl();
      graph->RemoveStreamGraphThread(mStream);
    }
    void RunDuringShutdown() override
    { Run(); }
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this));
  // Message::RunDuringShutdown may have removed this stream from the graph,
  // but our kungFuDeathGrip above will have kept this stream alive if
  // necessary.
  mMainThreadDestroyed = true;
}

void
MediaStream::RegisterUser()
{
  MOZ_ASSERT(NS_IsMainThread());
  ++mNrOfMainThreadUsers;
}

void
MediaStream::UnregisterUser()
{
  MOZ_ASSERT(NS_IsMainThread());

  --mNrOfMainThreadUsers;
  NS_ASSERTION(mNrOfMainThreadUsers >= 0, "Double-removal of main thread user");
  NS_ASSERTION(!IsDestroyed(), "Do not mix Destroy() and RegisterUser()/UnregisterUser()");
  if (mNrOfMainThreadUsers == 0) {
    Destroy();
  }
}

void
MediaStream::AddAudioOutput(void* aKey)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, void* aKey) : ControlMessage(aStream), mKey(aKey) {}
    void Run() override
    {
      mStream->AddAudioOutputImpl(mKey);
    }
    void* mKey;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aKey));
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
    void Run() override
    {
      mStream->SetAudioOutputVolumeImpl(mKey, mVolume);
    }
    void* mKey;
    float mVolume;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aKey, aVolume));
}

void
MediaStream::AddAudioOutputImpl(void* aKey)
{
  LOG(LogLevel::Info,
      ("MediaStream %p Adding AudioOutput for key %p", this, aKey));
  mAudioOutputs.AppendElement(AudioOutput(aKey));
}

void
MediaStream::RemoveAudioOutputImpl(void* aKey)
{
  LOG(LogLevel::Info,
      ("MediaStream %p Removing AudioOutput for key %p", this, aKey));
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
    void Run() override
    {
      mStream->RemoveAudioOutputImpl(mKey);
    }
    void* mKey;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aKey));
}

void
MediaStream::AddVideoOutputImpl(already_AddRefed<MediaStreamVideoSink> aSink,
                                TrackID aID)
{
  RefPtr<MediaStreamVideoSink> sink = aSink;
  LOG(LogLevel::Info,
      ("MediaStream %p Adding MediaStreamVideoSink %p as output",
       this,
       sink.get()));
  MOZ_ASSERT(aID != TRACK_NONE);
   for (auto entry : mVideoOutputs) {
     if (entry.mListener == sink &&
         (entry.mTrackID == TRACK_ANY || entry.mTrackID == aID)) {
       return;
     }
   }
   TrackBound<MediaStreamVideoSink>* l = mVideoOutputs.AppendElement();
   l->mListener = sink;
   l->mTrackID = aID;

   AddDirectTrackListenerImpl(sink.forget(), aID);
}

void
MediaStream::RemoveVideoOutputImpl(MediaStreamVideoSink* aSink,
                                   TrackID aID)
{
  LOG(
    LogLevel::Info,
    ("MediaStream %p Removing MediaStreamVideoSink %p as output", this, aSink));
  MOZ_ASSERT(aID != TRACK_NONE);

  // Ensure that any frames currently queued for playback by the compositor
  // are removed.
  aSink->ClearFrames();
  for (size_t i = 0; i < mVideoOutputs.Length(); ++i) {
    if (mVideoOutputs[i].mListener == aSink &&
        (mVideoOutputs[i].mTrackID == TRACK_ANY ||
         mVideoOutputs[i].mTrackID == aID)) {
      mVideoOutputs.RemoveElementAt(i);
    }
  }

  RemoveDirectTrackListenerImpl(aSink, aID);
}

void
MediaStream::AddVideoOutput(MediaStreamVideoSink* aSink, TrackID aID)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, MediaStreamVideoSink* aSink, TrackID aID) :
      ControlMessage(aStream), mSink(aSink), mID(aID) {}
    void Run() override
    {
      mStream->AddVideoOutputImpl(mSink.forget(), mID);
    }
    RefPtr<MediaStreamVideoSink> mSink;
    TrackID mID;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aSink, aID));
}

void
MediaStream::RemoveVideoOutput(MediaStreamVideoSink* aSink, TrackID aID)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, MediaStreamVideoSink* aSink, TrackID aID) :
      ControlMessage(aStream), mSink(aSink), mID(aID) {}
    void Run() override
    {
      mStream->RemoveVideoOutputImpl(mSink, mID);
    }
    RefPtr<MediaStreamVideoSink> mSink;
    TrackID mID;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aSink, aID));
}

void
MediaStream::Suspend()
{
  class Message : public ControlMessage {
  public:
    explicit Message(MediaStream* aStream) :
      ControlMessage(aStream) {}
    void Run() override
    {
      mStream->GraphImpl()->IncrementSuspendCount(mStream);
    }
  };

  // This can happen if this method has been called asynchronously, and the
  // stream has been destroyed since then.
  if (mMainThreadDestroyed) {
    return;
  }
  GraphImpl()->AppendMessage(MakeUnique<Message>(this));
}

void
MediaStream::Resume()
{
  class Message : public ControlMessage {
  public:
    explicit Message(MediaStream* aStream) :
      ControlMessage(aStream) {}
    void Run() override
    {
      mStream->GraphImpl()->DecrementSuspendCount(mStream);
    }
  };

  // This can happen if this method has been called asynchronously, and the
  // stream has been destroyed since then.
  if (mMainThreadDestroyed) {
    return;
  }
  GraphImpl()->AppendMessage(MakeUnique<Message>(this));
}

void
MediaStream::AddListenerImpl(already_AddRefed<MediaStreamListener> aListener)
{
  MediaStreamListener* listener = *mListeners.AppendElement() = aListener;
  listener->NotifyBlockingChanged(GraphImpl(),
    mNotifiedBlocked ? MediaStreamListener::BLOCKED : MediaStreamListener::UNBLOCKED);

  for (StreamTracks::TrackIter it(mTracks); !it.IsEnded(); it.Next()) {
    MediaStream* inputStream = nullptr;
    TrackID inputTrackID = TRACK_INVALID;
    if (ProcessedMediaStream* ps = AsProcessedStream()) {
      // The only ProcessedMediaStream where we should have listeners is
      // TrackUnionStream - it's what's used as owned stream in DOMMediaStream,
      // the only main-thread exposed stream type.
      // TrackUnionStream guarantees that each of its tracks has an input track.
      // Other types do not implement GetInputStreamFor() and will return null.
      inputStream = ps->GetInputStreamFor(it->GetID());
      if (!inputStream && it->IsEnded()) {
        // If this track has no input anymore we assume there's no data for the
        // current iteration either and thus no need to expose it to a listener.
        continue;
      }
      MOZ_ASSERT(inputStream);
      inputTrackID = ps->GetInputTrackIDFor(it->GetID());
      MOZ_ASSERT(IsTrackIDExplicit(inputTrackID));
    }

    uint32_t flags = TrackEventCommand::TRACK_EVENT_CREATED;
    if (it->IsEnded()) {
      flags |= TrackEventCommand::TRACK_EVENT_ENDED;
    }
    nsAutoPtr<MediaSegment> segment(it->GetSegment()->CreateEmptyClone());
    listener->NotifyQueuedTrackChanges(Graph(), it->GetID(), it->GetEnd(),
                                       static_cast<TrackEventCommand>(flags), *segment,
                                       inputStream, inputTrackID);
  }
  if (mNotifiedFinished) {
    listener->NotifyEvent(GraphImpl(), MediaStreamGraphEvent::EVENT_FINISHED);
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
    void Run() override
    {
      mStream->AddListenerImpl(mListener.forget());
    }
    RefPtr<MediaStreamListener> mListener;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aListener));
}

void
MediaStream::RemoveListenerImpl(MediaStreamListener* aListener)
{
  // wouldn't need this if we could do it in the opposite order
  RefPtr<MediaStreamListener> listener(aListener);
  mListeners.RemoveElement(aListener);
  listener->NotifyEvent(GraphImpl(), MediaStreamGraphEvent::EVENT_REMOVED);
}

void
MediaStream::RemoveListener(MediaStreamListener* aListener)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, MediaStreamListener* aListener) :
      ControlMessage(aStream), mListener(aListener) {}
    void Run() override
    {
      mStream->RemoveListenerImpl(mListener);
    }
    RefPtr<MediaStreamListener> mListener;
  };
  // If the stream is destroyed the Listeners have or will be
  // removed.
  if (!IsDestroyed()) {
    GraphImpl()->AppendMessage(MakeUnique<Message>(this, aListener));
  }
}

void
MediaStream::AddTrackListenerImpl(already_AddRefed<MediaStreamTrackListener> aListener,
                                  TrackID aTrackID)
{
  TrackBound<MediaStreamTrackListener>* l = mTrackListeners.AppendElement();
  l->mListener = aListener;
  l->mTrackID = aTrackID;

  StreamTracks::Track* track = FindTrack(aTrackID);
  if (!track) {
    return;
  }
  PrincipalHandle lastPrincipalHandle =
    track->GetSegment()->GetLastPrincipalHandle();
  l->mListener->NotifyPrincipalHandleChanged(Graph(), lastPrincipalHandle);
  if (track->IsEnded() &&
      track->GetEnd() <= GraphTimeToStreamTime(GraphImpl()->mStateComputedTime)) {
    l->mListener->NotifyEnded();
  }
}

void
MediaStream::AddTrackListener(MediaStreamTrackListener* aListener,
                              TrackID aTrackID)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, MediaStreamTrackListener* aListener,
            TrackID aTrackID) :
      ControlMessage(aStream), mListener(aListener), mTrackID(aTrackID) {}
    void Run() override
    {
      mStream->AddTrackListenerImpl(mListener.forget(), mTrackID);
    }
    RefPtr<MediaStreamTrackListener> mListener;
    TrackID mTrackID;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aListener, aTrackID));
}

void
MediaStream::RemoveTrackListenerImpl(MediaStreamTrackListener* aListener,
                                     TrackID aTrackID)
{
  for (size_t i = 0; i < mTrackListeners.Length(); ++i) {
    if (mTrackListeners[i].mListener == aListener &&
        mTrackListeners[i].mTrackID == aTrackID) {
      mTrackListeners[i].mListener->NotifyRemoved();
      mTrackListeners.RemoveElementAt(i);
      return;
    }
  }
}

void
MediaStream::RemoveTrackListener(MediaStreamTrackListener* aListener,
                                 TrackID aTrackID)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, MediaStreamTrackListener* aListener,
            TrackID aTrackID) :
      ControlMessage(aStream), mListener(aListener), mTrackID(aTrackID) {}
    void Run() override
    {
      mStream->RemoveTrackListenerImpl(mListener, mTrackID);
    }
    void RunDuringShutdown() override
    {
      // During shutdown we still want the listener's NotifyRemoved to be
      // called, since not doing that might block shutdown of other modules.
      Run();
    }
    RefPtr<MediaStreamTrackListener> mListener;
    TrackID mTrackID;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aListener, aTrackID));
}

void
MediaStream::AddDirectTrackListenerImpl(already_AddRefed<DirectMediaStreamTrackListener> aListener,
                                        TrackID aTrackID)
{
  // Base implementation, for streams that don't support direct track listeners.
  RefPtr<DirectMediaStreamTrackListener> listener = aListener;
  listener->NotifyDirectListenerInstalled(
    DirectMediaStreamTrackListener::InstallationResult::STREAM_NOT_SUPPORTED);
}

void
MediaStream::AddDirectTrackListener(DirectMediaStreamTrackListener* aListener,
                                    TrackID aTrackID)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, DirectMediaStreamTrackListener* aListener,
            TrackID aTrackID) :
      ControlMessage(aStream), mListener(aListener), mTrackID(aTrackID) {}
    void Run() override
    {
      mStream->AddDirectTrackListenerImpl(mListener.forget(), mTrackID);
    }
    RefPtr<DirectMediaStreamTrackListener> mListener;
    TrackID mTrackID;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aListener, aTrackID));
}

void
MediaStream::RemoveDirectTrackListenerImpl(DirectMediaStreamTrackListener* aListener,
                                           TrackID aTrackID)
{
  // Base implementation, the listener was never added so nothing to do.
  RefPtr<DirectMediaStreamTrackListener> listener = aListener;
}

void
MediaStream::RemoveDirectTrackListener(DirectMediaStreamTrackListener* aListener,
                                       TrackID aTrackID)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, DirectMediaStreamTrackListener* aListener,
            TrackID aTrackID) :
      ControlMessage(aStream), mListener(aListener), mTrackID(aTrackID) {}
    void Run() override
    {
      mStream->RemoveDirectTrackListenerImpl(mListener, mTrackID);
    }
    void RunDuringShutdown() override
    {
      // During shutdown we still want the listener's
      // NotifyDirectListenerUninstalled to be called, since not doing that
      // might block shutdown of other modules.
      Run();
    }
    RefPtr<DirectMediaStreamTrackListener> mListener;
    TrackID mTrackID;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aListener, aTrackID));
}

void
MediaStream::RunAfterPendingUpdates(already_AddRefed<nsIRunnable> aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MediaStreamGraphImpl* graph = GraphImpl();
  nsCOMPtr<nsIRunnable> runnable(aRunnable);

  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, already_AddRefed<nsIRunnable> aRunnable)
      : ControlMessage(aStream)
      , mRunnable(aRunnable)
    {}
    void Run() override
    {
      mStream->Graph()->DispatchToMainThreadAfterStreamStateUpdate(
        mRunnable.forget());
    }
    void RunDuringShutdown() override
    {
      // Don't run mRunnable now as it may call AppendMessage() which would
      // assume that there are no remaining controlMessagesToRunDuringShutdown.
      MOZ_ASSERT(NS_IsMainThread());
      mStream->GraphImpl()->Dispatch(mRunnable.forget());
    }
  private:
    nsCOMPtr<nsIRunnable> mRunnable;
  };

  graph->AppendMessage(MakeUnique<Message>(this, runnable.forget()));
}

void
MediaStream::SetTrackEnabledImpl(TrackID aTrackID, DisabledTrackMode aMode)
{
  if (aMode == DisabledTrackMode::ENABLED) {
    for (int32_t i = mDisabledTracks.Length() - 1; i >= 0; --i) {
      if (aTrackID == mDisabledTracks[i].mTrackID) {
        mDisabledTracks.RemoveElementAt(i);
        return;
      }
    }
  } else {
    for (const DisabledTrack& t : mDisabledTracks) {
      if (aTrackID == t.mTrackID) {
        NS_ERROR("Changing disabled track mode for a track is not allowed");
        return;
      }
    }
    mDisabledTracks.AppendElement(DisabledTrack(aTrackID, aMode));
  }
}

DisabledTrackMode
MediaStream::GetDisabledTrackMode(TrackID aTrackID)
{
  for (const DisabledTrack& t : mDisabledTracks) {
    if (t.mTrackID == aTrackID) {
      return t.mMode;
    }
  }
  return DisabledTrackMode::ENABLED;
}

void
MediaStream::SetTrackEnabled(TrackID aTrackID, DisabledTrackMode aMode)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, TrackID aTrackID, DisabledTrackMode aMode) :
      ControlMessage(aStream),
      mTrackID(aTrackID),
      mMode(aMode) {}
    void Run() override
    {
      mStream->SetTrackEnabledImpl(mTrackID, mMode);
    }
    TrackID mTrackID;
    DisabledTrackMode mMode;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aTrackID, aMode));
}

void
MediaStream::ApplyTrackDisabling(TrackID aTrackID, MediaSegment* aSegment, MediaSegment* aRawSegment)
{
  DisabledTrackMode mode = GetDisabledTrackMode(aTrackID);
  if (mode == DisabledTrackMode::ENABLED) {
    return;
  }
  if (mode == DisabledTrackMode::SILENCE_BLACK) {
    aSegment->ReplaceWithDisabled();
    if (aRawSegment) {
      aRawSegment->ReplaceWithDisabled();
    }
  } else if (mode == DisabledTrackMode::SILENCE_FREEZE) {
    aSegment->ReplaceWithNull();
    if (aRawSegment) {
      aRawSegment->ReplaceWithNull();
    }
  } else {
    MOZ_CRASH("Unsupported mode");
  }
}

void
MediaStream::AddMainThreadListener(MainThreadMediaStreamListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aListener);
  MOZ_ASSERT(!mMainThreadListeners.Contains(aListener));

  mMainThreadListeners.AppendElement(aListener);

  // If it is not yet time to send the notification, then finish here.
  if (!mFinishedNotificationSent) {
    return;
  }

  class NotifyRunnable final : public Runnable
  {
  public:
    explicit NotifyRunnable(MediaStream* aStream)
      : Runnable("MediaStream::NotifyRunnable")
      , mStream(aStream)
    {}

    NS_IMETHOD Run() override
    {
      MOZ_ASSERT(NS_IsMainThread());
      mStream->NotifyMainThreadListeners();
      return NS_OK;
    }

  private:
    ~NotifyRunnable() {}

    RefPtr<MediaStream> mStream;
  };

  nsCOMPtr<nsIRunnable> runnable = new NotifyRunnable(this);
  GraphImpl()->Dispatch(runnable.forget());
}

SourceMediaStream::SourceMediaStream()
  : MediaStream()
  , mMutex("mozilla::media::SourceMediaStream")
  , mUpdateKnownTracksTime(0)
  , mPullEnabled(false)
  , mFinishPending(false)
{
}

nsresult
SourceMediaStream::OpenAudioInput(CubebUtils::AudioDeviceID aID,
                                  AudioDataListener *aListener)
{
  MOZ_ASSERT(GraphImpl());
  mInputListener = aListener;
  return GraphImpl()->OpenAudioInput(aID, aListener);
}

void
SourceMediaStream::CloseAudioInput(Maybe<CubebUtils::AudioDeviceID>& aID,
                                   AudioDataListener* aListener)
{
  MOZ_ASSERT(mInputListener == aListener);
  // Destroy() may have run already and cleared this
  if (GraphImpl() && mInputListener) {
    GraphImpl()->CloseAudioInput(aID, aListener);
  }
  mInputListener = nullptr;
}

void
SourceMediaStream::DestroyImpl()
{
  Maybe<CubebUtils::AudioDeviceID> id = Nothing();
  CloseAudioInput(id, mInputListener);

  GraphImpl()->AssertOnGraphThreadOrNotRunning();
  for (int32_t i = mConsumers.Length() - 1; i >= 0; --i) {
    // Disconnect before we come under mMutex's lock since it can call back
    // through RemoveDirectTrackListenerImpl() and deadlock.
    mConsumers[i]->Disconnect();
  }

  // Hold mMutex while mGraph is reset so that other threads holding mMutex
  // can null-check know that the graph will not destroyed.
  MutexAutoLock lock(mMutex);
  MediaStream::DestroyImpl();
}

void
SourceMediaStream::SetPullEnabled(bool aEnabled)
{
  class Message : public ControlMessage {
  public:
    Message(SourceMediaStream* aStream, bool aEnabled)
      : ControlMessage(nullptr)
      , mStream(aStream)
      , mEnabled(aEnabled)
    {}
    void Run() override
    {
      MutexAutoLock lock(mStream->mMutex);
      mStream->mPullEnabled = mEnabled;
    }
    SourceMediaStream* mStream;
    bool mEnabled;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aEnabled));
}

bool
SourceMediaStream::PullNewData(StreamTime aDesiredUpToTime)
{
  TRACE_AUDIO_CALLBACK_COMMENT("SourceMediaStream %p", this);
  MutexAutoLock lock(mMutex);
  if (!mPullEnabled || mFinished) {
    return false;
  }
  // Compute how much stream time we'll need assuming we don't block
  // the stream at all.
  StreamTime t = GraphTimeToStreamTime(aDesiredUpToTime);
  StreamTime current = mTracks.GetEnd();
  LOG(LogLevel::Verbose,
      ("%p: Calling NotifyPull aStream=%p t=%f current end=%f",
        GraphImpl(),
        this,
        GraphImpl()->MediaTimeToSeconds(t),
        GraphImpl()->MediaTimeToSeconds(current)));
  if (t <= current) {
    return false;
  }
  for (uint32_t j = 0; j < mListeners.Length(); ++j) {
    MediaStreamListener* l = mListeners[j];
    {
      MutexAutoUnlock unlock(mMutex);
      l->NotifyPull(GraphImpl(), t);
    }
  }
  return true;
}

void
SourceMediaStream::ExtractPendingInput()
{
  MutexAutoLock lock(mMutex);

  bool finished = mFinishPending;
  bool shouldNotifyTrackCreated = false;

  for (int32_t i = mUpdateTracks.Length() - 1; i >= 0; --i) {
    SourceMediaStream::TrackData* data = &mUpdateTracks[i];
    ApplyTrackDisabling(data->mID, data->mData);
    // Dealing with NotifyQueuedTrackChanges and NotifyQueuedAudioData part.

    // The logic is different from the manipulating of aStream->mTracks part.
    // So it is not combined with the manipulating of aStream->mTracks part.
    StreamTime offset =
      (data->mCommands & SourceMediaStream::TRACK_CREATE)
      ? data->mStart
      : mTracks.FindTrack(data->mID)->GetSegment()->GetDuration();

    // Audio case.
    if (data->mData->GetType() == MediaSegment::AUDIO) {
      if (data->mCommands) {
        MOZ_ASSERT(!(data->mCommands & SourceMediaStream::TRACK_UNUSED));
        for (MediaStreamListener* l : mListeners) {
          if (data->mCommands & SourceMediaStream::TRACK_END) {
            l->NotifyQueuedAudioData(
              GraphImpl(),
              data->mID,
              offset,
              *(static_cast<AudioSegment*>(data->mData.get())));
          }
          l->NotifyQueuedTrackChanges(
            GraphImpl(),
            data->mID,
            offset,
            static_cast<TrackEventCommand>(data->mCommands),
            *data->mData);
          if (data->mCommands & SourceMediaStream::TRACK_CREATE) {
            l->NotifyQueuedAudioData(
              GraphImpl(),
              data->mID,
              offset,
              *(static_cast<AudioSegment*>(data->mData.get())));
          }
        }
      } else {
        for (MediaStreamListener* l : mListeners) {
          l->NotifyQueuedAudioData(
            GraphImpl(),
            data->mID,
            offset,
            *(static_cast<AudioSegment*>(data->mData.get())));
        }
      }
    }

    // Video case.
    if (data->mData->GetType() == MediaSegment::VIDEO) {
      if (data->mCommands) {
        MOZ_ASSERT(!(data->mCommands & SourceMediaStream::TRACK_UNUSED));
        for (MediaStreamListener* l : mListeners) {
          l->NotifyQueuedTrackChanges(
            GraphImpl(),
            data->mID,
            offset,
            static_cast<TrackEventCommand>(data->mCommands),
            *data->mData);
        }
      }
    }

    for (TrackBound<MediaStreamTrackListener>& b : mTrackListeners) {
      if (b.mTrackID != data->mID) {
        continue;
      }
      b.mListener->NotifyQueuedChanges(GraphImpl(), offset, *data->mData);
      if (data->mCommands & SourceMediaStream::TRACK_END) {
        b.mListener->NotifyEnded();
      }
    }
    if (data->mCommands & SourceMediaStream::TRACK_CREATE) {
      MediaSegment* segment = data->mData.forget();
      LOG(LogLevel::Debug,
          ("%p: SourceMediaStream %p creating track %d, start %" PRId64
            ", initial end %" PRId64,
            GraphImpl(),
            this,
            data->mID,
            int64_t(data->mStart),
            int64_t(segment->GetDuration())));

      data->mEndOfFlushedData += segment->GetDuration();
      mTracks.AddTrack(data->mID, data->mStart, segment);
      // The track has taken ownership of data->mData, so let's replace
      // data->mData with an empty clone.
      data->mData = segment->CreateEmptyClone();
      data->mCommands &= ~SourceMediaStream::TRACK_CREATE;
      shouldNotifyTrackCreated = true;
    } else if (data->mData->GetDuration() > 0) {
      MediaSegment* dest = mTracks.FindTrack(data->mID)->GetSegment();
      LOG(LogLevel::Verbose,
          ("%p: SourceMediaStream %p track %d, advancing end from %" PRId64
            " to %" PRId64,
            GraphImpl(),
            this,
            data->mID,
            int64_t(dest->GetDuration()),
            int64_t(dest->GetDuration() + data->mData->GetDuration())));
      data->mEndOfFlushedData += data->mData->GetDuration();
      dest->AppendFrom(data->mData);
    }
    if (data->mCommands & SourceMediaStream::TRACK_END) {
      mTracks.FindTrack(data->mID)->SetEnded();
      mUpdateTracks.RemoveElementAt(i);
    }
  }
  if (shouldNotifyTrackCreated) {
    for (MediaStreamListener* l : mListeners) {
      l->NotifyFinishedTrackCreation(GraphImpl());
    }
  }
  if (!mFinished) {
    mTracks.AdvanceKnownTracksTime(mUpdateKnownTracksTime);
  }

  if (mTracks.GetEnd() > 0) {
    mHasCurrentData = true;
  }

  if (finished) {
    FinishOnGraphThread();
  }
}

void
SourceMediaStream::AddTrackInternal(TrackID aID, TrackRate aRate, StreamTime aStart,
                                    MediaSegment* aSegment, uint32_t aFlags)
{
  MutexAutoLock lock(mMutex);
  nsTArray<TrackData> *track_data = (aFlags & ADDTRACK_QUEUED) ?
                                    &mPendingTracks : &mUpdateTracks;
  TrackData* data = track_data->AppendElement();
  LOG(LogLevel::Debug,
      ("%p: AddTrackInternal: %lu/%lu",
       GraphImpl(),
       (long)mPendingTracks.Length(),
       (long)mUpdateTracks.Length()));
  data->mID = aID;
  data->mInputRate = aRate;
  data->mResamplerChannelCount = 0;
  data->mStart = aStart;
  data->mEndOfFlushedData = aStart;
  data->mCommands = TRACK_CREATE;
  data->mData = aSegment;
  ResampleAudioToGraphSampleRate(data, aSegment);
  if (!(aFlags & ADDTRACK_QUEUED) && GraphImpl()) {
    GraphImpl()->EnsureNextIteration();
  }
}

void
SourceMediaStream::AddAudioTrack(TrackID aID, TrackRate aRate, StreamTime aStart,
                                 AudioSegment* aSegment, uint32_t aFlags)
{
  AddTrackInternal(aID, aRate, aStart, aSegment, aFlags);
}

void
SourceMediaStream::FinishAddTracks()
{
  MutexAutoLock lock(mMutex);
  mUpdateTracks.AppendElements(std::move(mPendingTracks));
  LOG(LogLevel::Debug,
      ("%p: FinishAddTracks: %lu/%lu",
       GraphImpl(),
       (long)mPendingTracks.Length(),
       (long)mUpdateTracks.Length()));
  if (GraphImpl()) {
    GraphImpl()->EnsureNextIteration();
  }
}

void
SourceMediaStream::ResampleAudioToGraphSampleRate(TrackData* aTrackData, MediaSegment* aSegment)
{
  if (aSegment->GetType() != MediaSegment::AUDIO ||
      aTrackData->mInputRate == GraphImpl()->GraphRate()) {
    return;
  }
  AudioSegment* segment = static_cast<AudioSegment*>(aSegment);
  int channels = segment->ChannelCount();

  // If this segment is just silence, we delay instanciating the resampler. We
  // also need to recreate the resampler if the channel count changes.
  if (channels && aTrackData->mResamplerChannelCount != channels) {
    SpeexResamplerState* state = speex_resampler_init(channels,
        aTrackData->mInputRate,
        GraphImpl()->GraphRate(),
        SPEEX_RESAMPLER_QUALITY_MIN,
        nullptr);
    if (!state) {
      return;
    }
    aTrackData->mResampler.own(state);
    aTrackData->mResamplerChannelCount = channels;
  }
  segment->ResampleChunks(aTrackData->mResampler, aTrackData->mInputRate, GraphImpl()->GraphRate());
}

void
SourceMediaStream::AdvanceTimeVaryingValuesToCurrentTime(GraphTime aCurrentTime,
                                                         GraphTime aBlockedTime)
{
  MutexAutoLock lock(mMutex);
  mTracksStartTime += aBlockedTime;
  mStreamTracksStartTimeStamp += TimeDuration::FromSeconds(GraphImpl()->MediaTimeToSeconds(aBlockedTime));
  mTracks.ForgetUpTo(aCurrentTime - mTracksStartTime);
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
      GraphImpl()->EnsureNextIteration();
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
  mMutex.AssertCurrentThreadOwns();
  MOZ_ASSERT(aTrack);

  for (const TrackBound<DirectMediaStreamTrackListener>& source
         : mDirectTrackListeners) {
    if (aTrack->mID != source.mTrackID) {
      continue;
    }
    StreamTime offset = 0; // FIX! need a separate StreamTime.... or the end of the internal buffer
    source.mListener->NotifyRealtimeTrackDataAndApplyTrackDisabling(Graph(), offset, *aSegment);
  }
}

// These handle notifying all the listeners of an event
void
SourceMediaStream::NotifyListenersEventImpl(MediaStreamGraphEvent aEvent)
{
  for (uint32_t j = 0; j < mListeners.Length(); ++j) {
    MediaStreamListener* l = mListeners[j];
    l->NotifyEvent(GraphImpl(), aEvent);
  }
}

void
SourceMediaStream::NotifyListenersEvent(MediaStreamGraphEvent aNewEvent)
{
  class Message : public ControlMessage {
  public:
    Message(SourceMediaStream* aStream, MediaStreamGraphEvent aEvent) :
      ControlMessage(aStream), mEvent(aEvent) {}
    void Run() override
      {
        mStream->AsSourceStream()->NotifyListenersEventImpl(mEvent);
      }
    MediaStreamGraphEvent mEvent;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aNewEvent));
}

void
SourceMediaStream::AddDirectTrackListenerImpl(already_AddRefed<DirectMediaStreamTrackListener> aListener,
                                              TrackID aTrackID)
{
  MOZ_ASSERT(IsTrackIDExplicit(aTrackID));
  MutexAutoLock lock(mMutex);

  RefPtr<DirectMediaStreamTrackListener> listener = aListener;
  LOG(LogLevel::Debug,
      ("%p: Adding direct track listener %p bound to track %d to source stream %p",
       GraphImpl(),
       listener.get(),
       aTrackID,
       this));

  StreamTracks::Track* track = FindTrack(aTrackID);

  if (!track) {
    LOG(LogLevel::Warning,
        ("%p: Couldn't find source track for direct track listener %p",
         GraphImpl(),
         listener.get()));
    listener->NotifyDirectListenerInstalled(
      DirectMediaStreamTrackListener::InstallationResult::TRACK_NOT_FOUND_AT_SOURCE);
    return;
  }

  bool isAudio = track->GetType() == MediaSegment::AUDIO;
  bool isVideo = track->GetType() == MediaSegment::VIDEO;
  if (!isAudio && !isVideo) {
    LOG(
      LogLevel::Warning,
      ("%p: Source track for direct track listener %p is unknown", GraphImpl(), listener.get()));
    MOZ_ASSERT(false);
    return;
  }

  for (auto entry : mDirectTrackListeners) {
    if (entry.mListener == listener &&
        (entry.mTrackID == TRACK_ANY || entry.mTrackID == aTrackID)) {
      listener->NotifyDirectListenerInstalled(
        DirectMediaStreamTrackListener::InstallationResult::ALREADY_EXISTS);
      return;
    }
  }

  TrackBound<DirectMediaStreamTrackListener>* sourceListener =
    mDirectTrackListeners.AppendElement();
  sourceListener->mListener = listener;
  sourceListener->mTrackID = aTrackID;

  LOG(LogLevel::Debug, ("%p: Added direct track listener %p", GraphImpl(), listener.get()));
  listener->NotifyDirectListenerInstalled(
    DirectMediaStreamTrackListener::InstallationResult::SUCCESS);

  // Pass buffered data to the listener
  AudioSegment bufferedAudio;
  VideoSegment bufferedVideo;
  MediaSegment& bufferedData =
    isAudio ? static_cast<MediaSegment&>(bufferedAudio)
            : static_cast<MediaSegment&>(bufferedVideo);

  MediaSegment& trackSegment = *track->GetSegment();
  if (mTracks.GetForgottenDuration() < trackSegment.GetDuration()) {
    bufferedData.AppendSlice(trackSegment,
                             mTracks.GetForgottenDuration(),
                             trackSegment.GetDuration());
  }

  if (TrackData* updateData = FindDataForTrack(aTrackID)) {
    bufferedData.AppendSlice(*updateData->mData, 0, updateData->mData->GetDuration());
  }

  if (bufferedData.GetDuration() != 0) {
    listener->NotifyRealtimeTrackData(Graph(), 0, bufferedData);
  }
}

void
SourceMediaStream::RemoveDirectTrackListenerImpl(DirectMediaStreamTrackListener* aListener,
                                                 TrackID aTrackID)
{
  MutexAutoLock lock(mMutex);
  for (int32_t i = mDirectTrackListeners.Length() - 1; i >= 0; --i) {
    const TrackBound<DirectMediaStreamTrackListener>& source =
      mDirectTrackListeners[i];
    if (source.mListener == aListener && source.mTrackID == aTrackID) {
      aListener->NotifyDirectListenerUninstalled();
      mDirectTrackListeners.RemoveElementAt(i);
    }
  }
}

StreamTime
SourceMediaStream::GetEndOfAppendedData(TrackID aID)
{
  MutexAutoLock lock(mMutex);
  TrackData *track = FindDataForTrack(aID);
  if (track) {
    return track->mEndOfFlushedData + track->mData->GetDuration();
  }
  MOZ_CRASH("Track not found");
  return 0;
}

void
SourceMediaStream::EndTrack(TrackID aID)
{
  MutexAutoLock lock(mMutex);
  TrackData *track = FindDataForTrack(aID);
  if (track) {
    track->mCommands |= TrackEventCommand::TRACK_EVENT_ENDED;
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
SourceMediaStream::FinishPendingWithLockHeld()
{
  mMutex.AssertCurrentThreadOwns();
  mFinishPending = true;
  if (auto graph = GraphImpl()) {
    graph->EnsureNextIteration();
  }
}

void
SourceMediaStream::SetTrackEnabledImpl(TrackID aTrackID, DisabledTrackMode aMode)
{
  {
    MutexAutoLock lock(mMutex);
    for (TrackBound<DirectMediaStreamTrackListener>& l: mDirectTrackListeners) {
      if (l.mTrackID != aTrackID) {
        continue;
      }
      DisabledTrackMode oldMode = GetDisabledTrackMode(aTrackID);
      bool oldEnabled = oldMode == DisabledTrackMode::ENABLED;
      if (!oldEnabled && aMode == DisabledTrackMode::ENABLED) {
        LOG(LogLevel::Debug,
            ("%p: SourceMediaStream %p track %d setting "
             "direct listener enabled",
             GraphImpl(),
             this,
             aTrackID));
        l.mListener->DecreaseDisabled(oldMode);
      } else if (oldEnabled && aMode != DisabledTrackMode::ENABLED) {
        LOG(LogLevel::Debug,
            ("%p: SourceMediaStream %p track %d setting "
             "direct listener disabled",
             GraphImpl(),
             this,
             aTrackID));
        l.mListener->IncreaseDisabled(aMode);
      }
    }
  }
  MediaStream::SetTrackEnabledImpl(aTrackID, aMode);
}

void
SourceMediaStream::EndAllTrackAndFinish()
{
  MutexAutoLock lock(mMutex);
  for (uint32_t i = 0; i < mUpdateTracks.Length(); ++i) {
    SourceMediaStream::TrackData* data = &mUpdateTracks[i];
    data->mCommands |= TrackEventCommand::TRACK_EVENT_ENDED;
  }
  mPendingTracks.Clear();
  FinishPendingWithLockHeld();
  // we will call NotifyEvent() to let GetUserMedia know
}

void
SourceMediaStream::RemoveAllDirectListenersImpl()
{
  GraphImpl()->AssertOnGraphThreadOrNotRunning();

  auto directListeners(mDirectTrackListeners);
  for (auto& l : directListeners) {
    l.mListener->NotifyDirectListenerUninstalled();
  }
  mDirectTrackListeners.Clear();
}

SourceMediaStream::~SourceMediaStream()
{
}

bool
SourceMediaStream::HasPendingAudioTrack()
{
  MutexAutoLock lock(mMutex);
  bool audioTrackPresent = false;

  for (auto& data : mPendingTracks) {
    if (data.mData->GetType() == MediaSegment::AUDIO) {
      audioTrackPresent = true;
      break;
    }
  }

  return audioTrackPresent;
}

void
MediaInputPort::Init()
{
  LOG(LogLevel::Debug,
      ("%p: Adding MediaInputPort %p (from %p to %p)",
       mSource->GraphImpl(),
       this,
       mSource,
       mDest));
  mSource->AddConsumer(this);
  mDest->AddInput(this);
  // mPortCount decremented via MediaInputPort::Destroy's message
  ++mDest->GraphImpl()->mPortCount;
}

void
MediaInputPort::Disconnect()
{
  GraphImpl()->AssertOnGraphThreadOrNotRunning();
  NS_ASSERTION(!mSource == !mDest,
               "mSource must either both be null or both non-null");
  if (!mSource)
    return;

  mSource->RemoveConsumer(this);
  mDest->RemoveInput(this);
  mSource = nullptr;
  mDest = nullptr;

  GraphImpl()->SetStreamOrderDirty();
}

MediaInputPort::InputInterval
MediaInputPort::GetNextInputInterval(GraphTime aTime) const
{
  InputInterval result = { GRAPH_TIME_MAX, GRAPH_TIME_MAX, false };
  if (aTime >= mDest->mStartBlocking) {
    return result;
  }
  result.mStart = aTime;
  result.mEnd = mDest->mStartBlocking;
  result.mInputIsBlocked = aTime >= mSource->mStartBlocking;
  if (!result.mInputIsBlocked) {
    result.mEnd = std::min(result.mEnd, mSource->mStartBlocking);
  }
  return result;
}

void
MediaInputPort::Suspended()
{
  mDest->InputSuspended(this);
}

void
MediaInputPort::Resumed()
{
  mDest->InputResumed(this);
}

void
MediaInputPort::Destroy()
{
  class Message : public ControlMessage {
  public:
    explicit Message(MediaInputPort* aPort)
      : ControlMessage(nullptr), mPort(aPort) {}
    void Run() override
    {
      mPort->Disconnect();
      --mPort->GraphImpl()->mPortCount;
      mPort->SetGraphImpl(nullptr);
      NS_RELEASE(mPort);
    }
    void RunDuringShutdown() override
    {
      Run();
    }
    MediaInputPort* mPort;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this));
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

void
MediaInputPort::BlockSourceTrackIdImpl(TrackID aTrackId, BlockingMode aBlockingMode)
{
  mBlockedTracks.AppendElement(Pair<TrackID, BlockingMode>(aTrackId, aBlockingMode));
}

already_AddRefed<Pledge<bool>>
MediaInputPort::BlockSourceTrackId(TrackID aTrackId, BlockingMode aBlockingMode)
{
  class Message : public ControlMessage {
  public:
    Message(MediaInputPort* aPort,
            TrackID aTrackId,
            BlockingMode aBlockingMode,
            already_AddRefed<nsIRunnable> aRunnable)
      : ControlMessage(aPort->GetDestination())
      , mPort(aPort)
      , mTrackId(aTrackId)
      , mBlockingMode(aBlockingMode)
      , mRunnable(aRunnable)
    {
    }
    void Run() override
    {
      mPort->BlockSourceTrackIdImpl(mTrackId, mBlockingMode);
      if (mRunnable) {
        mStream->Graph()->DispatchToMainThreadAfterStreamStateUpdate(
          mRunnable.forget());
      }
    }
    void RunDuringShutdown() override
    {
      Run();
    }
    RefPtr<MediaInputPort> mPort;
    TrackID mTrackId;
    BlockingMode mBlockingMode;
    nsCOMPtr<nsIRunnable> mRunnable;
  };

  MOZ_ASSERT(IsTrackIDExplicit(aTrackId),
             "Only explicit TrackID is allowed");

  auto pledge = MakeRefPtr<Pledge<bool>>();
  nsCOMPtr<nsIRunnable> runnable = NewRunnableFrom([pledge]() {
    MOZ_ASSERT(NS_IsMainThread());
    pledge->Resolve(true);
    return NS_OK;
  });
  GraphImpl()->AppendMessage(
    MakeUnique<Message>(this, aTrackId, aBlockingMode, runnable.forget()));
  return pledge.forget();
}

already_AddRefed<MediaInputPort>
ProcessedMediaStream::AllocateInputPort(MediaStream* aStream, TrackID aTrackID,
                                        TrackID aDestTrackID,
                                        uint16_t aInputNumber, uint16_t aOutputNumber,
                                        nsTArray<TrackID>* aBlockedTracks)
{
  // This method creates two references to the MediaInputPort: one for
  // the main thread, and one for the MediaStreamGraph.
  class Message : public ControlMessage {
  public:
    explicit Message(MediaInputPort* aPort)
      : ControlMessage(aPort->GetDestination()),
        mPort(aPort) {}
    void Run() override
    {
      mPort->Init();
      // The graph holds its reference implicitly
      mPort->GraphImpl()->SetStreamOrderDirty();
      Unused << mPort.forget();
    }
    void RunDuringShutdown() override
    {
      Run();
    }
    RefPtr<MediaInputPort> mPort;
  };

  MOZ_ASSERT(aStream->GraphImpl() == GraphImpl());
  MOZ_ASSERT(aTrackID == TRACK_ANY || IsTrackIDExplicit(aTrackID),
             "Only TRACK_ANY and explicit ID are allowed for source track");
  MOZ_ASSERT(aDestTrackID == TRACK_ANY || IsTrackIDExplicit(aDestTrackID),
             "Only TRACK_ANY and explicit ID are allowed for destination track");
  MOZ_ASSERT(aTrackID != TRACK_ANY || aDestTrackID == TRACK_ANY,
             "Generic MediaInputPort cannot produce a single destination track");
  RefPtr<MediaInputPort> port = new MediaInputPort(
    aStream, aTrackID, this, aDestTrackID, aInputNumber, aOutputNumber);
  if (aBlockedTracks) {
    for (TrackID trackID : *aBlockedTracks) {
      port->BlockSourceTrackIdImpl(trackID, BlockingMode::CREATION);
    }
  }
  port->SetGraphImpl(GraphImpl());
  GraphImpl()->AppendMessage(MakeUnique<Message>(port));
  return port.forget();
}

void
ProcessedMediaStream::QueueSetAutofinish(bool aAutofinish)
{
  class Message : public ControlMessage {
  public:
    Message(ProcessedMediaStream* aStream, bool aAutofinish)
      : ControlMessage(aStream), mAutofinish(aAutofinish) {}
    void Run() override
    {
      static_cast<ProcessedMediaStream*>(mStream)->SetAutofinishImpl(mAutofinish);
    }
    bool mAutofinish;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aAutofinish));
}

void
ProcessedMediaStream::DestroyImpl()
{
  for (int32_t i = mInputs.Length() - 1; i >= 0; --i) {
    mInputs[i]->Disconnect();
  }

  for (int32_t i = mSuspendedInputs.Length() - 1; i >= 0; --i) {
    mSuspendedInputs[i]->Disconnect();
  }

  MediaStream::DestroyImpl();
  // The stream order is only important if there are connections, in which
  // case MediaInputPort::Disconnect() called SetStreamOrderDirty().
  // MediaStreamGraphImpl::RemoveStreamGraphThread() will also call
  // SetStreamOrderDirty(), for other reasons.
}

MediaStreamGraphImpl::MediaStreamGraphImpl(GraphDriverType aDriverRequested,
                                           TrackRate aSampleRate,
                                           AbstractThread* aMainThread)
  : MediaStreamGraph(aSampleRate)
  , mFirstCycleBreaker(0)
  // An offline graph is not initially processing.
  , mEndTime(aDriverRequested == OFFLINE_THREAD_DRIVER ? 0 : GRAPH_TIME_MAX)
  , mPortCount(0)
  , mInputDeviceID(nullptr)
  , mOutputDeviceID(nullptr)
  , mNeedAnotherIteration(false)
  , mGraphDriverAsleep(false)
  , mMonitor("MediaStreamGraphImpl")
  , mLifecycleState(LIFECYCLE_THREAD_NOT_STARTED)
  , mForceShutDown(false)
  , mPostedRunInStableStateEvent(false)
  , mDetectedNotRunning(false)
  , mPostedRunInStableState(false)
  , mRealtime(aDriverRequested != OFFLINE_THREAD_DRIVER)
  , mStreamOrderDirty(false)
  , mAbstractMainThread(aMainThread)
  , mSelfRef(this)
  , mOutputChannels(std::min<uint32_t>(8, CubebUtils::MaxNumberOfChannels()))
  , mGlobalVolume(CubebUtils::GetVolumeScale())
#ifdef DEBUG
  , mCanRunMessagesSynchronously(false)
#endif
{
  if (mRealtime) {
    if (aDriverRequested == AUDIO_THREAD_DRIVER) {
      // Always start with zero input channels.
      mDriver = new AudioCallbackDriver(this, 0);
    } else {
      mDriver = new SystemClockDriver(this);
    }

#ifdef TRACING
    // This is a noop if the logger has not been enabled.
    gMSGTraceLogger.Start();
    gMSGTraceLogger.Log("[");
#endif
  } else {
    mDriver = new OfflineClockDriver(this, MEDIA_GRAPH_TARGET_PERIOD_MS);
  }

  mLastMainThreadUpdate = TimeStamp::Now();

  RegisterWeakAsyncMemoryReporter(this);
}

AbstractThread*
MediaStreamGraph::AbstractMainThread()
{
  MOZ_ASSERT(static_cast<MediaStreamGraphImpl*>(this)->mAbstractMainThread);
  return static_cast<MediaStreamGraphImpl*>(this)->mAbstractMainThread;
}

void
MediaStreamGraphImpl::Destroy()
{
  // First unregister from memory reporting.
  UnregisterWeakMemoryReporter(this);

  // Clear the self reference which will destroy this instance if all
  // associated GraphDrivers are destroyed.
  mSelfRef = nullptr;
}

static
uint32_t WindowToHash(nsPIDOMWindowInner* aWindow,
                      TrackRate aSampleRate)
{
  uint32_t hashkey = 0;

  hashkey = AddToHash(hashkey, aWindow);
  hashkey = AddToHash(hashkey, aSampleRate);

  return hashkey;
}

MediaStreamGraph*
MediaStreamGraph::GetInstanceIfExists(nsPIDOMWindowInner* aWindow,
                                      TrackRate aSampleRate)
{
  MOZ_ASSERT(NS_IsMainThread(), "Main thread only");

  TrackRate sampleRate = aSampleRate ? aSampleRate : CubebUtils::PreferredSampleRate();
  uint32_t hashkey = WindowToHash(aWindow, sampleRate);

  MediaStreamGraphImpl* graph = nullptr;
  gGraphs.Get(hashkey, &graph);
  return graph;
}

MediaStreamGraph*
MediaStreamGraph::GetInstance(MediaStreamGraph::GraphDriverType aGraphDriverRequested,
                              nsPIDOMWindowInner* aWindow,
                              TrackRate aSampleRate)
{
  MOZ_ASSERT(NS_IsMainThread(), "Main thread only");

  TrackRate sampleRate = aSampleRate ? aSampleRate : CubebUtils::PreferredSampleRate();
  MediaStreamGraphImpl* graph =
    static_cast<MediaStreamGraphImpl*>(GetInstanceIfExists(aWindow, sampleRate));

  if (!graph) {
    if (!gMediaStreamGraphShutdownBlocker) {

      class Blocker : public media::ShutdownBlocker
      {
      public:
        Blocker()
        : media::ShutdownBlocker(NS_LITERAL_STRING(
            "MediaStreamGraph shutdown: blocking on msg thread"))
        {}

        NS_IMETHOD
        BlockShutdown(nsIAsyncShutdownClient* aProfileBeforeChange) override
        {
          // Distribute the global async shutdown blocker in a ticket. If there
          // are zero graphs then shutdown is unblocked when we go out of scope.
          auto ticket = MakeRefPtr<media::ShutdownTicket>(
              gMediaStreamGraphShutdownBlocker.get());
          gMediaStreamGraphShutdownBlocker = nullptr;

          for (auto iter = gGraphs.Iter(); !iter.Done(); iter.Next()) {
            iter.UserData()->ForceShutDown(ticket);
          }
          return NS_OK;
        }
      };

      gMediaStreamGraphShutdownBlocker = new Blocker();
      nsCOMPtr<nsIAsyncShutdownClient> barrier = media::GetShutdownBarrier();
      nsresult rv = barrier->
          AddBlocker(gMediaStreamGraphShutdownBlocker,
                     NS_LITERAL_STRING(__FILE__), __LINE__,
                     NS_LITERAL_STRING("MediaStreamGraph shutdown"));
      MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
    }

    AbstractThread* mainThread;
    if (aWindow) {
      mainThread = aWindow->AsGlobal()->AbstractMainThreadFor(TaskCategory::Other);
    } else {
      // Uncommon case, only for some old configuration of webspeech.
      mainThread = AbstractThread::MainThread();
    }
    graph = new MediaStreamGraphImpl(aGraphDriverRequested,
                                     sampleRate,
                                     mainThread);

    uint32_t hashkey = WindowToHash(aWindow, sampleRate);
    gGraphs.Put(hashkey, graph);

    LOG(LogLevel::Debug,
        ("Starting up MediaStreamGraph %p for window %p", graph, aWindow));
  }

  return graph;
}

MediaStreamGraph*
MediaStreamGraph::CreateNonRealtimeInstance(TrackRate aSampleRate,
                                            nsPIDOMWindowInner* aWindow)
{
  MOZ_ASSERT(NS_IsMainThread(), "Main thread only");

  MediaStreamGraphImpl* graph = new MediaStreamGraphImpl(
    OFFLINE_THREAD_DRIVER,
    aSampleRate,
    aWindow->AsGlobal()->AbstractMainThreadFor(TaskCategory::Other));

  LOG(LogLevel::Debug, ("Starting up Offline MediaStreamGraph %p", graph));

  return graph;
}

void
MediaStreamGraph::DestroyNonRealtimeInstance(MediaStreamGraph* aGraph)
{
  MOZ_ASSERT(NS_IsMainThread(), "Main thread only");
  MOZ_ASSERT(aGraph->IsNonRealtime(), "Should not destroy the global graph here");

  MediaStreamGraphImpl* graph = static_cast<MediaStreamGraphImpl*>(aGraph);

  graph->ForceShutDown(nullptr);
}

NS_IMPL_ISUPPORTS(MediaStreamGraphImpl, nsIMemoryReporter, nsITimerCallback,
                  nsINamed)

NS_IMETHODIMP
MediaStreamGraphImpl::CollectReports(nsIHandleReportCallback* aHandleReport,
                                     nsISupports* aData, bool aAnonymize)
{
  MOZ_ASSERT(NS_IsMainThread());
  {
    MonitorAutoLock mon(mMonitor);
    if (LifecycleStateRef() >= LIFECYCLE_WAITING_FOR_THREAD_SHUTDOWN) {
      // Shutting down, nothing to report.
      FinishCollectReports(aHandleReport, aData, nsTArray<AudioNodeSizes>());
      return NS_OK;
    }
  }

  class Message final : public ControlMessage {
  public:
    Message(MediaStreamGraphImpl *aGraph,
            nsIHandleReportCallback* aHandleReport,
            nsISupports *aHandlerData)
      : ControlMessage(nullptr)
      , mGraph(aGraph)
      , mHandleReport(aHandleReport)
      , mHandlerData(aHandlerData) {}
    void Run() override
    {
      mGraph->CollectSizesForMemoryReport(mHandleReport.forget(),
                                          mHandlerData.forget());
    }
    void RunDuringShutdown() override
    {
      // Run this message during shutdown too, so that endReports is called.
      Run();
    }
    MediaStreamGraphImpl *mGraph;
    // nsMemoryReporterManager keeps the callback and data alive only if it
    // does not time out.
    nsCOMPtr<nsIHandleReportCallback> mHandleReport;
    nsCOMPtr<nsISupports> mHandlerData;
  };

  AppendMessage(MakeUnique<Message>(this, aHandleReport, aData));

  return NS_OK;
}

void
MediaStreamGraphImpl::CollectSizesForMemoryReport(
  already_AddRefed<nsIHandleReportCallback> aHandleReport,
  already_AddRefed<nsISupports> aHandlerData)
{
  class FinishCollectRunnable final : public Runnable
  {
  public:
    explicit FinishCollectRunnable(
      already_AddRefed<nsIHandleReportCallback> aHandleReport,
      already_AddRefed<nsISupports> aHandlerData)
      : mozilla::Runnable("FinishCollectRunnable")
      , mHandleReport(aHandleReport)
      , mHandlerData(aHandlerData)
    {}

    NS_IMETHOD Run() override
    {
      MediaStreamGraphImpl::FinishCollectReports(mHandleReport, mHandlerData,
                                                 std::move(mAudioStreamSizes));
      return NS_OK;
    }

    nsTArray<AudioNodeSizes> mAudioStreamSizes;

  private:
    ~FinishCollectRunnable() {}

    // Avoiding nsCOMPtr because NSCAP_ASSERT_NO_QUERY_NEEDED in its
    // constructor modifies the ref-count, which cannot be done off main
    // thread.
    RefPtr<nsIHandleReportCallback> mHandleReport;
    RefPtr<nsISupports> mHandlerData;
  };

  RefPtr<FinishCollectRunnable> runnable =
    new FinishCollectRunnable(std::move(aHandleReport), std::move(aHandlerData));

  auto audioStreamSizes = &runnable->mAudioStreamSizes;

  for (MediaStream* s : AllStreams()) {
    AudioNodeStream* stream = s->AsAudioNodeStream();
    if (stream) {
      AudioNodeSizes* usage = audioStreamSizes->AppendElement();
      stream->SizeOfAudioNodesIncludingThis(MallocSizeOf, *usage);
    }
  }

  mAbstractMainThread->Dispatch(runnable.forget());
}

void
MediaStreamGraphImpl::
FinishCollectReports(nsIHandleReportCallback* aHandleReport, nsISupports* aData,
                     const nsTArray<AudioNodeSizes>& aAudioStreamSizes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIMemoryReporterManager> manager =
    do_GetService("@mozilla.org/memory-reporter-manager;1");

  if (!manager)
    return;

#define REPORT(_path, _amount, _desc) \
  aHandleReport->Callback(EmptyCString(), _path, KIND_HEAP, UNITS_BYTES, \
                          _amount, NS_LITERAL_CSTRING(_desc), aData);

  for (size_t i = 0; i < aAudioStreamSizes.Length(); i++) {
    const AudioNodeSizes& usage = aAudioStreamSizes[i];
    const char* const nodeType =
      usage.mNodeType ? usage.mNodeType : "<unknown>";

    nsPrintfCString enginePath("explicit/webaudio/audio-node/%s/engine-objects",
                               nodeType);
    REPORT(enginePath, usage.mEngine,
           "Memory used by AudioNode engine objects (Web Audio).");

    nsPrintfCString streamPath("explicit/webaudio/audio-node/%s/stream-objects",
                               nodeType);
    REPORT(streamPath, usage.mStream,
           "Memory used by AudioNode stream objects (Web Audio).");

  }

  size_t hrtfLoaders = WebCore::HRTFDatabaseLoader::sizeOfLoaders(MallocSizeOf);
  if (hrtfLoaders) {
    REPORT(NS_LITERAL_CSTRING(
             "explicit/webaudio/audio-node/PannerNode/hrtf-databases"),
           hrtfLoaders,
           "Memory used by PannerNode databases (Web Audio).");
  }

#undef REPORT

  manager->EndReport();
}

SourceMediaStream*
MediaStreamGraph::CreateSourceStream()
{
  SourceMediaStream* stream = new SourceMediaStream();
  AddStream(stream);
  return stream;
}

ProcessedMediaStream*
MediaStreamGraph::CreateTrackUnionStream()
{
  TrackUnionStream* stream = new TrackUnionStream();
  AddStream(stream);
  return stream;
}

ProcessedMediaStream*
MediaStreamGraph::CreateAudioCaptureStream(TrackID aTrackId)
{
  AudioCaptureStream* stream = new AudioCaptureStream(aTrackId);
  AddStream(stream);
  return stream;
}

void
MediaStreamGraph::AddStream(MediaStream* aStream)
{
  NS_ADDREF(aStream);
  MediaStreamGraphImpl* graph = static_cast<MediaStreamGraphImpl*>(this);
  aStream->SetGraphImpl(graph);
  graph->AppendMessage(MakeUnique<CreateMessage>(aStream));
}

class GraphStartedRunnable final : public Runnable
{
public:
  GraphStartedRunnable(AudioNodeStream* aStream, MediaStreamGraph* aGraph)
    : Runnable("GraphStartedRunnable")
    , mStream(aStream)
    , mGraph(aGraph)
  { }

  NS_IMETHOD Run() override {
    mGraph->NotifyWhenGraphStarted(mStream);
    return NS_OK;
  }

private:
  RefPtr<AudioNodeStream> mStream;
  MediaStreamGraph* mGraph;
};

void
MediaStreamGraph::NotifyWhenGraphStarted(AudioNodeStream* aStream)
{
  MOZ_ASSERT(NS_IsMainThread());

  class GraphStartedNotificationControlMessage : public ControlMessage
  {
  public:
    explicit GraphStartedNotificationControlMessage(AudioNodeStream* aStream)
      : ControlMessage(aStream)
    {
    }
    void Run() override
    {
      // This runs on the graph thread, so when this runs, and the current
      // driver is an AudioCallbackDriver, we know the audio hardware is
      // started. If not, we are going to switch soon, keep reposting this
      // ControlMessage.
      MediaStreamGraphImpl* graphImpl = mStream->GraphImpl();
      if (graphImpl->CurrentDriver()->AsAudioCallbackDriver()) {
        nsCOMPtr<nsIRunnable> event = new dom::StateChangeTask(
            mStream->AsAudioNodeStream(), nullptr, AudioContextState::Running);
        graphImpl->Dispatch(event.forget());
      } else {
        nsCOMPtr<nsIRunnable> event = new GraphStartedRunnable(
            mStream->AsAudioNodeStream(), mStream->Graph());
        graphImpl->Dispatch(event.forget());
      }
    }
    void RunDuringShutdown() override
    {
    }
  };

  if (!aStream->IsDestroyed()) {
    MediaStreamGraphImpl* graphImpl = static_cast<MediaStreamGraphImpl*>(this);
    graphImpl->AppendMessage(MakeUnique<GraphStartedNotificationControlMessage>(aStream));
  }
}

void
MediaStreamGraphImpl::IncrementSuspendCount(MediaStream* aStream)
{
  MOZ_ASSERT(OnGraphThreadOrNotRunning());
  if (!aStream->IsSuspended()) {
    MOZ_ASSERT(mStreams.Contains(aStream));
    mStreams.RemoveElement(aStream);
    mSuspendedStreams.AppendElement(aStream);
    SetStreamOrderDirty();
  }
  aStream->IncrementSuspendCount();
}

void
MediaStreamGraphImpl::DecrementSuspendCount(MediaStream* aStream)
{
  MOZ_ASSERT(OnGraphThreadOrNotRunning());
  bool wasSuspended = aStream->IsSuspended();
  aStream->DecrementSuspendCount();
  if (wasSuspended && !aStream->IsSuspended()) {
    MOZ_ASSERT(mSuspendedStreams.Contains(aStream));
    mSuspendedStreams.RemoveElement(aStream);
    mStreams.AppendElement(aStream);
    ProcessedMediaStream* ps = aStream->AsProcessedStream();
    if (ps) {
      ps->mCycleMarker = NOT_VISITED;
    }
    SetStreamOrderDirty();
  }
}

void
MediaStreamGraphImpl::SuspendOrResumeStreams(AudioContextOperation aAudioContextOperation,
                                             const nsTArray<MediaStream*>& aStreamSet)
{
  MOZ_ASSERT(OnGraphThreadOrNotRunning());
  // For our purpose, Suspend and Close are equivalent: we want to remove the
  // streams from the set of streams that are going to be processed.
  for (MediaStream* stream : aStreamSet) {
    if (aAudioContextOperation == AudioContextOperation::Resume) {
      DecrementSuspendCount(stream);
    } else {
      IncrementSuspendCount(stream);
    }
  }
  LOG(LogLevel::Debug,
      ("Moving streams between suspended and running"
       "state: mStreams: %zu, mSuspendedStreams: %zu",
       mStreams.Length(),
       mSuspendedStreams.Length()));
#ifdef DEBUG
  // The intersection of the two arrays should be null.
  for (uint32_t i = 0; i < mStreams.Length(); i++) {
    for (uint32_t j = 0; j < mSuspendedStreams.Length(); j++) {
      MOZ_ASSERT(
        mStreams[i] != mSuspendedStreams[j],
        "The suspended stream set and running stream set are not disjoint.");
    }
  }
#endif
}

void
MediaStreamGraphImpl::AudioContextOperationCompleted(MediaStream* aStream,
                                                     void* aPromise,
                                                     AudioContextOperation aOperation)
{
  // This can be called from the thread created to do cubeb operation, or the
  // MSG thread. The pointers passed back here are refcounted, so are still
  // alive.
  AudioContextState state;
  switch (aOperation) {
    case AudioContextOperation::Suspend:
      state = AudioContextState::Suspended;
      break;
    case AudioContextOperation::Resume:
      state = AudioContextState::Running;
      break;
    case AudioContextOperation::Close:
      state = AudioContextState::Closed;
      break;
    default: MOZ_CRASH("Not handled.");
  }

  nsCOMPtr<nsIRunnable> event = new dom::StateChangeTask(
      aStream->AsAudioNodeStream(), aPromise, state);
  mAbstractMainThread->Dispatch(event.forget());
}

void
MediaStreamGraphImpl::ApplyAudioContextOperationImpl(
    MediaStream* aDestinationStream, const nsTArray<MediaStream*>& aStreams,
    AudioContextOperation aOperation, void* aPromise)
{
  MOZ_ASSERT(OnGraphThread());

  SuspendOrResumeStreams(aOperation, aStreams);

  bool switching = false;
  GraphDriver* nextDriver = nullptr;
  {
    MonitorAutoLock lock(mMonitor);
    switching = CurrentDriver()->Switching();
    if (switching) {
      nextDriver = CurrentDriver()->NextDriver();
    }
  }

  // If we have suspended the last AudioContext, and we don't have other
  // streams that have audio, this graph will automatically switch to a
  // SystemCallbackDriver, because it can't find a MediaStream that has an audio
  // track. When resuming, force switching to an AudioCallbackDriver (if we're
  // not already switching). It would have happened at the next iteration
  // anyways, but doing this now save some time.
  if (aOperation == AudioContextOperation::Resume) {
    if (!CurrentDriver()->AsAudioCallbackDriver()) {
      AudioCallbackDriver* driver;
      if (switching) {
        MOZ_ASSERT(nextDriver->AsAudioCallbackDriver());
        driver = nextDriver->AsAudioCallbackDriver();
      } else {
        driver = new AudioCallbackDriver(this, AudioInputChannelCount());
        MonitorAutoLock lock(mMonitor);
        CurrentDriver()->SwitchAtNextIteration(driver);
      }
      driver->EnqueueStreamAndPromiseForOperation(aDestinationStream,
          aPromise, aOperation);
    } else {
      // We are resuming a context, but we are already using an
      // AudioCallbackDriver, we can resolve the promise now.
      AudioContextOperationCompleted(aDestinationStream, aPromise, aOperation);
    }
  }
  // Close, suspend: check if we are going to switch to a
  // SystemAudioCallbackDriver, and pass the promise to the AudioCallbackDriver
  // if that's the case, so it can notify the content.
  // This is the same logic as in UpdateStreamOrder, but it's simpler to have it
  // here as well so we don't have to store the Promise(s) on the Graph.
  if (aOperation != AudioContextOperation::Resume) {
    bool audioTrackPresent = AudioTrackPresent();

    if (!audioTrackPresent && CurrentDriver()->AsAudioCallbackDriver()) {
      CurrentDriver()->AsAudioCallbackDriver()->
        EnqueueStreamAndPromiseForOperation(aDestinationStream, aPromise,
                                            aOperation);

      SystemClockDriver* driver;
      if (nextDriver) {
        MOZ_ASSERT(!nextDriver->AsAudioCallbackDriver());
      } else {
        driver = new SystemClockDriver(this);
        MonitorAutoLock lock(mMonitor);
        CurrentDriver()->SwitchAtNextIteration(driver);
      }
      // We are closing or suspending an AudioContext, but we just got resumed.
      // Queue the operation on the next driver so that the ordering is
      // preserved.
    } else if (!audioTrackPresent && switching) {
      MOZ_ASSERT(nextDriver->AsAudioCallbackDriver() ||
                 nextDriver->AsSystemClockDriver()->IsFallback());
      if (nextDriver->AsAudioCallbackDriver()) {
        nextDriver->AsAudioCallbackDriver()->
          EnqueueStreamAndPromiseForOperation(aDestinationStream, aPromise,
                                              aOperation);
      } else {
        // If this is not an AudioCallbackDriver, this means we failed opening an
        // AudioCallbackDriver in the past, and we're constantly trying to re-open
        // an new audio stream, but are running this graph that has an audio track
        // off a SystemClockDriver for now to keep things moving.  This is the
        // case where we're trying to switch an an system driver (because suspend
        // or close have been called on an AudioContext, or we've closed the
        // page), but we're already running one. We can just resolve the promise
        // now: we're already running off a system thread.
        AudioContextOperationCompleted(aDestinationStream, aPromise, aOperation);
      }
    } else {
      // We are closing or suspending an AudioContext, but something else is
      // using the audio stream, we can resolve the promise now.
      AudioContextOperationCompleted(aDestinationStream, aPromise, aOperation);
    }
  }
}

void
MediaStreamGraph::ApplyAudioContextOperation(MediaStream* aDestinationStream,
                                             const nsTArray<MediaStream*>& aStreams,
                                             AudioContextOperation aOperation,
                                             void* aPromise)
{
  class AudioContextOperationControlMessage : public ControlMessage
  {
  public:
    AudioContextOperationControlMessage(MediaStream* aDestinationStream,
                                        const nsTArray<MediaStream*>& aStreams,
                                        AudioContextOperation aOperation,
                                        void* aPromise)
      : ControlMessage(aDestinationStream)
      , mStreams(aStreams)
      , mAudioContextOperation(aOperation)
      , mPromise(aPromise)
    {
    }
    void Run() override
    {
      mStream->GraphImpl()->ApplyAudioContextOperationImpl(mStream,
        mStreams, mAudioContextOperation, mPromise);
    }
    void RunDuringShutdown() override
    {
      MOZ_ASSERT(mAudioContextOperation == AudioContextOperation::Close,
                 "We should be reviving the graph?");
    }

  private:
    // We don't need strong references here for the same reason ControlMessage
    // doesn't.
    nsTArray<MediaStream*> mStreams;
    AudioContextOperation mAudioContextOperation;
    void* mPromise;
  };

  MediaStreamGraphImpl* graphImpl = static_cast<MediaStreamGraphImpl*>(this);
  graphImpl->AppendMessage(
    MakeUnique<AudioContextOperationControlMessage>(aDestinationStream, aStreams,
                                                    aOperation, aPromise));
}

bool
MediaStreamGraph::IsNonRealtime() const
{
  return !static_cast<const MediaStreamGraphImpl*>(this)->mRealtime;
}

void
MediaStreamGraph::StartNonRealtimeProcessing(uint32_t aTicksToProcess)
{
  MOZ_ASSERT(NS_IsMainThread(), "main thread only");

  MediaStreamGraphImpl* graph = static_cast<MediaStreamGraphImpl*>(this);
  NS_ASSERTION(!graph->mRealtime, "non-realtime only");

  class Message : public ControlMessage {
  public:
    explicit Message(MediaStreamGraphImpl* aGraph, uint32_t aTicksToProcess)
      : ControlMessage(nullptr)
      , mGraph(aGraph)
      , mTicksToProcess(aTicksToProcess)
    {}
    void Run() override
    {
      MOZ_ASSERT(mGraph->mEndTime == 0,
                 "StartNonRealtimeProcessing should be called only once");
      mGraph->mEndTime =
        mGraph->RoundUpToEndOfAudioBlock(mGraph->mStateComputedTime +
                                         mTicksToProcess);
    }
    // The graph owns this message.
    MediaStreamGraphImpl* MOZ_NON_OWNING_REF mGraph;
    uint32_t mTicksToProcess;
  };

  graph->AppendMessage(MakeUnique<Message>(graph, aTicksToProcess));
}

void
ProcessedMediaStream::AddInput(MediaInputPort* aPort)
{
  MediaStream* s = aPort->GetSource();
  if (!s->IsSuspended()) {
    mInputs.AppendElement(aPort);
  } else {
    mSuspendedInputs.AppendElement(aPort);
  }
  GraphImpl()->SetStreamOrderDirty();
}

void
ProcessedMediaStream::InputSuspended(MediaInputPort* aPort)
{
  GraphImpl()->AssertOnGraphThreadOrNotRunning();
  mInputs.RemoveElement(aPort);
  mSuspendedInputs.AppendElement(aPort);
  GraphImpl()->SetStreamOrderDirty();
}

void
ProcessedMediaStream::InputResumed(MediaInputPort* aPort)
{
  GraphImpl()->AssertOnGraphThreadOrNotRunning();
  mSuspendedInputs.RemoveElement(aPort);
  mInputs.AppendElement(aPort);
  GraphImpl()->SetStreamOrderDirty();
}

void
MediaStreamGraph::RegisterCaptureStreamForWindow(
    uint64_t aWindowId, ProcessedMediaStream* aCaptureStream)
{
  MOZ_ASSERT(NS_IsMainThread());
  MediaStreamGraphImpl* graphImpl = static_cast<MediaStreamGraphImpl*>(this);
  graphImpl->RegisterCaptureStreamForWindow(aWindowId, aCaptureStream);
}

void
MediaStreamGraphImpl::RegisterCaptureStreamForWindow(
  uint64_t aWindowId, ProcessedMediaStream* aCaptureStream)
{
  MOZ_ASSERT(NS_IsMainThread());
  WindowAndStream winAndStream;
  winAndStream.mWindowId = aWindowId;
  winAndStream.mCaptureStreamSink = aCaptureStream;
  mWindowCaptureStreams.AppendElement(winAndStream);
}

void
MediaStreamGraph::UnregisterCaptureStreamForWindow(uint64_t aWindowId)
{
  MOZ_ASSERT(NS_IsMainThread());
  MediaStreamGraphImpl* graphImpl = static_cast<MediaStreamGraphImpl*>(this);
  graphImpl->UnregisterCaptureStreamForWindow(aWindowId);
}

void
MediaStreamGraphImpl::UnregisterCaptureStreamForWindow(uint64_t aWindowId)
{
  MOZ_ASSERT(NS_IsMainThread());
  for (int32_t i = mWindowCaptureStreams.Length() - 1; i >= 0; i--) {
    if (mWindowCaptureStreams[i].mWindowId == aWindowId) {
      mWindowCaptureStreams.RemoveElementAt(i);
    }
  }
}

already_AddRefed<MediaInputPort>
MediaStreamGraph::ConnectToCaptureStream(uint64_t aWindowId,
                                         MediaStream* aMediaStream)
{
  return aMediaStream->GraphImpl()->ConnectToCaptureStream(aWindowId,
                                                           aMediaStream);
}

already_AddRefed<MediaInputPort>
MediaStreamGraphImpl::ConnectToCaptureStream(uint64_t aWindowId,
                                             MediaStream* aMediaStream)
{
  MOZ_ASSERT(NS_IsMainThread());
  for (uint32_t i = 0; i < mWindowCaptureStreams.Length(); i++) {
    if (mWindowCaptureStreams[i].mWindowId == aWindowId) {
      ProcessedMediaStream* sink = mWindowCaptureStreams[i].mCaptureStreamSink;
      return sink->AllocateInputPort(aMediaStream);
    }
  }
  return nullptr;
}

void
MediaStreamGraph::DispatchToMainThreadAfterStreamStateUpdate(
  already_AddRefed<nsIRunnable> aRunnable)
{
  AssertOnGraphThreadOrNotRunning();
  *mPendingUpdateRunnables.AppendElement() =
    AbstractMainThread()->CreateDirectTaskDrainer(std::move(aRunnable));
}

} // namespace mozilla
