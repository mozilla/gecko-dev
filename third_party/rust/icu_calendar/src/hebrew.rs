// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains types and implementations for the Hebrew calendar.
//!
//! ```rust
//! use icu::calendar::{hebrew::Hebrew, Date, DateTime, Ref};
//!
//! let hebrew = Hebrew::new_always_calculating();
//! let hebrew = Ref(&hebrew); // to avoid cloning
//!
//! // `Date` type
//! let hebrew_date =
//!     Date::try_new_hebrew_date_with_calendar(3425, 10, 11, hebrew)
//!         .expect("Failed to initialize hebrew Date instance.");
//!
//! // `DateTime` type
//! let hebrew_datetime = DateTime::try_new_hebrew_datetime_with_calendar(
//!     3425, 10, 11, 13, 1, 0, hebrew,
//! )
//! .expect("Failed to initialize hebrew DateTime instance.");
//!
//! // `Date` checks
//! assert_eq!(hebrew_date.year().number, 3425);
//! assert_eq!(hebrew_date.month().ordinal, 10);
//! assert_eq!(hebrew_date.day_of_month().0, 11);
//!
//! // `DateTime` checks
//! assert_eq!(hebrew_datetime.date.year().number, 3425);
//! assert_eq!(hebrew_datetime.date.month().ordinal, 10);
//! assert_eq!(hebrew_datetime.date.day_of_month().0, 11);
//! assert_eq!(hebrew_datetime.time.hour.number(), 13);
//! assert_eq!(hebrew_datetime.time.minute.number(), 1);
//! assert_eq!(hebrew_datetime.time.second.number(), 0);
//! ```

use crate::calendar_arithmetic::{ArithmeticDate, CalendarArithmetic};
use crate::types::FormattableMonth;
use crate::AnyCalendarKind;
use crate::AsCalendar;
use crate::Iso;
use crate::{types, Calendar, CalendarError, Date, DateDuration, DateDurationUnit, DateTime};
use ::tinystr::tinystr;
use calendrical_calculations::hebrew::BookHebrew;
use calendrical_calculations::rata_die::RataDie;

/// The Civil Hebrew Calendar
///
/// The [Hebrew calendar] is a lunisolar calendar used as the Jewish liturgical calendar
/// as well as an official calendar in Israel.
///
/// This calendar is the _civil_ Hebrew calendar, with the year starting at in the month of Tishrei.
///
/// # Era codes
///
/// This calendar supports a single era code, Anno Mundi, with code `"am"`
///
/// # Month codes
///
/// This calendar is a lunisolar calendar and thus has a leap month. It supports codes `"M01"-"M12"`
/// for regular months, and the leap month Adar I being coded as `"M05L"`.
///
/// [`FormattableMonth`] has slightly divergent behavior: because the regular month Adar is formatted
/// as "Adar II" in a leap year, this calendar will produce the special code `"M06L"` in any [`FormattableMonth`]
/// objects it creates.
///
/// [Hebrew calendar]: https://en.wikipedia.org/wiki/Hebrew_calendar
#[derive(Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
#[non_exhaustive] // we'll be adding precompiled data to this
pub struct Hebrew;

/// The inner date type used for representing [`Date`]s of [`BookHebrew`]. See [`Date`] and [`BookHebrew`] for more details.
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
struct BookHebrewDateInner;
/// The inner date type used for representing [`Date`]s of [`Hebrew`]. See [`Date`] and [`Hebrew`] for more details.
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
pub struct HebrewDateInner(ArithmeticDate<Hebrew>);

impl Hebrew {
    /// Construct a new [`Hebrew`] without any precomputed calendrical calculations.
    ///
    /// This is the only mode currently possible, but once precomputing is available (#3933)
    /// there will be additional constructors that load from data providers.
    pub fn new_always_calculating() -> Self {
        Hebrew
    }
}

//  HEBREW CALENDAR

impl CalendarArithmetic for Hebrew {
    fn month_days(civil_year: i32, civil_month: u8) -> u8 {
        Self::last_day_of_civil_hebrew_month(civil_year, civil_month)
    }

