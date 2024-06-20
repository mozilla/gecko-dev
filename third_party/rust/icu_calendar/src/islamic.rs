// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains types and implementations for the Islamic calendars.
//!
//! ```rust
//! use icu::calendar::islamic::IslamicObservational;
//! use icu::calendar::{Date, DateTime, Ref};
//!
//! let islamic = IslamicObservational::new_always_calculating();
//! let islamic = Ref(&islamic); // to avoid cloning
//!
//! // `Date` type
//! let islamic_date =
//!     Date::try_new_observational_islamic_date(1348, 10, 11, islamic)
//!         .expect("Failed to initialize islamic Date instance.");
//!
//! // `DateTime` type
//! let islamic_datetime = DateTime::try_new_observational_islamic_datetime(
//!     1348, 10, 11, 13, 1, 0, islamic,
//! )
//! .expect("Failed to initialize islamic DateTime instance.");
//!
//! // `Date` checks
//! assert_eq!(islamic_date.year().number, 1348);
//! assert_eq!(islamic_date.month().ordinal, 10);
//! assert_eq!(islamic_date.day_of_month().0, 11);
//!
//! // `DateTime` checks
//! assert_eq!(islamic_datetime.date.year().number, 1348);
//! assert_eq!(islamic_datetime.date.month().ordinal, 10);
//! assert_eq!(islamic_datetime.date.day_of_month().0, 11);
//! assert_eq!(islamic_datetime.time.hour.number(), 13);
//! assert_eq!(islamic_datetime.time.minute.number(), 1);
//! assert_eq!(islamic_datetime.time.second.number(), 0);
//! ```

use crate::calendar_arithmetic::{ArithmeticDate, CalendarArithmetic};
use crate::AnyCalendarKind;
use crate::AsCalendar;
use crate::Iso;
use crate::{types, Calendar, CalendarError, Date, DateDuration, DateDurationUnit, DateTime};
use calendrical_calculations::rata_die::RataDie;
use tinystr::tinystr;

/// Islamic Observational Calendar (Default)
///
/// # Era codes
///
/// This calendar supports a single era code, Anno Mundi, with code `"ah"`
///
/// # Month codes
///
/// This calendar is a pure lunar calendar with no leap months. It uses month codes
/// `"M01" - "M12"`.
#[derive(Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
#[non_exhaustive] // we'll be adding precompiled data to this
pub struct IslamicObservational;

/// Civil / Arithmetical Islamic Calendar (Used for administrative purposes)
///
/// # Era codes
///
/// This calendar supports a single era code, Anno Mundi, with code `"ah"`
///
/// # Month codes
///
/// This calendar is a pure lunar calendar with no leap months. It uses month codes
/// `"M01" - "M12"`.
#[derive(Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
#[non_exhaustive] // we'll be adding precompiled data to this
pub struct IslamicCivil;

/// Umm al-Qura Hijri Calendar (Used in Saudi Arabia)
///
/// # Era codes
///
/// This calendar supports a single era code, Anno Mundi, with code `"ah"`
///
/// # Month codes
///
/// This calendar is a pure lunar calendar with no leap months. It uses month codes
/// `"M01" - "M12"`.
#[derive(Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
#[non_exhaustive] // we'll be adding precompiled data to this
pub struct IslamicUmmAlQura;

/// A Tabular version of the Arithmetical Islamic Calendar
///
/// # Era codes
///
/// This calendar supports a single era code, Anno Mundi, with code `"ah"`
///
/// # Month codes
///
/// This calendar is a pure lunar calendar with no leap months. It uses month codes
/// `"M01" - "M12"`.
#[derive(Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
#[non_exhaustive] // we'll be adding precompiled data to this
pub struct IslamicTabular;

impl IslamicObservational {
    /// Construct a new [`IslamicObservational`] without any precomputed calendrical calculations.
    ///
    /// This is the only mode currently possible, but once precomputing is available (#3933)
    /// there will be additional constructors that load from data providers.
    pub fn new_always_calculating() -> Self {
        IslamicObservational
    }
}

impl IslamicCivil {
    /// Construct a new [`IslamicCivil`] without any precomputed calendrical calculations.
    ///
    /// This is the only mode currently possible, but once precomputing is available (#3933)
    /// there will be additional constructors that load from data providers.
    pub fn new_always_calculating() -> Self {
        IslamicCivil
    }
}

impl IslamicUmmAlQura {
    /// Construct a new [`IslamicUmmAlQura`] without any precomputed calendrical calculations.
    ///
    /// This is the only mode currently possible, but once precomputing is available (#3933)
    /// there will be additional constructors that load from data providers.
    pub fn new_always_calculating() -> Self {
        IslamicUmmAlQura
    }
}

impl IslamicTabular {
    /// Construct a new [`IslamicTabular`] without any precomputed calendrical calculations.
    ///
    /// This is the only mode currently possible, but once precomputing is available (#3933)
    /// there will be additional constructors that load from data providers.
    pub fn new_always_calculating() -> Self {
        IslamicTabular
    }
}

/// The inner date type used for representing [`Date`]s of [`IslamicObservational`]. See [`Date`] and [`IslamicObservational`] for more details.

#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
pub struct IslamicDateInner(ArithmeticDate<IslamicObservational>);

impl CalendarArithmetic for IslamicObservational {
    fn month_days(year: i32, month: u8) -> u8 {
        calendrical_calculations::islamic::observational_islamic_month_days(year, month)
    }

    fn months_for_every_year(_year: i32) -> u8 {
        12
    }

    fn days_in_provided_year(year: i32) -> u16 {
        (1..=12)
            .map(|month| IslamicObservational::month_days(year, month) as u16)
            .sum()
    }

    // As an observational-lunar calendar, it does not have leap years.
    fn is_leap_year(_year: i32) -> bool {
        false
    }

    fn last_month_day_in_year(year: i32) -> (u8, u8) {
        let days = Self::month_days(year, 12);

        (12, days)
    }
}

