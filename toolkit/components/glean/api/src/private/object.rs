// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use super::{CommonMetricData, MetricId};

use crate::ipc::need_ipc;

use glean::traits::ObjectSerialize;

#[cfg(feature = "with_gecko")]
use super::profiler_utils::{truncate_string_for_marker, TelemetryProfilerCategory};

#[cfg(feature = "with_gecko")]
#[derive(serde::Serialize, serde::Deserialize, Debug)]
struct ObjectMetricMarker {
    id: MetricId,
    value: String,
}

#[cfg(feature = "with_gecko")]
impl gecko_profiler::ProfilerMarker for ObjectMetricMarker {
    fn marker_type_name() -> &'static str {
        "ObjectMetric"
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
        schema.add_key_label_format("value", "Value", Format::String);
        schema
    }

    fn stream_json_marker_data(&self, json_writer: &mut gecko_profiler::JSONWriter) {
        let name = self.id.get_name();
        json_writer.unique_string_property("id", &name);
        json_writer.string_property("value", self.value.as_str());
    }
}

/// A dynamic object at runtime.
///
/// Does not do any schema validation
/// and just passes through the JSON string unmodified.
#[derive(Clone)]
pub struct RuntimeObject(serde_json::Value);

impl ObjectSerialize for RuntimeObject {
    fn from_str(obj: &str) -> Result<Self, glean::traits::ObjectError>
    where
        Self: Sized,
    {
        Ok(RuntimeObject(serde_json::Value::from_str(obj)?))
    }

    fn into_serialized_object(self) -> Result<serde_json::Value, glean::traits::ObjectError> {
        Ok(self.0)
    }
}

/// An object metric.
pub enum ObjectMetric<K> {
    Parent {
        /// The metric's ID. Used for testing and profiler markers. Object
        /// metrics canot be labeled, so we only store a MetricId. If this
        /// changes, this should be changed to a MetricGetter to distinguish
        /// between metrics and sub-metrics.
        id: MetricId,
        inner: glean::private::ObjectMetric<K>,
    },
    Child,
}

impl<K: ObjectSerialize + Clone> ObjectMetric<K> {
    /// Create a new object metric.
    pub fn new(id: MetricId, meta: CommonMetricData) -> Self {
        if need_ipc() {
            ObjectMetric::Child
        } else {
            let inner = glean::private::ObjectMetric::new(meta);
            ObjectMetric::Parent { id, inner }
        }
    }

    pub fn set(&self, value: K) {
        match self {
            #[allow(unused)]
            ObjectMetric::Parent { id, inner } => {
                #[cfg(feature = "with_gecko")]
                gecko_profiler::lazy_add_marker!(
                    "Object::set",
                    TelemetryProfilerCategory,
                    ObjectMetricMarker {
                        id: *id,
                        // It might be better to store the "raw"
                        // Result<Value, ObjectError> in the marker, as we
                        // are writing a lot of strings here. That would,
                        // however, require us to parameterise the marker
                        // type with `K`, the type parameter to
                        // ObjectMetric. This would be treated by
                        // rust's `typeid` as another concrete type,
                        // meaning that it would have a unique tag for
                        // (de)serialisation, and may quickly exhaust our
                        // (current) marker (de)serialisation tag limit.
                        value: truncate_string_for_marker(
                            value.clone().into_serialized_object().map_or_else(
                                |e| glean::traits::ObjectError::to_string(&e),
                                |v| serde_json::Value::to_string(&v),
                            ),
                        ),
                    }
                );
                inner.set(value);
            }
            ObjectMetric::Child => {
                log::error!("Unable to set object metric in non-main process. This operation will be ignored.");
                // TODO: Record an error.
            }
        };
    }

    pub fn set_string(&self, value: String) {
        match self {
            #[allow(unused)]
            ObjectMetric::Parent { id, inner } => {
                #[cfg(feature = "with_gecko")]
                gecko_profiler::lazy_add_marker!(
                    "Object::set",
                    TelemetryProfilerCategory,
                    ObjectMetricMarker {
                        id: *id,
                        value: truncate_string_for_marker(value.clone()),
                    }
                );
                inner.set_string(value);
            }
            ObjectMetric::Child => {
                log::error!("Unable to set object metric in non-main process. This operation will be ignored.");
                // TODO: Record an error.
            }
        };
    }

    pub fn test_get_value<'a, S: Into<Option<&'a str>>>(
        &self,
        ping_name: S,
    ) -> Option<serde_json::Value> {
        match self {
            ObjectMetric::Parent { inner, .. } => inner.test_get_value(ping_name),
            ObjectMetric::Child => {
                panic!("Cannot get test value for object metric in non-parent process!",)
            }
        }
    }

    pub fn test_get_value_as_str<'a, S: Into<Option<&'a str>>>(
        &self,
        ping_name: S,
    ) -> Option<String> {
        self.test_get_value(ping_name)
            .map(|val| serde_json::to_string(&val).unwrap())
    }

    pub fn test_get_num_recorded_errors(&self, error: glean::ErrorType) -> i32 {
        match self {
            ObjectMetric::Parent { inner, .. } => inner.test_get_num_recorded_errors(error),
            ObjectMetric::Child => {
                panic!("Cannot get the number of recorded errors in non-parent process!")
            }
        }
    }
}
