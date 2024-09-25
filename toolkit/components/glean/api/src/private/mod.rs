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
pub use self::boolean::BooleanMetric as LabeledBooleanMetric;
pub use self::counter::CounterMetric;
pub use self::custom_distribution::{CustomDistributionMetric, LocalCustomDistribution};
pub use self::datetime::DatetimeMetric;
pub use self::denominator::DenominatorMetric;
pub use self::event::{EventMetric, EventRecordingError, ExtraKeys, NoExtraKeys};
pub use self::labeled::LabeledMetric;
pub use self::labeled_counter::LabeledCounterMetric;
pub use self::labeled_custom_distribution::LabeledCustomDistributionMetric;
pub use self::labeled_memory_distribution::LabeledMemoryDistributionMetric;
pub use self::labeled_timing_distribution::LabeledTimingDistributionMetric;
pub use self::memory_distribution::{LocalMemoryDistribution, MemoryDistributionMetric};
pub use self::numerator::NumeratorMetric;
pub use self::object::ObjectMetric;
pub use self::ping::Ping;
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
