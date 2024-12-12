// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use inherent::inherent;
use std::collections::HashMap;
use std::convert::TryInto;
use std::sync::{
    atomic::{AtomicUsize, Ordering},
    Arc, RwLock,
};
use std::time::{Duration, Instant};

use super::{CommonMetricData, MetricId, TimeUnit};
use glean::{DistributionData, ErrorType, TimerId};

use crate::ipc::{need_ipc, with_ipc_payload};
use glean::traits::TimingDistribution;

#[cfg(feature = "with_gecko")]
use super::profiler_utils::{
    lookup_canonical_metric_name, truncate_vector_for_marker, LookupError,
};

#[cfg(feature = "with_gecko")]
use gecko_profiler::{gecko_profiler_category, MarkerOptions, MarkerTiming};

#[cfg(feature = "with_gecko")]
#[derive(serde::Serialize, serde::Deserialize, Debug)]
pub(crate) enum TDMPayload {
    Duration(std::time::Duration),
    Sample(i64),
    Samples(Vec<i64>),
    SamplesNS(Vec<u64>),
}

#[cfg(feature = "with_gecko")]
impl TDMPayload {
    pub fn from_samples_signed(samples: &Vec<i64>) -> TDMPayload {
        TDMPayload::Samples(truncate_vector_for_marker(samples))
    }
    pub fn from_samples_unsigned(samples: &Vec<u64>) -> TDMPayload {
        TDMPayload::SamplesNS(truncate_vector_for_marker(samples))
    }
}

#[cfg(feature = "with_gecko")]
#[derive(serde::Serialize, serde::Deserialize, Debug)]
pub(crate) struct TimingDistributionMetricMarker {
    id: MetricId,
    label: Option<String>,
    timer_id: Option<u64>,
    value: Option<TDMPayload>,
}

#[cfg(feature = "with_gecko")]
impl TimingDistributionMetricMarker {
    pub fn new(
        id: MetricId,
        label: Option<String>,
        timer_id: Option<u64>,
        value: Option<TDMPayload>,
    ) -> TimingDistributionMetricMarker {
        TimingDistributionMetricMarker {
            id,
            label,
            timer_id,
            value,
        }
    }
}

#[cfg(feature = "with_gecko")]
impl gecko_profiler::ProfilerMarker for TimingDistributionMetricMarker {
    fn marker_type_name() -> &'static str {
        "TimingDist"
    }

    fn marker_type_display() -> gecko_profiler::MarkerSchema {
        use gecko_profiler::schema::*;
        let mut schema = MarkerSchema::new(&[Location::MarkerChart, Location::MarkerTable]);
        schema.set_tooltip_label(
            "{marker.data.id} {marker.data.label} {marker.data.duration}{marker.data.sample}",
        );
        schema.set_table_label("{marker.name} - {marker.data.id}: {marker.data.duration}{marker.data.sample}{marker.data.samples}");
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
            Format::String,
            Searchable::Searchable,
        );
        schema.add_key_label_format_searchable(
            "timer_id",
            "TimerId",
            Format::Integer,
            Searchable::Searchable,
        );
        schema.add_key_label_format("duration", "Duration", Format::String);
        schema.add_key_label_format("sample", "Sample", Format::String);
        schema.add_key_label_format("samples", "Samples", Format::String);
        schema
    }

    fn stream_json_marker_data(&self, json_writer: &mut gecko_profiler::JSONWriter) {
        json_writer.unique_string_property(
            "id",
            lookup_canonical_metric_name(&self.id).unwrap_or_else(LookupError::as_str),
        );

        match &self.label {
            Some(s) => json_writer.string_property("label", s.as_str()),
            _ => {}
        };

        match &self.timer_id {
            Some(id) => {
                // We don't care about exactly what the timer id is - so just
                // perform a bitwise cast, as that provides a 1:1 mapping.
                json_writer.int_property("timer_id", *id as i64);
            }
            _ => {}
        };

        match &self.value {
            Some(p) => {
                match p {
                    TDMPayload::Duration(d) => {
                        // Durations do not have a `Display` implementation,
                        // however for the profiler, the debug formatting
                        // should be more than sufficient.
                        let s = format!("{:?}", d);
                        json_writer.string_property("duration", s.as_str());
                    }
                    TDMPayload::Sample(s) => {
                        let s = format!("{}", s);
                        json_writer.string_property("sample", s.as_str());
                    }
                    TDMPayload::Samples(s) => {
                        let s = format!(
                            "[{}]",
                            s.iter()
                                .map(|v| v.to_string())
                                .collect::<Vec<_>>()
                                .join(",")
                        );
                        json_writer.string_property("samples", s.as_str());
                    }
                    TDMPayload::SamplesNS(s) => {
                        let s = format!(
                            "(ns) [{}]",
                            s.iter()
                                .map(|v| v.to_string())
                                .collect::<Vec<_>>()
                                .join(",")
                        );
                        json_writer.string_property("samples", s.as_str());
                    }
                };
            }
            None => {}
        };
    }
}

