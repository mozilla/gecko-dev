// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use inherent::inherent;

use glean::traits::TimingDistribution;
#[cfg(feature = "with_gecko")]
use nsstring::{nsACString, nsCString};
use std::collections::HashMap;
use std::convert::TryInto;
use std::sync::Arc;
use std::time::Duration;

use crate::private::{DistributionData, ErrorType, MetricId, TimerId, TimingDistributionMetric};

use crate::ipc::with_ipc_payload;

/// A timing distribution metric that knows it's a labeled timing distribution's submetric.
///
/// Due to having to support GIFFT from Rust, this type ends up looking a little different from the rest.
#[derive(Clone)]
pub struct LabeledTimingDistributionMetric {
    pub(crate) inner: Arc<TimingDistributionMetric>,
    pub(crate) id: MetricId,
    pub(crate) label: String,
    pub(crate) kind: LabeledTimingDistributionMetricKind,
}
#[derive(Clone)]
pub enum LabeledTimingDistributionMetricKind {
    Parent,
    Child,
}

impl LabeledTimingDistributionMetric {
    #[cfg(test)]
    pub(crate) fn metric_id(&self) -> MetricId {
        self.id
    }
}

#[inherent]
impl TimingDistribution for LabeledTimingDistributionMetric {
    pub fn start(&self) -> TimerId {
        let timer_id = self.inner.inner_start();
        #[cfg(feature = "with_gecko")]
        {
            extern "C" {
                fn GIFFT_LabeledTimingDistributionStart(
                    metric_id: u32,
                    label: &nsACString,
                    timer_id: u64,
                );
            }
            // SAFETY: We're only loaning to C++ data we don't later use.
            unsafe {
                GIFFT_LabeledTimingDistributionStart(
                    self.id.0,
                    &nsCString::from(&self.label),
                    timer_id.id,
                );
            }
        }
        timer_id
    }

    pub fn stop_and_accumulate(&self, timer_id: TimerId) {
        match self.kind {
            LabeledTimingDistributionMetricKind::Parent => {
                self.inner.inner_stop_and_accumulate(timer_id)
            }
            LabeledTimingDistributionMetricKind::Child => {
                if let Some(sample) = self.inner.child_stop(timer_id) {
                    with_ipc_payload(move |payload| {
                        if let Some(map) = payload.labeled_timing_samples.get_mut(&self.id) {
                            if let Some(v) = map.get_mut(&self.label) {
                                v.push(sample);
                            } else {
                                map.insert(self.label.to_string(), vec![sample]);
                            }
                        } else {
                            let mut map = HashMap::new();
                            map.insert(self.label.to_string(), vec![sample]);
                            payload.labeled_timing_samples.insert(self.id, map);
                        }
                    });
                } else {
                    // TODO: report an error (timer id for stop wasn't started).
                }
            }
        }
        #[cfg(feature = "with_gecko")]
        {
            extern "C" {
                fn GIFFT_LabeledTimingDistributionStopAndAccumulate(
                    metric_id: u32,
                    label: &nsACString,
                    timer_id: u64,
                );
            }
            // SAFETY: We're only loaning to C++ data we don't later use.
            unsafe {
                GIFFT_LabeledTimingDistributionStopAndAccumulate(
                    self.id.0,
                    &nsCString::from(&self.label),
                    timer_id.id,
                );
            }
        }
    }

    pub fn cancel(&self, id: TimerId) {
        self.inner.inner_cancel(id);
        #[cfg(feature = "with_gecko")]
        {
            extern "C" {
                fn GIFFT_LabeledTimingDistributionCancel(
                    metric_id: u32,
                    label: &nsACString,
                    timer_id: u64,
                );
            }
            // SAFETY: We're only loaning to C++ data we don't later use.
            unsafe {
                GIFFT_LabeledTimingDistributionCancel(
                    self.id.0,
                    &nsCString::from(&self.label),
                    id.id,
                );
            }
        }
    }

    pub fn accumulate_samples(&self, samples: Vec<i64>) {
        self.inner.accumulate_samples(samples);
    }

    pub fn accumulate_raw_samples_nanos(&self, samples: Vec<u64>) {
        self.inner.accumulate_raw_samples_nanos(samples);
    }

    pub fn accumulate_single_sample(&self, sample: i64) {
        self.inner.accumulate_single_sample(sample);
    }

