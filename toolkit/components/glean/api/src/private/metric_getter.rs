/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use serde::{Deserialize, Serialize};

/// Uniquely identify a metric so that we can look up names, labels (etc) and
/// perform IPC
#[derive(Debug, PartialEq, Eq, Hash, Copy, Clone, Deserialize, Serialize)]
pub enum MetricId {
    Id(BaseMetricId),
    SubId(SubMetricId),
}

impl MetricId {
    /// Extract the underlying metric_id, if there is one.
    pub fn base_metric_id(self) -> Option<BaseMetricId> {
        match self {
            MetricId::Id(base_metric_id) => Some(base_metric_id),
            _ => None,
        }
    }

    pub fn is_dynamic(self) -> bool {
        *self & (1 << crate::factory::DYNAMIC_METRIC_BIT) > 0
    }
}

#[cfg(feature = "with_gecko")]
impl MetricId {
    /// Given a metric getter, retrieve the name and (optionally) label of the
    /// underlying metric. Note, this currently returns the name of the
    /// metric in the so-called "JavaScript conjugation", while labels are
    /// returned in the so-called "YAML conjugation". Bug 1938145 captures
    /// the work for looking up the actual metric, after which Bug 1934880
    /// will allow us to get both in the yaml conjugation.
    pub fn get_identifiers(&self) -> (String, Option<String>) {
        match self {
            MetricId::Id(id) => (id.get_name(), None),
            MetricId::SubId(sid) => match sid.lookup_metric_id_and_label() {
                Some((id, label)) => (id.get_name(), Some(label)),
                None => (String::from("Could not find submetric in maps"), None),
            },
        }
    }
}

impl From<BaseMetricId> for MetricId {
    fn from(base_metric_id: BaseMetricId) -> MetricId {
        MetricId::Id(base_metric_id)
    }
}

impl From<SubMetricId> for MetricId {
    fn from(submetric_id: SubMetricId) -> MetricId {
        MetricId::SubId(submetric_id)
    }
}

impl std::ops::Deref for MetricId {
    type Target = u32;

    fn deref(&self) -> &Self::Target {
        match self {
            MetricId::Id(BaseMetricId(m)) => m,
            MetricId::SubId(SubMetricId(m)) => m,
        }
    }
}

/// Uniquely identifies a single metric across all metric types.
#[derive(Debug, PartialEq, Eq, Hash, Copy, Clone, Deserialize, Serialize)]
#[repr(transparent)]
pub struct BaseMetricId(pub(crate) u32);

impl BaseMetricId {
    pub fn new(id: u32) -> Self {
        Self(id)
    }

    pub fn is_dynamic(self) -> bool {
        self.0 & (1 << crate::factory::DYNAMIC_METRIC_BIT) > 0
    }
}

#[cfg(feature = "with_gecko")]
impl BaseMetricId {
    // Wraps the result of `lookup_canonical_metric_name` so that it's
    // slightly easier for consumers to use. Also provides a slightly more
    // abstracted interface, so that in future we can use other ways to get
    // the name of a metric.
    pub(crate) fn get_name(&self) -> String {
        String::from(self.lookup_canonical_metric_name())
    }
    pub(crate) fn lookup_canonical_metric_name(&self) -> &'static str {
        #[allow(unused)]
        use std::ffi::{c_char, CStr};
        extern "C" {
            fn FOG_GetMetricIdentifier(id: u32) -> *const c_char;
        }
        // SAFETY: We check to make sure that the returned pointer is not null
        // before trying to construct a string from it. As the string array
        // that `FOG_GetMetricIdentifier` references is statically defined
        // and allocated, we know that any strings will be guaranteed to have
        // a null terminator, and will have the same lifetime as the program,
        // meaning we're safe to return a static lifetime, knowing that they
        // won't be changed "underneath" us. Additionally, we surface any
        // errors from parsing the string as utf8.
        unsafe {
            let raw_name_ptr = FOG_GetMetricIdentifier(**self);
            if raw_name_ptr.is_null() {
                "id not found"
            } else {
                match CStr::from_ptr(raw_name_ptr).to_str() {
                    Ok(s) => s,
                    Err(_) => "utf8 parse error",
                }
            }
        }
    }
}

impl From<u32> for BaseMetricId {
    fn from(id: u32) -> Self {
        Self(id)
    }
}

impl std::ops::Deref for BaseMetricId {
    type Target = u32;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

/// Uniquely identifies a sub-metric across all metric types.
#[derive(Debug, PartialEq, Eq, Hash, Copy, Clone, Deserialize, Serialize)]
#[repr(transparent)]
pub struct SubMetricId(pub(crate) u32);

impl SubMetricId {
    pub fn new(id: u32) -> Self {
        Self(id)
    }

    pub fn is_dynamic(self) -> bool {
        self.0 & (1 << crate::factory::DYNAMIC_METRIC_BIT) > 0
    }
}

#[cfg(feature = "with_gecko")]
impl SubMetricId {
    /// Given a submetric id, use the glean submetric maps to look up the
    /// underlying metric id, and label. Note that this essentially performs
    /// the reverse of `private::submetric_id_for`.
    pub(crate) fn lookup_metric_id_and_label(&self) -> Option<(BaseMetricId, String)> {
        let map = crate::metrics::__glean_metric_maps::submetric_maps::LABELED_METRICS_TO_IDS
            .read()
            .expect("read lock of submetric ids was poisoned");
        map.iter()
            .find(|(_, &value)| value == *self)
            .map(|(key, _)| key.clone())
    }
}

impl From<u32> for SubMetricId {
    fn from(id: u32) -> Self {
        Self(id)
    }
}

impl std::ops::Deref for SubMetricId {
    type Target = u32;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}
