/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngine.h"
#include "mozilla/Services.h"
#include "mozilla/unused.h"
#include "nsIMediaManager.h"

#include "nsHashKeys.h"
#include "nsGlobalWindow.h"
#include "nsClassHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsIObserver.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

#include "nsPIDOMWindow.h"
#include "nsIDOMNavigatorUserMedia.h"
#include "nsXULAppAPI.h"
#include "mozilla/Attributes.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/dom/MediaStreamBinding.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"
#include "prlog.h"
#include "DOMMediaStream.h"

#ifdef MOZ_WEBRTC
#include "mtransport/runnable_utils.h"
#endif

#ifdef MOZ_WIDGET_GONK
#include "DOMCameraManager.h"
#endif

namespace mozilla {
namespace dom {
class MediaStreamConstraints;
class NavigatorUserMediaSuccessCallback;
class NavigatorUserMediaErrorCallback;
}

#ifdef PR_LOGGING
extern PRLogModuleInfo* GetMediaManagerLog();
#define MM_LOG(msg) PR_LOG(GetMediaManagerLog(), PR_LOG_DEBUG, msg)
#else
#define MM_LOG(msg)
#endif

/**
 * This class is an implementation of MediaStreamListener. This is used
 * to Start() and Stop() the underlying MediaEngineSource when MediaStreams
 * are assigned and deassigned in content.
 */
class GetUserMediaCallbackMediaStreamListener : public MediaStreamListener
{
public:
  // Create in an inactive state
  GetUserMediaCallbackMediaStreamListener(nsIThread *aThread,
    uint64_t aWindowID)
    : mMediaThread(aThread)
    , mWindowID(aWindowID)
    , mStopped(false)
    , mFinished(false)
    , mLock("mozilla::GUMCMSL")
    , mRemoved(false) {}

  ~GetUserMediaCallbackMediaStreamListener()
  {
    // It's OK to release mStream on any thread; they have thread-safe
    // refcounts.
  }

  void Activate(already_AddRefed<SourceMediaStream> aStream,
    MediaEngineSource* aAudioSource,
    MediaEngineSource* aVideoSource)
  {
    NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
    mStream = aStream;
    mAudioSource = aAudioSource;
    mVideoSource = aVideoSource;
    mLastEndTimeAudio = 0;
    mLastEndTimeVideo = 0;

    mStream->AddListener(this);
  }

  MediaStream *Stream() // Can be used to test if Activate was called
  {
    return mStream;
  }
  SourceMediaStream *GetSourceStream()
  {
    NS_ASSERTION(mStream,"Getting stream from never-activated GUMCMSListener");
    if (!mStream) {
      return nullptr;
    }
    return mStream->AsSourceStream();
  }

  // mVideo/AudioSource are set by Activate(), so we assume they're capturing
  // if set and represent a real capture device.
  bool CapturingVideo()
  {
    NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
    return mVideoSource && !mStopped &&
           (!mVideoSource->IsFake() ||
            Preferences::GetBool("media.navigator.permission.fake"));
  }
  bool CapturingAudio()
  {
    NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
    return mAudioSource && !mStopped &&
           (!mAudioSource->IsFake() ||
            Preferences::GetBool("media.navigator.permission.fake"));
  }

  void SetStopped()
  {
    mStopped = true;
  }

  // implement in .cpp to avoid circular dependency with MediaOperationRunnable
  // Can be invoked from EITHER MainThread or MSG thread
  void Invalidate();

  void
  AudioConfig(bool aEchoOn, uint32_t aEcho,
              bool aAgcOn, uint32_t aAGC,
              bool aNoiseOn, uint32_t aNoise,
              int32_t aPlayoutDelay)
  {
    if (mAudioSource) {
#ifdef MOZ_WEBRTC
      // Right now these configs are only of use if webrtc is available
      RUN_ON_THREAD(mMediaThread,
                    WrapRunnable(nsRefPtr<MediaEngineSource>(mAudioSource), // threadsafe
                                 &MediaEngineSource::Config,
                                 aEchoOn, aEcho, aAgcOn, aAGC, aNoiseOn, aNoise, aPlayoutDelay),
                    NS_DISPATCH_NORMAL);
#endif
    }
  }

