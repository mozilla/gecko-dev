// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::mem;
use std::sync::Arc;

use crate::common_metric_data::CommonMetricDataInternal;
use crate::error_recording::{record_error, test_get_num_recorded_errors, ErrorType};
use crate::histogram::{Bucketing, Histogram, HistogramType, LinearOrExponential};
use crate::metrics::{DistributionData, Metric, MetricType};
use crate::storage::StorageManager;
use crate::CommonMetricData;
use crate::Glean;

/// A custom distribution metric.
#[derive(Clone, Debug)]
pub struct CustomDistributionMetric {
    meta: Arc<CommonMetricDataInternal>,
    range_min: u64,
    range_max: u64,
    bucket_count: u64,
    histogram_type: HistogramType,
}

/// Create a snapshot of the histogram.
///
/// The snapshot can be serialized into the payload format.
pub(crate) fn snapshot<B: Bucketing>(hist: &Histogram<B>) -> DistributionData {
    DistributionData {
        values: hist
            .snapshot_values()
            .into_iter()
            .map(|(k, v)| (k as i64, v as i64))
            .collect(),
        sum: hist.sum() as i64,
        count: hist.count() as i64,
    }
}

impl MetricType for CustomDistributionMetric {
    fn meta(&self) -> &CommonMetricDataInternal {
        &self.meta
    }

    fn with_name(&self, name: String) -> Self {
        let mut meta = (*self.meta).clone();
        meta.inner.name = name;
        Self {
            meta: Arc::new(meta),
            range_min: self.range_min,
            range_max: self.range_max,
            bucket_count: self.bucket_count,
            histogram_type: self.histogram_type,
        }
    }

    fn with_dynamic_label(&self, label: String) -> Self {
        let mut meta = (*self.meta).clone();
        meta.inner.dynamic_label = Some(label);
        Self {
            meta: Arc::new(meta),
            range_min: self.range_min,
            range_max: self.range_max,
            bucket_count: self.bucket_count,
            histogram_type: self.histogram_type,
        }
    }
}

// IMPORTANT:
//
// When changing this implementation, make sure all the operations are
// also declared in the related trait in `../traits/`.
impl CustomDistributionMetric {
    /// Creates a new memory distribution metric.
    pub fn new(
        meta: CommonMetricData,
        range_min: i64,
        range_max: i64,
        bucket_count: i64,
        histogram_type: HistogramType,
    ) -> Self {
        Self {
            meta: Arc::new(meta.into()),
            range_min: range_min as u64,
            range_max: range_max as u64,
            bucket_count: bucket_count as u64,
            histogram_type,
        }
    }

    /// Accumulates the provided signed samples in the metric.
    ///
    /// This is required so that the platform-specific code can provide us with
    /// 64 bit signed integers if no `u64` comparable type is available. This
    /// will take care of filtering and reporting errors for any provided negative
    /// sample.
    ///
    /// # Arguments
    ///
    /// - `samples` - The vector holding the samples to be recorded by the metric.
    ///
    /// ## Notes
    ///
    /// Discards any negative value in `samples` and report an [`ErrorType::InvalidValue`]
    /// for each of them.
    pub fn accumulate_samples(&self, samples: Vec<i64>) {
        let metric = self.clone();
        crate::launch_with_glean(move |glean| metric.accumulate_samples_sync(glean, &samples))
    }

    /// Accumulates precisely one signed sample and appends it to the metric.
    ///
    /// Signed is required so that the platform-specific code can provide us with a
    /// 64 bit signed integer if no `u64` comparable type is available. This
    /// will take care of filtering and reporting errors.
    ///
    /// # Arguments
    ///
    /// - `sample` - The singular sample to be recorded by the metric.
    ///
    /// ## Notes
    ///
    /// Discards any negative value of `sample` and reports an
    /// [`ErrorType::InvalidValue`](crate::ErrorType::InvalidValue).
    pub fn accumulate_single_sample(&self, sample: i64) {
        let metric = self.clone();
        crate::launch_with_glean(move |glean| metric.accumulate_samples_sync(glean, &[sample]))
    }

