/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_DTLS_DTLS_TRANSPORT_H_
#define P2P_DTLS_DTLS_TRANSPORT_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/crypto/crypto_options.h"
#include "api/dtls_transport_interface.h"
#include "api/rtc_error.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/dtls/dtls_stun_piggyback_controller.h"
#include "p2p/dtls/dtls_transport_internal.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/buffer.h"
#include "rtc_base/buffer_queue.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/network_route.h"
#include "rtc_base/socket.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/ssl_fingerprint.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/stream.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread_annotations.h"

namespace rtc {
class PacketTransportInternal;
}

namespace cricket {

// A bridge between a packet-oriented/transport-type interface on
// the bottom and a StreamInterface on the top.
class StreamInterfaceChannel : public rtc::StreamInterface {
 public:
  explicit StreamInterfaceChannel(webrtc::IceTransportInternal* ice_transport);

  void SetDtlsStunPiggybackController(
      DtlsStunPiggybackController* dtls_stun_piggyback_controller);

  StreamInterfaceChannel(const StreamInterfaceChannel&) = delete;
  StreamInterfaceChannel& operator=(const StreamInterfaceChannel&) = delete;

  // Push in a packet; this gets pulled out from Read().
  bool OnPacketReceived(const char* data, size_t size);

  // Implementations of StreamInterface
  rtc::StreamState GetState() const override;
  void Close() override;
  rtc::StreamResult Read(rtc::ArrayView<uint8_t> buffer,
                         size_t& read,
                         int& error) override;
  rtc::StreamResult Write(rtc::ArrayView<const uint8_t> data,
                          size_t& written,
                          int& error) override;

 private:
  webrtc::IceTransportInternal* const ice_transport_;  // owned by DtlsTransport
  DtlsStunPiggybackController* dtls_stun_piggyback_controller_ =
      nullptr;  // owned by DtlsTransport
  rtc::StreamState state_ RTC_GUARDED_BY(callback_sequence_);
  rtc::BufferQueue packets_ RTC_GUARDED_BY(callback_sequence_);
};

// This class provides a DTLS SSLStreamAdapter inside a TransportChannel-style
// packet-based interface, wrapping an existing TransportChannel instance
// (e.g a P2PTransportChannel)
// Here's the way this works:
//
//   DtlsTransport {
//       SSLStreamAdapter* dtls_ {
//           StreamInterfaceChannel downward_ {
//               IceTransportInternal* ice_transport_;
//           }
//       }
//   }
//
//   - Data which comes into DtlsTransport from the underlying
//     ice_transport_ via OnReadPacket() is checked for whether it is DTLS
//     or not, and if it is, is passed to DtlsTransport::HandleDtlsPacket,
//     which pushes it into to downward_. dtls_ is listening for events on
//     downward_, so it immediately calls downward_->Read().
//
//   - Data written to DtlsTransport is passed either to downward_ or directly
//     to ice_transport_, depending on whether DTLS is negotiated and whether
//     the flags include PF_SRTP_BYPASS
//
//   - The SSLStreamAdapter writes to downward_->Write() which translates it
//     into packet writes on ice_transport_.
//
// This class is not thread safe; all methods must be called on the same thread
// as the constructor.
class DtlsTransport : public DtlsTransportInternal {
 public:
  // `ice_transport` is the ICE transport this DTLS transport is wrapping.  It
  // must outlive this DTLS transport.
  //
  // `crypto_options` are the options used for the DTLS handshake. This affects
  // whether GCM crypto suites are negotiated.
  //
  // `event_log` is an optional RtcEventLog for logging state changes. It should
  // outlive the DtlsTransport.
  DtlsTransport(
      webrtc::IceTransportInternal* ice_transport,
      const webrtc::CryptoOptions& crypto_options,
      webrtc::RtcEventLog* event_log,
      webrtc::SSLProtocolVersion max_version = webrtc::SSL_PROTOCOL_DTLS_12);

  ~DtlsTransport() override;

  DtlsTransport(const DtlsTransport&) = delete;
  DtlsTransport& operator=(const DtlsTransport&) = delete;

