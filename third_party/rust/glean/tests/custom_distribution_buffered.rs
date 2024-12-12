// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! This integration test should model how the RLB is used when embedded in another Rust application
//! (e.g. FOG/Firefox Desktop).
//!
//! We write a single test scenario per file to avoid any state keeping across runs
//! (different files run as different processes).

mod common;

use glean::ConfigurationBuilder;

mod metrics {
    use glean::private::*;
    use glean::{HistogramType, Lifetime};
    use glean_core::CommonMetricData;
    use once_cell::sync::Lazy;

    #[allow(non_upper_case_globals)]
    pub static measure: Lazy<CustomDistributionMetric> = Lazy::new(|| {
        CustomDistributionMetric::new(
            CommonMetricData {
                name: "measure".into(),
                category: "sample".into(),
                send_in_pings: vec!["store1".into()],
                lifetime: Lifetime::Ping,
                disabled: false,
                ..Default::default()
            },
            0,
            100,
            100,
            HistogramType::Linear,
        )
    });
}

/// Test scenario: Ensure buffered accumulation works.
#[test]
fn buffered_memory_distribution_works() {
    common::enable_test_logging();

    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().to_path_buf();

    let cfg = ConfigurationBuilder::new(true, tmpname, "firefox-desktop")
        .with_server_endpoint("invalid-test-host")
        .build();
    common::initialize(cfg);

    let mut buffer = metrics::measure.start_buffer();

    for _ in 0..3 {
        buffer.accumulate(10);
    }

    // Nothing accumulated while buffer uncommitted.
    assert_eq!(None, metrics::measure.test_get_value(None));

    // Commit buffer by dropping it.
    drop(buffer);

    let data = metrics::measure.test_get_value(None).unwrap();
    assert_eq!(3, data.count);
    assert_eq!(30, data.sum);

    let mut buffer = metrics::measure.start_buffer();
    for _ in 0..3 {
        buffer.accumulate(10);
    }
    // Don't record any of this data into the metric.
    buffer.abandon();

    // Metric is unchanged.
    let data = metrics::measure.test_get_value(None).unwrap();
    assert_eq!(3, data.count);
    assert_eq!(30, data.sum);

    glean::shutdown(); // Cleanly shut down at the end of the test.
}
