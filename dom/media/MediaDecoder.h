/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
Each video element based on MediaDecoder has a state machine to manage
its play state and keep the current frame up to date. All state machines
share time in a single shared thread. Each decoder also has a MediaTaskQueue
running in a SharedThreadPool to decode audio and video data.
Each decoder also has a thread to push decoded audio
to the hardware. This thread is not created until playback starts, but
currently is not destroyed when paused, only when playback ends.

The decoder owns the resources for downloading the media file, and the
high level state. It holds an owning reference to the state machine that
owns all the resources related to decoding data, and manages the low level
decoding operations and A/V sync.

Each state machine runs on the shared state machine thread. Every time some
action is required for a state machine, it is scheduled to run on the shared
the state machine thread. The state machine runs one "cycle" on the state
machine thread, and then returns. If necessary, it will schedule itself to
run again in future. While running this cycle, it must not block the
thread, as other state machines' events may need to run. State shared
between a state machine's threads is synchronised via the monitor owned
by its MediaDecoder object.

The Main thread controls the decode state machine by setting the value
of a mPlayState variable and notifying on the monitor based on the
high level player actions required (Seek, Pause, Play, etc).

The player states are the states requested by the client through the
DOM API.  They represent the desired state of the player, while the
decoder's state represents the actual state of the decoder.

The high level state of the player is maintained via a PlayState value.
It can have the following states:

START
  The decoder has been initialized but has no resource loaded.
PAUSED
  A request via the API has been received to pause playback.
LOADING
  A request via the API has been received to load a resource.
PLAYING
  A request via the API has been received to start playback.
SEEKING
  A request via the API has been received to start seeking.
COMPLETED
  Playback has completed.
SHUTDOWN
  The decoder is about to be destroyed.

State transition occurs when the Media Element calls the Play, Seek,
etc methods on the MediaDecoder object. When the transition
occurs MediaDecoder then calls the methods on the decoder state
machine object to cause it to behave as required by the play state.
State transitions will likely schedule the state machine to run to
affect the change.

An implementation of the MediaDecoderStateMachine class is the event
that gets dispatched to the state machine thread. Each time the event is run,
the state machine must cycle the state machine once, and then return.

The state machine has the following states:

DECODING_METADATA
  The media headers are being loaded, and things like framerate, etc are
  being determined.
DECODING_FIRSTFRAME
  The first frame of audio/video data is being decoded.
DECODING
  The decode has started. If the PlayState is PLAYING, the decode thread
  should be alive and decoding video and audio frame, the audio thread
  should be playing audio, and the state machine should run periodically
  to update the video frames being displayed.
SEEKING
  A seek operation is in progress. The decode thread should be seeking.
BUFFERING
  Decoding is paused while data is buffered for smooth playback. If playback
  is paused (PlayState transitions to PAUSED) we'll destory the decode thread.
COMPLETED
  The resource has completed decoding, but possibly not finished playback.
  The decode thread will be destroyed. Once playback finished, the audio
  thread will also be destroyed.
SHUTDOWN
  The decoder object and its state machine are about to be destroyed.
  Once the last state machine has been destroyed, the shared state machine
  thread will also be destroyed. It will be recreated later if needed.

The following result in state transitions.

Shutdown()
  Clean up any resources the MediaDecoderStateMachine owns.
Play()
  Start decoding and playback of media data.
Buffer
  This is not user initiated. It occurs when the
  available data in the stream drops below a certain point.
Complete
  This is not user initiated. It occurs when the
  stream is completely decoded.
Seek(double)
  Seek to the time position given in the resource.

A state transition diagram:

   |---<-- DECODING_METADATA ----->--------|
   |        |                              |
 Seek(t)    v                          Shutdown()
   |        |                              |
   -->--- DECODING_FIRSTFRAME              |------->-----------------|
            |        |                                               |
            |    Shutdown()                                          |
            |        |                                               |
            v        |-->----------------->--------------------------|
            |---------------->----->------------------------|        v
          DECODING             |          |  |              |        |
            ^                  v Seek(t)  |  |              |        |
            |         Play()   |          v  |              |        |
            ^-----------<----SEEKING      |  v Complete     v        v
            |                  |          |  |              |        |
            |                  |          |  COMPLETED    SHUTDOWN-<-|
            ^                  ^          |  |Shutdown()    |
            |                  |          |  >-------->-----^
            |          Play()  |Seek(t)   |Buffer()         |
            -----------<--------<-------BUFFERING           |
                                          |                 ^
                                          v Shutdown()      |
                                          |                 |
                                          ------------>-----|

The following represents the states that the MediaDecoder object
can be in, and the valid states the MediaDecoderStateMachine can be in at that
time:

player LOADING   decoder DECODING_METADATA, DECODING_FIRSTFRAME
player PLAYING   decoder DECODING, BUFFERING, SEEKING, COMPLETED
player PAUSED    decoder DECODING, BUFFERING, SEEKING, COMPLETED
player SEEKING   decoder SEEKING
player COMPLETED decoder SHUTDOWN
player SHUTDOWN  decoder SHUTDOWN

