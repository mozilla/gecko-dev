#ifndef ICU4XIanaToBcp47Mapper_HPP
#define ICU4XIanaToBcp47Mapper_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XIanaToBcp47Mapper.h"

class ICU4XDataProvider;
class ICU4XIanaToBcp47Mapper;
#include "ICU4XError.hpp"

/**
 * A destruction policy for using ICU4XIanaToBcp47Mapper with std::unique_ptr.
 */
struct ICU4XIanaToBcp47MapperDeleter {
  void operator()(capi::ICU4XIanaToBcp47Mapper* l) const noexcept {
    capi::ICU4XIanaToBcp47Mapper_destroy(l);
  }
};

/**
 * An object capable of mapping from an IANA time zone ID to a BCP-47 ID.
 * 
 * This can be used via `try_set_iana_time_zone_id()` on [`ICU4XCustomTimeZone`].
 * 
 * [`ICU4XCustomTimeZone`]: crate::timezone::ffi::ICU4XCustomTimeZone
 * 
 * See the [Rust documentation for `IanaToBcp47Mapper`](https://docs.rs/icu/latest/icu/timezone/struct.IanaToBcp47Mapper.html) for more information.
 */
class ICU4XIanaToBcp47Mapper {
 public:

  /**
   * See the [Rust documentation for `new`](https://docs.rs/icu/latest/icu/timezone/struct.IanaToBcp47Mapper.html#method.new) for more information.
   */
  static diplomat::result<ICU4XIanaToBcp47Mapper, ICU4XError> create(const ICU4XDataProvider& provider);

  /**
   * See the [Rust documentation for `get`](https://docs.rs/icu/latest/icu/timezone/struct.IanaToBcp47MapperBorrowed.html#method.get) for more information.
   * 
   * See the [Rust documentation for `iana_to_bcp47`](https://docs.rs/icu/latest/icu/timezone/struct.IanaBcp47RoundTripMapperBorrowed.html#method.iana_to_bcp47) for more information.
   */
  template<typename W> diplomat::result<std::monostate, ICU4XError> get_to_writeable(const std::string_view value, W& write) const;

  /**
   * See the [Rust documentation for `get`](https://docs.rs/icu/latest/icu/timezone/struct.IanaToBcp47MapperBorrowed.html#method.get) for more information.
   * 
   * See the [Rust documentation for `iana_to_bcp47`](https://docs.rs/icu/latest/icu/timezone/struct.IanaBcp47RoundTripMapperBorrowed.html#method.iana_to_bcp47) for more information.
   */
  diplomat::result<std::string, ICU4XError> get(const std::string_view value) const;
  inline const capi::ICU4XIanaToBcp47Mapper* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XIanaToBcp47Mapper* AsFFIMut() { return this->inner.get(); }
  inline explicit ICU4XIanaToBcp47Mapper(capi::ICU4XIanaToBcp47Mapper* i) : inner(i) {}
  ICU4XIanaToBcp47Mapper() = default;
  ICU4XIanaToBcp47Mapper(ICU4XIanaToBcp47Mapper&&) noexcept = default;
  ICU4XIanaToBcp47Mapper& operator=(ICU4XIanaToBcp47Mapper&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XIanaToBcp47Mapper, ICU4XIanaToBcp47MapperDeleter> inner;
};

#include "ICU4XDataProvider.hpp"

inline diplomat::result<ICU4XIanaToBcp47Mapper, ICU4XError> ICU4XIanaToBcp47Mapper::create(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XIanaToBcp47Mapper_create(provider.AsFFI());
  diplomat::result<ICU4XIanaToBcp47Mapper, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XIanaToBcp47Mapper>(ICU4XIanaToBcp47Mapper(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
template<typename W> inline diplomat::result<std::monostate, ICU4XError> ICU4XIanaToBcp47Mapper::get_to_writeable(const std::string_view value, W& write) const {
  capi::DiplomatWriteable write_writer = diplomat::WriteableTrait<W>::Construct(write);
  auto diplomat_result_raw_out_value = capi::ICU4XIanaToBcp47Mapper_get(this->inner.get(), value.data(), value.size(), &write_writer);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<std::monostate>(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline diplomat::result<std::string, ICU4XError> ICU4XIanaToBcp47Mapper::get(const std::string_view value) const {
  std::string diplomat_writeable_string;
  capi::DiplomatWriteable diplomat_writeable_out = diplomat::WriteableFromString(diplomat_writeable_string);
  auto diplomat_result_raw_out_value = capi::ICU4XIanaToBcp47Mapper_get(this->inner.get(), value.data(), value.size(), &diplomat_writeable_out);
  diplomat::result<std::monostate, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<std::monostate>(std::monostate());
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value.replace_ok(std::move(diplomat_writeable_string));
}
#endif
