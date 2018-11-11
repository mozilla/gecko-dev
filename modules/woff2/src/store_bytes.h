// Copyright 2013 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Helper functions for storing integer values into byte streams.
// No bounds checking is performed, that is the responsibility of the caller.

#ifndef WOFF2_STORE_BYTES_H_
#define WOFF2_STORE_BYTES_H_

#include <inttypes.h>
#include <stddef.h>
#include <string.h>

namespace woff2 {

inline size_t StoreU32(uint8_t* dst, size_t offset, uint32_t x) {
  dst[offset] = x >> 24;
  dst[offset + 1] = x >> 16;
  dst[offset + 2] = x >> 8;
  dst[offset + 3] = x;
  return offset + 4;
}

inline size_t Store16(uint8_t* dst, size_t offset, int x) {
#if (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__))
  uint16_t v = ((x & 0xFF) << 8) | ((x & 0xFF00) >> 8);
  memcpy(dst + offset, &v, 2);
#elif (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__))
  uint16_t v = static_cast<uint16_t>(x);
  memcpy(dst + offset, &v, 2);
#else
  dst[offset] = x >> 8;
  dst[offset + 1] = x;
#endif
  return offset + 2;
}

inline void StoreU32(uint32_t val, size_t* offset, uint8_t* dst) {
  dst[(*offset)++] = val >> 24;
  dst[(*offset)++] = val >> 16;
  dst[(*offset)++] = val >> 8;
  dst[(*offset)++] = val;
}

inline void Store16(int val, size_t* offset, uint8_t* dst) {
#if (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__))
  uint16_t v = ((val & 0xFF) << 8) | ((val & 0xFF00) >> 8);
  memcpy(dst + *offset, &v, 2);
      ((val & 0xFF) << 8) | ((val & 0xFF00) >> 8);
  *offset += 2;
#elif (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__))
  uint16_t v = static_cast<uint16_t>(val);
  memcpy(dst + *offset, &v, 2);
  *offset += 2;
#else
  dst[(*offset)++] = val >> 8;
  dst[(*offset)++] = val;
#endif
}

inline void StoreBytes(const uint8_t* data, size_t len,
                       size_t* offset, uint8_t* dst) {
  memcpy(&dst[*offset], data, len);
  *offset += len;
}

} // namespace woff2

#endif  // WOFF2_STORE_BYTES_H_
