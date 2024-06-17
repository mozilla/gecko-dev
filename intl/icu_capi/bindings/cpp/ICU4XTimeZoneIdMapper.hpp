#ifndef ICU4XTimeZoneIdMapper_HPP
#define ICU4XTimeZoneIdMapper_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XTimeZoneIdMapper.h"

class ICU4XDataProvider;
class ICU4XTimeZoneIdMapper;
#include "ICU4XError.hpp"

/**
 * A destruction policy for using ICU4XTimeZoneIdMapper with std::unique_ptr.
 */
struct ICU4XTimeZoneIdMapperDeleter {
  void operator()(capi::ICU4XTimeZoneIdMapper* l) const noexcept {
    capi::ICU4XTimeZoneIdMapper_destroy(l);
  }
};

/**
 * A mapper between IANA time zone identifiers and BCP-47 time zone identifiers.
 * 
 * This mapper supports two-way mapping, but it is optimized for the case of IANA to BCP-47.
 * It also supports normalizing and canonicalizing the IANA strings.
 * 
 * See the [Rust documentation for `TimeZoneIdMapper`](https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneIdMapper.html) for more information.
 */
class ICU4XTimeZoneIdMapper {
 public:

  /**
   * See the [Rust documentation for `new`](https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneIdMapper.html#method.new) for more information.
   */
  static diplomat::result<ICU4XTimeZoneIdMapper, ICU4XError> create(const ICU4XDataProvider& provider);

  /**
   * See the [Rust documentation for `iana_to_bcp47`](https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneIdMapperBorrowed.html#method.iana_to_bcp47) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> iana_to_bcp47_to_writeable(const std::string_view value, W& write) const;

  /**
   * See the [Rust documentation for `iana_to_bcp47`](https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneIdMapperBorrowed.html#method.iana_to_bcp47) for more information.
   */
  diplomat::result<std::string, ICU4XError> iana_to_bcp47(const std::string_view value) const;

  /**
   * See the [Rust documentation for `normalize_iana`](https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneIdMapperBorrowed.html#method.normalize_iana) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> normalize_iana_to_writeable(const std::string_view value, W& write) const;

  /**
   * See the [Rust documentation for `normalize_iana`](https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneIdMapperBorrowed.html#method.normalize_iana) for more information.
   */
  diplomat::result<std::string, ICU4XError> normalize_iana(const std::string_view value) const;

  /**
   * See the [Rust documentation for `canonicalize_iana`](https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneIdMapperBorrowed.html#method.canonicalize_iana) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> canonicalize_iana_to_writeable(const std::string_view value, W& write) const;

  /**
   * See the [Rust documentation for `canonicalize_iana`](https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneIdMapperBorrowed.html#method.canonicalize_iana) for more information.
   */
  diplomat::result<std::string, ICU4XError> canonicalize_iana(const std::string_view value) const;

  /**
   * See the [Rust documentation for `find_canonical_iana_from_bcp47`](https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneIdMapperBorrowed.html#method.find_canonical_iana_from_bcp47) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> find_canonical_iana_from_bcp47_to_writeable(const std::string_view value, W& write) const;

  /**
   * See the [Rust documentation for `find_canonical_iana_from_bcp47`](https://docs.rs/icu/latest/icu/timezone/struct.TimeZoneIdMapperBorrowed.html#method.find_canonical_iana_from_bcp47) for more information.
   */
  diplomat::result<std::string, ICU4XError> find_canonical_iana_from_bcp47(const std::string_view value) const;
  inline const capi::ICU4XTimeZoneIdMapper* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XTimeZoneIdMapper* AsFFIMut() { return this->inner.get(); }
  inline explicit ICU4XTimeZoneIdMapper(capi::ICU4XTimeZoneIdMapper* i) : inner(i) {}
  ICU4XTimeZoneIdMapper() = default;
  ICU4XTimeZoneIdMapper(ICU4XTimeZoneIdMapper&&) noexcept = default;
  ICU4XTimeZoneIdMapper& operator=(ICU4XTimeZoneIdMapper&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XTimeZoneIdMapper, ICU4XTimeZoneIdMapperDeleter> inner;
};

#include "ICU4XDataProvider.hpp"

inline diplomat::result<ICU4XTimeZoneIdMapper, ICU4XError> ICU4XTimeZoneIdMapper::create(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XTimeZoneIdMapper_create(provider.AsFFI());
  diplomat::result<ICU4XTimeZoneIdMapper, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XTimeZoneIdMapper>(ICU4XTimeZoneIdMapper(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XTimeZoneIdMapper::iana_to_bcp47_to_writeable(const std::string_view value, W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XTimeZoneIdMapper_iana_to_bcp47(this->inner.get(), value.data(), value.size(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<std::monostate>(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XTimeZoneIdMapper::iana_to_bcp47(const std::string_view value) const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XTimeZoneIdMapper_iana_to_bcp47(this->inner.get(), value.data(), value.size(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<std::monostate>(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XTimeZoneIdMapper::normalize_iana_to_writeable(const std::string_view value, W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XTimeZoneIdMapper_normalize_iana(this->inner.get(), value.data(), value.size(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<std::monostate>(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XTimeZoneIdMapper::normalize_iana(const std::string_view value) const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XTimeZoneIdMapper_normalize_iana(this->inner.get(), value.data(), value.size(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<std::monostate>(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XTimeZoneIdMapper::canonicalize_iana_to_writeable(const std::string_view value, W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XTimeZoneIdMapper_canonicalize_iana(this->inner.get(), value.data(), value.size(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<std::monostate>(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XTimeZoneIdMapper::canonicalize_iana(const std::string_view value) const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XTimeZoneIdMapper_canonicalize_iana(this->inner.get(), value.data(), value.size(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<std::monostate>(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XTimeZoneIdMapper::find_canonical_iana_from_bcp47_to_writeable(const std::string_view value, W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XTimeZoneIdMapper_find_canonical_iana_from_bcp47(this->inner.get(), value.data(), value.size(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<std::monostate>(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XTimeZoneIdMapper::find_canonical_iana_from_bcp47(const std::string_view value) const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XTimeZoneIdMapper_find_canonical_iana_from_bcp47(this->inner.get(), value.data(), value.size(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<std::monostate>(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
#endif
