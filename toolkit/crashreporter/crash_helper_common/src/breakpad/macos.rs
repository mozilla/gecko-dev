/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// On macOS breakpad uses a mach port to communicate with the exception
// handler and passes around its name as a string, additionally the crash
// generator uses regular C strings for paths and names.

use nix::libc::pid_t;
use std::ffi::{c_char, OsString};

use crate::{BreakpadString, MessageError};

pub type Pid = pid_t;
pub type BreakpadChar = c_char;
pub type AncillaryData = ();
pub type BreakpadRawData = *const c_char;

pub struct BreakpadData {
    data: OsString,
}

impl BreakpadData {
    pub fn new(raw: BreakpadRawData) -> BreakpadData {
        BreakpadData {
            data: <OsString as BreakpadString>::from_ptr(raw),
        }
    }

    pub fn into_raw(self) -> BreakpadRawData {
        // SAFETY: Valid string that we own
        unsafe { self.data.into_raw() }
    }

    pub(crate) fn from_slice_with_ancillary(
        data: &[u8],
        ancillary: Option<AncillaryData>,
    ) -> Result<BreakpadData, MessageError> {
        debug_assert!(
            ancillary.is_none(),
            "BreakpadData doesn't have ancillary data on macOS"
        );
        let data = <OsString as BreakpadString>::deserialize(data)?;
        Ok(BreakpadData { data })
    }

    pub(crate) fn serialize(&self) -> Vec<u8> {
        self.data.serialize()
    }

    pub(crate) fn ancillary(&self) -> Option<AncillaryData> {
        None
    }
}
