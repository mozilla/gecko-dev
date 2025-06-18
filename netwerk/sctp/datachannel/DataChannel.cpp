/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#if !defined(__Userspace_os_Windows)
#  include <arpa/inet.h>
#endif
// usrsctp.h expects to have errno definitions prior to its inclusion.
#include <errno.h>

#define SCTP_DEBUG 1
#define SCTP_STDINT_INCLUDE <stdint.h>

#ifdef _MSC_VER
// Disable "warning C4200: nonstandard extension used : zero-sized array in
//          struct/union"
// ...which the third-party file usrsctp.h runs afoul of.
#  pragma warning(push)
#  pragma warning(disable : 4200)
#endif

#include "usrsctp.h"

#ifdef _MSC_VER
#  pragma warning(pop)
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
#include "mozilla/media/MediaUtils.h"
#ifdef MOZ_PEERCONNECTION
#  include "transport/runnable_utils.h"
#  include "jsapi/MediaTransportHandler.h"
#  include "mediapacket.h"
#endif

#include "DataChannel.h"
#include "DataChannelLog.h"
#include "DataChannelProtocol.h"

// Let us turn on and off important assertions in non-debug builds
#ifdef DEBUG
#  define ASSERT_WEBRTC(x) MOZ_ASSERT((x))
#elif defined(MOZ_WEBRTC_ASSERT_ALWAYS)
#  define ASSERT_WEBRTC(x) \
    do {                   \
      if (!(x)) {          \
        MOZ_CRASH();       \
      }                    \
    } while (0)
#endif

