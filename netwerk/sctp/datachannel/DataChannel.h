/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NETWERK_SCTP_DATACHANNEL_DATACHANNEL_H_
#define NETWERK_SCTP_DATACHANNEL_DATACHANNEL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <errno.h>
#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WeakPtr.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsTArray.h"
#include "nsDeque.h"
#include "mozilla/dom/Blob.h"
#include "mozilla/Mutex.h"
#include "DataChannelProtocol.h"
#include "DataChannelListener.h"
#include "mozilla/net/NeckoTargetHolder.h"
#include "MediaEventSource.h"

#include "transport/transportlayer.h"  // For TransportLayer::State

namespace mozilla {

class DataChannelConnection;
class DataChannel;
class DataChannelOnMessageAvailable;
class MediaPacket;
class MediaTransportHandler;
namespace dom {
struct RTCStatsCollection;
};

enum class DataChannelState { Connecting, Open, Closing, Closed };
enum class DataChannelConnectionState { Connecting, Open, Closed };
enum class DataChannelReliabilityPolicy {
  Reliable,
  LimitedRetransmissions,
  LimitedLifetime
};

class DataChannelMessageMetadata {
 public:
  DataChannelMessageMetadata(uint16_t aStreamId, uint32_t aPpid,
                             bool aUnordered,
                             Maybe<uint16_t> aMaxRetransmissions = Nothing(),
                             Maybe<uint16_t> aMaxLifetimeMs = Nothing())
      : mStreamId(aStreamId),
        mPpid(aPpid),
        mUnordered(aUnordered),
        mMaxRetransmissions(aMaxRetransmissions),
        mMaxLifetimeMs(aMaxLifetimeMs) {}

  DataChannelMessageMetadata(const DataChannelMessageMetadata& aOrig) = default;
  DataChannelMessageMetadata(DataChannelMessageMetadata&& aOrig) = default;
  DataChannelMessageMetadata& operator=(
      const DataChannelMessageMetadata& aOrig) = default;
  DataChannelMessageMetadata& operator=(DataChannelMessageMetadata&& aOrig) =
      default;

  uint16_t mStreamId;
  uint32_t mPpid;
  bool mUnordered;
  Maybe<uint16_t> mMaxRetransmissions;
  Maybe<uint16_t> mMaxLifetimeMs;
};

class OutgoingMsg {
 public:
  OutgoingMsg(nsACString&& data, const DataChannelMessageMetadata& aMetadata);
  OutgoingMsg(OutgoingMsg&& aOrig) = default;
  OutgoingMsg& operator=(OutgoingMsg&& aOrig) = default;
  OutgoingMsg(const OutgoingMsg&) = delete;
  OutgoingMsg& operator=(const OutgoingMsg&) = delete;

  void Advance(size_t offset);
  const DataChannelMessageMetadata& GetMetadata() const { return mMetadata; };
  size_t GetLength() const { return mData.Length(); };
  Span<const uint8_t> GetRemainingData() const {
    auto span = Span<const uint8_t>(mData);
    return span.From(mPos);
  }

 protected:
  nsCString mData;
  DataChannelMessageMetadata mMetadata;
  size_t mPos = 0;
};

class IncomingMsg {
 public:
  explicit IncomingMsg(uint32_t aPpid, uint16_t aStreamId)
      : mPpid(aPpid), mStreamId(aStreamId) {}
  IncomingMsg(IncomingMsg&& aOrig) = default;
  IncomingMsg& operator=(IncomingMsg&& aOrig) = default;
  IncomingMsg(const IncomingMsg&) = delete;
  IncomingMsg& operator=(const IncomingMsg&) = delete;

  void Append(const uint8_t* aData, size_t aLen) {
    mData.Append((const char*)aData, aLen);
  }

  const nsCString& GetData() const { return mData; }
  nsCString& GetData() { return mData; }
  size_t GetLength() const { return mData.Length(); };
  uint16_t GetStreamId() const { return mStreamId; }
  uint32_t GetPpid() const { return mPpid; }

 protected:
  // TODO(bug 1949918): We've historically passed this around as a c-string, but
  // that's not really appropriate for binary messages.
  nsCString mData;
  uint32_t mPpid;
  uint16_t mStreamId;
};

// One per PeerConnection
class DataChannelConnection : public net::NeckoTargetHolder {
  friend class DataChannel;
  friend class DataChannelOnMessageAvailable;
  friend class DataChannelConnectRunnable;
  friend class DataChannelConnectionUsrsctp;

 protected:
  virtual ~DataChannelConnection();

