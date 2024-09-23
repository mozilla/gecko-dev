/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pub use std::ffi::CString;
use std::ffi::OsStr;
use std::os::windows::ffi::OsStrExt;

/// Windows wide strings.
///
/// These are utf16 encoded with a terminating nul character (0).
#[derive(Debug)]
pub struct WideString(Vec<u16>);

/// An error indicating that an interior nul byte was found.
#[derive(Clone, PartialEq, Eq, Debug)]
pub struct NulError(usize, Vec<u16>);

impl std::fmt::Display for NulError {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "nul byte found in provided data at position: {}", self.0)
    }
}

impl std::error::Error for NulError {}

impl WideString {
    pub fn new(os_str: impl AsRef<OsStr>) -> Result<Self, NulError> {
        let mut v: Vec<u16> = os_str.as_ref().encode_wide().collect();
        if let Some(p) = v.iter().position(|c| *c == 0) {
            Err(NulError(p, v))
        } else {
            v.push(0);
            Ok(WideString(v))
        }
    }

    pub fn pcwstr(&self) -> windows_sys::core::PCWSTR {
        self.0.as_ptr()
    }
}

/// Iterator over wide characters in a wide string.
///
/// This is useful for wide string constants.
#[derive(Debug)]
pub struct FfiWideCharIterator(*const u16);

impl FfiWideCharIterator {
    pub fn new(ptr: *const u16) -> Self {
        FfiWideCharIterator(ptr)
    }
}

impl Iterator for FfiWideCharIterator {
    type Item = u16;

    fn next(&mut self) -> Option<Self::Item> {
        let c = unsafe { self.0.read() };
        if c == 0 {
            None
        } else {
            self.0 = unsafe { self.0.add(1) };
            Some(c)
        }
    }
}

/// Convert a utf16 ptr to an ascii CString.
pub fn utf16_ptr_to_ascii(ptr: *const u16) -> Option<CString> {
    char::decode_utf16(FfiWideCharIterator::new(ptr))
        // Using try_into() accepts extended ascii as well; we don't care much about the
        // distinction here, it'll still be a valid conversion.
        .map(|res| res.ok().and_then(|c| c.try_into().ok()))
        .collect::<Option<Vec<_>>>()
        .map(CString::new)
        .and_then(|res| {
            if res.is_err() {
                log::error!("FfiWideCharIterator provided nul character");
            }
            res.ok()
        })
}
