/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "logging.h"
#include "nsIGfxInfo.h"
#include "nsServiceManagerUtils.h"

#include "PeerConnectionImpl.h"
#include "PeerConnectionMedia.h"
#include "MediaPipelineFactory.h"
#include "transportflow.h"
#include "transportlayer.h"
#include "transportlayerdtls.h"
#include "transportlayerice.h"

#include "signaling/src/jsep/JsepTrack.h"
#include "signaling/src/jsep/JsepTransport.h"
#include "signaling/src/common/PtrVector.h"

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
#include "MediaStreamTrack.h"
#include "nsIPrincipal.h"
#include "nsIDocument.h"
#include "mozilla/Preferences.h"
#include "MediaEngine.h"
#endif

#include "GmpVideoCodec.h"
#ifdef MOZ_WEBRTC_OMX
#include "OMXVideoCodec.h"
#include "OMXCodecWrapper.h"
#endif

#ifdef MOZ_WEBRTC_MEDIACODEC
#include "MediaCodecVideoCodec.h"
#endif

#ifdef MOZILLA_INTERNAL_API
#include "mozilla/Preferences.h"
#endif

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
#include "WebrtcGmpVideoCodec.h"
#endif

#include <stdlib.h>

namespace mozilla {

MOZ_MTLOG_MODULE("MediaPipelineFactory")

static nsresult
JsepCodecDescToCodecConfig(const JsepCodecDescription& aCodec,
                           AudioCodecConfig** aConfig)
{
  MOZ_ASSERT(aCodec.mType == SdpMediaSection::kAudio);
  if (aCodec.mType != SdpMediaSection::kAudio)
    return NS_ERROR_INVALID_ARG;

  const JsepAudioCodecDescription& desc =
      static_cast<const JsepAudioCodecDescription&>(aCodec);

  uint16_t pt;

  if (!desc.GetPtAsInt(&pt)) {
    MOZ_MTLOG(ML_ERROR, "Invalid payload type: " << desc.mDefaultPt);
    return NS_ERROR_INVALID_ARG;
  }

  *aConfig = new AudioCodecConfig(pt,
                                  desc.mName,
                                  desc.mClock,
                                  desc.mPacketSize,
                                  desc.mChannels,
                                  desc.mBitrate);

  return NS_OK;
}

static nsresult
JsepCodecDescToCodecConfig(const JsepCodecDescription& aCodec,
                           VideoCodecConfig** aConfig)
{
  MOZ_ASSERT(aCodec.mType == SdpMediaSection::kVideo);
  if (aCodec.mType != SdpMediaSection::kVideo) {
    MOZ_ASSERT(false, "JsepCodecDescription has wrong type");
    return NS_ERROR_INVALID_ARG;
  }

  const JsepVideoCodecDescription& desc =
      static_cast<const JsepVideoCodecDescription&>(aCodec);

  uint16_t pt;

  if (!desc.GetPtAsInt(&pt)) {
    MOZ_MTLOG(ML_ERROR, "Invalid payload type: " << desc.mDefaultPt);
    return NS_ERROR_INVALID_ARG;
  }

  ScopedDeletePtr<VideoCodecConfigH264> h264Config;

  if (desc.mName == "H264") {
    h264Config = new VideoCodecConfigH264;
    size_t spropSize = sizeof(h264Config->sprop_parameter_sets);
    strncpy(h264Config->sprop_parameter_sets,
            desc.mSpropParameterSets.c_str(),
            spropSize);
    h264Config->sprop_parameter_sets[spropSize - 1] = '\0';
    h264Config->packetization_mode = desc.mPacketizationMode;
    h264Config->profile_level_id = desc.mProfileLevelId;
    h264Config->max_mbps = desc.mMaxMbps;
    h264Config->max_fs = desc.mMaxFs;
    h264Config->max_cpb = desc.mMaxCpb;
    h264Config->max_dpb = desc.mMaxDpb;
    h264Config->max_br = desc.mMaxBr;
    h264Config->tias_bw = 0; // TODO. Issue 165.
  }

  VideoCodecConfig* configRaw;
  configRaw = new VideoCodecConfig(
      pt, desc.mName, desc.mMaxFs, desc.mMaxFr, h264Config);

  configRaw->mAckFbTypes = desc.mAckFbTypes;
  configRaw->mNackFbTypes = desc.mNackFbTypes;
  configRaw->mCcmFbTypes = desc.mCcmFbTypes;

  *aConfig = configRaw;
  return NS_OK;
}

// Accessing the PCMedia should be safe here because we shouldn't
// have enqueued this function unless it was still active and
// the ICE data is destroyed on the STS.
static void
FinalizeTransportFlow_s(RefPtr<PeerConnectionMedia> aPCMedia,
                        RefPtr<TransportFlow> aFlow, size_t aLevel,
                        bool aIsRtcp,
                        nsAutoPtr<PtrVector<TransportLayer> > aLayerList)
{
  TransportLayerIce* ice =
      static_cast<TransportLayerIce*>(aLayerList->values.front());
  ice->SetParameters(
      aPCMedia->ice_ctx(), aPCMedia->ice_media_stream(aLevel), aIsRtcp ? 2 : 1);
  nsAutoPtr<std::queue<TransportLayer*> > layerQueue(
      new std::queue<TransportLayer*>);
  for (auto i = aLayerList->values.begin(); i != aLayerList->values.end();
       ++i) {
    layerQueue->push(*i);
  }
  aLayerList->values.clear();
  (void)aFlow->PushLayers(layerQueue); // TODO(bug 854518): Process errors.
}

nsresult
MediaPipelineFactory::CreateOrGetTransportFlow(
    size_t aLevel,
    bool aIsRtcp,
    const JsepTransport& aTransport,
    RefPtr<TransportFlow>* aFlowOutparam)
{
  nsresult rv;
  RefPtr<TransportFlow> flow;

  flow = mPCMedia->GetTransportFlow(aLevel, aIsRtcp);
  if (flow) {
    *aFlowOutparam = flow;
    return NS_OK;
  }

  std::ostringstream osId;
  osId << mPC->GetHandle() << ":" << aLevel << ","
       << (aIsRtcp ? "rtcp" : "rtp");
  flow = new TransportFlow(osId.str());

  // The media streams are made on STS so we need to defer setup.
  auto ice = MakeUnique<TransportLayerIce>(mPC->GetHandle());
  auto dtls = MakeUnique<TransportLayerDtls>();
  dtls->SetRole(aTransport.mDtls->GetRole() ==
                        JsepDtlsTransport::kJsepDtlsClient
                    ? TransportLayerDtls::CLIENT
                    : TransportLayerDtls::SERVER);

  RefPtr<DtlsIdentity> pcid = mPC->GetIdentity();
  if (!pcid) {
    MOZ_MTLOG(ML_ERROR, "Failed to get DTLS identity.");
    return NS_ERROR_FAILURE;
  }
  dtls->SetIdentity(pcid);

  const SdpFingerprintAttributeList& fingerprints =
      aTransport.mDtls->GetFingerprints();
  for (auto fp = fingerprints.mFingerprints.begin();
       fp != fingerprints.mFingerprints.end();
       ++fp) {
    std::ostringstream ss;
    ss << fp->hashFunc;
    rv = dtls->SetVerificationDigest(ss.str(), &fp->fingerprint[0],
                                     fp->fingerprint.size());
    if (NS_FAILED(rv)) {
      MOZ_MTLOG(ML_ERROR, "Could not set fingerprint");
      return rv;
    }
  }

  std::vector<uint16_t> srtpCiphers;
  srtpCiphers.push_back(SRTP_AES128_CM_HMAC_SHA1_80);
  srtpCiphers.push_back(SRTP_AES128_CM_HMAC_SHA1_32);

  rv = dtls->SetSrtpCiphers(srtpCiphers);
  if (NS_FAILED(rv)) {
    MOZ_MTLOG(ML_ERROR, "Couldn't set SRTP ciphers");
    return rv;
  }

  // Always permits negotiation of the confidential mode.
  // Only allow non-confidential (which is an allowed default),
  // if we aren't confidential.
  std::set<std::string> alpn;
  std::string alpnDefault = "";
  alpn.insert("c-webrtc");
  if (!mPC->PrivacyRequested()) {
    alpnDefault = "webrtc";
    alpn.insert(alpnDefault);
  }
  rv = dtls->SetAlpn(alpn, alpnDefault);
  if (NS_FAILED(rv)) {
    MOZ_MTLOG(ML_ERROR, "Couldn't set ALPN");
    return rv;
  }

  nsAutoPtr<PtrVector<TransportLayer> > layers(new PtrVector<TransportLayer>);
  layers->values.push_back(ice.release());
  layers->values.push_back(dtls.release());

  rv = mPCMedia->GetSTSThread()->Dispatch(
      WrapRunnableNM(FinalizeTransportFlow_s, mPCMedia, flow, aLevel, aIsRtcp,
                     layers),
      NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    MOZ_MTLOG(ML_ERROR, "Failed to dispatch FinalizeTransportFlow_s");
    return rv;
  }

  mPCMedia->AddTransportFlow(aLevel, aIsRtcp, flow);

  *aFlowOutparam = flow;

  return NS_OK;
}

nsresult
MediaPipelineFactory::GetTransportParameters(
    const JsepTrackPair& aTrackPair,
    const JsepTrack& aTrack,
    size_t* aLevelOut,
    RefPtr<TransportFlow>* aRtpOut,
    RefPtr<TransportFlow>* aRtcpOut,
    nsAutoPtr<MediaPipelineFilter>* aFilterOut)
{
  *aLevelOut = aTrackPair.mLevel;

  size_t transportLevel = aTrackPair.mBundleLevel.isSome() ?
                          *aTrackPair.mBundleLevel :
                          aTrackPair.mLevel;

  nsresult rv = CreateOrGetTransportFlow(
      transportLevel, false, *aTrackPair.mRtpTransport, aRtpOut);
  if (NS_FAILED(rv)) {
    return rv;
  }
  MOZ_ASSERT(aRtpOut);

  if (aTrackPair.mRtcpTransport) {
    rv = CreateOrGetTransportFlow(
        transportLevel, true, *aTrackPair.mRtcpTransport, aRtcpOut);
    if (NS_FAILED(rv)) {
      return rv;
    }
    MOZ_ASSERT(aRtcpOut);
  }

  if (aTrackPair.mBundleLevel.isSome()) {
    bool receiving =
        aTrack.GetDirection() == JsepTrack::Direction::kJsepTrackReceiving;

    *aFilterOut = new MediaPipelineFilter;

    if (receiving) {
      // Add remote SSRCs so we can distinguish which RTP packets actually
      // belong to this pipeline (also RTCP sender reports).
      for (auto i = aTrack.GetSsrcs().begin();
          i != aTrack.GetSsrcs().end(); ++i) {
        (*aFilterOut)->AddRemoteSSRC(*i);
      }

      // TODO(bug 1105005): Tell the filter about the mid for this track

      // Add unique payload types as a last-ditch fallback
      auto uniquePts = aTrack.GetNegotiatedDetails()->GetUniquePayloadTypes();
      for (auto i = uniquePts.begin(); i != uniquePts.end(); ++i) {
        (*aFilterOut)->AddUniquePT(*i);
      }
    } else {
      // Add local SSRCs so we can distinguish which RTCP packets actually
      // belong to this pipeline.
      for (auto i = aTrack.GetSsrcs().begin();
           i != aTrack.GetSsrcs().end(); ++i) {
        (*aFilterOut)->AddLocalSSRC(*i);
      }
    }
  }

  return NS_OK;
}

nsresult
MediaPipelineFactory::CreateOrUpdateMediaPipeline(
    const JsepTrackPair& aTrackPair,
    const JsepTrack& aTrack)
{
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  // The GMP code is all the way on the other side of webrtc.org, and it is not
  // feasible to plumb this information all the way through. So, we set it (for
  // the duration of this call) in a global variable. This allows the GMP code
  // to report errors to the PC.
  WebrtcGmpPCHandleSetter setter(mPC->GetHandle());
#endif

  MOZ_ASSERT(aTrackPair.mRtpTransport);

  bool receiving =
      aTrack.GetDirection() == JsepTrack::Direction::kJsepTrackReceiving;

  size_t level;
  RefPtr<TransportFlow> rtpFlow;
  RefPtr<TransportFlow> rtcpFlow;
  nsAutoPtr<MediaPipelineFilter> filter;

  nsresult rv = GetTransportParameters(aTrackPair,
                                       aTrack,
                                       &level,
                                       &rtpFlow,
                                       &rtcpFlow,
                                       &filter);
  if (NS_FAILED(rv)) {
    MOZ_MTLOG(ML_ERROR, "Failed to get transport parameters for pipeline, rv="
              << static_cast<unsigned>(rv));
    return rv;
  }

  if (aTrack.GetMediaType() == SdpMediaSection::kApplication) {
    // GetTransportParameters has already done everything we need for
    // datachannel.
    return NS_OK;
  }

  // Find the stream we need
  SourceStreamInfo* stream;
  if (receiving) {
    stream = mPCMedia->GetRemoteStreamById(aTrack.GetStreamId());
  } else {
    stream = mPCMedia->GetLocalStreamById(aTrack.GetStreamId());
  }

  if (!stream) {
    MOZ_MTLOG(ML_ERROR, "Negotiated " << (receiving ? "recv" : "send")
              << " stream id " << aTrack.GetStreamId() << " was never added");
    MOZ_ASSERT(false);
    return NS_ERROR_FAILURE;
  }

  if (!stream->HasTrack(aTrack.GetTrackId())) {
    MOZ_MTLOG(ML_ERROR, "Negotiated " << (receiving ? "recv" : "send")
              << " track id " << aTrack.GetTrackId() << " was never added");
    MOZ_ASSERT(false);
    return NS_ERROR_FAILURE;
  }

  RefPtr<MediaSessionConduit> conduit;
  if (aTrack.GetMediaType() == SdpMediaSection::kAudio) {
    rv = GetOrCreateAudioConduit(aTrackPair, aTrack, &conduit);
    if (NS_FAILED(rv))
      return rv;
  } else if (aTrack.GetMediaType() == SdpMediaSection::kVideo) {
    rv = GetOrCreateVideoConduit(aTrackPair, aTrack, &conduit);
    if (NS_FAILED(rv))
      return rv;
  } else {
    // We've created the TransportFlow, nothing else to do here.
    return NS_OK;
  }

  RefPtr<MediaPipeline> pipeline =
    stream->GetPipelineByTrackId_m(aTrack.GetTrackId());

  if (pipeline && pipeline->level() != static_cast<int>(level)) {
    MOZ_MTLOG(ML_WARNING, "Track " << aTrack.GetTrackId() <<
                          " has moved from level " << pipeline->level() <<
                          " to level " << level <<
                          ". This requires re-creating the MediaPipeline.");
    // Since we do not support changing the conduit on a pre-existing
    // MediaPipeline
    pipeline = nullptr;
    stream->RemoveTrack(aTrack.GetTrackId());
    stream->AddTrack(aTrack.GetTrackId());
  }

  if (pipeline) {
    pipeline->UpdateTransport_m(level, rtpFlow, rtcpFlow, filter);
    return NS_OK;
  }

  MOZ_MTLOG(ML_DEBUG,
            "Creating media pipeline"
                << " m-line index=" << aTrackPair.mLevel
                << " type=" << aTrack.GetMediaType()
                << " direction=" << aTrack.GetDirection());

  if (receiving) {
    rv = CreateMediaPipelineReceiving(aTrackPair, aTrack,
                                      level, rtpFlow, rtcpFlow, filter,
                                      conduit);
    if (NS_FAILED(rv))
      return rv;
  } else {
    rv = CreateMediaPipelineSending(aTrackPair, aTrack,
                                    level, rtpFlow, rtcpFlow, filter,
                                    conduit);
    if (NS_FAILED(rv))
      return rv;
  }

  return NS_OK;
}

nsresult
MediaPipelineFactory::CreateMediaPipelineReceiving(
    const JsepTrackPair& aTrackPair,
    const JsepTrack& aTrack,
    size_t aLevel,
    RefPtr<TransportFlow> aRtpFlow,
    RefPtr<TransportFlow> aRtcpFlow,
    nsAutoPtr<MediaPipelineFilter> aFilter,
    const RefPtr<MediaSessionConduit>& aConduit)
{
  // We will error out earlier if this isn't here.
  nsRefPtr<RemoteSourceStreamInfo> stream =
      mPCMedia->GetRemoteStreamById(aTrack.GetStreamId());

  RefPtr<MediaPipelineReceive> pipeline;

  TrackID numericTrackId = stream->GetNumericTrackId(aTrack.GetTrackId());
  MOZ_ASSERT(numericTrackId != TRACK_INVALID);

  bool queue_track = stream->ShouldQueueTracks();

  MOZ_MTLOG(ML_DEBUG, __FUNCTION__ << ": Creating pipeline for "
            << numericTrackId << " -> " << aTrack.GetTrackId());

  if (aTrack.GetMediaType() == SdpMediaSection::kAudio) {
    pipeline = new MediaPipelineReceiveAudio(
        mPC->GetHandle(),
        mPC->GetMainThread().get(),
        mPC->GetSTSThread(),
        stream->GetMediaStream()->GetStream(),
        aTrack.GetTrackId(),
        numericTrackId,
        aLevel,
        static_cast<AudioSessionConduit*>(aConduit.get()), // Ugly downcast.
        aRtpFlow,
        aRtcpFlow,
        aFilter,
        queue_track);
  } else if (aTrack.GetMediaType() == SdpMediaSection::kVideo) {
    pipeline = new MediaPipelineReceiveVideo(
        mPC->GetHandle(),
        mPC->GetMainThread().get(),
        mPC->GetSTSThread(),
        stream->GetMediaStream()->GetStream(),
        aTrack.GetTrackId(),
        numericTrackId,
        aLevel,
        static_cast<VideoSessionConduit*>(aConduit.get()), // Ugly downcast.
        aRtpFlow,
        aRtcpFlow,
        aFilter,
        queue_track);
  } else {
    MOZ_ASSERT(false);
    MOZ_MTLOG(ML_ERROR, "Invalid media type in CreateMediaPipelineReceiving");
    return NS_ERROR_FAILURE;
  }

  nsresult rv = pipeline->Init();
  if (NS_FAILED(rv)) {
    MOZ_MTLOG(ML_ERROR, "Couldn't initialize receiving pipeline");
    return rv;
  }

  rv = stream->StorePipeline(aTrack.GetTrackId(),
                             RefPtr<MediaPipeline>(pipeline));
  if (NS_FAILED(rv)) {
    MOZ_MTLOG(ML_ERROR, "Couldn't store receiving pipeline " <<
                        static_cast<unsigned>(rv));
    return rv;
  }

  stream->SyncPipeline(pipeline);

  return NS_OK;
}

nsresult
MediaPipelineFactory::CreateMediaPipelineSending(
    const JsepTrackPair& aTrackPair,
    const JsepTrack& aTrack,
    size_t aLevel,
    RefPtr<TransportFlow> aRtpFlow,
    RefPtr<TransportFlow> aRtcpFlow,
    nsAutoPtr<MediaPipelineFilter> aFilter,
    const RefPtr<MediaSessionConduit>& aConduit)
{
  nsresult rv;

  // This is checked earlier
  nsRefPtr<LocalSourceStreamInfo> stream =
      mPCMedia->GetLocalStreamById(aTrack.GetStreamId());

  // Now we have all the pieces, create the pipeline
  RefPtr<MediaPipelineTransmit> pipeline = new MediaPipelineTransmit(
      mPC->GetHandle(),
      mPC->GetMainThread().get(),
      mPC->GetSTSThread(),
      stream->GetMediaStream(),
      aTrack.GetTrackId(),
      aLevel,
      aTrack.GetMediaType() == SdpMediaSection::kVideo,
      aConduit,
      aRtpFlow,
      aRtcpFlow,
      aFilter);

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  // implement checking for peerIdentity (where failure == black/silence)
  nsIDocument* doc = mPC->GetWindow()->GetExtantDoc();
  if (doc) {
    pipeline->UpdateSinkIdentity_m(doc->NodePrincipal(),
                                   mPC->GetPeerIdentity());
  } else {
    MOZ_MTLOG(ML_ERROR, "Cannot initialize pipeline without attached doc");
    return NS_ERROR_FAILURE; // Don't remove this till we know it's safe.
  }
#endif

  rv = pipeline->Init();
  if (NS_FAILED(rv)) {
    MOZ_MTLOG(ML_ERROR, "Couldn't initialize sending pipeline");
    return rv;
  }

  rv = stream->StorePipeline(aTrack.GetTrackId(),
                             RefPtr<MediaPipeline>(pipeline));
  if (NS_FAILED(rv)) {
    MOZ_MTLOG(ML_ERROR, "Couldn't store receiving pipeline " <<
                        static_cast<unsigned>(rv));
    return rv;
  }

  return NS_OK;
}

nsresult
MediaPipelineFactory::GetOrCreateAudioConduit(
    const JsepTrackPair& aTrackPair,
    const JsepTrack& aTrack,
    RefPtr<MediaSessionConduit>* aConduitp)
{

  if (!aTrack.GetNegotiatedDetails()) {
    MOZ_ASSERT(false, "Track is missing negotiated details");
    return NS_ERROR_INVALID_ARG;
  }

  bool receiving =
      aTrack.GetDirection() == JsepTrack::Direction::kJsepTrackReceiving;

  RefPtr<AudioSessionConduit> conduit =
    mPCMedia->GetAudioConduit(aTrackPair.mLevel);

  if (!conduit) {
    conduit = AudioSessionConduit::Create();
    if (!conduit) {
      MOZ_MTLOG(ML_ERROR, "Could not create audio conduit");
      return NS_ERROR_FAILURE;
    }

    mPCMedia->AddAudioConduit(aTrackPair.mLevel, conduit);
  }

  size_t numCodecs = aTrack.GetNegotiatedDetails()->GetCodecCount();
  if (numCodecs == 0) {
    MOZ_MTLOG(ML_ERROR, "Can't set up a conduit with 0 codecs");
    return NS_ERROR_FAILURE;
  }

  if (receiving) {
    PtrVector<AudioCodecConfig> configs;

    for (size_t i = 0; i < numCodecs; i++) {
      const JsepCodecDescription* cdesc;
      nsresult rv = aTrack.GetNegotiatedDetails()->GetCodec(i, &cdesc);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      if (NS_FAILED(rv)) {
        MOZ_MTLOG(ML_ERROR, "Failed to get codec from jsep track, rv="
                                << static_cast<uint32_t>(rv));
        return rv;
      }

      AudioCodecConfig* configRaw;
      rv = JsepCodecDescToCodecConfig(*cdesc, &configRaw);
      if (NS_FAILED(rv))
        return rv;

      configs.values.push_back(configRaw);
    }

    auto error = conduit->ConfigureRecvMediaCodecs(configs.values);

    if (error) {
      MOZ_MTLOG(ML_ERROR, "ConfigureRecvMediaCodecs failed: " << error);
      return NS_ERROR_FAILURE;
    }

    if (!aTrackPair.mSending) {
      // No send track, but we still need to configure an SSRC for receiver
      // reports.
      if (!conduit->SetLocalSSRC(aTrackPair.mRecvonlySsrc)) {
        MOZ_MTLOG(ML_ERROR, "SetLocalSSRC failed");
        return NS_ERROR_FAILURE;
      }
    }
  } else {
    // For now we only expect to have one ssrc per local track.
    auto ssrcs = aTrack.GetSsrcs();
    if (!ssrcs.empty()) {
      if (!conduit->SetLocalSSRC(ssrcs.front())) {
        MOZ_MTLOG(ML_ERROR, "SetLocalSSRC failed");
        return NS_ERROR_FAILURE;
      }
    }

    conduit->SetLocalCNAME(aTrack.GetCNAME().c_str());

    const JsepCodecDescription* cdesc;
    // Best codec.
    nsresult rv = aTrack.GetNegotiatedDetails()->GetCodec(0, &cdesc);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    if (NS_FAILED(rv)) {
      MOZ_MTLOG(ML_ERROR, "Failed to get codec from jsep track, rv="
                              << static_cast<uint32_t>(rv));
      return rv;
    }

    AudioCodecConfig* configRaw;
    rv = JsepCodecDescToCodecConfig(*cdesc, &configRaw);
    if (NS_FAILED(rv))
      return rv;

    ScopedDeletePtr<AudioCodecConfig> config(configRaw);
    auto error = conduit->ConfigureSendMediaCodec(config.get());
    if (error) {
      MOZ_MTLOG(ML_ERROR, "ConfigureSendMediaCodec failed: " << error);
      return NS_ERROR_FAILURE;
    }

    const SdpExtmapAttributeList::Extmap* audioLevelExt =
        aTrack.GetNegotiatedDetails()->GetExt(
            "urn:ietf:params:rtp-hdrext:ssrc-audio-level");

    if (audioLevelExt) {
      MOZ_MTLOG(ML_DEBUG, "Calling EnableAudioLevelExtension");
      error = conduit->EnableAudioLevelExtension(true, audioLevelExt->entry);

      if (error) {
        MOZ_MTLOG(ML_ERROR, "EnableAudioLevelExtension failed: " << error);
        return NS_ERROR_FAILURE;
      }
    }
  }

  *aConduitp = conduit;

  return NS_OK;
}

nsresult
MediaPipelineFactory::GetOrCreateVideoConduit(
    const JsepTrackPair& aTrackPair,
    const JsepTrack& aTrack,
    RefPtr<MediaSessionConduit>* aConduitp)
{

  if (!aTrack.GetNegotiatedDetails()) {
    MOZ_ASSERT(false, "Track is missing negotiated details");
    return NS_ERROR_INVALID_ARG;
  }

  bool receiving =
      aTrack.GetDirection() == JsepTrack::Direction::kJsepTrackReceiving;

  RefPtr<VideoSessionConduit> conduit =
    mPCMedia->GetVideoConduit(aTrackPair.mLevel);

  if (!conduit) {
    conduit = VideoSessionConduit::Create();
    if (!conduit) {
      MOZ_MTLOG(ML_ERROR, "Could not create audio conduit");
      return NS_ERROR_FAILURE;
    }

    mPCMedia->AddVideoConduit(aTrackPair.mLevel, conduit);
  }

  size_t numCodecs = aTrack.GetNegotiatedDetails()->GetCodecCount();
  if (numCodecs == 0) {
    MOZ_MTLOG(ML_ERROR, "Can't set up a conduit with 0 codecs");
    return NS_ERROR_FAILURE;
  }

  if (receiving) {
    PtrVector<VideoCodecConfig> configs;

    for (size_t i = 0; i < numCodecs; i++) {
      const JsepCodecDescription* cdesc;

      nsresult rv = aTrack.GetNegotiatedDetails()->GetCodec(i, &cdesc);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      if (NS_FAILED(rv)) {
        MOZ_MTLOG(ML_ERROR, "Failed to get codec from jsep track, rv="
                                << static_cast<uint32_t>(rv));
        return rv;
      }

      VideoCodecConfig* configRaw;
      rv = JsepCodecDescToCodecConfig(*cdesc, &configRaw);
      if (NS_FAILED(rv))
        return rv;

      UniquePtr<VideoCodecConfig> config(configRaw);
      if (EnsureExternalCodec(*conduit, config.get(), false)) {
        continue;
      }

      configs.values.push_back(config.release());
    }

    auto error = conduit->ConfigureRecvMediaCodecs(configs.values);

    if (error) {
      MOZ_MTLOG(ML_ERROR, "ConfigureRecvMediaCodecs failed: " << error);
      return NS_ERROR_FAILURE;
    }

    if (!aTrackPair.mSending) {
      // No send track, but we still need to configure an SSRC for receiver
      // reports.
      if (!conduit->SetLocalSSRC(aTrackPair.mRecvonlySsrc)) {
        MOZ_MTLOG(ML_ERROR, "SetLocalSSRC failed");
        return NS_ERROR_FAILURE;
      }
    }
  } else {
    // For now we only expect to have one ssrc per local track.
    auto ssrcs = aTrack.GetSsrcs();
    if (!ssrcs.empty()) {
      if (!conduit->SetLocalSSRC(ssrcs.front())) {
        MOZ_MTLOG(ML_ERROR, "SetLocalSSRC failed");
        return NS_ERROR_FAILURE;
      }
    }

    conduit->SetLocalCNAME(aTrack.GetCNAME().c_str());

    const JsepCodecDescription* cdesc;
    // Best codec.
    nsresult rv = aTrack.GetNegotiatedDetails()->GetCodec(0, &cdesc);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    if (NS_FAILED(rv)) {
      MOZ_MTLOG(ML_ERROR, "Failed to get codec from jsep track, rv="
                              << static_cast<uint32_t>(rv));
      return rv;
    }

    VideoCodecConfig* configRaw;
    rv = JsepCodecDescToCodecConfig(*cdesc, &configRaw);
    if (NS_FAILED(rv))
      return rv;

    rv = ConfigureVideoCodecMode(aTrack,*conduit);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Take possession of this pointer
    ScopedDeletePtr<VideoCodecConfig> config(configRaw);

    if (EnsureExternalCodec(*conduit, config, true)) {
      MOZ_MTLOG(ML_ERROR, "External codec not available");
      return NS_ERROR_FAILURE;
    }

    auto error = conduit->ConfigureSendMediaCodec(config);

    if (error) {
      MOZ_MTLOG(ML_ERROR, "ConfigureSendMediaCodec failed: " << error);
      return NS_ERROR_FAILURE;
    }
  }

  *aConduitp = conduit;

  return NS_OK;
}

nsresult
MediaPipelineFactory::ConfigureVideoCodecMode(const JsepTrack& aTrack,
                                              VideoSessionConduit& aConduit)
{
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  nsRefPtr<LocalSourceStreamInfo> stream =
    mPCMedia->GetLocalStreamById(aTrack.GetStreamId());

  //get video track
  nsRefPtr<mozilla::dom::VideoStreamTrack> videotrack =
    stream->GetVideoTrackByTrackId(aTrack.GetTrackId());

  if (!videotrack) {
    MOZ_MTLOG(ML_ERROR, "video track not available");
    return NS_ERROR_FAILURE;
  }

  //get video source type
  nsRefPtr<DOMMediaStream> mediastream =
    mPCMedia->GetLocalStreamById(aTrack.GetStreamId())->GetMediaStream();

  DOMLocalMediaStream* domLocalStream = mediastream->AsDOMLocalMediaStream();
  if (!domLocalStream) {
    return NS_OK;
  }

  MediaEngineSource *engine =
    domLocalStream->GetMediaEngine(videotrack->GetTrackID());

  dom::MediaSourceEnum source = engine->GetMediaSource();
  webrtc::VideoCodecMode mode = webrtc::kRealtimeVideo;
  switch (source) {
    case dom::MediaSourceEnum::Browser:
    case dom::MediaSourceEnum::Screen:
    case dom::MediaSourceEnum::Application:
    case dom::MediaSourceEnum::Window:
      mode = webrtc::kScreensharing;
      break;

    case dom::MediaSourceEnum::Camera:
    default:
      mode = webrtc::kRealtimeVideo;
      break;
  }

  auto error = aConduit.ConfigureCodecMode(mode);
  if (error) {
    MOZ_MTLOG(ML_ERROR, "ConfigureCodecMode failed: " << error);
    return NS_ERROR_FAILURE;
  }

#endif
  return NS_OK;
}

/*
 * Add external H.264 video codec.
 */
MediaConduitErrorCode
MediaPipelineFactory::EnsureExternalCodec(VideoSessionConduit& aConduit,
                                          VideoCodecConfig* aConfig,
                                          bool aIsSend)
{
  if (aConfig->mName == "VP8") {
#ifdef MOZ_WEBRTC_MEDIACODEC
     if (aIsSend) {
#ifdef MOZILLA_INTERNAL_API
       bool enabled = mozilla::Preferences::GetBool("media.navigator.hardware.vp8_encode.acceleration_enabled", false);
#else
       bool enabled = false;
#endif
       if (enabled) {
         nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
         if (gfxInfo) {
           int32_t status;
           if (NS_SUCCEEDED(gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_WEBRTC_HW_ACCELERATION, &status))) {
             if (status != nsIGfxInfo::FEATURE_STATUS_OK) {
               NS_WARNING("VP8 encoder hardware is not whitelisted: disabling.\n");
             } else {
               VideoEncoder* encoder = nullptr;
               encoder = MediaCodecVideoCodec::CreateEncoder(MediaCodecVideoCodec::CodecType::CODEC_VP8);
               if (encoder) {
                 return aConduit.SetExternalSendCodec(aConfig, encoder);
               } else {
                 return kMediaConduitNoError;
               }
             }
           }
         }
       }
     } else {
#ifdef MOZILLA_INTERNAL_API
       bool enabled = mozilla::Preferences::GetBool("media.navigator.hardware.vp8_decode.acceleration_enabled", false);
#else
       bool enabled = false;
#endif
       if (enabled) {
         nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
         if (gfxInfo) {
           int32_t status;
           if (NS_SUCCEEDED(gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_WEBRTC_HW_ACCELERATION, &status))) {
             if (status != nsIGfxInfo::FEATURE_STATUS_OK) {
               NS_WARNING("VP8 decoder hardware is not whitelisted: disabling.\n");
             } else {

               VideoDecoder* decoder;
               decoder = MediaCodecVideoCodec::CreateDecoder(MediaCodecVideoCodec::CodecType::CODEC_VP8);
               if (decoder) {
                 return aConduit.SetExternalRecvCodec(aConfig, decoder);
               } else {
                 return kMediaConduitNoError;
               }
             }
           }
         }
       }
     }
#endif
     return kMediaConduitNoError;
  } else if (aConfig->mName == "VP9") {
    return kMediaConduitNoError;
  } else if (aConfig->mName == "H264") {
    if (aConduit.CodecPluginID() != 0) {
      return kMediaConduitNoError;
    }
    // Register H.264 codec.
    if (aIsSend) {
      VideoEncoder* encoder = nullptr;
#ifdef MOZ_WEBRTC_OMX
      encoder =
          OMXVideoCodec::CreateEncoder(OMXVideoCodec::CodecType::CODEC_H264);
#elif !defined(MOZILLA_XPCOMRT_API)
      encoder = GmpVideoCodec::CreateEncoder();
#endif
      if (encoder) {
        return aConduit.SetExternalSendCodec(aConfig, encoder);
      } else {
        return kMediaConduitInvalidSendCodec;
      }
    } else {
      VideoDecoder* decoder = nullptr;
#ifdef MOZ_WEBRTC_OMX
      decoder =
          OMXVideoCodec::CreateDecoder(OMXVideoCodec::CodecType::CODEC_H264);
#elif !defined(MOZILLA_XPCOMRT_API)
      decoder = GmpVideoCodec::CreateDecoder();
#endif
      if (decoder) {
        return aConduit.SetExternalRecvCodec(aConfig, decoder);
      } else {
        return kMediaConduitInvalidReceiveCodec;
      }
    }
    NS_NOTREACHED("Shouldn't get here!");
  } else {
    MOZ_MTLOG(ML_ERROR,
              "Invalid video codec configured: " << aConfig->mName.c_str());
    return aIsSend ? kMediaConduitInvalidSendCodec
                   : kMediaConduitInvalidReceiveCodec;
  }

  NS_NOTREACHED("Shouldn't get here!");
}

} // namespace mozilla