namespace mozilla {

LazyLogModule gDataChannelLog("DataChannel");
static LazyLogModule gSCTPLog("SCTP");

#define SCTP_LOG(args) \
  MOZ_LOG(mozilla::gSCTPLog, mozilla::LogLevel::Debug, args)

static void debug_printf(const char* format, ...) {
  va_list ap;
  char buffer[1024];

  if (MOZ_LOG_TEST(gSCTPLog, LogLevel::Debug)) {
    va_start(ap, format);
#ifdef _WIN32
    if (vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, ap) > 0) {
#else
    if (VsprintfLiteral(buffer, format, ap) > 0) {
#endif
      SCTP_LOG(("%s", buffer));
    }
    va_end(ap);
  }
}

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

static constexpr const char* ToString(
    DataChannelOnMessageAvailable::EventType type) {
  switch (type) {
    case DataChannelOnMessageAvailable::EventType::OnConnection:
      return "ON_CONNECTION";
    case DataChannelOnMessageAvailable::EventType::OnDisconnected:
      return "ON_DISCONNECTED";
    case DataChannelOnMessageAvailable::EventType::OnChannelCreated:
      return "ON_CHANNEL_CREATED";
    case DataChannelOnMessageAvailable::EventType::OnDataString:
      return "ON_DATA_STRING";
    case DataChannelOnMessageAvailable::EventType::OnDataBinary:
      return "ON_DATA_BINARY";
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

class DataChannelRegistry {
 public:
  static uintptr_t Register(DataChannelConnection* aConnection) {
    StaticMutexAutoLock lock(sInstanceMutex);
    uintptr_t result = EnsureInstance()->RegisterImpl(aConnection);
    DC_DEBUG(
        ("Registering connection %p as ulp %p", aConnection, (void*)result));
    return result;
  }

  static void Deregister(uintptr_t aId) {
    std::unique_ptr<DataChannelRegistry> maybeTrash;

    {
      StaticMutexAutoLock lock(sInstanceMutex);
      DC_DEBUG(("Deregistering connection ulp = %p", (void*)aId));
      if (NS_WARN_IF(!Instance())) {
        return;
      }
      Instance()->DeregisterImpl(aId);
      if (Instance()->Empty()) {
        // Unset singleton inside mutex lock, but don't call Shutdown until we
        // unlock, since that involves calling into libusrsctp, which invites
        // deadlock.
        maybeTrash = std::move(Instance());
      }
    }
  }

  static RefPtr<DataChannelConnection> Lookup(uintptr_t aId) {
    StaticMutexAutoLock lock(sInstanceMutex);
    if (NS_WARN_IF(!Instance())) {
      return nullptr;
    }
    return Instance()->LookupImpl(aId);
  }

  virtual ~DataChannelRegistry() {
    MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());

    if (NS_WARN_IF(!mConnections.empty())) {
      MOZ_DIAGNOSTIC_CRASH("mConnections not empty");
      mConnections.clear();
    }

    MOZ_DIAGNOSTIC_ASSERT(!Instance());
    DeinitUsrSctp();
  }

 private:
  // This is a singleton class, so don't let just anyone create one of these
  DataChannelRegistry() {
    MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
    mShutdownBlocker = media::ShutdownBlockingTicket::Create(
        u"DataChannelRegistry::mShutdownBlocker"_ns,
        NS_LITERAL_STRING_FROM_CSTRING(__FILE__), __LINE__);
    MOZ_DIAGNOSTIC_ASSERT(!Instance());
    InitUsrSctp();
  }

  static std::unique_ptr<DataChannelRegistry>& Instance() {
    static std::unique_ptr<DataChannelRegistry> sRegistry;
    return sRegistry;
  }

  static std::unique_ptr<DataChannelRegistry>& EnsureInstance() {
    MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
    if (!Instance()) {
      Instance().reset(new DataChannelRegistry());
    }
    return Instance();
  }

  uintptr_t RegisterImpl(DataChannelConnection* aConnection) {
    MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
    mConnections.emplace(mNextId, aConnection);
    return mNextId++;
  }

  void DeregisterImpl(uintptr_t aId) {
    MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
    size_t removed = mConnections.erase(aId);
    mozilla::Unused << removed;
    MOZ_DIAGNOSTIC_ASSERT(removed);
  }

  bool Empty() const { return mConnections.empty(); }

  RefPtr<DataChannelConnection> LookupImpl(uintptr_t aId) {
    auto it = mConnections.find(aId);
    if (NS_WARN_IF(it == mConnections.end())) {
      DC_DEBUG(("Can't find connection ulp %p", (void*)aId));
      return nullptr;
    }
    return it->second;
  }

  static int SctpDtlsOutput(void* addr, void* buffer, size_t length,
                            uint8_t tos, uint8_t set_df) {
    uintptr_t id = reinterpret_cast<uintptr_t>(addr);
    RefPtr<DataChannelConnection> connection = DataChannelRegistry::Lookup(id);
    if (NS_WARN_IF(!connection) || connection->InShutdown()) {
      return 0;
    }
    return connection->SctpDtlsOutput(addr, buffer, length, tos, set_df);
  }

  void InitUsrSctp() {
    MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
#ifndef MOZ_PEERCONNECTION
    MOZ_CRASH("Trying to use SCTP/DTLS without dom/media/webrtc/transport");
#endif

    DC_DEBUG(("Calling usrsctp_init %p", this));

    MOZ_DIAGNOSTIC_ASSERT(!sInitted);
    usrsctp_init(0, DataChannelRegistry::SctpDtlsOutput, debug_printf);
    sInitted = true;

    // Set logging to SCTP:LogLevel::Debug to get SCTP debugs
    if (MOZ_LOG_TEST(gSCTPLog, LogLevel::Debug)) {
      usrsctp_sysctl_set_sctp_debug_on(SCTP_DEBUG_ALL);
    }

    // Do not send ABORTs in response to INITs (1).
    // Do not send ABORTs for received Out of the Blue packets (2).
    usrsctp_sysctl_set_sctp_blackhole(2);

    // Disable the Explicit Congestion Notification extension (currently not
    // supported by the Firefox code)
    usrsctp_sysctl_set_sctp_ecn_enable(0);

    // Enable interleaving messages for different streams (incoming)
    // See: https://tools.ietf.org/html/rfc6458#section-8.1.20
    usrsctp_sysctl_set_sctp_default_frag_interleave(2);

    // Disabling authentication and dynamic address reconfiguration as neither
    // of them are used for data channel and only result in additional code
    // paths being used.
    usrsctp_sysctl_set_sctp_asconf_enable(0);
    usrsctp_sysctl_set_sctp_auth_enable(0);
  }

  void DeinitUsrSctp() {
    MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
    MOZ_DIAGNOSTIC_ASSERT(sInitted);
    DC_DEBUG(("Calling usrsctp_finish %p", this));
    usrsctp_finish();
    sInitted = false;
  }

  uintptr_t mNextId = 1;
  std::map<uintptr_t, RefPtr<DataChannelConnection>> mConnections;
  UniquePtr<media::ShutdownBlockingTicket> mShutdownBlocker;
  static StaticMutex sInstanceMutex MOZ_UNANNOTATED;
  static bool sInitted;
};

bool DataChannelRegistry::sInitted = false;

StaticMutex DataChannelRegistry::sInstanceMutex;

OutgoingMsg::OutgoingMsg(nsACString&& aData,
                         const DataChannelMessageMetadata& aMetadata)
    : mData(std::move(aData)), mMetadata(aMetadata) {}

void OutgoingMsg::Advance(size_t offset) {
  mPos += offset;
  if (mPos > mData.Length()) {
    mPos = mData.Length();
  }
}

static int receive_cb(struct socket* sock, union sctp_sockstore addr,
                      void* data, size_t datalen, struct sctp_rcvinfo rcv,
                      int flags, void* ulp_info) {
  DC_DEBUG(("In receive_cb, ulp_info=%p", ulp_info));
  uintptr_t id = reinterpret_cast<uintptr_t>(ulp_info);
  RefPtr<DataChannelConnection> connection = DataChannelRegistry::Lookup(id);
  if (!connection) {
    // Unfortunately, we can get callbacks after calling
    // usrsctp_close(socket), so we need to simply ignore them if we've
    // already killed the DataChannelConnection object
    DC_DEBUG((
        "Ignoring receive callback for terminated Connection ulp=%p, %zu bytes",
        ulp_info, datalen));
    return 0;
  }
  return connection->ReceiveCallback(sock, data, datalen, rcv, flags);
}

static RefPtr<DataChannelConnection> GetConnectionFromSocket(
    struct socket* sock) {
  struct sockaddr* addrs = nullptr;
  int naddrs = usrsctp_getladdrs(sock, 0, &addrs);
  if (naddrs <= 0 || addrs[0].sa_family != AF_CONN) {
    return nullptr;
  }
  // usrsctp_getladdrs() returns the addresses bound to this socket, which
  // contains the SctpDataMediaChannel* as sconn_addr.  Read the pointer,
  // then free the list of addresses once we have the pointer.  We only open
  // AF_CONN sockets, and they should all have the sconn_addr set to the
  // pointer that created them, so [0] is as good as any other.
  struct sockaddr_conn* sconn =
      reinterpret_cast<struct sockaddr_conn*>(&addrs[0]);
  uintptr_t id = reinterpret_cast<uintptr_t>(sconn->sconn_addr);
  RefPtr<DataChannelConnection> connection = DataChannelRegistry::Lookup(id);
  usrsctp_freeladdrs(addrs);

  return connection;
}

// Called when the buffer empties to the threshold value.  This is called
// from OnSctpPacketReceived() through the sctp stack.
int DataChannelConnection::OnThresholdEvent(struct socket* sock,
                                            uint32_t sb_free, void* ulp_info) {
  RefPtr<DataChannelConnection> connection = GetConnectionFromSocket(sock);
  connection->mLock.AssertCurrentThreadOwns();
  if (connection) {
    connection->SendDeferredMessages();
  } else {
    DC_ERROR(("Can't find connection for socket %p", sock));
  }
  return 0;
}

DataChannelConnection::~DataChannelConnection() {
  DC_DEBUG(("Deleting DataChannelConnection %p", (void*)this));
  // This may die on the MainThread, or on the STS thread, or on an
  // sctp thread if we were in a callback when the DOM side shut things down.
  ASSERT_WEBRTC(mState == DataChannelConnectionState::Closed);
  MOZ_ASSERT(mPending.empty());

  if (!IsSTSThread()) {
    // We may be on MainThread *or* on an sctp thread (being called from
    // receive_cb() or SctpDtlsOutput())
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
  ASSERT_WEBRTC(NS_IsMainThread());
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

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  auto self = DataChannelRegistry::Lookup(mId);
  MOZ_DIAGNOSTIC_ASSERT(self);
  MOZ_DIAGNOSTIC_ASSERT(this == self.get());
#endif
  // Finish Destroy on STS thread to avoid bug 876167 - once that's fixed,
  // the usrsctp_close() calls can move back here (and just proxy the
  // disconnect_all())
  RUN_ON_THREAD(mSTS,
                WrapRunnable(RefPtr<DataChannelConnection>(this),
                             &DataChannelConnection::DestroyOnSTS),
                NS_DISPATCH_NORMAL);

  // All existing callbacks have refs to DataChannelConnection - however,
  // we need to handle their destroying the object off mainthread/STS

  // nsDOMDataChannel objects have refs to DataChannels that have refs to us
}

void DataChannelConnection::DestroyOnSTS() {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());

  if (mSocket) usrsctp_close(mSocket);
  mSocket = nullptr;

  usrsctp_deregister_address(reinterpret_cast<void*>(mId));
  DC_DEBUG(
      ("Deregistered %p from the SCTP stack.", reinterpret_cast<void*>(mId)));

  // We do this at the very last because it might tear down usrsctp, and we
  // don't want that to happen before the usrsctp_close call above
  Dispatch(NS_NewRunnableFunction(
      "DataChannelConnection::Destroy",
      [this, self = RefPtr<DataChannelConnection>(this)]() {
        DataChannelRegistry::Deregister(mId);
      }));
}

Maybe<RefPtr<DataChannelConnection>> DataChannelConnection::Create(
    DataChannelConnection::DataConnectionListener* aListener,
    nsISerialEventTarget* aTarget, MediaTransportHandler* aHandler,
    const uint16_t aLocalPort, const uint16_t aNumStreams,
    const Maybe<uint64_t>& aMaxMessageSize) {
  ASSERT_WEBRTC(NS_IsMainThread());

  RefPtr<DataChannelConnection> connection = new DataChannelConnection(
      aListener, aTarget, aHandler);  // Walks into a bar
  return connection->Init(aLocalPort, aNumStreams, aMaxMessageSize)
             ? Some(connection)
             : Nothing();
}

DataChannelConnection::DataChannelConnection(
    DataChannelConnection::DataConnectionListener* aListener,
    nsISerialEventTarget* aTarget, MediaTransportHandler* aHandler)
    : NeckoTargetHolder(aTarget),
      mLock("netwerk::sctp::DataChannelConnection"),
      mListener(aListener),
      mTransportHandler(aHandler) {
  DC_VERBOSE(("Constructor DataChannelConnection=%p, listener=%p", this,
              mListener.get()));
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  mShutdown = false;
#endif
}

bool DataChannelConnection::Init(const uint16_t aLocalPort,
                                 const uint16_t aNumStreams,
                                 const Maybe<uint64_t>& aMaxMessageSize) {
  ASSERT_WEBRTC(NS_IsMainThread());

  struct sctp_initmsg initmsg = {};
  struct sctp_assoc_value av = {};
  struct sctp_event event = {};
  socklen_t len;

  uint16_t event_types[] = {
      SCTP_ASSOC_CHANGE,          SCTP_PEER_ADDR_CHANGE,
      SCTP_REMOTE_ERROR,          SCTP_SHUTDOWN_EVENT,
      SCTP_ADAPTATION_INDICATION, SCTP_PARTIAL_DELIVERY_EVENT,
      SCTP_SEND_FAILED_EVENT,     SCTP_STREAM_RESET_EVENT,
      SCTP_STREAM_CHANGE_EVENT};

  SetMaxMessageSize(aMaxMessageSize.isSome(), aMaxMessageSize.valueOr(0));

  mId = DataChannelRegistry::Register(this);

  // XXX FIX! make this a global we get once
  // Find the STS thread
  nsresult rv;
  mSTS = mozilla::components::SocketTransport::Service(&rv);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  socklen_t buf_size = 1024 * 1024;

  // Open sctp with a callback
  if ((mSocket = usrsctp_socket(AF_CONN, SOCK_STREAM, IPPROTO_SCTP, receive_cb,
                                &DataChannelConnection::OnThresholdEvent,
                                usrsctp_sysctl_get_sctp_sendspace() / 2,
                                reinterpret_cast<void*>(mId))) == nullptr) {
    goto error_cleanup;
  }

  if (usrsctp_setsockopt(mSocket, SOL_SOCKET, SO_RCVBUF, (const void*)&buf_size,
                         sizeof(buf_size)) < 0) {
    DC_ERROR(("Couldn't change receive buffer size on SCTP socket"));
    goto error_cleanup;
  }
  if (usrsctp_setsockopt(mSocket, SOL_SOCKET, SO_SNDBUF, (const void*)&buf_size,
                         sizeof(buf_size)) < 0) {
    DC_ERROR(("Couldn't change send buffer size on SCTP socket"));
    goto error_cleanup;
  }

  // Make non-blocking for bind/connect.  SCTP over UDP defaults to non-blocking
  // in associations for normal IO
  if (usrsctp_set_non_blocking(mSocket, 1) < 0) {
    DC_ERROR(("Couldn't set non_blocking on SCTP socket"));
    // We can't handle connect() safely if it will block, not that this will
    // even happen.
    goto error_cleanup;
  }

  // Make sure when we close the socket, make sure it doesn't call us back
  // again! This would cause it try to use an invalid DataChannelConnection
  // pointer
  struct linger l;
  l.l_onoff = 1;
  l.l_linger = 0;
  if (usrsctp_setsockopt(mSocket, SOL_SOCKET, SO_LINGER, (const void*)&l,
                         (socklen_t)sizeof(struct linger)) < 0) {
    DC_ERROR(("Couldn't set SO_LINGER on SCTP socket"));
    // unsafe to allow it to continue if this fails
    goto error_cleanup;
  }

  // XXX Consider disabling this when we add proper SDP negotiation.
  // We may want to leave enabled for supporting 'cloning' of SDP offers, which
  // implies re-use of the same pseudo-port number, or forcing a renegotiation.
  {
    const int option_value = 1;
    if (usrsctp_setsockopt(mSocket, IPPROTO_SCTP, SCTP_REUSE_PORT,
                           (const void*)&option_value,
                           (socklen_t)sizeof(option_value)) < 0) {
      DC_WARN(("Couldn't set SCTP_REUSE_PORT on SCTP socket"));
    }
    if (usrsctp_setsockopt(mSocket, IPPROTO_SCTP, SCTP_NODELAY,
                           (const void*)&option_value,
                           (socklen_t)sizeof(option_value)) < 0) {
      DC_WARN(("Couldn't set SCTP_NODELAY on SCTP socket"));
    }
  }

  // Set explicit EOR
  {
    const int option_value = 1;
    if (usrsctp_setsockopt(mSocket, IPPROTO_SCTP, SCTP_EXPLICIT_EOR,
                           (const void*)&option_value,
                           (socklen_t)sizeof(option_value)) < 0) {
      DC_ERROR(("*** failed to enable explicit EOR mode %d", errno));
      goto error_cleanup;
    }
  }

  // Enable ndata
  av.assoc_id = SCTP_FUTURE_ASSOC;
  av.assoc_value = 1;
  if (usrsctp_setsockopt(mSocket, IPPROTO_SCTP, SCTP_INTERLEAVING_SUPPORTED,
                         &av, (socklen_t)sizeof(struct sctp_assoc_value)) < 0) {
    DC_ERROR(("*** failed enable ndata errno %d", errno));
    goto error_cleanup;
  }

  av.assoc_id = SCTP_ALL_ASSOC;
  av.assoc_value = SCTP_ENABLE_RESET_STREAM_REQ | SCTP_ENABLE_CHANGE_ASSOC_REQ;
  if (usrsctp_setsockopt(mSocket, IPPROTO_SCTP, SCTP_ENABLE_STREAM_RESET, &av,
                         (socklen_t)sizeof(struct sctp_assoc_value)) < 0) {
    DC_ERROR(("*** failed enable stream reset errno %d", errno));
    goto error_cleanup;
  }

  /* Enable the events of interest. */
  event.se_assoc_id = SCTP_ALL_ASSOC;
  event.se_on = 1;
  for (unsigned short event_type : event_types) {
    event.se_type = event_type;
    if (usrsctp_setsockopt(mSocket, IPPROTO_SCTP, SCTP_EVENT, &event,
                           sizeof(event)) < 0) {
      DC_ERROR(("*** failed setsockopt SCTP_EVENT errno %d", errno));
      goto error_cleanup;
    }
  }

  len = sizeof(initmsg);
  if (usrsctp_getsockopt(mSocket, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, &len) <
      0) {
    DC_ERROR(("*** failed getsockopt SCTP_INITMSG"));
    goto error_cleanup;
  }
  DC_DEBUG(("Setting number of SCTP streams to %u, was %u/%u", aNumStreams,
            initmsg.sinit_num_ostreams, initmsg.sinit_max_instreams));
  initmsg.sinit_num_ostreams = aNumStreams;
  initmsg.sinit_max_instreams = MAX_NUM_STREAMS;
  if (usrsctp_setsockopt(mSocket, IPPROTO_SCTP, SCTP_INITMSG, &initmsg,
                         (socklen_t)sizeof(initmsg)) < 0) {
    DC_ERROR(("*** failed setsockopt SCTP_INITMSG, errno %d", errno));
    goto error_cleanup;
  }

  mSTS->Dispatch(
      NS_NewRunnableFunction("DataChannelConnection::Init", [id = mId]() {
        usrsctp_register_address(reinterpret_cast<void*>(id));
        DC_DEBUG(("Registered %p within the SCTP stack.",
                  reinterpret_cast<void*>(id)));
      }));

  return true;

error_cleanup:
  usrsctp_close(mSocket);
  mSocket = nullptr;
  DataChannelRegistry::Deregister(mId);
  return false;
}

// Only called on MainThread, mMaxMessageSize is read on other threads
void DataChannelConnection::SetMaxMessageSize(bool aMaxMessageSizeSet,
                                              uint64_t aMaxMessageSize) {
  ASSERT_WEBRTC(NS_IsMainThread());

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
  ASSERT_WEBRTC(NS_IsMainThread());
  return mMaxMessageSize;
}

void DataChannelConnection::AppendStatsToReport(
    const UniquePtr<dom::RTCStatsCollection>& aReport,
    const DOMHighResTimeStamp aTimestamp) const {
  ASSERT_WEBRTC(NS_IsMainThread());
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

#ifdef MOZ_PEERCONNECTION

bool DataChannelConnection::ConnectToTransport(const std::string& aTransportId,
                                               const bool aClient,
                                               const uint16_t aLocalPort,
                                               const uint16_t aRemotePort) {
  MutexAutoLock lock(mLock);
  ASSERT_WEBRTC(NS_IsMainThread());

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
          MutexAutoLock lock(mLock);
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
  ASSERT_WEBRTC(IsSTSThread());
  MutexAutoLock lock(mLock);
  if (mTransportId == aTransportId) {
    // Nothing to do!
    return;
  }

  mTransportId = aTransportId;

  if (!mConnectedToTransportHandler) {
    mPacketReceivedListener =
        mTransportHandler->GetSctpPacketReceived().Connect(
            mSTS, this, &DataChannelConnection::SctpDtlsInput);
    mStateChangeListener = mTransportHandler->GetStateChange().Connect(
        mSTS, this, &DataChannelConnection::TransportStateChange);
    mConnectedToTransportHandler = true;
  }
  // SignalStateChange() doesn't call you with the initial state
  if (mTransportHandler->GetState(mTransportId, false) ==
      TransportLayer::TS_OPEN) {
    DC_DEBUG(("Setting transport signals, dtls already open"));
    CompleteConnect();
  } else {
    DC_DEBUG(("Setting transport signals, dtls not open yet"));
  }
}

void DataChannelConnection::TransportStateChange(
    const std::string& aTransportId, TransportLayer::State aState) {
  ASSERT_WEBRTC(IsSTSThread());
  MutexAutoLock lock(mLock);
  if (aTransportId == mTransportId) {
    if (aState == TransportLayer::TS_OPEN) {
      DC_DEBUG(("Transport is open!"));
      CompleteConnect();
    } else if (aState == TransportLayer::TS_CLOSED ||
               aState == TransportLayer::TS_NONE ||
               aState == TransportLayer::TS_ERROR) {
      DC_DEBUG(("Transport is closed!"));
      Stop();
    }
  }
}

void DataChannelConnection::CompleteConnect() {
  DC_DEBUG(("dtls open"));
  ASSERT_WEBRTC(IsSTSThread());
  mLock.AssertCurrentThreadOwns();
  if (mSctpConfigured) {
    // mSocket could have been closed by an error or for some other reason,
    // don't open an opportunity to reinit.
    return;
  }

  mSctpConfigured = true;

  struct sockaddr_conn addr = {};
  addr.sconn_family = AF_CONN;
#  if defined(__Userspace_os_Darwin)
  addr.sconn_len = sizeof(addr);
#  endif
  addr.sconn_port = htons(mLocalPort);
  addr.sconn_addr = reinterpret_cast<void*>(mId);

  DC_DEBUG(("Calling usrsctp_bind"));
  int r = usrsctp_bind(mSocket, reinterpret_cast<struct sockaddr*>(&addr),
                       sizeof(addr));
  if (r < 0) {
    DC_ERROR(("usrsctp_bind failed: %d", r));
  } else {
    // This is the remote addr
    addr.sconn_port = htons(mRemotePort);
    DC_DEBUG(("Calling usrsctp_connect"));
    r = usrsctp_connect(mSocket, reinterpret_cast<struct sockaddr*>(&addr),
                        sizeof(addr));
    if (r >= 0 || errno == EINPROGRESS) {
      struct sctp_paddrparams paddrparams = {};
      socklen_t opt_len;

      memcpy(&paddrparams.spp_address, &addr, sizeof(struct sockaddr_conn));
      opt_len = (socklen_t)sizeof(struct sctp_paddrparams);
      r = usrsctp_getsockopt(mSocket, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS,
                             &paddrparams, &opt_len);
      if (r < 0) {
        DC_ERROR(("usrsctp_getsockopt failed: %d", r));
      } else {
        // This field is misnamed. |spp_pathmtu| represents the maximum
        // _payload_ size in libusrsctp. So:
        // 1280 (a reasonable IPV6 MTU according to RFC 8831)
        //  -12 (sctp header)
        //  -24 (GCM sipher)
        //  -13 (DTLS record header)
        //   -8 (UDP header)
        //   -4 (TURN ChannelData)
        //  -40 (IPV6 header)
        // = 1179
        // We could further restrict this, because RFC 8831 suggests a starting
        // IPV4 path MTU of 1200, which would lead to a value of 1115.
        // I suspect that in practice the path MTU for IPV4 is substantially
        // larger than 1200.
        paddrparams.spp_pathmtu = 1179;
        paddrparams.spp_flags &= ~SPP_PMTUD_ENABLE;
        paddrparams.spp_flags |= SPP_PMTUD_DISABLE;
        opt_len = (socklen_t)sizeof(struct sctp_paddrparams);
        r = usrsctp_setsockopt(mSocket, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS,
                               &paddrparams, opt_len);
        if (r < 0) {
          DC_ERROR(("usrsctp_getsockopt failed: %d", r));
        } else {
          DC_ERROR(("usrsctp: PMTUD disabled, MTU set to %u",
                    paddrparams.spp_pathmtu));
        }
      }
    }
    if (r < 0) {
      if (errno == EINPROGRESS) {
        // non-blocking
        return;
      }
      DC_ERROR(("usrsctp_connect failed: %d", errno));
      SetState(DataChannelConnectionState::Closed);
    } else {
      // We fire ON_CONNECTION via SCTP_COMM_UP when we get that
      return;
    }
  }
  // Note: currently this doesn't actually notify the application
  Dispatch(do_AddRef(new DataChannelOnMessageAvailable(
      DataChannelOnMessageAvailable::EventType::OnConnection, this)));
}

// Process any pending Opens
void DataChannelConnection::ProcessQueuedOpens() {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  mLock.AssertCurrentThreadOwns();
  std::set<RefPtr<DataChannel>> temp(std::move(mPending));
  for (auto channel : temp) {
    DC_DEBUG(("Processing queued open for %p (%u)", channel.get(),
              channel->mStream));
    OpenFinish(channel);  // may end up back in mPending
  }
}

void DataChannelConnection::SctpDtlsInput(const std::string& aTransportId,
                                          const MediaPacket& packet) {
  MutexAutoLock lock(mLock);
  if ((packet.type() != MediaPacket::SCTP) || (mTransportId != aTransportId)) {
    return;
  }

  if (MOZ_LOG_TEST(gSCTPLog, LogLevel::Debug)) {
    char* buf;

    if ((buf = usrsctp_dumppacket((void*)packet.data(), packet.len(),
                                  SCTP_DUMP_INBOUND)) != nullptr) {
      SCTP_LOG(("%s", buf));
      usrsctp_freedumpbuffer(buf);
    }
  }
  // Pass the data to SCTP
  usrsctp_conninput(reinterpret_cast<void*>(mId), packet.data(), packet.len(),
                    0);
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

int DataChannelConnection::SctpDtlsOutput(void* addr, void* buffer,
                                          size_t length, uint8_t tos,
                                          uint8_t set_df) {
  if (MOZ_LOG_TEST(gSCTPLog, LogLevel::Debug)) {
    char* buf;

    if ((buf = usrsctp_dumppacket(buffer, length, SCTP_DUMP_OUTBOUND)) !=
        nullptr) {
      SCTP_LOG(("%s", buf));
      usrsctp_freedumpbuffer(buf);
    }
  }

  // We're async proxying even if on the STSThread because this is called
  // with internal SCTP locks held in some cases (such as in usrsctp_connect()).
  // SCTP has an option for Apple, on IP connections only, to release at least
  // one of the locks before calling a packet output routine; with changes to
  // the underlying SCTP stack this might remove the need to use an async proxy.
  std::unique_ptr<MediaPacket> packet(new MediaPacket);
  packet->SetType(MediaPacket::SCTP);
  packet->Copy(static_cast<const uint8_t*>(buffer), length);

  SendPacket(std::move(packet));
  return 0;  // cheat!  Packets can always be dropped later anyways
}
#endif

DataChannel* DataChannelConnection::FindChannelByStream(uint16_t stream) {
  return mChannels.Get(stream).get();
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

uint32_t DataChannelConnection::UpdateCurrentStreamIndex() {
  RefPtr<DataChannel> channel = mChannels.GetNextChannel(mCurrentStream);
  if (!channel) {
    mCurrentStream = 0;
  } else {
    mCurrentStream = channel->mStream;
  }
  return mCurrentStream;
}

uint32_t DataChannelConnection::GetCurrentStreamIndex() {
  if (!mChannels.Get(mCurrentStream)) {
    // The stream muse have been removed, reset
    DC_DEBUG(("Reset mCurrentChannel"));
    mCurrentStream = 0;
  }
  return mCurrentStream;
}

bool DataChannelConnection::RaiseStreamLimitTo(uint16_t aNewLimit) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  mLock.AssertCurrentThreadOwns();
  if (GetState() == DataChannelConnectionState::Closed) {
    // Smile and nod, could end up here via a dispatch
    return true;
  }

  if (mNegotiatedIdLimit == MAX_NUM_STREAMS) {
    // We're already maxed out!
    return false;
  }

  if (aNewLimit <= mNegotiatedIdLimit) {
    // We already have enough
    return true;
  }

  if (aNewLimit > MAX_NUM_STREAMS) {
    // Hard cap: if someone calls again asking for this much, we'll return
    // false above
    aNewLimit = MAX_NUM_STREAMS;
  }

  struct sctp_status status = {};
  socklen_t len = (socklen_t)sizeof(struct sctp_status);
  if (usrsctp_getsockopt(mSocket, IPPROTO_SCTP, SCTP_STATUS, &status, &len) <
      0) {
    DC_ERROR(("***failed: getsockopt SCTP_STATUS"));
    return false;
  }
  const uint16_t outStreamsNeeded =
      aNewLimit - mNegotiatedIdLimit;  // number to add

  // Note: if multiple channel opens happen when we don't have enough space,
  // we'll call RaiseStreamLimitTo() multiple times
  struct sctp_add_streams sas = {};
  sas.sas_instrms = 0;
  sas.sas_outstrms = outStreamsNeeded; /* XXX error handling */
  // Doesn't block, we get an event when it succeeds or fails
  if (usrsctp_setsockopt(mSocket, IPPROTO_SCTP, SCTP_ADD_STREAMS, &sas,
                         (socklen_t)sizeof(struct sctp_add_streams)) < 0) {
    if (errno == EALREADY) {
      // Uhhhh, ok?
      DC_DEBUG(("Already have %u output streams", outStreamsNeeded));
      return true;
    }

    DC_ERROR(("***failed: setsockopt ADD errno=%d", errno));
    return false;
  }
  DC_DEBUG(("Requested %u more streams", outStreamsNeeded));
  // We add to mNegotiatedIdLimit when we get a SCTP_STREAM_CHANGE_EVENT and the
  // values are larger than mNegotiatedIdLimit
  return true;
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

// XXX This should use a separate thread (outbound queue) which should
// select() to know when to *try* to send data to the socket again.
// Alternatively, it can use a timeout, but that's guaranteed to be wrong
// (just not sure in what direction).  We could re-implement NSPR's
// PR_POLL_WRITE/etc handling... with a lot of work.

// Better yet, use the SCTP stack's notifications on buffer state to avoid
// filling the SCTP's buffers.

// returns if we're still blocked (true)
bool DataChannelConnection::SendDeferredMessages() {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  RefPtr<DataChannel> channel;  // we may null out the refs to this

  mLock.AssertCurrentThreadOwns();

  DC_DEBUG(("SendDeferredMessages called, pending type: %s",
            ToString(mPendingType)));
  if (mPendingType == PendingType::None) {
    return false;
  }

  // Send pending control messages
  // Note: If ndata is not active, check if DCEP messages are currently
  // outstanding. These need to
  //       be sent first before other streams can be used for sending.
  if (!mBufferedControl.IsEmpty() &&
      (mSendInterleaved || mPendingType == PendingType::Dcep)) {
    if (SendBufferedMessages(mBufferedControl, nullptr)) {
      return true;
    }

    // Note: There may or may not be pending data messages
    mPendingType = PendingType::Data;
  }

  bool blocked = false;
  uint32_t i = GetCurrentStreamIndex();
  uint32_t end = i;
  do {
    channel = mChannels.Get(i);
    if (!channel) {
      continue;
    }

    // Should already be cleared if closing/closed
    if (channel->mBufferedData.IsEmpty()) {
      i = UpdateCurrentStreamIndex();
      continue;
    }

    // Send buffered data messages
    // Warning: This will fail in case ndata is inactive and a previously
    //          deallocated data channel has not been closed properly. If you
    //          ever see that no messages can be sent on any channel, this is
    //          likely the cause (an explicit EOR message partially sent whose
    //          remaining chunks are still being waited for).
    size_t written = 0;
    blocked = SendBufferedMessages(channel->mBufferedData, &written);
    if (written) {
      channel->DecrementBufferedAmount(written);
    }

    // Update current stream index
    // Note: If ndata is not active, the outstanding data messages on this
    //       stream need to be sent first before other streams can be used for
    //       sending.
    if (mSendInterleaved || !blocked) {
      i = UpdateCurrentStreamIndex();
    }
  } while (!blocked && i != end);

  if (!blocked) {
    mPendingType =
        mBufferedControl.IsEmpty() ? PendingType::None : PendingType::Dcep;
  }
  return blocked;
}

// buffer MUST have at least one item!
// returns if we're still blocked (true)
bool DataChannelConnection::SendBufferedMessages(nsTArray<OutgoingMsg>& buffer,
                                                 size_t* aWritten) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  mLock.AssertCurrentThreadOwns();
  do {
    // Re-send message
    const int error = SendMsgInternal(buffer[0], aWritten);
    switch (error) {
      case 0:
        buffer.RemoveElementAt(0);
        break;
      case EAGAIN:
#if (EAGAIN != EWOULDBLOCK)
      case EWOULDBLOCK:
#endif
        return true;
      default:
        buffer.RemoveElementAt(0);
        DC_ERROR(("error on sending: %d", error));
        break;
    }
  } while (!buffer.IsEmpty());

  return false;
}

// Caller must ensure that length <= SIZE_MAX
void DataChannelConnection::HandleOpenRequestMessage(
    const struct rtcweb_datachannel_open_request* req, uint32_t length,
    uint16_t stream) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  RefPtr<DataChannel> channel;
  uint32_t prValue;
  DataChannelReliabilityPolicy prPolicy;

  ASSERT_WEBRTC(!NS_IsMainThread());
  mLock.AssertCurrentThreadOwns();

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
  prValue = ntohl(req->reliability_param);
  bool ordered = !(req->channel_type & 0x80);

  if ((channel = FindChannelByStream(stream))) {
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
      if (prPolicy != channel->mPrPolicy || prValue != channel->mPrValue ||
          ordered != channel->mOrdered) {
        DC_WARN(
            ("external negotiation mismatch with OpenRequest:"
             "channel %u, policy %s/%s, value %u/%u, ordered %d/%d",
             stream, ToString(prPolicy), ToString(channel->mPrPolicy), prValue,
             channel->mPrValue, static_cast<int>(ordered),
             static_cast<int>(channel->mOrdered)));
      }
    }
    return;
  }
  if (stream >= mNegotiatedIdLimit) {
    DC_ERROR(("%s: stream %u out of bounds (%u)", __FUNCTION__, stream,
              mNegotiatedIdLimit));
    return;
  }

  nsCString label(
      nsDependentCSubstring(&req->label[0], ntohs(req->label_length)));
  nsCString protocol(nsDependentCSubstring(
      &req->label[ntohs(req->label_length)], ntohs(req->protocol_length)));

  channel =
      new DataChannel(this, stream, DataChannelState::Open, label, protocol,
                      prPolicy, prValue, ordered, false, nullptr, nullptr);
  mChannels.Insert(channel);

  DC_DEBUG(("%s: sending ON_CHANNEL_CREATED for %s/%s: %u", __FUNCTION__,
            channel->mLabel.get(), channel->mProtocol.get(), stream));
  Dispatch(do_AddRef(new DataChannelOnMessageAvailable(
      DataChannelOnMessageAvailable::EventType::OnChannelCreated, this,
      channel)));

  DC_DEBUG(("%s: deferring sending ON_CHANNEL_OPEN for %p", __FUNCTION__,
            channel.get()));
  channel->AnnounceOpen();

  // Note that any message can be buffered; SendOpenAckMessage may
  // error later than this check.
  const auto error = SendOpenAckMessage(*channel);
  if (error) {
    DC_ERROR(("SendOpenRequest failed, error = %d", error));
    Dispatch(NS_NewRunnableFunction(
        "DataChannelConnection::HandleOpenRequestMessage",
        [channel, connection = RefPtr<DataChannelConnection>(this)]() {
          // Close the channel on failure
          connection->Close(channel);
        }));
    return;
  }
  DeliverQueuedData(channel->mStream);
}

// NOTE: the updated spec from the IETF says we should set in-order until we
// receive an ACK. That would make this code moot.  Keep it for now for
// backwards compatibility.
void DataChannelConnection::DeliverQueuedData(uint16_t stream) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  mLock.AssertCurrentThreadOwns();

  mQueuedData.RemoveElementsBy([stream, this](const auto& dataItem) {
    mLock.AssertCurrentThreadOwns();
    const bool match = dataItem->mStream == stream;
    if (match) {
      DC_DEBUG(("Delivering queued data for stream %u, length %zu", stream,
                dataItem->mData.Length()));
      // Deliver the queued data
      HandleDataMessageChunk(
          dataItem->mData.Elements(), dataItem->mData.Length(), dataItem->mPpid,
          dataItem->mStream, dataItem->mMessageId, dataItem->mFlags);
    }
    return match;
  });
}

// Caller must ensure that length <= SIZE_MAX
void DataChannelConnection::HandleOpenAckMessage(
    const struct rtcweb_datachannel_ack* ack, uint32_t length,
    uint16_t stream) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  DataChannel* channel;

