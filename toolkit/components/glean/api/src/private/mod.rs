// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! The different metric types supported by the Glean SDK to handle data.

use serde::{Deserialize, Serialize};

// Re-export of `glean` types we can re-use.
// That way a user only needs to depend on this crate, not on glean (and there can't be a
// version mismatch).
pub use glean::{
    traits, CommonMetricData, DistributionData, ErrorType, LabeledMetricData, Lifetime, MemoryUnit,
    RecordedEvent, TimeUnit, TimerId,
};

mod boolean;
mod counter;
mod custom_distribution;
mod datetime;
mod denominator;
mod event;
mod labeled;
mod labeled_boolean;
mod labeled_counter;
mod labeled_custom_distribution;
mod labeled_memory_distribution;
mod labeled_timing_distribution;
mod memory_distribution;
mod numerator;
mod object;
mod ping;
mod quantity;
mod rate;
pub(crate) mod string;
mod string_list;
mod text;
mod timespan;
mod timing_distribution;
mod url;
mod uuid;

pub use self::boolean::BooleanMetric;
pub use self::counter::CounterMetric;
pub use self::custom_distribution::{CustomDistributionMetric, LocalCustomDistribution};
pub use self::datetime::DatetimeMetric;
pub use self::denominator::DenominatorMetric;
pub use self::event::{EventMetric, EventRecordingError, ExtraKeys, NoExtraKeys};
pub use self::labeled::LabeledMetric;
pub use self::labeled_boolean::LabeledBooleanMetric;
pub use self::labeled_counter::LabeledCounterMetric;
pub use self::labeled_custom_distribution::LabeledCustomDistributionMetric;
pub use self::labeled_memory_distribution::LabeledMemoryDistributionMetric;
pub use self::labeled_timing_distribution::LabeledTimingDistributionMetric;
pub use self::memory_distribution::{LocalMemoryDistribution, MemoryDistributionMetric};
pub use self::numerator::NumeratorMetric;
pub use self::object::ObjectMetric;
pub use self::ping::Ping;
pub use self::quantity::QuantityMetric as LabeledQuantityMetric;
pub use self::quantity::QuantityMetric;
pub use self::rate::RateMetric;
pub use self::string::StringMetric;
pub use self::string::StringMetric as LabeledStringMetric;
pub use self::string_list::StringListMetric;
pub use self::text::TextMetric;
pub use self::timespan::TimespanMetric;
pub use self::timing_distribution::TimingDistributionMetric;
pub use self::url::UrlMetric;
pub use self::uuid::UuidMetric;

/// Uniquely identifies a single metric within its metric type.
#[derive(Debug, PartialEq, Eq, Hash, Copy, Clone, Deserialize, Serialize)]
#[repr(transparent)]
pub struct MetricId(pub(crate) u32);

impl MetricId {
    pub fn new(id: u32) -> Self {
        Self(id)
    }
}

impl From<u32> for MetricId {
    fn from(id: u32) -> Self {
        Self(id)
    }
}

// We only access the methods here when we're building with Gecko, as that's
// when we have access to the profiler. We don't need alternative (i.e.
// non-gecko) implementations, as any imports from this sub-module are also
// gated with the same #[cfg(feature...)]
#[cfg(feature = "with_gecko")]
pub(crate) mod profiler_utils {
    pub(crate) use super::truncate_string_for_marker;

    #[derive(Debug)]
    pub(crate) enum LookupError {
        NullPointer,
        Utf8ParseError(std::str::Utf8Error),
    }

    impl LookupError {
        pub fn as_str(self) -> &'static str {
            match self {
                LookupError::NullPointer => "id not found",
                LookupError::Utf8ParseError(_) => "utf8 parse error",
            }
        }
    }

    impl std::fmt::Display for LookupError {
        fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
            match self {
                LookupError::NullPointer => write!(f, "id not found"),
                LookupError::Utf8ParseError(p) => write!(f, "utf8 parse error: {}", p),
            }
        }
    }

    pub(crate) fn lookup_canonical_metric_name(
        id: &super::MetricId,
    ) -> Result<&'static str, LookupError> {
        #[allow(unused)]
        use std::ffi::{c_char, CStr};
        extern "C" {
            fn FOG_GetMetricIdentifier(id: u32) -> *const c_char;
        }
        // SAFETY: We check to make sure that the returned pointer is not null
        // before trying to construct a string from it. As the string array that
        // `FOG_GetMetricIdentifier` references is statically defined and allocated,
        // we know that any strings will be guaranteed to have a null terminator,
        // and will have the same lifetime as the program, meaning we're safe to
        // return a static lifetime, knowing that they won't be changed "underneath"
        // us. Additionally, we surface any errors from parsing the string as utf8.
        unsafe {
            let raw_name_ptr = FOG_GetMetricIdentifier(id.0);
            if raw_name_ptr.is_null() {
                Err(LookupError::NullPointer)
            } else {
                let name = CStr::from_ptr(raw_name_ptr).to_str();
                match name {
                    Ok(s) => Ok(s),
                    Err(ut8err) => Err(LookupError::Utf8ParseError(ut8err)),
                }
            }
        }
    }
}

// These two methods "live" within profiler_utils, but as we need them available
// testing, when we might not have gecko available, we use a different cfg
// set of features to enable them in both cases.
#[cfg(any(feature = "with_gecko", test))]
pub(crate) fn truncate_string_for_marker(input: String) -> String {
    const MAX_STRING_BYTE_LENGTH: usize = 1024;
    truncate_string_for_marker_to_length(input, MAX_STRING_BYTE_LENGTH)
}

