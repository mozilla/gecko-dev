/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use anyhow::Result;
use crash_helper_common::{
    ignore_eintr, BreakpadChar, BreakpadData, IPCChannel, IPCConnector, IPCListener,
};
use nix::{
    sys::wait::waitpid,
    unistd::{execv, fork, getpid, setsid, ForkResult},
};
use std::ffi::{CStr, CString};

use crate::CrashHelperClient;

impl CrashHelperClient {
    pub(crate) fn new(
        program: *const BreakpadChar,
        breakpad_data: BreakpadData,
        minidump_path: *const BreakpadChar,
    ) -> Result<CrashHelperClient> {
        let channel = IPCChannel::new()?;
        let (listener, server_endpoint, client_endpoint) = channel.deconstruct();
        CrashHelperClient::spawn_crash_helper(
            program,
            breakpad_data,
            minidump_path,
            listener,
            server_endpoint,
        )?;

        Ok(CrashHelperClient {
            connector: client_endpoint,
            spawner_thread: None,
            helper_process: Some(()),
        })
    }

    fn spawn_crash_helper(
        program: *const BreakpadChar,
        breakpad_data: BreakpadData,
        minidump_path: *const BreakpadChar,
        listener: IPCListener,
        endpoint: IPCConnector,
    ) -> Result<()> {
        let parent_pid = getpid().to_string();
        let parent_pid_arg = unsafe { CString::from_vec_unchecked(parent_pid.into_bytes()) };
        let pid = unsafe { fork() }?;

        match pid {
            ForkResult::Child => {
                // Create a new process group and a new session, this guarantees
                // that the crash helper process will be disconnected from the
                // signals of Firefox main process' controlling terminal. Killing
                // Firefox via the terminal shouldn't kill the crash helper which
                // has its own lifecycle management.
                //
                // We don't check for errors as there's nothing we can do to
                // handle one in this context.
                let _ = setsid();

                // fork() again to daemonize the process, the parent will wait on
                // the first child so that we don't leave zombie processes around.
                let pid = unsafe { fork() }.unwrap();

                match pid {
                    ForkResult::Child => {
                        let program = unsafe { CStr::from_ptr(program) };
                        let breakpad_data_arg = unsafe {
                            CString::from_vec_unchecked(breakpad_data.to_string().into_bytes())
                        };
                        let minidump_path = unsafe { CStr::from_ptr(minidump_path) };
                        let listener_arg = listener.serialize();
                        let endpoint_arg = endpoint.serialize();

                        let _ = execv(
                            program,
                            &[
                                program,
                                &parent_pid_arg,
                                &breakpad_data_arg,
                                minidump_path,
                                &listener_arg,
                                &endpoint_arg,
                            ],
                        );

                        // This point should be unreachable, but let's play it safe
                        unsafe { nix::libc::_exit(1) };
                    }
                    _ => unsafe {
                        // We're done, exit cleanly
                        nix::libc::_exit(0);
                    },
                }
            }
            ForkResult::Parent { child } => {
                // The child should exit quickly after having forked off the
                // actual crash helper process, let's wait for it.
                ignore_eintr!(waitpid(child, None))?;
                Ok(())
            }
        }
    }

    #[cfg(not(target_os = "linux"))]
    pub(crate) fn prepare_for_minidump(_pid: crash_helper_common::Pid) {
        // This is a no-op on platforms that don't need it
    }
}
