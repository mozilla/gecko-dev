/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set et sw=2 ts=4: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <arpa/inet.h>
#include <netinet/ether.h>
#include <net/if.h>
#include <poll.h>
#include <linux/rtnetlink.h>

#include "nsThreadUtils.h"
#include "nsServiceManagerUtils.h"
#include "NetlinkService.h"
#include "nsString.h"
#include "nsPrintfCString.h"
#include "mozilla/Logging.h"
#include "../../base/IPv6Utils.h"

#include "mozilla/Base64.h"
#include "mozilla/FileUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/Sprintf.h"
#include "mozilla/Telemetry.h"
#include "mozilla/DebugOnly.h"

#if defined(HAVE_RES_NINIT)
#  include <netinet/in.h>
#  include <arpa/nameser.h>
#  include <resolv.h>
#endif

/* a shorter name that better explains what it does */
#define EINTR_RETRY(x) MOZ_TEMP_FAILURE_RETRY(x)

namespace mozilla {
namespace net {

// period during which to absorb subsequent network change events, in
// milliseconds
static const unsigned int kNetworkChangeCoalescingPeriod = 1000;

static LazyLogModule gNlSvcLog("NetlinkService");
#define LOG(args) MOZ_LOG(gNlSvcLog, mozilla::LogLevel::Debug, args)

typedef union {
  struct in_addr addr4;
  struct in6_addr addr6;
} in_common_addr;

static void GetAddrStr(const in_common_addr* aAddr, uint8_t aFamily,
                       nsACString& _retval) {
  char addr[INET6_ADDRSTRLEN];
  addr[0] = 0;

  if (aFamily == AF_INET) {
    inet_ntop(AF_INET, &(aAddr->addr4), addr, INET_ADDRSTRLEN);
  } else {
    inet_ntop(AF_INET6, &(aAddr->addr6), addr, INET6_ADDRSTRLEN);
  }
  _retval.Assign(addr);
}

class NetlinkAddress {
 public:
  NetlinkAddress() {}

  uint8_t Family() const { return mIfam.ifa_family; }
  uint32_t GetIndex() const { return mIfam.ifa_index; }
  uint8_t GetPrefixLen() const { return mIfam.ifa_prefixlen; }
  bool ScopeIsUniverse() const { return mIfam.ifa_scope == RT_SCOPE_UNIVERSE; }
  const in_common_addr* GetAddrPtr() const { return &mAddr; }

  bool Equals(const NetlinkAddress* aOther) const {
    if (mIfam.ifa_family != aOther->mIfam.ifa_family) {
      return false;
    }
    if (mIfam.ifa_index != aOther->mIfam.ifa_index) {
      // addresses are different when they are on a different interface
      return false;
    }
    if (mIfam.ifa_prefixlen != aOther->mIfam.ifa_prefixlen) {
      // It's possible to have two equal addresses with a different netmask on
      // the same interface, so we need to check prefixlen too.
      return false;
    }
    size_t addrSize = (mIfam.ifa_family == AF_INET) ? sizeof(mAddr.addr4)
                                                    : sizeof(mAddr.addr6);
    return memcmp(&mAddr, aOther->GetAddrPtr(), addrSize) == 0;
  }

  bool ContainsAddr(const in_common_addr* aAddr) {
    int32_t addrSize = (mIfam.ifa_family == AF_INET)
                           ? (int32_t)sizeof(mAddr.addr4)
                           : (int32_t)sizeof(mAddr.addr6);
    uint8_t maskit[] = {0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe};
    int32_t bits = mIfam.ifa_prefixlen;
    if (bits > addrSize * 8) {
      MOZ_ASSERT(false, "Unexpected prefix length!");
      LOG(("Unexpected prefix length %d, maximum for this family is %d", bits,
           addrSize * 8));
      return false;
    }
    for (int32_t i = 0; i < addrSize; i++) {
      uint8_t mask = (bits >= 8) ? 0xff : maskit[bits];
      if ((((unsigned char*)aAddr)[i] & mask) !=
          (((unsigned char*)(&mAddr))[i] & mask)) {
        return false;
      }
      bits -= 8;
      if (bits <= 0) {
        return true;
      }
    }
    return true;
  }

  bool Init(struct nlmsghdr* aNlh) {
    struct ifaddrmsg* ifam;
    struct rtattr* attr;
    int len;

    ifam = (ifaddrmsg*)NLMSG_DATA(aNlh);
    len = IFA_PAYLOAD(aNlh);

    if (ifam->ifa_family != AF_INET && ifam->ifa_family != AF_INET6) {
      return false;
    }

    bool hasAddr = false;
    for (attr = IFA_RTA(ifam); RTA_OK(attr, len); attr = RTA_NEXT(attr, len)) {
      if (attr->rta_type == IFA_ADDRESS || attr->rta_type == IFA_LOCAL) {
        memcpy(&mAddr, RTA_DATA(attr),
               ifam->ifa_family == AF_INET ? sizeof(mAddr.addr4)
                                           : sizeof(mAddr.addr6));
        hasAddr = true;
        if (attr->rta_type == IFA_LOCAL) {
          // local address is preferred, so don't continue parsing other
          // attributes
          break;
        }
      }
    }

    if (!hasAddr) {
      return false;
    }

    memcpy(&mIfam, (ifaddrmsg*)NLMSG_DATA(aNlh), sizeof(mIfam));
    return true;
  }

 private:
  in_common_addr mAddr;
  struct ifaddrmsg mIfam;
};

class NetlinkNeighbor {
 public:
  NetlinkNeighbor() : mHasMAC(false) {}

  uint8_t Family() const { return mNeigh.ndm_family; }
  uint32_t GetIndex() const { return mNeigh.ndm_ifindex; }
  const in_common_addr* GetAddrPtr() const { return &mAddr; }
  const uint8_t* GetMACPtr() const { return mMAC; }
  bool HasMAC() const { return mHasMAC; };

#ifdef NL_DEBUG_LOG
  void GetAsString(nsACString& _retval) const {
    nsAutoCString addrStr;
    _retval.Assign("addr=");
    GetAddrStr(&mAddr, mNeigh.ndm_family, addrStr);
    _retval.Append(addrStr);
    if (mNeigh.ndm_family == AF_INET) {
      _retval.Append(" family=AF_INET if=");
    } else {
      _retval.Append(" family=AF_INET6 if=");
    }
    _retval.AppendInt(mNeigh.ndm_ifindex);
    if (mHasMAC) {
      _retval.Append(" mac=");
      _retval.Append(nsPrintfCString("%02x:%02x:%02x:%02x:%02x:%02x", mMAC[0],
                                     mMAC[1], mMAC[2], mMAC[3], mMAC[4],
                                     mMAC[5]));
    }
  }
#endif

  bool Init(struct nlmsghdr* aNlh) {
    struct ndmsg* neigh;
    struct rtattr* attr;
    int len;

    neigh = (ndmsg*)NLMSG_DATA(aNlh);
    len = aNlh->nlmsg_len - NLMSG_LENGTH(sizeof(*neigh));

    if (neigh->ndm_family != AF_INET && neigh->ndm_family != AF_INET6) {
      return false;
    }

    bool hasDST = false;
    for (attr = RTM_RTA(neigh); RTA_OK(attr, len); attr = RTA_NEXT(attr, len)) {
      if (attr->rta_type == NDA_LLADDR) {
        memcpy(mMAC, RTA_DATA(attr), ETH_ALEN);
        mHasMAC = true;
      }

      if (attr->rta_type == NDA_DST) {
        memcpy(&mAddr, RTA_DATA(attr),
               neigh->ndm_family == AF_INET ? sizeof(mAddr.addr4)
                                            : sizeof(mAddr.addr6));
        hasDST = true;
      }
    }

    if (!hasDST) {
      return false;
    }

    memcpy(&mNeigh, (ndmsg*)NLMSG_DATA(aNlh), sizeof(mNeigh));
    return true;
  }

 private:
  bool mHasMAC;
  uint8_t mMAC[ETH_ALEN];
  in_common_addr mAddr;
  struct ndmsg mNeigh;
};

class NetlinkLink {
 public:
  NetlinkLink() {}

  bool IsUp() const {
    return (mIface.ifi_flags & IFF_RUNNING) &&
           !(mIface.ifi_flags & IFF_LOOPBACK);
  }

