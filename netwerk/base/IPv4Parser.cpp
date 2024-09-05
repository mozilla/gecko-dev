/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPv4Parser.h"
#include "mozilla/EndianUtils.h"
#include "nsPrintfCString.h"
#include "nsTArray.h"

namespace mozilla::net::IPv4Parser {

// https://url.spec.whatwg.org/#ends-in-a-number-checker
bool EndsInANumber(const nsCString& input) {
  // 1. Let parts be the result of strictly splitting input on U+002E (.).
  nsTArray<nsDependentCSubstring> parts;
  for (const nsDependentCSubstring& part : input.Split('.')) {
    parts.AppendElement(part);
  }

  if (parts.Length() == 0) {
    return false;
  }

  // 2.If the last item in parts is the empty string, then:
  //    1. If parts’s size is 1, then return false.
  //    2. Remove the last item from parts.
  if (parts.LastElement().IsEmpty()) {
    if (parts.Length() == 1) {
      return false;
    }
    Unused << parts.PopLastElement();
  }

  // 3. Let last be the last item in parts.
  const nsDependentCSubstring& last = parts.LastElement();

  // 4. If last is non-empty and contains only ASCII digits, then return true.
  // The erroneous input "09" will be caught by the IPv4 parser at a later
  // stage.
  if (!last.IsEmpty()) {
    if (ContainsOnlyAsciiDigits(last)) {
      return true;
    }
  }

  // 5. If parsing last as an IPv4 number does not return failure, then return
  // true. This is equivalent to checking that last is "0X" or "0x", followed by
  // zero or more ASCII hex digits.
  if (StringBeginsWith(last, "0x"_ns) || StringBeginsWith(last, "0X"_ns)) {
    if (ContainsOnlyAsciiHexDigits(Substring(last, 2))) {
      return true;
    }
  }

  return false;
}

nsresult ParseIPv4Number10(const nsACString& input, uint32_t& number,
                           uint32_t maxNumber) {
  uint64_t value = 0;
  const char* current = input.BeginReading();
  const char* end = input.EndReading();
  for (; current < end; ++current) {
    char c = *current;
    MOZ_ASSERT(c >= '0' && c <= '9');
    value *= 10;
    value += c - '0';
  }
  if (value <= maxNumber) {
    number = value;
    return NS_OK;
  }

  // The error case
  number = 0;
  return NS_ERROR_FAILURE;
}

nsresult ParseIPv4Number(const nsACString& input, int32_t base,
                         uint32_t& number, uint32_t maxNumber) {
  // Accumulate in the 64-bit value
  uint64_t value = 0;
  const char* current = input.BeginReading();
  const char* end = input.EndReading();
  switch (base) {
    case 16:
      ++current;
      [[fallthrough]];
    case 8:
      ++current;
      break;
    case 10:
    default:
      break;
  }
  for (; current < end; ++current) {
    value *= base;
    char c = *current;
    MOZ_ASSERT((base == 10 && IsAsciiDigit(c)) ||
               (base == 8 && c >= '0' && c <= '7') ||
               (base == 16 && IsAsciiHexDigit(c)));
    if (IsAsciiDigit(c)) {
      value += c - '0';
    } else if (c >= 'a' && c <= 'f') {
      value += c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
      value += c - 'A' + 10;
    }
  }

  if (value <= maxNumber) {
    number = value;
    return NS_OK;
  }

  // The error case
  number = 0;
  return NS_ERROR_FAILURE;
}

// IPv4 parser spec: https://url.spec.whatwg.org/#concept-ipv4-parser
nsresult NormalizeIPv4(const nsACString& host, nsCString& result) {
  int32_t bases[4] = {10, 10, 10, 10};
  bool onlyBase10 = true;  // Track this as a special case
  int32_t dotIndex[3];     // The positions of the dots in the string

  // Use "length" rather than host.Length() after call to
  // ValidateIPv4Number because of potential trailing period.
  nsDependentCSubstring filteredHost;
  bool trailingDot = false;
  if (host.Length() > 0 && host.Last() == '.') {
    trailingDot = true;
    filteredHost.Rebind(host.BeginReading(), host.Length() - 1);
  } else {
    filteredHost.Rebind(host.BeginReading(), host.Length());
  }

  int32_t length = static_cast<int32_t>(filteredHost.Length());
  int32_t dotCount = ValidateIPv4Number(filteredHost, bases, dotIndex,
                                        onlyBase10, length, trailingDot);
  if (dotCount < 0 || length <= 0) {
    return NS_ERROR_FAILURE;
  }

  // Max values specified by the spec
  static const uint32_t upperBounds[] = {0xffffffffu, 0xffffffu, 0xffffu,
                                         0xffu};
  uint32_t ipv4;
  int32_t start = (dotCount > 0 ? dotIndex[dotCount - 1] + 1 : 0);

  // parse the last part first
  nsresult res;
  // Doing a special case for all items being base 10 gives ~35% speedup
  res = (onlyBase10
             ? ParseIPv4Number10(Substring(host, start, length - start), ipv4,
                                 upperBounds[dotCount])
             : ParseIPv4Number(Substring(host, start, length - start),
                               bases[dotCount], ipv4, upperBounds[dotCount]));
  if (NS_FAILED(res)) {
    return NS_ERROR_FAILURE;
  }

  // parse remaining parts starting from first part
  int32_t lastUsed = -1;
  for (int32_t i = 0; i < dotCount; i++) {
    uint32_t number;
    start = lastUsed + 1;
    lastUsed = dotIndex[i];
    res =
        (onlyBase10 ? ParseIPv4Number10(
                          Substring(host, start, lastUsed - start), number, 255)
                    : ParseIPv4Number(Substring(host, start, lastUsed - start),
                                      bases[i], number, 255));
    if (NS_FAILED(res)) {
      return NS_ERROR_FAILURE;
    }
    ipv4 += number << (8 * (3 - i));
  }

  // A special case for ipv4 URL like "127." should have the same result as
  // "127".
  if (dotCount == 1 && dotIndex[0] == length - 1) {
    ipv4 = (ipv4 & 0xff000000) >> 24;
  }

  uint8_t ipSegments[4];
  NetworkEndian::writeUint32(ipSegments, ipv4);
  result = nsPrintfCString("%d.%d.%d.%d", ipSegments[0], ipSegments[1],
                           ipSegments[2], ipSegments[3]);
  return NS_OK;
}

// Return the number of "dots" in the string, or -1 if invalid.  Note that the
// number of relevant entries in the bases/starts/ends arrays is number of
// dots + 1.
//
// length is assumed to be <= host.Length(); the caller is responsible for that
//
// Note that the value returned is guaranteed to be in [-1, 3] range.
int32_t ValidateIPv4Number(const nsACString& host, int32_t bases[4],
                           int32_t dotIndex[3], bool& onlyBase10,
                           int32_t length, bool trailingDot) {
  MOZ_ASSERT(length <= (int32_t)host.Length());
  if (length <= 0) {
    return -1;
  }

  bool lastWasNumber = false;  // We count on this being false for i == 0
  int32_t dotCount = 0;
  onlyBase10 = true;

  for (int32_t i = 0; i < length; i++) {
    char current = host[i];
    if (current == '.') {
      // A dot should not follow a dot, or be first - it can follow an x though.
      if (!(lastWasNumber ||
            (i >= 2 && (host[i - 1] == 'X' || host[i - 1] == 'x') &&
             host[i - 2] == '0')) ||
          (i == (length - 1) && trailingDot)) {
        return -1;
      }

      if (dotCount > 2) {
        return -1;
      }
      lastWasNumber = false;
      dotIndex[dotCount] = i;
      dotCount++;
    } else if (current == 'X' || current == 'x') {
      if (!lastWasNumber ||  // An X should not follow an X or a dot or be first
          i == (length - 1) ||  // No trailing Xs allowed
          (dotCount == 0 &&
           i != 1) ||            // If we had no dots, an X should be second
          host[i - 1] != '0' ||  // X should always follow a 0.  Guaranteed i >
                                 // 0 as lastWasNumber is true
          (dotCount > 0 &&
           host[i - 2] != '.')) {  // And that zero follows a dot if it exists
        return -1;
      }
      lastWasNumber = false;
      bases[dotCount] = 16;
      onlyBase10 = false;

    } else if (current == '0') {
      if (i < length - 1 &&      // Trailing zero doesn't signal octal
          host[i + 1] != '.' &&  // Lone zero is not octal
          (i == 0 || host[i - 1] == '.')) {  // Zero at start or following a dot
                                             // is a candidate for octal
        bases[dotCount] = 8;  // This will turn to 16 above if X shows up
        onlyBase10 = false;
      }
      lastWasNumber = true;

    } else if (current >= '1' && current <= '7') {
      lastWasNumber = true;

    } else if (current >= '8' && current <= '9') {
      if (bases[dotCount] == 8) {
        return -1;
      }
      lastWasNumber = true;

    } else if ((current >= 'a' && current <= 'f') ||
               (current >= 'A' && current <= 'F')) {
      if (bases[dotCount] != 16) {
        return -1;
      }
      lastWasNumber = true;

    } else {
      return -1;
    }
  }

  return dotCount;
}

bool ContainsOnlyAsciiDigits(const nsDependentCSubstring& input) {
  for (const auto* c = input.BeginReading(); c < input.EndReading(); c++) {
    if (!IsAsciiDigit(*c)) {
      return false;
    }
  }

  return true;
}

bool ContainsOnlyAsciiHexDigits(const nsDependentCSubstring& input) {
  for (const auto* c = input.BeginReading(); c < input.EndReading(); c++) {
    if (!IsAsciiHexDigit(*c)) {
      return false;
    }
  }
  return true;
}

}  // namespace mozilla::net::IPv4Parser
