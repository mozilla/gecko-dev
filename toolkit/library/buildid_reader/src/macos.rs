/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use crate::result::{Error, Result};

use goblin::mach;

use super::BuildIdReader;

use log::trace;

const HEADER_SIZE: usize = std::mem::size_of::<goblin::mach::header::Header64>();

impl BuildIdReader {
    pub fn get_build_id_bytes(&mut self, buffer: &[u8], note_name: &str) -> Result<Vec<u8>> {
        trace!("get_build_id_bytes: {}", note_name);
        let (section, note) = note_name.split_once(",").ok_or(Error::InvalidNoteName)?;
        trace!("get_build_id_bytes: {} {}", section, note);

        let fat_header = mach::fat::FatHeader::parse(buffer).map_err(|source| Error::Goblin {
            action: "parse fat header",
            source,
        })?;
        trace!("get_build_id_bytes: fat header: {:?}", fat_header);

        /* First we attempt to parse if there's a Fat header there
         *
         * If we have one, then we have a universal binary so we are going to
         * search the architectures to find the one we want, extract the correct
         * MachO buffer as well as the offset at which the MachO binary is.
         * Testing Universal binaries will require running gtest against a
         * Shippable build.
         *
         * If not we have a normal MachO and we directly parse the buffer we
         * have read earlier, and use a 0 offset.
         */
        let (buf, main_offset): ([u8; HEADER_SIZE], usize) =
            if fat_header.magic == mach::fat::FAT_CIGAM || fat_header.magic == mach::fat::FAT_MAGIC
            {
                let total = std::mem::size_of::<mach::fat::FatHeader>() as usize
                    + (std::mem::size_of::<mach::fat::FatArch>() * fat_header.nfat_arch as usize);

                let mach_buffer = self.copy_bytes(0, total)?;

                if let mach::Mach::Fat(multi_arch) =
                    mach::Mach::parse_lossy(&mach_buffer).map_err(|source| Error::Goblin {
                        action: "parse mach binary",
                        source,
                    })?
                {
                    let arches = multi_arch.arches().map_err(|source| Error::Goblin {
                        action: "get multiarch arches",
                        source,
                    })?;

                    #[cfg(target_arch = "x86_64")]
                    let that_arch = mach::constants::cputype::CPU_TYPE_X86_64;
                    #[cfg(target_arch = "aarch64")]
                    let that_arch = mach::constants::cputype::CPU_TYPE_ARM64;

                    let arch_index = arches
                        .iter()
                        .position(|&x| x.cputype == that_arch)
                        .ok_or(Error::ArchNotAvailable)?;
                    trace!("get_build_id_bytes: arches[]: {:?}", arches[arch_index]);

                    let offset = arches[arch_index].offset as usize;

                    let b = self
                        .copy_bytes(offset, HEADER_SIZE)?
                        .try_into()
                        .expect("copy_bytes didn't copy exactly as many bytes as requested");
                    (b, offset)
                } else {
                    return Err(Error::NotFatArchive);
                }
            } else {
                (
                    buffer.try_into().map_err(|source| Error::NotEnoughData {
                        expected: HEADER_SIZE,
                        source,
                    })?,
                    0,
                )
            };
        trace!("get_build_id_bytes: {} {}", section, note);

        let macho_head = mach::header::Header64::from_bytes(&buf);
        let mut address = main_offset + HEADER_SIZE;
        let end_of_commands = address + (macho_head.sizeofcmds as usize);

        while address < end_of_commands {
            let command =
                unsafe { self.copy::<mach::load_command::LoadCommandHeader>(address as usize)? };
            trace!("get_build_id_bytes: command {:?}", command);

            if command.cmd == mach::load_command::LC_SEGMENT_64 {
                let segment =
                    unsafe { self.copy::<mach::load_command::SegmentCommand64>(address as usize)? };
                trace!("get_build_id_bytes: segment {:?}", segment);

                let name = segment.name().map_err(|source| Error::Goblin {
                    action: "get segment name",
                    source,
                })?;
                if name == section {
                    let sections_addr =
                        address + std::mem::size_of::<mach::load_command::SegmentCommand64>();
                    let sections = unsafe {
                        self.copy_array::<mach::load_command::Section64>(
                            sections_addr as usize,
                            segment.nsects as usize,
                        )?
                    };
                    trace!("get_build_id_bytes: sections {:?}", sections);

                    for section in &sections {
                        trace!("get_build_id_bytes: section {:?}", section);
                        if let Ok(sname) = Self::string_from_bytes(&section.sectname) {
                            trace!("get_build_id_bytes: sname {:?}", sname);
                            if (sname.len() == 0) || (sname != note) {
                                continue;
                            }

                            return self.copy_bytes(
                                main_offset + section.addr as usize,
                                section.size as usize,
                            );
                        }
                    }
                }
            }
            address += command.cmdsize as usize;
        }

        Err(Error::NoteNotAvailable)
    }
}
