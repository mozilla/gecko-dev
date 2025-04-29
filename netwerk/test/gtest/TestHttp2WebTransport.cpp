/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestCommon.h"
#include "gtest/gtest.h"
#include "Http2WebTransportSession.h"
#include "Http2WebTransportStream.h"
#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/Queue.h"
#include "mozilla/net/NeqoHttp3Conn.h"
#include "Capsule.h"
#include "CapsuleEncoder.h"
#include "CapsuleParser.h"
#include "nsIWebTransport.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"

using namespace mozilla;
using namespace mozilla::net;

class MockWebTransportClient : public CapsuleIOHandler {
 public:
  NS_INLINE_DECL_REFCOUNTING(MockWebTransportClient, override)

  MockWebTransportClient(Http2WebTransportInitialSettings aSettings)
      : mSession(new Http2WebTransportSessionImpl(this, aSettings)),
        mParser(MakeUnique<CapsuleParser>(mSession)) {}

  Http2WebTransportSessionImpl* Session() { return mSession; }

  void HasCapsuleToSend() override {
    mSession->PrepareCapsulesToSend(mOutCapsules);
  }

  void SetSentFin() override { mSetSentFinCalled = true; }

  void StartReading() override { mStartReadingCalled = true; }

  void OnCapsuleParseFailure(nsresult aError) override {
    mOnParseFailureCalled = true;
  }

  void ProcessInputCapsules(
      mozilla::Queue<UniquePtr<CapsuleEncoder>>&& aCapsules) {
    while (!aCapsules.IsEmpty()) {
      UniquePtr<CapsuleEncoder> capsule = aCapsules.Pop();
      auto buffer = capsule->GetBuffer();
      mParser->ProcessCapsuleData(buffer.Elements(), buffer.Length());
    }
  }

  void ProcessOutput() { mSession->PrepareCapsulesToSend(mOutCapsules); }

  mozilla::Queue<UniquePtr<CapsuleEncoder>> GetOutCapsules() {
    return std::move(mOutCapsules);
  }

  void Done() {
    mParser = nullptr;
    mSession->Close(NS_OK);
    mSession = nullptr;
  }

  // State tracking
  bool mSetSentFinCalled = false;
  bool mStartReadingCalled = false;
  bool mOnParseFailureCalled = false;

 private:
  ~MockWebTransportClient() = default;

  RefPtr<Http2WebTransportSessionImpl> mSession;
  UniquePtr<CapsuleParser> mParser;
  mozilla::Queue<UniquePtr<CapsuleEncoder>> mOutCapsules;
};

class MockWebTransportServer : public CapsuleParser::Listener {
 public:
  NS_INLINE_DECL_REFCOUNTING(MockWebTransportServer, override)

  explicit MockWebTransportServer()
      : mParser(MakeUnique<CapsuleParser>(this)) {}

  bool OnCapsule(Capsule&& aCapsule) override {
    mReceivedCapsules.AppendElement(std::move(aCapsule));
    return true;
  }
  void OnCapsuleParseFailure(nsresult aError) override {
    MOZ_RELEASE_ASSERT(false);
  }

  nsTArray<Capsule> GetReceivedCapsules() {
    return std::move(mReceivedCapsules);
  }

  void ProcessInputCapsules(
      mozilla::Queue<UniquePtr<CapsuleEncoder>>&& aCapsules) {
    while (!aCapsules.IsEmpty()) {
      UniquePtr<CapsuleEncoder> capsule = aCapsules.Pop();
      auto buffer = capsule->GetBuffer();
      mParser->ProcessCapsuleData(buffer.Elements(), buffer.Length());
    }
  }

  void SendWebTransportMaxStreamsCapsule(uint64_t aLimit, bool aBidi) {
    Capsule capsule = Capsule::WebTransportMaxStreams(aLimit, aBidi);
    UniquePtr<CapsuleEncoder> encoder = MakeUnique<CapsuleEncoder>();
    encoder->EncodeCapsule(capsule);
    mOutCapsules.Push(std::move(encoder));
  }

  void SendWebTransportStreamDataCapsule(uint64_t aID, bool aFin,
                                         nsTArray<uint8_t>&& aData) {
    Capsule capsule =
        Capsule::WebTransportStreamData(aID, aFin, std::move(aData));
    UniquePtr<CapsuleEncoder> encoder = MakeUnique<CapsuleEncoder>();
    encoder->EncodeCapsule(capsule);
    mOutCapsules.Push(std::move(encoder));
  }

