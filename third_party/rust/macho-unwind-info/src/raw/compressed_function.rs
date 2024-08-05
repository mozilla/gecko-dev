use crate::num_display::HexNum;
use std::fmt::Debug;

/// Allows accessing the two packed values from a "compressed" function entry.
#[derive(Clone, Copy, PartialEq, Eq)]
pub struct CompressedFunctionEntry(pub u32);

/// Entries are a u32 that contains two packed values (from high to low):
/// * 8 bits: opcode index
/// * 24 bits: function address
impl CompressedFunctionEntry {
    /// Wrap the u32.
    pub fn new(value: u32) -> Self {
        Self(value)
    }

    /// The opcode index.
    ///   * 0..global_opcodes_len => index into global palette
    ///   * global_opcodes_len..255 => index into local palette
    ///     (subtract global_opcodes_len to get the real local index)
    pub fn opcode_index(&self) -> u8 {
        (self.0 >> 24) as u8
    }

    /// The function address, relative to the page's first_address.
    pub fn relative_address(&self) -> u32 {
        self.0 & 0xffffff
    }
}

impl From<u32> for CompressedFunctionEntry {
    fn from(entry: u32) -> CompressedFunctionEntry {
        CompressedFunctionEntry::new(entry)
    }
}

impl Debug for CompressedFunctionEntry {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("CompressedFunctionEntry")
            .field("opcode_index", &HexNum(self.opcode_index()))
            .field("relative_address", &HexNum(self.relative_address()))
            .finish()
    }
}
