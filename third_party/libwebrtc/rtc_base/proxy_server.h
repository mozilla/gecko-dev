/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_PROXY_SERVER_H_
#define RTC_BASE_PROXY_SERVER_H_

#include <memory>
#include <vector>

#include "absl/memory/memory.h"
#include "rtc_base/memory/fifo_buffer.h"
#include "rtc_base/server_socket_adapters.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/socket_factory.h"

namespace rtc {

// ProxyServer is a base class that allows for easy construction of proxy
// servers. With its helper class ProxyBinding, it contains all the necessary
// logic for receiving and bridging connections. The specific client-server
// proxy protocol is implemented by an instance of the AsyncProxyServerSocket
// class; children of ProxyServer implement WrapSocket appropriately to return
// the correct protocol handler.

class ProxyBinding : public sigslot::has_slots<> {
 public:
  ProxyBinding(webrtc::AsyncProxyServerSocket* in_socket,
               webrtc::Socket* out_socket);
  ~ProxyBinding() override;

  ProxyBinding(const ProxyBinding&) = delete;
  ProxyBinding& operator=(const ProxyBinding&) = delete;

  sigslot::signal1<ProxyBinding*> SignalDestroyed;

 private:
  void OnConnectRequest(webrtc::AsyncProxyServerSocket* socket,
                        const webrtc::SocketAddress& addr);
  void OnInternalRead(webrtc::Socket* socket);
  void OnInternalWrite(webrtc::Socket* socket);
  void OnInternalClose(webrtc::Socket* socket, int err);
  void OnExternalConnect(webrtc::Socket* socket);
  void OnExternalRead(webrtc::Socket* socket);
  void OnExternalWrite(webrtc::Socket* socket);
  void OnExternalClose(webrtc::Socket* socket, int err);

  static void Read(webrtc::Socket* socket, FifoBuffer* buffer);
  static void Write(webrtc::Socket* socket, FifoBuffer* buffer);
  void Destroy();

  static const int kBufferSize = 4096;
  std::unique_ptr<webrtc::AsyncProxyServerSocket> int_socket_;
  std::unique_ptr<webrtc::Socket> ext_socket_;
  bool connected_;
  FifoBuffer out_buffer_;
  FifoBuffer in_buffer_;
};

class ProxyServer : public sigslot::has_slots<> {
 public:
  ProxyServer(webrtc::SocketFactory* int_factory,
              const webrtc::SocketAddress& int_addr,
              webrtc::SocketFactory* ext_factory,
              const webrtc::SocketAddress& ext_ip);
  ~ProxyServer() override;

  ProxyServer(const ProxyServer&) = delete;
  ProxyServer& operator=(const ProxyServer&) = delete;

  // Returns the address to which the proxy server is bound
  webrtc::SocketAddress GetServerAddress();

 protected:
  void OnAcceptEvent(webrtc::Socket* socket);
  virtual webrtc::AsyncProxyServerSocket* WrapSocket(
      webrtc::Socket* socket) = 0;

 private:
  webrtc::SocketFactory* ext_factory_;
  webrtc::SocketAddress ext_ip_;
  std::unique_ptr<webrtc::Socket> server_socket_;
  std::vector<std::unique_ptr<ProxyBinding>> bindings_;
};

}  // namespace rtc

#endif  // RTC_BASE_PROXY_SERVER_H_
