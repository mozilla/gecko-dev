/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#[doc(hidden)]
pub use bincode as _bincode;

/// Takes a type, a serializer, and a deserializer name. The type must implement serde::Serialize.
/// Defines a pair of ffi functions that go along with the MOZ_DEFINE_RUST_PARAMTRAITS macro.
#[macro_export]
macro_rules! define_ffi_serializer {
    ($ty:ty, $serializer:ident, $deserializer:ident) => {
        #[no_mangle]
        pub extern "C" fn $serializer(
            v: &$ty,
            out_len: &mut usize,
            out_cap: &mut usize,
        ) -> *mut u8 {
            *out_len = 0;
            *out_cap = 0;
            let mut buf = match $crate::_bincode::serialize(v) {
                Ok(buf) => buf,
                Err(..) => return std::ptr::null_mut(),
            };
            *out_len = buf.len();
            *out_cap = buf.capacity();
            let ptr = buf.as_mut_ptr();
            std::mem::forget(buf);
            ptr
        }

        #[no_mangle]
        pub unsafe extern "C" fn $deserializer(
            input: *const u8,
            len: usize,
            out: *mut $ty,
        ) -> bool {
            let slice = std::slice::from_raw_parts(input, len);
            let element = match $crate::_bincode::deserialize(slice) {
                Ok(buf) => buf,
                Err(..) => return false,
            };
            std::ptr::write(out, element);
            true
        }
    };
}