/// A timing distribution metric.
///
/// Timing distributions are used to accumulate and store time measurements for analyzing distributions of the timing data.
pub enum TimingDistributionMetric {
    Parent {
        /// The metric's ID.
        ///
        /// No longer test-only, is also used for GIFFT.
        id: MetricId,
        inner: Arc<glean::private::TimingDistributionMetric>,
    },
    Child(TimingDistributionMetricIpc),
}
#[derive(Debug)]
pub struct TimingDistributionMetricIpc {
    metric_id: MetricId,
    next_timer_id: AtomicUsize,
    instants: RwLock<HashMap<u64, Instant>>,
}

impl TimingDistributionMetric {
    /// Create a new timing distribution metric, _child process only_.
    pub(crate) fn new_child(id: MetricId) -> Self {
        debug_assert!(need_ipc());
        TimingDistributionMetric::Child(TimingDistributionMetricIpc {
            metric_id: id,
            next_timer_id: AtomicUsize::new(0),
            instants: RwLock::new(HashMap::new()),
        })
    }

    /// Create a new timing distribution metric.
    pub fn new(id: MetricId, meta: CommonMetricData, time_unit: TimeUnit) -> Self {
        if need_ipc() {
            Self::new_child(id)
        } else {
            let inner = glean::private::TimingDistributionMetric::new(meta, time_unit);
            TimingDistributionMetric::Parent {
                id,
                inner: Arc::new(inner),
            }
        }
    }

    #[cfg(test)]
    pub(crate) fn child_metric(&self) -> Self {
        match self {
            TimingDistributionMetric::Parent { id, .. } => {
                TimingDistributionMetric::Child(TimingDistributionMetricIpc {
                    metric_id: *id,
                    next_timer_id: AtomicUsize::new(0),
                    instants: RwLock::new(HashMap::new()),
                })
            }
            TimingDistributionMetric::Child(_) => {
                panic!("Can't get a child metric from a child metric")
            }
        }
    }

    /// Performs the core portions of a start() call, but no frippery like GIFFT.
    pub(crate) fn inner_start(&self) -> TimerId {
        match self {
            TimingDistributionMetric::Parent { inner, .. } => inner.start(),
            TimingDistributionMetric::Child(c) => {
                // There is no glean-core on this process to give us a TimerId,
                // so we'll have to make our own and do our own bookkeeping.
                let id = c
                    .next_timer_id
                    .fetch_add(1, Ordering::SeqCst)
                    .try_into()
                    .unwrap();
                let mut map = c
                    .instants
                    .write()
                    .expect("lock of instants map was poisoned");
                if let Some(_v) = map.insert(id, Instant::now()) {
                    // TODO: report an error and find a different TimerId.
                }
                id.into()
            }
        }
    }

