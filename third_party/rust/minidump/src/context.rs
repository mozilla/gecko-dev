// Copyright 2015 Ted Mielczarek. See the COPYRIGHT
// file at the top-level directory of this distribution.

//! CPU contexts.

use num_traits::FromPrimitive;
use scroll::Pread;
use std::collections::HashSet;
use std::fmt;
use std::io;
use std::io::prelude::*;
use std::mem;
use tracing::warn;

use crate::iostuff::*;
use crate::{MinidumpMiscInfo, MinidumpSystemInfo};
use minidump_common::format as md;
use minidump_common::format::ContextFlagsCpu;

/// The CPU-specific context structure.
#[derive(Debug, Clone)]
#[cfg_attr(feature = "arbitrary_impls", derive(arbitrary::Arbitrary))]
pub enum MinidumpRawContext {
    X86(md::CONTEXT_X86),
    Ppc(md::CONTEXT_PPC),
    Ppc64(md::CONTEXT_PPC64),
    Amd64(md::CONTEXT_AMD64),
    Sparc(md::CONTEXT_SPARC),
    Arm(md::CONTEXT_ARM),
    Arm64(md::CONTEXT_ARM64),
    OldArm64(md::CONTEXT_ARM64_OLD),
    Mips(md::CONTEXT_MIPS),
}

/// Generic over the specifics of a CPU context.
pub trait CpuContext {
    /// The word size of general-purpose registers in the context.
    type Register: fmt::LowerHex;

    /// General purpose registers in this context type.
    const REGISTERS: &'static [&'static str];

    /// Gets whether the given register is valid
    ///
    /// This is exposed so that the context can map aliases. For instance
    /// "lr" and "x30" are aliases in ARM64.
    fn register_is_valid(&self, reg: &str, valid: &MinidumpContextValidity) -> bool {
        if let MinidumpContextValidity::Some(ref which) = *valid {
            which.contains(reg)
        } else {
            self.memoize_register(reg).is_some()
        }
    }

    /// Get a register value if it is valid.
    ///
    /// Get the value of the register named `reg` from this CPU context
    /// if `valid` indicates that it has a valid value, otherwise return
    /// `None`.
    fn get_register(&self, reg: &str, valid: &MinidumpContextValidity) -> Option<Self::Register> {
        if self.register_is_valid(reg, valid) {
            Some(self.get_register_always(reg))
        } else {
            None
        }
    }

    /// Get a register value regardless of whether it is valid.
    fn get_register_always(&self, reg: &str) -> Self::Register;

    /// Set a register value, if that register name it exists.
    ///
    /// Returns None if the register name isn't supported.
    fn set_register(&mut self, reg: &str, val: Self::Register) -> Option<()>;

    /// Gets a static version of the given register name, if possible.
    ///
    /// Returns the default name of the register for register name aliases.
    fn memoize_register(&self, reg: &str) -> Option<&'static str> {
        default_memoize_register(Self::REGISTERS, reg)
    }

    /// Return a String containing the value of `reg` formatted to its natural width.
    fn format_register(&self, reg: &str) -> String {
        format!(
            "0x{:01$x}",
            self.get_register_always(reg),
            mem::size_of::<Self::Register>() * 2
        )
    }

    /// An iterator over all registers in this context.
    ///
    /// This iterator yields registers and values regardless of whether the register is valid. To
    /// get valid values, use [`valid_registers`](Self::valid_registers), instead.
    fn registers(&self) -> CpuRegisters<'_, Self> {
        self.valid_registers(&MinidumpContextValidity::All)
    }

    /// An iterator over valid registers in this context.
    ///
    /// This iterator yields valid registers and their values.
    fn valid_registers<'a>(&'a self, valid: &'a MinidumpContextValidity) -> CpuRegisters<'a, Self> {
        let regs = match valid {
            MinidumpContextValidity::All => CpuRegistersInner::Slice(Self::REGISTERS.iter()),
            MinidumpContextValidity::Some(valid) => CpuRegistersInner::Set(valid.iter()),
        };

        CpuRegisters {
            regs,
            context: self,
        }
    }

    /// Gets the name of the stack pointer register (for use with get_register/set_register).
    fn stack_pointer_register_name(&self) -> &'static str;

    /// Gets the name of the instruction pointer register (for use with get_register/set_register).
    fn instruction_pointer_register_name(&self) -> &'static str;
}

/// Default implementation for `CpuContext::memoize_register`.
fn default_memoize_register(registers: &[&'static str], reg: &str) -> Option<&'static str> {
    let idx = registers.iter().position(|val| *val == reg)?;
    Some(registers[idx])
}

#[derive(Debug, Clone)]
enum CpuRegistersInner<'a> {
    Slice(std::slice::Iter<'a, &'static str>),
    Set(std::collections::hash_set::Iter<'a, &'static str>),
}

/// An iterator over registers and values in a [`CpuContext`].
///
/// Returned by [`CpuContext::registers`] and [`CpuContext::valid_registers`].
#[derive(Clone, Debug)]
pub struct CpuRegisters<'a, T: ?Sized> {
    regs: CpuRegistersInner<'a>,
    context: &'a T,
}

impl<'a, T> Iterator for CpuRegisters<'a, T>
where
    T: CpuContext,
{
    type Item = (&'static str, T::Register);

    fn next(&mut self) -> Option<Self::Item> {
        let reg = match &mut self.regs {
            CpuRegistersInner::Slice(iter) => iter.next(),
            CpuRegistersInner::Set(iter) => iter.next(),
        }?;

        Some((reg, self.context.get_register_always(reg)))
    }
}

impl CpuContext for md::CONTEXT_X86 {
    type Register = u32;

    const REGISTERS: &'static [&'static str] = &[
        "eip", "esp", "ebp", "ebx", "esi", "edi", "eax", "ecx", "edx", "eflags",
    ];

    fn get_register_always(&self, reg: &str) -> u32 {
        match reg {
            "eip" => self.eip,
            "esp" => self.esp,
            "ebp" => self.ebp,
            "ebx" => self.ebx,
            "esi" => self.esi,
            "edi" => self.edi,
            "eax" => self.eax,
            "ecx" => self.ecx,
            "edx" => self.edx,
            "eflags" => self.eflags,
            _ => unreachable!("Invalid x86 register! {}", reg),
        }
    }

    fn set_register(&mut self, reg: &str, val: Self::Register) -> Option<()> {
        match reg {
            "eip" => self.eip = val,
            "esp" => self.esp = val,
            "ebp" => self.ebp = val,
            "ebx" => self.ebx = val,
            "esi" => self.esi = val,
            "edi" => self.edi = val,
            "eax" => self.eax = val,
            "ecx" => self.ecx = val,
            "edx" => self.edx = val,
            "eflags" => self.eflags = val,
            _ => return None,
        }
        Some(())
    }

    fn stack_pointer_register_name(&self) -> &'static str {
        "esp"
    }

    fn instruction_pointer_register_name(&self) -> &'static str {
        "eip"
    }
}

impl CpuContext for md::CONTEXT_AMD64 {
    type Register = u64;

    const REGISTERS: &'static [&'static str] = &[
        "rax", "rdx", "rcx", "rbx", "rsi", "rdi", "rbp", "rsp", "r8", "r9", "r10", "r11", "r12",
        "r13", "r14", "r15", "rip",
    ];

    fn get_register_always(&self, reg: &str) -> u64 {
        match reg {
            "rax" => self.rax,
            "rdx" => self.rdx,
            "rcx" => self.rcx,
            "rbx" => self.rbx,
            "rsi" => self.rsi,
            "rdi" => self.rdi,
            "rbp" => self.rbp,
            "rsp" => self.rsp,
            "r8" => self.r8,
            "r9" => self.r9,
            "r10" => self.r10,
            "r11" => self.r11,
            "r12" => self.r12,
            "r13" => self.r13,
            "r14" => self.r14,
            "r15" => self.r15,
            "rip" => self.rip,
            _ => unreachable!("Invalid x86-64 register! {}", reg),
        }
    }

    fn set_register(&mut self, reg: &str, val: Self::Register) -> Option<()> {
        match reg {
            "rax" => self.rax = val,
            "rdx" => self.rdx = val,
            "rcx" => self.rcx = val,
            "rbx" => self.rbx = val,
            "rsi" => self.rsi = val,
            "rdi" => self.rdi = val,
            "rbp" => self.rbp = val,
            "rsp" => self.rsp = val,
            "r8" => self.r8 = val,
            "r9" => self.r9 = val,
            "r10" => self.r10 = val,
            "r11" => self.r11 = val,
            "r12" => self.r12 = val,
            "r13" => self.r13 = val,
            "r14" => self.r14 = val,
            "r15" => self.r15 = val,
            "rip" => self.rip = val,
            _ => return None,
        }
        Some(())
    }

    fn stack_pointer_register_name(&self) -> &'static str {
        "rsp"
    }

    fn instruction_pointer_register_name(&self) -> &'static str {
        "rip"
    }
}