    fn months_for_every_year(civil_year: i32) -> u8 {
        Self::last_month_of_civil_hebrew_year(civil_year)
    }

    fn days_in_provided_year(civil_year: i32) -> u16 {
        BookHebrew::days_in_book_hebrew_year(civil_year) // number of days don't change between BookHebrew and Civil Hebrew
    }

    fn is_leap_year(civil_year: i32) -> bool {
        // civil and book years are the same
        BookHebrew::is_hebrew_leap_year(civil_year)
    }

    fn last_month_day_in_year(civil_year: i32) -> (u8, u8) {
        let civil_month = Self::last_month_of_civil_hebrew_year(civil_year);
        let civil_day = Self::last_day_of_civil_hebrew_month(civil_year, civil_month);

        (civil_month, civil_day)
    }
}

impl Calendar for Hebrew {
    type DateInner = HebrewDateInner;

    fn date_from_codes(
        &self,
        era: types::Era,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, CalendarError> {
        let is_leap_year = Self::is_leap_year(year);
        let year = if era.0 == tinystr!(16, "hebrew") || era.0 == tinystr!(16, "am") {
            year
        } else {
            return Err(CalendarError::UnknownEra(era.0, self.debug_name()));
        };

        let month_code_str = month_code.0.as_str();

        let month_ordinal = if is_leap_year {
            match month_code_str {
                "M01" => 1,
                "M02" => 2,
                "M03" => 3,
                "M04" => 4,
                "M05" => 5,
                "M05L" => 6,
                // M06L is the formatting era code used for Adar II
                "M06" | "M06L" => 7,
                "M07" => 8,
                "M08" => 9,
                "M09" => 10,
                "M10" => 11,
                "M11" => 12,
                "M12" => 13,
                _ => {
                    return Err(CalendarError::UnknownMonthCode(
                        month_code.0,
                        self.debug_name(),
                    ))
                }
            }
        } else {
            match month_code_str {
                "M01" => 1,
                "M02" => 2,
                "M03" => 3,
                "M04" => 4,
                "M05" => 5,
                "M06" => 6,
                "M07" => 7,
                "M08" => 8,
                "M09" => 9,
                "M10" => 10,
                "M11" => 11,
                "M12" => 12,
                _ => {
                    return Err(CalendarError::UnknownMonthCode(
                        month_code.0,
                        self.debug_name(),
                    ))
                }
            }
        };

        ArithmeticDate::new_from_lunar_ordinals(year, month_ordinal, day).map(HebrewDateInner)
    }

    fn date_from_iso(&self, iso: Date<Iso>) -> Self::DateInner {
        let fixed_iso = Iso::fixed_from_iso(*iso.inner());
        Self::civil_hebrew_from_fixed(fixed_iso).inner
    }

    fn date_to_iso(&self, date: &Self::DateInner) -> Date<Iso> {
        let fixed_hebrew = Self::fixed_from_civil_hebrew(*date);
        Iso::iso_from_fixed(fixed_hebrew)
    }

    fn months_in_year(&self, date: &Self::DateInner) -> u8 {
        date.0.months_in_year()
    }

    fn days_in_year(&self, date: &Self::DateInner) -> u16 {
        date.0.days_in_year()
    }

    fn days_in_month(&self, date: &Self::DateInner) -> u8 {
        date.0.days_in_month()
    }

    fn offset_date(&self, date: &mut Self::DateInner, offset: DateDuration<Self>) {
        date.0.offset_date(offset)
    }

    fn until(
        &self,
        date1: &Self::DateInner,
        date2: &Self::DateInner,
        _calendar2: &Self,
        _largest_unit: DateDurationUnit,
        _smallest_unit: DateDurationUnit,
    ) -> DateDuration<Self> {
        date1.0.until(date2.0, _largest_unit, _smallest_unit)
    }

    fn debug_name(&self) -> &'static str {
        "Hebrew"
    }

