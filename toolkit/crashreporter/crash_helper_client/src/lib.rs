/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use anyhow::{bail, Result};
use crash_helper_common::{
    messages::{self},
    BreakpadData, BreakpadString, IPCConnector,
};
use std::{
    ffi::{c_char, CStr, CString, OsString},
    ptr::null_mut,
};
#[cfg(target_os = "windows")]
use windows_sys::Win32::System::Diagnostics::Debug::{CONTEXT, EXCEPTION_RECORD};

extern crate num_traits;

pub use crash_helper_common::{BreakpadChar, BreakpadRawData, Pid};

mod platform;

pub struct CrashHelperClient {
    connector: IPCConnector,
    pid: Pid,
}

impl CrashHelperClient {
    fn send_initialize(
        &self,
        minidump_path: OsString,
        breakpad_data: BreakpadData,
        release_channel: &str,
    ) -> Result<()> {
        let message = messages::Initialize::new(minidump_path, breakpad_data, release_channel);
        self.connector.send_message(&message)?;
        self.connector.recv_reply::<messages::InitializeReply>()?;
        Ok(())
    }

    fn transfer_crash_report(&self, pid: Pid) -> Result<CrashReport> {
        let message = messages::TransferMinidump::new(pid);
        self.connector.send_message(&message)?;

        // HACK: Workaround for a macOS-specific bug
        #[cfg(target_os = "macos")]
        self.connector.poll(nix::poll::PollFlags::POLLIN)?;

        let reply = self
            .connector
            .recv_reply::<messages::TransferMinidumpReply>()?;

        if reply.path.is_empty() {
            // TODO: We should return Result<Option<CrashReport>> instead of
            // this. Semantics would be better once we interact with Rust
            bail!("Minidump for pid {pid:} was not found");
        }

        Ok(CrashReport {
            // SAFETY: path is a valid C string we generated ourselves
            path: unsafe { reply.path.into_raw() },
            error: reply.error.map_or(null_mut(), |error| error.into_raw()),
        })
    }
}

/// Launch the crash helper process, initialize it and connect to it. Returns
/// a pointer to the client connection or `null` upon failure.
///
/// # Safety
///
/// The `helper_name` and `minidump_path` arguments must point to byte or wide
/// strings where appropriate. The `breakpad_raw_data` argument must either
/// point to a string or must contain a valid file descriptor create via
/// `CrashGenerationServer::CreateReportChannel()` depending on the platform.
#[no_mangle]
pub unsafe extern "C" fn crash_helper_launch(
    helper_name: *const BreakpadChar,
    user_app_data_dir: *const BreakpadChar,
    minidump_path: *const BreakpadChar,
    breakpad_raw_data: BreakpadRawData,
    release_channel: *const c_char,
) -> *mut CrashHelperClient {
    if let Ok(crash_helper) = CrashHelperClient::new(helper_name, user_app_data_dir) {
        let minidump_path = <OsString as BreakpadString>::from_ptr(minidump_path);
        let breakpad_data = BreakpadData::new(breakpad_raw_data);
        let release_channel = CStr::from_ptr(release_channel);
        let res = crash_helper.send_initialize(
            minidump_path,
            breakpad_data,
            &release_channel.to_string_lossy(),
        );

        if res.is_err() {
            return null_mut();
        }

        let crash_helper_box = Box::new(crash_helper);
        // The object will be owned by the C++ code from now on, until it is
        // passed back in `crash_helper_shutdown`.
        Box::into_raw(crash_helper_box)
    } else {
        null_mut()
    }
}

/// Shutdown the crash helper and dispose of the client object.
///
/// # Safety
///
/// The `client` parameter must be a valid pointer to the crash helper client
/// object returned by the [`crash_helper_launch()`] function.
#[no_mangle]
pub unsafe extern "C" fn crash_helper_shutdown(client: *mut CrashHelperClient) {
    // The CrashHelperClient object will be automatically destroyed when the
    // contents of this box are automatically dropped at the end of the function
    let _crash_helper_box = Box::from_raw(client);
}

