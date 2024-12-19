// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use inherent::inherent;

use super::{CommonMetricData, MetricId, TimeUnit};
use std::convert::TryInto;
use std::time::Duration;

use glean::traits::Timespan;

use crate::ipc::need_ipc;

#[cfg(feature = "with_gecko")]
use super::profiler_utils::{lookup_canonical_metric_name, LookupError, TelemetryProfilerCategory};

#[cfg(feature = "with_gecko")]
#[derive(serde::Serialize, serde::Deserialize, Debug)]
struct TimespanMetricMarker {
    id: MetricId,
    value: Option<u64>,
}

#[cfg(feature = "with_gecko")]
impl gecko_profiler::ProfilerMarker for TimespanMetricMarker {
    fn marker_type_name() -> &'static str {
        "TimespanMetric"
    }

    fn marker_type_display() -> gecko_profiler::MarkerSchema {
        use gecko_profiler::schema::*;
        let mut schema = MarkerSchema::new(&[Location::MarkerChart, Location::MarkerTable]);
        schema.set_tooltip_label("{marker.data.id} {marker.data.val}{marker.data.stringval}");
        schema.set_table_label(
            "{marker.name} - {marker.data.id}: {marker.data.val}{marker.data.stringval}",
        );
        schema.add_key_label_format_searchable(
            "id",
            "Metric",
            Format::UniqueString,
            Searchable::Searchable,
        );
        schema.add_key_label_format("val", "Value", Format::Integer);
        schema.add_key_label_format("stringval", "Value", Format::String);
        schema
    }

    fn stream_json_marker_data(&self, json_writer: &mut gecko_profiler::JSONWriter) {
        json_writer.unique_string_property(
            "id",
            lookup_canonical_metric_name(&self.id).unwrap_or_else(LookupError::as_str),
        );
        if let Some(v) = self.value {
            use std::convert::TryFrom;
            // We will always be writing values as nanoseconds, however,
            // the value may be too large for a signed 64-bit integer
            // (note that ns is given to us as an unsigned 64-bit int in
            // some locations)
            match i64::try_from(v) {
                // value was within 64-bit signed space
                Ok(v) => {
                    json_writer.int_property("val", v);
                }
                // value was too big - write it as a string
                Err(_) => {
                    let strv = format!("{}", v);
                    json_writer.string_property("stringval", strv.as_str());
                }
            };
        };
    }
}

/// A timespan metric.
///
/// Timespans are used to make a measurement of how much time is spent in a particular task.
pub enum TimespanMetric {
    Parent {
        id: MetricId,
        inner: glean::private::TimespanMetric,
        time_unit: TimeUnit,
    },
    Child,
}

impl TimespanMetric {
    /// Create a new timespan metric.
    pub fn new(id: MetricId, meta: CommonMetricData, time_unit: TimeUnit) -> Self {
        if need_ipc() {
            TimespanMetric::Child
        } else {
            TimespanMetric::Parent {
                id: id,
                inner: glean::private::TimespanMetric::new(meta, time_unit),
                time_unit: time_unit,
            }
        }
    }

    /// Only to be called from the MLA FFI.
    /// If you don't know what that is, don't call this.
    pub fn set_raw_unitless(&self, duration: u64) {
        match self {
            #[allow(unused)]
            TimespanMetric::Parent {
                id,
                inner,
                time_unit,
            } => {
                #[cfg(feature = "with_gecko")]
                if gecko_profiler::can_accept_markers() {
                    gecko_profiler::add_marker(
                        "TimeSpan::setRaw",
                        TelemetryProfilerCategory,
                        Default::default(),
                        TimespanMetricMarker {
                            id: *id,
                            value: Some(duration),
                        },
                    );
                }
                inner.set_raw(Duration::from_nanos(time_unit.as_nanos(duration)));
            }
            TimespanMetric::Child => {
                log::error!(
                    "Unable to set_raw_unitless on timespan in non-main process. This operation will be ignored."
                );
                // If we're in automation we can panic so the instrumentor knows they've gone wrong.
                // This is a deliberate violation of Glean's "metric APIs must not throw" design.
                assert!(!crate::ipc::is_in_automation(), "Attempted to set_raw_unitless on timespan metric in non-main process, which is forbidden. This panics in automation.");
                // TODO: Record an error. bug 1704504.
            }
        }
    }
}