 public:
  enum class PendingType {
    None,  // No outgoing messages are pending.
    Dcep,  // Outgoing DCEP messages are pending.
    Data,  // Outgoing data channel messages are pending.
  };

  class DataConnectionListener : public SupportsWeakPtr {
   public:
    virtual ~DataConnectionListener() = default;

    // Called when a new DataChannel has been opened by the other side.
    virtual void NotifyDataChannel(already_AddRefed<DataChannel> channel) = 0;

    // Called when a DataChannel transitions to state open
    virtual void NotifyDataChannelOpen(DataChannel* aChannel) = 0;

    // Called when a DataChannel (that was open at some point in the past)
    // transitions to state closed
    virtual void NotifyDataChannelClosed(DataChannel* aChannel) = 0;

    // Called when SCTP connects
    virtual void NotifySctpConnected() = 0;

    // Called when SCTP closes
    virtual void NotifySctpClosed() = 0;
  };

  // Create a new DataChannel Connection
  // Must be called on Main thread
  static Maybe<RefPtr<DataChannelConnection>> Create(
      DataConnectionListener* aListener, nsISerialEventTarget* aTarget,
      MediaTransportHandler* aHandler, const uint16_t aLocalPort,
      const uint16_t aNumStreams, const Maybe<uint64_t>& aMaxMessageSize);

  DataChannelConnection(const DataChannelConnection&) = delete;
  DataChannelConnection(DataChannelConnection&&) = delete;
  DataChannelConnection& operator=(const DataChannelConnection&) = delete;
  DataChannelConnection& operator=(DataChannelConnection&&) = delete;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DataChannelConnection)

  // Called immediately after construction
  virtual bool Init(const uint16_t aLocalPort, const uint16_t aNumStreams,
                    const Maybe<uint64_t>& aMaxMessageSize) = 0;
  // Called when our transport is ready to send and recv
  virtual void OnTransportReady() = 0;
  // This is called after an ACK comes in, to prompt subclasses to deliver
  // anything they've buffered while awaiting the ACK.
  virtual void OnStreamOpen(uint16_t stream) = 0;
  // Called when the base class wants to raise the stream limit
  virtual bool RaiseStreamLimitTo(uint16_t aNewLimit) = 0;
  // Called when the base class wants to send a message; it is expected that
  // this will eventually result in a call/s to SendSctpPacket once the SCTP
  // packet is ready to be sent to the transport.
  virtual int SendMessage(DataChannel& aChannel, OutgoingMsg&& aMsg) = 0;
  // Called when the base class receives a packet from the transport
  virtual void OnSctpPacketReceived(const MediaPacket& packet) = 0;
  // Called when the base class is closing streams
  virtual void ResetStreams(nsTArray<uint16_t>& aStreams) = 0;
  // Called when the SCTP connection is being shut down
  virtual void Destroy();

  void SetMaxMessageSize(bool aMaxMessageSizeSet, uint64_t aMaxMessageSize);
  uint64_t GetMaxMessageSize();
  void HandleDataMessage(IncomingMsg&& aMsg);
  void HandleDCEPMessage(IncomingMsg&& aMsg);
  void ProcessQueuedOpens();
  void OnStreamsReset(std::vector<uint16_t>&& aStreams);

  void AppendStatsToReport(const UniquePtr<dom::RTCStatsCollection>& aReport,
                           const DOMHighResTimeStamp aTimestamp) const;

  bool ConnectToTransport(const std::string& aTransportId, const bool aClient,
                          const uint16_t aLocalPort,
                          const uint16_t aRemotePort);
  void TransportStateChange(const std::string& aTransportId,
                            TransportLayer::State aState);
  void SetSignals(const std::string& aTransportId);

  [[nodiscard]] already_AddRefed<DataChannel> Open(
      const nsACString& label, const nsACString& protocol,
      DataChannelReliabilityPolicy prPolicy, bool inOrder, uint32_t prValue,
      bool aExternalNegotiated, uint16_t aStream);

  void Stop();
  void Close(DataChannel* aChannel);
  void GracefulClose(DataChannel* aChannel);
  void FinishClose(DataChannel* aChannel);
  void FinishClose_s(DataChannel* aChannel);
  void CloseAll();

  // Returns a POSIX error code.
  int SendMessage(uint16_t stream, nsACString&& aMsg) {
    return SendDataMessage(stream, std::move(aMsg), false);
  }

  // Returns a POSIX error code.
  int SendBinaryMessage(uint16_t stream, nsACString&& aMsg) {
    return SendDataMessage(stream, std::move(aMsg), true);
  }