  mLock.AssertCurrentThreadOwns();

  channel = FindChannelByStream(stream);
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

void DataChannelConnection::HandleDataMessageChunk(const void* data,
                                                   size_t length, uint32_t ppid,
                                                   uint16_t stream,
                                                   uint16_t messageId,
                                                   int flags) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  DC_DEBUG(("%s: stream %u, length %zu, ppid %u, message-id %u", __func__,
            stream, length, ppid, messageId));
  DataChannel* channel;

  mLock.AssertCurrentThreadOwns();
  channel = FindChannelByStream(stream);

  // XXX A closed channel may trip this... check
  // NOTE: the updated spec from the IETF says we should set in-order until we
  // receive an ACK. That would make this code moot.  Keep it for now for
  // backwards compatibility.
  if (!channel) {
    // In the updated 0-RTT open case, the sender can send data immediately
    // after Open, and doesn't set the in-order bit (since we don't have a
    // response or ack).  Also, with external negotiation, data can come in
    // before we're told about the external negotiation.  We need to buffer
    // data until either a) Open comes in, if the ordering get messed up,
    // or b) the app tells us this channel was externally negotiated.  When
    // these occur, we deliver the data.

    // Since this is rare and non-performance, keep a single list of queued
    // data messages to deliver once the channel opens.
    DC_DEBUG(("Queuing data for stream %u, length %zu", stream, length));
    // Copies data
    mQueuedData.AppendElement(
        new QueuedDataMessage(stream, ppid, messageId, flags,
                              static_cast<const uint8_t*>(data), length));
    return;
  }

