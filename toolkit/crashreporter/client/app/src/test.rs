/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Tests here mostly interact with the [test UI](crate::ui::test). As such, most tests read a bit
//! more like integration tests than unit tests, testing the behavior of the application as a
//! whole.

use super::*;
use crate::config::{test::MINIDUMP_PRUNE_SAVE_COUNT, Config};
use crate::settings::Settings;
use crate::std::{
    ffi::OsString,
    fs::{MockFS, MockFiles, OpenOptions},
    io::ErrorKind,
    mock,
    process::{Command, Output},
    sync::{
        atomic::{AtomicUsize, Ordering::Relaxed},
        Arc,
    },
};
use crate::ui::{self, test::model, ui_impl::Interact};

/// A simple thread-safe counter which can be used in tests to mark that certain code paths were
/// hit.
#[derive(Clone, Default)]
pub struct Counter(Arc<AtomicUsize>);

impl Counter {
    /// Create a new zero counter.
    pub fn new() -> Self {
        Self::default()
    }

    /// Increment the counter.
    pub fn inc(&self) {
        self.0.fetch_add(1, Relaxed);
    }

    /// Get the current count.
    pub fn count(&self) -> usize {
        self.0.load(Relaxed)
    }

    /// Assert that the current count is 1.
    pub fn assert_one(&self) {
        assert_eq!(self.count(), 1);
    }
}

/// Fluent wraps arguments with the unicode BiDi characters.
struct FluentArg<T>(T);

impl<T: std::fmt::Display> std::fmt::Display for FluentArg<T> {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        use crate::std::fmt::Write;
        f.write_char('\u{2068}')?;
        self.0.fmt(f)?;
        f.write_char('\u{2069}')
    }
}

/// Run a gui and interaction on separate threads.
///
/// If the `gui` function returns an error, any panics in the interaction thread are ignored.
fn gui_interact<G, I, R>(gui: G, interact: I) -> anyhow::Result<R>
where
    G: FnOnce() -> anyhow::Result<R>,
    I: FnOnce(&Interact) + Send + 'static,
{
    let mut spawned_interact = Interact::spawn(interact);
    let result = gui();
    if result.is_err() {
        spawned_interact.ignore_panic();
    }
    result
}

const MOCK_MINIDUMP_EXTRA: &str = r#"{
        "Vendor": "FooCorp",
        "ProductName": "Bar",
        "ReleaseChannel": "release",
        "BuildID": "1234",
        "AsyncShutdownTimeout": "{}",
        "StackTraces": {
            "status": "OK"
        },
        "Version": "100.0",
        "ServerURL": "https://reports.example.com",
        "TelemetryServerURL": "https://telemetry.example.com",
        "TelemetryClientId": "telemetry_client",
        "TelemetryProfileGroupId": "telemetry_profile_group",
        "TelemetrySessionId": "telemetry_session",
        "SomeNestedJson": { "foo": "bar" },
        "URL": "https://url.example.com"
    }"#;

fn compact_json(json: &str) -> String {
    let value: serde_json::Value = serde_json::from_str(json).unwrap();
    serde_json::to_string(&value).unwrap()
}

// Actual content doesn't matter, aside from the hash that is generated.
const MOCK_MINIDUMP_FILE: &[u8] = &[1, 2, 3, 4];
const MOCK_MINIDUMP_SHA256: &str =
    "9f64a747e1b97f131fabb6b447296c9b6f0201e79fb3c5356e6c77e89b6a806a";
macro_rules! current_date {
    () => {
        "2004-11-09"
    };
}
const MOCK_CURRENT_DATE: &str = current_date!();
const MOCK_CURRENT_TIME: &str = concat!(current_date!(), "T12:34:56.000Z");
const MOCK_PING_UUID: uuid::Uuid = uuid::Uuid::nil();
const MOCK_REMOTE_CRASH_ID: &str = "8cbb847c-def2-4f68-be9e-000000000000";

fn current_datetime() -> time::OffsetDateTime {
    time::OffsetDateTime::parse(
        MOCK_CURRENT_TIME,
        &time::format_description::well_known::Iso8601::DEFAULT,
    )
    .unwrap()
}

fn current_unix_time() -> i64 {
    current_datetime().unix_timestamp()
}

fn current_system_time() -> ::std::time::SystemTime {
    current_datetime().into()
}

/// A basic configuration which populates some necessary/useful fields.
fn test_config() -> Config {
    let mut cfg = Config::default();
    cfg.data_dir = Some("data_dir".into());
    cfg.events_dir = Some("events_dir".into());
    cfg.ping_dir = Some("ping_dir".into());
    cfg.dump_file = Some("minidump.dmp".into());
    cfg.strings = Some(Default::default());
    // Set delete_dump to true: this matches the default case in practice.
    cfg.delete_dump = true;
    cfg
}

fn init_test_logger() {
    static INIT: std::sync::Once = std::sync::Once::new();
    INIT.call_once(|| {
        env_logger::builder()
            .target(env_logger::Target::Stderr)
            .filter(Some("crashreporter"), log::LevelFilter::Debug)
            .is_test(true)
            .init();
    })
}

/// A test fixture to make configuration, mocking, and assertions easier.
struct GuiTest {
    /// The configuration used in the test. Initialized to [`test_config`].
    pub config: Config,
    /// The mock builder used in the test, initialized with a basic set of mocked values to ensure
    /// most things will work out of the box.
    pub mock: mock::Builder,
    /// The mocked filesystem, which can be used for mock setup and assertions after completion.
    pub files: MockFiles,
    /// Whether glean should be initialized.
    enable_glean: bool,
    /// Callback to call before `try_run` but after test setup.
    before_run: Option<Box<dyn FnOnce()>>,
}

