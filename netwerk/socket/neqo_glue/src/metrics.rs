/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use std::cell::RefCell;
#[cfg(not(target_os = "android"))]
use std::convert::{TryFrom, TryInto};

use firefox_on_glean::metrics::networking as glean;

#[cfg(not(target_os = "android"))]
const BUFFER_SIZE: usize = 1024;

std::thread_local! {
    pub static METRICS: RefCell<BufferedMetrics> = RefCell::new(BufferedMetrics::default());
}

#[allow(dead_code)]
pub struct BufferedMetric<S> {
    buffer: Vec<S>,
    sync: fn(Vec<S>),
}

impl<S> BufferedMetric<S> {
    fn new(f: fn(Vec<S>)) -> Self {
        Self {
            buffer: vec![],
            sync: f,
        }
    }
}

#[cfg(not(target_os = "android"))]
impl<S: TryFrom<usize>> BufferedMetric<S> {
    pub fn sample(&mut self, sample: usize) {
        let Ok(sample) = sample.try_into() else {
            neqo_common::qdebug!("failed to convert {sample} to metric's sample type");
            return;
        };
        self.buffer.push(sample);
        if self.buffer.len() == BUFFER_SIZE {
            self.sync_to_glean();
        }
    }

    fn sync_to_glean(&mut self) {
        if !self.buffer.is_empty() {
            (self.sync)(std::mem::take(&mut self.buffer));
        }
    }
}

// Noop on Android for now, due to performance regressions.
// - <https://bugzilla.mozilla.org/show_bug.cgi?id=1898810>
// - <https://bugzilla.mozilla.org/show_bug.cgi?id=1906664>
#[cfg(target_os = "android")]
impl<S> BufferedMetric<S> {
    pub fn sample(&mut self, _sample: usize) {}

    fn sync_to_glean(&mut self) {}
}

pub struct BufferedMetrics {
    pub datagram_segment_size_sent: BufferedMetric<u64>,
    pub datagram_segment_size_received: BufferedMetric<u64>,
    pub datagram_size_received: BufferedMetric<u64>,
    pub datagram_segments_received: BufferedMetric<i64>,
}

impl Default for BufferedMetrics {
    fn default() -> Self {
        Self {
            datagram_segment_size_sent: BufferedMetric::new(|samples| {
                glean::http_3_udp_datagram_segment_size_sent.accumulate_samples(samples)
            }),
            datagram_segment_size_received: BufferedMetric::new(|samples| {
                glean::http_3_udp_datagram_segment_size_received.accumulate_samples(samples)
            }),
            datagram_size_received: BufferedMetric::new(|samples| {
                glean::http_3_udp_datagram_size_received.accumulate_samples(samples)
            }),
            datagram_segments_received: BufferedMetric::new(|samples| {
                glean::http_3_udp_datagram_segments_received.accumulate_samples_signed(samples)
            }),
        }
    }
}

impl BufferedMetrics {
    pub fn sync_to_glean(&mut self) {
        self.datagram_segment_size_sent.sync_to_glean();
        self.datagram_segment_size_received.sync_to_glean();
        self.datagram_size_received.sync_to_glean();
        self.datagram_segments_received.sync_to_glean();
    }
}
