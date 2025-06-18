#ifndef icu4x_DecimalSignDisplay_HPP
#define icu4x_DecimalSignDisplay_HPP

#include "DecimalSignDisplay.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::capi::DecimalSignDisplay icu4x::DecimalSignDisplay::AsFFI() const {
  return static_cast<icu4x::capi::DecimalSignDisplay>(value);
}

inline icu4x::DecimalSignDisplay icu4x::DecimalSignDisplay::FromFFI(icu4x::capi::DecimalSignDisplay c_enum) {
  switch (c_enum) {
    case icu4x::capi::DecimalSignDisplay_Auto:
    case icu4x::capi::DecimalSignDisplay_Never:
    case icu4x::capi::DecimalSignDisplay_Always:
    case icu4x::capi::DecimalSignDisplay_ExceptZero:
    case icu4x::capi::DecimalSignDisplay_Negative:
      return static_cast<icu4x::DecimalSignDisplay::Value>(c_enum);
    default:
      std::abort();
  }
}
#endif // icu4x_DecimalSignDisplay_HPP