    fn year(&self, date: &Self::DateInner) -> types::FormattableYear {
        Self::year_as_hebrew(date.0.year)
    }

    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        Self::is_leap_year(date.0.year)
    }

    fn month(&self, date: &Self::DateInner) -> FormattableMonth {
        let mut ordinal = date.0.month;
        let is_leap_year = Self::is_leap_year(date.0.year);

        if is_leap_year {
            if ordinal == 6 {
                return types::FormattableMonth {
                    ordinal: ordinal as u32,
                    code: types::MonthCode(tinystr!(4, "M05L")),
                };
            } else if ordinal == 7 {
                return types::FormattableMonth {
                    ordinal: ordinal as u32,
                    code: types::MonthCode(tinystr!(4, "M06L")),
                };
            }
        }

        if is_leap_year && ordinal > 6 {
            ordinal -= 1;
        }

        let code = match ordinal {
            1 => tinystr!(4, "M01"),
            2 => tinystr!(4, "M02"),
            3 => tinystr!(4, "M03"),
            4 => tinystr!(4, "M04"),
            5 => tinystr!(4, "M05"),
            6 => tinystr!(4, "M06"),
            7 => tinystr!(4, "M07"),
            8 => tinystr!(4, "M08"),
            9 => tinystr!(4, "M09"),
            10 => tinystr!(4, "M10"),
            11 => tinystr!(4, "M11"),
            12 => tinystr!(4, "M12"),
            _ => tinystr!(4, "und"),
        };

        types::FormattableMonth {
            ordinal: date.0.month as u32,
            code: types::MonthCode(code),
        }
    }

    fn day_of_month(&self, date: &Self::DateInner) -> types::DayOfMonth {
        date.0.day_of_month()
    }

    fn day_of_year_info(&self, date: &Self::DateInner) -> types::DayOfYearInfo {
        let prev_year = date.0.year.saturating_sub(1);
        let next_year = date.0.year.saturating_add(1);
        types::DayOfYearInfo {
            day_of_year: date.0.day_of_year(),
            days_in_year: date.0.days_in_year(),
            prev_year: Self::year_as_hebrew(prev_year),
            days_in_prev_year: Self::days_in_provided_year(prev_year),
            next_year: Self::year_as_hebrew(next_year),
        }
    }
    fn any_calendar_kind(&self) -> Option<AnyCalendarKind> {
        Some(AnyCalendarKind::Hebrew)
    }
}

impl Hebrew {
    // Converts a Biblical Hebrew Date to a Civil Hebrew Date
    fn biblical_to_civil_date(biblical_date: BookHebrew) -> HebrewDateInner {
        let (y, m, d) = biblical_date.to_civil_date();

        debug_assert!(ArithmeticDate::<Hebrew>::new_from_lunar_ordinals(y, m, d,).is_ok());
        HebrewDateInner(ArithmeticDate::new_unchecked(y, m, d))
    }

    // Converts a Civil Hebrew Date to a Biblical Hebrew Date
    fn civil_to_biblical_date(civil_date: HebrewDateInner) -> BookHebrew {
        BookHebrew::from_civil_date(civil_date.0.year, civil_date.0.month, civil_date.0.day)
    }

    fn last_month_of_civil_hebrew_year(civil_year: i32) -> u8 {
        if Self::is_leap_year(civil_year) {
            13 // there are 13 months in a leap year
        } else {
            12
        }
    }

    fn last_day_of_civil_hebrew_month(civil_year: i32, civil_month: u8) -> u8 {
        let book_date = Hebrew::civil_to_biblical_date(HebrewDateInner(
            ArithmeticDate::new_unchecked(civil_year, civil_month, 1),
        ));
        BookHebrew::last_day_of_book_hebrew_month(book_date.year, book_date.month)
    }

    // "Fixed" is a day count representation of calendars staring from Jan 1st of year 1 of the Georgian Calendar.
    fn fixed_from_civil_hebrew(date: HebrewDateInner) -> RataDie {
        let book_date = Hebrew::civil_to_biblical_date(date);
        BookHebrew::fixed_from_book_hebrew(book_date)
    }

