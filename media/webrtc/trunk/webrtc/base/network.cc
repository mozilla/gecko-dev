/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "webrtc/base/network.h"

#if defined(WEBRTC_POSIX)
// linux/if.h can't be included at the same time as the posix sys/if.h, and
// it's transitively required by linux/route.h, so include that version on
// linux instead of the standard posix one.
#if defined(WEBRTC_LINUX)
#include <linux/if.h>
#include <linux/route.h>
#elif !defined(__native_client__)
#include <net/if.h>
#endif
#include <sys/socket.h>
#include <sys/utsname.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>

#if defined(WEBRTC_ANDROID)
#include "webrtc/base/ifaddrs-android.h"
#elif !defined(__native_client__)
#include <ifaddrs.h>
#endif

#endif  // WEBRTC_POSIX

#if defined(WEBRTC_WIN)
#include "webrtc/base/win32.h"
#include <Iphlpapi.h>
#endif

#include <stdio.h>

#include <algorithm>

#include "webrtc/base/logging.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/socket.h"  // includes something that makes windows happy
#include "webrtc/base/stream.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/base/thread.h"

namespace rtc {
namespace {

// Turning on IPv6 could make many IPv6 interfaces available for connectivity
// check and delay the call setup time. kMaxIPv6Networks is the default upper
// limit of IPv6 networks but could be changed by set_max_ipv6_networks().
const int kMaxIPv6Networks = 5;

const uint32 kUpdateNetworksMessage = 1;
const uint32 kSignalNetworksMessage = 2;

// Fetch list of networks every two seconds.
const int kNetworksUpdateIntervalMs = 2000;

const int kHighestNetworkPreference = 127;

typedef struct {
  Network* net;
  std::vector<InterfaceAddress> ips;
} AddressList;

bool CompareNetworks(const Network* a, const Network* b) {
  if (a->prefix_length() == b->prefix_length()) {
    if (a->name() == b->name()) {
      return a->prefix() < b->prefix();
    }
  }
  return a->name() < b->name();
}

bool SortNetworks(const Network* a, const Network* b) {
  // Network types will be preferred above everything else while sorting
  // Networks.

  // Networks are sorted first by type.
  if (a->type() != b->type()) {
    return a->type() < b->type();
  }

  IPAddress ip_a = a->GetBestIP();
  IPAddress ip_b = b->GetBestIP();

  // After type, networks are sorted by IP address precedence values
  // from RFC 3484-bis
  if (IPAddressPrecedence(ip_a) != IPAddressPrecedence(ip_b)) {
    return IPAddressPrecedence(ip_a) > IPAddressPrecedence(ip_b);
  }

  // TODO(mallinath) - Add VPN and Link speed conditions while sorting.

  // Networks are sorted last by key.
  return a->key() > b->key();
}

std::string AdapterTypeToString(AdapterType type) {
  switch (type) {
    case ADAPTER_TYPE_UNKNOWN:
      return "Unknown";
    case ADAPTER_TYPE_ETHERNET:
      return "Ethernet";
    case ADAPTER_TYPE_WIFI:
      return "Wifi";
    case ADAPTER_TYPE_CELLULAR:
      return "Cellular";
    case ADAPTER_TYPE_VPN:
      return "VPN";
    case ADAPTER_TYPE_LOOPBACK:
      return "Loopback";
    default:
      DCHECK(false) << "Invalid type " << type;
      return std::string();
  }
}

bool IsIgnoredIPv6(const IPAddress& ip) {
  if (ip.family() != AF_INET6) {
    return false;
  }

  // Link-local addresses require scope id to be bound successfully.
  // However, our IPAddress structure doesn't carry that so the
  // information is lost and causes binding failure.
  if (IPIsLinkLocal(ip)) {
    return true;
  }

  // Any MAC based IPv6 should be avoided to prevent the MAC tracking.
  if (IPIsMacBased(ip)) {
    return true;
  }

  return false;
}

}  // namespace

std::string MakeNetworkKey(const std::string& name, const IPAddress& prefix,
                           int prefix_length) {
  std::ostringstream ost;
  ost << name << "%" << prefix.ToString() << "/" << prefix_length;
  return ost.str();
}

NetworkManager::NetworkManager() {
}

NetworkManager::~NetworkManager() {
}

NetworkManagerBase::NetworkManagerBase()
    : max_ipv6_networks_(kMaxIPv6Networks), ipv6_enabled_(true) {
}

NetworkManagerBase::~NetworkManagerBase() {
  for (const auto& kv : networks_map_) {
    delete kv.second;
  }
}

void NetworkManagerBase::GetAnyAddressNetworks(NetworkList* networks) {
  if (!ipv4_any_address_network_) {
    const rtc::IPAddress ipv4_any_address(INADDR_ANY);
    ipv4_any_address_network_.reset(
        new rtc::Network("any", "any", ipv4_any_address, 0));
    ipv4_any_address_network_->AddIP(ipv4_any_address);
  }
  networks->push_back(ipv4_any_address_network_.get());

  if (ipv6_enabled()) {
    if (!ipv6_any_address_network_) {
      const rtc::IPAddress ipv6_any_address(in6addr_any);
      ipv6_any_address_network_.reset(
          new rtc::Network("any", "any", ipv6_any_address, 0));
      ipv6_any_address_network_->AddIP(ipv6_any_address);
    }
    networks->push_back(ipv6_any_address_network_.get());
  }
}

void NetworkManagerBase::GetNetworks(NetworkList* result) const {
  int ipv6_networks = 0;
  result->clear();
  for (Network* network : networks_) {
    // Keep the number of IPv6 networks under |max_ipv6_networks_|.
    if (network->prefix().family() == AF_INET6) {
      if (ipv6_networks >= max_ipv6_networks_) {
        continue;
      }
      ++ipv6_networks;
    }
    result->push_back(network);
  }
}

void NetworkManagerBase::MergeNetworkList(const NetworkList& new_networks,
                                          bool* changed) {
  NetworkManager::Stats stats;
  MergeNetworkList(new_networks, changed, &stats);
}

void NetworkManagerBase::MergeNetworkList(const NetworkList& new_networks,
                                          bool* changed,
                                          NetworkManager::Stats* stats) {
  // AddressList in this map will track IP addresses for all Networks
  // with the same key.
  std::map<std::string, AddressList> consolidated_address_list;
  NetworkList list(new_networks);

  // Result of Network merge. Element in this list should have unique key.
  NetworkList merged_list;
  std::sort(list.begin(), list.end(), CompareNetworks);

  *changed = false;

  if (networks_.size() != list.size())
    *changed = true;

  // First, build a set of network-keys to the ipaddresses.
  for (Network* network : list) {
    bool might_add_to_merged_list = false;
    std::string key = MakeNetworkKey(network->name(),
                                     network->prefix(),
                                     network->prefix_length());
    if (consolidated_address_list.find(key) ==
        consolidated_address_list.end()) {
      AddressList addrlist;
      addrlist.net = network;
      consolidated_address_list[key] = addrlist;
      might_add_to_merged_list = true;
    }
    const std::vector<InterfaceAddress>& addresses = network->GetIPs();
    AddressList& current_list = consolidated_address_list[key];
    for (const InterfaceAddress& address : addresses) {
      current_list.ips.push_back(address);
    }
    if (!might_add_to_merged_list) {
      delete network;
    } else {
      if (current_list.ips[0].family() == AF_INET) {
        stats->ipv4_network_count++;
      } else {
        ASSERT(current_list.ips[0].family() == AF_INET6);
        stats->ipv6_network_count++;
      }
    }
  }

  // Next, look for existing network objects to re-use.
  for (const auto& kv : consolidated_address_list) {
    const std::string& key = kv.first;
    Network* net = kv.second.net;
    auto existing = networks_map_.find(key);
    if (existing == networks_map_.end()) {
      // This network is new. Place it in the network map.
      merged_list.push_back(net);
      networks_map_[key] = net;
      // Also, we might have accumulated IPAddresses from the first
      // step, set it here.
      net->SetIPs(kv.second.ips, true);
      *changed = true;
    } else {
      // This network exists in the map already. Reset its IP addresses.
      *changed = existing->second->SetIPs(kv.second.ips, *changed);
      merged_list.push_back(existing->second);
      if (existing->second != net) {
        delete net;
      }
    }
  }
  networks_ = merged_list;

  // If the network lists changes, we resort it.
  if (*changed) {
    std::sort(networks_.begin(), networks_.end(), SortNetworks);
    // Now network interfaces are sorted, we should set the preference value
    // for each of the interfaces we are planning to use.
    // Preference order of network interfaces might have changed from previous
    // sorting due to addition of higher preference network interface.
    // Since we have already sorted the network interfaces based on our
    // requirements, we will just assign a preference value starting with 127,
    // in decreasing order.
    int pref = kHighestNetworkPreference;
    for (Network* network : networks_) {
      network->set_preference(pref);
      if (pref > 0) {
        --pref;
      } else {
        LOG(LS_ERROR) << "Too many network interfaces to handle!";
        break;
      }
    }
  }
}

BasicNetworkManager::BasicNetworkManager()
    : thread_(NULL), sent_first_update_(false), start_count_(0),
      network_ignore_mask_(kDefaultNetworkIgnoreMask),
      ignore_non_default_routes_(false) {
}

BasicNetworkManager::~BasicNetworkManager() {
}

#if defined(__native_client__)

bool BasicNetworkManager::CreateNetworks(bool include_ignored,
                                         NetworkList* networks) const {
  ASSERT(false);
  LOG(LS_WARNING) << "BasicNetworkManager doesn't work on NaCl yet";
  return false;
}

#elif defined(WEBRTC_POSIX)
void BasicNetworkManager::ConvertIfAddrs(struct ifaddrs* interfaces,
                                         bool include_ignored,
                                         NetworkList* networks) const {
  NetworkMap current_networks;
  for (struct ifaddrs* cursor = interfaces;
       cursor != NULL; cursor = cursor->ifa_next) {
    IPAddress prefix;
    IPAddress mask;
    IPAddress ip;
    int scope_id = 0;

    // Some interfaces may not have address assigned.
    if (!cursor->ifa_addr || !cursor->ifa_netmask)
      continue;

    switch (cursor->ifa_addr->sa_family) {
      case AF_INET: {
        ip = IPAddress(
            reinterpret_cast<sockaddr_in*>(cursor->ifa_addr)->sin_addr);
        mask = IPAddress(
            reinterpret_cast<sockaddr_in*>(cursor->ifa_netmask)->sin_addr);
        break;
      }
      case AF_INET6: {
        if (ipv6_enabled()) {
          ip = IPAddress(
              reinterpret_cast<sockaddr_in6*>(cursor->ifa_addr)->sin6_addr);

          if (IsIgnoredIPv6(ip)) {
            continue;
          }

          mask = IPAddress(
              reinterpret_cast<sockaddr_in6*>(cursor->ifa_netmask)->sin6_addr);
          scope_id =
              reinterpret_cast<sockaddr_in6*>(cursor->ifa_addr)->sin6_scope_id;
          break;
        } else {
          continue;
        }
      }
      default: {
        continue;
      }
    }

    int prefix_length = CountIPMaskBits(mask);
    prefix = TruncateIP(ip, prefix_length);
    std::string key = MakeNetworkKey(std::string(cursor->ifa_name),
                                     prefix, prefix_length);
    auto existing_network = current_networks.find(key);
    if (existing_network == current_networks.end()) {
      AdapterType adapter_type = ADAPTER_TYPE_UNKNOWN;
      if (cursor->ifa_flags & IFF_LOOPBACK) {
        // TODO(phoglund): Need to recognize other types as well.
        adapter_type = ADAPTER_TYPE_LOOPBACK;
      }
      scoped_ptr<Network> network(new Network(cursor->ifa_name,
                                              cursor->ifa_name,
                                              prefix,
                                              prefix_length,
                                              adapter_type));
      network->set_scope_id(scope_id);
      network->AddIP(ip);
      network->set_ignored(IsIgnoredNetwork(*network));
      if (include_ignored || !network->ignored()) {
        networks->push_back(network.release());
      }
    } else {
      (*existing_network).second->AddIP(ip);
    }
  }
}

bool BasicNetworkManager::CreateNetworks(bool include_ignored,
                                         NetworkList* networks) const {
  struct ifaddrs* interfaces;
  int error = getifaddrs(&interfaces);
  if (error != 0) {
    LOG_ERR(LERROR) << "getifaddrs failed to gather interface data: " << error;
    return false;
  }

  ConvertIfAddrs(interfaces, include_ignored, networks);

  freeifaddrs(interfaces);
  return true;
}

#elif defined(WEBRTC_WIN)

unsigned int GetPrefix(PIP_ADAPTER_PREFIX prefixlist,
              const IPAddress& ip, IPAddress* prefix) {
  IPAddress current_prefix;
  IPAddress best_prefix;
  unsigned int best_length = 0;
  while (prefixlist) {
    // Look for the longest matching prefix in the prefixlist.
    if (prefixlist->Address.lpSockaddr == NULL ||
        prefixlist->Address.lpSockaddr->sa_family != ip.family()) {
      prefixlist = prefixlist->Next;
      continue;
    }
    switch (prefixlist->Address.lpSockaddr->sa_family) {
      case AF_INET: {
        sockaddr_in* v4_addr =
            reinterpret_cast<sockaddr_in*>(prefixlist->Address.lpSockaddr);
        current_prefix = IPAddress(v4_addr->sin_addr);
        break;
      }
      case AF_INET6: {
          sockaddr_in6* v6_addr =
              reinterpret_cast<sockaddr_in6*>(prefixlist->Address.lpSockaddr);
          current_prefix = IPAddress(v6_addr->sin6_addr);
          break;
      }
      default: {
        prefixlist = prefixlist->Next;
        continue;
      }
    }
    if (TruncateIP(ip, prefixlist->PrefixLength) == current_prefix &&
        prefixlist->PrefixLength > best_length) {
      best_prefix = current_prefix;
      best_length = prefixlist->PrefixLength;
    }
    prefixlist = prefixlist->Next;
  }
  *prefix = best_prefix;
  return best_length;
}

bool BasicNetworkManager::CreateNetworks(bool include_ignored,
                                         NetworkList* networks) const {
  NetworkMap current_networks;
  // MSDN recommends a 15KB buffer for the first try at GetAdaptersAddresses.
  size_t buffer_size = 16384;
  scoped_ptr<char[]> adapter_info(new char[buffer_size]);
  PIP_ADAPTER_ADDRESSES adapter_addrs =
      reinterpret_cast<PIP_ADAPTER_ADDRESSES>(adapter_info.get());
  int adapter_flags = (GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_ANYCAST |
                       GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_INCLUDE_PREFIX);
  int ret = 0;
  do {
    adapter_info.reset(new char[buffer_size]);
    adapter_addrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(adapter_info.get());
    ret = GetAdaptersAddresses(AF_UNSPEC, adapter_flags,
                               0, adapter_addrs,
                               reinterpret_cast<PULONG>(&buffer_size));
  } while (ret == ERROR_BUFFER_OVERFLOW);
  if (ret != ERROR_SUCCESS) {
    return false;
  }
  int count = 0;
  while (adapter_addrs) {
    if (adapter_addrs->OperStatus == IfOperStatusUp) {
      PIP_ADAPTER_UNICAST_ADDRESS address = adapter_addrs->FirstUnicastAddress;
      PIP_ADAPTER_PREFIX prefixlist = adapter_addrs->FirstPrefix;
      std::string name;
      std::string description;
#ifdef _DEBUG
      name = ToUtf8(adapter_addrs->FriendlyName,
                    wcslen(adapter_addrs->FriendlyName));
#endif
      description = ToUtf8(adapter_addrs->Description,
                           wcslen(adapter_addrs->Description));
      for (; address; address = address->Next) {
#ifndef _DEBUG
        name = rtc::ToString(count);
#endif

        IPAddress ip;
        int scope_id = 0;
        scoped_ptr<Network> network;
        switch (address->Address.lpSockaddr->sa_family) {
          case AF_INET: {
            sockaddr_in* v4_addr =
                reinterpret_cast<sockaddr_in*>(address->Address.lpSockaddr);
            ip = IPAddress(v4_addr->sin_addr);
            break;
          }
          case AF_INET6: {
            if (ipv6_enabled()) {
              sockaddr_in6* v6_addr =
                  reinterpret_cast<sockaddr_in6*>(address->Address.lpSockaddr);
              scope_id = v6_addr->sin6_scope_id;
              ip = IPAddress(v6_addr->sin6_addr);

              if (IsIgnoredIPv6(ip)) {
                continue;
              }

              break;
            } else {
              continue;
            }
          }
          default: {
            continue;
          }
        }

        IPAddress prefix;
        int prefix_length = GetPrefix(prefixlist, ip, &prefix);
        std::string key = MakeNetworkKey(name, prefix, prefix_length);
        auto existing_network = current_networks.find(key);
        if (existing_network == current_networks.end()) {
          AdapterType adapter_type = ADAPTER_TYPE_UNKNOWN;
          if (adapter_addrs->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
            // TODO(phoglund): Need to recognize other types as well.
            adapter_type = ADAPTER_TYPE_LOOPBACK;
          }
          scoped_ptr<Network> network(new Network(name,
                                                  description,
                                                  prefix,
                                                  prefix_length,
                                                  adapter_type));
          network->set_scope_id(scope_id);
          network->AddIP(ip);
          bool ignored = IsIgnoredNetwork(*network);
          network->set_ignored(ignored);
          if (include_ignored || !network->ignored()) {
            networks->push_back(network.release());
          }
        } else {
          (*existing_network).second->AddIP(ip);
        }
      }
      // Count is per-adapter - all 'Networks' created from the same
      // adapter need to have the same name.
      ++count;
    }
    adapter_addrs = adapter_addrs->Next;
  }
  return true;
}
#endif  // WEBRTC_WIN

#if defined(WEBRTC_LINUX)
bool IsDefaultRoute(const std::string& network_name) {
  FileStream fs;
  if (!fs.Open("/proc/net/route", "r", NULL)) {
    LOG(LS_WARNING) << "Couldn't read /proc/net/route, skipping default "
                    << "route check (assuming everything is a default route).";
    return true;
  } else {
    std::string line;
    while (fs.ReadLine(&line) == SR_SUCCESS) {
      char iface_name[256];
      unsigned int iface_ip, iface_gw, iface_mask, iface_flags;
      if (sscanf(line.c_str(),
                 "%255s %8X %8X %4X %*d %*u %*d %8X",
                 iface_name, &iface_ip, &iface_gw,
                 &iface_flags, &iface_mask) == 5 &&
          network_name == iface_name &&
          iface_mask == 0 &&
          (iface_flags & (RTF_UP | RTF_HOST)) == RTF_UP) {
        return true;
      }
    }
  }
  return false;
}
#endif

bool BasicNetworkManager::IsIgnoredNetwork(const Network& network) const {
  // Ignore networks on the explicit ignore list.
  for (const std::string& ignored_name : network_ignore_list_) {
    if (network.name() == ignored_name) {
      return true;
    }
  }

  if (network_ignore_mask_ & network.type()) {
    return true;
  }
#if defined(WEBRTC_POSIX)
  // Filter out VMware/VirtualBox interfaces, typically named vmnet1,
  // vmnet8, or vboxnet0.
  if (strncmp(network.name().c_str(), "vmnet", 5) == 0 ||
      strncmp(network.name().c_str(), "vnic", 4) == 0 ||
      strncmp(network.name().c_str(), "vboxnet", 7) == 0) {
    return true;
  }
#if defined(WEBRTC_LINUX)
  // Make sure this is a default route, if we're ignoring non-defaults.
  if (ignore_non_default_routes_ && !IsDefaultRoute(network.name())) {
    return true;
  }
#endif
#elif defined(WEBRTC_WIN)
  // Ignore any HOST side vmware adapters with a description like:
  // VMware Virtual Ethernet Adapter for VMnet1
  // but don't ignore any GUEST side adapters with a description like:
  // VMware Accelerated AMD PCNet Adapter #2
  if (strstr(network.description().c_str(), "VMnet") != NULL) {
    return true;
  }
#endif

  // Ignore any networks with a 0.x.y.z IP
  if (network.prefix().family() == AF_INET) {
    return (network.prefix().v4AddressAsHostOrderInteger() < 0x01000000);
  }

  return false;
}

void BasicNetworkManager::StartUpdating() {
  thread_ = Thread::Current();
  if (start_count_) {
    // If network interfaces are already discovered and signal is sent,
    // we should trigger network signal immediately for the new clients
    // to start allocating ports.
    if (sent_first_update_)
      thread_->Post(this, kSignalNetworksMessage);
  } else {
    thread_->Post(this, kUpdateNetworksMessage);
  }
  ++start_count_;
}

void BasicNetworkManager::StopUpdating() {
  ASSERT(Thread::Current() == thread_);
  if (!start_count_)
    return;

  --start_count_;
  if (!start_count_) {
    thread_->Clear(this);
    sent_first_update_ = false;
  }
}

void BasicNetworkManager::OnMessage(Message* msg) {
  switch (msg->message_id) {
    case kUpdateNetworksMessage:  {
      DoUpdateNetworks();
      break;
    }
    case kSignalNetworksMessage:  {
      SignalNetworksChanged();
      break;
    }
    default:
      ASSERT(false);
  }
}

void BasicNetworkManager::DoUpdateNetworks() {
  if (!start_count_)
    return;

  ASSERT(Thread::Current() == thread_);

  NetworkList list;
  if (!CreateNetworks(false, &list)) {
    SignalError();
  } else {
    bool changed;
    MergeNetworkList(list, &changed);
    if (changed || !sent_first_update_) {
      SignalNetworksChanged();
      sent_first_update_ = true;
    }
  }

  thread_->PostDelayed(kNetworksUpdateIntervalMs, this, kUpdateNetworksMessage);
}

void BasicNetworkManager::DumpNetworks(bool include_ignored) {
  NetworkList list;
  CreateNetworks(include_ignored, &list);
  LOG(LS_INFO) << "NetworkManager detected " << list.size() << " networks:";
  for (const Network* network : list) {
    if (!network->ignored() || include_ignored) {
      LOG(LS_INFO) << network->ToString() << ": "
                   << network->description()
                   << ((network->ignored()) ? ", Ignored" : "");
    }
  }
  // Release the network list created previously.
  // Do this in a seperated for loop for better readability.
  for (Network* network : list) {
    delete network;
  }
}

Network::Network(const std::string& name, const std::string& desc,
                 const IPAddress& prefix, int prefix_length)
    : name_(name), description_(desc), prefix_(prefix),
      prefix_length_(prefix_length),
      key_(MakeNetworkKey(name, prefix, prefix_length)), scope_id_(0),
      ignored_(false), type_(ADAPTER_TYPE_UNKNOWN), preference_(0) {
}

Network::Network(const std::string& name, const std::string& desc,
                 const IPAddress& prefix, int prefix_length, AdapterType type)
    : name_(name), description_(desc), prefix_(prefix),
      prefix_length_(prefix_length),
      key_(MakeNetworkKey(name, prefix, prefix_length)), scope_id_(0),
      ignored_(false), type_(type), preference_(0) {
}

Network::~Network() = default;

// Sets the addresses of this network. Returns true if the address set changed.
// Change detection is short circuited if the changed argument is true.
bool Network::SetIPs(const std::vector<InterfaceAddress>& ips, bool changed) {
  // Detect changes with a nested loop; n-squared but we expect on the order
  // of 2-3 addresses per network.
  changed = changed || ips.size() != ips_.size();
  if (!changed) {
    for (const InterfaceAddress& ip : ips) {
      if (std::find(ips_.begin(), ips_.end(), ip) == ips_.end()) {
        changed = true;
        break;
      }
    }
  }

  ips_ = ips;
  return changed;
}

// Select the best IP address to use from this Network.
IPAddress Network::GetBestIP() const {
  if (ips_.size() == 0) {
    return IPAddress();
  }

  if (prefix_.family() == AF_INET) {
    return static_cast<IPAddress>(ips_.at(0));
  }

  InterfaceAddress selected_ip, ula_ip;

  for (const InterfaceAddress& ip : ips_) {
    // Ignore any address which has been deprecated already.
    if (ip.ipv6_flags() & IPV6_ADDRESS_FLAG_DEPRECATED)
      continue;

    // ULA address should only be returned when we have no other
    // global IP.
    if (IPIsULA(static_cast<const IPAddress&>(ip))) {
      ula_ip = ip;
      continue;
    }
    selected_ip = ip;

    // Search could stop once a temporary non-deprecated one is found.
    if (ip.ipv6_flags() & IPV6_ADDRESS_FLAG_TEMPORARY)
      break;
  }

  // No proper global IPv6 address found, use ULA instead.
  if (IPIsUnspec(selected_ip) && !IPIsUnspec(ula_ip)) {
    selected_ip = ula_ip;
  }

  return static_cast<IPAddress>(selected_ip);
}

std::string Network::ToString() const {
  std::stringstream ss;
  // Print out the first space-terminated token of the network desc, plus
  // the IP address.
  ss << "Net[" << description_.substr(0, description_.find(' '))
     << ":" << prefix_.ToSensitiveString() << "/" << prefix_length_
     << ":" << AdapterTypeToString(type_) << "]";
  return ss.str();
}

}  // namespace rtc