The general sequence of events is:

1) The video element calls Load on MediaDecoder. This creates the
   state machine and starts the channel for downloading the
   file. It instantiates and schedules the MediaDecoderStateMachine. The
   high level LOADING state is entered, which results in the decode
   thread being created and starting to decode metadata. These are
   the headers that give the video size, framerate, etc. Load() returns
   immediately to the calling video element.

2) When the metadata has been loaded by the decode thread, the state machine
   will call a method on the video element object to inform it that this
   step is done, so it can do the things required by the video specification
   at this stage. The decode thread then continues to decode the first frame
   of data.

3) When the first frame of data has been successfully decoded the state
   machine calls a method on the video element object to inform it that
   this step has been done, once again so it can do the required things
   by the video specification at this stage.

   This results in the high level state changing to PLAYING or PAUSED
   depending on any user action that may have occurred.

   While the play state is PLAYING, the decode thread will decode
   data, and the audio thread will push audio data to the hardware to
   be played. The state machine will run periodically on the shared
   state machine thread to ensure video frames are played at the
   correct time; i.e. the state machine manages A/V sync.

The Shutdown method on MediaDecoder closes the download channel, and
signals to the state machine that it should shutdown. The state machine
shuts down asynchronously, and will release the owning reference to the
state machine once its threads are shutdown.

The owning object of a MediaDecoder object *MUST* call Shutdown when
destroying the MediaDecoder object.

*/
#if !defined(MediaDecoder_h_)
#define MediaDecoder_h_

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsAutoPtr.h"
#include "nsITimer.h"
#include "MediaPromise.h"
#include "MediaResource.h"
#include "mozilla/dom/AudioChannelBinding.h"
#include "mozilla/ReentrantMonitor.h"
#include "MediaDecoderOwner.h"
#include "MediaStreamGraph.h"
#include "AbstractMediaDecoder.h"
#include "DecodedStream.h"
#include "StateMirroring.h"
#include "StateWatching.h"
#include "necko-config.h"
#ifdef MOZ_EME
#include "mozilla/CDMProxy.h"
#endif
#include "TimeUnits.h"

class nsIStreamListener;
class nsIPrincipal;

namespace mozilla {

class VideoFrameContainer;
class MediaDecoderStateMachine;

// GetCurrentTime is defined in winbase.h as zero argument macro forwarding to
// GetTickCount() and conflicts with MediaDecoder::GetCurrentTime implementation.
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

// Stores the seek target; the time to seek to, and whether an Accurate,
// or "Fast" (nearest keyframe) seek was requested.
struct SeekTarget {
  enum Type {
    Invalid,
    PrevSyncPoint,
    Accurate
  };
  SeekTarget()
    : mTime(-1.0)
    , mType(SeekTarget::Invalid)
    , mEventVisibility(MediaDecoderEventVisibility::Observable)
  {
  }
  SeekTarget(int64_t aTimeUsecs,
             Type aType,
             MediaDecoderEventVisibility aEventVisibility =
               MediaDecoderEventVisibility::Observable)
    : mTime(aTimeUsecs)
    , mType(aType)
    , mEventVisibility(aEventVisibility)
  {
  }
  SeekTarget(const SeekTarget& aOther)
    : mTime(aOther.mTime)
    , mType(aOther.mType)
    , mEventVisibility(aOther.mEventVisibility)
  {
  }
  bool IsValid() const {
    return mType != SeekTarget::Invalid;
  }
  void Reset() {
    mTime = -1;
    mType = SeekTarget::Invalid;
  }
  // Seek target time in microseconds.
  int64_t mTime;
  // Whether we should seek "Fast", or "Accurate".
  // "Fast" seeks to the seek point preceeding mTime, whereas
  // "Accurate" seeks as close as possible to mTime.
  Type mType;
  MediaDecoderEventVisibility mEventVisibility;
};

class MediaDecoder : public AbstractMediaDecoder
{
public:
  struct SeekResolveValue {
    SeekResolveValue(bool aAtEnd, MediaDecoderEventVisibility aEventVisibility)
      : mAtEnd(aAtEnd), mEventVisibility(aEventVisibility) {}
    bool mAtEnd;
    MediaDecoderEventVisibility mEventVisibility;
  };

  typedef MediaPromise<SeekResolveValue, bool /* aIgnored */, /* IsExclusive = */ true> SeekPromise;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  // Enumeration for the valid play states (see mPlayState)
  enum PlayState {
    PLAY_STATE_START,
    PLAY_STATE_LOADING,
    PLAY_STATE_PAUSED,
    PLAY_STATE_PLAYING,
    PLAY_STATE_ENDED,
    PLAY_STATE_SHUTDOWN
  };

  // Must be called exactly once, on the main thread, during startup.
  static void InitStatics();

  MediaDecoder();

  // Reset the decoder and notify the media element that
  // server connection is closed.
  virtual void ResetConnectionState();
  // Create a new decoder of the same type as this one.
  // Subclasses must implement this.
  virtual MediaDecoder* Clone() = 0;
  // Create a new state machine to run this decoder.
  // Subclasses must implement this.
  virtual MediaDecoderStateMachine* CreateStateMachine() = 0;

