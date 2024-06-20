// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains types and traits for use in the Chinese traditional lunar calendar,
//! as well as in related and derived calendars such as the Korean and Vietnamese lunar calendars.
//!
//! ```rust
//! use icu::calendar::{chinese::Chinese, Date, Iso};
//!
//! let iso_date = Date::try_new_iso_date(2023, 6, 23).unwrap();
//! let chinese_date =
//!     Date::new_from_iso(iso_date, Chinese::new_always_calculating());
//!
//! assert_eq!(chinese_date.year().number, 4660);
//! assert_eq!(chinese_date.year().related_iso, Some(2023));
//! assert_eq!(chinese_date.year().cyclic.unwrap().get(), 40);
//! assert_eq!(chinese_date.month().ordinal, 6);
//! assert_eq!(chinese_date.day_of_month().0, 6);
//! ```

use crate::{
    calendar_arithmetic::{ArithmeticDate, CalendarArithmetic},
    types::MonthCode,
    CalendarError, Iso,
};

use calendrical_calculations::chinese_based::{self, ChineseBased, YearBounds};
use calendrical_calculations::rata_die::RataDie;
use core::num::NonZeroU8;

/// The trait ChineseBased is used by Chinese-based calendars to perform computations shared by such calendar.
///
/// For an example of how to use this trait, see `impl ChineseBasedWithDataLoading for Chinese` in [`Chinese`].
pub(crate) trait ChineseBasedWithDataLoading: CalendarArithmetic {
    type CB: ChineseBased;
    /// Get the compiled const data for a ChineseBased calendar; can return `None` if the given year
    /// does not correspond to any compiled data.
    fn get_compiled_data_for_year(extended_year: i32) -> Option<ChineseBasedCompiledData>;
}

/// Chinese-based calendars define DateInner as a calendar-specific struct wrapping ChineseBasedDateInner.
#[derive(Debug, Eq, PartialEq, PartialOrd, Ord)]
pub(crate) struct ChineseBasedDateInner<C>(
    pub(crate) ArithmeticDate<C>,
    pub(crate) ChineseBasedYearInfo,
);

// we want these impls without the `C: Copy/Clone` bounds
impl<C> Copy for ChineseBasedDateInner<C> {}
impl<C> Clone for ChineseBasedDateInner<C> {
    fn clone(&self) -> Self {
        *self
    }
}

/// A `ChineseBasedDateInner` has additional information about the year corresponding to the Inner;
/// if there is available data for that year, the ChineseBasedYearInfo will be in the form of `Data`,
/// with a `ChineseBasedCompiledData` struct which contains more information; otherwise, a `Cache`
/// with a `ChineseBasedCache`, which contains less information, but is faster to compute.
#[derive(Copy, Clone, Debug, Eq, PartialEq, PartialOrd, Ord)]
pub(crate) enum ChineseBasedYearInfo {
    Cache(ChineseBasedCache),
    Data(ChineseBasedCompiledData),
}

impl ChineseBasedYearInfo {
    pub(crate) fn get_new_year(&self) -> RataDie {
        match self {
            Self::Cache(cache) => cache.new_year,
            Self::Data(data) => data.new_year,
        }
    }

    pub(crate) fn get_next_new_year(&self) -> RataDie {
        match self {
            Self::Cache(cache) => cache.next_new_year,
            Self::Data(data) => data.next_new_year(),
        }
    }

    pub(crate) fn get_leap_month(&self) -> Option<NonZeroU8> {
        match self {
            Self::Cache(cache) => cache.leap_month,
            Self::Data(data) => data.leap_month,
        }
    }

    pub(crate) fn get_year_info<C: ChineseBasedWithDataLoading>(year: i32) -> ChineseBasedYearInfo {
        if let Some(data) = C::get_compiled_data_for_year(year) {
            Self::Data(data)
        } else {
            Self::Cache(ChineseBasedDateInner::<C>::compute_cache(year))
        }
    }
}

/// A caching struct used to store information for ChineseBasedDates
#[derive(Copy, Clone, Debug, Eq, PartialEq, PartialOrd, Ord)]
pub(crate) struct ChineseBasedCache {
    pub(crate) new_year: RataDie,
    pub(crate) next_new_year: RataDie,
    pub(crate) leap_month: Option<NonZeroU8>,
}

