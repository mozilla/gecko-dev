#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(clippy::missing_safety_doc)] // obviously needs to be fixed long-term
#![doc = include_str!("../README.md")]
#![cfg_attr(not(feature = "std"), no_std)]

//! # Safety of `*mut z_stream`
//!
//! Most functions require an argument of type `*mut z_stream`. Unless
//! otherwise noted, the safety requirements on such arguments are at least that the
//! pointer must be either:
//!
//! - A `NULL` pointer
//! - A pointer to a correctly aligned, initialized value of type `z_stream`.
//!
//! In other words, it must be safe to cast the `*mut z_stream` to a `Option<&mut z_stream>`. It is
//! always safe to provide an argument of type `&mut z_stream`: rust will automatically downcast
//! the argument to `*mut z_stream`.

use core::mem::MaybeUninit;

use core::ffi::{c_char, c_int, c_long, c_uchar, c_uint, c_ulong, c_void};

use zlib_rs::{
    deflate::{DeflateConfig, DeflateStream, Method, Strategy},
    inflate::{InflateConfig, InflateStream},
    DeflateFlush, InflateFlush, ReturnCode,
};

pub use zlib_rs::c_api::*;

#[cfg(feature = "custom-prefix")]
macro_rules! prefix {
    ($name:expr) => {
        concat!(env!("LIBZ_RS_SYS_PREFIX"), stringify!($name))
    };
}

#[cfg(all(
    not(feature = "custom-prefix"),
    not(any(test, feature = "testing-prefix"))
))]
macro_rules! prefix {
    ($name:expr) => {
        stringify!($name)
    };
}

#[cfg(all(not(feature = "custom-prefix"), any(test, feature = "testing-prefix")))]
macro_rules! prefix {
    ($name:expr) => {
        concat!("LIBZ_RS_SYS_TEST_", stringify!($name))
    };
}

#[cfg(all(feature = "rust-allocator", feature = "c-allocator"))]
const _: () =
    compile_error!("Only one of `rust-allocator` and `c-allocator` can be enabled at a time");

#[allow(unreachable_code)]
const DEFAULT_ZALLOC: Option<alloc_func> = '_blk: {
    // this `break 'blk'` construction exists to generate just one compile error and not other
    // warnings when multiple allocators are configured.

    #[cfg(feature = "c-allocator")]
    break '_blk Some(zlib_rs::allocate::Allocator::C.zalloc);

    #[cfg(feature = "rust-allocator")]
    break '_blk Some(zlib_rs::allocate::Allocator::RUST.zalloc);

    None
};

#[allow(unreachable_code)]
const DEFAULT_ZFREE: Option<free_func> = '_blk: {
    #[cfg(feature = "c-allocator")]
    break '_blk Some(zlib_rs::allocate::Allocator::C.zfree);

    #[cfg(feature = "rust-allocator")]
    break '_blk Some(zlib_rs::allocate::Allocator::RUST.zfree);

    None
};

// In spirit this type is `libc::off_t`, but it would be our only libc dependency, and so we
// hardcode the type here. This should be correct on most operating systems. If we ever run into
// issues with it, we can either special-case or add a feature flag to force a particular width
pub type z_off_t = c_long;

/// Calculates the [crc32](https://en.wikipedia.org/wiki/Computation_of_cyclic_redundancy_checks#CRC-32_algorithm) checksum
/// of a sequence of bytes.
///
/// When the pointer argument is `NULL`, the initial checksum value is returned.
///
/// ## Safety
///
/// The caller must guarantee that either:
///
/// - `buf` is `NULL`
/// - `buf` and `len` satisfy the requirements of [`core::slice::from_raw_parts`]
///
/// ## Example
///
/// ```
/// use libz_rs_sys::crc32;
///
/// unsafe {
///     assert_eq!(crc32(0, core::ptr::null(), 0), 0);
///     assert_eq!(crc32(1, core::ptr::null(), 32), 0);
///
///     let input = [1,2,3];
///     assert_eq!(crc32(0, input.as_ptr(), input.len() as _), 1438416925);
/// }
/// ```
#[export_name = prefix!(crc32)]
pub unsafe extern "C" fn crc32(crc: c_ulong, buf: *const Bytef, len: uInt) -> c_ulong {
    if buf.is_null() {
        0
    } else {
        // SAFETY: requirements must be satisfied by the caller
        let buf = unsafe { core::slice::from_raw_parts(buf, len as usize) };
        zlib_rs::crc32(crc as u32, buf) as c_ulong
    }
}

