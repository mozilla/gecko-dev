use crate::astronomy::{self, Astronomical, Location, MEAN_SYNODIC_MONTH, MEAN_TROPICAL_YEAR};
use crate::helpers::i64_to_i32;
use crate::rata_die::{Moment, RataDie};
use core::num::NonZeroU8;
#[allow(unused_imports)]
use core_maths::*;

// Don't iterate more than 14 times (which accounts for checking for 13 months)
const MAX_ITERS_FOR_MONTHS_OF_YEAR: u8 = 14;

/// The trait ChineseBased is used by Chinese-based calendars to perform computations shared by such calendar.
/// To do so, calendars should:
///
/// - Implement `fn location` by providing a location at which observations of the moon are recorded, which
/// may change over time (the zone is important, long, lat, and elevation are not relevant for these calculations)
/// - Define `const EPOCH` as a `RataDie` marking the start date of the era of the Calendar for internal use,
/// which may not accurately reflect how years or eras are marked traditionally or seen by end-users
pub trait ChineseBased {
    /// Given a fixed date, return the location used for observations of the new moon in order to
    /// calculate the beginning of months. For multiple Chinese-based lunar calendars, this has
    /// changed over the years, and can cause differences in calendar date.
    fn location(fixed: RataDie) -> Location;

    /// The RataDie of the beginning of the epoch used for internal computation; this may not
    /// reflect traditional methods of year-tracking or eras, since Chinese-based calendars
    /// may not track years ordinally in the same way many western calendars do.
    const EPOCH: RataDie;
}

// The equivalent first day in the Chinese calendar (based on inception of the calendar)
const CHINESE_EPOCH: RataDie = RataDie::new(-963099); // Feb. 15, 2637 BCE (-2636)

/// The Chinese calendar relies on knowing the current day at the moment of a new moon;
/// however, this can vary depending on location. As such, new moon calculations are based
/// on the time in Beijing. Before 1929, local time was used, represented as UTC+(1397/180 h).
/// In 1929, China adopted a standard time zone based on 120 degrees of longitude, meaning
/// from 1929 onward, all new moon calculations are based on UTC+8h.
///
/// Offsets are not given in hours, but in partial days (1 hour = 1 / 24 day)
const UTC_OFFSET_PRE_1929: f64 = (1397.0 / 180.0) / 24.0;
const UTC_OFFSET_POST_1929: f64 = 8.0 / 24.0;

const CHINESE_LOCATION_PRE_1929: Location =
    Location::new_unchecked(39.0, 116.0, 43.5, UTC_OFFSET_PRE_1929);
const CHINESE_LOCATION_POST_1929: Location =
    Location::new_unchecked(39.0, 116.0, 43.5, UTC_OFFSET_POST_1929);

// The first day in the Korean Dangi calendar (based on the founding of Gojoseon)
const KOREAN_EPOCH: RataDie = RataDie::new(-852065); // Lunar new year 2333 BCE (-2332 ISO)

/// The Korean Dangi calendar relies on knowing the current day at the moment of a new moon;
/// however, this can vary depending on location. As such, new moon calculations are based on
/// the time in Seoul. Before 1908, local time was used, represented as UTC+(3809/450 h).
/// This changed multiple times as different standard timezones were adopted in Korea.
/// Currently, UTC+9h is used.
///
/// Offsets are not given in hours, but in partial days (1 hour = 1 / 24 day).
const UTC_OFFSET_ORIGINAL: f64 = (3809.0 / 450.0) / 24.0;
const UTC_OFFSET_1908: f64 = 8.5 / 24.0;
const UTC_OFFSET_1912: f64 = 9.0 / 24.0;
const UTC_OFFSET_1954: f64 = 8.5 / 24.0;
const UTC_OFFSET_1961: f64 = 9.0 / 24.0;