impl Calendar for IslamicObservational {
    type DateInner = IslamicDateInner;
    fn date_from_codes(
        &self,
        era: types::Era,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, CalendarError> {
        let year = if era.0 == tinystr!(16, "islamic") || era.0 == tinystr!(16, "ah") {
            year
        } else {
            return Err(CalendarError::UnknownEra(era.0, self.debug_name()));
        };

        ArithmeticDate::new_from_codes(self, year, month_code, day).map(IslamicDateInner)
    }

    fn date_from_iso(&self, iso: Date<crate::Iso>) -> Self::DateInner {
        let fixed_iso = Iso::fixed_from_iso(*iso.inner());
        Self::islamic_from_fixed(fixed_iso).inner
    }

    fn date_to_iso(&self, date: &Self::DateInner) -> Date<crate::Iso> {
        let fixed_islamic = Self::fixed_from_islamic(*date);
        Iso::iso_from_fixed(fixed_islamic)
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

    fn day_of_week(&self, date: &Self::DateInner) -> types::IsoWeekday {
        Iso.day_of_week(self.date_to_iso(date).inner())
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
        "Islamic (observational)"
    }

    fn year(&self, date: &Self::DateInner) -> types::FormattableYear {
        Self::year_as_islamic(date.0.year)
    }

    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        Self::is_leap_year(date.0.year)
    }

    fn month(&self, date: &Self::DateInner) -> types::FormattableMonth {
        date.0.month()
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
            prev_year: Self::year_as_islamic(prev_year),
            days_in_prev_year: Self::days_in_provided_year(prev_year),
            next_year: Self::year_as_islamic(next_year),
        }
    }

    fn any_calendar_kind(&self) -> Option<AnyCalendarKind> {
        Some(AnyCalendarKind::IslamicObservational)
    }
}

impl IslamicObservational {
    fn fixed_from_islamic(i_date: IslamicDateInner) -> RataDie {
        calendrical_calculations::islamic::fixed_from_islamic_observational(
            i_date.0.year,
            i_date.0.month,
            i_date.0.day,
        )
    }

    fn islamic_from_fixed(date: RataDie) -> Date<IslamicObservational> {
        let (y, m, d) = calendrical_calculations::islamic::observational_islamic_from_fixed(date);

        debug_assert!(Date::try_new_observational_islamic_date(
            y,
            m,
            d,
            IslamicObservational::new_always_calculating()
        )
        .is_ok());
        Date::from_raw(
            IslamicDateInner(ArithmeticDate::new_unchecked(y, m, d)),
            IslamicObservational,
        )
    }

    fn year_as_islamic(year: i32) -> types::FormattableYear {
        types::FormattableYear {
            era: types::Era(tinystr!(16, "islamic")),
            number: year,
            cyclic: None,
            related_iso: None,
        }
    }
}

impl<A: AsCalendar<Calendar = IslamicObservational>> Date<A> {
    /// Construct new Islamic Observational Date.
    ///
    /// Has no negative years, only era is the AH.
    ///
    /// ```rust
    /// use icu::calendar::islamic::IslamicObservational;
    /// use icu::calendar::Date;
    ///
    /// let islamic = IslamicObservational::new_always_calculating();
    ///
    /// let date_islamic =
    ///     Date::try_new_observational_islamic_date(1392, 4, 25, islamic)
    ///         .expect("Failed to initialize Islamic Date instance.");
    ///
    /// assert_eq!(date_islamic.year().number, 1392);
    /// assert_eq!(date_islamic.month().ordinal, 4);
    /// assert_eq!(date_islamic.day_of_month().0, 25);
    /// ```
    pub fn try_new_observational_islamic_date(
        year: i32,
        month: u8,
        day: u8,
        calendar: A,
    ) -> Result<Date<A>, CalendarError> {
        ArithmeticDate::new_from_lunar_ordinals(year, month, day)
            .map(IslamicDateInner)
            .map(|inner| Date::from_raw(inner, calendar))
    }
}

impl<A: AsCalendar<Calendar = IslamicObservational>> DateTime<A> {
    /// Construct a new Islamic Observational datetime from integers.
    ///
    /// ```rust
    /// use icu::calendar::islamic::IslamicObservational;
    /// use icu::calendar::DateTime;
    ///
    /// let islamic = IslamicObservational::new_always_calculating();
    ///
    /// let datetime_islamic = DateTime::try_new_observational_islamic_datetime(
    ///     474, 10, 11, 13, 1, 0, islamic,
    /// )
    /// .expect("Failed to initialize Islamic DateTime instance.");
    ///
    /// assert_eq!(datetime_islamic.date.year().number, 474);
    /// assert_eq!(datetime_islamic.date.month().ordinal, 10);
    /// assert_eq!(datetime_islamic.date.day_of_month().0, 11);
    /// assert_eq!(datetime_islamic.time.hour.number(), 13);
    /// assert_eq!(datetime_islamic.time.minute.number(), 1);
    /// assert_eq!(datetime_islamic.time.second.number(), 0);
    /// ```
    pub fn try_new_observational_islamic_datetime(
        year: i32,
        month: u8,
        day: u8,
        hour: u8,
        minute: u8,
        second: u8,
        calendar: A,
    ) -> Result<DateTime<A>, CalendarError> {
        Ok(DateTime {
            date: Date::try_new_observational_islamic_date(year, month, day, calendar)?,
            time: types::Time::try_new(hour, minute, second, 0)?,
        })
    }
}

#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
/// The inner date type used for representing [`Date`]s of [`IslamicUmmAlQura`]. See [`Date`] and [`IslamicUmmAlQura`] for more details.
pub struct IslamicUmmAlQuraDateInner(ArithmeticDate<IslamicUmmAlQura>);

impl CalendarArithmetic for IslamicUmmAlQura {
    fn month_days(year: i32, month: u8) -> u8 {
        calendrical_calculations::islamic::saudi_islamic_month_days(year, month)
    }

    fn months_for_every_year(_year: i32) -> u8 {
        12
    }

    fn days_in_provided_year(year: i32) -> u16 {
        (1..=12)
            .map(|month| IslamicUmmAlQura::month_days(year, month) as u16)
            .sum()
    }

    // As an observational-lunar calendar, it does not have leap years.
    fn is_leap_year(_year: i32) -> bool {
        false
    }

    fn last_month_day_in_year(year: i32) -> (u8, u8) {
        let days = Self::month_days(year, 12);

        (12, days)
    }
}

