#ifndef icu4x_GraphemeClusterBreak_D_HPP
#define icu4x_GraphemeClusterBreak_D_HPP

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
class GraphemeClusterBreak;
}


namespace icu4x {
namespace capi {
    enum GraphemeClusterBreak {
      GraphemeClusterBreak_Other = 0,
      GraphemeClusterBreak_Control = 1,
      GraphemeClusterBreak_CR = 2,
      GraphemeClusterBreak_Extend = 3,
      GraphemeClusterBreak_L = 4,
      GraphemeClusterBreak_LF = 5,
      GraphemeClusterBreak_LV = 6,
      GraphemeClusterBreak_LVT = 7,
      GraphemeClusterBreak_T = 8,
      GraphemeClusterBreak_V = 9,
      GraphemeClusterBreak_SpacingMark = 10,
      GraphemeClusterBreak_Prepend = 11,
      GraphemeClusterBreak_RegionalIndicator = 12,
      GraphemeClusterBreak_EBase = 13,
      GraphemeClusterBreak_EBaseGAZ = 14,
      GraphemeClusterBreak_EModifier = 15,
      GraphemeClusterBreak_GlueAfterZwj = 16,
      GraphemeClusterBreak_ZWJ = 17,
    };

    typedef struct GraphemeClusterBreak_option {union { GraphemeClusterBreak ok; }; bool is_ok; } GraphemeClusterBreak_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `GraphemeClusterBreak`](https://docs.rs/icu/latest/icu/properties/props/struct.GraphemeClusterBreak.html) for more information.
 */
class GraphemeClusterBreak {
public:
  enum Value {
    Other = 0,
    Control = 1,
    CR = 2,
    Extend = 3,
    L = 4,
    LF = 5,
    LV = 6,
    LVT = 7,
    T = 8,
    V = 9,
    SpacingMark = 10,
    Prepend = 11,
    RegionalIndicator = 12,
    EBase = 13,
    EBaseGAZ = 14,
    EModifier = 15,
    GlueAfterZwj = 16,
    ZWJ = 17,
  };

  GraphemeClusterBreak() = default;
  // Implicit conversions between enum and ::Value
  constexpr GraphemeClusterBreak(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  /**
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/latest/icu/properties/props/trait.EnumeratedProperty.html#tymethod.for_char) for more information.
   */
  inline static icu4x::GraphemeClusterBreak for_char(char32_t ch);

  /**
   * Convert to an integer value usable with ICU4C and CodePointMapData
   *
   * See the [Rust documentation for `to_icu4c_value`](https://docs.rs/icu/latest/icu/properties/props/struct.GraphemeClusterBreak.html#method.to_icu4c_value) for more information.
   */
  inline uint8_t to_integer_value() const;

  /**
   * Convert from an integer value from ICU4C or CodePointMapData
   *
   * See the [Rust documentation for `from_icu4c_value`](https://docs.rs/icu/latest/icu/properties/props/struct.GraphemeClusterBreak.html#method.from_icu4c_value) for more information.
   */
  inline static std::optional<icu4x::GraphemeClusterBreak> from_integer_value(uint8_t other);

  inline icu4x::capi::GraphemeClusterBreak AsFFI() const;
  inline static icu4x::GraphemeClusterBreak FromFFI(icu4x::capi::GraphemeClusterBreak c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_GraphemeClusterBreak_D_HPP
