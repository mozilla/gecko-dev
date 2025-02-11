/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use anyhow::Result;
use std::{ffi::c_char, process};

use crate::CrashHelperClient;
use crash_helper_common::{IPCConnector, Pid};

impl CrashHelperClient {
    pub(crate) fn new(program: *const c_char) -> Result<CrashHelperClient> {
        let pid = CrashHelperClient::spawn_crash_helper(program)?;
        let connector = IPCConnector::connect(process::id() as Pid)?;

        Ok(CrashHelperClient { connector, pid })
    }

    #[allow(unused_variables)]
    fn spawn_crash_helper(program: *const c_char) -> Result<nix::libc::pid_t> {
        #[cfg(any(target_os = "linux", target_os = "macos"))]
        {
            use nix::unistd::{execv, fork, getpid, ForkResult};
            use std::ffi::{CStr, CString};

            let parent_pid = getpid().to_string();
            let parent_pid_arg = unsafe { CString::from_vec_unchecked(parent_pid.into_bytes()) };
            let pid = unsafe { fork() }?;

            // TODO: daemonize the helper by double fork()'ing and waiting on the child
            match pid {
                ForkResult::Child => {
                    let program = unsafe { CStr::from_ptr(program) };
                    let _ = execv(program, &[program, &parent_pid_arg]);

                    // This point should be unreachable, but let's play it safe
                    unsafe { nix::libc::_exit(1) };
                }
                ForkResult::Parent { child } => Ok(child.as_raw()),
            }
        }

        // This is a no-op on Android because we spawn the crash helper as an
        // Android service directly from the Java code, before loading libxul.
        #[cfg(target_os = "android")]
        Ok(0)
    }
}