impl GuiTest {
    /// Create a new GuiTest with enough configured for the application to run
    pub fn new() -> Self {
        init_test_logger();

        // Create a default set of files which allow successful operation.
        let mock_files = MockFiles::new();
        mock_files
            .add_file_result(
                "minidump.dmp",
                Ok(MOCK_MINIDUMP_FILE.into()),
                current_system_time(),
            )
            .add_file_result(
                "minidump.extra",
                Ok(compact_json(MOCK_MINIDUMP_EXTRA).into()),
                current_system_time(),
            );

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
                Ok(output)
            }),
        )
        .set(MockFS, mock_files.clone())
        .set(
            crate::std::env::MockCurrentExe,
            "work_dir/crashreporter".into(),
        )
        .set(crate::std::env::MockTempDir, "tmp".into())
        .set(crate::std::time::MockCurrentTime, current_system_time())
        .set(mock::MockHook::new("enable_glean_pings"), false)
        .set(mock::MockHook::new("ping_uuid"), MOCK_PING_UUID);

        GuiTest {
            config: test_config(),
            mock,
            files: mock_files,
            enable_glean: false,
            before_run: None,
        }
    }

    /// Enable glean pings (which will serialize the test run with other glean tests).
    pub fn enable_glean_pings(&mut self) {
        self.enable_glean = true;
        self.mock
            .set(mock::MockHook::new("enable_glean_pings"), true);
    }

    /// Run the given callback after test setup but before running the tests.
    pub fn before_run(&mut self, f: impl FnOnce() + 'static) {
        self.before_run = Some(Box::new(f));
    }

    /// Run the test as configured, using the given function to interact with the GUI.
    ///
    /// Returns the final result of the application logic.
    pub fn try_run<F: FnOnce(&Interact) + Send + 'static>(
        &mut self,
        interact: F,
    ) -> anyhow::Result<bool> {
        let GuiTest {
            ref mut config,
            ref mut mock,
            ref enable_glean,
            ..
        } = self;
        let before_run = self.before_run.take();
        let mut config = Arc::new(std::mem::take(config));

        // Run the mock environment.
        mock.run(move || {
            let _glean = if *enable_glean {
                Some(glean::test_init(&config))
            } else {
                None
            };
            gui_interact(
                move || {
                    if let Some(f) = before_run {
                        f();
                    }
                    try_run(&mut config)
                },
                interact,
            )
        })
    }

    /// Run the test as configured, using the given function to interact with the GUI.
    ///
    /// Panics if the application logic returns an error (which would normally be displayed to the
    /// user).
    pub fn run<F: FnOnce(&Interact) + Send + 'static>(&mut self, interact: F) {
        if let Err(e) = self.try_run(interact) {
            panic!(
                "gui failure:{}",
                e.chain().map(|e| format!("\n  {e}")).collect::<String>()
            );
        }
    }

    /// Get the file assertion helper.
    pub fn assert_files(&self) -> AssertFiles {
        AssertFiles {
            data_dir: "data_dir".into(),
            events_dir: "events_dir".into(),
            inner: self.files.assert_files(),
        }
    }
}

/// A wrapper around the mock [`AssertFiles`](crate::std::fs::AssertFiles).
///
/// This implements higher-level assertions common across tests, but also supports the lower-level
/// assertions (though those return the [`AssertFiles`](crate::std::fs::AssertFiles) reference so
/// higher-level assertions must be chained first).
struct AssertFiles {
    data_dir: String,
    events_dir: String,
    inner: std::fs::AssertFiles,
}

impl AssertFiles {
    fn data(&self, rest: &str) -> String {
        format!("{}/{rest}", &self.data_dir)
    }

    fn events(&self, rest: &str) -> String {
        format!("{}/{rest}", &self.events_dir)
    }

    /// Set the data dir if not the default.
    pub fn set_data_dir<S: ToString>(&mut self, data_dir: S) -> &mut Self {
        let data_dir = data_dir.to_string();
        // Data dir should be relative to root.
        self.data_dir = data_dir.trim_start_matches('/').to_string();
        self
    }

    /// Assert that the crash report was submitted according to the filesystem.
    pub fn submitted(&mut self) -> &mut Self {
        self.inner.check(
            self.data(&format!("submitted/{MOCK_REMOTE_CRASH_ID}.txt")),
            format!("Crash ID: {}\n", FluentArg(MOCK_REMOTE_CRASH_ID)),
        );
        self
    }

    /// Assert that the given settings where saved.
    pub fn saved_settings(&mut self, settings: Settings) -> &mut Self {
        self.inner.check(
            self.data("crashreporter_settings.json"),
            settings.to_string(),
        );
        self
    }

    /// Assert that a crash is pending according to the filesystem. The pending crash will have an
    /// unchanged extra file (due to the crash report not being submitted).
    pub fn pending_unchanged_extra(&mut self) -> &mut Self {
        let dmp = self.data("pending/minidump.dmp");
        self.inner
            .check(
                self.data("pending/minidump.extra"),
                compact_json(MOCK_MINIDUMP_EXTRA),
            )
            .check_bytes(dmp, MOCK_MINIDUMP_FILE);
        self
    }

    /// Assert that a crash is pending according to the filesystem.
    pub fn pending(&mut self) -> &mut Self {
        let dmp = self.data("pending/minidump.dmp");
        self.inner
            .check(
                self.data("pending/minidump.extra"),
                compact_json(MOCK_MINIDUMP_EXTRA),
            )
            .check_bytes(dmp, MOCK_MINIDUMP_FILE);
        self
    }

    /// Assert that a crash is pending according to the filesystem, with updated files.
    pub fn pending_with_change(&mut self, new_dmp: &[u8], new_extra: &str) -> &mut Self {
        let dmp = self.data("pending/minidump.dmp");
        self.inner
            .check(self.data("pending/minidump.extra"), new_extra)
            .check_bytes(dmp, new_dmp);
        self
    }

    /// Assert that a crash ping was created for sending according to the filesystem.
    pub fn ping(&mut self) -> &mut Self {
        self.inner.check(
            format!("ping_dir/{MOCK_PING_UUID}.json"),
            serde_json::json! {{
                "type": "crash",
                "id": MOCK_PING_UUID,
                "version": 4,
                "creationDate": MOCK_CURRENT_TIME,
                "clientId": "telemetry_client",
                "profileGroupId": "telemetry_profile_group",
                "payload": {
                    "sessionId": "telemetry_session",
                    "version": 1,
                    "crashDate": MOCK_CURRENT_DATE,
                    "crashTime": MOCK_CURRENT_TIME,
                    "hasCrashEnvironment": true,
                    "crashId": "minidump",
                    "minidumpSha256Hash": MOCK_MINIDUMP_SHA256,
                    "processType": "main",
                    "stackTraces": {
                        "status": "OK"
                    },
                    "metadata": {
                        "AsyncShutdownTimeout": "{}",
                        "BuildID": "1234",
                        "ProductName": "Bar",
                        "ReleaseChannel": "release",
                        "Version": "100.0",
                    }
                },
                "application": {
                    "vendor": "FooCorp",
                    "name": "Bar",
                    "buildId": "1234",
                    "displayVersion": "",
                    "platformVersion": "",
                    "version": "100.0",
                    "channel": "release"
                }
            }}
            .to_string(),
        );
        self
    }

    /// Assert that a crash submission event was written with the given submission status.
    pub fn submission_event(&mut self, success: bool) -> &mut Self {
        self.inner.check(
            self.events("minidump-submission"),
            format!(
                "crash.submission.1\n\
                {}\n\
                minidump\n\
                {success}\n\
                {}",
                current_unix_time(),
                if success { MOCK_REMOTE_CRASH_ID } else { "" }
            ),
        );
        self
    }
}

impl std::ops::Deref for AssertFiles {
    type Target = std::fs::AssertFiles;
    fn deref(&self) -> &Self::Target {
        &self.inner
    }
}

impl std::ops::DerefMut for AssertFiles {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.inner
    }
}

#[test]
fn error_dialog() {
    gui_interact(
        || {
            let cfg = Config::default();
            ui::error_dialog(Arc::new(cfg), "an error occurred");
            Ok(())
        },
        |interact| {
            interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
        },
    )
    .unwrap();
}