  webrtc::DtlsTransportState dtls_state() const override;
  const std::string& transport_name() const override;
  int component() const override;

  // DTLS is active if a local certificate was set. Otherwise this acts in a
  // "passthrough" mode, sending packets directly through the underlying ICE
  // transport.
  // TODO(deadbeef): Remove this weirdness, and handle it in the upper layers.
  bool IsDtlsActive() const override;

  // SetLocalCertificate is what makes DTLS active. It must be called before
  // SetRemoteFinterprint.
  // TODO(deadbeef): Once DtlsTransport no longer has the concept of being
  // "active" or not (acting as a passthrough if not active), just require this
  // certificate on construction or "Start".
  bool SetLocalCertificate(
      const rtc::scoped_refptr<webrtc::RTCCertificate>& certificate) override;
  rtc::scoped_refptr<webrtc::RTCCertificate> GetLocalCertificate()
      const override;

  // SetRemoteFingerprint must be called after SetLocalCertificate, and any
  // other methods like SetDtlsRole. It's what triggers the actual DTLS setup.
  // TODO(deadbeef): Rename to "Start" like in ORTC?
  bool SetRemoteFingerprint(absl::string_view digest_alg,
                            const uint8_t* digest,
                            size_t digest_len) override;

  // SetRemoteParameters must be called after SetLocalCertificate.
  webrtc::RTCError SetRemoteParameters(
      absl::string_view digest_alg,
      const uint8_t* digest,
      size_t digest_len,
      std::optional<webrtc::SSLRole> role) override;

  // Called to send a packet (via DTLS, if turned on).
  int SendPacket(const char* data,
                 size_t size,
                 const rtc::PacketOptions& options,
                 int flags) override;

  bool GetOption(webrtc::Socket::Option opt, int* value) override;

  // Find out which TLS version was negotiated
  bool GetSslVersionBytes(int* version) const override;
  // Find out which DTLS-SRTP cipher was negotiated
  bool GetSrtpCryptoSuite(int* cipher) const override;

  // Find out which signature algorithm was used by the peer. Returns values
  // from
  // https://www.iana.org/assignments/tls-parameters/tls-parameters.xhtml#tls-signaturescheme
  // If not applicable, it returns zero.
  uint16_t GetSslPeerSignatureAlgorithm() const override;

  bool GetDtlsRole(webrtc::SSLRole* role) const override;
  bool SetDtlsRole(webrtc::SSLRole role) override;

  // Find out which DTLS cipher was negotiated
  bool GetSslCipherSuite(int* cipher) const override;
  std::optional<absl::string_view> GetTlsCipherSuiteName() const override;

  // Once DTLS has been established, this method retrieves the certificate
  // chain in use by the remote peer, for use in external identity
  // verification.
  std::unique_ptr<rtc::SSLCertChain> GetRemoteSSLCertChain() const override;

  // Once DTLS has established (i.e., this ice_transport is writable), this
  // method extracts the keys negotiated during the DTLS handshake, for use in
  // external encryption. DTLS-SRTP uses this to extract the needed SRTP keys.
  bool ExportSrtpKeyingMaterial(
      rtc::ZeroOnFreeBuffer<uint8_t>& keying_material) override;

  webrtc::IceTransportInternal* ice_transport() override;

  // For informational purposes. Tells if the DTLS handshake has finished.
  // This may be true even if writable() is false, if the remote fingerprint
  // has not yet been verified.
  bool IsDtlsConnected();

  bool receiving() const override;
  bool writable() const override;

  int GetError() override;

  std::optional<rtc::NetworkRoute> network_route() const override;

  int SetOption(webrtc::Socket::Option opt, int value) override;

  std::string ToString() const {
    const absl::string_view RECEIVING_ABBREV[2] = {"_", "R"};
    const absl::string_view WRITABLE_ABBREV[2] = {"_", "W"};
    rtc::StringBuilder sb;
    sb << "DtlsTransport[" << transport_name() << "|" << component_ << "|"
       << RECEIVING_ABBREV[receiving()] << WRITABLE_ABBREV[writable()] << "]";
    return sb.Release();
  }

