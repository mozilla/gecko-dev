/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaDecoder.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/MathAlgorithms.h"
#include <limits>
#include "nsIObserver.h"
#include "nsTArray.h"
#include "VideoUtils.h"
#include "MediaDecoderStateMachine.h"
#include "ImageContainer.h"
#include "MediaResource.h"
#include "nsError.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPtr.h"
#include "nsIMemoryReporter.h"
#include "nsComponentManagerUtils.h"
#include <algorithm>
#include "MediaShutdownManager.h"
#include "AudioChannelService.h"
#include "mozilla/dom/AudioTrack.h"
#include "mozilla/dom/AudioTrackList.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/VideoTrack.h"
#include "mozilla/dom/VideoTrackList.h"

using namespace mozilla::dom;
using namespace mozilla::layers;
using namespace mozilla::media;

// Default timeout msecs until try to enter dormant state by heuristic.
static const int DEFAULT_HEURISTIC_DORMANT_TIMEOUT_MSECS = 60000;

namespace mozilla {

// Number of estimated seconds worth of data we need to have buffered
// ahead of the current playback position before we allow the media decoder
// to report that it can play through the entire media without the decode
// catching up with the download. Having this margin make the
// MediaDecoder::CanPlayThrough() calculation more stable in the case of
// fluctuating bitrates.
static const int64_t CAN_PLAY_THROUGH_MARGIN = 1;

// The amount of instability we tollerate in calls to
// MediaDecoder::UpdateEstimatedMediaDuration(); changes of duration
// less than this are ignored, as they're assumed to be the result of
// instability in the duration estimation.
static const uint64_t ESTIMATED_DURATION_FUZZ_FACTOR_USECS = USECS_PER_S / 2;

// avoid redefined macro in unified build
#undef DECODER_LOG

PRLogModuleInfo* gMediaDecoderLog;
#define DECODER_LOG(x, ...) \
  MOZ_LOG(gMediaDecoderLog, LogLevel::Debug, ("Decoder=%p " x, this, ##__VA_ARGS__))

static const char* const gPlayStateStr[] = {
  "START",
  "LOADING",
  "PAUSED",
  "PLAYING",
  "SEEKING",
  "ENDED",
  "SHUTDOWN"
};

class MediaMemoryTracker : public nsIMemoryReporter
{
  virtual ~MediaMemoryTracker();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER

  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf);

  MediaMemoryTracker();
  void InitMemoryReporter();

  static StaticRefPtr<MediaMemoryTracker> sUniqueInstance;

  static MediaMemoryTracker* UniqueInstance() {
    if (!sUniqueInstance) {
      sUniqueInstance = new MediaMemoryTracker();
      sUniqueInstance->InitMemoryReporter();
    }
    return sUniqueInstance;
  }

  typedef nsTArray<MediaDecoder*> DecodersArray;
  static DecodersArray& Decoders() {
    return UniqueInstance()->mDecoders;
  }

  DecodersArray mDecoders;

public:
  static void AddMediaDecoder(MediaDecoder* aDecoder)
  {
    Decoders().AppendElement(aDecoder);
  }

  static void RemoveMediaDecoder(MediaDecoder* aDecoder)
  {
    DecodersArray& decoders = Decoders();
    decoders.RemoveElement(aDecoder);
    if (decoders.IsEmpty()) {
      sUniqueInstance = nullptr;
    }
  }
};

StaticRefPtr<MediaMemoryTracker> MediaMemoryTracker::sUniqueInstance;

PRLogModuleInfo* gStateWatchingLog;
PRLogModuleInfo* gMediaPromiseLog;
PRLogModuleInfo* gMediaTimerLog;
PRLogModuleInfo* gMediaSampleLog;

void
MediaDecoder::InitStatics()
{
  AbstractThread::InitStatics();

  // Log modules.
  gMediaDecoderLog = PR_NewLogModule("MediaDecoder");
  gMediaPromiseLog = PR_NewLogModule("MediaPromise");
  gStateWatchingLog = PR_NewLogModule("StateWatching");
  gMediaTimerLog = PR_NewLogModule("MediaTimer");
  gMediaSampleLog = PR_NewLogModule("MediaSample");
}

NS_IMPL_ISUPPORTS(MediaMemoryTracker, nsIMemoryReporter)

NS_IMPL_ISUPPORTS(MediaDecoder, nsIObserver)

void MediaDecoder::NotifyOwnerActivityChanged()
{
  MOZ_ASSERT(NS_IsMainThread());
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());

  if (!mOwner) {
    NS_WARNING("MediaDecoder without a decoder owner, can't update dormant");
    return;
  }

  UpdateDormantState(false /* aDormantTimeout */, false /* aActivity */);
  // Start dormant timer if necessary
  StartDormantTimer();
}

bool
MediaDecoder::IsHeuristicDormantSupported() const
{
  return
#if defined(MOZ_EME) && defined(RELEASE_BUILD)
    // We disallow dormant for encrypted media on Beta and Release until
    // bug 1181864 is fixed.
    mInfo &&
    !mInfo->IsEncrypted() &&
#endif
    mIsHeuristicDormantSupported;
}

void MediaDecoder::UpdateDormantState(bool aDormantTimeout, bool aActivity)
{
  MOZ_ASSERT(NS_IsMainThread());
  GetReentrantMonitor().AssertCurrentThreadIn();

  if (!mDecoderStateMachine ||
      mPlayState == PLAY_STATE_SHUTDOWN ||
      !mOwner->GetVideoFrameContainer() ||
      (mOwner->GetMediaElement() && mOwner->GetMediaElement()->IsBeingDestroyed()) ||
      !mDecoderStateMachine->IsDormantNeeded())
  {
    return;
  }

  DECODER_LOG("UpdateDormantState aTimeout=%d aActivity=%d mIsDormant=%d "
              "ownerActive=%d ownerHidden=%d mIsHeuristicDormant=%d "
              "mPlayState=%s encrypted=%s",
              aDormantTimeout, aActivity, mIsDormant, mOwner->IsActive(),
              mOwner->IsHidden(), mIsHeuristicDormant, PlayStateStr(),
              (!mInfo ? "Unknown" : (mInfo->IsEncrypted() ? "1" : "0")));

  bool prevDormant = mIsDormant;
  mIsDormant = false;
  if (!mOwner->IsActive()) {
    mIsDormant = true;
  }
#ifdef MOZ_WIDGET_GONK
  if (mOwner->IsHidden()) {
    mIsDormant = true;
  }
#endif

  // Try to enable dormant by idle heuristic, when the owner is hidden.
  bool prevHeuristicDormant = mIsHeuristicDormant;
  mIsHeuristicDormant = false;
  if (IsHeuristicDormantSupported() && mOwner->IsHidden()) {
    if (aDormantTimeout && !aActivity &&
        (mPlayState == PLAY_STATE_PAUSED || IsEnded())) {
      // Enable heuristic dormant
      mIsHeuristicDormant = true;
    } else if(prevHeuristicDormant && !aActivity) {
      // Continue heuristic dormant
      mIsHeuristicDormant = true;
    }

    if (mIsHeuristicDormant) {
      mIsDormant = true;
    }
  }

  if (prevDormant == mIsDormant) {
    // No update to dormant state
    return;
  }

  if (mIsDormant) {
    DECODER_LOG("UpdateDormantState() entering DORMANT state");
    // enter dormant state
    RefPtr<nsRunnable> event =
      NS_NewRunnableMethodWithArg<bool>(
        mDecoderStateMachine,
        &MediaDecoderStateMachine::SetDormant,
        true);
    mDecoderStateMachine->TaskQueue()->Dispatch(event.forget());

    if (IsEnded()) {
      mWasEndedWhenEnteredDormant = true;
    }
    mNextState = mPlayState;
    ChangeState(PLAY_STATE_LOADING);
  } else {
    DECODER_LOG("UpdateDormantState() leaving DORMANT state");
    // exit dormant state
    // trigger to state machine.
    RefPtr<nsRunnable> event =
      NS_NewRunnableMethodWithArg<bool>(
        mDecoderStateMachine,
        &MediaDecoderStateMachine::SetDormant,
        false);
    mDecoderStateMachine->TaskQueue()->Dispatch(event.forget());
  }
}

void MediaDecoder::DormantTimerExpired(nsITimer* aTimer, void* aClosure)
{
  MOZ_ASSERT(aClosure);
  MediaDecoder* decoder = static_cast<MediaDecoder*>(aClosure);
  ReentrantMonitorAutoEnter mon(decoder->GetReentrantMonitor());
  decoder->UpdateDormantState(true /* aDormantTimeout */,
                              false /* aActivity */);
}

void MediaDecoder::StartDormantTimer()
{
  if (!IsHeuristicDormantSupported()) {
    return;
  }

  if (mIsHeuristicDormant ||
      mShuttingDown ||
      !mOwner ||
      !mOwner->IsHidden() ||
      (mPlayState != PLAY_STATE_PAUSED &&
       !IsEnded()))
  {
    return;
  }

  if (!mDormantTimer) {
    mDormantTimer = do_CreateInstance("@mozilla.org/timer;1");
  }
  mDormantTimer->InitWithFuncCallback(&MediaDecoder::DormantTimerExpired,
                                      this,
                                      mHeuristicDormantTimeout,
                                      nsITimer::TYPE_ONE_SHOT);
}

void MediaDecoder::CancelDormantTimer()
{
  if (mDormantTimer) {
    mDormantTimer->Cancel();
  }
}

void MediaDecoder::Pause()
{
  MOZ_ASSERT(NS_IsMainThread());
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  if (mPlayState == PLAY_STATE_LOADING ||
      IsEnded()) {
    mNextState = PLAY_STATE_PAUSED;
    return;
  }

  ChangeState(PLAY_STATE_PAUSED);
}

void MediaDecoder::SetVolume(double aVolume)
{
  MOZ_ASSERT(NS_IsMainThread());
  mVolume = aVolume;
}

void MediaDecoder::AddOutputStream(ProcessedMediaStream* aStream,
                                   bool aFinishWhenEnded)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDecoderStateMachine, "Must be called after Load().");
  mDecoderStateMachine->AddOutputStream(aStream, aFinishWhenEnded);
}