  void GetName(nsACString& _retval) const { _retval = mName; }
  bool IsTypeEther() const { return mIface.ifi_type == ARPHRD_ETHER; }
  uint32_t GetIndex() const { return mIface.ifi_index; }
  uint32_t GetFlags() const { return mIface.ifi_flags; }
  uint16_t GetType() const { return mIface.ifi_type; }

  bool Init(struct nlmsghdr* aNlh) {
    struct ifinfomsg* iface;
    struct rtattr* attr;
    int len;

    iface = (ifinfomsg*)NLMSG_DATA(aNlh);
    len = aNlh->nlmsg_len - NLMSG_LENGTH(sizeof(*iface));

    bool hasName = false;
    for (attr = IFLA_RTA(iface); RTA_OK(attr, len);
         attr = RTA_NEXT(attr, len)) {
      if (attr->rta_type == IFLA_IFNAME) {
        mName.Assign((char*)RTA_DATA(attr));
        hasName = true;
        break;
      }
    }

    if (!hasName) {
      return false;
    }

    memcpy(&mIface, (ifinfomsg*)NLMSG_DATA(aNlh), sizeof(mIface));
    return true;
  }

 private:
  nsCString mName;
  struct ifinfomsg mIface;
};

class NetlinkRoute {
 public:
  NetlinkRoute()
      : mHasGWAddr(false),
        mHasPrefSrcAddr(false),
        mHasDstAddr(false),
        mHasOif(false),
        mHasPrio(false) {}

  bool IsUnicast() const { return mRtm.rtm_type == RTN_UNICAST; }
  bool ScopeIsUniverse() const { return mRtm.rtm_scope == RT_SCOPE_UNIVERSE; }
  bool IsDefault() const { return mRtm.rtm_dst_len == 0; }
  bool HasOif() const { return mHasOif; }
  uint8_t Oif() const { return mOif; }
  uint8_t Family() const { return mRtm.rtm_family; }
  bool HasPrefSrcAddr() const { return mHasPrefSrcAddr; }
  const in_common_addr* GetGWAddrPtr() const {
    return mHasGWAddr ? &mGWAddr : nullptr;
  }
  const in_common_addr* GetPrefSrcAddrPtr() const {
    return mHasPrefSrcAddr ? &mPrefSrcAddr : nullptr;
  }

  bool Equals(const NetlinkRoute* aOther) const {
    size_t addrSize = (mRtm.rtm_family == AF_INET) ? sizeof(mDstAddr.addr4)
                                                   : sizeof(mDstAddr.addr6);
    if (memcmp(&mRtm, &(aOther->mRtm), sizeof(mRtm))) {
      return false;
    }
    if (mHasOif != aOther->mHasOif || mOif != aOther->mOif) {
      return false;
    }
    if (mHasPrio != aOther->mHasPrio || mPrio != aOther->mPrio) {
      return false;
    }
    if ((mHasGWAddr != aOther->mHasGWAddr) ||
        (mHasGWAddr && memcmp(&mGWAddr, &(aOther->mGWAddr), addrSize))) {
      return false;
    }
    if ((mHasDstAddr != aOther->mHasDstAddr) ||
        (mHasDstAddr && memcmp(&mDstAddr, &(aOther->mDstAddr), addrSize))) {
      return false;
    }
    if ((mHasPrefSrcAddr != aOther->mHasPrefSrcAddr) ||
        (mHasPrefSrcAddr &&
         memcmp(&mPrefSrcAddr, &(aOther->mPrefSrcAddr), addrSize))) {
      return false;
    }
    return true;
  }

  bool GatewayEquals(const NetlinkNeighbor* aNeigh) const {
    if (!mHasGWAddr) {
      return false;
    }
    if (aNeigh->Family() != mRtm.rtm_family) {
      return false;
    }
    size_t addrSize = (mRtm.rtm_family == AF_INET) ? sizeof(mGWAddr.addr4)
                                                   : sizeof(mGWAddr.addr6);
    return memcmp(&mGWAddr, aNeigh->GetAddrPtr(), addrSize) == 0;
  }

  bool GatewayEquals(const NetlinkRoute* aRoute) const {
    if (!mHasGWAddr || !aRoute->mHasGWAddr) {
      return false;
    }
    if (mRtm.rtm_family != aRoute->mRtm.rtm_family) {
      return false;
    }
    size_t addrSize = (mRtm.rtm_family == AF_INET) ? sizeof(mGWAddr.addr4)
                                                   : sizeof(mGWAddr.addr6);
    return memcmp(&mGWAddr, &(aRoute->mGWAddr), addrSize) == 0;
  }

  bool PrefSrcAddrEquals(const NetlinkAddress* aAddress) const {
    if (!mHasPrefSrcAddr) {
      return false;
    }
    if (mRtm.rtm_family != aAddress->Family()) {
      return false;
    }
    size_t addrSize = (mRtm.rtm_family == AF_INET) ? sizeof(mPrefSrcAddr.addr4)
                                                   : sizeof(mPrefSrcAddr.addr6);
    return memcmp(&mPrefSrcAddr, aAddress->GetAddrPtr(), addrSize) == 0;
  }

#ifdef NL_DEBUG_LOG
  void GetAsString(nsACString& _retval) const {
    nsAutoCString addrStr;
    _retval.Assign("table=");
    _retval.AppendInt(mRtm.rtm_table);
    _retval.Append(" type=");
    _retval.AppendInt(mRtm.rtm_type);
    _retval.Append(" scope=");
    _retval.AppendInt(mRtm.rtm_scope);
    if (mRtm.rtm_family == AF_INET) {
      _retval.Append(" family=AF_INET dst=");
      addrStr.Assign("0.0.0.0/");
    } else {
      _retval.Append(" family=AF_INET6 dst=");
      addrStr.Assign("::/");
    }
    if (mHasDstAddr) {
      GetAddrStr(&mDstAddr, mRtm.rtm_family, addrStr);
      addrStr.Append("/");
    }
    _retval.Append(addrStr);
    _retval.AppendInt(mRtm.rtm_dst_len);
    if (mHasPrefSrcAddr) {
      _retval.Append(" src=");
      GetAddrStr(&mPrefSrcAddr, mRtm.rtm_family, addrStr);
      _retval.Append(addrStr);
    }
    if (mHasGWAddr) {
      _retval.Append(" via=");
      GetAddrStr(&mGWAddr, mRtm.rtm_family, addrStr);
      _retval.Append(addrStr);
    }
    if (mHasOif) {
      _retval.Append(" oif=");
      _retval.AppendInt(mOif);
    }
    if (mHasPrio) {
      _retval.Append(" prio=");
      _retval.AppendInt(mPrio);
    }
  }
#endif

  bool Init(struct nlmsghdr* aNlh) {
    struct rtmsg* rtm;
    struct rtattr* attr;
    int len;

    rtm = (rtmsg*)NLMSG_DATA(aNlh);
    len = RTM_PAYLOAD(aNlh);

    if (rtm->rtm_family != AF_INET && rtm->rtm_family != AF_INET6) {
      return false;
    }

    for (attr = RTM_RTA(rtm); RTA_OK(attr, len); attr = RTA_NEXT(attr, len)) {
      if (attr->rta_type == RTA_DST) {
        memcpy(&mDstAddr, RTA_DATA(attr),
               (rtm->rtm_family == AF_INET) ? sizeof(mDstAddr.addr4)
                                            : sizeof(mDstAddr.addr6));
        mHasDstAddr = true;
      } else if (attr->rta_type == RTA_GATEWAY) {
        memcpy(&mGWAddr, RTA_DATA(attr),
               (rtm->rtm_family == AF_INET) ? sizeof(mGWAddr.addr4)
                                            : sizeof(mGWAddr.addr6));
        mHasGWAddr = true;
      } else if (attr->rta_type == RTA_PREFSRC) {
        memcpy(&mPrefSrcAddr, RTA_DATA(attr),
               (rtm->rtm_family == AF_INET) ? sizeof(mPrefSrcAddr.addr4)
                                            : sizeof(mPrefSrcAddr.addr6));
        mHasPrefSrcAddr = true;
      } else if (attr->rta_type == RTA_OIF) {
        mOif = *(uint32_t*)RTA_DATA(attr);
        mHasOif = true;
      } else if (attr->rta_type == RTA_PRIORITY) {
        mPrio = *(uint32_t*)RTA_DATA(attr);
        mHasPrio = true;
      }
    }

    memcpy(&mRtm, (rtmsg*)NLMSG_DATA(aNlh), sizeof(mRtm));
    return true;
  }

