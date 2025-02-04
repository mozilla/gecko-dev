/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/******************************************************************************
 * Wrappers used to call into Breakpad code                                   *
 ******************************************************************************/

use std::{
    ffi::{c_char, c_void, OsString},
    ptr::NonNull,
};

use anyhow::{bail, Result};
use crash_helper_common::{BreakpadChar, BreakpadData, BreakpadString};

use crate::crash_generation::BreakpadProcessId;

#[cfg(target_os = "windows")]
type BreakpadInitType = *const u16;
#[cfg(target_os = "macos")]
type BreakpadInitType = *const c_char;
#[cfg(any(target_os = "linux", target_os = "android"))]
type BreakpadInitType = std::os::fd::RawFd;

extern "C" {
    fn CrashGenerationServer_init(
        breakpad_data: BreakpadInitType,
        minidump_path: *const BreakpadChar,
        cb: extern "C" fn(BreakpadProcessId, *const c_char, *const BreakpadChar),
    ) -> *mut c_void;
    fn CrashGenerationServer_shutdown(server: *mut c_void);
}

pub(crate) struct BreakpadCrashGenerator {
    ptr: NonNull<c_void>,
}

// Safety: We own the pointer to the Breakpad C++ CrashGeneration server object
// so we can safely transfer this object to another thread.
unsafe impl Send for BreakpadCrashGenerator {}

// Safety: All mutations to the pointer to the Breakpad C++ CrashGeneration
// server happen within this object meaning it's safe to read it from different
// threads.
unsafe impl Sync for BreakpadCrashGenerator {}

impl BreakpadCrashGenerator {
    pub(crate) fn new(
        breakpad_data: BreakpadData,
        path: OsString,
        finalize_callback: extern "C" fn(BreakpadProcessId, *const c_char, *const BreakpadChar),
    ) -> Result<BreakpadCrashGenerator> {
        let breakpad_raw_data = breakpad_data.into_raw();
        // SAFETY: We're converting a valid string for use within C++ code.
        let path_ptr = unsafe { path.into_raw() };

        // SAFETY: Calling into breakpad code with parameters that have been previously validated.
        let breakpad_server =
            unsafe { CrashGenerationServer_init(breakpad_raw_data, path_ptr, finalize_callback) };

        // Retake ownership of the raw data & strings so we don't leak them.
        let _breakpad_data = BreakpadData::new(breakpad_raw_data);
        // SAFETY: We've allocated this string within this same block.
        let _path = unsafe { <OsString as BreakpadString>::from_raw(path_ptr) };

        if breakpad_server.is_null() {
            bail!("Could not initialize Breakpad crash generator");
        }

        Ok(BreakpadCrashGenerator {
            // SAFETY: We already verified that the pointer is non-null.
            ptr: unsafe { NonNull::new(breakpad_server).unwrap_unchecked() },
        })
    }
}

impl Drop for BreakpadCrashGenerator {
    fn drop(&mut self) {
        // SAFETY: The pointer we're passing is guaranteed to be non-null and
        // valid since we created it ourselves during construction.
        unsafe {
            CrashGenerationServer_shutdown(self.ptr.as_ptr());
        }
    }
}