  // Returns a POSIX error code.
  int SendBlob(uint16_t stream, nsIInputStream* aBlob);

  void ReadBlob(already_AddRefed<DataChannelConnection> aThis, uint16_t aStream,
                nsIInputStream* aBlob);

  bool InShutdown() const {
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
    return mShutdown;
#else
    return false;
#endif
  }

 protected:
  class Channels {
   public:
    using ChannelArray = AutoTArray<RefPtr<DataChannel>, 16>;

    Channels() : mMutex("DataChannelConnection::Channels::mMutex") {}
    Channels(const Channels&) = delete;
    Channels(Channels&&) = delete;
    Channels& operator=(const Channels&) = delete;
    Channels& operator=(Channels&&) = delete;

    void Insert(const RefPtr<DataChannel>& aChannel);
    bool Remove(const RefPtr<DataChannel>& aChannel);
    RefPtr<DataChannel> Get(uint16_t aId) const;
    ChannelArray GetAll() const {
      MutexAutoLock lock(mMutex);
      return mChannels.Clone();
    }
    RefPtr<DataChannel> GetNextChannel(uint16_t aCurrentId) const;

   private:
    struct IdComparator {
      bool Equals(const RefPtr<DataChannel>& aChannel, uint16_t aId) const;
      bool LessThan(const RefPtr<DataChannel>& aChannel, uint16_t aId) const;
      bool Equals(const RefPtr<DataChannel>& a1,
                  const RefPtr<DataChannel>& a2) const;
      bool LessThan(const RefPtr<DataChannel>& a1,
                    const RefPtr<DataChannel>& a2) const;
    };
    mutable Mutex mMutex;
    ChannelArray mChannels MOZ_GUARDED_BY(mMutex);
  };

  DataChannelConnection(DataConnectionListener* aListener,
                        nsISerialEventTarget* aTarget,
                        MediaTransportHandler* aHandler);

  int SendDataMessage(uint16_t aStream, nsACString&& aMsg, bool aIsBinary);

  DataChannelConnectionState GetState() const {
    MOZ_ASSERT(mSTS->IsOnCurrentThread());
    return mState;
  }

  void SetState(DataChannelConnectionState aState);

  static void DTLSConnectThread(void* data);
  void SendPacket(std::unique_ptr<MediaPacket>&& packet);
  void OnPacketReceived(const std::string& aTransportId,
                        const MediaPacket& packet);
  already_AddRefed<DataChannel> FindChannelByStream(uint16_t stream);
  uint16_t FindFreeStream() const;
  int SendControlMessage(DataChannel& aChannel, const uint8_t* data,
                         uint32_t len);
  int SendOpenAckMessage(DataChannel& aChannel);
  int SendOpenRequestMessage(DataChannel& aChannel);

  void OpenFinish(RefPtr<DataChannel> aChannel);

  void ClearResets();
  void MarkStreamForReset(DataChannel& aChannel);
  void HandleUnknownMessage(uint32_t ppid, uint32_t length, uint16_t stream);
  void HandleOpenRequestMessage(
      const struct rtcweb_datachannel_open_request* req, uint32_t length,
      uint16_t stream);
  void HandleOpenAckMessage(const struct rtcweb_datachannel_ack* ack,
                            uint32_t length, uint16_t stream);
  bool ReassembleMessageChunk(IncomingMsg& aReassembled, const void* buffer,
                              size_t length, uint32_t ppid, uint16_t stream);

  /******************** Mainthread only **********************/
  // Avoid cycles with PeerConnectionImpl
  // Use from main thread only as WeakPtr is not threadsafe
  WeakPtr<DataConnectionListener> mListener;
  bool mMaxMessageSizeSet = false;
  uint64_t mMaxMessageSize = 0;
  nsTArray<uint16_t> mStreamIds;
  Maybe<bool> mAllocateEven;
  nsCOMPtr<nsIThread> mInternalIOThread = nullptr;
  /***********************************************************/

  /*********************** STS only **************************/
  bool mSendInterleaved = false;
  uint32_t mCurrentStream = 0;
  std::set<RefPtr<DataChannel>> mPending;
  uint16_t mNegotiatedIdLimit = 0;
  PendingType mPendingType = PendingType::None;
  // holds outgoing control messages
  nsTArray<OutgoingMsg> mBufferedControl;
  // For partial DCEP messages (should be _really_ rare, since they're small)
  Maybe<IncomingMsg> mRecvBuffer;
  bool mSctpConfigured = false;
  std::string mTransportId;
  bool mConnectedToTransportHandler = false;
  RefPtr<MediaTransportHandler> mTransportHandler;
  MediaEventListener mPacketReceivedListener;
  MediaEventListener mStateChangeListener;
  // Streams pending reset.
  AutoTArray<uint16_t, 4> mStreamsResetting;
  DataChannelConnectionState mState = DataChannelConnectionState::Closed;
  /***********************************************************/

