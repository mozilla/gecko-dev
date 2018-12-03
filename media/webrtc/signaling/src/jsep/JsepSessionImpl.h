/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _JSEPSESSIONIMPL_H_
#define _JSEPSESSIONIMPL_H_

#include <set>
#include <string>
#include <vector>

#include "signaling/src/jsep/JsepCodecDescription.h"
#include "signaling/src/jsep/JsepSession.h"
#include "signaling/src/jsep/JsepTrack.h"
#include "signaling/src/jsep/JsepTransceiver.h"
#include "signaling/src/jsep/SsrcGenerator.h"
#include "signaling/src/sdp/RsdparsaSdpParser.h"
#include "signaling/src/sdp/SipccSdpParser.h"
#include "signaling/src/sdp/SdpHelper.h"

namespace mozilla {

class JsepUuidGenerator {
 public:
  virtual ~JsepUuidGenerator() {}
  virtual bool Generate(std::string* id) = 0;
};

class JsepSessionImpl : public JsepSession {
 public:
  JsepSessionImpl(const std::string& name, UniquePtr<JsepUuidGenerator> uuidgen)
      : JsepSession(name),
        mIsOfferer(false),
        mWasOffererLastTime(false),
        mIceControlling(false),
        mRemoteIsIceLite(false),
        mBundlePolicy(kBundleBalanced),
        mSessionId(0),
        mSessionVersion(0),
        mMidCounter(0),
        mTransportIdCounter(0),
        mUuidGen(std::move(uuidgen)),
        mSdpHelper(&mLastError),
        mRunRustParser(false),
        mRunSdpComparer(false),
        mEncodeTrackId(true) {}

  // Implement JsepSession methods.
  virtual nsresult Init() override;

  nsresult SetBundlePolicy(JsepBundlePolicy policy) override;

  virtual bool RemoteIsIceLite() const override { return mRemoteIsIceLite; }

  virtual std::vector<std::string> GetIceOptions() const override {
    return mIceOptions;
  }

  virtual nsresult AddDtlsFingerprint(
      const std::string& algorithm, const std::vector<uint8_t>& value) override;

  nsresult AddRtpExtension(JsepMediaType mediaType,
                           const std::string& extensionName,
                           SdpDirectionAttribute::Direction direction);
  virtual nsresult AddAudioRtpExtension(
      const std::string& extensionName,
      SdpDirectionAttribute::Direction direction =
          SdpDirectionAttribute::Direction::kSendrecv) override;

  virtual nsresult AddVideoRtpExtension(
      const std::string& extensionName,
      SdpDirectionAttribute::Direction direction =
          SdpDirectionAttribute::Direction::kSendrecv) override;

  virtual nsresult AddAudioVideoRtpExtension(
      const std::string& extensionName,
      SdpDirectionAttribute::Direction direction =
          SdpDirectionAttribute::Direction::kSendrecv) override;

  virtual std::vector<UniquePtr<JsepCodecDescription>>& Codecs() override {
    return mSupportedCodecs;
  }

  virtual nsresult CreateOffer(const JsepOfferOptions& options,
                               std::string* offer) override;

  virtual nsresult CreateAnswer(const JsepAnswerOptions& options,
                                std::string* answer) override;

  virtual std::string GetLocalDescription(
      JsepDescriptionPendingOrCurrent type) const override;

  virtual std::string GetRemoteDescription(
      JsepDescriptionPendingOrCurrent type) const override;

  virtual nsresult SetLocalDescription(JsepSdpType type,
                                       const std::string& sdp) override;

  virtual nsresult SetRemoteDescription(JsepSdpType type,
                                        const std::string& sdp) override;

  virtual nsresult AddRemoteIceCandidate(const std::string& candidate,
                                         const std::string& mid,
                                         const Maybe<uint16_t>& level,
                                         std::string* transportId) override;

  virtual nsresult AddLocalIceCandidate(const std::string& candidate,
                                        const std::string& transportId,
                                        uint16_t* level, std::string* mid,
                                        bool* skipped) override;