/// The struct containing compiled ChineseData
///
/// Bit structure:
///
/// ```text
/// Bit:             7   6   5   4   3   2   1   0
/// Byte 0:          [new year offset] | [  month lengths ..
/// Byte 1:          ....... month lengths .......
/// Byte 2:          ... ] | [ leap month index  ]
/// ```
///
/// Where the New Year Offset is the offset from ISO Jan 21 of that year for Chinese New Year,
/// the month lengths are stored as 1 = 30, 0 = 29 for each month including the leap month.
#[derive(Debug, Copy, Clone)]
pub(crate) struct PackedChineseBasedCompiledData(pub(crate) u8, pub(crate) u8, pub(crate) u8);

impl PackedChineseBasedCompiledData {
    pub(crate) fn unpack(self, related_iso: i32) -> ChineseBasedCompiledData {
        fn month_length(is_long: bool) -> u16 {
            if is_long {
                30
            } else {
                29
            }
        }

        let new_year_offset = ((self.0 & 0b11111000) >> 3) as u16;
        let new_year =
            Iso::fixed_from_iso(Iso::iso_from_year_day(related_iso, 21 + new_year_offset).inner);

        let mut last_day_of_month: [u16; 13] = [0; 13];
        let mut months_total = 0;

        months_total += month_length(self.0 & 0b100 != 0);
        last_day_of_month[0] = months_total;
        months_total += month_length(self.0 & 0b010 != 0);
        last_day_of_month[1] = months_total;
        months_total += month_length(self.0 & 0b001 != 0);
        last_day_of_month[2] = months_total;
        months_total += month_length(self.1 & 0b10000000 != 0);
        last_day_of_month[3] = months_total;
        months_total += month_length(self.1 & 0b01000000 != 0);
        last_day_of_month[4] = months_total;
        months_total += month_length(self.1 & 0b00100000 != 0);
        last_day_of_month[5] = months_total;
        months_total += month_length(self.1 & 0b00010000 != 0);
        last_day_of_month[6] = months_total;
        months_total += month_length(self.1 & 0b00001000 != 0);
        last_day_of_month[7] = months_total;
        months_total += month_length(self.1 & 0b00000100 != 0);
        last_day_of_month[8] = months_total;
        months_total += month_length(self.1 & 0b00000010 != 0);
        last_day_of_month[9] = months_total;
        months_total += month_length(self.1 & 0b00000001 != 0);
        last_day_of_month[10] = months_total;
        months_total += month_length(self.2 & 0b10000000 != 0);
        last_day_of_month[11] = months_total;

        let leap_month_bits = self.2 & 0b00111111;
        // Leap month is if the sentinel bit is set
        if leap_month_bits != 0 {
            months_total += month_length(self.2 & 0b01000000 != 0);
        }
        // In non-leap months, `last_day_of_month` will have identical entries at 12 and 11
        last_day_of_month[12] = months_total;

        // Will automatically set to None when the leap month bits are zero
        let leap_month = NonZeroU8::new(leap_month_bits);

        ChineseBasedCompiledData {
            new_year,
            last_day_of_month,
            leap_month,
        }
    }
}
/// A data struct used to load and use information for a set of ChineseBasedDates
#[derive(Copy, Clone, Debug, Eq, PartialEq, PartialOrd, Ord)]
pub(crate) struct ChineseBasedCompiledData {
    pub(crate) new_year: RataDie,
    /// last_day_of_month[12] = last_day_of_month[11] in non-leap years
    /// These days are 1-indexed: so the last day of month for a 30-day 一月 is 30
    /// The array itself is zero-indexed, be careful passing it self.0.month!
    last_day_of_month: [u16; 13],
    ///
    pub(crate) leap_month: Option<NonZeroU8>,
}

impl ChineseBasedCompiledData {
    fn next_new_year(self) -> RataDie {
        self.new_year + i64::from(self.last_day_of_month[12])
    }

    /// The last day of year in the previous month.
    /// `month` is 1-indexed, and the returned value is also
    /// a 1-indexed day of year
    ///
    /// Will be zero for the first month as the last day of the previous month
    /// is not in this year
    fn last_day_of_previous_month(self, month: u8) -> u16 {
        debug_assert!((1..=13).contains(&month), "Month out of bounds!");
        // Get the last day of the previous month.
        // Since `month` is 1-indexed, this needs to subtract *two* to get to the right index of the array
        if month < 2 {
            0
        } else {
            self.last_day_of_month
                .get(usize::from(month - 2))
                .copied()
                .unwrap_or(0)
        }
    }

