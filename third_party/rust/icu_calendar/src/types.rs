// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains various types used by `icu_calendar` and `icu_datetime`

use crate::error::CalendarError;
use core::convert::TryFrom;
use core::convert::TryInto;
use core::fmt;
use core::num::NonZeroU8;
use core::str::FromStr;
use tinystr::TinyAsciiStr;
use tinystr::{TinyStr16, TinyStr4};
use zerovec::maps::ZeroMapKV;
use zerovec::ule::AsULE;

/// The era of a particular date
///
/// Different calendars use different era codes, see their documentation
/// for details.
///
/// Era codes are shared with Temporal, [see Temporal proposal][era-proposal].
///
/// [era-proposal]: https://tc39.es/proposal-intl-era-monthcode/
#[derive(Copy, Clone, Debug, PartialEq)]
#[allow(clippy::exhaustive_structs)] // this is a newtype
pub struct Era(pub TinyStr16);

impl From<TinyStr16> for Era {
    fn from(x: TinyStr16) -> Self {
        Self(x)
    }
}

impl FromStr for Era {
    type Err = <TinyStr16 as FromStr>::Err;
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        s.parse().map(Self)
    }
}

/// Representation of a formattable year.
///
/// More fields may be added in the future for things like extended year
#[derive(Copy, Clone, Debug, PartialEq)]
#[non_exhaustive]
pub struct FormattableYear {
    /// The era containing the year.
    ///
    /// This may not always be the canonical era for the calendar and could be an alias,
    /// for example all `islamic` calendars return `islamic` as the formattable era code
    /// which allows them to share data.
    pub era: Era,

    /// The year number in the current era (usually 1-based).
    pub number: i32,

    /// The year in the current cycle for cyclic calendars (1-indexed)
    /// can be set to `None` for non-cyclic calendars
    ///
    /// For chinese and dangi it will be
    /// a number between 1 and 60, for hypothetical other calendars it may be something else.
    pub cyclic: Option<NonZeroU8>,

    /// The related ISO year. This is normally the ISO (proleptic Gregorian) year having the greatest
    /// overlap with the calendar year. It is used in certain date formatting patterns.
    ///
    /// Can be `None` if the calendar does not typically use `related_iso` (and CLDR does not contain patterns
    /// using it)
    pub related_iso: Option<i32>,
}

impl FormattableYear {
    /// Construct a new Year given an era and number
    ///
    /// Other fields can be set mutably after construction
    /// as needed
    pub fn new(era: Era, number: i32, cyclic: Option<NonZeroU8>) -> Self {
        Self {
            era,
            number,
            cyclic,
            related_iso: None,
        }
    }
}

/// Representation of a month in a year
///
/// Month codes typically look like `M01`, `M02`, etc, but can handle leap months
/// (`M03L`) in lunar calendars. Solar calendars will have codes between `M01` and `M12`
/// potentially with an `M13` for epagomenal months. Check the docs for a particular calendar
/// for details on what its month codes are.
///
/// Month codes are shared with Temporal, [see Temporal proposal][era-proposal].
///
/// [era-proposal]: https://tc39.es/proposal-intl-era-monthcode/
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[allow(clippy::exhaustive_structs)] // this is a newtype
#[cfg_attr(
    feature = "datagen",
    derive(serde::Serialize, databake::Bake),
    databake(path = icu_calendar::types),
)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct MonthCode(pub TinyStr4);

impl MonthCode {
    /// Returns an option which is `Some` containing the non-month version of a leap month
    /// if the [`MonthCode`] this method is called upon is a leap month, and `None` otherwise.
    /// This method assumes the [`MonthCode`] is valid.
    pub fn get_normal_if_leap(self) -> Option<MonthCode> {
        let bytes = self.0.all_bytes();
        if bytes[3] == b'L' {
            Some(MonthCode(TinyAsciiStr::from_bytes(&bytes[0..3]).ok()?))
        } else {
            None
        }
    }
    /// Get the month number and whether or not it is leap from the month code
    pub fn parsed(self) -> Option<(u8, bool)> {
        // Match statements on tinystrs are annoying so instead
        // we calculate it from the bytes directly

        let bytes = self.0.all_bytes();
        let is_leap = bytes[3] == b'L';
        if bytes[0] != b'M' {
            return None;
        }
        if bytes[1] == b'0' {
            if bytes[2] >= b'1' && bytes[2] <= b'9' {
                return Some((bytes[2] - b'0', is_leap));
            }
        } else if bytes[1] == b'1' && bytes[2] >= b'0' && bytes[2] <= b'3' {
            return Some((10 + bytes[2] - b'0', is_leap));
        }
        None
    }
}