impl CpuContext for md::CONTEXT_ARM {
    type Register = u32;

    const REGISTERS: &'static [&'static str] = &[
        "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r12", "fp", "sp", "lr",
        "pc",
    ];

    fn memoize_register(&self, reg: &str) -> Option<&'static str> {
        match reg {
            "r11" => Some("fp"),
            "r13" => Some("sp"),
            "r14" => Some("lr"),
            "r15" => Some("pc"),
            _ => default_memoize_register(Self::REGISTERS, reg),
        }
    }

    fn register_is_valid(&self, reg: &str, valid: &MinidumpContextValidity) -> bool {
        if let MinidumpContextValidity::Some(ref which) = valid {
            match reg {
                "r11" | "fp" => which.contains("r11") || which.contains("fp"),
                "r13" | "sp" => which.contains("r13") || which.contains("sp"),
                "r14" | "lr" => which.contains("r14") || which.contains("lr"),
                "r15" | "pc" => which.contains("r15") || which.contains("pc"),
                _ => which.contains(reg),
            }
        } else {
            self.memoize_register(reg).is_some()
        }
    }

    fn get_register_always(&self, reg: &str) -> u32 {
        match reg {
            "r0" => self.iregs[0],
            "r1" => self.iregs[1],
            "r2" => self.iregs[2],
            "r3" => self.iregs[3],
            "r4" => self.iregs[4],
            "r5" => self.iregs[5],
            "r6" => self.iregs[6],
            "r7" => self.iregs[7],
            "r8" => self.iregs[8],
            "r9" => self.iregs[9],
            "r10" => self.iregs[10],
            "r11" => self.iregs[11],
            "r12" => self.iregs[12],
            "r13" => self.iregs[13],
            "r14" => self.iregs[14],
            "r15" => self.iregs[15],
            "pc" => self.iregs[md::ArmRegisterNumbers::ProgramCounter as usize],
            "lr" => self.iregs[md::ArmRegisterNumbers::LinkRegister as usize],
            "fp" => self.iregs[md::ArmRegisterNumbers::FramePointer as usize],
            "sp" => self.iregs[md::ArmRegisterNumbers::StackPointer as usize],
            _ => unreachable!("Invalid arm register! {}", reg),
        }
    }

    fn set_register(&mut self, reg: &str, val: Self::Register) -> Option<()> {
        match reg {
            "r0" => self.iregs[0] = val,
            "r1" => self.iregs[1] = val,
            "r2" => self.iregs[2] = val,
            "r3" => self.iregs[3] = val,
            "r4" => self.iregs[4] = val,
            "r5" => self.iregs[5] = val,
            "r6" => self.iregs[6] = val,
            "r7" => self.iregs[7] = val,
            "r8" => self.iregs[8] = val,
            "r9" => self.iregs[9] = val,
            "r10" => self.iregs[10] = val,
            "r11" => self.iregs[11] = val,
            "r12" => self.iregs[12] = val,
            "r13" => self.iregs[13] = val,
            "r14" => self.iregs[14] = val,
            "r15" => self.iregs[15] = val,
            "pc" => self.iregs[md::ArmRegisterNumbers::ProgramCounter as usize] = val,
            "lr" => self.iregs[md::ArmRegisterNumbers::LinkRegister as usize] = val,
            "fp" => self.iregs[md::ArmRegisterNumbers::FramePointer as usize] = val,
            "sp" => self.iregs[md::ArmRegisterNumbers::StackPointer as usize] = val,
            _ => return None,
        }
        Some(())
    }

    fn stack_pointer_register_name(&self) -> &'static str {
        "sp"
    }

    fn instruction_pointer_register_name(&self) -> &'static str {
        "pc"
    }
}

impl CpuContext for md::CONTEXT_ARM64_OLD {
    type Register = u64;

    const REGISTERS: &'static [&'static str] = &[
        "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13",
        "x14", "x15", "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23", "x24", "x25", "x26",
        "x27", "x28", "fp", "lr", "sp", "pc",
    ];

    fn memoize_register(&self, reg: &str) -> Option<&'static str> {
        match reg {
            "x29" => Some("fp"),
            "x30" => Some("lr"),
            _ => default_memoize_register(Self::REGISTERS, reg),
        }
    }

    fn register_is_valid(&self, reg: &str, valid: &MinidumpContextValidity) -> bool {
        if let MinidumpContextValidity::Some(ref which) = valid {
            match reg {
                "x29" | "fp" => which.contains("x29") || which.contains("fp"),
                "x30" | "lr" => which.contains("x30") || which.contains("lr"),
                _ => which.contains(reg),
            }
        } else {
            self.memoize_register(reg).is_some()
        }
    }

    fn get_register_always(&self, reg: &str) -> u64 {
        match reg {
            "x0" => self.iregs[0],
            "x1" => self.iregs[1],
            "x2" => self.iregs[2],
            "x3" => self.iregs[3],
            "x4" => self.iregs[4],
            "x5" => self.iregs[5],
            "x6" => self.iregs[6],
            "x7" => self.iregs[7],
            "x8" => self.iregs[8],
            "x9" => self.iregs[9],
            "x10" => self.iregs[10],
            "x11" => self.iregs[11],
            "x12" => self.iregs[12],
            "x13" => self.iregs[13],
            "x14" => self.iregs[14],
            "x15" => self.iregs[15],
            "x16" => self.iregs[16],
            "x17" => self.iregs[17],
            "x18" => self.iregs[18],
            "x19" => self.iregs[19],
            "x20" => self.iregs[20],
            "x21" => self.iregs[21],
            "x22" => self.iregs[22],
            "x23" => self.iregs[23],
            "x24" => self.iregs[24],
            "x25" => self.iregs[25],
            "x26" => self.iregs[26],
            "x27" => self.iregs[27],
            "x28" => self.iregs[28],
            "x29" => self.iregs[29],
            "x30" => self.iregs[30],
            "pc" => self.pc,
            "sp" => self.sp,
            "lr" => self.iregs[md::Arm64RegisterNumbers::LinkRegister as usize],
            "fp" => self.iregs[md::Arm64RegisterNumbers::FramePointer as usize],
            _ => unreachable!("Invalid aarch64 register! {}", reg),
        }
    }

    fn set_register(&mut self, reg: &str, val: Self::Register) -> Option<()> {
        match reg {
            "x0" => self.iregs[0] = val,
            "x1" => self.iregs[1] = val,
            "x2" => self.iregs[2] = val,
            "x3" => self.iregs[3] = val,
            "x4" => self.iregs[4] = val,
            "x5" => self.iregs[5] = val,
            "x6" => self.iregs[6] = val,
            "x7" => self.iregs[7] = val,
            "x8" => self.iregs[8] = val,
            "x9" => self.iregs[9] = val,
            "x10" => self.iregs[10] = val,
            "x11" => self.iregs[11] = val,
            "x12" => self.iregs[12] = val,
            "x13" => self.iregs[13] = val,
            "x14" => self.iregs[14] = val,
            "x15" => self.iregs[15] = val,
            "x16" => self.iregs[16] = val,
            "x17" => self.iregs[17] = val,
            "x18" => self.iregs[18] = val,
            "x19" => self.iregs[19] = val,
            "x20" => self.iregs[20] = val,
            "x21" => self.iregs[21] = val,
            "x22" => self.iregs[22] = val,
            "x23" => self.iregs[23] = val,
            "x24" => self.iregs[24] = val,
            "x25" => self.iregs[25] = val,
            "x26" => self.iregs[26] = val,
            "x27" => self.iregs[27] = val,
            "x28" => self.iregs[28] = val,
            "x29" => self.iregs[29] = val,
            "x30" => self.iregs[30] = val,
            "pc" => self.pc = val,
            "sp" => self.sp = val,
            "lr" => self.iregs[md::Arm64RegisterNumbers::LinkRegister as usize] = val,
            "fp" => self.iregs[md::Arm64RegisterNumbers::FramePointer as usize] = val,
            _ => return None,
        }
        Some(())
    }

    fn stack_pointer_register_name(&self) -> &'static str {
        "sp"
    }

    fn instruction_pointer_register_name(&self) -> &'static str {
        "pc"
    }
}

