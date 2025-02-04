/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// On Windows Breakpad uses a named pipe for communication between the
// exception handlers and the crash generator, and Windows UTF-16 strings for
// name and paths, so all we need to pass between processes are
// 2-bytes-per-character strings.

use std::ffi::OsString;

use crate::{errors::MessageError, BreakpadString};

pub type Pid = u32;
pub type BreakpadChar = u16;
pub type AncillaryData = ();
pub type BreakpadRawData = *const u16;

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
        // SAFETY: Valid Windows string that we own
        unsafe { self.data.into_raw() }
    }

    pub(crate) fn from_slice_with_ancillary(
        data: &[u8],
        ancillary: Option<AncillaryData>,
    ) -> Result<BreakpadData, MessageError> {
        debug_assert!(
            ancillary.is_none(),
            "BreakpadData doesn't have ancillary data on Windows"
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
