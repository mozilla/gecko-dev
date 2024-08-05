//! Unwind information for x86_64 (usually called x64 in microsoft documentation).
//!
//! The high-level API is accessed through `FunctionTableEntries::unwind_frame`. This function
//! allows you to unwind a frame to get the return address and all updated contextual registers.

use arrayvec::ArrayVec;
use std::ops::ControlFlow;
use thiserror::Error;
use zerocopy::{FromBytes, Ref, Unaligned, LE};
use zerocopy_derive::{FromBytes, FromZeroes, Unaligned};

type U16 = zerocopy::U16<LE>;

/// Little-endian u32.
pub type U32 = zerocopy::U32<LE>;

/// A view over function table entries in the `.pdata` section.
#[derive(Debug, Clone, Copy)]
pub struct FunctionTableEntries<'a> {
    data: &'a [u8],
}

/// A runtime function record in the function table.
#[derive(Unaligned, FromZeroes, FromBytes, Debug, Clone, Copy)]
#[repr(C)]
pub struct RuntimeFunction {
    /// The start relative virtual address of the function.
    pub begin_address: U32,
    /// The end relative virtual address of the function.
    pub end_address: U32,
    /// The relative virtual address of the unwind information related to the function.
    pub unwind_info_address: U32,
}

impl<'a> FunctionTableEntries<'a> {
    /// Parse function table entries from the given `.pdata` section contents.
    pub fn parse(data: &'a [u8]) -> Self {
        FunctionTableEntries { data }
    }

    /// Get the number of `RuntimeFunction` stored in the function table.
    pub fn functions_len(&self) -> usize {
        self.data.len() / std::mem::size_of::<RuntimeFunction>()
    }

    /// Get the `RuntimeFunction`s in the function table, if the parsed data is well-aligned and
    /// sized.
    pub fn functions(&self) -> Option<&'a [RuntimeFunction]> {
        Ref::new_slice_unaligned(self.data).map(|lv| lv.into_slice())
    }

    /// Lookup the runtime function that contains the given relative virtual address.
    pub fn lookup(&self, address: u32) -> Option<&'a RuntimeFunction> {
        let functions = self.functions()?;
        match functions.binary_search_by_key(&address, |f| f.begin_address.get()) {
            Ok(i) => Some(&functions[i]),
            Err(i) if i > 0 && address < functions[i - 1].end_address.get() => {
                Some(&functions[i - 1])
            }
            _ => None,
        }
    }

    pub fn unwind_frame<'m, S: UnwindState, M>(
        &self,
        state: &mut S,
        mut memory_at_rva: M,
        address: u32,
    ) -> Option<u64>
    where
        M: FnMut(u32) -> Option<&'m [u8]> + 'm,
    {
        // This implements the procedure found
        // [here](https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64?view=msvc-170#unwind-procedure).
        if let Some(mut function) = self.lookup(address) {
            let offset = address - function.begin_address.get();
            let mut is_chained = false;
            loop {
                let unwind_info =
                    UnwindInfo::parse(memory_at_rva(function.unwind_info_address.get())?)?;

                if !is_chained {
                    // Check whether the address is in the function epilog. If so, we need to
                    // simulate the remaining epilog instructions (unwind codes don't account for
                    // unwinding from the epilog).
                    let bytes = (function.end_address.get() - address) as usize;
                    let instruction = &memory_at_rva(address)?[..bytes];
                    if let Ok(epilog_instructions) = FunctionEpilogInstruction::parse_sequence(
                        instruction,
                        unwind_info.frame_register(),
                    ) {
                        for instruction in epilog_instructions.iter() {
                            match instruction {
                                FunctionEpilogInstruction::AddSP(offset) => {
                                    let rsp = state.read_register(Register::RSP);
                                    state.write_register(Register::RSP, rsp + *offset as u64);
                                }
                                FunctionEpilogInstruction::AddSPFromFP(offset) => {
                                    let fp = unwind_info
                                        .frame_register()
                                        .expect("invalid fp register offset");
                                    let fp = state.read_register(fp);
                                    state.write_register(Register::RSP, fp + *offset as u64);
                                }
                                FunctionEpilogInstruction::Pop(reg) => {
                                    let rsp = state.read_register(Register::RSP);
                                    let val = state.read_stack(rsp)?;
                                    state.write_register(*reg, val);
                                    state.write_register(Register::RSP, rsp + 8);
                                }
                            }
                        }
                        break;
                    }
                }

                for (_, op) in unwind_info
                    .unwind_operations()
                    .skip_while(|(o, _)| !is_chained && *o as u32 > offset)
                {
                    if let ControlFlow::Break(rip) = unwind_info.resolve_operation(state, &op)? {
                        return Some(rip);
                    }
                }
                if let Some(UnwindInfoTrailer::ChainedUnwindInfo { chained }) =
                    unwind_info.trailer()
                {
                    is_chained = true;
                    function = chained;
                } else {
                    break;
                }
            }
        }
        let rsp = state.read_register(Register::RSP);
        let rip = state.read_stack(rsp)?;
        state.write_register(Register::RSP, rsp + 8);
        Some(rip)
    }

    /// Unwind a single frame at the given relative virtual address.
    ///
    /// This does not attempt to invoke any exception or termination handlers.
    ///
    /// Returns `None` if `UnwindInfo` could not be parsed, a stack value could not be read, or a
    /// memory offset in the binary could not be read (whether when parsing the section table or
    /// when reading memory pointed to by the section table).
    pub fn unwind_frame_with_image<S: UnwindState>(
        &self,
        state: &mut S,
        image: &[u8],
        address: u32,
    ) -> Option<u64> {
        let sections = Sections::parse(image)?;
        self.unwind_frame(state, |addr| sections.memory_at_rva(addr), address)
    }
}

