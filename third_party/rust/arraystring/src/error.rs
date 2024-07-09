//! Contains all of this crate errors

use core::fmt::{self, Debug, Display, Formatter};
use core::{char::DecodeUtf16Error, hash::Hash, hash::Hasher, str::EncodeUtf16, str::Utf8Error};
#[cfg(feature = "logs")]
use log::trace;

/// Every error possible when using [`ArrayString`]
///
/// [`ArrayString`]: ../struct.ArrayString.html
#[derive(Copy, Clone)]
pub enum Error {
    /// Conversion between available byte slice and UTF-8 failed (invalid data or invalid utf-8 character index)
    Utf8,
    /// Conversion between available `u16` slice and string failed
    Utf16,
    /// Out of boundaries access
    OutOfBounds,
}

impl PartialEq for Error {
    #[inline]
    fn eq(&self, other: &Self) -> bool {
        use self::Error::*;
        match (self, other) {
            (Utf8, Utf8) | (Utf16, Utf16) | (OutOfBounds, OutOfBounds) => true,
            _ => false,
        }
    }
}

impl Eq for Error {}

impl Hash for Error {
    #[inline]
    fn hash<H: Hasher>(&self, hasher: &mut H) {
        match self {
            Error::Utf8 => "Utf8".hash(hasher),
            Error::Utf16 => "Utf16".hash(hasher),
            Error::OutOfBounds => "OutOfBounds".hash(hasher),
        }
    }
}

impl Display for Error {
    #[inline]
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        match self {
            Error::Utf8 => write!(f, "Utf8"),
            Error::Utf16 => write!(f, "Utf16"),
            Error::OutOfBounds => write!(f, "OutOfBounds"),
        }
    }
}

impl Debug for Error {
    #[inline]
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        match self {
            Error::Utf8 => write!(f, "Error::Utf8"),
            Error::Utf16 => write!(f, "Error::Utf16"),
            Error::OutOfBounds => write!(f, "Error::OutOfBounds"),
        }
    }
}

#[cfg(feature = "std")]
impl std::error::Error for Error {}

impl From<Utf8Error> for Error {
    #[inline]
    fn from(_: Utf8Error) -> Self {
        Error::Utf8
    }
}

impl From<DecodeUtf16Error> for Error {
    #[inline]
    fn from(_: DecodeUtf16Error) -> Self {
        Error::Utf16
    }
}

impl<'a> From<EncodeUtf16<'a>> for Error {
    #[inline]
    fn from(_: EncodeUtf16) -> Self {
        Error::Utf16
    }
}

/// Error caused by invalid UTF-8 data
#[derive(Copy, Clone, Default, PartialEq, Eq)]
pub struct Utf8;

impl Debug for Utf8 {
    #[inline]
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        write!(f, "Utf8")
    }
}

impl Display for Utf8 {
    #[inline]
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        write!(f, "Utf8")
    }
}

#[cfg(feature = "std")]
impl std::error::Error for Utf8 {}

impl From<Utf8Error> for Utf8 {
    #[inline]
    fn from(_: Utf8Error) -> Self {
        Utf8
    }
}

impl From<Utf8> for Error {
    #[inline]
    fn from(_: Utf8) -> Self {
        trace!("From Utf8");
        Error::Utf8
    }
}

/// Error caused by invalid UTF-16 data
#[derive(Copy, Clone, Default, PartialEq, Eq)]
pub struct Utf16;

impl Debug for Utf16 {
    #[inline]
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        write!(f, "Utf16")
    }
}

#[cfg(feature = "std")]
impl std::error::Error for Utf16 {}

impl Display for Utf16 {
    #[inline]
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        write!(f, "Utf16")
    }
}

impl From<Utf16> for Error {
    #[inline]
    fn from(_: Utf16) -> Self {
        trace!("From Utf16");
        Error::Utf16
    }
}

impl From<DecodeUtf16Error> for Utf16 {
    #[inline]
    fn from(_: DecodeUtf16Error) -> Self {
        Utf16
    }
}

impl<'a> From<EncodeUtf16<'a>> for Utf16 {
    #[inline]
    fn from(_: EncodeUtf16) -> Self {
        Utf16
    }
}

/// Error caused by out of bounds access to [`ArrayString`]
///
/// [`ArrayString`]: ../struct.ArrayString.html
#[derive(Copy, Clone, Default, PartialEq, Eq)]
pub struct OutOfBounds;

impl Debug for OutOfBounds {
    #[inline]
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        write!(f, "OutOfBounds")
    }
}

impl Display for OutOfBounds {
    #[inline]
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        write!(f, "OutOfBounds")
    }
}

#[cfg(feature = "std")]
impl std::error::Error for OutOfBounds {}

impl From<OutOfBounds> for Error {
    #[inline]
    fn from(_: OutOfBounds) -> Self {
        trace!("From OutOfBounds");
        Error::OutOfBounds
    }
}