  // Call on the main thread only.
  // Perform any initialization required for the decoder.
  // Return true on successful initialisation, false
  // on failure.
  virtual bool Init(MediaDecoderOwner* aOwner);

  // Cleanup internal data structures. Must be called on the main
  // thread by the owning object before that object disposes of this object.
  virtual void Shutdown();

  // Start downloading the media. Decode the downloaded data up to the
  // point of the first frame of data.
  // This is called at most once per decoder, after Init().
  virtual nsresult Load(nsIStreamListener** aListener,
                        MediaDecoder* aCloneDonor);

  // Called in |Load| to open mResource.
  nsresult OpenResource(nsIStreamListener** aStreamListener);

  // Called if the media file encounters a network error.
  virtual void NetworkError();

  // Get the current MediaResource being used. Its URI will be returned
  // by currentSrc. Returns what was passed to Load(), if Load() has been called.
  // Note: The MediaResource is refcounted, but it outlives the MediaDecoder,
  // so it's OK to use the reference returned by this function without
  // refcounting, *unless* you need to store and use the reference after the
  // MediaDecoder has been destroyed. You might need to do this if you're
  // wrapping the MediaResource in some kind of byte stream interface to be
  // passed to a platform decoder.
  MediaResource* GetResource() const final override
  {
    return mResource;
  }
  void SetResource(MediaResource* aResource)
  {
    NS_ASSERTION(NS_IsMainThread(), "Should only be called on main thread");
    mResource = aResource;
  }

  // Return the principal of the current URI being played or downloaded.
  virtual already_AddRefed<nsIPrincipal> GetCurrentPrincipal();

  // Return the time position in the video stream being
  // played measured in seconds.
  virtual double GetCurrentTime();

  // Seek to the time position in (seconds) from the start of the video.
  // If aDoFastSeek is true, we'll seek to the sync point/keyframe preceeding
  // the seek target.
  virtual nsresult Seek(double aTime, SeekTarget::Type aSeekType);

  // Initialize state machine and schedule it.
  nsresult InitializeStateMachine(MediaDecoder* aCloneDonor);

  // Start playback of a video. 'Load' must have previously been
  // called.
  virtual nsresult Play();

  // Notify activity of the decoder owner is changed.
  // Based on the activity, dormant state is updated.
  // Dormant state is a state to free all scarce media resources
  //  (like hw video codec), did not decoding and stay dormant.
  // It is used to share scarece media resources in system.
  virtual void NotifyOwnerActivityChanged();

  void UpdateDormantState(bool aDormantTimeout, bool aActivity);

  // Pause video playback.
  virtual void Pause();
  // Adjust the speed of the playback, optionally with pitch correction,
  virtual void SetVolume(double aVolume);

  virtual void NotifyWaitingForResourcesStatusChanged() override;

  virtual void SetPlaybackRate(double aPlaybackRate);
  void SetPreservesPitch(bool aPreservesPitch);

  // Directs the decoder to not preroll extra samples until the media is
  // played. This reduces the memory overhead of media elements that may
  // not be played. Note that seeking also doesn't cause us start prerolling.
  void SetMinimizePrerollUntilPlaybackStarts();

  // All MediaStream-related data is protected by mReentrantMonitor.
  // We have at most one DecodedStreamData per MediaDecoder. Its stream
  // is used as the input for each ProcessedMediaStream created by calls to
  // captureStream(UntilEnded). Seeking creates a new source stream, as does
  // replaying after the input as ended. In the latter case, the new source is
  // not connected to streams created by captureStreamUntilEnded.

  // Add an output stream. All decoder output will be sent to the stream.
  // The stream is initially blocked. The decoder is responsible for unblocking
  // it while it is playing back.
  virtual void AddOutputStream(ProcessedMediaStream* aStream, bool aFinishWhenEnded);

  // Return the duration of the video in seconds.
  virtual double GetDuration();

  AbstractCanonical<media::NullableTimeUnit>* CanonicalDurationOrNull() override;

  // A media stream is assumed to be infinite if the metadata doesn't
  // contain the duration, and range requests are not supported, and
  // no headers give a hint of a possible duration (Content-Length,
  // Content-Duration, and variants), and we cannot seek in the media
  // stream to determine the duration.
  //
  // When the media stream ends, we can know the duration, thus the stream is
  // no longer considered to be infinite.
  virtual void SetInfinite(bool aInfinite);

  // Return true if the stream is infinite (see SetInfinite).
  virtual bool IsInfinite();

  // Called by MediaResource when the "cache suspended" status changes.
  // If MediaResource::IsSuspendedByCache returns true, then the decoder
  // should stop buffering or otherwise waiting for download progress and
  // start consuming data, if possible, because the cache is full.
  virtual void NotifySuspendedStatusChanged();

  // Called by MediaResource when some data has been received.
  // Call on the main thread only.
  virtual void NotifyBytesDownloaded();