  virtual nsresult UpdateDefaultCandidate(
      const std::string& defaultCandidateAddr, uint16_t defaultCandidatePort,
      const std::string& defaultRtcpCandidateAddr,
      uint16_t defaultRtcpCandidatePort,
      const std::string& transportId) override;

  virtual nsresult EndOfLocalCandidates(
      const std::string& transportId) override;

  virtual nsresult Close() override;

  virtual const std::string GetLastError() const override;

  virtual bool IsIceControlling() const override { return mIceControlling; }

  virtual bool IsOfferer() const override { return mIsOfferer; }

  virtual bool IsIceRestarting() const override {
    return !mOldIceUfrag.empty();
  }

  virtual const std::vector<RefPtr<JsepTransceiver>>& GetTransceivers()
      const override {
    return mTransceivers;
  }

  virtual std::vector<RefPtr<JsepTransceiver>>& GetTransceivers() override {
    return mTransceivers;
  }

  virtual nsresult AddTransceiver(RefPtr<JsepTransceiver> transceiver) override;

  virtual bool CheckNegotiationNeeded() const override;

 private:
  struct JsepDtlsFingerprint {
    std::string mAlgorithm;
    std::vector<uint8_t> mValue;
  };

  // Non-const so it can set mLastError
  nsresult CreateGenericSDP(UniquePtr<Sdp>* sdp);
  void AddExtmap(SdpMediaSection* msection);
  std::vector<SdpExtmapAttributeList::Extmap> GetRtpExtensions(
      const SdpMediaSection& msection);
  std::string GetNewMid();

  void AddCommonExtmaps(const SdpMediaSection& remoteMsection,
                        SdpMediaSection* msection);
  nsresult SetupIds();
  void SetupDefaultCodecs();
  void SetupDefaultRtpExtensions();
  void SetState(JsepSignalingState state);
  // Non-const so it can set mLastError
  nsresult ParseSdp(const std::string& sdp, UniquePtr<Sdp>* parsedp);
  nsresult SetLocalDescriptionOffer(UniquePtr<Sdp> offer);
  nsresult SetLocalDescriptionAnswer(JsepSdpType type, UniquePtr<Sdp> answer);
  nsresult SetRemoteDescriptionOffer(UniquePtr<Sdp> offer);
  nsresult SetRemoteDescriptionAnswer(JsepSdpType type, UniquePtr<Sdp> answer);
  nsresult ValidateLocalDescription(const Sdp& description);
  nsresult ValidateRemoteDescription(const Sdp& description);
  nsresult ValidateOffer(const Sdp& offer);
  nsresult ValidateAnswer(const Sdp& offer, const Sdp& answer);
  nsresult UpdateTransceiversFromRemoteDescription(const Sdp& remote);
  bool WasMsectionDisabledLastNegotiation(size_t level) const;
  JsepTransceiver* GetTransceiverForLevel(size_t level);
  JsepTransceiver* GetTransceiverForMid(const std::string& mid);
  JsepTransceiver* GetTransceiverForLocal(size_t level);
  JsepTransceiver* GetTransceiverForRemote(const SdpMediaSection& msection);
  JsepTransceiver* GetTransceiverWithTransport(const std::string& transportId);
  // The w3c and IETF specs have a lot of "magical" behavior that happens when
  // addTrack is used. This was a deliberate design choice. Sadface.
  JsepTransceiver* FindUnassociatedTransceiver(SdpMediaSection::MediaType type,
                                               bool magic);
  // Called for rollback of local description
  void RollbackLocalOffer();
  // Called for rollback of remote description
  void RollbackRemoteOffer();
  nsresult HandleNegotiatedSession(const UniquePtr<Sdp>& local,
                                   const UniquePtr<Sdp>& remote);
  nsresult AddTransportAttributes(SdpMediaSection* msection,
                                  SdpSetupAttribute::Role dtlsRole);
  nsresult CopyPreviousTransportParams(const Sdp& oldAnswer,
                                       const Sdp& offerersPreviousSdp,
                                       const Sdp& newOffer, Sdp* newLocal);
  void CopyPreviousMsid(const Sdp& oldLocal, Sdp* newLocal);
  void EnsureMsid(Sdp* remote);
  void SetupBundle(Sdp* sdp) const;
  nsresult GetRemoteIds(const Sdp& sdp, const SdpMediaSection& msection,
                        std::vector<std::string>* streamIds,
                        std::string* trackId);
  nsresult RemoveDuplicateTrackIds(Sdp* sdp);
  nsresult CreateOfferMsection(const JsepOfferOptions& options,
                               JsepTransceiver& transceiver, Sdp* local);
  nsresult CreateAnswerMsection(const JsepAnswerOptions& options,
                                JsepTransceiver& transceiver,
                                const SdpMediaSection& remoteMsection,
                                Sdp* sdp);
  nsresult DetermineAnswererSetupRole(const SdpMediaSection& remoteMsection,
                                      SdpSetupAttribute::Role* rolep);
  nsresult MakeNegotiatedTransceiver(const SdpMediaSection& remote,
                                     const SdpMediaSection& local,
                                     JsepTransceiver* transceiverOut);
  void EnsureHasOwnTransport(const SdpMediaSection& msection,
                             JsepTransceiver* transceiver);

