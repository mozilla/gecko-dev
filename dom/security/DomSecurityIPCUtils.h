/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_domsecurityipcutils_h
#define mozilla_dom_domsecurityipcutils_h

#include "ipc/EnumSerializer.h"
#include "nsILoadInfo.h"

namespace IPC {

// nsILoadInfo::HTTPSUpgradeTelemetryType over IPC.
template <>
struct ParamTraits<nsILoadInfo::HTTPSUpgradeTelemetryType>
    : public ContiguousEnumSerializerInclusive<
          nsILoadInfo::HTTPSUpgradeTelemetryType,
          nsILoadInfo::HTTPSUpgradeTelemetryType::NOT_INITIALIZED,
          nsILoadInfo::HTTPSUpgradeTelemetryType::UPGRADE_EXCEPTION> {};

template <>
struct ParamTraits<nsILoadInfo::SchemelessInputType>
    : public ContiguousEnumSerializerInclusive<
          nsILoadInfo::SchemelessInputType,
          nsILoadInfo::SchemelessInputType::SchemelessInputTypeUnset,
          nsILoadInfo::SchemelessInputType::SchemelessInputTypeSchemeless> {};

}  // namespace IPC

#endif  // mozilla_dom_domsecurityipcutils_h