/// Combines the checksum of two slices into one.
///
/// The combined value is equivalent to calculating the checksum of the whole input.
///
/// This function can be used when input arrives in chunks, or when different threads
/// calculate the checksum of different sections of the input.
///
/// ## Example
///
/// ```
/// use libz_rs_sys::{crc32, crc32_combine};
///
/// let input = [1, 2, 3, 4, 5, 6, 7, 8];
/// let lo = &input[..4];
/// let hi = &input[4..];
///
/// unsafe {
///     let full = crc32(0, input.as_ptr(), input.len() as _);
///
///     let crc1 = crc32(0, lo.as_ptr(), lo.len() as _);
///     let crc2 = crc32(0, hi.as_ptr(), hi.len() as _);
///
///     let combined = crc32_combine(crc1, crc2, hi.len() as _);
///
///     assert_eq!(full, combined);
/// }
/// ```
#[export_name = prefix!(crc32_combine)]
pub extern "C" fn crc32_combine(crc1: c_ulong, crc2: c_ulong, len2: z_off_t) -> c_ulong {
    zlib_rs::crc32_combine(crc1 as u32, crc2 as u32, len2 as u64) as c_ulong
}

/// Calculates the [adler32](https://en.wikipedia.org/wiki/Adler-32) checksum
/// of a sequence of bytes.
///
/// When the pointer argument is `NULL`, the initial checksum value is returned.
///
/// ## Safety
///
/// The caller must guarantee that either:
///
/// - `buf` is `NULL`
/// - `buf` and `len` satisfy the requirements of [`core::slice::from_raw_parts`]
///
/// ## Example
///
/// ```
/// use libz_rs_sys::adler32;
///
/// unsafe {
///     assert_eq!(adler32(0, core::ptr::null(), 0), 1);
///     assert_eq!(adler32(1, core::ptr::null(), 32), 1);
///
///     let input = [1,2,3];
///     assert_eq!(adler32(0, input.as_ptr(), input.len() as _), 655366);
/// }
/// ```
#[export_name = prefix!(adler32)]
pub unsafe extern "C" fn adler32(adler: c_ulong, buf: *const Bytef, len: uInt) -> c_ulong {
    if buf.is_null() {
        1
    } else {
        // SAFETY: requirements must be satisfied by the caller
        let buf = unsafe { core::slice::from_raw_parts(buf, len as usize) };
        zlib_rs::adler32(adler as u32, buf) as c_ulong
    }
}

/// Combines the checksum of two slices into one.
///
/// The combined value is equivalent to calculating the checksum of the whole input.
///
/// This function can be used when input arrives in chunks, or when different threads
/// calculate the checksum of different sections of the input.
///
/// ## Example
///
/// ```
/// use libz_rs_sys::{adler32, adler32_combine};
///
/// let input = [1, 2, 3, 4, 5, 6, 7, 8];
/// let lo = &input[..4];
/// let hi = &input[4..];
///
/// unsafe {
///     let full = adler32(1, input.as_ptr(), input.len() as _);
///
///     let adler1 = adler32(1, lo.as_ptr(), lo.len() as _);
///     let adler2 = adler32(1, hi.as_ptr(), hi.len() as _);
///
///     let combined = adler32_combine(adler1, adler2, hi.len() as _);
///
///     assert_eq!(full, combined);
/// }
/// ```
#[export_name = prefix!(adler32_combine)]
pub extern "C" fn adler32_combine(adler1: c_ulong, adler2: c_ulong, len2: z_off_t) -> c_ulong {
    match u64::try_from(len2) {
        Ok(len2) => zlib_rs::adler32_combine(adler1 as u32, adler2 as u32, len2) as c_ulong,
        Err(_) => {
            // for negative len, return invalid adler32 as a clue for debugging
            0xFFFF_FFFF
        }
    }
}

