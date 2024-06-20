// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Compiled and compressed data for Chinese calendar years; includes `RataDie` types for the beginning of years

use crate::chinese_based::PackedChineseBasedCompiledData;
/// The minimum year present in CHINESE_DATA_ARRAY
pub(crate) const MIN_YEAR: i32 = 4660;
pub(crate) const MIN_YEAR_ISO: i32 = 2023;

/// The array of year data for Chinese years between MIN_YEAR and MAX_YEAR; currently, this array must also have
/// an entry for the year after max year, since the function for unpacking this data also uses the next entry's new year.
///
/// Each data entry consists of a `ChineseData`, which in turn is composed of three bytes of data.
/// The first 5 bits represents the offset of the Chinese New Year in the given year from Jan 21 of that year.
/// The next 13 bits are used to indicate the month lengths of the 13 months; a 1 represents 30 days, and a 0 represents 29 days;
/// if there is no 13th month, the 13th in this sequence will be set to 0, but this does not represent 29 days in this case.
/// The final 6 bits indicate which ordinal month is a leap month, or is set to zero if the year is not a leap year.
///
/// TODO: Generate this data
#[allow(clippy::unusual_byte_groupings)]
pub(crate) const CHINESE_DATA_ARRAY: [PackedChineseBasedCompiledData; 2] = [
    PackedChineseBasedCompiledData(0b_00001_010, 0b_01101101, 0b_01_000011),
    PackedChineseBasedCompiledData(0b_10100_010, 0b_01011011, 0b_00_000000),
];
