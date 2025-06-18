#ifndef icu4x_VariantOffsetsCalculator_D_HPP
#define icu4x_VariantOffsetsCalculator_D_HPP

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
namespace capi { struct IsoDate; }
class IsoDate;
namespace capi { struct Time; }
class Time;
namespace capi { struct TimeZone; }
class TimeZone;
namespace capi { struct VariantOffsetsCalculator; }
class VariantOffsetsCalculator;
struct VariantOffsets;
class DataError;
}


namespace icu4x {
namespace capi {
    struct VariantOffsetsCalculator;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `VariantOffsetsCalculator`](https://docs.rs/icu/latest/icu/time/zone/struct.VariantOffsetsCalculator.html) for more information.
 */
class VariantOffsetsCalculator {
public:

  /**
   * Construct a new [`VariantOffsetsCalculator`] instance using compiled data.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/latest/icu/time/zone/struct.VariantOffsetsCalculator.html#method.new) for more information.
   */
  inline static std::unique_ptr<icu4x::VariantOffsetsCalculator> create();

  /**
   * Construct a new [`VariantOffsetsCalculator`] instance using a particular data source.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/latest/icu/time/zone/struct.VariantOffsetsCalculator.html#method.new) for more information.
   */
  inline static diplomat::result<std::unique_ptr<icu4x::VariantOffsetsCalculator>, icu4x::DataError> create_with_provider(const icu4x::DataProvider& provider);

  /**
   * See the [Rust documentation for `compute_offsets_from_time_zone_and_name_timestamp`](https://docs.rs/icu/latest/icu/time/zone/struct.VariantOffsetsCalculatorBorrowed.html#method.compute_offsets_from_time_zone_and_name_timestamp) for more information.
   */
  inline std::optional<icu4x::VariantOffsets> compute_offsets_from_time_zone_and_date_time(const icu4x::TimeZone& time_zone, const icu4x::IsoDate& local_date, const icu4x::Time& local_time) const;

  inline const icu4x::capi::VariantOffsetsCalculator* AsFFI() const;
  inline icu4x::capi::VariantOffsetsCalculator* AsFFI();
  inline static const icu4x::VariantOffsetsCalculator* FromFFI(const icu4x::capi::VariantOffsetsCalculator* ptr);
  inline static icu4x::VariantOffsetsCalculator* FromFFI(icu4x::capi::VariantOffsetsCalculator* ptr);
  inline static void operator delete(void* ptr);
private:
  VariantOffsetsCalculator() = delete;
  VariantOffsetsCalculator(const icu4x::VariantOffsetsCalculator&) = delete;
  VariantOffsetsCalculator(icu4x::VariantOffsetsCalculator&&) noexcept = delete;
  VariantOffsetsCalculator operator=(const icu4x::VariantOffsetsCalculator&) = delete;
  VariantOffsetsCalculator operator=(icu4x::VariantOffsetsCalculator&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_VariantOffsetsCalculator_D_HPP