impl<'a> Iterator for FunctionTableEntries<'a> {
    type Item = &'a RuntimeFunction;

    fn next(&mut self) -> Option<Self::Item> {
        let (rf, rest) = Ref::<_, RuntimeFunction>::new_unaligned_from_prefix(self.data)?;
        self.data = rest;
        Some(rf.into_ref())
    }
}

#[derive(Debug, Clone, Copy)]
struct Sections<'a> {
    image: &'a [u8],
    sections: &'a [Section],
}

impl<'a> Sections<'a> {
    pub fn parse(image: &'a [u8]) -> Option<Self> {
        let sig_offset = Ref::<_, U32>::new_unaligned(image.get(0x3c..0x40)?)?.get() as usize;
        // Offset to the COFF header
        let coff_image = image.get(sig_offset + 4..)?;
        let section_count = Ref::<_, U16>::new_unaligned(coff_image.get(2..4)?)?.get() as usize;
        let size_of_optional_header =
            Ref::<_, U16>::new_unaligned(coff_image.get(16..18)?)?.get() as usize;
        let sections = Ref::<_, [Section]>::new_slice_unaligned_from_prefix(
            &coff_image[20 + size_of_optional_header..],
            section_count,
        )?
        .0
        .into_slice();
        Some(Sections { image, sections })
    }

    pub fn memory_at_rva(&self, rva: u32) -> Option<&'a [u8]> {
        let section_index = match self
            .sections
            .binary_search_by_key(&rva, |s| s.virtual_address.get())
        {
            Ok(i) => i,
            Err(i)
                if i > 0
                    && rva
                        < self.sections[i - 1].virtual_address.get()
                            + self.sections[i - 1].virtual_size.get() =>
            {
                i - 1
            }
            Err(_) => return None,
        };
        let section = &self.sections[section_index];
        let start = section.pointer_to_raw_data.get() as usize;
        let offset = (rva - section.virtual_address.get()) as usize;
        Some(&self.image[start + offset..])
    }
}

#[derive(Unaligned, FromZeroes, FromBytes, Debug, Clone, Copy)]
#[repr(C)]
struct Section {
    _name: [u8; 8],
    virtual_size: U32,
    virtual_address: U32,
    _size_of_raw_data: U32,
    pointer_to_raw_data: U32,
    _pointer_to_relocations: U32,
    _pointer_to_line_numbers: U32,
    _number_of_relocations: U16,
    _number_of_line_numbers: U16,
    _characteristics: U32,
}

/// A general-purpose register.
///
/// If converted to a u8, the resulting value matches those in the x86_64 spec for register
/// operands as well as the operation info bits in unwind codes.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[repr(u8)]
pub enum Register {
    RAX,
    RCX,
    RDX,
    RBX,
    RSP,
    RBP,
    RSI,
    RDI,
    R8,
    R9,
    R10,
    R11,
    R12,
    R13,
    R14,
    R15,
}

impl TryFrom<u8> for Register {
    type Error = ();
    #[inline(always)]
    fn try_from(reg: u8) -> Result<Self, ()> {
        let reg = match reg {
            0 => Self::RAX,
            1 => Self::RCX,
            2 => Self::RDX,
            3 => Self::RBX,
            4 => Self::RSP,
            5 => Self::RBP,
            6 => Self::RSI,
            7 => Self::RDI,
            8 => Self::R8,
            9 => Self::R9,
            10 => Self::R10,
            11 => Self::R11,
            12 => Self::R12,
            13 => Self::R13,
            14 => Self::R14,
            15 => Self::R15,
            _ => return Err(()),
        };
        Ok(reg)
    }
}

/// A 128-bit XMM register.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[repr(u8)]
pub enum XmmRegister {
    XMM0,
    XMM1,
    XMM2,
    XMM3,
    XMM4,
    XMM5,
    XMM6,
    XMM7,
    XMM8,
    XMM9,
    XMM10,
    XMM11,
    XMM12,
    XMM13,
    XMM14,
    XMM15,
}

impl TryFrom<u8> for XmmRegister {
    type Error = ();
    #[inline(always)]
    fn try_from(reg: u8) -> Result<Self, ()> {
        let reg = match reg {
            0 => Self::XMM0,
            1 => Self::XMM1,
            2 => Self::XMM2,
            3 => Self::XMM3,
            4 => Self::XMM4,
            5 => Self::XMM5,
            6 => Self::XMM6,
            7 => Self::XMM7,
            8 => Self::XMM8,
            9 => Self::XMM9,
            10 => Self::XMM10,
            11 => Self::XMM11,
            12 => Self::XMM12,
            13 => Self::XMM13,
            14 => Self::XMM14,
            15 => Self::XMM15,
            _ => return Err(()),
        };
        Ok(reg)
    }
}

/// Fixed data at the start of [PE UnwindInfo][unwindinfo].
///
/// [unwindinfo]: https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64?view=msvc-170#struct-unwind_info
#[derive(Unaligned, FromZeroes, FromBytes, Debug, Clone, Copy)]
#[repr(C)]
pub struct UnwindInfoHeader {
    /// The unwind information version and flags.
    pub version_and_flags: u8,
    /// The length of the function prolog, in bytes.
    pub prolog_size: u8,
    /// The number of u16 slots in the unwind codes array.
    pub unwind_codes_len: u8,
    /// The frame register and offset.
    pub frame_register_and_offset: u8,
}

