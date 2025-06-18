// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use ffi::IsoWeekOfYear;

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;
    use alloc::sync::Arc;
    use core::fmt::Write;
    use icu_calendar::Iso;

    use crate::unstable::calendar::ffi::Calendar;
    use crate::unstable::errors::ffi::{CalendarError, Rfc9557ParseError};

    use tinystr::TinyAsciiStr;

    #[diplomat::enum_convert(icu_calendar::types::Weekday)]
    pub enum Weekday {
        Monday = 1,
        Tuesday,
        Wednesday,
        Thursday,
        Friday,
        Saturday,
        Sunday,
    }
    #[diplomat::opaque]
    #[diplomat::transparent_convert]
    /// An ICU4X Date object capable of containing a ISO-8601 date
    #[diplomat::rust_link(icu::calendar::Date, Struct)]
    pub struct IsoDate(pub icu_calendar::Date<icu_calendar::Iso>);

    impl IsoDate {
        /// Creates a new [`IsoDate`] from the specified date.
        #[diplomat::rust_link(icu::calendar::Date::try_new_iso, FnInStruct)]
        #[diplomat::attr(supports = fallible_constructors, constructor)]
        pub fn create(year: i32, month: u8, day: u8) -> Result<Box<IsoDate>, CalendarError> {
            Ok(Box::new(IsoDate(icu_calendar::Date::try_new_iso(
                year, month, day,
            )?)))
        }

        /// Creates a new [`IsoDate`] from the given Rata Die
        #[diplomat::rust_link(icu::calendar::Date::from_rata_die, FnInStruct)]
        #[diplomat::attr(all(supports = named_constructors), named_constructor)]
        #[diplomat::demo(default_constructor)]
        pub fn from_rata_die(rd: i64) -> Box<IsoDate> {
            Box::new(IsoDate(icu_calendar::Date::from_rata_die(
                icu_calendar::types::RataDie::new(rd),
                Iso,
            )))
        }

        /// Creates a new [`IsoDate`] from an IXDTF string.
        #[diplomat::rust_link(icu::calendar::Date::try_from_str, FnInStruct)]
        #[diplomat::rust_link(icu::calendar::Date::try_from_utf8, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::calendar::Date::from_str, FnInStruct, hidden)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor)]
        pub fn from_string(v: &DiplomatStr) -> Result<Box<IsoDate>, Rfc9557ParseError> {
            Ok(Box::new(IsoDate(icu_calendar::Date::try_from_utf8(
                v, Iso,
            )?)))
        }

        /// Convert this date to one in a different calendar
        #[diplomat::rust_link(icu::calendar::Date::to_calendar, FnInStruct)]
        pub fn to_calendar(&self, calendar: &Calendar) -> Box<Date> {
            Box::new(Date(self.0.to_calendar(calendar.0.clone())))
        }

        #[diplomat::rust_link(icu::calendar::Date::to_any, FnInStruct)]
        pub fn to_any(&self) -> Box<Date> {
            Box::new(Date(self.0.to_any().into_atomic_ref_counted()))
        }

        /// Returns this date's Rata Die
        #[diplomat::rust_link(icu::calendar::Date::to_rata_die, FnInStruct)]
        #[diplomat::attr(auto, getter = "rata_die")]
        pub fn to_rata_die(&self) -> i64 {
            self.0.to_rata_die().to_i64_date()
        }

        /// Returns the 1-indexed day in the year for this date
        #[diplomat::rust_link(icu::calendar::Date::day_of_year, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn day_of_year(&self) -> u16 {
            self.0.day_of_year().0
        }

        /// Returns the 1-indexed day in the month for this date
        #[diplomat::rust_link(icu::calendar::Date::day_of_month, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn day_of_month(&self) -> u8 {
            self.0.day_of_month().0
        }

        /// Returns the day in the week for this day
        #[diplomat::rust_link(icu::calendar::Date::day_of_week, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn day_of_week(&self) -> Weekday {
            self.0.day_of_week().into()
        }

        /// Returns the week number in this year, using week data
        #[diplomat::rust_link(icu::calendar::Date::week_of_year, FnInStruct)]
        #[cfg(feature = "calendar")]
        pub fn week_of_year(&self) -> IsoWeekOfYear {
            self.0.week_of_year().into()
        }

        /// Returns 1-indexed number of the month of this date in its year
        #[diplomat::rust_link(icu::calendar::types::MonthInfo::ordinal, StructField)]
        #[diplomat::rust_link(icu::calendar::Date::month, FnInStruct, compact)]
        #[diplomat::attr(auto, getter)]
        pub fn month(&self) -> u8 {
            self.0.month().ordinal
        }

        /// Returns the year number in the current era for this date
        ///
        /// For calendars without an era, returns the extended year
        #[diplomat::rust_link(icu::calendar::Date::year, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn year(&self) -> i32 {
            self.0.extended_year()
        }

        /// Returns if the year is a leap year for this date
        #[diplomat::rust_link(icu::calendar::Date::is_in_leap_year, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn is_in_leap_year(&self) -> bool {
            self.0.is_in_leap_year()
        }

        /// Returns the number of months in the year represented by this date
        #[diplomat::rust_link(icu::calendar::Date::months_in_year, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn months_in_year(&self) -> u8 {
            self.0.months_in_year()
        }

        /// Returns the number of days in the month represented by this date
        #[diplomat::rust_link(icu::calendar::Date::days_in_month, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn days_in_month(&self) -> u8 {
            self.0.days_in_month()
        }

        /// Returns the number of days in the year represented by this date
        #[diplomat::rust_link(icu::calendar::Date::days_in_year, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn days_in_year(&self) -> u16 {
            self.0.days_in_year()
        }
    }

    #[diplomat::opaque]
    #[diplomat::transparent_convert]
    /// An ICU4X Date object capable of containing a date for any calendar.
    #[diplomat::rust_link(icu::calendar::Date, Struct)]
    pub struct Date(pub icu_calendar::Date<Arc<icu_calendar::AnyCalendar>>);

    impl Date {
        /// Creates a new [`Date`] representing the ISO date
        /// given but in a given calendar
        #[diplomat::rust_link(icu::calendar::Date::new_from_iso, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor)]
        #[diplomat::demo(default_constructor)]
        pub fn from_iso_in_calendar(
            year: i32,
            month: u8,
            day: u8,
            calendar: &Calendar,
        ) -> Result<Box<Date>, CalendarError> {
            let cal = calendar.0.clone();
            Ok(Box::new(Date(
                icu_calendar::Date::try_new_iso(year, month, day)?.to_calendar(cal),
            )))
        }

        /// Creates a new [`Date`] from the given codes, which are interpreted in the given calendar system
        ///
        /// An empty era code will treat the year as an extended year
        #[diplomat::rust_link(icu::calendar::Date::try_new_from_codes, FnInStruct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor)]
        pub fn from_codes_in_calendar(
            era_code: &DiplomatStr,
            year: i32,
            month_code: &DiplomatStr,
            day: u8,
            calendar: &Calendar,
        ) -> Result<Box<Date>, CalendarError> {
            let era = if !era_code.is_empty() {
                Some(core::str::from_utf8(era_code).map_err(|_| CalendarError::UnknownEra)?)
            } else {
                None
            };
            let month = icu_calendar::types::MonthCode(
                TinyAsciiStr::try_from_utf8(month_code)
                    .map_err(|_| CalendarError::UnknownMonthCode)?,
            );
            let cal = calendar.0.clone();
            Ok(Box::new(Date(icu_calendar::Date::try_new_from_codes(
                era, year, month, day, cal,
            )?)))
        }

        /// Creates a new [`Date`] from the given Rata Die
        #[diplomat::rust_link(icu::calendar::Date::from_rata_die, FnInStruct)]
        #[diplomat::attr(all(supports = named_constructors), named_constructor)]
        #[diplomat::demo(default_constructor)]
        pub fn from_rata_die(rd: i64, calendar: &Calendar) -> Result<Box<Date>, CalendarError> {
            let cal = calendar.0.clone();
            Ok(Box::new(Date(icu_calendar::Date::from_rata_die(
                icu_calendar::types::RataDie::new(rd),
                cal,
            ))))
        }

        /// Creates a new [`Date`] from an IXDTF string.
        #[diplomat::rust_link(icu::calendar::Date::try_from_str, FnInStruct)]
        #[diplomat::rust_link(icu::calendar::Date::try_from_utf8, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::calendar::Date::from_str, FnInStruct, hidden)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor)]
        pub fn from_string(
            v: &DiplomatStr,
            calendar: &Calendar,
        ) -> Result<Box<Date>, Rfc9557ParseError> {
            Ok(Box::new(Date(icu_calendar::Date::try_from_utf8(
                v,
                calendar.0.clone(),
            )?)))
        }

        /// Convert this date to one in a different calendar
        #[diplomat::rust_link(icu::calendar::Date::to_calendar, FnInStruct)]
        #[diplomat::rust_link(icu::calendar::Date::convert_any, FnInStruct, hidden)]
        pub fn to_calendar(&self, calendar: &Calendar) -> Box<Date> {
            Box::new(Date(self.0.to_calendar(calendar.0.clone())))
        }

        /// Converts this date to ISO
        #[diplomat::rust_link(icu::calendar::Date::to_iso, FnInStruct)]
        pub fn to_iso(&self) -> Box<IsoDate> {
            Box::new(IsoDate(self.0.to_iso()))
        }

        /// Returns this date's Rata Die
        #[diplomat::rust_link(icu::calendar::Date::to_rata_die, FnInStruct)]
        #[diplomat::attr(auto, getter = "rata_die")]
        pub fn to_rata_die(&self) -> i64 {
            self.0.to_rata_die().to_i64_date()
        }

        /// Returns the 1-indexed day in the year for this date
        #[diplomat::rust_link(icu::calendar::Date::day_of_year, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn day_of_year(&self) -> u16 {
            self.0.day_of_year().0
        }

        /// Returns the 1-indexed day in the month for this date
        #[diplomat::rust_link(icu::calendar::Date::day_of_month, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn day_of_month(&self) -> u8 {
            self.0.day_of_month().0
        }

        /// Returns the day in the week for this day
        #[diplomat::rust_link(icu::calendar::Date::day_of_week, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn day_of_week(&self) -> Weekday {
            self.0.day_of_week().into()
        }

        /// Returns 1-indexed number of the month of this date in its year
        ///
        /// Note that for lunar calendars this may not lead to the same month
        /// having the same ordinal month across years; use month_code if you care
        /// about month identity.
        #[diplomat::rust_link(icu::calendar::Date::month, FnInStruct)]
        #[diplomat::rust_link(icu::calendar::types::MonthInfo::ordinal, StructField)]
        #[diplomat::attr(auto, getter)]
        pub fn ordinal_month(&self) -> u8 {
            self.0.month().ordinal
        }

        /// Returns the month code for this date. Typically something
        /// like "M01", "M02", but can be more complicated for lunar calendars.
        #[diplomat::rust_link(icu::calendar::types::MonthInfo::standard_code, StructField)]
        #[diplomat::rust_link(icu::calendar::Date::month, FnInStruct, compact)]
        #[diplomat::rust_link(icu::calendar::types::MonthInfo, Struct, hidden)]
        #[diplomat::rust_link(
            icu::calendar::types::MonthInfo::formatting_code,
            StructField,
            hidden
        )]
        #[diplomat::rust_link(icu::calendar::types::MonthInfo, Struct, hidden)]
        #[diplomat::attr(auto, getter)]
        pub fn month_code(&self, write: &mut diplomat_runtime::DiplomatWrite) {
            let code = self.0.month().standard_code;
            let _infallible = write.write_str(&code.0);
        }

        /// Returns the month number of this month.
        #[diplomat::rust_link(icu::calendar::types::MonthInfo::month_number, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn month_number(&self) -> u8 {
            self.0.month().month_number()
        }

        /// Returns whether the month is a leap month.
        #[diplomat::rust_link(icu::calendar::types::MonthInfo::is_leap, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn month_is_leap(&self) -> bool {
            self.0.month().is_leap()
        }

        /// Returns the year number in the current era for this date
        ///
        /// For calendars without an era, returns the related ISO year.
        #[diplomat::rust_link(icu::calendar::types::YearInfo::era_year_or_related_iso, FnInEnum)]
        #[diplomat::rust_link(icu::calendar::types::EraYear::year, StructField, compact)]
        #[diplomat::rust_link(icu::calendar::types::CyclicYear::related_iso, StructField, compact)]
        #[diplomat::rust_link(icu::calendar::Date::year, FnInStruct, compact)]
        #[diplomat::rust_link(icu::calendar::Date::era_year, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::calendar::Date::cyclic_year, FnInStruct, hidden)]
        #[diplomat::rust_link(icu::calendar::types::YearInfo, Enum, hidden)]
        #[diplomat::rust_link(icu::calendar::types::YearInfo::era, FnInEnum, hidden)]
        #[diplomat::rust_link(icu::calendar::types::YearInfo::cyclic, FnInEnum, hidden)]
        #[diplomat::rust_link(icu::calendar::types::EraYear, Struct, hidden)]
        #[diplomat::rust_link(icu::calendar::types::CyclicYear, Struct, hidden)]
        #[diplomat::attr(auto, getter)]
        pub fn era_year_or_related_iso(&self) -> i32 {
            self.0.year().era_year_or_related_iso()
        }

        /// Returns the extended year in the Date
        #[diplomat::rust_link(icu::calendar::Date::extended_year, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn extended_year(&self) -> i32 {
            self.0.extended_year()
        }

        /// Returns the era for this date, or an empty string
        #[diplomat::rust_link(icu::calendar::types::EraYear::era, StructField)]
        #[diplomat::rust_link(icu::calendar::Date::year, FnInStruct, compact)]
        #[diplomat::attr(auto, getter)]
        pub fn era(&self, write: &mut diplomat_runtime::DiplomatWrite) {
            if let Some(era) = self.0.year().era() {
                let _infallible = write.write_str(&era.era);
            }
        }

        /// Returns the number of months in the year represented by this date
        #[diplomat::rust_link(icu::calendar::Date::months_in_year, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn months_in_year(&self) -> u8 {
            self.0.months_in_year()
        }

        /// Returns the number of days in the month represented by this date
        #[diplomat::rust_link(icu::calendar::Date::days_in_month, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn days_in_month(&self) -> u8 {
            self.0.days_in_month()
        }

        /// Returns the number of days in the year represented by this date
        #[diplomat::rust_link(icu::calendar::Date::days_in_year, FnInStruct)]
        #[diplomat::attr(auto, getter)]
        pub fn days_in_year(&self) -> u16 {
            self.0.days_in_year()
        }

        /// Returns the [`Calendar`] object backing this date
        #[diplomat::rust_link(icu::calendar::Date::calendar, FnInStruct)]
        #[diplomat::rust_link(icu::calendar::Date::calendar_wrapper, FnInStruct, hidden)]
        #[diplomat::attr(auto, getter)]
        pub fn calendar(&self) -> Box<Calendar> {
            Box::new(Calendar(self.0.calendar_wrapper().clone()))
        }
    }

    pub struct IsoWeekOfYear {
        pub week_number: u8,
        pub iso_year: i32,
    }
}

impl From<icu_calendar::types::IsoWeekOfYear> for IsoWeekOfYear {
    fn from(
        icu_calendar::types::IsoWeekOfYear {
            week_number,
            iso_year,
        }: icu_calendar::types::IsoWeekOfYear,
    ) -> Self {
        Self {
            week_number,
            iso_year,
        }
    }
}