    /// Performs the core portions of a stop_and_accumulate() call, but no frippery like GIFFT.
    pub(crate) fn inner_stop_and_accumulate(&self, id: TimerId) {
        match self {
            TimingDistributionMetric::Parent { inner, .. } => inner.stop_and_accumulate(id),
            TimingDistributionMetric::Child(c) => {
                if let Some(sample) = self.child_stop(id) {
                    with_ipc_payload(move |payload| {
                        if let Some(v) = payload.timing_samples.get_mut(&c.metric_id) {
                            v.push(sample);
                        } else {
                            payload.timing_samples.insert(c.metric_id, vec![sample]);
                        }
                    });
                } else {
                    // TODO: report an error (timer id for stop wasn't started).
                }
            }
        }
    }

    /// Stops the provided TimerId, but instead of accumulating the sample, returns it. No GIFFT neither.
    pub(crate) fn child_stop(&self, id: TimerId) -> Option<u64> {
        match self {
            TimingDistributionMetric::Parent { .. } => {
                panic!("Can't child_stop a parent-process timing_distribution")
            }
            TimingDistributionMetric::Child(c) => {
                let mut map = c
                    .instants
                    .write()
                    .expect("Write lock must've been poisoned.");
                if let Some(start) = map.remove(&id.id) {
                    let now = Instant::now();
                    let sample = now
                        .checked_duration_since(start)
                        .map(|s| s.as_nanos().try_into());
                    match sample {
                        Some(Ok(sample)) => Some(sample),
                        Some(Err(_)) => {
                            log::warn!("Elapsed time larger than fits into 64-bytes. Saturating at u64::MAX.");
                            Some(u64::MAX)
                        }
                        None => {
                            log::warn!("Time went backwards. Not recording.");
                            // TODO: report an error (timer id for stop was started, but time went backwards).
                            None
                        }
                    }
                } else {
                    // TODO: report an error (timer id for stop was never started).
                    None
                }
            }
        }
    }

    /// Cancels the provided TimerId without notifying GIFFT.
    pub(crate) fn inner_cancel(&self, id: TimerId) {
        match self {
            TimingDistributionMetric::Parent { inner, .. } => inner.cancel(id),
            TimingDistributionMetric::Child(c) => {
                let mut map = c
                    .instants
                    .write()
                    .expect("Write lock must've been poisoned.");
                if map.remove(&id.id).is_none() {
                    // TODO: report an error (cancelled a non-started id).
                }
            }
        }
    }

    /// Accumulates the raw duration without notifying GIFFT.
    pub(crate) fn inner_accumulate_raw_duration(&self, duration: Duration) {
        let sample = duration.as_nanos().try_into().unwrap_or_else(|_| {
            // TODO: Instrument this error
            log::warn!(
                "Elapsed nanoseconds larger than fits into 64-bytes. Saturating at u64::MAX."
            );
            u64::MAX
        });
        match self {
            TimingDistributionMetric::Parent { inner, .. } => {
                inner.accumulate_raw_duration(duration)
            }
            TimingDistributionMetric::Child(c) => {
                with_ipc_payload(move |payload| {
                    if let Some(v) = payload.timing_samples.get_mut(&c.metric_id) {
                        v.push(sample);
                    } else {
                        payload.timing_samples.insert(c.metric_id, vec![sample]);
                    }
                });
            }
        }
    }
}

#[inherent]
impl TimingDistribution for TimingDistributionMetric {
    /// Starts tracking time for the provided metric.
    ///
    /// This records an error if it’s already tracking time (i.e.
    /// [`start`](TimingDistribution::start) was already called with no corresponding
    /// [`stop_and_accumulate`](TimingDistribution::stop_and_accumulate)): in that case the
    /// original start time will be preserved.
    ///
    /// # Returns
    ///
    /// A unique [`TimerId`] for the new timer.
    pub fn start(&self) -> TimerId {
        let timer_id = self.inner_start();
        #[cfg(feature = "with_gecko")]
        {
            let metric_id = match self {
                TimingDistributionMetric::Parent { id, .. } => id,
                TimingDistributionMetric::Child(c) => &c.metric_id,
            };
            extern "C" {
                fn GIFFT_TimingDistributionStart(metric_id: u32, timer_id: u64);
            }
            // SAFETY: using only primitives, no return value.
            unsafe {
                GIFFT_TimingDistributionStart(metric_id.0, timer_id.id);
            }
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
            if gecko_profiler::can_accept_markers() {
                gecko_profiler::add_marker(
                    "TimingDistribution::start",
                    gecko_profiler_category!(Telemetry),
                    MarkerOptions::default().with_timing(MarkerTiming::instant_now()),
                    TimingDistributionMetricMarker::new(*metric_id, None, Some(timer_id.id), None),
                );
            }
        }
        timer_id.into()
    }