    /// The last day of year in the current month.
    /// `month` is 1-indexed, and the returned value is also
    /// a 1-indexed day of year
    ///
    /// Will be zero for the first month as the last day of the previous month
    /// is not in this year
    fn last_day_of_month(self, month: u8) -> u16 {
        debug_assert!((1..=13).contains(&month), "Month out of bounds!");
        // Get the last day of the previous month.
        // Since `month` is 1-indexed, this needs to subtract one
        self.last_day_of_month
            .get(usize::from(month - 1))
            .copied()
            .unwrap_or(0)
    }

    fn days_in_month(self, month: u8) -> u8 {
        let ret =
            u8::try_from(self.last_day_of_month(month) - self.last_day_of_previous_month(month));
        debug_assert!(ret.is_ok(), "Month too big!");
        ret.unwrap_or(30)
    }
}

impl<C: ChineseBasedWithDataLoading> ChineseBasedDateInner<C> {
    /// Given a 1-indexed chinese extended year, fetch its data from the cache.
    ///
    /// If the actual year data that was fetched is for a different year, update the getter year
    fn get_compiled_data_for_year_helper(
        date: RataDie,
        getter_year: &mut i32,
    ) -> Option<ChineseBasedCompiledData> {
        let data_option = C::get_compiled_data_for_year(*getter_year);
        // todo we should be able to do this without unpacking
        if let Some(data) = data_option {
            if date < data.new_year {
                *getter_year -= 1;
                C::get_compiled_data_for_year(*getter_year)
            } else if date >= data.next_new_year() {
                *getter_year += 1;
                C::get_compiled_data_for_year(*getter_year)
            } else {
                data_option
            }
        } else {
            None
        }
    }

    /// Get a ChineseBasedDateInner from a fixed date and the cache/extended year associated with it
    fn chinese_based_date_from_cached(
        date: RataDie,
        data: ChineseBasedCompiledData,
        extended_year: i32,
    ) -> ChineseBasedDateInner<C> {
        debug_assert!(
            date < data.next_new_year(),
            "Stored date {date:?} out of bounds!"
        );
        // 1-indexed day of year
        let day_of_year = u16::try_from(date - data.new_year + 1);
        debug_assert!(day_of_year.is_ok(), "Somehow got a very large year in data");
        let day_of_year = day_of_year.unwrap_or(1);
        let mut month = 1;
        // todo perhaps use a binary search
        for iter_month in 1..=13 {
            month = iter_month;
            if data.last_day_of_month(iter_month) >= day_of_year {
                break;
            }
        }

        debug_assert!((1..=13).contains(&month), "Month out of bounds!");

        debug_assert!(
            month < 13 || data.leap_month.is_some(),
            "Cannot have 13 months in a non-leap year!"
        );
        let day_before_month_start = data.last_day_of_previous_month(month);
        let day_of_month = day_of_year - day_before_month_start;
        let day_of_month = u8::try_from(day_of_month);
        debug_assert!(day_of_month.is_ok(), "Month too big!");
        let day_of_month = day_of_month.unwrap_or(1);

        // This can use `new_unchecked` because this function is only ever called from functions which
        // generate the year, month, and day; therefore, there should never be a situation where
        // creating this ArithmeticDate would fail, since the same algorithms used to generate the ymd
        // are also used to check for valid ymd.
        ChineseBasedDateInner(
            ArithmeticDate::new_unchecked(extended_year, month, day_of_month),
            ChineseBasedYearInfo::Data(data),
        )
    }

