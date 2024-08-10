/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{io, path::PathBuf};

use nsstring::nsAString;

/// Returns the canonical, absolute form of a path, normalizing
/// `/./` and `/../` components, symbolic links, and
/// {drive, directory}-relative paths.
///
/// ### Notes
///
/// This function calls [`std::fs::canonicalize`] on Windows, and
/// reimplements [`std::fs::canonicalize`] on Unix platforms
/// to work around an [allocator crash][1].
///
/// [1]: https://bugzilla.mozilla.org/show_bug.cgi?id=1531887
pub fn canonicalize(path: &nsAString) -> io::Result<PathBuf> {
    #[cfg(windows)]
    {
        use std::{ffi::OsString, os::windows::prelude::*};
        std::fs::canonicalize(OsString::from_wide(&*path))
    }
    #[cfg(unix)]
    {
        use std::{
            ffi::{CStr, CString, OsString},
            os::unix::prelude::*,
        };
        let path = CString::new(String::from_utf16(&*path).map_err(io::Error::other)?)?;
        let mut bytes = [0 as libc::c_char; libc::PATH_MAX as usize];
        let ptr = unsafe { libc::realpath(path.as_ptr(), bytes[..].as_mut_ptr()) };
        if ptr.is_null() {
            return Err(io::Error::last_os_error());
        }
        Ok(OsString::from_vec(unsafe { CStr::from_ptr(ptr) }.to_bytes().into()).into())
    }
    #[cfg(all(not(unix), not(windows)))]
    compile_error!("`kvstore::fs::canonicalize` requires Windows or Unix")
}