/// Inflates `source` into `dest`, and writes the final inflated size into `destLen`.
///
/// # Safety
///
/// Behavior is undefined if any of the following conditions are violated:
///
/// - `source` must be [valid](https://doc.rust-lang.org/std/ptr/index.html#safety) for reads for
///   `sourceLen` bytes. The entity of `source` must be contained in one allocated object!
/// - `source` must point to `sourceLen` consecutive properly initialized values of type `u8`.
/// - `dest` must be [valid](https://doc.rust-lang.org/std/ptr/index.html#safety) for reads for
///   `*destLen` bytes. The entity of `source` must be contained in one allocated object!
/// - `dest` must point to `*destLen` consecutive properly initialized values of type `u8`.
/// - while this function runs, both read and write actions to the `source` and `dest` memory
///   ranges are forbidden
#[export_name = prefix!(uncompress)]
pub unsafe extern "C" fn uncompress(
    dest: *mut u8,
    destLen: *mut c_ulong,
    source: *const u8,
    sourceLen: c_ulong,
) -> c_int {
    let data = dest;
    let len = core::ptr::read(destLen) as usize;
    let output = core::slice::from_raw_parts_mut(data as *mut MaybeUninit<u8>, len);

    let data = source;
    let len = sourceLen as usize;
    let input = core::slice::from_raw_parts(data, len);

    let (output, err) = zlib_rs::inflate::uncompress(output, input, InflateConfig::default());

    core::ptr::write(destLen, output.len() as _);

    err as c_int
}

#[export_name = prefix!(inflate)]
pub unsafe extern "C" fn inflate(strm: *mut z_stream, flush: i32) -> i32 {
    if let Some(stream) = InflateStream::from_stream_mut(strm) {
        let flush = InflateFlush::try_from(flush).unwrap_or_default();
        zlib_rs::inflate::inflate(stream, flush) as _
    } else {
        ReturnCode::StreamError as _
    }
}

#[export_name = prefix!(inflateEnd)]
pub unsafe extern "C" fn inflateEnd(strm: *mut z_stream) -> i32 {
    match InflateStream::from_stream_mut(strm) {
        Some(stream) => {
            zlib_rs::inflate::end(stream);
            ReturnCode::Ok as _
        }
        None => ReturnCode::StreamError as _,
    }
}

#[export_name = prefix!(inflateBackInit_)]
pub unsafe extern "C" fn inflateBackInit_(
    _strm: z_streamp,
    _windowBits: c_int,
    _window: *mut c_uchar,
    _version: *const c_char,
    _stream_size: c_int,
) -> c_int {
    todo!("inflateBack is not implemented yet")
}

#[export_name = prefix!(inflateBack)]
pub unsafe extern "C" fn inflateBack(
    _strm: z_streamp,
    _in: in_func,
    _in_desc: *mut c_void,
    _out: out_func,
    _out_desc: *mut c_void,
) -> c_int {
    todo!("inflateBack is not implemented yet")
}

#[export_name = prefix!(inflateBackEnd)]
pub unsafe extern "C" fn inflateBackEnd(_strm: z_streamp) -> c_int {
    todo!("inflateBack is not implemented yet")
}

