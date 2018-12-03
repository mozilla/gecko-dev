/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#ifndef mediapipeline_h__
#define mediapipeline_h__

#include <map>

#include "sigslot.h"
#include "transportlayer.h"  // For TransportLayer::State

#include "signaling/src/media-conduit/MediaConduitInterface.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/Atomics.h"
#include "SrtpFlow.h"  // For SRTP_MAX_EXPANSION
#include "mediapacket.h"
#include "mtransport/runnable_utils.h"
#include "AudioPacketizer.h"
#include "StreamTracks.h"
#include "signaling/src/peerconnection/PacketDumper.h"

#include "webrtc/modules/rtp_rtcp/include/rtp_header_parser.h"

// Should come from MediaEngine.h, but that's a pain to include here
// because of the MOZILLA_EXTERNAL_LINKAGE stuff.
#define WEBRTC_MAX_SAMPLE_RATE 48000

class nsIPrincipal;

namespace mozilla {
class MediaPipelineFilter;
class MediaTransportBase;
class PeerIdentity;
class AudioProxyThread;
class VideoFrameConverter;

namespace dom {
class MediaStreamTrack;
struct RTCRTPContributingSourceStats;
}  // namespace dom

class SourceMediaStream;

// A class that represents the pipeline of audio and video
// The dataflow looks like:
//
// TRANSMIT
// CaptureDevice -> stream -> [us] -> conduit -> [us] -> transport -> network
//
// RECEIVE
// network -> transport -> [us] -> conduit -> [us] -> stream -> Playout
//
// The boxes labeled [us] are just bridge logic implemented in this class
//
// We have to deal with a number of threads:
//
// GSM:
//   * Assembles the pipeline
// SocketTransportService
//   * Receives notification that ICE and DTLS have completed
//   * Processes incoming network data and passes it to the conduit
//   * Processes outgoing RTP and RTCP
// MediaStreamGraph
//   * Receives outgoing data from the MediaStreamGraph
//   * Receives pull requests for more data from the
//     MediaStreamGraph
// One or another GIPS threads
//   * Receives RTCP messages to send to the other side
//   * Processes video frames GIPS wants to render
//
// For a transmitting conduit, "output" is RTP and "input" is RTCP.
// For a receiving conduit, "input" is RTP and "output" is RTCP.
//

class MediaPipeline : public sigslot::has_slots<> {
 public:
  enum class DirectionType { TRANSMIT, RECEIVE };
  MediaPipeline(const std::string& aPc, MediaTransportBase* aTransportHandler,
                DirectionType aDirection, nsCOMPtr<nsIEventTarget> aMainThread,
                nsCOMPtr<nsIEventTarget> aStsThread,
                RefPtr<MediaSessionConduit> aConduit);

  virtual void Start() = 0;
  virtual void Stop() = 0;
  virtual void DetachMedia() {}

  void SetLevel(size_t aLevel) { mLevel = aLevel; }

  // Must be called on the main thread.
  void Shutdown_m();

  void UpdateTransport_m(const std::string& aTransportId,
                         nsAutoPtr<MediaPipelineFilter> aFilter);

  void UpdateTransport_s(const std::string& aTransportId,
                         nsAutoPtr<MediaPipelineFilter> aFilter);

  // Used only for testing; adds RTP header extension for RTP Stream Id with
  // the given id.
  void AddRIDExtension_m(size_t aExtensionId);
  void AddRIDExtension_s(size_t aExtensionId);
  // Used only for testing; installs a MediaPipelineFilter that filters
  // everything but the given RID
  void AddRIDFilter_m(const std::string& aRid);
  void AddRIDFilter_s(const std::string& aRid);

  virtual DirectionType Direction() const { return mDirection; }
  int Level() const { return mLevel; }
  virtual bool IsVideo() const = 0;

  class RtpCSRCStats {
   public:
    // Gets an expiration time for CRC info given a reference time,
    //   this reference time would normally be the time of calling.
    //   This value can then be used to check if a RtpCSRCStats
    //   has expired via Expired(...)
    static DOMHighResTimeStamp GetExpiryFromTime(
        const DOMHighResTimeStamp aTime);

    RtpCSRCStats(const uint32_t aCsrc, const DOMHighResTimeStamp aTime);
    ~RtpCSRCStats(){};
    // Initialize a webidl representation suitable for adding to a report.
    //   This assumes that the webidl object is empty.
    // @param aWebidlObj the webidl binding object to popluate
    // @param aInboundRtpStreamId the associated RTCInboundRTPStreamStats.id
    void GetWebidlInstance(dom::RTCRTPContributingSourceStats& aWebidlObj,
                           const nsString& aInboundRtpStreamId) const;
    void SetTimestamp(const DOMHighResTimeStamp aTime) { mTimestamp = aTime; }
    // Check if the RtpCSRCStats has expired, checks against a
    //   given expiration time.
    bool Expired(const DOMHighResTimeStamp aExpiry) const {
      return mTimestamp < aExpiry;
    }

