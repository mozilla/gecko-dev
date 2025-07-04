/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use anyhow::{bail, Result};
use crash_helper_common::{
    messages::{self},
    AncillaryData, BreakpadString, IPCClientChannel, IPCConnector, ProcessHandle,
    INVALID_ANCILLARY_DATA,
};
#[cfg(any(target_os = "android", target_os = "linux"))]
use minidump_writer::minidump_writer::{AuxvType, DirectAuxvDumpInfo};
#[cfg(target_os = "android")]
use std::os::fd::RawFd;
use std::{
    ffi::{c_char, CString, OsString},
    hint::spin_loop,
    ptr::null_mut,
    sync::{
        atomic::{AtomicBool, Ordering},
        OnceLock,
    },
    thread::{self, JoinHandle},
};
#[cfg(target_os = "windows")]
use windows_sys::Win32::System::Diagnostics::Debug::{CONTEXT, EXCEPTION_RECORD};

extern crate num_traits;

pub use crash_helper_common::{BreakpadChar, BreakpadRawData, Pid};

mod platform;

pub struct CrashHelperClient {
    connector: IPCConnector,
    spawner_thread: Option<JoinHandle<Result<ProcessHandle>>>,
    helper_process: Option<ProcessHandle>,
}

impl CrashHelperClient {
    fn set_crash_report_path(&mut self, path: OsString) -> Result<()> {
        let message = messages::SetCrashReportPath::new(path);
        self.connector.send_message(&message)?;
        Ok(())
    }

    fn register_child_process(&mut self) -> Result<AncillaryData> {
        let ipc_channel = IPCClientChannel::new()?;
        let (server_endpoint, client_endpoint) = ipc_channel.deconstruct();

        if let Some(join_handle) = self.spawner_thread.take() {
            let Ok(process_handle) = join_handle.join() else {
                bail!("The spawner thread failed to execute");
            };

            let Ok(process_handle) = process_handle else {
                bail!("The crash helper process failed to launch");
            };

            self.helper_process = Some(process_handle);
        }

        if self.helper_process.is_none() {
            bail!("The crash helper process is not available");
        };

        let Ok(ancillary_data) = server_endpoint.into_ancillary(&self.helper_process) else {
            bail!("Could not convert the server IPC endpoint");
        };

        let message = messages::RegisterChildProcess::new(ancillary_data);
        self.connector.send_message(&message)?;
        let Ok(ancillary_data) = client_endpoint.into_ancillary(/* dst_process */ &None) else {
            bail!("Could not convert the local IPC endpoint");
        };

        Ok(ancillary_data)
    }

    #[cfg(any(target_os = "android", target_os = "linux"))]
    fn register_auxv_info(&mut self, pid: Pid, auxv_info: DirectAuxvDumpInfo) -> Result<()> {
        let message = messages::RegisterAuxvInfo::new(pid, auxv_info);
        self.connector.send_message(&message)?;
        Ok(())
    }

    #[cfg(any(target_os = "android", target_os = "linux"))]
    fn unregister_auxv_info(&mut self, pid: Pid) -> Result<()> {
        let message = messages::UnregisterAuxvInfo::new(pid);
        self.connector.send_message(&message)?;
        Ok(())
    }

    fn transfer_crash_report(&mut self, pid: Pid) -> Result<CrashReport> {
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
            path: reply.path.into_raw(),
            error: reply.error.map_or(null_mut(), |error| error.into_raw()),
        })
    }
}

/******************************************************************************
 * Main process interface                                                     *
 ******************************************************************************/

/// Launch the crash helper process, initialize it and connect to it. Returns
/// a pointer to the client connection or `null` upon failure.
///
/// # Safety
///
/// The `helper_name` and `minidump_path` arguments must point to byte or wide
/// strings where appropriate. The `breakpad_raw_data` argument must either
/// point to a string or must contain a valid file descriptor create via
/// `CrashGenerationServer::CreateReportChannel()` depending on the platform.
#[cfg(not(target_os = "android"))]
#[no_mangle]
pub unsafe extern "C" fn crash_helper_launch(
    helper_name: *const BreakpadChar,
    breakpad_raw_data: BreakpadRawData,
    minidump_path: *const BreakpadChar,
) -> *mut CrashHelperClient {
    use crash_helper_common::BreakpadData;

    let breakpad_data = BreakpadData::new(breakpad_raw_data);

    if let Ok(crash_helper) = CrashHelperClient::new(helper_name, breakpad_data, minidump_path) {
        let crash_helper_box = Box::new(crash_helper);

        // The object will be owned by the C++ code from now on, until it is
        // passed back in `crash_helper_shutdown`.
        Box::into_raw(crash_helper_box)
    } else {
        null_mut()
    }
}

