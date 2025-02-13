/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use std::path::PathBuf;

#[derive(Debug, thiserror::Error)]
pub enum Error {
    // NS_ERROR_INVALID_ARG
    #[error("failed to open {}: {source}", .path.display())]
    FailedToOpenFile {
        path: PathBuf,
        source: std::io::Error,
    },
    // NS_ERROR_OUT_OF_MEMORY
    #[error("failed to read buffer of size {size}: {source}")]
    FailedToRead { size: usize, source: std::io::Error },
    // NS_ERROR_NOT_AVAILABLE
    #[error("failed to convert to string from bytes: {0}")]
    StringFromBytesNulError(#[from] std::ffi::FromBytesUntilNulError),
    // NS_ERROR_NOT_AVAILABLE
    #[error("failed to convert to string from bytes: {0}")]
    StringFromBytesUtf8Error(#[from] std::str::Utf8Error),
    #[error("failed to {action}: {source}")]
    Goblin {
        action: &'static str,
        source: goblin::error::Error,
    },
    #[error("failed to copy {count} bytes at {offset}: {source}")]
    CopyBytes {
        offset: usize,
        count: usize,
        source: std::io::Error,
    },
    #[error("failed to find note")]
    NoteNotAvailable,
    // NS_ERROR_INVALID_SIGNATURE
    #[error("the note name isn't a valid form")]
    InvalidNoteName,
    #[error("failed to find architecture in fat binary")]
    ArchNotAvailable,
    // NS_ERROR_INVALID_ARG
    #[error("mach file looked like a fat archive but couldn't be parsed as one")]
    NotFatArchive,
    #[error("not enough data provided, expected at least {expected} bytes: {source}")]
    NotEnoughData {
        expected: usize,
        source: std::array::TryFromSliceError,
    },
}

pub type Result<T> = std::result::Result<T, Error>;
