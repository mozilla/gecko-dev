/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <stdio.h>
#include <stdlib.h>

#ifdef XP_WIN
#  include <winsock.h>  // for htonl, htons, ntohl, ntohs
#endif

#include "nsIInputStream.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "mozilla/Sprintf.h"
#include "nsProxyRelease.h"
#include "nsThread.h"
#include "nsThreadUtils.h"
#include "nsNetUtil.h"
#include "mozilla/Components.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/RTCDataChannelBinding.h"
#include "mozilla/dom/RTCStatsReportBinding.h"
#ifdef MOZ_PEERCONNECTION
#  include "transport/runnable_utils.h"
#  include "jsapi/MediaTransportHandler.h"
#  include "mediapacket.h"
#endif

#include "DataChannel.h"
#include "DataChannelDcSctp.h"
#include "DataChannelUsrsctp.h"
#include "DataChannelLog.h"
#include "DataChannelProtocol.h"

namespace mozilla {

LazyLogModule gDataChannelLog("DataChannel");

static constexpr const char* ToString(DataChannelState state) {
  switch (state) {
    case DataChannelState::Connecting:
      return "CONNECTING";
    case DataChannelState::Open:
      return "OPEN";
    case DataChannelState::Closing:
      return "CLOSING";
    case DataChannelState::Closed:
      return "CLOSED";
  }
  return "";
};

static constexpr const char* ToString(
    DataChannelOnMessageAvailable::EventType type) {
  switch (type) {
    case DataChannelOnMessageAvailable::EventType::OnConnection:
      return "ON_CONNECTION";
    case DataChannelOnMessageAvailable::EventType::OnDisconnected:
      return "ON_DISCONNECTED";
    case DataChannelOnMessageAvailable::EventType::OnDataString:
      return "ON_DATA_STRING";
    case DataChannelOnMessageAvailable::EventType::OnDataBinary:
      return "ON_DATA_BINARY";
  }
  return "";
};

OutgoingMsg::OutgoingMsg(nsACString&& aData,
                         const DataChannelMessageMetadata& aMetadata)
    : mData(std::move(aData)), mMetadata(aMetadata) {}

void OutgoingMsg::Advance(size_t offset) {
  mPos += offset;
  if (mPos > mData.Length()) {
    mPos = mData.Length();
  }
}

DataChannelConnection::~DataChannelConnection() {
  DC_DEBUG(("Deleting DataChannelConnection %p", (void*)this));
  // This may die on the MainThread, or on the STS thread, or on an
  // sctp thread if we were in a callback when the DOM side shut things down.
  MOZ_ASSERT(mState == DataChannelConnectionState::Closed);
  MOZ_ASSERT(mPending.empty());

  if (!mSTS->IsOnCurrentThread()) {
    // We may be on MainThread *or* on an sctp thread (being called from
    // receive_cb() or SendSctpPacket())
    if (mInternalIOThread) {
      // Avoid spinning the event thread from here (which if we're mainthread
      // is in the event loop already)
      nsCOMPtr<nsIRunnable> r = WrapRunnable(
          nsCOMPtr<nsIThread>(mInternalIOThread), &nsIThread::AsyncShutdown);
      Dispatch(r.forget());
    }
  } else {
    // on STS, safe to call shutdown
    if (mInternalIOThread) {
      mInternalIOThread->Shutdown();
    }
  }
}

void DataChannelConnection::Destroy() {
  MOZ_ASSERT(NS_IsMainThread());
  DC_DEBUG(("Destroying DataChannelConnection %p", (void*)this));
  CloseAll();
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  MOZ_DIAGNOSTIC_ASSERT(mSTS);
#endif
  mListener = nullptr;
  mSTS->Dispatch(NS_NewRunnableFunction(
      __func__, [this, self = RefPtr<DataChannelConnection>(this)]() {
        mPacketReceivedListener.DisconnectIfExists();
        mStateChangeListener.DisconnectIfExists();
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
        mShutdown = true;
        DC_DEBUG(("Shutting down connection %p, id %p", this, (void*)mId));
#endif
      }));
}

Maybe<RefPtr<DataChannelConnection>> DataChannelConnection::Create(
    DataChannelConnection::DataConnectionListener* aListener,
    nsISerialEventTarget* aTarget, MediaTransportHandler* aHandler,
    const uint16_t aLocalPort, const uint16_t aNumStreams,
    const Maybe<uint64_t>& aMaxMessageSize) {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<DataChannelConnection> connection;
  if (Preferences::GetBool("media.peerconnection.sctp.use_dcsctp", false)) {
    connection = new DataChannelConnectionDcSctp(aListener, aTarget,
                                                 aHandler);  // Walks into a bar
  } else {
    connection = new DataChannelConnectionUsrsctp(
        aListener, aTarget, aHandler);  // Walks into a bar
  }
  return connection->Init(aLocalPort, aNumStreams, aMaxMessageSize)
             ? Some(connection)
             : Nothing();
}

DataChannelConnection::DataChannelConnection(
    DataChannelConnection::DataConnectionListener* aListener,
    nsISerialEventTarget* aTarget, MediaTransportHandler* aHandler)
    : NeckoTargetHolder(aTarget),
      mListener(aListener),
      mTransportHandler(aHandler) {
  MOZ_ASSERT(NS_IsMainThread());
  DC_VERBOSE(("Constructor DataChannelConnection=%p, listener=%p", this,
              mListener.get()));

  // XXX FIX! make this a global we get once
  // Find the STS thread
  nsresult rv;
  mSTS = mozilla::components::SocketTransport::Service(&rv);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  mShutdown = false;
#endif
}

// Only called on MainThread, mMaxMessageSize is read on other threads
void DataChannelConnection::SetMaxMessageSize(bool aMaxMessageSizeSet,
                                              uint64_t aMaxMessageSize) {
  MOZ_ASSERT(NS_IsMainThread());

  if (mMaxMessageSizeSet && !aMaxMessageSizeSet) {
    // Don't overwrite already set MMS with default values
    return;
  }

  mMaxMessageSizeSet = aMaxMessageSizeSet;
  mMaxMessageSize = aMaxMessageSize;

  nsresult rv;
  nsCOMPtr<nsIPrefService> prefs;
  prefs = mozilla::components::Preferences::Service(&rv);
  if (!NS_WARN_IF(NS_FAILED(rv))) {
    nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(prefs);

    if (branch) {
      int32_t temp;
      if (!NS_FAILED(branch->GetIntPref(
              "media.peerconnection.sctp.force_maximum_message_size", &temp))) {
        if (temp >= 0) {
          mMaxMessageSize = (uint64_t)temp;
        }
      }
    }
  }

  // Fix remote MMS. This code exists, so future implementations of
  // RTCSctpTransport.maxMessageSize can simply provide that value from
  // GetMaxMessageSize.

  // TODO: Bug 1382779, once resolved, can be increased to
  // min(Uint8ArrayMaxSize, UINT32_MAX)
  // TODO: Bug 1381146, once resolved, can be increased to whatever we support
  // then (hopefully
  //       SIZE_MAX)
  if (mMaxMessageSize == 0 ||
      mMaxMessageSize > WEBRTC_DATACHANNEL_MAX_MESSAGE_SIZE_REMOTE) {
    mMaxMessageSize = WEBRTC_DATACHANNEL_MAX_MESSAGE_SIZE_REMOTE;
  }

  DC_DEBUG(("Maximum message size (outgoing data): %" PRIu64
            " (set=%s, enforced=%s)",
            mMaxMessageSize, mMaxMessageSizeSet ? "yes" : "no",
            aMaxMessageSize != mMaxMessageSize ? "yes" : "no"));
}

uint64_t DataChannelConnection::GetMaxMessageSize() {
  MOZ_ASSERT(NS_IsMainThread());
  return mMaxMessageSize;
}

void DataChannelConnection::AppendStatsToReport(
    const UniquePtr<dom::RTCStatsCollection>& aReport,
    const DOMHighResTimeStamp aTimestamp) const {
  MOZ_ASSERT(NS_IsMainThread());
  nsString temp;
  for (const RefPtr<DataChannel>& chan : mChannels.GetAll()) {
    // If channel is empty, ignore
    if (!chan) {
      continue;
    }
    mozilla::dom::RTCDataChannelStats stats;
    nsString id = u"dc"_ns;
    id.AppendInt(chan->GetStream());
    stats.mId.Construct(id);
    chan->GetLabel(temp);
    stats.mTimestamp.Construct(aTimestamp);
    stats.mType.Construct(mozilla::dom::RTCStatsType::Data_channel);
    stats.mLabel.Construct(temp);
    chan->GetProtocol(temp);
    stats.mProtocol.Construct(temp);
    stats.mDataChannelIdentifier.Construct(chan->GetStream());
    {
      using State = mozilla::dom::RTCDataChannelState;
      State state;
      switch (chan->GetReadyState()) {
        case DataChannelState::Connecting:
          state = State::Connecting;
          break;
        case DataChannelState::Open:
          state = State::Open;
          break;
        case DataChannelState::Closing:
          state = State::Closing;
          break;
        case DataChannelState::Closed:
          state = State::Closed;
          break;
      };
      stats.mState.Construct(state);
    }
    auto counters = chan->GetTrafficCounters();
    stats.mMessagesSent.Construct(counters.mMessagesSent);
    stats.mBytesSent.Construct(counters.mBytesSent);
    stats.mMessagesReceived.Construct(counters.mMessagesReceived);
    stats.mBytesReceived.Construct(counters.mBytesReceived);
    if (!aReport->mDataChannelStats.AppendElement(stats, fallible)) {
      mozalloc_handle_oom(0);
    }
  }
}

bool DataChannelConnection::ConnectToTransport(const std::string& aTransportId,
                                               const bool aClient,
                                               const uint16_t aLocalPort,
                                               const uint16_t aRemotePort) {
  MOZ_ASSERT(NS_IsMainThread());

  static const auto paramString =
      [](const std::string& tId, const Maybe<bool>& client,
         const uint16_t localPort, const uint16_t remotePort) -> std::string {
    std::ostringstream stream;
    stream << "Transport ID: '" << tId << "', Role: '"
           << (client ? (client.value() ? "client" : "server") : "")
           << "', Local Port: '" << localPort << "', Remote Port: '"
           << remotePort << "'";
    return stream.str();
  };

  const auto params =
      paramString(aTransportId, Some(aClient), aLocalPort, aRemotePort);
  DC_DEBUG(("ConnectToTransport connecting DTLS transport with parameters: %s",
            params.c_str()));

  DC_WARN(("New transport parameters: %s", params.c_str()));
  if (NS_WARN_IF(aTransportId.empty())) {
    return false;
  }

  if (!mAllocateEven.isSome()) {
    // Do this stuff once.
    mLocalPort = aLocalPort;
    mRemotePort = aRemotePort;
    mAllocateEven = Some(aClient);
    nsTArray<RefPtr<DataChannel>> hasStreamId;
    // Could be faster. Probably doesn't matter.
    while (auto channel = mChannels.Get(INVALID_STREAM)) {
      mChannels.Remove(channel);
      auto id = FindFreeStream();
      if (id != INVALID_STREAM) {
        channel->mStream = id;
        mChannels.Insert(channel);
        DC_DEBUG(("%s %p: Inserting auto-selected id %u", __func__, this,
                  static_cast<unsigned>(id)));
        mStreamIds.InsertElementSorted(id);
        hasStreamId.AppendElement(channel);
      } else {
        // Spec language is very similar to AnnounceClosed, the differences
        // being a lack of a closed check at the top, a different error event,
        // and no removal of the channel from the [[DataChannels]] slot.
        // We don't support firing errors right now, and we probabaly want the
        // closed check anyway, and we don't really have something equivalent
        // to the [[DataChannels]] slot, so just use AnnounceClosed for now.
        channel->AnnounceClosed();
      }
    }

    mSTS->Dispatch(NS_NewRunnableFunction(
        __func__, [this, self = RefPtr<DataChannelConnection>(this),
                   hasStreamId = std::move(hasStreamId)]() {
          SetState(DataChannelConnectionState::Connecting);
          for (auto channel : hasStreamId) {
            OpenFinish(channel);
          }
        }));
  }

  // We do not check whether this is a new transport id here, that happens on
  // STS.
  RUN_ON_THREAD(mSTS,
                WrapRunnable(RefPtr<DataChannelConnection>(this),
                             &DataChannelConnection::SetSignals, aTransportId),
                NS_DISPATCH_NORMAL);
  return true;
}

void DataChannelConnection::SetSignals(const std::string& aTransportId) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  if (mTransportId == aTransportId) {
    // Nothing to do!
    return;
  }

