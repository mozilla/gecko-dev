/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(RtspMediaResource_h_)
#define RtspMediaResource_h_

#include "MediaResource.h"
#include "mozilla/Monitor.h"
#include "nsITimer.h"
#include "VideoUtils.h"

namespace mozilla {

class RtspTrackBuffer;

/* RtspMediaResource
 * RtspMediaResource provides an interface to deliver and control RTSP media
 * data to RtspDecoder.
 *
 * RTSP Flow Start vs HTTP Flow Start:
 * For HTTP (and files stored on disk), once the channel is created and response
 * data is available, HTMLMediaElement::MediaLoadListener::OnStartRequest is
 * called. (Note, this is an asynchronous call following channel->AsyncOpen).
 * The decoder and MediaResource are set up to talk to each other:
 * InitializeDecoderForChannel and FinishDecoderSetup.
 * RtspMediaResource is different from this, in that FinishDecoderSetup is
 * postponed until after the initial connection with the server is made.
 * RtspController, owned by RtspMediaResource, provides the interface to setup
 * the connection, and calls RtspMediaResource::Listener::OnConnected
 * (from nsIStreamingProtocolListener). FinishDecoderSetup is then called to
 * connect RtspMediaResource with RtspDecoder and allow HTMLMediaElement to
 * request playback etc.
 *
 * Playback:
 * When the user presses play/pause, HTMLMediaElement::Play/::Pause is called,
 * subsequently making calls to the decoder state machine. Upon these state
 * changes, the decoder is told to start reading and decoding data. This causes
 * the nsIStreamingMediaController object to send play/pause commands to the
 * server.
 * Data is then delivered to the host and eventually written to the
 * RtspTrackBuffer objects. Note that RtspMediaResource does not know about the
 * play or pause state. It only knows about the data written into its buffers.
 *
 * Data Structures and Flow:
 * Unlike HTTP, RTSP provides separate streams for audio and video.
 * As such, it creates two RtspTrackBuffer objects for the audio and video data.
 * Data is read using the function ReadFrameFromTrack. These buffer objects are
 * ring buffers, implying that data from the network may be discarded if the
 * decoder cannot read at a high enough rate.
 *
 * Data is delivered via RtspMediaResource::Listener::OnMediaDataAvailable.
 * This Listener implements nsIStreamingProtocolListener, and writes the data to
 * the appropriate RtspTrackBuffer. The decoder then reads the data by calling
 * RtspMediaResource::ReadFrameFromTrack. Note that the decoder and decode
 * thread will be blocked until data is available in one of the two buffers.
 *
 * Seeking:
 * Since the frame data received after seek is not continuous with existing
 * frames in RtspTrackBuffer, the buffer must be cleared. If we don't clear the
 * old frame data in RtspTrackBuffer, the decoder's behavior will be
 * unpredictable. So we add |mFrameType| in RtspTrackBuffer to do this:
 * When we are seeking, the mFrameType flag is set, and RtspTrackBuffer will
 * drop the incoming data until the RTSP server completes the seek operation.
 * Note: seeking for RTSP is carried out based on sending the seek time to the
 * server, unlike HTTP in which the seek time is converted to a byte offset.
 * Thus, RtspMediaResource has a SeekTime function which should be called
 * instead of Seek.
 * */
class RtspMediaResource : public BaseMediaResource
{
public:
  RtspMediaResource(MediaDecoder* aDecoder, nsIChannel* aChannel, nsIURI* aURI,
                    const nsACString& aContentType);
  virtual ~RtspMediaResource();

  // The following methods can be called on any thread.

  // Get the RtspMediaResource pointer if this MediaResource is a
  // RtspMediaResource. For calling Rtsp specific functions.
  virtual RtspMediaResource* GetRtspPointer() override final {
    return this;
  }

  // Returns the nsIStreamingProtocolController in the RtspMediaResource.
  // RtspMediaExtractor: request it to get mime type for creating decoder.
  // RtspOmxDecoder: request it to send play/pause commands to RTSP server.
  // The lifetime of mMediaStreamController is controlled by RtspMediaResource
  // because the RtspMediaExtractor and RtspOmxDecoder won't hold the reference.
  nsIStreamingProtocolController* GetMediaStreamController() {
    return mMediaStreamController;
  }

  // Even it is a live stream, as long as it provides valid timestamps,
  // we tell state machine it's not a live stream.
  virtual bool IsRealTime() override {
    return !mHasTimestamp;
  }

  // Called by RtspOmxReader, dispatch a runnable to notify mDecoder.
  // Other thread only.
  void SetSuspend(bool aIsSuspend);

  // The following methods can be called on any thread except main thread.

  // Read data from track.
  // Parameters:
  //   aToBuffer, aToBufferSize: buffer pointer and buffer size.
  //   aReadCount: output actual read bytes.
  //   aFrameTime: output frame time stamp.
  //   aFrameSize: actual data size in track.
  nsresult ReadFrameFromTrack(uint8_t* aBuffer, uint32_t aBufferSize,
                              uint32_t aTrackIdx, uint32_t& aBytes,
                              uint64_t& aTime, uint32_t& aFrameSize);