  void
  Remove()
  {
    NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
    // allow calling even if inactive (!mStream) for easier cleanup
    // Caller holds strong reference to us, so no death grip required
    MutexAutoLock lock(mLock); // protect access to mRemoved
    if (mStream && !mRemoved) {
      MM_LOG(("Listener removed on purpose, mFinished = %d", (int) mFinished));
      mRemoved = true; // RemoveListener is async, avoid races
      // If it's destroyed, don't call - listener will be removed and we'll be notified!
      if (!mStream->IsDestroyed()) {
        mStream->RemoveListener(this);
      }
    }
  }

  // Proxy NotifyPull() to sources
  virtual void
  NotifyPull(MediaStreamGraph* aGraph, StreamTime aDesiredTime) MOZ_OVERRIDE
  {
    // Currently audio sources ignore NotifyPull, but they could
    // watch it especially for fake audio.
    if (mAudioSource) {
      mAudioSource->NotifyPull(aGraph, mStream, kAudioTrack, aDesiredTime, mLastEndTimeAudio);
    }
    if (mVideoSource) {
      mVideoSource->NotifyPull(aGraph, mStream, kVideoTrack, aDesiredTime, mLastEndTimeVideo);
    }
  }

  virtual void
  NotifyFinished(MediaStreamGraph* aGraph) MOZ_OVERRIDE;

  virtual void
  NotifyRemoved(MediaStreamGraph* aGraph) MOZ_OVERRIDE;

private:
  // Set at construction
  nsCOMPtr<nsIThread> mMediaThread;
  uint64_t mWindowID;

  bool mStopped; // MainThread only

  // Set at Activate on MainThread

  // Accessed from MediaStreamGraph thread, MediaManager thread, and MainThread
  // No locking needed as they're only addrefed except on the MediaManager thread
  nsRefPtr<MediaEngineSource> mAudioSource; // threadsafe refcnt
  nsRefPtr<MediaEngineSource> mVideoSource; // threadsafe refcnt
  nsRefPtr<SourceMediaStream> mStream; // threadsafe refcnt
  TrackTicks mLastEndTimeAudio;
  TrackTicks mLastEndTimeVideo;
  bool mFinished;

  // Accessed from MainThread and MSG thread
  Mutex mLock; // protects mRemoved access from MainThread
  bool mRemoved;
};

class GetUserMediaNotificationEvent: public nsRunnable
{
  public:
    enum GetUserMediaStatus {
      STARTING,
      STOPPING
    };
    GetUserMediaNotificationEvent(GetUserMediaCallbackMediaStreamListener* aListener,
                                  GetUserMediaStatus aStatus,
                                  bool aIsAudio, bool aIsVideo, uint64_t aWindowID)
    : mListener(aListener) , mStatus(aStatus) , mIsAudio(aIsAudio)
    , mIsVideo(aIsVideo), mWindowID(aWindowID) {}

    GetUserMediaNotificationEvent(GetUserMediaStatus aStatus,
                                  already_AddRefed<DOMMediaStream> aStream,
                                  DOMMediaStream::OnTracksAvailableCallback* aOnTracksAvailableCallback,
                                  bool aIsAudio, bool aIsVideo, uint64_t aWindowID,
                                  already_AddRefed<nsIDOMGetUserMediaErrorCallback> aError)
    : mStream(aStream), mOnTracksAvailableCallback(aOnTracksAvailableCallback),
      mStatus(aStatus), mIsAudio(aIsAudio), mIsVideo(aIsVideo), mWindowID(aWindowID),
      mError(aError) {}
    virtual ~GetUserMediaNotificationEvent()
    {

    }

    NS_IMETHOD Run() MOZ_OVERRIDE;

