// Copyright 2013, ARM Limited
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of ARM Limited nor the names of its contributors may be
//     used to endorse or promote products derived from this software without
//     specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef VIXL_UTILS_H
#define VIXL_UTILS_H

#include "mozilla/FloatingPoint.h"

#include "jit/arm64/vixl/Globals-vixl.h"

namespace vixl {

// Macros for compile-time format checking.
#if defined(__GNUC__)
#define PRINTF_CHECK(format_index, varargs_index) \
  __attribute__((format(printf, format_index, varargs_index)))
#else
#define PRINTF_CHECK(format_index, varargs_index)
#endif

// Check number width.
inline bool is_intn(unsigned n, int64_t x) {
  VIXL_ASSERT((0 < n) && (n < 64));
  int64_t limit = INT64_C(1) << (n - 1);
  return (-limit <= x) && (x < limit);
}

inline bool is_uintn(unsigned n, int64_t x) {
  VIXL_ASSERT((0 < n) && (n < 64));
  return !(x >> n);
}

inline unsigned truncate_to_intn(unsigned n, int64_t x) {
  VIXL_ASSERT((0 < n) && (n < 64));
  return (x & ((INT64_C(1) << n) - 1));
}

#define INT_1_TO_63_LIST(V)                                                    \
V(1)  V(2)  V(3)  V(4)  V(5)  V(6)  V(7)  V(8)                                 \
V(9)  V(10) V(11) V(12) V(13) V(14) V(15) V(16)                                \
V(17) V(18) V(19) V(20) V(21) V(22) V(23) V(24)                                \
V(25) V(26) V(27) V(28) V(29) V(30) V(31) V(32)                                \
V(33) V(34) V(35) V(36) V(37) V(38) V(39) V(40)                                \
V(41) V(42) V(43) V(44) V(45) V(46) V(47) V(48)                                \
V(49) V(50) V(51) V(52) V(53) V(54) V(55) V(56)                                \
V(57) V(58) V(59) V(60) V(61) V(62) V(63)

#define DECLARE_IS_INT_N(N)                                                    \
inline bool is_int##N(int64_t x) { return is_intn(N, x); }
#define DECLARE_IS_UINT_N(N)                                                   \
inline bool is_uint##N(int64_t x) { return is_uintn(N, x); }
#define DECLARE_TRUNCATE_TO_INT_N(N)                                           \
inline int truncate_to_int##N(int x) { return truncate_to_intn(N, x); }
INT_1_TO_63_LIST(DECLARE_IS_INT_N)
INT_1_TO_63_LIST(DECLARE_IS_UINT_N)
INT_1_TO_63_LIST(DECLARE_TRUNCATE_TO_INT_N)
#undef DECLARE_IS_INT_N
#undef DECLARE_IS_UINT_N
#undef DECLARE_TRUNCATE_TO_INT_N

// Bit field extraction.
inline uint32_t unsigned_bitextract_32(int msb, int lsb, uint32_t x) {
  return (x >> lsb) & ((1 << (1 + msb - lsb)) - 1);
}

inline uint64_t unsigned_bitextract_64(int msb, int lsb, uint64_t x) {
  return (x >> lsb) & ((static_cast<uint64_t>(1) << (1 + msb - lsb)) - 1);
}

inline int32_t signed_bitextract_32(int msb, int lsb, int32_t x) {
  return (x << (31 - msb)) >> (lsb + 31 - msb);
}

inline int64_t signed_bitextract_64(int msb, int lsb, int64_t x) {
  return (x << (63 - msb)) >> (lsb + 63 - msb);
}

// Floating point representation.
uint32_t float_to_rawbits(float value);
uint64_t double_to_rawbits(double value);
float rawbits_to_float(uint32_t bits);
double rawbits_to_double(uint64_t bits);

// NaN tests.
inline bool IsSignallingNaN(double num) {
  const uint64_t kFP64QuietNaNMask = UINT64_C(0x0008000000000000);
  uint64_t raw = double_to_rawbits(num);
  if (mozilla::IsNaN(num) && ((raw & kFP64QuietNaNMask) == 0))
    return true;
  return false;
}

inline bool IsSignallingNaN(float num) {
  const uint32_t kFP32QuietNaNMask = 0x00400000;
  uint32_t raw = float_to_rawbits(num);
  if (mozilla::IsNaN(num) && ((raw & kFP32QuietNaNMask) == 0))
    return true;
  return false;
}

template <typename T>
inline bool IsQuietNaN(T num) {
  return mozilla::IsNaN(num) && !IsSignallingNaN(num);
}

// Convert the NaN in 'num' to a quiet NaN.
inline double ToQuietNaN(double num) {
  const uint64_t kFP64QuietNaNMask = UINT64_C(0x0008000000000000);
  VIXL_ASSERT(mozilla::IsNaN(num));
  return rawbits_to_double(double_to_rawbits(num) | kFP64QuietNaNMask);
}

inline float ToQuietNaN(float num) {
  const uint32_t kFP32QuietNaNMask = 0x00400000;
  VIXL_ASSERT(mozilla::IsNaN(num));
  return rawbits_to_float(float_to_rawbits(num) | kFP32QuietNaNMask);
}

// Fused multiply-add.
inline double FusedMultiplyAdd(double op1, double op2, double a) {
  return fma(op1, op2, a);
}

inline float FusedMultiplyAdd(float op1, float op2, float a) {
  return fmaf(op1, op2, a);
}

// Bit counting.
int CountLeadingZeros(uint64_t value, int width);
int CountLeadingSignBits(int64_t value, int width);
int CountTrailingZeros(uint64_t value, int width);
int CountSetBits(uint64_t value, int width);
uint64_t LowestSetBit(uint64_t value);

// Pointer alignment
// TODO: rename/refactor to make it specific to instructions.
template<typename T>
bool IsWordAligned(T pointer) {
  VIXL_ASSERT(sizeof(pointer) == sizeof(intptr_t));   // NOLINT(runtime/sizeof)
  return (reinterpret_cast<intptr_t>(pointer) & 3) == 0;
}

// Increment a pointer until it has the specified alignment.
template<class T>
T AlignUp(T pointer, size_t alignment) {
  // Use C-style casts to get static_cast behaviour for integral types (T), and
  // reinterpret_cast behaviour for other types.

  uintptr_t pointer_raw = (uintptr_t)pointer;
  JS_STATIC_ASSERT(sizeof(pointer) == sizeof(pointer_raw));

  size_t align_step = (alignment - pointer_raw) % alignment;
  VIXL_ASSERT((pointer_raw + align_step) % alignment == 0);

  return (T)(pointer_raw + align_step);
}

// Decrement a pointer until it has the specified alignment.
template<class T>
T AlignDown(T pointer, size_t alignment) {
  // Use C-style casts to get static_cast behaviour for integral types (T), and
  // reinterpret_cast behaviour for other types.

  uintptr_t pointer_raw = (uintptr_t)pointer;
  JS_STATIC_ASSERT(sizeof(pointer) == sizeof(pointer_raw));

  size_t align_step = pointer_raw % alignment;
  VIXL_ASSERT((pointer_raw - align_step) % alignment == 0);

  return (T)(pointer_raw - align_step);
}

}  // namespace vixl

#endif  // VIXL_UTILS_H

