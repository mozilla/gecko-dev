// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// Provider structs must be stable
#![allow(clippy::exhaustive_structs, clippy::exhaustive_enums)]

//! 🚧 \[Unstable\] Data provider struct definitions for this ICU4X component.
//!
//! <div class="stab unstable">
//! 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
//! including in SemVer minor releases. While the serde representation of data structs is guaranteed
//! to be stable, their Rust representation might not be. Use with caution.
//! </div>
//!
//! Read more about data providers: [`icu_provider`]

use crate::zone::{UtcOffset, VariantOffsets, ZoneNameTimestamp};
#[cfg(feature = "datagen")]
use icu_provider::prelude::*;
use zerovec::maps::ZeroMapKV;
use zerovec::ule::AsULE;
use zerovec::{ZeroMap2d, ZeroSlice, ZeroVec};

pub use crate::zone::ule::TimeZoneVariantULE;
pub use crate::zone::TimeZone;
pub mod iana;
pub mod windows;

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
#[allow(unused_imports)]
const _: () = {
    use icu_time_data::*;
    pub mod icu {
        pub use crate as time;
    }
    make_provider!(Baked);
    impl_timezone_identifiers_iana_extended_v1!(Baked);
    impl_timezone_identifiers_iana_core_v1!(Baked);
    impl_timezone_identifiers_windows_v1!(Baked);
    impl_timezone_variants_offsets_v1!(Baked);
};

#[cfg(feature = "datagen")]
/// The latest minimum set of markers required by this component.
pub const MARKERS: &[DataMarkerInfo] = &[
    iana::TimezoneIdentifiersIanaExtendedV1::INFO,
    iana::TimezoneIdentifiersIanaCoreV1::INFO,
    windows::TimezoneIdentifiersWindowsV1::INFO,
    TimezoneVariantsOffsetsV1::INFO,
];

const SECONDS_TO_EIGHTS_OF_HOURS: i32 = 60 * 60 / 8;

impl AsULE for VariantOffsets {
    type ULE = [i8; 2];

    fn from_unaligned([std, dst]: Self::ULE) -> Self {
        fn decode(encoded: i8) -> i32 {
            encoded as i32 * SECONDS_TO_EIGHTS_OF_HOURS
                + match encoded % 8 {
                    // 7.5, 37.5, representing 10, 40
                    1 | 5 => 150,
                    -1 | -5 => -150,
                    // 22.5, 52.5, representing 20, 50
                    3 | 7 => -150,
                    -3 | -7 => 150,
                    // 0, 15, 30, 45
                    _ => 0,
                }
        }

        Self {
            standard: UtcOffset::from_seconds_unchecked(decode(std)),
            daylight: (dst != 0).then(|| UtcOffset::from_seconds_unchecked(decode(std + dst))),
        }
    }

    fn to_unaligned(self) -> Self::ULE {
        fn encode(offset: i32) -> i8 {
            debug_assert_eq!(offset.abs() % 60, 0);
            let scaled = match offset.abs() / 60 % 60 {
                0 | 15 | 30 | 45 => offset / SECONDS_TO_EIGHTS_OF_HOURS,
                10 | 40 => {
                    // stored as 7.5, 37.5, truncating div
                    offset / SECONDS_TO_EIGHTS_OF_HOURS
                }
                20 | 50 => {
                    // stored as 22.5, 52.5, need to add one
                    offset / SECONDS_TO_EIGHTS_OF_HOURS + offset.signum()
                }
                _ => {
                    debug_assert!(false, "{offset:?}");
                    offset / SECONDS_TO_EIGHTS_OF_HOURS
                }
            };
            debug_assert!(i8::MIN as i32 <= scaled && scaled <= i8::MAX as i32);
            scaled as i8
        }
        [
            encode(self.standard.to_seconds()),
            self.daylight
                .map(|d| encode(d.to_seconds() - self.standard.to_seconds()))
                .unwrap_or_default(),
        ]
    }
}

