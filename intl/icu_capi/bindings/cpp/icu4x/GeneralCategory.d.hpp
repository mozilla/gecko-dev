#ifndef icu4x_GeneralCategory_D_HPP
#define icu4x_GeneralCategory_D_HPP

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
struct GeneralCategoryGroup;
class GeneralCategory;
}


namespace icu4x {
namespace capi {
    enum GeneralCategory {
      GeneralCategory_Unassigned = 0,
      GeneralCategory_UppercaseLetter = 1,
      GeneralCategory_LowercaseLetter = 2,
      GeneralCategory_TitlecaseLetter = 3,
      GeneralCategory_ModifierLetter = 4,
      GeneralCategory_OtherLetter = 5,
      GeneralCategory_NonspacingMark = 6,
      GeneralCategory_SpacingMark = 8,
      GeneralCategory_EnclosingMark = 7,
      GeneralCategory_DecimalNumber = 9,
      GeneralCategory_LetterNumber = 10,
      GeneralCategory_OtherNumber = 11,
      GeneralCategory_SpaceSeparator = 12,
      GeneralCategory_LineSeparator = 13,
      GeneralCategory_ParagraphSeparator = 14,
      GeneralCategory_Control = 15,
      GeneralCategory_Format = 16,
      GeneralCategory_PrivateUse = 17,
      GeneralCategory_Surrogate = 18,
      GeneralCategory_DashPunctuation = 19,
      GeneralCategory_OpenPunctuation = 20,
      GeneralCategory_ClosePunctuation = 21,
      GeneralCategory_ConnectorPunctuation = 22,
      GeneralCategory_InitialPunctuation = 28,
      GeneralCategory_FinalPunctuation = 29,
      GeneralCategory_OtherPunctuation = 23,
      GeneralCategory_MathSymbol = 24,
      GeneralCategory_CurrencySymbol = 25,
      GeneralCategory_ModifierSymbol = 26,
      GeneralCategory_OtherSymbol = 27,
    };

    typedef struct GeneralCategory_option {union { GeneralCategory ok; }; bool is_ok; } GeneralCategory_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `GeneralCategory`](https://docs.rs/icu/latest/icu/properties/props/enum.GeneralCategory.html) for more information.
 */
class GeneralCategory {
public:
  enum Value {
    Unassigned = 0,
    UppercaseLetter = 1,
    LowercaseLetter = 2,
    TitlecaseLetter = 3,
    ModifierLetter = 4,
    OtherLetter = 5,
    NonspacingMark = 6,
    SpacingMark = 8,
    EnclosingMark = 7,
    DecimalNumber = 9,
    LetterNumber = 10,
    OtherNumber = 11,
    SpaceSeparator = 12,
    LineSeparator = 13,
    ParagraphSeparator = 14,
    Control = 15,
    Format = 16,
    PrivateUse = 17,
    Surrogate = 18,
    DashPunctuation = 19,
    OpenPunctuation = 20,
    ClosePunctuation = 21,
    ConnectorPunctuation = 22,
    InitialPunctuation = 28,
    FinalPunctuation = 29,
    OtherPunctuation = 23,
    MathSymbol = 24,
    CurrencySymbol = 25,
    ModifierSymbol = 26,
    OtherSymbol = 27,
  };

  GeneralCategory() = default;
  // Implicit conversions between enum and ::Value
  constexpr GeneralCategory(Value v) : value(v) {}
  constexpr operator Value() const { return value; }
  // Prevent usage as boolean value
  explicit operator bool() const = delete;

  /**
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/latest/icu/properties/props/trait.EnumeratedProperty.html#tymethod.for_char) for more information.
   */
  inline static icu4x::GeneralCategory for_char(char32_t ch);

  /**
   * Convert to an integer using the ICU4C integer mappings for `General_Category`
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
   */
  inline uint8_t to_integer_value() const;

  /**
   * Produces a GeneralCategoryGroup mask that can represent a group of general categories
   *
   * See the [Rust documentation for `GeneralCategoryGroup`](https://docs.rs/icu/latest/icu/properties/props/struct.GeneralCategoryGroup.html) for more information.
   */
  inline icu4x::GeneralCategoryGroup to_group() const;

  /**
   * Convert from an integer using the ICU4C integer mappings for `General_Category`
   * Convert from an integer value from ICU4C or CodePointMapData
   */
  inline static std::optional<icu4x::GeneralCategory> from_integer_value(uint8_t other);

  inline icu4x::capi::GeneralCategory AsFFI() const;
  inline static icu4x::GeneralCategory FromFFI(icu4x::capi::GeneralCategory c_enum);
private:
    Value value;
};

} // namespace
#endif // icu4x_GeneralCategory_D_HPP
