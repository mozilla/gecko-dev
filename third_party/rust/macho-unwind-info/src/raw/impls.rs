use std::fmt::Debug;

use super::format::{
    CompactUnwindInfoHeader, CompressedPage, Opcode, PageEntry, RegularFunctionEntry, RegularPage,
};
use super::unaligned::U32;
use crate::error::ReadError;
use crate::num_display::HexNum;
use crate::reader::Reader;

type Result<T> = std::result::Result<T, ReadError>;

impl CompactUnwindInfoHeader {
    pub fn parse(data: &[u8]) -> Result<&Self> {
        data.read_at::<CompactUnwindInfoHeader>(0)
            .ok_or(ReadError::Header)
    }

    pub fn global_opcodes_offset(&self) -> u32 {
        self.global_opcodes_offset.into()
    }

    pub fn global_opcodes_len(&self) -> u32 {
        self.global_opcodes_len.into()
    }

    pub fn pages_offset(&self) -> u32 {
        self.pages_offset.into()
    }

    pub fn pages_len(&self) -> u32 {
        self.pages_len.into()
    }

    /// Return the list of global opcodes.
    pub fn global_opcodes<'data>(&self, data: &'data [u8]) -> Result<&'data [Opcode]> {
        data.read_slice_at::<Opcode>(
            self.global_opcodes_offset().into(),
            self.global_opcodes_len() as usize,
        )
        .ok_or(ReadError::GlobalOpcodes)
    }

    /// Return the list of pages.
    pub fn pages<'data>(&self, data: &'data [u8]) -> Result<&'data [PageEntry]> {
        data.read_slice_at::<PageEntry>(self.pages_offset().into(), self.pages_len() as usize)
            .ok_or(ReadError::Pages)
    }
}

impl RegularPage {
    pub fn parse(data: &[u8], page_offset: u64) -> Result<&Self> {
        data.read_at::<Self>(page_offset)
            .ok_or(ReadError::RegularPage)
    }

    pub fn functions_offset(&self) -> u16 {
        self.functions_offset.into()
    }

    pub fn functions_len(&self) -> u16 {
        self.functions_len.into()
    }

    pub fn functions<'data>(
        &self,
        data: &'data [u8],
        page_offset: u32,
    ) -> Result<&'data [RegularFunctionEntry]> {
        let relative_functions_offset = self.functions_offset();
        let functions_len: usize = self.functions_len().into();
        let functions_offset = page_offset as u64 + relative_functions_offset as u64;
        data.read_slice_at::<RegularFunctionEntry>(functions_offset, functions_len)
            .ok_or(ReadError::RegularPageFunctions)
    }
}

impl CompressedPage {
    pub fn parse(data: &[u8], page_offset: u64) -> Result<&Self> {
        data.read_at::<Self>(page_offset)
            .ok_or(ReadError::CompressedPage)
    }

    pub fn functions_offset(&self) -> u16 {
        self.functions_offset.into()
    }

    pub fn functions_len(&self) -> u16 {
        self.functions_len.into()
    }

    pub fn local_opcodes_offset(&self) -> u16 {
        self.local_opcodes_offset.into()
    }

    pub fn local_opcodes_len(&self) -> u16 {
        self.local_opcodes_len.into()
    }

    pub fn functions<'data>(&self, data: &'data [u8], page_offset: u32) -> Result<&'data [U32]> {
        let relative_functions_offset = self.functions_offset();
        let functions_len: usize = self.functions_len().into();
        let functions_offset = page_offset as u64 + relative_functions_offset as u64;
        data.read_slice_at::<U32>(functions_offset, functions_len)
            .ok_or(ReadError::CompressedPageFunctions)
    }

    /// Return the list of local opcodes.
    pub fn local_opcodes<'data>(
        &self,
        data: &'data [u8],
        page_offset: u32,
    ) -> Result<&'data [Opcode]> {
        let relative_local_opcodes_offset = self.local_opcodes_offset();
        let local_opcodes_len: usize = self.local_opcodes_len().into();
        let local_opcodes_offset = page_offset as u64 + relative_local_opcodes_offset as u64;
        data.read_slice_at::<Opcode>(local_opcodes_offset, local_opcodes_len)
            .ok_or(ReadError::LocalOpcodes)
    }
}

impl Opcode {
    pub fn opcode(&self) -> u32 {
        self.0.into()
    }
}

impl RegularFunctionEntry {
    pub fn address(&self) -> u32 {
        self.address.into()
    }

    pub fn opcode(&self) -> u32 {
        self.opcode.opcode()
    }
}

impl PageEntry {
    pub fn page_offset(&self) -> u32 {
        self.page_offset.into()
    }

    pub fn first_address(&self) -> u32 {
        self.first_address.into()
    }

    pub fn lsda_index_offset(&self) -> u32 {
        self.lsda_index_offset.into()
    }

    pub fn page_kind(&self, data: &[u8]) -> Result<u32> {
        let kind = *data
            .read_at::<U32>(self.page_offset().into())
            .ok_or(ReadError::PageKind)?;
        Ok(kind.into())
    }
}

impl Debug for PageEntry {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("PageEntry")
            .field("first_address", &HexNum(self.first_address()))
            .field("page_offset", &HexNum(self.page_offset()))
            .field("lsda_index_offset", &HexNum(self.lsda_index_offset()))
            .finish()
    }
}