#[inherent]
impl Timespan for TimespanMetric {
    pub fn start(&self) {
        match self {
            #[allow(unused)]
            TimespanMetric::Parent { id, inner, .. } => {
                // NOTE: we would like to record interval markers, either separate
                // markers with start/end, or a single marker with both start/end.
                // This is currently not possible, as the profiler incorrectly
                // matches separate start/end markers in the frontend, and we do
                // not have sufficient information to emit one marker when we stop
                // or cancel a timer.
                // This is being tracked in the following two bugs:
                // - Profiler, Bug 1929070,
                // - Glean, Bug 1931369,
                // While these bugs are being solved, we record instant markers so
                // that we still have *some* information.
                #[cfg(feature = "with_gecko")]
                if gecko_profiler::can_accept_markers() {
                    gecko_profiler::add_marker(
                        "TimeSpan::start",
                        TelemetryProfilerCategory,
                        gecko_profiler::MarkerOptions::default()
                            .with_timing(gecko_profiler::MarkerTiming::instant_now()),
                        TimespanMetricMarker {
                            id: *id,
                            value: None,
                        },
                    );
                };
                inner.start();
            }
            TimespanMetric::Child => {
                log::error!("Unable to start timespan metric in non-main process. This operation will be ignored.");
                // If we're in automation we can panic so the instrumentor knows they've gone wrong.
                // This is a deliberate violation of Glean's "metric APIs must not throw" design.assert!(!crate::ipc::is_in_automation());
                assert!(!crate::ipc::is_in_automation(), "Attempted to start timespan metric in non-main process, which is forbidden. This panics in automation.");
                // TODO: Record an error. bug 1704504.
            }
        }
    }

    pub fn stop(&self) {
        match self {
            #[allow(unused)]
            TimespanMetric::Parent { id, inner, .. } => {
                // See comment on Timespan::start
                #[cfg(feature = "with_gecko")]
                if gecko_profiler::can_accept_markers() {
                    gecko_profiler::add_marker(
                        "TimeSpan::stop",
                        TelemetryProfilerCategory,
                        gecko_profiler::MarkerOptions::default()
                            .with_timing(gecko_profiler::MarkerTiming::instant_now()),
                        TimespanMetricMarker {
                            id: *id,
                            value: None,
                        },
                    );
                };
                inner.stop();
            }
            TimespanMetric::Child => {
                log::error!("Unable to stop timespan metric in non-main process. This operation will be ignored.");
                // If we're in automation we can panic so the instrumentor knows they've gone wrong.
                // This is a deliberate violation of Glean's "metric APIs must not throw" design.assert!(!crate::ipc::is_in_automation());
                assert!(!crate::ipc::is_in_automation(), "Attempted to stop timespan metric in non-main process, which is forbidden. This panics in automation.");
                // TODO: Record an error. bug 1704504.
            }
        }
    }

    pub fn cancel(&self) {
        match self {
            #[allow(unused)]
            TimespanMetric::Parent { id, inner, .. } => {
                // See comment on Timespan::start
                #[cfg(feature = "with_gecko")]
                if gecko_profiler::can_accept_markers() {
                    gecko_profiler::add_marker(
                        "TimeSpan::cancel",
                        TelemetryProfilerCategory,
                        gecko_profiler::MarkerOptions::default()
                            .with_timing(gecko_profiler::MarkerTiming::instant_now()),
                        TimespanMetricMarker {
                            id: *id,
                            value: None,
                        },
                    );
                };
                inner.cancel();
            }
            TimespanMetric::Child => {
                log::error!("Unable to cancel timespan metric in non-main process. This operation will be ignored.");
                // If we're in automation we can panic so the instrumentor knows they've gone wrong.
                // This is a deliberate violation of Glean's "metric APIs must not throw" design.assert!(!crate::ipc::is_in_automation());
                assert!(!crate::ipc::is_in_automation(), "Attempted to cancel timespan metric in non-main process, which is forbidden. This panics in automation.");
                // TODO: Record an error. bug 1704504.
            }
        }
    }

