/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/base/turnport.h"

#include <functional>

#include "webrtc/p2p/base/common.h"
#include "webrtc/p2p/base/stun.h"
#include "webrtc/base/asyncpacketsocket.h"
#include "webrtc/base/byteorder.h"
#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/nethelpers.h"
#include "webrtc/base/socketaddress.h"
#include "webrtc/base/stringencode.h"

namespace cricket {

// TODO(juberti): Move to stun.h when relay messages have been renamed.
static const int TURN_ALLOCATE_REQUEST = STUN_ALLOCATE_REQUEST;

// TODO(juberti): Extract to turnmessage.h
static const int TURN_DEFAULT_PORT = 3478;
static const int TURN_CHANNEL_NUMBER_START = 0x4000;
static const int TURN_PERMISSION_TIMEOUT = 5 * 60 * 1000;  // 5 minutes

static const size_t TURN_CHANNEL_HEADER_SIZE = 4U;

// Retry at most twice (i.e. three different ALLOCATE requests) on
// STUN_ERROR_ALLOCATION_MISMATCH error per rfc5766.
static const size_t MAX_ALLOCATE_MISMATCH_RETRIES = 2;

inline bool IsTurnChannelData(uint16 msg_type) {
  return ((msg_type & 0xC000) == 0x4000);  // MSB are 0b01
}

static int GetRelayPreference(cricket::ProtocolType proto, bool secure) {
  int relay_preference = ICE_TYPE_PREFERENCE_RELAY;
  if (proto == cricket::PROTO_TCP) {
    relay_preference -= 1;
    if (secure)
      relay_preference -= 1;
  }

  ASSERT(relay_preference >= 0);
  return relay_preference;
}

class TurnAllocateRequest : public StunRequest {
 public:
  explicit TurnAllocateRequest(TurnPort* port);
  virtual void Prepare(StunMessage* request);
  virtual void OnResponse(StunMessage* response);
  virtual void OnErrorResponse(StunMessage* response);
  virtual void OnTimeout();

 private:
  // Handles authentication challenge from the server.
  void OnAuthChallenge(StunMessage* response, int code);
  void OnTryAlternate(StunMessage* response, int code);
  void OnUnknownAttribute(StunMessage* response);

  TurnPort* port_;
};

class TurnRefreshRequest : public StunRequest {
 public:
  explicit TurnRefreshRequest(TurnPort* port);
  virtual void Prepare(StunMessage* request);
  virtual void OnResponse(StunMessage* response);
  virtual void OnErrorResponse(StunMessage* response);
  virtual void OnTimeout();
  void set_lifetime(int lifetime) { lifetime_ = lifetime; }

 private:
  TurnPort* port_;
  int lifetime_;
};

class TurnCreatePermissionRequest : public StunRequest,
                                    public sigslot::has_slots<> {
 public:
  TurnCreatePermissionRequest(TurnPort* port, TurnEntry* entry,
                              const rtc::SocketAddress& ext_addr);
  virtual void Prepare(StunMessage* request);
  virtual void OnResponse(StunMessage* response);
  virtual void OnErrorResponse(StunMessage* response);
  virtual void OnTimeout();

 private:
  void OnEntryDestroyed(TurnEntry* entry);

  TurnPort* port_;
  TurnEntry* entry_;
  rtc::SocketAddress ext_addr_;
};

class TurnChannelBindRequest : public StunRequest,
                               public sigslot::has_slots<> {
 public:
  TurnChannelBindRequest(TurnPort* port, TurnEntry* entry, int channel_id,
                         const rtc::SocketAddress& ext_addr);
  virtual void Prepare(StunMessage* request);
  virtual void OnResponse(StunMessage* response);
  virtual void OnErrorResponse(StunMessage* response);
  virtual void OnTimeout();

 private:
  void OnEntryDestroyed(TurnEntry* entry);

  TurnPort* port_;
  TurnEntry* entry_;
  int channel_id_;
  rtc::SocketAddress ext_addr_;
};

// Manages a "connection" to a remote destination. We will attempt to bring up
// a channel for this remote destination to reduce the overhead of sending data.
class TurnEntry : public sigslot::has_slots<> {
 public:
  enum BindState { STATE_UNBOUND, STATE_BINDING, STATE_BOUND };
  TurnEntry(TurnPort* port, int channel_id,
            const rtc::SocketAddress& ext_addr);

  TurnPort* port() { return port_; }

  int channel_id() const { return channel_id_; }
  const rtc::SocketAddress& address() const { return ext_addr_; }
  BindState state() const { return state_; }

  // Helper methods to send permission and channel bind requests.
  void SendCreatePermissionRequest();
  void SendChannelBindRequest(int delay);
  // Sends a packet to the given destination address.
  // This will wrap the packet in STUN if necessary.
  int Send(const void* data, size_t size, bool payload,
           const rtc::PacketOptions& options);

  void OnCreatePermissionSuccess();
  void OnCreatePermissionError(StunMessage* response, int code);
  void OnChannelBindSuccess();
  void OnChannelBindError(StunMessage* response, int code);
  // Signal sent when TurnEntry is destroyed.
  sigslot::signal1<TurnEntry*> SignalDestroyed;

