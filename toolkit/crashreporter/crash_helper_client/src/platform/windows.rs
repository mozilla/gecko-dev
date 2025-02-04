/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use anyhow::{bail, Result};
use crash_helper_common::{BreakpadChar, BreakpadString, IPCConnector, Pid};
use std::{
    ffi::OsString,
    mem::{size_of, zeroed},
    os::windows::ffi::OsStrExt,
    process,
    ptr::{null, null_mut},
};
use windows_sys::Win32::{
    Foundation::FALSE,
    System::Threading::{
        CreateProcessW, GetCurrentProcessId, CREATE_UNICODE_ENVIRONMENT, PROCESS_INFORMATION,
        STARTUPINFOW,
    },
};

use crate::CrashHelperClient;

impl CrashHelperClient {
    pub(crate) fn new(program: *const BreakpadChar) -> Result<CrashHelperClient> {
        let pid = CrashHelperClient::spawn_crash_helper(program)?;
        let connector = IPCConnector::connect(process::id())?;

        Ok(CrashHelperClient { connector, pid })
    }

    fn spawn_crash_helper(program: *const u16) -> Result<Pid> {
        let pid = unsafe { GetCurrentProcessId() };
        let program = <OsString as BreakpadString>::from_ptr(program);
        let mut cmd_line = OsString::from("\"");
        cmd_line.push(program);
        cmd_line.push("\" \"");
        cmd_line.push(pid.to_string());
        cmd_line.push("\"\0");
        let mut cmd_line: Vec<u16> = cmd_line.encode_wide().collect();

        let mut pi = unsafe { zeroed::<PROCESS_INFORMATION>() };
        let si = STARTUPINFOW {
            cb: size_of::<STARTUPINFOW>().try_into().unwrap(),
            ..unsafe { zeroed() }
        };

        let res = unsafe {
            CreateProcessW(
                null(),
                cmd_line.as_mut_ptr(),
                /* lpProcessAttributes */ null_mut(),
                /* lpThreadAttributes */ null_mut(),
                /* bInheritHandles */ FALSE,
                CREATE_UNICODE_ENVIRONMENT, // TODO: DETACHED_PROCESS?
                /* lpEnvironment */ null_mut(),
                /* lpCurrentDirectory */ null_mut(),
                &si,
                &mut pi,
            )
        };

        if res == FALSE {
            bail!("Could not create the crash helper process");
        }

        Ok(pi.dwProcessId)
    }
}
