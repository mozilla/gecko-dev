//! a binary just so we can look at the optimized assembly

use std::{collections::hash_map::DefaultHasher, env::temp_dir, hash::Hash};

// we use the libz_sys but configure zlib-ng in zlib compat mode
use libz_sys as libz_ng_sys;

use zlib_rs::{deflate::DeflateConfig, DeflateFlush, ReturnCode};

use std::ffi::{c_int, c_uint};

fn main() {
    let mut it = std::env::args();

    let _ = it.next().unwrap();

    let level: i32 = it.next().unwrap().parse().unwrap();

    let mut hasher = DefaultHasher::new();
    use std::hash::Hasher;

    match it.next().unwrap().as_str() {
        "ng" => {
            let path = it.next().unwrap();
            let input = std::fs::read(path).unwrap();

            let mut dest_vec = vec![0u8; 1 << 28];
            let mut dest_len = dest_vec.len();

            let err = compress_ng(&mut dest_vec, &mut dest_len, &input, level);

            if err != ReturnCode::Ok {
                panic!("error {err:?}");
            }

            dest_vec.truncate(dest_len);

            dest_vec.hash(&mut hasher);
            dbg!(hasher.finish());

            std::fs::write(temp_dir().join("ng.txt"), &dest_vec).unwrap();

            drop(dest_vec)
        }
        "rs" => {
            let path = it.next().unwrap();
            let Ok(input) = std::fs::read(&path) else {
                panic!("could not read file {path:?}");
            };

            let mut dest_vec = vec![0u8; 1 << 28];
            let mut dest_len = dest_vec.len();

            let err = compress_rs(&mut dest_vec, &mut dest_len, &input, level);

            if err != ReturnCode::Ok {
                panic!("error {err:?}");
            }

            dest_vec.truncate(dest_len);

            dest_vec.hash(&mut hasher);
            dbg!(hasher.finish());

            std::fs::write(temp_dir().join("rs.txt"), &dest_vec).unwrap();

            drop(dest_vec)
        }
        "xx" => {
            let path = it.next().unwrap();
            let Ok(input) = std::fs::read(&path) else {
                panic!("could not read file {path:?}");
            };

            let mut dest_vec = vec![0u8; 1 << 28];
            let mut dest_len = dest_vec.len();

            let err = compress_dynamic(&mut dest_vec, &mut dest_len, &input, level);

            if err != ReturnCode::Ok {
                panic!("error {err:?}");
            }

            dest_vec.truncate(dest_len);

            dest_vec.hash(&mut hasher);
            dbg!(hasher.finish());

            std::fs::write(temp_dir().join("xx.tar.gz"), &dest_vec).unwrap();

            drop(dest_vec)
        }
        "qq" => {
            let ng = std::fs::read(temp_dir().join("ng.txt")).unwrap();
            let rs = std::fs::read(temp_dir().join("rs.txt")).unwrap();

            for (i, (a, b)) in (ng.iter().zip(rs.iter())).enumerate() {
                if a != b {
                    panic!("index {i}");
                }
            }
        }
        other => panic!("invalid option '{other}', expected one of 'rs' or 'ng'"),
    }
}

const METHOD: i32 = zlib_rs::c_api::Z_DEFLATED;
const WINDOW_BITS: i32 = 15;
const MEM_LEVEL: i32 = 8;
const STRATEGY: i32 = zlib_rs::c_api::Z_DEFAULT_STRATEGY;

