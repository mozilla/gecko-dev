#ifndef ICU4XUnitsConverterFactory_HPP
#define ICU4XUnitsConverterFactory_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XUnitsConverterFactory.h"

class ICU4XDataProvider;
class ICU4XUnitsConverterFactory;
#include "ICU4XError.hpp"
class ICU4XMeasureUnit;
class ICU4XUnitsConverter;
class ICU4XMeasureUnitParser;

/**
 * A destruction policy for using ICU4XUnitsConverterFactory with std::unique_ptr.
 */
struct ICU4XUnitsConverterFactoryDeleter {
  void operator()(capi::ICU4XUnitsConverterFactory* l) const noexcept {
    capi::ICU4XUnitsConverterFactory_destroy(l);
  }
};

/**
 * An ICU4X Units Converter Factory object, capable of creating converters a [`ICU4XUnitsConverter`]
 * for converting between two [`ICU4XMeasureUnit`]s.
 * Also, it can parse the CLDR unit identifier (e.g. `meter-per-square-second`) and get the [`ICU4XMeasureUnit`].
 * 
 * See the [Rust documentation for `ConverterFactory`](https://docs.rs/icu/latest/icu/experimental/units/converter_factory/struct.ConverterFactory.html) for more information.
 */
class ICU4XUnitsConverterFactory {
 public:

  /**
   * Construct a new [`ICU4XUnitsConverterFactory`] instance.
   * 
   * See the [Rust documentation for `new`](https://docs.rs/icu/latest/icu/experimental/units/converter_factory/struct.ConverterFactory.html#method.new) for more information.
   */
  static diplomat::result<ICU4XUnitsConverterFactory, ICU4XError> create(const ICU4XDataProvider& provider);

  /**
   * Creates a new [`ICU4XUnitsConverter`] from the input and output [`ICU4XMeasureUnit`]s.
   * Returns nothing if the conversion between the two units is not possible.
   * For example, conversion between `meter` and `second` is not possible.
   * 
   * See the [Rust documentation for `converter`](https://docs.rs/icu/latest/icu/experimental/units/converter_factory/struct.ConverterFactory.html#method.converter) for more information.
   */
  std::optional<ICU4XUnitsConverter> converter(const ICU4XMeasureUnit& from, const ICU4XMeasureUnit& to) const;

  /**
   * Creates a parser to parse the CLDR unit identifier (e.g. `meter-per-square-second`) and get the [`ICU4XMeasureUnit`].
   * 
   * See the [Rust documentation for `parser`](https://docs.rs/icu/latest/icu/experimental/units/converter_factory/struct.ConverterFactory.html#method.parser) for more information.
   * 
   * Lifetimes: `this` must live at least as long as the output.
   */
  ICU4XMeasureUnitParser parser() const;
  inline const capi::ICU4XUnitsConverterFactory* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XUnitsConverterFactory* AsFFIMut() { return this->inner.get(); }
  inline explicit ICU4XUnitsConverterFactory(capi::ICU4XUnitsConverterFactory* i) : inner(i) {}
  ICU4XUnitsConverterFactory() = default;
  ICU4XUnitsConverterFactory(ICU4XUnitsConverterFactory&&) noexcept = default;
  ICU4XUnitsConverterFactory& operator=(ICU4XUnitsConverterFactory&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XUnitsConverterFactory, ICU4XUnitsConverterFactoryDeleter> inner;
};

#include "ICU4XDataProvider.hpp"
#include "ICU4XMeasureUnit.hpp"
#include "ICU4XUnitsConverter.hpp"
#include "ICU4XMeasureUnitParser.hpp"

inline diplomat::result<ICU4XUnitsConverterFactory, ICU4XError> ICU4XUnitsConverterFactory::create(const ICU4XDataProvider& provider) {
  auto diplomat_result_raw_out_value = capi::ICU4XUnitsConverterFactory_create(provider.AsFFI());
  diplomat::result<ICU4XUnitsConverterFactory, ICU4XError> diplomat_result_out_value;
  if (diplomat_result_raw_out_value.is_ok) {
    diplomat_result_out_value = diplomat::Ok<ICU4XUnitsConverterFactory>(ICU4XUnitsConverterFactory(diplomat_result_raw_out_value.ok));
  } else {
    diplomat_result_out_value = diplomat::Err<ICU4XError>(static_cast<ICU4XError>(diplomat_result_raw_out_value.err));
  }
  return diplomat_result_out_value;
}
inline std::optional<ICU4XUnitsConverter> ICU4XUnitsConverterFactory::converter(const ICU4XMeasureUnit& from, const ICU4XMeasureUnit& to) const {
  auto diplomat_optional_raw_out_value = capi::ICU4XUnitsConverterFactory_converter(this->inner.get(), from.AsFFI(), to.AsFFI());
  std::optional<ICU4XUnitsConverter> diplomat_optional_out_value;
  if (diplomat_optional_raw_out_value != nullptr) {
    diplomat_optional_out_value = ICU4XUnitsConverter(diplomat_optional_raw_out_value);
  } else {
    diplomat_optional_out_value = std::nullopt;
  }
  return diplomat_optional_out_value;
}
inline ICU4XMeasureUnitParser ICU4XUnitsConverterFactory::parser() const {
  return ICU4XMeasureUnitParser(capi::ICU4XUnitsConverterFactory_parser(this->inner.get()));
}
#endif
