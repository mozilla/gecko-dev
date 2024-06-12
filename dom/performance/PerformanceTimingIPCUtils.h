/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_PerformanceTimingIPCUtils_h
#define _mozilla_dom_PerformanceTimingIPCUtils_h

#include "mozilla/EnumTypeTraits.h"
#include "ipc/EnumSerializer.h"
#include "nsITimedChannel.h"

namespace IPC {
template <>
struct ParamTraits<nsITimedChannel::BodyInfoAccess>
    : ContiguousEnumSerializerInclusive<
          nsITimedChannel::BodyInfoAccess,
          nsITimedChannel::BodyInfoAccess::DISALLOWED,
          nsITimedChannel::BodyInfoAccess::ALLOW_ALL> {};
}  // namespace IPC
#endif  // _mozilla_dom_PerformanceTimingIPCUtils_h