    /// Accumulates the provided sample in the metric synchronously.
    ///
    /// See [`accumulate_samples`](Self::accumulate_samples) for details.
    #[doc(hidden)]
    pub fn accumulate_samples_sync(&self, glean: &Glean, samples: &[i64]) {
        if !self.should_record(glean) {
            return;
        }

        let mut num_negative_samples = 0;

        // Generic accumulation function to handle the different histogram types and count negative
        // samples.
        fn accumulate<B: Bucketing, F>(
            samples: &[i64],
            mut hist: Histogram<B>,
            metric: F,
        ) -> (i32, Metric)
        where
            F: Fn(Histogram<B>) -> Metric,
        {
            let mut num_negative_samples = 0;
            for &sample in samples.iter() {
                if sample < 0 {
                    num_negative_samples += 1;
                } else {
                    let sample = sample as u64;
                    hist.accumulate(sample);
                }
            }
            (num_negative_samples, metric(hist))
        }

        glean.storage().record_with(glean, &self.meta, |old_value| {
            let (num_negative, hist) = match self.histogram_type {
                HistogramType::Linear => {
                    let hist = if let Some(Metric::CustomDistributionLinear(hist)) = old_value {
                        hist
                    } else {
                        Histogram::linear(
                            self.range_min,
                            self.range_max,
                            self.bucket_count as usize,
                        )
                    };
                    accumulate(samples, hist, Metric::CustomDistributionLinear)
                }
                HistogramType::Exponential => {
                    let hist = if let Some(Metric::CustomDistributionExponential(hist)) = old_value
                    {
                        hist
                    } else {
                        Histogram::exponential(
                            self.range_min,
                            self.range_max,
                            self.bucket_count as usize,
                        )
                    };
                    accumulate(samples, hist, Metric::CustomDistributionExponential)
                }
            };

            num_negative_samples = num_negative;
            hist
        });

        if num_negative_samples > 0 {
            let msg = format!("Accumulated {} negative samples", num_negative_samples);
            record_error(
                glean,
                &self.meta,
                ErrorType::InvalidValue,
                msg,
                num_negative_samples,
            );
        }
    }

    /// Gets the currently stored histogram.
    #[doc(hidden)]
    pub fn get_value<'a, S: Into<Option<&'a str>>>(
        &self,
        glean: &Glean,
        ping_name: S,
    ) -> Option<DistributionData> {
        let queried_ping_name = ping_name
            .into()
            .unwrap_or_else(|| &self.meta().inner.send_in_pings[0]);

        match StorageManager.snapshot_metric_for_test(
            glean.storage(),
            queried_ping_name,
            &self.meta.identifier(glean),
            self.meta.inner.lifetime,
        ) {
            // Boxing the value, in order to return either of the possible buckets
            Some(Metric::CustomDistributionExponential(hist)) => Some(snapshot(&hist)),
            Some(Metric::CustomDistributionLinear(hist)) => Some(snapshot(&hist)),
            _ => None,
        }
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Gets the currently stored value as an integer.
    ///
    /// This doesn't clear the stored value.
    ///
    /// # Arguments
    ///
    /// * `ping_name` - the optional name of the ping to retrieve the metric
    ///                 for. Defaults to the first value in `send_in_pings`.
    ///
    /// # Returns
    ///
    /// The stored value or `None` if nothing stored.
    pub fn test_get_value(&self, ping_name: Option<String>) -> Option<DistributionData> {
        crate::block_on_dispatcher();
        crate::core::with_glean(|glean| self.get_value(glean, ping_name.as_deref()))
    }

    /// **Exported for test purposes.**
    ///
    /// Gets the number of recorded errors for the given metric and error type.
    ///
    /// # Arguments
    ///
    /// * `error` - The type of error
    ///
    /// # Returns
    ///
    /// The number of errors reported.
    pub fn test_get_num_recorded_errors(&self, error: ErrorType) -> i32 {
        crate::block_on_dispatcher();

        crate::core::with_glean(|glean| {
            test_get_num_recorded_errors(glean, self.meta(), error).unwrap_or(0)
        })
    }

    /// **Experimental:** Start a new histogram buffer associated with this custom distribution metric.
    ///
    /// A histogram buffer accumulates in-memory.
    /// Data is recorded into the metric on drop.
    pub fn start_buffer(&self) -> LocalCustomDistribution<'_> {
        LocalCustomDistribution::new(self)
    }