#[export_name = prefix!(inflateCopy)]
pub unsafe extern "C" fn inflateCopy(dest: *mut z_stream, source: *const z_stream) -> i32 {
    if dest.is_null() {
        return ReturnCode::StreamError as _;
    }

    if let Some(source) = InflateStream::from_stream_ref(source) {
        zlib_rs::inflate::copy(&mut *(dest as *mut MaybeUninit<InflateStream>), source) as _
    } else {
        ReturnCode::StreamError as _
    }
}

#[export_name = prefix!(inflateMark)]
pub unsafe extern "C" fn inflateMark(strm: *const z_stream) -> c_long {
    if let Some(stream) = InflateStream::from_stream_ref(strm) {
        zlib_rs::inflate::mark(stream)
    } else {
        c_long::MIN
    }
}

#[export_name = prefix!(inflateSync)]
pub unsafe extern "C" fn inflateSync(strm: *mut z_stream) -> i32 {
    if let Some(stream) = InflateStream::from_stream_mut(strm) {
        zlib_rs::inflate::sync(stream) as _
    } else {
        ReturnCode::StreamError as _
    }
}

// undocumented
#[export_name = prefix!(inflateSyncPoint)]
pub unsafe extern "C" fn inflateSyncPoint(strm: *mut z_stream) -> i32 {
    if let Some(stream) = InflateStream::from_stream_mut(strm) {
        zlib_rs::inflate::sync_point(stream) as i32
    } else {
        ReturnCode::StreamError as _
    }
}

#[export_name = prefix!(inflateInit_)]
pub unsafe extern "C" fn inflateInit_(
    strm: z_streamp,
    version: *const c_char,
    stream_size: c_int,
) -> c_int {
    if !is_version_compatible(version, stream_size) {
        ReturnCode::VersionError as _
    } else if strm.is_null() {
        ReturnCode::StreamError as _
    } else {
        zlib_rs::inflate::init(&mut *strm, InflateConfig::default()) as _
    }
}

#[export_name = prefix!(inflateInit2_)]
pub unsafe extern "C" fn inflateInit2_(
    strm: z_streamp,
    windowBits: c_int,
    version: *const c_char,
    stream_size: c_int,
) -> c_int {
    if !is_version_compatible(version, stream_size) {
        ReturnCode::VersionError as _
    } else {
        inflateInit2(strm, windowBits)
    }
}

unsafe extern "C" fn inflateInit2(strm: z_streamp, windowBits: c_int) -> c_int {
    if strm.is_null() {
        ReturnCode::StreamError as _
    } else {
        let config = InflateConfig {
            window_bits: windowBits,
        };

        let stream = &mut *strm;

        if stream.zalloc.is_none() {
            stream.zalloc = DEFAULT_ZALLOC;
            stream.opaque = core::ptr::null_mut();
        }

        if stream.zfree.is_none() {
            stream.zfree = DEFAULT_ZFREE;
        }

        zlib_rs::inflate::init(stream, config) as _
    }
}

#[export_name = prefix!(inflatePrime)]
pub unsafe extern "C" fn inflatePrime(strm: *mut z_stream, bits: i32, value: i32) -> i32 {
    if let Some(stream) = InflateStream::from_stream_mut(strm) {
        zlib_rs::inflate::prime(stream, bits, value) as _
    } else {
        ReturnCode::StreamError as _
    }
}

#[export_name = prefix!(inflateReset)]
pub unsafe extern "C" fn inflateReset(strm: *mut z_stream) -> i32 {
    if let Some(stream) = InflateStream::from_stream_mut(strm) {
        zlib_rs::inflate::reset(stream) as _
    } else {
        ReturnCode::StreamError as _
    }
}

#[export_name = prefix!(inflateReset2)]
pub unsafe extern "C" fn inflateReset2(strm: *mut z_stream, windowBits: c_int) -> i32 {
    if let Some(stream) = InflateStream::from_stream_mut(strm) {
        let config = InflateConfig {
            window_bits: windowBits,
        };
        zlib_rs::inflate::reset_with_config(stream, config) as _
    } else {
        ReturnCode::StreamError as _
    }
}

