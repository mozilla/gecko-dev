/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Tests in this file are not intended to be run by default, they are just entrypoints for
//! creating minidumps.

use crash_handler::{CrashContext, CrashEvent, CrashEventResult, CrashHandler};

struct TestCrashClient {
    client: minidumper::Client,
}

impl TestCrashClient {
    pub fn new(name: &str) -> Self {
        TestCrashClient {
            client: minidumper::Client::with_name(name)
                .expect("failed to create minidumper client"),
        }
    }
}

unsafe impl CrashEvent for TestCrashClient {
    fn on_crash(&self, context: &CrashContext) -> CrashEventResult {
        CrashEventResult::Handled(self.client.request_dump(context).is_ok())
    }
}

pub fn handle_crashes(name: &str) -> CrashHandler {
    CrashHandler::attach(Box::new(TestCrashClient::new(name)))
        .expect("failed to install crash handler")
}

macro_rules! sadness_test {
    ( $name:ident ) => {
        #[test]
        #[ignore]
        fn $name() {
            let _handler = handle_crashes(stringify!($name));
            unsafe {
                sadness_generator::$name();
            }
        }
    };
}

sadness_test!(raise_abort);