  mTransportId = aTransportId;

  if (!mConnectedToTransportHandler) {
    mPacketReceivedListener =
        mTransportHandler->GetSctpPacketReceived().Connect(
            mSTS, this, &DataChannelConnection::OnPacketReceived);
    mStateChangeListener = mTransportHandler->GetStateChange().Connect(
        mSTS, this, &DataChannelConnection::TransportStateChange);
    mConnectedToTransportHandler = true;
  }
  // SignalStateChange() doesn't call you with the initial state
  if (mTransportHandler->GetState(mTransportId, false) ==
      TransportLayer::TS_OPEN) {
    DC_DEBUG(("Setting transport signals, dtls already open"));
    OnTransportReady();
  } else {
    DC_DEBUG(("Setting transport signals, dtls not open yet"));
  }
}

void DataChannelConnection::TransportStateChange(
    const std::string& aTransportId, TransportLayer::State aState) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  if (aTransportId == mTransportId) {
    if (aState == TransportLayer::TS_OPEN) {
      DC_DEBUG(("Transport is open!"));
      OnTransportReady();
    } else if (aState == TransportLayer::TS_CLOSED ||
               aState == TransportLayer::TS_NONE ||
               aState == TransportLayer::TS_ERROR) {
      DC_DEBUG(("Transport is closed!"));
      Stop();
    }
  }
}

// Process any pending Opens
void DataChannelConnection::ProcessQueuedOpens() {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  std::set<RefPtr<DataChannel>> temp(std::move(mPending));
  for (auto channel : temp) {
    DC_DEBUG(("Processing queued open for %p (%u)", channel.get(),
              channel->mStream));
    OpenFinish(channel);  // may end up back in mPending
  }
}

void DataChannelConnection::OnPacketReceived(const std::string& aTransportId,
                                             const MediaPacket& packet) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  if (packet.type() == MediaPacket::SCTP && mTransportId == aTransportId) {
    OnSctpPacketReceived(packet);
  }
}

void DataChannelConnection::SendPacket(std::unique_ptr<MediaPacket>&& packet) {
  mSTS->Dispatch(NS_NewRunnableFunction(
      "DataChannelConnection::SendPacket",
      [this, self = RefPtr<DataChannelConnection>(this),
       packet = std::move(packet)]() mutable {
        // DC_DEBUG(("%p: SCTP/DTLS sent %ld bytes", this, len));
        if (!mTransportId.empty() && mTransportHandler) {
          mTransportHandler->SendPacket(mTransportId, std::move(*packet));
        }
      }));
}

already_AddRefed<DataChannel> DataChannelConnection::FindChannelByStream(
    uint16_t stream) {
  return mChannels.Get(stream).forget();
}

uint16_t DataChannelConnection::FindFreeStream() const {
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(mAllocateEven.isSome());
  if (!mAllocateEven.isSome()) {
    return INVALID_STREAM;
  }

  uint16_t i = (*mAllocateEven ? 0 : 1);

  // Find the lowest odd/even id that is not present in mStreamIds
  for (auto id : mStreamIds) {
    if (i >= MAX_NUM_STREAMS) {
      return INVALID_STREAM;
    }

    if (id == i) {
      // i is in use, try the next one
      i += 2;
    } else if (id > i) {
      // i is definitely not in use
      break;
    }
  }

  return i;
}

// Returns a POSIX error code.
int DataChannelConnection::SendControlMessage(DataChannel& aChannel,
                                              const uint8_t* data,
                                              uint32_t len) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  // Create message instance and send
  // Note: Main-thread IO, but doesn't block