double MediaDecoder::GetDuration()
{
  MOZ_ASSERT(NS_IsMainThread());
  return mDuration;
}

AbstractCanonical<media::NullableTimeUnit>*
MediaDecoder::CanonicalDurationOrNull()
{
  MOZ_ASSERT(mDecoderStateMachine);
  return mDecoderStateMachine->CanonicalDuration();
}

void MediaDecoder::SetInfinite(bool aInfinite)
{
  MOZ_ASSERT(NS_IsMainThread());
  mInfiniteStream = aInfinite;
  DurationChanged();
}

bool MediaDecoder::IsInfinite()
{
  MOZ_ASSERT(NS_IsMainThread());
  return mInfiniteStream;
}

MediaDecoder::MediaDecoder() :
  mWatchManager(this, AbstractThread::MainThread()),
  mBuffered(AbstractThread::MainThread(), TimeIntervals(), "MediaDecoder::mBuffered (Mirror)"),
  mNextFrameStatus(AbstractThread::MainThread(),
                   MediaDecoderOwner::NEXT_FRAME_UNINITIALIZED,
                   "MediaDecoder::mNextFrameStatus (Mirror)"),
  mDecoderPosition(0),
  mPlaybackPosition(0),
  mLogicalPosition(0.0),
  mCurrentPosition(AbstractThread::MainThread(), 0, "MediaDecoder::mCurrentPosition (Mirror)"),
  mVolume(AbstractThread::MainThread(), 0.0, "MediaDecoder::mVolume (Canonical)"),
  mPlaybackRate(AbstractThread::MainThread(), 1.0, "MediaDecoder::mPlaybackRate (Canonical)"),
  mPreservesPitch(AbstractThread::MainThread(), true, "MediaDecoder::mPreservesPitch (Canonical)"),
  mDuration(std::numeric_limits<double>::quiet_NaN()),
  mStateMachineDuration(AbstractThread::MainThread(), NullableTimeUnit(), "MediaDecoder::mStateMachineDuration (Mirror)"),
  mMediaSeekable(true),
  mSameOriginMedia(false),
  mReentrantMonitor("media.decoder"),
  mEstimatedDuration(AbstractThread::MainThread(), NullableTimeUnit(),
                     "MediaDecoder::mEstimatedDuration (Canonical)"),
  mExplicitDuration(AbstractThread::MainThread(), Maybe<double>(),
                   "MediaDecoder::mExplicitDuration (Canonical)"),
  mPlayState(AbstractThread::MainThread(), PLAY_STATE_LOADING,
             "MediaDecoder::mPlayState (Canonical)"),
  mNextState(AbstractThread::MainThread(), PLAY_STATE_PAUSED,
             "MediaDecoder::mNextState (Canonical)"),
  mLogicallySeeking(AbstractThread::MainThread(), false,
             "MediaDecoder::mLogicallySeeking (Canonical)"),
  mIgnoreProgressData(false),
  mInfiniteStream(false),
  mOwner(nullptr),
  mPlaybackStatistics(new MediaChannelStatistics()),
  mPinnedForSeek(false),
  mShuttingDown(false),
  mPausedForPlaybackRateNull(false),
  mMinimizePreroll(false),
  mMediaTracksConstructed(false),
  mFiredMetadataLoaded(false),
  mIsDormant(false),
  mWasEndedWhenEnteredDormant(false),
  mIsHeuristicDormantSupported(
    Preferences::GetBool("media.decoder.heuristic.dormant.enabled", false)),
  mHeuristicDormantTimeout(
    Preferences::GetInt("media.decoder.heuristic.dormant.timeout",
                        DEFAULT_HEURISTIC_DORMANT_TIMEOUT_MSECS)),
  mIsHeuristicDormant(false)
{
  MOZ_COUNT_CTOR(MediaDecoder);
  MOZ_ASSERT(NS_IsMainThread());
  MediaMemoryTracker::AddMediaDecoder(this);

  mAudioChannel = AudioChannelService::GetDefaultAudioChannel();

  //
  // Initialize watchers.
  //

  // mDuration
  mWatchManager.Watch(mStateMachineDuration, &MediaDecoder::DurationChanged);

  // readyState
  mWatchManager.Watch(mPlayState, &MediaDecoder::UpdateReadyState);
  mWatchManager.Watch(mNextFrameStatus, &MediaDecoder::UpdateReadyState);

  // mLogicalPosition
  mWatchManager.Watch(mCurrentPosition, &MediaDecoder::UpdateLogicalPosition);
  mWatchManager.Watch(mPlayState, &MediaDecoder::UpdateLogicalPosition);
  mWatchManager.Watch(mLogicallySeeking, &MediaDecoder::UpdateLogicalPosition);
}

