/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use crate::result::{Error, Result};

use super::BuildIdReader;
use goblin::{elf, elf::note::Note, elf::section_header::SectionHeader};
use scroll::ctx::TryFromCtx;

use log::trace;

impl BuildIdReader {
    pub fn get_build_id_bytes(&mut self, buffer: &[u8], note_name: &str) -> Result<Vec<u8>> {
        trace!("get_build_id_bytes: {}", note_name);
        let elf_head = elf::Elf::parse_header(buffer).map_err(|source| Error::Goblin {
            action: "parse Elf header",
            source,
        })?;
        let mut elf = elf::Elf::lazy_parse(elf_head).map_err(|source| Error::Goblin {
            action: "lazy parse Elf",
            source,
        })?;

        trace!("get_build_id_bytes: {:?}", elf);
        let context = goblin::container::Ctx {
            container: elf.header.container().map_err(|source| Error::Goblin {
                action: "get Elf container",
                source,
            })?,
            le: elf.header.endianness().map_err(|source| Error::Goblin {
                action: "get Elf endianness",
                source,
            })?,
        };

        trace!("get_build_id_bytes: {:?}", context);
        let section_header_bytes = self.copy_bytes(
            elf_head.e_shoff as usize,
            (elf_head.e_shnum as usize) * (elf_head.e_shentsize as usize),
        )?;

        trace!("get_build_id_bytes: {:?}", section_header_bytes);
        elf.section_headers =
            SectionHeader::parse_from(&section_header_bytes, 0, elf_head.e_shnum as usize, context)
                .map_err(|source| Error::Goblin {
                    action: "parse section headers",
                    source,
                })?;

        trace!("get_build_id_bytes: {:?}", elf.section_headers);
        let shdr_strtab = &elf.section_headers[elf_head.e_shstrndx as usize];
        let shdr_strtab_bytes =
            self.copy_bytes(shdr_strtab.sh_offset as usize, shdr_strtab.sh_size as usize)?;

        trace!("get_build_id_bytes: {:?}", shdr_strtab_bytes);
        elf.shdr_strtab =
            goblin::strtab::Strtab::parse(&shdr_strtab_bytes, 0, shdr_strtab.sh_size as usize, 0x0)
                .map_err(|source| Error::Goblin {
                    action: "parse section header string tab",
                    source,
                })?;

        trace!("get_build_id_bytes: {:?}", elf.shdr_strtab);
        let tk_note = elf
            .section_headers
            .iter()
            .find(|s| elf.shdr_strtab.get_at(s.sh_name) == Some(note_name))
            .ok_or(Error::NoteNotAvailable)?;

        trace!("get_build_id_bytes: {:?}", tk_note);
        let note_bytes = self.copy_bytes(tk_note.sh_offset as usize, tk_note.sh_size as usize)?;

        trace!("get_build_id_bytes: {:?}", note_bytes);
        let (note, _size) =
            Note::try_from_ctx(&note_bytes, (4, context)).map_err(|source| Error::Goblin {
                action: "parse note",
                source,
            })?;

        trace!("get_build_id_bytes: {:?}", note);
        Ok(note.desc.to_vec())
    }
}