#if (UINT32_MAX > SIZE_MAX)
  if (len > SIZE_MAX) {
    return EMSGSIZE;
  }
#endif

  DataChannelMessageMetadata metadata(aChannel.mStream,
                                      DATA_CHANNEL_PPID_CONTROL, false);
  nsCString buffer(reinterpret_cast<const char*>(data), len);
  OutgoingMsg msg(std::move(buffer), metadata);

  return SendMessage(aChannel, std::move(msg));
}

// Returns a POSIX error code.
int DataChannelConnection::SendOpenAckMessage(DataChannel& aChannel) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  struct rtcweb_datachannel_ack ack = {};
  ack.msg_type = DATA_CHANNEL_ACK;

  return SendControlMessage(aChannel, (const uint8_t*)&ack, sizeof(ack));
}

// Returns a POSIX error code.
int DataChannelConnection::SendOpenRequestMessage(DataChannel& aChannel) {
  const nsACString& label = aChannel.mLabel;
  const nsACString& protocol = aChannel.mProtocol;
  const bool unordered = !aChannel.mOrdered;
  const DataChannelReliabilityPolicy prPolicy = aChannel.mPrPolicy;
  const uint32_t prValue = aChannel.mPrValue;

  const size_t label_len = label.Length();     // not including nul
  const size_t proto_len = protocol.Length();  // not including nul
  // careful - request struct include one char for the label
  const size_t req_size = sizeof(struct rtcweb_datachannel_open_request) - 1 +
                          label_len + proto_len;
  UniqueFreePtr<struct rtcweb_datachannel_open_request> req(
      (struct rtcweb_datachannel_open_request*)moz_xmalloc(req_size));

  memset(req.get(), 0, req_size);
  req->msg_type = DATA_CHANNEL_OPEN_REQUEST;
  switch (prPolicy) {
    case DataChannelReliabilityPolicy::Reliable:
      req->channel_type = DATA_CHANNEL_RELIABLE;
      break;
    case DataChannelReliabilityPolicy::LimitedLifetime:
      req->channel_type = DATA_CHANNEL_PARTIAL_RELIABLE_TIMED;
      break;
    case DataChannelReliabilityPolicy::LimitedRetransmissions:
      req->channel_type = DATA_CHANNEL_PARTIAL_RELIABLE_REXMIT;
      break;
    default:
      return EINVAL;
  }
  if (unordered) {
    // Per the current types, all differ by 0x80 between ordered and unordered
    req->channel_type |=
        0x80;  // NOTE: be careful if new types are added in the future
  }

  req->reliability_param = htonl(prValue);
  req->priority = htons(0); /* XXX: add support */
  req->label_length = htons(label_len);
  req->protocol_length = htons(proto_len);
  memcpy(&req->label[0], PromiseFlatCString(label).get(), label_len);
  memcpy(&req->label[label_len], PromiseFlatCString(protocol).get(), proto_len);

  // TODO: req_size is an int... that looks hairy
  return SendControlMessage(aChannel, (const uint8_t*)req.get(), req_size);
}

// Caller must ensure that length <= SIZE_MAX
void DataChannelConnection::HandleOpenRequestMessage(
    const struct rtcweb_datachannel_open_request* req, uint32_t length,
    uint16_t stream) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  uint32_t prValue;
  DataChannelReliabilityPolicy prPolicy;

  const size_t requiredLength = (sizeof(*req) - 1) + ntohs(req->label_length) +
                                ntohs(req->protocol_length);
  if (((size_t)length) != requiredLength) {
    if (((size_t)length) < requiredLength) {
      DC_ERROR(
          ("%s: insufficient length: %u, should be %zu. Unable to continue.",
           __FUNCTION__, length, requiredLength));
      return;
    }
    DC_WARN(("%s: Inconsistent length: %u, should be %zu", __FUNCTION__, length,
             requiredLength));
  }

  DC_DEBUG(("%s: length %u, sizeof(*req) = %zu", __FUNCTION__, length,
            sizeof(*req)));

  switch (req->channel_type) {
    case DATA_CHANNEL_RELIABLE:
    case DATA_CHANNEL_RELIABLE_UNORDERED:
      prPolicy = DataChannelReliabilityPolicy::Reliable;
      break;
    case DATA_CHANNEL_PARTIAL_RELIABLE_REXMIT:
    case DATA_CHANNEL_PARTIAL_RELIABLE_REXMIT_UNORDERED:
      prPolicy = DataChannelReliabilityPolicy::LimitedRetransmissions;
      break;
    case DATA_CHANNEL_PARTIAL_RELIABLE_TIMED:
    case DATA_CHANNEL_PARTIAL_RELIABLE_TIMED_UNORDERED:
      prPolicy = DataChannelReliabilityPolicy::LimitedLifetime;
      break;
    default:
      DC_ERROR(("Unknown channel type %d", req->channel_type));
      /* XXX error handling */
      return;
  }

  if (stream >= mNegotiatedIdLimit) {
    DC_ERROR(("%s: stream %u out of bounds (%u)", __FUNCTION__, stream,
              mNegotiatedIdLimit));
    return;
  }

  prValue = ntohl(req->reliability_param);
  const bool ordered = !(req->channel_type & 0x80);
  const nsCString label(&req->label[0], ntohs(req->label_length));
  const nsCString protocol(&req->label[ntohs(req->label_length)],
                           ntohs(req->protocol_length));

  Dispatch(NS_NewRunnableFunction(
      "DataChannelConnection::HandleOpenRequestMessage",
      [this, self = RefPtr<DataChannelConnection>(this), stream, prPolicy,
       prValue, ordered, label, protocol]() {
        RefPtr<DataChannel> channel = FindChannelByStream(stream);
        if (channel) {
          if (!channel->mNegotiated) {
            DC_ERROR(
                ("HandleOpenRequestMessage: channel for pre-existing stream "
                 "%u that was not externally negotiated. JS is lying to us, or "
                 "there's an id collision.",
                 stream));
            /* XXX: some error handling */
          } else {
            DC_DEBUG(("Open for externally negotiated channel %u", stream));
            // XXX should also check protocol, maybe label
            if (prPolicy != channel->mPrPolicy ||
                prValue != channel->mPrValue || ordered != channel->mOrdered) {
              DC_WARN(
                  ("external negotiation mismatch with OpenRequest:"
                   "channel %u, policy %s/%s, value %u/%u, ordered %d/%d",
                   stream, ToString(prPolicy), ToString(channel->mPrPolicy),
                   prValue, channel->mPrValue, static_cast<int>(ordered),
                   static_cast<int>(channel->mOrdered)));
            }
          }
          return;
        }
        channel = new DataChannel(this, stream, DataChannelState::Open, label,
                                  protocol, prPolicy, prValue, ordered, false);
        mChannels.Insert(channel);
        mStreamIds.InsertElementSorted(stream);

        DC_DEBUG(("%s: sending ON_CHANNEL_CREATED for %s/%s: %u", __FUNCTION__,
                  channel->mLabel.get(), channel->mProtocol.get(), stream));
        if (mListener) {
          // important to give it an already_AddRefed pointer!
          mListener->NotifyDataChannel(do_AddRef(channel));
          // Spec says to queue this in the queued task for ondatachannel
          channel->AnnounceOpen();
        }

        mSTS->Dispatch(NS_NewRunnableFunction(
            "DataChannelConnection::HandleOpenRequestMessage",
            [this, self = RefPtr<DataChannelConnection>(this), channel]() {
              // Note that any message can be buffered; SendOpenAckMessage may
              // error later than this check.
              const auto error = SendOpenAckMessage(*channel);
              if (error) {
                DC_ERROR(("SendOpenAckMessage failed, error = %d", error));
                FinishClose_s(channel);
                return;
              }
              channel->mWaitingForAck = false;
              OnStreamOpen(channel->mStream);
            }));
      }));
}