bool MediaDecoder::Init(MediaDecoderOwner* aOwner)
{
  MOZ_ASSERT(NS_IsMainThread());
  mOwner = aOwner;
  mVideoFrameContainer = aOwner->GetVideoFrameContainer();
  MediaShutdownManager::Instance().Register(this);
  return true;
}

void MediaDecoder::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mShuttingDown)
    return;

  mShuttingDown = true;

  // This changes the decoder state to SHUTDOWN and does other things
  // necessary to unblock the state machine thread if it's blocked, so
  // the asynchronous shutdown in nsDestroyStateMachine won't deadlock.
  if (mDecoderStateMachine) {
    mDecoderStateMachine->DispatchShutdown();
  }

  // Force any outstanding seek and byterange requests to complete
  // to prevent shutdown from deadlocking.
  if (mResource) {
    mResource->Close();
  }

  CancelDormantTimer();

  ChangeState(PLAY_STATE_SHUTDOWN);

  mOwner = nullptr;

  MediaShutdownManager::Instance().Unregister(this);
}

MediaDecoder::~MediaDecoder()
{
  MOZ_ASSERT(NS_IsMainThread());
  MediaMemoryTracker::RemoveMediaDecoder(this);
  UnpinForSeek();
  MOZ_COUNT_DTOR(MediaDecoder);
}

nsresult MediaDecoder::OpenResource(nsIStreamListener** aStreamListener)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (aStreamListener) {
    *aStreamListener = nullptr;
  }

  {
    // Hold the lock while we do this to set proper lock ordering
    // expectations for dynamic deadlock detectors: decoder lock(s)
    // should be grabbed before the cache lock
    ReentrantMonitorAutoEnter mon(GetReentrantMonitor());

    nsresult rv = mResource->Open(aStreamListener);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult MediaDecoder::Load(nsIStreamListener** aStreamListener,
                            MediaDecoder* aCloneDonor)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv = OpenResource(aStreamListener);
  NS_ENSURE_SUCCESS(rv, rv);

  SetStateMachine(CreateStateMachine());
  NS_ENSURE_TRUE(GetStateMachine(), NS_ERROR_FAILURE);

  return InitializeStateMachine(aCloneDonor);
}

nsresult MediaDecoder::InitializeStateMachine(MediaDecoder* aCloneDonor)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ASSERTION(mDecoderStateMachine, "Cannot initialize null state machine!");

  MediaDecoder* cloneDonor = static_cast<MediaDecoder*>(aCloneDonor);
  nsresult rv = mDecoderStateMachine->Init(
      cloneDonor ? cloneDonor->mDecoderStateMachine : nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  // If some parameters got set before the state machine got created,
  // set them now
  SetStateMachineParameters();

  return ScheduleStateMachine();
}

void MediaDecoder::SetStateMachineParameters()
{
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  if (mMinimizePreroll) {
    mDecoderStateMachine->DispatchMinimizePrerollUntilPlaybackStarts();
  }
}

void MediaDecoder::SetMinimizePrerollUntilPlaybackStarts()
{
  DECODER_LOG("SetMinimizePrerollUntilPlaybackStarts()");
  MOZ_ASSERT(NS_IsMainThread());
  mMinimizePreroll = true;

  // This needs to be called before we init the state machine, otherwise it will
  // have no effect.
  MOZ_DIAGNOSTIC_ASSERT(!mDecoderStateMachine);
}

nsresult MediaDecoder::ScheduleStateMachine()
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ASSERTION(mDecoderStateMachine,
               "Must have state machine to start state machine thread");
  NS_ENSURE_STATE(mDecoderStateMachine);

  if (mShuttingDown)
    return NS_OK;
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  mDecoderStateMachine->ScheduleStateMachineCrossThread();
  return NS_OK;
}

nsresult MediaDecoder::Play()
{
  MOZ_ASSERT(NS_IsMainThread());
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  UpdateDormantState(false /* aDormantTimeout */, true /* aActivity */);

  NS_ASSERTION(mDecoderStateMachine != nullptr, "Should have state machine.");
  if (mPausedForPlaybackRateNull) {
    return NS_OK;
  }
  ScheduleStateMachine();
  if (IsEnded()) {
    return Seek(0, SeekTarget::PrevSyncPoint);
  } else if (mPlayState == PLAY_STATE_LOADING) {
    mNextState = PLAY_STATE_PLAYING;
    return NS_OK;
  }

  ChangeState(PLAY_STATE_PLAYING);
  return NS_OK;
}