impl Calendar for IslamicUmmAlQura {
    type DateInner = IslamicUmmAlQuraDateInner;
    fn date_from_codes(
        &self,
        era: types::Era,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, CalendarError> {
        let year = if era.0 == tinystr!(16, "islamic-umalqura")
            || era.0 == tinystr!(16, "islamic")
            || era.0 == tinystr!(16, "ah")
        {
            year
        } else {
            return Err(CalendarError::UnknownEra(era.0, self.debug_name()));
        };

        ArithmeticDate::new_from_codes(self, year, month_code, day).map(IslamicUmmAlQuraDateInner)
    }

    fn date_from_iso(&self, iso: Date<Iso>) -> Self::DateInner {
        let fixed_iso = Iso::fixed_from_iso(*iso.inner());
        Self::saudi_islamic_from_fixed(fixed_iso).inner
    }

    fn date_to_iso(&self, date: &Self::DateInner) -> Date<Iso> {
        let fixed_islamic = Self::fixed_from_saudi_islamic(*date);
        Iso::iso_from_fixed(fixed_islamic)
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
        "Islamic (Umm al-Qura)"
    }

    fn year(&self, date: &Self::DateInner) -> types::FormattableYear {
        Self::year_as_islamic(date.0.year)
    }

    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        Self::is_leap_year(date.0.year)
    }

    fn month(&self, date: &Self::DateInner) -> types::FormattableMonth {
        date.0.month()
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
            prev_year: Self::year_as_islamic(prev_year),
            days_in_prev_year: Self::days_in_provided_year(prev_year),
            next_year: Self::year_as_islamic(next_year),
        }
    }

    fn any_calendar_kind(&self) -> Option<AnyCalendarKind> {
        Some(AnyCalendarKind::IslamicUmmAlQura)
    }
}

impl<A: AsCalendar<Calendar = IslamicUmmAlQura>> Date<A> {
    /// Construct new Islamic Umm al-Qura Date.
    ///
    /// Has no negative years, only era is the AH.
    ///
    /// ```rust
    /// use icu::calendar::islamic::IslamicUmmAlQura;
    /// use icu::calendar::Date;
    ///
    /// let islamic = IslamicUmmAlQura::new_always_calculating();
    ///
    /// let date_islamic = Date::try_new_ummalqura_date(1392, 4, 25, islamic)
    ///     .expect("Failed to initialize Islamic Date instance.");
    ///
    /// assert_eq!(date_islamic.year().number, 1392);
    /// assert_eq!(date_islamic.month().ordinal, 4);
    /// assert_eq!(date_islamic.day_of_month().0, 25);
    /// ```
    pub fn try_new_ummalqura_date(
        year: i32,
        month: u8,
        day: u8,
        calendar: A,
    ) -> Result<Date<A>, CalendarError> {
        ArithmeticDate::new_from_lunar_ordinals(year, month, day)
            .map(IslamicUmmAlQuraDateInner)
            .map(|inner| Date::from_raw(inner, calendar))
    }
}

impl<A: AsCalendar<Calendar = IslamicUmmAlQura>> DateTime<A> {
    /// Construct a new Islamic Umm al-Qura datetime from integers.
    ///
    /// ```rust
    /// use icu::calendar::islamic::IslamicUmmAlQura;
    /// use icu::calendar::DateTime;
    ///
    /// let islamic = IslamicUmmAlQura::new_always_calculating();
    ///
    /// let datetime_islamic =
    ///     DateTime::try_new_ummalqura_datetime(474, 10, 11, 13, 1, 0, islamic)
    ///         .expect("Failed to initialize Islamic DateTime instance.");
    ///
    /// assert_eq!(datetime_islamic.date.year().number, 474);
    /// assert_eq!(datetime_islamic.date.month().ordinal, 10);
    /// assert_eq!(datetime_islamic.date.day_of_month().0, 11);
    /// assert_eq!(datetime_islamic.time.hour.number(), 13);
    /// assert_eq!(datetime_islamic.time.minute.number(), 1);
    /// assert_eq!(datetime_islamic.time.second.number(), 0);
    /// ```
    pub fn try_new_ummalqura_datetime(
        year: i32,
        month: u8,
        day: u8,
        hour: u8,
        minute: u8,
        second: u8,
        calendar: A,
    ) -> Result<DateTime<A>, CalendarError> {
        Ok(DateTime {
            date: Date::try_new_ummalqura_date(year, month, day, calendar)?,
            time: types::Time::try_new(hour, minute, second, 0)?,
        })
    }
}

impl IslamicUmmAlQura {
    fn fixed_from_saudi_islamic(i_date: IslamicUmmAlQuraDateInner) -> RataDie {
        calendrical_calculations::islamic::fixed_from_saudi_islamic(
            i_date.0.year,
            i_date.0.month,
            i_date.0.day,
        )
    }

    fn saudi_islamic_from_fixed(date: RataDie) -> Date<IslamicUmmAlQura> {
        let (y, m, d) = calendrical_calculations::islamic::saudi_islamic_from_fixed(date);

        debug_assert!(Date::try_new_ummalqura_date(
            y,
            m,
            d,
            IslamicUmmAlQura::new_always_calculating()
        )
        .is_ok());
        Date::from_raw(
            IslamicUmmAlQuraDateInner(ArithmeticDate::new_unchecked(y, m, d)),
            IslamicUmmAlQura,
        )
    }

    fn year_as_islamic(year: i32) -> types::FormattableYear {
        types::FormattableYear {
            era: types::Era(tinystr!(16, "islamic")),
            number: year,
            cyclic: None,
            related_iso: None,
        }
    }
}

/// The inner date type used for representing [`Date`]s of [`IslamicCivil`]. See [`Date`] and [`IslamicCivil`] for more details.

#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
pub struct IslamicCivilDateInner(ArithmeticDate<IslamicCivil>);

impl CalendarArithmetic for IslamicCivil {
    fn month_days(year: i32, month: u8) -> u8 {
        match month {
            1 | 3 | 5 | 7 | 9 | 11 => 30,
            2 | 4 | 6 | 8 | 10 => 29,
            12 if Self::is_leap_year(year) => 30,
            12 => 29,
            _ => 0,
        }
    }

    fn months_for_every_year(_year: i32) -> u8 {
        12
    }

    fn days_in_provided_year(year: i32) -> u16 {
        if Self::is_leap_year(year) {
            355
        } else {
            354
        }
    }