const FIXED_1908: RataDie = RataDie::new(696608); // Apr 1, 1908
const FIXED_1912: RataDie = RataDie::new(697978); // Jan 1, 1912
const FIXED_1954: RataDie = RataDie::new(713398); // Mar 21, 1954
const FIXED_1961: RataDie = RataDie::new(716097); // Aug 10, 1961

const KOREAN_LATITUDE: f64 = 37.0 + (34.0 / 60.0);
const KOREAN_LONGITUDE: f64 = 126.0 + (58.0 / 60.0);
const KOREAN_ELEVATION: f64 = 0.0;

const KOREAN_LOCATION_ORIGINAL: Location = Location::new_unchecked(
    KOREAN_LATITUDE,
    KOREAN_LONGITUDE,
    KOREAN_ELEVATION,
    UTC_OFFSET_ORIGINAL,
);
const KOREAN_LOCATION_1908: Location = Location::new_unchecked(
    KOREAN_LATITUDE,
    KOREAN_LONGITUDE,
    KOREAN_ELEVATION,
    UTC_OFFSET_1908,
);
const KOREAN_LOCATION_1912: Location = Location::new_unchecked(
    KOREAN_LATITUDE,
    KOREAN_LONGITUDE,
    KOREAN_ELEVATION,
    UTC_OFFSET_1912,
);
const KOREAN_LOCATION_1954: Location = Location::new_unchecked(
    KOREAN_LATITUDE,
    KOREAN_LONGITUDE,
    KOREAN_ELEVATION,
    UTC_OFFSET_1954,
);
const KOREAN_LOCATION_1961: Location = Location::new_unchecked(
    KOREAN_LATITUDE,
    KOREAN_LONGITUDE,
    KOREAN_ELEVATION,
    UTC_OFFSET_1961,
);

/// A type implementing [`ChineseBased`] for the Chinese calendar
#[derive(Debug)]
#[allow(clippy::exhaustive_structs)] // newtype
pub struct Chinese;

/// A type implementing [`ChineseBased`] for the Dangi (Korean) calendar
#[derive(Debug)]
#[allow(clippy::exhaustive_structs)] // newtype
pub struct Dangi;

impl ChineseBased for Chinese {
    fn location(fixed: RataDie) -> Location {
        let year = crate::iso::iso_year_from_fixed(fixed);
        if year < 1929 {
            CHINESE_LOCATION_PRE_1929
        } else {
            CHINESE_LOCATION_POST_1929
        }
    }

    const EPOCH: RataDie = CHINESE_EPOCH;
}

impl ChineseBased for Dangi {
    fn location(fixed: RataDie) -> Location {
        if fixed < FIXED_1908 {
            KOREAN_LOCATION_ORIGINAL
        } else if fixed < FIXED_1912 {
            KOREAN_LOCATION_1908
        } else if fixed < FIXED_1954 {
            KOREAN_LOCATION_1912
        } else if fixed < FIXED_1961 {
            KOREAN_LOCATION_1954
        } else {
            KOREAN_LOCATION_1961
        }
    }

    const EPOCH: RataDie = KOREAN_EPOCH;
}

/// Marks the bounds of a lunar year
#[derive(Debug, Copy, Clone)]
#[allow(clippy::exhaustive_structs)] // we're comfortable making frequent breaking changes to this crate
pub struct YearBounds {
    /// The date marking the start of the current lunar year
    pub new_year: RataDie,
    /// The date marking the start of the next lunar year
    pub next_new_year: RataDie,
}

impl YearBounds {
    /// Compute the YearBounds for the lunar year (年) containing `date`,
    /// as well as the corresponding solar year (歲). Note that since the two
    /// years overlap significantly but not entirely, the solstice bounds for the solar
    /// year *may* not include `date`.
    #[inline]
    pub fn compute<C: ChineseBased>(date: RataDie) -> Self {
        let prev_solstice = winter_solstice_on_or_before::<C>(date);
        let (new_year, next_solstice) = new_year_on_or_before_fixed_date::<C>(date, prev_solstice);
        // Using 400 here since new years can be up to 390 days apart, and we add some padding
        let next_new_year = new_year_on_or_before_fixed_date::<C>(new_year + 400, next_solstice).0;

        Self {
            new_year,
            next_new_year,
        }
    }

