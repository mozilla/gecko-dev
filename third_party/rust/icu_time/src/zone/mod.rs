// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Types for resolving and manipulating time zones.
//!
//! # Fields
//!
//! In ICU4X, a [`TimeZoneInfo`] consists of up to four different fields:
//!
//! 1. The time zone ID
//! 2. The offset from UTC
//! 3. A timestamp, as time zone names can change over time
//! 4. The zone variant, representing concepts such as Standard, Summer, Daylight, and Ramadan time
//!
//! ## Time Zone
//!
//! The time zone ID corresponds to a time zone from the time zone database. The time zone ID
//! usually corresponds to the largest city in the time zone.
//!
//! There are two mostly-interchangeable standards for time zone IDs:
//!
//! 1. IANA time zone IDs, like `"America/Chicago"`
//! 2. BCP-47 time zone IDs, like `"uschi"`
//!
//! ICU4X uses BCP-47 time zone IDs for all of its APIs. To get a BCP-47 time zone from an
//! IANA time zone, use [`IanaParser`].
//!
//! ## UTC Offset
//!
//! The UTC offset precisely states the time difference between the time zone in question and
//! Coordinated Universal Time (UTC).
//!
//! In localized strings, it is often rendered as "UTC-6", meaning 6 hours less than UTC (some locales
//! use the term "GMT" instead of "UTC").
//!
//! ## Timestamp
//!
//! Some time zones change names over time, such as when changing "metazone". For example, Portugal changed from
//! "Western European Time" to "Central European Time" and back in the 1990s, without changing time zone ID
//! (`Europe/Lisbon`, `ptlis`). Therefore, a timestamp is needed to resolve such generic time zone names.
//!
//! It is not required to set the timestamp on [`TimeZoneInfo`]. If it is not set, some string
//! formats may be unsupported.
//!
//! ## Zone Variant
//!
//! Many zones use different names and offsets in the summer than in the winter. In ICU4X,
//! this is called the _zone variant_.
//!
//! CLDR has two zone variants, named `"standard"` and `"daylight"`. However, the mapping of these
//! variants to specific observed offsets varies from time zone to time zone, and they may not
//! consistently represent winter versus summer time.
//!
//! Note: It is not required to set the zone variant on [`TimeZoneInfo`]. If it is not set, some string
//! formats may be unsupported.
//!
//! # Obtaining time zone information
//!
//! This crate does not ship time zone offset information. Other Rust crates such as [`chrono_tz`](https://docs.rs/chrono-tz) or [`jiff`](https://docs.rs/jiff)
//! are available for this purpose. See our [`example`](https://github.com/unicode-org/icu4x/blob/main/components/icu/examples/chrono_jiff.rs).

pub mod iana;
mod offset;
pub mod windows;
mod zone_name_timestamp;

#[doc(inline)]
pub use offset::InvalidOffsetError;
pub use offset::UtcOffset;
pub use offset::VariantOffsets;
pub use offset::VariantOffsetsCalculator;
pub use offset::VariantOffsetsCalculatorBorrowed;

#[doc(no_inline)]
pub use iana::{IanaParser, IanaParserBorrowed};
#[doc(no_inline)]
pub use windows::{WindowsParser, WindowsParserBorrowed};

pub use zone_name_timestamp::ZoneNameTimestamp;

use crate::scaffold::IntoOption;
use crate::DateTime;
use core::fmt;
use core::ops::Deref;
use icu_calendar::Iso;
use icu_locale_core::subtags::{subtag, Subtag};
use icu_provider::prelude::yoke;
use zerovec::ule::{AsULE, ULE};
use zerovec::{ZeroSlice, ZeroVec};

/// Time zone data model choices.
pub mod models {
    use super::*;
    mod private {
        pub trait Sealed {}
    }

