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

pub mod chinese_based;
pub mod islamic;
pub use chinese_based::{ChineseCacheV1Marker, DangiCacheV1Marker};
pub use islamic::{IslamicObservationalCacheV1Marker, IslamicUmmAlQuraCacheV1Marker};

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
        #[allow(unused_imports)] // baked data may or may not need this
        pub use icu_locid_transform as locid_transform;
    }
    icu_calendar_data::make_provider!(Baked);
    icu_calendar_data::impl_calendar_chinesecache_v1!(Baked);
    icu_calendar_data::impl_calendar_dangicache_v1!(Baked);
    icu_calendar_data::impl_calendar_islamicobservationalcache_v1!(Baked);
    icu_calendar_data::impl_calendar_islamicummalquracache_v1!(Baked);
    icu_calendar_data::impl_calendar_japanese_v1!(Baked);
    icu_calendar_data::impl_calendar_japanext_v1!(Baked);
    icu_calendar_data::impl_datetime_week_data_v1!(Baked);
    icu_calendar_data::impl_datetime_week_data_v2!(Baked);
};

#[cfg(feature = "datagen")]
/// The latest minimum set of keys required by this component.
pub const KEYS: &[DataKey] = &[
    ChineseCacheV1Marker::KEY,
    DangiCacheV1Marker::KEY,
    IslamicObservationalCacheV1Marker::KEY,
    IslamicUmmAlQuraCacheV1Marker::KEY,
    JapaneseErasV1Marker::KEY,
    JapaneseExtendedErasV1Marker::KEY,
    WeekDataV2Marker::KEY,
    // We include the duplicate data for now, as icu_datetime loads it directly
    // https://github.com/unicode-org/icu4x/pull/4364#discussion_r1419877997
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
    fn from_str(mut s: &str) -> Result<Self, Self::Err> {
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

/// An ICU4X mapping to a subset of CLDR weekData.
/// See CLDR-JSON's weekData.json for more context.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[icu_provider::data_struct(marker(
    WeekDataV2Marker,
    "datetime/week_data@2",
    fallback_by = "region"
))]
#[derive(Clone, Copy, Debug, PartialEq)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake), databake(path = icu_calendar::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[allow(clippy::exhaustive_structs)] // used in data provider
pub struct WeekDataV2 {
    /// The first day of a week.
    pub first_weekday: IsoWeekday,
    /// For a given week, the minimum number of that week's days present in a given month or year for the week to be considered part of that month or year.
    pub min_week_days: u8,
    /// Bitset representing weekdays that are part of the 'weekend', for calendar purposes.
    /// The number of days can be different between locales, and may not be contiguous.
    pub weekend: WeekdaySet,
}

/// Bitset representing weekdays.
//
// This Bitset uses an [u8] to represent the weekend, thus leaving one bit free.
// Each bit represents a day in the following order:
//
//   ┌▷Mon
//   │┌▷Tue
//   ││┌▷Wed
//   │││┌▷Thu
//   ││││ ┌▷Fri
//   ││││ │┌▷Sat
//   ││││ ││┌▷Sun
//   ││││ │││
// 0b0000_1010
//
// Please note that this is not a range, this are the discrete days representing a weekend. Other examples:
// 0b0101_1000 -> Tue, Thu, Fri
// 0b0000_0110 -> Sat, Sun
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct WeekdaySet(u8);

impl WeekdaySet {
    /// Returns whether the set contains the day.
    pub const fn contains(&self, day: IsoWeekday) -> bool {
        self.0 & day.bit_value() != 0
    }
}

impl WeekdaySet {
    /// Creates a new [WeekdaySet] using the provided days.
    pub const fn new(days: &[IsoWeekday]) -> Self {
        let mut i = 0;
        let mut w = 0;
        #[allow(clippy::indexing_slicing)]
        while i < days.len() {
            w |= days[i].bit_value();
            i += 1;
        }
        Self(w)
    }
}