  // Called by nsChannelToPipeListener or MediaResource when the
  // download has ended. Called on the main thread only. aStatus is
  // the result from OnStopRequest.
  virtual void NotifyDownloadEnded(nsresult aStatus);

  // Called as data arrives on the stream and is read into the cache.  Called
  // on the main thread only.
  virtual void NotifyDataArrived(uint32_t aLength, int64_t aOffset,
                                 bool aThrottleUpdates) override;

  // Called by MediaResource when the principal of the resource has
  // changed. Called on main thread only.
  virtual void NotifyPrincipalChanged();

  // Called by the MediaResource to keep track of the number of bytes read
  // from the resource. Called on the main by an event runner dispatched
  // by the MediaResource read functions.
  void NotifyBytesConsumed(int64_t aBytes, int64_t aOffset) final override;

  // Return true if we are currently seeking in the media resource.
  // Call on the main thread only.
  virtual bool IsSeeking() const;

  // Return true if the decoder has reached the end of playback or the decoder
  // has shutdown.
  // Call on the main thread only.
  virtual bool IsEndedOrShutdown() const;

protected:
  // Updates the media duration. This is called while the media is being
  // played, calls before the media has reached loaded metadata are ignored.
  // The duration is assumed to be an estimate, and so a degree of
  // instability is expected; if the incoming duration is not significantly
  // different from the existing duration, the change request is ignored.
  // If the incoming duration is significantly different, the duration is
  // changed, this causes a durationchanged event to fire to the media
  // element.
  void UpdateEstimatedMediaDuration(int64_t aDuration) override;
public:

  // Set a flag indicating whether seeking is supported
  virtual void SetMediaSeekable(bool aMediaSeekable) override;

  // Returns true if this media supports seeking. False for example for WebM
  // files without an index and chained ogg files.
  virtual bool IsMediaSeekable() final override;
  // Returns true if seeking is supported on a transport level (e.g. the server
  // supports range requests, we are playing a file, etc.).
  virtual bool IsTransportSeekable() override;

  // Return the time ranges that can be seeked into.
  virtual media::TimeIntervals GetSeekable();

  // Set the end time of the media resource. When playback reaches
  // this point the media pauses. aTime is in seconds.
  virtual void SetFragmentEndTime(double aTime);

  // Invalidate the frame.
  void Invalidate();
  void InvalidateWithFlags(uint32_t aFlags);

  // Suspend any media downloads that are in progress. Called by the
  // media element when it is sent to the bfcache, or when we need
  // to throttle the download. Call on the main thread only. This can
  // be called multiple times, there's an internal "suspend count".
  virtual void Suspend();

  // Resume any media downloads that have been suspended. Called by the
  // media element when it is restored from the bfcache, or when we need
  // to stop throttling the download. Call on the main thread only.
  // The download will only actually resume once as many Resume calls
  // have been made as Suspend calls. When aForceBuffering is true,
  // we force the decoder to go into buffering state before resuming
  // playback.
  virtual void Resume(bool aForceBuffering);

  // Moves any existing channel loads into or out of background. Background
  // loads don't block the load event. This is called when we stop or restart
  // delaying the load event. This also determines whether any new loads
  // initiated (for example to seek) will be in the background.  This calls
  // SetLoadInBackground() on mResource.
  void SetLoadInBackground(bool aLoadInBackground);

  // Returns a weak reference to the media decoder owner.
  MediaDecoderOwner* GetMediaOwner() const;

  bool OnStateMachineTaskQueue() const override;

  bool OnDecodeTaskQueue() const override;

  MediaDecoderStateMachine* GetStateMachine() { return mDecoderStateMachine; }
  void SetStateMachine(MediaDecoderStateMachine* aStateMachine);

  // Returns the monitor for other threads to synchronise access to
  // state.
  ReentrantMonitor& GetReentrantMonitor() override;

  // Returns true if the decoder is shut down
  bool IsShutdown() const final override;

  // Constructs the time ranges representing what segments of the media
  // are buffered and playable.
  virtual media::TimeIntervals GetBuffered();

  // Returns the size, in bytes, of the heap memory used by the currently
  // queued decoded video and audio data.
  size_t SizeOfVideoQueue();
  size_t SizeOfAudioQueue();

  VideoFrameContainer* GetVideoFrameContainer() final override
  {
    return mVideoFrameContainer;
  }
  layers::ImageContainer* GetImageContainer() override;

  // Fire timeupdate events if needed according to the time constraints
  // outlined in the specification.
  void FireTimeUpdate();

  // Stop updating the bytes downloaded for progress notifications. Called
  // when seeking to prevent wild changes to the progress notification.
  // Must be called with the decoder monitor held.
  virtual void StopProgressUpdates();

  // Allow updating the bytes downloaded for progress notifications. Must
  // be called with the decoder monitor held.
  virtual void StartProgressUpdates();

  // Something has changed that could affect the computed playback rate,
  // so recompute it. The monitor must be held.
  virtual void UpdatePlaybackRate();