// Caller must ensure that length <= SIZE_MAX
void DataChannelConnection::HandleOpenAckMessage(
    const struct rtcweb_datachannel_ack* ack, uint32_t length,
    uint16_t stream) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());

  RefPtr<DataChannel> channel = FindChannelByStream(stream);
  if (NS_WARN_IF(!channel)) {
    return;
  }

  DC_DEBUG(("OpenAck received for stream %u, waiting=%d", stream,
            channel->mWaitingForAck ? 1 : 0));

  channel->mWaitingForAck = false;
}

// Caller must ensure that length <= SIZE_MAX
void DataChannelConnection::HandleUnknownMessage(uint32_t ppid, uint32_t length,
                                                 uint16_t stream) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  /* XXX: Send an error message? */
  DC_ERROR(("unknown DataChannel message received: %u, len %u on stream %d",
            ppid, length, stream));
  // XXX Log to JS error console if possible
}

void DataChannelConnection::HandleDataMessage(IncomingMsg&& aMsg) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  DataChannelOnMessageAvailable::EventType type;

  size_t data_length = aMsg.GetData().Length();

  RefPtr<DataChannel> channel = FindChannelByStream(aMsg.GetStreamId());
  if (!channel) {
    MOZ_ASSERT(false,
               "Wait until OnStreamOpen is called before calling "
               "HandleDataMessage!");
    return;
  }

  // Receiving any data implies that the other end has received an OPEN
  // request from us.
  channel->mWaitingForAck = false;

  switch (aMsg.GetPpid()) {
    case DATA_CHANNEL_PPID_DOMSTRING:
    case DATA_CHANNEL_PPID_DOMSTRING_PARTIAL:
      DC_DEBUG(
          ("DataChannel: Received string message of length %zu on "
           "channel %u",
           data_length, channel->mStream));
      type = DataChannelOnMessageAvailable::EventType::OnDataString;
      // WebSockets checks IsUTF8() here; we can try to deliver it
      break;

    case DATA_CHANNEL_PPID_DOMSTRING_EMPTY:
      DC_DEBUG((
          "DataChannel: Received empty string message of length %zu on channel "
          "%u",
          data_length, channel->mStream));
      // Just in case.
      aMsg.GetData().Truncate(0);
      type = DataChannelOnMessageAvailable::EventType::OnDataString;
      break;

    case DATA_CHANNEL_PPID_BINARY:
    case DATA_CHANNEL_PPID_BINARY_PARTIAL:
      DC_DEBUG(
          ("DataChannel: Received binary message of length %zu on "
           "channel id %u",
           data_length, channel->mStream));
      type = DataChannelOnMessageAvailable::EventType::OnDataBinary;
      break;

    case DATA_CHANNEL_PPID_BINARY_EMPTY:
      DC_DEBUG((
          "DataChannel: Received empty binary message of length %zu on channel "
          "id %u",
          data_length, channel->mStream));
      // Just in case.
      aMsg.GetData().Truncate(0);
      type = DataChannelOnMessageAvailable::EventType::OnDataBinary;
      break;

    default:
      NS_ERROR("Unknown data PPID");
      DC_ERROR(("Unknown data PPID %" PRIu32, aMsg.GetPpid()));
      return;
  }

  Dispatch(NS_NewRunnableFunction(
      "DataChannelConnection::HandleDataMessage", [channel, data_length]() {
        channel->mTrafficCounters.mMessagesReceived++;
        channel->mTrafficCounters.mBytesReceived += data_length;
      }));

  // Notify onmessage
  DC_DEBUG(
      ("%s: sending %s for %p", __FUNCTION__, ToString(type), channel.get()));
  channel->SendOrQueue(new DataChannelOnMessageAvailable(
      type, this, channel, std::move(aMsg.GetData())));
}

void DataChannelConnection::HandleDCEPMessage(IncomingMsg&& aMsg) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  const struct rtcweb_datachannel_open_request* req;
  const struct rtcweb_datachannel_ack* ack;

  req = reinterpret_cast<const struct rtcweb_datachannel_open_request*>(
      aMsg.GetData().BeginReading());

  size_t data_length = aMsg.GetLength();

  DC_DEBUG(("Handling DCEP message of length %zu", data_length));

  // Ensure minimum message size (ack is the smallest DCEP message)
  if (data_length < sizeof(*ack)) {
    DC_WARN(("Ignored invalid DCEP message (too short)"));
    return;
  }

  switch (req->msg_type) {
    case DATA_CHANNEL_OPEN_REQUEST:
      // structure includes a possibly-unused char label[1] (in a packed
      // structure)
      if (NS_WARN_IF(data_length < sizeof(*req) - 1)) {
        return;
      }

      HandleOpenRequestMessage(req, data_length, aMsg.GetStreamId());
      break;
    case DATA_CHANNEL_ACK:
      // >= sizeof(*ack) checked above

      ack = reinterpret_cast<const struct rtcweb_datachannel_ack*>(
          aMsg.GetData().BeginReading());
      HandleOpenAckMessage(ack, data_length, aMsg.GetStreamId());
      break;
    default:
      HandleUnknownMessage(aMsg.GetPpid(), data_length, aMsg.GetStreamId());
      break;
  }
}

bool DataChannelConnection::ReassembleMessageChunk(IncomingMsg& aReassembled,
                                                   const void* buffer,
                                                   size_t length, uint32_t ppid,
                                                   uint16_t stream) {
  // Note: Until we support SIZE_MAX sized messages, we need this check
#if (SIZE_MAX > UINT32_MAX)
  if (length > UINT32_MAX) {
    DC_ERROR(("DataChannel: Cannot handle message of size %zu (max=%u)", length,
              UINT32_MAX));
    return false;
  }
#endif

  // Ensure it doesn't blow up our buffer
  // TODO: Change 'WEBRTC_DATACHANNEL_MAX_MESSAGE_SIZE_LOCAL' to whatever the
  //       new buffer is capable of holding.
  if (length + aReassembled.GetLength() >
      WEBRTC_DATACHANNEL_MAX_MESSAGE_SIZE_LOCAL) {
    DC_ERROR(
        ("DataChannel: Buffered message would become too large to handle, "
         "closing connection"));
    return false;
  }

  if (aReassembled.GetPpid() != ppid) {
    NS_WARNING("DataChannel message aborted by fragment type change!");
    return false;
  }

  aReassembled.Append((uint8_t*)buffer, length);

  return true;
}

void DataChannelConnection::ClearResets() {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  // Clear all pending resets
  if (!mStreamsResetting.IsEmpty()) {
    DC_DEBUG(("Clearing resets for %zu streams", mStreamsResetting.Length()));
  }
  mStreamsResetting.Clear();
}

void DataChannelConnection::MarkStreamForReset(DataChannel& aChannel) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());

  DC_DEBUG(("%s %p: Resetting outgoing stream %u", __func__, this,
            aChannel.mStream));
  // Rarely has more than a couple items and only for a short time
  for (size_t i = 0; i < mStreamsResetting.Length(); ++i) {
    if (mStreamsResetting[i] == aChannel.mStream) {
      return;
    }
  }
  mStreamsResetting.AppendElement(aChannel.mStream);
}

void DataChannelConnection::OnStreamsReset(std::vector<uint16_t>&& aStreams) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  for (auto stream : aStreams) {
    RefPtr<DataChannel> channel = FindChannelByStream(stream);
    if (channel) {
      // The other side closed the channel
      // We could be in three states:
      // 1. Normal state (input and output streams (OPEN)
      //    Notify application, send a RESET in response on our
      //    outbound channel.  Go to CLOSED
      // 2. We sent our own reset (CLOSING); either they crossed on the
      //    wire, or this is a response to our Reset.
      //    Go to CLOSED
      // 3. We've sent a open but haven't gotten a response yet (CONNECTING)
      //    I believe this is impossible, as we don't have an input stream
      //    yet.

      DC_DEBUG(("Connection %p: stream %u closed", this, stream));

      DC_DEBUG(("Disconnected DataChannel %p from connection %p",
                (void*)channel, this));
      FinishClose_s(channel);
    } else {
      DC_WARN(("Connection %p: Can't find incoming stream %u", this, stream));
    }
  }

  Dispatch(
      NS_NewRunnableFunction("DataChannelConnection::HandleStreamResetEvent",
                             [this, self = RefPtr<DataChannelConnection>(this),
                              streamsReset = std::move(aStreams)]() {
                               for (auto stream : streamsReset) {
                                 mStreamIds.RemoveElementSorted(stream);
                               }
                             }));

  // Process pending resets in bulk
  if (!mStreamsResetting.IsEmpty()) {
    DC_DEBUG(("Sending %zu pending resets", mStreamsResetting.Length()));
    ResetStreams(mStreamsResetting);
  }
}

