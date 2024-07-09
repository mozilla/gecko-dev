//! Fixed capacity stack based generic string
//!
//! Since rust doesn't have constant generics yet `typenum` is used to allow for generic arrays (`U1` to `U255`)
//!
//! Can't outgrow initial capacity (defined at compile time), always occupies [`capacity`] `+ 1` bytes of memory
//!
//! *Doesn't allocate memory on the heap and never panics in release (all panic branches are stripped at compile time - except `Index`/`IndexMut` traits, since they are supposed to)*
//!
//! ## Why
//!
//! Data is generally bounded, you don't want a phone number with 30 characters, nor a username with 100. You probably don't even support it in your database.
//!
//! Why pay the cost of heap allocations of strings with unlimited capacity if you have limited boundaries?
//!
//! Stack based strings are generally faster to create, clone and append to than heap based strings (custom allocators and thread-locals may help with heap based ones).
//!
//! But that becomes less true as you increase the array size, 255 bytes is the maximum we accept (bigger will just wrap) and it's probably already slower than heap based strings of that size (like in `std::string::String`)
//!
//! There are other stack based strings out there, they generally can have "unlimited" capacity (heap allocate), but the stack based size is defined by the library implementor, we go through a different route by implementing a string based in a generic array.
//!
//! Array based strings always occupies the full space in memory, so they may use more memory (in the stack) than dynamic strings.
//!
//! [`capacity`]: ./struct.ArrayString.html#method.capacity
//!
//! ## Features
//!
//! **default:** `std`
//!
//! - `std` enabled by default, enables `std` compatibility - `impl Error` trait for errors (remove it to be `#[no_std]` compatible)
//! - `serde-traits` enables serde traits integration (`Serialize`/`Deserialize`)
//!
//!     Opperates like `String`, but truncates it if it's bigger than capacity
//!
//!  - `diesel-traits` enables diesel traits integration (`Insertable`/`Queryable`)
//!
//!      Opperates like `String`, but truncates it if it's bigger than capacity
//!
//! - `logs` enables internal logging
//!
//!     You will probably only need this if you are debugging this library
//!
//! ## Examples
//!
//! ```rust
//! use arraystring::{Error, ArrayString, typenum::U5, typenum::U20};
//!
//! type Username = ArrayString<U20>;
//! type Role = ArrayString<U5>;
//!
//! #[derive(Debug)]
//! pub struct User {
//!     pub username: Username,
//!     pub role: Role,
//! }
//!
//! fn main() -> Result<(), Error> {
//!     let user = User {
//!         username: Username::try_from_str("user")?,
//!         role: Role::try_from_str("admin")?
//!     };
//!     println!("{:?}", user);
//!
//!     Ok(())
//! }
//! ```
//!
//!  ## Comparisons
//! 
//! *These benchmarks ran while I streamed video and used my computer (with* **non-disclosed specs**) *as usual, so don't take the actual times too seriously, just focus on the comparison*
//! 
//! ```my_custom_benchmark
//! small-string  (23 bytes)      clone                  4.837 ns
//! small-string  (23 bytes)      try_from_str          14.777 ns
//! small-string  (23 bytes)      from_str_truncate     11.360 ns
//! small-string  (23 bytes)      from_str_unchecked    11.291 ns
//! small-string  (23 bytes)      try_push_str           1.162 ns
//! small-string  (23 bytes)      push_str               3.490 ns
//! small-string  (23 bytes)      push_str_unchecked     1.098 ns
//! -------------------------------------------------------------
//! cache-string  (63 bytes)      clone                 10.170 ns
//! cache-string  (63 bytes)      try_from_str          25.579 ns
//! cache-string  (63 bytes)      from_str_truncate     16.977 ns
//! cache-string  (63 bytes)      from_str_unchecked    17.201 ns
//! cache-string  (63 bytes)      try_push_str           1.160 ns
//! cache-string  (63 bytes)      push_str               3.486 ns
//! cache-string  (63 bytes)      push_str_unchecked     1.115 ns
//! -------------------------------------------------------------
//! max-string   (255 bytes)      clone                147.410 ns
//! max-string   (255 bytes)      try_from_str         157.340 ns
//! max-string   (255 bytes)      from_str_truncate    158.000 ns
//! max-string   (255 bytes)      from_str_unchecked   158.420 ns
//! max-string   (255 bytes)      try_push_str           1.167 ns
//! max-string   (255 bytes)      push_str               4.337 ns
//! max-string   (255 bytes)      push_str_unchecked     1.103 ns
//! -------------------------------------------------------------
//! string                        clone                 33.295 ns
//! string                        from                  32.512 ns
//! string                        push str              28.128 ns
//! -------------------------------------------------------------
//! inlinable-string (30 bytes)   clone                 16.751 ns
//! inlinable-string (30 bytes)   from_str              29.310 ns
//! inlinable-string (30 bytes)   push_str               2.865 ns
//! -------------------------------------------------------------
//! smallstring crate (20 bytes)  clone                 60.988 ns
//! smallstring crate (20 bytes)  from_str              50.233 ns
//! ```
//!
//! ## Licenses
//!
//! `MIT` and `Apache-2.0`