#[test]
fn test_get_normal_month_code_if_leap() {
    let mc1 = MonthCode(tinystr::tinystr!(4, "M01L"));
    let result1 = mc1.get_normal_if_leap();
    assert_eq!(result1, Some(MonthCode(tinystr::tinystr!(4, "M01"))));

    let mc2 = MonthCode(tinystr::tinystr!(4, "M11L"));
    let result2 = mc2.get_normal_if_leap();
    assert_eq!(result2, Some(MonthCode(tinystr::tinystr!(4, "M11"))));

    let mc_invalid = MonthCode(tinystr::tinystr!(4, "M10"));
    let result_invalid = mc_invalid.get_normal_if_leap();
    assert_eq!(result_invalid, None);
}

impl AsULE for MonthCode {
    type ULE = TinyStr4;
    fn to_unaligned(self) -> TinyStr4 {
        self.0
    }
    fn from_unaligned(u: TinyStr4) -> Self {
        Self(u)
    }
}

impl<'a> ZeroMapKV<'a> for MonthCode {
    type Container = zerovec::ZeroVec<'a, MonthCode>;
    type Slice = zerovec::ZeroSlice<MonthCode>;
    type GetType = <MonthCode as AsULE>::ULE;
    type OwnedType = MonthCode;
}

impl fmt::Display for MonthCode {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.0)
    }
}

impl From<TinyStr4> for MonthCode {
    fn from(x: TinyStr4) -> Self {
        Self(x)
    }
}
impl FromStr for MonthCode {
    type Err = <TinyStr4 as FromStr>::Err;
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        s.parse().map(Self)
    }
}

/// Representation of a formattable month.
#[derive(Copy, Clone, Debug, PartialEq)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct FormattableMonth {
    /// The month number in this given year. For calendars with leap months, all months after
    /// the leap month will end up with an incremented number.
    ///
    /// In general, prefer using the month code in generic code.
    pub ordinal: u32,

    /// The month code, used to distinguish months during leap years.
    ///
    /// This may not necessarily be the canonical month code for a month in cases where a month has different
    /// formatting in a leap year, for example Adar/Adar II in the Hebrew calendar in a leap year has
    /// the code M06, but for formatting specifically the Hebrew calendar will return M06L since it is formatted
    /// differently.
    pub code: MonthCode,
}

/// A struct containing various details about the position of the day within a year. It is returned
// by the [`day_of_year_info()`](trait.DateInput.html#tymethod.day_of_year_info) method of the
// [`DateInput`] trait.
#[derive(Copy, Clone, Debug, PartialEq)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct DayOfYearInfo {
    /// The current day of the year, 1-based.
    pub day_of_year: u16,
    /// The number of days in a year.
    pub days_in_year: u16,
    /// The previous year.
    pub prev_year: FormattableYear,
    /// The number of days in the previous year.
    pub days_in_prev_year: u16,
    /// The next year.
    pub next_year: FormattableYear,
}

/// A day number in a month. Usually 1-based.
#[allow(clippy::exhaustive_structs)] // this is a newtype
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct DayOfMonth(pub u32);

/// A week number in a month. Usually 1-based.
#[derive(Clone, Copy, Debug, PartialEq)]
#[allow(clippy::exhaustive_structs)] // this is a newtype
pub struct WeekOfMonth(pub u32);

/// A week number in a year. Usually 1-based.
#[derive(Clone, Copy, Debug, PartialEq)]
#[allow(clippy::exhaustive_structs)] // this is a newtype
pub struct WeekOfYear(pub u32);