#[test]
fn error_dialog_restart() {
    let mut config = Config::default();
    config.restart_command = Some("my_process".into());
    config.restart_args = vec!["a".into(), "b".into()];
    let ran_process = Counter::new();
    let mock_ran_process = ran_process.clone();
    mock::builder()
        .set(
            Command::mock("my_process"),
            Box::new(move |cmd| {
                assert_eq!(cmd.args, &["a", "b"]);
                mock_ran_process.inc();
                Ok(crate::std::process::success_output())
            }),
        )
        .run(|| {
            gui_interact(
                move || {
                    ui::error_dialog(Arc::new(config), "an error occurred");
                    Ok(())
                },
                |interact| {
                    interact.element("restart", |_style, b: &model::Button| b.click.fire(&()));
                },
            )
        })
        .unwrap();
    ran_process.assert_one();
}

#[test]
fn no_dump_file() {
    let mut cfg = Arc::new(Config::default());
    {
        let cfg = Arc::get_mut(&mut cfg).unwrap();
        cfg.strings = Some(Default::default());
    }
    assert!(try_run(&mut cfg).is_err());
    Arc::get_mut(&mut cfg).unwrap().auto_submit = true;
    assert!(try_run(&mut cfg).is_ok());
}

#[test]
fn dump_file_does_not_exist() {
    let mut test = GuiTest::new();
    test.config.dump_file = Some("does_not_exist.dmp".into());
    test.try_run(|_interact| {})
        .expect_err("the gui should fail with an error");
}

#[test]
fn no_extra_file() {
    mock::builder()
        .set(
            crate::std::env::MockCurrentExe,
            "work_dir/crashreporter".into(),
        )
        .set(MockFS, {
            let files = MockFiles::new();
            files.add_file_result(
                "minidump.extra",
                Err(ErrorKind::NotFound.into()),
                ::std::time::SystemTime::UNIX_EPOCH,
            );
            files
        })
        .run(|| {
            let cfg = test_config();
            assert!(try_run(&mut Arc::new(cfg)).is_err());
        });
}

#[test]
fn auto_submit() {
    let mut test = GuiTest::new();
    test.config.auto_submit = true;
    // auto_submit should not do any GUI things, including creating the crashreporter_settings.json
    // file.
    test.mock.run(|| {
        assert!(try_run(&mut Arc::new(std::mem::take(&mut test.config))).is_ok());
    });
    test.assert_files().submitted();
}

#[test]
fn restart() {
    let mut test = GuiTest::new();
    test.config.restart_command = Some("my_process".into());
    test.config.restart_args = vec!["a".into(), "b".into()];
    let ran_process = Counter::new();
    let mock_ran_process = ran_process.clone();
    test.mock.set(
        Command::mock("my_process"),
        Box::new(move |cmd| {
            assert_eq!(cmd.args, &["a", "b"]);
            mock_ran_process.inc();
            Ok(crate::std::process::success_output())
        }),
    );
    test.run(|interact| {
        interact.element("restart", |_style, b: &model::Button| b.click.fire(&()));
    });
    test.assert_files()
        .saved_settings(Settings::default())
        .submitted();
    ran_process.assert_one();
}

#[test]
fn no_restart_with_windows_error_reporting() {
    let mut test = GuiTest::new();
    test.config.restart_command = Some("my_process".into());
    test.config.restart_args = vec!["a".into(), "b".into()];
    // Keep the files around so we can ensure they match what we expect.
    test.config.delete_dump = false;
    // Add the "WindowsErrorReporting" key to the extra file
    const MINIDUMP_EXTRA_CONTENTS: &str = r#"{
                            "Vendor": "FooCorp",
                            "ProductName": "Bar",
                            "ReleaseChannel": "release",
                            "BuildID": "1234",
                            "StackTraces": {
                                "status": "OK"
                            },
                            "Version": "100.0",
                            "ServerURL": "https://reports.example.com",
                            "TelemetryServerURL": "https://telemetry.example.com",
                            "TelemetryClientId": "telemetry_client",
                            "TelemetryProfileGroupId": "telemetry_profile_group",
                            "TelemetrySessionId": "telemetry_session",
                            "SomeNestedJson": { "foo": "bar" },
                            "URL": "https://url.example.com",
                            "WindowsErrorReporting": "1"
                        }"#;
    test.files = {
        let mock_files = MockFiles::new();
        mock_files
            .add_file_result(
                "minidump.dmp",
                Ok(MOCK_MINIDUMP_FILE.into()),
                current_system_time(),
            )
            .add_file_result(
                "minidump.extra",
                Ok(MINIDUMP_EXTRA_CONTENTS.into()),
                current_system_time(),
            );
        test.mock.set(MockFS, mock_files.clone());
        mock_files
    };
    let ran_process = Counter::new();
    let mock_ran_process = ran_process.clone();
    test.mock.set(
        Command::mock("my_process"),
        Box::new(move |cmd| {
            assert_eq!(cmd.args, &["a", "b"]);
            mock_ran_process.inc();
            Ok(crate::std::process::success_output())
        }),
    );
    test.run(|interact| {
        interact.element("restart", |style, b: &model::Button| {
            // Check that the button is hidden, and invoke the click anyway to ensure the process
            // isn't restarted (the window will still be closed).
            assert_eq!(style.visible.get(), false);
            b.click.fire(&())
        });
    });
    let mut assert_files = test.assert_files();
    assert_files.saved_settings(Settings::default()).submitted();
    {
        let dmp = assert_files.data("pending/minidump.dmp");
        let extra = assert_files.data("pending/minidump.extra");
        assert_files
            .check(extra, compact_json(MINIDUMP_EXTRA_CONTENTS))
            .check_bytes(dmp, MOCK_MINIDUMP_FILE);
    }

    assert_eq!(ran_process.count(), 0);
}

#[test]
fn quit() {
    let mut test = GuiTest::new();
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });
    test.assert_files()
        .saved_settings(Settings::default())
        .submitted();
}

#[test]
fn no_delete_dump() {
    let mut test = GuiTest::new();
    test.config.delete_dump = false;
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });
    test.assert_files()
        .saved_settings(Settings::default())
        .submitted()
        .pending();
}

#[test]
fn no_submit() {
    let mut test = GuiTest::new();
    test.files.add_dir("data_dir").add_file(
        "data_dir/crashreporter_settings.json",
        Settings {
            submit_report: true,
            include_url: false,
            test_hardware: false,
        }
        .to_string(),
    );
    test.run(|interact| {
        interact.element("send", |_style, c: &model::Checkbox| {
            assert!(c.checked.get())
        });
        interact.element("include-url", |_style, c: &model::Checkbox| {
            assert!(!c.checked.get())
        });
        interact.element("send", |_style, c: &model::Checkbox| c.checked.set(false));
        interact.element("include-url", |_style, c: &model::Checkbox| {
            c.checked.set(false)
        });

        // When submission is unchecked, the following elements should be disabled.
        interact.element("details", |style, _: &model::Button| {
            assert!(!style.enabled.get());
        });
        interact.element("comment", |style, _: &model::TextBox| {
            assert!(!style.enabled.get());
        });
        interact.element("include-url", |style, _: &model::Checkbox| {
            assert!(!style.enabled.get());
        });

        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });
    test.assert_files()
        .saved_settings(Settings {
            submit_report: false,
            include_url: false,
            test_hardware: false,
        })
        .pending_unchanged_extra();
}

