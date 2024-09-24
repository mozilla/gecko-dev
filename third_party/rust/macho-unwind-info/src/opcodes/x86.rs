use std::fmt::Display;

use super::bitfield::OpcodeBitfield;
use super::permutation::decode_permutation_6;
use crate::consts::*;

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum RegisterNameX86 {
    Ebx,
    Ecx,
    Edx,
    Edi,
    Esi,
    Ebp,
}

impl RegisterNameX86 {
    pub fn parse(n: u8) -> Option<Self> {
        match n {
            1 => Some(RegisterNameX86::Ebx),
            2 => Some(RegisterNameX86::Ecx),
            3 => Some(RegisterNameX86::Edx),
            4 => Some(RegisterNameX86::Edi),
            5 => Some(RegisterNameX86::Esi),
            6 => Some(RegisterNameX86::Ebp),
            _ => None,
        }
    }

    pub fn dwarf_name(&self) -> &'static str {
        match self {
            RegisterNameX86::Ebx => "reg3",
            RegisterNameX86::Ecx => "reg1",
            RegisterNameX86::Edx => "reg2",
            RegisterNameX86::Edi => "reg7",
            RegisterNameX86::Esi => "reg6",
            RegisterNameX86::Ebp => "reg5",
        }
    }
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum OpcodeX86 {
    Null,
    FrameBased {
        stack_offset_in_bytes: u16,
        saved_regs: [Option<RegisterNameX86>; 5],
    },
    FramelessImmediate {
        stack_size_in_bytes: u16,
        saved_regs: [Option<RegisterNameX86>; 6],
    },
    FramelessIndirect {
        /// Offset from the start of the function into the middle of a `sub`
        /// instruction, pointing right at the instruction's "immediate" which
        /// is a u32 value with the offset we need. (NOTE: not divided by anything!)
        immediate_offset_from_function_start: u8,

        /// An offset to add to the loaded stack size.
        /// This allows the stack size to differ slightly from the `sub`, to
        /// compensate for any function prologue that pushes a bunch of
        /// pointer-sized registers. This adjust value includes the return
        /// address on the stack. For example, if the function begins with six push
        /// instructions, followed by a sub instruction, then stack_adjust_in_bytes
        /// is 28: 4 bytes for the return address + 6 * 4 for each pushed register.
        stack_adjust_in_bytes: u8,

        /// The registers, in the order that they need to be popped in when
        /// returning / unwinding from this function. (Reverse order from
        /// function prologue!)
        /// Can have leading `None`s.
        saved_regs: [Option<RegisterNameX86>; 6],
    },
    Dwarf {
        eh_frame_fde: u32,
    },
    InvalidFrameless,
    UnrecognizedKind(u8),
}

impl OpcodeX86 {
    pub fn parse(opcode: u32) -> Self {
        match OpcodeBitfield::new(opcode).kind() {
            OPCODE_KIND_NULL => OpcodeX86::Null,
            OPCODE_KIND_X86_FRAMEBASED => OpcodeX86::FrameBased {
                stack_offset_in_bytes: (((opcode >> 16) & 0xff) as u16) * 4,
                saved_regs: [
                    RegisterNameX86::parse(((opcode >> 12) & 0b111) as u8),
                    RegisterNameX86::parse(((opcode >> 9) & 0b111) as u8),
                    RegisterNameX86::parse(((opcode >> 6) & 0b111) as u8),
                    RegisterNameX86::parse(((opcode >> 3) & 0b111) as u8),
                    RegisterNameX86::parse((opcode & 0b111) as u8),
                ],
            },
            OPCODE_KIND_X86_FRAMELESS_IMMEDIATE => {
                let stack_size_in_bytes = (((opcode >> 16) & 0xff) as u16) * 4;
                let register_count = (opcode >> 10) & 0b111;
                let register_permutation = opcode & 0b11_1111_1111;
                let saved_registers =
                    match decode_permutation_6(register_count, register_permutation) {
                        Ok(regs) => regs,
                        Err(_) => return OpcodeX86::InvalidFrameless,
                    };
                OpcodeX86::FramelessImmediate {
                    stack_size_in_bytes,
                    saved_regs: [
                        RegisterNameX86::parse(saved_registers[0]),
                        RegisterNameX86::parse(saved_registers[1]),
                        RegisterNameX86::parse(saved_registers[2]),
                        RegisterNameX86::parse(saved_registers[3]),
                        RegisterNameX86::parse(saved_registers[4]),
                        RegisterNameX86::parse(saved_registers[5]),
                    ],
                }
            }
            OPCODE_KIND_X86_FRAMELESS_INDIRECT => {
                let immediate_offset_from_function_start = (opcode >> 16) as u8;
                let stack_adjust_in_bytes = ((opcode >> 13) & 0b111) as u8 * 4;
                let register_count = (opcode >> 10) & 0b111;
                let register_permutation = opcode & 0b11_1111_1111;
                let saved_registers =
                    match decode_permutation_6(register_count, register_permutation) {
                        Ok(regs) => regs,
                        Err(_) => return OpcodeX86::InvalidFrameless,
                    };
                OpcodeX86::FramelessIndirect {
                    immediate_offset_from_function_start,
                    stack_adjust_in_bytes,
                    saved_regs: [
                        RegisterNameX86::parse(saved_registers[0]),
                        RegisterNameX86::parse(saved_registers[1]),
                        RegisterNameX86::parse(saved_registers[2]),
                        RegisterNameX86::parse(saved_registers[3]),
                        RegisterNameX86::parse(saved_registers[4]),
                        RegisterNameX86::parse(saved_registers[5]),
                    ],
                }
            }
            OPCODE_KIND_X86_DWARF => OpcodeX86::Dwarf {
                eh_frame_fde: (opcode & 0xffffff),
            },
            kind => OpcodeX86::UnrecognizedKind(kind),
        }
    }
}

