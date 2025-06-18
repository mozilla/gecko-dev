/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChannelMediaDecoder_h_
#define ChannelMediaDecoder_h_

#include "MediaDecoder.h"
#include "MediaResourceCallback.h"
#include "MediaChannelStatistics.h"

class nsIChannel;
class nsIStreamListener;

namespace mozilla {

class BaseMediaResource;

DDLoggedTypeDeclNameAndBase(ChannelMediaDecoder, MediaDecoder);

class ChannelMediaDecoder
    : public MediaDecoder,
      public DecoderDoctorLifeLogger<ChannelMediaDecoder> {
  // Used to register with MediaResource to receive notifications which will
  // be forwarded to MediaDecoder.
  class ResourceCallback : public MediaResourceCallback {
    // Throttle calls to MediaDecoder::NotifyDataArrived()
    // to be at most once per 500ms.
    static const uint32_t sDelay = 500;

   public:
    explicit ResourceCallback(AbstractThread* aMainThread);
    // Start to receive notifications from ResourceCallback.
    void Connect(ChannelMediaDecoder* aDecoder);
    // Called upon shutdown to stop receiving notifications.
    void Disconnect();

   private:
    ~ResourceCallback();

    /* MediaResourceCallback functions */
    AbstractThread* AbstractMainThread() const override;
    MediaDecoderOwner* GetMediaOwner() const override;
    void NotifyNetworkError(const MediaResult& aError) override;
    void NotifyDataArrived() override;
    void NotifyDataEnded(nsresult aStatus) override;
    void NotifyPrincipalChanged() override;
    void NotifySuspendedStatusChanged(bool aSuspendedByCache) override;

    static void TimerCallback(nsITimer* aTimer, void* aClosure);

    // The decoder to send notifications. Main-thread only.
    ChannelMediaDecoder* mDecoder = nullptr;
    nsCOMPtr<nsITimer> mTimer;
    bool mTimerArmed = false;
    const RefPtr<AbstractThread> mAbstractMainThread;
  };

 protected:
  void ShutdownInternal() override;
  void OnPlaybackEvent(MediaPlaybackEvent&& aEvent) override;
  void DurationChanged() override;
  void MetadataLoaded(UniquePtr<MediaInfo> aInfo, UniquePtr<MetadataTags> aTags,
                      MediaDecoderEventVisibility aEventVisibility) override;
  void NotifyPrincipalChanged() override;

  RefPtr<ResourceCallback> mResourceCallback;
  RefPtr<BaseMediaResource> mResource;

  explicit ChannelMediaDecoder(MediaDecoderInit& aInit);

  void GetDebugInfo(dom::MediaDecoderDebugInfo& aInfo);

 public:
  // Create a decoder for the given aType. Returns null if we were unable
  // to create the decoder, for example because the requested MIME type in
  // the init struct was unsupported.
  static already_AddRefed<ChannelMediaDecoder> Create(
      MediaDecoderInit& aInit, DecoderDoctorDiagnostics* aDiagnostics);

  void Shutdown() override;

  bool CanClone();

  // Create a new decoder of the same type as this one.
  already_AddRefed<ChannelMediaDecoder> Clone(MediaDecoderInit& aInit);

  nsresult Load(nsIChannel* aChannel, bool aIsPrivateBrowsing,
                nsIStreamListener** aStreamListener);

  void AddSizeOfResources(ResourceSizes* aSizes) override;
  already_AddRefed<nsIPrincipal> GetCurrentPrincipal() override;
  bool HadCrossOriginRedirects() override;
  bool IsTransportSeekable() override;
  void SetLoadInBackground(bool aLoadInBackground) override;
  void Suspend() override;
  void Resume() override;

 private:
  // A snapshot of the media playback and download state used to determine if
  // playback can proceed without interruption.
  struct MediaStatistics {
    // Estimate of the current playback rate (bytes/second).
    double mPlaybackByteRate;
    // Estimate of the current download rate (bytes/second). This
    // ignores time that the channel was paused by Gecko.
    double mDownloadByteRate;
    // Total length of media stream in bytes; -1 if not known
    int64_t mTotalBytes;
    // Current position of the download, in bytes. This is the offset of
    // the first uncached byte after the decoder position.
    int64_t mDownloadBytePosition;
    // Current position of playback, in bytes
    int64_t mPlaybackByteOffset;
    // If false, then mDownloadRate cannot be considered a reliable
    // estimate (probably because the download has only been running
    // a short time).
    bool mDownloadByteRateReliable;
    // If false, then mPlaybackRate cannot be considered a reliable
    // estimate (probably because playback has only been running
    // a short time).
    bool mPlaybackByteRateReliable;

    bool CanPlayThrough() const;
    nsCString ToString() const;
  };

  void DownloadProgressed();

  // Create a new state machine to run this decoder.
  MediaDecoderStateMachineBase* CreateStateMachine(
      bool aDisableExternalEngine) override;

  nsresult Load(BaseMediaResource* aOriginal);

  // Called by MediaResource when the download has ended.
  // Called on the main thread only. aStatus is the result from OnStopRequest.
  void NotifyDownloadEnded(nsresult aStatus);

  // Called by the MediaResource to keep track of the number of bytes read
  // from the resource. Called on the main by an event runner dispatched
  // by the MediaResource read functions.
  void NotifyBytesConsumed(int64_t aBytes, int64_t aOffset);

  bool CanPlayThroughImpl() final;

  struct PlaybackRateInfo {
    uint32_t mRate;  // Estimate of the current playback rate (bytes/second).
    bool mReliable;  // True if mRate is a reliable estimate.
  };

  // Return a PlaybackRateInfo and update the expected byte rate per second for
  // playback in the media resource, which improves cache usage prediction
  // accuracy. This can only be run off the main thread.
  static PlaybackRateInfo UpdateResourceOfPlaybackByteRate(
      const MediaChannelStatistics& aStats, BaseMediaResource* aResource,
      const media::TimeUnit& aDuration);

  bool ShouldThrottleDownload(const MediaStatistics& aStats);

  // Data needed to estimate playback data rate. The timeline used for
  // this estimate is "decode time" (where the "current time" is the
  // time of the last decoded video frame).
  MediaChannelStatistics mPlaybackStatistics;

  // Current playback byte offset in the stream. This is (approximately)
  // where we're up to playing back the stream. This is not adjusted immediately
  // after seek happens, but it will be updated when playback starts or stops.
  int64_t mPlaybackByteOffset = 0;

  bool mCanPlayThrough = false;

  // True if we've been notified that the ChannelMediaResource has
  // a principal.
  bool mInitialChannelPrincipalKnown = false;

  // Set in Shutdown() when we start closing mResource, if mResource is set.
  // Must resolve before we unregister the shutdown blocker.
  RefPtr<GenericPromise> mResourceClosePromise;
};

}  // namespace mozilla

#endif  // ChannelMediaDecoder_h_
