/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#include <iostream>

#include "logging.h"
#include "nss.h"

#include "AudioSegment.h"
#include "AudioStreamTrack.h"
#include "DOMMediaStream.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "MediaPipeline.h"
#include "MediaPipelineFilter.h"
#include "MediaStreamGraph.h"
#include "MediaStreamListener.h"
#include "MediaStreamTrack.h"
#include "transportflow.h"
#include "transportlayerloopback.h"
#include "transportlayerdtls.h"
#include "transportlayersrtp.h"
#include "mozilla/SyncRunnable.h"
#include "mtransport_test_utils.h"
#include "SharedBuffer.h"
#include "MediaTransportHandler.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

using namespace mozilla;
MOZ_MTLOG_MODULE("mediapipeline")

static MtransportTestUtils* test_utils;

namespace {

class FakeSourceMediaStream : public mozilla::SourceMediaStream {
 public:
  FakeSourceMediaStream() : SourceMediaStream() {}

  virtual ~FakeSourceMediaStream() override { mMainThreadDestroyed = true; }

  virtual StreamTime AppendToTrack(
      TrackID aID, MediaSegment* aSegment,
      MediaSegment* aRawSegment = nullptr) override {
    return aSegment->GetDuration();
  }
};

class FakeMediaStreamTrackSource : public mozilla::dom::MediaStreamTrackSource {
 public:
  FakeMediaStreamTrackSource() : MediaStreamTrackSource(nullptr, nsString()) {}

  virtual mozilla::dom::MediaSourceEnum GetMediaSource() const override {
    return mozilla::dom::MediaSourceEnum::Microphone;
  }

  virtual void Disable() override {}

  virtual void Enable() override {}

  virtual void Stop() override {}
};

class FakeAudioStreamTrack : public mozilla::dom::AudioStreamTrack {
 public:
  FakeAudioStreamTrack()
      : AudioStreamTrack(new DOMMediaStream(nullptr), 0, 1,
                         new FakeMediaStreamTrackSource()),
        mMutex("Fake AudioStreamTrack"),
        mStop(false),
        mCount(0) {
    NS_NewTimerWithFuncCallback(
        getter_AddRefs(mTimer), FakeAudioStreamTrackGenerateData, this, 20,
        nsITimer::TYPE_REPEATING_SLACK,
        "FakeAudioStreamTrack::FakeAudioStreamTrackGenerateData",
        test_utils->sts_target());
  }

  void Stop() {
    mozilla::MutexAutoLock lock(mMutex);
    mStop = true;
    mTimer->Cancel();
  }

  virtual void AddListener(MediaStreamTrackListener* aListener) override {
    mozilla::MutexAutoLock lock(mMutex);
    mListeners.push_back(aListener);
  }

 private:
  std::vector<MediaStreamTrackListener*> mListeners;
  mozilla::Mutex mMutex;
  bool mStop;
  nsCOMPtr<nsITimer> mTimer;
  int mCount;

  static void FakeAudioStreamTrackGenerateData(nsITimer* timer, void* closure) {
    auto mst = static_cast<FakeAudioStreamTrack*>(closure);
    const int AUDIO_BUFFER_SIZE = 1600;
    const int NUM_CHANNELS = 2;

    mozilla::MutexAutoLock lock(mst->mMutex);
    if (mst->mStop) {
      return;
    }

    RefPtr<mozilla::SharedBuffer> samples = mozilla::SharedBuffer::Create(
        AUDIO_BUFFER_SIZE * NUM_CHANNELS * sizeof(int16_t));
    int16_t* data = reinterpret_cast<int16_t*>(samples->Data());
    for (int i = 0; i < (AUDIO_BUFFER_SIZE * NUM_CHANNELS); i++) {
      // saw tooth audio sample
      data[i] = ((mst->mCount % 8) * 4000) - (7 * 4000) / 2;
      mst->mCount++;
    }

    mozilla::AudioSegment segment;
    AutoTArray<const int16_t*, 1> channels;
    channels.AppendElement(data);
    segment.AppendFrames(samples.forget(), channels, AUDIO_BUFFER_SIZE,
                         PRINCIPAL_HANDLE_NONE);

    for (auto& listener : mst->mListeners) {
      listener->NotifyQueuedChanges(nullptr, 0, segment);
    }
  }
};

class LoopbackTransport : public MediaTransportBase {
 public:
  LoopbackTransport() {
    SetState("mux", TransportLayer::TS_INIT, false);
    SetState("mux", TransportLayer::TS_INIT, true);
    SetState("non-mux", TransportLayer::TS_INIT, false);
    SetState("non-mux", TransportLayer::TS_INIT, true);
  }

