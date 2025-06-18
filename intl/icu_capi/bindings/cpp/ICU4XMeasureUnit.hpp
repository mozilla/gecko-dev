#ifndef ICU4XMeasureUnit_HPP
#define ICU4XMeasureUnit_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XMeasureUnit.h"


/**
 * A destruction policy for using ICU4XMeasureUnit with std::unique_ptr.
 */
struct ICU4XMeasureUnitDeleter {
  void operator()(capi::ICU4XMeasureUnit* l) const noexcept {
    capi::ICU4XMeasureUnit_destroy(l);
  }
};

/**
 * An ICU4X Measurement Unit object which represents a single unit of measurement
 * such as `meter`, `second`, `kilometer-per-hour`, `square-meter`, etc.
 * 
 * You can create an instance of this object using [`ICU4XMeasureUnitParser`] by calling the `parse_measure_unit` method.
 * 
 * See the [Rust documentation for `MeasureUnit`](https://docs.rs/icu/latest/icu/experimental/units/measureunit/struct.MeasureUnit.html) for more information.
 */
class ICU4XMeasureUnit {
 public:
  inline const capi::ICU4XMeasureUnit* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XMeasureUnit* AsFFIMut() { return this->inner.get(); }
  inline explicit ICU4XMeasureUnit(capi::ICU4XMeasureUnit* i) : inner(i) {}
  ICU4XMeasureUnit() = default;
  ICU4XMeasureUnit(ICU4XMeasureUnit&&) noexcept = default;
  ICU4XMeasureUnit& operator=(ICU4XMeasureUnit&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XMeasureUnit, ICU4XMeasureUnitDeleter> inner;
};


#endif
