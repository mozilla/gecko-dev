use core::marker::PhantomData;

use crate::dwarf::DwarfUnwinderError;
use crate::{arch::Arch, unwind_rule::UnwindRule};
use macho_unwind_info::UnwindInfo;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CompactUnwindInfoUnwinderError {
    BadFormat(macho_unwind_info::Error),
    AddressOutsideRange(u32),
    CallerCannotBeFrameless,
    FunctionHasNoInfo,
    BpOffsetDoesNotFit,
    BadOpcodeKind(u8),
    BadDwarfUnwinding(DwarfUnwinderError),
    NoTextBytesToLookUpIndirectStackOffset,
    IndirectStackOffsetOutOfBounds,
    StackAdjustOverflow,
    StackSizeDoesNotFit,
    StubFunctionCannotBeCaller,
    InvalidFrameless,
}

impl core::fmt::Display for CompactUnwindInfoUnwinderError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Self::BadFormat(err) => write!(f, "Bad __unwind_info format: {err}"),
            Self::AddressOutsideRange(addr) => write!(f, "Address 0x{addr:x} outside of the range covered by __unwind_info"),
            Self::CallerCannotBeFrameless => write!(f, "Encountered a non-leaf function which was marked as frameless."),
            Self::FunctionHasNoInfo => write!(f, "No unwind info (null opcode) for this function in __unwind_info"),
            Self::BpOffsetDoesNotFit => write!(f, "rbp offset from the stack pointer divided by 8 does not fit into i16"),
            Self::BadOpcodeKind(kind) => write!(f, "Unrecognized __unwind_info opcode kind {kind}"),
            Self::BadDwarfUnwinding(err) => write!(f, "DWARF unwinding failed: {err}"),
            Self::NoTextBytesToLookUpIndirectStackOffset => write!(f, "Don't have the function bytes to look up the offset for frameless function with indirect stack offset"),
            Self::IndirectStackOffsetOutOfBounds => write!(f, "Stack offset not found inside the bounds of the text bytes"),
            Self::StackAdjustOverflow => write!(f, "Stack adjust addition overflowed"),
            Self::StackSizeDoesNotFit => write!(f, "Stack size does not fit into the rule representation"),
            Self::StubFunctionCannotBeCaller => write!(f, "A caller had its address in the __stubs section"),
            Self::InvalidFrameless => write!(f, "Encountered invalid unwind entry"),
        }
    }
}

impl From<macho_unwind_info::Error> for CompactUnwindInfoUnwinderError {
    fn from(e: macho_unwind_info::Error) -> Self {
        Self::BadFormat(e)
    }
}

impl From<DwarfUnwinderError> for CompactUnwindInfoUnwinderError {
    fn from(e: DwarfUnwinderError) -> Self {
        Self::BadDwarfUnwinding(e)
    }
}

#[cfg(feature = "std")]
impl std::error::Error for CompactUnwindInfoUnwinderError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            Self::BadFormat(e) => Some(e),
            Self::BadDwarfUnwinding(e) => Some(e),
            _ => None,
        }
    }
}

#[derive(Clone, Debug)]
pub enum CuiUnwindResult<R: UnwindRule> {
    ExecRule(R),
    NeedDwarf(u32),
}

pub trait CompactUnwindInfoUnwinding: Arch {
    fn unwind_frame(
        function: macho_unwind_info::Function,
        is_first_frame: bool,
        address_offset_within_function: usize,
        function_bytes: Option<&[u8]>,
    ) -> Result<CuiUnwindResult<Self::UnwindRule>, CompactUnwindInfoUnwinderError>;

    fn rule_for_stub_helper(
        offset: u32,
    ) -> Result<CuiUnwindResult<Self::UnwindRule>, CompactUnwindInfoUnwinderError>;
}

#[derive(Clone, Copy)]
pub struct TextBytes<'a> {
    offset_from_base_address: u32,
    bytes: &'a [u8],
}

impl<'a> TextBytes<'a> {
    pub fn new(offset_from_base_address: u32, bytes: &'a [u8]) -> Self {
        Self {
            offset_from_base_address,
            bytes,
        }
    }
}