    fn commit_histogram(&self, histogram: Histogram<LinearOrExponential>) {
        let metric = self.clone();
        crate::launch_with_glean(move |glean| {
            glean
                .storage()
                .record_with(glean, &metric.meta, move |old_value| {
                    match metric.histogram_type {
                        HistogramType::Linear => {
                            let mut hist =
                                if let Some(Metric::CustomDistributionLinear(hist)) = old_value {
                                    hist
                                } else {
                                    Histogram::linear(
                                        metric.range_min,
                                        metric.range_max,
                                        metric.bucket_count as usize,
                                    )
                                };

                            hist._merge(&histogram);
                            Metric::CustomDistributionLinear(hist)
                        }
                        HistogramType::Exponential => {
                            let mut hist = if let Some(Metric::CustomDistributionExponential(
                                hist,
                            )) = old_value
                            {
                                hist
                            } else {
                                Histogram::exponential(
                                    metric.range_min,
                                    metric.range_max,
                                    metric.bucket_count as usize,
                                )
                            };

                            hist._merge(&histogram);
                            Metric::CustomDistributionExponential(hist)
                        }
                    }
                });
        });
    }
}

/// **Experimental:** A histogram buffer associated with a specific instance of a [`CustomDistributionMetric`].
///
/// Accumulation happens in-memory.
/// Data is merged into the metric on [`Drop::drop`].
pub struct LocalCustomDistribution<'a> {
    histogram: Histogram<LinearOrExponential>,
    metric: &'a CustomDistributionMetric,
}

impl<'a> LocalCustomDistribution<'a> {
    /// Create a new histogram buffer referencing the custom distribution it will record into.
    fn new(metric: &'a CustomDistributionMetric) -> Self {
        let histogram = match metric.histogram_type {
            HistogramType::Linear => Histogram::<LinearOrExponential>::_linear(
                metric.range_min,
                metric.range_max,
                metric.bucket_count as usize,
            ),
            HistogramType::Exponential => Histogram::<LinearOrExponential>::_exponential(
                metric.range_min,
                metric.range_max,
                metric.bucket_count as usize,
            ),
        };
        Self { histogram, metric }
    }

    /// Accumulates one sample into the histogram.
    ///
    /// The provided sample must be in the "unit" declared by the instance of the metric type
    /// (e.g. if the instance this method was called on is using [`crate::TimeUnit::Second`], then
    /// `sample` is assumed to be in seconds).
    ///
    /// Accumulation happens in-memory only.
    pub fn accumulate(&mut self, sample: u64) {
        self.histogram.accumulate(sample)
    }

    /// Abandon this histogram buffer and don't commit accumulated data.
    pub fn abandon(mut self) {
        self.histogram.clear();
    }
}

impl Drop for LocalCustomDistribution<'_> {
    fn drop(&mut self) {
        if self.histogram.is_empty() {
            return;
        }

        // We want to move that value.
        // A `0/0` histogram doesn't allocate.
        let empty = Histogram::_linear(0, 0, 0);
        let buffer = mem::replace(&mut self.histogram, empty);
        self.metric.commit_histogram(buffer);
    }
}