#![doc(html_root_url = "https://docs.rs/arraystring/0.3.0/arraystring")]
#![cfg_attr(docs_rs_workaround, feature(doc_cfg))]
#![cfg_attr(not(feature = "std"), no_std)]
#![warn(
    missing_docs,
    missing_debug_implementations,
    trivial_numeric_casts,
    unused_extern_crates,
    unused_import_braces,
    unused_qualifications,
    unused_results,
    bad_style,
    const_err,
    dead_code,
    improper_ctypes,
    legacy_directory_ownership,
    non_shorthand_field_patterns,
    no_mangle_generic_items,
    overflowing_literals,
    path_statements,
    patterns_in_fns_without_body,
    plugin_as_library,
    private_in_public,
    safe_extern_statics,
    unconditional_recursion,
    unions_with_drop_fields,
    unused_allocation,
    unused_comparisons,
    unused_parens,
    while_true
)]
#![doc(test(attr(deny(warnings))))]

pub use typenum;

/// Remove logging macros when they are disabled (at compile time)
#[macro_use]
#[cfg(not(feature = "logs"))]
#[allow(unused)]
mod mock {
    macro_rules! trace(($($x:tt)*) => ());
    macro_rules! debug(($($x:tt)*) => ());
    macro_rules! info(($($x:tt)*) => ());
    macro_rules! warn(($($x:tt)*) => ());
    macro_rules! error(($($x:tt)*) => ());
}

#[cfg(all(feature = "diesel-traits", test))]
#[macro_use]
extern crate diesel;

mod arraystring;
pub mod drain;
pub mod error;
mod generic;
mod implementations;
#[cfg(any(feature = "serde-traits", feature = "diesel-traits"))]
mod integration;
#[doc(hidden)]
pub mod utils;

/// Most used traits and data-strucutres
pub mod prelude {
    pub use crate::arraystring::ArrayString;
    pub use crate::drain::Drain;
    pub use crate::error::{OutOfBounds, Utf16, Utf8};
    pub use crate::{generic::Capacity, CacheString, MaxString, SmallString};
}

pub use crate::arraystring::ArrayString;
pub use crate::error::Error;

use crate::prelude::*;
use core::fmt::{self, Debug, Display, Formatter, Write};
use core::{borrow::Borrow, borrow::BorrowMut, ops::*};
use core::{cmp::Ordering, hash::Hash, hash::Hasher, str::FromStr};
#[cfg(feature = "logs")]
use log::trace;
use typenum::{Unsigned, U255, U63};

#[cfg(target_pointer_width="64")]
use typenum::U23;

#[cfg(target_pointer_width="32")]
use typenum::U11;

/// String with the same `mem::size_of` of a `String`
///
/// 24 bytes in 64 bits architecture
///
/// 12 bytes in 32 bits architecture (or others)
#[cfg(target_pointer_width="64")]
pub type SmallString = ArrayString<U23>;

/// String with the same `mem::size_of` of a `String`
///
/// 24 bytes in 64 bits architecture
///
/// 12 bytes in 32 bits architecture (or others)
#[cfg(not(target_pointer_width="64"))]
pub type SmallString = ArrayString<U11>;

/// Biggest array based string (255 bytes of string)
pub type MaxString = ArrayString<U255>;

/// Newtype string that occupies 64 bytes in memory and is 64 bytes aligned (full cache line)
///
/// 63 bytes of string
#[repr(align(64))]
#[derive(Copy, Clone, Default)]
pub struct CacheString(pub ArrayString<U63>);