    /// The number of days in this year
    pub fn count_days(self) -> u16 {
        let result = self.next_new_year - self.new_year;
        debug_assert!(
            ((u16::MIN as i64)..=(u16::MAX as i64)).contains(&result),
            "Days in year should be in range of u16."
        );
        result as u16
    }

    /// Whether or not this is a leap year
    pub fn is_leap(self) -> bool {
        let difference = self.next_new_year - self.new_year;
        difference > 365
    }
}

/// Get the current major solar term of a fixed date, output as an integer from 1..=12.
///
/// Based on functions from _Calendrical Calculations_ by Reingold & Dershowitz.
/// Lisp reference code: https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L5273-L5281
pub(crate) fn major_solar_term_from_fixed<C: ChineseBased>(date: RataDie) -> u32 {
    let moment: Moment = date.as_moment();
    let location = C::location(date);
    let universal: Moment = Location::universal_from_standard(moment, location);
    let solar_longitude =
        i64_to_i32(Astronomical::solar_longitude(Astronomical::julian_centuries(universal)) as i64);
    debug_assert!(
        solar_longitude.is_ok(),
        "Solar longitude should be in range of i32"
    );
    let s = solar_longitude.unwrap_or_else(|e| e.saturate());
    let result_signed = (2 + s.div_euclid(30) - 1).rem_euclid(12) + 1;
    debug_assert!(result_signed >= 0);
    result_signed as u32
}

/// Returns true if the month of a given fixed date does not have a major solar term,
/// false otherwise.
///
/// Based on functions from _Calendrical Calculations_ by Reingold & Dershowitz.
/// Lisp reference code: https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L5345-L5351
pub(crate) fn no_major_solar_term<C: ChineseBased>(date: RataDie) -> bool {
    major_solar_term_from_fixed::<C>(date)
        == major_solar_term_from_fixed::<C>(new_moon_on_or_after::<C>((date + 1).as_moment()))
}

/// The fixed date in standard time at the observation location of the next new moon on or after a given Moment.
///
/// Based on functions from _Calendrical Calculations_ by Reingold & Dershowitz.
/// Lisp reference code: https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L5329-L5338
pub(crate) fn new_moon_on_or_after<C: ChineseBased>(moment: Moment) -> RataDie {
    let new_moon_moment = Astronomical::new_moon_at_or_after(midnight::<C>(moment));
    let location = C::location(new_moon_moment.as_rata_die());
    Location::standard_from_universal(new_moon_moment, location).as_rata_die()
}

/// The fixed date in standard time at the observation location of the previous new moon before a given Moment.
///
/// Based on functions from _Calendrical Calculations_ by Reingold & Dershowitz.
/// Lisp reference code: https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L5318-L5327
pub(crate) fn new_moon_before<C: ChineseBased>(moment: Moment) -> RataDie {
    let new_moon_moment = Astronomical::new_moon_before(midnight::<C>(moment));
    let location = C::location(new_moon_moment.as_rata_die());
    Location::standard_from_universal(new_moon_moment, location).as_rata_die()
}

/// Universal time of midnight at start of a Moment's day at the observation location
///
/// Based on functions from _Calendrical Calculations_ by Reingold & Dershowitz.
/// Lisp reference code: https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L5353-L5357
pub(crate) fn midnight<C: ChineseBased>(moment: Moment) -> Moment {
    Location::universal_from_standard(moment, C::location(moment.as_rata_die()))
}