   private:
    static const double constexpr EXPIRY_TIME_MILLISECONDS = 10 * 1000;
    const uint32_t mCsrc;
    DOMHighResTimeStamp mTimestamp;
  };

  // Gets the gathered contributing source stats for the last expiration period.
  // @param aId the stream id to use for populating inboundRtpStreamId field
  // @param aArr the array to append the stats objects to
  void GetContributingSourceStats(
      const nsString& aInboundStreamId,
      FallibleTArray<dom::RTCRTPContributingSourceStats>& aArr) const;

  int32_t RtpPacketsSent() const { return mRtpPacketsSent; }
  int64_t RtpBytesSent() const { return mRtpBytesSent; }
  int32_t RtcpPacketsSent() const { return mRtcpPacketsSent; }
  int32_t RtpPacketsReceived() const { return mRtpPacketsReceived; }
  int64_t RtpBytesReceived() const { return mRtpBytesReceived; }
  int32_t RtcpPacketsReceived() const { return mRtcpPacketsReceived; }

  MediaSessionConduit* Conduit() const { return mConduit; }

  // Thread counting
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaPipeline)

  // Separate class to allow ref counting
  class PipelineTransport : public TransportInterface {
   public:
    // Implement the TransportInterface functions
    explicit PipelineTransport(nsIEventTarget* aStsThread)
        : mPipeline(nullptr), mStsThread(aStsThread) {}

    void Attach(MediaPipeline* pipeline) { mPipeline = pipeline; }
    void Detach() { mPipeline = nullptr; }
    MediaPipeline* Pipeline() const { return mPipeline; }

    virtual nsresult SendRtpPacket(const uint8_t* aData, size_t aLen) override;
    virtual nsresult SendRtcpPacket(const uint8_t* aData, size_t aLen) override;

   private:
    void SendRtpRtcpPacket_s(nsAutoPtr<MediaPacket> aPacket);

    // Creates a cycle, which we break with Detach
    RefPtr<MediaPipeline> mPipeline;
    const nsCOMPtr<nsIEventTarget> mStsThread;
  };

 protected:
  virtual ~MediaPipeline();
  friend class PipelineTransport;

  // The transport is ready
  virtual void TransportReady_s() {}

  void IncrementRtpPacketsSent(int aBytes);
  void IncrementRtcpPacketsSent();
  void IncrementRtpPacketsReceived(int aBytes);
  virtual void OnRtpPacketReceived(){};
  void IncrementRtcpPacketsReceived();

  virtual void SendPacket(MediaPacket& packet);

  // Process slots on transports
  void RtpStateChange(const std::string& aTransportId, TransportLayer::State);
  void RtcpStateChange(const std::string& aTransportId, TransportLayer::State);
  virtual void CheckTransportStates();
  void PacketReceived(const std::string& aTransportId, MediaPacket& packet);

  void RtpPacketReceived(MediaPacket& packet);
  void RtcpPacketReceived(MediaPacket& packet);

  void EncryptedPacketSending(const std::string& aTransportId,
                              MediaPacket& aPacket);

  void SetDescription_s(const std::string& description);

  const DirectionType mDirection;
  size_t mLevel;
  std::string mTransportId;
  RefPtr<MediaTransportBase> mTransportHandler;
  RefPtr<MediaSessionConduit> mConduit;  // Our conduit. Written on the main
                                         // thread. Read on STS thread.

  TransportLayer::State mRtpState = TransportLayer::TS_NONE;
  TransportLayer::State mRtcpState = TransportLayer::TS_NONE;
  bool mSignalsConnected = false;

  // Pointers to the threads we need. Initialized at creation
  // and used all over the place.
  const nsCOMPtr<nsIEventTarget> mMainThread;
  const nsCOMPtr<nsIEventTarget> mStsThread;

  // Created in c'tor. Referenced by the conduit.
  RefPtr<PipelineTransport> mTransport;

  // Only safe to access from STS thread.
  int32_t mRtpPacketsSent;
  int32_t mRtcpPacketsSent;
  int32_t mRtpPacketsReceived;
  int32_t mRtcpPacketsReceived;
  int64_t mRtpBytesSent;
  int64_t mRtpBytesReceived;

  // Only safe to access from STS thread.
  std::map<uint32_t, RtpCSRCStats> mCsrcStats;

  // Written in c'tor. Read on STS thread.
  const std::string mPc;
  std::string mDescription;

  // Written in c'tor, all following accesses are on the STS thread.
  nsAutoPtr<MediaPipelineFilter> mFilter;
  const nsAutoPtr<webrtc::RtpHeaderParser> mRtpParser;

  nsAutoPtr<PacketDumper> mPacketDumper;

 private:
  // Gets the current time as a DOMHighResTimeStamp
  static DOMHighResTimeStamp GetNow();

