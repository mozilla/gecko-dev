use crate::error;
use alloc::vec::Vec;
use scroll::{Pread, Pwrite, SizeWith};

use crate::pe::data_directories;
use crate::pe::options;
use crate::pe::section_table;
use crate::pe::utils;

/// Represents the TLS directory `IMAGE_TLS_DIRECTORY64`.
#[repr(C)]
#[derive(Debug, PartialEq, Copy, Clone, Default, Pread, Pwrite, SizeWith)]
pub struct ImageTlsDirectory {
    /// The starting address of the TLS raw data.
    // NOTE: `u32` for 32-bit binaries, `u64` for 64-bit binaries.
    pub start_address_of_raw_data: u64,
    /// The ending address of the TLS raw data.
    // NOTE: `u32` for 32-bit binaries, `u64` for 64-bit binaries.
    pub end_address_of_raw_data: u64,
    /// The address of the TLS index.
    // NOTE: `u32` for 32-bit binaries, `u64` for 64-bit binaries.
    pub address_of_index: u64,
    /// The address of the TLS callback functions.
    ///
    /// Terminated by a null pointer.
    // NOTE: `u32` for 32-bit binaries, `u64` for 64-bit binaries.
    pub address_of_callbacks: u64,
    /// The size of the zero fill.
    pub size_of_zero_fill: u32,
    /// The characteristics of the TLS.
    pub characteristics: u32,
}

/// TLS information.
#[derive(Debug, Clone, PartialEq, Default)]
pub struct TlsData<'a> {
    /// TLS directory.
    pub image_tls_directory: ImageTlsDirectory,
    /// Raw data of the TLS.
    pub raw_data: Option<&'a [u8]>,
    /// TLS index.
    pub slot: Option<u32>,
    /// TLS callbacks.
    pub callbacks: Vec<u64>,
}

impl ImageTlsDirectory {
    pub fn parse<T: Sized>(
        bytes: &[u8],
        dd: data_directories::DataDirectory,
        sections: &[section_table::SectionTable],
        file_alignment: u32,
    ) -> error::Result<Self> {
        Self::parse_with_opts::<T>(
            bytes,
            dd,
            sections,
            file_alignment,
            &options::ParseOptions::default(),
        )
    }

    pub fn parse_with_opts<T: Sized>(
        bytes: &[u8],
        dd: data_directories::DataDirectory,
        sections: &[section_table::SectionTable],
        file_alignment: u32,
        opts: &options::ParseOptions,
    ) -> error::Result<Self> {
        let rva = dd.virtual_address as usize;
        let mut offset =
            utils::find_offset(rva, sections, file_alignment, opts).ok_or_else(|| {
                error::Error::Malformed(format!(
                    "Cannot map ImageTlsDirectory rva {:#x} into offset",
                    rva
                ))
            })?;

        let is_64 = core::mem::size_of::<T>() == 8;

        let start_address_of_raw_data = if is_64 {
            bytes.gread_with::<u64>(&mut offset, scroll::LE)?
        } else {
            bytes.gread_with::<u32>(&mut offset, scroll::LE)? as u64
        };
        let end_address_of_raw_data = if is_64 {
            bytes.gread_with::<u64>(&mut offset, scroll::LE)?
        } else {
            bytes.gread_with::<u32>(&mut offset, scroll::LE)? as u64
        };
        let address_of_index = if is_64 {
            bytes.gread_with::<u64>(&mut offset, scroll::LE)?
        } else {
            bytes.gread_with::<u32>(&mut offset, scroll::LE)? as u64
        };
        let address_of_callbacks = if is_64 {
            bytes.gread_with::<u64>(&mut offset, scroll::LE)?
        } else {
            bytes.gread_with::<u32>(&mut offset, scroll::LE)? as u64
        };
        let size_of_zero_fill = bytes.gread_with::<u32>(&mut offset, scroll::LE)?;
        let characteristics = bytes.gread_with::<u32>(&mut offset, scroll::LE)?;

        let itd = Self {
            start_address_of_raw_data,
            end_address_of_raw_data,
            address_of_index,
            address_of_callbacks,
            size_of_zero_fill,
            characteristics,
        };

        Ok(itd)
    }
}