  mozilla::Queue<UniquePtr<CapsuleEncoder>> GetOutCapsules() {
    return std::move(mOutCapsules);
  }

  void Done() { mParser = nullptr; }

 private:
  ~MockWebTransportServer() = default;

  UniquePtr<CapsuleParser> mParser;
  nsTArray<Capsule> mReceivedCapsules;
  mozilla::Queue<UniquePtr<CapsuleEncoder>> mOutCapsules;
};

// TODO: will be used when testing incoming streams.
class MockWebTransportSessionEventListener
    : public WebTransportSessionEventListener,
      public WebTransportSessionEventListenerInternal {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_WEBTRANSPORTSESSIONEVENTLISTENER
  NS_DECL_WEBTRANSPORTSESSIONEVENTLISTENERINTERNAL

  MockWebTransportSessionEventListener() {}

 private:
  virtual ~MockWebTransportSessionEventListener() = default;
};

NS_IMPL_ISUPPORTS(MockWebTransportSessionEventListener,
                  WebTransportSessionEventListener,
                  WebTransportSessionEventListenerInternal)

NS_IMETHODIMP
MockWebTransportSessionEventListener::OnSessionReadyInternal(
    WebTransportSessionBase* aSession) {
  return NS_OK;
}

NS_IMETHODIMP
MockWebTransportSessionEventListener::OnIncomingStreamAvailableInternal(
    WebTransportStreamBase* aStream) {
  return NS_OK;
}

NS_IMETHODIMP
MockWebTransportSessionEventListener::OnIncomingBidirectionalStreamAvailable(
    nsIWebTransportBidirectionalStream* aStream) {
  return NS_OK;
}

NS_IMETHODIMP
MockWebTransportSessionEventListener::OnIncomingUnidirectionalStreamAvailable(
    nsIWebTransportReceiveStream* aStream) {
  return NS_OK;
}

NS_IMETHODIMP
MockWebTransportSessionEventListener::OnSessionReady(uint64_t ready) {
  return NS_OK;
}

NS_IMETHODIMP
MockWebTransportSessionEventListener::OnSessionClosed(
    bool aCleanly, uint32_t aStatus, const nsACString& aReason) {
  return NS_OK;
}

NS_IMETHODIMP MockWebTransportSessionEventListener::OnDatagramReceivedInternal(
    nsTArray<uint8_t>&& aData) {
  return NS_OK;
}

NS_IMETHODIMP MockWebTransportSessionEventListener::OnDatagramReceived(
    const nsTArray<uint8_t>& aData) {
  return NS_OK;
}

NS_IMETHODIMP MockWebTransportSessionEventListener::OnMaxDatagramSize(
    uint64_t aSize) {
  return NS_OK;
}

NS_IMETHODIMP
MockWebTransportSessionEventListener::OnOutgoingDatagramOutCome(
    uint64_t aId, WebTransportSessionEventListener::DatagramOutcome aOutCome) {
  return NS_OK;
}

NS_IMETHODIMP MockWebTransportSessionEventListener::OnStopSending(
    uint64_t aStreamId, nsresult aError) {
  return NS_OK;
}

NS_IMETHODIMP MockWebTransportSessionEventListener::OnResetReceived(
    uint64_t aStreamId, nsresult aError) {
  return NS_OK;
}

static void ServerProcessCapsules(MockWebTransportServer* aServer,
                                  MockWebTransportClient* aClient) {
  aClient->ProcessOutput();
  mozilla::Queue<UniquePtr<CapsuleEncoder>> outCapsules =
      aClient->GetOutCapsules();
  aServer->ProcessInputCapsules(std::move(outCapsules));
}

static void ClientProcessCapsules(MockWebTransportServer* aServer,
                                  MockWebTransportClient* aClient) {
  mozilla::Queue<UniquePtr<CapsuleEncoder>> outCapsules =
      aServer->GetOutCapsules();
  aClient->ProcessInputCapsules(std::move(outCapsules));
}

TEST(TestHttp2WebTransport, CloseSessionCapsule)
{
  RefPtr<MockWebTransportClient> client =
      new MockWebTransportClient(Http2WebTransportInitialSettings());
  RefPtr<MockWebTransportServer> server = new MockWebTransportServer();

  nsCString reason("test");
  client->Session()->CloseSession(42, "test"_ns);

  ServerProcessCapsules(server, client);

  nsTArray<Capsule> received = server->GetReceivedCapsules();
  ASSERT_EQ(received.Length(), 1u);

  CloseWebTransportSessionCapsule& parsedCapsule =
      received[0].GetCloseWebTransportSessionCapsule();
  ASSERT_EQ(parsedCapsule.mStatus, 42u);
  ASSERT_EQ(parsedCapsule.mReason, reason);

  client->Done();
  server->Done();
}