impl CpuContext for md::CONTEXT_ARM64 {
    type Register = u64;

    const REGISTERS: &'static [&'static str] = &[
        "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13",
        "x14", "x15", "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23", "x24", "x25", "x26",
        "x27", "x28", "fp", "lr", "sp", "pc",
    ];

    fn memoize_register(&self, reg: &str) -> Option<&'static str> {
        match reg {
            "x29" => Some("fp"),
            "x30" => Some("lr"),
            _ => default_memoize_register(Self::REGISTERS, reg),
        }
    }

    fn register_is_valid(&self, reg: &str, valid: &MinidumpContextValidity) -> bool {
        if let MinidumpContextValidity::Some(ref which) = valid {
            match reg {
                "x29" | "fp" => which.contains("x29") || which.contains("fp"),
                "x30" | "lr" => which.contains("x30") || which.contains("lr"),
                _ => which.contains(reg),
            }
        } else {
            self.memoize_register(reg).is_some()
        }
    }

    fn get_register_always(&self, reg: &str) -> u64 {
        match reg {
            "x0" => self.iregs[0],
            "x1" => self.iregs[1],
            "x2" => self.iregs[2],
            "x3" => self.iregs[3],
            "x4" => self.iregs[4],
            "x5" => self.iregs[5],
            "x6" => self.iregs[6],
            "x7" => self.iregs[7],
            "x8" => self.iregs[8],
            "x9" => self.iregs[9],
            "x10" => self.iregs[10],
            "x11" => self.iregs[11],
            "x12" => self.iregs[12],
            "x13" => self.iregs[13],
            "x14" => self.iregs[14],
            "x15" => self.iregs[15],
            "x16" => self.iregs[16],
            "x17" => self.iregs[17],
            "x18" => self.iregs[18],
            "x19" => self.iregs[19],
            "x20" => self.iregs[20],
            "x21" => self.iregs[21],
            "x22" => self.iregs[22],
            "x23" => self.iregs[23],
            "x24" => self.iregs[24],
            "x25" => self.iregs[25],
            "x26" => self.iregs[26],
            "x27" => self.iregs[27],
            "x28" => self.iregs[28],
            "x29" => self.iregs[29],
            "x30" => self.iregs[30],
            "pc" => self.pc,
            "sp" => self.sp,
            "lr" => self.iregs[md::Arm64RegisterNumbers::LinkRegister as usize],
            "fp" => self.iregs[md::Arm64RegisterNumbers::FramePointer as usize],
            _ => unreachable!("Invalid aarch64 register! {}", reg),
        }
    }

    fn set_register(&mut self, reg: &str, val: Self::Register) -> Option<()> {
        match reg {
            "x0" => self.iregs[0] = val,
            "x1" => self.iregs[1] = val,
            "x2" => self.iregs[2] = val,
            "x3" => self.iregs[3] = val,
            "x4" => self.iregs[4] = val,
            "x5" => self.iregs[5] = val,
            "x6" => self.iregs[6] = val,
            "x7" => self.iregs[7] = val,
            "x8" => self.iregs[8] = val,
            "x9" => self.iregs[9] = val,
            "x10" => self.iregs[10] = val,
            "x11" => self.iregs[11] = val,
            "x12" => self.iregs[12] = val,
            "x13" => self.iregs[13] = val,
            "x14" => self.iregs[14] = val,
            "x15" => self.iregs[15] = val,
            "x16" => self.iregs[16] = val,
            "x17" => self.iregs[17] = val,
            "x18" => self.iregs[18] = val,
            "x19" => self.iregs[19] = val,
            "x20" => self.iregs[20] = val,
            "x21" => self.iregs[21] = val,
            "x22" => self.iregs[22] = val,
            "x23" => self.iregs[23] = val,
            "x24" => self.iregs[24] = val,
            "x25" => self.iregs[25] = val,
            "x26" => self.iregs[26] = val,
            "x27" => self.iregs[27] = val,
            "x28" => self.iregs[28] = val,
            "x29" => self.iregs[29] = val,
            "x30" => self.iregs[30] = val,
            "pc" => self.pc = val,
            "sp" => self.sp = val,
            "lr" => self.iregs[md::Arm64RegisterNumbers::LinkRegister as usize] = val,
            "fp" => self.iregs[md::Arm64RegisterNumbers::FramePointer as usize] = val,
            _ => return None,
        }
        Some(())
    }

    fn stack_pointer_register_name(&self) -> &'static str {
        "sp"
    }

    fn instruction_pointer_register_name(&self) -> &'static str {
        "pc"
    }
}

impl CpuContext for md::CONTEXT_PPC {
    type Register = u32;

    const REGISTERS: &'static [&'static str] = &[
        "srr0", "srr1", "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11",
        "r12", "r13", "r14", "r15", "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23", "r24",
        "r25", "r26", "r27", "r28", "r29", "r30", "r31", "cr", "xer", "lr", "ctr", "mq", "vrsave",
    ];

    fn get_register_always(&self, reg: &str) -> Self::Register {
        match reg {
            "srr0" => self.srr0,
            "srr1" => self.srr1,
            "r0" => self.gpr[0],
            "r1" => self.gpr[1],
            "r2" => self.gpr[2],
            "r3" => self.gpr[3],
            "r4" => self.gpr[4],
            "r5" => self.gpr[5],
            "r6" => self.gpr[6],
            "r7" => self.gpr[7],
            "r8" => self.gpr[8],
            "r9" => self.gpr[9],
            "r10" => self.gpr[10],
            "r11" => self.gpr[11],
            "r12" => self.gpr[12],
            "r13" => self.gpr[13],
            "r14" => self.gpr[14],
            "r15" => self.gpr[15],
            "r16" => self.gpr[16],
            "r17" => self.gpr[17],
            "r18" => self.gpr[18],
            "r19" => self.gpr[19],
            "r20" => self.gpr[20],
            "r21" => self.gpr[21],
            "r22" => self.gpr[22],
            "r23" => self.gpr[23],
            "r24" => self.gpr[24],
            "r25" => self.gpr[25],
            "r26" => self.gpr[26],
            "r27" => self.gpr[27],
            "r28" => self.gpr[28],
            "r29" => self.gpr[29],
            "r30" => self.gpr[30],
            "r31" => self.gpr[31],
            "cr" => self.cr,
            "xer" => self.xer,
            "lr" => self.lr,
            "ctr" => self.ctr,
            "mq" => self.mq,
            "vrsave" => self.vrsave,
            _ => unreachable!("Invalid ppc register! {}", reg),
        }
    }

    fn set_register(&mut self, reg: &str, val: Self::Register) -> Option<()> {
        match reg {
            "srr0" => self.srr0 = val,
            "srr1" => self.srr1 = val,
            "r0" => self.gpr[0] = val,
            "r1" => self.gpr[1] = val,
            "r2" => self.gpr[2] = val,
            "r3" => self.gpr[3] = val,
            "r4" => self.gpr[4] = val,
            "r5" => self.gpr[5] = val,
            "r6" => self.gpr[6] = val,
            "r7" => self.gpr[7] = val,
            "r8" => self.gpr[8] = val,
            "r9" => self.gpr[9] = val,
            "r10" => self.gpr[10] = val,
            "r11" => self.gpr[11] = val,
            "r12" => self.gpr[12] = val,
            "r13" => self.gpr[13] = val,
            "r14" => self.gpr[14] = val,
            "r15" => self.gpr[15] = val,
            "r16" => self.gpr[16] = val,
            "r17" => self.gpr[17] = val,
            "r18" => self.gpr[18] = val,
            "r19" => self.gpr[19] = val,
            "r20" => self.gpr[20] = val,
            "r21" => self.gpr[21] = val,
            "r22" => self.gpr[22] = val,
            "r23" => self.gpr[23] = val,
            "r24" => self.gpr[24] = val,
            "r25" => self.gpr[25] = val,
            "r26" => self.gpr[26] = val,
            "r27" => self.gpr[27] = val,
            "r28" => self.gpr[28] = val,
            "r29" => self.gpr[29] = val,
            "r30" => self.gpr[30] = val,
            "r31" => self.gpr[31] = val,
            "cr" => self.cr = val,
            "xer" => self.xer = val,
            "lr" => self.lr = val,
            "ctr" => self.ctr = val,
            "mq" => self.mq = val,
            "vrsave" => self.vrsave = val,
            _ => return None,
        }
        Some(())
    }

    fn stack_pointer_register_name(&self) -> &'static str {
        "r1"
    }

    fn instruction_pointer_register_name(&self) -> &'static str {
        "srr0"
    }
}