  // Note that `channel->mConnection` is `this`. This is just here to satisfy
  // the thread safety annotations on DataChannel.
  channel->mConnection->mLock.AssertCurrentThreadOwns();

  // RFC8832: "MUST be sent ordered, ... After the DATA_CHANNEL_ACK **or any
  // other message** has been received on the data channel".
  // If the channel was opened on this side, and a message is received, this
  // indicates that the peer has already received the DATA_CHANNEL_ACK, as the
  // channel is ordered initially.
  channel->mWaitingForAck = false;

  const char* type = (ppid == DATA_CHANNEL_PPID_DOMSTRING_PARTIAL ||
                      ppid == DATA_CHANNEL_PPID_DOMSTRING ||
                      ppid == DATA_CHANNEL_PPID_DOMSTRING_EMPTY)
                         ? "string"
                         : "binary";

  auto it = channel->mRecvBuffers.find(messageId);
  if (it != channel->mRecvBuffers.end()) {
    IncomingMsg& msg(it->second);
    if (!ReassembleMessageChunk(msg, data, length, ppid, stream)) {
      FinishClose_s(channel);
      return;
    }

    if (flags & MSG_EOR) {
      DC_DEBUG(
          ("%s: last chunk of multi-chunk %s message, id %u, "
           "stream %u, length %zu",
           __func__, type, messageId, stream, length));
      HandleDataMessage(std::move(msg));
      channel->mRecvBuffers.erase(messageId);
    } else {
      DC_DEBUG(
          ("%s: middle chunk of multi-chunk %s message, id %u, "
           "stream %u, length %zu",
           __func__, type, messageId, stream, length));
    }
    return;
  }