    fn is_leap_year(year: i32) -> bool {
        (14 + 11 * year).rem_euclid(30) < 11
    }

    fn last_month_day_in_year(year: i32) -> (u8, u8) {
        if Self::is_leap_year(year) {
            (12, 30)
        } else {
            (12, 29)
        }
    }
}

impl Calendar for IslamicCivil {
    type DateInner = IslamicCivilDateInner;

    fn date_from_codes(
        &self,
        era: types::Era,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, CalendarError> {
        let year = if era.0 == tinystr!(16, "islamic-civil")
            || era.0 == tinystr!(16, "islamicc")
            || era.0 == tinystr!(16, "islamic")
            || era.0 == tinystr!(16, "ah")
        {
            // TODO: Check name and alias
            year
        } else {
            return Err(CalendarError::UnknownEra(era.0, self.debug_name()));
        };

        ArithmeticDate::new_from_codes(self, year, month_code, day).map(IslamicCivilDateInner)
    }

    fn date_from_iso(&self, iso: Date<Iso>) -> Self::DateInner {
        let fixed_iso = Iso::fixed_from_iso(*iso.inner());
        Self::islamic_from_fixed(fixed_iso).inner
    }

    fn date_to_iso(&self, date: &Self::DateInner) -> Date<Iso> {
        let fixed_islamic = Self::fixed_from_islamic(*date);
        Iso::iso_from_fixed(fixed_islamic)
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

    fn day_of_week(&self, date: &Self::DateInner) -> types::IsoWeekday {
        Iso.day_of_week(self.date_to_iso(date).inner())
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
        "Islamic (civil)"
    }

    fn year(&self, date: &Self::DateInner) -> types::FormattableYear {
        Self::year_as_islamic(date.0.year)
    }

    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        Self::is_leap_year(date.0.year)
    }

    fn month(&self, date: &Self::DateInner) -> types::FormattableMonth {
        date.0.month()
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
            prev_year: Self::year_as_islamic(prev_year),
            days_in_prev_year: Self::days_in_provided_year(prev_year),
            next_year: Self::year_as_islamic(next_year),
        }
    }
    fn any_calendar_kind(&self) -> Option<AnyCalendarKind> {
        Some(AnyCalendarKind::IslamicCivil)
    }
}

impl IslamicCivil {
    fn fixed_from_islamic(i_date: IslamicCivilDateInner) -> RataDie {
        calendrical_calculations::islamic::fixed_from_islamic_civil(
            i_date.0.year,
            i_date.0.month,
            i_date.0.day,
        )
    }

    fn islamic_from_fixed(date: RataDie) -> Date<IslamicCivil> {
        let (y, m, d) = calendrical_calculations::islamic::islamic_civil_from_fixed(date);

        debug_assert!(Date::try_new_islamic_civil_date_with_calendar(
            y,
            m,
            d,
            IslamicCivil::new_always_calculating()
        )
        .is_ok());
        Date::from_raw(
            IslamicCivilDateInner(ArithmeticDate::new_unchecked(y, m, d)),
            IslamicCivil,
        )
    }

    fn year_as_islamic(year: i32) -> types::FormattableYear {
        types::FormattableYear {
            era: types::Era(tinystr!(16, "islamic")),
            number: year,
            cyclic: None,
            related_iso: None,
        }
    }
}

impl<A: AsCalendar<Calendar = IslamicCivil>> Date<A> {
    /// Construct new Civil Islamic Date.
    ///
    /// Has no negative years, only era is the AH.
    ///
    /// ```rust
    /// use icu::calendar::islamic::IslamicCivil;
    /// use icu::calendar::Date;
    ///
    /// let islamic = IslamicCivil::new_always_calculating();
    ///
    /// let date_islamic =
    ///     Date::try_new_islamic_civil_date_with_calendar(1392, 4, 25, islamic)
    ///         .expect("Failed to initialize Islamic Date instance.");
    ///
    /// assert_eq!(date_islamic.year().number, 1392);
    /// assert_eq!(date_islamic.month().ordinal, 4);
    /// assert_eq!(date_islamic.day_of_month().0, 25);
    /// ```
    pub fn try_new_islamic_civil_date_with_calendar(
        year: i32,
        month: u8,
        day: u8,
        calendar: A,
    ) -> Result<Date<A>, CalendarError> {
        ArithmeticDate::new_from_lunar_ordinals(year, month, day)
            .map(IslamicCivilDateInner)
            .map(|inner| Date::from_raw(inner, calendar))
    }
}

impl<A: AsCalendar<Calendar = IslamicCivil>> DateTime<A> {
    /// Construct a new Civil Islamic datetime from integers.
    ///
    /// ```rust
    /// use icu::calendar::islamic::IslamicCivil;
    /// use icu::calendar::DateTime;
    ///
    /// let islamic = IslamicCivil::new_always_calculating();
    ///
    /// let datetime_islamic =
    ///     DateTime::try_new_islamic_civil_datetime_with_calendar(
    ///         474, 10, 11, 13, 1, 0, islamic,
    ///     )
    ///     .expect("Failed to initialize Islamic DateTime instance.");
    ///
    /// assert_eq!(datetime_islamic.date.year().number, 474);
    /// assert_eq!(datetime_islamic.date.month().ordinal, 10);
    /// assert_eq!(datetime_islamic.date.day_of_month().0, 11);
    /// assert_eq!(datetime_islamic.time.hour.number(), 13);
    /// assert_eq!(datetime_islamic.time.minute.number(), 1);
    /// assert_eq!(datetime_islamic.time.second.number(), 0);
    /// ```
    pub fn try_new_islamic_civil_datetime_with_calendar(
        year: i32,
        month: u8,
        day: u8,
        hour: u8,
        minute: u8,
        second: u8,
        calendar: A,
    ) -> Result<DateTime<A>, CalendarError> {
        Ok(DateTime {
            date: Date::try_new_islamic_civil_date_with_calendar(year, month, day, calendar)?,
            time: types::Time::try_new(hour, minute, second, 0)?,
        })
    }
}

/// The inner date type used for representing [`Date`]s of [`IslamicTabular`]. See [`Date`] and [`IslamicTabular`] for more details.