#[test]
fn ping_and_event_files() {
    let mut test = GuiTest::new();
    test.files
        .add_dir("ping_dir")
        .add_dir("events_dir")
        .add_file(
            "events_dir/minidump",
            "1\n\
         12:34:56\n\
         e0423878-8d59-4452-b82e-cad9c846836e\n\
         {\"foo\":\"bar\"}",
        );
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });
    test.assert_files()
        .saved_settings(Settings::default())
        .submitted()
        .submission_event(true)
        .ping()
        .check(
            "events_dir/minidump",
            format!(
                "1\n\
                12:34:56\n\
                e0423878-8d59-4452-b82e-cad9c846836e\n\
                {}",
                serde_json::json! {{
                    "foo": "bar",
                    "MinidumpSha256Hash": MOCK_MINIDUMP_SHA256,
                    "CrashPingUUID": MOCK_PING_UUID,
                    "StackTraces": { "status": "OK" }
                }}
            ),
        );
}

#[test]
fn network_failure() {
    let invoked = Counter::new();
    let mut test = GuiTest::new();
    test.files
        .add_dir("ping_dir")
        .add_dir("events_dir")
        .add_file(
            "events_dir/minidump",
            "1\n\
         12:34:56\n\
         e0423878-8d59-4452-b82e-cad9c846836e\n\
         {\"foo\":\"bar\"}",
        );
    test.mock.set(
        net::http::MockHttp,
        Box::new(cc! { (invoked) move |_request, _url| {
            invoked.inc();
            Ok(Err(std::io::ErrorKind::HostUnreachable.into()))
        }}),
    );
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });
    assert!(invoked.count() > 0);
    test.assert_files()
        .saved_settings(Settings::default())
        .pending()
        .submission_event(false)
        .ping()
        .check(
            "events_dir/minidump",
            format!(
                "1\n\
                12:34:56\n\
                e0423878-8d59-4452-b82e-cad9c846836e\n\
                {}",
                serde_json::json! {{
                    "foo": "bar",
                    "MinidumpSha256Hash": MOCK_MINIDUMP_SHA256,
                    "CrashPingUUID": MOCK_PING_UUID,
                    "StackTraces": { "status": "OK" }
                }}
            ),
        );
}

#[test]
fn pingsender_failure() {
    let mut test = GuiTest::new();
    test.mock.set(
        Command::mock("work_dir/pingsender"),
        Box::new(|_| Err(ErrorKind::NotFound.into())),
    );
    test.files
        .add_dir("ping_dir")
        .add_dir("events_dir")
        .add_file(
            "events_dir/minidump",
            "1\n\
         12:34:56\n\
         e0423878-8d59-4452-b82e-cad9c846836e\n\
         {\"foo\":\"bar\"}",
        );
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });
    test.assert_files()
        .saved_settings(Settings::default())
        .submitted()
        .submission_event(true)
        .ping()
        .check(
            "events_dir/minidump",
            format!(
                "1\n\
                12:34:56\n\
                e0423878-8d59-4452-b82e-cad9c846836e\n\
                {}",
                serde_json::json! {{
                    "foo": "bar",
                    "MinidumpSha256Hash": MOCK_MINIDUMP_SHA256,
                    // No crash ping UUID since pingsender fails
                    "StackTraces": { "status": "OK" }
                }}
            ),
        );
}

#[test]
fn glean_ping() {
    let mut test = GuiTest::new();
    test.enable_glean_pings();
    let submitted_glean_ping = Counter::new();
    cc! { (submitted_glean_ping)
        test.before_run(move || {
            crate::glean::crash.test_before_next_submit(move |_| {
                submitted_glean_ping.inc();
            });
        })
    };
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });
    submitted_glean_ping.assert_one();
}

#[test]
fn glean_ping_extra_stack_trace_fields() {
    let mut test = GuiTest::new();
    test.enable_glean_pings();
    let submitted_glean_ping = Counter::new();

    const MINIDUMP_EXTRA_CONTENTS: &str = r#"{
                            "Vendor": "FooCorp",
                            "ProductName": "Bar",
                            "ReleaseChannel": "release",
                            "BuildID": "1234",
                            "StackTraces": {
                                "status": "OK",
                                "foobar": "baz",
                                "crash_info": {
                                    "type": "bad crash",
                                    "address": "0xcafe",
                                    "crashing_thread": 1
                                },
                                "main_module": 0,
                                "modules": [{
                                    "base_addr": "0xcafe",
                                    "end_addr": "0xf000",
                                    "code_id": "CODEID",
                                    "debug_file": "debug_file.so",
                                    "debug_id": "DEBUGID",
                                    "filename": "file.so",
                                    "version": "1.0.0"
                                }],
                                "threads": [
                                    {"frames": [
                                        {
                                            "ip": "0xf00",
                                            "trust": "crash",
                                            "module_index": 0
                                        }
                                    ]},
                                    {"frames": [
                                        {
                                            "ip": "0x0",
                                            "trust": "crash",
                                            "module_index": 0
                                        },
                                        {
                                            "ip": "0xbadf00d",
                                            "trust": "cfi",
                                            "module_index": 0
                                        }
                                    ]}
                                ]
                            },
                            "Version": "100.0",
                            "ServerURL": "https://reports.example.com",
                            "TelemetryServerURL": "https://telemetry.example.com",
                            "TelemetryClientId": "telemetry_client",
                            "TelemetryProfileGroupId": "telemetry_profile_group",
                            "TelemetrySessionId": "telemetry_session",
                            "SomeNestedJson": { "foo": "bar" },
                            "URL": "https://url.example.com",
                            "WindowsErrorReporting": "1"
                        }"#;
    test.files = {
        let mock_files = MockFiles::new();
        mock_files
            .add_file_result(
                "minidump.dmp",
                Ok(MOCK_MINIDUMP_FILE.into()),
                current_system_time(),
            )
            .add_file_result(
                "minidump.extra",
                Ok(MINIDUMP_EXTRA_CONTENTS.into()),
                current_system_time(),
            );
        test.mock.set(MockFS, mock_files.clone());
        mock_files
    };

    cc! { (submitted_glean_ping)
        test.before_run(move || {
            glean::crash.test_before_next_submit(move |_| {
                assert_eq!(
                    glean::crash::stack_traces.test_get_value(None),
                    Some(serde_json::json! {{
                        "crash_type": "bad crash",
                        "crash_address":"0xcafe",
                        "crash_thread": 1,
                        "main_module": 0,
                        "modules": [{
                            "base_address": "0xcafe",
                            "end_address": "0xf000",
                            "code_id": "CODEID",
                            "debug_file": "debug_file.so",
                            "debug_id": "DEBUGID",
                            "filename": "file.so",
                            "version": "1.0.0"
                        }],
                        "threads": [
                            {"frames": [
                                {
                                    "module_index": 0,
                                    "ip": "0xf00",
                                    "trust": "crash"
                                },
                            ]},
                            {"frames": [
                                {
                                    "module_index": 0,
                                    "ip": "0x0",
                                    "trust": "crash"
                                },
                                {
                                    "module_index": 0,
                                    "ip": "0xbadf00d",
                                    "trust": "cfi"
                                }
                            ]}
                        ]
                    }})
                );
                submitted_glean_ping.inc();
            });
        })
    };

    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });
    submitted_glean_ping.assert_one();
}