bitflags::bitflags! {
    /// The unwind info bit flags.
    ///
    /// Note that while they are individual bits, it seems as if they can only be
    /// mutually-exclusive.
    #[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
    pub struct UnwindInfoFlags: u8 {
        /// The function has an exception handler that should be called when looking for functions
        /// that need to examine exceptions.
        const EHANDLER = 0x1;
        /// The function has a termination handler that should be called when unwinding an
        /// exception.
        const UHANDLER = 0x2;
        /// This unwind info structure is not the primary one for the procedure. Instead, the
        /// chained unwind info entry is the contents of a previous RuntimeFunction entry. If this
        /// flag is set, then the EHANDLER and UHANDLER flags must be cleared. Also, the frame
        /// register and fixed-stack allocation fields must have the same values as in the primary
        /// unwind info.
        const CHAININFO = 0x4;
    }
}

impl UnwindInfoHeader {
    /// The UnwindInfo version. Should be `1`.
    #[inline]
    pub fn version(&self) -> u8 {
        self.version_and_flags & 0x7
    }

    /// The raw flags bits.
    #[inline]
    pub fn flags_raw(&self) -> u8 {
        self.version_and_flags >> 3
    }

    /// The raw frame register value.
    #[inline]
    pub fn frame_register_raw(&self) -> u8 {
        self.frame_register_and_offset & 0xf
    }

    /// The raw frame register offset value.
    #[inline]
    pub fn frame_register_offset_raw(&self) -> u8 {
        self.frame_register_and_offset >> 4
    }

    /// The unwind info flags.
    pub fn flags(&self) -> UnwindInfoFlags {
        UnwindInfoFlags::from_bits_truncate(self.flags_raw())
    }

    /// The frame register, if any.
    pub fn frame_register(&self) -> Option<Register> {
        let reg = self.frame_register_raw();
        (reg != 0).then_some(
            reg.try_into()
                .expect("reg is <= 15 so this always succeeds"),
        )
    }

    /// The scaled frame register offset.
    pub fn frame_register_offset(&self) -> u8 {
        // u8 is appropriate as the maximum value is 15 * 16 = 240
        self.frame_register_offset_raw() * 16
    }

    /// Get an absolute address from the given StackFrameOffset.
    pub fn resolve_offset<F>(&self, read_register: F, offset: StackFrameOffset) -> u64
    where
        F: FnOnce(Register) -> u64,
    {
        match self.frame_register() {
            Some(reg) => read_register(reg) - self.frame_register_offset() as u64 + offset.0 as u64,
            None => read_register(Register::RSP) + offset.0 as u64,
        }
    }

    /// Perform the given UnwindOperation, changing `state` appropriately.
    ///
    /// Returns `None` when reading the stack fails.
    pub fn resolve_operation<S: UnwindState>(
        &self,
        state: &mut S,
        op: &UnwindOperation,
    ) -> Option<ControlFlow<u64>> {
        match op {
            UnwindOperation::PopNonVolatile(reg) => {
                let rsp = state.read_register(Register::RSP);
                let value = state.read_stack(rsp)?;
                state.write_register(*reg, value);
                state.write_register(Register::RSP, rsp + 8);
            }
            UnwindOperation::UnStackAlloc(bytes) => {
                let rsp = state.read_register(Register::RSP);
                state.write_register(Register::RSP, rsp + *bytes as u64);
            }
            UnwindOperation::RestoreSPFromFP => {
                if let Some(reg) = self.frame_register() {
                    let value = state.read_register(reg) - self.frame_register_offset() as u64;
                    state.write_register(Register::RSP, value);
                }
            }
            UnwindOperation::ReadNonVolatile(reg, offset) => {
                let addr = self.resolve_offset(|reg| state.read_register(reg), *offset);
                let value = state.read_stack(addr)?;
                state.write_register(*reg, value);
            }
            UnwindOperation::ReadXMM(reg, offset) => {
                let addr = self.resolve_offset(|reg| state.read_register(reg), *offset);
                let value =
                    state.read_stack(addr)? as u128 | ((state.read_stack(addr + 8)? as u128) << 64);
                state.write_xmm_register(*reg, value);
            }
            UnwindOperation::PopMachineFrame { error_code } => {
                let offset = if *error_code { 8 } else { 0 };
                let rsp = state.read_register(Register::RSP);
                let return_address = state.read_stack(rsp + offset)?;
                let rsp = state.read_stack(rsp + offset + 24)?;
                state.write_register(Register::RSP, rsp);
                return Some(ControlFlow::Break(return_address));
            }
        }

        Some(ControlFlow::Continue(()))
    }
}

/// A virtual instruction in the function epilog, interpreted from x86_64 instructions.
#[derive(Debug, Clone, Copy)]
pub enum FunctionEpilogInstruction {
    /// Add the given offset to the stack pointer.
    AddSP(u32),
    /// Add the given offset to the frame pointer to recover the stack pointer.
    AddSPFromFP(u32),
    /// Pop a value from the stack into the given register.
    Pop(Register),
}

/// An error resulting from an attempt at parsing an epilog instruction.
#[derive(Error, Debug)]
pub enum InstructionParseError {
    #[error("not enough data")]
    NotEnoughData,
    #[error("invalid instruction found")]
    InvalidInstruction,
    #[error("too many instructions for epilog")]
    TooManyInstructions,
}

/// The maximum number of instructions to allow when parsing a function epilog.
///
/// There is at most one AddSP/AddSPFromFP, and only 8 caller-saved registers (disregarding the
/// implicit RSP). We give a bit of extra space just in case, but it shouldn't be necessary.
pub const FUNCTION_EPILOG_LIMIT: usize = 12;

