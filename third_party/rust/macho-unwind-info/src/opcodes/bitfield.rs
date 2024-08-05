use crate::num_display::BinNum;
use std::fmt::Debug;

pub struct OpcodeBitfield(pub u32);

impl OpcodeBitfield {
    pub fn new(value: u32) -> Self {
        Self(value)
    }

    /// Whether this instruction is the start of a function.
    pub fn is_function_start(&self) -> bool {
        self.0 >> 31 == 1
    }

    /// Whether there is an lsda entry for this instruction.
    pub fn has_lsda(&self) -> bool {
        (self.0 >> 30) & 0b1 == 1
    }

    /// An index into the global personalities array
    /// (TODO: ignore if has_lsda() == false?)
    pub fn personality_index(&self) -> u8 {
        ((self.0 >> 28) & 0b11) as u8
    }

    /// The architecture-specific kind of opcode this is, specifying how to
    /// interpret the remaining 24 bits of the opcode.
    pub fn kind(&self) -> u8 {
        ((self.0 >> 24) & 0b1111) as u8
    }

    /// The architecture-specific remaining 24 bits.
    pub fn specific_bits(&self) -> u32 {
        self.0 & 0xffffff
    }
}

impl From<u32> for OpcodeBitfield {
    fn from(opcode: u32) -> OpcodeBitfield {
        OpcodeBitfield::new(opcode)
    }
}

impl Debug for OpcodeBitfield {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Opcode")
            .field("kind", &self.kind())
            .field("is_function_start", &self.is_function_start())
            .field("has_lsda", &self.has_lsda())
            .field("personality_index", &self.personality_index())
            .field("specific_bits", &BinNum(self.specific_bits()))
            .finish()
    }
}