#[test]
fn eol_version() {
    let mut test = GuiTest::new();
    test.files
        .add_dir("data_dir")
        .add_file("data_dir/EndOfLife100.0", "");
    // Should fail before opening the gui
    let result = test.try_run(|_| ());
    assert_eq!(
        result.expect_err("should fail on EOL version").to_string(),
        "Version end of life: crash reports are no longer accepted."
    );
    test.assert_files().ignore("data_dir/EndOfLife100.0");
}

#[test]
fn details_window() {
    let mut test = GuiTest::new();
    test.run(|interact| {
        let details_visible = || {
            interact.window("crash-details-window", |style, _w: &model::Window| {
                style.visible.get()
            })
        };
        assert_eq!(details_visible(), false);
        interact.element("details", |_style, b: &model::Button| b.click.fire(&()));
        assert_eq!(details_visible(), true);
        let details_text = loop {
            let v = interact.element("details-text", |_style, t: &model::TextBox| t.content.get());
            if v == "Loading…" {
                // Wait for the details to be populated.
                std::thread::sleep(std::time::Duration::from_millis(50));
                continue;
            } else {
                break v;
            }
        };
        interact.element("close-details", |_style, b: &model::Button| b.click.fire(&()));
        assert_eq!(details_visible(), false);
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
        assert_eq!(details_text,
            "AsyncShutdownTimeout: {}\n\
             BuildID: 1234\n\
             ProductName: Bar\n\
             ReleaseChannel: release\n\
             SubmittedFrom: Client\n\
             Throttleable: 1\n\
             URL: https://url.example.com\n\
             Vendor: FooCorp\n\
             Version: 100.0\n\
             This report also contains technical information about the state of the application when it crashed.\n"
        );
    });
}

#[test]
fn data_dir_default() {
    let mut test = GuiTest::new();
    test.config.data_dir = None;
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });
    test.assert_files()
        .set_data_dir("data_dir/FooCorp/Bar/Crash Reports")
        .saved_settings(Settings::default())
        .submitted();
}

#[test]
fn include_url() {
    for setting in [false, true] {
        let mut test = GuiTest::new();
        test.files.add_dir("data_dir").add_file(
            "data_dir/crashreporter_settings.json",
            Settings {
                submit_report: true,
                include_url: setting,
                test_hardware: false,
            }
            .to_string(),
        );
        test.mock.set(
            net::report::MockReport,
            Box::new(move |report| {
                assert_eq!(
                    report.extra.get("URL").and_then(|v| v.as_str()),
                    setting.then_some("https://url.example.com")
                );
                Ok(Ok(format!("CrashID={MOCK_REMOTE_CRASH_ID}")))
            }),
        );
        test.run(|interact| {
            interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
        });
    }
}

#[test]
fn persistent_settings() {
    let mut test = GuiTest::new();
    test.run(|interact| {
        interact.element("include-url", |_style, c: &model::Checkbox| {
            c.checked.set(false)
        });
        interact.element("send", |_style, c: &model::Checkbox| c.checked.set(false));
        interact.element("test-hardware", |_style, c: &model::Checkbox| {
            c.checked.set(false)
        });
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });

    test.assert_files()
        .saved_settings(Settings {
            submit_report: false,
            include_url: false,
            test_hardware: false,
        })
        .pending_unchanged_extra();
}

#[test]
fn send_memtest_output() {
    let mut test = GuiTest::new();
    test.config.run_memtest = true;
    let invoked = Counter::new();
    let mock_invoked = invoked.clone();
    test.mock
        .set(
            // memtest is the only expected process spawned using current exe
            Command::mock("work_dir/crashreporter"),
            Box::new(|cmd| assert_mock_memtest(cmd)),
        )
        .set(
            net::report::MockReport,
            Box::new(move |report| {
                mock_invoked.inc();
                assert_eq!(
                    report.extra.get("MemtestOutput").and_then(|v| v.as_str()),
                    Some("memtest output")
                );
                Ok(Ok(format!("CrashID={MOCK_REMOTE_CRASH_ID}")))
            }),
        );
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });

    invoked.assert_one();
}

#[test]
fn add_memtest_output_to_extra() {
    let mut test = GuiTest::new();
    test.config.run_memtest = true;
    test.config.delete_dump = false;
    test.files.add_dir("data_dir").add_file(
        "data_dir/crashreporter_settings.json",
        Settings {
            submit_report: true,
            include_url: true,
            test_hardware: true,
        }
        .to_string(),
    );
    test.mock.set(
        Command::mock("work_dir/crashreporter"),
        Box::new(|cmd| assert_mock_memtest(cmd)),
    );
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });

    let mut value: serde_json::Value = serde_json::from_str(MOCK_MINIDUMP_EXTRA).unwrap();
    value["MemtestOutput"] = "memtest output".into();
    let new_extra = serde_json::to_string(&value).unwrap();

    test.assert_files()
        .saved_settings(Settings {
            submit_report: true,
            include_url: true,
            test_hardware: true,
        })
        .submitted()
        .pending_with_change(MOCK_MINIDUMP_FILE, &new_extra);
}

#[test]
fn toggle_memtest_spawn() {
    let mut test = GuiTest::new();
    test.config.run_memtest = true;
    test.files.add_dir("data_dir").add_file(
        "data_dir/crashreporter_settings.json",
        Settings {
            submit_report: true,
            include_url: false,
            test_hardware: false,
        }
        .to_string(),
    );
    let invoked = Counter::new();
    let mock_invoked = invoked.clone();
    test.mock
        .set(
            Command::mock("work_dir/crashreporter"),
            Box::new(|cmd| assert_mock_memtest(cmd)),
        )
        .set(
            net::report::MockReport,
            Box::new(move |report| {
                mock_invoked.inc();
                assert_eq!(
                    report.extra.get("MemtestOutput").and_then(|v| v.as_str()),
                    Some("memtest output")
                );
                Ok(Ok(format!("CrashID={MOCK_REMOTE_CRASH_ID}")))
            }),
        );
    test.run(|interact| {
        interact.element("test-hardware", |_style, c: &model::Checkbox| {
            c.checked.set(true)
        });
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });

    invoked.assert_one();
}

