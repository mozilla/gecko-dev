#ifndef icu4x_VerticalOrientation_D_HPP
#define icu4x_VerticalOrientation_D_HPP

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
class VerticalOrientation;
}


namespace icu4x {
namespace capi {
    enum VerticalOrientation {
      VerticalOrientation_Rotated = 0,
      VerticalOrientation_TransformedRotated = 1,
      VerticalOrientation_TransformedUpright = 2,
      VerticalOrientation_Upright = 3,
    };

    typedef struct VerticalOrientation_option {union { VerticalOrientation ok; }; bool is_ok; } VerticalOrientation_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `VerticalOrientation`](https://docs.rs/icu/latest/icu/properties/props/struct.VerticalOrientation.html) for more information.
 */
class VerticalOrientation {
public:
  enum Value {
    Rotated = 0,
    TransformedRotated = 1,
    TransformedUpright = 2,
    Upright = 3,
  };

  VerticalOrientation() = default;
  // Implicit conversions between enum and ::Value
  constexpr VerticalOrientation(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  /**
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/latest/icu/properties/props/trait.EnumeratedProperty.html#tymethod.for_char) for more information.
   */
  inline static icu4x::VerticalOrientation for_char(char32_t ch);

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
   * See the [Rust documentation for `to_icu4c_value`](https://docs.rs/icu/latest/icu/properties/props/struct.VerticalOrientation.html#method.to_icu4c_value) for more information.
   */
  inline uint8_t to_integer_value() const;

  /**
   * Convert from an integer value from ICU4C or CodePointMapData
   *
   * See the [Rust documentation for `from_icu4c_value`](https://docs.rs/icu/latest/icu/properties/props/struct.VerticalOrientation.html#method.from_icu4c_value) for more information.
   */
  inline static std::optional<icu4x::VerticalOrientation> from_integer_value(uint8_t other);

  inline icu4x::capi::VerticalOrientation AsFFI() const;
  inline static icu4x::VerticalOrientation FromFFI(icu4x::capi::VerticalOrientation c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_VerticalOrientation_D_HPP
