#ifndef icu4x_UnitsConverter_D_HPP
#define icu4x_UnitsConverter_D_HPP

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
namespace capi { struct UnitsConverter; }
class UnitsConverter;
}


namespace icu4x {
namespace capi {
    struct UnitsConverter;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * An ICU4X Units Converter object, capable of converting between two [`MeasureUnit`]s.
 *
 * You can create an instance of this object using [`UnitsConverterFactory`] by calling the `converter` method.
 *
 * See the [Rust documentation for `UnitsConverter`](https://docs.rs/icu/latest/icu/experimental/units/converter/struct.UnitsConverter.html) for more information.
 */
class UnitsConverter {
public:

  /**
   * Converts the input value from the input unit to the output unit (that have been used to create this converter).
   * NOTE:
   * The conversion using floating-point operations is not as accurate as the conversion using ratios.
   *
   * See the [Rust documentation for `convert`](https://docs.rs/icu/latest/icu/experimental/units/converter/struct.UnitsConverter.html#method.convert) for more information.
   */
  inline double convert(double value) const;

  /**
   * Clones the current [`UnitsConverter`] object.
   *
   * See the [Rust documentation for `clone`](https://docs.rs/icu/latest/icu/experimental/units/converter/struct.UnitsConverter.html#method.clone) for more information.
   */
  inline std::unique_ptr<icu4x::UnitsConverter> clone() const;

  inline const icu4x::capi::UnitsConverter* AsFFI() const;
  inline icu4x::capi::UnitsConverter* AsFFI();
  inline static const icu4x::UnitsConverter* FromFFI(const icu4x::capi::UnitsConverter* ptr);
  inline static icu4x::UnitsConverter* FromFFI(icu4x::capi::UnitsConverter* ptr);
  inline static void operator delete(void* ptr);
private:
  UnitsConverter() = delete;
  UnitsConverter(const icu4x::UnitsConverter&) = delete;
  UnitsConverter(icu4x::UnitsConverter&&) noexcept = delete;
  UnitsConverter operator=(const icu4x::UnitsConverter&) = delete;
  UnitsConverter operator=(icu4x::UnitsConverter&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_UnitsConverter_D_HPP
