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
    use super::max_string_byte_length;
    pub(crate) use super::truncate_string_for_marker;

    // Declare the telemetry profiling category as a constant here.
    // This lets us avoid re-importing gecko_profiler ... within metric files,
    // which keeps the importing a bit cleaner, and reduces profiler intrusion.
    #[allow(non_upper_case_globals)]
    pub const TelemetryProfilerCategory: gecko_profiler::ProfilingCategoryPair =
        gecko_profiler::ProfilingCategoryPair::Telemetry(None);

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

    // Get the datetime *now*
    // From https://searchfox.org/mozilla-central/source/third_party/rust/glean-core/src/util.rs#51
    // This should be removed when Bug 1925313 is fixed.
    /// Get the current date & time with a fixed-offset timezone.
    ///
    /// This converts from the `Local` timezone into its fixed-offset equivalent.
    /// If a timezone outside of [-24h, +24h] is detected it corrects the timezone offset to UTC (+0).
    pub(crate) fn local_now_with_offset() -> chrono::DateTime<chrono::FixedOffset> {
        use chrono::{DateTime, Local};
        #[cfg(target_os = "windows")]
        {
            // `Local::now` takes the user's timezone offset
            // and panics if it's not within a range of [-24, +24] hours.
            // This causes crashes in a small number of clients on Windows.
            //
            // We can't determine the faulty clients
            // or the circumstancens under which this happens,
            // so the best we can do is have a workaround:
            //
            // We try getting the time and timezone first,
            // then manually check that it is a valid timezone offset.
            // If it is, we proceed and use that time and offset.
            // If it isn't we fallback to UTC.
            //
            // This has the small downside that it will use 2 calls to get the time,
            // but only on Windows.
            //
            // See https://bugzilla.mozilla.org/show_bug.cgi?id=1611770.

            use chrono::{FixedOffset, Utc};

            // Get timespec, including the user's timezone.
            let tm = time::now();
            // Same as chrono:
            // https://docs.rs/chrono/0.4.10/src/chrono/offset/local.rs.html#37
            let offset = tm.tm_utcoff;
            if let None = FixedOffset::east_opt(offset) {
                log::warn!(
                    "Detected invalid timezone offset: {}. Using UTC fallback.",
                    offset
                );
                let now: DateTime<Utc> = Utc::now();
                let utc_offset = FixedOffset::east(0);
                return now.with_timezone(&utc_offset);
            }
        }

        let now: DateTime<Local> = Local::now();
        now.with_timezone(now.offset())
    }

    /// Try to convert a glean::Datetime into a chrono::DateTime Returns none if
    /// the glean::Datetime offset is not a valid timezone We would prefer to
    /// use .into or similar, but we need to wait until this is implemented in
    /// the Glean SDK. See Bug 1925313 for more details.
    pub(crate) fn glean_to_chrono_datetime(
        gdt: &glean::Datetime,
    ) -> Option<chrono::LocalResult<chrono::DateTime<chrono::FixedOffset>>> {
        use chrono::{FixedOffset, TimeZone};
        let tz = FixedOffset::east_opt(gdt.offset_seconds);
        if tz.is_none() {
            return None;
        }

        Some(
            FixedOffset::east(gdt.offset_seconds)
                .ymd_opt(gdt.year, gdt.month, gdt.day)
                .and_hms_nano_opt(gdt.hour, gdt.minute, gdt.second, gdt.nanosecond),
        )
    }

    // Truncate a vector down to a maximum size.
    // We want to avoid storing large vectors of values in the profiler buffer,
    // so this helper method allows markers to explicitly limit the size of
    // vectors of values that might originate from Glean
    pub(crate) fn truncate_vector_for_marker<T>(vec: &Vec<T>) -> Vec<T>
    where
        T: Clone,
    {
        const MAX_VECTOR_LENGTH: usize = 1024;
        if vec.len() > MAX_VECTOR_LENGTH {
            vec[0..MAX_VECTOR_LENGTH - 1].to_vec()
        } else {
            vec.clone()
        }
    }

    // Generic marker structs:

    #[derive(serde::Serialize, serde::Deserialize, Debug)]
    pub(crate) struct StringLikeMetricMarker {
        id: super::MetricId,
        val: String,
    }

    impl StringLikeMetricMarker {
        pub fn new(id: super::MetricId, val: &String) -> StringLikeMetricMarker {
            StringLikeMetricMarker {
                id: id,
                val: truncate_string_for_marker(val.clone()),
            }
        }

        pub fn new_owned(id: super::MetricId, val: String) -> StringLikeMetricMarker {
            StringLikeMetricMarker {
                id: id,
                val: truncate_string_for_marker(val),
            }
        }
    }

    impl gecko_profiler::ProfilerMarker for StringLikeMetricMarker {
        fn marker_type_name() -> &'static str {
            "StringLikeMetric"
        }

        fn marker_type_display() -> gecko_profiler::MarkerSchema {
            use gecko_profiler::schema::*;
            let mut schema = MarkerSchema::new(&[Location::MarkerChart, Location::MarkerTable]);
            schema.set_tooltip_label("{marker.data.id}");
            schema.set_table_label("{marker.name} - {marker.data.id}: {marker.data.value}");
            schema.add_key_label_format_searchable(
                "id",
                "Metric",
                Format::UniqueString,
                Searchable::Searchable,
            );
            schema.add_key_label_format("val", "Value", Format::String);
            schema
        }

        fn stream_json_marker_data(&self, json_writer: &mut gecko_profiler::JSONWriter) {
            json_writer.unique_string_property(
                "id",
                lookup_canonical_metric_name(&self.id).unwrap_or_else(LookupError::as_str),
            );

            debug_assert!(self.val.len() <= max_string_byte_length());
            json_writer.string_property("val", self.val.as_str());
        }
    }

    #[derive(serde::Serialize, serde::Deserialize, Debug)]
    pub(crate) struct IntLikeMetricMarker<T>
    where
        T: Into<i64>,
    {
        id: super::MetricId,
        label: Option<String>,
        val: T,
    }

    impl<T> IntLikeMetricMarker<T>
    where
        T: Into<i64>,
    {
        pub fn new(id: super::MetricId, label: Option<String>, val: T) -> IntLikeMetricMarker<T> {
            IntLikeMetricMarker { id, label, val }
        }
    }

    impl<T> gecko_profiler::ProfilerMarker for IntLikeMetricMarker<T>
    where
        T: serde::Serialize + serde::de::DeserializeOwned + Into<i64> + Copy,
    {
        fn marker_type_name() -> &'static str {
            "IntLikeMetric"
        }

        fn marker_type_display() -> gecko_profiler::MarkerSchema {
            use gecko_profiler::schema::*;
            let mut schema = MarkerSchema::new(&[Location::MarkerChart, Location::MarkerTable]);
            schema.set_tooltip_label("{marker.data.id} {marker.data.label} {marker.data.val}");
            schema.set_table_label(
                "{marker.name} - {marker.data.id} {marker.data.label}: {marker.data.val}",
            );
            schema.add_key_label_format_searchable(
                "id",
                "Metric",
                Format::UniqueString,
                Searchable::Searchable,
            );
            schema.add_key_label_format("val", "Value", Format::Integer);
            schema.add_key_label_format_searchable(
                "label",
                "Label",
                Format::UniqueString,
                Searchable::Searchable,
            );
            schema
        }

        fn stream_json_marker_data(&self, json_writer: &mut gecko_profiler::JSONWriter) {
            json_writer.unique_string_property(
                "id",
                lookup_canonical_metric_name(&self.id).unwrap_or_else(LookupError::as_str),
            );
            json_writer.int_property("val", self.val.clone().into());
            if let Some(l) = &self.label {
                json_writer.unique_string_property("label", &l);
            };
        }
    }

    // This might seem like overkill for discerning between a single element and
    // a vector of elements. However, from the perspective of the profiler buffer
    // this is quite reasonable, as it has a lower memory overhead. Doing the maths
    // (and assuming a 64-bit system, so usize = 8 bytes):
    // Enum: i64 value (8-bytes), enum discernment byte = 9 bytes,
    // Vector: i64 values (at least 8 bytes), usize length, usize capacity, data
    //     pointer = 32 bytes
    #[derive(serde::Serialize, serde::Deserialize, Debug)]
    pub(crate) enum DistributionValues<T> {
        Sample(T),
        Samples(Vec<T>),
    }

    #[derive(serde::Serialize, serde::Deserialize, Debug)]
    pub(crate) struct DistributionMetricMarker<T> {
        id: super::MetricId,
        label: Option<String>,
        value: DistributionValues<T>,
    }

    impl<T> DistributionMetricMarker<T> {
        pub fn new(
            id: super::MetricId,
            label: Option<String>,
            value: DistributionValues<T>,
        ) -> DistributionMetricMarker<T> {
            DistributionMetricMarker { id, label, value }
        }
    }

    impl<T> gecko_profiler::ProfilerMarker for DistributionMetricMarker<T>
    where
        T: serde::Serialize + serde::de::DeserializeOwned + Copy + std::fmt::Display,
    {
        fn marker_type_name() -> &'static str {
            "DistMetric"
        }

        fn marker_type_display() -> gecko_profiler::MarkerSchema {
            use gecko_profiler::schema::*;
            let mut schema = MarkerSchema::new(&[Location::MarkerChart, Location::MarkerTable]);
            schema.set_tooltip_label("{marker.data.id} {marker.data.label} {marker.data.sample}");
            schema.set_table_label(
                "{marker.name} - {marker.data.id} {marker.data.label}: {marker.data.sample}{marker.data.samples}",
            );
            schema.set_chart_label("{marker.data.id}");
            schema.add_key_label_format_searchable(
                "id",
                "Metric",
                Format::UniqueString,
                Searchable::Searchable,
            );
            schema.add_key_label_format_searchable(
                "label",
                "Label",
                Format::UniqueString,
                Searchable::Searchable,
            );
            schema.add_key_label_format("sample", "Sample", Format::String);
            schema.add_key_label_format("samples", "Samples", Format::String);
            schema
        }

        fn stream_json_marker_data(&self, json_writer: &mut gecko_profiler::JSONWriter) {
            json_writer.unique_string_property(
                "id",
                lookup_canonical_metric_name(&self.id).unwrap_or_else(LookupError::as_str),
            );

            if let Some(l) = &self.label {
                json_writer.unique_string_property("label", l.as_str());
            };

            match &self.value {
                DistributionValues::Sample(s) => {
                    let s = format!("{}", s);
                    json_writer.string_property("sample", s.as_str());
                }
                DistributionValues::Samples(s) => {
                    let s = format!(
                        "[{}]",
                        s.iter()
                            .map(|v| v.to_string())
                            .collect::<Vec<_>>()
                            .join(",")
                    );
                    json_writer.string_property("samples", s.as_str());
                }
            };
        }
    }
}

// These two methods, and the constant function, "live" within profiler_utils,
// but as we need them available for testing, when we might not have gecko
// available, we use a different set of cfg features to enable them in both
// cases. Note that we re-export the main truncation method within
// `profiler_utils` to correct the namespace.
#[cfg(any(feature = "with_gecko", test))]
pub(crate) fn truncate_string_for_marker(input: String) -> String {
    truncate_string_for_marker_to_length(input, max_string_byte_length())
}

#[cfg(any(feature = "with_gecko", test))]
const fn max_string_byte_length() -> usize {
    1024
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
