/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIAENGINEDEFAULT_H_
#define MEDIAENGINEDEFAULT_H_

#include "nsITimer.h"

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "DOMMediaStream.h"
#include "nsComponentManagerUtils.h"
#include "mozilla/Monitor.h"

#include "VideoUtils.h"
#include "MediaEngine.h"
#include "VideoSegment.h"
#include "AudioSegment.h"
#include "StreamTracks.h"
#ifdef MOZ_WEBRTC
#include "MediaEngineCameraVideoSource.h"
#endif
#include "MediaStreamGraph.h"
#include "MediaTrackConstraints.h"

namespace mozilla {

namespace layers {
class ImageContainer;
} // namespace layers

class MediaEngineDefault;

/**
 * The default implementation of the MediaEngine interface.
 */
class MediaEngineDefaultVideoSource : public nsITimerCallback,
#ifdef MOZ_WEBRTC
                                      public MediaEngineCameraVideoSource
#else
                                      public MediaEngineVideoSource
#endif
{
public:
  MediaEngineDefaultVideoSource();

  void GetName(nsAString&) const override;
  void GetUUID(nsACString&) const override;

  nsresult Allocate(const dom::MediaTrackConstraints &aConstraints,
                    const MediaEnginePrefs &aPrefs,
                    const nsString& aDeviceId,
                    const nsACString& aOrigin,
                    AllocationHandle** aOutHandle,
                    const char** aOutBadConstraint) override;
  nsresult Deallocate(AllocationHandle* aHandle) override;
  nsresult Start(SourceMediaStream*, TrackID, const PrincipalHandle&) override;
  nsresult Stop(SourceMediaStream*, TrackID) override;
  nsresult Restart(AllocationHandle* aHandle,
                   const dom::MediaTrackConstraints& aConstraints,
                   const MediaEnginePrefs &aPrefs,
                   const nsString& aDeviceId,
                   const char** aOutBadConstraint) override;
  void SetDirectListeners(bool aHasDirectListeners) override {};
  void NotifyPull(MediaStreamGraph* aGraph,
                  SourceMediaStream *aSource,
                  TrackID aId,
                  StreamTime aDesiredTime,
                  const PrincipalHandle& aPrincipalHandle) override;
  uint32_t GetBestFitnessDistance(
      const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
      const nsString& aDeviceId) const override;

  bool IsFake() override {
    return true;
  }

  dom::MediaSourceEnum GetMediaSource() const override {
    return dom::MediaSourceEnum::Camera;
  }

  nsresult TakePhoto(MediaEnginePhotoCallback* aCallback) override
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

protected:
  ~MediaEngineDefaultVideoSource();

  friend class MediaEngineDefault;

  TrackID mTrackID;
  nsCOMPtr<nsITimer> mTimer;
  // mMonitor protects mImage access/changes, and transitions of mState
  // from kStarted to kStopped (which are combined with EndTrack() and
  // image changes).
  Monitor mMonitor;
  RefPtr<layers::Image> mImage;

  RefPtr<layers::ImageContainer> mImageContainer;

  MediaEnginePrefs mOpts;
  int mCb;
  int mCr;
};

class SineWaveGenerator;

class MediaEngineDefaultAudioSource : public MediaEngineAudioSource
{
public:
  MediaEngineDefaultAudioSource();

  void GetName(nsAString&) const override;
  void GetUUID(nsACString&) const override;

  nsresult Allocate(const dom::MediaTrackConstraints &aConstraints,
                    const MediaEnginePrefs &aPrefs,
                    const nsString& aDeviceId,
                    const nsACString& aOrigin,
                    AllocationHandle** aOutHandle,
                    const char** aOutBadConstraint) override;
  nsresult Deallocate(AllocationHandle* aHandle) override;
  nsresult Start(SourceMediaStream*, TrackID, const PrincipalHandle&) override;
  nsresult Stop(SourceMediaStream*, TrackID) override;
  nsresult Restart(AllocationHandle* aHandle,
                   const dom::MediaTrackConstraints& aConstraints,
                   const MediaEnginePrefs &aPrefs,
                   const nsString& aDeviceId,
                   const char** aOutBadConstraint) override;
  void SetDirectListeners(bool aHasDirectListeners) override {};
  void inline AppendToSegment(AudioSegment& aSegment,
                              TrackTicks aSamples,
                              const PrincipalHandle& aPrincipalHandle);
  void NotifyPull(MediaStreamGraph* aGraph,
                  SourceMediaStream *aSource,
                  TrackID aId,
                  StreamTime aDesiredTime,
                  const PrincipalHandle& aPrincipalHandle) override;

  void NotifyOutputData(MediaStreamGraph* aGraph,
                        AudioDataValue* aBuffer, size_t aFrames,
                        TrackRate aRate, uint32_t aChannels) override
  {}
  void NotifyInputData(MediaStreamGraph* aGraph,
                       const AudioDataValue* aBuffer, size_t aFrames,
                       TrackRate aRate, uint32_t aChannels) override
  {}
  void DeviceChanged() override
  {}
  bool IsFake() override {
    return true;
  }

  dom::MediaSourceEnum GetMediaSource() const override {
    return dom::MediaSourceEnum::Microphone;
  }

  nsresult TakePhoto(MediaEnginePhotoCallback* aCallback) override
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  uint32_t GetBestFitnessDistance(
      const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
      const nsString& aDeviceId) const override;

  NS_DECL_THREADSAFE_ISUPPORTS

protected:
  ~MediaEngineDefaultAudioSource();

  TrackID mTrackID;

  TrackTicks mLastNotify; // Accessed in ::Start(), then on NotifyPull (from MSG thread)

  // Created on Allocate, then accessed from NotifyPull (MSG thread)
  nsAutoPtr<SineWaveGenerator> mSineGenerator;
};


class MediaEngineDefault : public MediaEngine
{
  typedef MediaEngine Super;
public:
  explicit MediaEngineDefault() : mMutex("mozilla::MediaEngineDefault") {}

  void EnumerateVideoDevices(dom::MediaSourceEnum,
                             nsTArray<RefPtr<MediaEngineVideoSource> >*) override;
  void EnumerateAudioDevices(dom::MediaSourceEnum,
                             nsTArray<RefPtr<MediaEngineAudioSource> >*) override;
  void Shutdown() override {
    MutexAutoLock lock(mMutex);

    mVSources.Clear();
    mASources.Clear();
  };

private:
  ~MediaEngineDefault() {}

  Mutex mMutex;
  // protected with mMutex:

  nsTArray<RefPtr<MediaEngineVideoSource> > mVSources;
  nsTArray<RefPtr<MediaEngineAudioSource> > mASources;
};

} // namespace mozilla

#endif /* NSMEDIAENGINEDEFAULT_H_ */