impl CacheString {
    /// Creates new empty `CacheString`.
    ///
    /// ```rust
    /// # use arraystring::prelude::*;
    /// # let _ = env_logger::try_init();
    /// let string = CacheString::new();
    /// assert!(string.is_empty());
    /// ```
    #[inline]
    pub fn new() -> Self {
        trace!("New empty CacheString");
        Self::default()
    }

    /// Creates new `CacheString` from string slice if length is lower or equal to [`capacity`], otherwise returns an error.
    ///
    /// [`capacity`]: ./struct.CacheString.html#method.capacity
    /// ```rust
    /// # use arraystring::{error::Error, prelude::*};
    /// # fn main() -> Result<(), Error> {
    /// # let _ = env_logger::try_init();
    /// let string = CacheString::try_from_str("My String")?;
    /// assert_eq!(string.as_str(), "My String");
    ///
    /// assert_eq!(CacheString::try_from_str("")?.as_str(), "");
    ///
    /// let out_of_bounds = "0".repeat(CacheString::capacity() as usize + 1);
    /// assert!(CacheString::try_from_str(out_of_bounds).is_err());
    /// # Ok(())
    /// # }
    /// ```
    #[inline]
    pub fn try_from_str<S>(s: S) -> Result<Self, OutOfBounds>
    where
        S: AsRef<str>,
    {
        Ok(CacheString(ArrayString::try_from_str(s)?))
    }

    /// Creates new `CacheString` from string slice truncating size if bigger than [`capacity`].
    ///
    /// [`capacity`]: ./struct.CacheString.html#method.capacity
    ///
    /// ```rust
    /// # use arraystring::prelude::*;
    /// # let _ = env_logger::try_init();
    /// let string = CacheString::from_str_truncate("My String");
    /// # assert_eq!(string.as_str(), "My String");
    /// println!("{}", string);
    ///
    /// let truncate = "0".repeat(CacheString::capacity() as usize + 1);
    /// let truncated = "0".repeat(CacheString::capacity().into());
    /// let string = CacheString::from_str_truncate(&truncate);
    /// assert_eq!(string.as_str(), truncated);
    /// ```
    #[inline]
    pub fn from_str_truncate<S>(string: S) -> Self
    where
        S: AsRef<str>,
    {
        CacheString(ArrayString::from_str_truncate(string))
    }

    /// Creates new `CacheString` from string slice assuming length is appropriate.
    ///
    /// # Safety
    ///
    /// It's UB if `string.len()` > [`capacity`].
    ///
    /// [`capacity`]: ./struct.CacheString.html#method.capacity
    ///
    /// ```rust
    /// # use arraystring::prelude::*;
    /// let filled = "0".repeat(CacheString::capacity().into());
    /// let string = unsafe {
    ///     CacheString::from_str_unchecked(&filled)
    /// };
    /// assert_eq!(string.as_str(), filled.as_str());
    ///
    /// // Undefined behavior, don't do it
    /// // let out_of_bounds = "0".repeat(CacheString::capacity().into() + 1);
    /// // let ub = unsafe { CacheString::from_str_unchecked(out_of_bounds) };
    /// ```
    #[inline]
    pub unsafe fn from_str_unchecked<S>(string: S) -> Self
    where
        S: AsRef<str>,
    {
        CacheString(ArrayString::from_str_unchecked(string))
    }

    /// Creates new `CacheString` from string slice iterator if total length is lower or equal to [`capacity`], otherwise returns an error.
    ///
    /// [`capacity`]: ./struct.CacheString.html#method.capacity
    ///
    /// ```rust
    /// # use arraystring::prelude::*;
    /// # fn main() -> Result<(), OutOfBounds> {
    /// let string = CacheString::try_from_iterator(&["My String", " My Other String"][..])?;
    /// assert_eq!(string.as_str(), "My String My Other String");
    ///
    /// let out_of_bounds = (0..100).map(|_| "000");
    /// assert!(CacheString::try_from_iterator(out_of_bounds).is_err());
    /// # Ok(())
    /// # }
    /// ```
    #[inline]
    pub fn try_from_iterator<U, I>(iter: I) -> Result<Self, OutOfBounds>
    where
        U: AsRef<str>,
        I: IntoIterator<Item = U>,
    {
        Ok(CacheString(ArrayString::try_from_iterator(iter)?))
    }

