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

use crossbeam_channel::bounded;

use crossbeam_channel::Sender;
use crossbeam_channel::TryRecvError;
use flate2::read::GzDecoder;
use glean::net;
use glean::ClientInfoMetrics;
use glean::ConfigurationBuilder;
use pings::nofollows;
use serde_json::Value as JsonValue;

mod pings {
    use super::*;
    use glean::private::PingType;
    use once_cell::sync::Lazy;

    #[allow(non_upper_case_globals)]
    pub static nofollows: Lazy<PingType> = Lazy::new(|| {
        common::PingBuilder::new("nofollows")
            .with_send_if_empty(true)
            .with_include_info_sections(true) // WITH info sections
            .with_enabled(false)
            .with_follows_collection_enabled(false)
            .with_include_client_id(true)
            .build()
    });

    #[allow(non_upper_case_globals)]
    pub static manual: Lazy<PingType> = Lazy::new(|| {
        common::PingBuilder::new("manual")
            .with_send_if_empty(true)
            .build()
    });
}

// Define a fake uploader that reports when and what it uploads.
#[derive(Debug)]
struct ReportingUploader {
    sender: Sender<JsonValue>,
}

impl net::PingUploader for ReportingUploader {
    fn upload(&self, upload_request: net::CapablePingUploadRequest) -> net::UploadResult {
        let upload_request = upload_request.capable(|_| true).unwrap();
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

        self.sender.send(decode(body)).unwrap();
        net::UploadResult::http_status(200)
    }
}

/// Test scenario:
///
/// * Glean has _some_ data already stored.
/// * Glean is started with collection-enabled=false.
///   * Most data is cleared, but not `client_info`` (except `client_id`)
/// * Pings with `follows_collection_enabled=false` still have the `client_info` filled in.
#[test]
fn nofollows_contains_client_info_when_collection_disabled() {
    common::enable_test_logging();

    // Create a custom configuration to use our reporting uploader.
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().to_path_buf();

    // collection-enabled = true
    // Forces database to be created with data, then clears data.
    // Keeps `first_run_date`.
    // Ensures the _next_ init is NOT a first-run.
    let cfg = ConfigurationBuilder::new(true, tmpname.clone(), "glean-fc")
        .with_server_endpoint("invalid-test-host")
        .with_use_core_mps(false)
        .build();
    common::initialize(cfg);
    glean::set_upload_enabled(false);
    glean::shutdown();

    // collection-enabled = false
    let (tx, rx) = bounded(1);
    let cfg = ConfigurationBuilder::new(false, tmpname.clone(), "glean-fc")
        .with_server_endpoint("invalid-test-host")
        .with_use_core_mps(false)
        .with_uploader(ReportingUploader { sender: tx })
        .build();
    // Same as `common::initialize`.
    let client_info = ClientInfoMetrics {
        app_build: "1.0.0".to_string(),
        app_display_version: "1.0.0".to_string(),
        channel: Some("testing".to_string()),
        locale: Some("xx-XX".to_string()),
    };
    glean::test_reset_glean(cfg, client_info, false);

    _ = &*pings::nofollows;
    _ = &*pings::manual;
    nofollows.set_enabled(true);

    pings::manual.submit(None);
    pings::nofollows.submit(None);

    // Wait for the ping to arrive.
    let payload = rx.recv().unwrap();

    let client_info = payload["client_info"].as_object().unwrap();
    // General client info is set
    assert!(client_info["app_build"].is_string());
    assert!(client_info["architecture"].is_string());
    assert!(client_info["os"].is_string());
    assert!(client_info["telemetry_sdk_build"].is_string());
    // No client_id
    assert_eq!(None, client_info.get("client_id"));

    // No second ping received.
    assert!(matches!(rx.try_recv(), Err(TryRecvError::Empty)));

    // Now we enable collection.
    // This should give us a client ID.
    glean::set_collection_enabled(true);

    pings::manual.submit(None);
    let payload = rx.recv().unwrap();
    let client_info = payload["client_info"].as_object().unwrap();
    // General client info is set
    assert!(client_info["app_build"].is_string());
    assert!(client_info["architecture"].is_string());
    assert!(client_info["os"].is_string());
    assert!(client_info["telemetry_sdk_build"].is_string());
    // No client_id
    let client_id = client_info["client_id"].as_str().unwrap();

    pings::nofollows.submit(None);
    let payload = rx.recv().unwrap();
    let client_info = payload["client_info"].as_object().unwrap();
    // General client info is set
    assert!(client_info["app_build"].is_string());
    assert!(client_info["architecture"].is_string());
    assert!(client_info["os"].is_string());
    assert!(client_info["telemetry_sdk_build"].is_string());
    // No client_id
    let nf_client_id = client_info["client_id"].as_str().unwrap();

    assert_eq!(client_id, nf_client_id);

    glean::shutdown();
}