 private:
  TurnPort* port_;
  int channel_id_;
  rtc::SocketAddress ext_addr_;
  BindState state_;
};

TurnPort::TurnPort(rtc::Thread* thread,
                   rtc::PacketSocketFactory* factory,
                   rtc::Network* network,
                   rtc::AsyncPacketSocket* socket,
                   const std::string& username,
                   const std::string& password,
                   const ProtocolAddress& server_address,
                   const RelayCredentials& credentials,
                   int server_priority,
                   const std::string& origin)
    : Port(thread, factory, network, socket->GetLocalAddress().ipaddr(),
           username, password),
      server_address_(server_address),
      credentials_(credentials),
      socket_(socket),
      resolver_(NULL),
      error_(0),
      request_manager_(thread),
      next_channel_number_(TURN_CHANNEL_NUMBER_START),
      connected_(false),
      server_priority_(server_priority),
      allocate_mismatch_retries_(0) {
  request_manager_.SignalSendPacket.connect(this, &TurnPort::OnSendStunPacket);
  request_manager_.set_origin(origin);
}

TurnPort::TurnPort(rtc::Thread* thread,
                   rtc::PacketSocketFactory* factory,
                   rtc::Network* network,
                   const rtc::IPAddress& ip,
                   uint16 min_port,
                   uint16 max_port,
                   const std::string& username,
                   const std::string& password,
                   const ProtocolAddress& server_address,
                   const RelayCredentials& credentials,
                   int server_priority,
                   const std::string& origin)
    : Port(thread, RELAY_PORT_TYPE, factory, network, ip, min_port, max_port,
           username, password),
      server_address_(server_address),
      credentials_(credentials),
      socket_(NULL),
      resolver_(NULL),
      error_(0),
      request_manager_(thread),
      next_channel_number_(TURN_CHANNEL_NUMBER_START),
      connected_(false),
      server_priority_(server_priority),
      allocate_mismatch_retries_(0) {
  request_manager_.SignalSendPacket.connect(this, &TurnPort::OnSendStunPacket);
  request_manager_.set_origin(origin);
}

TurnPort::~TurnPort() {
  // TODO(juberti): Should this even be necessary?

  // release the allocation by sending a refresh with
  // lifetime 0.
  if (connected_) {
    TurnRefreshRequest bye(this);
    bye.set_lifetime(0);
    SendRequest(&bye, 0);
  }

  while (!entries_.empty()) {
    DestroyEntry(entries_.front()->address());
  }
  if (resolver_) {
    resolver_->Destroy(false);
  }
  if (!SharedSocket()) {
    delete socket_;
  }
}

rtc::SocketAddress TurnPort::GetLocalAddress() const {
  return socket_ ? socket_->GetLocalAddress() : rtc::SocketAddress();
}

void TurnPort::PrepareAddress() {
  if (credentials_.username.empty() ||
      credentials_.password.empty()) {
    LOG(LS_ERROR) << "Allocation can't be started without setting the"
                  << " TURN server credentials for the user.";
    OnAllocateError();
    return;
  }

  if (!server_address_.address.port()) {
    // We will set default TURN port, if no port is set in the address.
    server_address_.address.SetPort(TURN_DEFAULT_PORT);
  }

  if (server_address_.address.IsUnresolved()) {
    ResolveTurnAddress(server_address_.address);
  } else {
    // If protocol family of server address doesn't match with local, return.
    if (!IsCompatibleAddress(server_address_.address)) {
      LOG(LS_ERROR) << "Server IP address family does not match with "
                    << "local host address family type";
      OnAllocateError();
      return;
    }

    // Insert the current address to prevent redirection pingpong.
    attempted_server_addresses_.insert(server_address_.address);

    LOG_J(LS_INFO, this) << "Trying to connect to TURN server via "
                         << ProtoToString(server_address_.proto) << " @ "
                         << server_address_.address.ToSensitiveString();
    if (!CreateTurnClientSocket()) {
      OnAllocateError();
    } else if (server_address_.proto == PROTO_UDP) {
      // If its UDP, send AllocateRequest now.
      // For TCP and TLS AllcateRequest will be sent by OnSocketConnect.
      SendRequest(new TurnAllocateRequest(this), 0);
    }
  }
}

bool TurnPort::CreateTurnClientSocket() {
  ASSERT(!socket_ || SharedSocket());

  if (server_address_.proto == PROTO_UDP && !SharedSocket()) {
    socket_ = socket_factory()->CreateUdpSocket(
        rtc::SocketAddress(ip(), 0), min_port(), max_port());
  } else if (server_address_.proto == PROTO_TCP) {
    ASSERT(!SharedSocket());
    int opts = rtc::PacketSocketFactory::OPT_STUN;
    // If secure bit is enabled in server address, use TLS over TCP.
    if (server_address_.secure) {
      opts |= rtc::PacketSocketFactory::OPT_TLS;
    }
    socket_ = socket_factory()->CreateClientTcpSocket(
        rtc::SocketAddress(ip(), 0), server_address_.address,
        proxy(), user_agent(), opts);
  }

  if (!socket_) {
    error_ = SOCKET_ERROR;
    return false;
  }

  // Apply options if any.
  for (SocketOptionsMap::iterator iter = socket_options_.begin();
       iter != socket_options_.end(); ++iter) {
    socket_->SetOption(iter->first, iter->second);
  }

  if (!SharedSocket()) {
    // If socket is shared, AllocationSequence will receive the packet.
    socket_->SignalReadPacket.connect(this, &TurnPort::OnReadPacket);
  }

  socket_->SignalReadyToSend.connect(this, &TurnPort::OnReadyToSend);

  if (server_address_.proto == PROTO_TCP) {
    socket_->SignalConnect.connect(this, &TurnPort::OnSocketConnect);
    socket_->SignalClose.connect(this, &TurnPort::OnSocketClose);
  }
  return true;
}

void TurnPort::OnSocketConnect(rtc::AsyncPacketSocket* socket) {
  ASSERT(server_address_.proto == PROTO_TCP);
  // Do not use this port if the socket bound to a different address than
  // the one we asked for. This is seen in Chrome, where TCP sockets cannot be
  // given a binding address, and the platform is expected to pick the
  // correct local address.

  // Further, to workaround issue 3927 in which a proxy is forcing TCP bound to
  // localhost only, we're allowing Loopback IP even if it's not the same as the
  // local Turn port.
  if (socket->GetLocalAddress().ipaddr() != ip()) {
    if (socket->GetLocalAddress().IsLoopbackIP()) {
      LOG(LS_WARNING) << "Socket is bound to a different address:"
                      << socket->GetLocalAddress().ipaddr().ToString()
                      << ", rather then the local port:" << ip().ToString()
                      << ". Still allowing it since it's localhost.";
    } else {
      LOG(LS_WARNING) << "Socket is bound to a different address:"
                      << socket->GetLocalAddress().ipaddr().ToString()
                      << ", rather then the local port:" << ip().ToString()
                      << ". Discarding TURN port.";
      OnAllocateError();
      return;
    }
  }

  if (server_address_.address.IsUnresolved()) {
    server_address_.address = socket_->GetRemoteAddress();
  }

  LOG(LS_INFO) << "TurnPort connected to " << socket->GetRemoteAddress()
               << " using tcp.";
  SendRequest(new TurnAllocateRequest(this), 0);
}

void TurnPort::OnSocketClose(rtc::AsyncPacketSocket* socket, int error) {
  LOG_J(LS_WARNING, this) << "Connection with server failed, error=" << error;
  ASSERT(socket == socket_);
  if (!connected_) {
    OnAllocateError();
  }
  connected_ = false;
}

void TurnPort::OnAllocateMismatch() {
  if (allocate_mismatch_retries_ >= MAX_ALLOCATE_MISMATCH_RETRIES) {
    LOG_J(LS_WARNING, this) << "Giving up on the port after "
                            << allocate_mismatch_retries_
                            << " retries for STUN_ERROR_ALLOCATION_MISMATCH";
    OnAllocateError();
    return;
  }

  LOG_J(LS_INFO, this) << "Allocating a new socket after "
                       << "STUN_ERROR_ALLOCATION_MISMATCH, retry = "
                       << allocate_mismatch_retries_ + 1;
  if (SharedSocket()) {
    ResetSharedSocket();
  } else {
    delete socket_;
  }
  socket_ = NULL;

  PrepareAddress();
  ++allocate_mismatch_retries_;
}

Connection* TurnPort::CreateConnection(const Candidate& address,
                                       CandidateOrigin origin) {
  // TURN-UDP can only connect to UDP candidates.
  if (address.protocol() != UDP_PROTOCOL_NAME) {
    return NULL;
  }

  if (!IsCompatibleAddress(address.address())) {
    return NULL;
  }

  // Create an entry, if needed, so we can get our permissions set up correctly.
  CreateEntry(address.address());

  // A TURN port will have two candiates, STUN and TURN. STUN may not
  // present in all cases. If present stun candidate will be added first
  // and TURN candidate later.
  for (size_t index = 0; index < Candidates().size(); ++index) {
    if (Candidates()[index].type() == RELAY_PORT_TYPE) {
      ProxyConnection* conn = new ProxyConnection(this, index, address);
      conn->SignalDestroyed.connect(this, &TurnPort::OnConnectionDestroyed);
      AddConnection(conn);
      return conn;
    }
  }
  return NULL;
}

int TurnPort::SetOption(rtc::Socket::Option opt, int value) {
  if (!socket_) {
    // If socket is not created yet, these options will be applied during socket
    // creation.
    socket_options_[opt] = value;
    return 0;
  }
  return socket_->SetOption(opt, value);
}

int TurnPort::GetOption(rtc::Socket::Option opt, int* value) {
  if (!socket_) {
    SocketOptionsMap::const_iterator it = socket_options_.find(opt);
    if (it == socket_options_.end()) {
      return -1;
    }
    *value = it->second;
    return 0;
  }

  return socket_->GetOption(opt, value);
}

int TurnPort::GetError() {
  return error_;
}

int TurnPort::SendTo(const void* data, size_t size,
                     const rtc::SocketAddress& addr,
                     const rtc::PacketOptions& options,
                     bool payload) {
  // Try to find an entry for this specific address; we should have one.
  TurnEntry* entry = FindEntry(addr);
  ASSERT(entry != NULL);
  if (!entry) {
    return 0;
  }

  if (!connected()) {
    error_ = EWOULDBLOCK;
    return SOCKET_ERROR;
  }

  // Send the actual contents to the server using the usual mechanism.
  int sent = entry->Send(data, size, payload, options);
  if (sent <= 0) {
    return SOCKET_ERROR;
  }

  // The caller of the function is expecting the number of user data bytes,
  // rather than the size of the packet.
  return static_cast<int>(size);
}

void TurnPort::OnReadPacket(
    rtc::AsyncPacketSocket* socket, const char* data, size_t size,
    const rtc::SocketAddress& remote_addr,
    const rtc::PacketTime& packet_time) {
  ASSERT(socket == socket_);

  // This is to guard against a STUN response from previous server after
  // alternative server redirection. TODO(guoweis): add a unit test for this
  // race condition.
  if (remote_addr != server_address_.address) {
    LOG_J(LS_WARNING, this) << "Discarding TURN message from unknown address:"
                            << remote_addr.ToString()
                            << ", server_address_:"
                            << server_address_.address.ToString();
    return;
  }

  // The message must be at least the size of a channel header.
  if (size < TURN_CHANNEL_HEADER_SIZE) {
    LOG_J(LS_WARNING, this) << "Received TURN message that was too short";
    return;
  }

  // Check the message type, to see if is a Channel Data message.
  // The message will either be channel data, a TURN data indication, or
  // a response to a previous request.
  uint16 msg_type = rtc::GetBE16(data);
  if (IsTurnChannelData(msg_type)) {
    HandleChannelData(msg_type, data, size, packet_time);
  } else if (msg_type == TURN_DATA_INDICATION) {
    HandleDataIndication(data, size, packet_time);
  } else {
    if (SharedSocket() &&
        (msg_type == STUN_BINDING_RESPONSE ||
         msg_type == STUN_BINDING_ERROR_RESPONSE)) {
      LOG_J(LS_VERBOSE, this) <<
          "Ignoring STUN binding response message on shared socket.";
      return;
    }

    // This must be a response for one of our requests.
    // Check success responses, but not errors, for MESSAGE-INTEGRITY.
    if (IsStunSuccessResponseType(msg_type) &&
        !StunMessage::ValidateMessageIntegrity(data, size, hash())) {
      LOG_J(LS_WARNING, this) << "Received TURN message with invalid "
                              << "message integrity, msg_type=" << msg_type;
      return;
    }
    request_manager_.CheckResponse(data, size);
  }
}

void TurnPort::OnReadyToSend(rtc::AsyncPacketSocket* socket) {
  if (connected_) {
    Port::OnReadyToSend();
  }
}


// Update current server address port with the alternate server address port.
bool TurnPort::SetAlternateServer(const rtc::SocketAddress& address) {
  // Check if we have seen this address before and reject if we did.
  AttemptedServerSet::iterator iter = attempted_server_addresses_.find(address);
  if (iter != attempted_server_addresses_.end()) {
    LOG_J(LS_WARNING, this) << "Redirection to ["
                            << address.ToSensitiveString()
                            << "] ignored, allocation failed.";
    return false;
  }

  // If protocol family of server address doesn't match with local, return.
  if (!IsCompatibleAddress(address)) {
    LOG(LS_WARNING) << "Server IP address family does not match with "
                    << "local host address family type";
    return false;
  }

  LOG_J(LS_INFO, this) << "Redirecting from TURN server ["
                       << server_address_.address.ToSensitiveString()
                       << "] to TURN server ["
                       << address.ToSensitiveString()
                       << "]";
  server_address_ = ProtocolAddress(address, server_address_.proto,
                                    server_address_.secure);

  // Insert the current address to prevent redirection pingpong.
  attempted_server_addresses_.insert(server_address_.address);
  return true;
}

void TurnPort::ResolveTurnAddress(const rtc::SocketAddress& address) {
  if (resolver_)
    return;

  resolver_ = socket_factory()->CreateAsyncResolver();
  resolver_->SignalDone.connect(this, &TurnPort::OnResolveResult);
  resolver_->Start(address);
}

void TurnPort::OnResolveResult(rtc::AsyncResolverInterface* resolver) {
  ASSERT(resolver == resolver_);
  // If DNS resolve is failed when trying to connect to the server using TCP,
  // one of the reason could be due to DNS queries blocked by firewall.
  // In such cases we will try to connect to the server with hostname, assuming
  // socket layer will resolve the hostname through a HTTP proxy (if any).
  if (resolver_->GetError() != 0 && server_address_.proto == PROTO_TCP) {
    if (!CreateTurnClientSocket()) {
      OnAllocateError();
    }
    return;
  }

  // Copy the original server address in |resolved_address|. For TLS based
  // sockets we need hostname along with resolved address.
  rtc::SocketAddress resolved_address = server_address_.address;
  if (resolver_->GetError() != 0 ||
      !resolver_->GetResolvedAddress(ip().family(), &resolved_address)) {
    LOG_J(LS_WARNING, this) << "TURN host lookup received error "
                            << resolver_->GetError();
    error_ = resolver_->GetError();
    OnAllocateError();
    return;
  }
  // Signal needs both resolved and unresolved address. After signal is sent
  // we can copy resolved address back into |server_address_|.
  SignalResolvedServerAddress(this, server_address_.address,
                              resolved_address);
  server_address_.address = resolved_address;
  PrepareAddress();
}

void TurnPort::OnSendStunPacket(const void* data, size_t size,
                                StunRequest* request) {
  rtc::PacketOptions options(DefaultDscpValue());
  if (Send(data, size, options) < 0) {
    LOG_J(LS_ERROR, this) << "Failed to send TURN message, err="
                          << socket_->GetError();
  }
}

void TurnPort::OnStunAddress(const rtc::SocketAddress& address) {
  // STUN Port will discover STUN candidate, as it's supplied with first TURN
  // server address.
  // Why not using this address? - P2PTransportChannel will start creating
  // connections after first candidate, which means it could start creating the
  // connections before TURN candidate added. For that to handle, we need to
  // supply STUN candidate from this port to UDPPort, and TurnPort should have
  // handle to UDPPort to pass back the address.
}

void TurnPort::OnAllocateSuccess(const rtc::SocketAddress& address,
                                 const rtc::SocketAddress& stun_address) {
  connected_ = true;

  rtc::SocketAddress related_address = stun_address;
    if (!(candidate_filter() & CF_REFLEXIVE)) {
    // If candidate filter only allows relay type of address, empty raddr to
    // avoid local address leakage.
    related_address = rtc::EmptySocketAddressWithFamily(stun_address.family());
  }

  // For relayed candidate, Base is the candidate itself.
  AddAddress(address,          // Candidate address.
             address,          // Base address.
             related_address,  // Related address.
             UDP_PROTOCOL_NAME,
             "",  // TCP canddiate type, empty for turn candidates.
             RELAY_PORT_TYPE,
             GetRelayPreference(server_address_.proto, server_address_.secure),
             server_priority_,
             true);
}

void TurnPort::OnAllocateError() {
  // We will send SignalPortError asynchronously as this can be sent during
  // port initialization. This way it will not be blocking other port
  // creation.
  thread()->Post(this, MSG_ERROR);
}

void TurnPort::OnMessage(rtc::Message* message) {
  if (message->message_id == MSG_ERROR) {
    SignalPortError(this);
    return;
  } else if (message->message_id == MSG_ALLOCATE_MISMATCH) {
    OnAllocateMismatch();
    return;
  } else if (message->message_id == MSG_TRY_ALTERNATE_SERVER) {
    if (server_address().proto == PROTO_UDP) {
      // Send another allocate request to alternate server, with the received
      // realm and nonce values.
      SendRequest(new TurnAllocateRequest(this), 0);
    } else {
      // Since it's TCP, we have to delete the connected socket and reconnect
      // with the alternate server. PrepareAddress will send stun binding once
      // the new socket is connected.
      ASSERT(server_address().proto == PROTO_TCP);
      ASSERT(!SharedSocket());
      delete socket_;
      socket_ = NULL;
      PrepareAddress();
    }
    return;
  }

  Port::OnMessage(message);
}

void TurnPort::OnAllocateRequestTimeout() {
  OnAllocateError();
}

void TurnPort::HandleDataIndication(const char* data, size_t size,
                                    const rtc::PacketTime& packet_time) {
  // Read in the message, and process according to RFC5766, Section 10.4.
  rtc::ByteBuffer buf(data, size);
  TurnMessage msg;
  if (!msg.Read(&buf)) {
    LOG_J(LS_WARNING, this) << "Received invalid TURN data indication";
    return;
  }

  // Check mandatory attributes.
  const StunAddressAttribute* addr_attr =
      msg.GetAddress(STUN_ATTR_XOR_PEER_ADDRESS);
  if (!addr_attr) {
    LOG_J(LS_WARNING, this) << "Missing STUN_ATTR_XOR_PEER_ADDRESS attribute "
                            << "in data indication.";
    return;
  }

  const StunByteStringAttribute* data_attr =
      msg.GetByteString(STUN_ATTR_DATA);
  if (!data_attr) {
    LOG_J(LS_WARNING, this) << "Missing STUN_ATTR_DATA attribute in "
                            << "data indication.";
    return;
  }

  // Verify that the data came from somewhere we think we have a permission for.
  rtc::SocketAddress ext_addr(addr_attr->GetAddress());
  if (!HasPermission(ext_addr.ipaddr())) {
    LOG_J(LS_WARNING, this) << "Received TURN data indication with invalid "
                            << "peer address, addr="
                            << ext_addr.ToSensitiveString();
    return;
  }

  DispatchPacket(data_attr->bytes(), data_attr->length(), ext_addr,
                 PROTO_UDP, packet_time);
}

void TurnPort::HandleChannelData(int channel_id, const char* data,
                                 size_t size,
                                 const rtc::PacketTime& packet_time) {
  // Read the message, and process according to RFC5766, Section 11.6.
  //    0                   1                   2                   3
  //    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |         Channel Number        |            Length             |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |                                                               |
  //   /                       Application Data                        /
  //   /                                                               /
  //   |                                                               |
  //   |                               +-------------------------------+
  //   |                               |
  //   +-------------------------------+

  // Extract header fields from the message.
  uint16 len = rtc::GetBE16(data + 2);
  if (len > size - TURN_CHANNEL_HEADER_SIZE) {
    LOG_J(LS_WARNING, this) << "Received TURN channel data message with "
                            << "incorrect length, len=" << len;
    return;
  }
  // Allowing messages larger than |len|, as ChannelData can be padded.

  TurnEntry* entry = FindEntry(channel_id);
  if (!entry) {
    LOG_J(LS_WARNING, this) << "Received TURN channel data message for invalid "
                            << "channel, channel_id=" << channel_id;
    return;
  }

  DispatchPacket(data + TURN_CHANNEL_HEADER_SIZE, len, entry->address(),
                 PROTO_UDP, packet_time);
}

void TurnPort::DispatchPacket(const char* data, size_t size,
    const rtc::SocketAddress& remote_addr,
    ProtocolType proto, const rtc::PacketTime& packet_time) {
  if (Connection* conn = GetConnection(remote_addr)) {
    conn->OnReadPacket(data, size, packet_time);
  } else {
    Port::OnReadPacket(data, size, remote_addr, proto);
  }
}

bool TurnPort::ScheduleRefresh(int lifetime) {
  // Lifetime is in seconds; we schedule a refresh for one minute less.
  if (lifetime < 2 * 60) {
    LOG_J(LS_WARNING, this) << "Received response with lifetime that was "
                            << "too short, lifetime=" << lifetime;
    return false;
  }

  SendRequest(new TurnRefreshRequest(this), (lifetime - 60) * 1000);
  return true;
}

void TurnPort::SendRequest(StunRequest* req, int delay) {
  request_manager_.SendDelayed(req, delay);
}

void TurnPort::AddRequestAuthInfo(StunMessage* msg) {
  // If we've gotten the necessary data from the server, add it to our request.
  VERIFY(!hash_.empty());
  VERIFY(msg->AddAttribute(new StunByteStringAttribute(
      STUN_ATTR_USERNAME, credentials_.username)));
  VERIFY(msg->AddAttribute(new StunByteStringAttribute(
      STUN_ATTR_REALM, realm_)));
  VERIFY(msg->AddAttribute(new StunByteStringAttribute(
      STUN_ATTR_NONCE, nonce_)));
  VERIFY(msg->AddMessageIntegrity(hash()));
}

int TurnPort::Send(const void* data, size_t len,
                   const rtc::PacketOptions& options) {
  return socket_->SendTo(data, len, server_address_.address, options);
}

void TurnPort::UpdateHash() {
  VERIFY(ComputeStunCredentialHash(credentials_.username, realm_,
                                   credentials_.password, &hash_));
}

bool TurnPort::UpdateNonce(StunMessage* response) {
  // When stale nonce error received, we should update
  // hash and store realm and nonce.
  // Check the mandatory attributes.
  const StunByteStringAttribute* realm_attr =
      response->GetByteString(STUN_ATTR_REALM);
  if (!realm_attr) {
    LOG(LS_ERROR) << "Missing STUN_ATTR_REALM attribute in "
                  << "stale nonce error response.";
    return false;
  }
  set_realm(realm_attr->GetString());

  const StunByteStringAttribute* nonce_attr =
      response->GetByteString(STUN_ATTR_NONCE);
  if (!nonce_attr) {
    LOG(LS_ERROR) << "Missing STUN_ATTR_NONCE attribute in "
                  << "stale nonce error response.";
    return false;
  }
  set_nonce(nonce_attr->GetString());
  return true;
}

static bool MatchesIP(TurnEntry* e, rtc::IPAddress ipaddr) {
  return e->address().ipaddr() == ipaddr;
}
bool TurnPort::HasPermission(const rtc::IPAddress& ipaddr) const {
  return (std::find_if(entries_.begin(), entries_.end(),
      std::bind2nd(std::ptr_fun(MatchesIP), ipaddr)) != entries_.end());
}

static bool MatchesAddress(TurnEntry* e, rtc::SocketAddress addr) {
  return e->address() == addr;
}
TurnEntry* TurnPort::FindEntry(const rtc::SocketAddress& addr) const {
  EntryList::const_iterator it = std::find_if(entries_.begin(), entries_.end(),
      std::bind2nd(std::ptr_fun(MatchesAddress), addr));
  return (it != entries_.end()) ? *it : NULL;
}

static bool MatchesChannelId(TurnEntry* e, int id) {
  return e->channel_id() == id;
}
TurnEntry* TurnPort::FindEntry(int channel_id) const {
  EntryList::const_iterator it = std::find_if(entries_.begin(), entries_.end(),
      std::bind2nd(std::ptr_fun(MatchesChannelId), channel_id));
  return (it != entries_.end()) ? *it : NULL;
}

TurnEntry* TurnPort::CreateEntry(const rtc::SocketAddress& addr) {
  ASSERT(FindEntry(addr) == NULL);
  TurnEntry* entry = new TurnEntry(this, next_channel_number_++, addr);
  entries_.push_back(entry);
  return entry;
}

void TurnPort::DestroyEntry(const rtc::SocketAddress& addr) {
  TurnEntry* entry = FindEntry(addr);
  ASSERT(entry != NULL);
  entry->SignalDestroyed(entry);
  entries_.remove(entry);
  delete entry;
}

void TurnPort::OnConnectionDestroyed(Connection* conn) {
  // Destroying TurnEntry for the connection, which is already destroyed.
  DestroyEntry(conn->remote_candidate().address());
}

TurnAllocateRequest::TurnAllocateRequest(TurnPort* port)
    : StunRequest(new TurnMessage()),
      port_(port) {
}

void TurnAllocateRequest::Prepare(StunMessage* request) {
  // Create the request as indicated in RFC 5766, Section 6.1.
  request->SetType(TURN_ALLOCATE_REQUEST);
  StunUInt32Attribute* transport_attr = StunAttribute::CreateUInt32(
      STUN_ATTR_REQUESTED_TRANSPORT);
  transport_attr->SetValue(IPPROTO_UDP << 24);
  VERIFY(request->AddAttribute(transport_attr));
  if (!port_->hash().empty()) {
    port_->AddRequestAuthInfo(request);
  }
}

void TurnAllocateRequest::OnResponse(StunMessage* response) {
  // Check mandatory attributes as indicated in RFC5766, Section 6.3.
  const StunAddressAttribute* mapped_attr =
      response->GetAddress(STUN_ATTR_XOR_MAPPED_ADDRESS);
  if (!mapped_attr) {
    LOG_J(LS_WARNING, port_) << "Missing STUN_ATTR_XOR_MAPPED_ADDRESS "
                             << "attribute in allocate success response";
    return;
  }
  // Using XOR-Mapped-Address for stun.
  port_->OnStunAddress(mapped_attr->GetAddress());

  const StunAddressAttribute* relayed_attr =
      response->GetAddress(STUN_ATTR_XOR_RELAYED_ADDRESS);
  if (!relayed_attr) {
    LOG_J(LS_WARNING, port_) << "Missing STUN_ATTR_XOR_RELAYED_ADDRESS "
                             << "attribute in allocate success response";
    return;
  }

  const StunUInt32Attribute* lifetime_attr =
      response->GetUInt32(STUN_ATTR_TURN_LIFETIME);
  if (!lifetime_attr) {
    LOG_J(LS_WARNING, port_) << "Missing STUN_ATTR_TURN_LIFETIME attribute in "
                             << "allocate success response";
    return;
  }
  // Notify the port the allocate succeeded, and schedule a refresh request.
  port_->OnAllocateSuccess(relayed_attr->GetAddress(),
                           mapped_attr->GetAddress());
  port_->ScheduleRefresh(lifetime_attr->value());
}

void TurnAllocateRequest::OnErrorResponse(StunMessage* response) {
  // Process error response according to RFC5766, Section 6.4.
  const StunErrorCodeAttribute* error_code = response->GetErrorCode();
  switch (error_code->code()) {
    case STUN_ERROR_UNAUTHORIZED:       // Unauthrorized.
      OnAuthChallenge(response, error_code->code());
      break;
    case STUN_ERROR_TRY_ALTERNATE:
      OnTryAlternate(response, error_code->code());
      break;
    case STUN_ERROR_ALLOCATION_MISMATCH:
      // We must handle this error async because trying to delete the socket in
      // OnErrorResponse will cause a deadlock on the socket.
      port_->thread()->Post(port_, TurnPort::MSG_ALLOCATE_MISMATCH);
      break;
    default:
      LOG_J(LS_WARNING, port_) << "Allocate response error, code="
                               << error_code->code();
      port_->OnAllocateError();
  }
}

void TurnAllocateRequest::OnTimeout() {
  LOG_J(LS_WARNING, port_) << "Allocate request timeout";
  port_->OnAllocateRequestTimeout();
}

void TurnAllocateRequest::OnAuthChallenge(StunMessage* response, int code) {
  // If we failed to authenticate even after we sent our credentials, fail hard.
  if (code == STUN_ERROR_UNAUTHORIZED && !port_->hash().empty()) {
    LOG_J(LS_WARNING, port_) << "Failed to authenticate with the server "
                             << "after challenge.";
    port_->OnAllocateError();
    return;
  }

  // Check the mandatory attributes.
  const StunByteStringAttribute* realm_attr =
      response->GetByteString(STUN_ATTR_REALM);
  if (!realm_attr) {
    LOG_J(LS_WARNING, port_) << "Missing STUN_ATTR_REALM attribute in "
                             << "allocate unauthorized response.";
    return;
  }
  port_->set_realm(realm_attr->GetString());

  const StunByteStringAttribute* nonce_attr =
      response->GetByteString(STUN_ATTR_NONCE);
  if (!nonce_attr) {
    LOG_J(LS_WARNING, port_) << "Missing STUN_ATTR_NONCE attribute in "
                             << "allocate unauthorized response.";
    return;
  }
  port_->set_nonce(nonce_attr->GetString());

  // Send another allocate request, with the received realm and nonce values.
  port_->SendRequest(new TurnAllocateRequest(port_), 0);
}

void TurnAllocateRequest::OnTryAlternate(StunMessage* response, int code) {

  // According to RFC 5389 section 11, there are use cases where
  // authentication of response is not possible, we're not validating
  // message integrity.

  // Get the alternate server address attribute value.
  const StunAddressAttribute* alternate_server_attr =
      response->GetAddress(STUN_ATTR_ALTERNATE_SERVER);
  if (!alternate_server_attr) {
    LOG_J(LS_WARNING, port_) << "Missing STUN_ATTR_ALTERNATE_SERVER "
                             << "attribute in try alternate error response";
    port_->OnAllocateError();
    return;
  }
  if (!port_->SetAlternateServer(alternate_server_attr->GetAddress())) {
    port_->OnAllocateError();
    return;
  }

  // Check the attributes.
  const StunByteStringAttribute* realm_attr =
      response->GetByteString(STUN_ATTR_REALM);
  if (realm_attr) {
    LOG_J(LS_INFO, port_) << "Applying STUN_ATTR_REALM attribute in "
                          << "try alternate error response.";
    port_->set_realm(realm_attr->GetString());
  }

  const StunByteStringAttribute* nonce_attr =
      response->GetByteString(STUN_ATTR_NONCE);
  if (nonce_attr) {
    LOG_J(LS_INFO, port_) << "Applying STUN_ATTR_NONCE attribute in "
                          << "try alternate error response.";
    port_->set_nonce(nonce_attr->GetString());
  }

  // For TCP, we can't close the original Tcp socket during handling a 300 as
  // we're still inside that socket's event handler. Doing so will cause
  // deadlock.
  port_->thread()->Post(port_, TurnPort::MSG_TRY_ALTERNATE_SERVER);
}

TurnRefreshRequest::TurnRefreshRequest(TurnPort* port)
    : StunRequest(new TurnMessage()),
      port_(port),
      lifetime_(-1) {
}

void TurnRefreshRequest::Prepare(StunMessage* request) {
  // Create the request as indicated in RFC 5766, Section 7.1.
  // No attributes need to be included.
  request->SetType(TURN_REFRESH_REQUEST);
  if (lifetime_ > -1) {
    VERIFY(request->AddAttribute(new StunUInt32Attribute(
        STUN_ATTR_LIFETIME, lifetime_)));
  }

  port_->AddRequestAuthInfo(request);
}

void TurnRefreshRequest::OnResponse(StunMessage* response) {
  // Check mandatory attributes as indicated in RFC5766, Section 7.3.
  const StunUInt32Attribute* lifetime_attr =
      response->GetUInt32(STUN_ATTR_TURN_LIFETIME);
  if (!lifetime_attr) {
    LOG_J(LS_WARNING, port_) << "Missing STUN_ATTR_TURN_LIFETIME attribute in "
                             << "refresh success response.";
    return;
  }

  // Schedule a refresh based on the returned lifetime value.
  port_->ScheduleRefresh(lifetime_attr->value());
}

void TurnRefreshRequest::OnErrorResponse(StunMessage* response) {
  const StunErrorCodeAttribute* error_code = response->GetErrorCode();
  LOG_J(LS_WARNING, port_) << "Refresh response error, code="
                           << error_code->code();

  if (error_code->code() == STUN_ERROR_STALE_NONCE) {
    if (port_->UpdateNonce(response)) {
      // Send RefreshRequest immediately.
      port_->SendRequest(new TurnRefreshRequest(port_), 0);
    }
  }
}

void TurnRefreshRequest::OnTimeout() {
}

TurnCreatePermissionRequest::TurnCreatePermissionRequest(
    TurnPort* port, TurnEntry* entry,
    const rtc::SocketAddress& ext_addr)
    : StunRequest(new TurnMessage()),
      port_(port),
      entry_(entry),
      ext_addr_(ext_addr) {
  entry_->SignalDestroyed.connect(
      this, &TurnCreatePermissionRequest::OnEntryDestroyed);
}

void TurnCreatePermissionRequest::Prepare(StunMessage* request) {
  // Create the request as indicated in RFC5766, Section 9.1.
  request->SetType(TURN_CREATE_PERMISSION_REQUEST);
  VERIFY(request->AddAttribute(new StunXorAddressAttribute(
      STUN_ATTR_XOR_PEER_ADDRESS, ext_addr_)));
  port_->AddRequestAuthInfo(request);
}

void TurnCreatePermissionRequest::OnResponse(StunMessage* response) {
  if (entry_) {
    entry_->OnCreatePermissionSuccess();
  }
}

void TurnCreatePermissionRequest::OnErrorResponse(StunMessage* response) {
  if (entry_) {
    const StunErrorCodeAttribute* error_code = response->GetErrorCode();
    entry_->OnCreatePermissionError(response, error_code->code());
  }
}

void TurnCreatePermissionRequest::OnTimeout() {
  LOG_J(LS_WARNING, port_) << "Create permission timeout";
}

void TurnCreatePermissionRequest::OnEntryDestroyed(TurnEntry* entry) {
  ASSERT(entry_ == entry);
  entry_ = NULL;
}

TurnChannelBindRequest::TurnChannelBindRequest(
    TurnPort* port, TurnEntry* entry,
    int channel_id, const rtc::SocketAddress& ext_addr)
    : StunRequest(new TurnMessage()),
      port_(port),
      entry_(entry),
      channel_id_(channel_id),
      ext_addr_(ext_addr) {
  entry_->SignalDestroyed.connect(
      this, &TurnChannelBindRequest::OnEntryDestroyed);
}

void TurnChannelBindRequest::Prepare(StunMessage* request) {
  // Create the request as indicated in RFC5766, Section 11.1.
  request->SetType(TURN_CHANNEL_BIND_REQUEST);
  VERIFY(request->AddAttribute(new StunUInt32Attribute(
      STUN_ATTR_CHANNEL_NUMBER, channel_id_ << 16)));
  VERIFY(request->AddAttribute(new StunXorAddressAttribute(
      STUN_ATTR_XOR_PEER_ADDRESS, ext_addr_)));
  port_->AddRequestAuthInfo(request);
}

void TurnChannelBindRequest::OnResponse(StunMessage* response) {
  if (entry_) {
    entry_->OnChannelBindSuccess();
    // Refresh the channel binding just under the permission timeout
    // threshold. The channel binding has a longer lifetime, but
    // this is the easiest way to keep both the channel and the
    // permission from expiring.
    entry_->SendChannelBindRequest(TURN_PERMISSION_TIMEOUT - 60 * 1000);
  }
}

void TurnChannelBindRequest::OnErrorResponse(StunMessage* response) {
  if (entry_) {
    const StunErrorCodeAttribute* error_code = response->GetErrorCode();
    entry_->OnChannelBindError(response, error_code->code());
  }
}

void TurnChannelBindRequest::OnTimeout() {
  LOG_J(LS_WARNING, port_) << "Channel bind timeout";
}

void TurnChannelBindRequest::OnEntryDestroyed(TurnEntry* entry) {
  ASSERT(entry_ == entry);
  entry_ = NULL;
}

TurnEntry::TurnEntry(TurnPort* port, int channel_id,
                     const rtc::SocketAddress& ext_addr)
    : port_(port),
      channel_id_(channel_id),
      ext_addr_(ext_addr),
      state_(STATE_UNBOUND) {
  // Creating permission for |ext_addr_|.
  SendCreatePermissionRequest();
}

void TurnEntry::SendCreatePermissionRequest() {
  port_->SendRequest(new TurnCreatePermissionRequest(
      port_, this, ext_addr_), 0);
}

void TurnEntry::SendChannelBindRequest(int delay) {
  port_->SendRequest(new TurnChannelBindRequest(
      port_, this, channel_id_, ext_addr_), delay);
}

int TurnEntry::Send(const void* data, size_t size, bool payload,
                    const rtc::PacketOptions& options) {
  rtc::ByteBuffer buf;
  if (state_ != STATE_BOUND) {
    // If we haven't bound the channel yet, we have to use a Send Indication.
    TurnMessage msg;
    msg.SetType(TURN_SEND_INDICATION);
    msg.SetTransactionID(
        rtc::CreateRandomString(kStunTransactionIdLength));
    VERIFY(msg.AddAttribute(new StunXorAddressAttribute(
        STUN_ATTR_XOR_PEER_ADDRESS, ext_addr_)));
    VERIFY(msg.AddAttribute(new StunByteStringAttribute(
        STUN_ATTR_DATA, data, size)));
    VERIFY(msg.Write(&buf));

    // If we're sending real data, request a channel bind that we can use later.
    if (state_ == STATE_UNBOUND && payload) {
      SendChannelBindRequest(0);
      state_ = STATE_BINDING;
    }
  } else {
    // If the channel is bound, we can send the data as a Channel Message.
    buf.WriteUInt16(channel_id_);
    buf.WriteUInt16(static_cast<uint16>(size));
    buf.WriteBytes(reinterpret_cast<const char*>(data), size);
  }
  return port_->Send(buf.Data(), buf.Length(), options);
}

void TurnEntry::OnCreatePermissionSuccess() {
  LOG_J(LS_INFO, port_) << "Create permission for "
                        << ext_addr_.ToSensitiveString()
                        << " succeeded";
  // For success result code will be 0.
  port_->SignalCreatePermissionResult(port_, ext_addr_, 0);
}

void TurnEntry::OnCreatePermissionError(StunMessage* response, int code) {
  LOG_J(LS_WARNING, port_) << "Create permission for "
                           << ext_addr_.ToSensitiveString()
                           << " failed, code=" << code;
  if (code == STUN_ERROR_STALE_NONCE) {
    if (port_->UpdateNonce(response)) {
      SendCreatePermissionRequest();
    }
  } else {
    // Send signal with error code.
    port_->SignalCreatePermissionResult(port_, ext_addr_, code);
  }
}

void TurnEntry::OnChannelBindSuccess() {
  LOG_J(LS_INFO, port_) << "Channel bind for " << ext_addr_.ToSensitiveString()
                        << " succeeded";
  ASSERT(state_ == STATE_BINDING || state_ == STATE_BOUND);
  state_ = STATE_BOUND;
}

void TurnEntry::OnChannelBindError(StunMessage* response, int code) {
  // TODO(mallinath) - Implement handling of error response for channel
  // bind request as per http://tools.ietf.org/html/rfc5766#section-11.3
  LOG_J(LS_WARNING, port_) << "Channel bind for "
                           << ext_addr_.ToSensitiveString()
                           << " failed, code=" << code;
  if (code == STUN_ERROR_STALE_NONCE) {
    if (port_->UpdateNonce(response)) {
      // Send channel bind request with fresh nonce.
      SendChannelBindRequest(0);
    }
  }
}

}  // namespace cricket
