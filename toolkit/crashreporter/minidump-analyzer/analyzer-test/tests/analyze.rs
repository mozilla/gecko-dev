/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use std::fs::File;
use std::io::{Read, Write};
use std::path::{Path, PathBuf};
use std::process::{Child, Command};
use tempfile::tempdir;

/// Child failure types.
///
/// These must correspond to tests defined in `client.rs` (per the `Display` implementation).
#[derive(Debug, Clone, Copy)]
enum FailureType {
    RaiseAbort,
}

impl FailureType {
    pub fn test_name(&self) -> &str {
        match self {
            FailureType::RaiseAbort => "raise_abort",
        }
    }
}

impl AsRef<std::ffi::OsStr> for FailureType {
    fn as_ref(&self) -> &std::ffi::OsStr {
        self.test_name().as_ref()
    }
}

impl std::fmt::Display for FailureType {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        f.write_str(self.test_name())
    }
}

fn start_child(failure: FailureType) -> Child {
    Command::new("cargo")
        .args([
            "test",
            "--package",
            env!("CARGO_PKG_NAME"),
            "--test",
            "client",
            "--",
            "--include-ignored",
        ])
        .arg(failure)
        .stdout(std::process::Stdio::null())
        .stderr(std::process::Stdio::null())
        .spawn()
        .expect("failed to execute child")
}

// XXX: minidumper is somewhat slow to establish the connection, making the test slow.
fn write_minidump(minidump_file: &Path, failure: FailureType) {
    use minidumper::{LoopAction, MinidumpBinary, Server, ServerHandler};
    use std::sync::atomic::{AtomicBool, Ordering::Relaxed};

    struct Handler {
        minidump_file: PathBuf,
    }

    impl ServerHandler for Handler {
        fn create_minidump_file(&self) -> std::io::Result<(File, PathBuf)> {
            let f = File::create(&self.minidump_file)?;
            let p = self.minidump_file.clone();
            Ok((f, p))
        }

        fn on_minidump_created(
            &self,
            result: Result<MinidumpBinary, minidumper::Error>,
        ) -> LoopAction {
            result.expect("failed to write minidump");
            LoopAction::Exit
        }

        fn on_message(&self, _kind: u32, _buffer: Vec<u8>) {}
    }

    let mut server =
        Server::with_name(&failure.to_string()).expect("failed to create minidumper server");
    let mut child = start_child(failure);

    /// Maximum time we want to wait for the child to execute and crash.
    const CHILD_TIMEOUT: std::time::Duration = std::time::Duration::from_secs(10);

    // Run the server.
    let shutdown = AtomicBool::default();
    std::thread::scope(|s| {
        let handle = s.spawn(|| {
            std::thread::park_timeout(CHILD_TIMEOUT);
            shutdown.store(true, Relaxed);
        });
        server
            .run(
                Box::new(Handler {
                    minidump_file: minidump_file.into(),
                }),
                &shutdown,
                None,
            )
            .expect("minidumper server failure");
        handle.thread().unpark();
    });
    drop(child.kill());
    if !minidump_file.exists() {
        // Likely a timeout occurred
        panic!("expected child process to crash within {:?}", CHILD_TIMEOUT);
    }
}

#[test]
fn analyze_basic_minidump() {
    let dir = tempdir().expect("failed to create temporary directory");
    let minidump_file = dir.path().join("mini.dump");
    let extra_file = dir.path().join("mini.extra");

    let Some(analyzer) = std::env::var_os("MINIDUMP_ANALYZER") else {
        panic!("Specify the path to the minidump analyzer binary as the MINIDUMP_ANALYZER environment variable.");
    };

    // Create minidump from test.
    write_minidump(&minidump_file, FailureType::RaiseAbort);

    // Create empty extra file
    {
        let mut extra = File::create(&extra_file).expect("failed to create extra json file");
        write!(&mut extra, "{{}}").expect("failed to write to extra json file");
    }

    // Run minidump-analyzer
    {
        let output = Command::new(analyzer)
            .env("RUST_BACKTRACE", "1")
            .arg(&minidump_file)
            .output()
            .expect("failed to run minidump-analyzer");
        assert!(
            output.status.success(),
            "stderr:\n{}",
            std::str::from_utf8(&output.stderr).unwrap()
        );
    }

    // Check the output JSON
    // The stack trace will actually be in cargo. It forks and execs the test program; there is no
    // clean way to make it just exec one or to directly address the binary (without creating a new
    // crate).
    {
        let mut extra_content = String::new();
        File::open(extra_file)
            .expect("failed to open extra json file")
            .read_to_string(&mut extra_content)
            .expect("failed to read extra json file");

        let extra = json::parse(&extra_content).expect("failed to parse extra json");
        let stack_traces = &extra["StackTraces"];
        assert!(stack_traces.is_object());
        let threads = &stack_traces["threads"];
        assert!(threads.is_array() && threads.len() == 1);
        assert!(threads[0].is_object());
        let frames = &threads[0]["frames"];
        assert!(frames.is_array() && !frames.is_empty());
    }
}
