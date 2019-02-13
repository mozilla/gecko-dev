/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/crc32.h"

#include "webrtc/base/basicdefs.h"

namespace rtc {

// This implementation is based on the sample implementation in RFC 1952.

// CRC32 polynomial, in reversed form.
// See RFC 1952, or http://en.wikipedia.org/wiki/Cyclic_redundancy_check
static const uint32 kCrc32Polynomial = 0xEDB88320;
static uint32 kCrc32Table[256] = { 0 };

static void EnsureCrc32TableInited() {
  if (kCrc32Table[ARRAY_SIZE(kCrc32Table) - 1])
    return;  // already inited
  for (uint32 i = 0; i < ARRAY_SIZE(kCrc32Table); ++i) {
    uint32 c = i;
    for (size_t j = 0; j < 8; ++j) {
      if (c & 1) {
        c = kCrc32Polynomial ^ (c >> 1);
      } else {
        c >>= 1;
      }
    }
    kCrc32Table[i] = c;
  }
}

uint32 UpdateCrc32(uint32 start, const void* buf, size_t len) {
  EnsureCrc32TableInited();

  uint32 c = start ^ 0xFFFFFFFF;
  const uint8* u = static_cast<const uint8*>(buf);
  for (size_t i = 0; i < len; ++i) {
    c = kCrc32Table[(c ^ u[i]) & 0xFF] ^ (c >> 8);
  }
  return c ^ 0xFFFFFFFF;
}

}  // namespace rtc

