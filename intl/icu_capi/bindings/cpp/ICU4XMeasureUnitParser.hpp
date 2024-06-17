#ifndef ICU4XMeasureUnitParser_HPP
#define ICU4XMeasureUnitParser_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XMeasureUnitParser.h"

class ICU4XMeasureUnit;
#include "ICU4XError.hpp"

/**
 * A destruction policy for using ICU4XMeasureUnitParser with std::unique_ptr.
 */
struct ICU4XMeasureUnitParserDeleter {
  void operator()(capi::ICU4XMeasureUnitParser* l) const noexcept {
    capi::ICU4XMeasureUnitParser_destroy(l);
  }
};

/**
 * An ICU4X Measurement Unit parser object which is capable of parsing the CLDR unit identifier
 * (e.g. `meter-per-square-second`) and get the [`ICU4XMeasureUnit`].
 * 
 * See the [Rust documentation for `MeasureUnitParser`](https://docs.rs/icu/latest/icu/experimental/units/measureunit/struct.MeasureUnitParser.html) for more information.
 */
class ICU4XMeasureUnitParser {
 public:

  /**
   * Parses the CLDR unit identifier (e.g. `meter-per-square-second`) and returns the corresponding [`ICU4XMeasureUnit`].
   * Returns an error if the unit identifier is not valid.
   * 
   * See the [Rust documentation for `parse`](https://docs.rs/icu/latest/icu/experimental/units/measureunit/struct.MeasureUnitParser.html#method.parse) for more information.
   */
  diplomat::result<ICU4XMeasureUnit, ICU4XError> parse(const std::string_view unit_id) const;
  inline const capi::ICU4XMeasureUnitParser* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XMeasureUnitParser* AsFFIMut() { return this->inner.get(); }
  inline explicit ICU4XMeasureUnitParser(capi::ICU4XMeasureUnitParser* i) : inner(i) {}
  ICU4XMeasureUnitParser() = default;
  ICU4XMeasureUnitParser(ICU4XMeasureUnitParser&&) noexcept = default;
  ICU4XMeasureUnitParser& operator=(ICU4XMeasureUnitParser&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XMeasureUnitParser, ICU4XMeasureUnitParserDeleter> inner;
};

#include "ICU4XMeasureUnit.hpp"

inline diplomat::result<ICU4XMeasureUnit, ICU4XError> ICU4XMeasureUnitParser::parse(const std::string_view unit_id) const {
  auto diplomat_result_raw_out_value = capi::ICU4XMeasureUnitParser_parse(this->inner.get(), unit_id.data(), unit_id.size());
  diplomat::result<ICU4XMeasureUnit, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XMeasureUnit>(ICU4XMeasureUnit(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
#endif