/// Determines the fixed date of the lunar new year given the start of its corresponding solar year (歲), which is
/// also the winter solstice
///
/// Based on functions from _Calendrical Calculations_ by Reingold & Dershowitz.
/// Lisp reference code: https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L5370-L5394
pub(crate) fn new_year_in_sui<C: ChineseBased>(prior_solstice: RataDie) -> (RataDie, RataDie) {
    // s1 is prior_solstice
    // Using 370 here since solstices are ~365 days apart
    let following_solstice = winter_solstice_on_or_before::<C>(prior_solstice + 370); // s2
    let month_after_eleventh = new_moon_on_or_after::<C>((prior_solstice + 1).as_moment()); // m12
    let month_after_twelfth = new_moon_on_or_after::<C>((month_after_eleventh + 1).as_moment()); // m13
    let next_eleventh_month = new_moon_before::<C>((following_solstice + 1).as_moment()); // next-m11
    let lhs_argument =
        ((next_eleventh_month - month_after_eleventh) as f64 / MEAN_SYNODIC_MONTH).round() as i64;
    if lhs_argument == 12
        && (no_major_solar_term::<C>(month_after_eleventh)
            || no_major_solar_term::<C>(month_after_twelfth))
    {
        (
            new_moon_on_or_after::<C>((month_after_twelfth + 1).as_moment()),
            following_solstice,
        )
    } else {
        (month_after_twelfth, following_solstice)
    }
}

/// Get the moment of the nearest winter solstice on or before a given fixed date
///
/// Based on functions from _Calendrical Calculations_ by Reingold & Dershowitz.
/// Lisp reference code: https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L5359-L5368
pub(crate) fn winter_solstice_on_or_before<C: ChineseBased>(date: RataDie) -> RataDie {
    let approx = Astronomical::estimate_prior_solar_longitude(
        astronomy::WINTER,
        midnight::<C>((date + 1).as_moment()),
    );
    let mut iters = 0;
    let mut day = Moment::new((approx.inner() - 1.0).floor());
    while iters < MAX_ITERS_FOR_MONTHS_OF_YEAR
        && astronomy::WINTER
            >= Astronomical::solar_longitude(Astronomical::julian_centuries(midnight::<C>(
                day + 1.0,
            )))
    {
        iters += 1;
        day += 1.0;
    }
    debug_assert!(
        iters < MAX_ITERS_FOR_MONTHS_OF_YEAR,
        "Number of iterations was higher than expected"
    );
    day.as_rata_die()
}

/// Get the fixed date of the nearest Lunar New Year on or before a given fixed date.
/// This function also returns the solstice following a given date for optimization (see #3743).
///
/// To call this function you must precompute the value of the prior solstice, which
/// is the result of winter_solstice_on_or_before
///
/// Based on functions from _Calendrical Calculations_ by Reingold & Dershowitz.
/// Lisp reference code: https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L5396-L5405
pub(crate) fn new_year_on_or_before_fixed_date<C: ChineseBased>(
    date: RataDie,
    prior_solstice: RataDie,
) -> (RataDie, RataDie) {
    let new_year = new_year_in_sui::<C>(prior_solstice);
    if date >= new_year.0 {
        new_year
    } else {
        // This happens when we're at the end of the current lunar year
        // and the solstice has already happened. Thus the relevant solstice
        // for the current lunar year is the previous one, which we calculate by offsetting
        // back by a year.
        let date_in_last_sui = date - 180; // This date is in the current lunar year, but the last solar year
        let prior_solstice = winter_solstice_on_or_before::<C>(date_in_last_sui);
        new_year_in_sui::<C>(prior_solstice)
    }
}

/// Get a RataDie in the middle of a year; this is not necessarily meant for direct use in
/// calculations; rather, it is useful for getting a RataDie guaranteed to be in a given year
/// as input for other calculations like calculating the leap month in a year.
///
/// Based on functions from _Calendrical Calculations_ by Reingold & Dershowitz
/// Lisp reference code: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L5469-L5475>
pub fn fixed_mid_year_from_year<C: ChineseBased>(elapsed_years: i32) -> RataDie {
    let cycle = (elapsed_years - 1).div_euclid(60) + 1;
    let year = (elapsed_years - 1).rem_euclid(60) + 1;
    C::EPOCH + ((((cycle - 1) * 60 + year - 1) as f64 + 0.5) * MEAN_TROPICAL_YEAR) as i64
}