already_AddRefed<DataChannel> DataChannelConnection::Open(
    const nsACString& label, const nsACString& protocol,
    DataChannelReliabilityPolicy prPolicy, bool inOrder, uint32_t prValue,
    bool aExternalNegotiated, uint16_t aStream) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!aExternalNegotiated) {
    if (mAllocateEven.isSome()) {
      aStream = FindFreeStream();
      if (aStream == INVALID_STREAM) {
        return nullptr;
      }
    } else {
      // We do not yet know whether we are client or server, and an id has not
      // been chosen for us. We will need to choose later.
      aStream = INVALID_STREAM;
    }
  }

  DC_DEBUG(
      ("DC Open: label %s/%s, type %s, inorder %d, prValue %u, "
       "external: %s, stream %u",
       PromiseFlatCString(label).get(), PromiseFlatCString(protocol).get(),
       ToString(prPolicy), inOrder, prValue,
       aExternalNegotiated ? "true" : "false", aStream));

  if ((prPolicy == DataChannelReliabilityPolicy::Reliable) && (prValue != 0)) {
    return nullptr;
  }

  if (aStream != INVALID_STREAM) {
    if (mStreamIds.ContainsSorted(aStream)) {
      DC_ERROR(("external negotiation of already-open channel %u", aStream));
      // This is the only place where duplicate id checking is performed. The
      // JSImpl code assumes that any error is due to id-related problems. This
      // probably needs some cleanup.
      return nullptr;
    }

    DC_DEBUG(("%s %p: Inserting externally-negotiated id %u", __func__, this,
              static_cast<unsigned>(aStream)));
    mStreamIds.InsertElementSorted(aStream);
  }

  RefPtr<DataChannel> channel(new DataChannel(
      this, aStream, DataChannelState::Connecting, label, protocol, prPolicy,
      prValue, inOrder, aExternalNegotiated));
  mChannels.Insert(channel);

  if (aStream != INVALID_STREAM) {
    mSTS->Dispatch(NS_NewRunnableFunction(
        "DataChannel::OpenFinish",
        [this, self = RefPtr<DataChannelConnection>(this), channel]() mutable {
          OpenFinish(channel);
        }));
  }

  return channel.forget();
}

// Separate routine so we can also call it to finish up from pending opens
void DataChannelConnection::OpenFinish(RefPtr<DataChannel> aChannel) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  const uint16_t stream = aChannel->mStream;

  // Cases we care about:
  // Pre-negotiated:
  //    Not Open:
  //      Doesn't fit:
  //         -> change initial ask or renegotiate after open
  //      -> queue open
  //    Open:
  //      Doesn't fit:
  //         -> RaiseStreamLimitTo && queue
  //      Does fit:
  //         -> open
  // Not negotiated:
  //    Not Open:
  //      -> queue open
  //    Open:
  //      -> Try to get a stream
  //      Doesn't fit:
  //         -> RaiseStreamLimitTo && queue
  //      Does fit:
  //         -> open
  // So the Open cases are basically the same
  // Not Open cases are simply queue for non-negotiated, and
  // either change the initial ask or possibly renegotiate after open.
  DataChannelConnectionState state = GetState();
  if (state != DataChannelConnectionState::Open ||
      stream >= mNegotiatedIdLimit) {
    if (state == DataChannelConnectionState::Open) {
      MOZ_ASSERT(stream != INVALID_STREAM);
      // RaiseStreamLimitTo() limits to MAX_NUM_STREAMS -- allocate extra
      // streams to avoid asking for more every time we want a higher limit.
      uint16_t num_desired = std::min(16 * (stream / 16 + 1), MAX_NUM_STREAMS);
      DC_DEBUG(("Attempting to raise stream limit %u -> %u", mNegotiatedIdLimit,
                num_desired));
      if (!RaiseStreamLimitTo(num_desired)) {
        NS_ERROR("Failed to request more streams");
        FinishClose_s(aChannel);
        return;
      }
    }
    DC_DEBUG(
        ("Queuing channel %p (%u) to finish open", aChannel.get(), stream));
    mPending.insert(aChannel);
    return;
  }

  MOZ_ASSERT(state == DataChannelConnectionState::Open);
  MOZ_ASSERT(stream != INVALID_STREAM);
  MOZ_ASSERT(stream < mNegotiatedIdLimit);

  if (!aChannel->mNegotiated) {
    if (!aChannel->mOrdered) {
      // Don't send unordered until this gets cleared.
      aChannel->mWaitingForAck = true;
    }

    const int error = SendOpenRequestMessage(*aChannel);
    if (error) {
      DC_ERROR(("SendOpenRequest failed, error = %d", error));
      FinishClose_s(aChannel);
      return;
    }
  }

  // Either externally negotiated or we sent Open
  // FIX?  Move into DOMDataChannel?  I don't think we can send it yet here
  aChannel->AnnounceOpen();
  OnStreamOpen(stream);
}

class ReadBlobRunnable : public Runnable {
 public:
  ReadBlobRunnable(DataChannelConnection* aConnection, uint16_t aStream,
                   nsIInputStream* aBlob)
      : Runnable("ReadBlobRunnable"),
        mConnection(aConnection),
        mStream(aStream),
        mBlob(aBlob) {}

  NS_IMETHOD Run() override {
    // ReadBlob() is responsible to releasing the reference
    DataChannelConnection* self = mConnection;
    self->ReadBlob(mConnection.forget(), mStream, mBlob);
    return NS_OK;
  }

 private:
  // Make sure the Connection doesn't die while there are jobs outstanding.
  // Let it die (if released by PeerConnectionImpl while we're running)
  // when we send our runnable back to MainThread.  Then ~DataChannelConnection
  // can send the IOThread to MainThread to die in a runnable, avoiding
  // unsafe event loop recursion.  Evil.
  RefPtr<DataChannelConnection> mConnection;
  uint16_t mStream;
  // Use RefCount for preventing the object is deleted when SendBlob returns.
  RefPtr<nsIInputStream> mBlob;
};

// Returns a POSIX error code.
int DataChannelConnection::SendBlob(uint16_t stream, nsIInputStream* aBlob) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<DataChannel> channel = mChannels.Get(stream);
  if (NS_WARN_IF(!channel)) {
    return EINVAL;  // TODO: Find a better error code
  }

  // Spawn a thread to send the data
  if (!mInternalIOThread) {
    nsresult rv =
        NS_NewNamedThread("DataChannel IO", getter_AddRefs(mInternalIOThread));
    if (NS_FAILED(rv)) {
      return EINVAL;  // TODO: Find a better error code
    }
  }

  mInternalIOThread->Dispatch(
      do_AddRef(new ReadBlobRunnable(this, stream, aBlob)), NS_DISPATCH_NORMAL);
  return 0;
}

class DataChannelBlobSendRunnable : public Runnable {
 public:
  DataChannelBlobSendRunnable(
      already_AddRefed<DataChannelConnection>& aConnection, uint16_t aStream)
      : Runnable("DataChannelBlobSendRunnable"),
        mConnection(aConnection),
        mStream(aStream) {}

  ~DataChannelBlobSendRunnable() override {
    if (!NS_IsMainThread() && mConnection) {
      MOZ_ASSERT(false);
      // explicitly leak the connection if destroyed off mainthread
      Unused << mConnection.forget().take();
    }
  }

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());

    mConnection->SendBinaryMessage(mStream, std::move(mData));
    mConnection = nullptr;
    return NS_OK;
  }

  // explicitly public so we can avoid allocating twice and copying
  nsCString mData;

 private:
  // Note: we can be destroyed off the target thread, so be careful not to let
  // this get Released()ed on the temp thread!
  RefPtr<DataChannelConnection> mConnection;
  uint16_t mStream;
};

