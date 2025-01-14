/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! The crash reporter application.
//!
//! # Architecture
//! The application uses a simple declarative [UI model](ui::model) to define the UI. This model
//! contains [data bindings](data) which provide the dynamic behaviors of the UI. Separate UI
//! implementations for linux (gtk), macos (cocoa), and windows (win32) exist, as well as a test UI
//! which is virtual (no actual interface is presented) but allows runtime introspection.
//!
//! # Mocking
//! This application contains mock interfaces for all the `std` functions it uses which interact
//! with the host system. You can see their implementation in [`crate::std`]. To enable mocking,
//! use the `mock` feature or build with `MOZ_CRASHREPORTER_MOCK` set (which, in `build.rs`, is
//! translated to a `cfg` option). *Note* that this cfg _must_ be enabled when running tests.
//! Unfortunately it is not possible to detect whether tests are being built in `build.rs, which
//! is why a feature needed to be made in the first place (it is enabled automatically when running
//! `mach rusttests`).
//!
//! Currently the input program configuration which is mocked when running the application is fixed
//! (see the [`main`] implementation in this file). If needed in the future, it would be nice to
//! extend this to allow runtime tweaking.
//!
//! # Development
//! Because of the mocking support previously mentioned, in generally any `std` imports should
//! actually use `crate::std`. If mocked functions/types are missing, they should be added with
//! appropriate mocking hooks.

// Use the WINDOWS windows subsystem. This prevents a console window from opening with the
// application.
#![cfg_attr(windows, windows_subsystem = "windows")]

use crate::std::sync::Arc;
use config::Config;

// A few macros are defined here to allow use in all submodules via textual scope lookup.

/// cc is short for Clone Capture, a shorthand way to clone a bunch of values before an expression
/// (particularly useful for closures).
macro_rules! cc {
    ( ($($c:ident),*) $e:expr ) => {
        {
            $(let $c = $c.clone();)*
            $e
        }
    }
}

/// Create a string literal to be used as an environment variable name.
///
/// This adds the application prefix `MOZ_CRASHREPORTER_`.
macro_rules! ekey {
    ( $name:literal ) => {
        concat!("MOZ_CRASHREPORTER_", $name)
    };
}

mod analyze;
mod async_task;
mod config;
mod data;
mod glean;
mod lang;
mod logging;
mod logic;
mod net;
mod process;
mod settings;
mod std;
mod thread_bound;
mod ui;

#[cfg(test)]
mod test;

fn main() {
    // Determine the mode in which to run. This is very simplistic, but need not be more permissive
    // nor flexible since we control how the program is invoked.
    if std::env::args_os()
        .nth(1)
        .map(|s| s == "--analyze")
        .unwrap_or(false)
    {
        analyze::main()
    } else {
        report_main()
    }
}

#[cfg(not(mock))]
fn report_main() {
    let log_target = logging::init();

    let mut config = Config::new();
    config.read_from_environment();
    config.log_target = Some(log_target);

    let mut config = Arc::new(config);

    match try_run(&mut config) {
        Ok(attempted_send) => {
            if !attempted_send {
                // Exited without attempting to send the crash report; delete files.
                config.delete_files();
            }
        }
        Err(message) => {
            // TODO maybe errors should also delete files?
            log::error!("exiting with error: {message}");
            if !config.auto_submit {
                // Only show a dialog if auto_submit is disabled.
                ui::error_dialog(&config, message);
            }
            std::process::exit(1);
        }
    }
}

