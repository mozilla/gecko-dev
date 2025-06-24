#ifndef icu4x_TimeZoneInfo_D_HPP
#define icu4x_TimeZoneInfo_D_HPP

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
namespace capi { struct IsoDate; }
class IsoDate;
namespace capi { struct Time; }
class Time;
namespace capi { struct TimeZone; }
class TimeZone;
namespace capi { struct TimeZoneInfo; }
class TimeZoneInfo;
namespace capi { struct UtcOffset; }
class UtcOffset;
namespace capi { struct VariantOffsetsCalculator; }
class VariantOffsetsCalculator;
struct IsoDateTime;
class TimeZoneVariant;
}


namespace icu4x {
namespace capi {
    struct TimeZoneInfo;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `TimeZoneInfo`](https://docs.rs/icu/latest/icu/time/struct.TimeZoneInfo.html) for more information.
 */
class TimeZoneInfo {
public:

  /**
   * Creates a time zone for UTC (Coordinated Universal Time).
   *
   * See the [Rust documentation for `utc`](https://docs.rs/icu/latest/icu/time/struct.TimeZoneInfo.html#method.utc) for more information.
   */
  inline static std::unique_ptr<icu4x::TimeZoneInfo> utc();

  /**
   * Creates a time zone info from parts.
   */
  inline static std::unique_ptr<icu4x::TimeZoneInfo> from_parts(const icu4x::TimeZone& id, const icu4x::UtcOffset* offset, std::optional<icu4x::TimeZoneVariant> variant);

  /**
   * See the [Rust documentation for `id`](https://docs.rs/icu/latest/icu/time/struct.TimeZoneInfo.html#method.id) for more information.
   */
  inline std::unique_ptr<icu4x::TimeZone> id() const;

  /**
   * Sets the datetime at which to interpret the time zone
   * for display name lookup.
   *
   * Notes:
   *
   * - If not set, the formatting datetime is used if possible.
   * - The constraints are the same as with `ZoneNameTimestamp` in Rust.
   * - Set to year 1000 or 9999 for a reference far in the past or future.
   *
   * See the [Rust documentation for `at_date_time_iso`](https://docs.rs/icu/latest/icu/time/struct.TimeZoneInfo.html#method.at_date_time_iso) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/latest/icu/time/zone/struct.ZoneNameTimestamp.html)
   */
  inline std::unique_ptr<icu4x::TimeZoneInfo> at_date_time_iso(const icu4x::IsoDate& date, const icu4x::Time& time) const;

  /**
   * See the [Rust documentation for `zone_name_timestamp`](https://docs.rs/icu/latest/icu/time/struct.TimeZoneInfo.html#method.zone_name_timestamp) for more information.
   */
  inline std::optional<icu4x::IsoDateTime> zone_name_date_time() const;

  /**
   * See the [Rust documentation for `with_variant`](https://docs.rs/icu/latest/icu/time/struct.TimeZoneInfo.html#method.with_variant) for more information.
   */
  inline std::unique_ptr<icu4x::TimeZoneInfo> with_variant(icu4x::TimeZoneVariant time_variant) const;

  /**
   * Infers the zone variant.
   *
   * Requires the offset and local time to be set.
   *
   * See the [Rust documentation for `infer_variant`](https://docs.rs/icu/latest/icu/time/struct.TimeZoneInfo.html#method.infer_variant) for more information.
   *
   * Additional information: [1](https://docs.rs/icu/latest/icu/time/zone/enum.TimeZoneVariant.html)
   */
  inline std::optional<std::monostate> infer_variant(const icu4x::VariantOffsetsCalculator& offset_calculator);

  /**
   * See the [Rust documentation for `variant`](https://docs.rs/icu/latest/icu/time/struct.TimeZoneInfo.html#method.variant) for more information.
   */
  inline std::optional<icu4x::TimeZoneVariant> variant() const;

  inline const icu4x::capi::TimeZoneInfo* AsFFI() const;
  inline icu4x::capi::TimeZoneInfo* AsFFI();
  inline static const icu4x::TimeZoneInfo* FromFFI(const icu4x::capi::TimeZoneInfo* ptr);
  inline static icu4x::TimeZoneInfo* FromFFI(icu4x::capi::TimeZoneInfo* ptr);
  inline static void operator delete(void* ptr);
private:
  TimeZoneInfo() = delete;
  TimeZoneInfo(const icu4x::TimeZoneInfo&) = delete;
  TimeZoneInfo(icu4x::TimeZoneInfo&&) noexcept = delete;
  TimeZoneInfo operator=(const icu4x::TimeZoneInfo&) = delete;
  TimeZoneInfo operator=(icu4x::TimeZoneInfo&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_TimeZoneInfo_D_HPP
