#ifndef icu4x_MeasureUnitParser_D_HPP
#define icu4x_MeasureUnitParser_D_HPP

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
namespace capi { struct MeasureUnitParser; }
class MeasureUnitParser;
class DataError;
}


namespace icu4x {
namespace capi {
    struct MeasureUnitParser;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * An ICU4X Measure Unit Parser object, capable of parsing the CLDR unit identifier (e.g. `meter-per-square-second`) and get the [`MeasureUnit`].
 *
 * See the [Rust documentation for `MeasureUnitParser`](https://docs.rs/icu/latest/icu/experimental/measure/parser/struct.MeasureUnitParser.html) for more information.
 */
class MeasureUnitParser {
public:

  /**
   * Construct a new [`MeasureUnitParser`] instance using compiled data.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/latest/icu/experimental/measure/parser/struct.MeasureUnitParser.html#method.new) for more information.
   */
  inline static std::unique_ptr<icu4x::MeasureUnitParser> create();

  /**
   * Construct a new [`MeasureUnitParser`] instance using a particular data source.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/latest/icu/experimental/measure/parser/struct.MeasureUnitParser.html#method.new) for more information.
   */
  inline static diplomat::result<std::unique_ptr<icu4x::MeasureUnitParser>, icu4x::DataError> create_with_provider(const icu4x::DataProvider& provider);

  /**
   * See the [Rust documentation for `parse`](https://docs.rs/icu/latest/icu/experimental/measure/parser/struct.MeasureUnitParser.html#method.parse) for more information.
   */
  inline std::unique_ptr<icu4x::MeasureUnit> parse(std::string_view unit_id) const;

  inline const icu4x::capi::MeasureUnitParser* AsFFI() const;
  inline icu4x::capi::MeasureUnitParser* AsFFI();
  inline static const icu4x::MeasureUnitParser* FromFFI(const icu4x::capi::MeasureUnitParser* ptr);
  inline static icu4x::MeasureUnitParser* FromFFI(icu4x::capi::MeasureUnitParser* ptr);
  inline static void operator delete(void* ptr);
private:
  MeasureUnitParser() = delete;
  MeasureUnitParser(const icu4x::MeasureUnitParser&) = delete;
  MeasureUnitParser(icu4x::MeasureUnitParser&&) noexcept = delete;
  MeasureUnitParser operator=(const icu4x::MeasureUnitParser&) = delete;
  MeasureUnitParser operator=(icu4x::MeasureUnitParser&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_MeasureUnitParser_D_HPP