  IncomingMsg msg(ppid, stream);
  if (!ReassembleMessageChunk(msg, data, length, ppid, stream)) {
    FinishClose_s(channel);
    return;
  }

  if (flags & MSG_EOR) {
    DC_DEBUG(
        ("%s: single-chunk %s message, id %u, stream %u, "
         "length %zu",
         __func__, type, messageId, stream, length));
    HandleDataMessage(std::move(msg));
  } else {
    DC_DEBUG(
        ("%s: first chunk of multi-chunk %s message, id %u, "
         "stream %u, length %zu",
         __func__, type, messageId, stream, length));
    channel->mRecvBuffers.insert({messageId, std::move(msg)});
  }
}

void DataChannelConnection::HandleDataMessage(IncomingMsg&& aMsg) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  DataChannelOnMessageAvailable::EventType type;

  size_t data_length = aMsg.GetData().Length();

  mLock.AssertCurrentThreadOwns();

  RefPtr<DataChannel> channel = FindChannelByStream(aMsg.GetStreamId());
  if (!channel) {
    MOZ_ASSERT(false,
               "Wait until OnStreamOpenAck is called before calling "
               "HandleDataMessage!");
    return;
  }

  // Note that `channel->mConnection` is `this`. This is just here to satisfy
  // the thread safety annotations on DataChannel.
  channel->mConnection->mLock.AssertCurrentThreadOwns();

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

  channel->WithTrafficCounters(
      [&data_length](DataChannel::TrafficCounters& counters) {
        counters.mMessagesReceived++;
        counters.mBytesReceived += data_length;
      });

  // Notify onmessage
  DC_DEBUG(
      ("%s: sending %s for %p", __FUNCTION__, ToString(type), channel.get()));
  channel->SendOrQueue(new DataChannelOnMessageAvailable(
      type, this, channel, std::move(aMsg.GetData())));
}

// A sane endpoint should not be fragmenting DCEP, but I think it is allowed
// technically? Use the same chunk reassembly logic that we use for DATA.
void DataChannelConnection::HandleDCEPMessageChunk(const void* buffer,
                                                   size_t length, uint32_t ppid,
                                                   uint16_t stream, int flags) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());

  mLock.AssertCurrentThreadOwns();

  if (!mRecvBuffer.isSome()) {
    mRecvBuffer = Some(IncomingMsg(ppid, stream));
  }

  if (!ReassembleMessageChunk(*mRecvBuffer, buffer, length, ppid, stream)) {
    Stop();
    return;
  }

  if (!(flags & MSG_EOR)) {
    DC_DEBUG(("%s: No EOR, waiting for more chunks", __func__));
    return;
  }

  DC_DEBUG(("%s: EOR, handling", __func__));
  // Last chunk, ready to go.
  HandleDCEPMessage(std::move(*mRecvBuffer));
  mRecvBuffer = Nothing();
}

void DataChannelConnection::HandleDCEPMessage(IncomingMsg&& aMsg) {
  const struct rtcweb_datachannel_open_request* req;
  const struct rtcweb_datachannel_ack* ack;

  mLock.AssertCurrentThreadOwns();

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

void DataChannelConnection::HandleMessageChunk(const void* buffer,
                                               size_t length, uint32_t ppid,
                                               uint16_t stream,
                                               uint16_t messageId, int flags) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());

  switch (ppid) {
    case DATA_CHANNEL_PPID_CONTROL:
      DC_DEBUG(("%s: Got DCEP message size %zu", __func__, length));
      HandleDCEPMessageChunk(buffer, length, ppid, stream, flags);
      break;
    case DATA_CHANNEL_PPID_DOMSTRING_PARTIAL:
    case DATA_CHANNEL_PPID_DOMSTRING:
    case DATA_CHANNEL_PPID_DOMSTRING_EMPTY:
    case DATA_CHANNEL_PPID_BINARY_PARTIAL:
    case DATA_CHANNEL_PPID_BINARY:
    case DATA_CHANNEL_PPID_BINARY_EMPTY:
      HandleDataMessageChunk(buffer, length, ppid, stream, messageId, flags);
      break;
    default:
      DC_ERROR((
          "Unhandled message of length %zu PPID %u on stream %u received (%s).",
          length, ppid, stream, (flags & MSG_EOR) ? "complete" : "partial"));
      break;
  }
}

void DataChannelConnection::HandleAssociationChangeEvent(
    const struct sctp_assoc_change* sac) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  mLock.AssertCurrentThreadOwns();

  uint32_t i, n;
  DataChannelConnectionState state = GetState();
  switch (sac->sac_state) {
    case SCTP_COMM_UP:
      DC_DEBUG(("Association change: SCTP_COMM_UP"));
      if (state == DataChannelConnectionState::Connecting) {
        SetState(DataChannelConnectionState::Open);

        DC_DEBUG(("Negotiated number of incoming streams: %" PRIu16,
                  sac->sac_inbound_streams));
        DC_DEBUG(("Negotiated number of outgoing streams: %" PRIu16,
                  sac->sac_outbound_streams));
        mNegotiatedIdLimit = std::max(
            mNegotiatedIdLimit,
            std::max(sac->sac_outbound_streams, sac->sac_inbound_streams));

        Dispatch(do_AddRef(new DataChannelOnMessageAvailable(
            DataChannelOnMessageAvailable::EventType::OnConnection, this)));
        DC_DEBUG(("DTLS connect() succeeded!  Entering connected mode"));

        // Open any streams pending...
        ProcessQueuedOpens();

      } else if (state == DataChannelConnectionState::Open) {
        DC_DEBUG(("DataConnection Already OPEN"));
      } else {
        DC_ERROR(("Unexpected state: %s", ToString(state)));
      }
      break;
    case SCTP_COMM_LOST:
      DC_DEBUG(("Association change: SCTP_COMM_LOST"));
      // This association is toast, so also close all the channels -- from
      // mainthread!
      Stop();
      break;
    case SCTP_RESTART:
      DC_DEBUG(("Association change: SCTP_RESTART"));
      break;
    case SCTP_SHUTDOWN_COMP:
      DC_DEBUG(("Association change: SCTP_SHUTDOWN_COMP"));
      Stop();
      break;
    case SCTP_CANT_STR_ASSOC:
      DC_DEBUG(("Association change: SCTP_CANT_STR_ASSOC"));
      break;
    default:
      DC_DEBUG(("Association change: UNKNOWN"));
      break;
  }
  DC_DEBUG(("Association change: streams (in/out) = (%u/%u)",
            sac->sac_inbound_streams, sac->sac_outbound_streams));

  if (NS_WARN_IF(!sac)) {
    return;
  }

  n = sac->sac_length - sizeof(*sac);
  if ((sac->sac_state == SCTP_COMM_UP) || (sac->sac_state == SCTP_RESTART)) {
    if (n > 0) {
      for (i = 0; i < n; ++i) {
        switch (sac->sac_info[i]) {
          case SCTP_ASSOC_SUPPORTS_PR:
            DC_DEBUG(("Supports: PR"));
            break;
          case SCTP_ASSOC_SUPPORTS_AUTH:
            DC_DEBUG(("Supports: AUTH"));
            break;
          case SCTP_ASSOC_SUPPORTS_ASCONF:
            DC_DEBUG(("Supports: ASCONF"));
            break;
          case SCTP_ASSOC_SUPPORTS_MULTIBUF:
            DC_DEBUG(("Supports: MULTIBUF"));
            break;
          case SCTP_ASSOC_SUPPORTS_RE_CONFIG:
            DC_DEBUG(("Supports: RE-CONFIG"));
            break;
#if defined(SCTP_ASSOC_SUPPORTS_INTERLEAVING)
          case SCTP_ASSOC_SUPPORTS_INTERLEAVING:
            DC_DEBUG(("Supports: NDATA"));
            // TODO: This should probably be set earlier above in 'case
            //       SCTP_COMM_UP' but we also need this for 'SCTP_RESTART'.
            mSendInterleaved = true;
            break;
#endif
          default:
            DC_ERROR(("Supports: UNKNOWN(0x%02x)", sac->sac_info[i]));
            break;
        }
      }
    }
  } else if (((sac->sac_state == SCTP_COMM_LOST) ||
              (sac->sac_state == SCTP_CANT_STR_ASSOC)) &&
             (n > 0)) {
    DC_DEBUG(("Association: ABORT ="));
    for (i = 0; i < n; ++i) {
      DC_DEBUG((" 0x%02x", sac->sac_info[i]));
    }
  }
  if ((sac->sac_state == SCTP_CANT_STR_ASSOC) ||
      (sac->sac_state == SCTP_SHUTDOWN_COMP) ||
      (sac->sac_state == SCTP_COMM_LOST)) {
    return;
  }
}