    /// Trait encoding a particular data model for time zones.
    ///
    /// <div class="stab unstable">
    /// 🚫 This trait is sealed; it cannot be implemented by user code. If an API requests an item that implements this
    /// trait, please consider using a type from the implementors listed below.
    /// </div>
    pub trait TimeZoneModel: private::Sealed {
        /// The zone variant, if required for this time zone model.
        type TimeZoneVariant: IntoOption<TimeZoneVariant> + fmt::Debug + Copy;
        /// The local time, if required for this time zone model.
        type ZoneNameTimestamp: IntoOption<ZoneNameTimestamp> + fmt::Debug + Copy;
    }

    /// A time zone containing a time zone ID and optional offset.
    #[derive(Debug, PartialEq, Eq)]
    #[non_exhaustive]
    pub struct Base;

    impl private::Sealed for Base {}
    impl TimeZoneModel for Base {
        type TimeZoneVariant = ();
        type ZoneNameTimestamp = ();
    }

    /// A time zone containing a time zone ID, optional offset, and local time.
    #[derive(Debug, PartialEq, Eq)]
    #[non_exhaustive]
    pub struct AtTime;

    impl private::Sealed for AtTime {}
    impl TimeZoneModel for AtTime {
        type TimeZoneVariant = ();
        type ZoneNameTimestamp = ZoneNameTimestamp;
    }

    /// A time zone containing a time zone ID, optional offset, local time, and zone variant.
    #[derive(Debug, PartialEq, Eq)]
    #[non_exhaustive]
    pub struct Full;

    impl private::Sealed for Full {}
    impl TimeZoneModel for Full {
        type TimeZoneVariant = TimeZoneVariant;
        type ZoneNameTimestamp = ZoneNameTimestamp;
    }
}

/// A CLDR time zone identity.
///
/// **The primary definition of this type is in the [`icu_time`](https://docs.rs/icu_time) crate. Other ICU4X crates re-export it for convenience.**
///
/// This can be created directly from BCP-47 strings, or it can be parsed from IANA IDs.
///
/// CLDR uses difference equivalence classes than IANA. For example, `Europe/Oslo` is
/// an alias to `Europe/Berlin` in IANA (because they agree since 1970), but these are
/// different identities in CLDR, as we want to be able to say "Norway Time" and
/// "Germany Time". On the other hand `Europe/Belfast` and `Europe/London` are the same
/// CLDR identity ("UK Time").
///
/// See the docs on [`zone`](crate::zone) for more information.
///
/// ```
/// use icu::locale::subtags::subtag;
/// use icu::time::zone::{IanaParser, TimeZone};
///
/// let parser = IanaParser::new();
/// assert_eq!(parser.parse("Europe/Oslo"), TimeZone(subtag!("noosl")));
/// assert_eq!(parser.parse("Europe/Berlin"), TimeZone(subtag!("deber")));
/// assert_eq!(parser.parse("Europe/Belfast"), TimeZone(subtag!("gblon")));
/// assert_eq!(parser.parse("Europe/London"), TimeZone(subtag!("gblon")));
/// ```
#[repr(transparent)]
#[derive(Debug, Clone, Copy, Eq, Ord, PartialEq, PartialOrd, yoke::Yokeable, ULE, Hash)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_time::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[allow(clippy::exhaustive_structs)] // This is a stable newtype
pub struct TimeZone(pub Subtag);

impl TimeZone {
    /// The synthetic `Etc/Unknown` time zone.
    ///
    /// This is the result of parsing unknown zones. It's important that such parsing does not
    /// fail, as new zones are added all the time, and ICU4X might not be up to date.
    pub const UNKNOWN: Self = Self(subtag!("unk"));

    /// Whether this [`TimeZone`] equals [`TimeZone::UNKNOWN`].
    pub const fn is_unknown(self) -> bool {
        matches!(self, Self::UNKNOWN)
    }
}

