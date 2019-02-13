/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SDPENUM_H_
#define _SDPENUM_H_

#include <ostream>

#include "mozilla/Assertions.h"

namespace mozilla
{
namespace sdp
{

enum NetType { kNetTypeNone, kInternet };

inline std::ostream& operator<<(std::ostream& os, sdp::NetType t)
{
  switch (t) {
    case sdp::kNetTypeNone:
      MOZ_ASSERT(false);
      return os << "NONE";
    case sdp::kInternet:
      return os << "IN";
  }
  MOZ_CRASH("Unknown NetType");
}

enum AddrType { kAddrTypeNone, kIPv4, kIPv6 };

inline std::ostream& operator<<(std::ostream& os, sdp::AddrType t)
{
  switch (t) {
    case sdp::kAddrTypeNone:
      MOZ_ASSERT(false);
      return os << "NONE";
    case sdp::kIPv4:
      return os << "IP4";
    case sdp::kIPv6:
      return os << "IP6";
  }
  MOZ_CRASH("Unknown AddrType");
}

} // namespace sdp

} // namespace mozilla

#endif