#[export_name = prefix!(inflateSetDictionary)]
pub unsafe extern "C" fn inflateSetDictionary(
    strm: *mut z_stream,
    dictionary: *const u8,
    dictLength: c_uint,
) -> c_int {
    let Some(stream) = InflateStream::from_stream_mut(strm) else {
        return ReturnCode::StreamError as _;
    };

    let dict = if dictLength == 0 || dictionary.is_null() {
        &[]
    } else {
        unsafe { core::slice::from_raw_parts(dictionary, dictLength as usize) }
    };

    zlib_rs::inflate::set_dictionary(stream, dict) as _
}

// part of gzip
#[export_name = prefix!(inflateGetHeader)]
pub unsafe extern "C" fn inflateGetHeader(strm: z_streamp, head: gz_headerp) -> c_int {
    if let Some(stream) = InflateStream::from_stream_mut(strm) {
        let header = if head.is_null() {
            None
        } else {
            Some(unsafe { &mut *(head) })
        };

        zlib_rs::inflate::get_header(stream, header) as i32
    } else {
        ReturnCode::StreamError as _
    }
}

// undocumented but exposed function
#[export_name = prefix!(inflateUndermine)]
pub unsafe extern "C" fn inflateUndermine(strm: *mut z_stream, subvert: i32) -> c_int {
    if let Some(stream) = InflateStream::from_stream_mut(strm) {
        zlib_rs::inflate::undermine(stream, subvert) as i32
    } else {
        ReturnCode::StreamError as _
    }
}

// undocumented but exposed function
#[export_name = prefix!(inflateResetKeep)]
pub unsafe extern "C" fn inflateResetKeep(strm: *mut z_stream) -> i32 {
    if let Some(stream) = InflateStream::from_stream_mut(strm) {
        zlib_rs::inflate::reset_keep(stream) as _
    } else {
        ReturnCode::StreamError as _
    }
}

// undocumented but exposed function
#[export_name = prefix!(inflateCodesUsed)]
pub unsafe extern "C" fn inflateCodesUsed(_strm: *mut z_stream) -> c_ulong {
    todo!()
}

#[export_name = prefix!(deflate)]
pub unsafe extern "C" fn deflate(strm: *mut z_stream, flush: i32) -> i32 {
    if let Some(stream) = DeflateStream::from_stream_mut(strm) {
        match DeflateFlush::try_from(flush) {
            Ok(flush) => zlib_rs::deflate::deflate(stream, flush) as _,
            Err(()) => ReturnCode::StreamError as _,
        }
    } else {
        ReturnCode::StreamError as _
    }
}

#[export_name = prefix!(deflateSetHeader)]
pub unsafe extern "C" fn deflateSetHeader(strm: *mut z_stream, head: gz_headerp) -> i32 {
    if let Some(stream) = DeflateStream::from_stream_mut(strm) {
        zlib_rs::deflate::set_header(
            stream,
            if head.is_null() {
                None
            } else {
                Some(&mut *head)
            },
        ) as _
    } else {
        ReturnCode::StreamError as _
    }
}

#[export_name = prefix!(deflateBound)]
pub unsafe extern "C" fn deflateBound(strm: *mut z_stream, sourceLen: c_ulong) -> c_ulong {
    zlib_rs::deflate::bound(DeflateStream::from_stream_mut(strm), sourceLen as usize) as c_ulong
}

#[export_name = prefix!(compress)]
pub unsafe extern "C" fn compress(
    dest: *mut Bytef,
    destLen: *mut c_ulong,
    source: *const Bytef,
    sourceLen: c_ulong,
) -> c_int {
    compress2(
        dest,
        destLen,
        source,
        sourceLen,
        DeflateConfig::default().level,
    )
}