    /// Get a ChineseBasedDateInner from a fixed date, with the related ISO year
    pub(crate) fn chinese_based_date_from_fixed(
        date: RataDie,
        iso_year: i32,
    ) -> ChineseBasedDateInner<C> {
        // Get the 1-indexed Chinese extended year, used for fetching data from the cache
        let epoch_as_iso = Iso::iso_from_fixed(C::CB::EPOCH);
        let mut getter_year = iso_year - epoch_as_iso.year().number + 1;

        let data_option = Self::get_compiled_data_for_year_helper(date, &mut getter_year);

        if let Some(data) = data_option {
            // cache fetch successful, getter year is just the regular extended year
            Self::chinese_based_date_from_cached(date, data, getter_year)
        } else {
            let date = chinese_based::chinese_based_date_from_fixed::<C::CB>(date);

            let cache = ChineseBasedCache {
                new_year: date.year_bounds.new_year,
                next_new_year: date.year_bounds.next_new_year,
                leap_month: date.leap_month,
            };

            // This can use `new_unchecked` because this function is only ever called from functions which
            // generate the year, month, and day; therefore, there should never be a situation where
            // creating this ArithmeticDate would fail, since the same algorithms used to generate the ymd
            // are also used to check for valid ymd.
            ChineseBasedDateInner(
                ArithmeticDate::new_unchecked(date.year, date.month, date.day),
                ChineseBasedYearInfo::Cache(cache),
            )
        }
    }

    /// Get a RataDie from a ChineseBasedDateInner
    ///
    /// This finds the RataDie of the new year of the year given, then finds the RataDie of the new moon
    /// (beginning of the month) of the month given, then adds the necessary number of days.
    pub(crate) fn fixed_from_chinese_based_date_inner(date: ChineseBasedDateInner<C>) -> RataDie {
        let first_day_of_year = date.1.get_new_year();
        let day_of_year = date.day_of_year(); // 1 indexed
        first_day_of_year + i64::from(day_of_year) - 1
    }

    /// Create a new arithmetic date from a year, month ordinal, and day with bounds checking; returns the
    /// result of creating this arithmetic date, as well as a ChineseBasedYearInfo - either the one passed in
    /// optionally as an argument, or a new ChineseBasedYearInfo for the given year, month, and day args.
    pub(crate) fn new_from_ordinals(
        year: i32,
        month: u8,
        day: u8,
        year_info: &ChineseBasedYearInfo,
    ) -> Result<ArithmeticDate<C>, CalendarError> {
        let max_month = Self::months_in_year_with_info(year_info);
        if !(1..=max_month).contains(&month) {
            return Err(CalendarError::Overflow {
                field: "month",
                max: max_month as usize,
            });
        }

        let max_day = if let ChineseBasedYearInfo::Data(data) = year_info {
            data.days_in_month(month)
        } else {
            chinese_based::days_in_month::<C::CB>(month, year_info.get_new_year(), None).0
        };
        if day > max_day {
            return Err(CalendarError::Overflow {
                field: "day",
                max: max_day as usize,
            });
        }

        // Unchecked can be used because month and day are already checked in this fn

        Ok(ArithmeticDate::<C>::new_unchecked(year, month, day))
    }

    /// Call `months_in_year_with_info` on a `ChineseBasedDateInner`
    pub(crate) fn months_in_year_inner(&self) -> u8 {
        Self::months_in_year_with_info(&self.1)
    }

    /// Return the number of months in a given year, which is 13 in a leap year, and 12 in a common year.
    /// Also takes a `ChineseBasedCache` argument.
    fn months_in_year_with_info(year_info: &ChineseBasedYearInfo) -> u8 {
        if year_info.get_leap_month().is_some() {
            13
        } else {
            12
        }
    }

    /// Calls `days_in_month` on an instance of ChineseBasedDateInner
    pub(crate) fn days_in_month_inner(&self) -> u8 {
        if let ChineseBasedYearInfo::Data(data) = self.1 {
            data.days_in_month(self.0.month)
        } else {
            chinese_based::days_in_month::<C::CB>(self.0.month, self.1.get_new_year(), None).0
        }
    }

    pub(crate) fn fixed_mid_year_from_year(year: i32) -> RataDie {
        chinese_based::fixed_mid_year_from_year::<C::CB>(year)
    }

    /// Calls days_in_year on an instance of ChineseBasedDateInner
    pub(crate) fn days_in_year_inner(&self) -> u16 {
        let next_new_year = self.1.get_next_new_year();
        let new_year = self.1.get_new_year();
        YearBounds {
            new_year,
            next_new_year,
        }
        .count_days()
    }