fn compress_rs(
    dest: &mut [u8],
    dest_len: &mut usize,
    source: &[u8],
    //
    level: i32,
) -> ReturnCode {
    use libz_rs_sys::{deflate, deflateEnd, deflateInit2_, z_stream, zlibVersion};

    let mut stream = z_stream {
        next_in: source.as_ptr() as *mut u8,
        avail_in: 0, // for special logic in the first  iteration
        total_in: 0,
        next_out: dest.as_mut_ptr(),
        avail_out: 0, // for special logic on the first iteration
        total_out: 0,
        msg: std::ptr::null_mut(),
        state: std::ptr::null_mut(),
        zalloc: Some(zlib_rs::allocate::zalloc_c),
        zfree: Some(zlib_rs::allocate::zfree_c),
        opaque: std::ptr::null_mut(),
        data_type: 0,
        adler: 0,
        reserved: 0,
    };

    let err = {
        let strm: *mut z_stream = &mut stream;
        unsafe {
            deflateInit2_(
                strm,
                level,
                METHOD,
                WINDOW_BITS,
                MEM_LEVEL,
                STRATEGY,
                zlibVersion(),
                std::mem::size_of::<z_stream>() as c_int,
            )
        }
    };

    if ReturnCode::from(err) != ReturnCode::Ok as _ {
        return ReturnCode::from(err);
    }

    let max = c_uint::MAX as usize;

    let mut left = dest.len();
    let mut source_len = source.len();

    loop {
        if stream.avail_out == 0 {
            stream.avail_out = Ord::min(left, max) as _;
            left -= stream.avail_out as usize;
        }

        if stream.avail_in == 0 {
            stream.avail_in = Ord::min(source_len, max) as _;
            source_len -= stream.avail_in as usize;
        }

        let flush = if source_len > 0 {
            DeflateFlush::NoFlush
        } else {
            DeflateFlush::Finish
        };

        let err = unsafe { deflate(&mut stream, flush as i32) };
        if ReturnCode::from(err) != ReturnCode::Ok {
            break;
        }
    }

    *dest_len = stream.total_out as _;

    unsafe { deflateEnd(&mut stream) };

    ReturnCode::Ok
}

fn compress_ng(
    dest: &mut [u8],
    dest_len: &mut usize,
    source: &[u8],
    //
    level: i32,
) -> ReturnCode {
    use libz_ng_sys::{deflate, deflateEnd, deflateInit2_, z_stream, zlibVersion};

    let mut stream = z_stream {
        next_in: source.as_ptr() as *mut u8,
        avail_in: 0, // for special logic in the first  iteration
        total_in: 0,
        next_out: dest.as_mut_ptr(),
        avail_out: 0, // for special logic on the first iteration
        total_out: 0,
        msg: std::ptr::null_mut(),
        state: std::ptr::null_mut(),
        zalloc: zlib_rs::allocate::zalloc_c,
        zfree: zlib_rs::allocate::zfree_c,
        opaque: std::ptr::null_mut(),
        data_type: 0,
        adler: 0,
        reserved: 0,
    };

    let err = {
        let strm: *mut z_stream = &mut stream;
        unsafe {
            deflateInit2_(
                strm,
                level,
                METHOD,
                WINDOW_BITS,
                MEM_LEVEL,
                STRATEGY,
                zlibVersion(),
                std::mem::size_of::<z_stream>() as c_int,
            )
        }
    };

    if ReturnCode::from(err) != ReturnCode::Ok as _ {
        return ReturnCode::from(err);
    }

    let max = c_uint::MAX as usize;

    let mut left = dest.len();
    let mut source_len = source.len();

    loop {
        if stream.avail_out == 0 {
            stream.avail_out = Ord::min(left, max) as _;
            left -= stream.avail_out as usize;
        }

        if stream.avail_in == 0 {
            stream.avail_in = Ord::min(source_len, max) as _;
            source_len -= stream.avail_in as usize;
        }

        let flush = if source_len > 0 {
            DeflateFlush::NoFlush
        } else {
            DeflateFlush::Finish
        };

        let err = unsafe { deflate(&mut stream, flush as i32) };
        if ReturnCode::from(err) != ReturnCode::Ok {
            break;
        }
    }

    *dest_len = stream.total_out as _;

    unsafe { deflateEnd(&mut stream) };

    ReturnCode::Ok
}

fn compress_dynamic(
    dest: &mut [u8],
    dest_len: &mut usize,
    source: &[u8],
    //
    level: i32,
) -> ReturnCode {
    let config = DeflateConfig::new(level);
    let (output, err) = dynamic_libz_sys::compress_slice(
        dest,
        source,
        config.level,
        config.method as i32,
        config.window_bits,
        config.mem_level,
        config.strategy as i32,
    );

    *dest_len = output.len();

    ReturnCode::from(err)
}
