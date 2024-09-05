/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_IPv4Parser_h
#define mozilla_net_IPv4Parser_h

#include "nsString.h"

namespace mozilla::net::IPv4Parser {

bool EndsInANumber(const nsCString& input);
nsresult NormalizeIPv4(const nsACString& host, nsCString& result);

nsresult ParseIPv4Number(const nsACString& input, int32_t base,
                         uint32_t& number, uint32_t maxNumber);
int32_t ValidateIPv4Number(const nsACString& host, int32_t bases[4],
                           int32_t dotIndex[3], bool& onlyBase10,
                           int32_t length, bool trailingDot);

bool ContainsOnlyAsciiDigits(const nsDependentCSubstring& input);
bool ContainsOnlyAsciiHexDigits(const nsDependentCSubstring& input);
nsresult ParseIPv4Number10(const nsACString& input, uint32_t& number,
                           uint32_t maxNumber);
}  // namespace mozilla::net::IPv4Parser

#endif  // mozilla_net_IPv4Parser_h
