/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_LoadInfo_h
#define mozilla_LoadInfo_h

#include "nsIContentPolicy.h"
#include "nsILoadInfo.h"
#include "nsIPrincipal.h"
#include "nsIWeakReferenceUtils.h" // for nsWeakPtr
#include "nsIURI.h"

class nsINode;

namespace mozilla {

namespace net {
class LoadInfoArgs;
}

namespace ipc {
// we have to forward declare that function so we can use it as a friend.
nsresult
LoadInfoArgsToLoadInfo(const mozilla::net::LoadInfoArgs& aLoadInfoArgs,
                       nsILoadInfo** outLoadInfo);
}

/**
 * Class that provides an nsILoadInfo implementation.
 *
 * Note that there is no reason why this class should be MOZ_EXPORT, but
 * Thunderbird relies on some insane hacks which require this, so we'll leave it
 * as is for now, but hopefully we'll be able to remove the MOZ_EXPORT keyword
 * from this class at some point.  See bug 1149127 for the discussion.
 */
class MOZ_EXPORT LoadInfo final : public nsILoadInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSILOADINFO

  // aLoadingPrincipal MUST NOT BE NULL.
  LoadInfo(nsIPrincipal* aLoadingPrincipal,
           nsIPrincipal* aTriggeringPrincipal,
           nsINode* aLoadingContext,
           nsSecurityFlags aSecurityFlags,
           nsContentPolicyType aContentPolicyType,
           nsIURI* aBaseURI = nullptr);

private:
  // private constructor that is only allowed to be called from within
  // HttpChannelParent and FTPChannelParent declared as friends undeneath.
  // In e10s we can not serialize nsINode, hence we store the innerWindowID.
  LoadInfo(nsIPrincipal* aLoadingPrincipal,
           nsIPrincipal* aTriggeringPrincipal,
           nsSecurityFlags aSecurityFlags,
           nsContentPolicyType aContentPolicyType,
           uint64_t aInnerWindowID,
           uint64_t aOuterWindowID,
           uint64_t aParentOuterWindowID);

  friend nsresult
  mozilla::ipc::LoadInfoArgsToLoadInfo(const mozilla::net::LoadInfoArgs& aLoadInfoArgs,
                                       nsILoadInfo** outLoadInfo);

  ~LoadInfo();

  nsCOMPtr<nsIPrincipal> mLoadingPrincipal;
  nsCOMPtr<nsIPrincipal> mTriggeringPrincipal;
  nsWeakPtr mLoadingContext;
  nsSecurityFlags mSecurityFlags;
  nsContentPolicyType mContentPolicyType;
  nsCOMPtr<nsIURI> mBaseURI;
  uint64_t mInnerWindowID;
  uint64_t mOuterWindowID;
  uint64_t mParentOuterWindowID;
};

} // namespace mozilla

#endif // mozilla_LoadInfo_h