    /// Creates new `CacheString` from string slice iterator truncating size if bigger than [`capacity`].
    ///
    /// [`capacity`]: ./struct.CacheString.html#method.capacity
    ///
    /// ```rust
    /// # use arraystring::prelude::*;
    /// # fn main() -> Result<(), OutOfBounds> {
    /// # let _ = env_logger::try_init();
    /// let string = CacheString::from_iterator(&["My String", " Other String"][..]);
    /// assert_eq!(string.as_str(), "My String Other String");
    ///
    /// let out_of_bounds = (0..400).map(|_| "000");
    /// let truncated = "0".repeat(CacheString::capacity().into());
    ///
    /// let truncate = CacheString::from_iterator(out_of_bounds);
    /// assert_eq!(truncate.as_str(), truncated.as_str());
    /// # Ok(())
    /// # }
    /// ```
    #[inline]
    pub fn from_iterator<U, I>(iter: I) -> Self
    where
        U: AsRef<str>,
        I: IntoIterator<Item = U>,
    {
        CacheString(ArrayString::from_iterator(iter))
    }

    /// Creates new `CacheString` from string slice iterator assuming length is appropriate.
    ///
    /// # Safety
    ///
    /// It's UB if `iter.map(|c| c.len()).sum()` > [`capacity`].
    ///
    /// [`capacity`]: ./struct.CacheString.html#method.capacity
    ///
    /// ```rust
    /// # use arraystring::prelude::*;
    /// let string = unsafe {
    ///     CacheString::from_iterator_unchecked(&["My String", " My Other String"][..])
    /// };
    /// assert_eq!(string.as_str(), "My String My Other String");
    ///
    /// // Undefined behavior, don't do it
    /// // let out_of_bounds = (0..400).map(|_| "000");
    /// // let undefined_behavior = unsafe {
    /// //     CacheString::from_iterator_unchecked(out_of_bounds)
    /// // };
    /// ```
    #[inline]
    pub unsafe fn from_iterator_unchecked<U, I>(iter: I) -> Self
    where
        U: AsRef<str>,
        I: IntoIterator<Item = U>,
    {
        CacheString(ArrayString::from_iterator_unchecked(iter))
    }

    /// Creates new `CacheString` from char iterator if total length is lower or equal to [`capacity`], otherwise returns an error.
    ///
    /// [`capacity`]: ./struct.CacheString.html#method.capacity
    ///
    /// ```rust
    /// # use arraystring::{error::Error, prelude::*};
    /// # fn main() -> Result<(), Error> {
    /// # let _ = env_logger::try_init();
    /// let string = CacheString::try_from_chars("My String".chars())?;
    /// assert_eq!(string.as_str(), "My String");
    ///
    /// let out_of_bounds = "0".repeat(CacheString::capacity() as usize + 1);
    /// assert!(CacheString::try_from_chars(out_of_bounds.chars()).is_err());
    /// # Ok(())
    /// # }
    /// ```
    #[inline]
    pub fn try_from_chars<I>(iter: I) -> Result<Self, OutOfBounds>
    where
        I: IntoIterator<Item = char>,
    {
        Ok(CacheString(ArrayString::try_from_chars(iter)?))
    }

    /// Creates new `CacheString` from char iterator truncating size if bigger than [`capacity`].
    ///
    /// [`capacity`]: ./struct.CacheString.html#method.capacity
    ///
    /// ```rust
    /// # use arraystring::prelude::*;
    /// # let _ = env_logger::try_init();
    /// let string = CacheString::from_chars("My String".chars());
    /// assert_eq!(string.as_str(), "My String");
    ///
    /// let out_of_bounds = "0".repeat(CacheString::capacity() as usize + 1);
    /// let truncated = "0".repeat(CacheString::capacity().into());
    ///
    /// let truncate = CacheString::from_chars(out_of_bounds.chars());
    /// assert_eq!(truncate.as_str(), truncated.as_str());
    /// ```
    #[inline]
    pub fn from_chars<I>(iter: I) -> Self
    where
        I: IntoIterator<Item = char>,
    {
        CacheString(ArrayString::from_chars(iter))
    }