#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, PartialOrd, Ord)]
pub struct IslamicTabularDateInner(ArithmeticDate<IslamicTabular>);

impl CalendarArithmetic for IslamicTabular {
    fn month_days(year: i32, month: u8) -> u8 {
        match month {
            1 | 3 | 5 | 7 | 9 | 11 => 30,
            2 | 4 | 6 | 8 | 10 => 29,
            12 if Self::is_leap_year(year) => 30,
            12 => 29,
            _ => 0,
        }
    }

    fn months_for_every_year(_year: i32) -> u8 {
        12
    }

    fn days_in_provided_year(year: i32) -> u16 {
        if Self::is_leap_year(year) {
            355
        } else {
            354
        }
    }

    fn is_leap_year(year: i32) -> bool {
        (14 + 11 * year).rem_euclid(30) < 11
    }

    fn last_month_day_in_year(year: i32) -> (u8, u8) {
        if Self::is_leap_year(year) {
            (12, 30)
        } else {
            (12, 29)
        }
    }
}

impl Calendar for IslamicTabular {
    type DateInner = IslamicTabularDateInner;

    fn date_from_codes(
        &self,
        era: types::Era,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, CalendarError> {
        let year = if era.0 == tinystr!(16, "islamic-tbla")
            || era.0 == tinystr!(16, "islamic")
            || era.0 == tinystr!(16, "ah")
        {
            year
        } else {
            return Err(CalendarError::UnknownEra(era.0, self.debug_name()));
        };

        ArithmeticDate::new_from_codes(self, year, month_code, day).map(IslamicTabularDateInner)
    }

    fn date_from_iso(&self, iso: Date<Iso>) -> Self::DateInner {
        let fixed_iso = Iso::fixed_from_iso(*iso.inner());
        Self::islamic_from_fixed(fixed_iso).inner
    }

    fn date_to_iso(&self, date: &Self::DateInner) -> Date<Iso> {
        let fixed_islamic = Self::fixed_from_islamic(*date);
        Iso::iso_from_fixed(fixed_islamic)
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

    fn day_of_week(&self, date: &Self::DateInner) -> types::IsoWeekday {
        Iso.day_of_week(self.date_to_iso(date).inner())
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
        "Islamic (tabular)"
    }

    fn year(&self, date: &Self::DateInner) -> types::FormattableYear {
        Self::year_as_islamic(date.0.year)
    }

    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        Self::is_leap_year(date.0.year)
    }

    fn month(&self, date: &Self::DateInner) -> types::FormattableMonth {
        date.0.month()
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
            prev_year: Self::year_as_islamic(prev_year),
            days_in_prev_year: Self::days_in_provided_year(prev_year),
            next_year: Self::year_as_islamic(next_year),
        }
    }
    fn any_calendar_kind(&self) -> Option<AnyCalendarKind> {
        Some(AnyCalendarKind::IslamicTabular)
    }
}

impl IslamicTabular {
    fn fixed_from_islamic(i_date: IslamicTabularDateInner) -> RataDie {
        calendrical_calculations::islamic::fixed_from_islamic_tabular(
            i_date.0.year,
            i_date.0.month,
            i_date.0.day,
        )
    }

    fn islamic_from_fixed(date: RataDie) -> Date<IslamicTabular> {
        let (y, m, d) = calendrical_calculations::islamic::islamic_tabular_from_fixed(date);

        debug_assert!(Date::try_new_islamic_civil_date_with_calendar(
            y,
            m,
            d,
            IslamicCivil::new_always_calculating()
        )
        .is_ok());
        Date::from_raw(
            IslamicTabularDateInner(ArithmeticDate::new_unchecked(y, m, d)),
            IslamicTabular,
        )
    }

    fn year_as_islamic(year: i32) -> types::FormattableYear {
        types::FormattableYear {
            era: types::Era(tinystr!(16, "islamic")),
            number: year,
            cyclic: None,
            related_iso: None,
        }
    }
}

impl<A: AsCalendar<Calendar = IslamicTabular>> Date<A> {
    /// Construct new Tabular Islamic Date.
    ///
    /// Has no negative years, only era is the AH.
    ///
    /// ```rust
    /// use icu::calendar::islamic::IslamicTabular;
    /// use icu::calendar::Date;
    ///
    /// let islamic = IslamicTabular::new_always_calculating();
    ///
    /// let date_islamic =
    ///     Date::try_new_islamic_tabular_date_with_calendar(1392, 4, 25, islamic)
    ///         .expect("Failed to initialize Islamic Date instance.");
    ///
    /// assert_eq!(date_islamic.year().number, 1392);
    /// assert_eq!(date_islamic.month().ordinal, 4);
    /// assert_eq!(date_islamic.day_of_month().0, 25);
    /// ```
    pub fn try_new_islamic_tabular_date_with_calendar(
        year: i32,
        month: u8,
        day: u8,
        calendar: A,
    ) -> Result<Date<A>, CalendarError> {
        ArithmeticDate::new_from_lunar_ordinals(year, month, day)
            .map(IslamicTabularDateInner)
            .map(|inner| Date::from_raw(inner, calendar))
    }
}

impl<A: AsCalendar<Calendar = IslamicTabular>> DateTime<A> {
    /// Construct a new Tabular Islamic datetime from integers.
    ///
    /// ```rust
    /// use icu::calendar::islamic::IslamicTabular;
    /// use icu::calendar::DateTime;
    ///
    /// let islamic = IslamicTabular::new_always_calculating();
    ///
    /// let datetime_islamic =
    ///     DateTime::try_new_islamic_tabular_datetime_with_calendar(
    ///         474, 10, 11, 13, 1, 0, islamic,
    ///     )
    ///     .expect("Failed to initialize Islamic DateTime instance.");
    ///
    /// assert_eq!(datetime_islamic.date.year().number, 474);
    /// assert_eq!(datetime_islamic.date.month().ordinal, 10);
    /// assert_eq!(datetime_islamic.date.day_of_month().0, 11);
    /// assert_eq!(datetime_islamic.time.hour.number(), 13);
    /// assert_eq!(datetime_islamic.time.minute.number(), 1);
    /// assert_eq!(datetime_islamic.time.second.number(), 0);
    /// ```
    pub fn try_new_islamic_tabular_datetime_with_calendar(
        year: i32,
        month: u8,
        day: u8,
        hour: u8,
        minute: u8,
        second: u8,
        calendar: A,
    ) -> Result<DateTime<A>, CalendarError> {
        Ok(DateTime {
            date: Date::try_new_islamic_tabular_date_with_calendar(year, month, day, calendar)?,
            time: types::Time::try_new(hour, minute, second, 0)?,
        })
    }
}

