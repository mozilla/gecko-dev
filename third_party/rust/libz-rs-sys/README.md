This crate is a C API for [zlib-rs](https://docs.rs/zlib-rs/latest/zlib_rs/). The API is broadly equivalent to [`zlib-sys`](https://docs.rs/libz-sys/latest/libz_sys/) and [`zlib-ng-sys`](https://docs.rs/libz-ng-sys/latest/libz_ng_sys/), but does not currently provide the `gz*` family of functions.

From a rust perspective, this API is not very ergonomic. Use the [`flate2`](https://crates.io/crates/flate2) crate for a more
ergonomic rust interface to zlib.

# Features

**`custom-prefix`**

Add a custom prefix to all exported symbols.

The value of the `LIBZ_RS_SYS_PREFIX` is used as a prefix for all exported symbols. For example:

```ignore
> LIBZ_RS_SYS_PREFIX="MY_CUSTOM_PREFIX" cargo build -p libz-rs-sys --features=custom-prefix
   Compiling libz-rs-sys v0.2.1 (/home/folkertdev/rust/zlib-rs/libz-rs-sys)
    Finished `dev` profile [optimized + debuginfo] target(s) in 0.21s
> objdump -tT target/debug/liblibz_rs_sys.so | grep "uncompress"
0000000000081028 l     O .got	0000000000000000              _ZN7zlib_rs7inflate10uncompress17he7d985e55c58a189E$got
000000000002c570 l     F .text	00000000000001ef              _ZN7zlib_rs7inflate10uncompress17he7d985e55c58a189E
0000000000024330 g     F .text	000000000000008e              MY_CUSTOM_PREFIXuncompress
0000000000024330 g    DF .text	000000000000008e  Base        MY_CUSTOM_PREFIXuncompress
```

**`c-allocator`, `rust-allocator`**

Pick the default allocator implementation that is used if no `zalloc` and `zfree` are configured in the input `z_stream`.

- `c-allocator`: use `malloc`/`free` for the implementation of `zalloc` and `zfree`
- `rust-allocator`: the rust global allocator for the implementation of `zalloc` and `zfree`

The `rust-allocator` is the default when this crate is used as a rust dependency, and slightly more efficient because alignment is handled by the allocator. When building a dynamic library, it may make sense to use `c-allocator` instead. 

**`std`**

Assume that `std` is available. When this feature is turned off, this crate is compatible with `#![no_std]`.

# Example

This example compresses ("deflates") the string `"Hello, World!"` and then decompresses
("inflates") it again.

```rust
let mut strm = libz_rs_sys::z_stream::default();

let version = libz_rs_sys::zlibVersion();
let stream_size = core::mem::size_of_val(&strm) as i32;

let level = 6; // the default compression level
let err = unsafe { libz_rs_sys::deflateInit_(&mut strm, level, version, stream_size) };
assert_eq!(err, libz_rs_sys::Z_OK);

let input = "Hello, World!";
strm.avail_in = input.len() as _;
strm.next_in = input.as_ptr();

let mut output = [0u8; 32];
strm.avail_out = output.len() as _;
strm.next_out = output.as_mut_ptr();

let err = unsafe { libz_rs_sys::deflate(&mut strm, libz_rs_sys::Z_FINISH) };
assert_eq!(err, libz_rs_sys::Z_STREAM_END);

let err = unsafe { libz_rs_sys::deflateEnd(&mut strm) };
assert_eq!(err, libz_rs_sys::Z_OK);

let deflated = &mut output[..strm.total_out as usize];

let mut strm = libz_rs_sys::z_stream::default();
let err = unsafe { libz_rs_sys::inflateInit_(&mut strm, version, stream_size) };
assert_eq!(err, libz_rs_sys::Z_OK);

strm.avail_in = deflated.len() as _;
strm.next_in = deflated.as_ptr();

let mut output = [0u8; 32];
strm.avail_out = output.len() as _;
strm.next_out = output.as_mut_ptr();

let err = unsafe { libz_rs_sys::inflate(&mut strm, libz_rs_sys::Z_FINISH) };
assert_eq!(err, libz_rs_sys::Z_STREAM_END);

let err = unsafe { libz_rs_sys::inflateEnd(&mut strm) };
assert_eq!(err, libz_rs_sys::Z_OK);

let inflated = &output[..strm.total_out as usize];

assert_eq!(inflated, input.as_bytes())
```