  static void InitAndConnect(LoopbackTransport& client,
                             LoopbackTransport& server) {
    client.Connect(&server);
    server.Connect(&client);
  }

  void Connect(LoopbackTransport* peer) { peer_ = peer; }

  void Shutdown() { peer_ = nullptr; }

  void SendPacket(const std::string& aTransportId,
                  MediaPacket& aPacket) override {
    peer_->SignalPacketReceived(aTransportId, aPacket);
  }

  TransportLayer::State GetState(const std::string& aTransportId,
                                 bool aRtcp) const override {
    if (aRtcp) {
      auto it = mRtcpStates.find(aTransportId);
      if (it != mRtcpStates.end()) {
        return it->second;
      }
    } else {
      auto it = mRtpStates.find(aTransportId);
      if (it != mRtpStates.end()) {
        return it->second;
      }
    }

    return TransportLayer::TS_NONE;
  }

  void SetState(const std::string& aTransportId, TransportLayer::State aState,
                bool aRtcp) {
    if (aRtcp) {
      mRtcpStates[aTransportId] = aState;
      SignalRtcpStateChange(aTransportId, aState);
    } else {
      mRtpStates[aTransportId] = aState;
      SignalStateChange(aTransportId, aState);
    }
  }

 private:
  RefPtr<MediaTransportBase> peer_;
  std::map<std::string, TransportLayer::State> mRtpStates;
  std::map<std::string, TransportLayer::State> mRtcpStates;
};

class TestAgent {
 public:
  TestAgent()
      : audio_config_(109, "opus", 48000, 2, false),
        audio_conduit_(mozilla::AudioSessionConduit::Create(
            WebRtcCallWrapper::Create(), test_utils->sts_target())),
        audio_pipeline_(),
        transport_(new LoopbackTransport) {}

  static void Connect(TestAgent* client, TestAgent* server) {
    LoopbackTransport::InitAndConnect(*client->transport_, *server->transport_);
  }

  virtual void CreatePipeline(const std::string& aTransportId) = 0;

  void SetState(const std::string& aTransportId, TransportLayer::State aState,
                bool aRtcp) {
    mozilla::SyncRunnable::DispatchToThread(
        test_utils->sts_target(),
        WrapRunnable(transport_, &LoopbackTransport::SetState, aTransportId,
                     aState, aRtcp));
  }

  void UpdateTransport(const std::string& aTransportId,
                       nsAutoPtr<MediaPipelineFilter> aFilter) {
    mozilla::SyncRunnable::DispatchToThread(
        test_utils->sts_target(),
        WrapRunnable(audio_pipeline_, &MediaPipeline::UpdateTransport_s,
                     aTransportId, aFilter));
  }

  void Stop() {
    MOZ_MTLOG(ML_DEBUG, "Stopping");

    if (audio_pipeline_) audio_pipeline_->Stop();
  }

  void Shutdown_s() { transport_->Shutdown(); }

  void Shutdown() {
    if (audio_pipeline_) audio_pipeline_->Shutdown_m();
    if (audio_stream_track_) audio_stream_track_->Stop();

    mozilla::SyncRunnable::DispatchToThread(
        test_utils->sts_target(), WrapRunnable(this, &TestAgent::Shutdown_s));
  }

  uint32_t GetRemoteSSRC() {
    uint32_t res = 0;
    audio_conduit_->GetRemoteSSRC(&res);
    return res;
  }

  uint32_t GetLocalSSRC() {
    std::vector<uint32_t> res;
    res = audio_conduit_->GetLocalSSRCs();
    return res.empty() ? 0 : res[0];
  }

  int GetAudioRtpCountSent() { return audio_pipeline_->RtpPacketsSent(); }

  int GetAudioRtpCountReceived() {
    return audio_pipeline_->RtpPacketsReceived();
  }

  int GetAudioRtcpCountSent() { return audio_pipeline_->RtcpPacketsSent(); }