impl<'a> TlsData<'a> {
    pub fn parse<T: Sized>(
        bytes: &'a [u8],
        image_base: usize,
        dd: &data_directories::DataDirectory,
        sections: &[section_table::SectionTable],
        file_alignment: u32,
    ) -> error::Result<Option<Self>> {
        Self::parse_with_opts::<T>(
            bytes,
            image_base,
            dd,
            sections,
            file_alignment,
            &options::ParseOptions::default(),
        )
    }

    pub fn parse_with_opts<T: Sized>(
        bytes: &'a [u8],
        image_base: usize,
        dd: &data_directories::DataDirectory,
        sections: &[section_table::SectionTable],
        file_alignment: u32,
        opts: &options::ParseOptions,
    ) -> error::Result<Option<Self>> {
        let mut raw_data = None;
        let mut slot = None;
        let mut callbacks = Vec::new();

        let is_64 = core::mem::size_of::<T>() == 8;

        let itd =
            ImageTlsDirectory::parse_with_opts::<T>(bytes, *dd, sections, file_alignment, opts)?;

        // Parse the raw data if any
        if itd.end_address_of_raw_data != 0 && itd.start_address_of_raw_data != 0 {
            if itd.start_address_of_raw_data > itd.end_address_of_raw_data {
                return Err(error::Error::Malformed(format!(
                    "tls start_address_of_raw_data ({:#x}) is greater than end_address_of_raw_data ({:#x})",
                    itd.start_address_of_raw_data,
                    itd.end_address_of_raw_data
                )));
            }

            if (itd.start_address_of_raw_data as usize) < image_base {
                return Err(error::Error::Malformed(format!(
                    "tls start_address_of_raw_data ({:#x}) is less than image base ({:#x})",
                    itd.start_address_of_raw_data, image_base
                )));
            }

            // VA to RVA
            let rva = itd.start_address_of_raw_data as usize - image_base;
            let size = itd.end_address_of_raw_data - itd.start_address_of_raw_data;
            let offset =
                utils::find_offset(rva, sections, file_alignment, opts).ok_or_else(|| {
                    error::Error::Malformed(format!(
                        "cannot map tls start_address_of_raw_data rva ({:#x}) into offset",
                        rva
                    ))
                })?;
            raw_data = Some(&bytes[offset..offset + size as usize]);
        }

        // Parse the index if any
        if itd.address_of_index != 0 {
            if (itd.address_of_index as usize) < image_base {
                return Err(error::Error::Malformed(format!(
                    "tls address_of_index ({:#x}) is less than image base ({:#x})",
                    itd.address_of_index, image_base
                )));
            }

            // VA to RVA
            let rva = itd.address_of_index as usize - image_base;
            let offset = utils::find_offset(rva, sections, file_alignment, opts);
            slot = offset.and_then(|x| bytes.pread_with::<u32>(x, scroll::LE).ok());
        }

        // Parse the callbacks if any
        if itd.address_of_callbacks != 0 {
            if (itd.address_of_callbacks as usize) < image_base {
                return Err(error::Error::Malformed(format!(
                    "tls address_of_callbacks ({:#x}) is less than image base ({:#x})",
                    itd.address_of_callbacks, image_base
                )));
            }

            // VA to RVA
            let rva = itd.address_of_callbacks as usize - image_base;
            let offset =
                utils::find_offset(rva, sections, file_alignment, opts).ok_or_else(|| {
                    error::Error::Malformed(format!(
                        "cannot map tls address_of_callbacks rva ({:#x}) into offset",
                        rva
                    ))
                })?;
            let mut i = 0;
            // Read the callbacks until we find a null terminator
            loop {
                let callback: u64 = if is_64 {
                    bytes.pread_with::<u64>(offset + i * 8, scroll::LE)?
                } else {
                    bytes.pread_with::<u32>(offset + i * 4, scroll::LE)? as u64
                };
                if callback == 0 {
                    break;
                }
                // Each callback is an VA so convert it to RVA
                let callback_rva = callback as usize - image_base;
                // Check if the callback is in the image
                if utils::find_offset(callback_rva, sections, file_alignment, opts).is_none() {
                    return Err(error::Error::Malformed(format!(
                        "cannot map tls callback ({:#x})",
                        callback
                    )));
                }
                callbacks.push(callback);
                i += 1;
            }
        }

        Ok(Some(TlsData {
            image_tls_directory: itd,
            raw_data,
            slot,
            callbacks,
        }))
    }
}