    /// Stops tracking time for the provided metric and associated timer id.
    ///
    /// Adds a count to the corresponding bucket in the timing distribution.
    /// This will record an error if no [`start`](TimingDistribution::start) was
    /// called.
    ///
    /// # Arguments
    ///
    /// * `id` - The [`TimerId`] to associate with this timing. This allows
    ///   for concurrent timing of events associated with different ids to the
    ///   same timespan metric.
    pub fn stop_and_accumulate(&self, id: TimerId) {
        self.inner_stop_and_accumulate(id);
        #[cfg(feature = "with_gecko")]
        {
            let metric_id = match self {
                TimingDistributionMetric::Parent { id, .. } => id,
                TimingDistributionMetric::Child(c) => &c.metric_id,
            };
            extern "C" {
                fn GIFFT_TimingDistributionStopAndAccumulate(metric_id: u32, timer_id: u64);
            }
            // SAFETY: using only primitives, no return value.
            unsafe {
                GIFFT_TimingDistributionStopAndAccumulate(metric_id.0, id.id);
            }
            // See note on TimingDistribution::start
            if gecko_profiler::can_accept_markers() {
                gecko_profiler::add_marker(
                    "TimingDistribution::stop",
                    gecko_profiler_category!(Telemetry),
                    MarkerOptions::default().with_timing(MarkerTiming::instant_now()),
                    TimingDistributionMetricMarker::new(*metric_id, None, Some(id.id), None),
                );
            }
        }
    }

    /// Aborts a previous [`start`](TimingDistribution::start) call. No
    /// error is recorded if no [`start`](TimingDistribution::start) was
    /// called.
    ///
    /// # Arguments
    ///
    /// * `id` - The [`TimerId`] to associate with this timing. This allows
    ///   for concurrent timing of events associated with different ids to the
    ///   same timing distribution metric.
    pub fn cancel(&self, id: TimerId) {
        self.inner_cancel(id);
        #[cfg(feature = "with_gecko")]
        {
            let metric_id = match self {
                TimingDistributionMetric::Parent { id, .. } => id,
                TimingDistributionMetric::Child(c) => &c.metric_id,
            };
            extern "C" {
                fn GIFFT_TimingDistributionCancel(metric_id: u32, timer_id: u64);
            }
            // SAFETY: using only primitives, no return value.
            unsafe {
                GIFFT_TimingDistributionCancel(metric_id.0, id.id);
            }
            // See note on TimingDistribution::start
            if gecko_profiler::can_accept_markers() {
                gecko_profiler::add_marker(
                    "TimingDistribution::cancel",
                    gecko_profiler_category!(Telemetry),
                    MarkerOptions::default().with_timing(MarkerTiming::instant_now()),
                    TimingDistributionMetricMarker::new(*metric_id, None, Some(id.id), None),
                );
            }
        }
    }

