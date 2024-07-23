/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Uint8Clamped_h
#define vm_Uint8Clamped_h

#include <algorithm>
#include <limits>
#include <stdint.h>
#include <type_traits>

#include "vm/Float16.h"

namespace js {

extern uint32_t ClampDoubleToUint8(const double x);

class uint8_clamped final {
  uint8_t val;

  template <typename IntT>
  static constexpr uint8_t ClampIntToUint8(IntT x) {
    static_assert(std::is_integral_v<IntT>);

    if constexpr (std::numeric_limits<IntT>::max() < 255) {
      return std::clamp<IntT>(x, 0, std::numeric_limits<IntT>::max());
    } else {
      return std::clamp<IntT>(x, 0, 255);
    }
  }

 public:
  // The default constructor can be 'constexpr' when we switch to C++20.
  //
  // C++17 requires explicit initialization of all members when using a
  // 'constexpr' default constructor. That means `val` needs to be initialized
  // through a member initializer. But adding a member initializer makes the
  // class no longer trivial, which breaks memcpy/memset optimizations.

  /* constexpr */ uint8_clamped() = default;
  constexpr uint8_clamped(const uint8_clamped&) = default;

  constexpr explicit uint8_clamped(uint8_t x) : val(x) {}
  constexpr explicit uint8_clamped(uint16_t x) : val(ClampIntToUint8(x)) {}
  constexpr explicit uint8_clamped(uint32_t x) : val(ClampIntToUint8(x)) {}
  constexpr explicit uint8_clamped(uint64_t x) : val(ClampIntToUint8(x)) {}
  constexpr explicit uint8_clamped(int8_t x) : val(ClampIntToUint8(x)) {}
  constexpr explicit uint8_clamped(int16_t x) : val(ClampIntToUint8(x)) {}
  constexpr explicit uint8_clamped(int32_t x) : val(ClampIntToUint8(x)) {}
  constexpr explicit uint8_clamped(int64_t x) : val(ClampIntToUint8(x)) {}
  explicit uint8_clamped(double x) : val(uint8_t(ClampDoubleToUint8(x))) {}

  constexpr uint8_clamped& operator=(const uint8_clamped&) = default;

  // Invoke constructors for the assignment helpers.

  constexpr uint8_clamped& operator=(uint8_t x) {
    *this = uint8_clamped{x};
    return *this;
  }

  constexpr uint8_clamped& operator=(uint16_t x) {
    *this = uint8_clamped{x};
    return *this;
  }

  constexpr uint8_clamped& operator=(uint32_t x) {
    *this = uint8_clamped{x};
    return *this;
  }

  constexpr uint8_clamped& operator=(uint64_t x) {
    *this = uint8_clamped{x};
    return *this;
  }

  constexpr uint8_clamped& operator=(int8_t x) {
    *this = uint8_clamped{x};
    return *this;
  }

  constexpr uint8_clamped& operator=(int16_t x) {
    *this = uint8_clamped{x};
    return *this;
  }

  constexpr uint8_clamped& operator=(int32_t x) {
    *this = uint8_clamped{x};
    return *this;
  }

  constexpr uint8_clamped& operator=(int64_t x) {
    *this = uint8_clamped{x};
    return *this;
  }

  uint8_clamped& operator=(const double x) {
    *this = uint8_clamped{x};
    return *this;
  }

  constexpr operator uint8_t() const { return val; }
};

static_assert(sizeof(uint8_clamped) == 1,
              "uint8_clamped must be layout-compatible with uint8_t");

static_assert(std::is_trivial_v<uint8_clamped>,
              "uint8_clamped must be trivial to be eligible for memcpy/memset "
              "optimizations");

/* Note that we can't use std::numeric_limits here due to uint8_clamped. */
template <typename T>
inline constexpr bool TypeIsFloatingPoint() {
  return false;
}
template <>
inline constexpr bool TypeIsFloatingPoint<float16>() {
  return true;
}
template <>
inline constexpr bool TypeIsFloatingPoint<float>() {
  return true;
}
template <>
inline constexpr bool TypeIsFloatingPoint<double>() {
  return true;
}

template <typename T>
inline constexpr bool TypeIsUnsigned() {
  return false;
}
template <>
inline constexpr bool TypeIsUnsigned<uint8_t>() {
  return true;
}
template <>
inline constexpr bool TypeIsUnsigned<uint16_t>() {
  return true;
}
template <>
inline constexpr bool TypeIsUnsigned<uint32_t>() {
  return true;
}

}  // namespace js

#endif  // vm_Uint8Clamped_h
