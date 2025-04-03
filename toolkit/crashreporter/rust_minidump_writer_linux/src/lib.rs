// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at http://mozilla.org/MPL/2.0/.

//! C FFI interface to the rust-minidump crate

use {
    anyhow::Context,
    libc::pid_t,
    minidump_writer::{crash_context::CrashContext, minidump_writer::MinidumpWriter},
    std::{
        ffi::{c_char, CStr, CString},
        fs::File,
    },
};

#[allow(non_camel_case_types)]
#[cfg(not(target_arch = "arm"))]
type fpregset_t = crash_context::fpregset_t;

// This structure is absent on ARM.
// (We use u8 because it has no alignment requirements and zero-sized types are not FFI-safe)
#[allow(non_camel_case_types)]
#[cfg(target_arch = "arm")]
type fpregset_t = u8;

/// Context gatherer for [`MinidumpWriter`]
///
/// Creates the target minidump file and gathers any context needed for the minidump generation.
pub struct MinidumpWriterContext {
    dump_file: File,
    writer: MinidumpWriter,
}

/// Create the [`MinidumpWriterContext`] object through FFI
///
/// The [`MinidumpWriterContext`] will create the target file specified by `dump_path` and gather
/// context needed for [`MinidumpWriter`] to
/// write the dump.
///
/// Additional context can be added to the dump using functions like
/// [`minidump_writer_set_crash_context()`].
///
/// When ready to dump, [`minidump_writer_dump()`] should be called on the returned object. Failure
/// to do so will result in a memory leak.
///
/// # Return value
///
/// Remember that `Option<Box<T>>` has the same ABI as a `T*` in C, so this function will return a
/// valid `MinidumpWriterContext*` on success, and `nullptr` on failure.
///
/// An optional `char**` can be passed via the `error_msg` parameter to receive a string
/// representing the error message on failure. It must be freed with [`free_minidump_error_msg()`].
///
/// On success, the caller code owns the object, and [`minidump_writer_dump()`] must eventually be
/// called with the returned pointer to avoid a memory leak.
///
/// # Safety
///
/// `dump_path` must be a valid null-terminated C string. `error_msg` must be either a valid
/// pointer or null.
#[no_mangle]
pub unsafe extern "C" fn minidump_writer_create(
    dump_path: *const c_char,
    child: pid_t,
    child_blamed_thread: pid_t,
    error_msg: *mut *mut c_char,
) -> Option<Box<MinidumpWriterContext>> {
    err_to_error_msg(error_msg, || {
        let dump_path = CStr::from_ptr(dump_path)
            .to_str()
            .context("path not valid UTF-8")?;

        let dump_file = std::fs::OpenOptions::new()
            .create(true) // Create file if it doesn't exist
            .truncate(true) // Truncate file
            .write(true)
            .open(dump_path)
            .context("failed to open minidump file")?;

        let writer = MinidumpWriter::new(child, child_blamed_thread);

        Ok(Box::new(MinidumpWriterContext { dump_file, writer }))
    })
}

/// Set the "Crash Context" in the given `writer`
///
/// Adds the `ucontext`, `float_state` (on non-ARM), and `siginfo` (optional)  to the context
/// information for the crash.
///
/// # Panics
///
/// On non-ARM systems, will panic if `float_state` is null. On ARM, will panic if it is non-NULL.
#[no_mangle]
pub extern "C" fn minidump_writer_set_crash_context(
    context: &mut MinidumpWriterContext,
    ucontext: &crash_context::ucontext_t,
    float_state: Option<&fpregset_t>,
    siginfo: Option<&libc::signalfd_siginfo>,
) {
    #[cfg(not(target_arch = "arm"))]
    let float_state = float_state.unwrap().clone();

    #[cfg(target_arch = "arm")]
    assert!(float_state.is_none());

    context.writer.set_crash_context(CrashContext {
        inner: crash_context::CrashContext {
            context: ucontext.clone(),
            #[cfg(not(target_arch = "arm"))]
            float_state,
            siginfo: siginfo
                .cloned()
                .unwrap_or_else(|| unsafe { std::mem::zeroed() }),
            pid: context.writer.process_id,
            tid: context.writer.blamed_thread,
        },
    });
}

/// Write the minidump to the file
///
/// Generates the minidump and writes it out to the file specified when the object was created.
///
/// Consumes the given `writer`, so that same object should never be used again after calling this
/// function.
///
/// Returns a boolean indicating success. `error_msg` can be used to get more info when an error
/// occurs.
///
/// # Safety
///
/// `error_msg` must be either a valid pointer or null.
#[no_mangle]
pub unsafe extern "C" fn minidump_writer_dump(
    mut context: Box<MinidumpWriterContext>,
    error_msg: *mut *mut c_char,
) -> bool {
    err_to_error_msg(error_msg, || {
        context
            .writer
            .dump(&mut context.dump_file)
            .context("failed to write dump file")
    })
    .is_some()
}

/// Free an error returned by any other function in this API
///
/// Failing to call this function on a returned error is a memory leak.
///
/// # Safety
///
/// `error_msg` must be a valid pointer that was previously returned as the `error_msg` of one of
/// the other functions in this API.
#[no_mangle]
pub unsafe extern "C" fn free_minidump_error_msg(error_msg: *mut c_char) {
    // Unused because we just need to drop it
    let _error_msg = CString::from_raw(error_msg);
}

/// Runs a closure and converts any error into a C string
///
/// Wraps any closure that returns an `anyhow::Result<T>` and converts the error result into a C
/// string. Will return `None` if an error occurred and `Some<T>` on success.
unsafe fn err_to_error_msg<F, T>(error_msg: *mut *mut c_char, f: F) -> Option<T>
where
    F: FnOnce() -> anyhow::Result<T>,
{
    match f() {
        Ok(t) => Some(t),
        Err(e) => {
            if !error_msg.is_null() {
                *error_msg = CString::new(e.to_string()).unwrap().into_raw();
            }
            None
        }
    }
}
