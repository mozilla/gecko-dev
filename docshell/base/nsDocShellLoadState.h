/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDocShellLoadState_h__
#define nsDocShellLoadState_h__

// Helper Classes
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsDocShellLoadTypes.h"
#include "mozilla/net/ReferrerPolicy.h"

class nsIInputStream;
class nsISHEntry;
class nsIURI;
class nsIDocShell;
class OriginAttibutes;

/**
 * nsDocShellLoadState contains setup information used in a nsIDocShell::loadURI
 * call.
 */
class nsDocShellLoadState final {
 public:
  NS_INLINE_DECL_REFCOUNTING(nsDocShellLoadState);

  nsDocShellLoadState();

  // Getters and Setters

  nsIURI* Referrer() const;

  void SetReferrer(nsIURI* aReferrer);

  nsIURI* URI() const;

  void SetURI(nsIURI* aURI);

  nsIURI* OriginalURI() const;

  void SetOriginalURI(nsIURI* aOriginalURI);

  nsIURI* ResultPrincipalURI() const;

  void SetResultPrincipalURI(nsIURI* aResultPrincipalURI);

  bool ResultPrincipalURIIsSome() const;

  void SetResultPrincipalURIIsSome(bool aIsSome);

  bool KeepResultPrincipalURIIfSet() const;

  void SetKeepResultPrincipalURIIfSet(bool aKeep);

  nsIPrincipal* PrincipalToInherit() const;

  void SetPrincipalToInherit(nsIPrincipal* aPrincipalToInherit);

  bool LoadReplace() const;

  void SetLoadReplace(bool aLoadReplace);

  nsIPrincipal* TriggeringPrincipal() const;

  void SetTriggeringPrincipal(nsIPrincipal* aTriggeringPrincipal);

  bool InheritPrincipal() const;

  void SetInheritPrincipal(bool aInheritPrincipal);

  bool PrincipalIsExplicit() const;

  void SetPrincipalIsExplicit(bool aPrincipalIsExplicit);

  bool ForceAllowDataURI() const;

  void SetForceAllowDataURI(bool aForceAllowDataURI);

  bool OriginalFrameSrc() const;

  void SetOriginalFrameSrc(bool aOriginalFrameSrc);

  uint32_t LoadType() const;

  void SetLoadType(uint32_t aLoadType);

  nsISHEntry* SHEntry() const;

  void SetSHEntry(nsISHEntry* aSHEntry);

  const nsString& Target() const;

  void SetTarget(const nsAString& aTarget);

  nsIInputStream* PostDataStream() const;

  void SetPostDataStream(nsIInputStream* aStream);

  nsIInputStream* HeadersStream() const;

  void SetHeadersStream(nsIInputStream* aHeadersStream);

  bool SendReferrer() const;

  void SetSendReferrer(bool aSendReferrer);

  uint32_t ReferrerPolicy() const;

  void SetReferrerPolicy(mozilla::net::ReferrerPolicy aReferrerPolicy);

  bool IsSrcdocLoad() const;

  const nsString& SrcdocData() const;

  void SetSrcdocData(const nsAString& aSrcdocData);

  nsIDocShell* SourceDocShell() const;

  void SetSourceDocShell(nsIDocShell* aSourceDocShell);

  nsIURI* BaseURI() const;

  void SetBaseURI(nsIURI* aBaseURI);

  // Helper function allowing convenient work with mozilla::Maybe in C++, hiding
  // resultPrincipalURI and resultPrincipalURIIsSome attributes from the
  // consumer.
  void GetMaybeResultPrincipalURI(
      mozilla::Maybe<nsCOMPtr<nsIURI>>& aRPURI) const;

  void SetMaybeResultPrincipalURI(
      mozilla::Maybe<nsCOMPtr<nsIURI>> const& aRPURI);

  uint32_t LoadFlags() const;

  void SetLoadFlags(uint32_t aFlags);

  bool FirstParty() const;

  void SetFirstParty(bool aFirstParty);

  const nsCString& TypeHint() const;

  void SetTypeHint(const nsCString& aTypeHint);

  const nsString& FileName() const;

  void SetFileName(const nsAString& aFileName);

  uint32_t DocShellInternalLoadFlags() const;

  void SetDocShellInternalLoadFlags(uint32_t aFlags);

  // Give the type of DocShell we're loading into (chrome/content/etc) and
  // origin attributes for the URI we're loading, figure out if we should
  // inherit our principal from the document the load was requested from, or
  // else if the principal should be set up later in the process (after loads).
  // See comments in function for more info on principal selection algorithm
  nsresult SetupInheritingPrincipal(
      uint32_t aItemType, const mozilla::OriginAttributes& aOriginAttributes);

  // If no triggering principal exists at the moment, create one using referrer
  // information and origin attributes.
  nsresult SetupTriggeringPrincipal(
      const mozilla::OriginAttributes& aOriginAttributes);

  void SetIsFromProcessingFrameAttributes() {
    mIsFromProcessingFrameAttributes = true;
  }
  bool GetIsFromProcessingFrameAttributes() {
    return mIsFromProcessingFrameAttributes;
  }