  int GetAudioRtcpCountReceived() {
    return audio_pipeline_->RtcpPacketsReceived();
  }

 protected:
  mozilla::AudioCodecConfig audio_config_;
  RefPtr<mozilla::MediaSessionConduit> audio_conduit_;
  RefPtr<FakeAudioStreamTrack> audio_stream_track_;
  // TODO(bcampen@mozilla.com): Right now this does not let us test RTCP in
  // both directions; only the sender's RTCP is sent, but the receiver should
  // be sending it too.
  RefPtr<mozilla::MediaPipeline> audio_pipeline_;
  RefPtr<LoopbackTransport> transport_;
};

class TestAgentSend : public TestAgent {
 public:
  TestAgentSend() {
    mozilla::MediaConduitErrorCode err =
        static_cast<mozilla::AudioSessionConduit*>(audio_conduit_.get())
            ->ConfigureSendMediaCodec(&audio_config_);
    EXPECT_EQ(mozilla::kMediaConduitNoError, err);

    audio_stream_track_ = new FakeAudioStreamTrack();
  }

  virtual void CreatePipeline(const std::string& aTransportId) {
    std::string test_pc;

    RefPtr<MediaPipelineTransmit> audio_pipeline =
        new mozilla::MediaPipelineTransmit(test_pc, transport_, nullptr,
                                           test_utils->sts_target(), false,
                                           audio_conduit_);

    audio_pipeline->SetTrack(audio_stream_track_.get());
    audio_pipeline->Start();

    audio_pipeline_ = audio_pipeline;

    audio_pipeline_->UpdateTransport_m(aTransportId,
                                       nsAutoPtr<MediaPipelineFilter>(nullptr));
  }
};

class TestAgentReceive : public TestAgent {
 public:
  TestAgentReceive() {
    std::vector<UniquePtr<mozilla::AudioCodecConfig>> codecs;
    codecs.emplace_back(new AudioCodecConfig(audio_config_));

    mozilla::MediaConduitErrorCode err =
        static_cast<mozilla::AudioSessionConduit*>(audio_conduit_.get())
            ->ConfigureRecvMediaCodecs(codecs);
    EXPECT_EQ(mozilla::kMediaConduitNoError, err);
  }

  virtual void CreatePipeline(const std::string& aTransportId) {
    std::string test_pc;

    audio_pipeline_ = new mozilla::MediaPipelineReceiveAudio(
        test_pc, transport_, nullptr, test_utils->sts_target(),
        static_cast<mozilla::AudioSessionConduit*>(audio_conduit_.get()),
        nullptr);

    audio_pipeline_->Start();

    audio_pipeline_->UpdateTransport_m(aTransportId, bundle_filter_);
  }

  void SetBundleFilter(nsAutoPtr<MediaPipelineFilter> filter) {
    bundle_filter_ = filter;
  }

  void UpdateTransport_s(const std::string& aTransportId,
                         nsAutoPtr<MediaPipelineFilter> filter) {
    audio_pipeline_->UpdateTransport_s(aTransportId, filter);
  }

 private:
  nsAutoPtr<MediaPipelineFilter> bundle_filter_;
};

class MediaPipelineTest : public ::testing::Test {
 public:
  ~MediaPipelineTest() {
    p1_.Shutdown();
    p2_.Shutdown();
  }

  static void SetUpTestCase() {
    test_utils = new MtransportTestUtils();
    NSS_NoDB_Init(nullptr);
    NSS_SetDomesticPolicy();
  }

  // Setup transport.
  void InitTransports() {
    mozilla::SyncRunnable::DispatchToThread(
        test_utils->sts_target(),
        WrapRunnableNM(&TestAgent::Connect, &p2_, &p1_));
  }