nsresult MediaDecoder::Seek(double aTime, SeekTarget::Type aSeekType)
{
  MOZ_ASSERT(NS_IsMainThread());
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  NS_ENSURE_TRUE(!mShuttingDown, NS_ERROR_FAILURE);

  UpdateDormantState(false /* aDormantTimeout */, true /* aActivity */);

  MOZ_ASSERT(aTime >= 0.0, "Cannot seek to a negative value.");

  int64_t timeUsecs = 0;
  nsresult rv = SecondsToUsecs(aTime, timeUsecs);
  NS_ENSURE_SUCCESS(rv, rv);

  mLogicalPosition = aTime;
  mWasEndedWhenEnteredDormant = false;

  mLogicallySeeking = true;
  SeekTarget target = SeekTarget(timeUsecs, aSeekType);
  CallSeek(target);

  if (mPlayState == PLAY_STATE_ENDED) {
    bool paused = false;
    if (mOwner) {
      paused = mOwner->GetPaused();
    }
    PinForSeek();
    ChangeState(paused ? PLAY_STATE_PAUSED : PLAY_STATE_PLAYING);
  }
  return NS_OK;
}

void MediaDecoder::CallSeek(const SeekTarget& aTarget)
{
  mSeekRequest.DisconnectIfExists();
  mSeekRequest.Begin(ProxyMediaCall(mDecoderStateMachine->TaskQueue(),
                                    mDecoderStateMachine.get(), __func__,
                                    &MediaDecoderStateMachine::Seek, aTarget)
    ->Then(AbstractThread::MainThread(), __func__, this,
           &MediaDecoder::OnSeekResolved, &MediaDecoder::OnSeekRejected));
}

double MediaDecoder::GetCurrentTime()
{
  MOZ_ASSERT(NS_IsMainThread());
  return mLogicalPosition;
}

already_AddRefed<nsIPrincipal> MediaDecoder::GetCurrentPrincipal()
{
  MOZ_ASSERT(NS_IsMainThread());
  return mResource ? mResource->GetCurrentPrincipal() : nullptr;
}

void MediaDecoder::QueueMetadata(int64_t aPublishTime,
                                 nsAutoPtr<MediaInfo> aInfo,
                                 nsAutoPtr<MetadataTags> aTags)
{
  MOZ_ASSERT(OnDecodeTaskQueue());
  GetReentrantMonitor().AssertCurrentThreadIn();
  mDecoderStateMachine->QueueMetadata(aPublishTime, aInfo, aTags);
}

bool
MediaDecoder::IsExpectingMoreData()
{
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());

  // If there's no resource, we're probably just getting set up.
  if (!mResource) {
    return true;
  }

  // If we've downloaded anything, we're not waiting for anything.
  if (mResource->IsDataCachedToEndOfResource(mDecoderPosition)) {
    return false;
  }

  // Otherwise, we should be getting data unless the stream is suspended.
  return !mResource->IsSuspended();
}

void MediaDecoder::MetadataLoaded(nsAutoPtr<MediaInfo> aInfo,
                                  nsAutoPtr<MetadataTags> aTags,
                                  MediaDecoderEventVisibility aEventVisibility)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mShuttingDown) {
    return;
  }

  DECODER_LOG("MetadataLoaded, channels=%u rate=%u hasAudio=%d hasVideo=%d",
              aInfo->mAudio.mChannels, aInfo->mAudio.mRate,
              aInfo->HasAudio(), aInfo->HasVideo());

  mInfo = aInfo.forget();
  ConstructMediaTracks();

  if (mOwner) {
    // Make sure the element and the frame (if any) are told about
    // our new size.
    Invalidate();
    if (aEventVisibility != MediaDecoderEventVisibility::Suppressed) {
      mFiredMetadataLoaded = true;
      mOwner->MetadataLoaded(mInfo, nsAutoPtr<const MetadataTags>(aTags.forget()));
    }
  }
}

const char*
MediaDecoder::PlayStateStr()
{
  switch (mPlayState) {
    case PLAY_STATE_START: return "PLAY_STATE_START";
    case PLAY_STATE_LOADING: return "PLAY_STATE_LOADING";
    case PLAY_STATE_PAUSED: return "PLAY_STATE_PAUSED";
    case PLAY_STATE_PLAYING: return "PLAY_STATE_PLAYING";
    case PLAY_STATE_ENDED: return "PLAY_STATE_ENDED";
    case PLAY_STATE_SHUTDOWN: return "PLAY_STATE_SHUTDOWN";
    default: return "INVALID_PLAY_STATE";
  }
}

void MediaDecoder::FirstFrameLoaded(nsAutoPtr<MediaInfo> aInfo,
                                    MediaDecoderEventVisibility aEventVisibility)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mShuttingDown) {
    return;
  }

  DECODER_LOG("FirstFrameLoaded, channels=%u rate=%u hasAudio=%d hasVideo=%d mPlayState=%s mIsDormant=%d",
              aInfo->mAudio.mChannels, aInfo->mAudio.mRate,
              aInfo->HasAudio(), aInfo->HasVideo(), PlayStateStr(), mIsDormant);

  mInfo = aInfo.forget();

  if (mOwner) {
    Invalidate();
    if (aEventVisibility != MediaDecoderEventVisibility::Suppressed) {
      mOwner->FirstFrameLoaded();
    }
  }

  // This can run cache callbacks.
  mResource->EnsureCacheUpToDate();

  // The element can run javascript via events
  // before reaching here, so only change the
  // state if we're still set to the original
  // loading state.
  if (mPlayState == PLAY_STATE_LOADING && !mIsDormant) {
    ChangeState(mNextState);
  }

  // Run NotifySuspendedStatusChanged now to give us a chance to notice
  // that autoplay should run.
  NotifySuspendedStatusChanged();
}

void MediaDecoder::ResetConnectionState()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mShuttingDown)
    return;

  if (mOwner) {
    // Notify the media element that connection gets lost.
    mOwner->ResetConnectionState();
  }

  // Since we have notified the media element the connection
  // lost event, the decoder will be reloaded when user tries
  // to play the Rtsp streaming next time.
  Shutdown();
}

void MediaDecoder::NetworkError()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mShuttingDown)
    return;

  if (mOwner)
    mOwner->NetworkError();

  Shutdown();
}

void MediaDecoder::DecodeError()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mShuttingDown)
    return;

  if (mOwner)
    mOwner->DecodeError();

  Shutdown();
}

void MediaDecoder::UpdateSameOriginStatus(bool aSameOrigin)
{
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  mSameOriginMedia = aSameOrigin;
}

bool MediaDecoder::IsSameOriginMedia()
{
  GetReentrantMonitor().AssertCurrentThreadIn();
  return mSameOriginMedia;
}

bool MediaDecoder::IsSeeking() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mLogicallySeeking;
}