impl CpuContext for md::CONTEXT_PPC64 {
    type Register = u64;

    const REGISTERS: &'static [&'static str] = &[
        "srr0", "srr1", "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11",
        "r12", "r13", "r14", "r15", "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23", "r24",
        "r25", "r26", "r27", "r28", "r29", "r30", "r31", "cr", "xer", "lr", "ctr", "vrsave",
    ];

    fn get_register_always(&self, reg: &str) -> Self::Register {
        match reg {
            "srr0" => self.srr0,
            "srr1" => self.srr1,
            "r0" => self.gpr[0],
            "r1" => self.gpr[1],
            "r2" => self.gpr[2],
            "r3" => self.gpr[3],
            "r4" => self.gpr[4],
            "r5" => self.gpr[5],
            "r6" => self.gpr[6],
            "r7" => self.gpr[7],
            "r8" => self.gpr[8],
            "r9" => self.gpr[9],
            "r10" => self.gpr[10],
            "r11" => self.gpr[11],
            "r12" => self.gpr[12],
            "r13" => self.gpr[13],
            "r14" => self.gpr[14],
            "r15" => self.gpr[15],
            "r16" => self.gpr[16],
            "r17" => self.gpr[17],
            "r18" => self.gpr[18],
            "r19" => self.gpr[19],
            "r20" => self.gpr[20],
            "r21" => self.gpr[21],
            "r22" => self.gpr[22],
            "r23" => self.gpr[23],
            "r24" => self.gpr[24],
            "r25" => self.gpr[25],
            "r26" => self.gpr[26],
            "r27" => self.gpr[27],
            "r28" => self.gpr[28],
            "r29" => self.gpr[29],
            "r30" => self.gpr[30],
            "r31" => self.gpr[31],
            "cr" => self.cr,
            "xer" => self.xer,
            "lr" => self.lr,
            "ctr" => self.ctr,
            "vrsave" => self.vrsave,
            _ => unreachable!("Invalid ppc64 register! {}", reg),
        }
    }

    fn set_register(&mut self, reg: &str, val: Self::Register) -> Option<()> {
        match reg {
            "srr0" => self.srr0 = val,
            "srr1" => self.srr1 = val,
            "r0" => self.gpr[0] = val,
            "r1" => self.gpr[1] = val,
            "r2" => self.gpr[2] = val,
            "r3" => self.gpr[3] = val,
            "r4" => self.gpr[4] = val,
            "r5" => self.gpr[5] = val,
            "r6" => self.gpr[6] = val,
            "r7" => self.gpr[7] = val,
            "r8" => self.gpr[8] = val,
            "r9" => self.gpr[9] = val,
            "r10" => self.gpr[10] = val,
            "r11" => self.gpr[11] = val,
            "r12" => self.gpr[12] = val,
            "r13" => self.gpr[13] = val,
            "r14" => self.gpr[14] = val,
            "r15" => self.gpr[15] = val,
            "r16" => self.gpr[16] = val,
            "r17" => self.gpr[17] = val,
            "r18" => self.gpr[18] = val,
            "r19" => self.gpr[19] = val,
            "r20" => self.gpr[20] = val,
            "r21" => self.gpr[21] = val,
            "r22" => self.gpr[22] = val,
            "r23" => self.gpr[23] = val,
            "r24" => self.gpr[24] = val,
            "r25" => self.gpr[25] = val,
            "r26" => self.gpr[26] = val,
            "r27" => self.gpr[27] = val,
            "r28" => self.gpr[28] = val,
            "r29" => self.gpr[29] = val,
            "r30" => self.gpr[30] = val,
            "r31" => self.gpr[31] = val,
            "cr" => self.cr = val,
            "xer" => self.xer = val,
            "lr" => self.lr = val,
            "ctr" => self.ctr = val,
            "vrsave" => self.vrsave = val,
            _ => return None,
        }
        Some(())
    }

    fn stack_pointer_register_name(&self) -> &'static str {
        "r1"
    }

    fn instruction_pointer_register_name(&self) -> &'static str {
        "srr0"
    }
}

impl CpuContext for md::CONTEXT_MIPS {
    type Register = u64;

    const REGISTERS: &'static [&'static str] = &[
        "gp", "sp", "fp", "ra", "pc", "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    ];

    fn get_register_always(&self, reg: &str) -> Self::Register {
        match reg {
            "gp" => self.iregs[md::MipsRegisterNumbers::GlobalPointer as usize],
            "sp" => self.iregs[md::MipsRegisterNumbers::StackPointer as usize],
            "fp" => self.iregs[md::MipsRegisterNumbers::FramePointer as usize],
            "ra" => self.iregs[md::MipsRegisterNumbers::ReturnAddress as usize],
            "pc" => self.epc,
            "s0" => self.iregs[md::MipsRegisterNumbers::S0 as usize],
            "s1" => self.iregs[md::MipsRegisterNumbers::S1 as usize],
            "s2" => self.iregs[md::MipsRegisterNumbers::S2 as usize],
            "s3" => self.iregs[md::MipsRegisterNumbers::S3 as usize],
            "s4" => self.iregs[md::MipsRegisterNumbers::S4 as usize],
            "s5" => self.iregs[md::MipsRegisterNumbers::S5 as usize],
            "s6" => self.iregs[md::MipsRegisterNumbers::S6 as usize],
            "s7" => self.iregs[md::MipsRegisterNumbers::S7 as usize],
            _ => unreachable!("Invalid mips register! {}", reg),
        }
    }

    fn set_register(&mut self, reg: &str, val: Self::Register) -> Option<()> {
        match reg {
            "gp" => self.iregs[md::MipsRegisterNumbers::GlobalPointer as usize] = val,
            "sp" => self.iregs[md::MipsRegisterNumbers::StackPointer as usize] = val,
            "fp" => self.iregs[md::MipsRegisterNumbers::FramePointer as usize] = val,
            "ra" => self.iregs[md::MipsRegisterNumbers::ReturnAddress as usize] = val,
            "pc" => self.epc = val,
            "s0" => self.iregs[md::MipsRegisterNumbers::S0 as usize] = val,
            "s1" => self.iregs[md::MipsRegisterNumbers::S1 as usize] = val,
            "s2" => self.iregs[md::MipsRegisterNumbers::S2 as usize] = val,
            "s3" => self.iregs[md::MipsRegisterNumbers::S3 as usize] = val,
            "s4" => self.iregs[md::MipsRegisterNumbers::S4 as usize] = val,
            "s5" => self.iregs[md::MipsRegisterNumbers::S5 as usize] = val,
            "s6" => self.iregs[md::MipsRegisterNumbers::S6 as usize] = val,
            "s7" => self.iregs[md::MipsRegisterNumbers::S7 as usize] = val,
            _ => return None,
        }
        Some(())
    }

    fn stack_pointer_register_name(&self) -> &'static str {
        "sp"
    }

    fn instruction_pointer_register_name(&self) -> &'static str {
        "pc"
    }
}

impl CpuContext for md::CONTEXT_SPARC {
    type Register = u64;

