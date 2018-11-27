/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIAENGINE_REMOTE_VIDEO_SOURCE_H_
#define MEDIAENGINE_REMOTE_VIDEO_SOURCE_H_

#include "prcvar.h"
#include "prthread.h"
#include "nsIThread.h"
#include "nsIRunnable.h"

#include "mozilla/Mutex.h"
#include "nsCOMPtr.h"
#include "nsThreadUtils.h"
#include "DOMMediaStream.h"
#include "nsDirectoryServiceDefs.h"
#include "nsComponentManagerUtils.h"

// Avoid warnings about redefinition of WARN_UNUSED_RESULT
#include "ipc/IPCMessageUtils.h"
#include "VideoUtils.h"
#include "MediaEngineSource.h"
#include "VideoSegment.h"
#include "AudioSegment.h"
#include "StreamTracks.h"
#include "MediaStreamGraph.h"

#include "MediaEngineWrapper.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"

// Camera Access via IPC
#include "CamerasChild.h"

#include "NullTransport.h"

// WebRTC includes
#include "webrtc/common_video/include/i420_buffer_pool.h"
#include "webrtc/modules/video_capture/video_capture_defines.h"

namespace webrtc {
using CaptureCapability = VideoCaptureCapability;
}

namespace mozilla {

// Fitness distance is defined in
// https://w3c.github.io/mediacapture-main/getusermedia.html#dfn-selectsettings

// The main difference of feasibility and fitness distance is that if the
// constraint is required ('max', or 'exact'), and the settings dictionary's
// value for the constraint does not satisfy the constraint, the fitness
// distance is positive infinity. Given a continuous space of settings
// dictionaries comprising all discrete combinations of dimension and frame-rate
// related properties, the feasibility distance is still in keeping with the
// constraints algorithm.
enum DistanceCalculation { kFitness, kFeasibility };

/**
 * The WebRTC implementation of the MediaEngine interface.
 */
class MediaEngineRemoteVideoSource : public MediaEngineSource,
                                     public camera::FrameRelay {
  ~MediaEngineRemoteVideoSource() = default;

  struct CapabilityCandidate {
    explicit CapabilityCandidate(webrtc::CaptureCapability&& aCapability,
                                 uint32_t aDistance = 0)
        : mCapability(std::forward<webrtc::CaptureCapability>(aCapability)),
          mDistance(aDistance) {}

    const webrtc::CaptureCapability mCapability;
    uint32_t mDistance;
  };

  class CapabilityComparator {
   public:
    bool Equals(const CapabilityCandidate& aCandidate,
                const webrtc::CaptureCapability& aCapability) const {
      return aCandidate.mCapability == aCapability;
    }
  };

  bool ChooseCapability(const NormalizedConstraints& aConstraints,
                        const MediaEnginePrefs& aPrefs,
                        const nsString& aDeviceId,
                        webrtc::CaptureCapability& aCapability,
                        const DistanceCalculation aCalculate);

  uint32_t GetDistance(const webrtc::CaptureCapability& aCandidate,
                       const NormalizedConstraintSet& aConstraints,
                       const nsString& aDeviceId,
                       const DistanceCalculation aCalculate) const;

  uint32_t GetFitnessDistance(const webrtc::CaptureCapability& aCandidate,
                              const NormalizedConstraintSet& aConstraints,
                              const nsString& aDeviceId) const;

  uint32_t GetFeasibilityDistance(const webrtc::CaptureCapability& aCandidate,
                                  const NormalizedConstraintSet& aConstraints,
                                  const nsString& aDeviceId) const;

  static void TrimLessFitCandidates(nsTArray<CapabilityCandidate>& aSet);

  uint32_t GetBestFitnessDistance(
      const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
      const nsString& aDeviceId) const override;

 public:
  MediaEngineRemoteVideoSource(int aIndex, camera::CaptureEngine aCapEngine,
                               dom::MediaSourceEnum aMediaSource, bool aScary);

  // ExternalRenderer
  int DeliverFrame(uint8_t* buffer,
                   const camera::VideoFrameProperties& properties) override;

  // MediaEngineSource
  dom::MediaSourceEnum GetMediaSource() const override { return mMediaSource; }
  nsresult Allocate(const dom::MediaTrackConstraints& aConstraints,
                    const MediaEnginePrefs& aPrefs, const nsString& aDeviceId,
                    const ipc::PrincipalInfo& aPrincipalInfo,
                    AllocationHandle** aOutHandle,
                    const char** aOutBadConstraint) override;
  nsresult Deallocate(const RefPtr<const AllocationHandle>& aHandle) override;
  nsresult SetTrack(const RefPtr<const AllocationHandle>& aHandle,
                    const RefPtr<SourceMediaStream>& aStream, TrackID aTrackID,
                    const PrincipalHandle& aPrincipal) override;
  nsresult Start(const RefPtr<const AllocationHandle>& aHandle) override;
  nsresult Reconfigure(const RefPtr<AllocationHandle>& aHandle,
                       const dom::MediaTrackConstraints& aConstraints,
                       const MediaEnginePrefs& aPrefs,
                       const nsString& aDeviceId,
                       const char** aOutBadConstraint) override;
  nsresult FocusOnSelectedSource(
      const RefPtr<const AllocationHandle>& aHandle) override;
  nsresult Stop(const RefPtr<const AllocationHandle>& aHandle) override;
  void Pull(const RefPtr<const AllocationHandle>& aHandle,
            const RefPtr<SourceMediaStream>& aStream, TrackID aTrackID,
            StreamTime aEndOfAppendedData, StreamTime aDesiredTime,
            const PrincipalHandle& aPrincipalHandle) override;

  void GetSettings(dom::MediaTrackSettings& aOutSettings) const override;

  void Refresh(int aIndex);

  void Shutdown() override;

  nsString GetName() const override;
  void SetName(nsString aName);

  nsCString GetUUID() const override;
  void SetUUID(const char* aUUID);

  bool GetScary() const override { return mScary; }

 private:
  // Initialize the needed Video engine interfaces.
  void Init();

  /**
   * Returns the number of capabilities for the underlying device.
   *
   * Guaranteed to return at least one capability.
   */
  size_t NumCapabilities() const;

  /**
   * Returns the capability with index `aIndex` for our assigned device.
   *
   * It is an error to call this with `aIndex >= NumCapabilities()`.
   */
  webrtc::CaptureCapability GetCapability(size_t aIndex) const;

  int mCaptureIndex;
  const dom::MediaSourceEnum
      mMediaSource;  // source of media (camera | application | screen)
  const camera::CaptureEngine mCapEngine;
  const bool mScary;

  // mMutex protects certain members on 3 threads:
  // MediaManager, Cameras IPC and MediaStreamGraph.
  Mutex mMutex;

  // Current state of this source.
  // Set under mMutex on the owning thread. Accessed under one of the two.
  MediaEngineSourceState mState = kReleased;

  // The source stream that we feed video data to.
  // Set under mMutex on the owning thread. Accessed under one of the two.
  RefPtr<SourceMediaStream> mStream;

  // The TrackID in mStream that we feed video data to.
  // Set under mMutex on the owning thread. Accessed under one of the two.
  TrackID mTrackID = TRACK_NONE;

  // The PrincipalHandle that gets attached to the frames we feed to mStream.
  // Set under mMutex on the owning thread. Accessed under one of the two.
  PrincipalHandle mPrincipal = PRINCIPAL_HANDLE_NONE;

  // Set in Start() and Deallocate() on the owning thread.
  // Accessed in DeliverFrame() on the camera IPC thread, guaranteed to happen
  // after Start() and before the end of Stop().
  RefPtr<layers::ImageContainer> mImageContainer;

  // The latest frame delivered from the video capture backend.
  // Protected by mMutex.
  RefPtr<layers::Image> mImage;

  // A buffer pool used to manage the temporary buffer used when rescaling
  // incoming images. Cameras IPC thread only.
  webrtc::I420BufferPool mRescalingBufferPool;

  // The intrinsic size of the latest captured image, so we can feed black
  // images of the same size while stopped.
  // Set under mMutex on the Cameras IPC thread. Accessed under one of the two.
  gfx::IntSize mImageSize = gfx::IntSize(0, 0);

  struct AtomicBool {
    Atomic<bool> mValue;
  };

  // True when resolution settings have been updated from a real frame's
  // resolution. Threadsafe.
  // TODO: This can be removed in bug 1453269.
  const RefPtr<media::Refcountable<AtomicBool>> mSettingsUpdatedByFrame;

  // The current settings of this source.
  // Note that these may be different from the settings of the underlying device
  // since we scale frames to avoid fingerprinting.
  // Members are main thread only.
  const RefPtr<media::Refcountable<dom::MediaTrackSettings>> mSettings;

  // The capability currently chosen by constraints of the user of this source.
  // Set under mMutex on the owning thread. Accessed under one of the two.
  webrtc::CaptureCapability mCapability;

  /**
   * Capabilities that we choose between when applying constraints.
   *
   * This is mutable so that the const method NumCapabilities() can reset it.
   * Owning thread only.
   */
  mutable nsTArray<webrtc::CaptureCapability> mHardcodedCapabilities;

  nsString mDeviceName;
  nsCString mUniqueId;
  nsString mFacingMode;

  // Whether init has successfully completed.
  // Set in Init(), reset in Shutdown().
  // Owning thread only.
  bool mInitDone = false;
};

}  // namespace mozilla

#endif /* MEDIAENGINE_REMOTE_VIDEO_SOURCE_H_ */
