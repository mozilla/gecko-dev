#ifndef icu4x_SentenceBreak_D_HPP
#define icu4x_SentenceBreak_D_HPP

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
class SentenceBreak;
}


namespace icu4x {
namespace capi {
    enum SentenceBreak {
      SentenceBreak_Other = 0,
      SentenceBreak_ATerm = 1,
      SentenceBreak_Close = 2,
      SentenceBreak_Format = 3,
      SentenceBreak_Lower = 4,
      SentenceBreak_Numeric = 5,
      SentenceBreak_OLetter = 6,
      SentenceBreak_Sep = 7,
      SentenceBreak_Sp = 8,
      SentenceBreak_STerm = 9,
      SentenceBreak_Upper = 10,
      SentenceBreak_CR = 11,
      SentenceBreak_Extend = 12,
      SentenceBreak_LF = 13,
      SentenceBreak_SContinue = 14,
    };

    typedef struct SentenceBreak_option {union { SentenceBreak ok; }; bool is_ok; } SentenceBreak_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `SentenceBreak`](https://docs.rs/icu/latest/icu/properties/props/struct.SentenceBreak.html) for more information.
 */
class SentenceBreak {
public:
  enum Value {
    Other = 0,
    ATerm = 1,
    Close = 2,
    Format = 3,
    Lower = 4,
    Numeric = 5,
    OLetter = 6,
    Sep = 7,
    Sp = 8,
    STerm = 9,
    Upper = 10,
    CR = 11,
    Extend = 12,
    LF = 13,
    SContinue = 14,
  };

  SentenceBreak() = default;
  // Implicit conversions between enum and ::Value
  constexpr SentenceBreak(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  /**
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/latest/icu/properties/props/trait.EnumeratedProperty.html#tymethod.for_char) for more information.
   */
  inline static icu4x::SentenceBreak for_char(char32_t ch);

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
   * See the [Rust documentation for `to_icu4c_value`](https://docs.rs/icu/latest/icu/properties/props/struct.SentenceBreak.html#method.to_icu4c_value) for more information.
   */
  inline uint8_t to_integer_value() const;

  /**
   * Convert from an integer value from ICU4C or CodePointMapData
   *
   * See the [Rust documentation for `from_icu4c_value`](https://docs.rs/icu/latest/icu/properties/props/struct.SentenceBreak.html#method.from_icu4c_value) for more information.
   */
  inline static std::optional<icu4x::SentenceBreak> from_integer_value(uint8_t other);

  inline icu4x::capi::SentenceBreak AsFFI() const;
  inline static icu4x::SentenceBreak FromFFI(icu4x::capi::SentenceBreak c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_SentenceBreak_D_HPP
