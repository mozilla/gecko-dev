//! a binary just so we can look at the optimized  assembly

// we use the libz_sys but configure zlib-ng in zlib compat mode
use libz_sys as libz_ng_sys;

use std::{ffi::c_ulong, path::PathBuf};

unsafe fn uncompress(
    dest: *mut u8,
    dest_len: *mut std::ffi::c_ulong,
    source: *const u8,
    source_len: std::ffi::c_ulong,
) -> std::ffi::c_int {
    let lib = libloading::Library::new("/home/folkertdev/rust/zlib-ng/libz-ng.so").unwrap();

    type Func = unsafe extern "C" fn(
        dest: *mut u8,
        dest_len: *mut std::ffi::c_ulong,
        source: *const u8,
        source_len: std::ffi::c_ulong,
    ) -> std::ffi::c_int;

    let f: libloading::Symbol<Func> = lib.get(b"zng_uncompress").unwrap();

    f(dest, dest_len, source, source_len)
}

fn main() {
    let mut it = std::env::args();

    let _ = it.next().unwrap();

    match it.next().unwrap().as_str() {
        "ng" => {
            let path = it.next().unwrap();
            let input = std::fs::read(&path).unwrap();

            let mut dest_vec = vec![0u8; 1 << 28];

            let mut dest_len = dest_vec.len() as c_ulong;
            let dest = dest_vec.as_mut_ptr();

            let source = input.as_ptr();
            let source_len = input.len() as _;

            let err = unsafe { libz_ng_sys::uncompress(dest, &mut dest_len, source, source_len) };

            if err != 0 {
                panic!("error {err}");
            }

            dest_vec.truncate(dest_len as usize);

            let path = PathBuf::from(path);
            std::fs::write(path.with_extension(""), &dest_vec).unwrap();

            drop(dest_vec)
        }
        "rs" => {
            let path = it.next().unwrap();
            let input = std::fs::read(&path).unwrap();

            let mut dest_vec = vec![0u8; 1 << 28];

            let mut dest_len = dest_vec.len() as std::ffi::c_ulong;
            let dest = dest_vec.as_mut_ptr();

            let source = input.as_ptr();
            let source_len = input.len() as _;

            let err = unsafe { ::libz_rs_sys::uncompress(dest, &mut dest_len, source, source_len) };

            if err != 0 {
                panic!("error {err}");
            }

            dest_vec.truncate(dest_len as usize);

            let path = PathBuf::from(path);
            std::fs::write(path.with_extension(""), &dest_vec).unwrap();

            drop(dest_vec)
        }
        "xx" => {
            let path = it.next().unwrap();
            let input = std::fs::read(&path).unwrap();

            let mut dest_vec = vec![0u8; 1 << 28];

            let mut dest_len = dest_vec.len() as std::ffi::c_ulong;
            let dest = dest_vec.as_mut_ptr();

            let source = input.as_ptr();
            let source_len = input.len() as _;

            let err = unsafe { uncompress(dest, &mut dest_len, source, source_len) };

            if err != 0 {
                panic!();
            }

            dest_vec.truncate(dest_len as usize);

            let path = PathBuf::from(path);
            std::fs::write(path.with_extension(""), &dest_vec).unwrap();

            drop(dest_vec)
        }
        other => panic!("invalid option '{other}', expected one of 'rs' or 'ng'"),
    }
}