#[export_name = prefix!(compress2)]
pub unsafe extern "C" fn compress2(
    dest: *mut Bytef,
    destLen: *mut c_ulong,
    source: *const Bytef,
    sourceLen: c_ulong,
    level: c_int,
) -> c_int {
    let data = dest;
    let len = core::ptr::read(destLen) as usize;
    let output = core::slice::from_raw_parts_mut(data as *mut MaybeUninit<u8>, len);

    let data = source;
    let len = sourceLen as usize;
    let input = core::slice::from_raw_parts(data, len);

    let config = DeflateConfig::new(level);
    let (output, err) = zlib_rs::deflate::compress(output, input, config);

    core::ptr::write(destLen, output.len() as _);

    err as c_int
}

#[export_name = prefix!(compressBound)]
pub extern "C" fn compressBound(sourceLen: c_ulong) -> c_ulong {
    zlib_rs::deflate::compress_bound(sourceLen as usize) as c_ulong
}

#[export_name = prefix!(deflateEnd)]
pub unsafe extern "C" fn deflateEnd(strm: *mut z_stream) -> i32 {
    match DeflateStream::from_stream_mut(strm) {
        Some(stream) => match zlib_rs::deflate::end(stream) {
            Ok(_) => ReturnCode::Ok as _,
            Err(_) => ReturnCode::DataError as _,
        },
        None => ReturnCode::StreamError as _,
    }
}

#[export_name = prefix!(deflateReset)]
pub unsafe extern "C" fn deflateReset(strm: *mut z_stream) -> i32 {
    match DeflateStream::from_stream_mut(strm) {
        Some(stream) => zlib_rs::deflate::reset(stream) as _,
        None => ReturnCode::StreamError as _,
    }
}

#[export_name = prefix!(deflateParams)]
pub unsafe extern "C" fn deflateParams(strm: z_streamp, level: c_int, strategy: c_int) -> c_int {
    let Ok(strategy) = Strategy::try_from(strategy) else {
        return ReturnCode::StreamError as _;
    };

    match DeflateStream::from_stream_mut(strm) {
        Some(stream) => zlib_rs::deflate::params(stream, level, strategy) as _,
        None => ReturnCode::StreamError as _,
    }
}

#[export_name = prefix!(deflateSetDictionary)]
pub unsafe extern "C" fn deflateSetDictionary(
    strm: z_streamp,
    dictionary: *const Bytef,
    dictLength: uInt,
) -> c_int {
    let dictionary = core::slice::from_raw_parts(dictionary, dictLength as usize);

    match DeflateStream::from_stream_mut(strm) {
        Some(stream) => zlib_rs::deflate::set_dictionary(stream, dictionary) as _,
        None => ReturnCode::StreamError as _,
    }
}

#[export_name = prefix!(deflatePrime)]
pub unsafe extern "C" fn deflatePrime(strm: z_streamp, bits: c_int, value: c_int) -> c_int {
    match DeflateStream::from_stream_mut(strm) {
        Some(stream) => zlib_rs::deflate::prime(stream, bits, value) as _,
        None => ReturnCode::StreamError as _,
    }
}

#[export_name = prefix!(deflatePending)]
pub unsafe extern "C" fn deflatePending(
    strm: z_streamp,
    pending: *mut c_uint,
    bits: *mut c_int,
) -> c_int {
    match DeflateStream::from_stream_mut(strm) {
        Some(stream) => {
            let (current_pending, current_bits) = stream.pending();

            if !pending.is_null() {
                *pending = current_pending as c_uint;
            }

            if !bits.is_null() {
                *bits = current_bits as c_int;
            }

            ReturnCode::Ok as _
        }
        None => ReturnCode::StreamError as _,
    }
}

#[export_name = prefix!(deflateCopy)]
pub unsafe extern "C" fn deflateCopy(dest: z_streamp, source: z_streamp) -> c_int {
    let dest = if dest.is_null() {
        return ReturnCode::StreamError as _;
    } else {
        &mut *(dest as *mut MaybeUninit<_>)
    };

    match DeflateStream::from_stream_mut(source) {
        Some(source) => zlib_rs::deflate::copy(dest, source) as _,
        None => ReturnCode::StreamError as _,
    }
}