 private:
  bool mHasGWAddr : 1;
  bool mHasPrefSrcAddr : 1;
  bool mHasDstAddr : 1;
  bool mHasOif : 1;
  bool mHasPrio : 1;

  in_common_addr mGWAddr;
  in_common_addr mDstAddr;
  in_common_addr mPrefSrcAddr;

  uint32_t mOif;
  uint32_t mPrio;
  struct rtmsg mRtm;
};

class NetlinkMsg {
 public:
  static uint8_t const kGenMsg = 1;
  static uint8_t const kRtMsg = 2;

  NetlinkMsg() : mIsPending(false) {}
  virtual ~NetlinkMsg() = default;

  virtual bool Send(int aFD) = 0;
  virtual bool IsPending() { return mIsPending; }
  virtual uint32_t SeqId() = 0;
  virtual uint8_t Family() = 0;
  virtual uint8_t MsgType() = 0;

 protected:
  bool SendRequest(int aFD, void* aRequest, uint32_t aRequestLength) {
    MOZ_ASSERT(!mIsPending, "Request has been already sent!");

    struct sockaddr_nl kernel;
    memset(&kernel, 0, sizeof(kernel));
    kernel.nl_family = AF_NETLINK;
    kernel.nl_groups = 0;

    struct iovec io;
    memset(&io, 0, sizeof(io));
    io.iov_base = aRequest;
    io.iov_len = aRequestLength;

    struct msghdr rtnl_msg;
    memset(&rtnl_msg, 0, sizeof(rtnl_msg));
    rtnl_msg.msg_iov = &io;
    rtnl_msg.msg_iovlen = 1;
    rtnl_msg.msg_name = &kernel;
    rtnl_msg.msg_namelen = sizeof(kernel);

    ssize_t rc = EINTR_RETRY(sendmsg(aFD, (struct msghdr*)&rtnl_msg, 0));
    if (rc > 0 && (uint32_t)rc == aRequestLength) {
      mIsPending = true;
    }

    return mIsPending;
  }

  bool mIsPending;
};

class NetlinkGenMsg : public NetlinkMsg {
 public:
  NetlinkGenMsg(uint16_t aMsgType, uint8_t aFamily, uint32_t aSeqId) {
    memset(&mReq, 0, sizeof(mReq));

    mReq.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtgenmsg));
    mReq.hdr.nlmsg_type = aMsgType;
    mReq.hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    mReq.hdr.nlmsg_seq = aSeqId;
    mReq.hdr.nlmsg_pid = 0;

    mReq.gen.rtgen_family = aFamily;
  }

  virtual bool Send(int aFD) {
    return SendRequest(aFD, &mReq, mReq.hdr.nlmsg_len);
  }

  virtual uint32_t SeqId() { return mReq.hdr.nlmsg_seq; }
  virtual uint8_t Family() { return mReq.gen.rtgen_family; }
  virtual uint8_t MsgType() { return kGenMsg; }

 private:
  struct {
    struct nlmsghdr hdr;
    struct rtgenmsg gen;
  } mReq;
};

class NetlinkRtMsg : public NetlinkMsg {
 public:
  NetlinkRtMsg(uint8_t aFamily, void* aAddress, uint32_t aSeqId) {
    MOZ_ASSERT(aFamily == AF_INET || aFamily == AF_INET6);

    memset(&mReq, 0, sizeof(mReq));

    mReq.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
    mReq.hdr.nlmsg_type = RTM_GETROUTE;
    mReq.hdr.nlmsg_flags = NLM_F_REQUEST;
    mReq.hdr.nlmsg_seq = aSeqId;
    mReq.hdr.nlmsg_pid = 0;

    mReq.rtm.rtm_family = aFamily;
    mReq.rtm.rtm_flags = 0;
    mReq.rtm.rtm_dst_len = aFamily == AF_INET ? 32 : 128;

    struct rtattr* rta;
    rta = (struct rtattr*)(((char*)&mReq) + NLMSG_ALIGN(mReq.hdr.nlmsg_len));
    rta->rta_type = RTA_DST;
    size_t addrSize =
        aFamily == AF_INET ? sizeof(struct in_addr) : sizeof(struct in6_addr);
    rta->rta_len = RTA_LENGTH(addrSize);
    memcpy(RTA_DATA(rta), aAddress, addrSize);
    mReq.hdr.nlmsg_len = NLMSG_ALIGN(mReq.hdr.nlmsg_len) + RTA_LENGTH(addrSize);
  }

  virtual bool Send(int aFD) {
    return SendRequest(aFD, &mReq, mReq.hdr.nlmsg_len);
  }

  virtual uint32_t SeqId() { return mReq.hdr.nlmsg_seq; }
  virtual uint8_t Family() { return mReq.rtm.rtm_family; }
  virtual uint8_t MsgType() { return kRtMsg; }

 private:
  struct {
    struct nlmsghdr hdr;
    struct rtmsg rtm;
    unsigned char data[1024];
  } mReq;
};

NetlinkService::LinkInfo::LinkInfo(NetlinkLink* aLink)
    : mLink(aLink), mIsUp(false) {}

NetlinkService::LinkInfo::~LinkInfo() {}

bool NetlinkService::LinkInfo::UpdateLinkStatus() {
  LOG(("NetlinkService::LinkInfo::UpdateLinkStatus"));

  bool oldIsUp = mIsUp;
  mIsUp = false;

  if (!mLink->IsUp()) {
    // The link is not up or is a loopback
    LOG(("The link is down or is a loopback"));
  } else {
    // Link is up when there is non-local address associated with it.
    for (uint32_t i = 0; i < mAddresses.Length(); ++i) {
#ifdef NL_DEBUG_LOG
      nsAutoCString dbgStr;
      GetAddrStr(mAddresses[i]->GetAddrPtr(), mAddresses[i]->Family(), dbgStr);
      LOG(("checking address %s", dbgStr.get()));
#endif
      if (mAddresses[i]->ScopeIsUniverse()) {
        mIsUp = true;
        LOG(("global address found"));
        break;
      }
    }
  }

  return mIsUp == oldIsUp;
}

NS_IMPL_ISUPPORTS(NetlinkService, nsIRunnable)

NetlinkService::NetlinkService()
    : mMutex("NetlinkService::mMutex"),
      mInitialScanFinished(false),
      mDoRouteCheckIPv4(false),
      mDoRouteCheckIPv6(false),
      mMsgId(0),
      mLinkUp(true),
      mRecalculateNetworkId(false) {
  mPid = getpid();
  mShutdownPipe[0] = -1;
  mShutdownPipe[1] = -1;
}

NetlinkService::~NetlinkService() {
  MOZ_ASSERT(!mThread, "NetlinkService thread shutdown failed");

  if (mShutdownPipe[0] != -1) {
    EINTR_RETRY(close(mShutdownPipe[0]));
  }
  if (mShutdownPipe[1] != -1) {
    EINTR_RETRY(close(mShutdownPipe[1]));
  }
}

