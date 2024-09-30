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
    /// Passes the closure a DownloadTimer.  Use this to measure the times for all
    /// downloads that happen during the ingest
    pub fn measure_ingest<F, T>(&mut self, record_type: impl Into<String>, operation: F) -> T
    where
        F: FnOnce(&mut DownloadTimer) -> T,
    {
        let timer = Instant::now();
        let record_type = record_type.into();
        let mut download_metrics = DownloadTimer::default();
        let result = operation(&mut download_metrics);
        let elapsed = timer.elapsed().as_micros() as u64;
        self.ingestion_times.push(LabeledTimingSample::new(
            record_type.clone(),
            elapsed - download_metrics.total_time,
        ));
        self.download_times.push(LabeledTimingSample::new(
            record_type,
            download_metrics.total_time,
        ));
        result
    }
}

/// Records download times for a single loop in ingest
///
/// [Self::measure_download] can be called multiple times.  [DownloadTimer] will track the total
/// time for all calls.
#[derive(Default)]
pub struct DownloadTimer {
    total_time: u64,
}

impl DownloadTimer {
    pub fn measure_download<F, T>(&mut self, operation: F) -> T
    where
        F: FnOnce() -> T,
    {
        let timer = Instant::now();
        let result = operation();
        self.total_time += timer.elapsed().as_micros() as u64;
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
