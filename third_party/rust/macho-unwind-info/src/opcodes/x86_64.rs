use std::fmt::Display;

use super::bitfield::OpcodeBitfield;
use super::permutation::decode_permutation_6;
use crate::consts::*;

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum RegisterNameX86_64 {
    Rbx,
    R12,
    R13,
    R14,
    R15,
    Rbp,
}

impl RegisterNameX86_64 {
    pub fn parse(n: u8) -> Option<Self> {
        match n {
            1 => Some(RegisterNameX86_64::Rbx),
            2 => Some(RegisterNameX86_64::R12),
            3 => Some(RegisterNameX86_64::R13),
            4 => Some(RegisterNameX86_64::R14),
            5 => Some(RegisterNameX86_64::R15),
            6 => Some(RegisterNameX86_64::Rbp),
            _ => None,
        }
    }

    pub fn dwarf_name(&self) -> &'static str {
        match self {
            RegisterNameX86_64::Rbx => "reg3",
            RegisterNameX86_64::R12 => "reg12",
            RegisterNameX86_64::R13 => "reg13",
            RegisterNameX86_64::R14 => "reg14",
            RegisterNameX86_64::R15 => "reg15",
            RegisterNameX86_64::Rbp => "reg6",
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum OpcodeX86_64 {
    Null,
    FrameBased {
        stack_offset_in_bytes: u16,
        saved_regs: [Option<RegisterNameX86_64>; 5],
    },
    FramelessImmediate {
        stack_size_in_bytes: u16,
        saved_regs: [Option<RegisterNameX86_64>; 6],
    },
    FramelessIndirect {
        /// Offset from the start of the function into the middle of a `sub`
        /// instruction, pointing right at the instruction's "immediate" which
        /// is a u32 value with the offset we need. (NOTE: not divided by anything!)
        /// Example:
        ///   - function_start is 0x1c20
        ///   - immediate_offset_from_function_start is 13 (= 0xd),
        ///   - there's sub instruction at 0x1c2a: sub rsp, 0xc28.
        ///   This instruction is encoded as 48 81 EC 28 0C 00 00, with the 28
        ///   byte at 0x1c2d (= 0x1c20 + 13). The immediate is 28 0C 00 00,
        ///   interpreted as a little-endian u32: 0xc28.
        immediate_offset_from_function_start: u8,

        /// An offset to add to the loaded stack size.
        /// This allows the stack size to differ slightly from the `sub`, to
        /// compensate for any function prologue that pushes a bunch of
        /// pointer-sized registers. This adjust value includes the return
        /// address on the stack. For example, if the function begins with six push
        /// instructions, followed by a sub instruction, then stack_adjust_in_bytes
        /// is 56: 8 bytes for the return address + 6 * 8 for each pushed register.
        stack_adjust_in_bytes: u8,

        /// The registers, in the order that they need to be popped in when
        /// returning / unwinding from this function. (Reverse order from
        /// function prologue!)
        /// Can have leading `None`s.
        saved_regs: [Option<RegisterNameX86_64>; 6],
    },
    Dwarf {
        eh_frame_fde: u32,
    },
    InvalidFrameless,
    UnrecognizedKind(u8),
}

impl OpcodeX86_64 {
    pub fn parse(opcode: u32) -> Self {
        match OpcodeBitfield::new(opcode).kind() {
            OPCODE_KIND_NULL => OpcodeX86_64::Null,
            OPCODE_KIND_X86_FRAMEBASED => OpcodeX86_64::FrameBased {
                stack_offset_in_bytes: (((opcode >> 16) & 0xff) as u16) * 8,
                saved_regs: [
                    RegisterNameX86_64::parse(((opcode >> 12) & 0b111) as u8),
                    RegisterNameX86_64::parse(((opcode >> 9) & 0b111) as u8),
                    RegisterNameX86_64::parse(((opcode >> 6) & 0b111) as u8),
                    RegisterNameX86_64::parse(((opcode >> 3) & 0b111) as u8),
                    RegisterNameX86_64::parse((opcode & 0b111) as u8),
                ],
            },
            OPCODE_KIND_X86_FRAMELESS_IMMEDIATE => {
                let stack_size_in_bytes = (((opcode >> 16) & 0xff) as u16) * 8;
                let register_count = (opcode >> 10) & 0b111;
                let register_permutation = opcode & 0b11_1111_1111;
                let saved_registers =
                    match decode_permutation_6(register_count, register_permutation) {
                        Ok(regs) => regs,
                        Err(_) => return OpcodeX86_64::InvalidFrameless,
                    };
                OpcodeX86_64::FramelessImmediate {
                    stack_size_in_bytes,
                    saved_regs: [
                        RegisterNameX86_64::parse(saved_registers[0]),
                        RegisterNameX86_64::parse(saved_registers[1]),
                        RegisterNameX86_64::parse(saved_registers[2]),
                        RegisterNameX86_64::parse(saved_registers[3]),
                        RegisterNameX86_64::parse(saved_registers[4]),
                        RegisterNameX86_64::parse(saved_registers[5]),
                    ],
                }
            }
            OPCODE_KIND_X86_FRAMELESS_INDIRECT => {
                let immediate_offset_from_function_start = (opcode >> 16) as u8;
                let stack_adjust_in_bytes = ((opcode >> 13) & 0b111) as u8 * 8;
                let register_count = (opcode >> 10) & 0b111;
                let register_permutation = opcode & 0b11_1111_1111;
                let saved_registers =
                    match decode_permutation_6(register_count, register_permutation) {
                        Ok(regs) => regs,
                        Err(_) => return OpcodeX86_64::InvalidFrameless,
                    };
                OpcodeX86_64::FramelessIndirect {
                    immediate_offset_from_function_start,
                    stack_adjust_in_bytes,
                    saved_regs: [
                        RegisterNameX86_64::parse(saved_registers[0]),
                        RegisterNameX86_64::parse(saved_registers[1]),
                        RegisterNameX86_64::parse(saved_registers[2]),
                        RegisterNameX86_64::parse(saved_registers[3]),
                        RegisterNameX86_64::parse(saved_registers[4]),
                        RegisterNameX86_64::parse(saved_registers[5]),
                    ],
                }
            }
            OPCODE_KIND_X86_DWARF => OpcodeX86_64::Dwarf {
                eh_frame_fde: (opcode & 0xffffff),
            },
            kind => OpcodeX86_64::UnrecognizedKind(kind),
        }
    }
}

impl Display for OpcodeX86_64 {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            OpcodeX86_64::Null => {
                write!(f, "(uncovered)")?;
            }
            OpcodeX86_64::FrameBased {
                stack_offset_in_bytes,
                saved_regs,
            } => {
                // rbp was set to rsp before the saved registers were pushed.
                // The first pushed register is at rbp - 8 (== CFA - 24), the last at rbp - stack_offset_in_bytes.
                write!(f, "CFA=reg6+16: reg6=[CFA-16], reg16=[CFA-8]")?;
                let max_count = (*stack_offset_in_bytes / 8) as usize;
                let mut offset = *stack_offset_in_bytes + 16; // + 2 for rbp, return address
                for reg in saved_regs.iter().rev().take(max_count) {
                    if let Some(reg) = reg {
                        write!(f, ", {}=[CFA-{}]", reg.dwarf_name(), offset)?;
                    }
                    offset -= 8;
                }
            }
            OpcodeX86_64::FramelessImmediate {
                stack_size_in_bytes,
                saved_regs,
            } => {
                if *stack_size_in_bytes == 0 {
                    write!(f, "CFA=reg7:",)?;
                } else {
                    write!(f, "CFA=reg7+{}:", *stack_size_in_bytes)?;
                }
                write!(f, " reg16=[CFA-8]")?;
                let mut offset = 2 * 8;
                for reg in saved_regs.iter().rev().flatten() {
                    write!(f, ", {}=[CFA-{}]", reg.dwarf_name(), offset)?;
                    offset += 8;
                }
            }
            OpcodeX86_64::FramelessIndirect {
                immediate_offset_from_function_start,
                stack_adjust_in_bytes,
                saved_regs,
            } => {
                write!(
                    f,
                    "CFA=[function_start+{}]+{}",
                    immediate_offset_from_function_start, stack_adjust_in_bytes
                )?;
                write!(f, " reg16=[CFA-8]")?;
                let mut offset = 2 * 8;
                for reg in saved_regs.iter().rev().flatten() {
                    write!(f, ", {}=[CFA-{}]", reg.dwarf_name(), offset)?;
                    offset += 8;
                }
            }
            OpcodeX86_64::Dwarf { eh_frame_fde } => {
                write!(f, "(check eh_frame FDE 0x{:x})", eh_frame_fde)?;
            }
            OpcodeX86_64::InvalidFrameless => {
                write!(
                    f,
                    "!! frameless immediate or indirect with invalid permutation encoding"
                )?;
            }
            OpcodeX86_64::UnrecognizedKind(kind) => {
                write!(f, "!! Unrecognized kind {}", kind)?;
            }
        }
        Ok(())
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_frameless_indirect() {
        use RegisterNameX86_64::*;
        assert_eq!(
            OpcodeX86_64::parse(0x30df800),
            OpcodeX86_64::FramelessIndirect {
                immediate_offset_from_function_start: 13,
                stack_adjust_in_bytes: 56,
                saved_regs: [
                    Some(Rbx),
                    Some(R12),
                    Some(R13),
                    Some(R14),
                    Some(R15),
                    Some(Rbp)
                ]
            }
        )
    }
}