void NetlinkService::OnNetlinkMessage(int aNetlinkSocket) {
  // The buffer size 4096 is a common page size, which is a recommended limit
  // for netlink messages.
  char buffer[4096];

  struct sockaddr_nl kernel;
  memset(&kernel, 0, sizeof(kernel));
  kernel.nl_family = AF_NETLINK;
  kernel.nl_groups = 0;

  struct iovec io;
  memset(&io, 0, sizeof(io));
  io.iov_base = buffer;
  io.iov_len = sizeof(buffer);

  struct msghdr rtnl_reply;
  memset(&rtnl_reply, 0, sizeof(rtnl_reply));
  rtnl_reply.msg_iov = &io;
  rtnl_reply.msg_iovlen = 1;
  rtnl_reply.msg_name = &kernel;
  rtnl_reply.msg_namelen = sizeof(kernel);

  ssize_t rc = EINTR_RETRY(recvmsg(aNetlinkSocket, &rtnl_reply, MSG_DONTWAIT));
  if (rc < 0) {
    return;
  }
  size_t netlink_bytes = rc;

  struct nlmsghdr* nlh = reinterpret_cast<struct nlmsghdr*>(buffer);

  for (; NLMSG_OK(nlh, netlink_bytes); nlh = NLMSG_NEXT(nlh, netlink_bytes)) {
    // If PID in the message is our PID, then it's a response to our request.
    // Otherwise it's a multicast message.
    bool isResponse = (pid_t)nlh->nlmsg_pid == mPid;
    if (isResponse) {
      if (!mOutgoingMessages.Length() || !mOutgoingMessages[0]->IsPending()) {
        // There is no enqueued message pending?
        LOG((
            "Ignoring message seq_id %u, because there is no associated message"
            " pending",
            nlh->nlmsg_seq));
        continue;
      }

      if (mOutgoingMessages[0]->SeqId() != nlh->nlmsg_seq) {
        LOG(("Received unexpected seq_id [received=%u, expected=%u]",
             nlh->nlmsg_seq, mOutgoingMessages[0]->SeqId()));
        RemovePendingMsg();
        continue;
      }
    }

    switch (nlh->nlmsg_type) {
      case NLMSG_DONE: /* Message signalling end of dump for responses to
                          request containing NLM_F_DUMP flag */
        LOG(("received NLMSG_DONE"));
        if (isResponse) {
          RemovePendingMsg();
        }
        break;
      case NLMSG_ERROR:
        LOG(("received NLMSG_ERROR"));
        if (isResponse) {
          if (mOutgoingMessages[0]->MsgType() == NetlinkMsg::kRtMsg) {
            OnRouteCheckResult(nullptr);
          }
          RemovePendingMsg();
        }
        break;
      case RTM_NEWLINK:
      case RTM_DELLINK:
        MOZ_ASSERT(!isResponse ||
                   (nlh->nlmsg_flags & NLM_F_MULTI) == NLM_F_MULTI);
        OnLinkMessage(nlh);
        break;
      case RTM_NEWADDR:
      case RTM_DELADDR:
        MOZ_ASSERT(!isResponse ||
                   (nlh->nlmsg_flags & NLM_F_MULTI) == NLM_F_MULTI);
        OnAddrMessage(nlh);
        break;
      case RTM_NEWROUTE:
      case RTM_DELROUTE:
        if (isResponse && ((nlh->nlmsg_flags & NLM_F_MULTI) != NLM_F_MULTI)) {
          // If it's not multipart message, then it must be response to a route
          // check.
          MOZ_ASSERT(mOutgoingMessages[0]->MsgType() == NetlinkMsg::kRtMsg);
          OnRouteCheckResult(nlh);
          RemovePendingMsg();
        } else {
          OnRouteMessage(nlh);
        }
        break;
      case RTM_NEWNEIGH:
      case RTM_DELNEIGH:
        MOZ_ASSERT(!isResponse ||
                   (nlh->nlmsg_flags & NLM_F_MULTI) == NLM_F_MULTI);
        OnNeighborMessage(nlh);
        break;
      default:
        break;
    }
  }
}

void NetlinkService::OnLinkMessage(struct nlmsghdr* aNlh) {
  LOG(("NetlinkService::OnLinkMessage [type=%s]",
       aNlh->nlmsg_type == RTM_NEWLINK ? "new" : "del"));

  nsAutoPtr<NetlinkLink> link(new NetlinkLink());
  if (!link->Init(aNlh)) {
    return;
  }

  uint32_t linkIndex = link->GetIndex();
  nsAutoCString linkName;
  link->GetName(linkName);

  bool checkLinks = false;
  LinkInfo* linkInfo = nullptr;
  mLinks.Get(linkIndex, &linkInfo);

  if (aNlh->nlmsg_type == RTM_NEWLINK) {
    if (!linkInfo) {
      LOG(("Creating new link [index=%u, name=%s, flags=%u, type=%u]",
           linkIndex, linkName.get(), link->GetFlags(), link->GetType()));
      linkInfo = new LinkInfo(link.forget());
      mLinks.Put(linkIndex, linkInfo);
    } else {
      LOG(("Updating link [index=%u, name=%s, flags=%u, type=%u]", linkIndex,
           linkName.get(), link->GetFlags(), link->GetType()));

      // Check whether administrative state has changed.
      if (linkInfo->mLink->GetFlags() & IFF_UP &&
          !(link->GetFlags() & IFF_UP)) {
        LOG(("  link went down"));
        // If the link went down, remove all routes and neighbors, but keep
        // addresses.
        linkInfo->mDefaultRoutes.Clear();
        linkInfo->mNeighbors.Clear();
      }

      linkInfo->mLink = link.forget();
      if (linkInfo->UpdateLinkStatus()) {
        // Link status has changed
        checkLinks = true;
      }
    }
  } else {
    if (!linkInfo) {
      // This can happen during startup
      LOG(("Link info doesn't exist [index=%u, name=%s]", linkIndex,
           linkName.get()));
    } else {
      LOG(("Removing link [index=%u, name=%s]", linkIndex, linkName.get()));
      if (linkInfo->mIsUp) {
        // We're removing link that is up, check link status
        checkLinks = true;
      }
      mLinks.Remove(linkIndex);
    }
  }

  if (checkLinks) {
    UpdateLinkStatus();
  }
}

void NetlinkService::UpdateLinkStatus() {
  if (!mInitialScanFinished) {
    // Wait until we get all links via netlink
    return;
  }

  LOG(("NetlinkService::UpdateLinkStatus"));

  // Link is up when there is non-local address associated with it.
  bool newLinkUp = false;

  for (auto iter = mLinks.ConstIter(); !iter.Done(); iter.Next()) {
    LinkInfo* linkInfo = iter.Data();
    nsAutoCString linkName;
    linkInfo->mLink->GetName(linkName);

    if (linkInfo->mIsUp) {
      LOG((" %s is up", linkName.get()));
      newLinkUp = true;
    } else {
      LOG((" %s is down", linkName.get()));
    }
  }

  if (mLinkUp != newLinkUp) {
    RefPtr<NetlinkServiceListener> listener;
    {
      MutexAutoLock lock(mMutex);
      listener = mListener;
      mLinkUp = newLinkUp;
    }
    if (mLinkUp) {
      if (listener) {
        listener->OnLinkUp();
      }
    } else {
      if (listener) {
        listener->OnLinkDown();
      }
    }
  }
}

void NetlinkService::OnAddrMessage(struct nlmsghdr* aNlh) {
  LOG(("NetlinkService::OnAddrMessage [type=%s]",
       aNlh->nlmsg_type == RTM_NEWADDR ? "new" : "del"));

  nsAutoPtr<NetlinkAddress> address(new NetlinkAddress());
  if (!address->Init(aNlh)) {
    return;
  }

  uint32_t ifIdx = address->GetIndex();

  nsAutoCString addrStr;
  GetAddrStr(address->GetAddrPtr(), address->Family(), addrStr);

  LinkInfo* linkInfo = nullptr;
  mLinks.Get(ifIdx, &linkInfo);
  if (!linkInfo) {
    // This can happen during startup
    LOG(("Cannot find link info [ifIdx=%u, addr=%s/%u", ifIdx, addrStr.get(),
         address->GetPrefixLen()));
    return;
  }

  // There might be already an equal address in the array even in case of
  // RTM_NEWADDR message, e.g. when lifetime of IPv6 address is renewed. Remove
  // existing equal address in case of RTM_DELADDR as well as RTM_NEWADDR
  // message and add a new one in the latter case.
  for (uint32_t i = 0; i < linkInfo->mAddresses.Length(); ++i) {
    if (linkInfo->mAddresses[i]->Equals(address)) {
      LOG(("Removing address [ifIdx=%u, addr=%s/%u]", ifIdx, addrStr.get(),
           address->GetPrefixLen()));
      linkInfo->mAddresses.RemoveElementAt(i);
      break;
    }
  }

  if (aNlh->nlmsg_type == RTM_NEWADDR) {
    LOG(("Adding address [ifIdx=%u, addr=%s/%u]", ifIdx, addrStr.get(),
         address->GetPrefixLen()));
    linkInfo->mAddresses.AppendElement(address.forget());
  } else {
    // Remove all routes associated with this address
    for (uint32_t i = linkInfo->mDefaultRoutes.Length(); i-- > 0;) {
      MOZ_ASSERT(linkInfo->mDefaultRoutes[i]->GetGWAddrPtr(),
                 "Stored routes must have gateway!");
      if (linkInfo->mDefaultRoutes[i]->Family() == address->Family() &&
          address->ContainsAddr(linkInfo->mDefaultRoutes[i]->GetGWAddrPtr())) {
#ifdef NL_DEBUG_LOG
        nsAutoCString routeDbgStr;
        linkInfo->mDefaultRoutes[i]->GetAsString(routeDbgStr);
        LOG(("Removing default route: %s", routeDbgStr.get()));
#else
        LOG(("Removing default route"));
#endif
        linkInfo->mDefaultRoutes.RemoveElementAt(i);
      }
    }

    // Remove all neighbors associated with this address
    for (auto iter = linkInfo->mNeighbors.Iter(); !iter.Done(); iter.Next()) {
      NetlinkNeighbor* neigh = iter.Data();
      if (neigh->Family() == address->Family() &&
          address->ContainsAddr(neigh->GetAddrPtr())) {
#ifdef NL_DEBUG_LOG
        nsAutoCString neighDbgStr;
        neigh->GetAsString(neighDbgStr);
        LOG(("Removing neighbor %s", neighDbgStr.get()));
#else
        LOG(("Removing neighbor"));
#endif
        iter.Remove();
      }
    }
  }

  if (linkInfo->UpdateLinkStatus()) {
    UpdateLinkStatus();
    TriggerNetworkIDCalculation();
  } else {
    // Even if the link status hasn't changed, network ID might have changed
    // if it's an address change on a link that's up.
    if (linkInfo->mLink->IsUp()) {
      TriggerNetworkIDCalculation();
    }
  }
}

