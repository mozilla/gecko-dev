// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! This integration test should model how the RLB is used when embedded in another Rust application
//! (e.g. FOG/Firefox Desktop).
//!
//! We write a single test scenario per file to avoid any state keeping across runs
//! (different files run as different processes).

mod common;

use glean::{ConfigurationBuilder, ErrorType};

/// A timing_distribution
mod metrics {
    use glean::private::*;
    use glean::{Lifetime, TimeUnit};
    use glean_core::CommonMetricData;
    use once_cell::sync::Lazy;

    #[allow(non_upper_case_globals)]
    pub static boo: Lazy<TimingDistributionMetric> = Lazy::new(|| {
        TimingDistributionMetric::new(
            CommonMetricData {
                name: "boo".into(),
                category: "sample".into(),
                send_in_pings: vec!["store1".into()],
                lifetime: Lifetime::Ping,
                disabled: false,
                ..Default::default()
            },
            TimeUnit::Millisecond,
        )
    });
}

/// Test scenario: Ensure buffered accumulation works.
#[test]
fn buffered_timing_distribution_works() {
    common::enable_test_logging();

    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().to_path_buf();

    let cfg = ConfigurationBuilder::new(true, tmpname, "firefox-desktop")
        .with_server_endpoint("invalid-test-host")
        .build();
    common::initialize(cfg);

    let mut buffer = metrics::boo.start_buffer();

    for _ in 0..3 {
        buffer.accumulate(10);
    }

    // Nothing accumulated while buffer uncommitted.
    assert_eq!(None, metrics::boo.test_get_value(None));

    // Commit buffer by dropping it.
    drop(buffer);

    let data = metrics::boo.test_get_value(None).unwrap();
    assert_eq!(3, data.count);
    // 1e6 nanoseconds in a millisecond
    assert_eq!(30 * 1_000_000, data.sum);

    let mut buffer = metrics::boo.start_buffer();
    for _ in 0..3 {
        buffer.accumulate(10);
    }
    // Don't record any of this data into the metric.
    buffer.abandon();

    // Metric is unchanged.
    let data = metrics::boo.test_get_value(None).unwrap();
    assert_eq!(3, data.count);
    assert_eq!(30 * 1_000_000, data.sum);

    // Check error recording.
    let mut buffer = metrics::boo.start_buffer();
    let large_sample = (1000 * 1000 * 1000 * 60 * 10) + 1;
    for _ in 0..10 {
        buffer.accumulate(large_sample);
    }
    drop(buffer);

    let data = metrics::boo.test_get_value(None).unwrap();
    assert_eq!(13, data.count);

    assert_eq!(
        10,
        metrics::boo.test_get_num_recorded_errors(ErrorType::InvalidValue)
    );

    glean::shutdown(); // Cleanly shut down at the end of the test.
}