/// Whether this year is a leap year
pub fn is_leap_year<C: ChineseBased>(year: i32) -> bool {
    let mid_year = fixed_mid_year_from_year::<C>(year);
    YearBounds::compute::<C>(mid_year).is_leap()
}

/// The last month and day in this year
pub fn last_month_day_in_year<C: ChineseBased>(year: i32) -> (u8, u8) {
    let mid_year = fixed_mid_year_from_year::<C>(year);
    let year_bounds = YearBounds::compute::<C>(mid_year);
    let last_day = year_bounds.next_new_year - 1;
    let month = if year_bounds.is_leap() { 13 } else { 12 };
    let day = last_day - new_moon_before::<C>(last_day.as_moment()) + 1;
    (month, day as u8)
}

/// Calculated the numbers of days in the given year
pub fn days_in_provided_year<C: ChineseBased>(year: i32) -> u16 {
    let mid_year = fixed_mid_year_from_year::<C>(year);
    let bounds = YearBounds::compute::<C>(mid_year);

    bounds.count_days()
}

/// chinese_based_date_from_fixed returns extra things for use in caching
#[derive(Debug)]
#[non_exhaustive]
pub struct ChineseFromFixedResult {
    /// The chinese year
    pub year: i32,
    /// The chinese month
    pub month: u8,
    /// The chinese day
    pub day: u8,
    /// The bounds of the current lunar year
    pub year_bounds: YearBounds,
    /// The index of the leap month, if any
    pub leap_month: Option<NonZeroU8>,
}

/// Get a chinese based date from a fixed date, with the related ISO year
///
/// Months are calculated by iterating through the dates of new moons until finding the last month which
/// does not exceed the given fixed date. The day of month is calculated by subtracting the fixed date
/// from the fixed date of the beginning of the month.
///
/// The calculation for `elapsed_years` and `month` in this function are based on code from _Calendrical Calculations_ by Reingold & Dershowitz.
/// Lisp reference code: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L5414-L5459>
pub fn chinese_based_date_from_fixed<C: ChineseBased>(date: RataDie) -> ChineseFromFixedResult {
    let year_bounds = YearBounds::compute::<C>(date);
    let first_day_of_year = year_bounds.new_year;

    let year_float =
        (1.5 - 1.0 / 12.0 + ((first_day_of_year - C::EPOCH) as f64) / MEAN_TROPICAL_YEAR).floor();
    let year_int = i64_to_i32(year_float as i64);
    debug_assert!(year_int.is_ok(), "Year should be in range of i32");
    let year = year_int.unwrap_or_else(|e| e.saturate());

    let new_moon = new_moon_before::<C>((date + 1).as_moment());
    let month_i64 = ((new_moon - first_day_of_year) as f64 / MEAN_SYNODIC_MONTH).round() as i64 + 1;
    debug_assert!(
        ((u8::MIN as i64)..=(u8::MAX as i64)).contains(&month_i64),
        "Month should be in range of u8! Value {month_i64} failed for RD {date:?}"
    );
    let month = month_i64 as u8;
    let day_i64 = date - new_moon + 1;
    debug_assert!(
        ((u8::MIN as i64)..=(u8::MAX as i64)).contains(&month_i64),
        "Day should be in range of u8! Value {month_i64} failed for RD {date:?}"
    );
    let day = day_i64 as u8;
    let leap_month = if year_bounds.is_leap() {
        // This doesn't need to be checked for `None`, since `get_leap_month_from_new_year`
        // will always return a number greater than or equal to 1, and less than 14.
        NonZeroU8::new(get_leap_month_from_new_year::<C>(first_day_of_year))
    } else {
        None
    };

    ChineseFromFixedResult {
        year,
        month,
        day,
        year_bounds,
        leap_month,
    }
}