#[cfg(mock)]
fn report_main() {
    // TODO it'd be nice to be able to set these values at runtime in some way when running the
    // mock application.

    use crate::std::{
        fs::{MockFS, MockFiles},
        mock,
        process::Command,
    };
    const MOCK_MINIDUMP_EXTRA: &str = r#"{
                            "Vendor": "FooCorp",
                            "ProductName": "Bar",
                            "ReleaseChannel": "release",
                            "BuildID": "1234",
                            "StackTraces": {
                                "status": "OK"
                            },
                            "Version": "100.0",
                            "ServerURL": "https://reports.example",
                            "TelemetryServerURL": "https://telemetry.example",
                            "TelemetryClientId": "telemetry_client",
                            "TelemetryProfileGroupId": "telemetry_profile_group",
                            "TelemetrySessionId": "telemetry_session",
                            "URL": "https://url.example"
                        }"#;

    // Actual content doesn't matter, aside from the hash that is generated.
    const MOCK_MINIDUMP_FILE: &[u8] = &[1, 2, 3, 4];
    const MOCK_CURRENT_TIME: &str = "2004-11-09T12:34:56Z";
    const MOCK_PING_UUID: uuid::Uuid = uuid::Uuid::nil();
    const MOCK_REMOTE_CRASH_ID: &str = "8cbb847c-def2-4f68-be9e-000000000000";

    // Initialize logging but don't set it in the configuration, so that it won't be redirected to
    // a file (only shown on stderr).
    logging::init();

    // Create a default set of files which allow successful operation.
    let mock_files = MockFiles::new();
    mock_files
        .add_file("minidump.dmp", MOCK_MINIDUMP_FILE)
        .add_file("minidump.extra", MOCK_MINIDUMP_EXTRA);

    // Create a default mock environment which allows successful operation.
    let mut mock = mock::builder();
    mock.set(
        Command::mock("work_dir/pingsender"),
        Box::new(|_| Ok(crate::std::process::success_output())),
    )
    .set(
        Command::mock("curl"),
        Box::new(|_| {
            let mut output = crate::std::process::success_output();
            output.stdout = format!("CrashID={MOCK_REMOTE_CRASH_ID}").into();
            // Network latency.
            std::thread::sleep(std::time::Duration::from_secs(2));
            Ok(output)
        }),
    )
    .set(MockFS, mock_files.clone())
    .set(
        crate::std::env::MockCurrentExe,
        "work_dir/crashreporter".into(),
    )
    .set(
        crate::std::time::MockCurrentTime,
        time::OffsetDateTime::parse(
            MOCK_CURRENT_TIME,
            &time::format_description::well_known::Iso8601::DEFAULT,
        )
        .unwrap()
        .into(),
    )
    .set(mock::MockHook::new("ping_uuid"), MOCK_PING_UUID)
    .set(mock::MockHook::new("enable_glean_pings"), false);

    let result = mock.run(|| {
        let mut cfg = Config::new();
        cfg.data_dir = Some("data_dir".into());
        cfg.events_dir = Some("events_dir".into());
        cfg.ping_dir = Some("ping_dir".into());
        cfg.dump_file = Some("minidump.dmp".into());
        cfg.restart_command = Some("mockfox".into());
        cfg.strings = Some(lang::load());

        let mut cfg = Arc::new(cfg);
        try_run(&mut cfg)
    });

    if let Err(e) = result {
        log::error!("exiting with error: {e}");
        std::process::exit(1);
    }
}

fn try_run(config: &mut Arc<Config>) -> anyhow::Result<bool> {
    if config.dump_file.is_none() {
        if !config.auto_submit {
            Err(anyhow::anyhow!(config.string("crashreporter-information")))
        } else {
            Ok(false)
        }
    } else {
        // Use minidump-analyzer to gather stack traces.
        #[cfg(not(mock))]
        {
            if let Err(e) = minidump_analyzer::MinidumpAnalyzer::new(config.dump_file())
                .all_threads(config.dump_all_threads)
                .analyze()
            {
                // Minidump analysis gives optional additional information; if it fails, we should
                // still proceed.
                log::warn!("minidump analyzer failed: {e}");
            }
        }

        let extra = {
            // Perform a few things which may change the config, then treat it as immutable.
            let config = Arc::get_mut(config).expect("unexpected config references");
            let extra = config.load_extra_file()?;
            config.move_crash_data_to_pending()?;
            extra
        };

        // Since Glean v63.0.0, custom pings are required to be instantiated prior to Glean init
        // in order to ensure they are enabled and able to collect data. This is due to the data
        // collection state being determined at the ping level now instead of just by the global
        // Glean collection enabled flag. See Bug 1934931 for more information.
        _ = &*glean::crash;

        // Initialize glean here since it relies on the data directory (which will not change after
        // this point). We could potentially initialize it even later (only just before we need
        // it), however we may use it for more than just the crash ping in the future, in which
        // case it makes more sense to do it as early as possible.
        //
        // When we are testing, glean will already be initialized (if needed).
        #[cfg(not(test))]
        glean::init(&config);

        logic::ReportCrash::new(config.clone(), extra)?.run()
    }
}