#[cfg(test)]
mod test {
    use super::*;

    const START_YEAR: i32 = -1245;
    const END_YEAR: i32 = 1518;

    #[derive(Debug)]
    struct DateCase {
        year: i32,
        month: u8,
        day: u8,
    }

    static TEST_FIXED_DATE: [i64; 33] = [
        -214193, -61387, 25469, 49217, 171307, 210155, 253427, 369740, 400085, 434355, 452605,
        470160, 473837, 507850, 524156, 544676, 567118, 569477, 601716, 613424, 626596, 645554,
        664224, 671401, 694799, 704424, 708842, 709409, 709580, 727274, 728714, 744313, 764652,
    ];
    // Removed: 601716 and 727274 fixed dates
    static TEST_FIXED_DATE_UMMALQURA: [i64; 31] = [
        -214193, -61387, 25469, 49217, 171307, 210155, 253427, 369740, 400085, 434355, 452605,
        470160, 473837, 507850, 524156, 544676, 567118, 569477, 613424, 626596, 645554, 664224,
        671401, 694799, 704424, 708842, 709409, 709580, 728714, 744313, 764652,
    ];

    static UMMALQURA_DATE_EXPECTED: [DateCase; 31] = [
        DateCase {
            year: -1245,
            month: 12,
            day: 11,
        },
        DateCase {
            year: -813,
            month: 2,
            day: 26,
        },
        DateCase {
            year: -568,
            month: 4,
            day: 3,
        },
        DateCase {
            year: -501,
            month: 4,
            day: 8,
        },
        DateCase {
            year: -157,
            month: 10,
            day: 18,
        },
        DateCase {
            year: -47,
            month: 6,
            day: 4,
        },
        DateCase {
            year: 75,
            month: 7,
            day: 14,
        },
        DateCase {
            year: 403,
            month: 10,
            day: 6,
        },
        DateCase {
            year: 489,
            month: 5,
            day: 23,
        },
        DateCase {
            year: 586,
            month: 2,
            day: 8,
        },
        DateCase {
            year: 637,
            month: 8,
            day: 8,
        },
        DateCase {
            year: 687,
            month: 2,
            day: 22,
        },
        DateCase {
            year: 697,
            month: 7,
            day: 8,
        },
        DateCase {
            year: 793,
            month: 7,
            day: 1,
        },
        DateCase {
            year: 839,
            month: 7,
            day: 7,
        },
        DateCase {
            year: 897,
            month: 6,
            day: 3,
        },
        DateCase {
            year: 960,
            month: 10,
            day: 1,
        },
        DateCase {
            year: 967,
            month: 5,
            day: 28,
        },
        DateCase {
            year: 1091,
            month: 6,
            day: 4,
        },
        DateCase {
            year: 1128,
            month: 8,
            day: 5,
        },
        DateCase {
            year: 1182,
            month: 2,
            day: 4,
        },
        DateCase {
            year: 1234,
            month: 10,
            day: 11,
        },
        DateCase {
            year: 1255,
            month: 1,
            day: 11,
        },
        DateCase {
            year: 1321,
            month: 1,
            day: 21,
        },
        DateCase {
            year: 1348,
            month: 3,
            day: 20,
        },
        DateCase {
            year: 1360,
            month: 9,
            day: 8,
        },
        DateCase {
            year: 1362,
            month: 4,
            day: 14,
        },
        DateCase {
            year: 1362,
            month: 10,
            day: 8,
        },
        DateCase {
            year: 1416,
            month: 10,
            day: 6,
        },
        DateCase {
            year: 1460,
            month: 10,
            day: 13,
        },
        DateCase {
            year: 1518,
            month: 3,
            day: 6,
        },
    ];

    static OBSERVATIONAL_CASES: [DateCase; 33] = [
        DateCase {
            year: -1245,
            month: 12,
            day: 11,
        },
        DateCase {
            year: -813,
            month: 2,
            day: 25,
        },
        DateCase {
            year: -568,
            month: 4,
            day: 2,
        },
        DateCase {
            year: -501,
            month: 4,
            day: 7,
        },
        DateCase {
            year: -157,
            month: 10,
            day: 18,
        },
        DateCase {
            year: -47,
            month: 6,
            day: 3,
        },
        DateCase {
            year: 75,
            month: 7,
            day: 13,
        },
        DateCase {
            year: 403,
            month: 10,
            day: 5,
        },
        DateCase {
            year: 489,
            month: 5,
            day: 22,
        },
        DateCase {
            year: 586,
            month: 2,
            day: 7,
        },
        DateCase {
            year: 637,
            month: 8,
            day: 7,
        },
        DateCase {
            year: 687,
            month: 2,
            day: 21,
        },
        DateCase {
            year: 697,
            month: 7,
            day: 7,
        },
        DateCase {
            year: 793,
            month: 6,
            day: 30,
        },
        DateCase {
            year: 839,
            month: 7,
            day: 6,
        },
        DateCase {
            year: 897,
            month: 6,
            day: 2,
        },
        DateCase {
            year: 960,
            month: 9,
            day: 30,
        },
        DateCase {
            year: 967,
            month: 5,
            day: 27,
        },
        DateCase {
            year: 1058,
            month: 5,
            day: 18,
        },
        DateCase {
            year: 1091,
            month: 6,
            day: 3,
        },
        DateCase {
            year: 1128,
            month: 8,
            day: 4,
        },
        DateCase {
            year: 1182,
            month: 2,
            day: 4,
        },
        DateCase {
            year: 1234,
            month: 10,
            day: 10,
        },
        DateCase {
            year: 1255,
            month: 1,
            day: 11,
        },
        DateCase {
            year: 1321,
            month: 1,
            day: 20,
        },
        DateCase {
            year: 1348,
            month: 3,
            day: 19,
        },
        DateCase {
            year: 1360,
            month: 9,
            day: 7,
        },
        DateCase {
            year: 1362,
            month: 4,
            day: 14,
        },
        DateCase {
            year: 1362,
            month: 10,
            day: 7,
        },
        DateCase {
            year: 1412,
            month: 9,
            day: 12,
        },
        DateCase {
            year: 1416,
            month: 10,
            day: 5,
        },
        DateCase {
            year: 1460,
            month: 10,
            day: 12,
        },
        DateCase {
            year: 1518,
            month: 3,
            day: 5,
        },
    ];

