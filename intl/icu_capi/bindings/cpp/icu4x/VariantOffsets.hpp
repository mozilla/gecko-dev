#ifndef icu4x_VariantOffsets_HPP
#define icu4x_VariantOffsets_HPP

#include "VariantOffsets.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "../diplomat_runtime.hpp"
#include "UtcOffset.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    } // extern "C"
} // namespace capi
} // namespace


inline icu4x::capi::VariantOffsets icu4x::VariantOffsets::AsFFI() const {
  return icu4x::capi::VariantOffsets {
    /* .standard = */ standard->AsFFI(),
    /* .daylight = */ daylight ? daylight->AsFFI() : nullptr,
  };
}

inline icu4x::VariantOffsets icu4x::VariantOffsets::FromFFI(icu4x::capi::VariantOffsets c_struct) {
  return icu4x::VariantOffsets {
    /* .standard = */ std::unique_ptr<icu4x::UtcOffset>(icu4x::UtcOffset::FromFFI(c_struct.standard)),
    /* .daylight = */ std::unique_ptr<icu4x::UtcOffset>(icu4x::UtcOffset::FromFFI(c_struct.daylight)),
  };
}


#endif // icu4x_VariantOffsets_HPP