/// This module exists so we can cleanly reexport TimeZoneVariantULE from the provider module, whilst retaining a public stable TimeZoneVariant type.
pub(crate) mod ule {
    /// A time zone variant, such as Standard Time, or Daylight/Summer Time.
    ///
    /// This should not generally be constructed by client code. Instead, use
    /// * [`TimeZoneVariant::from_rearguard_isdst`]
    /// * [`TimeZoneInfo::infer_variant`](crate::TimeZoneInfo::infer_variant)
    #[derive(Copy, Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
    #[zerovec::make_ule(TimeZoneVariantULE)]
    #[repr(u8)]
    #[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
    #[cfg_attr(feature = "datagen", databake(path = icu_time))]
    #[cfg_attr(feature = "serde", derive(serde::Deserialize))]
    #[non_exhaustive]
    pub enum TimeZoneVariant {
        /// The variant corresponding to `"standard"` in CLDR.
        ///
        /// The semantics vary from time zone to time zone. The time zone display
        /// name of this variant may or may not be called "Standard Time".
        ///
        /// This is the variant with the lower UTC offset.
        Standard = 0,
        /// The variant corresponding to `"daylight"` in CLDR.
        ///
        /// The semantics vary from time zone to time zone. The time zone display
        /// name of this variant may or may not be called "Daylight Time".
        ///
        /// This is the variant with the higher UTC offset.
        Daylight = 1,
    }
}
pub use ule::TimeZoneVariant;

impl Deref for TimeZone {
    type Target = Subtag;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl AsULE for TimeZone {
    type ULE = Self;

    #[inline]
    fn to_unaligned(self) -> Self::ULE {
        self
    }

    #[inline]
    fn from_unaligned(unaligned: Self::ULE) -> Self {
        unaligned
    }
}

impl<'a> zerovec::maps::ZeroMapKV<'a> for TimeZone {
    type Container = ZeroVec<'a, TimeZone>;
    type Slice = ZeroSlice<TimeZone>;
    type GetType = TimeZone;
    type OwnedType = TimeZone;
}

/// A utility type that can hold time zone information.
///
/// **The primary definition of this type is in the [`icu_time`](https://docs.rs/icu_time) crate. Other ICU4X crates re-export it for convenience.**
///
/// See the docs on [`zone`](self) for more information.
///
/// # Examples
///
/// ```
/// use icu::calendar::Date;
/// use icu::locale::subtags::subtag;
/// use icu::time::zone::IanaParser;
/// use icu::time::zone::TimeZoneVariant;
/// use icu::time::DateTime;
/// use icu::time::Time;
/// use icu::time::TimeZone;
///
/// // Parse the IANA ID
/// let id = IanaParser::new().parse("America/Chicago");
///
/// // Alternatively, use the BCP47 ID directly
/// let id = TimeZone(subtag!("uschi"));
///
/// // Create a TimeZoneInfo<Base> by associating the ID with an offset
/// let time_zone = id.with_offset("-0600".parse().ok());
///
/// // Extend to a TimeZoneInfo<AtTime> by adding a local time
/// let time_zone_at_time = time_zone.at_date_time_iso(DateTime {
///     date: Date::try_new_iso(2023, 12, 2).unwrap(),
///     time: Time::start_of_day(),
/// });
///
/// // Extend to a TimeZoneInfo<Full> by adding a zone variant
/// let time_zone_with_variant =
///     time_zone_at_time.with_variant(TimeZoneVariant::Standard);
/// ```
#[derive(Debug, PartialEq, Eq)]
#[allow(clippy::exhaustive_structs)] // these four fields fully cover the needs of UTS 35
pub struct TimeZoneInfo<Model: models::TimeZoneModel> {
    id: TimeZone,
    offset: Option<UtcOffset>,
    zone_name_timestamp: Model::ZoneNameTimestamp,
    variant: Model::TimeZoneVariant,
}

impl<Model: models::TimeZoneModel> Clone for TimeZoneInfo<Model> {
    fn clone(&self) -> Self {
        *self
    }
}

impl<Model: models::TimeZoneModel> Copy for TimeZoneInfo<Model> {}

impl<Model: models::TimeZoneModel> TimeZoneInfo<Model> {
    /// The BCP47 time-zone identifier.
    pub fn id(self) -> TimeZone {
        self.id
    }

