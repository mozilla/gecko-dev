// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! 🚧 \[Unstable\] Data provider struct definitions for this ICU4X component.
//!
//! <div class="stab unstable">
//! 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
//! including in SemVer minor releases. While the serde representation of data structs is guaranteed
//! to be stable, their Rust representation might not be. Use with caution.
//! </div>
//!
//! Read more about data providers: [`icu_provider`]

// Provider structs must be stable
#![allow(clippy::exhaustive_structs, clippy::exhaustive_enums)]

use crate::types::IsoWeekday;
use core::str::FromStr;
use icu_provider::prelude::*;
use tinystr::TinyStr16;
use zerovec::ZeroVec;

#[cfg(feature = "compiled_data")]
#[derive(Debug)]
/// Baked data
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
pub struct Baked;

#[cfg(feature = "compiled_data")]
const _: () = {
    pub mod icu {
        pub use crate as calendar;
        pub use icu_locid_transform as locid_transform;
    }
    icu_calendar_data::make_provider!(Baked);
    icu_calendar_data::impl_calendar_japanese_v1!(Baked);
    icu_calendar_data::impl_calendar_japanext_v1!(Baked);
    icu_calendar_data::impl_datetime_week_data_v1!(Baked);
};

#[cfg(feature = "datagen")]
/// The latest minimum set of keys required by this component.
pub const KEYS: &[DataKey] = &[
    JapaneseErasV1Marker::KEY,
    JapaneseExtendedErasV1Marker::KEY,
    WeekDataV1Marker::KEY,
];

/// The date at which an era started
///
/// The order of fields in this struct is important!
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[zerovec::make_ule(EraStartDateULE)]
#[derive(
    Copy, Clone, PartialEq, PartialOrd, Eq, Ord, Hash, Debug, yoke::Yokeable, zerofrom::ZeroFrom,
)]
#[cfg_attr(
    feature = "datagen",
    derive(serde::Serialize, databake::Bake),
    databake(path = icu_calendar::provider),
)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct EraStartDate {
    /// The year the era started in
    pub year: i32,
    /// The month the era started in
    pub month: u8,
    /// The day the era started in
    pub day: u8,
}

/// A data structure containing the necessary era data for constructing a
/// [`Japanese`](crate::japanese::Japanese) calendar object
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[icu_provider::data_struct(
    marker(JapaneseErasV1Marker, "calendar/japanese@1", singleton),
    marker(JapaneseExtendedErasV1Marker, "calendar/japanext@1", singleton)
)]
#[derive(Debug, PartialEq, Clone, Default)]
#[cfg_attr(
    feature = "datagen",
    derive(serde::Serialize, databake::Bake),
    databake(path = icu_calendar::provider),
)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct JapaneseErasV1<'data> {
    /// A map from era start dates to their era codes
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub dates_to_eras: ZeroVec<'data, (EraStartDate, TinyStr16)>,
}

impl FromStr for EraStartDate {
    type Err = ();
    fn from_str(mut s: &str) -> Result<Self, ()> {
        let sign = if let Some(suffix) = s.strip_prefix('-') {
            s = suffix;
            -1
        } else {
            1
        };

        let mut split = s.split('-');
        let year = split.next().ok_or(())?.parse::<i32>().map_err(|_| ())? * sign;
        let month = split.next().ok_or(())?.parse().map_err(|_| ())?;
        let day = split.next().ok_or(())?.parse().map_err(|_| ())?;

        Ok(EraStartDate { year, month, day })
    }
}

/// An ICU4X mapping to a subset of CLDR weekData.
/// See CLDR-JSON's weekData.json for more context.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[icu_provider::data_struct(marker(
    WeekDataV1Marker,
    "datetime/week_data@1",
    fallback_by = "region"
))]
#[derive(Clone, Copy, Debug, PartialEq)]
#[cfg_attr(
    feature = "datagen",
    derive(serde::Serialize, databake::Bake),
    databake(path = icu_calendar::provider),
)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[allow(clippy::exhaustive_structs)] // used in data provider
pub struct WeekDataV1 {
    /// The first day of a week.
    pub first_weekday: IsoWeekday,
    /// For a given week, the minimum number of that week's days present in a given month or year for the week to be considered part of that month or year.
    pub min_week_days: u8,
}
