/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use crash_helper_common::Pid;
use nix::libc::{prctl, PR_SET_PTRACER};

use crate::CrashHelperClient;

impl CrashHelperClient {
    pub(crate) fn prepare_for_minidump(crash_helper_pid: Pid) {
        unsafe {
            // TODO: Log a warning in case we fail to set the ptracer
            let _ = prctl(PR_SET_PTRACER, crash_helper_pid);
        }
    }
}
