/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MTRANSPORTHANDLER_H__
#define _MTRANSPORTHANDLER_H__

#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"
#include "transport/transportlayer.h"  // Need the State enum
#include "transport/dtlsidentity.h"    // For DtlsDigest
#include "mozilla/dom/RTCPeerConnectionBinding.h"
#include "mozilla/dom/RTCConfigurationBinding.h"
#include "mozilla/dom/RTCIceTransportBinding.h"  // RTCIceTransportState
#include "transport/nricectx.h"                  // Need some enums
#include "common/CandidateInfo.h"
#include "transport/nr_socket_proxy_config.h"
#include "RTCStatsReport.h"
#include "MediaEventSource.h"

#include "nsString.h"

#include <string>
#include <set>
#include <vector>

namespace mozilla {
class DtlsIdentity;
class NrIceCtx;
class NrIceMediaStream;
class NrIceResolver;
class TransportFlow;
class RTCStatsQuery;

namespace dom {
struct RTCStatsReportInternal;
}

class MediaTransportHandler {
 public:
  // Creates either a MediaTransportHandlerSTS or a MediaTransportHandlerIPC,
  // as appropriate.
  static already_AddRefed<MediaTransportHandler> Create();

  explicit MediaTransportHandler()
      : mStateCacheMutex("MediaTransportHandler::mStateCacheMutex") {}

  // Exposed so we can synchronously validate ICE servers from PeerConnection
  static nsresult ConvertIceServers(
      const nsTArray<dom::RTCIceServer>& aIceServers,
      std::vector<NrIceStunServer>* aStunServers,
      std::vector<NrIceTurnServer>* aTurnServers);

  typedef MozPromise<dom::Sequence<nsString>, nsresult, true> IceLogPromise;

  virtual void Initialize() {}

  // There's a wrinkle here; the ICE logging is not separated out by
  // MediaTransportHandler. These are a little more like static methods, but
  // to avoid needing yet another IPC interface, we bolt them on here.
  virtual RefPtr<IceLogPromise> GetIceLog(const nsCString& aPattern) = 0;
  virtual void ClearIceLog() = 0;
  virtual void EnterPrivateMode() = 0;
  virtual void ExitPrivateMode() = 0;

  virtual void CreateIceCtx(const std::string& aName) = 0;

  virtual nsresult SetIceConfig(const nsTArray<dom::RTCIceServer>& aIceServers,
                                dom::RTCIceTransportPolicy aIcePolicy) = 0;

  // We will probably be able to move the proxy lookup stuff into
  // this class once we move mtransport to its own process.
  virtual void SetProxyConfig(NrSocketProxyConfig&& aProxyConfig) = 0;

  virtual void EnsureProvisionalTransport(const std::string& aTransportId,
                                          const std::string& aLocalUfrag,
                                          const std::string& aLocalPwd,
                                          int aComponentCount) = 0;

  virtual void SetTargetForDefaultLocalAddressLookup(
      const std::string& aTargetIp, uint16_t aTargetPort) = 0;

  // We set default-route-only as late as possible because it depends on what
  // capture permissions have been granted on the window, which could easily
  // change between Init (ie; when the PC is created) and StartIceGathering
  // (ie; when we set the local description).
  virtual void StartIceGathering(bool aDefaultRouteOnly,
                                 bool aObfuscateHostAddresses,
                                 // TODO: It probably makes sense to look
                                 // this up internally
                                 const nsTArray<NrIceStunAddr>& aStunAddrs) = 0;

  virtual void ActivateTransport(
      const std::string& aTransportId, const std::string& aLocalUfrag,
      const std::string& aLocalPwd, size_t aComponentCount,
      const std::string& aUfrag, const std::string& aPassword,
      const nsTArray<uint8_t>& aKeyDer, const nsTArray<uint8_t>& aCertDer,
      SSLKEAType aAuthType, bool aDtlsClient, const DtlsDigestList& aDigests,
      bool aPrivacyRequested) = 0;

  virtual void RemoveTransportsExcept(
      const std::set<std::string>& aTransportIds) = 0;