  // Used to estimate rates of data passing through the decoder's channel.
  // Records activity stopping on the channel. The monitor must be held.
  virtual void NotifyPlaybackStarted() {
    GetReentrantMonitor().AssertCurrentThreadIn();
    mPlaybackStatistics->Start();
  }

  // Used to estimate rates of data passing through the decoder's channel.
  // Records activity stopping on the channel. The monitor must be held.
  virtual void NotifyPlaybackStopped() {
    GetReentrantMonitor().AssertCurrentThreadIn();
    mPlaybackStatistics->Stop();
  }

  // The actual playback rate computation. The monitor must be held.
  virtual double ComputePlaybackRate(bool* aReliable);

  // Return true when the media is same-origin with the element. The monitor
  // must be held.
  bool IsSameOriginMedia();

  // Returns true if we can play the entire media through without stopping
  // to buffer, given the current download and playback rates.
  bool CanPlayThrough();

  void SetAudioChannel(dom::AudioChannel aChannel) { mAudioChannel = aChannel; }
  dom::AudioChannel GetAudioChannel() { return mAudioChannel; }

  // Send a new set of metadata to the state machine, to be dispatched to the
  // main thread to be presented when the |currentTime| of the media is greater
  // or equal to aPublishTime.
  void QueueMetadata(int64_t aPublishTime,
                     nsAutoPtr<MediaInfo> aInfo,
                     nsAutoPtr<MetadataTags> aTags) override;

  /******
   * The following methods must only be called on the main
   * thread.
   ******/

  // Change to a new play state. This updates the mState variable and
  // notifies any thread blocking on this object's monitor of the
  // change. Call on the main thread only.
  virtual void ChangeState(PlayState aState);

  // May be called by the reader to notify this decoder that the metadata from
  // the media file has been read. Call on the decode thread only.
  void OnReadMetadataCompleted() override { }

  // Called when the metadata from the media file has been loaded by the
  // state machine. Call on the main thread only.
  virtual void MetadataLoaded(nsAutoPtr<MediaInfo> aInfo,
                              nsAutoPtr<MetadataTags> aTags,
                              MediaDecoderEventVisibility aEventVisibility) override;

  // Called when the first audio and/or video from the media file has been loaded
  // by the state machine. Call on the main thread only.
  virtual void FirstFrameLoaded(nsAutoPtr<MediaInfo> aInfo,
                                MediaDecoderEventVisibility aEventVisibility) override;

  // Called from MetadataLoaded(). Creates audio tracks and adds them to its
  // owner's audio track list, and implies to video tracks respectively.
  // Call on the main thread only.
  void ConstructMediaTracks();

  // Removes all audio tracks and video tracks that are previously added into
  // the track list. Call on the main thread only.
  virtual void RemoveMediaTracks() override;

  // Returns true if the this decoder is expecting any more data to arrive
  // sometime in the not-too-distant future, either from the network or from
  // an appendBuffer call on a MediaSource element.
  //
  // Acquires the monitor. Call from any thread.
  virtual bool IsExpectingMoreData();

  // Called when the video has completed playing.
  // Call on the main thread only.
  void PlaybackEnded();

  void OnSeekRejected()
  {
    mSeekRequest.Complete();
    mLogicallySeeking = false;
  }
  void OnSeekResolved(SeekResolveValue aVal);

  // Seeking has started. Inform the element on the main
  // thread.
  void SeekingStarted(MediaDecoderEventVisibility aEventVisibility = MediaDecoderEventVisibility::Observable);

  void UpdateLogicalPosition(MediaDecoderEventVisibility aEventVisibility);
  void UpdateLogicalPosition() { UpdateLogicalPosition(MediaDecoderEventVisibility::Observable); }

  // Find the end of the cached data starting at the current decoder
  // position.
  int64_t GetDownloadPosition();

  // Updates the approximate byte offset which playback has reached. This is
  // used to calculate the readyState transitions.
  void UpdatePlaybackOffset(int64_t aOffset);

  // Provide access to the state machine object
  MediaDecoderStateMachine* GetStateMachine() const;

  // Drop reference to state machine.  Only called during shutdown dance.
  virtual void BreakCycles();

  // Notifies the element that decoding has failed.
  virtual void DecodeError();

  // Indicate whether the media is same-origin with the element.
  void UpdateSameOriginStatus(bool aSameOrigin);

  MediaDecoderOwner* GetOwner() override;

#ifdef MOZ_EME
  // This takes the decoder monitor.
  virtual nsresult SetCDMProxy(CDMProxy* aProxy) override;

  // Decoder monitor must be held.
  virtual CDMProxy* GetCDMProxy() override;
#endif

#ifdef MOZ_RAW
  static bool IsRawEnabled();
#endif

  static bool IsOggEnabled();
  static bool IsOpusEnabled();

#ifdef MOZ_WAVE
  static bool IsWaveEnabled();
#endif

#ifdef MOZ_WEBM
  static bool IsWebMEnabled();
#endif
#ifdef NECKO_PROTOCOL_rtsp
  static bool IsRtspEnabled();
#endif

#ifdef MOZ_GSTREAMER
  static bool IsGStreamerEnabled();
#endif

#ifdef MOZ_OMX_DECODER
  static bool IsOmxEnabled();
  static bool IsOmxAsyncEnabled();
#endif

#ifdef MOZ_ANDROID_OMX
  static bool IsAndroidMediaEnabled();
#endif

#ifdef MOZ_WMF
  static bool IsWMFEnabled();
#endif

#ifdef MOZ_APPLEMEDIA
  static bool IsAppleMP3Enabled();
#endif