/// Return the pid of the crash helper process.
///
/// # Safety
///
/// The `client` parameter must be a valid pointer to the crash helper client
/// object returned by the [`crash_helper_launch()`] function.
#[no_mangle]
pub unsafe extern "C" fn crash_helper_pid(client: *const CrashHelperClient) -> Pid {
    (*client).pid
}

#[repr(C)]
pub struct CrashReport {
    path: *mut BreakpadChar,
    error: *mut c_char,
}

/// Request the crash report generated for the process associated with `pid`.
/// If the crash report is found an object holding a pointer to the minidump
/// and a potential error message will be returned. Otherwise the function will
/// return `null`.
///
/// # Safety
///
/// The `client` parameter must be a valid pointer to the crash helper client
/// object returned by the [`crash_helper_launch()`] function.
#[no_mangle]
pub unsafe extern "C" fn transfer_crash_report(
    client: *mut CrashHelperClient,
    pid: Pid,
) -> *mut CrashReport {
    let client = client.as_ref().unwrap();
    if let Ok(crash_report) = client.transfer_crash_report(pid) {
        // The object will be owned by the C++ code from now on, until it is
        // passed back in `release_crash_report`.
        Box::into_raw(Box::new(crash_report))
    } else {
        null_mut()
    }
}

/// Release an object obtained via [`transfer_crash_report()`].
///
/// # Safety
///
/// The `crash_report` argument must be a pointer returned by the
/// [`transfer_crash_report()`] function.
#[no_mangle]
pub unsafe extern "C" fn release_crash_report(crash_report: *mut CrashReport) {
    let crash_report = Box::from_raw(crash_report);

    // Release the strings, we just get back ownership of the raw objects and let the drop logic get rid of them
    let _path = <OsString as BreakpadString>::from_raw(crash_report.path);

    if !crash_report.error.is_null() {
        let _error = CString::from_raw(crash_report.error);
    }
}

/// Report an exception to the crash manager that has been captured outside of
/// Firefox processes, typically within the Windows Error Reporting runtime
/// exception module.
///
/// # Safety
///
/// The `exception_record_ptr` and `context_ptr` parameters must point to valid
/// objects of the corresponding types.
#[cfg(target_os = "windows")]
pub unsafe fn report_external_exception(
    main_process_pid: Pid,
    pid: Pid,
    thread: Pid, // TODO: This should be a different type, but it's the same on Windows
    exception_record_ptr: *mut EXCEPTION_RECORD,
    context_ptr: *mut CONTEXT,
) {
    let exception_records = collect_exception_records(exception_record_ptr);
    let context = unsafe { context_ptr.read() };
    let message =
        messages::WindowsErrorReportingMinidump::new(pid, thread, exception_records, context);

    // In the code below we connect to the crash helper, send our message and wait for a reply before returning, but we ignore errors because we can't do anything about them in the calling code
    if let Ok(connector) = IPCConnector::connect(main_process_pid) {
        let _ = connector
            .send_message(&message)
            .and_then(|_| connector.recv_reply::<messages::WindowsErrorReportingMinidumpReply>());
    }
}

// Collect a linked-list of exception records and turn it into a vector
#[cfg(target_os = "windows")]
fn collect_exception_records(
    mut exception_record_ptr: *mut EXCEPTION_RECORD,
) -> Vec<EXCEPTION_RECORD> {
    let mut exception_records = Vec::<EXCEPTION_RECORD>::with_capacity(1);
    loop {
        if exception_record_ptr.is_null() {
            return exception_records;
        }

        let mut exception_record = unsafe { exception_record_ptr.read() };
        exception_record_ptr = exception_record.ExceptionRecord;
        exception_record.ExceptionRecord = null_mut();
        exception_records.push(exception_record);
    }
}