  // When loading a document through nsDocShell::LoadURI(), a special set of
  // flags needs to be set based on other values in nsDocShellLoadState. This
  // function calculates those flags, before the LoadState is passed to
  // nsDocShell::InternalLoad.
  void CalculateDocShellInternalLoadFlags();

 protected:
  // Destructor can't be defaulted or inlined, as header doesn't have all type
  // includes it needs to do so.
  ~nsDocShellLoadState();

 protected:
  // This is the referrer for the load.
  nsCOMPtr<nsIURI> mReferrer;

  // The URI we are navigating to. Will not be null once set.
  nsCOMPtr<nsIURI> mURI;

  // The originalURI to be passed to nsIDocShell.internalLoad. May be null.
  nsCOMPtr<nsIURI> mOriginalURI;

  // Result principal URL from nsILoadInfo, may be null. Valid only if
  // mResultPrincipalURIIsSome is true (has the same meaning as isSome() on
  // mozilla::Maybe.)
  nsCOMPtr<nsIURI> mResultPrincipalURI;
  bool mResultPrincipalURIIsSome;

  // The principal of the load, that is, the entity responsible for causing the
  // load to occur. In most cases the referrer and the triggeringPrincipal's URI
  // will be identical.
  nsCOMPtr<nsIPrincipal> mTriggeringPrincipal;

  // if http-equiv="refresh" cause reload we do not want to replace
  // ResultPrinicpalURI if it was already set.
  bool mKeepResultPrincipalURIIfSet;

  // loadReplace flag to be passed to nsIDocShell.internalLoad.
  bool mLoadReplace;

  // If this attribute is true and no triggeringPrincipal is specified,
  // copy the principal from the referring document.
  bool mInheritPrincipal;

  // If this attribute is true only ever use the principal specified
  // by the triggeringPrincipal and inheritPrincipal attributes.
  // If there are security reasons for why this is unsafe, such
  // as trying to use a systemprincipal as the triggeringPrincipal
  // for a content docshell the load fails.
  bool mPrincipalIsExplicit;

  // Principal we're inheriting. If null, this means the principal should be
  // inherited from the current document. If set to NullPrincipal, the channel
  // will fill in principal information later in the load. See internal function
  // comments for more info.
  nsCOMPtr<nsIPrincipal> mPrincipalToInherit;

  // If this attribute is true, then a top-level navigation
  // to a data URI will be allowed.
  bool mForceAllowDataURI;

  // If this attribute is true, this load corresponds to a frame
  // element loading its original src (or srcdoc) attribute.
  bool mOriginalFrameSrc;

  // True if the referrer should be sent, false if it shouldn't be sent, even if
  // it's available. This attribute defaults to true.
  bool mSendReferrer;

  // Referrer policy for the load. This attribute holds one of the values
  // (REFERRER_POLICY_*) defined in nsIHttpChannel.
  mozilla::net::ReferrerPolicy mReferrerPolicy;

  // Contains a load type as specified by the nsDocShellLoadTypes::load*
  // constants
  uint32_t mLoadType;

  // SHEntry for this page
  nsCOMPtr<nsISHEntry> mSHEntry;

  // Target for load, like _content, _blank etc.
  nsString mTarget;

  // Post data
  nsCOMPtr<nsIInputStream> mPostDataStream;

  // Additional Headers
  nsCOMPtr<nsIInputStream> mHeadersStream;

  // True if the docshell has been created to load an iframe where the srcdoc
  // attribute has been set. Set when srcdocData is specified.
  bool mIsSrcdocLoad;

  // When set, the load will be interpreted as a srcdoc load, where contents of
  // this string will be loaded instead of the URI. Setting srcdocData sets
  // isSrcdocLoad to true
  nsString mSrcdocData;

  // When set, this is the Source Browsing Context for the navigation.
  nsCOMPtr<nsIDocShell> mSourceDocShell;

  // Used for srcdoc loads to give view-source knowledge of the load's base URI
  // as this information isn't embedded in the load's URI.
  nsCOMPtr<nsIURI> mBaseURI;

  // Set of Load Flags, taken from nsDocShellLoadTypes.h
  uint32_t mLoadFlags;

  // Is this a First Party Load?
  bool mFirstParty;

  // A hint as to the content-type of the resulting data. If no hint, IsVoid()
  // should return true.
  nsCString mTypeHint;

  // Non-void when the link should be downloaded as the given filename.
  // mFileName being non-void but empty means that no filename hint was
  // specified, but link should still trigger a download. If not a download,
  // mFileName.IsVoid() should return true.
  nsString mFileName;

  // LoadFlags calculated in nsDocShell::LoadURI and passed to
  // nsDocShell::InternalLoad, taken from the INTERNAL_LOAD consts in
  // nsIDocShell.idl
  uint32_t mDocShellInternalLoadFlags;

  // This will be true if this load is triggered by attribute changes.
  // See nsILoadInfo.isFromProcessingFrameAttributes
  bool mIsFromProcessingFrameAttributes;
};

#endif /* nsDocShellLoadState_h__ */