#[export_name = prefix!(deflateInit_)]
pub unsafe extern "C" fn deflateInit_(
    strm: z_streamp,
    level: c_int,
    version: *const c_char,
    stream_size: c_int,
) -> c_int {
    if !is_version_compatible(version, stream_size) {
        ReturnCode::VersionError as _
    } else if strm.is_null() {
        ReturnCode::StreamError as _
    } else {
        let stream = &mut *strm;

        if stream.zalloc.is_none() {
            stream.zalloc = DEFAULT_ZALLOC;
            stream.opaque = core::ptr::null_mut();
        }

        if stream.zfree.is_none() {
            stream.zfree = DEFAULT_ZFREE;
        }

        zlib_rs::deflate::init(stream, DeflateConfig::new(level)) as _
    }
}

#[export_name = prefix!(deflateInit2_)]
pub unsafe extern "C" fn deflateInit2_(
    strm: z_streamp,
    level: c_int,
    method: c_int,
    windowBits: c_int,
    memLevel: c_int,
    strategy: c_int,
    version: *const c_char,
    stream_size: c_int,
) -> c_int {
    if !is_version_compatible(version, stream_size) {
        ReturnCode::VersionError as _
    } else if strm.is_null() {
        ReturnCode::StreamError as _
    } else {
        let Ok(method) = Method::try_from(method) else {
            return ReturnCode::StreamError as _;
        };

        let Ok(strategy) = Strategy::try_from(strategy) else {
            return ReturnCode::StreamError as _;
        };

        let config = DeflateConfig {
            level,
            method,
            window_bits: windowBits,
            mem_level: memLevel,
            strategy,
        };

        let stream = &mut *strm;

        if stream.zalloc.is_none() {
            stream.zalloc = DEFAULT_ZALLOC;
            stream.opaque = core::ptr::null_mut();
        }

        if stream.zfree.is_none() {
            stream.zfree = DEFAULT_ZFREE;
        }

        zlib_rs::deflate::init(stream, config) as _
    }
}

#[export_name = prefix!(deflateTune)]
pub unsafe extern "C" fn deflateTune(
    strm: z_streamp,
    good_length: c_int,
    max_lazy: c_int,
    nice_length: c_int,
    max_chain: c_int,
) -> c_int {
    match DeflateStream::from_stream_mut(strm) {
        Some(stream) => zlib_rs::deflate::tune(
            stream,
            good_length as usize,
            max_lazy as usize,
            nice_length as usize,
            max_chain as usize,
        ) as _,
        None => ReturnCode::StreamError as _,
    }
}

#[export_name = prefix!(zError)]
pub const unsafe extern "C" fn error_message(err: c_int) -> *const c_char {
    match ReturnCode::try_from_c_int(err) {
        Some(return_code) => return_code.error_message(),
        None => core::ptr::null(),
    }
}

// the first part of this version specifies the zlib that we're compatible with (in terms of
// supported functions). In practice in most cases only the major version is checked, unless
// specific functions that were added later are used.
const LIBZ_RS_SYS_VERSION: &str = concat!("1.3.0-zlib-rs-", env!("CARGO_PKG_VERSION"), "\0");

unsafe fn is_version_compatible(version: *const c_char, stream_size: i32) -> bool {
    if version.is_null() {
        return false;
    }

    let expected_major_version = core::ptr::read(version);
    if expected_major_version as u8 != LIBZ_RS_SYS_VERSION.as_bytes()[0] {
        return false;
    }

    core::mem::size_of::<z_stream>() as i32 == stream_size
}

#[export_name = prefix!(zlibVersion)]
pub const extern "C" fn zlibVersion() -> *const c_char {
    LIBZ_RS_SYS_VERSION.as_ptr() as *const c_char
}