    const REGISTERS: &'static [&'static str] = &[
        "g_r0", "g_r1", "g_r2", "g_r3", "g_r4", "g_r5", "g_r6", "g_r7", "g_r8", "g_r9", "g_r10",
        "g_r11", "g_r12", "g_r13", "g_r14", "g_r15", "g_r16", "g_r17", "g_r18", "g_r19", "g_r20",
        "g_r21", "g_r22", "g_r23", "g_r24", "g_r25", "g_r26", "g_r27", "g_r28", "g_r29", "g_r30",
        "g_r31", "ccr", "pc", "npc", "y", "asi", "fprs",
    ];

    fn get_register_always(&self, reg: &str) -> Self::Register {
        match reg {
            "g_r0" | "g0" => self.g_r[0],
            "g_r1" | "g1" => self.g_r[1],
            "g_r2" | "g2" => self.g_r[2],
            "g_r3" | "g3" => self.g_r[3],
            "g_r4" | "g4" => self.g_r[4],
            "g_r5" | "g5" => self.g_r[5],
            "g_r6" | "g6" => self.g_r[6],
            "g_r7" | "g7" => self.g_r[7],
            "g_r8" | "o0" => self.g_r[8],
            "g_r9" | "o1" => self.g_r[9],
            "g_r10" | "o2" => self.g_r[10],
            "g_r11" | "o3" => self.g_r[11],
            "g_r12" | "o4" => self.g_r[12],
            "g_r13" | "o5" => self.g_r[13],
            "g_r14" | "o6" => self.g_r[14],
            "g_r15" | "o7" => self.g_r[15],
            "g_r16" | "l0" => self.g_r[16],
            "g_r17" | "l1" => self.g_r[17],
            "g_r18" | "l2" => self.g_r[18],
            "g_r19" | "l3" => self.g_r[19],
            "g_r20" | "l4" => self.g_r[20],
            "g_r21" | "l5" => self.g_r[21],
            "g_r22" | "l6" => self.g_r[22],
            "g_r23" | "l7" => self.g_r[23],
            "g_r24" | "i0" => self.g_r[24],
            "g_r25" | "i1" => self.g_r[25],
            "g_r26" | "i2" => self.g_r[26],
            "g_r27" | "i3" => self.g_r[27],
            "g_r28" | "i4" => self.g_r[28],
            "g_r29" | "i5" => self.g_r[29],
            "g_r30" | "i6" => self.g_r[30],
            "g_r31" | "i7" => self.g_r[31],
            "ccr" => self.ccr,
            "pc" => self.pc,
            "npc" => self.npc,
            "y" => self.y,
            "asi" => self.asi,
            "fprs" => self.fprs,
            _ => unreachable!("Invalid sparc register! {}", reg),
        }
    }

    fn set_register(&mut self, reg: &str, val: Self::Register) -> Option<()> {
        match reg {
            "g_r0" | "g0" => self.g_r[0] = val,
            "g_r1" | "g1" => self.g_r[1] = val,
            "g_r2" | "g2" => self.g_r[2] = val,
            "g_r3" | "g3" => self.g_r[3] = val,
            "g_r4" | "g4" => self.g_r[4] = val,
            "g_r5" | "g5" => self.g_r[5] = val,
            "g_r6" | "g6" => self.g_r[6] = val,
            "g_r7" | "g7" => self.g_r[7] = val,
            "g_r8" | "o0" => self.g_r[8] = val,
            "g_r9" | "o1" => self.g_r[9] = val,
            "g_r10" | "o2" => self.g_r[10] = val,
            "g_r11" | "o3" => self.g_r[11] = val,
            "g_r12" | "o4" => self.g_r[12] = val,
            "g_r13" | "o5" => self.g_r[13] = val,
            "g_r14" | "o6" => self.g_r[14] = val,
            "g_r15" | "o7" => self.g_r[15] = val,
            "g_r16" | "l0" => self.g_r[16] = val,
            "g_r17" | "l1" => self.g_r[17] = val,
            "g_r18" | "l2" => self.g_r[18] = val,
            "g_r19" | "l3" => self.g_r[19] = val,
            "g_r20" | "l4" => self.g_r[20] = val,
            "g_r21" | "l5" => self.g_r[21] = val,
            "g_r22" | "l6" => self.g_r[22] = val,
            "g_r23" | "l7" => self.g_r[23] = val,
            "g_r24" | "i0" => self.g_r[24] = val,
            "g_r25" | "i1" => self.g_r[25] = val,
            "g_r26" | "i2" => self.g_r[26] = val,
            "g_r27" | "i3" => self.g_r[27] = val,
            "g_r28" | "i4" => self.g_r[28] = val,
            "g_r29" | "i5" => self.g_r[29] = val,
            "g_r30" | "i6" => self.g_r[30] = val,
            "g_r31" | "i7" => self.g_r[31] = val,
            "ccr" => self.ccr = val,
            "pc" => self.pc = val,
            "npc" => self.npc = val,
            "y" => self.y = val,
            "asi" => self.asi = val,
            "fprs" => self.fprs = val,
            _ => return None,
        }
        Some(())
    }

    fn stack_pointer_register_name(&self) -> &'static str {
        "g_r14" // alias out register o6
    }

    fn instruction_pointer_register_name(&self) -> &'static str {
        "pc"
    }
}

/// Information about which registers are valid in a `MinidumpContext`.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum MinidumpContextValidity {
    // All registers are valid.
    All,
    // The registers in this set are valid.
    Some(HashSet<&'static str>),
}

/// CPU context such as register states.
///
/// MinidumpContext carries a CPU-specific MDRawContext structure, which
/// contains CPU context such as register states.  Each thread has its
/// own context, and the exception record, if present, also has its own
/// context.  Note that if the exception record is present, the context it
/// refers to is probably what the user wants to use for the exception
/// thread, instead of that thread's own context.  The exception thread's
/// context (as opposed to the exception record's context) will contain
/// context for the exception handler (which performs minidump generation),
/// and not the context that caused the exception (which is probably what the
/// user wants).
#[derive(Debug, Clone)]
pub struct MinidumpContext {
    /// The raw CPU register state.
    pub raw: MinidumpRawContext,
    /// Which registers are valid in `raw`.
    pub valid: MinidumpContextValidity,
}

/// Errors encountered while reading a `MinidumpContext`.
#[derive(Debug)]
pub enum ContextError {
    /// Failed to read data.
    ReadFailure,
    /// Encountered an unknown CPU context.
    UnknownCpuContext,
}

//======================================================
// Implementations

impl MinidumpContext {
    /// Return a MinidumpContext given a `MinidumpRawContext`.
    pub fn from_raw(raw: MinidumpRawContext) -> MinidumpContext {
        MinidumpContext {
            raw,
            valid: MinidumpContextValidity::All,
        }
    }