impl IsoWeekday {
    /// Defines the bit order used for encoding and reading weekend days.
    const fn bit_value(&self) -> u8 {
        match self {
            IsoWeekday::Monday => 1 << 6,
            IsoWeekday::Tuesday => 1 << 5,
            IsoWeekday::Wednesday => 1 << 4,
            IsoWeekday::Thursday => 1 << 3,
            IsoWeekday::Friday => 1 << 2,
            IsoWeekday::Saturday => 1 << 1,
            IsoWeekday::Sunday => 1 << 0,
        }
    }
}

#[cfg(feature = "datagen")]
impl databake::Bake for WeekdaySet {
    fn bake(&self, ctx: &databake::CrateEnv) -> databake::TokenStream {
        ctx.insert("icu_calendar");
        let days =
            crate::week_of::WeekdaySetIterator::new(IsoWeekday::Monday, *self).map(|d| d.bake(ctx));
        databake::quote! {
            icu_calendar::provider::WeekdaySet::new(&[#(#days),*])
        }
    }
}

#[cfg(feature = "datagen")]
impl serde::Serialize for WeekdaySet {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        if serializer.is_human_readable() {
            crate::week_of::WeekdaySetIterator::new(IsoWeekday::Monday, *self)
                .collect::<alloc::vec::Vec<_>>()
                .serialize(serializer)
        } else {
            self.0.serialize(serializer)
        }
    }
}

#[cfg(feature = "serde")]
impl<'de> serde::Deserialize<'de> for WeekdaySet {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        if deserializer.is_human_readable() {
            alloc::vec::Vec::<IsoWeekday>::deserialize(deserializer).map(|s| Self::new(&s))
        } else {
            u8::deserialize(deserializer).map(Self)
        }
    }
}

#[test]
fn test_weekdayset_bake() {
    databake::test_bake!(
        WeekdaySet,
        const: crate::provider::WeekdaySet::new(
            &[crate::types::IsoWeekday::Monday, crate::types::IsoWeekday::Wednesday, crate::types::IsoWeekday::Friday]),
        icu_calendar
    );
}

#[test]
fn test_weekdayset_new() {
    use IsoWeekday::*;

    let sat_sun_bitmap = Saturday.bit_value() | Sunday.bit_value();
    let sat_sun_weekend = WeekdaySet::new(&[Saturday, Sunday]);
    assert_eq!(sat_sun_bitmap, sat_sun_weekend.0);

    let fri_sat_bitmap = Friday.bit_value() | Saturday.bit_value();
    let fri_sat_weekend = WeekdaySet::new(&[Friday, Saturday]);
    assert_eq!(fri_sat_bitmap, fri_sat_weekend.0);

    let fri_sun_bitmap = Friday.bit_value() | Sunday.bit_value();
    let fri_sun_weekend = WeekdaySet::new(&[Friday, Sunday]);
    assert_eq!(fri_sun_bitmap, fri_sun_weekend.0);

    let fri_bitmap = Friday.bit_value();
    let fri_weekend = WeekdaySet::new(&[Friday, Friday]);
    assert_eq!(fri_bitmap, fri_weekend.0);

    let sun_mon_bitmap = Sunday.bit_value() | Monday.bit_value();
    let sun_mon_weekend = WeekdaySet::new(&[Sunday, Monday]);
    assert_eq!(sun_mon_bitmap, sun_mon_weekend.0);

    let mon_sun_bitmap = Monday.bit_value() | Sunday.bit_value();
    let mon_sun_weekend = WeekdaySet::new(&[Monday, Sunday]);
    assert_eq!(mon_sun_bitmap, mon_sun_weekend.0);

    let mon_bitmap = Monday.bit_value();
    let mon_weekend = WeekdaySet::new(&[Monday]);
    assert_eq!(mon_bitmap, mon_weekend.0);
}