/// Given that `new_year` is the first day of a leap year, find which month in the year is a leap month.
/// Since the first month in which there are no major solar terms is a leap month, this function
/// cycles through months until it finds the leap month, then returns the number of that month. This
/// function assumes the date passed in is in a leap year and tests to ensure this is the case in debug
/// mode by asserting that no more than thirteen months are analyzed.
///
/// Conceptually similar to code from _Calendrical Calculations_ by Reingold & Dershowitz
/// Lisp reference code: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L5443-L5450>
pub fn get_leap_month_from_new_year<C: ChineseBased>(new_year: RataDie) -> u8 {
    let mut cur = new_year;
    let mut result = 1;
    while result < MAX_ITERS_FOR_MONTHS_OF_YEAR && !no_major_solar_term::<C>(cur) {
        cur = new_moon_on_or_after::<C>((cur + 1).as_moment());
        result += 1;
    }
    debug_assert!(result < MAX_ITERS_FOR_MONTHS_OF_YEAR, "The given year was not a leap year and an unexpected number of iterations occurred searching for a leap month.");
    result
}

/// Returns the number of days in the given (year, month). In the Chinese calendar, months start at each
/// new moon, so this function finds the number of days between the new moon at the beginning of the given
/// month and the new moon at the beginning of the next month.
pub fn month_days<C: ChineseBased>(year: i32, month: u8) -> u8 {
    let mid_year = fixed_mid_year_from_year::<C>(year);
    let prev_solstice = winter_solstice_on_or_before::<C>(mid_year);
    let new_year = new_year_on_or_before_fixed_date::<C>(mid_year, prev_solstice).0;
    days_in_month::<C>(month, new_year, None).0
}

/// Returns the number of days in the given `month` after the given `new_year`.
/// Also returns the RataDie of the new moon beginning the next month.
pub fn days_in_month<C: ChineseBased>(
    month: u8,
    new_year: RataDie,
    prev_new_moon: Option<RataDie>,
) -> (u8, RataDie) {
    let approx = new_year + ((month - 1) as i64 * 29);
    let prev_new_moon = if let Some(prev_moon) = prev_new_moon {
        prev_moon
    } else {
        new_moon_before::<C>((approx + 15).as_moment())
    };
    let next_new_moon = new_moon_on_or_after::<C>((approx + 15).as_moment());
    let result = (next_new_moon - prev_new_moon) as u8;
    debug_assert!(result == 29 || result == 30);
    (result, next_new_moon)
}

/// Given the new year and a month/day pair, calculate the number of days until the first day of the given month
pub fn days_until_month<C: ChineseBased>(new_year: RataDie, month: u8) -> u16 {
    let month_approx = 28_u16.saturating_mul(u16::from(month) - 1);

    let new_moon = new_moon_on_or_after::<C>(new_year.as_moment() + (month_approx as f64));
    let result = new_moon - new_year;
    debug_assert!(((u16::MIN as i64)..=(u16::MAX as i64)).contains(&result), "Result {result} from new moon: {new_moon:?} and new year: {new_year:?} should be in range of u16!");
    result as u16
}

#[cfg(test)]
mod test {

    use super::*;
    use crate::rata_die::Moment;

    #[test]
    fn test_chinese_new_moon_directionality() {
        for i in (-1000..1000).step_by(31) {
            let moment = Moment::new(i as f64);
            let before = new_moon_before::<Chinese>(moment);
            let after = new_moon_on_or_after::<Chinese>(moment);
            assert!(before < after, "Chinese new moon directionality failed for Moment: {moment:?}, with:\n\tBefore: {before:?}\n\tAfter: {after:?}");
        }
    }