#[test]
fn toggle_memtest_kill() {
    let mut test = GuiTest::new();
    test.config.run_memtest = true;
    test.files.add_dir("data_dir").add_file(
        "data_dir/crashreporter_settings.json",
        Settings {
            submit_report: true,
            include_url: false,
            test_hardware: true,
        }
        .to_string(),
    );
    let ran_process = Counter::new();
    let mock_ran_process = ran_process.clone();
    test.mock
        .set(
            Command::mock("work_dir/crashreporter"),
            Box::new(move |cmd| {
                // To allow accurate count of ran_process, Early return when spawning
                if cmd.spawning {
                    return Ok(crate::std::process::success_output());
                }
                mock_ran_process.inc();
                assert_mock_memtest(cmd)
            }),
        )
        .set(
            net::report::MockReport,
            Box::new(move |report| {
                assert!(report.extra.get("MemtestOutput").is_none());
                Ok(Ok(format!("CrashID={MOCK_REMOTE_CRASH_ID}")))
            }),
        );
    test.run(|interact| {
        interact.element("test-hardware", |_style, c: &model::Checkbox| {
            c.checked.set(false)
        });
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });

    ran_process.assert_one();
}

fn assert_mock_memtest(cmd: &Command) -> std::io::Result<Output> {
    use ::std::borrow::Borrow;
    assert_eq!(cmd.args.len(), 3);
    assert_eq!(cmd.args[0], "--memtest");
    assert!(cmd.args[1].to_string_lossy().parse::<u32>().is_ok());
    assert!(
        serde_json::from_str::<memtest::RunnerArgs>(cmd.args[2].to_string_lossy().borrow()).is_ok()
    );

    let mut output = crate::std::process::success_output();
    output.stdout = "memtest output".into();
    Ok(output)
}

#[test]
fn comment() {
    const COMMENT: &str = "My program crashed";

    for set_comment in [false, true] {
        let invoked = Counter::new();
        let mock_invoked = invoked.clone();
        let mut test = GuiTest::new();
        test.mock.set(
            net::report::MockReport,
            Box::new(move |report| {
                mock_invoked.inc();
                assert_eq!(
                    report.extra.get("Comments").and_then(|v| v.as_str()),
                    set_comment.then_some(COMMENT)
                );
                Ok(Ok(format!("CrashID={MOCK_REMOTE_CRASH_ID}")))
            }),
        );
        test.run(move |interact| {
            if set_comment {
                interact.element("comment", |_style, c: &model::TextBox| {
                    c.content.set(COMMENT.into())
                });
            }
            interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
        });

        invoked.assert_one();
    }
}

fn platform_path(s: &str) -> String {
    s.replace('/', std::path::MAIN_SEPARATOR_STR)
}

/// Test the interface to the primary network backend (Necko, through a background task).
///
/// This doesn't yet test Glean pings because of reliability issues (see Bug 1937295).
#[test]
fn background_task_network_backend() {
    let mut test = GuiTest::new();
    test.files.add_file("minidump.memory.json.gz", "");
    let ran_process = Counter::new();
    let mock_ran_process = ran_process.clone();
    test.mock.set(
        Command::mock("work_dir/firefox"),
        Box::new(move |cmd| {
            if cmd.spawning {
                return Ok(crate::std::process::success_output());
            }

            mock_ran_process.inc();

            let expected_args: Vec<OsString> = [
                "--backgroundtask",
                "crashreporterNetworkBackend",
                "https://reports.example.com",
                "crashreporter/1.0.0",
            ]
            .into_iter()
            .map(Into::into)
            .collect();

            assert_eq!(cmd.args.len(), 5);
            assert_eq!(cmd.args[..4], expected_args);
            let request_file = &cmd.args[4];

            let expected_contents = serde_json::json!({
                "type": "MimePost",
                "parts": [
                    {
                        "name": "extra",
                        "content": {
                            "type": "String",
                            "value": serde_json::json!({
                                "Vendor":"FooCorp",
                                "ProductName":"Bar",
                                "ReleaseChannel":"release",
                                "BuildID":"1234",
                                "AsyncShutdownTimeout":"{}",
                                "Version":"100.0",
                                "URL":"https://url.example.com",
                                "SubmittedFrom":"Client",
                                "Throttleable":"1"
                            }).to_string(),
                        },
                        "filename": "extra.json",
                        "mime_type": "application/json",
                    },
                    {
                        "name": "upload_file_minidump",
                        "content": {
                            "type": "File",
                            "value": platform_path("data_dir/pending/minidump.dmp"),
                        },
                    },
                    {
                        "name": "memory_report",
                        "content": {
                            "type": "File",
                            "value": platform_path("data_dir/pending/minidump.memory.json.gz"),
                        },
                    }
                ]
            })
            .to_string();

            use ::std::io::{Read, Seek, Write};
            let mut file = OpenOptions::new()
                .read(true)
                .write(true)
                .open(request_file)
                .expect("cannot open request file");
            {
                let mut contents = String::new();
                file.read_to_string(&mut contents)
                    .expect("cannot read request file");
                assert_eq!(contents, expected_contents);
            }

            file.rewind().expect("cannot rewind file");
            file.set_len(0).expect("cannot truncate file");
            file.write_all(format!("CrashID={MOCK_REMOTE_CRASH_ID}").as_bytes())
                .expect("cannot write to request file");

            Ok(crate::std::process::success_output())
        }),
    );
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });

    ran_process.assert_one();

    test.assert_files()
        .saved_settings(Settings::default())
        .submitted();
}

#[test]
fn curl_binary() {
    let mut test = GuiTest::new();
    test.files.add_file("minidump.memory.json.gz", "");
    let ran_process = Counter::new();
    let mock_ran_process = ran_process.clone();
    test.mock.set(
        Command::mock("curl"),
        Box::new(move |cmd| {
            if cmd.spawning {
                return Ok(crate::std::process::success_output());
            }

            // Curl strings need backslashes escaped.
            let curl_escaped_separator = if std::path::MAIN_SEPARATOR == '\\' {
                "\\\\"
            } else {
                std::path::MAIN_SEPARATOR_STR
            };

            let expected_args: Vec<OsString> = [
                "--user-agent",
                net::http::USER_AGENT,
                "--form",
                "extra=@-;filename=extra.json;type=application/json",
                "--form",
                &format!(
                    "upload_file_minidump=@\"data_dir{0}pending{0}minidump.dmp\"",
                    curl_escaped_separator
                ),
                "--form",
                &format!(
                    "memory_report=@\"data_dir{0}pending{0}minidump.memory.json.gz\"",
                    curl_escaped_separator
                ),
                "https://reports.example.com",
            ]
            .into_iter()
            .map(Into::into)
            .collect();
            assert_eq!(cmd.args, expected_args);
            let mut output = crate::std::process::success_output();
            output.stdout = format!("CrashID={MOCK_REMOTE_CRASH_ID}").into();
            mock_ran_process.inc();
            Ok(output)
        }),
    );
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });

    ran_process.assert_one();
}