void DataChannelConnection::HandlePeerAddressChangeEvent(
    const struct sctp_paddr_change* spc) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  const char* addr = "";
#if !defined(__Userspace_os_Windows)
  char addr_buf[INET6_ADDRSTRLEN];
  struct sockaddr_in* sin;
  struct sockaddr_in6* sin6;
#endif

  switch (spc->spc_aaddr.ss_family) {
    case AF_INET:
#if !defined(__Userspace_os_Windows)
      sin = (struct sockaddr_in*)&spc->spc_aaddr;
      addr = inet_ntop(AF_INET, &sin->sin_addr, addr_buf, INET6_ADDRSTRLEN);
#endif
      break;
    case AF_INET6:
#if !defined(__Userspace_os_Windows)
      sin6 = (struct sockaddr_in6*)&spc->spc_aaddr;
      addr = inet_ntop(AF_INET6, &sin6->sin6_addr, addr_buf, INET6_ADDRSTRLEN);
#endif
      break;
    case AF_CONN:
      addr = "DTLS connection";
      break;
    default:
      break;
  }
  DC_DEBUG(("Peer address %s is now ", addr));
  switch (spc->spc_state) {
    case SCTP_ADDR_AVAILABLE:
      DC_DEBUG(("SCTP_ADDR_AVAILABLE"));
      break;
    case SCTP_ADDR_UNREACHABLE:
      DC_DEBUG(("SCTP_ADDR_UNREACHABLE"));
      break;
    case SCTP_ADDR_REMOVED:
      DC_DEBUG(("SCTP_ADDR_REMOVED"));
      break;
    case SCTP_ADDR_ADDED:
      DC_DEBUG(("SCTP_ADDR_ADDED"));
      break;
    case SCTP_ADDR_MADE_PRIM:
      DC_DEBUG(("SCTP_ADDR_MADE_PRIM"));
      break;
    case SCTP_ADDR_CONFIRMED:
      DC_DEBUG(("SCTP_ADDR_CONFIRMED"));
      break;
    default:
      DC_ERROR(("UNKNOWN SCP STATE"));
      break;
  }
  if (spc->spc_error) {
    DC_ERROR((" (error = 0x%08x).\n", spc->spc_error));
  }
}

void DataChannelConnection::HandleRemoteErrorEvent(
    const struct sctp_remote_error* sre) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  size_t i, n;

  n = sre->sre_length - sizeof(struct sctp_remote_error);
  DC_WARN(("Remote Error (error = 0x%04x): ", sre->sre_error));
  for (i = 0; i < n; ++i) {
    DC_WARN((" 0x%02x", sre->sre_data[i]));
  }
}

void DataChannelConnection::HandleShutdownEvent(
    const struct sctp_shutdown_event* sse) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  DC_DEBUG(("Shutdown event."));
  /* XXX: notify all channels. */
  // Attempts to actually send anything will fail
}

void DataChannelConnection::HandleAdaptationIndication(
    const struct sctp_adaptation_event* sai) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  DC_DEBUG(("Adaptation indication: %x.", sai->sai_adaptation_ind));
}

void DataChannelConnection::HandlePartialDeliveryEvent(
    const struct sctp_pdapi_event* spde) {
  // Note: Be aware that stream and sequence number being u32 instead of u16 is
  //       a bug in the SCTP API. This may change in the future.

  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  DC_DEBUG(("Partial delivery event: "));
  switch (spde->pdapi_indication) {
    case SCTP_PARTIAL_DELIVERY_ABORTED:
      DC_DEBUG(("delivery aborted "));
      break;
    default:
      DC_ERROR(("??? "));
      break;
  }
  DC_DEBUG(("(flags = %x), stream = %" PRIu32 ", sn = %" PRIu32,
            spde->pdapi_flags, spde->pdapi_stream, spde->pdapi_seq));

  // Validate stream ID
  if (spde->pdapi_stream >= UINT16_MAX) {
    DC_ERROR(("Invalid stream id in partial delivery event: %" PRIu32 "\n",
              spde->pdapi_stream));
    return;
  }

  // Find channel and reset buffer
  DataChannel* channel = FindChannelByStream((uint16_t)spde->pdapi_stream);
  if (channel) {
    auto it = channel->mRecvBuffers.find(spde->pdapi_seq);
    if (it != channel->mRecvBuffers.end()) {
      DC_WARN(("Abort partially delivered message of %zu bytes\n",
               it->second.GetLength()));
      channel->mRecvBuffers.erase(it);
    } else {
      // Uhhh, ok?
      DC_WARN(
          ("Abort partially delivered message that we've never seen any "
           "of? What?"));
    }
  }
}

void DataChannelConnection::HandleSendFailedEvent(
    const struct sctp_send_failed_event* ssfe) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  size_t i, n;

  if (ssfe->ssfe_flags & SCTP_DATA_UNSENT) {
    DC_DEBUG(("Unsent "));
  }
  if (ssfe->ssfe_flags & SCTP_DATA_SENT) {
    DC_DEBUG(("Sent "));
  }
  if (ssfe->ssfe_flags & ~(SCTP_DATA_SENT | SCTP_DATA_UNSENT)) {
    DC_DEBUG(("(flags = %x) ", ssfe->ssfe_flags));
  }
#ifdef XP_WIN
#  define PRIPPID "lu"
#else
#  define PRIPPID "u"
#endif
  DC_DEBUG(("message with PPID = %" PRIPPID
            ", SID = %d, flags: 0x%04x due to error = 0x%08x",
            ntohl(ssfe->ssfe_info.snd_ppid), ssfe->ssfe_info.snd_sid,
            ssfe->ssfe_info.snd_flags, ssfe->ssfe_error));
#undef PRIPPID
  n = ssfe->ssfe_length - sizeof(struct sctp_send_failed_event);
  for (i = 0; i < n; ++i) {
    DC_DEBUG((" 0x%02x", ssfe->ssfe_data[i]));
  }
}

void DataChannelConnection::ClearResets() {
  // Clear all pending resets
  if (!mStreamsResetting.IsEmpty()) {
    DC_DEBUG(("Clearing resets for %zu streams", mStreamsResetting.Length()));
  }
  mStreamsResetting.Clear();
}

void DataChannelConnection::MarkStreamForReset(DataChannel& aChannel) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());

  mLock.AssertCurrentThreadOwns();
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

void DataChannelConnection::ResetStreams(nsTArray<uint16_t>& aStreams) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());

  DC_DEBUG(("%s %p: Sending outgoing stream reset for %zu streams", __func__,
            this, aStreams.Length()));
  if (aStreams.IsEmpty()) {
    DC_DEBUG(("No streams to reset"));
    return;
  }
  const size_t len =
      sizeof(sctp_reset_streams) + (aStreams.Length()) * sizeof(uint16_t);
  struct sctp_reset_streams* srs = static_cast<struct sctp_reset_streams*>(
      moz_xmalloc(len));  // infallible malloc
  memset(srs, 0, len);
  srs->srs_flags = SCTP_STREAM_RESET_OUTGOING;
  srs->srs_number_streams = aStreams.Length();
  for (size_t i = 0; i < aStreams.Length(); ++i) {
    srs->srs_stream_list[i] = aStreams[i];
  }
  if (usrsctp_setsockopt(mSocket, IPPROTO_SCTP, SCTP_RESET_STREAMS, srs,
                         (socklen_t)len) < 0) {
    DC_ERROR(("***failed: setsockopt RESET, errno %d", errno));
    // if errno == EALREADY, this is normal - we can't send another reset
    // with one pending.
    // When we get an incoming reset (which may be a response to our
    // outstanding one), see if we have any pending outgoing resets and
    // send them
  } else {
    aStreams.Clear();
  }
  free(srs);
}

void DataChannelConnection::HandleStreamResetEvent(
    const struct sctp_stream_reset_event* strrst) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  mLock.AssertCurrentThreadOwns();
  std::vector<uint16_t> streamsReset;

  if (!(strrst->strreset_flags & SCTP_STREAM_RESET_DENIED) &&
      !(strrst->strreset_flags & SCTP_STREAM_RESET_FAILED)) {
    size_t n =
        (strrst->strreset_length - sizeof(struct sctp_stream_reset_event)) /
        sizeof(uint16_t);
    for (size_t i = 0; i < n; ++i) {
      if (strrst->strreset_flags & SCTP_STREAM_RESET_INCOMING_SSN) {
        streamsReset.push_back(strrst->strreset_stream_list[i]);
      }
    }
  }

  OnStreamsReset(std::move(streamsReset));
}