  // Verify RTP and RTCP
  void TestAudioSend(bool aIsRtcpMux,
                     nsAutoPtr<MediaPipelineFilter> initialFilter =
                         nsAutoPtr<MediaPipelineFilter>(nullptr),
                     nsAutoPtr<MediaPipelineFilter> refinedFilter =
                         nsAutoPtr<MediaPipelineFilter>(nullptr),
                     unsigned int ms_until_filter_update = 500,
                     unsigned int ms_of_traffic_after_answer = 10000) {
    bool bundle = !!(initialFilter);
    // We do not support testing bundle without rtcp mux, since that doesn't
    // make any sense.
    ASSERT_FALSE(!aIsRtcpMux && bundle);

    p2_.SetBundleFilter(initialFilter);

    // Setup transport flows
    InitTransports();

    std::string transportId = aIsRtcpMux ? "mux" : "non-mux";
    p1_.CreatePipeline(transportId);
    p2_.CreatePipeline(transportId);

    // Set state of transports to CONNECTING. MediaPipeline doesn't really care
    // about this transition, but we're trying to simluate what happens in a
    // real case.
    p1_.SetState(transportId, TransportLayer::TS_CONNECTING, false);
    p1_.SetState(transportId, TransportLayer::TS_CONNECTING, true);
    p2_.SetState(transportId, TransportLayer::TS_CONNECTING, false);
    p2_.SetState(transportId, TransportLayer::TS_CONNECTING, true);

    PR_Sleep(10);

    // Set state of transports to OPEN (ie; connected). This should result in
    // media flowing.
    p1_.SetState(transportId, TransportLayer::TS_OPEN, false);
    p1_.SetState(transportId, TransportLayer::TS_OPEN, true);
    p2_.SetState(transportId, TransportLayer::TS_OPEN, false);
    p2_.SetState(transportId, TransportLayer::TS_OPEN, true);

    if (bundle) {
      PR_Sleep(ms_until_filter_update);

      // Leaving refinedFilter not set implies we want to just update with
      // the other side's SSRC
      if (!refinedFilter) {
        refinedFilter = new MediaPipelineFilter;
        // Might not be safe, strictly speaking.
        refinedFilter->AddRemoteSSRC(p1_.GetLocalSSRC());
      }

      p2_.UpdateTransport(transportId, refinedFilter);
    }

    // wait for some RTP/RTCP tx and rx to happen
    PR_Sleep(ms_of_traffic_after_answer);

    p1_.Stop();
    p2_.Stop();

    // wait for any packets in flight to arrive
    PR_Sleep(100);

    p1_.Shutdown();
    p2_.Shutdown();

    if (!bundle) {
      // If we are filtering, allow the test-case to do this checking.
      ASSERT_GE(p1_.GetAudioRtpCountSent(), 40);
      ASSERT_EQ(p1_.GetAudioRtpCountReceived(), p2_.GetAudioRtpCountSent());
      ASSERT_EQ(p1_.GetAudioRtpCountSent(), p2_.GetAudioRtpCountReceived());
    }

    // No RTCP packets should have been dropped, because we do not filter them.
    // Calling ShutdownMedia_m on both pipelines does not stop the flow of
    // RTCP. So, we might be off by one here.
    ASSERT_LE(p2_.GetAudioRtcpCountReceived(), p1_.GetAudioRtcpCountSent());
    ASSERT_GE(p2_.GetAudioRtcpCountReceived() + 1, p1_.GetAudioRtcpCountSent());
  }

  void TestAudioReceiverBundle(
      bool bundle_accepted, nsAutoPtr<MediaPipelineFilter> initialFilter,
      nsAutoPtr<MediaPipelineFilter> refinedFilter =
          nsAutoPtr<MediaPipelineFilter>(nullptr),
      unsigned int ms_until_answer = 500,
      unsigned int ms_of_traffic_after_answer = 10000) {
    TestAudioSend(true, initialFilter, refinedFilter, ms_until_answer,
                  ms_of_traffic_after_answer);
  }