    static ARITHMETIC_CASES: [DateCase; 33] = [
        DateCase {
            year: -1245,
            month: 12,
            day: 9,
        },
        DateCase {
            year: -813,
            month: 2,
            day: 23,
        },
        DateCase {
            year: -568,
            month: 4,
            day: 1,
        },
        DateCase {
            year: -501,
            month: 4,
            day: 6,
        },
        DateCase {
            year: -157,
            month: 10,
            day: 17,
        },
        DateCase {
            year: -47,
            month: 6,
            day: 3,
        },
        DateCase {
            year: 75,
            month: 7,
            day: 13,
        },
        DateCase {
            year: 403,
            month: 10,
            day: 5,
        },
        DateCase {
            year: 489,
            month: 5,
            day: 22,
        },
        DateCase {
            year: 586,
            month: 2,
            day: 7,
        },
        DateCase {
            year: 637,
            month: 8,
            day: 7,
        },
        DateCase {
            year: 687,
            month: 2,
            day: 20,
        },
        DateCase {
            year: 697,
            month: 7,
            day: 7,
        },
        DateCase {
            year: 793,
            month: 7,
            day: 1,
        },
        DateCase {
            year: 839,
            month: 7,
            day: 6,
        },
        DateCase {
            year: 897,
            month: 6,
            day: 1,
        },
        DateCase {
            year: 960,
            month: 9,
            day: 30,
        },
        DateCase {
            year: 967,
            month: 5,
            day: 27,
        },
        DateCase {
            year: 1058,
            month: 5,
            day: 18,
        },
        DateCase {
            year: 1091,
            month: 6,
            day: 2,
        },
        DateCase {
            year: 1128,
            month: 8,
            day: 4,
        },
        DateCase {
            year: 1182,
            month: 2,
            day: 3,
        },
        DateCase {
            year: 1234,
            month: 10,
            day: 10,
        },
        DateCase {
            year: 1255,
            month: 1,
            day: 11,
        },
        DateCase {
            year: 1321,
            month: 1,
            day: 21,
        },
        DateCase {
            year: 1348,
            month: 3,
            day: 19,
        },
        DateCase {
            year: 1360,
            month: 9,
            day: 8,
        },
        DateCase {
            year: 1362,
            month: 4,
            day: 13,
        },
        DateCase {
            year: 1362,
            month: 10,
            day: 7,
        },
        DateCase {
            year: 1412,
            month: 9,
            day: 13,
        },
        DateCase {
            year: 1416,
            month: 10,
            day: 5,
        },
        DateCase {
            year: 1460,
            month: 10,
            day: 12,
        },
        DateCase {
            year: 1518,
            month: 3,
            day: 5,
        },
    ];

    static TABULAR_CASES: [DateCase; 33] = [
        DateCase {
            year: -1245,
            month: 12,
            day: 10,
        },
        DateCase {
            year: -813,
            month: 2,
            day: 24,
        },
        DateCase {
            year: -568,
            month: 4,
            day: 2,
        },
        DateCase {
            year: -501,
            month: 4,
            day: 7,
        },
        DateCase {
            year: -157,
            month: 10,
            day: 18,
        },
        DateCase {
            year: -47,
            month: 6,
            day: 4,
        },
        DateCase {
            year: 75,
            month: 7,
            day: 14,
        },
        DateCase {
            year: 403,
            month: 10,
            day: 6,
        },
        DateCase {
            year: 489,
            month: 5,
            day: 23,
        },
        DateCase {
            year: 586,
            month: 2,
            day: 8,
        },
        DateCase {
            year: 637,
            month: 8,
            day: 8,
        },
        DateCase {
            year: 687,
            month: 2,
            day: 21,
        },
        DateCase {
            year: 697,
            month: 7,
            day: 8,
        },
        DateCase {
            year: 793,
            month: 7,
            day: 2,
        },
        DateCase {
            year: 839,
            month: 7,
            day: 7,
        },
        DateCase {
            year: 897,
            month: 6,
            day: 2,
        },
        DateCase {
            year: 960,
            month: 10,
            day: 1,
        },
        DateCase {
            year: 967,
            month: 5,
            day: 28,
        },
        DateCase {
            year: 1058,
            month: 5,
            day: 19,
        },
        DateCase {
            year: 1091,
            month: 6,
            day: 3,
        },
        DateCase {
            year: 1128,
            month: 8,
            day: 5,
        },
        DateCase {
            year: 1182,
            month: 2,
            day: 4,
        },
        DateCase {
            year: 1234,
            month: 10,
            day: 11,
        },
        DateCase {
            year: 1255,
            month: 1,
            day: 12,
        },
        DateCase {
            year: 1321,
            month: 1,
            day: 22,
        },
        DateCase {
            year: 1348,
            month: 3,
            day: 20,
        },
        DateCase {
            year: 1360,
            month: 9,
            day: 9,
        },
        DateCase {
            year: 1362,
            month: 4,
            day: 14,
        },
        DateCase {
            year: 1362,
            month: 10,
            day: 8,
        },
        DateCase {
            year: 1412,
            month: 9,
            day: 14,
        },
        DateCase {
            year: 1416,
            month: 10,
            day: 6,
        },
        DateCase {
            year: 1460,
            month: 10,
            day: 13,
        },
        DateCase {
            year: 1518,
            month: 3,
            day: 6,
        },
    ];

    #[test]
    fn test_observational_islamic_from_fixed() {
        for (case, f_date) in OBSERVATIONAL_CASES.iter().zip(TEST_FIXED_DATE.iter()) {
            let date = Date::try_new_observational_islamic_date(
                case.year,
                case.month,
                case.day,
                IslamicObservational::new_always_calculating(),
            )
            .unwrap();
            assert_eq!(
                IslamicObservational::islamic_from_fixed(RataDie::new(*f_date)),
                date,
                "{case:?}"
            );
        }
    }