TEST(TestHttp2WebTransport, CreateOutgoingStream)
{
  RefPtr<MockWebTransportClient> client =
      new MockWebTransportClient(Http2WebTransportInitialSettings());
  RefPtr<MockWebTransportServer> server = new MockWebTransportServer();

  RefPtr<WebTransportStreamBase> bidiStream;
  auto callback =
      [&](Result<RefPtr<WebTransportStreamBase>, nsresult>&& aResult) {
        if (aResult.isErr()) {
          return;
        }
        bidiStream = aResult.unwrap();
      };
  client->Session()->CreateOutgoingBidirectionalStream(std::move(callback));
  ASSERT_TRUE(bidiStream == nullptr);

  ServerProcessCapsules(server, client);

  // Creating a stream is blocked, we should see a
  // WebTransportStreamsBlockedCapsule from the client.
  nsTArray<Capsule> received = server->GetReceivedCapsules();
  ASSERT_EQ(received.Length(), 1u);

  WebTransportStreamsBlockedCapsule& streamsBlocked =
      received[0].GetWebTransportStreamsBlockedCapsule();
  ASSERT_EQ(streamsBlocked.mLimit, 0u);
  ASSERT_EQ(streamsBlocked.mBidi, true);

  server->SendWebTransportMaxStreamsCapsule(1, true);
  ClientProcessCapsules(server, client);
  ASSERT_TRUE(bidiStream != nullptr);

  RefPtr<WebTransportStreamBase> unidiStream;
  auto callback1 =
      [&](Result<RefPtr<WebTransportStreamBase>, nsresult>&& aResult) {
        if (aResult.isErr()) {
          return;
        }
        unidiStream = aResult.unwrap();
      };
  client->Session()->CreateOutgoingUnidirectionalStream(std::move(callback1));
  ASSERT_TRUE(unidiStream == nullptr);

  ServerProcessCapsules(server, client);

  received = server->GetReceivedCapsules();
  ASSERT_EQ(received.Length(), 1u);

  WebTransportStreamsBlockedCapsule& streamsBlocked1 =
      received[0].GetWebTransportStreamsBlockedCapsule();
  ASSERT_EQ(streamsBlocked1.mLimit, 0u);
  ASSERT_EQ(streamsBlocked1.mBidi, false);

  server->SendWebTransportMaxStreamsCapsule(1, false);
  ClientProcessCapsules(server, client);
  ASSERT_TRUE(unidiStream != nullptr);

  client->Done();
  server->Done();
}

static void CreateTestData(uint32_t aNumBytes, nsTArray<uint8_t>& aDataOut) {
  static constexpr const char kSampleText[] =
      "{\"type\":\"message\",\"id\":42,\"payload\":\"The quick brown fox jumps "
      "over the lazy dog.\"}";
  static constexpr uint32_t kSampleTextLen = sizeof(kSampleText) - 1;

  aDataOut.SetCapacity(aNumBytes);

  while (aNumBytes > 0) {
    uint32_t chunkSize = std::min(kSampleTextLen, aNumBytes);
    aDataOut.AppendElements(reinterpret_cast<const uint8_t*>(kSampleText),
                            chunkSize);
    aNumBytes -= chunkSize;
  }
}

static void ValidateData(nsTArray<uint8_t>& aInput,
                         nsTArray<uint8_t>& aExpectedData) {
  ASSERT_EQ(aExpectedData.Length(), aInput.Length());
  for (size_t i = 0; i < aExpectedData.Length(); i++) {
    ASSERT_EQ(aExpectedData[i], aInput[i]);
  }
}

static void ValidateData(nsIInputStream* aStream,
                         nsTArray<uint8_t>& aExpectedData) {
  nsTArray<uint8_t> outputData;
  nsresult rv = NS_ConsumeStream(aStream, UINT32_MAX, outputData);
  ASSERT_NS_SUCCEEDED(rv);
  ValidateData(outputData, aExpectedData);
}

