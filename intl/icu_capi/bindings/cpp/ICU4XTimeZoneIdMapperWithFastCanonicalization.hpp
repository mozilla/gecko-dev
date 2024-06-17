#ifndef ICU4XTimeZoneIdMapperWithFastCanonicalization_HPP
#define ICU4XTimeZoneIdMapperWithFastCanonicalization_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XTimeZoneIdMapperWithFastCanonicalization.h"

class ICU4XDataProvider;
class ICU4XTimeZoneIdMapperWithFastCanonicalization;
#include "ICU4XError.hpp"

/**
 * A destruction policy for using ICU4XTimeZoneIdMapperWithFastCanonicalization with std::unique_ptr.
 */
struct ICU4XTimeZoneIdMapperWithFastCanonicalizationDeleter {
  void operator()(capi::ICU4XTimeZoneIdMapperWithFastCanonicalization* l) const noexcept {
    capi::ICU4XTimeZoneIdMapperWithFastCanonicalization_destroy(l);
  }
};

/**
 * A mapper between IANA time zone identifiers and BCP-47 time zone identifiers.
 * 
 * This mapper supports two-way mapping, but it is optimized for the case of IANA to BCP-47.
 * It also supports normalizing and canonicalizing the IANA strings.
 * 
 * See the [Rust documentation for `TimeZoneIdMapperWithFastCanonicalization`](https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneIdMapperWithFastCanonicalization.html) for more information.
 */
class ICU4XTimeZoneIdMapperWithFastCanonicalization {
 public:

  /**
   * See the [Rust documentation for `new`](https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneIdMapperWithFastCanonicalization.html#method.new) for more information.
   */
  static diplomat::result<ICU4XTimeZoneIdMapperWithFastCanonicalization, ICU4XError> create(const ICU4XDataProvider& provider);

  /**
   * See the [Rust documentation for `canonicalize_iana`](https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneIdMapperWithFastCanonicalizationBorrowed.html#method.canonicalize_iana) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> canonicalize_iana_to_writeable(const std::string_view value, W& write) const;

  /**
   * See the [Rust documentation for `canonicalize_iana`](https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneIdMapperWithFastCanonicalizationBorrowed.html#method.canonicalize_iana) for more information.
   */
  diplomat::result<std::string, ICU4XError> canonicalize_iana(const std::string_view value) const;

  /**
   * See the [Rust documentation for `canonical_iana_from_bcp47`](https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneIdMapperWithFastCanonicalizationBorrowed.html#method.canonical_iana_from_bcp47) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> canonical_iana_from_bcp47_to_writeable(const std::string_view value, W& write) const;

  /**
   * See the [Rust documentation for `canonical_iana_from_bcp47`](https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneIdMapperWithFastCanonicalizationBorrowed.html#method.canonical_iana_from_bcp47) for more information.
   */
  diplomat::result<std::string, ICU4XError> canonical_iana_from_bcp47(const std::string_view value) const;
  inline const capi::ICU4XTimeZoneIdMapperWithFastCanonicalization* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XTimeZoneIdMapperWithFastCanonicalization* AsFFIMut() { return this->inner.get(); }
  inline explicit ICU4XTimeZoneIdMapperWithFastCanonicalization(capi::ICU4XTimeZoneIdMapperWithFastCanonicalization* i) : inner(i) {}
  ICU4XTimeZoneIdMapperWithFastCanonicalization() = default;
  ICU4XTimeZoneIdMapperWithFastCanonicalization(ICU4XTimeZoneIdMapperWithFastCanonicalization&&) noexcept = default;
  ICU4XTimeZoneIdMapperWithFastCanonicalization& operator=(ICU4XTimeZoneIdMapperWithFastCanonicalization&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XTimeZoneIdMapperWithFastCanonicalization, ICU4XTimeZoneIdMapperWithFastCanonicalizationDeleter> inner;
};

#include "ICU4XDataProvider.hpp"

inline diplomat::result<ICU4XTimeZoneIdMapperWithFastCanonicalization, ICU4XError> ICU4XTimeZoneIdMapperWithFastCanonicalization::create(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XTimeZoneIdMapperWithFastCanonicalization_create(provider.AsFFI());
  diplomat::result<ICU4XTimeZoneIdMapperWithFastCanonicalization, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XTimeZoneIdMapperWithFastCanonicalization>(ICU4XTimeZoneIdMapperWithFastCanonicalization(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XTimeZoneIdMapperWithFastCanonicalization::canonicalize_iana_to_writeable(const std::string_view value, W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XTimeZoneIdMapperWithFastCanonicalization_canonicalize_iana(this->inner.get(), value.data(), value.size(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<std::monostate>(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XTimeZoneIdMapperWithFastCanonicalization::canonicalize_iana(const std::string_view value) const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XTimeZoneIdMapperWithFastCanonicalization_canonicalize_iana(this->inner.get(), value.data(), value.size(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<std::monostate>(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XTimeZoneIdMapperWithFastCanonicalization::canonical_iana_from_bcp47_to_writeable(const std::string_view value, W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XTimeZoneIdMapperWithFastCanonicalization_canonical_iana_from_bcp47(this->inner.get(), value.data(), value.size(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<std::monostate>(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XTimeZoneIdMapperWithFastCanonicalization::canonical_iana_from_bcp47(const std::string_view value) const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XTimeZoneIdMapperWithFastCanonicalization_canonical_iana_from_bcp47(this->inner.get(), value.data(), value.size(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<std::monostate>(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
#endif