    /// Accumulates the provided signed samples in the metric.
    ///
    /// This is required so that the platform-specific code can provide us with
    /// 64 bit signed integers if no `u64` comparable type is available. This
    /// will take care of filtering and reporting errors for any provided negative
    /// sample.
    ///
    /// Please note that this assumes that the provided samples are already in
    /// the "unit" declared by the instance of the metric type (e.g. if the
    /// instance this method was called on is using [`crate::TimeUnit::Second`], then
    /// `samples` are assumed to be in that unit).
    ///
    /// # Arguments
    ///
    /// * `samples` - The vector holding the samples to be recorded by the metric.
    ///
    /// ## Notes
    ///
    /// Discards any negative value in `samples` and report an [`ErrorType::InvalidValue`]
    /// for each of them. Reports an [`ErrorType::InvalidOverflow`] error for samples that
    /// are longer than `MAX_SAMPLE_TIME`.
    pub fn accumulate_samples(&self, samples: Vec<i64>) {
        match self {
            #[allow(unused)]
            TimingDistributionMetric::Parent { id, inner } => {
                #[cfg(feature = "with_gecko")]
                if gecko_profiler::can_accept_markers() {
                    gecko_profiler::add_marker(
                        "TimingDistribution::accumulate",
                        gecko_profiler_category!(Telemetry),
                        MarkerOptions::default(),
                        TimingDistributionMetricMarker::new(
                            *id,
                            None,
                            None,
                            Some(TDMPayload::from_samples_signed(&samples)),
                        ),
                    );
                }
                inner.accumulate_samples(samples)
            }
            TimingDistributionMetric::Child(_c) => {
                // TODO: Instrument this error
                log::error!("Can't record samples for a timing distribution from a child metric");
            }
        }
    }

    /// Accumulates the provided samples in the metric.
    ///
    /// # Arguments
    ///
    /// * `samples` - A list of samples recorded by the metric.
    ///               Samples must be in nanoseconds.
    /// ## Notes
    ///
    /// Reports an [`ErrorType::InvalidOverflow`] error for samples that
    /// are longer than `MAX_SAMPLE_TIME`.
    pub fn accumulate_raw_samples_nanos(&self, samples: Vec<u64>) {
        match self {
            #[allow(unused)]
            TimingDistributionMetric::Parent { id, inner } => {
                #[cfg(feature = "with_gecko")]
                if gecko_profiler::can_accept_markers() {
                    gecko_profiler::add_marker(
                        "TimingDistribution::accumulate",
                        gecko_profiler_category!(Telemetry),
                        MarkerOptions::default(),
                        TimingDistributionMetricMarker::new(
                            *id,
                            None,
                            None,
                            Some(TDMPayload::from_samples_unsigned(&samples)),
                        ),
                    );
                }
                inner.accumulate_raw_samples_nanos(samples)
            }
            TimingDistributionMetric::Child(_c) => {
                // TODO: Instrument this error
                log::error!("Can't record samples for a timing distribution from a child metric");
            }
        }
    }

    pub fn accumulate_single_sample(&self, sample: i64) {
        match self {
            #[allow(unused)]
            TimingDistributionMetric::Parent { id, inner } => {
                #[cfg(feature = "with_gecko")]
                if gecko_profiler::can_accept_markers() {
                    gecko_profiler::add_marker(
                        "TimingDistribution::accumulate",
                        gecko_profiler_category!(Telemetry),
                        MarkerOptions::default(),
                        TimingDistributionMetricMarker::new(
                            *id,
                            None,
                            None,
                            Some(TDMPayload::Sample(sample.clone())),
                        ),
                    );
                }
                inner.accumulate_single_sample(sample)
            }
            TimingDistributionMetric::Child(_c) => {
                // TODO: Instrument this error
                log::error!("Can't record samples for a timing distribution from a child metric");
            }
        }
    }