TEST(TestHttp2WebTransport, OutgoingUniStream)
{
  Http2WebTransportInitialSettings settings;
  settings.mInitialMaxStreamsUni = 1;
  RefPtr<MockWebTransportClient> client = new MockWebTransportClient(settings);
  RefPtr<MockWebTransportServer> server = new MockWebTransportServer();

  RefPtr<WebTransportStreamBase> bidiStream;
  auto callback =
      [&](Result<RefPtr<WebTransportStreamBase>, nsresult>&& aResult) {
        if (aResult.isErr()) {
          return;
        }
        bidiStream = aResult.unwrap();
      };
  client->Session()->CreateOutgoingBidirectionalStream(std::move(callback));
  ASSERT_TRUE(bidiStream == nullptr);

  ServerProcessCapsules(server, client);

  nsTArray<Capsule> received = server->GetReceivedCapsules();
  ASSERT_EQ(received.Length(), 1u);

  WebTransportStreamsBlockedCapsule& streamsBlocked =
      received[0].GetWebTransportStreamsBlockedCapsule();
  ASSERT_EQ(streamsBlocked.mLimit, 0u);
  ASSERT_EQ(streamsBlocked.mBidi, true);

  RefPtr<WebTransportStreamBase> unidiStream;
  auto callback1 =
      [&](Result<RefPtr<WebTransportStreamBase>, nsresult>&& aResult) {
        if (aResult.isErr()) {
          return;
        }
        unidiStream = aResult.unwrap();
      };
  client->Session()->CreateOutgoingUnidirectionalStream(std::move(callback1));
  ASSERT_TRUE(unidiStream != nullptr);

  nsCOMPtr<nsIAsyncOutputStream> writer;
  nsCOMPtr<nsIAsyncInputStream> reader;
  unidiStream->GetWriterAndReader(getter_AddRefs(writer),
                                  getter_AddRefs(reader));

  nsTArray<uint8_t> inputData;
  CreateTestData(512, inputData);
  uint32_t numWritten = 0;
  Unused << writer->Write((const char*)inputData.Elements(), inputData.Length(),
                          &numWritten);

  NS_ProcessPendingEvents(nullptr);

  ServerProcessCapsules(server, client);

  received = server->GetReceivedCapsules();
  ASSERT_EQ(received.Length(), 1u);

  WebTransportStreamDataCapsule& streamData =
      received[0].GetWebTransportStreamDataCapsule();

  StreamId id(streamData.mID);
  ASSERT_TRUE(id.IsClientInitiated());
  ASSERT_TRUE(id.IsUni());
  ValidateData(streamData.mData, inputData);

  client->Done();
  server->Done();
}

TEST(TestHttp2WebTransport, OutgoingBidiStream)
{
  Http2WebTransportInitialSettings settings;
  settings.mInitialMaxStreamsUni = 1;
  settings.mInitialMaxStreamsBidi = 1;
  RefPtr<MockWebTransportClient> client = new MockWebTransportClient(settings);
  RefPtr<MockWebTransportServer> server = new MockWebTransportServer();

  RefPtr<WebTransportStreamBase> bidiStream;
  auto callback =
      [&](Result<RefPtr<WebTransportStreamBase>, nsresult>&& aResult) {
        if (aResult.isErr()) {
          return;
        }
        bidiStream = aResult.unwrap();
      };
  client->Session()->CreateOutgoingBidirectionalStream(std::move(callback));
  ASSERT_TRUE(bidiStream != nullptr);

  nsCOMPtr<nsIAsyncOutputStream> writer;
  nsCOMPtr<nsIAsyncInputStream> reader;
  bidiStream->GetWriterAndReader(getter_AddRefs(writer),
                                 getter_AddRefs(reader));

  nsTArray<uint8_t> inputData;
  CreateTestData(512, inputData);
  uint32_t numWritten = 0;
  Unused << writer->Write((const char*)inputData.Elements(), inputData.Length(),
                          &numWritten);

  NS_ProcessPendingEvents(nullptr);

  ServerProcessCapsules(server, client);

  nsTArray<Capsule> received = server->GetReceivedCapsules();
  ASSERT_EQ(received.Length(), 1u);

  WebTransportStreamDataCapsule& streamData =
      received[0].GetWebTransportStreamDataCapsule();

  StreamId id(streamData.mID);
  ASSERT_TRUE(id.IsClientInitiated());
  ASSERT_TRUE(id.IsBiDi());
  ValidateData(streamData.mData, inputData);

  nsTArray<uint8_t> echo;
  echo.AppendElements(inputData.Elements(), inputData.Length());

  server->SendWebTransportStreamDataCapsule(id, false, std::move(echo));
  ClientProcessCapsules(server, client);

  uint64_t available = 0;
  Unused << reader->Available(&available);
  EXPECT_EQ(available, inputData.Length());

  ValidateData(reader, inputData);

  client->Done();
  server->Done();
}