    fn civil_hebrew_from_fixed(date: RataDie) -> Date<Hebrew> {
        let book_hebrew = BookHebrew::book_hebrew_from_fixed(date);
        Date::from_raw(Hebrew::biblical_to_civil_date(book_hebrew), Hebrew)
    }

    fn year_as_hebrew(civil_year: i32) -> types::FormattableYear {
        types::FormattableYear {
            era: types::Era(tinystr!(16, "hebrew")),
            number: civil_year,
            cyclic: None,
            related_iso: None,
        }
    }
}

impl<A: AsCalendar<Calendar = Hebrew>> Date<A> {
    /// Construct new Hebrew Date.
    ///
    /// This datetime will not use any precomputed calendrical calculations,
    /// one that loads such data from a provider will be added in the future (#3933)
    ///
    ///
    /// ```rust
    /// use icu::calendar::hebrew::Hebrew;
    /// use icu::calendar::Date;
    ///
    /// let hebrew = Hebrew::new_always_calculating();
    ///
    /// let date_hebrew =
    ///     Date::try_new_hebrew_date_with_calendar(3425, 4, 25, hebrew)
    ///         .expect("Failed to initialize Hebrew Date instance.");
    ///
    /// assert_eq!(date_hebrew.year().number, 3425);
    /// assert_eq!(date_hebrew.month().ordinal, 4);
    /// assert_eq!(date_hebrew.day_of_month().0, 25);
    /// ```
    pub fn try_new_hebrew_date_with_calendar(
        year: i32,
        month: u8,
        day: u8,
        calendar: A,
    ) -> Result<Date<A>, CalendarError> {
        ArithmeticDate::new_from_lunar_ordinals(year, month, day)
            .map(HebrewDateInner)
            .map(|inner| Date::from_raw(inner, calendar))
    }
}

impl<A: AsCalendar<Calendar = Hebrew>> DateTime<A> {
    /// Construct a new Hebrew datetime from integers.
    ///
    /// This datetime will not use any precomputed calendrical calculations,
    /// one that loads such data from a provider will be added in the future (#3933)
    ///
    /// ```rust
    /// use icu::calendar::hebrew::Hebrew;
    /// use icu::calendar::DateTime;
    ///
    /// let hebrew = Hebrew::new_always_calculating();
    ///
    /// let datetime_hebrew = DateTime::try_new_hebrew_datetime_with_calendar(
    ///     4201, 10, 11, 13, 1, 0, hebrew,
    /// )
    /// .expect("Failed to initialize Hebrew DateTime instance");
    ///
    /// assert_eq!(datetime_hebrew.date.year().number, 4201);
    /// assert_eq!(datetime_hebrew.date.month().ordinal, 10);
    /// assert_eq!(datetime_hebrew.date.day_of_month().0, 11);
    /// assert_eq!(datetime_hebrew.time.hour.number(), 13);
    /// assert_eq!(datetime_hebrew.time.minute.number(), 1);
    /// assert_eq!(datetime_hebrew.time.second.number(), 0);
    /// ```
    pub fn try_new_hebrew_datetime_with_calendar(
        year: i32,
        month: u8,
        day: u8,
        hour: u8,
        minute: u8,
        second: u8,
        calendar: A,
    ) -> Result<DateTime<A>, CalendarError> {
        Ok(DateTime {
            date: Date::try_new_hebrew_date_with_calendar(year, month, day, calendar)?,
            time: types::Time::try_new(hour, minute, second, 0)?,
        })
    }
}

#[cfg(test)]
mod tests {

    use super::*;
    use calendrical_calculations::hebrew::*;