impl FunctionEpilogInstruction {
    /// Parse a function epilog instruction.
    ///
    /// Returns Ok(None) if the instruction is an epilog terminator (`ret` or `jmp`).
    ///
    /// `allow_add_sp` should only be true for the (potential) first instruction in an epilog.
    pub fn parse(
        ip: &[u8],
        fpreg: Option<Register>,
        allow_add_sp: bool,
    ) -> Result<Option<(Self, &[u8])>, InstructionParseError> {
        if ip.is_empty() {
            return Err(InstructionParseError::NotEnoughData);
        }

        // Read REX instruction byte if present.
        let (rex, ip) = if ip[0] & 0xf0 == 0x40 {
            (ip[0] & 0x0f, &ip[1..])
        } else {
            (0, &ip[0..])
        };

        // Both add and lea need at least 3 bytes after REX
        if allow_add_sp && ip.len() >= 3 {
            // add RSP,imm32
            if rex & 0x8 != 0 && ip[0] == 0x81 && ip[1] == 0xc4 {
                let (val, rest) = Ref::<_, U32>::new_unaligned_from_prefix(&ip[2..])
                    .ok_or(InstructionParseError::NotEnoughData)?;
                return Ok(Some((FunctionEpilogInstruction::AddSP(val.get()), rest)));
            }
            // add RSP,imm8
            if rex & 0x8 != 0 && ip[0] == 0x83 && ip[1] == 0xc4 {
                return Ok(Some((
                    FunctionEpilogInstruction::AddSP(ip[2] as u32),
                    &ip[3..],
                )));
            }

            if let Some(fpreg) = fpreg {
                let fpreg = fpreg as u8;
                if rex & 0x8 != 0 && (rex & 0x1 == fpreg >> 3) && ip[0] == 0x8d {
                    if ip[1] & 0x3f == (0x20 | (fpreg & 0b0111)) {
                        let op_mod = ip[1] >> 6;
                        // lea RSP,disp8[FP]
                        if op_mod == 0b01 {
                            return Ok(Some((
                                FunctionEpilogInstruction::AddSPFromFP(ip[2] as u32),
                                &ip[3..],
                            )));
                        // lea RSP,disp32[FP]
                        } else if op_mod == 0b10 {
                            let (val, rest) = Ref::<_, U32>::new_unaligned_from_prefix(&ip[2..])
                                .ok_or(InstructionParseError::NotEnoughData)?;
                            return Ok(Some((
                                FunctionEpilogInstruction::AddSPFromFP(val.get()),
                                rest,
                            )));
                        } else {
                            // Invalid op_mod
                            return Err(InstructionParseError::InvalidInstruction);
                        }
                    } else {
                        // Invalid lea
                        return Err(InstructionParseError::InvalidInstruction);
                    }
                }
            }
        }

        // pop r/m64
        if ip.len() >= 2 && ip[0] == 0x8f && ip[1] & 0xf8 == 0xc0 {
            let reg = ip[1] & 0x7 | ((rex & 1) << 3);
            return Ok(Some((
                FunctionEpilogInstruction::Pop(
                    reg.try_into().expect(
                        "`reg` is between 0 and 15, which are defined values of `Register`.",
                    ),
                ),
                &ip[2..],
            )));
        }
        // pop r64
        if !ip.is_empty() && ip[0] & 0xf8 == 0x58 {
            let reg = ip[0] & 0x7 | ((rex & 1) << 3);
            debug_assert!(reg <= 15);
            return Ok(Some((
                FunctionEpilogInstruction::Pop(
                    reg.try_into().expect(
                        "`reg` is between 0 and 15, which are defined values of `Register`.",
                    ),
                ),
                &ip[1..],
            )));
        }

        // ret
        if !ip.is_empty() && ip[0] == 0xc3 {
            return Ok(None);
        }

        if ip.len() >= 2 {
            // jmp with relative displacements
            //
            // The MS docs say epilogs only have jmp instructions with a ModRM byte, but I've seen
            // relative displacement jmps too (tail calls).
            if ip[0] == 0xeb || ip[0] == 0xe9 {
                return Ok(None);
            }
            // jmp with ModRM and mod bits as 00
            if ip[0] == 0xff {
                let mod_opcode = ip[1] & 0xf8;
                if mod_opcode == 0x20 || mod_opcode == 0x28 {
                    return Ok(None);
                } else {
                    return Err(InstructionParseError::InvalidInstruction);
                }
            }
        }

        // not a valid epilog instruction
        Err(InstructionParseError::InvalidInstruction)
    }

    /// Check whether a series of instructions are a tail of a function epilog
    /// and parse them into a limited sequence of epilog instructions.
    ///
    /// This function does not allocate memory; the result is stored in a
    /// fixed-capacity `ArrayVec`.
    ///
    /// Returns `Err` if too many instructions were encountered or if the
    /// instructions do not appear to be a function epilog.
    ///
    /// [Epilogs][] look like:
    /// * `add RSP,<constant>` or `lea RSP,constant[FPReg]`
    /// * zero or more `pop <GPREG>`
    /// * `ret` or `jmp` with a ModRM argument with mod field 00
    ///
    /// [Epilogs]: https://learn.microsoft.com/en-us/cpp/build/prolog-and-epilog?view=msvc-170#epilog-code
    pub fn parse_sequence(
        ip: &[u8],
        frame_register: Option<Register>,
    ) -> Result<ArrayVec<Self, FUNCTION_EPILOG_LIMIT>, InstructionParseError> {
        let mut buffer = ArrayVec::new();
        let mut instruction_and_rest = Self::parse(ip, frame_register, true)?;

        while let Some((instruction, rest)) = instruction_and_rest {
            buffer
                .try_push(instruction)
                .map_err(|_| InstructionParseError::TooManyInstructions)?;

            instruction_and_rest = Self::parse(rest, frame_register, false)?
        }

        Ok(buffer)
    }
}

