// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_calendar::{types::RataDie, AsCalendar, Date, Iso, RangeError};

use crate::zone::UtcOffset;

/// This macro defines a struct for 0-based date fields: hours, minutes, seconds
/// and fractional seconds. Each unit is bounded by a range. The traits implemented
/// here will return a Result on whether or not the unit is in range from the given
/// input.
macro_rules! dt_unit {
    ($name:ident, $storage:ident, $value:expr, $(#[$docs:meta])+) => {
        $(#[$docs])+
        #[derive(Debug, Default, Clone, Copy, PartialEq, Eq, Ord, PartialOrd, Hash)]
        pub struct $name($storage);

        impl $name {
            /// Gets the numeric value for this component.
            pub const fn number(self) -> $storage {
                self.0
            }

            /// Creates a new value at 0.
            pub const fn zero() -> $name {
                Self(0)
            }

            /// Returns whether the value is zero.
            #[inline]
            pub fn is_zero(self) -> bool {
                self.0 == 0
            }
        }

        impl TryFrom<$storage> for $name {
            type Error = RangeError;

            fn try_from(input: $storage) -> Result<Self, Self::Error> {
                if input > $value {
                    Err(RangeError {
                        field: stringify!($name),
                        min: 0,
                        max: $value,
                        value: input as i32,
                    })
                } else {
                    Ok(Self(input))
                }
            }
        }

        impl TryFrom<usize> for $name {
            type Error = RangeError;

            fn try_from(input: usize) -> Result<Self, Self::Error> {
                if input > $value {
                    Err(RangeError {
                        field: "$name",
                        min: 0,
                        max: $value,
                        value: input as i32,
                    })
                } else {
                    Ok(Self(input as $storage))
                }
            }
        }

        impl From<$name> for $storage {
            fn from(input: $name) -> Self {
                input.0
            }
        }

        impl From<$name> for usize {
            fn from(input: $name) -> Self {
                input.0 as Self
            }
        }
    };
}

dt_unit!(
    Hour,
    u8,
    23,
    /// An ISO-8601 hour component, for use with ISO calendars.
    ///
    /// Must be within inclusive bounds `[0, 23]`.
);

dt_unit!(
    Minute,
    u8,
    59,
    /// An ISO-8601 minute component, for use with ISO calendars.
    ///
    /// Must be within inclusive bounds `[0, 59]`.
);

dt_unit!(
    Second,
    u8,
    60,
    /// An ISO-8601 second component, for use with ISO calendars.
    ///
    /// Must be within inclusive bounds `[0, 60]`. `60` accommodates for leap seconds.
);

dt_unit!(
    Nanosecond,
    u32,
    999_999_999,
    /// A fractional second component, stored as nanoseconds.
    ///
    /// Must be within inclusive bounds `[0, 999_999_999]`."
);

/// A representation of a time in hours, minutes, seconds, and nanoseconds
///
/// **The primary definition of this type is in the [`icu_time`](https://docs.rs/icu_time) crate. Other ICU4X crates re-export it for convenience.**
///
/// This type supports the range [00:00:00.000000000, 23:59:60.999999999].
#[derive(Debug, Copy, Clone, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct Time {
    /// Hour
    pub hour: Hour,

    /// Minute
    pub minute: Minute,

    /// Second
    pub second: Second,

    /// Subsecond
    pub subsecond: Nanosecond,
}

impl Time {
    /// Construct a new [`Time`], without validating that all components are in range
    pub const fn new(hour: Hour, minute: Minute, second: Second, subsecond: Nanosecond) -> Self {
        Self {
            hour,
            minute,
            second,
            subsecond,
        }
    }

    /// Construct a new [`Time`] representing the start of the day (00:00:00.000)
    pub const fn start_of_day() -> Self {
        Self {
            hour: Hour(0),
            minute: Minute(0),
            second: Second(0),
            subsecond: Nanosecond(0),
        }
    }

    /// Construct a new [`Time`] representing noon (12:00:00.000)
    pub const fn noon() -> Self {
        Self {
            hour: Hour(12),
            minute: Minute(0),
            second: Second(0),
            subsecond: Nanosecond(0),
        }
    }

    /// Construct a new [`Time`], whilst validating that all components are in range
    pub fn try_new(hour: u8, minute: u8, second: u8, nanosecond: u32) -> Result<Self, RangeError> {
        Ok(Self {
            hour: hour.try_into()?,
            minute: minute.try_into()?,
            second: second.try_into()?,
            subsecond: nanosecond.try_into()?,
        })
    }
}

/// A date and time for a given calendar.
///
/// **The primary definition of this type is in the [`icu_time`](https://docs.rs/icu_time) crate. Other ICU4X crates re-export it for convenience.**
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct DateTime<A: AsCalendar> {
    /// The date
    pub date: Date<A>,
    /// The time
    pub time: Time,
}

/// A date and time for a given calendar, local to a specified time zone.
///
/// **The primary definition of this type is in the [`icu_time`](https://docs.rs/icu_time) crate. Other ICU4X crates re-export it for convenience.**
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct ZonedDateTime<A: AsCalendar, Z> {
    /// The date, local to the time zone
    pub date: Date<A>,
    /// The time, local to the time zone
    pub time: Time,
    /// The time zone
    pub zone: Z,
}

impl ZonedDateTime<Iso, UtcOffset> {
    /// Creates a [`ZonedDateTime`] from an absolute time, in milliseconds since the UNIX Epoch,
    /// and a UTC offset.
    ///
    /// This constructor returns a [`ZonedDateTime`] that supports only the localized offset
    /// time zone style.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::cal::Iso;
    /// use icu::time::zone::UtcOffset;
    /// use icu::time::ZonedDateTime;
    ///
    /// let iso_str = "2025-04-30T17:45-0700";
    /// let timestamp = 1746060300000; // milliseconds since UNIX epoch
    /// let offset: UtcOffset = "-0700".parse().unwrap();
    ///
    /// let zdt_from_timestamp =
    ///     ZonedDateTime::from_epoch_milliseconds_and_utc_offset(
    ///         timestamp, offset,
    ///     );
    ///
    /// // Check that it equals the same as the parse result:
    /// let zdt_from_str =
    ///     ZonedDateTime::try_offset_only_from_str(iso_str, Iso).unwrap();
    /// assert_eq!(zdt_from_timestamp, zdt_from_str);
    /// ```
    ///
    /// Negative timestamps are supported:
    ///
    /// ```
    /// use icu::calendar::cal::Iso;
    /// use icu::time::zone::UtcOffset;
    /// use icu::time::ZonedDateTime;
    ///
    /// let iso_str = "1920-01-02T03:04:05.250+0600";
    /// let timestamp = -1577847354750; // milliseconds since UNIX epoch
    /// let offset: UtcOffset = "+0600".parse().unwrap();
    ///
    /// let zdt_from_timestamp =
    ///     ZonedDateTime::from_epoch_milliseconds_and_utc_offset(
    ///         timestamp, offset,
    ///     );
    ///
    /// // Check that it equals the same as the parse result:
    /// let zdt_from_str =
    ///     ZonedDateTime::try_offset_only_from_str(iso_str, Iso).unwrap();
    /// assert_eq!(zdt_from_timestamp, zdt_from_str);
    /// ```
    pub fn from_epoch_milliseconds_and_utc_offset(
        epoch_milliseconds: i64,
        utc_offset: UtcOffset,
    ) -> Self {
        // TODO(#6512): Handle overflow
        let local_epoch_milliseconds = epoch_milliseconds + (1000 * utc_offset.to_seconds()) as i64;
        let (epoch_days, time_millisecs) = (
            local_epoch_milliseconds.div_euclid(86400000),
            local_epoch_milliseconds.rem_euclid(86400000),
        );
        const UNIX_EPOCH: RataDie = calendrical_calculations::iso::const_fixed_from_iso(1970, 1, 1);
        let rata_die = UNIX_EPOCH + epoch_days;
        #[allow(clippy::unwrap_used)] // these values are derived via modulo operators
        let time = Time::try_new(
            (time_millisecs / 3600000) as u8,
            ((time_millisecs % 3600000) / 60000) as u8,
            ((time_millisecs % 60000) / 1000) as u8,
            ((time_millisecs % 1000) as u32) * 1000000,
        )
        .unwrap();
        ZonedDateTime {
            date: Date::from_rata_die(rata_die, Iso),
            time,
            zone: utc_offset,
        }
    }
}
