// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! This integration test should model how the RLB is used when embedded in another Rust application
//! (e.g. FOG/Firefox Desktop).
//!
//! We write a single test scenario per file to avoid any state keeping across runs
//! (different files run as different processes).

mod common;

use crossbeam_channel::{bounded, Sender};

use glean::net;
use glean::ConfigurationBuilder;

mod pings {
    use super::*;
    use glean::private::PingType;
    use once_cell::sync::Lazy;

    #[allow(non_upper_case_globals)]
    pub static no_capabilities: Lazy<PingType> = Lazy::new(|| {
        common::PingBuilder::new("no-capabilities")
            .with_send_if_empty(true)
            .build()
    });

    #[allow(non_upper_case_globals)]
    pub static one_capability: Lazy<PingType> = Lazy::new(|| {
        common::PingBuilder::new("one-capability")
            .with_send_if_empty(true)
            .with_uploader_capabilities(vec!["capability1".to_string()])
            .build()
    });

    #[allow(non_upper_case_globals)]
    pub static two_capabilities: Lazy<PingType> = Lazy::new(|| {
        common::PingBuilder::new("two-capabilities")
            .with_send_if_empty(true)
            .with_uploader_capabilities(vec!["capability1".to_string(), "capability2".to_string()])
            .build()
    });
}

// Define a fake uploader that reports when and what it uploads.
#[derive(Debug)]
struct ReportingUploader {
    sender: Sender<net::UploadResult>,
}

impl net::PingUploader for ReportingUploader {
    fn upload(&self, upload_request: net::CapablePingUploadRequest) -> net::UploadResult {
        let uploader_capabilities: Vec<String> = vec!["capability1".to_string()];

        let Some(_upload_request) = upload_request.capable(|capabilities| {
            capabilities.iter().all(|required_capability| {
                uploader_capabilities
                    .iter()
                    .any(|uploader_capability| uploader_capability == required_capability)
            })
        }) else {
            self.sender.send(net::UploadResult::incapable()).unwrap();
            return net::UploadResult::incapable();
        };

        self.sender
            .send(net::UploadResult::http_status(200))
            .unwrap();
        net::UploadResult::http_status(200)
    }
}

/// Test scenario: We only upload pings we're capable of.
#[test]
fn interruptible_shutdown() {
    common::enable_test_logging();

    // Create a custom configuration to use our reporting uploader.
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().to_path_buf();
    let (tx, rx) = bounded(1);

    let cfg = ConfigurationBuilder::new(true, tmpname.clone(), "glean-interruptible-shutdown")
        .with_server_endpoint("invalid-test-host")
        .with_use_core_mps(false)
        .with_uploader(ReportingUploader { sender: tx })
        .build();
    common::initialize(cfg);

    pings::no_capabilities.submit(None);
    let result = rx.recv().unwrap();
    assert!(
        matches!(result, net::UploadResult::HttpStatus { code: 200 }),
        "Can upload pings requiring no capabilities."
    );

    pings::one_capability.submit(None);
    let result = rx.recv().unwrap();
    assert!(
        matches!(result, net::UploadResult::HttpStatus { code: 200 }),
        "Can upload pings with matching capability."
    );

    pings::two_capabilities.submit(None);
    let result = rx.recv().unwrap();
    assert!(
        matches!(result, net::UploadResult::Incapable { .. }),
        "Can't upload pings requiring capabilities we don't support."
    );
}