void NetlinkService::OnRouteMessage(struct nlmsghdr* aNlh) {
  LOG(("NetlinkService::OnRouteMessage [type=%s]",
       aNlh->nlmsg_type == RTM_NEWROUTE ? "new" : "del"));

  nsAutoPtr<NetlinkRoute> route(new NetlinkRoute());
  if (!route->Init(aNlh)) {
    return;
  }

#ifdef NL_DEBUG_LOG
  nsAutoCString routeDbgStr;
  route->GetAsString(routeDbgStr);
#endif

  if (!route->IsUnicast() || !route->ScopeIsUniverse()) {
    // Use only unicast routes
#ifdef NL_DEBUG_LOG
    LOG(("Not an unicast global route: %s", routeDbgStr.get()));
#else
    LOG(("Not an unicast global route"));
#endif
    return;
  }

  // Adding/removing any unicast route might change network ID
  TriggerNetworkIDCalculation();

  if (!route->IsDefault()) {
    // Store only default routes
#ifdef NL_DEBUG_LOG
    LOG(("Not a default route: %s", routeDbgStr.get()));
#else
    LOG(("Not a default route"));
#endif
    return;
  }

  if (!route->HasOif()) {
#ifdef NL_DEBUG_LOG
    LOG(("There is no output interface in route: %s", routeDbgStr.get()));
#else
    LOG(("There is no output interface in the route."));
#endif
    return;
  }

  if (!route->GetGWAddrPtr()) {
    // We won't use the route if there is no gateway, so don't store it
#ifdef NL_DEBUG_LOG
    LOG(("There is no gateway in route: %s", routeDbgStr.get()));
#else
    LOG(("There is no gateway in the route."));
#endif
    return;
  }

  if (route->Family() == AF_INET6 &&
      net::utils::ipv6_scope((const unsigned char*)route->GetGWAddrPtr()) !=
          IPV6_SCOPE_GLOBAL) {
#ifdef NL_DEBUG_LOG
    LOG(("Scope of GW isn't global: %s", routeDbgStr.get()));
#else
    LOG(("Scope of GW isn't global."));
#endif
    return;
  }

  LinkInfo* linkInfo = nullptr;
  mLinks.Get(route->Oif(), &linkInfo);
  if (!linkInfo) {
    // This can happen during startup
#ifdef NL_DEBUG_LOG
    LOG(("Cannot find link info for route: %s", routeDbgStr.get()));
#else
    LOG(("Cannot find link [ifIdx=%u]", route->Oif()));
#endif
    return;
  }

  for (uint32_t i = 0; i < linkInfo->mDefaultRoutes.Length(); ++i) {
    if (linkInfo->mDefaultRoutes[i]->Equals(route)) {
      // We shouldn't find equal route when adding a new one, but just in case
      // it can happen remove the old one to avoid duplicities.
#ifdef NL_DEBUG_LOG
      LOG(("Removing default route: %s", routeDbgStr.get()));
#else
      LOG(("Removing default route"));
#endif
      linkInfo->mDefaultRoutes.RemoveElementAt(i);
      break;
    }
  }

  if (aNlh->nlmsg_type == RTM_NEWROUTE) {
#ifdef NL_DEBUG_LOG
    LOG(("Adding default route: %s", routeDbgStr.get()));
#else
    LOG(("Adding default route"));
#endif
    linkInfo->mDefaultRoutes.AppendElement(route.forget());
  }
}

void NetlinkService::OnNeighborMessage(struct nlmsghdr* aNlh) {
  LOG(("NetlinkService::OnNeighborMessage [type=%s]",
       aNlh->nlmsg_type == RTM_NEWNEIGH ? "new" : "del"));

  nsAutoPtr<NetlinkNeighbor> neigh(new NetlinkNeighbor());
  if (!neigh->Init(aNlh)) {
    return;
  }

#ifdef NL_DEBUG_LOG
  nsAutoCString neighDbgStr;
  neigh->GetAsString(neighDbgStr);
#endif

  nsAutoCString key;
  GetAddrStr(neigh->GetAddrPtr(), neigh->Family(), key);

  LinkInfo* linkInfo = nullptr;
  mLinks.Get(neigh->GetIndex(), &linkInfo);
  if (!linkInfo) {
    // This can happen during startup
#ifdef NL_DEBUG_LOG
    LOG(("Cannot find link info for neighbor: %s", neighDbgStr.get()));
#else
    LOG(("Cannot find link info [ifIdx=%u, key=%s", neigh->GetIndex(),
         key.get()));
#endif
    return;
  }

  if (!linkInfo->mLink->IsTypeEther()) {
#ifdef NL_DEBUG_LOG
    LOG(("Ignoring message on non-ethernet link: %s", neighDbgStr.get()));
#else
    LOG(("Ignoring message on non-ethernet link [ifIdx=%u, key=%s",
         neigh->GetIndex(), key.get()));
#endif
    return;
  }

  if (aNlh->nlmsg_type == RTM_NEWNEIGH) {
    if (!mRecalculateNetworkId && neigh->HasMAC()) {
      NetlinkNeighbor* oldNeigh = nullptr;
      linkInfo->mNeighbors.Get(key, &oldNeigh);

      if (!oldNeigh || !oldNeigh->HasMAC()) {
        // The MAC address was added, if it's a host from some of the saved
        // routing tables we should recalculate network ID
        for (uint32_t i = 0; i < linkInfo->mDefaultRoutes.Length(); ++i) {
          if (linkInfo->mDefaultRoutes[i]->GatewayEquals(neigh)) {
            TriggerNetworkIDCalculation();
            break;
          }
        }
        if ((mIPv4RouteCheckResult &&
             mIPv4RouteCheckResult->GatewayEquals(neigh)) ||
            (mIPv6RouteCheckResult &&
             mIPv6RouteCheckResult->GatewayEquals(neigh))) {
          TriggerNetworkIDCalculation();
        }
      }
    }

#ifdef NL_DEBUG_LOG
    LOG(("Adding neighbor: %s", neighDbgStr.get()));
#else
    LOG(("Adding neighbor %s", key.get()));
#endif
    linkInfo->mNeighbors.Put(key, neigh.forget());
  } else {
#ifdef NL_DEBUG_LOG
    LOG(("Removing neighbor %s", neighDbgStr.get()));
#else
    LOG(("Removing neighbor %s", key.get()));
#endif
    linkInfo->mNeighbors.Remove(key);
  }
}

void NetlinkService::OnRouteCheckResult(struct nlmsghdr* aNlh) {
  LOG(("NetlinkService::OnRouteCheckResult"));
  nsAutoPtr<NetlinkRoute> route;

#ifdef NL_DEBUG_LOG
  nsAutoCString routeDbgStr;
#endif

  if (aNlh) {
    route = new NetlinkRoute();
    if (!route->Init(aNlh)) {
      route = nullptr;
    } else {
#ifdef NL_DEBUG_LOG
      route->GetAsString(routeDbgStr);
#endif

      if (!route->IsUnicast() || !route->ScopeIsUniverse()) {
#ifdef NL_DEBUG_LOG
        LOG(("Not an unicast global route: %s", routeDbgStr.get()));
#else
        LOG(("Not an unicast global route"));
#endif
        route = nullptr;
      } else if (!route->HasOif()) {
#ifdef NL_DEBUG_LOG
        LOG(("There is no output interface in route: %s", routeDbgStr.get()));
#else
        LOG(("There is no output interface in the route."));
#endif
        route = nullptr;
      }
    }
  }

  if (route) {
#ifdef NL_DEBUG_LOG
    LOG(("Storing route: %s", routeDbgStr.get()));
#else
    LOG(("Storing result for the check"));
#endif
  } else {
    LOG(("Clearing result for the check"));
  }

  if (mOutgoingMessages[0]->Family() == AF_INET) {
    mIPv4RouteCheckResult = route.forget();
  } else {
    mIPv6RouteCheckResult = route.forget();
  }
}

