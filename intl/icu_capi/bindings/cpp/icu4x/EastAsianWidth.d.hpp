#ifndef icu4x_EastAsianWidth_D_HPP
#define icu4x_EastAsianWidth_D_HPP

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
class EastAsianWidth;
}


namespace icu4x {
namespace capi {
    enum EastAsianWidth {
      EastAsianWidth_Neutral = 0,
      EastAsianWidth_Ambiguous = 1,
      EastAsianWidth_Halfwidth = 2,
      EastAsianWidth_Fullwidth = 3,
      EastAsianWidth_Narrow = 4,
      EastAsianWidth_Wide = 5,
    };

    typedef struct EastAsianWidth_option {union { EastAsianWidth ok; }; bool is_ok; } EastAsianWidth_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `EastAsianWidth`](https://docs.rs/icu/latest/icu/properties/props/struct.EastAsianWidth.html) for more information.
 */
class EastAsianWidth {
public:
  enum Value {
    Neutral = 0,
    Ambiguous = 1,
    Halfwidth = 2,
    Fullwidth = 3,
    Narrow = 4,
    Wide = 5,
  };

  EastAsianWidth() = default;
  // Implicit conversions between enum and ::Value
  constexpr EastAsianWidth(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  /**
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/latest/icu/properties/props/trait.EnumeratedProperty.html#tymethod.for_char) for more information.
   */
  inline static icu4x::EastAsianWidth for_char(char32_t ch);

  /**
   * Get the "long" name of this property value (returns empty if property value is unknown)
   *
   * See the [Rust documentation for `get`](https://docs.rs/icu/latest/icu/properties/struct.PropertyNamesLongBorrowed.html#method.get) for more information.
   */
  inline std::optional<std::string_view> long_name() const;

  /**
   * Get the "short" name of this property value (returns empty if property value is unknown)
   *
   * See the [Rust documentation for `get`](https://docs.rs/icu/latest/icu/properties/struct.PropertyNamesShortBorrowed.html#method.get) for more information.
   */
  inline std::optional<std::string_view> short_name() const;

  /**
   * Convert to an integer value usable with ICU4C and CodePointMapData
   *
   * See the [Rust documentation for `to_icu4c_value`](https://docs.rs/icu/latest/icu/properties/props/struct.EastAsianWidth.html#method.to_icu4c_value) for more information.
   */
  inline uint8_t to_integer_value() const;

  /**
   * Convert from an integer value from ICU4C or CodePointMapData
   *
   * See the [Rust documentation for `from_icu4c_value`](https://docs.rs/icu/latest/icu/properties/props/struct.EastAsianWidth.html#method.from_icu4c_value) for more information.
   */
  inline static std::optional<icu4x::EastAsianWidth> from_integer_value(uint8_t other);

  inline icu4x::capi::EastAsianWidth AsFFI() const;
  inline static icu4x::EastAsianWidth FromFFI(icu4x::capi::EastAsianWidth c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_EastAsianWidth_D_HPP
