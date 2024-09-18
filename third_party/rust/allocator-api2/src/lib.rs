//!
//! allocator-api2 crate.
//!
#![cfg_attr(not(feature = "std"), no_std)]
#![allow(unused)]

#[cfg(feature = "alloc")]
extern crate alloc as alloc_crate;

#[cfg(not(feature = "nightly"))]
mod stable;

#[cfg(feature = "nightly")]
mod nightly;

#[cfg(not(feature = "nightly"))]
pub use self::stable::*;

#[cfg(feature = "nightly")]
pub use self::nightly::*;