void NetlinkService::EnqueueGenMsg(uint16_t aMsgType, uint8_t aFamily) {
  NetlinkGenMsg* msg = new NetlinkGenMsg(aMsgType, aFamily, ++mMsgId);
  mOutgoingMessages.AppendElement(msg);
}

void NetlinkService::EnqueueRtMsg(uint8_t aFamily, void* aAddress) {
  NetlinkRtMsg* msg = new NetlinkRtMsg(aFamily, aAddress, ++mMsgId);
  mOutgoingMessages.AppendElement(msg);
}

void NetlinkService::RemovePendingMsg() {
  LOG(("NetlinkService::RemovePendingMsg [seqId=%u]",
       mOutgoingMessages[0]->SeqId()));

  MOZ_ASSERT(mOutgoingMessages[0]->IsPending());

  DebugOnly<bool> isRtMessage =
      (mOutgoingMessages[0]->MsgType() == NetlinkMsg::kRtMsg);

  mOutgoingMessages.RemoveElementAt(0);
  if (!mOutgoingMessages.Length()) {
    if (!mInitialScanFinished) {
      // Now we've received all initial data from the kernel. Perform a link
      // check and trigger network ID calculation even if it wasn't triggered
      // by the incoming messages.
      mInitialScanFinished = true;

      UpdateLinkStatus();
      TriggerNetworkIDCalculation();

      // Link status should be known by now.
      RefPtr<NetlinkServiceListener> listener;
      {
        MutexAutoLock lock(mMutex);
        listener = mListener;
      }
      if (listener) {
        listener->OnLinkStatusKnown();
      }
    } else {
      // We've received last response for route check, calculate ID now
      MOZ_ASSERT(isRtMessage);
      CalculateNetworkID();
    }
  }
}