    /// Read a `MinidumpContext` from `bytes`.
    pub fn read(
        bytes: &[u8],
        endian: scroll::Endian,
        system_info: &MinidumpSystemInfo,
        _misc: Option<&MinidumpMiscInfo>,
    ) -> Result<MinidumpContext, ContextError> {
        use md::ProcessorArchitecture::*;

        let mut offset = 0;

        // Although every context contains `context_flags` which tell us what kind
        // ok context we're handling, they aren't all in the same location, so we
        // need to use SystemInfo to choose what kind of context to parse this as.
        // We can then use the `context_flags` to validate our parse.
        // We need to use the raw processor_architecture because system_info.cpu
        // flattens away some key distinctions for this code.
        match md::ProcessorArchitecture::from_u16(system_info.raw.processor_architecture) {
            Some(PROCESSOR_ARCHITECTURE_INTEL) | Some(PROCESSOR_ARCHITECTURE_IA32_ON_WIN64) => {
                // Not 100% sure IA32_ON_WIN64 is this format, but let's assume so?
                let ctx: md::CONTEXT_X86 = bytes
                    .gread_with(&mut offset, endian)
                    .or(Err(ContextError::ReadFailure))?;

                let flags = ContextFlagsCpu::from_flags(ctx.context_flags);
                if flags == ContextFlagsCpu::CONTEXT_X86 {
                    if ctx.context_flags & md::CONTEXT_HAS_XSTATE != 0 {
                        // FIXME: uses MISC_INFO_5 to parse out extra sections here
                        warn!("Cpu context has extra XSTATE that is being ignored");
                    }
                    Ok(MinidumpContext::from_raw(MinidumpRawContext::X86(ctx)))
                } else {
                    Err(ContextError::ReadFailure)
                }
            }
            Some(PROCESSOR_ARCHITECTURE_AMD64) => {
                let ctx: md::CONTEXT_AMD64 = bytes
                    .gread_with(&mut offset, endian)
                    .or(Err(ContextError::ReadFailure))?;

                let flags = ContextFlagsCpu::from_flags(ctx.context_flags);
                if flags == ContextFlagsCpu::CONTEXT_AMD64 {
                    if ctx.context_flags & md::CONTEXT_HAS_XSTATE != 0 {
                        // FIXME: uses MISC_INFO_5 to parse out extra sections here
                        warn!("Cpu context has extra XSTATE that is being ignored");
                    }
                    Ok(MinidumpContext::from_raw(MinidumpRawContext::Amd64(ctx)))
                } else {
                    Err(ContextError::ReadFailure)
                }
            }
            Some(PROCESSOR_ARCHITECTURE_PPC) => {
                let ctx: md::CONTEXT_PPC = bytes
                    .gread_with(&mut offset, endian)
                    .or(Err(ContextError::ReadFailure))?;

                let flags = ContextFlagsCpu::from_flags(ctx.context_flags);
                if flags == ContextFlagsCpu::CONTEXT_PPC {
                    Ok(MinidumpContext::from_raw(MinidumpRawContext::Ppc(ctx)))
                } else {
                    Err(ContextError::ReadFailure)
                }
            }
            Some(PROCESSOR_ARCHITECTURE_PPC64) => {
                let ctx: md::CONTEXT_PPC64 = bytes
                    .gread_with(&mut offset, endian)
                    .or(Err(ContextError::ReadFailure))?;

                let flags = ContextFlagsCpu::from_flags(ctx.context_flags as u32);
                if flags == ContextFlagsCpu::CONTEXT_PPC64 {
                    Ok(MinidumpContext::from_raw(MinidumpRawContext::Ppc64(ctx)))
                } else {
                    Err(ContextError::ReadFailure)
                }
            }
            Some(PROCESSOR_ARCHITECTURE_SPARC) => {
                let ctx: md::CONTEXT_SPARC = bytes
                    .gread_with(&mut offset, endian)
                    .or(Err(ContextError::ReadFailure))?;

                let flags = ContextFlagsCpu::from_flags(ctx.context_flags);
                if flags == ContextFlagsCpu::CONTEXT_SPARC {
                    Ok(MinidumpContext::from_raw(MinidumpRawContext::Sparc(ctx)))
                } else {
                    Err(ContextError::ReadFailure)
                }
            }
            Some(PROCESSOR_ARCHITECTURE_ARM) => {
                let ctx: md::CONTEXT_ARM = bytes
                    .gread_with(&mut offset, endian)
                    .or(Err(ContextError::ReadFailure))?;

                let flags = ContextFlagsCpu::from_flags(ctx.context_flags);
                if flags == ContextFlagsCpu::CONTEXT_ARM {
                    Ok(MinidumpContext::from_raw(MinidumpRawContext::Arm(ctx)))
                } else {
                    Err(ContextError::ReadFailure)
                }
            }
            Some(PROCESSOR_ARCHITECTURE_ARM64) => {
                let ctx: md::CONTEXT_ARM64 = bytes
                    .gread_with(&mut offset, endian)
                    .or(Err(ContextError::ReadFailure))?;

                let flags = ContextFlagsCpu::from_flags(ctx.context_flags);
                if flags == ContextFlagsCpu::CONTEXT_ARM64 {
                    Ok(MinidumpContext::from_raw(MinidumpRawContext::Arm64(ctx)))
                } else {
                    Err(ContextError::ReadFailure)
                }
            }
            Some(PROCESSOR_ARCHITECTURE_ARM64_OLD) => {
                let ctx: md::CONTEXT_ARM64_OLD = bytes
                    .gread_with(&mut offset, endian)
                    .or(Err(ContextError::ReadFailure))?;

                let flags = ContextFlagsCpu::from_flags(ctx.context_flags as u32);
                if flags == ContextFlagsCpu::CONTEXT_ARM64_OLD {
                    Ok(MinidumpContext::from_raw(MinidumpRawContext::OldArm64(ctx)))
                } else {
                    Err(ContextError::ReadFailure)
                }
            }
            Some(PROCESSOR_ARCHITECTURE_MIPS) => {
                let ctx: md::CONTEXT_MIPS = bytes
                    .gread_with(&mut offset, endian)
                    .or(Err(ContextError::ReadFailure))?;

                let flags = ContextFlagsCpu::from_flags(ctx.context_flags);
                if flags == ContextFlagsCpu::CONTEXT_MIPS {
                    Ok(MinidumpContext::from_raw(MinidumpRawContext::Mips(ctx)))
                } else {
                    Err(ContextError::ReadFailure)
                }
            }
            _ => Err(ContextError::UnknownCpuContext),
        }
    }

    pub fn get_instruction_pointer(&self) -> u64 {
        match self.raw {
            MinidumpRawContext::Amd64(ref ctx) => ctx.rip,
            MinidumpRawContext::Arm(ref ctx) => {
                ctx.iregs[md::ArmRegisterNumbers::ProgramCounter as usize] as u64
            }
            MinidumpRawContext::Arm64(ref ctx) => ctx.pc,
            MinidumpRawContext::OldArm64(ref ctx) => ctx.pc,
            MinidumpRawContext::Ppc(ref ctx) => ctx.srr0 as u64,
            MinidumpRawContext::Ppc64(ref ctx) => ctx.srr0,
            MinidumpRawContext::Sparc(ref ctx) => ctx.pc,
            MinidumpRawContext::X86(ref ctx) => ctx.eip as u64,
            MinidumpRawContext::Mips(ref ctx) => ctx.epc,
        }
    }

    pub fn get_stack_pointer(&self) -> u64 {
        match self.raw {
            MinidumpRawContext::Amd64(ref ctx) => ctx.rsp,
            MinidumpRawContext::Arm(ref ctx) => {
                ctx.iregs[md::ArmRegisterNumbers::StackPointer as usize] as u64
            }
            MinidumpRawContext::Arm64(ref ctx) => ctx.sp,
            MinidumpRawContext::OldArm64(ref ctx) => ctx.sp,
            MinidumpRawContext::Ppc(ref ctx) => {
                ctx.gpr[md::PpcRegisterNumbers::StackPointer as usize] as u64
            }
            MinidumpRawContext::Ppc64(ref ctx) => {
                ctx.gpr[md::Ppc64RegisterNumbers::StackPointer as usize]
            }
            MinidumpRawContext::Sparc(ref ctx) => {
                ctx.g_r[md::SparcRegisterNumbers::StackPointer as usize]
            }
            MinidumpRawContext::X86(ref ctx) => ctx.esp as u64,
            MinidumpRawContext::Mips(ref ctx) => {
                ctx.iregs[md::MipsRegisterNumbers::StackPointer as usize]
            }
        }
    }

    pub fn get_register_always(&self, reg: &str) -> u64 {
        match self.raw {
            MinidumpRawContext::Amd64(ref ctx) => ctx.get_register_always(reg),
            MinidumpRawContext::Arm(ref ctx) => ctx.get_register_always(reg).into(),
            MinidumpRawContext::Arm64(ref ctx) => ctx.get_register_always(reg),
            MinidumpRawContext::OldArm64(ref ctx) => ctx.get_register_always(reg),
            MinidumpRawContext::Ppc(ref ctx) => ctx.get_register_always(reg).into(),
            MinidumpRawContext::Ppc64(ref ctx) => ctx.get_register_always(reg),
            MinidumpRawContext::Sparc(ref ctx) => ctx.get_register_always(reg),
            MinidumpRawContext::X86(ref ctx) => ctx.get_register_always(reg).into(),
            MinidumpRawContext::Mips(ref ctx) => ctx.get_register_always(reg),
        }
    }

