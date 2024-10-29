/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::time::Instant;

/// Single sample for a Glean labeled_timing_distribution
#[derive(uniffi::Record)]
pub struct LabeledTimingSample {
    pub label: String,
    /// Time in microseconds
    pub value: u64,
}

impl LabeledTimingSample {
    fn new(label: String, value: u64) -> Self {
        Self { label, value }
    }
}

/// Ingestion metrics
///
/// These are recorded during [crate::Store::ingest] and returned to the consumer to record.
#[derive(Default, uniffi::Record)]
pub struct SuggestIngestionMetrics {
    /// Samples for the `suggest.ingestion_time` metric
    pub ingestion_times: Vec<LabeledTimingSample>,
    /// Samples for the `suggest.ingestion_download_time` metric
    pub download_times: Vec<LabeledTimingSample>,
}

impl SuggestIngestionMetrics {
    /// Wraps each iteration in `ingest` and records the time for it.
    ///
    /// Passes the closure a `&mut MetricsContext`.
    pub fn measure_ingest<F, T>(&mut self, record_type: impl Into<String>, operation: F) -> T
    where
        F: FnOnce(&mut MetricsContext) -> T,
    {
        let timer = Instant::now();
        let record_type = record_type.into();
        let mut context = MetricsContext::default();
        let result = operation(&mut context);
        let elapsed = timer.elapsed().as_micros() as u64;
        match context {
            MetricsContext::Uninstrumented => (),
            MetricsContext::Instrumented { download_time } => {
                self.ingestion_times.push(LabeledTimingSample::new(
                    record_type.clone(),
                    elapsed - download_time,
                ));
                self.download_times
                    .push(LabeledTimingSample::new(record_type, download_time));
            }
        }
        result
    }
}

/// Context for a ingestion measurement
#[derive(Default)]
pub enum MetricsContext {
    /// Default state, if this is the state at the end of `measure_ingest`, then it means no work
    /// was done and we should not record anything
    #[default]
    Uninstrumented,
    /// State after `measure_download()` is called.  We currently always download an attachment
    /// whenever we do any ingestion work, so we can use this a test for if we should record
    /// anything.
    Instrumented { download_time: u64 },
}

impl MetricsContext {
    /// Tracks download times during the ingestion
    ///
    /// We report download times as a separate metric [Self::measure_download] can be called
    /// multiple times.  [DownloadTimer] will track the total time for all calls.
    pub fn measure_download<F, T>(&mut self, operation: F) -> T
    where
        F: FnOnce() -> T,
    {
        let timer = Instant::now();
        let result = operation();
        let elasped = timer.elapsed().as_micros() as u64;
        match self {
            Self::Uninstrumented => {
                *self = Self::Instrumented {
                    download_time: elasped,
                }
            }
            Self::Instrumented { download_time } => *download_time += elasped,
        }
        result
    }
}

/// Query metrics
///
/// These are recorded during [crate::Store::query] and returned to the consumer to record.
#[derive(Default)]
pub struct SuggestQueryMetrics {
    pub times: Vec<LabeledTimingSample>,
}

impl SuggestQueryMetrics {
    pub fn measure_query<F, T>(&mut self, provider: impl Into<String>, operation: F) -> T
    where
        F: FnOnce() -> T,
    {
        let provider = provider.into();
        let timer = Instant::now();
        // Make sure the compiler doesn't reorder/inline in a way that invalidates this
        // measurement.
        let result = std::hint::black_box(operation());
        let elapsed = timer.elapsed().as_micros() as u64;
        self.times.push(LabeledTimingSample::new(provider, elapsed));
        result
    }
}