  protected:
    nsRefPtr<GetUserMediaCallbackMediaStreamListener> mListener; // threadsafe
    nsRefPtr<DOMMediaStream> mStream;
    nsAutoPtr<DOMMediaStream::OnTracksAvailableCallback> mOnTracksAvailableCallback;
    GetUserMediaStatus mStatus;
    bool mIsAudio;
    bool mIsVideo;
    uint64_t mWindowID;
    nsRefPtr<nsIDOMGetUserMediaErrorCallback> mError;
};

typedef enum {
  MEDIA_START,
  MEDIA_STOP
} MediaOperation;

class MediaManager;
class GetUserMediaRunnable;

/**
 * Send an error back to content. The error is the form a string.
 * Do this only on the main thread. The success callback is also passed here
 * so it can be released correctly.
 */
class ErrorCallbackRunnable : public nsRunnable
{
public:
  ErrorCallbackRunnable(
    nsCOMPtr<nsIDOMGetUserMediaSuccessCallback>& aSuccess,
    nsCOMPtr<nsIDOMGetUserMediaErrorCallback>& aError,
    const nsAString& aErrorMsg, uint64_t aWindowID);
  NS_IMETHOD Run();
private:
  ~ErrorCallbackRunnable();

  nsCOMPtr<nsIDOMGetUserMediaSuccessCallback> mSuccess;
  nsCOMPtr<nsIDOMGetUserMediaErrorCallback> mError;
  const nsString mErrorMsg;
  uint64_t mWindowID;
  nsRefPtr<MediaManager> mManager; // get ref to this when creating the runnable
};

class ReleaseMediaOperationResource : public nsRunnable
{
public:
  ReleaseMediaOperationResource(already_AddRefed<DOMMediaStream> aStream,
    DOMMediaStream::OnTracksAvailableCallback* aOnTracksAvailableCallback):
    mStream(aStream),
    mOnTracksAvailableCallback(aOnTracksAvailableCallback) {}
  NS_IMETHOD Run() MOZ_OVERRIDE {return NS_OK;}
private:
  nsRefPtr<DOMMediaStream> mStream;
  nsAutoPtr<DOMMediaStream::OnTracksAvailableCallback> mOnTracksAvailableCallback;
};

// Generic class for running long media operations like Start off the main
// thread, and then (because nsDOMMediaStreams aren't threadsafe),
// ProxyReleases mStream since it's cycle collected.
class MediaOperationRunnable : public nsRunnable
{
public:
  // so we can send Stop without AddRef()ing from the MSG thread
  MediaOperationRunnable(MediaOperation aType,
    GetUserMediaCallbackMediaStreamListener* aListener,
    DOMMediaStream* aStream,
    DOMMediaStream::OnTracksAvailableCallback* aOnTracksAvailableCallback,
    MediaEngineSource* aAudioSource,
    MediaEngineSource* aVideoSource,
    bool aNeedsFinish,
    uint64_t aWindowID,
    already_AddRefed<nsIDOMGetUserMediaErrorCallback> aError)
    : mType(aType)
    , mStream(aStream)
    , mOnTracksAvailableCallback(aOnTracksAvailableCallback)
    , mAudioSource(aAudioSource)
    , mVideoSource(aVideoSource)
    , mListener(aListener)
    , mFinish(aNeedsFinish)
    , mWindowID(aWindowID)
    , mError(aError)
  {}

  ~MediaOperationRunnable()
  {
    // MediaStreams can be released on any thread.
  }

  nsresult returnAndCallbackError(nsresult rv, const char* errorLog)
  {
    MM_LOG(("%s , rv=%d", errorLog, rv));
    NS_DispatchToMainThread(new ReleaseMediaOperationResource(mStream.forget(),
          mOnTracksAvailableCallback.forget()));
    nsString log;

    log.AssignASCII(errorLog, strlen(errorLog));
    nsCOMPtr<nsIDOMGetUserMediaSuccessCallback> success;
    NS_DispatchToMainThread(new ErrorCallbackRunnable(success, mError,
      log, mWindowID));
    return NS_OK;
  }