    /// Calculate the number of days in the year so far for a ChineseBasedDate;
    /// similar to `CalendarArithmetic::day_of_year`
    pub(crate) fn day_of_year(&self) -> u16 {
        let days_until_month = if let ChineseBasedYearInfo::Data(data) = self.1 {
            data.last_day_of_previous_month(self.0.month)
        } else {
            let new_year = self.1.get_new_year();
            chinese_based::days_until_month::<C::CB>(new_year, self.0.month)
        };
        days_until_month + u16::from(self.0.day)
    }

    /// Compute a `ChineseBasedCache` from a ChineseBased year
    pub(crate) fn compute_cache(year: i32) -> ChineseBasedCache {
        let mid_year = Self::fixed_mid_year_from_year(year);
        let year_bounds = YearBounds::compute::<C::CB>(mid_year);
        let YearBounds {
            new_year,
            next_new_year,
            ..
        } = year_bounds;
        let is_leap_year = year_bounds.is_leap();
        let leap_month = if is_leap_year {
            // This doesn't need to be checked for None because `get_leap_month_from_new_year`
            // will always return a value between 1..=13
            NonZeroU8::new(chinese_based::get_leap_month_from_new_year::<C::CB>(
                new_year,
            ))
        } else {
            None
        };
        ChineseBasedCache {
            new_year,
            next_new_year,
            leap_month,
        }
    }
}

impl<C: ChineseBasedWithDataLoading> CalendarArithmetic for C {
    fn month_days(year: i32, month: u8) -> u8 {
        chinese_based::month_days::<C::CB>(year, month)
    }

    /// Returns the number of months in a given year, which is 13 in a leap year, and 12 in a common year.
    fn months_for_every_year(year: i32) -> u8 {
        if Self::is_leap_year(year) {
            13
        } else {
            12
        }
    }

    /// Returns true if the given year is a leap year, and false if not.
    fn is_leap_year(year: i32) -> bool {
        if let Some(data) = C::get_compiled_data_for_year(year) {
            data.leap_month.is_some()
        } else {
            chinese_based::is_leap_year::<C::CB>(year)
        }
    }

    /// Returns the (month, day) of the last day in a Chinese year (the day before Chinese New Year).
    /// The last month in a year will always be 12 in a common year or 13 in a leap year. The day is
    /// determined by finding the day immediately before the next new year and calculating the number
    /// of days since the last new moon (beginning of the last month in the year).
    fn last_month_day_in_year(year: i32) -> (u8, u8) {
        if let Some(data) = C::get_compiled_data_for_year(year) {
            if data.leap_month.is_some() {
                (13, data.days_in_month(13))
            } else {
                (12, data.days_in_month(12))
            }
        } else {
            chinese_based::last_month_day_in_year::<C::CB>(year)
        }
    }

    fn days_in_provided_year(year: i32) -> u16 {
        if let Some(data) = C::get_compiled_data_for_year(year) {
            data.last_day_of_month(13)
        } else {
            chinese_based::days_in_provided_year::<C::CB>(year)
        }
    }
}

/// Get the ordinal lunar month from a code for chinese-based calendars.
pub(crate) fn chinese_based_ordinal_lunar_month_from_code(
    code: MonthCode,
    year_info: ChineseBasedYearInfo,
) -> Option<u8> {
    let leap_month = if let Some(leap) = year_info.get_leap_month() {
        leap.get()
    } else {
        // 14 is a sentinel value, greater than all other months, for the purpose of computation only;
        // it is impossible to actually have 14 months in a year.
        14
    };

    if code.0.len() < 3 {
        return None;
    }
    let bytes = code.0.all_bytes();
    if bytes[0] != b'M' {
        return None;
    }
    if code.0.len() == 4 && bytes[3] != b'L' {
        return None;
    }
    let mut unadjusted = 0;
    if bytes[1] == b'0' {
        if bytes[2] >= b'1' && bytes[2] <= b'9' {
            unadjusted = bytes[2] - b'0';
        }
    } else if bytes[1] == b'1' && bytes[2] >= b'0' && bytes[2] <= b'2' {
        unadjusted = 10 + bytes[2] - b'0';
    }
    if bytes[3] == b'L' {
        if unadjusted + 1 != leap_month {
            return None;
        } else {
            return Some(unadjusted + 1);
        }
    }
    if unadjusted != 0 {
        if unadjusted + 1 > leap_month {
            return Some(unadjusted + 1);
        } else {
            return Some(unadjusted);
        }
    }
    None
}