    /// Accumulates a time duration sample for the provided metric.
    ///
    /// Adds a count to the corresponding bucket in the timing distribution.
    /// Saturates at u64::MAX nanoseconds.
    ///
    /// Prefer start() and stop_and_accumulate() where possible.
    ///
    /// Users of this API are responsible for ensuring the timing source used
    /// to calculate the duration is monotonic and consistent across platforms.
    ///
    /// # Arguments
    ///
    /// * `duration` - The [`Duration`] of the accumulated sample.
    pub fn accumulate_raw_duration(&self, duration: Duration) {
        self.inner_accumulate_raw_duration(duration);
        #[cfg(feature = "with_gecko")]
        {
            let sample_ms = duration.as_millis().try_into().unwrap_or_else(|_| {
                // TODO: Instrument this error
                log::warn!(
                    "Elapsed milliseconds larger than fits into 32-bytes. Saturating at u32::MAX."
                );
                u32::MAX
            });
            let metric_id = match self {
                TimingDistributionMetric::Parent { id, .. } => id,
                TimingDistributionMetric::Child(c) => &c.metric_id,
            };
            extern "C" {
                fn GIFFT_TimingDistributionAccumulateRawMillis(metric_id: u32, sample: u32);
            }
            // SAFETY: using only primitives, no return value.
            unsafe {
                GIFFT_TimingDistributionAccumulateRawMillis(metric_id.0, sample_ms);
            }

            if gecko_profiler::can_accept_markers() {
                gecko_profiler::add_marker(
                    "TimingDistribution::accumulate",
                    gecko_profiler_category!(Telemetry),
                    MarkerOptions::default(),
                    TimingDistributionMetricMarker::new(
                        *metric_id,
                        None,
                        None,
                        Some(TDMPayload::Duration(duration.clone())),
                    ),
                );
            }
        }
    }

    /// **Exported for test purposes.**
    ///
    /// Gets the currently stored value of the metric.
    ///
    /// This doesn't clear the stored value.
    ///
    /// # Arguments
    ///
    /// * `ping_name` - represents the optional name of the ping to retrieve the
    ///   metric for. Defaults to the first value in `send_in_pings`.
    pub fn test_get_value<'a, S: Into<Option<&'a str>>>(
        &self,
        ping_name: S,
    ) -> Option<DistributionData> {
        let ping_name = ping_name.into().map(|s| s.to_string());
        match self {
            TimingDistributionMetric::Parent { inner, .. } => inner.test_get_value(ping_name),
            TimingDistributionMetric::Child(c) => {
                panic!("Cannot get test value for {:?} in non-parent process!", c)
            }
        }
    }

    /// **Exported for test purposes.**
    ///
    /// Gets the number of recorded errors for the given error type.
    ///
    /// # Arguments
    ///
    /// * `error` - The type of error
    /// * `ping_name` - represents the optional name of the ping to retrieve the
    ///   metric for. Defaults to the first value in `send_in_pings`.
    ///
    /// # Returns
    ///
    /// The number of errors recorded.
    pub fn test_get_num_recorded_errors(&self, error: ErrorType) -> i32 {
        match self {
            TimingDistributionMetric::Parent { inner, .. } => {
                inner.test_get_num_recorded_errors(error)
            }
            TimingDistributionMetric::Child(c) => panic!(
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
    fn smoke_test_timing_distribution() {
        let _lock = lock_test();

        let metric = &metrics::test_only_ipc::a_timing_dist;

        let id = metric.start();
        // Stopping right away might not give us data, if the underlying clock source is not precise
        // enough.
        // So let's cancel and make sure nothing blows up.
        metric.cancel(id);

        // We can't inspect the values yet.
        assert!(metric.test_get_value("store1").is_none());
    }

    #[test]
    fn timing_distribution_child() {
        let _lock = lock_test();

        let parent_metric = &metrics::test_only_ipc::a_timing_dist;
        let id = parent_metric.start();
        std::thread::sleep(std::time::Duration::from_millis(10));
        parent_metric.stop_and_accumulate(id);

        {
            let child_metric = parent_metric.child_metric();

            // scope for need_ipc RAII
            let _raii = ipc::test_set_need_ipc(true);

            let id = child_metric.start();
            let id2 = child_metric.start();
            assert_ne!(id, id2);
            std::thread::sleep(std::time::Duration::from_millis(10));
            child_metric.stop_and_accumulate(id);

            child_metric.cancel(id2);
        }

        let buf = ipc::take_buf().unwrap();
        assert!(buf.len() > 0);
        assert!(ipc::replay_from_buf(&buf).is_ok());

        let data = parent_metric
            .test_get_value("store1")
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