bool MediaDecoder::IsEndedOrShutdown() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return IsEnded() || mPlayState == PLAY_STATE_SHUTDOWN;
}

bool MediaDecoder::IsEnded() const
{
  return mPlayState == PLAY_STATE_ENDED ||
         (mWasEndedWhenEnteredDormant && (mPlayState != PLAY_STATE_SHUTDOWN));
}

void MediaDecoder::PlaybackEnded()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mShuttingDown ||
      mLogicallySeeking ||
      mPlayState == PLAY_STATE_LOADING) {
    return;
  }

  ChangeState(PLAY_STATE_ENDED);
  InvalidateWithFlags(VideoFrameContainer::INVALIDATE_FORCE);

  if (mOwner)  {
    mOwner->PlaybackEnded();
  }

  // This must be called after |mOwner->PlaybackEnded()| call above, in order
  // to fire the required durationchange.
  if (IsInfinite()) {
    SetInfinite(false);
  }
}

NS_IMETHODIMP MediaDecoder::Observe(nsISupports *aSubjet,
                                        const char *aTopic,
                                        const char16_t *someData)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    Shutdown();
  }

  return NS_OK;
}

MediaDecoder::Statistics
MediaDecoder::GetStatistics()
{
  Statistics result;

  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  if (mResource) {
    result.mDownloadRate =
      mResource->GetDownloadRate(&result.mDownloadRateReliable);
    result.mDownloadPosition =
      mResource->GetCachedDataEnd(mDecoderPosition);
    result.mTotalBytes = mResource->GetLength();
    result.mPlaybackRate = ComputePlaybackRate(&result.mPlaybackRateReliable);
    result.mDecoderPosition = mDecoderPosition;
    result.mPlaybackPosition = mPlaybackPosition;
  }
  else {
    result.mDownloadRate = 0;
    result.mDownloadRateReliable = true;
    result.mPlaybackRate = 0;
    result.mPlaybackRateReliable = true;
    result.mDecoderPosition = 0;
    result.mPlaybackPosition = 0;
    result.mDownloadPosition = 0;
    result.mTotalBytes = 0;
  }

  return result;
}

double MediaDecoder::ComputePlaybackRate(bool* aReliable)
{
  GetReentrantMonitor().AssertCurrentThreadIn();
  MOZ_ASSERT(NS_IsMainThread() || OnStateMachineTaskQueue() || OnDecodeTaskQueue());

  int64_t length = mResource ? mResource->GetLength() : -1;
  if (!IsNaN(mDuration) && !mozilla::IsInfinite<double>(mDuration) && length >= 0) {
    *aReliable = true;
    return length / mDuration;
  }
  return mPlaybackStatistics->GetRateAtLastStop(aReliable);
}

void MediaDecoder::UpdatePlaybackRate()
{
  MOZ_ASSERT(NS_IsMainThread() || OnStateMachineTaskQueue());
  GetReentrantMonitor().AssertCurrentThreadIn();
  if (!mResource)
    return;
  bool reliable;
  uint32_t rate = uint32_t(ComputePlaybackRate(&reliable));
  if (reliable) {
    // Avoid passing a zero rate
    rate = std::max(rate, 1u);
  }
  else {
    // Set a minimum rate of 10,000 bytes per second ... sometimes we just
    // don't have good data
    rate = std::max(rate, 10000u);
  }
  mResource->SetPlaybackRate(rate);
}

void MediaDecoder::NotifySuspendedStatusChanged()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mResource && mOwner) {
    bool suspended = mResource->IsSuspendedByCache();
    mOwner->NotifySuspendedByCache(suspended);
  }
}

void MediaDecoder::NotifyBytesDownloaded()
{
  MOZ_ASSERT(NS_IsMainThread());
  {
    ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
    UpdatePlaybackRate();
  }
  if (mOwner) {
    mOwner->DownloadProgressed();
  }
}

void MediaDecoder::NotifyDownloadEnded(nsresult aStatus)
{
  MOZ_ASSERT(NS_IsMainThread());

  DECODER_LOG("NotifyDownloadEnded, status=%x", aStatus);

  if (aStatus == NS_BINDING_ABORTED) {
    // Download has been cancelled by user.
    if (mOwner) {
      mOwner->LoadAborted();
    }
    return;
  }

  {
    ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
    UpdatePlaybackRate();
  }

  if (NS_SUCCEEDED(aStatus)) {
    // A final progress event will be fired by the MediaResource calling
    // DownloadSuspended on the element.
    // Also NotifySuspendedStatusChanged() will be called to update readyState
    // if download ended with success.
  } else if (aStatus != NS_BASE_STREAM_CLOSED) {
    NetworkError();
  }
}

void MediaDecoder::NotifyPrincipalChanged()
{
  if (mOwner) {
    mOwner->NotifyDecoderPrincipalChanged();
  }
}

void MediaDecoder::NotifyBytesConsumed(int64_t aBytes, int64_t aOffset)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mShuttingDown) {
    return;
  }

  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  MOZ_ASSERT(mDecoderStateMachine);
  if (mIgnoreProgressData) {
    return;
  }
  if (aOffset >= mDecoderPosition) {
    mPlaybackStatistics->AddBytes(aBytes);
  }
  mDecoderPosition = aOffset + aBytes;
}

void MediaDecoder::OnSeekResolved(SeekResolveValue aVal)
{
  MOZ_ASSERT(NS_IsMainThread());
  mSeekRequest.Complete();

  if (mShuttingDown)
    return;

  bool fireEnded = false;
  {
    ReentrantMonitorAutoEnter mon(GetReentrantMonitor());

    // An additional seek was requested while the current seek was
    // in operation.
    UnpinForSeek();
    fireEnded = aVal.mAtEnd;
    if (aVal.mAtEnd) {
      ChangeState(PLAY_STATE_ENDED);
    }
    mLogicallySeeking = false;
  }

  UpdateLogicalPosition(aVal.mEventVisibility);

  if (mOwner) {
    if (aVal.mEventVisibility != MediaDecoderEventVisibility::Suppressed) {
      mOwner->SeekCompleted();
      if (fireEnded) {
        mOwner->PlaybackEnded();
      }
    }
  }
}

void MediaDecoder::SeekingStarted(MediaDecoderEventVisibility aEventVisibility)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mShuttingDown)
    return;

  if (mOwner) {
    if (aEventVisibility != MediaDecoderEventVisibility::Suppressed) {
      mOwner->SeekStarted();
    }
  }
}