    /// The UTC offset, if known.
    ///
    /// This field is not enforced to be consistent with the time zone id.
    pub fn offset(self) -> Option<UtcOffset> {
        self.offset
    }
}

impl<Model> TimeZoneInfo<Model>
where
    Model: models::TimeZoneModel<ZoneNameTimestamp = ZoneNameTimestamp>,
{
    /// The time at which to interpret the time zone.
    pub fn zone_name_timestamp(self) -> ZoneNameTimestamp {
        self.zone_name_timestamp
    }
}

impl<Model> TimeZoneInfo<Model>
where
    Model: models::TimeZoneModel<TimeZoneVariant = TimeZoneVariant>,
{
    /// The time variant e.g. daylight or standard, if known.
    ///
    /// This field is not enforced to be consistent with the time zone id and offset.
    pub fn variant(self) -> TimeZoneVariant {
        self.variant
    }
}

impl TimeZone {
    /// Associates this [`TimeZone`] with a UTC offset, returning a [`TimeZoneInfo`].
    pub const fn with_offset(self, offset: Option<UtcOffset>) -> TimeZoneInfo<models::Base> {
        TimeZoneInfo {
            offset,
            id: self,
            zone_name_timestamp: (),
            variant: (),
        }
    }

    /// Converts this [`TimeZone`] into a [`TimeZoneInfo`] without an offset.
    pub const fn without_offset(self) -> TimeZoneInfo<models::Base> {
        TimeZoneInfo {
            offset: None,
            id: self,
            zone_name_timestamp: (),
            variant: (),
        }
    }
}

impl TimeZoneInfo<models::Base> {
    /// Creates a time zone info with no information.
    pub const fn unknown() -> Self {
        TimeZone::UNKNOWN.with_offset(None)
    }

    /// Creates a new [`TimeZoneInfo`] for the UTC time zone.
    pub const fn utc() -> Self {
        TimeZone(subtag!("utc")).with_offset(Some(UtcOffset::zero()))
    }

    /// Sets the [`ZoneNameTimestamp`] field.
    pub fn with_zone_name_timestamp(
        self,
        zone_name_timestamp: ZoneNameTimestamp,
    ) -> TimeZoneInfo<models::AtTime> {
        TimeZoneInfo {
            offset: self.offset,
            id: self.id,
            zone_name_timestamp,
            variant: (),
        }
    }

    /// Sets the [`ZoneNameTimestamp`] to the given local datetime.
    pub fn at_date_time_iso(self, date_time: DateTime<Iso>) -> TimeZoneInfo<models::AtTime> {
        Self::with_zone_name_timestamp(self, ZoneNameTimestamp::from_date_time_iso(date_time))
    }
}

impl TimeZoneInfo<models::AtTime> {
    /// Sets a [`TimeZoneVariant`] on this time zone.
    pub const fn with_variant(self, variant: TimeZoneVariant) -> TimeZoneInfo<models::Full> {
        TimeZoneInfo {
            offset: self.offset,
            id: self.id,
            zone_name_timestamp: self.zone_name_timestamp,
            variant,
        }
    }