  // Schedules the state machine to run one cycle on the shared state
  // machine thread. Main thread only.
  nsresult ScheduleStateMachine();

  struct Statistics {
    // Estimate of the current playback rate (bytes/second).
    double mPlaybackRate;
    // Estimate of the current download rate (bytes/second). This
    // ignores time that the channel was paused by Gecko.
    double mDownloadRate;
    // Total length of media stream in bytes; -1 if not known
    int64_t mTotalBytes;
    // Current position of the download, in bytes. This is the offset of
    // the first uncached byte after the decoder position.
    int64_t mDownloadPosition;
    // Current position of decoding, in bytes (how much of the stream
    // has been consumed)
    int64_t mDecoderPosition;
    // Current position of playback, in bytes
    int64_t mPlaybackPosition;
    // If false, then mDownloadRate cannot be considered a reliable
    // estimate (probably because the download has only been running
    // a short time).
    bool mDownloadRateReliable;
    // If false, then mPlaybackRate cannot be considered a reliable
    // estimate (probably because playback has only been running
    // a short time).
    bool mPlaybackRateReliable;
  };

  // Return statistics. This is used for progress events and other things.
  // This can be called from any thread. It's only a snapshot of the
  // current state, since other threads might be changing the state
  // at any time.
  virtual Statistics GetStatistics();

  // Frame decoding/painting related performance counters.
  // Threadsafe.
  class FrameStatistics {
  public:

    FrameStatistics() :
        mReentrantMonitor("MediaDecoder::FrameStats"),
        mParsedFrames(0),
        mDecodedFrames(0),
        mPresentedFrames(0),
        mDroppedFrames(0),
        mCorruptFrames(0) {}

    // Returns number of frames which have been parsed from the media.
    // Can be called on any thread.
    uint32_t GetParsedFrames() {
      ReentrantMonitorAutoEnter mon(mReentrantMonitor);
      return mParsedFrames;
    }

    // Returns the number of parsed frames which have been decoded.
    // Can be called on any thread.
    uint32_t GetDecodedFrames() {
      ReentrantMonitorAutoEnter mon(mReentrantMonitor);
      return mDecodedFrames;
    }

    // Returns the number of decoded frames which have been sent to the rendering
    // pipeline for painting ("presented").
    // Can be called on any thread.
    uint32_t GetPresentedFrames() {
      ReentrantMonitorAutoEnter mon(mReentrantMonitor);
      return mPresentedFrames;
    }

    // Number of frames that have been skipped because they have missed their
    // compoisition deadline.
    uint32_t GetDroppedFrames() {
      ReentrantMonitorAutoEnter mon(mReentrantMonitor);
      return mDroppedFrames + mCorruptFrames;
    }

    uint32_t GetCorruptedFrames() {
      ReentrantMonitorAutoEnter mon(mReentrantMonitor);
      return mCorruptFrames;
    }

    // Increments the parsed and decoded frame counters by the passed in counts.
    // Can be called on any thread.
    void NotifyDecodedFrames(uint32_t aParsed, uint32_t aDecoded,
                             uint32_t aDropped) {
      if (aParsed == 0 && aDecoded == 0 && aDropped == 0)
        return;
      ReentrantMonitorAutoEnter mon(mReentrantMonitor);
      mParsedFrames += aParsed;
      mDecodedFrames += aDecoded;
      mDroppedFrames += aDropped;
    }

    // Increments the presented frame counters.
    // Can be called on any thread.
    void NotifyPresentedFrame() {
      ReentrantMonitorAutoEnter mon(mReentrantMonitor);
      ++mPresentedFrames;
    }

    void NotifyCorruptFrame() {
      ReentrantMonitorAutoEnter mon(mReentrantMonitor);
      ++mCorruptFrames;
    }

  private:

    // ReentrantMonitor to protect access of playback statistics.
    ReentrantMonitor mReentrantMonitor;

    // Number of frames parsed and demuxed from media.
    // Access protected by mReentrantMonitor.
    uint32_t mParsedFrames;

    // Number of parsed frames which were actually decoded.
    // Access protected by mReentrantMonitor.
    uint32_t mDecodedFrames;

    // Number of decoded frames which were actually sent down the rendering
    // pipeline to be painted ("presented"). Access protected by mReentrantMonitor.
    uint32_t mPresentedFrames;

    uint32_t mDroppedFrames;

    uint32_t mCorruptFrames;
  };

  // Return the frame decode/paint related statistics.
  FrameStatistics& GetFrameStatistics() { return mFrameStats; }