    pub fn set_raw(&self, elapsed: Duration) {
        match self {
            #[allow(unused)]
            TimespanMetric::Parent { id, inner, .. } => {
                let elapsed = elapsed.as_nanos().try_into().unwrap_or(i64::MAX);
                #[cfg(feature = "with_gecko")]
                if gecko_profiler::can_accept_markers() {
                    gecko_profiler::add_marker(
                        "TimeSpan::setRaw",
                        TelemetryProfilerCategory,
                        Default::default(),
                        TimespanMetricMarker {
                            id: *id,
                            // This up-cast is safe, as we know that "elapsed"
                            // is always a positive integer, as as_nanos
                            // returns u128
                            value: Some(elapsed as u64),
                        },
                    );
                };
                inner.set_raw_nanos(elapsed)
            }
            TimespanMetric::Child => {
                log::error!("Unable to set_raw on timespan in non-main process. This operation will be ignored.");
                // If we're in automation we can panic so the instrumentor knows they've gone wrong.
                // This is a deliberate violation of Glean's "metric APIs must not throw" design.assert!(!crate::ipc::is_in_automation());
                assert!(!crate::ipc::is_in_automation(), "Attempted to set_raw on timespan metric in non-main process, which is forbidden. This panics in automation.");
                // TODO: Record an error. bug 1704504.
            }
        }
    }

    pub fn test_get_value<'a, S: Into<Option<&'a str>>>(&self, ping_name: S) -> Option<u64> {
        let ping_name = ping_name.into().map(|s| s.to_string());
        match self {
            // Conversion is ok here:
            // Timespans are really tricky to set to excessive values with the pleasant APIs.
            TimespanMetric::Parent { inner, .. } => {
                inner.test_get_value(ping_name).map(|i| i as u64)
            }
            TimespanMetric::Child => {
                panic!("Cannot get test value for in non-main process!");
            }
        }
    }

    pub fn test_get_num_recorded_errors(&self, error: glean::ErrorType) -> i32 {
        match self {
            TimespanMetric::Parent { inner, .. } => inner.test_get_num_recorded_errors(error),
            TimespanMetric::Child => {
                panic!("Cannot get the number of recorded errors for timespan metric in non-main process!");
            }
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::{common_test::*, ipc, metrics};

    #[test]
    fn smoke_test_timespan() {
        let _lock = lock_test();

        let metric = TimespanMetric::new(
            0.into(),
            CommonMetricData {
                name: "timespan_metric".into(),
                category: "telemetry".into(),
                send_in_pings: vec!["test-ping".into()],
                disabled: false,
                ..Default::default()
            },
            TimeUnit::Nanosecond,
        );

        metric.start();
        // Stopping right away might not give us data, if the underlying clock source is not precise
        // enough.
        // So let's cancel and make sure nothing blows up.
        metric.cancel();

        assert_eq!(None, metric.test_get_value("test-ping"));
    }

    #[test]
    fn timespan_ipc() {
        let _lock = lock_test();
        let _raii = ipc::test_set_need_ipc(true);

        let child_metric = &metrics::test_only::can_we_time_it;

        // Instrumentation calls do not panic.
        child_metric.start();
        // Stopping right away might not give us data,
        // if the underlying clock source is not precise enough.
        // So let's cancel and make sure nothing blows up.
        child_metric.cancel();

        // (They also shouldn't do anything,
        // but that's not something we can inspect in this test)
    }
}