/// An interface over state needed for unwinding stack frames.
pub trait UnwindState {
    /// Return the value of the given register.
    fn read_register(&mut self, register: Register) -> u64;
    /// Return the 8-byte value at the given address on the stack, if any.
    fn read_stack(&mut self, addr: u64) -> Option<u64>;
    /// Write a new value to the given register, updating the unwind context.
    fn write_register(&mut self, register: Register, value: u64);
    /// Write a new value to the given xmm register, updating the unwind context.
    fn write_xmm_register(&mut self, register: XmmRegister, value: u128);
}

/// Optional information at the end of UnwindInfo.
pub enum UnwindInfoTrailer<'a> {
    /// There is an exception handler associated with this unwind info.
    ExceptionHandler {
        handler_address: &'a U32,
        handler_data: &'a [u8],
    },
    /// There is a termination handler associated with this unwind info.
    TerminationHandler {
        handler_address: &'a U32,
        handler_data: &'a [u8],
    },
    /// There is a chained unwind info entry associated with this unwind info.
    ChainedUnwindInfo { chained: &'a RuntimeFunction },
}

/// A function's unwind information.
#[derive(Clone, Copy)]
pub struct UnwindInfo<'a> {
    header: &'a UnwindInfoHeader,
    unwind_codes: &'a [u8],
    rest: &'a [u8],
}

impl std::fmt::Debug for UnwindInfo<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        f.debug_struct("UnwindInfo")
            .field("header", self.header)
            .field("unwind_codes", &self.unwind_codes)
            .finish_non_exhaustive()
    }
}

impl<'a> UnwindInfo<'a> {
    /// Read the unwind info from the given buffer.
    ///
    /// Returns None if there aren't enough bytes or the alignment is incorrect.
    pub fn parse(data: &'a [u8]) -> Option<Self> {
        let (header, rest) = Ref::<_, UnwindInfoHeader>::new_unaligned_from_prefix(data)?;
        if header.version() != 1 {
            return None;
        }
        let (unwind_codes, rest) =
            Ref::new_slice_unaligned_from_prefix(rest, header.unwind_codes_len as usize * 2)?;
        Some(UnwindInfo {
            header: header.into_ref(),
            unwind_codes: unwind_codes.into_slice(),
            rest,
        })
    }

    /// Get an iterator over the unwind operations.
    pub fn unwind_operations(&self) -> UnwindOperations<'a> {
        UnwindOperations(self.unwind_codes)
    }

    /// Get the trailing information of the unwind info, if any.
    pub fn trailer(&self) -> Option<UnwindInfoTrailer<'a>> {
        let flags = self.flags();
        if flags.contains(UnwindInfoFlags::EHANDLER) {
            let (handler_address, handler_data) =
                Ref::<_, U32>::new_unaligned_from_prefix(self.rest)?;
            Some(UnwindInfoTrailer::ExceptionHandler {
                handler_address: handler_address.into_ref(),
                handler_data,
            })
        } else if flags.contains(UnwindInfoFlags::UHANDLER) {
            let (handler_address, handler_data) =
                Ref::<_, U32>::new_unaligned_from_prefix(self.rest)?;
            Some(UnwindInfoTrailer::TerminationHandler {
                handler_address: handler_address.into_ref(),
                handler_data,
            })
        } else if flags.contains(UnwindInfoFlags::CHAININFO) {
            let (chained, _) = Ref::<_, RuntimeFunction>::new_unaligned_from_prefix(self.rest)?;
            Some(UnwindInfoTrailer::ChainedUnwindInfo {
                chained: chained.into_ref(),
            })
        } else {
            None
        }
    }
}

impl std::ops::Deref for UnwindInfo<'_> {
    type Target = UnwindInfoHeader;

    fn deref(&self) -> &Self::Target {
        self.header
    }
}

/// An iterator over `UnwindOperation`s.
///
/// This iterator parses the operations as it iterates, since it needs to parse them to know how
/// many slots each takes up.
#[derive(Clone, Copy, Debug)]
pub struct UnwindOperations<'a>(&'a [u8]);

impl<'a> UnwindOperations<'a> {
    /// Get the current `UnwindCode`.
    pub fn unwind_code(&self) -> Option<&'a UnwindCode> {
        let mut c = *self;
        c.read::<UnwindCode>()
    }

    fn read<T: Unaligned + FromBytes>(&mut self) -> Option<&'a T> {
        let (v, rest) = Ref::<_, T>::new_unaligned_from_prefix(self.0)?;
        self.0 = rest;
        Some(v.into_ref())
    }
}

impl<'a> Iterator for UnwindOperations<'a> {
    type Item = (u8, UnwindOperation);

