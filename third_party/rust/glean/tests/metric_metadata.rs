// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! This integration test should model how the RLB is used when embedded in another Rust application
//! (e.g. FOG/Firefox Desktop).
//!
//! We write a single test scenario per file to avoid any state keeping across runs
//! (different files run as different processes).

mod common;

/// Some user metrics.
mod metrics {
    use glean::private::*;
    use glean::traits;
    use glean::CommonMetricData;
    use once_cell::sync::Lazy;
    use std::collections::HashMap;

    pub struct SomeExtras {
        extra1: Option<String>,
        extra2: Option<bool>,
    }

    impl traits::ExtraKeys for SomeExtras {
        const ALLOWED_KEYS: &'static [&'static str] = &["extra1", "extra2"];

        fn into_ffi_extra(self) -> HashMap<String, String> {
            let mut map = HashMap::new();

            self.extra1
                .and_then(|val| map.insert("extra1".to_string(), val));
            self.extra2
                .and_then(|val| map.insert("extra2".to_string(), val.to_string()));

            map
        }
    }

    #[allow(non_upper_case_globals)]
    pub static countit: Lazy<CounterMetric> = Lazy::new(|| {
        CounterMetric::new(CommonMetricData {
            name: "count_von_count".into(),
            category: "sesame".into(),
            send_in_pings: vec!["validation".into()],
            dynamic_label: Some("ah_ah_ah".into()),
            ..Default::default()
        })
    });

    #[allow(non_upper_case_globals)]
    pub static event: Lazy<EventMetric<SomeExtras>> = Lazy::new(|| {
        EventMetric::new(CommonMetricData {
            name: "birthday".into(),
            category: "shire".into(),
            send_in_pings: vec!["validation".into()],
            dynamic_label: Some("111th".into()),
            ..Default::default()
        })
    });

    #[allow(non_upper_case_globals)]
    pub static object: Lazy<ObjectMetric<i32>> = Lazy::new(|| {
        ObjectMetric::new(CommonMetricData {
            name: "objection".into(),
            category: "court".into(),
            send_in_pings: vec!["validation".into()],
            ..Default::default()
        })
    });
}

/// Test scenario: We have an unknown metric, and wish to get metadata about the metric.
///
/// The app is initialized, in turn Glean gets initialized without problems.
/// We retrieve information about the metric.
/// And later the whole process is shutdown.
#[test]
fn check_metadata() {
    use glean::MetricIdentifier;
    common::enable_test_logging();

    // Pick the counter metric to test the blanket implementation of
    // MetricIdentifier for types that implement MetricType
    let (category, name, label) = metrics::countit.get_identifiers();
    assert_eq!(category, "sesame");
    assert_eq!(name, "count_von_count");
    assert_eq!(label, Some("ah_ah_ah"));

    // Events and Objects have MetricIdentifier implemented explicitly, as
    // they wrap the glean-core Event and Object types
    let (category, name, label) = metrics::event.get_identifiers();
    assert_eq!(category, "shire");
    assert_eq!(name, "birthday");
    assert_eq!(label, Some("111th"));

    let (category, name, label) = metrics::object.get_identifiers();
    assert_eq!(category, "court");
    assert_eq!(name, "objection");
    assert_eq!(label, None);

    glean::shutdown();
}
