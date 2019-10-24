/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSNETWORKLINKSERVICEMAC_H_
#define NSNETWORKLINKSERVICEMAC_H_

#include "nsINetworkLinkService.h"
#include "nsIObserver.h"
#include "nsITimer.h"
#include "mozilla/Mutex.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/SHA1.h"

#include <SystemConfiguration/SCNetworkReachability.h>
#include <SystemConfiguration/SystemConfiguration.h>

using prefix_and_netmask = std::pair<in6_addr, in6_addr>;

class nsNetworkLinkService : public nsINetworkLinkService,
                             public nsIObserver,
                             public nsITimerCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSINETWORKLINKSERVICE
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITIMERCALLBACK

  nsNetworkLinkService();

  nsresult Init();
  nsresult Shutdown();

  static void HashSortedPrefixesAndNetmasks(
      std::vector<prefix_and_netmask> prefixAndNetmaskStore,
      mozilla::SHA1Sum* sha1);

 protected:
  virtual ~nsNetworkLinkService();

 private:
  bool mLinkUp;
  bool mStatusKnown;

  SCNetworkReachabilityRef mReachability;
  CFRunLoopRef mCFRunLoop;
  CFRunLoopSourceRef mRunLoopSource;
  SCDynamicStoreRef mStoreRef;

  void UpdateReachability();
  void SendEvent(bool aNetworkChanged);
  static void ReachabilityChanged(SCNetworkReachabilityRef target,
                                  SCNetworkConnectionFlags flags, void* info);
  static void IPConfigChanged(SCDynamicStoreRef store, CFArrayRef changedKeys,
                              void* info);
  void calculateNetworkIdWithDelay(uint32_t aDelay);
  void calculateNetworkIdInternal(void);

  mozilla::Mutex mMutex;
  nsCString mNetworkId;

  // Time stamp of last NS_NETWORK_LINK_DATA_CHANGED event
  mozilla::TimeStamp mNetworkChangeTime;

  // The timer used to delay the calculation of network id since it takes some
  // time to discover the gateway's MAC address.
  nsCOMPtr<nsITimer> mNetworkIdTimer;
};

#endif /* NSNETWORKLINKSERVICEMAC_H_ */