/// A day of week in month. 1-based.
#[derive(Clone, Copy, Debug, PartialEq)]
#[allow(clippy::exhaustive_structs)] // this is a newtype
pub struct DayOfWeekInMonth(pub u32);

impl From<DayOfMonth> for DayOfWeekInMonth {
    fn from(day_of_month: DayOfMonth) -> Self {
        DayOfWeekInMonth(1 + ((day_of_month.0 - 1) / 7))
    }
}

#[test]
fn test_day_of_week_in_month() {
    assert_eq!(DayOfWeekInMonth::from(DayOfMonth(1)).0, 1);
    assert_eq!(DayOfWeekInMonth::from(DayOfMonth(7)).0, 1);
    assert_eq!(DayOfWeekInMonth::from(DayOfMonth(8)).0, 2);
}

/// This macro defines a struct for 0-based date fields: hours, minutes, seconds
/// and fractional seconds. Each unit is bounded by a range. The traits implemented
/// here will return a Result on whether or not the unit is in range from the given
/// input.
macro_rules! dt_unit {
    ($name:ident, $storage:ident, $value:expr, $docs:expr) => {
        #[doc=$docs]
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
        }

        impl FromStr for $name {
            type Err = CalendarError;

            fn from_str(input: &str) -> Result<Self, Self::Err> {
                let val: $storage = input.parse()?;
                if val > $value {
                    Err(CalendarError::Overflow {
                        field: "$name",
                        max: $value,
                    })
                } else {
                    Ok(Self(val))
                }
            }
        }

        impl TryFrom<$storage> for $name {
            type Error = CalendarError;

            fn try_from(input: $storage) -> Result<Self, Self::Error> {
                if input > $value {
                    Err(CalendarError::Overflow {
                        field: "$name",
                        max: $value,
                    })
                } else {
                    Ok(Self(input))
                }
            }
        }

        impl TryFrom<usize> for $name {
            type Error = CalendarError;

            fn try_from(input: usize) -> Result<Self, Self::Error> {
                if input > $value {
                    Err(CalendarError::Overflow {
                        field: "$name",
                        max: $value,
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

        impl $name {
            /// Attempts to add two values.
            /// Returns `Some` if the sum is within bounds.
            /// Returns `None` if the sum is out of bounds.
            pub fn try_add(self, other: $storage) -> Option<Self> {
                let sum = self.0.saturating_add(other);
                if sum > $value {
                    None
                } else {
                    Some(Self(sum))
                }
            }

            /// Attempts to subtract two values.
            /// Returns `Some` if the difference is within bounds.
            /// Returns `None` if the difference is out of bounds.
            pub fn try_sub(self, other: $storage) -> Option<Self> {
                self.0.checked_sub(other).map(Self)
            }
        }
    };
}

dt_unit!(
    IsoHour,
    u8,
    24,
    "An ISO-8601 hour component, for use with ISO calendars.

Must be within inclusive bounds `[0, 24]`. The value could be equal to 24 to
denote the end of a day, with the writing 24:00:00. It corresponds to the same
time as the next day at 00:00:00."
);

dt_unit!(
    IsoMinute,
    u8,
    60,
    "An ISO-8601 minute component, for use with ISO calendars.

Must be within inclusive bounds `[0, 60]`. The value could be equal to 60 to
denote the end of an hour, with the writing 12:60:00. This example corresponds
to the same time as 13:00:00. This is an extension to ISO 8601."
);

dt_unit!(
    IsoSecond,
    u8,
    61,
    "An ISO-8601 second component, for use with ISO calendars.

Must be within inclusive bounds `[0, 61]`. `60` accommodates for leap seconds.

The value could also be equal to 60 or 61, to indicate the end of a leap second,
with the writing `23:59:61.000000000Z` or `23:59:60.000000000Z`. These examples,
if used with this goal, would correspond to the same time as the next day, at
time `00:00:00.000000000Z`. This is an extension to ISO 8601."
);

dt_unit!(
    NanoSecond,
    u32,
    999_999_999,
    "A fractional second component, stored as nanoseconds.

Must be within inclusive bounds `[0, 999_999_999]`."
);

#[test]
fn test_iso_hour_arithmetic() {
    const HOUR_MAX: u8 = 24;
    const HOUR_VALUE: u8 = 5;
    let hour = IsoHour(HOUR_VALUE);

    // middle of bounds
    assert_eq!(
        hour.try_add(HOUR_VALUE - 1),
        Some(IsoHour(HOUR_VALUE + (HOUR_VALUE - 1)))
    );
    assert_eq!(
        hour.try_sub(HOUR_VALUE - 1),
        Some(IsoHour(HOUR_VALUE - (HOUR_VALUE - 1)))
    );

    // edge of bounds
    assert_eq!(hour.try_add(HOUR_MAX - HOUR_VALUE), Some(IsoHour(HOUR_MAX)));
    assert_eq!(hour.try_sub(HOUR_VALUE), Some(IsoHour(0)));

    // out of bounds
    assert_eq!(hour.try_add(1 + HOUR_MAX - HOUR_VALUE), None);
    assert_eq!(hour.try_sub(1 + HOUR_VALUE), None);
}

#[test]
fn test_iso_minute_arithmetic() {
    const MINUTE_MAX: u8 = 60;
    const MINUTE_VALUE: u8 = 5;
    let minute = IsoMinute(MINUTE_VALUE);

    // middle of bounds
    assert_eq!(
        minute.try_add(MINUTE_VALUE - 1),
        Some(IsoMinute(MINUTE_VALUE + (MINUTE_VALUE - 1)))
    );
    assert_eq!(
        minute.try_sub(MINUTE_VALUE - 1),
        Some(IsoMinute(MINUTE_VALUE - (MINUTE_VALUE - 1)))
    );

    // edge of bounds
    assert_eq!(
        minute.try_add(MINUTE_MAX - MINUTE_VALUE),
        Some(IsoMinute(MINUTE_MAX))
    );
    assert_eq!(minute.try_sub(MINUTE_VALUE), Some(IsoMinute(0)));

    // out of bounds
    assert_eq!(minute.try_add(1 + MINUTE_MAX - MINUTE_VALUE), None);
    assert_eq!(minute.try_sub(1 + MINUTE_VALUE), None);
}

#[test]
fn test_iso_second_arithmetic() {
    const SECOND_MAX: u8 = 61;
    const SECOND_VALUE: u8 = 5;
    let second = IsoSecond(SECOND_VALUE);

    // middle of bounds
    assert_eq!(
        second.try_add(SECOND_VALUE - 1),
        Some(IsoSecond(SECOND_VALUE + (SECOND_VALUE - 1)))
    );
    assert_eq!(
        second.try_sub(SECOND_VALUE - 1),
        Some(IsoSecond(SECOND_VALUE - (SECOND_VALUE - 1)))
    );

    // edge of bounds
    assert_eq!(
        second.try_add(SECOND_MAX - SECOND_VALUE),
        Some(IsoSecond(SECOND_MAX))
    );
    assert_eq!(second.try_sub(SECOND_VALUE), Some(IsoSecond(0)));

    // out of bounds
    assert_eq!(second.try_add(1 + SECOND_MAX - SECOND_VALUE), None);
    assert_eq!(second.try_sub(1 + SECOND_VALUE), None);
}

#[test]
fn test_iso_nano_second_arithmetic() {
    const NANO_SECOND_MAX: u32 = 999_999_999;
    const NANO_SECOND_VALUE: u32 = 5;
    let nano_second = NanoSecond(NANO_SECOND_VALUE);

    // middle of bounds
    assert_eq!(
        nano_second.try_add(NANO_SECOND_VALUE - 1),
        Some(NanoSecond(NANO_SECOND_VALUE + (NANO_SECOND_VALUE - 1)))
    );
    assert_eq!(
        nano_second.try_sub(NANO_SECOND_VALUE - 1),
        Some(NanoSecond(NANO_SECOND_VALUE - (NANO_SECOND_VALUE - 1)))
    );

    // edge of bounds
    assert_eq!(
        nano_second.try_add(NANO_SECOND_MAX - NANO_SECOND_VALUE),
        Some(NanoSecond(NANO_SECOND_MAX))
    );
    assert_eq!(nano_second.try_sub(NANO_SECOND_VALUE), Some(NanoSecond(0)));

    // out of bounds
    assert_eq!(
        nano_second.try_add(1 + NANO_SECOND_MAX - NANO_SECOND_VALUE),
        None
    );
    assert_eq!(nano_second.try_sub(1 + NANO_SECOND_VALUE), None);
}

/// A representation of a time in hours, minutes, seconds, and nanoseconds
#[derive(Debug, Copy, Clone, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct Time {
    /// 0-based hour.
    pub hour: IsoHour,

    /// 0-based minute.
    pub minute: IsoMinute,

    /// 0-based second.
    pub second: IsoSecond,

    /// Fractional second
    pub nanosecond: NanoSecond,
}

impl Time {
    /// Construct a new [`Time`], without validating that all components are in range
    pub const fn new(
        hour: IsoHour,
        minute: IsoMinute,
        second: IsoSecond,
        nanosecond: NanoSecond,
    ) -> Self {
        Self {
            hour,
            minute,
            second,
            nanosecond,
        }
    }

    /// Construct a new [`Time`] representing midnight (00:00.000)
    pub const fn midnight() -> Self {
        Self {
            hour: IsoHour::zero(),
            minute: IsoMinute::zero(),
            second: IsoSecond::zero(),
            nanosecond: NanoSecond::zero(),
        }
    }

    /// Construct a new [`Time`], whilst validating that all components are in range
    pub fn try_new(
        hour: u8,
        minute: u8,
        second: u8,
        nanosecond: u32,
    ) -> Result<Self, CalendarError> {
        Ok(Self {
            hour: hour.try_into()?,
            minute: minute.try_into()?,
            second: second.try_into()?,
            nanosecond: nanosecond.try_into()?,
        })
    }

    /// Takes a number of minutes, which could be positive or negative, and returns the Time
    /// and the day number, which could be positive or negative.
    pub(crate) fn from_minute_with_remainder_days(minute: i32) -> (Time, i32) {
        let (extra_days, minute_in_day) = (minute.div_euclid(1440), minute.rem_euclid(1440));
        let (hours, minutes) = (minute_in_day / 60, minute_in_day % 60);
        #[allow(clippy::unwrap_used)] // values are moduloed to be in range
        (
            Self {
                hour: (hours as u8).try_into().unwrap(),
                minute: (minutes as u8).try_into().unwrap(),
                second: IsoSecond::zero(),
                nanosecond: NanoSecond::zero(),
            },
            extra_days,
        )
    }
}

#[test]
fn test_from_minute_with_remainder_days() {
    #[derive(Debug)]
    struct TestCase {
        minute: i32,
        expected_time: Time,
        expected_remainder: i32,
    }
    let zero_time = Time::new(
        IsoHour::zero(),
        IsoMinute::zero(),
        IsoSecond::zero(),
        NanoSecond::zero(),
    );
    let first_minute_in_day = Time::new(
        IsoHour::zero(),
        IsoMinute::try_from(1u8).unwrap(),
        IsoSecond::zero(),
        NanoSecond::zero(),
    );
    let last_minute_in_day = Time::new(
        IsoHour::try_from(23u8).unwrap(),
        IsoMinute::try_from(59u8).unwrap(),
        IsoSecond::zero(),
        NanoSecond::zero(),
    );
    let cases = [
        TestCase {
            minute: 0,
            expected_time: zero_time,
            expected_remainder: 0,
        },
        TestCase {
            minute: 30,
            expected_time: Time::new(
                IsoHour::zero(),
                IsoMinute::try_from(30u8).unwrap(),
                IsoSecond::zero(),
                NanoSecond::zero(),
            ),
            expected_remainder: 0,
        },
        TestCase {
            minute: 60,
            expected_time: Time::new(
                IsoHour::try_from(1u8).unwrap(),
                IsoMinute::zero(),
                IsoSecond::zero(),
                NanoSecond::zero(),
            ),
            expected_remainder: 0,
        },
        TestCase {
            minute: 90,
            expected_time: Time::new(
                IsoHour::try_from(1u8).unwrap(),
                IsoMinute::try_from(30u8).unwrap(),
                IsoSecond::zero(),
                NanoSecond::zero(),
            ),
            expected_remainder: 0,
        },
        TestCase {
            minute: 1439,
            expected_time: last_minute_in_day,
            expected_remainder: 0,
        },
        TestCase {
            minute: 1440,
            expected_time: Time::new(
                IsoHour::zero(),
                IsoMinute::zero(),
                IsoSecond::zero(),
                NanoSecond::zero(),
            ),
            expected_remainder: 1,
        },
        TestCase {
            minute: 1441,
            expected_time: first_minute_in_day,
            expected_remainder: 1,
        },
        TestCase {
            minute: i32::MAX,
            expected_time: Time::new(
                IsoHour::try_from(2u8).unwrap(),
                IsoMinute::try_from(7u8).unwrap(),
                IsoSecond::zero(),
                NanoSecond::zero(),
            ),
            expected_remainder: 1491308,
        },
        TestCase {
            minute: -1,
            expected_time: last_minute_in_day,
            expected_remainder: -1,
        },
        TestCase {
            minute: -1439,
            expected_time: first_minute_in_day,
            expected_remainder: -1,
        },
        TestCase {
            minute: -1440,
            expected_time: zero_time,
            expected_remainder: -1,
        },
        TestCase {
            minute: -1441,
            expected_time: last_minute_in_day,
            expected_remainder: -2,
        },
        TestCase {
            minute: i32::MIN,
            expected_time: Time::new(
                IsoHour::try_from(21u8).unwrap(),
                IsoMinute::try_from(52u8).unwrap(),
                IsoSecond::zero(),
                NanoSecond::zero(),
            ),
            expected_remainder: -1491309,
        },
    ];
    for cas in cases {
        let (actual_time, actual_remainder) = Time::from_minute_with_remainder_days(cas.minute);
        assert_eq!(actual_time, cas.expected_time, "{cas:?}");
        assert_eq!(actual_remainder, cas.expected_remainder, "{cas:?}");
    }
}

/// A weekday in a 7-day week, according to ISO-8601.
///
/// The discriminant values correspond to ISO-8601 weekday numbers (Monday = 1, Sunday = 7).
///
/// # Examples
///
/// ```
/// use icu::calendar::types::IsoWeekday;
///
/// assert_eq!(1, IsoWeekday::Monday as usize);
/// assert_eq!(7, IsoWeekday::Sunday as usize);
/// ```
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[allow(missing_docs)] // The weekday variants should be self-obvious.
#[repr(i8)]
#[cfg_attr(
    feature = "datagen",
    derive(serde::Serialize, databake::Bake),
    databake(path = icu_calendar::types),
)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[allow(clippy::exhaustive_enums)] // This is stable
pub enum IsoWeekday {
    Monday = 1,
    Tuesday,
    Wednesday,
    Thursday,
    Friday,
    Saturday,
    Sunday,
}

impl From<usize> for IsoWeekday {
    /// Convert from an ISO-8601 weekday number to an [`IsoWeekday`] enum. 0 is automatically converted
    /// to 7 (Sunday). If the number is out of range, it is interpreted modulo 7.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::types::IsoWeekday;
    ///
    /// assert_eq!(IsoWeekday::Sunday, IsoWeekday::from(0));
    /// assert_eq!(IsoWeekday::Monday, IsoWeekday::from(1));
    /// assert_eq!(IsoWeekday::Sunday, IsoWeekday::from(7));
    /// assert_eq!(IsoWeekday::Monday, IsoWeekday::from(8));
    /// ```
    fn from(input: usize) -> Self {
        let mut ordinal = (input % 7) as i8;
        if ordinal == 0 {
            ordinal = 7;
        }
        unsafe { core::mem::transmute(ordinal) }
    }
}
