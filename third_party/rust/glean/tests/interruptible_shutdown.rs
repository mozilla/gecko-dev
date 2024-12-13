// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! This integration test should model how the RLB is used when embedded in another Rust application
//! (e.g. FOG/Firefox Desktop).
//!
//! We write a single test scenario per file to avoid any state keeping across runs
//! (different files run as different processes).

mod common;

use std::io::Read;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::time::Instant;

use crossbeam_channel::{bounded, Sender};
use flate2::read::GzDecoder;
use serde_json::Value as JsonValue;

use glean::net;
use glean::{ConfigurationBuilder, PingRateLimit};

mod metrics {
    #![allow(non_upper_case_globals)]
    use glean::{private::BooleanMetric, CommonMetricData, Lifetime};
    use once_cell::sync::Lazy;

    pub static sample_boolean: Lazy<BooleanMetric> = Lazy::new(|| {
        BooleanMetric::new(CommonMetricData {
            name: "sample_boolean".into(),
            category: "test.metrics".into(),
            send_in_pings: vec!["validation".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        })
    });
}

mod pings {
    use glean::private::PingType;
    use once_cell::sync::Lazy;

    #[allow(non_upper_case_globals)]
    pub static validation: Lazy<PingType> = Lazy::new(|| {
        glean::private::PingType::new(
            "validation",
            true,
            true,
            true,
            true,
            true,
            vec![],
            vec![],
            true,
        )
    });
}

// Define a fake uploader that reports when and what it uploads.
#[derive(Debug)]
struct ReportingUploader {
    calls: AtomicUsize,
    sender: Sender<JsonValue>,
}

impl net::PingUploader for ReportingUploader {
    fn upload(&self, upload_request: net::PingUploadRequest) -> net::UploadResult {
        let calls = self.calls.fetch_add(1, Ordering::SeqCst);
        let body = upload_request.body;
        let decode = |body: Vec<u8>| {
            let mut gzip_decoder = GzDecoder::new(&body[..]);
            let mut s = String::with_capacity(body.len());

            gzip_decoder
                .read_to_string(&mut s)
                .ok()
                .map(|_| &s[..])
                .or_else(|| std::str::from_utf8(&body).ok())
                .and_then(|payload| serde_json::from_str(payload).ok())
                .unwrap()
        };

        match calls {
            // First goes through immediately.
            0 => {
                self.sender.send(decode(body)).unwrap();
                net::UploadResult::http_status(200)
            }
            // Second SHOULD NEVER SEND
            1 => panic!("We should shutdown before getting to the second ping"),
            // Any others ought to be impossible.
            _ => panic!("Wat."),
        }
    }
}

/// Test scenario: We can interrupt a long glean.upload Wait during shutdown.
///
/// The app is initialized, in turn Glean gets initialized without problems.
/// A custom ping is submitted once, triggering ping throttling.
/// It is submitted a second time to convince `glean.upload` to be in a `Wait` task.
/// From this position we ask for Glean to promptly shut down.
/// We expect this to happen reasonably quickly (within 2s) instead of waiting for the
/// entire throttling interval _or_ for uploader_shutdown's last-ditch timeout.
#[test]
fn interruptible_shutdown() {
    common::enable_test_logging();

    // Create a custom configuration to use our reporting uploader.
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().to_path_buf();
    let (tx, rx) = bounded(1);

    let mut cfg = ConfigurationBuilder::new(true, tmpname.clone(), "glean-interruptible-shutdown")
        .with_server_endpoint("invalid-test-host")
        .with_use_core_mps(false)
        .with_uploader(ReportingUploader {
            calls: AtomicUsize::new(0),
            sender: tx,
        })
        .build();
    cfg.rate_limit = Some(PingRateLimit {
        seconds_per_interval: 600, // Needs only be longer than the timeout in `glean::uploader_shutdown()`.
        pings_per_interval: 1,     // throttle thyself immediately.
    });
    common::initialize(cfg);

    // Wait for init to finish,
    // otherwise we might be to quick with calling `shutdown`.
    let _ = metrics::sample_boolean.test_get_value(None);

    // fast
    pings::validation.submit(None);
    // wait for it to be uploaded
    let _body = rx.recv().unwrap();

    // Now we're in throttling territory.
    pings::validation.submit(None);

    // Now we shut down.
    // This should complete really fast because we'll interrupt the `glean.upload` thread
    // from its 600-second Wait task.
    // ...so long as everything's working properly. So let's time it.

    let pre_shutdown = Instant::now();

    glean::shutdown();

    let shutdown_elapsed = pre_shutdown.elapsed();
    assert!(
        shutdown_elapsed.as_secs() < 2,
        "We shouldn't have been slow on shutdown."
    );
}