  bool IsRtp(const unsigned char* aData, size_t aLen) const;
  // Must be called on the STS thread.  Must be called after DetachMedia().
  void DetachTransport_s();
};

// A specialization of pipeline for reading from an input device
// and transmitting to the network.
class MediaPipelineTransmit : public MediaPipeline {
 public:
  // Set aRtcpTransport to nullptr to use rtcp-mux
  MediaPipelineTransmit(const std::string& aPc,
                        MediaTransportBase* aTransportHandler,
                        nsCOMPtr<nsIEventTarget> aMainThread,
                        nsCOMPtr<nsIEventTarget> aStsThread, bool aIsVideo,
                        RefPtr<MediaSessionConduit> aConduit);

  bool Transmitting() const;

  void Start() override;
  void Stop() override;

  // written and used from MainThread
  bool IsVideo() const override;

  // When the principal of the domtrack changes, it calls through to here
  // so that we can determine whether to enable track transmission.
  // `track` has to be null or equal `mDomTrack` for us to apply the update.
  virtual void UpdateSinkIdentity_m(const dom::MediaStreamTrack* aTrack,
                                    nsIPrincipal* aPrincipal,
                                    const PeerIdentity* aSinkIdentity);

  // Called on the main thread.
  void DetachMedia() override;

  // Override MediaPipeline::TransportReady_s.
  void TransportReady_s() override;

  // Replace a track with a different one
  // In non-compliance with the likely final spec, allow the new
  // track to be part of a different stream (since we don't support
  // multiple tracks of a type in a stream yet).  bug 1056650
  virtual nsresult SetTrack(dom::MediaStreamTrack* aDomTrack);

  // Separate classes to allow ref counting
  class PipelineListener;
  class VideoFrameFeeder;

 protected:
  ~MediaPipelineTransmit();

  void SetDescription();

 private:
  const bool mIsVideo;
  const RefPtr<PipelineListener> mListener;
  const RefPtr<VideoFrameFeeder> mFeeder;
  RefPtr<AudioProxyThread> mAudioProcessing;
  RefPtr<VideoFrameConverter> mConverter;
  RefPtr<dom::MediaStreamTrack> mDomTrack;
  bool mTransmitting;
};

// A specialization of pipeline for reading from the network and
// rendering media.
class MediaPipelineReceive : public MediaPipeline {
 public:
  // Set aRtcpTransport to nullptr to use rtcp-mux
  MediaPipelineReceive(const std::string& aPc,
                       MediaTransportBase* aTransportHandler,
                       nsCOMPtr<nsIEventTarget> aMainThread,
                       nsCOMPtr<nsIEventTarget> aStsThread,
                       RefPtr<MediaSessionConduit> aConduit);

  // Sets the PrincipalHandle we set on the media chunks produced by this
  // pipeline. Must be called on the main thread.
  virtual void SetPrincipalHandle_m(
      const PrincipalHandle& aPrincipalHandle) = 0;

 protected:
  ~MediaPipelineReceive();
};

// A specialization of pipeline for reading from the network and
// rendering audio.
class MediaPipelineReceiveAudio : public MediaPipelineReceive {
 public:
  MediaPipelineReceiveAudio(const std::string& aPc,
                            MediaTransportBase* aTransportHandler,
                            nsCOMPtr<nsIEventTarget> aMainThread,
                            nsCOMPtr<nsIEventTarget> aStsThread,
                            RefPtr<AudioSessionConduit> aConduit,
                            dom::MediaStreamTrack* aTrack);

  void DetachMedia() override;

  bool IsVideo() const override { return false; }

  void SetPrincipalHandle_m(const PrincipalHandle& aPrincipalHandle) override;

  void Start() override;
  void Stop() override;

  void OnRtpPacketReceived() override;

 private:
  // Separate class to allow ref counting
  class PipelineListener;

  RefPtr<PipelineListener> mListener;
};

// A specialization of pipeline for reading from the network and
// rendering video.
class MediaPipelineReceiveVideo : public MediaPipelineReceive {
 public:
  MediaPipelineReceiveVideo(const std::string& aPc,
                            MediaTransportBase* aTransportHandler,
                            nsCOMPtr<nsIEventTarget> aMainThread,
                            nsCOMPtr<nsIEventTarget> aStsThread,
                            RefPtr<VideoSessionConduit> aConduit,
                            dom::MediaStreamTrack* aTrack);

  // Called on the main thread.
  void DetachMedia() override;

  bool IsVideo() const override { return true; }

  void SetPrincipalHandle_m(const PrincipalHandle& aPrincipalHandle) override;

  void Start() override;
  void Stop() override;

  void OnRtpPacketReceived() override;

 private:
  class PipelineRenderer;
  friend class PipelineRenderer;

  // Separate class to allow ref counting
  class PipelineListener;

  const RefPtr<PipelineRenderer> mRenderer;
  RefPtr<PipelineListener> mListener;
};

}  // namespace mozilla
#endif