  // Seek to the given time offset
  nsresult SeekTime(int64_t aOffset);

  // The idea of playout delay is to hold frames in the playout buffer
  // (RtspTrackBuffer) for a period of time in order to smooth timing variations
  // caused by the network.
  void EnablePlayoutDelay();
  void DisablePlayoutDelay();

  // dummy
  virtual nsresult ReadAt(int64_t aOffset, char* aBuffer,
                          uint32_t aCount, uint32_t* aBytes)  override{
    return NS_ERROR_FAILURE;
  }
  // dummy
  virtual void     SetReadMode(MediaCacheStream::ReadMode aMode) override {}
  // dummy
  virtual void     SetPlaybackRate(uint32_t aBytesPerSecond) override {}
  // dummy
  virtual int64_t  Tell() override { return 0; }

  // Any thread
  virtual void    Pin() override {}
  virtual void    Unpin() override {}

  virtual bool    IsSuspendedByCache() override { return mIsSuspend; }

  virtual bool    IsSuspended() override { return false; }
  virtual bool    IsTransportSeekable() override { return true; }
  // dummy
  virtual double  GetDownloadRate(bool* aIsReliable) override { *aIsReliable = false; return 0; }

  virtual int64_t GetLength() override {
    if (mIsLiveStream) {
      return -1;
    }
    return 0;
  }

  // dummy
  virtual int64_t GetNextCachedData(int64_t aOffset) override { return 0; }
  // dummy
  virtual int64_t GetCachedDataEnd(int64_t aOffset) override { return 0; }
  // dummy
  virtual bool    IsDataCachedToEndOfResource(int64_t aOffset) override {
    return false;
  }
  // dummy
  nsresult GetCachedRanges(nsTArray<MediaByteRange>& aRanges) override {
    return NS_ERROR_FAILURE;
  }

  // The following methods can be called on main thread only.

  virtual nsresult Open(nsIStreamListener** aStreamListener) override;
  virtual nsresult Close() override;
  virtual void     Suspend(bool aCloseImmediately) override;
  virtual void     Resume() override;
  virtual already_AddRefed<nsIPrincipal> GetCurrentPrincipal() override;
  virtual bool     CanClone() override {
    return false;
  }
  virtual already_AddRefed<MediaResource> CloneData(MediaDecoder* aDecoder)
  override {
    return nullptr;
  }
  // dummy
  virtual nsresult ReadFromCache(char* aBuffer, int64_t aOffset,
                                 uint32_t aCount) override {
    return NS_ERROR_FAILURE;
  }

  virtual size_t SizeOfExcludingThis(
                      MallocSizeOf aMallocSizeOf) const override;

  virtual size_t SizeOfIncludingThis(
                      MallocSizeOf aMallocSizeOf) const override {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  // Listener implements nsIStreamingProtocolListener as
  // mMediaStreamController's callback function.
  // It holds RtspMediaResource reference to notify the connection status and
  // data arrival. The Revoke function releases the reference when
  // RtspMediaResource::OnDisconnected is called.
  class Listener final : public nsIInterfaceRequestor,
                         public nsIStreamingProtocolListener
  {
    ~Listener() {}
  public:
    explicit Listener(RtspMediaResource* aResource) : mResource(aResource) {}

    NS_DECL_ISUPPORTS
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSISTREAMINGPROTOCOLLISTENER

    void Revoke();

  private:
    nsRefPtr<RtspMediaResource> mResource;
  };
  friend class Listener;

protected:
  // Main thread access only.
  // These are called on the main thread by Listener.
  nsresult OnMediaDataAvailable(uint8_t aIndex, const nsACString& aData,
                                uint32_t aLength, uint32_t aOffset,
                                nsIStreamingProtocolMetaData* aMeta);
  nsresult OnConnected(uint8_t aIndex, nsIStreamingProtocolMetaData* aMeta);
  nsresult OnDisconnected(uint8_t aIndex, nsresult aReason);

  nsRefPtr<Listener> mListener;

private:
  // Notify mDecoder the rtsp stream is suspend. Main thread only.
  void NotifySuspend(bool aIsSuspend);
  bool IsVideoEnabled();
  bool IsVideo(uint8_t tracks, nsIStreamingProtocolMetaData *meta);
  // These two members are created at |RtspMediaResource::OnConnected|.
  nsCOMPtr<nsIStreamingProtocolController> mMediaStreamController;
  nsTArray<nsAutoPtr<RtspTrackBuffer>> mTrackBuffer;

  // A flag that indicates the |RtspMediaResource::OnConnected| has already been
  // called.
  bool mIsConnected;
  // Whether it's a live stream.
  bool mIsLiveStream;
  // Whether it provides timestamps.
  bool mHasTimestamp;
  // Indicate the rtsp controller is suspended or not. Main thread only.
  bool mIsSuspend;
};

} // namespace mozilla

#endif

