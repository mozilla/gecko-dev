#ifndef icu4x_MeasureUnit_D_HPP
#define icu4x_MeasureUnit_D_HPP

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
namespace capi {
    struct MeasureUnit;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * An ICU4X Measurement Unit object which represents a single unit of measurement
 * such as `meter`, `second`, `kilometer-per-hour`, `square-meter`, etc.
 *
 * You can create an instance of this object using [`MeasureUnitParser`] by calling the `parse` method.
 *
 * See the [Rust documentation for `MeasureUnit`](https://docs.rs/icu/latest/icu/experimental/measure/measureunit/struct.MeasureUnit.html) for more information.
 */
class MeasureUnit {
public:

  inline const icu4x::capi::MeasureUnit* AsFFI() const;
  inline icu4x::capi::MeasureUnit* AsFFI();
  inline static const icu4x::MeasureUnit* FromFFI(const icu4x::capi::MeasureUnit* ptr);
  inline static icu4x::MeasureUnit* FromFFI(icu4x::capi::MeasureUnit* ptr);
  inline static void operator delete(void* ptr);
private:
  MeasureUnit() = delete;
  MeasureUnit(const icu4x::MeasureUnit&) = delete;
  MeasureUnit(icu4x::MeasureUnit&&) noexcept = delete;
  MeasureUnit operator=(const icu4x::MeasureUnit&) = delete;
  MeasureUnit operator=(icu4x::MeasureUnit&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_MeasureUnit_D_HPP