  // Increments the parsed and decoded frame counters by the passed in counts.
  // Can be called on any thread.
  virtual void NotifyDecodedFrames(uint32_t aParsed, uint32_t aDecoded,
                                   uint32_t aDropped) override
  {
    GetFrameStatistics().NotifyDecodedFrames(aParsed, aDecoded, aDropped);
  }

  void UpdateReadyState()
  {
    if (mOwner) {
      mOwner->UpdateReadyState();
    }
  }

  virtual MediaDecoderOwner::NextFrameStatus NextFrameStatus() { return mNextFrameStatus; }

protected:
  virtual ~MediaDecoder();
  void SetStateMachineParameters();

  static void DormantTimerExpired(nsITimer *aTimer, void *aClosure);

  // Start a timer for heuristic dormant.
  void StartDormantTimer();

  // Cancel a timer for heuristic dormant.
  void CancelDormantTimer();

  // Return true if the decoder has reached the end of playback
  bool IsEnded() const;

  // Called by the state machine to notify the decoder that the duration
  // has changed.
  void DurationChanged();

  // State-watching manager.
  WatchManager<MediaDecoder> mWatchManager;

  // Buffered range, mirrored from the reader.
  Mirror<media::TimeIntervals> mBuffered;

  // NextFrameStatus, mirrored from the state machine.
  Mirror<MediaDecoderOwner::NextFrameStatus> mNextFrameStatus;

  /******
   * The following members should be accessed with the decoder lock held.
   ******/

  // Current decoding position in the stream. This is where the decoder
  // is up to consuming the stream. This is not adjusted during decoder
  // seek operations, but it's updated at the end when we start playing
  // back again.
  int64_t mDecoderPosition;
  // Current playback position in the stream. This is (approximately)
  // where we're up to playing back the stream. This is not adjusted
  // during decoder seek operations, but it's updated at the end when we
  // start playing back again.
  int64_t mPlaybackPosition;

  // The logical playback position of the media resource in units of
  // seconds. This corresponds to the "official position" in HTML5. Note that
  // we need to store this as a double, rather than an int64_t (like
  // mCurrentPosition), so that |v.currentTime = foo; v.currentTime == foo|
  // returns true without being affected by rounding errors.
  double mLogicalPosition;

  // The current playback position of the underlying playback infrastructure.
  // This corresponds to the "current position" in HTML5.
  //
  // NB: Don't use mCurrentPosition directly, but rather CurrentPosition() below.
  Mirror<int64_t> mCurrentPosition;

  // We allow omx subclasses to substitute an alternative current position for
  // usage with the audio offload player.
  virtual int64_t CurrentPosition() { return mCurrentPosition; }

  // Volume of playback.  0.0 = muted. 1.0 = full volume.
  Canonical<double> mVolume;
public:
  AbstractCanonical<double>* CanonicalVolume() { return &mVolume; }
protected:

  // PlaybackRate and pitch preservation status we should start at.
  Canonical<double> mPlaybackRate;
public:
  AbstractCanonical<double>* CanonicalPlaybackRate() { return &mPlaybackRate; }
protected:

  Canonical<bool> mPreservesPitch;
public:
  AbstractCanonical<bool>* CanonicalPreservesPitch() { return &mPreservesPitch; }
protected:

  // Official duration of the media resource as observed by script.
  double mDuration;

  // Duration of the media resource according to the state machine.
  Mirror<media::NullableTimeUnit> mStateMachineDuration;

  // True if the media is seekable (i.e. supports random access).
  bool mMediaSeekable;

  // True if the media is same-origin with the element. Data can only be
  // passed to MediaStreams when this is true.
  bool mSameOriginMedia;

  /******
   * The following member variables can be accessed from any thread.
   ******/

  // Media data resource.
  nsRefPtr<MediaResource> mResource;

private:
  // The state machine object for handling the decoding. It is safe to
  // call methods of this object from other threads. Its internal data
  // is synchronised on a monitor. The lifetime of this object is
  // after mPlayState is LOADING and before mPlayState is SHUTDOWN. It
  // is safe to access it during this period.
  //
  // Explicitly prievate to force access via accessors.
  nsRefPtr<MediaDecoderStateMachine> mDecoderStateMachine;

  // |ReentrantMonitor| for detecting when the video play state changes. A call
  // to |Wait| on this monitor will block the thread until the next state
  // change.  Explicitly private for force access via GetReentrantMonitor.
  ReentrantMonitor mReentrantMonitor;

#ifdef MOZ_EME
  nsRefPtr<CDMProxy> mProxy;
#endif

  // Media duration according to the demuxer's current estimate.
  //
  // Note that it's quite bizarre for this to live on the main thread - it would
  // make much more sense for this to be owned by the demuxer's task queue. But
  // currently this is only every changed in NotifyDataArrived, which runs on
  // the main thread. That will need to be cleaned up at some point.
  Canonical<media::NullableTimeUnit> mEstimatedDuration;
public:
  AbstractCanonical<media::NullableTimeUnit>* CanonicalEstimatedDuration() { return &mEstimatedDuration; }
protected:

