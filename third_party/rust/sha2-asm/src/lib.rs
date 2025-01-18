//! Assembly implementation of the [SHA-2] compression functions.
//!
//! This crate is not intended for direct use, most users should
//! prefer the [`sha2`] crate with enabled `asm` feature instead.
//!
//! Only x86, x86-64, and (partially) AArch64 architectures are
//! currently supported.
//!
//! [SHA-2]: https://en.wikipedia.org/wiki/SHA-2
//! [`sha2`]: https://crates.io/crates/sha2

#![no_std]
#[cfg(not(any(target_arch = "x86_64", target_arch = "x86", target_arch = "aarch64")))]
compile_error!("crate can only be used on x86, x86-64 and aarch64 architectures");

#[cfg(target_os = "windows")]
compile_error!("crate does not support Windows targets");

#[link(name = "sha256", kind = "static")]
extern "C" {
    fn sha256_compress(state: &mut [u32; 8], block: &[u8; 64]);
}

/// Safe wrapper around assembly implementation of SHA256 compression function
#[inline]
pub fn compress256(state: &mut [u32; 8], blocks: &[[u8; 64]]) {
    for block in blocks {
        unsafe { sha256_compress(state, block) }
    }
}

#[cfg(not(target_arch = "aarch64"))]
#[link(name = "sha512", kind = "static")]
extern "C" {
    fn sha512_compress(state: &mut [u64; 8], block: &[u8; 128]);
}

/// Safe wrapper around assembly implementation of SHA512 compression function
///
/// This function is available only on x86 and x86-64 targets.
#[cfg(not(target_arch = "aarch64"))]
#[inline]
pub fn compress512(state: &mut [u64; 8], blocks: &[[u8; 128]]) {
    for block in blocks {
        unsafe { sha512_compress(state, block) }
    }
}