    fn next(&mut self) -> Option<Self::Item> {
        let unwind_code = self.read::<UnwindCode>()?;
        let op = match unwind_code.operation_code()? {
            UnwindOperationCode::PushNonvol => {
                UnwindOperation::PopNonVolatile(unwind_code.operation_info_as_register())
            }
            UnwindOperationCode::AllocLarge => match unwind_code.operation_info() {
                0 => UnwindOperation::UnStackAlloc(self.read::<U16>()?.get() as u32 * 8),
                1 => UnwindOperation::UnStackAlloc(self.read::<U32>()?.get()),
                _ => return None,
            },
            UnwindOperationCode::AllocSmall => {
                UnwindOperation::UnStackAlloc((unwind_code.operation_info() as u32 + 1) * 8)
            }
            UnwindOperationCode::SetFPReg => UnwindOperation::RestoreSPFromFP,
            UnwindOperationCode::SaveNonvol => UnwindOperation::ReadNonVolatile(
                unwind_code.operation_info_as_register(),
                StackFrameOffset(self.read::<U16>()?.get() as u32 * 8),
            ),
            UnwindOperationCode::SaveNonvolFar => UnwindOperation::ReadNonVolatile(
                unwind_code.operation_info_as_register(),
                StackFrameOffset(self.read::<U32>()?.get()),
            ),
            UnwindOperationCode::SaveXmm128 => UnwindOperation::ReadXMM(
                unwind_code.operation_info_as_xmm(),
                StackFrameOffset(self.read::<U16>()?.get() as u32 * 16),
            ),
            UnwindOperationCode::SaveXmm128Far => UnwindOperation::ReadXMM(
                unwind_code.operation_info_as_xmm(),
                StackFrameOffset(self.read::<U32>()?.get()),
            ),
            UnwindOperationCode::PushMachframe => UnwindOperation::PopMachineFrame {
                error_code: unwind_code.operation_info() == 1,
            },
        };

        Some((unwind_code.prolog_offset, op))
    }
}

/// An offset relative to the local stack frame.
#[derive(Debug, Clone, Copy)]
pub struct StackFrameOffset(u32);

/// An unwind operation to perform.
///
/// These generally correspond to `UnwindOperationCode`s, however they are named based on the
/// operation that needs to be done to unwind.
#[derive(Debug, Clone, Copy)]
pub enum UnwindOperation {
    /// Restore a register's value by popping from the stack (incrementing RSP by 8).
    PopNonVolatile(Register),
    /// Undo a stack allocation of the given size (incrementing RSP).
    UnStackAlloc(u32),
    /// Use the frame pointer register to restore RSP. The stack pointer should be restored from
    /// the frame pointer minus UnwindInfo::frame_register_offset().
    RestoreSPFromFP,
    /// Restore a register's value from the given stack frame offset.
    ReadNonVolatile(Register, StackFrameOffset),
    /// Restore an XMM register's value from the given stack frame offset.
    ReadXMM(XmmRegister, StackFrameOffset),
    /// Pop a machine frame. This restores from the stack an optional error code, and then (in
    /// order from lowest to highest addresses) IP, CS, EFLAGS, the old SP, and SS.
    PopMachineFrame { error_code: bool },
}

/// An operation represented by an `UnwindCode`.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[repr(u8)]
pub enum UnwindOperationCode {
    PushNonvol,
    AllocLarge,
    AllocSmall,
    SetFPReg,
    SaveNonvol,
    SaveNonvolFar,
    SaveXmm128 = 8,
    SaveXmm128Far,
    PushMachframe,
}

/// A single step to unwind operations done in a frame's prolog.
#[derive(Unaligned, FromZeroes, FromBytes, Debug, Clone, Copy)]
#[repr(C)]
pub struct UnwindCode {
    /// The byte offset into the prolog where the operation was done.
    pub prolog_offset: u8,
    /// The operation code and info.
    pub opcode_and_opinfo: u8,
}

impl UnwindCode {
    /// Get the raw operation code.
    #[inline]
    pub fn operation_code_raw(&self) -> u8 {
        self.opcode_and_opinfo & 0xf
    }

    /// Get the operation information bits.
    #[inline]
    pub fn operation_info(&self) -> u8 {
        self.opcode_and_opinfo >> 4
    }

    /// Get the operation code.
    pub fn operation_code(&self) -> Option<UnwindOperationCode> {
        match self.operation_code_raw() {
            0 => Some(UnwindOperationCode::PushNonvol),
            1 => Some(UnwindOperationCode::AllocLarge),
            2 => Some(UnwindOperationCode::AllocSmall),
            3 => Some(UnwindOperationCode::SetFPReg),
            4 => Some(UnwindOperationCode::SaveNonvol),
            5 => Some(UnwindOperationCode::SaveNonvolFar),
            8 => Some(UnwindOperationCode::SaveXmm128),
            9 => Some(UnwindOperationCode::SaveXmm128Far),
            10 => Some(UnwindOperationCode::PushMachframe),
            _ => None,
        }
    }

    /// Interpret the operation info as a register.
    #[inline]
    fn operation_info_as_register(&self) -> Register {
        let op_info = self.operation_info();
        op_info
            .try_into()
            .expect("`op_info` is between 0 and 15, which are defined values of `Register`.")
    }

    /// Interpret the operation info as an Xmm register.
    #[inline]
    fn operation_info_as_xmm(&self) -> XmmRegister {
        let op_info = self.operation_info();
        op_info
            .try_into()
            .expect("`op_info` is between 0 and 15, which are defined values of `XmmRegister`.")
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    use hex_literal::hex;
    use memmap2::Mmap;
    use object::read::{File, Object, ObjectSection};
    use std::sync::OnceLock;

    static FIXTURE: OnceLock<Mmap> = OnceLock::new();
    const FIXTURE_ADDRESS: u64 = 0x7ff725bf0000;

    fn get_fixture() -> &'static [u8] {
        FIXTURE.get_or_init(|| unsafe {
            Mmap::map(
                &std::fs::File::open(concat!(
                    env!("CARGO_MANIFEST_DIR"),
                    "/fixture/binary/x86_64.exe"
                ))
                .unwrap(),
            )
            .unwrap()
        })
    }

    struct FrameContext {
        registers: [u64; 16],
        ip: u64,
        stack: &'static [u8],
        stack_base: u64,

        pub changes: RegisterChanges,
    }

