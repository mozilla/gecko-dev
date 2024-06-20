// This file is part of ICU4X.
//
// The contents of this file implement algorithms from Calendrical Calculations
// by Reingold & Dershowitz, Cambridge University Press, 4th edition (2018),
// which have been released as Lisp code at <https://github.com/EdReingold/calendar-code2/>
// under the Apache-2.0 license. Accordingly, this file is released under
// the Apache License, Version 2.0 which can be found at the calendrical_calculations
// package root or at http://www.apache.org/licenses/LICENSE-2.0.

use crate::helpers::{i64_to_i32, I32CastError, IntegerRoundings};
use crate::rata_die::RataDie;
/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L4720>
// Book states that the Persian epoch is the date: 3/19/622 and since the Persian Calendar has no year 0, the best choice was to use the Julian function.
const FIXED_PERSIAN_EPOCH: RataDie = crate::julian::fixed_from_julian(622, 3, 19);

/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L4803>
pub fn fixed_from_arithmetic_persian(year: i32, month: u8, day: u8) -> RataDie {
    let p_year = i64::from(year);
    let month = i64::from(month);
    let day = i64::from(day);
    let y = if p_year > 0 {
        p_year - 474
    } else {
        p_year - 473
    };
    let year = y.rem_euclid(2820) + 474;

    RataDie::new(
        FIXED_PERSIAN_EPOCH.to_i64_date() - 1
            + 1029983 * y.div_euclid(2820)
            + 365 * (year - 1)
            + (31 * year - 5).div_euclid(128)
            + if month <= 7 {
                31 * (month - 1)
            } else {
                30 * (month - 1) + 6
            }
            + day,
    )
}

/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L4857>
pub fn arithmetic_persian_from_fixed(date: RataDie) -> Result<(i32, u8, u8), I32CastError> {
    let year = arithmetic_persian_year_from_fixed(date);
    let year = i64_to_i32(year)?;
    #[allow(clippy::unwrap_used)] // valid month,day
    let day_of_year = 1_i64 + (date - fixed_from_arithmetic_persian(year, 1, 1));
    #[allow(unstable_name_collisions)] // div_ceil is unstable and polyfilled
    let month = if day_of_year <= 186 {
        day_of_year.div_ceil(31) as u8
    } else {
        (day_of_year - 6).div_ceil(30) as u8
    };
    let day = (date - fixed_from_arithmetic_persian(year, month, 1) + 1) as u8;
    Ok((year, month, day))
}

/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L4829>
fn arithmetic_persian_year_from_fixed(date: RataDie) -> i64 {
    let d0 = date - fixed_from_arithmetic_persian(475, 1, 1);
    let n2820 = d0.div_euclid(1029983);
    let d1 = d0.rem_euclid(1029983);
    let y2820 = if d1 == 1029982 {
        2820
    } else {
        (128 * d1 + 46878).div_euclid(46751)
    };
    let year = 474 + n2820 * 2820 + y2820;
    if year > 0 {
        year
    } else {
        year - 1
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn test_persian_epoch() {
        let epoch = FIXED_PERSIAN_EPOCH.to_i64_date();
        // Iso year of Persian Epoch
        let epoch_year_from_fixed = crate::iso::iso_year_from_fixed(RataDie::new(epoch));
        // 622 is the correct ISO year for the Persian Epoch
        assert_eq!(epoch_year_from_fixed, 622);
    }

    // Persian New Year occurring in March of Gregorian year (g_year) to fixed date
    fn nowruz(g_year: i32) -> RataDie {
        let (y, _m, _d) = crate::iso::iso_from_fixed(FIXED_PERSIAN_EPOCH).unwrap();
        let persian_year = g_year - y + 1;
        let year = if persian_year <= 0 {
            persian_year - 1
        } else {
            persian_year
        };
        fixed_from_arithmetic_persian(year, 1, 1)
    }

    #[test]
    fn test_nowruz() {
        let fixed_date = nowruz(622).to_i64_date();
        assert_eq!(fixed_date, FIXED_PERSIAN_EPOCH.to_i64_date());
        // These values are used as test data in appendix C of the "Calendrical Calculations" book
        let nowruz_test_year_start = 2000;
        let nowruz_test_year_end = 2103;

        for year in nowruz_test_year_start..=nowruz_test_year_end {
            let two_thousand_eight_to_fixed = nowruz(year).to_i64_date();
            let iso_date = crate::iso::fixed_from_iso(year, 3, 21);
            let (persian_year, _m, _d) = arithmetic_persian_from_fixed(iso_date).unwrap();
            assert_eq!(
                arithmetic_persian_from_fixed(RataDie::new(two_thousand_eight_to_fixed))
                    .unwrap()
                    .0,
                persian_year
            );
        }
    }
}