void MediaDecoder::ChangeState(PlayState aState)
{
  MOZ_ASSERT(NS_IsMainThread());
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());

  if (mNextState == aState) {
    mNextState = PLAY_STATE_PAUSED;
  }

  if (mPlayState == PLAY_STATE_SHUTDOWN) {
    GetReentrantMonitor().NotifyAll();
    return;
  }

  DECODER_LOG("ChangeState %s => %s",
              gPlayStateStr[mPlayState], gPlayStateStr[aState]);
  mPlayState = aState;

  if (mPlayState == PLAY_STATE_PLAYING) {
    ConstructMediaTracks();
  } else if (IsEnded()) {
    RemoveMediaTracks();
  }

  ScheduleStateMachine();

  CancelDormantTimer();
  // Start dormant timer if necessary
  StartDormantTimer();

  GetReentrantMonitor().NotifyAll();
}

void MediaDecoder::UpdateLogicalPosition(MediaDecoderEventVisibility aEventVisibility)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mShuttingDown)
    return;

  // Per spec, offical position remains stable during pause and seek.
  if (mPlayState == PLAY_STATE_PAUSED || IsSeeking()) {
    return;
  }

  double currentPosition = static_cast<double>(CurrentPosition()) / static_cast<double>(USECS_PER_S);
  bool logicalPositionChanged = mLogicalPosition != currentPosition;
  mLogicalPosition = currentPosition;

  // Invalidate the frame so any video data is displayed.
  // Do this before the timeupdate event so that if that
  // event runs JavaScript that queries the media size, the
  // frame has reflowed and the size updated beforehand.
  Invalidate();

  if (mOwner && logicalPositionChanged &&
      aEventVisibility != MediaDecoderEventVisibility::Suppressed) {
    FireTimeUpdate();
  }
}

void MediaDecoder::DurationChanged()
{
  MOZ_ASSERT(NS_IsMainThread());
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());

  double oldDuration = mDuration;
  if (IsInfinite()) {
    mDuration = std::numeric_limits<double>::infinity();
  } else if (mExplicitDuration.Ref().isSome()) {
    mDuration = mExplicitDuration.Ref().ref();
  } else if (mStateMachineDuration.Ref().isSome()) {
    mDuration = mStateMachineDuration.Ref().ref().ToSeconds();
  }

  if (mDuration == oldDuration || IsNaN(mDuration)) {
    return;
  }

  DECODER_LOG("Duration changed to %f", mDuration);

  // Duration has changed so we should recompute playback rate
  UpdatePlaybackRate();

  // See https://www.w3.org/Bugs/Public/show_bug.cgi?id=28822 for a discussion
  // of whether we should fire durationchange on explicit infinity.
  if (mOwner && mFiredMetadataLoaded &&
      (!mozilla::IsInfinite<double>(mDuration) || mExplicitDuration.Ref().isSome())) {
    mOwner->DispatchAsyncEvent(NS_LITERAL_STRING("durationchange"));
  }

  if (CurrentPosition() > TimeUnit::FromSeconds(mDuration).ToMicroseconds()) {
    Seek(mDuration, SeekTarget::Accurate);
  }
}

void MediaDecoder::UpdateEstimatedMediaDuration(int64_t aDuration)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mPlayState <= PLAY_STATE_LOADING) {
    return;
  }

  // The duration is only changed if its significantly different than the
  // the current estimate, as the incoming duration is an estimate and so
  // often is unstable as more data is read and the estimate is updated.
  // Can result in a durationchangeevent. aDuration is in microseconds.
  if (mEstimatedDuration.Ref().isSome() &&
      mozilla::Abs(mEstimatedDuration.Ref().ref().ToMicroseconds() - aDuration) < ESTIMATED_DURATION_FUZZ_FACTOR_USECS) {
    return;
  }

  mEstimatedDuration = Some(TimeUnit::FromMicroseconds(aDuration));
}

void MediaDecoder::SetMediaSeekable(bool aMediaSeekable) {
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  mMediaSeekable = aMediaSeekable;
}

bool
MediaDecoder::IsTransportSeekable()
{
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  return GetResource()->IsTransportSeekable();
}

bool MediaDecoder::IsMediaSeekable()
{
  NS_ENSURE_TRUE(GetStateMachine(), false);
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  return mMediaSeekable;
}

media::TimeIntervals MediaDecoder::GetSeekable()
{
  // We can seek in buffered range if the media is seekable. Also, we can seek
  // in unbuffered ranges if the transport level is seekable (local file or the
  // server supports range requests, etc.)
  if (!IsMediaSeekable()) {
    return media::TimeIntervals();
  } else if (!IsTransportSeekable()) {
    return GetBuffered();
  } else {
    return media::TimeIntervals(
      media::TimeInterval(media::TimeUnit::FromMicroseconds(0),
                          IsInfinite() ?
                            media::TimeUnit::FromInfinity() :
                            media::TimeUnit::FromSeconds(GetDuration())));
  }
}

void MediaDecoder::SetFragmentEndTime(double aTime)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mDecoderStateMachine) {
    ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
    mDecoderStateMachine->SetFragmentEndTime(static_cast<int64_t>(aTime * USECS_PER_S));
  }
}

void MediaDecoder::Suspend()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mResource) {
    mResource->Suspend(true);
  }
}

void MediaDecoder::Resume(bool aForceBuffering)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mResource) {
    mResource->Resume();
  }
  if (aForceBuffering) {
    if (mDecoderStateMachine) {
      mDecoderStateMachine->DispatchStartBuffering();
    }
  }
}

void MediaDecoder::StopProgressUpdates()
{
  MOZ_ASSERT(OnStateMachineTaskQueue() || OnDecodeTaskQueue());
  GetReentrantMonitor().AssertCurrentThreadIn();
  mIgnoreProgressData = true;
  if (mResource) {
    mResource->SetReadMode(MediaCacheStream::MODE_METADATA);
  }
}

void MediaDecoder::StartProgressUpdates()
{
  MOZ_ASSERT(OnStateMachineTaskQueue() || OnDecodeTaskQueue());
  GetReentrantMonitor().AssertCurrentThreadIn();
  mIgnoreProgressData = false;
  if (mResource) {
    mResource->SetReadMode(MediaCacheStream::MODE_PLAYBACK);
  }
}

void MediaDecoder::SetLoadInBackground(bool aLoadInBackground)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mResource) {
    mResource->SetLoadInBackground(aLoadInBackground);
  }
}

void MediaDecoder::UpdatePlaybackOffset(int64_t aOffset)
{
  GetReentrantMonitor().AssertCurrentThreadIn();
  mPlaybackPosition = aOffset;
}

