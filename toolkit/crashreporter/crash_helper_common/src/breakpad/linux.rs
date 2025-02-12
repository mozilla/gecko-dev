/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// On Linux breakpad uses a pipe to communicate with the exception handler, so
// we need to pass a file descriptor between the client and the crash generator
// and regular C strings for paths and names.

use nix::libc::pid_t;
use std::{ffi::c_char, os::fd::RawFd};

use crate::errors::MessageError;

pub type Pid = pid_t;
pub type BreakpadChar = c_char;
pub type AncillaryData = RawFd;
pub type BreakpadRawData = RawFd;

pub struct BreakpadData {
    data: RawFd,
}

impl BreakpadData {
    pub fn new(raw: BreakpadRawData) -> BreakpadData {
        BreakpadData { data: raw }
    }

    pub fn into_raw(self) -> BreakpadRawData {
        self.data
    }

    pub(crate) fn from_slice_with_ancillary(
        _data: &[u8],
        ancillary: Option<AncillaryData>,
    ) -> Result<BreakpadData, MessageError> {
        Ok(BreakpadData {
            data: ancillary.unwrap(),
        })
    }

    pub(crate) fn serialize(&self) -> Vec<u8> {
        Vec::new()
    }

    pub(crate) fn ancillary(&self) -> Option<AncillaryData> {
        Some(self.data)
    }
}