  virtual void StartIceChecks(bool aIsControlling,
                              const std::vector<std::string>& aIceOptions) = 0;

  virtual void SendPacket(const std::string& aTransportId,
                          MediaPacket&& aPacket) = 0;

  virtual void AddIceCandidate(const std::string& aTransportId,
                               const std::string& aCandidate,
                               const std::string& aUFrag,
                               const std::string& aObfuscatedAddress) = 0;

  virtual void UpdateNetworkState(bool aOnline) = 0;

  virtual RefPtr<dom::RTCStatsPromise> GetIceStats(
      const std::string& aTransportId, DOMHighResTimeStamp aNow) = 0;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_DESTROY(MediaTransportHandler,
                                                     Destroy())

  TransportLayer::State GetState(const std::string& aTransportId,
                                 bool aRtcp) const;

  MediaEventSourceOneCopyPerThread<std::string, MediaPacket>&
  GetRtpPacketReceived() {
    return mRtpPacketReceived;
  }

  MediaEventSourceOneCopyPerThread<std::string, MediaPacket>&
  GetSctpPacketReceived() {
    return mSctpPacketReceived;
  }

  MediaEventSource<std::string, CandidateInfo>& GetCandidateGathered() {
    return mCandidateGathered;
  }

  MediaEventSource<std::string, bool>& GetAlpnNegotiated() {
    return mAlpnNegotiated;
  }

  MediaEventSource<std::string, dom::RTCIceGathererState>&
  GetGatheringStateChange() {
    return mGatheringStateChange;
  }
  MediaEventSource<std::string, dom::RTCIceTransportState>&
  GetConnectionStateChange() {
    return mConnectionStateChange;
  }
  MediaEventSource<std::string, MediaPacket>& GetEncryptedSending() {
    return mEncryptedSending;
  }
  MediaEventSource<std::string, TransportLayer::State>& GetStateChange() {
    return mStateChange;
  }
  MediaEventSource<std::string, TransportLayer::State>& GetRtcpStateChange() {
    return mRtcpStateChange;
  }

 protected:
  void OnCandidate(const std::string& aTransportId,
                   CandidateInfo&& aCandidateInfo);
  void OnAlpnNegotiated(const std::string& aAlpn);
  void OnGatheringStateChange(const std::string& aTransportId,
                              dom::RTCIceGathererState aState);
  void OnConnectionStateChange(const std::string& aTransportId,
                               dom::RTCIceTransportState aState);
  void OnPacketReceived(std::string&& aTransportId, MediaPacket&& aPacket);
  void OnEncryptedSending(const std::string& aTransportId,
                          MediaPacket&& aPacket);
  void OnStateChange(const std::string& aTransportId,
                     TransportLayer::State aState);
  void OnRtcpStateChange(const std::string& aTransportId,
                         TransportLayer::State aState);
  virtual void Destroy() = 0;
  virtual ~MediaTransportHandler() = default;
  mutable Mutex mStateCacheMutex;
  std::map<std::string, TransportLayer::State> mStateCache;
  std::map<std::string, TransportLayer::State> mRtcpStateCache;

  // Just RTP/RTCP
  MediaEventProducerOneCopyPerThread<std::string, MediaPacket>
      mRtpPacketReceived;
  // Just SCTP
  MediaEventProducerOneCopyPerThread<std::string, MediaPacket>
      mSctpPacketReceived;
  MediaEventProducer<std::string, CandidateInfo> mCandidateGathered;
  MediaEventProducer<std::string, bool> mAlpnNegotiated;
  MediaEventProducer<std::string, dom::RTCIceGathererState>
      mGatheringStateChange;
  MediaEventProducer<std::string, dom::RTCIceTransportState>
      mConnectionStateChange;
  MediaEventProducer<std::string, MediaPacket> mEncryptedSending;
  MediaEventProducer<std::string, TransportLayer::State> mStateChange;
  MediaEventProducer<std::string, TransportLayer::State> mRtcpStateChange;
};

void TokenizeCandidate(const std::string& aCandidate,
                       std::vector<std::string>& aTokens);

}  // namespace mozilla

#endif  //_MTRANSPORTHANDLER_H__