void DataChannelConnection::SetState(DataChannelConnectionState aState) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());

  DC_DEBUG(
      ("DataChannelConnection labeled %s (%p) switching connection state %s -> "
       "%s",
       mTransportId.c_str(), this, ToString(mState), ToString(aState)));

  mState = aState;
}

void DataChannelConnection::ReadBlob(
    already_AddRefed<DataChannelConnection> aThis, uint16_t aStream,
    nsIInputStream* aBlob) {
  MOZ_ASSERT(!mSTS->IsOnCurrentThread());
  MOZ_ASSERT(!NS_IsMainThread());
  // NOTE: 'aThis' has been forgotten by the caller to avoid releasing
  // it off mainthread; if PeerConnectionImpl has released then we want
  // ~DataChannelConnection() to run on MainThread

  // Must not let Dispatching it cause the DataChannelConnection to get
  // released on the wrong thread.  Using
  // WrapRunnable(RefPtr<DataChannelConnection>(aThis),... will occasionally
  // cause aThis to get released on this thread.  Also, an explicit Runnable
  // lets us avoid copying the blob data an extra time.
  RefPtr<DataChannelBlobSendRunnable> runnable =
      new DataChannelBlobSendRunnable(aThis, aStream);
  // avoid copying the blob data by passing the mData from the runnable
  if (NS_FAILED(NS_ReadInputStreamToString(aBlob, runnable->mData, -1))) {
    // Bug 966602:  Doesn't return an error to the caller via onerror.
    // We must release DataChannelConnection on MainThread to avoid issues (bug
    // 876167) aThis is now owned by the runnable; release it there
    NS_ReleaseOnMainThread("DataChannelBlobSendRunnable", runnable.forget());
    return;
  }
  aBlob->Close();
  Dispatch(runnable.forget());
}

int DataChannelConnection::SendDataMessage(uint16_t aStream, nsACString&& aMsg,
                                           bool aIsBinary) {
  MOZ_ASSERT(NS_IsMainThread());

  // Basic validation
  if (mMaxMessageSize != 0 && aMsg.Length() > mMaxMessageSize) {
    DC_ERROR(("Message rejected, too large (%zu > %" PRIu64 ")", aMsg.Length(),
              mMaxMessageSize));
    return EMSGSIZE;
  }

  nsCString temp(std::move(aMsg));

  mSTS->Dispatch(NS_NewRunnableFunction(
      __func__, [this, self = RefPtr<DataChannelConnection>(this), aStream,
                 msg = std::move(temp), aIsBinary]() mutable {
        RefPtr<DataChannel> channel = FindChannelByStream(aStream);
        if (!channel) {
          // Must have closed due to a transport error?
          return;
        }

        Maybe<uint16_t> maxRetransmissions;
        Maybe<uint16_t> maxLifetimeMs;

        switch (channel->mPrPolicy) {
          case DataChannelReliabilityPolicy::Reliable:
            break;
          case DataChannelReliabilityPolicy::LimitedRetransmissions:
            maxRetransmissions = Some(channel->mPrValue);
            break;
          case DataChannelReliabilityPolicy::LimitedLifetime:
            maxLifetimeMs = Some(channel->mPrValue);
            break;
        }

        uint32_t ppid;
        if (aIsBinary) {
          if (msg.Length()) {
            ppid = DATA_CHANNEL_PPID_BINARY;
          } else {
            ppid = DATA_CHANNEL_PPID_BINARY_EMPTY;
            msg.Append('\0');
          }
        } else {
          if (msg.Length()) {
            ppid = DATA_CHANNEL_PPID_DOMSTRING;
          } else {
            ppid = DATA_CHANNEL_PPID_DOMSTRING_EMPTY;
            msg.Append('\0');
          }
        }

        DataChannelMessageMetadata metadata(
            channel->mStream, ppid,
            !channel->mOrdered && !channel->mWaitingForAck, maxRetransmissions,
            maxLifetimeMs);
        // Create message instance and send
        OutgoingMsg outgoing(std::move(msg), metadata);

        if (!SendMessage(*channel, std::move(outgoing))) {
          Dispatch(
              NS_NewRunnableFunction(__func__, [channel, len = msg.Length()]() {
                channel->mTrafficCounters.mMessagesSent++;
                channel->mTrafficCounters.mBytesSent += len;
              }));
        }
      }));

  return 0;
}

void DataChannelConnection::Stop() {
  // Note: This will call 'CloseAll' from the main thread
  Dispatch(do_AddRef(new DataChannelOnMessageAvailable(
      DataChannelOnMessageAvailable::EventType::OnDisconnected, this)));
}

// Implementation of RTCDataChannel.close()
void DataChannelConnection::Close(DataChannel* aChannel) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aChannel);
  RefPtr<DataChannel> channel(aChannel);

  // close()

  // Closes the RTCDataChannel. It may be called regardless of whether the
  // RTCDataChannel object was created by this peer or the remote peer.

  // When the close method is called, the user agent MUST run the following
  // steps:

  // Let channel be the RTCDataChannel object which is about to be closed.

  // If channel.[[ReadyState]] is "closing" or "closed", then abort these
  // steps.
  DataChannelState channelState = channel->GetReadyState();
  if (channelState == DataChannelState::Closed ||
      channelState == DataChannelState::Closing) {
    DC_DEBUG(("Channel already closing/closed (%s)", ToString(channelState)));
    return;
  }

  // Set channel.[[ReadyState]] to "closing".
  channel->SetReadyState(DataChannelState::Closing);

  // If the closing procedure has not started yet, start it.
  GracefulClose(channel);
}

void DataChannelConnection::GracefulClose(DataChannel* aChannel) {
  MOZ_ASSERT(NS_IsMainThread());
  // An RTCDataChannel object's underlying data transport may be torn down in a
  // non-abrupt manner by running the closing procedure. When that happens the
  // user agent MUST queue a task to run the following steps:

  Dispatch(NS_NewRunnableFunction(
      __func__, [this, self = RefPtr<DataChannelConnection>(this),
                 channel = RefPtr<DataChannel>(aChannel)]() {
        // Let channel be the RTCDataChannel object whose underlying data
        // transport was closed.

        // Let connection be the RTCPeerConnection object associated with
        // channel.

        // Remove channel from connection.[[DataChannels]].
        // Note: We don't really have this slot. Reading the spec, it does not
        // appear this serves any function other than holding a ref to the
        // RTCDataChannel, which in our case is handled by a self ref in
        // nsDOMDataChannel.

        // Unless the procedure was initiated by channel.close, set
        // channel.[[ReadyState]] to "closing" and fire an event named closing
        // at channel. Note: channel.close will set [[ReadyState]] to Closing.
        // We also check for closed, just as belt and suspenders.
        if (channel->GetReadyState() != DataChannelState::Closing &&
            channel->GetReadyState() != DataChannelState::Closed) {
          channel->SetReadyState(DataChannelState::Closing);
          // TODO(bug 1611953): Fire event
        }

        // Run the following steps in parallel:
        // Finish sending all currently pending messages of the channel.
        // Note: We detect when all pending messages are sent with
        // mBufferedAmount. We do an initial check here, and subsequent checks
        // in DecrementBufferedAmount.
        // Caveat: mBufferedAmount is decremented when the bytes are first
        // transmitted, _not_ when they are acked. We might need to do some
        // work to ensure that the SCTP stack has delivered these last bytes to
        // the other end before that channel/connection is fully closed.
        if (!channel->mBufferedAmount &&
            channel->GetReadyState() != DataChannelState::Closed) {
          FinishClose(channel);
        }
      }));
}

void DataChannelConnection::FinishClose(DataChannel* aChannel) {
  MOZ_ASSERT(NS_IsMainThread());
  mSTS->Dispatch(NS_NewRunnableFunction(
      __func__,
      [this, self = RefPtr<DataChannelConnection>(this),
       channel = RefPtr<DataChannel>(aChannel)]() { FinishClose_s(channel); }));
}

