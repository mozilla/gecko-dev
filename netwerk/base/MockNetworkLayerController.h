/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MockNetworkLayerController_h__
#define MockNetworkLayerController_h__

#include "mozilla/net/DNS.h"
#include "mozilla/RWLock.h"
#include "nsIMockNetworkLayerController.h"
#include "nsTHashMap.h"
#include "nsTHashSet.h"

namespace mozilla::net {

bool FindNetAddrOverride(const NetAddr& aInput, NetAddr& aOutput);
bool FindBlockedUDPAddr(const NetAddr& aInput);

class MockNetworkLayerController : public nsIMockNetworkLayerController {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIMOCKNETWORKLAYERCONTROLLER

  MockNetworkLayerController() = default;

  static already_AddRefed<nsIMockNetworkLayerController> GetSingleton();

 private:
  virtual ~MockNetworkLayerController() = default;
  mozilla::RWLock mLock{"MockNetworkLayerController::mLock"};

  nsTHashMap<nsCStringHashKey, NetAddr> mNetAddrOverrides MOZ_GUARDED_BY(mLock);
  nsTHashSet<nsCStringHashKey> mBlockedUDPAddresses MOZ_GUARDED_BY(mLock);

  friend bool FindNetAddrOverride(const NetAddr& aInput, NetAddr& aOutput);
  friend bool FindBlockedUDPAddr(const NetAddr& aInput);
};

}  // namespace mozilla::net

#endif  // MockNetworkLayerController_h__