    pub fn accumulate_raw_duration(&self, duration: Duration) {
        match self.kind {
            LabeledTimingDistributionMetricKind::Parent => {
                self.inner.inner_accumulate_raw_duration(duration)
            }
            LabeledTimingDistributionMetricKind::Child => {
                let sample = duration.as_nanos().try_into().unwrap_or_else(|_| {
                    // TODO: Instrument this error
                    log::warn!(
                        "Elapsed nanoseconds larger than fits into 64-bytes. Saturating at u64::MAX."
                    );
                    u64::MAX
                });
                with_ipc_payload(move |payload| {
                    if let Some(map) = payload.labeled_timing_samples.get_mut(&self.id) {
                        if let Some(v) = map.get_mut(&self.label) {
                            v.push(sample);
                        } else {
                            map.insert(self.label.to_string(), vec![sample]);
                        }
                    } else {
                        let mut map = HashMap::new();
                        map.insert(self.label.to_string(), vec![sample]);
                        payload.labeled_timing_samples.insert(self.id, map);
                    }
                });
            }
        }
        #[cfg(feature = "with_gecko")]
        {
            let sample_ms = duration.as_millis().try_into().unwrap_or_else(|_| {
                // TODO: Instrument this error
                log::warn!(
                    "Elapsed milliseconds larger than fits into 32-bytes. Saturating at u32::MAX."
                );
                u32::MAX
            });
            extern "C" {
                fn GIFFT_LabeledTimingDistributionAccumulateRawMillis(
                    metric_id: u32,
                    label: &nsACString,
                    sample_ms: u32,
                );
            }
            // SAFETY: We're only loaning to C++ data we don't later use.
            unsafe {
                GIFFT_LabeledTimingDistributionAccumulateRawMillis(
                    self.id.0,
                    &nsCString::from(&self.label),
                    sample_ms,
                );
            }
        }
    }

    pub fn test_get_value<'a, S: Into<Option<&'a str>>>(
        &self,
        ping_name: S,
    ) -> Option<DistributionData> {
        self.inner.test_get_value(ping_name)
    }

    pub fn test_get_num_recorded_errors(&self, error: ErrorType) -> i32 {
        self.inner.test_get_num_recorded_errors(error)
    }
}

#[cfg(test)]
mod test {
    use crate::{common_test::*, ipc, metrics};

    #[test]
    fn smoke_test_labeled_timing_distribution() {
        let _lock = lock_test();

        let metric = &metrics::test_only::where_has_the_time_gone;

        let id = metric.get("Americas/Toronto").start();
        // Stopping right away might not give us data, if the underlying clock source is not precise
        // enough.
        // So let's cancel and make sure nothing blows up.
        metric.get("Americas/Toronto").cancel(id);

        assert!(metric
            .get("Americas/Toronto")
            .test_get_value(None)
            .is_none());
    }

    #[test]
    fn labeled_timing_distribution_child() {
        let _lock = lock_test();

        let parent_metric = &metrics::test_only::where_has_the_time_gone;
        let label = "days gone by";
        let id = parent_metric.get(label).start();
        std::thread::sleep(std::time::Duration::from_millis(10));
        parent_metric.get(label).stop_and_accumulate(id);

        {
            // scope for need_ipc RAII
            let _raii = ipc::test_set_need_ipc(true);

            // clear the per-process submetric cache,
            // or else we'll be given the parent-process child metric.
            {
                let mut map =
                    crate::metrics::__glean_metric_maps::submetric_maps::TIMING_DISTRIBUTION_MAP
                        .write()
                        .expect("Write lock for TIMING_DISTRIBUTION_MAP was poisoned");
                map.clear();
            }

            let child_metric = parent_metric.get(label);

            let id = child_metric.start();
            let id2 = child_metric.start();
            assert_ne!(id, id2);
            std::thread::sleep(std::time::Duration::from_millis(10));
            child_metric.stop_and_accumulate(id);

            child_metric.cancel(id2);

            ipc::with_ipc_payload(move |payload| {
                assert_eq!(
                    1,
                    payload
                        .labeled_timing_samples
                        .get(&child_metric.metric_id())
                        .unwrap()
                        .get(label)
                        .unwrap()
                        .len(),
                    "Stored the correct number of samples in the ipc payload"
                );
            });

            // clear the per-process submetric cache again,
            // or else we'll be given the child -process child metric below.
            {
                let mut map =
                    crate::metrics::__glean_metric_maps::submetric_maps::TIMING_DISTRIBUTION_MAP
                        .write()
                        .expect("Write lock for TIMING_DISTRIBUTION_MAP was poisoned");
                map.clear();
            }
        }

        let parent_only_data = parent_metric.get(label).test_get_value(None).unwrap();
        assert_eq!(
            1,
            parent_only_data
                .values
                .values()
                .fold(0, |acc, count| acc + count)
        );

        // Single-process IPC machine goes brrrrr...
        let buf = ipc::take_buf().unwrap();
        assert!(buf.len() > 0);
        assert!(ipc::replay_from_buf(&buf).is_ok());

        let data = parent_metric
            .get(label)
            .test_get_value(None)
            .expect("should have some data");

        // No guarantees from timers means no guarantees on buckets.
        // But we can guarantee it's only two samples.
        assert_eq!(
            2,
            data.values.values().fold(0, |acc, count| acc + count),
            "record 2 values, one parent, one child measurement"
        );
        assert!(0 < data.sum, "record some time");
    }
}