  nsresult FinalizeTransport(const SdpAttributeList& remote,
                             const SdpAttributeList& answer,
                             JsepTransport* transport);

  nsresult GetNegotiatedBundledMids(SdpHelper::BundledMids* bundledMids);

  nsresult EnableOfferMsection(SdpMediaSection* msection);

  mozilla::Sdp* GetParsedLocalDescription(
      JsepDescriptionPendingOrCurrent type) const;
  mozilla::Sdp* GetParsedRemoteDescription(
      JsepDescriptionPendingOrCurrent type) const;
  const Sdp* GetAnswer() const;
  void SetIceRestarting(bool restarting);

  // !!!NOT INDEXED BY LEVEL!!! These are in the order they were created in. The
  // level mapping is done with JsepTransceiver::mLevel.
  std::vector<RefPtr<JsepTransceiver>> mTransceivers;
  // So we can rollback. Not as simple as just going back to the old, though...
  std::vector<RefPtr<JsepTransceiver>> mOldTransceivers;

  bool mIsOfferer;
  bool mWasOffererLastTime;
  bool mIceControlling;
  std::string mIceUfrag;
  std::string mIcePwd;
  std::string mOldIceUfrag;
  std::string mOldIcePwd;
  bool mRemoteIsIceLite;
  std::vector<std::string> mIceOptions;
  JsepBundlePolicy mBundlePolicy;
  std::vector<JsepDtlsFingerprint> mDtlsFingerprints;
  uint64_t mSessionId;
  uint64_t mSessionVersion;
  size_t mMidCounter;
  std::set<std::string> mUsedMids;
  size_t mTransportIdCounter;
  std::vector<JsepExtmapMediaType> mRtpExtensions;
  UniquePtr<JsepUuidGenerator> mUuidGen;
  std::string mDefaultRemoteStreamId;
  std::string mCNAME;
  // Used to prevent duplicate local SSRCs. Not used to prevent local/remote or
  // remote-only duplication, which will be important for EKT but not now.
  std::set<uint32_t> mSsrcs;
  UniquePtr<Sdp> mGeneratedLocalDescription;  // Created but not set.
  UniquePtr<Sdp> mCurrentLocalDescription;
  UniquePtr<Sdp> mCurrentRemoteDescription;
  UniquePtr<Sdp> mPendingLocalDescription;
  UniquePtr<Sdp> mPendingRemoteDescription;
  std::vector<UniquePtr<JsepCodecDescription>> mSupportedCodecs;
  std::string mLastError;
  SipccSdpParser mSipccParser;
  SdpHelper mSdpHelper;
  SsrcGenerator mSsrcGenerator;
  bool mRunRustParser;
  bool mRunSdpComparer;
  bool mEncodeTrackId;
  RsdparsaSdpParser mRsdparsaParser;
};

}  // namespace mozilla

#endif