    /// Creates new `CacheString` from char iterator assuming length is appropriate.
    ///
    /// # Safety
    ///
    /// It's UB if `iter.map(|c| c.len_utf8()).sum()` > [`capacity`].
    ///
    /// [`capacity`]: ./struct.CacheString.html#method.capacity
    ///
    /// ```rust
    /// # use arraystring::prelude::*;
    /// let string = unsafe { CacheString::from_chars_unchecked("My String".chars()) };
    /// assert_eq!(string.as_str(), "My String");
    ///
    /// // Undefined behavior, don't do it
    /// // let out_of_bounds = "000".repeat(400);
    /// // let undefined_behavior = unsafe { CacheString::from_chars_unchecked(out_of_bounds.chars()) };
    /// ```
    #[inline]
    pub unsafe fn from_chars_unchecked<I>(iter: I) -> Self
    where
        I: IntoIterator<Item = char>,
    {
        CacheString(ArrayString::from_chars_unchecked(iter))
    }

    /// Creates new `CacheString` from byte slice, returning [`Utf8`] on invalid utf-8 data or [`OutOfBounds`] if bigger than [`capacity`]
    ///
    /// [`Utf8`]: ./error/enum.Error.html#variant.Utf8
    /// [`OutOfBounds`]: ./error/enum.Error.html#variant.OutOfBounds
    /// [`capacity`]: ./struct.CacheString.html#method.capacity
    ///
    /// ```rust
    /// # use arraystring::{error::Error, prelude::*};
    /// # fn main() -> Result<(), Error> {
    /// # let _ = env_logger::try_init();
    /// let string = CacheString::try_from_utf8("My String")?;
    /// assert_eq!(string.as_str(), "My String");
    ///
    /// let invalid_utf8 = [0, 159, 146, 150];
    /// assert_eq!(CacheString::try_from_utf8(invalid_utf8), Err(Error::Utf8));
    ///
    /// let out_of_bounds = "0000".repeat(400);
    /// assert_eq!(CacheString::try_from_utf8(out_of_bounds.as_bytes()), Err(Error::OutOfBounds));
    /// # Ok(())
    /// # }
    /// ```
    #[inline]
    pub fn try_from_utf8<B>(slice: B) -> Result<Self, Error>
    where
        B: AsRef<[u8]>,
    {
        Ok(CacheString(ArrayString::try_from_utf8(slice)?))
    }

    /// Creates new `CacheString` from byte slice, returning [`Utf8`] on invalid utf-8 data, truncating if bigger than [`capacity`].
    ///
    /// [`Utf8`]: ./error/struct.Utf8.html
    /// [`capacity`]: ./struct.CacheString.html#method.capacity
    ///
    /// ```rust
    /// # use arraystring::{error::Error, prelude::*};
    /// # fn main() -> Result<(), Error> {
    /// # let _ = env_logger::try_init();
    /// let string = CacheString::from_utf8("My String")?;
    /// assert_eq!(string.as_str(), "My String");
    ///
    /// let invalid_utf8 = [0, 159, 146, 150];
    /// assert_eq!(CacheString::from_utf8(invalid_utf8), Err(Utf8));
    ///
    /// let out_of_bounds = "0".repeat(300);
    /// assert_eq!(CacheString::from_utf8(out_of_bounds.as_bytes())?.as_str(),
    ///            "0".repeat(CacheString::capacity().into()).as_str());
    /// # Ok(())
    /// # }
    /// ```
    #[inline]
    pub fn from_utf8<B>(slice: B) -> Result<Self, Utf8>
    where
        B: AsRef<[u8]>,
    {
        Ok(CacheString(ArrayString::from_utf8(slice)?))
    }

    /// Creates new `CacheString` from byte slice assuming it's utf-8 and of a appropriate size.
    ///
    /// # Safety
    ///
    /// It's UB if `slice` is not a valid utf-8 string or `slice.len()` > [`capacity`].
    ///
    /// [`capacity`]: ./struct.CacheString.html#method.capacity
    ///
    /// ```rust
    /// # use arraystring::prelude::*;
    /// let string = unsafe { CacheString::from_utf8_unchecked("My String") };
    /// assert_eq!(string.as_str(), "My String");
    ///
    /// // Undefined behavior, don't do it
    /// // let out_of_bounds = "0".repeat(300);
    /// // let ub = unsafe { CacheString::from_utf8_unchecked(out_of_bounds)) };
    /// ```
    #[inline]
    pub unsafe fn from_utf8_unchecked<B>(slice: B) -> Self
    where
        B: AsRef<[u8]>,
    {
        CacheString(ArrayString::from_utf8_unchecked(slice))
    }