impl Display for OpcodeX86 {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            OpcodeX86::Null => {
                write!(f, "(uncovered)")?;
            }
            OpcodeX86::FrameBased {
                stack_offset_in_bytes,
                saved_regs,
            } => {
                // ebp was set to esp before the saved registers were pushed.
                // The first pushed register is at ebp - 4 (== CFA - 12), the last at ebp - stack_offset_in_bytes.
                write!(f, "CFA=reg6+8: reg6=[CFA-8], reg16=[CFA-4]")?;
                let max_count = (*stack_offset_in_bytes / 4) as usize;
                let mut offset = *stack_offset_in_bytes + 8; // + 2 for rbp, return address
                for reg in saved_regs.iter().rev().take(max_count) {
                    if let Some(reg) = reg {
                        write!(f, ", {}=[CFA-{}]", reg.dwarf_name(), offset)?;
                    }
                    offset -= 4;
                }
            }
            OpcodeX86::FramelessImmediate {
                stack_size_in_bytes,
                saved_regs,
            } => {
                if *stack_size_in_bytes == 0 {
                    write!(f, "CFA=reg7:",)?;
                } else {
                    write!(f, "CFA=reg7+{}:", *stack_size_in_bytes)?;
                }
                write!(f, " reg16=[CFA-4]")?;
                let mut offset = 2 * 4;
                for reg in saved_regs.iter().rev().flatten() {
                    write!(f, ", {}=[CFA-{}]", reg.dwarf_name(), offset)?;
                    offset += 4;
                }
            }
            OpcodeX86::FramelessIndirect {
                immediate_offset_from_function_start,
                stack_adjust_in_bytes,
                saved_regs,
            } => {
                write!(
                    f,
                    "CFA=[function_start+{}]+{}",
                    immediate_offset_from_function_start, stack_adjust_in_bytes
                )?;
                write!(f, " reg16=[CFA-4]")?;
                let mut offset = 2 * 4;
                for reg in saved_regs.iter().rev().flatten() {
                    write!(f, ", {}=[CFA-{}]", reg.dwarf_name(), offset)?;
                    offset += 4;
                }
            }
            OpcodeX86::Dwarf { eh_frame_fde } => {
                write!(f, "(check eh_frame FDE 0x{:x})", eh_frame_fde)?;
            }
            OpcodeX86::InvalidFrameless => {
                write!(
                    f,
                    "!! frameless immediate or indirect with invalid permutation encoding"
                )?;
            }
            OpcodeX86::UnrecognizedKind(kind) => {
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
        use RegisterNameX86::*;
        assert_eq!(
            OpcodeX86::parse(0x30df800),
            OpcodeX86::FramelessIndirect {
                immediate_offset_from_function_start: 13,
                stack_adjust_in_bytes: 28,
                saved_regs: [
                    Some(Ebx),
                    Some(Ecx),
                    Some(Edx),
                    Some(Edi),
                    Some(Esi),
                    Some(Ebp)
                ]
            }
        )
    }
}