    #[derive(Default, Debug, Clone, PartialEq, Eq)]
    struct RegisterChanges {
        changes: [Option<u64>; 16],
    }

    impl RegisterChanges {
        pub fn new() -> Self {
            Self::default()
        }

        pub fn set(mut self, reg: Register, value: u64) -> Self {
            self.changes[reg as usize] = Some(value);
            self
        }
    }

    impl UnwindState for FrameContext {
        fn read_register(&mut self, register: Register) -> u64 {
            self.registers[register as usize]
        }

        fn read_stack(&mut self, addr: u64) -> Option<u64> {
            if addr > self.stack_base {
                return None;
            }
            let offset = self.stack_base - addr;
            let offset = offset as usize;
            if offset < 8 || offset > self.stack.len() {
                return None;
            }
            let index = self.stack.len() - offset;
            Some(u64::from_le_bytes(
                (&self.stack[index..index + 8]).try_into().unwrap(),
            ))
        }

        fn write_register(&mut self, register: Register, value: u64) {
            self.registers[register as usize] = value;
            self.changes.changes[register as usize] = Some(value);
        }

        fn write_xmm_register(&mut self, _register: XmmRegister, _value: u128) {
            unimplemented!()
        }
    }

    macro_rules! windbg_frame_context {
        ( rax = $rax:literal rbx = $rbx:literal rcx = $rcx:literal
          rdx = $rdx:literal rsi = $rsi:literal rdi = $rdi:literal
          rip = $rip:literal rsp = $rsp:literal rbp = $rbp:literal
           r8 = $r8:literal   r9 = $r9:literal  r10 = $r10:literal
          r11 = $r11:literal r12 = $r12:literal r13 = $r13:literal
          r14 = $r14:literal r15 = $r15:literal
          stack_base = $stack_base:literal
          stack = $stack:literal
        ) => {
            FrameContext {
                registers: [
                    $rax, $rcx, $rdx, $rbx, $rsp, $rbp, $rsi, $rdi, $r8, $r9, $r10, $r11, $r12,
                    $r13, $r14, $r15,
                ],
                ip: $rip,
                stack: &hex!($stack),
                stack_base: $stack_base,
                changes: Default::default(),
            }
        };
    }

    fn assert_fixture_unwind(mut context: FrameContext, ra: u64, changes: RegisterChanges) {
        let file = File::parse(get_fixture()).unwrap();
        let pdata_section = file.section_by_name(".pdata").unwrap();
        let entries = FunctionTableEntries::parse(pdata_section.data().unwrap());
        let ip_offset = (context.ip - FIXTURE_ADDRESS) as u32;
        let result = entries.unwind_frame_with_image(&mut context, get_fixture(), ip_offset);
        assert_eq!(result, Some(ra), "mismatched return address");
        assert_eq!(context.changes, changes, "mismatched register changes");
    }

    fn assert_fixture_frames(mut context: FrameContext, return_addrs: &[u64]) {
        let file = File::parse(get_fixture()).unwrap();
        let pdata_section = file.section_by_name(".pdata").unwrap();
        let entries = FunctionTableEntries::parse(pdata_section.data().unwrap());

        let mut ip = context.ip;
        for ra in return_addrs {
            let ip_offset = (ip - FIXTURE_ADDRESS) as u32;
            let result = entries.unwind_frame_with_image(&mut context, get_fixture(), ip_offset);
            assert_eq!(result, Some(*ra), "mismatched return address");
            ip = ra - 1;
        }
    }

    #[test]
    fn unwind_frame_1() {
        let context = windbg_frame_context! {
            rax=0x000000000000001e rbx=0x0000020891345770 rcx=0x000000000000000a
            rdx=0x0000000000000001 rsi=0x0000000000000001 rdi=0x0000020891349e40
            rip=0x00007ff725bf1084 rsp=0x00000070c7d3fb38 rbp=0x0000000000000000
             r8=0x0000020891349e40  r9=0x0000000000000630 r10=0x0000000000000630
            r11=0x00000070c7d3f7f0 r12=0x0000000000000000 r13=0x0000000000000000
            r14=0x0000000000000000 r15=0x0000000000000000
            stack_base = 0x70c7d3fba0
            stack = "21 10 BF 25 F7 7F 00 00
                01 00 00 00 00 00 00 00 03 00 00 00 00 00 00 00
                02 00 00 00 00 00 00 00 39 72 C0 25 F7 7F 00 00
                70 57 34 91 08 02 00 00 40 9E 34 91 08 02 00 00
                00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
                00 00 00 00 00 00 00 00 90 6F C0 25 F7 7F 00 00
                00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
        };
        assert_fixture_unwind(
            context,
            0x7ff725bf1021,
            RegisterChanges::new().set(Register::RSP, 0x70c7d3fb38 + 8),
        );
    }

    #[test]
    fn unwind_frame_2() {
        let context = windbg_frame_context! {
            rax=0x0000000000000026 rbx=0x000000000000006b rcx=0x0000000000000002
            rdx=0x0000000000000026 rsi=0x0000000000000001 rdi=0x000000000000001f
            rip=0x00007ff725bf10b6 rsp=0x00000070c7d3fb38 rbp=0x0000000000000000
             r8=0x0000000000000002  r9=0x0000000000000000 r10=0x000000000000001f
            r11=0x00000070c7d3f7f0 r12=0x0000000000000000 r13=0x0000000000000000
            r14=0x0000000000000026 r15=0x0000000000000003
            stack_base = 0x70c7d3fba0
            stack = "4F 10 BF 25 F7 7F 00 00
                01 00 00 00 00 00 00 00 03 00 00 00 00 00 00 00
                02 00 00 00 00 00 00 00 39 72 C0 25 F7 7F 00 00
                70 57 34 91 08 02 00 00 40 9E 34 91 08 02 00 00
                00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
                00 00 00 00 00 00 00 00 90 6F C0 25 F7 7F 00 00
                00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
        };
        assert_fixture_unwind(
            context,
            0x7ff725bf104f,
            RegisterChanges::new().set(Register::RSP, 0x70c7d3fb38 + 8),
        );
    }