    #[test]
    fn test_fixed_from_observational_islamic() {
        for (case, f_date) in OBSERVATIONAL_CASES.iter().zip(TEST_FIXED_DATE.iter()) {
            let date = IslamicDateInner(ArithmeticDate::new_unchecked(
                case.year, case.month, case.day,
            ));
            assert_eq!(
                IslamicObservational::fixed_from_islamic(date),
                RataDie::new(*f_date),
                "{case:?}"
            );
        }
    }

    #[test]
    fn test_fixed_from_islamic() {
        for (case, f_date) in ARITHMETIC_CASES.iter().zip(TEST_FIXED_DATE.iter()) {
            let date = IslamicCivilDateInner(ArithmeticDate::new_unchecked(
                case.year, case.month, case.day,
            ));
            assert_eq!(
                IslamicCivil::fixed_from_islamic(date),
                RataDie::new(*f_date),
                "{case:?}"
            );
        }
    }

    #[test]
    fn test_islamic_from_fixed() {
        for (case, f_date) in ARITHMETIC_CASES.iter().zip(TEST_FIXED_DATE.iter()) {
            let date = Date::try_new_islamic_civil_date_with_calendar(
                case.year,
                case.month,
                case.day,
                IslamicCivil::new_always_calculating(),
            )
            .unwrap();
            assert_eq!(
                IslamicCivil::islamic_from_fixed(RataDie::new(*f_date)),
                date,
                "{case:?}"
            );
        }
    }

    #[test]
    fn test_fixed_from_islamic_tbla() {
        for (case, f_date) in TABULAR_CASES.iter().zip(TEST_FIXED_DATE.iter()) {
            let date = IslamicTabularDateInner(ArithmeticDate::new_unchecked(
                case.year, case.month, case.day,
            ));
            assert_eq!(
                IslamicTabular::fixed_from_islamic(date),
                RataDie::new(*f_date),
                "{case:?}"
            );
        }
    }

    #[test]
    fn test_islamic_tbla_from_fixed() {
        for (case, f_date) in TABULAR_CASES.iter().zip(TEST_FIXED_DATE.iter()) {
            let date = Date::try_new_islamic_tabular_date_with_calendar(
                case.year,
                case.month,
                case.day,
                IslamicTabular::new_always_calculating(),
            )
            .unwrap();
            assert_eq!(
                IslamicTabular::islamic_from_fixed(RataDie::new(*f_date)),
                date,
                "{case:?}"
            );
        }
    }

    #[test]
    fn test_saudi_islamic_from_fixed() {
        for (case, f_date) in UMMALQURA_DATE_EXPECTED
            .iter()
            .zip(TEST_FIXED_DATE_UMMALQURA.iter())
        {
            let date = Date::try_new_ummalqura_date(
                case.year,
                case.month,
                case.day,
                IslamicUmmAlQura::new_always_calculating(),
            )
            .unwrap();
            assert_eq!(
                IslamicUmmAlQura::saudi_islamic_from_fixed(RataDie::new(*f_date)),
                date,
                "{case:?}"
            );
        }
    }

    #[test]
    fn test_fixed_from_saudi_islamic() {
        for (case, f_date) in UMMALQURA_DATE_EXPECTED
            .iter()
            .zip(TEST_FIXED_DATE_UMMALQURA.iter())
        {
            let date = IslamicUmmAlQuraDateInner(ArithmeticDate::new_unchecked(
                case.year, case.month, case.day,
            ));
            assert_eq!(
                IslamicUmmAlQura::fixed_from_saudi_islamic(date),
                RataDie::new(*f_date),
                "{case:?}"
            );
        }
    }

    #[ignore]
    #[test]
    fn test_days_in_provided_year_observational() {
        // -1245 1 1 = -214526 (R.D Date)
        // 1518 1 1 = 764589 (R.D Date)
        let sum_days_in_year: i64 = (START_YEAR..END_YEAR)
            .map(|year| IslamicObservational::days_in_provided_year(year) as i64)
            .sum();
        let expected_number_of_days = IslamicObservational::fixed_from_islamic(IslamicDateInner(
            ArithmeticDate::new_from_lunar_ordinals(END_YEAR, 1, 1).unwrap(),
        )) - IslamicObservational::fixed_from_islamic(
            IslamicDateInner(ArithmeticDate::new_from_lunar_ordinals(START_YEAR, 1, 1).unwrap()),
        ); // The number of days between Islamic years -1245 and 1518
        let tolerance = 1; // One day tolerance (See Astronomical::month_length for more context)

        assert!(
            (sum_days_in_year - expected_number_of_days).abs() <= tolerance,
            "Difference between sum_days_in_year and expected_number_of_days is more than the tolerance"
        );
    }

    #[ignore]
    #[test]
    fn test_days_in_provided_year_ummalqura() {
        // -1245 1 1 = -214528 (R.D Date)
        // 1518 1 1 = 764588 (R.D Date)
        let sum_days_in_year: i64 = (START_YEAR..END_YEAR)
            .map(|year| IslamicUmmAlQura::days_in_provided_year(year) as i64)
            .sum();

        let expected_number_of_days =
            IslamicUmmAlQura::fixed_from_saudi_islamic(IslamicUmmAlQuraDateInner(
                ArithmeticDate::new_from_lunar_ordinals(END_YEAR, 1, 1).unwrap(),
            )) - IslamicUmmAlQura::fixed_from_saudi_islamic(IslamicUmmAlQuraDateInner(
                ArithmeticDate::new_from_lunar_ordinals(START_YEAR, 1, 1).unwrap(),
            )); // The number of days between Umm al-Qura Islamic years -1245 and 1518

        assert_eq!(sum_days_in_year, expected_number_of_days);
    }

    #[test]
    fn test_regression_3868() {
        // This date used to panic on creation
        let iso = Date::try_new_iso_date(2011, 4, 4).unwrap();
        let islamic = iso.to_calendar(IslamicUmmAlQura);
        // Data from https://www.ummulqura.org.sa/Index.aspx
        assert_eq!(islamic.day_of_month().0, 30);
        assert_eq!(islamic.month().ordinal, 4);
        assert_eq!(islamic.year().number, 1432);
    }
}