/// Test that the primary network backend (Necko) falls back to using curl if it fails.
#[test]
fn background_task_curl_fallback() {
    let mut test = GuiTest::new();
    let ran_bgtask = Counter::new();
    let mock_ran_bgtask = ran_bgtask.clone();
    let ran_curl = Counter::new();
    let mock_ran_curl = ran_curl.clone();
    test.mock
        .set(
            Command::mock("work_dir/firefox"),
            Box::new(move |cmd| {
                if cmd.spawning {
                    return Ok(crate::std::process::success_output());
                }
                mock_ran_bgtask.inc();

                let expected_args: Vec<OsString> = [
                    "--backgroundtask",
                    "crashreporterNetworkBackend",
                    "https://reports.example.com",
                    "crashreporter/1.0.0",
                ]
                .into_iter()
                .map(Into::into)
                .collect();

                assert_eq!(cmd.args.len(), 5);
                assert_eq!(cmd.args[..4], expected_args);
                let request_file = &cmd.args[4];

                let expected_contents = serde_json::json!({
                    "type": "MimePost",
                    "parts": [
                        {
                            "name": "extra",
                            "content": {
                                "type": "String",
                                "value": serde_json::json!({
                                    "Vendor":"FooCorp",
                                    "ProductName":"Bar",
                                    "ReleaseChannel":"release",
                                    "BuildID":"1234",
                                    "AsyncShutdownTimeout":"{}",
                                    "Version":"100.0",
                                    "URL":"https://url.example.com",
                                    "SubmittedFrom":"Client",
                                    "Throttleable":"1"
                                }).to_string(),
                            },
                            "filename": "extra.json",
                            "mime_type": "application/json",
                        },
                        {
                            "name": "upload_file_minidump",
                            "content": {
                                "type": "File",
                                "value": platform_path("data_dir/pending/minidump.dmp"),
                            },
                        }
                    ]
                })
                .to_string();

                use ::std::io::Read;
                let mut file = OpenOptions::new()
                    .read(true)
                    .write(true)
                    .open(request_file)
                    .expect("cannot open request file");
                {
                    let mut contents = String::new();
                    file.read_to_string(&mut contents)
                        .expect("cannot read request file");
                    assert_eq!(contents, expected_contents);
                }

                Ok(crate::std::process::Output {
                    status: crate::std::process::exit_status(3),
                    stdout: vec![],
                    stderr: vec![],
                })
            }),
        )
        .set(
            Command::mock("curl"),
            Box::new(move |cmd| {
                if cmd.spawning {
                    return Ok(crate::std::process::success_output());
                }

                // Curl strings need backslashes escaped.
                let curl_escaped_separator = if std::path::MAIN_SEPARATOR == '\\' {
                    "\\\\"
                } else {
                    std::path::MAIN_SEPARATOR_STR
                };

                let expected_args: Vec<OsString> = [
                    "--user-agent",
                    net::http::USER_AGENT,
                    "--form",
                    "extra=@-;filename=extra.json;type=application/json",
                    "--form",
                    &format!(
                        "upload_file_minidump=@\"data_dir{0}pending{0}minidump.dmp\"",
                        curl_escaped_separator
                    ),
                    "https://reports.example.com",
                ]
                .into_iter()
                .map(Into::into)
                .collect();
                assert_eq!(cmd.args, expected_args);
                let mut output = crate::std::process::success_output();
                output.stdout = format!("CrashID={MOCK_REMOTE_CRASH_ID}").into();
                mock_ran_curl.inc();
                Ok(output)
            }),
        );
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });

    ran_bgtask.assert_one();
    ran_curl.assert_one();

    test.assert_files()
        .saved_settings(Settings::default())
        .submitted();
}

#[test]
fn curl_library() {
    let invoked = Counter::new();
    let mock_invoked = invoked.clone();
    let mut test = GuiTest::new();
    test.mock.set(
        net::report::MockReport,
        Box::new(move |_| {
            mock_invoked.inc();
            Ok(Ok(format!("CrashID={MOCK_REMOTE_CRASH_ID}")))
        }),
    );
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });
    invoked.assert_one();
}

#[test]
fn report_not_sent() {
    let mut test = GuiTest::new();
    test.files.add_dir("events_dir");
    test.mock.set(
        net::report::MockReport,
        Box::new(move |_| Err(std::io::ErrorKind::NotFound.into())),
    );
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });

    test.assert_files()
        .saved_settings(Settings::default())
        .submission_event(false)
        .pending();
}

#[test]
fn report_response_failed() {
    let mut test = GuiTest::new();
    test.files.add_dir("events_dir");
    test.mock.set(
        net::report::MockReport,
        Box::new(move |_| Ok(Err(std::io::ErrorKind::NotFound.into()))),
    );
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });

    test.assert_files()
        .saved_settings(Settings::default())
        .submission_event(false)
        .pending();
}

#[test]
fn response_indicates_discarded() {
    let mut test = GuiTest::new();
    // A response indicating discarded triggers a prune of the directory containing the minidump.
    // Since there is one more minidump (the main one, minidump.dmp), pruning should keep all but
    // the first 3, which will be the oldest.
    const SHOULD_BE_PRUNED: usize = 3;

    for i in 0..MINIDUMP_PRUNE_SAVE_COUNT + SHOULD_BE_PRUNED - 1 {
        test.files.add_dir("data_dir/pending").add_file_result(
            format!("data_dir/pending/minidump{i}.dmp"),
            Ok("contents".into()),
            ::std::time::SystemTime::UNIX_EPOCH + ::std::time::Duration::from_secs(1234 + i as u64),
        );
        if i % 2 == 0 {
            test.files
                .add_file(format!("data_dir/pending/minidump{i}.extra"), "{}");
        }
        if i % 5 == 0 {
            test.files
                .add_file(format!("data_dir/pending/minidump{i}.memory.json.gz"), "{}");
        }
    }
    test.mock.set(
        Command::mock("curl"),
        Box::new(|_| {
            let mut output = crate::std::process::success_output();
            output.stdout = format!("CrashID={MOCK_REMOTE_CRASH_ID}\nDiscarded=1").into();
            Ok(output)
        }),
    );
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });

    let mut assert_files = test.assert_files();
    assert_files.saved_settings(Settings::default()).pending();
    for i in SHOULD_BE_PRUNED..MINIDUMP_PRUNE_SAVE_COUNT + SHOULD_BE_PRUNED - 1 {
        assert_files.check_exists(format!("data_dir/pending/minidump{i}.dmp"));
        if i % 2 == 0 {
            assert_files.check_exists(format!("data_dir/pending/minidump{i}.extra"));
        }
        if i % 5 == 0 {
            assert_files.check_exists(format!("data_dir/pending/minidump{i}.memory.json.gz"));
        }
    }
}

