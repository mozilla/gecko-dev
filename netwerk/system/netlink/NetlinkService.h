/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set et sw=2 ts=4: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef NETLINKSERVICE_H_
#define NETLINKSERVICE_H_

#include <netinet/in.h>
#include <linux/netlink.h>

#include "nsINetworkLinkService.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"
#include "nsCOMPtr.h"
#include "mozilla/Mutex.h"
#include "mozilla/TimeStamp.h"
#include "nsITimer.h"
#include "nsClassHashtable.h"
#include "mozilla/SHA1.h"

namespace mozilla {
namespace net {

#if defined(NIGHTLY_BUILD) || defined(MOZ_DEV_EDITION) || defined(DEBUG)
#  define NL_DEBUG_LOG
#endif

class NetlinkAddress;
class NetlinkNeighbor;
class NetlinkLink;
class NetlinkRoute;
class NetlinkMsg;

class NetlinkServiceListener : public nsISupports {
 public:
  virtual void OnNetworkChanged() = 0;
  virtual void OnLinkUp() = 0;
  virtual void OnLinkDown() = 0;
  virtual void OnLinkStatusKnown() = 0;

 protected:
  virtual ~NetlinkServiceListener() = default;
};

class NetlinkService : public nsIRunnable {
  virtual ~NetlinkService();

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  NetlinkService();
  nsresult Init(NetlinkServiceListener* aListener);
  nsresult Shutdown();
  void GetNetworkID(nsACString& aNetworkID);
  void GetIsLinkUp(bool* aIsUp);
  nsresult GetDnsSuffixList(nsTArray<nsCString>& aDnsSuffixList);

 private:
  void EnqueueGenMsg(uint16_t aMsgType, uint8_t aFamily);
  void EnqueueRtMsg(uint8_t aFamily, void* aAddress);
  void RemovePendingMsg();

  mozilla::Mutex mMutex;

  void OnNetlinkMessage(int aNetlinkSocket);
  void OnLinkMessage(struct nlmsghdr* aNlh);
  void OnAddrMessage(struct nlmsghdr* aNlh);
  void OnRouteMessage(struct nlmsghdr* aNlh);
  void OnNeighborMessage(struct nlmsghdr* aNlh);
  void OnRouteCheckResult(struct nlmsghdr* aNlh);

  void UpdateLinkStatus();

  void TriggerNetworkIDCalculation();
  int GetPollWait();
  bool CalculateIDForFamily(uint8_t aFamily, mozilla::SHA1Sum* aSHA1);
  void CalculateNetworkID();
  void ComputeDNSSuffixList();

  nsCOMPtr<nsIThread> mThread;

  bool mInitialScanFinished;

  // A pipe to signal shutdown with.
  int mShutdownPipe[2];

  // Is true if preference network.netlink.route.check.IPv4 was successfully
  // parsed and stored to mRouteCheckIPv4
  bool mDoRouteCheckIPv4;
  struct in_addr mRouteCheckIPv4;

  // Is true if preference network.netlink.route.check.IPv6 was successfully
  // parsed and stored to mRouteCheckIPv6
  bool mDoRouteCheckIPv6;
  struct in6_addr mRouteCheckIPv6;

  pid_t mPid;
  uint32_t mMsgId;

  bool mLinkUp;

  // Flag indicating that network ID could change and should be recalculated.
  // Calculation is postponed until we receive responses to all enqueued
  // messages.
  bool mRecalculateNetworkId;

  // Time stamp of setting mRecalculateNetworkId to true
  mozilla::TimeStamp mTriggerTime;

  nsCString mNetworkId;
  nsTArray<nsCString> mDNSSuffixList;

  class LinkInfo {
   public:
    explicit LinkInfo(NetlinkLink* aLink);
    virtual ~LinkInfo();

    // Updates mIsUp according to current mLink and mAddresses. Returns true if
    // the value has changed.
    bool UpdateLinkStatus();

    // NetlinkLink structure for this link
    nsAutoPtr<NetlinkLink> mLink;

    // All IPv4/IPv6 addresses on this link
    nsTArray<nsAutoPtr<NetlinkAddress> > mAddresses;

    // All neighbors on this link, key is an address
    nsClassHashtable<nsCStringHashKey, NetlinkNeighbor> mNeighbors;

    // Default IPv4/IPv6 routes
    nsTArray<nsAutoPtr<NetlinkRoute> > mDefaultRoutes;

    // Link is up when it's running, it's not a loopback and there is
    // a non-local address associated with it.
    bool mIsUp;
  };

  nsClassHashtable<nsUint32HashKey, LinkInfo> mLinks;

  // Route for mRouteCheckIPv4 address
  nsAutoPtr<NetlinkRoute> mIPv4RouteCheckResult;
  // Route for mRouteCheckIPv6 address
  nsAutoPtr<NetlinkRoute> mIPv6RouteCheckResult;

  nsTArray<nsAutoPtr<NetlinkMsg> > mOutgoingMessages;

  RefPtr<NetlinkServiceListener> mListener;
};

}  // namespace net
}  // namespace mozilla

#endif /* NETLINKSERVICE_H_ */