    /// Sets the zone variant by calculating it using a [`VariantOffsetsCalculator`].
    ///
    /// If `offset()` is `None`, or if it doesn't match either of the
    /// timezone's standard or daylight offset around `local_time()`,
    /// the variant will be set to [`TimeZoneVariant::Standard`] and the time zone
    /// to [`TimeZone::UNKNOWN`].
    ///
    /// # Example
    /// ```
    /// use icu::calendar::Date;
    /// use icu::locale::subtags::subtag;
    /// use icu::time::zone::TimeZoneVariant;
    /// use icu::time::zone::VariantOffsetsCalculator;
    /// use icu::time::DateTime;
    /// use icu::time::Time;
    /// use icu::time::TimeZone;
    ///
    /// // Chicago at UTC-6
    /// let info = TimeZone(subtag!("uschi"))
    ///     .with_offset("-0600".parse().ok())
    ///     .at_date_time_iso(DateTime {
    ///         date: Date::try_new_iso(2023, 12, 2).unwrap(),
    ///         time: Time::start_of_day(),
    ///     })
    ///     .infer_variant(VariantOffsetsCalculator::new());
    ///
    /// assert_eq!(info.variant(), TimeZoneVariant::Standard);
    ///
    /// // Chicago at at UTC-5
    /// let info = TimeZone(subtag!("uschi"))
    ///     .with_offset("-0500".parse().ok())
    ///     .at_date_time_iso(DateTime {
    ///         date: Date::try_new_iso(2023, 6, 2).unwrap(),
    ///         time: Time::start_of_day(),
    ///     })
    ///     .infer_variant(VariantOffsetsCalculator::new());
    ///
    /// assert_eq!(info.variant(), TimeZoneVariant::Daylight);
    ///
    /// // Chicago at UTC-7
    /// let info = TimeZone(subtag!("uschi"))
    ///     .with_offset("-0700".parse().ok())
    ///     .at_date_time_iso(DateTime {
    ///         date: Date::try_new_iso(2023, 12, 2).unwrap(),
    ///         time: Time::start_of_day(),
    ///     })
    ///     .infer_variant(VariantOffsetsCalculator::new());
    ///
    /// // Whatever it is, it's not Chicago
    /// assert_eq!(info.id(), TimeZone::UNKNOWN);
    /// assert_eq!(info.variant(), TimeZoneVariant::Standard);
    /// ```
    pub fn infer_variant(
        self,
        calculator: VariantOffsetsCalculatorBorrowed,
    ) -> TimeZoneInfo<models::Full> {
        let Some(offset) = self.offset else {
            return TimeZone::UNKNOWN
                .with_offset(self.offset)
                .with_zone_name_timestamp(self.zone_name_timestamp)
                .with_variant(TimeZoneVariant::Standard);
        };
        let Some(variant) = calculator
            .compute_offsets_from_time_zone_and_name_timestamp(self.id, self.zone_name_timestamp)
            .and_then(|os| {
                if os.standard == offset {
                    Some(TimeZoneVariant::Standard)
                } else if os.daylight == Some(offset) {
                    Some(TimeZoneVariant::Daylight)
                } else {
                    None
                }
            })
        else {
            return TimeZone::UNKNOWN
                .with_offset(self.offset)
                .with_zone_name_timestamp(self.zone_name_timestamp)
                .with_variant(TimeZoneVariant::Standard);
        };
        self.with_variant(variant)
    }
}

impl TimeZoneVariant {
    /// Creates a zone variant from a TZDB `isdst` flag, if it is known that the TZDB was built with
    /// `DATAFORM=rearguard`.
    ///
    /// If it is known that the database was *not* built with `rearguard`, a caller can try to adjust
    /// for the differences. This is a moving target, for example the known differences for 2025a are:
    ///
    /// * `Europe/Dublin` since 1968-10-27
    /// * `Africa/Windhoek` between 1994-03-20 and 2017-10-24
    /// * `Africa/Casablanca` and `Africa/El_Aaiun` since 2018-10-28
    ///
    /// If the TZDB build mode is unknown or variable, use [`TimeZoneInfo::infer_variant`].
    pub const fn from_rearguard_isdst(isdst: bool) -> Self {
        if isdst {
            TimeZoneVariant::Daylight
        } else {
            TimeZoneVariant::Standard
        }
    }
}