NS_IMETHODIMP
NetlinkService::Run() {
  int netlinkSocket = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  if (netlinkSocket < 0) {
    return NS_ERROR_FAILURE;
  }

  struct sockaddr_nl addr;
  memset(&addr, 0, sizeof(addr));

  addr.nl_family = AF_NETLINK;
  addr.nl_groups = RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR | RTMGRP_LINK |
                   RTMGRP_NEIGH | RTMGRP_IPV4_ROUTE | RTMGRP_IPV6_ROUTE;

  if (bind(netlinkSocket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    // failure!
    EINTR_RETRY(close(netlinkSocket));
    return NS_ERROR_FAILURE;
  }

  struct pollfd fds[2];
  fds[0].fd = mShutdownPipe[0];
  fds[0].events = POLLIN;
  fds[0].revents = 0;

  fds[1].fd = netlinkSocket;
  fds[1].events = POLLIN;
  fds[1].revents = 0;

  // send all requests to get initial network information
  EnqueueGenMsg(RTM_GETLINK, AF_PACKET);
  EnqueueGenMsg(RTM_GETNEIGH, AF_INET);
  EnqueueGenMsg(RTM_GETNEIGH, AF_INET6);
  EnqueueGenMsg(RTM_GETADDR, AF_PACKET);
  EnqueueGenMsg(RTM_GETROUTE, AF_PACKET);

  nsresult rv = NS_OK;
  bool shutdown = false;
  while (!shutdown) {
    if (mOutgoingMessages.Length() && !mOutgoingMessages[0]->IsPending()) {
      if (!mOutgoingMessages[0]->Send(netlinkSocket)) {
        LOG(("Failed to send netlink message"));
        mOutgoingMessages.RemoveElementAt(0);
        // try to send another message if available before polling
        continue;
      }
    }

    int rc = EINTR_RETRY(poll(fds, 2, GetPollWait()));

    if (rc > 0) {
      if (fds[0].revents & POLLIN) {
        // shutdown, abort the loop!
        LOG(("thread shutdown received, dying...\n"));
        shutdown = true;
      } else if (fds[1].revents & POLLIN) {
        LOG(("netlink message received, handling it...\n"));
        OnNetlinkMessage(netlinkSocket);
      }
    } else if (rc < 0) {
      rv = NS_ERROR_FAILURE;
      break;
    }
  }

  EINTR_RETRY(close(netlinkSocket));

  return rv;
}

nsresult NetlinkService::Init(NetlinkServiceListener* aListener) {
  nsresult rv;

  mListener = aListener;

  nsAutoCString routecheckIP;

  rv =
      Preferences::GetCString("network.netlink.route.check.IPv4", routecheckIP);
  if (NS_SUCCEEDED(rv)) {
    if (inet_pton(AF_INET, routecheckIP.get(), &mRouteCheckIPv4) == 1) {
      mDoRouteCheckIPv4 = true;
    }
  }

  rv =
      Preferences::GetCString("network.netlink.route.check.IPv6", routecheckIP);
  if (NS_SUCCEEDED(rv)) {
    if (inet_pton(AF_INET6, routecheckIP.get(), &mRouteCheckIPv6) == 1) {
      mDoRouteCheckIPv6 = true;
    }
  }

  if (pipe(mShutdownPipe) == -1) {
    return NS_ERROR_FAILURE;
  }

  rv = NS_NewNamedThread("Netlink Monitor", getter_AddRefs(mThread), this);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult NetlinkService::Shutdown() {
  LOG(("write() to signal thread shutdown\n"));

  {
    MutexAutoLock lock(mMutex);
    mListener = nullptr;
  }

  // awake the thread to make it terminate
  ssize_t rc = EINTR_RETRY(write(mShutdownPipe[1], "1", 1));
  LOG(("write() returned %d, errno == %d\n", (int)rc, errno));

  nsresult rv = mThread->Shutdown();

  // Have to break the cycle here, otherwise NetlinkService holds
  // onto the thread and the thread holds onto the NetlinkService
  // via its mRunnable
  mThread = nullptr;

  return rv;
}

/*
 * A network event that might change network ID has been registered. Delay
 * network ID calculation and sending of the event in case it changed for
 * a while. Absorbing potential subsequent events increases chance of successful
 * network ID calculation (e.g. MAC address of the router might be discovered in
 * the meantime)
 */
void NetlinkService::TriggerNetworkIDCalculation() {
  LOG(("NetlinkService::TriggerNetworkIDCalculation"));

  if (mRecalculateNetworkId) {
    return;
  }

  mRecalculateNetworkId = true;
  mTriggerTime = TimeStamp::Now();
}

int NetlinkService::GetPollWait() {
  if (!mRecalculateNetworkId) {
    return -1;
  }

  if (mOutgoingMessages.Length()) {
    MOZ_ASSERT(mOutgoingMessages[0]->IsPending());
    // Message is pending, we don't have to set timeout because we'll receive
    // reply from kernel ASAP
    return -1;
  }

  MOZ_ASSERT(mInitialScanFinished);

  double period = (TimeStamp::Now() - mTriggerTime).ToMilliseconds();
  if (period >= kNetworkChangeCoalescingPeriod) {
    // Coalescing time has elapsed, do route check
    if (!mDoRouteCheckIPv4 && !mDoRouteCheckIPv6) {
      // If route checking is disabled for whatever reason, calculate ID now
      CalculateNetworkID();
      return -1;
    }

    // Otherwise send route check messages and calculate network ID after the
    // response is received
    if (mDoRouteCheckIPv4) {
      EnqueueRtMsg(AF_INET, &mRouteCheckIPv4);
    }
    if (mDoRouteCheckIPv6) {
      EnqueueRtMsg(AF_INET6, &mRouteCheckIPv6);
    }

    // Return 0 to make sure we start sending enqueued messages immediately
    return 0;
  }

  return static_cast<int>(kNetworkChangeCoalescingPeriod - period);
}

class NeighborComparator {
 public:
  bool Equals(const NetlinkNeighbor* a, const NetlinkNeighbor* b) const {
    return (memcmp(a->GetMACPtr(), b->GetMACPtr(), ETH_ALEN) == 0);
  }
  bool LessThan(const NetlinkNeighbor* a, const NetlinkNeighbor* b) const {
    return (memcmp(a->GetMACPtr(), b->GetMACPtr(), ETH_ALEN) < 0);
  }
};

class LinknameComparator {
 public:
  bool LessThan(const nsCString& aA, const nsCString& aB) const {
    return aA < aB;
  }
  bool Equals(const nsCString& aA, const nsCString& aB) const {
    return aA == aB;
  }
};

bool NetlinkService::CalculateIDForFamily(uint8_t aFamily, SHA1Sum* aSHA1) {
  LOG(("NetlinkService::CalculateIDForFamily [family=%s]",
       aFamily == AF_INET ? "AF_INET" : "AF_INET6"));

  bool retval = false;

  if (!mLinkUp) {
    // Skip ID calculation if the link is down, we have no ID...
    LOG(("Link is down, skipping ID calculation."));
    return retval;
  }

  nsAutoPtr<NetlinkRoute>* routeCheckResultPtr;
  if (aFamily == AF_INET) {
    routeCheckResultPtr = &mIPv4RouteCheckResult;
  } else {
    routeCheckResultPtr = &mIPv6RouteCheckResult;
  }

  // All GW neighbors for which we know MAC address. We'll probably have at
  // most only one, but in case we have more default routes, we hash them all
  // even though the routing rules sends the traffic only via one of them.
  // If the system switches between them, we'll detect the change with
  // mIPv4/6RouteCheckResult.
  nsTArray<NetlinkNeighbor*> gwNeighbors;

  // Check only routes on links that are up
  for (auto iter = mLinks.ConstIter(); !iter.Done(); iter.Next()) {
    LinkInfo* linkInfo = iter.Data();
    nsAutoCString linkName;
    linkInfo->mLink->GetName(linkName);

    if (!linkInfo->mIsUp) {
      LOG((" %s is down", linkName.get()));
      continue;
    }

    if (!linkInfo->mLink->IsTypeEther()) {
      LOG((" %s is not ethernet link", linkName.get()));
      continue;
    }

    LOG((" checking link %s", linkName.get()));

    // Check all default routes and try to get MAC of the gateway
    for (uint32_t i = 0; i < linkInfo->mDefaultRoutes.Length(); ++i) {
#ifdef NL_DEBUG_LOG
      nsAutoCString routeDbgStr;
      linkInfo->mDefaultRoutes[i]->GetAsString(routeDbgStr);
      LOG(("Checking default route: %s", routeDbgStr.get()));
#endif

      if (linkInfo->mDefaultRoutes[i]->Family() != aFamily) {
#ifdef NL_DEBUG_LOG
        LOG(("  skipping due to different family"));
#endif
        continue;
      }

      MOZ_ASSERT(linkInfo->mDefaultRoutes[i]->GetGWAddrPtr(),
                 "Stored routes must have gateway!");

      nsAutoCString neighKey;
      GetAddrStr(linkInfo->mDefaultRoutes[i]->GetGWAddrPtr(), aFamily,
                 neighKey);

      NetlinkNeighbor* neigh = nullptr;
      if (!linkInfo->mNeighbors.Get(neighKey, &neigh)) {
        LOG(("Neighbor %s not found in hashtable.", neighKey.get()));
        continue;
      }

      if (!neigh->HasMAC()) {
        // We don't know MAC address
        LOG(("We have no MAC for neighbor %s.", neighKey.get()));
        continue;
      }

      if (gwNeighbors.IndexOf(neigh, 0, NeighborComparator()) !=
          nsTArray<NetlinkNeighbor*>::NoIndex) {
        // avoid host duplicities
        LOG(("MAC of neighbor %s is already selected for hashing.",
             neighKey.get()));
        continue;
      }

      LOG(("MAC of neighbor %s will be used for network ID.", neighKey.get()));
      gwNeighbors.AppendElement(neigh);
    }
  }

  // Sort them so we always have the same network ID on the same network
  gwNeighbors.Sort(NeighborComparator());

  for (uint32_t i = 0; i < gwNeighbors.Length(); ++i) {
#ifdef NL_DEBUG_LOG
    nsAutoCString neighDbgStr;
    gwNeighbors[i]->GetAsString(neighDbgStr);
    LOG(("Hashing MAC address of neighbor: %s", neighDbgStr.get()));
#endif
    aSHA1->update(gwNeighbors[i]->GetMACPtr(), ETH_ALEN);
    retval = true;
  }

  nsTArray<nsCString> linkNamesToHash;
  if (!gwNeighbors.Length()) {
    // If we don't know MAC of the gateway and link is up, it's probably not
    // an ethernet link. If the name of the link begins with "rmnet_data" then
    // the mobile data is used. We cannot easily differentiate when user
    // switches sim cards so let's treat mobile data as a single network. We'll
    // simply hash link name. If the traffic is redirected via some VPN, it'll
    // still be detected below.

    // TODO: maybe we could get operator name via AndroidBridge
    for (auto iter = mLinks.ConstIter(); !iter.Done(); iter.Next()) {
      LinkInfo* linkInfo = iter.Data();
      if (linkInfo->mIsUp) {
        nsAutoCString linkName;
        linkInfo->mLink->GetName(linkName);
        if (StringBeginsWith(linkName, NS_LITERAL_CSTRING("rmnet_data"))) {
          // Check whether there is some non-local address associated with this
          // link.
          for (uint32_t i = 0; i < linkInfo->mAddresses.Length(); ++i) {
            if (linkInfo->mAddresses[i]->Family() == aFamily &&
                linkInfo->mAddresses[i]->ScopeIsUniverse()) {
              linkNamesToHash.AppendElement(linkName);
              break;
            }
          }
        }
      }
    }

    // Sort link names to ensure consistent results
    linkNamesToHash.Sort(LinknameComparator());

    for (uint32_t i = 0; i < linkNamesToHash.Length(); ++i) {
      LOG(("Hashing name of adapter: %s", linkNamesToHash[i].get()));
      aSHA1->update(linkNamesToHash[i].get(), linkNamesToHash[i].Length());
      retval = true;
    }
  }

  if (!*routeCheckResultPtr) {
    // If we don't have result for route check to mRouteCheckIPv4/6 host. The
    // network is either unreachable or the checking was disabled by removing IP
    // from the preferences. In any case, there is no more to do.
    LOG(("There is no route check result."));
    return retval;
  }

  nsAutoCString routeCheckLinkName;
  LinkInfo* routeCheckLinkInfo = nullptr;
  uint32_t routeCheckIfIdx = (*routeCheckResultPtr)->Oif();
  if (!mLinks.Get(routeCheckIfIdx, &routeCheckLinkInfo)) {
    LOG(("Cannot find link with index %u ??", routeCheckIfIdx));
    return retval;
  }
  routeCheckLinkInfo->mLink->GetName(routeCheckLinkName);
  const in_common_addr* addrPtr = (*routeCheckResultPtr)->GetGWAddrPtr();

  if (routeCheckLinkInfo->mLink->IsTypeEther()) {
    // The traffic is routed through an ethernet device.

    if (!addrPtr) {
      // This shouldn't normally happen, missing next hop in case of ethernet
      // device would mean that the checked host is on the same network.
#ifdef NL_DEBUG_LOG
      nsAutoCString routeDbgStr;
      (*routeCheckResultPtr)->GetAsString(routeDbgStr);
      LOG(("There is no next hop in route: %s", routeDbgStr.get()));
#else
      LOG(("There is no next hop in route through an ethernet device %s",
           routeCheckLinkName.get()));
#endif
      return retval;
    }

    // If we know MAC address of the next hop for mRouteCheckIPv4/6 host, hash
    // it even if it's MAC of some of the default routes we've checked above.
    // This ensures that if we have 2 different default routes and next hop for
    // mRouteCheckIPv4/6 changes from one default route to the other, we'll
    // detect it as a network change.
    nsAutoCString neighKey;
    GetAddrStr(addrPtr, aFamily, neighKey);
    LOG(("Next hop for the checked host is %s on ifIdx %u.", neighKey.get(),
         routeCheckIfIdx));

    NetlinkNeighbor* neigh = nullptr;
    if (!routeCheckLinkInfo->mNeighbors.Get(neighKey, &neigh)) {
      LOG(("Neighbor %s not found in hashtable.", neighKey.get()));
      return retval;
    }

    if (!neigh->HasMAC()) {
      LOG(("We have no MAC for neighbor %s.", neighKey.get()));
      return retval;
    }

#ifdef NL_DEBUG_LOG
    nsAutoCString neighDbgStr;
    neigh->GetAsString(neighDbgStr);
    LOG(("Hashing MAC address of neighbor: %s", neighDbgStr.get()));
#else
    LOG(("Hashing MAC address of neighbor %s", neighKey.get()));
#endif
    aSHA1->update(neigh->GetMACPtr(), ETH_ALEN);
    retval = true;
  } else {
    // The traffic is routed through a non-ethernet device.
    if (addrPtr) {
      // The route contains next hop. Hash the name of the interface (e.g.
      // "tun1") and the IP address of the next hop.

      nsAutoCString addrStr;
      GetAddrStr(addrPtr, aFamily, addrStr);
      size_t addrSize = (aFamily == AF_INET) ? sizeof(addrPtr->addr4)
                                             : sizeof(addrPtr->addr6);

      LOG(("Hashing link name %s", routeCheckLinkName.get()));
      aSHA1->update(routeCheckLinkName.get(), routeCheckLinkName.Length());

      // Don't hash GW address if it's rmnet_data device.
      if (!linkNamesToHash.Contains(routeCheckLinkName)) {
        LOG(("Hashing GW address %s", addrStr.get()));
        aSHA1->update(addrPtr, addrSize);
      }

      retval = true;
    } else {
      // The traffic is routed directly via an interface. Hash the name of the
      // interface and the network address. Using host address would cause that
      // network ID would be different every time we get a different IP address
      // in this network/VPN.

      bool hasSrcAddr = (*routeCheckResultPtr)->HasPrefSrcAddr();
      if (!hasSrcAddr) {
        LOG(("There is no preferred source address."));
      }

      NetlinkAddress* linkAddress = nullptr;
      // Find network address of the interface matching the source address. In
      // theory there could be multiple addresses with different prefix length.
      // Get the one with smallest prefix length.
      for (uint32_t i = 0; i < routeCheckLinkInfo->mAddresses.Length(); ++i) {
        if (!hasSrcAddr) {
          // there is no preferred src, match just the family
          if (routeCheckLinkInfo->mAddresses[i]->Family() != aFamily) {
            continue;
          }
        } else if (!(*routeCheckResultPtr)
                        ->PrefSrcAddrEquals(
                            routeCheckLinkInfo->mAddresses[i])) {
          continue;
        }

        if (!linkAddress ||
            linkAddress->GetPrefixLen() >
                routeCheckLinkInfo->mAddresses[i]->GetPrefixLen()) {
          // We have no address yet or this one has smaller prefix length,
          // use it.
          linkAddress = routeCheckLinkInfo->mAddresses[i];
        }
      }

      if (!linkAddress) {
        // There is no address in our array?
        nsAutoCString dbgStr;
#ifdef NL_DEBUG_LOG
        (*routeCheckResultPtr)->GetAsString(dbgStr);
        LOG(("No address found for preferred source address in route: %s",
             dbgStr.get()));
#else
        GetAddrStr((*routeCheckResultPtr)->GetPrefSrcAddrPtr(), aFamily,
                   dbgStr);
        LOG(("No address found for preferred source address %s", dbgStr.get()));
#endif
        return retval;
      }

      in_common_addr prefix;
      int32_t prefixSize = (aFamily == AF_INET) ? (int32_t)sizeof(prefix.addr4)
                                                : (int32_t)sizeof(prefix.addr6);
      memcpy(&prefix, linkAddress->GetAddrPtr(), prefixSize);
      uint8_t maskit[] = {0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe};
      int32_t bits = linkAddress->GetPrefixLen();
      if (bits > prefixSize * 8) {
        MOZ_ASSERT(false, "Unexpected prefix length!");
        LOG(("Unexpected prefix length %d, maximum for this family is %d", bits,
             prefixSize * 8));
        return retval;
      }
      for (int32_t i = 0; i < prefixSize; i++) {
        uint8_t mask = (bits >= 8) ? 0xff : maskit[bits];
        ((unsigned char*)&prefix)[i] &= mask;
        bits -= 8;
        if (bits <= 0) {
          bits = 0;
        }
      }

      nsAutoCString addrStr;
      GetAddrStr(&prefix, aFamily, addrStr);
      LOG(("Hashing link name %s and network address %s/%u",
           routeCheckLinkName.get(), addrStr.get(),
           linkAddress->GetPrefixLen()));
      aSHA1->update(routeCheckLinkName.get(), routeCheckLinkName.Length());
      aSHA1->update(&prefix, prefixSize);
      bits = linkAddress->GetPrefixLen();
      aSHA1->update(&bits, sizeof(bits));
      retval = true;
    }
  }

  return retval;
}

void NetlinkService::ComputeDNSSuffixList() {
  MOZ_ASSERT(!NS_IsMainThread(), "Must not be called on the main thread");
#if defined(HAVE_RES_NINIT)
  nsTArray<nsCString> suffixList;
  struct __res_state res;
  if (res_ninit(&res) == 0) {
    for (int i = 0; i < MAXDNSRCH; i++) {
      if (!res.dnsrch[i]) {
        break;
      }
      suffixList.AppendElement(nsCString(res.dnsrch[i]));
    }
    res_nclose(&res);
  }

  MutexAutoLock lock(mMutex);
  mDNSSuffixList = std::move(suffixList);
#endif
}

// Figure out the "network identification".
void NetlinkService::CalculateNetworkID() {
  LOG(("NetlinkService::CalculateNetworkID"));

  MOZ_ASSERT(!NS_IsMainThread(), "Must not be called on the main thread");
  MOZ_ASSERT(mRecalculateNetworkId);

  mRecalculateNetworkId = false;

  SHA1Sum sha1;

  UpdateLinkStatus();
  ComputeDNSSuffixList();

  bool idChanged = false;
  bool found4 = CalculateIDForFamily(AF_INET, &sha1);
  bool found6 = CalculateIDForFamily(AF_INET6, &sha1);

  if (found4 || found6) {
    // This 'addition' could potentially be a fixed number from the
    // profile or something.
    nsAutoCString addition("local-rubbish");
    nsAutoCString output;
    sha1.update(addition.get(), addition.Length());
    uint8_t digest[SHA1Sum::kHashSize];
    sha1.finish(digest);
    nsAutoCString newString(reinterpret_cast<char*>(digest),
                            SHA1Sum::kHashSize);
    nsresult rv = Base64Encode(newString, output);
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
    LOG(("networkid: id %s\n", output.get()));
    MutexAutoLock lock(mMutex);
    if (mNetworkId != output) {
      // new id
      if (found4 && !found6) {
        Telemetry::Accumulate(Telemetry::NETWORK_ID2, 1);  // IPv4 only
      } else if (!found4 && found6) {
        Telemetry::Accumulate(Telemetry::NETWORK_ID2, 3);  // IPv6 only
      } else {
        Telemetry::Accumulate(Telemetry::NETWORK_ID2, 4);  // Both!
      }
      mNetworkId = output;
      idChanged = true;
    } else {
      // same id
      LOG(("Same network id"));
      Telemetry::Accumulate(Telemetry::NETWORK_ID2, 2);
    }
  } else {
    // no id
    LOG(("No network id"));
    MutexAutoLock lock(mMutex);
    if (!mNetworkId.IsEmpty()) {
      mNetworkId.Truncate();
      idChanged = true;
      Telemetry::Accumulate(Telemetry::NETWORK_ID2, 0);
    }
  }

  // If this is first time we calculate network ID, don't report it as a network
  // change. We've started with an empty ID and we've just calculated the
  // correct ID. The network hasn't really changed.
  static bool initialIDCalculation = true;

  if (idChanged && !initialIDCalculation) {
    RefPtr<NetlinkServiceListener> listener;
    {
      MutexAutoLock lock(mMutex);
      listener = mListener;
    }
    if (listener) {
      listener->OnNetworkChanged();
    }
  }

  initialIDCalculation = false;
}

void NetlinkService::GetNetworkID(nsACString& aNetworkID) {
  MutexAutoLock lock(mMutex);
  aNetworkID = mNetworkId;
}

nsresult NetlinkService::GetDnsSuffixList(nsTArray<nsCString>& aDnsSuffixList) {
#if defined(HAVE_RES_NINIT)
  MutexAutoLock lock(mMutex);
  aDnsSuffixList = mDNSSuffixList;
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

void NetlinkService::GetIsLinkUp(bool* aIsUp) {
  MutexAutoLock lock(mMutex);
  *aIsUp = mLinkUp;
}

}  // namespace net
}  // namespace mozilla