    /// Creates new `CacheString` from `u16` slice, returning [`Utf16`] on invalid utf-16 data or [`OutOfBounds`] if bigger than [`capacity`]
    ///
    /// [`Utf16`]: ./error/enum.Error.html#variant.Utf16
    /// [`OutOfBounds`]: ./error/enum.Error.html#variant.OutOfBounds
    /// [`capacity`]: ./struct.CacheString.html#method.capacity
    ///
    /// ```rust
    /// # use arraystring::{error::Error, prelude::*};
    /// # fn main() -> Result<(), Error> {
    /// # let _ = env_logger::try_init();
    /// let music = [0xD834, 0xDD1E, 0x006d, 0x0075, 0x0073, 0x0069, 0x0063];
    /// let string = CacheString::try_from_utf16(music)?;
    /// assert_eq!(string.as_str(), "𝄞music");
    ///
    /// let invalid_utf16 = [0xD834, 0xDD1E, 0x006d, 0x0075, 0xD800, 0x0069, 0x0063];
    /// assert_eq!(CacheString::try_from_utf16(invalid_utf16), Err(Error::Utf16));
    ///
    /// let out_of_bounds: Vec<_> = (0..300).map(|_| 0).collect();
    /// assert_eq!(CacheString::try_from_utf16(out_of_bounds), Err(Error::OutOfBounds));
    /// # Ok(())
    /// # }
    /// ```
    #[inline]
    pub fn try_from_utf16<B>(slice: B) -> Result<Self, Error>
    where
        B: AsRef<[u16]>,
    {
        Ok(CacheString(ArrayString::try_from_utf16(slice)?))
    }

    /// Creates new `CacheString` from `u16` slice, returning [`Utf16`] on invalid utf-16 data, truncating if bigger than [`capacity`].
    ///
    /// [`Utf16`]: ./error/struct.Utf16.html
    /// [`capacity`]: ./struct.CacheString.html#method.capacity
    ///
    /// ```rust
    /// # use arraystring::{error::Error, prelude::*};
    /// # fn main() -> Result<(), Error> {
    /// # let _ = env_logger::try_init();
    /// let music = [0xD834, 0xDD1E, 0x006d, 0x0075, 0x0073, 0x0069, 0x0063];
    /// let string = CacheString::from_utf16(music)?;
    /// assert_eq!(string.as_str(), "𝄞music");
    ///
    /// let invalid_utf16 = [0xD834, 0xDD1E, 0x006d, 0x0075, 0xD800, 0x0069, 0x0063];
    /// assert_eq!(CacheString::from_utf16(invalid_utf16), Err(Utf16));
    ///
    /// let out_of_bounds: Vec<u16> = (0..300).map(|_| 0).collect();
    /// assert_eq!(CacheString::from_utf16(out_of_bounds)?.as_str(),
    ///            "\0".repeat(CacheString::capacity().into()).as_str());
    /// # Ok(())
    /// # }
    /// ```
    #[inline]
    pub fn from_utf16<B>(slice: B) -> Result<Self, Utf16>
    where
        B: AsRef<[u16]>,
    {
        Ok(CacheString(ArrayString::from_utf16(slice)?))
    }

    /// Creates new `CacheString` from `u16` slice, replacing invalid utf-16 data with `REPLACEMENT_CHARACTER` (\u{FFFD}) and truncating size if bigger than [`capacity`]
    ///
    /// [`capacity`]: ./struct.CacheString.html#method.capacity
    ///
    /// ```rust
    /// # use arraystring::{error::Error, prelude::*};
    /// # fn main() -> Result<(), Error> {
    /// # let _ = env_logger::try_init();
    /// let music = [0xD834, 0xDD1E, 0x006d, 0x0075, 0x0073, 0x0069, 0x0063];
    /// let string = CacheString::from_utf16_lossy(music);
    /// assert_eq!(string.as_str(), "𝄞music");
    ///
    /// let invalid_utf16 = [0xD834, 0xDD1E, 0x006d, 0x0075, 0xD800, 0x0069, 0x0063];
    /// assert_eq!(CacheString::from_utf16_lossy(invalid_utf16).as_str(), "𝄞mu\u{FFFD}ic");
    ///
    /// let out_of_bounds: Vec<u16> = (0..300).map(|_| 0).collect();
    /// assert_eq!(CacheString::from_utf16_lossy(&out_of_bounds).as_str(),
    ///            "\0".repeat(CacheString::capacity().into()).as_str());
    /// # Ok(())
    /// # }
    /// ```
    #[inline]
    pub fn from_utf16_lossy<B>(slice: B) -> Self
    where
        B: AsRef<[u16]>,
    {
        CacheString(ArrayString::from_utf16_lossy(slice))
    }