  NS_IMETHOD
  Run() MOZ_OVERRIDE
  {
    SourceMediaStream *source = mListener->GetSourceStream();
    // No locking between these is required as all the callbacks for the
    // same MediaStream will occur on the same thread.
    if (!source) // means the stream was never Activated()
      return NS_OK;

    switch (mType) {
      case MEDIA_START:
        {
          NS_ASSERTION(!NS_IsMainThread(), "Never call on main thread");
          nsresult rv;

          source->SetPullEnabled(true);

          DOMMediaStream::TrackTypeHints expectedTracks = 0;
          if (mAudioSource) {
            rv = mAudioSource->Start(source, kAudioTrack);
            if (NS_SUCCEEDED(rv)) {
              expectedTracks |= DOMMediaStream::HINT_CONTENTS_AUDIO;
            } else {
              return returnAndCallbackError(rv, "Starting audio failed");
            }
          }
          if (mVideoSource) {
            rv = mVideoSource->Start(source, kVideoTrack);
            if (NS_SUCCEEDED(rv)) {
              expectedTracks |= DOMMediaStream::HINT_CONTENTS_VIDEO;
            } else {
              return returnAndCallbackError(rv, "Starting video failed");
            }
          }

          mOnTracksAvailableCallback->SetExpectedTracks(expectedTracks);

          MM_LOG(("started all sources"));
          // Forward mOnTracksAvailableCallback to GetUserMediaNotificationEvent,
          // because mOnTracksAvailableCallback needs to be added to mStream
          // on the main thread.
          nsIRunnable *event =
            new GetUserMediaNotificationEvent(GetUserMediaNotificationEvent::STARTING,
                                              mStream.forget(),
                                              mOnTracksAvailableCallback.forget(),
                                              mAudioSource != nullptr,
                                              mVideoSource != nullptr,
                                              mWindowID, mError.forget());
          // event must always be released on mainthread due to the JS callbacks
          // in the TracksAvailableCallback
          NS_DispatchToMainThread(event);
        }
        break;

      case MEDIA_STOP:
        {
          NS_ASSERTION(!NS_IsMainThread(), "Never call on main thread");
          if (mAudioSource) {
            mAudioSource->Stop(source, kAudioTrack);
            mAudioSource->Deallocate();
          }
          if (mVideoSource) {
            mVideoSource->Stop(source, kVideoTrack);
            mVideoSource->Deallocate();
          }
          // Do this after stopping all tracks with EndTrack()
          if (mFinish) {
            source->Finish();
          }
          nsIRunnable *event =
            new GetUserMediaNotificationEvent(mListener,
                                              GetUserMediaNotificationEvent::STOPPING,
                                              mAudioSource != nullptr,
                                              mVideoSource != nullptr,
                                              mWindowID);
          // event must always be released on mainthread due to the JS callbacks
          // in the TracksAvailableCallback
          NS_DispatchToMainThread(event);
        }
        break;

      default:
        MOZ_ASSERT(false,"invalid MediaManager operation");
        break;
    }
    return NS_OK;
  }

private:
  MediaOperation mType;
  nsRefPtr<DOMMediaStream> mStream;
  nsAutoPtr<DOMMediaStream::OnTracksAvailableCallback> mOnTracksAvailableCallback;
  nsRefPtr<MediaEngineSource> mAudioSource; // threadsafe
  nsRefPtr<MediaEngineSource> mVideoSource; // threadsafe
  nsRefPtr<GetUserMediaCallbackMediaStreamListener> mListener; // threadsafe
  bool mFinish;
  uint64_t mWindowID;
  nsCOMPtr<nsIDOMGetUserMediaErrorCallback> mError;
};

typedef nsTArray<nsRefPtr<GetUserMediaCallbackMediaStreamListener> > StreamListeners;
typedef nsClassHashtable<nsUint64HashKey, StreamListeners> WindowTable;

class MediaDevice : public nsIMediaDevice
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIMEDIADEVICE

  static MediaDevice* Create(MediaEngineVideoSource* source);
  static MediaDevice* Create(MediaEngineAudioSource* source);

  virtual ~MediaDevice() {}
protected:
  MediaDevice(MediaEngineSource* aSource);
  nsString mName;
  nsString mID;
  bool mHasFacingMode;
  dom::VideoFacingModeEnum mFacingMode;
  nsRefPtr<MediaEngineSource> mSource;
};

