// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use inherent::inherent;
use std::sync::Arc;

use super::{CommonMetricData, MetricId};
use glean::{DistributionData, ErrorType, HistogramType};

use crate::ipc::{need_ipc, with_ipc_payload};
use glean::traits::CustomDistribution;

#[cfg(feature = "with_gecko")]
use super::profiler_utils::{lookup_canonical_metric_name, LookupError};

#[cfg(feature = "with_gecko")]
use gecko_profiler::gecko_profiler_category;

#[cfg(feature = "with_gecko")]
#[derive(serde::Serialize, serde::Deserialize, Debug)]
struct CustomDistributionMetricMarker {
    id: MetricId,
    samples: Vec<i64>,
}

#[cfg(feature = "with_gecko")]
impl gecko_profiler::ProfilerMarker for CustomDistributionMetricMarker {
    fn marker_type_name() -> &'static str {
        "CustomDistribution"
    }

    fn marker_type_display() -> gecko_profiler::MarkerSchema {
        use gecko_profiler::schema::*;
        let mut schema = MarkerSchema::new(&[Location::MarkerChart, Location::MarkerTable]);
        schema.set_tooltip_label("{marker.data.id} {marker.data.samples}");
        schema.set_table_label("{marker.name} - {marker.data.id}: {marker.data.samples}");
        schema.add_key_label_format_searchable(
            "id",
            "Metric",
            Format::String,
            Searchable::Searchable,
        );
        schema.add_key_label_format("samples", "Samples", Format::String);
        schema
    }

    fn stream_json_marker_data(&self, json_writer: &mut gecko_profiler::JSONWriter) {
        json_writer.string_property(
            "id",
            lookup_canonical_metric_name(&self.id).unwrap_or_else(LookupError::as_str),
        );
        let vstr = if self.samples.len() == 1 {
            format!("{}", self.samples[0])
        } else {
            format!(
                "[{}]",
                self.samples
                    .iter()
                    .map(|x| x.to_string())
                    .collect::<Vec<_>>()
                    .join(", ")
            )
        };
        json_writer.string_property("samples", vstr.as_str());
    }
}

/// A custom distribution metric.
///
/// Custom distributions are used to record the distribution of arbitrary values.
#[derive(Clone)]
pub enum CustomDistributionMetric {
    Parent {
        /// The metric's ID.
        ///
        /// **TEST-ONLY** - Do not use unless gated with `#[cfg(test)]`.
        id: MetricId,
        inner: Arc<glean::private::CustomDistributionMetric>,
    },
    Child(CustomDistributionMetricIpc),
}
#[derive(Debug, Clone)]
pub struct CustomDistributionMetricIpc(pub MetricId);

impl CustomDistributionMetric {
    /// Create a new custom distribution metric.
    pub fn new(
        id: MetricId,
        meta: CommonMetricData,
        range_min: i64,
        range_max: i64,
        bucket_count: i64,
        histogram_type: HistogramType,
    ) -> Self {
        if need_ipc() {
            CustomDistributionMetric::Child(CustomDistributionMetricIpc(id))
        } else {
            let inner = glean::private::CustomDistributionMetric::new(
                meta,
                range_min,
                range_max,
                bucket_count,
                histogram_type,
            );
            CustomDistributionMetric::Parent {
                id,
                inner: Arc::new(inner),
            }
        }
    }

    #[cfg(test)]
    pub(crate) fn metric_id(&self) -> MetricId {
        match self {
            CustomDistributionMetric::Parent { id, .. } => *id,
            CustomDistributionMetric::Child(c) => c.0,
        }
    }

    #[cfg(test)]
    pub(crate) fn child_metric(&self) -> Self {
        match self {
            CustomDistributionMetric::Parent { id, .. } => {
                CustomDistributionMetric::Child(CustomDistributionMetricIpc(*id))
            }
            CustomDistributionMetric::Child(_) => {
                panic!("Can't get a child metric from a child metric")
            }
        }
    }

    pub fn start_buffer(&self) -> LocalCustomDistribution<'_> {
        match self {
            CustomDistributionMetric::Parent { inner, .. } => {
                LocalCustomDistribution::Parent(inner.start_buffer())
            }
            CustomDistributionMetric::Child(_) => {
                // TODO(bug 1920957): Buffering not implemented for child processes yet. We don't
                // want to panic though.
                log::warn!("Can't get a local custom distribution from a child metric. No data will be recorded.");
                LocalCustomDistribution::Child
            }
        }
    }
}

pub enum LocalCustomDistribution<'a> {
    Parent(glean::private::LocalCustomDistribution<'a>),
    Child,
}

impl LocalCustomDistribution<'_> {
    pub fn accumulate(&mut self, sample: u64) {
        match self {
            LocalCustomDistribution::Parent(p) => p.accumulate(sample),
            LocalCustomDistribution::Child => {
                log::debug!("Can't accumulate local custom distribution in a child process.")
            }
        }
    }
}

