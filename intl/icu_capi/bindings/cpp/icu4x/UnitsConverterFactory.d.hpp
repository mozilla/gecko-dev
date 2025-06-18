#ifndef icu4x_UnitsConverterFactory_D_HPP
#define icu4x_UnitsConverterFactory_D_HPP

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
namespace capi { struct MeasureUnit; }
class MeasureUnit;
namespace capi { struct UnitsConverter; }
class UnitsConverter;
namespace capi { struct UnitsConverterFactory; }
class UnitsConverterFactory;
class DataError;
}


namespace icu4x {
namespace capi {
    struct UnitsConverterFactory;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * An ICU4X Units Converter Factory object, capable of creating converters a [`UnitsConverter`]
 * for converting between two [`MeasureUnit`]s.
 *
 * Also, it can parse the CLDR unit identifier (e.g. `meter-per-square-second`) and get the [`MeasureUnit`].
 *
 * See the [Rust documentation for `ConverterFactory`](https://docs.rs/icu/latest/icu/experimental/units/converter_factory/struct.ConverterFactory.html) for more information.
 */
class UnitsConverterFactory {
public:

  /**
   * Construct a new [`UnitsConverterFactory`] instance using compiled data.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/latest/icu/experimental/units/converter_factory/struct.ConverterFactory.html#method.new) for more information.
   */
  inline static std::unique_ptr<icu4x::UnitsConverterFactory> create();

  /**
   * Construct a new [`UnitsConverterFactory`] instance using a particular data source.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/latest/icu/experimental/units/converter_factory/struct.ConverterFactory.html#method.new) for more information.
   */
  inline static diplomat::result<std::unique_ptr<icu4x::UnitsConverterFactory>, icu4x::DataError> create_with_provider(const icu4x::DataProvider& provider);

  /**
   * Creates a new [`UnitsConverter`] from the input and output [`MeasureUnit`]s.
   * Returns nothing if the conversion between the two units is not possible.
   * For example, conversion between `meter` and `second` is not possible.
   *
   * See the [Rust documentation for `converter`](https://docs.rs/icu/latest/icu/experimental/units/converter_factory/struct.ConverterFactory.html#method.converter) for more information.
   */
  inline std::unique_ptr<icu4x::UnitsConverter> converter(const icu4x::MeasureUnit& from, const icu4x::MeasureUnit& to) const;

  inline const icu4x::capi::UnitsConverterFactory* AsFFI() const;
  inline icu4x::capi::UnitsConverterFactory* AsFFI();
  inline static const icu4x::UnitsConverterFactory* FromFFI(const icu4x::capi::UnitsConverterFactory* ptr);
  inline static icu4x::UnitsConverterFactory* FromFFI(icu4x::capi::UnitsConverterFactory* ptr);
  inline static void operator delete(void* ptr);
private:
  UnitsConverterFactory() = delete;
  UnitsConverterFactory(const icu4x::UnitsConverterFactory&) = delete;
  UnitsConverterFactory(icu4x::UnitsConverterFactory&&) noexcept = delete;
  UnitsConverterFactory operator=(const icu4x::UnitsConverterFactory&) = delete;
  UnitsConverterFactory operator=(icu4x::UnitsConverterFactory&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_UnitsConverterFactory_D_HPP
