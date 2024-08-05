// Copyright 2015 Ted Mielczarek. See the COPYRIGHT
// file at the top-level directory of this distribution.

//! General-purpose I/O routines.

use std::io;
use std::io::prelude::*;

/// Shorthand for Read + Seek
pub trait Readable: Read + Seek {}
impl<T: Read + Seek> Readable for T {}

/// Format `bytes` to `f` as a hex string.
pub fn write_bytes<T: Write>(f: &mut T, bytes: &[u8]) -> io::Result<()> {
    for b in bytes {
        write!(f, "{b:02x}")?;
    }
    Ok(())
}