#[inherent]
impl CustomDistribution for CustomDistributionMetric {
    pub fn accumulate_samples_signed(&self, samples: Vec<i64>) {
        match self {
            // We need to allow unused for `id`, as it's
            // only used when we compile with gecko
            #[allow(unused_variables)]
            CustomDistributionMetric::Parent { id, inner } => {
                #[cfg(feature = "with_gecko")]
                if gecko_profiler::can_accept_markers() {
                    gecko_profiler::add_marker(
                        "CustomDistribution::accumulate",
                        gecko_profiler_category!(Telemetry),
                        Default::default(),
                        CustomDistributionMetricMarker {
                            id: *id,
                            samples: samples.clone(),
                        },
                    );
                }
                inner.accumulate_samples(samples)
            }
            CustomDistributionMetric::Child(c) => {
                #[cfg(feature = "with_gecko")]
                if gecko_profiler::can_accept_markers() {
                    gecko_profiler::add_marker(
                        "CustomDistribution::accumulate",
                        gecko_profiler_category!(Telemetry),
                        Default::default(),
                        CustomDistributionMetricMarker {
                            id: c.0,
                            samples: samples.clone(),
                        },
                    );
                }
                with_ipc_payload(move |payload| {
                    if let Some(v) = payload.custom_samples.get_mut(&c.0) {
                        v.extend(samples);
                    } else {
                        payload.custom_samples.insert(c.0, samples);
                    }
                });
            }
        }
    }

    pub fn accumulate_single_sample_signed(&self, sample: i64) {
        match self {
            // We need to allow unused for `id`, as it's
            // only used when we compile with gecko
            #[allow(unused_variables)]
            CustomDistributionMetric::Parent { id, inner } => {
                #[cfg(feature = "with_gecko")]
                if gecko_profiler::can_accept_markers() {
                    gecko_profiler::add_marker(
                        "CustomDistribution::accumulate",
                        gecko_profiler_category!(Telemetry),
                        Default::default(),
                        CustomDistributionMetricMarker {
                            id: *id,
                            samples: vec![sample],
                        },
                    );
                }
                inner.accumulate_single_sample(sample);
            }
            CustomDistributionMetric::Child(c) => {
                #[cfg(feature = "with_gecko")]
                if gecko_profiler::can_accept_markers() {
                    gecko_profiler::add_marker(
                        "CustomDistribution::accumulate",
                        gecko_profiler_category!(Telemetry),
                        Default::default(),
                        CustomDistributionMetricMarker {
                            id: c.0,
                            samples: vec![sample],
                        },
                    );
                }
                with_ipc_payload(move |payload| {
                    if let Some(v) = payload.custom_samples.get_mut(&c.0) {
                        v.push(sample);
                    } else {
                        payload.custom_samples.insert(c.0, vec![sample]);
                    }
                });
            }
        }
    }

    pub fn test_get_value<'a, S: Into<Option<&'a str>>>(
        &self,
        ping_name: S,
    ) -> Option<DistributionData> {
        let ping_name = ping_name.into().map(|s| s.to_string());
        match self {
            CustomDistributionMetric::Parent { inner, .. } => inner.test_get_value(ping_name),
            CustomDistributionMetric::Child(c) => {
                panic!("Cannot get test value for {:?} in non-parent process!", c)
            }
        }
    }

    pub fn test_get_num_recorded_errors(&self, error: ErrorType) -> i32 {
        match self {
            CustomDistributionMetric::Parent { inner, .. } => {
                inner.test_get_num_recorded_errors(error)
            }
            CustomDistributionMetric::Child(c) => panic!(
                "Cannot get number of recorded errors for {:?} in non-parent process!",
                c
            ),
        }
    }
}

#[cfg(test)]
mod test {
    use crate::{common_test::*, ipc, metrics};

    #[test]
    fn smoke_test_custom_distribution() {
        let _lock = lock_test();

        let metric = &metrics::test_only_ipc::a_custom_dist;

        metric.accumulate_samples_signed(vec![1, 2, 3]);

        assert!(metric.test_get_value("store1").is_some());
    }

    #[test]
    fn custom_distribution_child() {
        let _lock = lock_test();

        let parent_metric = &metrics::test_only_ipc::a_custom_dist;
        parent_metric.accumulate_samples_signed(vec![1, 268435458]);

        {
            let child_metric = parent_metric.child_metric();

            // scope for need_ipc RAII
            let _raii = ipc::test_set_need_ipc(true);

            child_metric.accumulate_samples_signed(vec![4, 268435460]);
        }

        let buf = ipc::take_buf().unwrap();
        assert!(buf.len() > 0);
        assert!(ipc::replay_from_buf(&buf).is_ok());

        let data = parent_metric
            .test_get_value("store1")
            .expect("should have some data");

        assert_eq!(2, data.values[&1], "Low bucket has 2 values");
        assert_eq!(
            2, data.values[&268435456],
            "Next higher bucket has 2 values"
        );
        assert_eq!(
            1 + 4 + 268435458 + 268435460,
            data.sum,
            "Sum of all recorded values"
        );
    }
}