  // NOTE: while this container will auto-expand, increases in the number of
  // channels available from the stack must be negotiated!
  // Accessed from both main and sts, API is threadsafe
  Channels mChannels;

  // Set once on main in Init, invariant thereafter
  uintptr_t mId = 0;

  // Set once on main in ConnectToTransport, and read only (STS) thereafter.
  // Nothing should be using these before that first ConnectToTransport call.
  uint16_t mLocalPort = 0;
  uint16_t mRemotePort = 0;

  nsCOMPtr<nsISerialEventTarget> mSTS;

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  bool mShutdown = false;
#endif
};

class DataChannel {
  friend class DataChannelOnMessageAvailable;
  friend class DataChannelConnection;
  friend class DataChannelConnectionUsrsctp;

 public:
  struct TrafficCounters {
    uint32_t mMessagesSent = 0;
    uint64_t mBytesSent = 0;
    uint32_t mMessagesReceived = 0;
    uint64_t mBytesReceived = 0;
  };

  DataChannel(DataChannelConnection* connection, uint16_t stream,
              DataChannelState state, const nsACString& label,
              const nsACString& protocol, DataChannelReliabilityPolicy policy,
              uint32_t value, bool ordered, bool negotiated);
  DataChannel(const DataChannel&) = delete;
  DataChannel(DataChannel&&) = delete;
  DataChannel& operator=(const DataChannel&) = delete;
  DataChannel& operator=(DataChannel&&) = delete;

 private:
  ~DataChannel();

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DataChannel)

  // Complete dropping of the link between DataChannel and the connection.
  // After this, except for a few methods below listed to be safe, you can't
  // call into DataChannel.
  void ReleaseConnection();

  // Close this DataChannel.  Can be called multiple times.  MUST be called
  // before destroying the DataChannel (state must be CLOSED or CLOSING).
  void Close();

  // Set the listener (especially for channels created from the other side)
  void SetListener(DataChannelListener* aListener, nsISupports* aContext);

  // Helper for send methods that converts POSIX error codes to an ErrorResult.
  static void SendErrnoToErrorResult(int error, size_t aMessageSize,
                                     ErrorResult& aRv);

  // Send a string
  void SendMsg(nsACString&& aMsg, ErrorResult& aRv);

  // Send a binary message (TypedArray)
  void SendBinaryMsg(nsACString&& aMsg, ErrorResult& aRv);

  // Send a binary blob
  void SendBinaryBlob(dom::Blob& aBlob, ErrorResult& aRv);

  DataChannelReliabilityPolicy GetType() const { return mPrPolicy; }

  dom::Nullable<uint16_t> GetMaxPacketLifeTime() const;

  dom::Nullable<uint16_t> GetMaxRetransmits() const;

  bool GetNegotiated() const { return mNegotiated; }

  bool GetOrdered() const { return mOrdered; }

  void IncrementBufferedAmount(uint32_t aSize, ErrorResult& aRv);
  void DecrementBufferedAmount(uint32_t aSize);

  // Amount of data buffered to send
  uint32_t GetBufferedAmount() const {
    MOZ_ASSERT(NS_IsMainThread());
    return mBufferedAmount;
  }

  // Trigger amount for generating BufferedAmountLow events
  uint32_t GetBufferedAmountLowThreshold() const;
  void SetBufferedAmountLowThreshold(uint32_t aThreshold);

  void AnnounceOpen();
  void AnnounceClosed();

  // Find out state
  DataChannelState GetReadyState() const {
    MOZ_ASSERT(NS_IsMainThread());
    return mReadyState;
  }

  // Set ready state
  void SetReadyState(DataChannelState aState);

  void GetLabel(nsAString& aLabel) { CopyUTF8toUTF16(mLabel, aLabel); }
  void GetProtocol(nsAString& aProtocol) {
    CopyUTF8toUTF16(mProtocol, aProtocol);
  }
  uint16_t GetStream() const {
    MOZ_ASSERT(NS_IsMainThread());
    return mStream;
  }

  void SendOrQueue(DataChannelOnMessageAvailable* aMessage);

  TrafficCounters GetTrafficCounters() const;

 private:
  nsresult AddDataToBinaryMsg(const char* data, uint32_t size);
  bool EnsureValidStream(ErrorResult& aRv);
  void WithTrafficCounters(const std::function<void(TrafficCounters&)>&);

  // Mainthread only
  DataChannelListener* mListener = nullptr;
  nsCOMPtr<nsISupports> mContext;
  bool mEverOpened = false;
  const nsCString mLabel;
  const nsCString mProtocol;
  DataChannelState mReadyState;
  uint16_t mStream;
  const DataChannelReliabilityPolicy mPrPolicy;
  const uint32_t mPrValue;
  size_t mBufferedThreshold;
  size_t mBufferedAmount;
  RefPtr<DataChannelConnection> mConnection;
  TrafficCounters mTrafficCounters;

  // STS only
  // The channel has been opened, but the peer has not yet acked - ensures that
  // the messages are sent ordered until this is cleared.
  bool mWaitingForAck = false;
  nsTArray<OutgoingMsg> mBufferedData;
  std::map<uint16_t, IncomingMsg> mRecvBuffers;

  // Accessed on main and STS
  const bool mNegotiated;
  const bool mOrdered;

  nsCOMPtr<nsISerialEventTarget> mMainThreadEventTarget;
};