 protected:
  TestAgentSend p1_;
  TestAgentReceive p2_;
};

class MediaPipelineFilterTest : public ::testing::Test {
 public:
  bool Filter(MediaPipelineFilter& filter, int32_t correlator, uint32_t ssrc,
              uint8_t payload_type) {
    webrtc::RTPHeader header;
    header.ssrc = ssrc;
    header.payloadType = payload_type;
    return filter.Filter(header, correlator);
  }
};

TEST_F(MediaPipelineFilterTest, TestConstruct) { MediaPipelineFilter filter; }

TEST_F(MediaPipelineFilterTest, TestDefault) {
  MediaPipelineFilter filter;
  ASSERT_FALSE(Filter(filter, 0, 233, 110));
}

TEST_F(MediaPipelineFilterTest, TestSSRCFilter) {
  MediaPipelineFilter filter;
  filter.AddRemoteSSRC(555);
  ASSERT_TRUE(Filter(filter, 0, 555, 110));
  ASSERT_FALSE(Filter(filter, 0, 556, 110));
}

#define SSRC(ssrc)                                                    \
  ((ssrc >> 24) & 0xFF), ((ssrc >> 16) & 0xFF), ((ssrc >> 8) & 0xFF), \
      (ssrc & 0xFF)

#define REPORT_FRAGMENT(ssrc) \
  SSRC(ssrc), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0

#define RTCP_TYPEINFO(num_rrs, type, size) 0x80 + num_rrs, type, 0, size

TEST_F(MediaPipelineFilterTest, TestCorrelatorFilter) {
  MediaPipelineFilter filter;
  filter.SetCorrelator(7777);
  ASSERT_TRUE(Filter(filter, 7777, 16, 110));
  ASSERT_FALSE(Filter(filter, 7778, 17, 110));
  // This should also have resulted in the SSRC 16 being added to the filter
  ASSERT_TRUE(Filter(filter, 0, 16, 110));
  ASSERT_FALSE(Filter(filter, 0, 17, 110));
}

TEST_F(MediaPipelineFilterTest, TestPayloadTypeFilter) {
  MediaPipelineFilter filter;
  filter.AddUniquePT(110);
  ASSERT_TRUE(Filter(filter, 0, 555, 110));
  ASSERT_FALSE(Filter(filter, 0, 556, 111));
}

TEST_F(MediaPipelineFilterTest, TestSSRCMovedWithCorrelator) {
  MediaPipelineFilter filter;
  filter.SetCorrelator(7777);
  ASSERT_TRUE(Filter(filter, 7777, 555, 110));
  ASSERT_TRUE(Filter(filter, 0, 555, 110));
  ASSERT_FALSE(Filter(filter, 7778, 555, 110));
  ASSERT_FALSE(Filter(filter, 0, 555, 110));
}

TEST_F(MediaPipelineFilterTest, TestRemoteSDPNoSSRCs) {
  // If the remote SDP doesn't have SSRCs, right now this is a no-op and
  // there is no point of even incorporating a filter, but we make the
  // behavior consistent to avoid confusion.
  MediaPipelineFilter filter;
  filter.SetCorrelator(7777);
  filter.AddUniquePT(111);
  ASSERT_TRUE(Filter(filter, 7777, 555, 110));

  MediaPipelineFilter filter2;

  filter.Update(filter2);

  // Ensure that the old SSRC still works.
  ASSERT_TRUE(Filter(filter, 0, 555, 110));
}

TEST_F(MediaPipelineTest, TestAudioSendNoMux) { TestAudioSend(false); }

TEST_F(MediaPipelineTest, TestAudioSendMux) { TestAudioSend(true); }

TEST_F(MediaPipelineTest, TestAudioSendBundle) {
  nsAutoPtr<MediaPipelineFilter> filter(new MediaPipelineFilter);
  // These durations have to be _extremely_ long to have any assurance that
  // some RTCP will be sent at all. This is because the first RTCP packet
  // is sometimes sent before the transports are ready, which causes it to
  // be dropped.
  TestAudioReceiverBundle(
      true, filter,
      // We do not specify the filter for the remote description, so it will be
      // set to something sane after a short time.
      nsAutoPtr<MediaPipelineFilter>(), 10000, 10000);

  // Some packets should have been dropped, but not all
  ASSERT_GT(p1_.GetAudioRtpCountSent(), p2_.GetAudioRtpCountReceived());
  ASSERT_GT(p2_.GetAudioRtpCountReceived(), 40);
  ASSERT_GT(p1_.GetAudioRtcpCountSent(), 1);
}

TEST_F(MediaPipelineTest, TestAudioSendEmptyBundleFilter) {
  nsAutoPtr<MediaPipelineFilter> filter(new MediaPipelineFilter);
  nsAutoPtr<MediaPipelineFilter> bad_answer_filter(new MediaPipelineFilter);
  TestAudioReceiverBundle(true, filter, bad_answer_filter);
  // Filter is empty, so should drop everything.
  ASSERT_EQ(0, p2_.GetAudioRtpCountReceived());
}

}  // end namespace
