#ifndef icu4x_TimeZone_D_HPP
#define icu4x_TimeZone_D_HPP

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
namespace capi { struct TimeZone; }
class TimeZone;
namespace capi { struct TimeZoneInfo; }
class TimeZoneInfo;
namespace capi { struct UtcOffset; }
class UtcOffset;
}


namespace icu4x {
namespace capi {
    struct TimeZone;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `TimeZone`](https://docs.rs/icu/latest/icu/time/struct.TimeZone.html) for more information.
 */
class TimeZone {
public:

  /**
   * The unknown time zone.
   *
   * See the [Rust documentation for `unknown`](https://docs.rs/icu/latest/icu/time/struct.TimeZoneInfo.html#method.unknown) for more information.
   */
  inline static std::unique_ptr<icu4x::TimeZone> unknown();

  /**
   * Whether the time zone is the unknown zone.
   *
   * See the [Rust documentation for `is_unknown`](https://docs.rs/icu/latest/icu/time/struct.TimeZone.html#method.is_unknown) for more information.
   */
  inline bool is_unknown() const;

  /**
   * Creates a time zone from a BCP-47 string.
   *
   * Returns the unknown time zone if the string is not a valid BCP-47 subtag.
   *
   * Additional information: [1](https://docs.rs/icu/latest/icu/time/struct.TimeZone.html)
   */
  inline static std::unique_ptr<icu4x::TimeZone> create_from_bcp47(std::string_view id);

  /**
   * See the [Rust documentation for `with_offset`](https://docs.rs/icu/latest/icu/time/struct.TimeZone.html#method.with_offset) for more information.
   */
  inline std::unique_ptr<icu4x::TimeZoneInfo> with_offset(const icu4x::UtcOffset& offset) const;

  /**
   * See the [Rust documentation for `without_offset`](https://docs.rs/icu/latest/icu/time/struct.TimeZone.html#method.without_offset) for more information.
   */
  inline std::unique_ptr<icu4x::TimeZoneInfo> without_offset() const;

  inline const icu4x::capi::TimeZone* AsFFI() const;
  inline icu4x::capi::TimeZone* AsFFI();
  inline static const icu4x::TimeZone* FromFFI(const icu4x::capi::TimeZone* ptr);
  inline static icu4x::TimeZone* FromFFI(icu4x::capi::TimeZone* ptr);
  inline static void operator delete(void* ptr);
private:
  TimeZone() = delete;
  TimeZone(const icu4x::TimeZone&) = delete;
  TimeZone(icu4x::TimeZone&&) noexcept = delete;
  TimeZone operator=(const icu4x::TimeZone&) = delete;
  TimeZone operator=(icu4x::TimeZone&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_TimeZone_D_HPP