    #[test]
    fn test_chinese_new_year_on_or_before() {
        let fixed = crate::iso::fixed_from_iso(2023, 6, 22);
        let prev_solstice = winter_solstice_on_or_before::<Chinese>(fixed);
        let result_fixed = new_year_on_or_before_fixed_date::<Chinese>(fixed, prev_solstice).0;
        let (y, m, d) = crate::iso::iso_from_fixed(result_fixed).unwrap();
        assert_eq!(y, 2023);
        assert_eq!(m, 1);
        assert_eq!(d, 22);
    }

    fn seollal_on_or_before(fixed: RataDie) -> RataDie {
        let prev_solstice = winter_solstice_on_or_before::<Dangi>(fixed);
        new_year_on_or_before_fixed_date::<Dangi>(fixed, prev_solstice).0
    }

    #[test]
    fn test_seollal() {
        #[derive(Debug)]
        struct TestCase {
            iso_year: i32,
            iso_month: u8,
            iso_day: u8,
            expected_year: i32,
            expected_month: u8,
            expected_day: u8,
        }

        let cases = [
            TestCase {
                iso_year: 2024,
                iso_month: 6,
                iso_day: 6,
                expected_year: 2024,
                expected_month: 2,
                expected_day: 10,
            },
            TestCase {
                iso_year: 2024,
                iso_month: 2,
                iso_day: 9,
                expected_year: 2023,
                expected_month: 1,
                expected_day: 22,
            },
            TestCase {
                iso_year: 2023,
                iso_month: 1,
                iso_day: 22,
                expected_year: 2023,
                expected_month: 1,
                expected_day: 22,
            },
            TestCase {
                iso_year: 2023,
                iso_month: 1,
                iso_day: 21,
                expected_year: 2022,
                expected_month: 2,
                expected_day: 1,
            },
            TestCase {
                iso_year: 2022,
                iso_month: 6,
                iso_day: 6,
                expected_year: 2022,
                expected_month: 2,
                expected_day: 1,
            },
            TestCase {
                iso_year: 2021,
                iso_month: 6,
                iso_day: 6,
                expected_year: 2021,
                expected_month: 2,
                expected_day: 12,
            },
            TestCase {
                iso_year: 2020,
                iso_month: 6,
                iso_day: 6,
                expected_year: 2020,
                expected_month: 1,
                expected_day: 25,
            },
            TestCase {
                iso_year: 2019,
                iso_month: 6,
                iso_day: 6,
                expected_year: 2019,
                expected_month: 2,
                expected_day: 5,
            },
            TestCase {
                iso_year: 2018,
                iso_month: 6,
                iso_day: 6,
                expected_year: 2018,
                expected_month: 2,
                expected_day: 16,
            },
            TestCase {
                iso_year: 2025,
                iso_month: 6,
                iso_day: 6,
                expected_year: 2025,
                expected_month: 1,
                expected_day: 29,
            },
            TestCase {
                iso_year: 2026,
                iso_month: 8,
                iso_day: 8,
                expected_year: 2026,
                expected_month: 2,
                expected_day: 17,
            },
            TestCase {
                iso_year: 2027,
                iso_month: 4,
                iso_day: 4,
                expected_year: 2027,
                expected_month: 2,
                expected_day: 7,
            },
            TestCase {
                iso_year: 2028,
                iso_month: 9,
                iso_day: 21,
                expected_year: 2028,
                expected_month: 1,
                expected_day: 27,
            },
        ];

        for case in cases {
            let fixed = crate::iso::fixed_from_iso(case.iso_year, case.iso_month, case.iso_day);
            let seollal = seollal_on_or_before(fixed);
            let (y, m, d) = crate::iso::iso_from_fixed(seollal).unwrap();
            assert_eq!(
                y, case.expected_year,
                "Year check failed for case: {case:?}"
            );
            assert_eq!(
                m, case.expected_month,
                "Month check failed for case: {case:?}"
            );
            assert_eq!(d, case.expected_day, "Day check failed for case: {case:?}");
        }
    }
}