    pub fn get_register(&self, reg: &str) -> Option<u64> {
        let valid = match &self.raw {
            MinidumpRawContext::X86(ctx) => ctx.register_is_valid(reg, &self.valid),
            MinidumpRawContext::Ppc(ctx) => ctx.register_is_valid(reg, &self.valid),
            MinidumpRawContext::Ppc64(ctx) => ctx.register_is_valid(reg, &self.valid),
            MinidumpRawContext::Amd64(ctx) => ctx.register_is_valid(reg, &self.valid),
            MinidumpRawContext::Sparc(ctx) => ctx.register_is_valid(reg, &self.valid),
            MinidumpRawContext::Arm(ctx) => ctx.register_is_valid(reg, &self.valid),
            MinidumpRawContext::Arm64(ctx) => ctx.register_is_valid(reg, &self.valid),
            MinidumpRawContext::OldArm64(ctx) => ctx.register_is_valid(reg, &self.valid),
            MinidumpRawContext::Mips(ctx) => ctx.register_is_valid(reg, &self.valid),
        };

        if valid {
            Some(self.get_register_always(reg))
        } else {
            None
        }
    }

    pub fn format_register(&self, reg: &str) -> String {
        match self.raw {
            MinidumpRawContext::Amd64(ref ctx) => ctx.format_register(reg),
            MinidumpRawContext::Arm(ref ctx) => ctx.format_register(reg),
            MinidumpRawContext::Arm64(ref ctx) => ctx.format_register(reg),
            MinidumpRawContext::OldArm64(ref ctx) => ctx.format_register(reg),
            MinidumpRawContext::Ppc(ref ctx) => ctx.format_register(reg),
            MinidumpRawContext::Ppc64(ref ctx) => ctx.format_register(reg),
            MinidumpRawContext::Sparc(ref ctx) => ctx.format_register(reg),
            MinidumpRawContext::X86(ref ctx) => ctx.format_register(reg),
            MinidumpRawContext::Mips(ref ctx) => ctx.format_register(reg),
        }
    }