class VideoDevice : public MediaDevice
{
public:
  VideoDevice(MediaEngineVideoSource* aSource);
  NS_IMETHOD GetType(nsAString& aType);
  MediaEngineVideoSource* GetSource();
};

class AudioDevice : public MediaDevice
{
public:
  AudioDevice(MediaEngineAudioSource* aSource);
  NS_IMETHOD GetType(nsAString& aType);
  MediaEngineAudioSource* GetSource();
};

class MediaManager MOZ_FINAL : public nsIMediaManagerService,
                               public nsIObserver
{
public:
  static already_AddRefed<MediaManager> GetInstance();

  // NOTE: never Dispatch(....,NS_DISPATCH_SYNC) to the MediaManager
  // thread from the MainThread, as we NS_DISPATCH_SYNC to MainThread
  // from MediaManager thread.
  static MediaManager* Get();

  static nsIThread* GetThread() {
    return Get()->mMediaThread;
  }

  static nsresult NotifyRecordingStatusChange(nsPIDOMWindow* aWindow,
                                              const nsString& aMsg,
                                              const bool& aIsAudio,
                                              const bool& aIsVideo);

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIMEDIAMANAGERSERVICE

  MediaEngine* GetBackend(uint64_t aWindowId = 0);
  StreamListeners *GetWindowListeners(uint64_t aWindowId) {
    NS_ASSERTION(NS_IsMainThread(), "Only access windowlist on main thread");

    return mActiveWindows.Get(aWindowId);
  }
  void RemoveWindowID(uint64_t aWindowId) {
    mActiveWindows.Remove(aWindowId);
  }
  bool IsWindowStillActive(uint64_t aWindowId) {
    return !!GetWindowListeners(aWindowId);
  }
  // Note: also calls aListener->Remove(), even if inactive
  void RemoveFromWindowList(uint64_t aWindowID,
    GetUserMediaCallbackMediaStreamListener *aListener);

  nsresult GetUserMedia(bool aPrivileged,
    nsPIDOMWindow* aWindow,
    const dom::MediaStreamConstraints& aRawConstraints,
    nsIDOMGetUserMediaSuccessCallback* onSuccess,
    nsIDOMGetUserMediaErrorCallback* onError);

  nsresult GetUserMediaDevices(nsPIDOMWindow* aWindow,
    const dom::MediaStreamConstraints& aConstraints,
    nsIGetUserMediaDevicesSuccessCallback* onSuccess,
    nsIDOMGetUserMediaErrorCallback* onError,
    uint64_t aInnerWindowID = 0);
  void OnNavigation(uint64_t aWindowID);

  MediaEnginePrefs mPrefs;

private:
  WindowTable *GetActiveWindows() {
    NS_ASSERTION(NS_IsMainThread(), "Only access windowlist on main thread");
    return &mActiveWindows;
  }

  void GetPref(nsIPrefBranch *aBranch, const char *aPref,
               const char *aData, int32_t *aVal);
  void GetPrefBool(nsIPrefBranch *aBranch, const char *aPref,
                   const char *aData, bool *aVal);
  void GetPrefs(nsIPrefBranch *aBranch, const char *aData);

  // Make private because we want only one instance of this class
  MediaManager();

  ~MediaManager() {}

  nsresult MediaCaptureWindowStateInternal(nsIDOMWindow* aWindow, bool* aVideo,
                                           bool* aAudio);

  void StopMediaStreams();

  // ONLY access from MainThread so we don't need to lock
  WindowTable mActiveWindows;
  nsRefPtrHashtable<nsStringHashKey, GetUserMediaRunnable> mActiveCallbacks;
  nsClassHashtable<nsUint64HashKey, nsTArray<nsString>> mCallIds;
  // Always exists
  nsCOMPtr<nsIThread> mMediaThread;

  Mutex mMutex;
  // protected with mMutex:
  RefPtr<MediaEngine> mBackend;

  static StaticRefPtr<MediaManager> sSingleton;

#ifdef MOZ_B2G_CAMERA
  nsRefPtr<nsDOMCameraManager> mCameraManager;
#endif
};

} // namespace mozilla
