/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use crate::result::{Error, Result};
use log::{error, trace};
use std::ffi::CStr;
use std::fs::File;
use std::io::{Read, Result as IOResult, Seek, SeekFrom};
use std::path::Path;

pub mod result;

#[cfg(target_os = "windows")]
mod windows;

#[cfg(any(target_os = "macos", target_os = "ios"))]
mod macos;

// Target Android, Linux, *BSD, Solaris, ... Anything using ELF
#[cfg(not(any(target_os = "windows", target_os = "macos", target_os = "ios")))]
mod elf;

pub struct BuildIdReader {
    file: File,
}

#[cfg(target_os = "windows")]
const MAX_BUFFER_READ: usize = std::mem::size_of::<goblin::pe::header::Header>();

#[cfg(any(target_os = "macos", target_os = "ios"))]
const MAX_BUFFER_READ: usize = std::mem::size_of::<goblin::mach::header::Header64>();

// Target Android, Linux, *BSD, Solaris, ... Anything using ELF
#[cfg(not(any(target_os = "windows", target_os = "macos", target_os = "ios")))]
const MAX_BUFFER_READ: usize = std::mem::size_of::<goblin::elf::header::Header>();

impl BuildIdReader {
    pub fn new(filename: &Path) -> Result<Self> {
        trace!("BuildIdReader::new {:?}", filename);
        let f = File::open(filename).map_err(|source| Error::FailedToOpenFile {
            path: filename.into(),
            source,
        })?;
        Ok(BuildIdReader { file: f })
    }

    fn read_raw_build_id(&mut self, note_name: &str) -> Result<Vec<u8>> {
        trace!("BuildIdReader::read_raw_build_id {}", note_name);
        let mut buffer = [0; MAX_BUFFER_READ];
        let _ = self
            .file
            .read_exact(&mut buffer)
            .map_err(|source| Error::FailedToRead {
                size: MAX_BUFFER_READ,
                source,
            })?;

        // This does actually depend on the platform, so it's not in this
        // impl nor source file but in the platform-dependant modules listed at
        // the end of this file
        self.get_build_id_bytes(&buffer, note_name)
    }

    pub fn read_string_build_id(&mut self, note_name: &str) -> Result<String> {
        trace!("BuildIdReader::read_string_build_id {}", note_name);
        let b = self.read_raw_build_id(note_name).map_err(|err| {
            error!(
                "BuildIdReader::read_string_build_id failed to read raw build id with error {}",
                err
            );
            err
        })?;
        Self::string_from_bytes(&b)
    }

    fn string_from_bytes(bytes: &[u8]) -> Result<String> {
        trace!("BuildIdReader::string_from_bytes {:?}", bytes);
        Ok(CStr::from_bytes_until_nul(bytes)?.to_str()?.to_string())
    }

    fn copy_bytes_into(&mut self, offset: usize, buffer: &mut [u8]) -> IOResult<()> {
        trace!("BuildIdReader::copy_bytes_into @{}", offset);
        self.file.seek(SeekFrom::Start(offset as u64))?;
        self.file.read_exact(buffer)
    }

    pub fn copy_bytes(&mut self, offset: usize, count: usize) -> Result<Vec<u8>> {
        trace!("BuildIdReader::copy_bytes @{} : {} bytes", offset, count);
        let mut buf = vec![0; count];
        self.copy_bytes_into(offset, &mut buf)
            .map_err(|source| Error::CopyBytes {
                offset,
                count,
                source,
            })?;
        Ok(buf)
    }

    #[cfg(any(target_os = "macos", target_os = "ios"))]
    /// SAFETY: Caller need to ensure that `T` is safe to cast to bytes
    pub unsafe fn copy<T>(&mut self, offset: usize) -> Result<T> {
        trace!("BuildIdReader::copy @{}", offset);
        self.copy_array(offset, 1)
            .map(|v| v.into_iter().next().unwrap())
    }

    #[cfg(any(target_os = "macos", target_os = "ios"))]
    /// SAFETY: Caller need to ensure that `T` is safe to cast to bytes
    pub unsafe fn copy_array<T>(&mut self, offset: usize, num: usize) -> Result<Vec<T>> {
        trace!("BuildIdReader::copy_array @{} : {} num", offset, num);
        let mut uninit: Vec<std::mem::MaybeUninit<T>> = Vec::with_capacity(num);
        for _ in 0..num {
            uninit.push(std::mem::MaybeUninit::uninit());
        }
        let slice = std::slice::from_raw_parts_mut(
            uninit.as_mut_ptr() as *mut u8,
            uninit.len() * std::mem::size_of::<T>(),
        );
        self.copy_bytes_into(offset, slice)
            .map_err(|source| Error::CopyBytes {
                offset,
                count: slice.len(),
                source,
            })?;
        Ok(std::mem::transmute(uninit))
    }
}