    #[test]
    fn test_conversions() {
        let iso_dates: [Date<Iso>; 48] = [
            Date::try_new_iso_date(2021, 1, 10).unwrap(),
            Date::try_new_iso_date(2021, 1, 25).unwrap(),
            Date::try_new_iso_date(2021, 2, 10).unwrap(),
            Date::try_new_iso_date(2021, 2, 25).unwrap(),
            Date::try_new_iso_date(2021, 3, 10).unwrap(),
            Date::try_new_iso_date(2021, 3, 25).unwrap(),
            Date::try_new_iso_date(2021, 4, 10).unwrap(),
            Date::try_new_iso_date(2021, 4, 25).unwrap(),
            Date::try_new_iso_date(2021, 5, 10).unwrap(),
            Date::try_new_iso_date(2021, 5, 25).unwrap(),
            Date::try_new_iso_date(2021, 6, 10).unwrap(),
            Date::try_new_iso_date(2021, 6, 25).unwrap(),
            Date::try_new_iso_date(2021, 7, 10).unwrap(),
            Date::try_new_iso_date(2021, 7, 25).unwrap(),
            Date::try_new_iso_date(2021, 8, 10).unwrap(),
            Date::try_new_iso_date(2021, 8, 25).unwrap(),
            Date::try_new_iso_date(2021, 9, 10).unwrap(),
            Date::try_new_iso_date(2021, 9, 25).unwrap(),
            Date::try_new_iso_date(2021, 10, 10).unwrap(),
            Date::try_new_iso_date(2021, 10, 25).unwrap(),
            Date::try_new_iso_date(2021, 11, 10).unwrap(),
            Date::try_new_iso_date(2021, 11, 25).unwrap(),
            Date::try_new_iso_date(2021, 12, 10).unwrap(),
            Date::try_new_iso_date(2021, 12, 25).unwrap(),
            Date::try_new_iso_date(2022, 1, 10).unwrap(),
            Date::try_new_iso_date(2022, 1, 25).unwrap(),
            Date::try_new_iso_date(2022, 2, 10).unwrap(),
            Date::try_new_iso_date(2022, 2, 25).unwrap(),
            Date::try_new_iso_date(2022, 3, 10).unwrap(),
            Date::try_new_iso_date(2022, 3, 25).unwrap(),
            Date::try_new_iso_date(2022, 4, 10).unwrap(),
            Date::try_new_iso_date(2022, 4, 25).unwrap(),
            Date::try_new_iso_date(2022, 5, 10).unwrap(),
            Date::try_new_iso_date(2022, 5, 25).unwrap(),
            Date::try_new_iso_date(2022, 6, 10).unwrap(),
            Date::try_new_iso_date(2022, 6, 25).unwrap(),
            Date::try_new_iso_date(2022, 7, 10).unwrap(),
            Date::try_new_iso_date(2022, 7, 25).unwrap(),
            Date::try_new_iso_date(2022, 8, 10).unwrap(),
            Date::try_new_iso_date(2022, 8, 25).unwrap(),
            Date::try_new_iso_date(2022, 9, 10).unwrap(),
            Date::try_new_iso_date(2022, 9, 25).unwrap(),
            Date::try_new_iso_date(2022, 10, 10).unwrap(),
            Date::try_new_iso_date(2022, 10, 25).unwrap(),
            Date::try_new_iso_date(2022, 11, 10).unwrap(),
            Date::try_new_iso_date(2022, 11, 25).unwrap(),
            Date::try_new_iso_date(2022, 12, 10).unwrap(),
            Date::try_new_iso_date(2022, 12, 25).unwrap(),
        ];

        let book_hebrew_dates: [(u8, u8, i32); 48] = [
            (26, TEVET, 5781),
            (12, SHEVAT, 5781),
            (28, SHEVAT, 5781),
            (13, ADAR, 5781),
            (26, ADAR, 5781),
            (12, NISAN, 5781),
            (28, NISAN, 5781),
            (13, IYYAR, 5781),
            (28, IYYAR, 5781),
            (14, SIVAN, 5781),
            (30, SIVAN, 5781),
            (15, TAMMUZ, 5781),
            (1, AV, 5781),
            (16, AV, 5781),
            (2, ELUL, 5781),
            (17, ELUL, 5781),
            (4, TISHRI, 5782),
            (19, TISHRI, 5782),
            (4, MARHESHVAN, 5782),
            (19, MARHESHVAN, 5782),
            (6, KISLEV, 5782),
            (21, KISLEV, 5782),
            (6, TEVET, 5782),
            (21, TEVET, 5782),
            (8, SHEVAT, 5782),
            (23, SHEVAT, 5782),
            (9, ADAR, 5782),
            (24, ADAR, 5782),
            (7, ADARII, 5782),
            (22, ADARII, 5782),
            (9, NISAN, 5782),
            (24, NISAN, 5782),
            (9, IYYAR, 5782),
            (24, IYYAR, 5782),
            (11, SIVAN, 5782),
            (26, SIVAN, 5782),
            (11, TAMMUZ, 5782),
            (26, TAMMUZ, 5782),
            (13, AV, 5782),
            (28, AV, 5782),
            (14, ELUL, 5782),
            (29, ELUL, 5782),
            (15, TISHRI, 5783),
            (30, TISHRI, 5783),
            (16, MARHESHVAN, 5783),
            (1, KISLEV, 5783),
            (16, KISLEV, 5783),
            (1, TEVET, 5783),
        ];

        let civil_hebrew_dates: [(u8, u8, i32); 48] = [
            (26, 4, 5781),
            (12, 5, 5781),
            (28, 5, 5781),
            (13, 6, 5781),
            (26, 6, 5781),
            (12, 7, 5781),
            (28, 7, 5781),
            (13, 8, 5781),
            (28, 8, 5781),
            (14, 9, 5781),
            (30, 9, 5781),
            (15, 10, 5781),
            (1, 11, 5781),
            (16, 11, 5781),
            (2, 12, 5781),
            (17, 12, 5781),
            (4, 1, 5782),
            (19, 1, 5782),
            (4, 2, 5782),
            (19, 2, 5782),
            (6, 3, 5782),
            (21, 3, 5782),
            (6, 4, 5782),
            (21, 4, 5782),
            (8, 5, 5782),
            (23, 5, 5782),
            (9, 6, 5782),
            (24, 6, 5782),
            (7, 7, 5782),
            (22, 7, 5782),
            (9, 8, 5782),
            (24, 8, 5782),
            (9, 9, 5782),
            (24, 9, 5782),
            (11, 10, 5782),
            (26, 10, 5782),
            (11, 11, 5782),
            (26, 11, 5782),
            (13, 12, 5782),
            (28, 12, 5782),
            (14, 13, 5782),
            (29, 13, 5782),
            (15, 1, 5783),
            (30, 1, 5783),
            (16, 2, 5783),
            (1, 3, 5783),
            (16, 3, 5783),
            (1, 4, 5783),
        ];

        for (iso_date, (book_date_nums, civil_date_nums)) in iso_dates
            .iter()
            .zip(book_hebrew_dates.iter().zip(civil_hebrew_dates.iter()))
        {
            let book_date = BookHebrew {
                year: book_date_nums.2,
                month: book_date_nums.1,
                day: book_date_nums.0,
            };
            let civil_date: HebrewDateInner = HebrewDateInner(ArithmeticDate::new_unchecked(
                civil_date_nums.2,
                civil_date_nums.1,
                civil_date_nums.0,
            ));

            let book_to_civil = Hebrew::biblical_to_civil_date(book_date);
            let civil_to_book = Hebrew::civil_to_biblical_date(civil_date);

            assert_eq!(civil_date, book_to_civil);
            assert_eq!(book_date, civil_to_book);

            let iso_to_fixed = Iso::fixed_from_iso(iso_date.inner);
            let fixed_to_hebrew = Hebrew::civil_hebrew_from_fixed(iso_to_fixed);

            let hebrew_to_fixed = Hebrew::fixed_from_civil_hebrew(civil_date);
            let fixed_to_iso = Iso::iso_from_fixed(hebrew_to_fixed);

            assert_eq!(fixed_to_hebrew.inner, civil_date);
            assert_eq!(fixed_to_iso.inner, iso_date.inner);
        }
    }

    #[test]
    fn test_icu_bug_22441() {
        assert_eq!(BookHebrew::days_in_book_hebrew_year(88369), 383);
    }
}