    #[test]
    fn unwind_frame_in_prolog() {
        let context = windbg_frame_context! {
            rax=0x00007ffa222c07a8 rbx=0x0000020891345770 rcx=0x0000000000000001
            rdx=0x0000020891345770 rsi=0x0000000000000000 rdi=0x0000020891349e40
            rip=0x00007ff725bf1005 rsp=0x00000070c7d3fb70 rbp=0x0000000000000000
             r8=0x0000020891349e40  r9=0x0000000000000630 r10=0x0000000000000630
            r11=0x00000070c7d3f7f0 r12=0x0000000000000000 r13=0x0000000000000000
            r14=0x0000000000000000 r15=0x0000000000000000
            stack_base = 0x70c7d3fba0
            stack = "
                00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
                00 00 00 00 00 00 00 00 90 6F C0 25 F7 7F 00 00
                00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
        };
        assert_fixture_unwind(
            context,
            0x7ff725c06f90,
            RegisterChanges::new()
                .set(Register::RSI, 0)
                .set(Register::R14, 0)
                .set(Register::R15, 0)
                .set(Register::RSP, 0x70c7d3fb70 + 8 * 4),
        );
    }

    #[test]
    fn unwind_frame_in_epilog_beginning() {
        let context = windbg_frame_context! {
            rax=0xf47e69ea626ba5db rbx=0x82bcd1c6ad5f6936 rcx=0x82bcd1c6ad5f6936
            rdx=0x0000000000000001 rsi=0x0000000000000001 rdi=0x000000000000001f
            rip=0x00007ff725bf1068 rsp=0x00000070c7d3fb40 rbp=0x0000000000000000
             r8=0x82bcd1c6ad5f6936  r9=0x0000000000000003 r10=0x0000000000000045
            r11=0x00000070c7d3f7f0 r12=0x0000000000000000 r13=0x0000000000000000
            r14=0x0000000000000026 r15=0x0000000000000064
            stack_base = 0x70c7d3fba0
            stack = "
            01 00 00 00 00 00 00 00 03 00 00 00 00 00 00 00
            02 00 00 00 00 00 00 00 39 72 C0 25 F7 7F 00 00
            70 57 34 91 08 02 00 00 40 9E 34 91 08 02 00 00
            00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
            00 00 00 00 00 00 00 00 90 6F C0 25 F7 7F 00 00
            00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
        };
        assert_fixture_unwind(
            context,
            0x7ff725c06f90,
            RegisterChanges::new()
                .set(Register::RBX, 0x20891345770)
                .set(Register::RDI, 0x20891349e40)
                .set(Register::RSI, 0)
                .set(Register::R14, 0)
                .set(Register::R15, 0)
                .set(Register::RSP, 0x70c7d3fb40 + 0x20 + 8 * 6),
        );
    }

    #[test]
    fn unwind_frame_in_epilog_middle() {
        let context = windbg_frame_context! {
            rax=0xf47e69ea626ba5db rbx=0x0000020891345770 rcx=0x82bcd1c6ad5f6936
            rdx=0x0000000000000001 rsi=0x0000000000000000 rdi=0x0000020891349e40
            rip=0x00007ff725bf106f rsp=0x00000070c7d3fb78 rbp=0x0000000000000000
             r8=0x82bcd1c6ad5f6936  r9=0x0000000000000003 r10=0x0000000000000045
            r11=0x00000070c7d3f7f0 r12=0x0000000000000000 r13=0x0000000000000000
            r14=0x0000000000000026 r15=0x0000000000000064
            stack_base = 0x70c7d3fba0
            stack = "00 00 00 00 00 00 00 00
                00 00 00 00 00 00 00 00 90 6F C0 25 F7 7F 00 00
                00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
        };
        assert_fixture_unwind(
            context,
            0x7ff725c06f90,
            RegisterChanges::new()
                .set(Register::R14, 0)
                .set(Register::R15, 0)
                .set(Register::RSP, 0x70c7d3fb78 + 8 * 3),
        );
    }

    #[test]
    fn multiple_frames() {
        let context = windbg_frame_context! {
            rax=0x0000000000000026 rbx=0x000000000000006b rcx=0x0000000000000002
            rdx=0x0000000000000026 rsi=0x0000000000000001 rdi=0x000000000000001f
            rip=0x00007ff725bf10b6 rsp=0x00000070c7d3fb38 rbp=0x0000000000000000
             r8=0x0000000000000002  r9=0x0000000000000000 r10=0x000000000000001f
            r11=0x00000070c7d3f7f0 r12=0x0000000000000000 r13=0x0000000000000000
            r14=0x0000000000000026 r15=0x0000000000000003
            stack_base = 0x70c7d3fba0
            stack = "4F 10 BF 25 F7 7F 00 00
                01 00 00 00 00 00 00 00 03 00 00 00 00 00 00 00
                02 00 00 00 00 00 00 00 39 72 C0 25 F7 7F 00 00
                70 57 34 91 08 02 00 00 40 9E 34 91 08 02 00 00
                00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
                00 00 00 00 00 00 00 00 90 6F C0 25 F7 7F 00 00
                00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
        };
        assert_fixture_frames(context, &[0x7ff725bf104f, 0x7ff725c06f90]);
    }
}