#[test]
fn response_view_url() {
    let mut test = GuiTest::new();
    test.mock.set(
        Command::mock("curl"),
        Box::new(|_| {
            let mut output = crate::std::process::success_output();
            output.stdout =
                format!("CrashID={MOCK_REMOTE_CRASH_ID}\nViewURL=https://foo.bar.example").into();
            Ok(output)
        }),
    );
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });

    test.assert_files()
        .saved_settings(Settings::default())
        .check(
            format!("data_dir/submitted/{MOCK_REMOTE_CRASH_ID}.txt"),
            format!(
                "\
                Crash ID: {}\n\
                You can view details of this crash at {}.\n",
                FluentArg(MOCK_REMOTE_CRASH_ID),
                FluentArg("https://foo.bar.example")
            ),
        );
}

#[test]
fn response_stop_sending_reports() {
    let mut test = GuiTest::new();
    test.mock.set(
        Command::mock("curl"),
        Box::new(|_| {
            let mut output = crate::std::process::success_output();
            output.stdout =
                format!("CrashID={MOCK_REMOTE_CRASH_ID}\nStopSendingReportsFor=100.0").into();
            Ok(output)
        }),
    );
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });

    test.assert_files()
        .saved_settings(Settings::default())
        .submitted()
        .check_exists("data_dir/EndOfLife100.0");
}

#[test]
fn rename_failure_uses_copy() {
    let mut test = GuiTest::new();
    test.mock.set(mock::MockHook::new("rename_fail"), true);
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });
    test.assert_files()
        .saved_settings(Settings::default())
        .submitted();
}

/// A real temporary directory in the host filesystem.
///
/// The directory is guaranteed to be unique to the test suite process (in case of crash, it can be
/// inspected).
///
/// When dropped, the directory is deleted.
struct TempDir {
    path: ::std::path::PathBuf,
}

impl TempDir {
    /// Create a new directory with the given identifying name.
    ///
    /// The name should be unique to deconflict amongst concurrent tests.
    pub fn new(name: &str) -> Self {
        let path = ::std::env::temp_dir().join(format!(
            "{}-test-{}-{name}",
            env!("CARGO_PKG_NAME"),
            std::process::id()
        ));
        ::std::fs::create_dir_all(&path).unwrap();
        TempDir { path }
    }

    /// Get the temporary directory path.
    pub fn path(&self) -> &::std::path::Path {
        &self.path
    }
}

impl Drop for TempDir {
    fn drop(&mut self) {
        // Best-effort removal, ignore errors.
        let _ = ::std::fs::remove_dir_all(&self.path);
    }
}

/// A mock crash report server.
///
/// When dropped, the server is shutdown.
struct TestCrashReportServer {
    addr: ::std::net::SocketAddr,
    shutdown_and_thread: Option<(
        tokio::sync::oneshot::Sender<()>,
        std::thread::JoinHandle<()>,
    )>,
}

impl TestCrashReportServer {
    /// Create and start a mock crash report server on an ephemeral port, returning a handle to the
    /// server.
    pub fn run() -> Self {
        let (shutdown, rx) = tokio::sync::oneshot::channel();

        use warp::Filter;

        let submit = warp::path("submit")
            .and(warp::filters::method::post())
            .and(warp::filters::header::header("content-type"))
            .and(warp::filters::body::bytes())
            .and_then(|content_type: String, body: bytes::Bytes| async move {
                let Some(boundary) = content_type.strip_prefix("multipart/form-data; boundary=")
                else {
                    return Err(warp::reject());
                };

                let body = String::from_utf8_lossy(&*body).to_owned();

                for part in body.split(&format!("--{boundary}")).skip(1) {
                    if part == "--\r\n" {
                        break;
                    }

                    let (_headers, _data) = part.split_once("\r\n\r\n").unwrap_or(("", part));
                    // TODO validate parts
                }
                Ok(format!("CrashID={MOCK_REMOTE_CRASH_ID}"))
            });

        let (addr_channel_tx, addr_channel_rx) = std::sync::mpsc::sync_channel(0);

        let thread = ::std::thread::spawn(move || {
            let rt = tokio::runtime::Builder::new_current_thread()
                .enable_all()
                .build()
                .expect("failed to create tokio runtime");
            let _guard = rt.enter();

            let (addr, server) =
                warp::serve(submit).bind_with_graceful_shutdown(([127, 0, 0, 1], 0), async move {
                    rx.await.ok();
                });

            addr_channel_tx.send(addr).unwrap();

            rt.block_on(server)
        });

        let addr = addr_channel_rx.recv().unwrap();

        TestCrashReportServer {
            addr,
            shutdown_and_thread: Some((shutdown, thread)),
        }
    }

    /// Get the url to which to submit crash reports for this mocked server.
    pub fn submit_url(&self) -> String {
        format!("http://{}/submit", self.addr)
    }
}

impl Drop for TestCrashReportServer {
    fn drop(&mut self) {
        let (shutdown, thread) = self.shutdown_and_thread.take().unwrap();
        let _ = shutdown.send(());
        thread.join().unwrap();
    }
}

#[test]
fn real_curl_binary() {
    if ::std::process::Command::new("curl").output().is_err() {
        eprintln!("no curl binary; skipping real_curl_binary test");
        return;
    }

    let server = TestCrashReportServer::run();

    let mut test = GuiTest::new();
    test.mock.set(
        Command::mock("curl"),
        Box::new(|cmd| cmd.output_from_real_command()),
    );
    test.config.report_url = Some(server.submit_url().into());

    // We need the dump file to actually exist since the curl binary is passed the file path.
    // The dump file needs to exist at the pending dir location.

    let tempdir = TempDir::new("real_curl_binary");
    let data_dir = tempdir.path().to_owned();
    let pending_dir = data_dir.join("pending");
    test.config.data_dir = Some(data_dir.clone().into());
    ::std::fs::create_dir_all(&pending_dir).unwrap();
    let dump_file = pending_dir.join("minidump.dmp");
    ::std::fs::write(&dump_file, MOCK_MINIDUMP_FILE).unwrap();

    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });

    test.assert_files()
        .set_data_dir(data_dir.display())
        .saved_settings(Settings::default())
        .submitted();
}

#[test]
fn real_curl_library() {
    if !crate::net::can_load_libcurl() {
        eprintln!("no libcurl; skipping real_libcurl test");
        return;
    }

    let server = TestCrashReportServer::run();

    let mut test = GuiTest::new();
    test.mock
        .set(
            Command::mock("curl"),
            Box::new(|_| Err(std::io::ErrorKind::NotFound.into())),
        )
        .set(mock::MockHook::new("use_system_libcurl"), true);
    test.config.report_url = Some(server.submit_url().into());

    // We need the dump file to actually exist since libcurl is passed the file path.
    // The dump file needs to exist at the pending dir location.

    let tempdir = TempDir::new("real_libcurl");
    let data_dir = tempdir.path().to_owned();
    let pending_dir = data_dir.join("pending");
    test.config.data_dir = Some(data_dir.clone().into());
    ::std::fs::create_dir_all(&pending_dir).unwrap();
    let dump_file = pending_dir.join("minidump.dmp");
    ::std::fs::write(&dump_file, MOCK_MINIDUMP_FILE).unwrap();

    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });

    test.assert_files()
        .set_data_dir(data_dir.display())
        .saved_settings(Settings::default())
        .submitted();
}