bool MediaDecoder::OnStateMachineTaskQueue() const
{
  return mDecoderStateMachine->OnTaskQueue();
}

void MediaDecoder::SetPlaybackRate(double aPlaybackRate)
{
  mPlaybackRate = aPlaybackRate;
  if (mPlaybackRate == 0.0) {
    mPausedForPlaybackRateNull = true;
    Pause();
  } else if (mPausedForPlaybackRateNull) {
    // Play() uses mPausedForPlaybackRateNull value, so must reset it first
    mPausedForPlaybackRateNull = false;
    // If the playbackRate is no longer null, restart the playback, iff the
    // media was playing.
    if (mOwner && !mOwner->GetPaused()) {
      Play();
    }
  }
}

void MediaDecoder::SetPreservesPitch(bool aPreservesPitch)
{
  mPreservesPitch = aPreservesPitch;
}

bool MediaDecoder::OnDecodeTaskQueue() const {
  NS_WARN_IF_FALSE(mDecoderStateMachine, "mDecoderStateMachine is null");
  return mDecoderStateMachine ? mDecoderStateMachine->OnDecodeTaskQueue() : false;
}

void
MediaDecoder::SetStateMachine(MediaDecoderStateMachine* aStateMachine)
{
  MOZ_ASSERT_IF(aStateMachine, !mDecoderStateMachine);
  mDecoderStateMachine = aStateMachine;

  if (mDecoderStateMachine) {
    mStateMachineDuration.Connect(mDecoderStateMachine->CanonicalDuration());
    mBuffered.Connect(mDecoderStateMachine->CanonicalBuffered());
    mNextFrameStatus.Connect(mDecoderStateMachine->CanonicalNextFrameStatus());
    mCurrentPosition.Connect(mDecoderStateMachine->CanonicalCurrentPosition());
  } else {
    mStateMachineDuration.DisconnectIfConnected();
    mBuffered.DisconnectIfConnected();
    mNextFrameStatus.DisconnectIfConnected();
    mCurrentPosition.DisconnectIfConnected();
  }
}

ReentrantMonitor& MediaDecoder::GetReentrantMonitor() {
  return mReentrantMonitor;
}

ImageContainer* MediaDecoder::GetImageContainer()
{
  return mVideoFrameContainer ? mVideoFrameContainer->GetImageContainer() : nullptr;
}

void MediaDecoder::InvalidateWithFlags(uint32_t aFlags)
{
  if (mVideoFrameContainer) {
    mVideoFrameContainer->InvalidateWithFlags(aFlags);
  }
}

void MediaDecoder::Invalidate()
{
  if (mVideoFrameContainer) {
    mVideoFrameContainer->Invalidate();
  }
}

// Constructs the time ranges representing what segments of the media
// are buffered and playable.
media::TimeIntervals MediaDecoder::GetBuffered() {
  return mBuffered.Ref();
}

size_t MediaDecoder::SizeOfVideoQueue() {
  if (mDecoderStateMachine) {
    return mDecoderStateMachine->SizeOfVideoQueue();
  }
  return 0;
}

size_t MediaDecoder::SizeOfAudioQueue() {
  if (mDecoderStateMachine) {
    return mDecoderStateMachine->SizeOfAudioQueue();
  }
  return 0;
}

void MediaDecoder::NotifyDataArrived(uint32_t aLength, int64_t aOffset, bool aThrottleUpdates) {
  MOZ_ASSERT(NS_IsMainThread());

  if (mDecoderStateMachine) {
    mDecoderStateMachine->DispatchNotifyDataArrived(aLength, aOffset, aThrottleUpdates);
  }

  // ReadyState computation depends on MediaDecoder::CanPlayThrough, which
  // depends on the download rate.
  UpdateReadyState();
}

// Provide access to the state machine object
MediaDecoderStateMachine* MediaDecoder::GetStateMachine() const {
  return mDecoderStateMachine;
}

void
MediaDecoder::NotifyWaitingForResourcesStatusChanged()
{
  if (mDecoderStateMachine) {
    RefPtr<nsRunnable> task =
      NS_NewRunnableMethod(mDecoderStateMachine,
                           &MediaDecoderStateMachine::NotifyWaitingForResourcesStatusChanged);
    mDecoderStateMachine->TaskQueue()->Dispatch(task.forget());
  }
}

bool MediaDecoder::IsShutdown() const {
  NS_ENSURE_TRUE(GetStateMachine(), true);
  return GetStateMachine()->IsShutdown();
}

// Drop reference to state machine.  Only called during shutdown dance.
void MediaDecoder::BreakCycles() {
  SetStateMachine(nullptr);
}

MediaDecoderOwner* MediaDecoder::GetMediaOwner() const
{
  return mOwner;
}

void MediaDecoder::FireTimeUpdate()
{
  if (!mOwner)
    return;
  mOwner->FireTimeUpdate(true);
}

void MediaDecoder::PinForSeek()
{
  MediaResource* resource = GetResource();
  if (!resource || mPinnedForSeek) {
    return;
  }
  mPinnedForSeek = true;
  resource->Pin();
}

void MediaDecoder::UnpinForSeek()
{
  MediaResource* resource = GetResource();
  if (!resource || !mPinnedForSeek) {
    return;
  }
  mPinnedForSeek = false;
  resource->Unpin();
}

bool MediaDecoder::CanPlayThrough()
{
  Statistics stats = GetStatistics();
  NS_ENSURE_TRUE(mDecoderStateMachine, false);

  if (mDecoderStateMachine->IsRealTime() ||
      (stats.mTotalBytes < 0 && stats.mDownloadRateReliable) ||
      (stats.mTotalBytes >= 0 && stats.mTotalBytes == stats.mDownloadPosition)) {
    return true;
  }
  if (!stats.mDownloadRateReliable || !stats.mPlaybackRateReliable) {
    return false;
  }
  int64_t bytesToDownload = stats.mTotalBytes - stats.mDownloadPosition;
  int64_t bytesToPlayback = stats.mTotalBytes - stats.mPlaybackPosition;
  double timeToDownload = bytesToDownload / stats.mDownloadRate;
  double timeToPlay = bytesToPlayback / stats.mPlaybackRate;

  if (timeToDownload > timeToPlay) {
    // Estimated time to download is greater than the estimated time to play.
    // We probably can't play through without having to stop to buffer.
    return false;
  }

  // Estimated time to download is less than the estimated time to play.
  // We can probably play through without having to buffer, but ensure that
  // we've got a reasonable amount of data buffered after the current
  // playback position, so that if the bitrate of the media fluctuates, or if
  // our download rate or decode rate estimation is otherwise inaccurate,
  // we don't suddenly discover that we need to buffer. This is particularly
  // required near the start of the media, when not much data is downloaded.
  int64_t readAheadMargin =
    static_cast<int64_t>(stats.mPlaybackRate * CAN_PLAY_THROUGH_MARGIN);
  return stats.mDownloadPosition > stats.mPlaybackPosition + readAheadMargin;
}