void DataChannelConnection::FinishClose_s(DataChannel* aChannel) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());

  // We're removing this from all containers, make sure the passed pointer
  // stays valid.
  // It is possible for this to be called twice if both JS and the transport
  // side cause closure at the same time, but this is idempotent so no big deal
  RefPtr<DataChannel> channel(aChannel);
  aChannel->mBufferedData.Clear();
  mChannels.Remove(aChannel);
  mPending.erase(aChannel);

  // Follow the closing procedure defined for the channel's underlying
  // data transport :

  // In the case of an SCTP-based transport, follow [RFC8831], section
  // 6.7.
  if (channel->mStream != INVALID_STREAM) {
    MarkStreamForReset(*aChannel);
    if (GetState() != DataChannelConnectionState::Closed) {
      // Individual channel is being closed, send reset now.
      // If the whole connection is closed, rely on the caller to send the
      // resets once it is done closing all of the channels.
      ResetStreams(mStreamsResetting);
    }
  }

  // Close the channel's data transport by following the associated
  // procedure.
  aChannel->AnnounceClosed();
}

void DataChannelConnection::CloseAll() {
  MOZ_ASSERT(NS_IsMainThread());
  DC_DEBUG(("Closing all channels (connection %p)", (void*)this));

  // Make sure no more channels will be opened
  // Close current channels
  // If there are runnables, they hold a strong ref and keep the channel
  // and/or connection alive (even if in a CLOSED state)
  for (auto& channel : mChannels.GetAll()) {
    channel->Close();
  }

  mSTS->Dispatch(NS_NewRunnableFunction(
      "DataChannelConnection::CloseAll",
      [this, self = RefPtr<DataChannelConnection>(this)]() {
        // Make sure no more channels will be opened
        SetState(DataChannelConnectionState::Closed);

        // Close current channels
        // If there are runnables, they hold a strong ref and keep the channel
        // and/or connection alive (even if in a CLOSED state)
        for (auto& channel : mChannels.GetAll()) {
          FinishClose_s(channel.get());
        }

        // Clean up any pending opens for channels
        for (const auto& channel : mPending) {
          DC_DEBUG(("closing pending channel %p, stream %u", channel.get(),
                    channel->mStream));
          FinishClose_s(
              channel.get());  // also releases the ref on each iteration
        }
        // It's more efficient to let the Resets queue in shutdown and then
        // ResetStreams() here.
        if (!mStreamsResetting.IsEmpty()) {
          ResetStreams(mStreamsResetting);
        }
      }));
}

bool DataChannelConnection::Channels::IdComparator::Equals(
    const RefPtr<DataChannel>& aChannel, uint16_t aId) const {
  return aChannel->mStream == aId;
}

bool DataChannelConnection::Channels::IdComparator::LessThan(
    const RefPtr<DataChannel>& aChannel, uint16_t aId) const {
  return aChannel->mStream < aId;
}

bool DataChannelConnection::Channels::IdComparator::Equals(
    const RefPtr<DataChannel>& a1, const RefPtr<DataChannel>& a2) const {
  return Equals(a1, a2->mStream);
}

bool DataChannelConnection::Channels::IdComparator::LessThan(
    const RefPtr<DataChannel>& a1, const RefPtr<DataChannel>& a2) const {
  return LessThan(a1, a2->mStream);
}

void DataChannelConnection::Channels::Insert(
    const RefPtr<DataChannel>& aChannel) {
  DC_DEBUG(("Inserting channel %u : %p", aChannel->mStream, aChannel.get()));
  MutexAutoLock lock(mMutex);
  if (aChannel->mStream != INVALID_STREAM) {
    MOZ_ASSERT(!mChannels.ContainsSorted(aChannel, IdComparator()));
  }

  MOZ_ASSERT(!mChannels.Contains(aChannel));

  mChannels.InsertElementSorted(aChannel, IdComparator());
}

bool DataChannelConnection::Channels::Remove(
    const RefPtr<DataChannel>& aChannel) {
  DC_DEBUG(("Removing channel %u : %p", aChannel->mStream, aChannel.get()));
  MutexAutoLock lock(mMutex);
  if (aChannel->mStream == INVALID_STREAM) {
    return mChannels.RemoveElement(aChannel);
  }

  return mChannels.RemoveElementSorted(aChannel, IdComparator());
}

RefPtr<DataChannel> DataChannelConnection::Channels::Get(uint16_t aId) const {
  MutexAutoLock lock(mMutex);
  auto index = mChannels.BinaryIndexOf(aId, IdComparator());
  if (index == ChannelArray::NoIndex) {
    return nullptr;
  }
  return mChannels[index];
}

RefPtr<DataChannel> DataChannelConnection::Channels::GetNextChannel(
    uint16_t aCurrentId) const {
  MutexAutoLock lock(mMutex);
  if (mChannels.IsEmpty()) {
    return nullptr;
  }

  auto index = mChannels.IndexOfFirstElementGt(aCurrentId, IdComparator());
  if (index == mChannels.Length()) {
    index = 0;
  }
  return mChannels[index];
}

DataChannel::DataChannel(DataChannelConnection* connection, uint16_t stream,
                         DataChannelState state, const nsACString& label,
                         const nsACString& protocol,
                         DataChannelReliabilityPolicy policy, uint32_t value,
                         bool ordered, bool negotiated)
    : mLabel(label),
      mProtocol(protocol),
      mReadyState(state),
      mStream(stream),
      mPrPolicy(policy),
      mPrValue(value),
      mBufferedThreshold(0),  // default from spec
      mBufferedAmount(0),
      mConnection(connection),
      mNegotiated(negotiated),
      mOrdered(ordered),
      mMainThreadEventTarget(connection->GetNeckoTarget()) {
  NS_ASSERTION(mConnection, "NULL connection");
}

DataChannel::~DataChannel() {
  // NS_ASSERTION since this is more "I think I caught all the cases that
  // can cause this" than a true kill-the-program assertion.  If this is
  // wrong, nothing bad happens.  A worst it's a leak.
  NS_ASSERTION(mReadyState == DataChannelState::Closed ||
                   mReadyState == DataChannelState::Closing,
               "unexpected state in ~DataChannel");
}

void DataChannel::Close() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mConnection) {
    // ensure we don't get deleted
    RefPtr<DataChannelConnection> connection(mConnection);
    connection->Close(this);
  }
}

void DataChannel::ReleaseConnection() {
  MOZ_ASSERT(NS_IsMainThread());
  mConnection = nullptr;
}

void DataChannel::SetListener(DataChannelListener* aListener,
                              nsISupports* aContext) {
  MOZ_ASSERT(NS_IsMainThread());
  mContext = aContext;
  mListener = aListener;
}

void DataChannel::SendErrnoToErrorResult(int error, size_t aMessageSize,
                                         ErrorResult& aRv) {
  switch (error) {
    case 0:
      break;
    case EMSGSIZE: {
      nsPrintfCString err("Message size (%zu) exceeds maxMessageSize",
                          aMessageSize);
      aRv.ThrowTypeError(err);
      break;
    }
    default:
      aRv.Throw(NS_ERROR_DOM_OPERATION_ERR);
      break;
  }
}

void DataChannel::IncrementBufferedAmount(uint32_t aSize, ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mBufferedAmount > UINT32_MAX - aSize) {
    aRv.Throw(NS_ERROR_FILE_TOO_BIG);
    return;
  }

  mBufferedAmount += aSize;
}

void DataChannel::DecrementBufferedAmount(uint32_t aSize) {
  mMainThreadEventTarget->Dispatch(NS_NewRunnableFunction(
      "DataChannel::DecrementBufferedAmount",
      [this, self = RefPtr<DataChannel>(this), aSize] {
        MOZ_ASSERT(aSize <= mBufferedAmount);
        bool wasLow = mBufferedAmount <= mBufferedThreshold;
        mBufferedAmount -= aSize;
        if (!wasLow && mBufferedAmount <= mBufferedThreshold) {
          DC_DEBUG(("%s: sending BUFFER_LOW_THRESHOLD for %s/%s: %u",
                    __FUNCTION__, mLabel.get(), mProtocol.get(), mStream));
          mListener->OnBufferLow(mContext);
        }
        if (mBufferedAmount == 0) {
          DC_DEBUG(("%s: sending NO_LONGER_BUFFERED for %s/%s: %u",
                    __FUNCTION__, mLabel.get(), mProtocol.get(), mStream));
          mListener->NotBuffered(mContext);
          if (mReadyState == DataChannelState::Closing) {
            if (mConnection) {
              // We're done sending
              mConnection->FinishClose(this);
            }
          }
        }
      }));
}