#[test]
fn offsets_ule() {
    #[track_caller]
    fn assert_round_trip(offset: UtcOffset) {
        let variants = VariantOffsets::from_standard(offset);
        assert_eq!(
            variants,
            VariantOffsets::from_unaligned(VariantOffsets::to_unaligned(variants))
        );
    }

    assert_round_trip(UtcOffset::try_from_str("+01:00").unwrap());
    assert_round_trip(UtcOffset::try_from_str("+01:15").unwrap());
    assert_round_trip(UtcOffset::try_from_str("+01:30").unwrap());
    assert_round_trip(UtcOffset::try_from_str("+01:45").unwrap());

    assert_round_trip(UtcOffset::try_from_str("+01:10").unwrap());
    assert_round_trip(UtcOffset::try_from_str("+01:20").unwrap());
    assert_round_trip(UtcOffset::try_from_str("+01:40").unwrap());
    assert_round_trip(UtcOffset::try_from_str("+01:50").unwrap());

    assert_round_trip(UtcOffset::try_from_str("-01:00").unwrap());
    assert_round_trip(UtcOffset::try_from_str("-01:15").unwrap());
    assert_round_trip(UtcOffset::try_from_str("-01:30").unwrap());
    assert_round_trip(UtcOffset::try_from_str("-01:45").unwrap());

    assert_round_trip(UtcOffset::try_from_str("-01:10").unwrap());
    assert_round_trip(UtcOffset::try_from_str("-01:20").unwrap());
    assert_round_trip(UtcOffset::try_from_str("-01:40").unwrap());
    assert_round_trip(UtcOffset::try_from_str("-01:50").unwrap());
}

impl<'a> ZeroMapKV<'a> for VariantOffsets {
    type Container = ZeroVec<'a, Self>;
    type Slice = ZeroSlice<Self>;
    type GetType = <Self as AsULE>::ULE;
    type OwnedType = Self;
}

#[cfg(all(feature = "alloc", feature = "serde"))]
impl serde::Serialize for VariantOffsets {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        if serializer.is_human_readable() {
            serializer.serialize_str(&if let Some(dst) = self.daylight {
                alloc::format!(
                    "{:+02}:{:02}/{:+02}:{:02}",
                    self.standard.hours_part(),
                    self.standard.minutes_part(),
                    dst.hours_part(),
                    dst.minutes_part(),
                )
            } else {
                alloc::format!(
                    "{:+02}:{:02}",
                    self.standard.hours_part(),
                    self.standard.minutes_part(),
                )
            })
        } else {
            self.to_unaligned().serialize(serializer)
        }
    }
}

#[cfg(feature = "serde")]
impl<'de> serde::Deserialize<'de> for VariantOffsets {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        use serde::de::Error;
        if deserializer.is_human_readable() {
            let raw = <&str>::deserialize(deserializer)?;
            Ok(if let Some((std, dst)) = raw.split_once('/') {
                Self {
                    standard: UtcOffset::try_from_str(std)
                        .map_err(|_| D::Error::custom("invalid offset"))?,
                    daylight: Some(
                        UtcOffset::try_from_str(dst)
                            .map_err(|_| D::Error::custom("invalid offset"))?,
                    ),
                }
            } else {
                Self {
                    standard: UtcOffset::try_from_str(raw)
                        .map_err(|_| D::Error::custom("invalid offset"))?,
                    daylight: None,
                }
            })
        } else {
            <_>::deserialize(deserializer).map(Self::from_unaligned)
        }
    }
}

icu_provider::data_marker!(
    /// The default mapping between period and offsets. The second level key is a wall-clock time encoded as
    /// [`ZoneNameTimestamp`]. It represents when the offsets started to be used.
    ///
    /// The values are the standard offset, and the daylight offset *relative to the standard offset*. As such,
    /// if the second value is 0, there is no daylight time.
    TimezoneVariantsOffsetsV1,
    "timezone/variants/offsets/v1",
    ZeroMap2d<'static, TimeZone, ZoneNameTimestamp, VariantOffsets>,
    is_singleton = true
);