void DataChannelConnection::OnStreamsReset(std::vector<uint16_t>&& aStreams) {
  mLock.AssertCurrentThreadOwns();
  for (auto stream : aStreams) {
    auto channel = FindChannelByStream(stream);
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

void DataChannelConnection::HandleStreamChangeEvent(
    const struct sctp_stream_change_event* strchg) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  mLock.AssertCurrentThreadOwns();
  if (strchg->strchange_flags == SCTP_STREAM_CHANGE_DENIED) {
    DC_ERROR(("*** Failed increasing number of streams from %u (%u/%u)",
              mNegotiatedIdLimit, strchg->strchange_instrms,
              strchg->strchange_outstrms));
    // XXX FIX! notify pending opens of failure
    return;
  }
  if (strchg->strchange_instrms > mNegotiatedIdLimit) {
    DC_DEBUG(("Other side increased streams from %u to %u", mNegotiatedIdLimit,
              strchg->strchange_instrms));
  }
  uint16_t old_limit = mNegotiatedIdLimit;
  uint16_t new_limit =
      std::min((uint16_t)MAX_NUM_STREAMS,
               std::max(strchg->strchange_outstrms, strchg->strchange_instrms));
  if (new_limit > mNegotiatedIdLimit) {
    DC_DEBUG(("Increasing number of streams from %u to %u - adding %u (in: %u)",
              old_limit, new_limit, new_limit - old_limit,
              strchg->strchange_instrms));
    // make sure both are the same length
    mNegotiatedIdLimit = new_limit;
    DC_DEBUG(("New length = %u (was %u)", mNegotiatedIdLimit, old_limit));
    // Re-process any channels waiting for streams.
    // Linear search, but we don't increase channels often and
    // the array would only get long in case of an app error normally

    // Make sure we request enough streams if there's a big jump in streams
    // Could make a more complex API for OpenXxxFinish() and avoid this loop
    auto channels = mChannels.GetAll();
    size_t num_needed =
        channels.Length() ? (channels.LastElement()->mStream + 1) : 0;
    Maybe<uint16_t> num_desired;
    MOZ_ASSERT(num_needed != INVALID_STREAM);
    if (num_needed > new_limit) {
      // Round up to a multiple of 16, or cap out
      num_desired =
          Some(std::min(16 * (num_needed / 16 + 1), (size_t)MAX_NUM_STREAMS));
      DC_DEBUG(("Not enough new streams, asking for %u", *num_desired));
    } else if (strchg->strchange_outstrms < strchg->strchange_instrms) {
      num_desired = Some(strchg->strchange_instrms);
      DC_DEBUG(("Requesting %u output streams to match partner", *num_desired));
    }

    if (num_desired.isSome()) {
      RaiseStreamLimitTo(*num_desired);
    }

    ProcessQueuedOpens();
  }
  // else probably not a change in # of streams

  if ((strchg->strchange_flags & SCTP_STREAM_CHANGE_DENIED) ||
      (strchg->strchange_flags & SCTP_STREAM_CHANGE_FAILED)) {
    // Other side denied our request. Need to AnnounceClosed some stuff.
    for (auto& channel : mChannels.GetAll()) {
      if (channel->mStream >= mNegotiatedIdLimit) {
        /* XXX: Signal to the other end. */
        FinishClose_s(channel);
        // maybe fire onError (bug 843625)
      }
    }
  }
}

void DataChannelConnection::HandleNotification(
    const union sctp_notification* notif, size_t n) {
  mLock.AssertCurrentThreadOwns();
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  if (notif->sn_header.sn_length != (uint32_t)n) {
    return;
  }
  switch (notif->sn_header.sn_type) {
    case SCTP_ASSOC_CHANGE:
      HandleAssociationChangeEvent(&(notif->sn_assoc_change));
      break;
    case SCTP_PEER_ADDR_CHANGE:
      HandlePeerAddressChangeEvent(&(notif->sn_paddr_change));
      break;
    case SCTP_REMOTE_ERROR:
      HandleRemoteErrorEvent(&(notif->sn_remote_error));
      break;
    case SCTP_SHUTDOWN_EVENT:
      HandleShutdownEvent(&(notif->sn_shutdown_event));
      break;
    case SCTP_ADAPTATION_INDICATION:
      HandleAdaptationIndication(&(notif->sn_adaptation_event));
      break;
    case SCTP_AUTHENTICATION_EVENT:
      DC_DEBUG(("SCTP_AUTHENTICATION_EVENT"));
      break;
    case SCTP_SENDER_DRY_EVENT:
      // DC_DEBUG(("SCTP_SENDER_DRY_EVENT"));
      break;
    case SCTP_NOTIFICATIONS_STOPPED_EVENT:
      DC_DEBUG(("SCTP_NOTIFICATIONS_STOPPED_EVENT"));
      break;
    case SCTP_PARTIAL_DELIVERY_EVENT:
      HandlePartialDeliveryEvent(&(notif->sn_pdapi_event));
      break;
    case SCTP_SEND_FAILED_EVENT:
      HandleSendFailedEvent(&(notif->sn_send_failed_event));
      break;
    case SCTP_STREAM_RESET_EVENT:
      HandleStreamResetEvent(&(notif->sn_strreset_event));
      break;
    case SCTP_ASSOC_RESET_EVENT:
      DC_DEBUG(("SCTP_ASSOC_RESET_EVENT"));
      break;
    case SCTP_STREAM_CHANGE_EVENT:
      HandleStreamChangeEvent(&(notif->sn_strchange_event));
      break;
    default:
      DC_ERROR(("unknown SCTP event: %u", (uint32_t)notif->sn_header.sn_type));
      break;
  }
}

int DataChannelConnection::ReceiveCallback(struct socket* sock, void* data,
                                           size_t datalen,
                                           struct sctp_rcvinfo rcv, int flags) {
  ASSERT_WEBRTC(!NS_IsMainThread());
  DC_DEBUG(("In ReceiveCallback"));

  // libusrsctp just went reentrant on us. Put a stop to this.
  mSTS->Dispatch(NS_NewRunnableFunction(
      "DataChannelConnection::ReceiveCallback",
      [data, datalen, rcv, flags, this,
       self = RefPtr<DataChannelConnection>(this)]() mutable {
        if (!data) {
          DC_DEBUG(("ReceiveCallback: SCTP has finished shutting down"));
        } else {
          mLock.Lock();
          if (flags & MSG_NOTIFICATION) {
            HandleNotification(static_cast<union sctp_notification*>(data),
                               datalen);
          } else {
            // NOTE: When interleaved mode is in use, rcv.rcv_ssn holds the
            // message id instead of the stream sequence number, based on a read
            // of the usrsctp code.
            HandleMessageChunk(data, datalen, ntohl(rcv.rcv_ppid), rcv.rcv_sid,
                               rcv.rcv_ssn, flags);
          }
          mLock.Unlock();
          // sctp allocates 'data' with malloc(), and expects the receiver to
          // free it.
          // It would be nice if it were possible to eliminate a copy by passing
          // ownership here, but because DATA messages end up in an nsCString,
          // and ncCString requires null termination (which usrsctp does not
          // do), we _have_ to make a copy somewhere. That might as well be
          // here. The downstream code can avoid further copies in whatever way
          // makes sense.
          free(data);
        }
      }));

  // usrsctp defines the callback as returning an int, but doesn't use it
  return 1;
}

already_AddRefed<DataChannel> DataChannelConnection::Open(
    const nsACString& label, const nsACString& protocol,
    DataChannelReliabilityPolicy prPolicy, bool inOrder, uint32_t prValue,
    DataChannelListener* aListener, nsISupports* aContext,
    bool aExternalNegotiated, uint16_t aStream) {
  ASSERT_WEBRTC(NS_IsMainThread());
  MutexAutoLock lock(mLock);
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
      ("DC Open: label %s/%s, type %s, inorder %d, prValue %u, listener %p, "
       "context %p, external: %s, stream %u",
       PromiseFlatCString(label).get(), PromiseFlatCString(protocol).get(),
       ToString(prPolicy), inOrder, prValue, aListener, aContext,
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
      prValue, inOrder, aExternalNegotiated, aListener, aContext));
  mChannels.Insert(channel);

  if (aStream != INVALID_STREAM) {
    mSTS->Dispatch(NS_NewRunnableFunction(
        "DataChannel::OpenFinish",
        [this, self = RefPtr<DataChannelConnection>(this), channel]() mutable {
          MutexAutoLock lock(mLock);
          OpenFinish(channel);
        }));
  }

  return channel.forget();
}

// Separate routine so we can also call it to finish up from pending opens
void DataChannelConnection::OpenFinish(RefPtr<DataChannel> aChannel) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  mLock.AssertCurrentThreadOwns();
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
}

