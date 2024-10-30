/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use minidump_analyzer::MinidumpAnalyzer;

#[derive(Debug)]
#[repr(C)]
pub struct Utf16String {
    chars: *mut u16,
    len: usize,
}

impl Utf16String {
    pub fn empty() -> Self {
        Utf16String {
            chars: std::ptr::null_mut(),
            len: 0,
        }
    }

    pub fn as_string_lossy(&self) -> String {
        if self.chars.is_null() {
            String::default()
        } else {
            String::from_utf16_lossy(unsafe { std::slice::from_raw_parts(self.chars, self.len) })
        }
    }
}

impl Drop for Utf16String {
    fn drop(&mut self) {
        if !self.chars.is_null() {
            // # Safety
            // We only own Utf16Strings which are created in Rust code, so when dropping, the
            // memory is that which we allocated (and thus is safe to deallocate).
            unsafe {
                std::alloc::dealloc(
                    self.chars as *mut u8,
                    // unwrap() because the memory must have been a size that doesn't overflow when
                    // it was originally allocated.
                    std::alloc::Layout::array::<u16>(self.len).unwrap(),
                )
            };
        }
    }
}

impl Default for Utf16String {
    fn default() -> Self {
        Self::empty()
    }
}

impl From<String> for Utf16String {
    fn from(value: String) -> Self {
        let utf16: Box<[u16]> = value.encode_utf16().collect();
        let utf16 = Box::leak(utf16);
        Utf16String {
            chars: utf16.as_mut_ptr(),
            len: utf16.len(),
        }
    }
}

#[no_mangle]
pub extern "C" fn minidump_analyzer_analyze(
    minidump_path: &Utf16String,
    extras_path: &Utf16String,
    all_threads: bool,
) -> Utf16String {
    let minidump_path = minidump_path.as_string_lossy();
    let extras_path = extras_path.as_string_lossy();
    MinidumpAnalyzer::new(minidump_path.as_ref())
        .extras_file(extras_path.as_ref())
        .all_threads(all_threads)
        .analyze()
        .err()
        .map(|e| e.to_string().into())
        .unwrap_or_default()
}

#[no_mangle]
pub extern "C" fn minidump_analyzer_free_result(result: Utf16String) {
    // This will happen anyway, but we're explicit about it for clarity.
    drop(result);
}