#[cfg(any(feature = "with_gecko", test))]
#[inline]
fn truncate_string_for_marker_to_length(mut input: String, byte_length: usize) -> String {
    // Truncating an arbitrary string in Rust is not not exactly easy, as
    // Strings are UTF-8 encoded. The "built-in" String::truncate, however,
    // operates on bytes, and panics if the truncation crosses a character
    // boundary.
    // To avoid this, we need to find the first unicode char boundary that
    // is less than the size that we're looking for. Note that we're
    // interested in how many *bytes* the string takes up (when we add it
    // to a marker), so we truncate to `MAX_STRING_BYTE_LENGTH` bytes, or
    // (by walking the truncation point back) to a number of bytes that
    // still represents valid UTF-8.
    // Note, this truncation may not provide a valid json result, and
    // truncation acts on glyphs, not graphemes, so the resulting text
    // may not render exactly the same as before it was truncated.

    // Copied from src/core/num/mod.rs
    // Check if a given byte is a utf8 character boundary
    #[inline]
    const fn is_utf8_char_boundary(b: u8) -> bool {
        // This is bit magic equivalent to: b < 128 || b >= 192
        (b as i8) >= -0x40
    }

    // Check if our truncation point is a char boundary. If it isn't, move
    // it "back" along the string until it is.
    // Note, this is an almost direct port of the rust standard library
    // function `str::floor_char_boundary`. We re-produce it as this API is
    // not yet stable, and we make some small changes (such as modifying
    // the input in-place) that are more convenient for this method.
    if byte_length < input.len() {
        let lower_bound = byte_length.saturating_sub(3);

        let new_byte_length = input.as_bytes()[lower_bound..=byte_length]
            .iter()
            .rposition(|b| is_utf8_char_boundary(*b));

        // SAFETY: we know that the character boundary will be within four bytes
        let truncation_point = unsafe { lower_bound + new_byte_length.unwrap_unchecked() };
        input.truncate(truncation_point)
    }
    input
}

#[cfg(test)]
mod truncation_tests {
    use crate::private::truncate_string_for_marker;
    use crate::private::truncate_string_for_marker_to_length;

    // Testing is heavily inspired/copied from the existing tests for the
    // standard library function `floor_char_boundary`.
    // See: https://github.com/rust-lang/rust/blob/bca5fdebe0e539d123f33df5f2149d5976392e76/library/alloc/tests/str.rs#L2363

    // Check a series of truncation points (i.e. string lengths), and assert
    // that they all produce the same trunctated string from the input.
    fn check_many(s: &str, arg: impl IntoIterator<Item = usize>, truncated: &str) {
        for len in arg {
            assert_eq!(
                truncate_string_for_marker_to_length(s.to_string(), len),
                truncated,
                "truncate_string_for_marker_to_length({:?}, {:?}) != {:?}",
                len,
                s,
                truncated
            );
        }
    }

    #[test]
    fn truncate_1byte_chars() {
        check_many("jp", [0], "");
        check_many("jp", [1], "j");
        check_many("jp", 2..4, "jp");
    }

    #[test]
    fn truncate_2byte_chars() {
        check_many("ĵƥ", 0..2, "");
        check_many("ĵƥ", 2..4, "ĵ");
        check_many("ĵƥ", 4..6, "ĵƥ");
    }

    #[test]
    fn truncate_3byte_chars() {
        check_many("日本", 0..3, "");
        check_many("日本", 3..6, "日");
        check_many("日本", 6..8, "日本");
    }

    #[test]
    fn truncate_4byte_chars() {
        check_many("🇯🇵", 0..4, "");
        check_many("🇯🇵", 4..8, "🇯");
        check_many("🇯🇵", 8..10, "🇯🇵");
    }

    // Check a single string against it's expected truncated outcome
    fn check_one(s: String, truncated: String) {
        assert_eq!(
            truncate_string_for_marker(s.clone()),
            truncated,
            "truncate_string_for_marker({:?}) != {:?}",
            s,
            truncated
        );
    }

    #[test]
    fn full_truncation() {
        // Keep the values in this up to date with MAX_STRING_BYTE_LENGTH

        // For each of these tests, we use a padding value to get near to 1024
        // bytes, then add on a variety of further characters that push us up
        // to or over the limit. We then check that we correctly truncated to
        // the correct character or grapheme.
        let pad = |reps: usize| -> String { "-".repeat(reps) };

        // Note: len(jpjpj) = 5
        check_one(pad(1020) + "jpjpj", pad(1020) + "jpjp");

        // Note: len(ĵƥ) = 4
        check_one(pad(1020) + "ĵƥ", pad(1020) + "ĵƥ");
        check_one(pad(1021) + "ĵƥ", pad(1021) + "ĵ");

        // Note: len(日本) = 6
        check_one(pad(1018) + "日本", pad(1018) + "日本");
        check_one(pad(1020) + "日本", pad(1020) + "日");
        check_one(pad(1022) + "日本", pad(1022));

        // Note: len(🇯🇵) = 8, len(🇯) = 4
        check_one(pad(1016) + "🇯🇵", pad(1016) + "🇯🇵");
        check_one(pad(1017) + "🇯🇵", pad(1017) + "🇯");
        check_one(pad(1021) + "🇯🇵", pad(1021) + "");
    }
}