// Returns a POSIX error code directly instead of setting errno.
int DataChannelConnection::SendMsgInternal(OutgoingMsg& msg, size_t* aWritten) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  mLock.AssertCurrentThreadOwns();
  struct sctp_sendv_spa info = {};
  // General flags
  info.sendv_flags = SCTP_SEND_SNDINFO_VALID;

  // Set stream identifier and protocol identifier
  info.sendv_sndinfo.snd_sid = msg.GetMetadata().mStreamId;
  info.sendv_sndinfo.snd_ppid = htonl(msg.GetMetadata().mPpid);

  if (msg.GetMetadata().mUnordered) {
    info.sendv_sndinfo.snd_flags |= SCTP_UNORDERED;
  }

  // Partial reliability policy, lifetime and rtx are mutually exclusive
  msg.GetMetadata().mMaxLifetimeMs.apply([&](auto value) {
    info.sendv_prinfo.pr_policy = SCTP_PR_SCTP_TTL;
    info.sendv_prinfo.pr_value = value;
    info.sendv_flags |= SCTP_SEND_PRINFO_VALID;
  });

  msg.GetMetadata().mMaxRetransmissions.apply([&](auto value) {
    info.sendv_prinfo.pr_policy = SCTP_PR_SCTP_RTX;
    info.sendv_prinfo.pr_value = value;
    info.sendv_flags |= SCTP_SEND_PRINFO_VALID;
  });

  // Send until buffer is empty
  Span<const uint8_t> chunk = msg.GetRemainingData();
  do {
    if (chunk.Length() <= DATA_CHANNEL_MAX_BINARY_FRAGMENT) {
      // Last chunk!
      info.sendv_sndinfo.snd_flags |= SCTP_EOR;
    } else {
      chunk = chunk.To(DATA_CHANNEL_MAX_BINARY_FRAGMENT);
    }

    // Send (or try at least)
    // SCTP will return EMSGSIZE if the message is bigger than the buffer
    // size (or EAGAIN if there isn't space). However, we can avoid EMSGSIZE
    // by carefully crafting small enough message chunks.
    const ssize_t writtenOrError = usrsctp_sendv(
        mSocket, chunk.Elements(), chunk.Length(), nullptr, 0, (void*)&info,
        (socklen_t)sizeof(struct sctp_sendv_spa), SCTP_SENDV_SPA, 0);

    if (writtenOrError < 0) {
      return errno;
    }

    const size_t written = writtenOrError;

    if (aWritten &&
        msg.GetMetadata().mPpid != DATA_CHANNEL_PPID_DOMSTRING_EMPTY &&
        msg.GetMetadata().mPpid != DATA_CHANNEL_PPID_BINARY_EMPTY) {
      *aWritten += written;
    }
    DC_DEBUG(("Sent buffer (written=%zu, len=%zu, left=%zu)", written,
              chunk.Length(), msg.GetRemainingData().Length() - written));

    // TODO: Remove once resolved
    // (https://github.com/sctplab/usrsctp/issues/132)
    if (written == 0) {
      DC_ERROR(("@tuexen: usrsctp_sendv returned 0"));
      return EAGAIN;
    }

    // Update buffer position
    msg.Advance(written);

    // If not all bytes have been written, this obviously means that usrsctp's
    // buffer is full and we need to try again later.
    if (written < chunk.Length()) {
      return EAGAIN;
    }

    chunk = msg.GetRemainingData();
  } while (chunk.Length() > 0);

  return 0;
}

// Returns a POSIX error code directly instead of setting errno.
// IMPORTANT: Ensure that the buffer passed is guarded by mLock!
int DataChannelConnection::SendMsgInternalOrBuffer(
    nsTArray<OutgoingMsg>& buffer, OutgoingMsg&& msg, bool* buffered,
    size_t* aWritten) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  NS_WARNING_ASSERTION(msg.GetLength() > 0, "Length is 0?!");

  int error = 0;
  bool need_buffering = false;

  // Note: Main-thread IO, but doesn't block!
  // XXX FIX!  to deal with heavy overruns of JS trying to pass data in
  // (more than the buffersize) queue data onto another thread to do the
  // actual sends.  See netwerk/protocol/websocket/WebSocketChannel.cpp

  // Avoid a race between buffer-full-failure (where we have to add the
  // packet to the buffered-data queue) and the buffer-now-only-half-full
  // callback, which happens on a different thread.  Otherwise we might
  // fail here, then before we add it to the queue get the half-full
  // callback, find nothing to do, then on this thread add it to the
  // queue - which would sit there.  Also, if we later send more data, it
  // would arrive ahead of the buffered message, but if the buffer ever
  // got to 1/2 full, the message would get sent - but at a semi-random
  // time, after other data it was supposed to be in front of.

  // Must lock before empty check for similar reasons!
  mLock.AssertCurrentThreadOwns();
  if (buffer.IsEmpty() &&
      (mSendInterleaved || mPendingType == PendingType::None)) {
    error = SendMsgInternal(msg, aWritten);
    switch (error) {
      case 0:
        break;
      case EAGAIN:
#if (EAGAIN != EWOULDBLOCK)
      case EWOULDBLOCK:
#endif
        need_buffering = true;
        break;
      default:
        DC_ERROR(("error %d on sending", error));
        break;
    }
  } else {
    need_buffering = true;
  }

  if (need_buffering) {
    // queue data for resend!  And queue any further data for the stream until
    // it is...
    buffer.EmplaceBack(std::move(msg));
    DC_DEBUG(("Queued %zu buffers (left=%zu, total=%zu)", buffer.Length(),
              buffer.LastElement().GetLength(), msg.GetLength()));
    if (buffered) {
      *buffered = true;
    }
    return 0;
  }

  if (buffered) {
    *buffered = false;
  }
  return error;
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
  MutexAutoLock lock(mLock);
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
    ASSERT_WEBRTC(NS_IsMainThread());

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
  mLock.AssertCurrentThreadOwns();
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
  // NOTE: 'aThis' has been forgotten by the caller to avoid releasing
  // it off mainthread; if PeerConnectionImpl has released then we want
  // ~DataChannelConnection() to run on MainThread

  // XXX to do this safely, we must enqueue these atomically onto the
  // output socket.  We need a sender thread(s?) to enqueue data into the
  // socket and to avoid main-thread IO that might block.  Even on a
  // background thread, we may not want to block on one stream's data.
  // I.e. run non-blocking and service multiple channels.

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
  ASSERT_WEBRTC(NS_IsMainThread());
  MutexAutoLock lock(mLock);

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
        MutexAutoLock lock(mLock);
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
          channel->WithTrafficCounters(
              [len = msg.Length()](DataChannel::TrafficCounters& counters) {
                counters.mMessagesSent++;
                counters.mBytesSent += len;
              });
        }
      }));

  return 0;
}

int DataChannelConnection::SendMessage(DataChannel& aChannel,
                                       OutgoingMsg&& aMsg) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  // Note that `aChannel->mConnection` is `this`. This is just here to
  // satisfy the thread safety annotations on DataChannel.
  aChannel.mConnection->mLock.AssertCurrentThreadOwns();
  bool buffered;
  if (aMsg.GetMetadata().mPpid == DATA_CHANNEL_PPID_CONTROL) {
    int error = SendMsgInternalOrBuffer(mBufferedControl, std::move(aMsg),
                                        &buffered, nullptr);
    // Set pending type (if buffered)
    if (!error && buffered && mPendingType == PendingType::None) {
      mPendingType = PendingType::Dcep;
    }
    return error;
  }

  size_t written = 0;
  if (const int error = SendMsgInternalOrBuffer(
          aChannel.mBufferedData, std::move(aMsg), &buffered, &written);
      error) {
    return error;
  }

  if (written &&
      aMsg.GetMetadata().mPpid != DATA_CHANNEL_PPID_DOMSTRING_EMPTY &&
      aMsg.GetMetadata().mPpid != DATA_CHANNEL_PPID_BINARY_EMPTY) {
    aChannel.DecrementBufferedAmount(written);
  }

  // Set pending type and stream index (if buffered)
  if (buffered && mPendingType == PendingType::None) {
    mPendingType = PendingType::Data;
    mCurrentStream = aChannel.mStream;
  }

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
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
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
      __func__, [this, self = RefPtr<DataChannelConnection>(this),
                 channel = RefPtr<DataChannel>(aChannel)]() {
        MutexAutoLock lock(mLock);
        FinishClose_s(channel);
      }));
}

void DataChannelConnection::FinishClose_s(DataChannel* aChannel) {
  MOZ_ASSERT(mSTS->IsOnCurrentThread());
  mLock.AssertCurrentThreadOwns();

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
  ASSERT_WEBRTC(NS_IsMainThread());
  DC_DEBUG(("Closing all channels (connection %p)", (void*)this));

  // Make sure no more channels will be opened
  MutexAutoLock lock(mLock);

  // Close current channels
  // If there are runnables, they hold a strong ref and keep the channel
  // and/or connection alive (even if in a CLOSED state)
  for (auto& channel : mChannels.GetAll()) {
    MutexAutoUnlock lock(mLock);
    channel->Close();
  }

  mSTS->Dispatch(NS_NewRunnableFunction(
      "DataChannelConnection::CloseAll",
      [this, self = RefPtr<DataChannelConnection>(this)]() {
        MutexAutoLock lock(mLock);
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
                         bool ordered, bool negotiated,
                         DataChannelListener* aListener, nsISupports* aContext)
    : mListener(aListener),
      mContext(aContext),
      mConnection(connection),
      mLabel(label),
      mProtocol(protocol),
      mReadyState(state),
      mStream(stream),
      mPrPolicy(policy),
      mPrValue(value),
      mNegotiated(negotiated),
      mOrdered(ordered),
      mIsRecvBinary(false),
      mBufferedThreshold(0),  // default from spec
      mBufferedAmount(0),
      mMainThreadEventTarget(connection->GetNeckoTarget()),
      mStatsLock("netwer::sctp::DataChannel::mStatsLock") {
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
  if (mConnection) {
    // ensure we don't get deleted
    RefPtr<DataChannelConnection> connection(mConnection);
    connection->Close(this);
  }
}

void DataChannel::ReleaseConnection() {
  ASSERT_WEBRTC(NS_IsMainThread());
  mConnection = nullptr;
}

void DataChannel::SetListener(DataChannelListener* aListener,
                              nsISupports* aContext) {
  ASSERT_WEBRTC(NS_IsMainThread());
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
  ASSERT_WEBRTC(NS_IsMainThread());
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
  MutexAutoLock lock(mStatsLock);
  return mTrafficCounters;
}

bool DataChannel::EnsureValidStream(ErrorResult& aRv) {
  MOZ_ASSERT(mConnection);
  if (mConnection && mStream != INVALID_STREAM) {
    return true;
  }
  aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  return false;
}

void DataChannel::WithTrafficCounters(
    const std::function<void(TrafficCounters&)>& aFn) {
  MutexAutoLock lock(mStatsLock);
  aFn(mTrafficCounters);
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
    case EventType::OnChannelCreated:
      if (!mConnection->mListener) {
        DC_ERROR(("DataChannelOnMessageAvailable (%s) with null Listener!",
                  ToString(mType)));
        return NS_OK;
      }

      // important to give it an already_AddRefed pointer!
      mConnection->mListener->NotifyDataChannel(mChannel.forget());
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
