/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestCommon.h"
#include "gtest/gtest.h"
#include "mozilla/gtest/MozAssertions.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/Preferences.h"
#include "mozilla/net/DNS.h"

TEST(TestNetAddrLNAUtil, IPAddressSpaceCategorization)
{
  /*--------------------------------------------------------------------------*
 | Network              | Description            | RFC       | Scope   |
 |----------------------|------------------------|-----------|------------------|
 127.0.0.0/8            | IPv4 Loopback          | RFC1122   | local   |
 10.0.0.0/8             | Private Use            | RFC1918   | private |
 100.64.0.0/10          | Carrier-Grade NAT      | RFC6598   | private |
 172.16.0.0/12          | Private Use            | RFC1918   | private |
 192.168.0.0/16         | Private Use            | RFC1918   | private |
 198.18.0.0/15          | Benchmarking           | RFC2544   | local   |
 169.254.0.0/16         | Link Local             | RFC3927   | private |
 ::1/128                | IPv6 Loopback          | RFC4291   | local   |
 fc00::/7               | Unique Local           | RFC4193   | private |
 fe80::/10              | Link-Local Unicast     | RFC4291   | private |
 ::ffff:0:0/96          | IPv4-mapped            | RFC4291   | IPv4-mapped
 address space |
 *--------------------------------------------------------------------------*/
  using namespace mozilla::net;

  struct TestCase {
    const char* mIp;
    nsILoadInfo::IPAddressSpace mExpectedSpace;
  };

  std::vector<TestCase> testCases = {
      // Local IPv4
      {"127.0.0.1", nsILoadInfo::IPAddressSpace::Local},
      {"198.18.0.0", nsILoadInfo::IPAddressSpace::Local},
      {"198.19.255.255", nsILoadInfo::IPAddressSpace::Local},

      // Private IPv4
      {"10.0.0.1", nsILoadInfo::IPAddressSpace::Private},
      {"100.64.0.1", nsILoadInfo::IPAddressSpace::Private},
      {"100.127.255.254", nsILoadInfo::IPAddressSpace::Private},
      {"172.16.0.1", nsILoadInfo::IPAddressSpace::Private},
      {"172.31.255.255", nsILoadInfo::IPAddressSpace::Private},
      {"192.168.1.1", nsILoadInfo::IPAddressSpace::Private},
      {"169.254.0.1", nsILoadInfo::IPAddressSpace::Private},
      {"169.254.255.254", nsILoadInfo::IPAddressSpace::Private},

      // IPv6 Local and Private
      {"::1", nsILoadInfo::IPAddressSpace::Local},       // Loopback
      {"fc00::", nsILoadInfo::IPAddressSpace::Private},  // Unique Local
      {"fdff:ffff:ffff:ffff:ffff:ffff:ffff:ffff",
       nsILoadInfo::IPAddressSpace::Private},
      {"fe80::1", nsILoadInfo::IPAddressSpace::Private},  // Link-local

      // IPv4-mapped IPv6 (mapped IPv4, should fall back to IPv4 classification)
      {"::ffff:127.0.0.1", nsILoadInfo::IPAddressSpace::Local},   // Loopback
      {"::ffff:10.0.0.1", nsILoadInfo::IPAddressSpace::Private},  // Private
      {"::ffff:1.1.1.1", nsILoadInfo::IPAddressSpace::Public},    // Public

      // Public IPv4
      {"8.8.8.8", nsILoadInfo::IPAddressSpace::Public},
      {"1.1.1.1", nsILoadInfo::IPAddressSpace::Public},

      // Public IPv6
      {"2001:4860:4860::8888", nsILoadInfo::IPAddressSpace::Public},
      {"2606:4700:4700::1111", nsILoadInfo::IPAddressSpace::Public}};

  for (const auto& testCase : testCases) {
    NetAddr addr;
    addr.InitFromString(nsCString(testCase.mIp));
    if (addr.raw.family == AF_INET) {
      EXPECT_EQ(addr.GetIpAddressSpace(), testCase.mExpectedSpace)
          << "Failed for IP: " << testCase.mIp;
    } else if (addr.GetIpAddressSpace() == AF_INET6) {
      EXPECT_EQ(addr.GetIpAddressSpace(), testCase.mExpectedSpace)
          << "Failed for IP: " << testCase.mIp;
    }
  }
}

TEST(TestNetAddrLNAUtil, DefaultAndOverrideTransitions)
{
  using mozilla::Preferences;
  using mozilla::net::NetAddr;
  using IPAddressSpace = nsILoadInfo::IPAddressSpace;
  struct TestCase {
    const char* ip;
    uint16_t port;
    IPAddressSpace defaultSpace;
    IPAddressSpace overrideSpace;
    const char* prefName;
  };

  std::vector<TestCase> testCases = {
      // Public -> Private
      {"8.8.8.8", 80, IPAddressSpace::Public, IPAddressSpace::Private,
       "network.lna.address_space.private.override"},

      // Public -> Local
      {"8.8.4.4", 53, IPAddressSpace::Public, IPAddressSpace::Local,
       "network.lna.address_space.local.override"},

      // Private -> Public
      {"192.168.0.1", 8080, IPAddressSpace::Private, IPAddressSpace::Public,
       "network.lna.address_space.public.override"},

      // Private -> Local
      {"10.0.0.1", 1234, IPAddressSpace::Private, IPAddressSpace::Local,
       "network.lna.address_space.local.override"},

      // Local -> Public
      {"127.0.0.1", 4444, IPAddressSpace::Local, IPAddressSpace::Public,
       "network.lna.address_space.public.override"},

      // Local -> Private
      {"198.18.0.1", 9999, IPAddressSpace::Local, IPAddressSpace::Private,
       "network.lna.address_space.private.override"},
  };

  for (const auto& tc : testCases) {
    NetAddr addr;
    addr.InitFromString(nsCString(tc.ip), tc.port);
    ASSERT_EQ(addr.GetIpAddressSpace(), tc.defaultSpace)
        << "Expected default space for " << tc.ip << ":" << tc.port;

    std::string overrideStr =
        std::string(tc.ip) + ":" + std::to_string(tc.port);
    Preferences::SetCString(tc.prefName, overrideStr.c_str());

    NetAddr overriddenAddr;
    overriddenAddr.InitFromString(nsCString(tc.ip), tc.port);
    ASSERT_EQ(overriddenAddr.GetIpAddressSpace(), tc.overrideSpace)
        << "Expected override to " << tc.overrideSpace << " for "
        << overrideStr;

    // Reset preference and confirm classification returns to default
    Preferences::SetCString(tc.prefName, ""_ns);
    NetAddr resetAddr;
    resetAddr.InitFromString(nsCString(tc.ip), tc.port);
    ASSERT_EQ(resetAddr.GetIpAddressSpace(), tc.defaultSpace)
        << "Expected reset back to default space for " << tc.ip;
  }
}