void DataChannel::AnnounceOpen() {
  mMainThreadEventTarget->Dispatch(NS_NewRunnableFunction(
      "DataChannel::AnnounceOpen", [this, self = RefPtr<DataChannel>(this)] {
        DataChannelState state = GetReadyState();
        // Special-case; spec says to put brand-new remote-created
        // DataChannel in "open", but queue the firing of the "open" event.
        if (state != DataChannelState::Closing &&
            state != DataChannelState::Closed) {
          // Stats stuff
          if (!mEverOpened && mConnection && mConnection->mListener) {
            mEverOpened = true;
            mConnection->mListener->NotifyDataChannelOpen(this);
          }
          SetReadyState(DataChannelState::Open);
          DC_DEBUG(("%s: sending ON_CHANNEL_OPEN for %s/%s: %u", __FUNCTION__,
                    mLabel.get(), mProtocol.get(), mStream));
          if (mListener) {
            mListener->OnChannelConnected(mContext);
          }
        }
      }));
}

void DataChannel::AnnounceClosed() {
  // When an RTCDataChannel object's underlying data transport has been closed,
  // the user agent MUST queue a task to run the following steps:
  mMainThreadEventTarget->Dispatch(NS_NewRunnableFunction(
      "DataChannel::AnnounceClosed", [this, self = RefPtr<DataChannel>(this)] {
        // Let channel be the RTCDataChannel object whose underlying data
        // transport was closed.
        // If channel.[[ReadyState]] is "closed", abort these steps.
        if (GetReadyState() == DataChannelState::Closed) {
          return;
        }

        // Set channel.[[ReadyState]] to "closed".
        SetReadyState(DataChannelState::Closed);

        // Remove channel from connection.[[DataChannels]] if it is still there.
        // Note: We don't really have this slot. Reading the spec, it does not
        // appear this serves any function other than holding a ref to the
        // RTCDataChannel, which in our case is handled by a self ref in
        // nsDOMDataChannel.

        // If the transport was closed with an error, fire an event named error
        // using the RTCErrorEvent interface with its errorDetail attribute set
        // to "sctp-failure" at channel.
        // Note: We don't support this yet.

        // Fire an event named close at channel.
        if (mListener) {
          DC_DEBUG(("%s: sending ON_CHANNEL_CLOSED for %s/%s: %u", __FUNCTION__,
                    mLabel.get(), mProtocol.get(), mStream));
          mListener->OnChannelClosed(mContext);
        }

        // Stats stuff
        if (mEverOpened && mConnection && mConnection->mListener) {
          mConnection->mListener->NotifyDataChannelClosed(this);
        }
      }));
}

// Set ready state
void DataChannel::SetReadyState(const DataChannelState aState) {
  MOZ_ASSERT(NS_IsMainThread());

  DC_DEBUG(
      ("DataChannelConnection labeled %s(%p) (stream %d) changing ready "
       "state "
       "%s -> %s",
       mLabel.get(), this, mStream, ToString(mReadyState), ToString(aState)));

  mReadyState = aState;
}

void DataChannel::SendMsg(nsACString&& aMsg, ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!EnsureValidStream(aRv)) {
    return;
  }

  const size_t length = aMsg.Length();
  SendErrnoToErrorResult(mConnection->SendMessage(mStream, std::move(aMsg)),
                         length, aRv);
  if (!aRv.Failed()) {
    IncrementBufferedAmount(length, aRv);
  }
}

void DataChannel::SendBinaryMsg(nsACString&& aMsg, ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!EnsureValidStream(aRv)) {
    return;
  }

  const size_t length = aMsg.Length();
  SendErrnoToErrorResult(
      mConnection->SendBinaryMessage(mStream, std::move(aMsg)), length, aRv);
  if (!aRv.Failed()) {
    IncrementBufferedAmount(length, aRv);
  }
}

void DataChannel::SendBinaryBlob(dom::Blob& aBlob, ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!EnsureValidStream(aRv)) {
    return;
  }

  uint64_t msgLength = aBlob.GetSize(aRv);
  if (aRv.Failed()) {
    return;
  }

  if (msgLength > UINT32_MAX) {
    aRv.Throw(NS_ERROR_FILE_TOO_BIG);
    return;
  }

  // We convert to an nsIInputStream here, because Blob is not threadsafe,
  // and we don't convert it earlier because we need to know how large this
  // is so we can update bufferedAmount.
  nsCOMPtr<nsIInputStream> msgStream;
  aBlob.CreateInputStream(getter_AddRefs(msgStream), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  SendErrnoToErrorResult(mConnection->SendBlob(mStream, msgStream), msgLength,
                         aRv);
  if (!aRv.Failed()) {
    IncrementBufferedAmount(msgLength, aRv);
  }
}

dom::Nullable<uint16_t> DataChannel::GetMaxPacketLifeTime() const {
  if (mPrPolicy == DataChannelReliabilityPolicy::LimitedLifetime) {
    return dom::Nullable<uint16_t>(mPrValue);
  }
  return dom::Nullable<uint16_t>();
}

dom::Nullable<uint16_t> DataChannel::GetMaxRetransmits() const {
  if (mPrPolicy == DataChannelReliabilityPolicy::LimitedRetransmissions) {
    return dom::Nullable<uint16_t>(mPrValue);
  }
  return dom::Nullable<uint16_t>();
}

uint32_t DataChannel::GetBufferedAmountLowThreshold() const {
  return mBufferedThreshold;
}

// Never fire immediately, as it's defined to fire on transitions, not state
void DataChannel::SetBufferedAmountLowThreshold(uint32_t aThreshold) {
  mBufferedThreshold = aThreshold;
}

void DataChannel::SendOrQueue(DataChannelOnMessageAvailable* aMessage) {
  nsCOMPtr<nsIRunnable> runnable = aMessage;
  mMainThreadEventTarget->Dispatch(runnable.forget());
}

DataChannel::TrafficCounters DataChannel::GetTrafficCounters() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mTrafficCounters;
}

bool DataChannel::EnsureValidStream(ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mConnection);
  if (mConnection && mStream != INVALID_STREAM) {
    return true;
  }
  aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  return false;
}

nsresult DataChannelOnMessageAvailable::Run() {
  MOZ_ASSERT(NS_IsMainThread());

  // Note: calling the listeners can indirectly cause the listeners to be
  // made available for GC (by removing event listeners), especially for
  // OnChannelClosed().  We hold a ref to the Channel and the listener
  // while calling this.
  switch (mType) {
    case EventType::OnDataString:
    case EventType::OnDataBinary:
      if (!mChannel->mListener) {
        DC_ERROR(("DataChannelOnMessageAvailable (%s) with null Listener!",
                  ToString(mType)));
        return NS_OK;
      }

      if (mChannel->GetReadyState() == DataChannelState::Closed ||
          mChannel->GetReadyState() == DataChannelState::Closing) {
        // Closed by JS, probably
        return NS_OK;
      }

      if (mType == EventType::OnDataString) {
        mChannel->mListener->OnMessageAvailable(mChannel->mContext, mData);
      } else {
        mChannel->mListener->OnBinaryMessageAvailable(mChannel->mContext,
                                                      mData);
      }
      break;
    case EventType::OnDisconnected:
      // If we've disconnected, make sure we close all the streams - from
      // mainthread!
      if (mConnection->mListener) {
        mConnection->mListener->NotifySctpClosed();
      }
      mConnection->CloseAll();
      break;
    case EventType::OnConnection:
      if (mConnection->mListener) {
        mConnection->mListener->NotifySctpConnected();
      }
      break;
  }
  return NS_OK;
}

}  // namespace mozilla