// used to dispatch notifications of incoming data to the main thread
// Patterned on CallOnMessageAvailable in WebSockets
// Also used to proxy other items to MainThread
class DataChannelOnMessageAvailable : public Runnable {
 public:
  enum class EventType {
    OnConnection,
    OnDisconnected,
    OnDataString,
    OnDataBinary,
  };

  DataChannelOnMessageAvailable(EventType aType,
                                DataChannelConnection* aConnection,
                                DataChannel* aChannel, nsCString&& aData)
      : Runnable("DataChannelOnMessageAvailable"),
        mType(aType),
        mChannel(aChannel),
        mConnection(aConnection),
        mData(std::move(aData)) {}

  DataChannelOnMessageAvailable(EventType aType, DataChannel* aChannel)
      : Runnable("DataChannelOnMessageAvailable"),
        mType(aType),
        mChannel(aChannel) {}
  // XXX is it safe to leave mData uninitialized?  This should only be
  // used for notifications that don't use them, but I'd like more
  // bulletproof compile-time checking.

  DataChannelOnMessageAvailable(EventType aType,
                                DataChannelConnection* aConnection,
                                DataChannel* aChannel)
      : Runnable("DataChannelOnMessageAvailable"),
        mType(aType),
        mChannel(aChannel),
        mConnection(aConnection) {}

  // for ON_CONNECTION/ON_DISCONNECTED
  DataChannelOnMessageAvailable(EventType aType,
                                DataChannelConnection* aConnection)
      : Runnable("DataChannelOnMessageAvailable"),
        mType(aType),
        mConnection(aConnection) {}
  DataChannelOnMessageAvailable(const DataChannelOnMessageAvailable&) = delete;
  DataChannelOnMessageAvailable(DataChannelOnMessageAvailable&&) = delete;
  DataChannelOnMessageAvailable& operator=(
      const DataChannelOnMessageAvailable&) = delete;
  DataChannelOnMessageAvailable& operator=(DataChannelOnMessageAvailable&&) =
      delete;

  NS_IMETHOD Run() override;

 private:
  ~DataChannelOnMessageAvailable() = default;

  EventType mType;
  // XXX should use union
  RefPtr<DataChannel> mChannel;
  RefPtr<DataChannelConnection> mConnection;
  nsCString mData;
};

static constexpr const char* ToString(DataChannelConnectionState state) {
  switch (state) {
    case DataChannelConnectionState::Connecting:
      return "CONNECTING";
    case DataChannelConnectionState::Open:
      return "OPEN";
    case DataChannelConnectionState::Closed:
      return "CLOSED";
  }
  return "";
};

static constexpr const char* ToString(DataChannelConnection::PendingType type) {
  switch (type) {
    case DataChannelConnection::PendingType::None:
      return "NONE";
    case DataChannelConnection::PendingType::Dcep:
      return "DCEP";
    case DataChannelConnection::PendingType::Data:
      return "DATA";
  }
  return "";
};

static constexpr const char* ToString(DataChannelReliabilityPolicy type) {
  switch (type) {
    case DataChannelReliabilityPolicy::Reliable:
      return "RELIABLE";
    case DataChannelReliabilityPolicy::LimitedRetransmissions:
      return "LIMITED_RETRANSMISSIONS";
    case DataChannelReliabilityPolicy::LimitedLifetime:
      return "LIMITED_LIFETIME";
  }
  return "";
};

}  // namespace mozilla

#endif  // NETWERK_SCTP_DATACHANNEL_DATACHANNEL_H_
