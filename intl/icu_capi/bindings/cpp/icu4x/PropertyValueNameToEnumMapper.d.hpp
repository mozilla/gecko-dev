#ifndef icu4x_PropertyValueNameToEnumMapper_D_HPP
#define icu4x_PropertyValueNameToEnumMapper_D_HPP

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
namespace capi { struct DataProvider; }
class DataProvider;
namespace capi { struct PropertyValueNameToEnumMapper; }
class PropertyValueNameToEnumMapper;
class DataError;
}


namespace icu4x {
namespace capi {
    struct PropertyValueNameToEnumMapper;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * A type capable of looking up a property value from a string name.
 *
 * See the [Rust documentation for `PropertyParser`](https://docs.rs/icu/latest/icu/properties/struct.PropertyParser.html) for more information.
 *
 * See the [Rust documentation for `PropertyParserBorrowed`](https://docs.rs/icu/latest/icu/properties/struct.PropertyParserBorrowed.html) for more information.
 *
 * See the [Rust documentation for `new`](https://docs.rs/icu/latest/icu/properties/struct.PropertyParser.html#method.new) for more information.
 */
class PropertyValueNameToEnumMapper {
public:

  /**
   * Get the property value matching the given name, using strict matching
   *
   * Returns -1 if the name is unknown for this property
   *
   * See the [Rust documentation for `get_strict`](https://docs.rs/icu/latest/icu/properties/struct.PropertyParserBorrowed.html#method.get_strict) for more information.
   */
  inline int16_t get_strict(std::string_view name) const;

  /**
   * Get the property value matching the given name, using loose matching
   *
   * Returns -1 if the name is unknown for this property
   *
   * See the [Rust documentation for `get_loose`](https://docs.rs/icu/latest/icu/properties/struct.PropertyParserBorrowed.html#method.get_loose) for more information.
   */
  inline int16_t get_loose(std::string_view name) const;

  /**
   * Create a name-to-enum mapper for the `General_Category` property, using compiled data.
   *
   * See the [Rust documentation for `GeneralCategory`](https://docs.rs/icu/latest/icu/properties/props/enum.GeneralCategory.html) for more information.
   */
  inline static std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> create_general_category();

  /**
   * Create a name-to-enum mapper for the `General_Category` property, using a particular data source.
   *
   * See the [Rust documentation for `GeneralCategory`](https://docs.rs/icu/latest/icu/properties/props/enum.GeneralCategory.html) for more information.
   */
  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> create_general_category_with_provider(const icu4x::DataProvider& provider);

  /**
   * Create a name-to-enum mapper for the `Hangul_Syllable_Type` property, using compiled data.
   *
   * See the [Rust documentation for `HangulSyllableType`](https://docs.rs/icu/latest/icu/properties/props/struct.HangulSyllableType.html) for more information.
   */
  inline static std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> create_hangul_syllable_type();

  /**
   * Create a name-to-enum mapper for the `Hangul_Syllable_Type` property, using a particular data source.
   *
   * See the [Rust documentation for `HangulSyllableType`](https://docs.rs/icu/latest/icu/properties/props/struct.HangulSyllableType.html) for more information.
   */
  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> create_hangul_syllable_type_with_provider(const icu4x::DataProvider& provider);

  /**
   * Create a name-to-enum mapper for the `East_Asian_Width` property, using compiled data.
   *
   * See the [Rust documentation for `EastAsianWidth`](https://docs.rs/icu/latest/icu/properties/props/struct.EastAsianWidth.html) for more information.
   */
  inline static std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> create_east_asian_width();

  /**
   * Create a name-to-enum mapper for the `East_Asian_Width` property, using a particular data source.
   *
   * See the [Rust documentation for `EastAsianWidth`](https://docs.rs/icu/latest/icu/properties/props/struct.EastAsianWidth.html) for more information.
   */
  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> create_east_asian_width_with_provider(const icu4x::DataProvider& provider);

  /**
   * Create a name-to-enum mapper for the `Bidi_Class` property, using compiled data.
   *
   * See the [Rust documentation for `BidiClass`](https://docs.rs/icu/latest/icu/properties/props/struct.BidiClass.html) for more information.
   */
  inline static std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> create_bidi_class();

  /**
   * Create a name-to-enum mapper for the `Bidi_Class` property, using a particular data source.
   *
   * See the [Rust documentation for `BidiClass`](https://docs.rs/icu/latest/icu/properties/props/struct.BidiClass.html) for more information.
   */
  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> create_bidi_class_with_provider(const icu4x::DataProvider& provider);

  /**
   * Create a name-to-enum mapper for the `Indic_Syllabic_Category` property, using compiled data.
   *
   * See the [Rust documentation for `IndicSyllabicCategory`](https://docs.rs/icu/latest/icu/properties/props/struct.IndicSyllabicCategory.html) for more information.
   */
  inline static std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> create_indic_syllabic_category();

  /**
   * Create a name-to-enum mapper for the `Indic_Syllabic_Category` property, using a particular data source.
   *
   * See the [Rust documentation for `IndicSyllabicCategory`](https://docs.rs/icu/latest/icu/properties/props/struct.IndicSyllabicCategory.html) for more information.
   */
  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> create_indic_syllabic_category_with_provider(const icu4x::DataProvider& provider);

  /**
   * Create a name-to-enum mapper for the `Line_Break` property, using compiled data.
   *
   * See the [Rust documentation for `LineBreak`](https://docs.rs/icu/latest/icu/properties/props/struct.LineBreak.html) for more information.
   */
  inline static std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> create_line_break();

  /**
   * Create a name-to-enum mapper for the `Line_Break` property, using a particular data source.
   *
   * See the [Rust documentation for `LineBreak`](https://docs.rs/icu/latest/icu/properties/props/struct.LineBreak.html) for more information.
   */
  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> create_line_break_with_provider(const icu4x::DataProvider& provider);

  /**
   * Create a name-to-enum mapper for the `Grapheme_Cluster_Break` property, using compiled data.
   *
   * See the [Rust documentation for `GraphemeClusterBreak`](https://docs.rs/icu/latest/icu/properties/props/struct.GraphemeClusterBreak.html) for more information.
   */
  inline static std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> create_grapheme_cluster_break();

  /**
   * Create a name-to-enum mapper for the `Grapheme_Cluster_Break` property, using a particular data source.
   *
   * See the [Rust documentation for `GraphemeClusterBreak`](https://docs.rs/icu/latest/icu/properties/props/struct.GraphemeClusterBreak.html) for more information.
   */
  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> create_grapheme_cluster_break_with_provider(const icu4x::DataProvider& provider);

  /**
   * Create a name-to-enum mapper for the `Word_Break` property, using compiled data.
   *
   * See the [Rust documentation for `WordBreak`](https://docs.rs/icu/latest/icu/properties/props/struct.WordBreak.html) for more information.
   */
  inline static std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> create_word_break();

  /**
   * Create a name-to-enum mapper for the `Word_Break` property, using a particular data source.
   *
   * See the [Rust documentation for `WordBreak`](https://docs.rs/icu/latest/icu/properties/props/struct.WordBreak.html) for more information.
   */
  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> create_word_break_with_provider(const icu4x::DataProvider& provider);

  /**
   * Create a name-to-enum mapper for the `Sentence_Break` property, using compiled data.
   *
   * See the [Rust documentation for `SentenceBreak`](https://docs.rs/icu/latest/icu/properties/props/struct.SentenceBreak.html) for more information.
   */
  inline static std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> create_sentence_break();

  /**
   * Create a name-to-enum mapper for the `Sentence_Break` property, using a particular data source.
   *
   * See the [Rust documentation for `SentenceBreak`](https://docs.rs/icu/latest/icu/properties/props/struct.SentenceBreak.html) for more information.
   */
  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> create_sentence_break_with_provider(const icu4x::DataProvider& provider);

  /**
   * Create a name-to-enum mapper for the `Script` property, using compiled data.
   *
   * See the [Rust documentation for `Script`](https://docs.rs/icu/latest/icu/properties/props/struct.Script.html) for more information.
   */
  inline static std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> create_script();

  /**
   * Create a name-to-enum mapper for the `Script` property, using a particular data source.
   *
   * See the [Rust documentation for `Script`](https://docs.rs/icu/latest/icu/properties/props/struct.Script.html) for more information.
   */
  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> create_script_with_provider(const icu4x::DataProvider& provider);

  /**
   * Create a name-to-enum mapper for the `Vertical_Orientation` property, using compiled data.
   *
   * See the [Rust documentation for `VerticalOrientation`](https://docs.rs/icu/latest/icu/properties/props/struct.VerticalOrientation.html) for more information.
   */
  inline static std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> create_vertical_orientation();

  /**
   * Create a name-to-enum mapper for the `Vertical_Orientation` property, using a particular data source.
   *
   * See the [Rust documentation for `VerticalOrientation`](https://docs.rs/icu/latest/icu/properties/props/struct.VerticalOrientation.html) for more information.
   */
  inline static diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> create_vertical_orientation_with_provider(const icu4x::DataProvider& provider);

  inline const icu4x::capi::PropertyValueNameToEnumMapper* AsFFI() const;
  inline icu4x::capi::PropertyValueNameToEnumMapper* AsFFI();
  inline static const icu4x::PropertyValueNameToEnumMapper* FromFFI(const icu4x::capi::PropertyValueNameToEnumMapper* ptr);
  inline static icu4x::PropertyValueNameToEnumMapper* FromFFI(icu4x::capi::PropertyValueNameToEnumMapper* ptr);
  inline static void operator delete(void* ptr);
private:
  PropertyValueNameToEnumMapper() = delete;
  PropertyValueNameToEnumMapper(const icu4x::PropertyValueNameToEnumMapper&) = delete;
  PropertyValueNameToEnumMapper(icu4x::PropertyValueNameToEnumMapper&&) noexcept = delete;
  PropertyValueNameToEnumMapper operator=(const icu4x::PropertyValueNameToEnumMapper&) = delete;
  PropertyValueNameToEnumMapper operator=(icu4x::PropertyValueNameToEnumMapper&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_PropertyValueNameToEnumMapper_D_HPP
