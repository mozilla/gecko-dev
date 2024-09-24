pub const PAGE_KIND_SENTINEL: u32 = 1; // used in the last page, whose first_address is the end address
pub const PAGE_KIND_REGULAR: u32 = 2;
pub const PAGE_KIND_COMPRESSED: u32 = 3;

pub const OPCODE_KIND_NULL: u8 = 0;

pub const OPCODE_KIND_X86_FRAMEBASED: u8 = 1;
pub const OPCODE_KIND_X86_FRAMELESS_IMMEDIATE: u8 = 2;
pub const OPCODE_KIND_X86_FRAMELESS_INDIRECT: u8 = 3;
pub const OPCODE_KIND_X86_DWARF: u8 = 4;

pub const OPCODE_KIND_ARM64_FRAMELESS: u8 = 2;
pub const OPCODE_KIND_ARM64_DWARF: u8 = 3;
pub const OPCODE_KIND_ARM64_FRAMEBASED: u8 = 4;
