#ifndef icu4x_Locale_D_HPP
#define icu4x_Locale_D_HPP

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
namespace capi { struct Locale; }
class Locale;
class LocaleParseError;
}


namespace icu4x {
namespace capi {
    struct Locale;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * An ICU4X Locale, capable of representing strings like `"en-US"`.
 *
 * See the [Rust documentation for `Locale`](https://docs.rs/icu/latest/icu/locale/struct.Locale.html) for more information.
 */
class Locale {
public:

  /**
   * Construct an [`Locale`] from an locale identifier.
   *
   * This will run the complete locale parsing algorithm. If code size and
   * performance are critical and the locale is of a known shape (such as
   * `aa-BB`) use `create_und`, `set_language`, `set_script`, and `set_region`.
   *
   * See the [Rust documentation for `try_from_str`](https://docs.rs/icu/latest/icu/locale/struct.Locale.html#method.try_from_str) for more information.
   */
  inline static diplomat::result<std::unique_ptr<icu4x::Locale>, icu4x::LocaleParseError> from_string(std::string_view name);

  /**
   * Construct a unknown [`Locale`] "und".
   *
   * See the [Rust documentation for `UNKNOWN`](https://docs.rs/icu/latest/icu/locale/struct.Locale.html#associatedconstant.UNKNOWN) for more information.
   */
  inline static std::unique_ptr<icu4x::Locale> unknown();

  /**
   * Clones the [`Locale`].
   *
   * See the [Rust documentation for `Locale`](https://docs.rs/icu/latest/icu/locale/struct.Locale.html) for more information.
   */
  inline std::unique_ptr<icu4x::Locale> clone() const;

  /**
   * Returns a string representation of the `LanguageIdentifier` part of
   * [`Locale`].
   *
   * See the [Rust documentation for `id`](https://docs.rs/icu/latest/icu/locale/struct.Locale.html#structfield.id) for more information.
   */
  inline std::string basename() const;

  /**
   * Returns a string representation of the unicode extension.
   *
   * See the [Rust documentation for `extensions`](https://docs.rs/icu/latest/icu/locale/struct.Locale.html#structfield.extensions) for more information.
   */
  inline std::optional<std::string> get_unicode_extension(std::string_view s) const;

  /**
   * Returns a string representation of [`Locale`] language.
   *
   * See the [Rust documentation for `id`](https://docs.rs/icu/latest/icu/locale/struct.Locale.html#structfield.id) for more information.
   */
  inline std::string language() const;

  /**
   * Set the language part of the [`Locale`].
   *
   * See the [Rust documentation for `try_from_str`](https://docs.rs/icu/latest/icu/locale/struct.Locale.html#method.try_from_str) for more information.
   */
  inline diplomat::result<std::monostate, icu4x::LocaleParseError> set_language(std::string_view s);

  /**
   * Returns a string representation of [`Locale`] region.
   *
   * See the [Rust documentation for `id`](https://docs.rs/icu/latest/icu/locale/struct.Locale.html#structfield.id) for more information.
   */
  inline std::optional<std::string> region() const;

  /**
   * Set the region part of the [`Locale`].
   *
   * See the [Rust documentation for `try_from_str`](https://docs.rs/icu/latest/icu/locale/struct.Locale.html#method.try_from_str) for more information.
   */
  inline diplomat::result<std::monostate, icu4x::LocaleParseError> set_region(std::string_view s);

  /**
   * Returns a string representation of [`Locale`] script.
   *
   * See the [Rust documentation for `id`](https://docs.rs/icu/latest/icu/locale/struct.Locale.html#structfield.id) for more information.
   */
  inline std::optional<std::string> script() const;

  /**
   * Set the script part of the [`Locale`]. Pass an empty string to remove the script.
   *
   * See the [Rust documentation for `try_from_str`](https://docs.rs/icu/latest/icu/locale/struct.Locale.html#method.try_from_str) for more information.
   */
  inline diplomat::result<std::monostate, icu4x::LocaleParseError> set_script(std::string_view s);

  /**
   * Normalizes a locale string.
   *
   * See the [Rust documentation for `normalize`](https://docs.rs/icu/latest/icu/locale/struct.Locale.html#method.normalize) for more information.
   */
  inline static diplomat::result<std::string, icu4x::LocaleParseError> normalize(std::string_view s);

  /**
   * Returns a string representation of [`Locale`].
   *
   * See the [Rust documentation for `write_to`](https://docs.rs/icu/latest/icu/locale/struct.Locale.html#method.write_to) for more information.
   */
  inline std::string to_string() const;

  /**
   * See the [Rust documentation for `normalizing_eq`](https://docs.rs/icu/latest/icu/locale/struct.Locale.html#method.normalizing_eq) for more information.
   */
  inline bool normalizing_eq(std::string_view other) const;

  /**
   * See the [Rust documentation for `strict_cmp`](https://docs.rs/icu/latest/icu/locale/struct.Locale.html#method.strict_cmp) for more information.
   */
  inline int8_t compare_to_string(std::string_view other) const;

  /**
   * See the [Rust documentation for `total_cmp`](https://docs.rs/icu/latest/icu/locale/struct.Locale.html#method.total_cmp) for more information.
   */
  inline int8_t compare_to(const icu4x::Locale& other) const;
  inline bool operator==(const icu4x::Locale& other) const;
  inline bool operator!=(const icu4x::Locale& other) const;
  inline bool operator<=(const icu4x::Locale& other) const;
  inline bool operator>=(const icu4x::Locale& other) const;
  inline bool operator<(const icu4x::Locale& other) const;
  inline bool operator>(const icu4x::Locale& other) const;

  inline const icu4x::capi::Locale* AsFFI() const;
  inline icu4x::capi::Locale* AsFFI();
  inline static const icu4x::Locale* FromFFI(const icu4x::capi::Locale* ptr);
  inline static icu4x::Locale* FromFFI(icu4x::capi::Locale* ptr);
  inline static void operator delete(void* ptr);
private:
  Locale() = delete;
  Locale(const icu4x::Locale&) = delete;
  Locale(icu4x::Locale&&) noexcept = delete;
  Locale operator=(const icu4x::Locale&) = delete;
  Locale operator=(icu4x::Locale&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_Locale_D_HPP