pub struct CompactUnwindInfoUnwinder<'a, A: CompactUnwindInfoUnwinding> {
    unwind_info_data: &'a [u8],
    text_bytes: Option<TextBytes<'a>>,
    stubs_range: (u32, u32),
    stub_helper_range: (u32, u32),
    _arch: PhantomData<A>,
}

impl<'a, A: CompactUnwindInfoUnwinding> CompactUnwindInfoUnwinder<'a, A> {
    pub fn new(
        unwind_info_data: &'a [u8],
        text_bytes: Option<TextBytes<'a>>,
        stubs_range: (u32, u32),
        stub_helper_range: (u32, u32),
    ) -> Self {
        Self {
            unwind_info_data,
            text_bytes,
            stubs_range,
            stub_helper_range,
            _arch: PhantomData,
        }
    }

    pub fn function_for_address(
        &self,
        address: u32,
    ) -> Result<macho_unwind_info::Function, CompactUnwindInfoUnwinderError> {
        let unwind_info = UnwindInfo::parse(self.unwind_info_data)
            .map_err(CompactUnwindInfoUnwinderError::BadFormat)?;
        let function = unwind_info
            .lookup(address)
            .map_err(CompactUnwindInfoUnwinderError::BadFormat)?;
        function.ok_or(CompactUnwindInfoUnwinderError::AddressOutsideRange(address))
    }

    pub fn unwind_frame(
        &mut self,
        rel_lookup_address: u32,
        is_first_frame: bool,
    ) -> Result<CuiUnwindResult<A::UnwindRule>, CompactUnwindInfoUnwinderError> {
        // Exclude __stubs and __stub_helper sections. The __unwind_info does not describe those
        // sections. These sections need to be manually excluded because the addresses in
        // __unwind_info can be both before and after the stubs/stub_helper sections, if there is
        // both a __text and a text_env section.
        if self.stubs_range.0 <= rel_lookup_address && rel_lookup_address < self.stubs_range.1 {
            if !is_first_frame {
                return Err(CompactUnwindInfoUnwinderError::StubFunctionCannotBeCaller);
            }
            // All stub functions are frameless.
            return Ok(CuiUnwindResult::ExecRule(
                A::UnwindRule::rule_for_stub_functions(),
            ));
        }
        if self.stub_helper_range.0 <= rel_lookup_address
            && rel_lookup_address < self.stub_helper_range.1
        {
            if !is_first_frame {
                return Err(CompactUnwindInfoUnwinderError::StubFunctionCannotBeCaller);
            }
            let lookup_address_relative_to_section = rel_lookup_address - self.stub_helper_range.0;
            return <A as CompactUnwindInfoUnwinding>::rule_for_stub_helper(
                lookup_address_relative_to_section,
            );
        }
        let function = match self.function_for_address(rel_lookup_address) {
            Ok(f) => f,
            Err(CompactUnwindInfoUnwinderError::AddressOutsideRange(_)) if is_first_frame => {
                // pc is falling into this module's address range, but it's not covered by __unwind_info.
                // This could mean that we're inside a stub function, in the __stubs section.
                // All stub functions are frameless.
                // TODO: Obtain the actual __stubs address range and do better checking here.
                return Ok(CuiUnwindResult::ExecRule(
                    A::UnwindRule::rule_for_stub_functions(),
                ));
            }
            Err(err) => return Err(err),
        };
        if is_first_frame && rel_lookup_address == function.start_address {
            return Ok(CuiUnwindResult::ExecRule(
                A::UnwindRule::rule_for_function_start(),
            ));
        }
        let address_offset_within_function =
            usize::try_from(rel_lookup_address - function.start_address).unwrap();
        let function_bytes = self.text_bytes.and_then(|text_bytes| {
            let TextBytes {
                offset_from_base_address,
                bytes,
            } = text_bytes;
            let function_start_relative_to_text = function
                .start_address
                .checked_sub(offset_from_base_address)?
                as usize;
            let function_end_relative_to_text =
                function.end_address.checked_sub(offset_from_base_address)? as usize;
            bytes.get(function_start_relative_to_text..function_end_relative_to_text)
        });
        <A as CompactUnwindInfoUnwinding>::unwind_frame(
            function,
            is_first_frame,
            address_offset_within_function,
            function_bytes,
        )
    }
}
