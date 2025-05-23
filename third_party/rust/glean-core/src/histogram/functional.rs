// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::collections::HashMap;

use malloc_size_of_derive::MallocSizeOf;
use serde::{Deserialize, Serialize};

use super::{Bucketing, Histogram};

use crate::util::floating_point_context::FloatingPointContext;

/// A functional bucketing algorithm.
///
/// Bucketing is performed by a function, rather than pre-computed buckets.
/// The bucket index of a given sample is determined with the following function:
///
/// i = ⌊n log<sub>base</sub>(𝑥)⌋
///
/// In other words, there are n buckets for each power of `base` magnitude.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, MallocSizeOf)]
pub struct Functional {
    exponent: f64,
}

impl Functional {
    /// Instantiate a new functional bucketing.
    fn new(log_base: f64, buckets_per_magnitude: f64) -> Functional {
        // Set the FPU control flag to the required state within this function
        let _fpc = FloatingPointContext::new();

        let exponent = log_base.powf(1.0 / buckets_per_magnitude);

        Functional { exponent }
    }

    /// Maps a sample to a "bucket index" that it belongs in.
    /// A "bucket index" is the consecutive integer index of each bucket, useful as a
    /// mathematical concept, even though the internal representation is stored and
    /// sent using the minimum value in each bucket.
    fn sample_to_bucket_index(&self, sample: u64) -> u64 {
        // Set the FPU control flag to the required state within this function
        let _fpc = FloatingPointContext::new();

        ((sample.saturating_add(1)) as f64).log(self.exponent) as u64
    }

    /// Determines the minimum value of a bucket, given a bucket index.
    fn bucket_index_to_bucket_minimum(&self, index: u64) -> u64 {
        // Set the FPU control flag to the required state within this function
        let _fpc = FloatingPointContext::new();

        self.exponent.powf(index as f64) as u64
    }
}

impl Bucketing for Functional {
    fn sample_to_bucket_minimum(&self, sample: u64) -> u64 {
        if sample == 0 {
            return 0;
        }

        let index = self.sample_to_bucket_index(sample);
        self.bucket_index_to_bucket_minimum(index)
    }

    fn ranges(&self) -> &[u64] {
        unimplemented!("Bucket ranges for functional bucketing are not precomputed")
    }
}

impl Histogram<Functional> {
    /// Creates a histogram with functional buckets.
    pub fn functional(log_base: f64, buckets_per_magnitude: f64) -> Histogram<Functional> {
        Histogram {
            values: HashMap::new(),
            count: 0,
            sum: 0,
            bucketing: Functional::new(log_base, buckets_per_magnitude),
        }
    }

    /// Gets a snapshot of all contiguous values.
    ///
    /// **Caution** This is a more specific implementation of `snapshot_values` on functional
    /// histograms. `snapshot_values` cannot be used with those, due to buckets not being
    /// precomputed.
    pub fn snapshot(&self) -> &HashMap<u64, u64> {
        &self.values
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn can_count() {
        let mut hist = Histogram::functional(2.0, 8.0);
        assert!(hist.is_empty());

        for i in 1..=10 {
            hist.accumulate(i);
        }

        assert_eq!(10, hist.count());
        assert_eq!(55, hist.sum());
    }

    #[test]
    fn sample_to_bucket_minimum_correctly_rounds_down() {
        let hist = Histogram::functional(2.0, 8.0);

        // Check each of the first 100 integers, where numerical accuracy of the round-tripping
        // is most potentially problematic
        for value in 0..100 {
            let bucket_minimum = hist.bucketing.sample_to_bucket_minimum(value);
            assert!(bucket_minimum <= value);

            assert_eq!(
                bucket_minimum,
                hist.bucketing.sample_to_bucket_minimum(bucket_minimum)
            );
        }

        // Do an exponential sampling of higher numbers
        for i in 11..500 {
            let value = 1.5f64.powi(i);
            let value = value as u64;
            let bucket_minimum = hist.bucketing.sample_to_bucket_minimum(value);
            assert!(bucket_minimum <= value);
            assert_eq!(
                bucket_minimum,
                hist.bucketing.sample_to_bucket_minimum(bucket_minimum)
            );
        }
    }

    #[test]
    fn histogram_merge() {
        let mut hist = Histogram::functional(2.0, 8.0);

        // Check each of the first 100 integers, where numerical accuracy of the round-tripping
        // is most potentially problematic
        for sample in 0..100 {
            hist.accumulate(sample);
        }

        let mut filled_hist = hist.clone();

        let mut hist2 = Histogram::functional(2.0, 8.0);
        for sample in 100..200 {
            hist.accumulate(sample);
            hist2.accumulate(sample);
        }

        filled_hist.merge(&hist2);

        assert_eq!(filled_hist, hist);
    }

    #[test]
    #[should_panic]
    fn histogram_merge_different_buckets() {
        let mut hist1 = Histogram::functional(2.0, 8.0);
        let hist2 = Histogram::functional(1.0, 4.0);

        hist1.merge(&hist2);
    }

    #[test]
    fn histogram_clears() {
        let mut hist = Histogram::functional(2.0, 8.0);
        let empty_hist = hist.clone();

        assert_eq!(empty_hist, hist);

        hist.accumulate(13);
        assert_ne!(empty_hist, hist);

        hist.clear();
        assert_eq!(empty_hist, hist);
    }
}