/// Connect to an already launching crash helper process. This is only available
/// on Android where the crash helper is a service. Returns a pointer to the
/// client connection or `null` upon failure.
///
/// # Safety
///
/// The `minidump_path` argument must point to a valid nul-terminated C string.
/// The `breakpad_raw_data` and `client_socket` arguments must be valid file
/// descriptors used to connect with Breakpad's crash generator and the crash
/// helper respectively..
#[cfg(target_os = "android")]
#[no_mangle]
pub unsafe extern "C" fn crash_helper_connect(client_socket: RawFd) -> *mut CrashHelperClient {
    if let Ok(crash_helper) = CrashHelperClient::new(client_socket) {
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
/// object returned by the [`crash_helper_launch()`] or
/// [`crash_helper_connect()`] functions.
#[no_mangle]
pub unsafe extern "C" fn crash_helper_shutdown(client: *mut CrashHelperClient) {
    // The CrashHelperClient object will be automatically destroyed when the
    // contents of this box are automatically dropped at the end of the function
    let _crash_helper_box = Box::from_raw(client);
}

#[repr(C)]
pub struct CrashReport {
    path: *mut BreakpadChar,
    error: *mut c_char,
}

/// Changes the path where crash reports are generated.
///
/// # Safety
///
/// The `client` parameter must be a valid pointer to the crash helper client
/// object returned by the [`crash_helper_launch()`] or
/// [`crash_helper_connect()`] functions.
#[no_mangle]
pub unsafe extern "C" fn set_crash_report_path(
    client: *mut CrashHelperClient,
    path: *const BreakpadChar,
) -> bool {
    let client = client.as_mut().unwrap();
    let path = <OsString as BreakpadString>::from_ptr(path);
    client.set_crash_report_path(path).is_ok()
}

/// Creates a new IPC channel to connect a soon-to-be-created child process
/// with the crash helper client. The server-side endpoint of this channel
/// will be sent to the crash helper, and the client-side endpoint will be
/// returned.
///
/// This function will return an invalid file handle if creation failed.
///
/// # Safety
///
/// The `client` parameter must be a valid pointer to the crash helper client
/// object returned by the [`crash_helper_launch()`] or
/// [`crash_helper_connect()`] functions.
#[no_mangle]
pub unsafe extern "C" fn register_child_ipc_channel(
    client: *mut CrashHelperClient,
) -> AncillaryData {
    let client = client.as_mut().unwrap();
    if let Ok(client_endpoint) = client.register_child_process() {
        client_endpoint
    } else {
        INVALID_ANCILLARY_DATA
    }
}

/// Request the crash report generated for the process associated with `pid`.
/// If the crash report is found an object holding a pointer to the minidump
/// and a potential error message will be returned. Otherwise the function will
/// return `null`.
///
/// # Safety
///
/// The `client` parameter must be a valid pointer to the crash helper client
/// object returned by the [`crash_helper_launch()`] or
/// [`crash_helper_connect()`] functions.
#[no_mangle]
pub unsafe extern "C" fn transfer_crash_report(
    client: *mut CrashHelperClient,
    pid: Pid,
) -> *mut CrashReport {
    let client = client.as_mut().unwrap();
    if let Ok(crash_report) = client.transfer_crash_report(pid) {
        // The object will be owned by the C++ code from now on, until it is
        // passed back in `release_crash_report`.
        Box::into_raw(Box::new(crash_report))
    } else {
        null_mut()
    }
}

/// Release an object obtained via [`transfer_crash_report()`]
///
/// # Safety
///
/// The `crash_report` argument must be a pointer returned by the
/// [`transfer_crash_report()`] or [`crash_helper_connect()`] functions.
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

    // In the code below we connect to the crash helper, send our message and
    // wait for a reply before returning, but we ignore errors because we
    // can't do anything about them in the calling code.
    let server_addr = crash_helper_common::server_addr(main_process_pid);
    if let Ok(connector) = IPCConnector::connect(&server_addr) {
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

/// Send the auxiliary vector information for the process identified by `pid`
/// to the crash helper.
///
/// # Safety
///
/// The `client` parameter must be a valid pointer to the crash helper client
/// object returned by the [`crash_helper_launch()`] or
/// [`crash_helper_connect()`] functions. The `auxv_info` pointer must be
/// non-null and point to a properly populated `DirectAuxvDumpInfo` structure.
#[cfg(any(target_os = "android", target_os = "linux"))]
#[no_mangle]
pub unsafe extern "C" fn register_child_auxv_info(
    client: *mut CrashHelperClient,
    pid: Pid,
    auxv_info_ptr: *const rust_minidump_writer_linux::DirectAuxvDumpInfo,
) -> bool {
    let client = client.as_mut().unwrap();
    let auxv_info = DirectAuxvDumpInfo {
        program_header_count: (*auxv_info_ptr).program_header_count as AuxvType,
        program_header_address: (*auxv_info_ptr).program_header_address as AuxvType,
        linux_gate_address: (*auxv_info_ptr).linux_gate_address as AuxvType,
        entry_address: (*auxv_info_ptr).entry_address as AuxvType,
    };

    client.register_auxv_info(pid, auxv_info).is_ok()
}

/// Deregister previously sent auxiliary vector information for the process
/// identified by `pid`.
///
/// # Safety
///
/// The `client` parameter must be a valid pointer to the crash helper client
/// object returned by the [`crash_helper_launch()`] or
/// [`crash_helper_connect()`] functions.
#[cfg(any(target_os = "android", target_os = "linux"))]
#[no_mangle]
pub unsafe extern "C" fn unregister_child_auxv_info(
    client: *mut CrashHelperClient,
    pid: Pid,
) -> bool {
    let client = client.as_mut().unwrap();
    client.unregister_auxv_info(pid).is_ok()
}

/******************************************************************************
 * Child process interface                                                    *
 ******************************************************************************/

// This contains the raw IPC endpoint that a child will use to reach out to
// the crash helper process. We explicitly store it in its raw form rather
// than as an IPCConnector because the latter is neither thread-safe nor
// signal/exception-safe. We will access this endpoint only from within the
// exception handler with bare syscalls so we can leave the `IPCConnector`
// object behind.
static CHILD_IPC_ENDPOINT: OnceLock<Box<AncillaryData>> = OnceLock::new();
static RENDEZVOUS_FAILED: AtomicBool = AtomicBool::new(false);

/// Let a client rendez-vous with the crash helper process. This step ensures
/// the crash helper will be able to dump the calling child. This will also
/// serve additional functionality in the future.
///
/// # Safety
///
/// This function is safe to use if the `client_endpoint` parameter contains
/// a valid pipe handle (on Windows) or a valid file descriptor (on all other
/// platforms).
#[no_mangle]
pub unsafe extern "C" fn crash_helper_rendezvous(client_endpoint: AncillaryData) {
    let Ok(connector) = IPCConnector::from_ancillary(client_endpoint) else {
        RENDEZVOUS_FAILED.store(true, Ordering::Relaxed);
        return;
    };

    let join_handle = thread::spawn(move || {
        if let Ok(message) = connector.recv_reply::<messages::ChildProcessRegistered>() {
            CrashHelperClient::prepare_for_minidump(message.crash_helper_pid);
            assert!(
                CHILD_IPC_ENDPOINT
                    .set(Box::new(connector.into_ancillary(&None).unwrap()))
                    .is_ok(),
                "The crash_helper_rendezvous() function must only be called once"
            );
        }

        RENDEZVOUS_FAILED.store(true, Ordering::Relaxed);
    });

    // If we couldn't spawn a thread the join handle will be already marked as
    // finished, check for this and flag the rendez-vous as failed. Don't wait
    // for the thread though, other failures will be dealt with within the
    // thread itself.
    if join_handle.is_finished() && join_handle.join().is_err() {
        RENDEZVOUS_FAILED.store(true, Ordering::Relaxed);
    }
}

/// Ensure that the rendez-vous with the crash helper has happened. This method
/// can be called safely from within an exception handler.
///
/// # Safety
///
/// It is always safe to call this function. It's safe even from within an
/// exception handler.
#[no_mangle]
pub unsafe extern "C" fn crash_helper_wait_for_rendezvous() {
    while CHILD_IPC_ENDPOINT.get().is_none() {
        if RENDEZVOUS_FAILED.load(Ordering::Relaxed) {
            break;
        }

        spin_loop();
    }
}