    pub fn general_purpose_registers(&self) -> &'static [&'static str] {
        match self.raw {
            MinidumpRawContext::Amd64(_) => md::CONTEXT_AMD64::REGISTERS,
            MinidumpRawContext::Arm(_) => md::CONTEXT_ARM::REGISTERS,
            MinidumpRawContext::Arm64(_) => md::CONTEXT_ARM64::REGISTERS,
            MinidumpRawContext::OldArm64(_) => md::CONTEXT_ARM64::REGISTERS,
            MinidumpRawContext::Ppc(_) => md::CONTEXT_PPC::REGISTERS,
            MinidumpRawContext::Ppc64(_) => md::CONTEXT_PPC64::REGISTERS,
            MinidumpRawContext::Sparc(_) => md::CONTEXT_SPARC::REGISTERS,
            MinidumpRawContext::X86(_) => md::CONTEXT_X86::REGISTERS,
            MinidumpRawContext::Mips(_) => md::CONTEXT_MIPS::REGISTERS,
        }
    }

    pub fn registers(&self) -> impl Iterator<Item = (&'static str, u64)> + '_ {
        self.general_purpose_registers()
            .iter()
            .map(move |&reg| (reg, self.get_register_always(reg)))
    }

    pub fn valid_registers(&self) -> impl Iterator<Item = (&'static str, u64)> + '_ {
        // This is suboptimal in theory, as we could iterate over self.valid just like the original
        // and faster `CpuRegisters` iterator does. However, this complicates code here, and the
        // minimal gain in performance hasn't been worth the added complexity.
        self.registers().filter(move |(reg, _)| match &self.raw {
            MinidumpRawContext::X86(ctx) => ctx.register_is_valid(reg, &self.valid),
            MinidumpRawContext::Ppc(ctx) => ctx.register_is_valid(reg, &self.valid),
            MinidumpRawContext::Ppc64(ctx) => ctx.register_is_valid(reg, &self.valid),
            MinidumpRawContext::Amd64(ctx) => ctx.register_is_valid(reg, &self.valid),
            MinidumpRawContext::Sparc(ctx) => ctx.register_is_valid(reg, &self.valid),
            MinidumpRawContext::Arm(ctx) => ctx.register_is_valid(reg, &self.valid),
            MinidumpRawContext::Arm64(ctx) => ctx.register_is_valid(reg, &self.valid),
            MinidumpRawContext::OldArm64(ctx) => ctx.register_is_valid(reg, &self.valid),
            MinidumpRawContext::Mips(ctx) => ctx.register_is_valid(reg, &self.valid),
        })
    }

    /// Get the size (in bytes) of general-purpose registers.
    pub fn register_size(&self) -> usize {
        fn get<T: CpuContext>(_: &T) -> usize {
            std::mem::size_of::<T::Register>()
        }

        match &self.raw {
            MinidumpRawContext::X86(ctx) => get(ctx),
            MinidumpRawContext::Ppc(ctx) => get(ctx),
            MinidumpRawContext::Ppc64(ctx) => get(ctx),
            MinidumpRawContext::Amd64(ctx) => get(ctx),
            MinidumpRawContext::Sparc(ctx) => get(ctx),
            MinidumpRawContext::Arm(ctx) => get(ctx),
            MinidumpRawContext::Arm64(ctx) => get(ctx),
            MinidumpRawContext::OldArm64(ctx) => get(ctx),
            MinidumpRawContext::Mips(ctx) => get(ctx),
        }
    }

    /// Write a human-readable description of this `MinidumpContext` to `f`.
    ///
    /// This is very verbose, it is the format used by `minidump_dump`.
    pub fn print<T: Write>(&self, f: &mut T) -> io::Result<()> {
        match self.raw {
            MinidumpRawContext::X86(ref raw) => {
                write!(
                    f,
                    r#"CONTEXT_X86
  context_flags                = {:#x}
  dr0                          = {:#x}
  dr1                          = {:#x}
  dr2                          = {:#x}
  dr3                          = {:#x}
  dr6                          = {:#x}
  dr7                          = {:#x}
  float_save.control_word      = {:#x}
  float_save.status_word       = {:#x}
  float_save.tag_word          = {:#x}
  float_save.error_offset      = {:#x}
  float_save.error_selector    = {:#x}
  float_save.data_offset       = {:#x}
  float_save.data_selector     = {:#x}
  float_save.register_area[{:2}] = 0x"#,
                    raw.context_flags,
                    raw.dr0,
                    raw.dr1,
                    raw.dr2,
                    raw.dr3,
                    raw.dr6,
                    raw.dr7,
                    raw.float_save.control_word,
                    raw.float_save.status_word,
                    raw.float_save.tag_word,
                    raw.float_save.error_offset,
                    raw.float_save.error_selector,
                    raw.float_save.data_offset,
                    raw.float_save.data_selector,
                    raw.float_save.register_area.len(),
                )?;
                write_bytes(f, &raw.float_save.register_area)?;
                writeln!(f)?;
                write!(
                    f,
                    r#"  float_save.cr0_npx_state     = {:#x}
  gs                           = {:#x}
  fs                           = {:#x}
  es                           = {:#x}
  ds                           = {:#x}
  edi                          = {:#x}
  esi                          = {:#x}
  ebx                          = {:#x}
  edx                          = {:#x}
  ecx                          = {:#x}
  eax                          = {:#x}
  ebp                          = {:#x}
  eip                          = {:#x}
  cs                           = {:#x}
  eflags                       = {:#x}
  esp                          = {:#x}
  ss                           = {:#x}
  extended_registers[{:3}]      = 0x"#,
                    raw.float_save.cr0_npx_state,
                    raw.gs,
                    raw.fs,
                    raw.es,
                    raw.ds,
                    raw.edi,
                    raw.esi,
                    raw.ebx,
                    raw.edx,
                    raw.ecx,
                    raw.eax,
                    raw.ebp,
                    raw.eip,
                    raw.cs,
                    raw.eflags,
                    raw.esp,
                    raw.ss,
                    raw.extended_registers.len(),
                )?;
                write_bytes(f, &raw.extended_registers)?;
                write!(f, "\n\n")?;
            }
            MinidumpRawContext::Ppc(_) => {
                unimplemented!();
            }
            MinidumpRawContext::Ppc64(_) => {
                unimplemented!();
            }
            MinidumpRawContext::Amd64(ref raw) => {
                write!(
                    f,
                    r#"CONTEXT_AMD64
  p1_home       = {:#x}
  p2_home       = {:#x}
  p3_home       = {:#x}
  p4_home       = {:#x}
  p5_home       = {:#x}
  p6_home       = {:#x}
  context_flags = {:#x}
  mx_csr        = {:#x}
  cs            = {:#x}
  ds            = {:#x}
  es            = {:#x}
  fs            = {:#x}
  gs            = {:#x}
  ss            = {:#x}
  eflags        = {:#x}
  dr0           = {:#x}
  dr1           = {:#x}
  dr2           = {:#x}
  dr3           = {:#x}
  dr6           = {:#x}
  dr7           = {:#x}
  rax           = {:#x}
  rcx           = {:#x}
  rdx           = {:#x}
  rbx           = {:#x}
  rsp           = {:#x}
  rbp           = {:#x}
  rsi           = {:#x}
  rdi           = {:#x}
  r8            = {:#x}
  r9            = {:#x}
  r10           = {:#x}
  r11           = {:#x}
  r12           = {:#x}
  r13           = {:#x}
  r14           = {:#x}
  r15           = {:#x}
  rip           = {:#x}

"#,
                    raw.p1_home,
                    raw.p2_home,
                    raw.p3_home,
                    raw.p4_home,
                    raw.p5_home,
                    raw.p6_home,
                    raw.context_flags,
                    raw.mx_csr,
                    raw.cs,
                    raw.ds,
                    raw.es,
                    raw.fs,
                    raw.gs,
                    raw.ss,
                    raw.eflags,
                    raw.dr0,
                    raw.dr1,
                    raw.dr2,
                    raw.dr3,
                    raw.dr6,
                    raw.dr7,
                    raw.rax,
                    raw.rcx,
                    raw.rdx,
                    raw.rbx,
                    raw.rsp,
                    raw.rbp,
                    raw.rsi,
                    raw.rdi,
                    raw.r8,
                    raw.r9,
                    raw.r10,
                    raw.r11,
                    raw.r12,
                    raw.r13,
                    raw.r14,
                    raw.r15,
                    raw.rip,
                )?;
            }
            MinidumpRawContext::Sparc(_) => {
                unimplemented!();
            }
            MinidumpRawContext::Arm(ref raw) => {
                write!(
                    f,
                    r#"CONTEXT_ARM
  context_flags       = {:#x}
"#,
                    raw.context_flags
                )?;
                for (i, reg) in raw.iregs.iter().enumerate() {
                    writeln!(f, "  iregs[{i:2}]            = {reg:#x}")?;
                }
                write!(
                    f,
                    r#"  cpsr                = {:#x}
  float_save.fpscr     = {:#x}
"#,
                    raw.cpsr, raw.float_save.fpscr
                )?;
                for (i, reg) in raw.float_save.regs.iter().enumerate() {
                    writeln!(f, "  float_save.regs[{i:2}] = {reg:#x}")?;
                }
                for (i, reg) in raw.float_save.extra.iter().enumerate() {
                    writeln!(f, "  float_save.extra[{i:2}] = {reg:#x}")?;
                }
            }
            MinidumpRawContext::Arm64(ref raw) => {
                write!(
                    f,
                    r#"CONTEXT_ARM64
  context_flags        = {:#x}
"#,
                    raw.context_flags
                )?;
                for (i, reg) in raw.iregs[..29].iter().enumerate() {
                    writeln!(f, "  x{i:<2}                  = {reg:#x}")?;
                }
                writeln!(f, "  x29 (fp)             = {:#x}", raw.iregs[29])?;
                writeln!(f, "  x30 (lr)             = {:#x}", raw.iregs[30])?;
                writeln!(f, "  sp                   = {:#x}", raw.sp)?;
                writeln!(f, "  pc                   = {:#x}", raw.pc)?;
                writeln!(f, "  cpsr                 = {:#x}", raw.cpsr)?;
                writeln!(f, "  fpsr                 = {:#x}", raw.fpsr)?;
                writeln!(f, "  fpcr                 = {:#x}", raw.fpcr)?;
                for (i, reg) in raw.float_regs.iter().enumerate() {
                    writeln!(f, "  d{i:<2} = {reg:#x}")?;
                }
                for (i, reg) in raw.bcr.iter().enumerate() {
                    writeln!(f, "  bcr[{i:2}] = {reg:#x}")?;
                }
                for (i, reg) in raw.bvr.iter().enumerate() {
                    writeln!(f, "  bvr[{i:2}] = {reg:#x}")?;
                }
                for (i, reg) in raw.wcr.iter().enumerate() {
                    writeln!(f, "  wcr[{i:2}] = {reg:#x}")?;
                }
                for (i, reg) in raw.wvr.iter().enumerate() {
                    writeln!(f, "  wvr[{i:2}] = {reg:#x}")?;
                }
            }
            MinidumpRawContext::OldArm64(ref raw) => {
                write!(
                    f,
                    r#"CONTEXT_ARM64_OLD
  context_flags        = {:#x}
"#,
                    raw.context_flags
                )?;
                for (i, reg) in raw.iregs[..29].iter().enumerate() {
                    writeln!(f, "  x{i:<2}                  = {reg:#x}")?;
                }
                writeln!(f, "  x29 (fp)             = {:#x}", raw.iregs[29])?;
                writeln!(f, "  x30 (lr)             = {:#x}", raw.iregs[30])?;
                writeln!(f, "  sp                   = {:#x}", raw.sp)?;
                writeln!(f, "  pc                   = {:#x}", raw.pc)?;
                writeln!(f, "  cpsr                 = {:#x}", raw.cpsr)?;
                writeln!(f, "  fpsr                 = {:#x}", raw.fpsr)?;
                writeln!(f, "  fpcr                 = {:#x}", raw.fpcr)?;
                for (i, reg) in raw.float_regs.iter().enumerate() {
                    writeln!(f, "  d{i:<2} = {reg:#x}")?;
                }
            }
            MinidumpRawContext::Mips(ref raw) => {
                write!(
                    f,
                    r#"CONTEXT_MIPS
  context_flags       = {:#x}
"#,
                    raw.context_flags
                )?;

                use md::MipsRegisterNumbers;
                const MIPS_REGS: &[MipsRegisterNumbers] = &[
                    MipsRegisterNumbers::S0,
                    MipsRegisterNumbers::S1,
                    MipsRegisterNumbers::S2,
                    MipsRegisterNumbers::S3,
                    MipsRegisterNumbers::S4,
                    MipsRegisterNumbers::S5,
                    MipsRegisterNumbers::S6,
                    MipsRegisterNumbers::S7,
                    MipsRegisterNumbers::GlobalPointer,
                    MipsRegisterNumbers::StackPointer,
                    MipsRegisterNumbers::FramePointer,
                    MipsRegisterNumbers::ReturnAddress,
                ];
                for reg in MIPS_REGS {
                    writeln!(
                        f,
                        r#"  {}                = {:#x}"#,
                        reg.name(),
                        raw.iregs[*reg as usize]
                    )?;
                }
            }
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    /// Smoke test for the default implementation of `memoize_register`.
    fn test_memoize_amd64() {
        let context = md::CONTEXT_AMD64::default();
        assert_eq!(context.memoize_register("rip"), Some("rip"));
        assert_eq!(context.memoize_register("foo"), None);
    }

    #[test]
    /// Test ARM register aliases by example of `fp`.
    fn test_memoize_arm_alias() {
        let context = md::CONTEXT_ARM::default();
        assert_eq!(context.memoize_register("r11"), Some("fp"));
        assert_eq!(context.memoize_register("fp"), Some("fp"));
        assert_eq!(context.memoize_register("foo"), None);
    }

    #[test]
    /// Test ARM register aliases by example of `fp`.
    fn test_memoize_arm64_alias() {
        let context = md::CONTEXT_ARM64::default();
        assert_eq!(context.memoize_register("x29"), Some("fp"));
        assert_eq!(context.memoize_register("fp"), Some("fp"));
        assert_eq!(context.memoize_register("foo"), None);
    }
}