  // Two methods for testing.
  bool IsDtlsPiggybackSupportedByPeer();
  bool WasDtlsCompletedByPiggybacking();

 private:
  void ConnectToIceTransport();

  void OnWritableState(rtc::PacketTransportInternal* transport);
  void OnReadPacket(rtc::PacketTransportInternal* transport,
                    const rtc::ReceivedPacket& packet,
                    bool piggybacked);
  void OnSentPacket(rtc::PacketTransportInternal* transport,
                    const rtc::SentPacket& sent_packet);
  void OnReadyToSend(rtc::PacketTransportInternal* transport);
  void OnReceivingState(rtc::PacketTransportInternal* transport);
  void OnDtlsEvent(int sig, int err);
  void OnNetworkRouteChanged(std::optional<rtc::NetworkRoute> network_route);
  bool SetupDtls();
  void MaybeStartDtls();
  bool HandleDtlsPacket(rtc::ArrayView<const uint8_t> payload);
  void OnDtlsHandshakeError(webrtc::SSLHandshakeError error);
  void ConfigureHandshakeTimeout();

  void set_receiving(bool receiving);
  void set_writable(bool writable);
  // Sets the DTLS state, signaling if necessary.
  void set_dtls_state(webrtc::DtlsTransportState state);
  void SetPiggybackDtlsDataCallback(
      absl::AnyInvocable<void(rtc::PacketTransportInternal* transport,
                              const rtc::ReceivedPacket& packet)> callback);
  void PeriodicRetransmitDtlsPacketUntilDtlsConnected();

  RTC_NO_UNIQUE_ADDRESS webrtc::SequenceChecker thread_checker_;

  const int component_;
  webrtc::DtlsTransportState dtls_state_ = webrtc::DtlsTransportState::kNew;
  // Underlying ice_transport, not owned by this class.
  webrtc::IceTransportInternal* const ice_transport_;
  std::unique_ptr<webrtc::SSLStreamAdapter> dtls_;  // The DTLS stream
  StreamInterfaceChannel*
      downward_;  // Wrapper for ice_transport_, owned by dtls_.
  const std::vector<int> srtp_ciphers_;  // SRTP ciphers to use with DTLS.
  bool dtls_active_ = false;
  rtc::scoped_refptr<webrtc::RTCCertificate> local_certificate_;
  std::optional<webrtc::SSLRole> dtls_role_;
  const webrtc::SSLProtocolVersion ssl_max_version_;
  rtc::Buffer remote_fingerprint_value_;
  std::string remote_fingerprint_algorithm_;

  // Cached DTLS ClientHello packet that was received before we started the
  // DTLS handshake. This could happen if the hello was received before the
  // ice transport became writable, or before a remote fingerprint was received.
  rtc::Buffer cached_client_hello_;

  bool receiving_ = false;
  bool writable_ = false;

  // Keep track if ICE has ever been writable.
  // This is used to prevent "spurious" Dtls::Writable with DTLS-in-STUN,
  // where DTLS can become writable before ICE. This can confuse other parts
  // of the stack.
  bool ice_has_been_writable_ = false;

  webrtc::RtcEventLog* const event_log_;

  // Initialized in constructor based on WebRTC-IceHandshakeDtls,
  // (so that we return PIGGYBACK_ACK to client if we get STUN_BINDING_REQUEST
  // directly). Maybe disabled in SetupDtls has been called.
  bool dtls_in_stun_ = false;

  // A controller for piggybacking DTLS in STUN.
  DtlsStunPiggybackController dtls_stun_piggyback_controller_;

  absl::AnyInvocable<void(rtc::PacketTransportInternal*,
                          const rtc::ReceivedPacket&)>
      piggybacked_dtls_callback_;

  // When ICE get writable during dtls piggybacked handshake
  // there is currently no safe way of updating the timeout
  // in boringssl (that is work in progress). Therefore
  // DtlsTransport has a "hack" to periodically retransmit.
  bool pending_periodic_retransmit_dtls_packet_ = false;
  webrtc::ScopedTaskSafetyDetached safety_flag_;
};

}  // namespace cricket

#endif  // P2P_DTLS_DTLS_TRANSPORT_H_