#ifdef MOZ_EME
nsresult
MediaDecoder::SetCDMProxy(CDMProxy* aProxy)
{
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  MOZ_ASSERT(NS_IsMainThread());
  mProxy = aProxy;
  // Awaken any readers waiting for the proxy.
  NotifyWaitingForResourcesStatusChanged();
  return NS_OK;
}

CDMProxy*
MediaDecoder::GetCDMProxy()
{
  GetReentrantMonitor().AssertCurrentThreadIn();
  return mProxy;
}
#endif

#ifdef MOZ_RAW
bool
MediaDecoder::IsRawEnabled()
{
  return Preferences::GetBool("media.raw.enabled");
}
#endif

bool
MediaDecoder::IsOpusEnabled()
{
  return Preferences::GetBool("media.opus.enabled");
}

bool
MediaDecoder::IsOggEnabled()
{
  return Preferences::GetBool("media.ogg.enabled");
}

#ifdef MOZ_WAVE
bool
MediaDecoder::IsWaveEnabled()
{
  return Preferences::GetBool("media.wave.enabled");
}
#endif

#ifdef MOZ_WEBM
bool
MediaDecoder::IsWebMEnabled()
{
  return Preferences::GetBool("media.webm.enabled");
}
#endif

#ifdef NECKO_PROTOCOL_rtsp
bool
MediaDecoder::IsRtspEnabled()
{
  //Currently the Rtsp decoded by omx.
  return (Preferences::GetBool("media.rtsp.enabled", false) && IsOmxEnabled());
}
#endif

#ifdef MOZ_GSTREAMER
bool
MediaDecoder::IsGStreamerEnabled()
{
  return Preferences::GetBool("media.gstreamer.enabled");
}
#endif

#ifdef MOZ_OMX_DECODER
bool
MediaDecoder::IsOmxEnabled()
{
  return Preferences::GetBool("media.omx.enabled", false);
}

bool
MediaDecoder::IsOmxAsyncEnabled()
{
#if ANDROID_VERSION >= 16
  return Preferences::GetBool("media.omx.async.enabled", false);
#else
  return false;
#endif
}
#endif

#ifdef MOZ_ANDROID_OMX
bool
MediaDecoder::IsAndroidMediaEnabled()
{
  return Preferences::GetBool("media.plugins.enabled");
}
#endif

#ifdef MOZ_APPLEMEDIA
bool
MediaDecoder::IsAppleMP3Enabled()
{
  return Preferences::GetBool("media.apple.mp3.enabled");
}
#endif

NS_IMETHODIMP
MediaMemoryTracker::CollectReports(nsIHandleReportCallback* aHandleReport,
                                   nsISupports* aData, bool aAnonymize)
{
  int64_t video = 0, audio = 0;
  size_t resources = 0;
  DecodersArray& decoders = Decoders();
  for (size_t i = 0; i < decoders.Length(); ++i) {
    MediaDecoder* decoder = decoders[i];
    video += decoder->SizeOfVideoQueue();
    audio += decoder->SizeOfAudioQueue();

    if (decoder->GetResource()) {
      resources += decoder->GetResource()->SizeOfIncludingThis(MallocSizeOf);
    }
  }

#define REPORT(_path, _amount, _desc)                                         \
  do {                                                                        \
      nsresult rv;                                                            \
      rv = aHandleReport->Callback(EmptyCString(), NS_LITERAL_CSTRING(_path), \
                                   KIND_HEAP, UNITS_BYTES, _amount,           \
                                   NS_LITERAL_CSTRING(_desc), aData);         \
      NS_ENSURE_SUCCESS(rv, rv);                                              \
  } while (0)

  REPORT("explicit/media/decoded/video", video,
         "Memory used by decoded video frames.");

  REPORT("explicit/media/decoded/audio", audio,
         "Memory used by decoded audio chunks.");

  REPORT("explicit/media/resources", resources,
         "Memory used by media resources including streaming buffers, caches, "
         "etc.");

#undef REPORT

  return NS_OK;
}

MediaDecoderOwner*
MediaDecoder::GetOwner()
{
  MOZ_ASSERT(NS_IsMainThread());
  return mOwner;
}

void
MediaDecoder::ConstructMediaTracks()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mMediaTracksConstructed) {
    return;
  }

  if (!mOwner || !mInfo) {
    return;
  }

  HTMLMediaElement* element = mOwner->GetMediaElement();
  if (!element) {
    return;
  }

  mMediaTracksConstructed = true;

  AudioTrackList* audioList = element->AudioTracks();
  if (audioList && mInfo->HasAudio()) {
    const TrackInfo& info = mInfo->mAudio;
    nsRefPtr<AudioTrack> track = MediaTrackList::CreateAudioTrack(
    info.mId, info.mKind, info.mLabel, info.mLanguage, info.mEnabled);

    audioList->AddTrack(track);
  }

  VideoTrackList* videoList = element->VideoTracks();
  if (videoList && mInfo->HasVideo()) {
    const TrackInfo& info = mInfo->mVideo;
    nsRefPtr<VideoTrack> track = MediaTrackList::CreateVideoTrack(
    info.mId, info.mKind, info.mLabel, info.mLanguage);

    videoList->AddTrack(track);
    track->SetEnabledInternal(info.mEnabled, MediaTrack::FIRE_NO_EVENTS);
  }
}

void
MediaDecoder::RemoveMediaTracks()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mOwner) {
    return;
  }

  HTMLMediaElement* element = mOwner->GetMediaElement();
  if (!element) {
    return;
  }

  AudioTrackList* audioList = element->AudioTracks();
  if (audioList) {
    audioList->RemoveTracks();
  }

  VideoTrackList* videoList = element->VideoTracks();
  if (videoList) {
    videoList->RemoveTracks();
  }

  mMediaTracksConstructed = false;
}

MediaMemoryTracker::MediaMemoryTracker()
{
}

void
MediaMemoryTracker::InitMemoryReporter()
{
  RegisterWeakMemoryReporter(this);
}

MediaMemoryTracker::~MediaMemoryTracker()
{
  UnregisterWeakMemoryReporter(this);
}

} // namespace mozilla

// avoid redefined macro in unified build
#undef DECODER_LOG