  // Media duration set explicitly by JS. At present, this is only ever present
  // for MSE.
  Canonical<Maybe<double>> mExplicitDuration;
  double ExplicitDuration() { return mExplicitDuration.Ref().ref(); }
  void SetExplicitDuration(double aValue)
  {
    mExplicitDuration.Set(Some(aValue));

    // We Invoke DurationChanged explicitly, rather than using a watcher, so
    // that it takes effect immediately, rather than at the end of the current task.
    DurationChanged();
  }

public:
  AbstractCanonical<Maybe<double>>* CanonicalExplicitDuration() { return &mExplicitDuration; }
protected:

  // Set to one of the valid play states.
  // This can only be changed on the main thread while holding the decoder
  // monitor. Thus, it can be safely read while holding the decoder monitor
  // OR on the main thread.
  // Any change to the state on the main thread must call NotifyAll on the
  // monitor so the decode thread can wake up.
  Canonical<PlayState> mPlayState;

  // This can only be changed on the main thread while holding the decoder
  // monitor. Thus, it can be safely read while holding the decoder monitor
  // OR on the main thread.
  // Any change to the state must call NotifyAll on the monitor.
  // This can only be PLAY_STATE_PAUSED or PLAY_STATE_PLAYING.
  Canonical<PlayState> mNextState;

  // True if the decoder is seeking.
  Canonical<bool> mLogicallySeeking;
public:
  AbstractCanonical<PlayState>* CanonicalPlayState() { return &mPlayState; }
  AbstractCanonical<PlayState>* CanonicalNextPlayState() { return &mNextState; }
  AbstractCanonical<bool>* CanonicalLogicallySeeking() { return &mLogicallySeeking; }
protected:

  virtual void CallSeek(const SeekTarget& aTarget);

  // Returns true if heuristic dormant is supported.
  bool IsHeuristicDormantSupported() const;

  MediaPromiseRequestHolder<SeekPromise> mSeekRequest;

  // True when seeking or otherwise moving the play position around in
  // such a manner that progress event data is inaccurate. This is set
  // during seek and duration operations to prevent the progress indicator
  // from jumping around. Read/Write from any thread. Must have decode monitor
  // locked before accessing.
  bool mIgnoreProgressData;

  // True if the stream is infinite (e.g. a webradio).
  bool mInfiniteStream;

  // Ensures our media stream has been pinned.
  void PinForSeek();

  // Ensures our media stream has been unpinned.
  void UnpinForSeek();

  const char* PlayStateStr();

  // This should only ever be accessed from the main thread.
  // It is set in Init and cleared in Shutdown when the element goes away.
  // The decoder does not add a reference the element.
  MediaDecoderOwner* mOwner;

  // Counters related to decode and presentation of frames.
  FrameStatistics mFrameStats;

  nsRefPtr<VideoFrameContainer> mVideoFrameContainer;

  // Data needed to estimate playback data rate. The timeline used for
  // this estimate is "decode time" (where the "current time" is the
  // time of the last decoded video frame).
  nsRefPtr<MediaChannelStatistics> mPlaybackStatistics;

  // True when our media stream has been pinned. We pin the stream
  // while seeking.
  bool mPinnedForSeek;

  // True if the decoder is being shutdown. At this point all events that
  // are currently queued need to return immediately to prevent javascript
  // being run that operates on the element and decoder during shutdown.
  // Read/Write from the main thread only.
  bool mShuttingDown;

  // True if the playback is paused because the playback rate member is 0.0.
  bool mPausedForPlaybackRateNull;

  // Be assigned from media element during the initialization and pass to
  // AudioStream Class.
  dom::AudioChannel mAudioChannel;

  // True if the decoder has been directed to minimize its preroll before
  // playback starts. After the first time playback starts, we don't attempt
  // to minimize preroll, as we assume the user is likely to keep playing,
  // or play the media again.
  bool mMinimizePreroll;

  // True if audio tracks and video tracks are constructed and added into the
  // track list, false if all tracks are removed from the track list.
  bool mMediaTracksConstructed;

  // True if we've already fired metadataloaded.
  bool mFiredMetadataLoaded;

  // Stores media info, including info of audio tracks and video tracks, should
  // only be accessed from main thread.
  nsAutoPtr<MediaInfo> mInfo;

  // True if MediaDecoder is in dormant state.
  bool mIsDormant;

  // True if MediaDecoder was PLAY_STATE_ENDED state, when entering to dormant.
  // When MediaCodec is in dormant during PLAY_STATE_ENDED state, PlayState
  // becomes different from PLAY_STATE_ENDED. But the MediaDecoder need to act
  // as in PLAY_STATE_ENDED state to MediaDecoderOwner.
  bool mWasEndedWhenEnteredDormant;

  // True if heuristic dormant is supported.
  const bool mIsHeuristicDormantSupported;

  // Timeout ms of heuristic dormant timer.
  const int mHeuristicDormantTimeout;

  // True if MediaDecoder is in dormant by heuristic.
  bool mIsHeuristicDormant;

  // Timer to schedule updating dormant state.
  nsCOMPtr<nsITimer> mDormantTimer;
};

} // namespace mozilla

#endif