    /// Returns maximum string capacity, defined at compile time, it will never change
    ///
    /// Should always return 63 bytes
    ///
    /// ```rust
    /// # use arraystring::prelude::*;
    /// # let _ = env_logger::try_init();
    /// assert_eq!(CacheString::capacity(), 63);
    /// ```
    #[inline]
    pub fn capacity() -> u8 {
        <U63 as Unsigned>::to_u8()
    }

    /// Splits `CacheString` in two if `at` is smaller than `self.len()`.
    ///
    /// Returns [`Utf8`] if `at` does not lie at a valid utf-8 char boundary and [`OutOfBounds`] if it's out of bounds
    ///
    /// [`OutOfBounds`]: ./error/enum.Error.html#variant.OutOfBounds
    /// [`Utf8`]: ./error/enum.Error.html#variant.Utf8
    ///
    /// ```rust
    /// # use arraystring::{error::Error, prelude::*};
    /// # fn main() -> Result<(), Error> {
    /// # let _ = env_logger::try_init();
    /// let mut s = CacheString::try_from_str("AB🤔CD")?;
    /// assert_eq!(s.split_off(6)?.as_str(), "CD");
    /// assert_eq!(s.as_str(), "AB🤔");
    /// assert_eq!(s.split_off(20), Err(Error::OutOfBounds));
    /// assert_eq!(s.split_off(4), Err(Error::Utf8));
    /// # Ok(())
    /// # }
    /// ```
    #[inline]
    pub fn split_off(&mut self, at: u8) -> Result<Self, Error> {
        Ok(CacheString(self.0.split_off(at)?))
    }
}

impl Debug for CacheString {
    #[inline]
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        f.debug_tuple("CacheString").field(&self.0).finish()
    }
}

impl Hash for CacheString {
    #[inline]
    fn hash<H: Hasher>(&self, hasher: &mut H) {
        self.0.hash(hasher);
    }
}

impl PartialEq for CacheString {
    #[inline]
    fn eq(&self, other: &Self) -> bool {
        self.0.eq(&other.0)
    }
}
impl Eq for CacheString {}

impl Ord for CacheString {
    #[inline]
    fn cmp(&self, other: &Self) -> Ordering {
        self.0.cmp(&other.0)
    }
}

impl PartialOrd for CacheString {
    #[inline]
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Deref for CacheString {
    type Target = ArrayString<U63>;

    #[inline]
    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl DerefMut for CacheString {
    #[inline]
    fn deref_mut(&mut self) -> &mut ArrayString<U63> {
        &mut self.0
    }
}

impl Display for CacheString {
    #[inline]
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        Display::fmt(&self.0, f)
    }
}

impl AsRef<str> for CacheString {
    #[inline]
    fn as_ref(&self) -> &str {
        self.0.as_ref()
    }
}

impl AsMut<str> for CacheString {
    #[inline]
    fn as_mut(&mut self) -> &mut str {
        self.0.as_mut()
    }
}

impl AsRef<[u8]> for CacheString {
    #[inline]
    fn as_ref(&self) -> &[u8] {
        self.0.as_ref()
    }
}

impl FromStr for CacheString {
    type Err = OutOfBounds;

    #[inline]
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Ok(CacheString(ArrayString::try_from_str(s)?))
    }
}

impl<'a, 'b> PartialEq<str> for CacheString {
    #[inline]
    fn eq(&self, other: &str) -> bool {
        self.0.eq(other)
    }
}

impl Borrow<str> for CacheString {
    #[inline]
    fn borrow(&self) -> &str {
        self.0.borrow()
    }
}
impl BorrowMut<str> for CacheString {
    #[inline]
    fn borrow_mut(&mut self) -> &mut str {
        self.0.borrow_mut()
    }
}

impl<'a> Add<&'a str> for CacheString {
    type Output = Self;

    #[inline]
    fn add(self, other: &str) -> Self::Output {
        CacheString(self.0.add(other))
    }
}

impl Write for CacheString {
    #[inline]
    fn write_str(&mut self, slice: &str) -> fmt::Result {
        self.0.write_str(slice)
    }
}

impl From<ArrayString<U63>> for CacheString {
    fn from(array: ArrayString<U63>) -> Self {
        CacheString(array)
    }
}
