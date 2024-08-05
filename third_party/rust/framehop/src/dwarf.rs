use core::marker::PhantomData;

use alloc::vec::Vec;
use gimli::{
    CfaRule, CieOrFde, DebugFrame, EhFrame, EhFrameHdr, Encoding, EndianSlice, Evaluation,
    EvaluationResult, EvaluationStorage, Expression, LittleEndian, Location, ParsedEhFrameHdr,
    Reader, ReaderOffset, Register, RegisterRule, UnwindContext, UnwindContextStorage,
    UnwindOffset, UnwindSection, UnwindTableRow, Value,
};

pub(crate) use gimli::BaseAddresses;

use crate::{arch::Arch, unwind_result::UnwindResult, ModuleSectionInfo};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DwarfUnwinderError {
    FdeFromOffsetFailed(gimli::Error),
    UnwindInfoForAddressFailed(gimli::Error),
    StackPointerMovedBackwards,
    DidNotAdvance,
    CouldNotRecoverCfa,
    CouldNotRecoverReturnAddress,
    CouldNotRecoverFramePointer,
}

impl core::fmt::Display for DwarfUnwinderError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Self::FdeFromOffsetFailed(err) => {
                write!(f, "Could not get the FDE for the supplied offset: {err}")
            }
            Self::UnwindInfoForAddressFailed(err) => write!(
                f,
                "Could not find DWARF unwind info for the requested address: {err}"
            ),
            Self::StackPointerMovedBackwards => write!(f, "Stack pointer moved backwards"),
            Self::DidNotAdvance => write!(f, "Did not advance"),
            Self::CouldNotRecoverCfa => write!(f, "Could not recover the CFA"),
            Self::CouldNotRecoverReturnAddress => write!(f, "Could not recover the return address"),
            Self::CouldNotRecoverFramePointer => write!(f, "Could not recover the frame pointer"),
        }
    }
}

#[cfg(feature = "std")]
impl std::error::Error for DwarfUnwinderError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            Self::FdeFromOffsetFailed(e) => Some(e),
            Self::UnwindInfoForAddressFailed(e) => Some(e),
            _ => None,
        }
    }
}

#[derive(Clone, Debug)]
pub enum ConversionError {
    CfaIsExpression,
    CfaIsOffsetFromUnknownRegister,
    ReturnAddressRuleWithUnexpectedOffset,
    ReturnAddressRuleWasWeird,
    SpOffsetDoesNotFit,
    RegisterNotStoredRelativeToCfa,
    RestoringFpButNotLr,
    LrStorageOffsetDoesNotFit,
    FpStorageOffsetDoesNotFit,
    SpOffsetFromFpDoesNotFit,
    FramePointerRuleDoesNotRestoreLr,
    FramePointerRuleDoesNotRestoreFp,
    FramePointerRuleDoesNotRestoreBp,
    FramePointerRuleHasStrangeBpOffset,
}

pub trait DwarfUnwinding: Arch {
    fn unwind_frame<F, R, UCS, ES>(
        section: &impl UnwindSection<R>,
        unwind_info: &UnwindTableRow<R::Offset, UCS>,
        encoding: Encoding,
        regs: &mut Self::UnwindRegs,
        is_first_frame: bool,
        read_stack: &mut F,
    ) -> Result<UnwindResult<Self::UnwindRule>, DwarfUnwinderError>
    where
        F: FnMut(u64) -> Result<u64, ()>,
        R: Reader,
        UCS: UnwindContextStorage<R::Offset>,
        ES: EvaluationStorage<R>;

    fn rule_if_uncovered_by_fde() -> Self::UnwindRule;
}

pub enum UnwindSectionType {
    EhFrame,
    DebugFrame,
}

pub struct DwarfUnwinder<'a, R, A, UCS>
where
    R: Reader,
    A: DwarfUnwinding,
    UCS: UnwindContextStorage<R::Offset>,
{
    unwind_section_data: R,
    unwind_section_type: UnwindSectionType,
    eh_frame_hdr: Option<ParsedEhFrameHdr<EndianSlice<'a, R::Endian>>>,
    unwind_context: &'a mut UnwindContext<R::Offset, UCS>,
    base_svma: u64,
    bases: BaseAddresses,
    _arch: PhantomData<A>,
}

impl<'a, R, A, UCS> DwarfUnwinder<'a, R, A, UCS>
where
    R: Reader,
    A: DwarfUnwinding,
    UCS: UnwindContextStorage<R::Offset>,
{
    pub fn new(
        unwind_section_data: R,
        unwind_section_type: UnwindSectionType,
        eh_frame_hdr_data: Option<&'a [u8]>,
        unwind_context: &'a mut UnwindContext<R::Offset, UCS>,
        bases: BaseAddresses,
        base_svma: u64,
    ) -> Self {
        let eh_frame_hdr = match eh_frame_hdr_data {
            Some(eh_frame_hdr_data) => {
                let hdr = EhFrameHdr::new(eh_frame_hdr_data, unwind_section_data.endian());
                match hdr.parse(&bases, 8) {
                    Ok(hdr) => Some(hdr),
                    Err(_) => None,
                }
            }
            None => None,
        };
        Self {
            unwind_section_data,
            unwind_section_type,
            eh_frame_hdr,
            unwind_context,
            bases,
            base_svma,
            _arch: PhantomData,
        }
    }

    pub fn get_fde_offset_for_relative_address(&self, rel_lookup_address: u32) -> Option<u32> {
        let lookup_svma = self.base_svma + rel_lookup_address as u64;
        let eh_frame_hdr = self.eh_frame_hdr.as_ref()?;
        let table = eh_frame_hdr.table()?;
        let fde_ptr = table.lookup(lookup_svma, &self.bases).ok()?;
        let fde_offset = table.pointer_to_offset(fde_ptr).ok()?;
        fde_offset.0.into_u64().try_into().ok()
    }

    pub fn unwind_frame_with_fde<F, ES>(
        &mut self,
        regs: &mut A::UnwindRegs,
        is_first_frame: bool,
        rel_lookup_address: u32,
        fde_offset: u32,
        read_stack: &mut F,
    ) -> Result<UnwindResult<A::UnwindRule>, DwarfUnwinderError>
    where
        F: FnMut(u64) -> Result<u64, ()>,
        ES: EvaluationStorage<R>,
    {
        let lookup_svma = self.base_svma + rel_lookup_address as u64;
        let unwind_section_data = self.unwind_section_data.clone();
        match self.unwind_section_type {
            UnwindSectionType::EhFrame => {
                let mut eh_frame = EhFrame::from(unwind_section_data);
                eh_frame.set_address_size(8);
                let unwind_info = self.unwind_info_for_fde(&eh_frame, lookup_svma, fde_offset);
                if let Err(DwarfUnwinderError::UnwindInfoForAddressFailed(_)) = unwind_info {
                    return Ok(UnwindResult::ExecRule(A::rule_if_uncovered_by_fde()));
                }
                let (unwind_info, encoding) = unwind_info?;
                A::unwind_frame::<F, R, UCS, ES>(
                    &eh_frame,
                    unwind_info,
                    encoding,
                    regs,
                    is_first_frame,
                    read_stack,
                )
            }
            UnwindSectionType::DebugFrame => {
                let mut debug_frame = DebugFrame::from(unwind_section_data);
                debug_frame.set_address_size(8);
                let unwind_info = self.unwind_info_for_fde(&debug_frame, lookup_svma, fde_offset);
                if let Err(DwarfUnwinderError::UnwindInfoForAddressFailed(_)) = unwind_info {
                    return Ok(UnwindResult::ExecRule(A::rule_if_uncovered_by_fde()));
                }
                let (unwind_info, encoding) = unwind_info?;
                A::unwind_frame::<F, R, UCS, ES>(
                    &debug_frame,
                    unwind_info,
                    encoding,
                    regs,
                    is_first_frame,
                    read_stack,
                )
            }
        }
    }

    fn unwind_info_for_fde<US: UnwindSection<R>>(
        &mut self,
        unwind_section: &US,
        lookup_svma: u64,
        fde_offset: u32,
    ) -> Result<(&UnwindTableRow<R::Offset, UCS>, Encoding), DwarfUnwinderError> {
        let fde = unwind_section.fde_from_offset(
            &self.bases,
            US::Offset::from(R::Offset::from_u32(fde_offset)),
            US::cie_from_offset,
        );
        let fde = fde.map_err(DwarfUnwinderError::FdeFromOffsetFailed)?;
        let encoding = fde.cie().encoding();
        let unwind_info: &UnwindTableRow<_, _> = fde
            .unwind_info_for_address(
                unwind_section,
                &self.bases,
                self.unwind_context,
                lookup_svma,
            )
            .map_err(DwarfUnwinderError::UnwindInfoForAddressFailed)?;
        Ok((unwind_info, encoding))
    }
}

pub(crate) fn base_addresses_for_sections<D>(
    section_info: &mut impl ModuleSectionInfo<D>,
) -> BaseAddresses {
    let mut start_addr = |names: &[&[u8]]| -> u64 {
        names
            .iter()
            .find_map(|name| section_info.section_svma_range(name))
            .map(|r| r.start)
            .unwrap_or_default()
    };
    BaseAddresses::default()
        .set_eh_frame(start_addr(&[b"__eh_frame", b".eh_frame"]))
        .set_eh_frame_hdr(start_addr(&[b"__eh_frame_hdr", b".eh_frame_hdr"]))
        .set_text(start_addr(&[b"__text", b".text"]))
        .set_got(start_addr(&[b"__got", b".got"]))
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DwarfCfiIndexError {
    Gimli(gimli::Error),
    CouldNotSubtractBaseAddress,
    RelativeAddressTooBig,
    FdeOffsetTooBig,
}

impl core::fmt::Display for DwarfCfiIndexError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Self::Gimli(e) => write!(f, "EhFrame processing failed: {e}"),
            Self::CouldNotSubtractBaseAddress => {
                write!(f, "Could not subtract base address to create relative pc")
            }
            Self::RelativeAddressTooBig => write!(f, "Relative address did not fit into u32"),
            Self::FdeOffsetTooBig => write!(f, "FDE offset did not fit into u32"),
        }
    }
}

impl From<gimli::Error> for DwarfCfiIndexError {
    fn from(e: gimli::Error) -> Self {
        Self::Gimli(e)
    }
}

#[cfg(feature = "std")]
impl std::error::Error for DwarfCfiIndexError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            Self::Gimli(e) => Some(e),
            _ => None,
        }
    }
}

/// A binary search table for eh_frame FDEs. We generate this whenever a module
/// without eh_frame_hdr is added.
pub struct DwarfCfiIndex {
    /// Contains the initial address for every FDE, relative to the base address.
    /// This vector is sorted so that it can be used for binary search.
    /// It has the same length as `fde_offsets`.
    sorted_fde_pc_starts: Vec<u32>,
    /// Contains the FDE offset for every FDE. The FDE at offset `fde_offsets[i]`
    /// has a PC range which starts at `sorted_fde_pc_starts[i]`.
    fde_offsets: Vec<u32>,
}

impl DwarfCfiIndex {
    pub fn try_new<R, US>(
        unwind_section: US,
        bases: BaseAddresses,
        base_svma: u64,
    ) -> Result<Self, DwarfCfiIndexError>
    where
        R: Reader,
        R::Offset: TryInto<u32>,
        US: UnwindSection<R>,
    {
        let mut fde_pc_and_offset = Vec::new();

        let mut cur_cie = None;
        let mut entries_iter = unwind_section.entries(&bases);
        while let Some(entry) = entries_iter.next()? {
            let fde = match entry {
                CieOrFde::Cie(cie) => {
                    cur_cie = Some(cie);
                    continue;
                }
                CieOrFde::Fde(partial_fde) => {
                    partial_fde.parse(|unwind_section, bases, cie_offset| {
                        if let Some(cie) = &cur_cie {
                            if cie.offset()
                                == <US::Offset as UnwindOffset<R::Offset>>::into(cie_offset)
                            {
                                return Ok(cie.clone());
                            }
                        }
                        let cie = unwind_section.cie_from_offset(bases, cie_offset);
                        if let Ok(cie) = &cie {
                            cur_cie = Some(cie.clone());
                        }
                        cie
                    })?
                }
            };
            let pc = fde.initial_address();
            let relative_pc = pc
                .checked_sub(base_svma)
                .ok_or(DwarfCfiIndexError::CouldNotSubtractBaseAddress)?;
            let relative_pc = u32::try_from(relative_pc)
                .map_err(|_| DwarfCfiIndexError::RelativeAddressTooBig)?;
            let fde_offset = <R::Offset as TryInto<u32>>::try_into(fde.offset())
                .map_err(|_| DwarfCfiIndexError::FdeOffsetTooBig)?;
            fde_pc_and_offset.push((relative_pc, fde_offset));
        }
        fde_pc_and_offset.sort_by_key(|(pc, _)| *pc);
        let sorted_fde_pc_starts = fde_pc_and_offset.iter().map(|(pc, _)| *pc).collect();
        let fde_offsets = fde_pc_and_offset.into_iter().map(|(_, fde)| fde).collect();
        Ok(Self {
            sorted_fde_pc_starts,
            fde_offsets,
        })
    }

    pub fn try_new_eh_frame<D>(
        eh_frame_data: &[u8],
        section_info: &mut impl ModuleSectionInfo<D>,
    ) -> Result<Self, DwarfCfiIndexError> {
        let bases = base_addresses_for_sections(section_info);
        let mut eh_frame = EhFrame::from(EndianSlice::new(eh_frame_data, LittleEndian));
        eh_frame.set_address_size(8);

        Self::try_new(eh_frame, bases, section_info.base_svma())
    }

    pub fn try_new_debug_frame<D>(
        debug_frame_data: &[u8],
        section_info: &mut impl ModuleSectionInfo<D>,
    ) -> Result<Self, DwarfCfiIndexError> {
        let bases = base_addresses_for_sections(section_info);
        let mut debug_frame = DebugFrame::from(EndianSlice::new(debug_frame_data, LittleEndian));
        debug_frame.set_address_size(8);

        Self::try_new(debug_frame, bases, section_info.base_svma())
    }

    pub fn fde_offset_for_relative_address(&self, rel_lookup_address: u32) -> Option<u32> {
        let i = match self.sorted_fde_pc_starts.binary_search(&rel_lookup_address) {
            Err(0) => return None,
            Ok(i) => i,
            Err(i) => i - 1,
        };
        Some(self.fde_offsets[i])
    }
}

pub trait DwarfUnwindRegs {
    fn get(&self, register: Register) -> Option<u64>;
}

pub fn eval_cfa_rule<R: Reader, UR: DwarfUnwindRegs, S: EvaluationStorage<R>>(
    section: &impl UnwindSection<R>,
    rule: &CfaRule<R::Offset>,
    encoding: Encoding,
    regs: &UR,
) -> Option<u64> {
    match rule {
        CfaRule::RegisterAndOffset { register, offset } => {
            let val = regs.get(*register)?;
            u64::try_from(i64::try_from(val).ok()?.checked_add(*offset)?).ok()
        }
        CfaRule::Expression(expr) => {
            let expr = expr.get(section).ok()?;
            eval_expr::<R, UR, S>(expr, encoding, regs)
        }
    }
}

fn eval_expr<R: Reader, UR: DwarfUnwindRegs, S: EvaluationStorage<R>>(
    expr: Expression<R>,
    encoding: Encoding,
    regs: &UR,
) -> Option<u64> {
    let mut eval = Evaluation::<R, S>::new_in(expr.0, encoding);
    let mut result = eval.evaluate().ok()?;
    loop {
        match result {
            EvaluationResult::Complete => break,
            EvaluationResult::RequiresRegister { register, .. } => {
                let value = regs.get(register)?;
                result = eval.resume_with_register(Value::Generic(value as _)).ok()?;
            }
            _ => return None,
        }
    }
    let x = &eval.as_result().last()?.location;
    if let Location::Address { address } = x {
        Some(*address)
    } else {
        None
    }
}

pub fn eval_register_rule<R, F, UR, S>(
    section: &impl UnwindSection<R>,
    rule: RegisterRule<R::Offset>,
    cfa: u64,
    encoding: Encoding,
    val: u64,
    regs: &UR,
    read_stack: &mut F,
) -> Option<u64>
where
    R: Reader,
    F: FnMut(u64) -> Result<u64, ()>,
    UR: DwarfUnwindRegs,
    S: EvaluationStorage<R>,
{
    match rule {
        RegisterRule::Undefined => None,
        RegisterRule::SameValue => Some(val),
        RegisterRule::Offset(offset) => {
            let cfa_plus_offset =
                u64::try_from(i64::try_from(cfa).ok()?.checked_add(offset)?).ok()?;
            read_stack(cfa_plus_offset).ok()
        }
        RegisterRule::ValOffset(offset) => {
            u64::try_from(i64::try_from(cfa).ok()?.checked_add(offset)?).ok()
        }
        RegisterRule::Register(register) => regs.get(register),
        RegisterRule::Expression(expr) => {
            let expr = expr.get(section).ok()?;
            let val = eval_expr::<R, UR, S>(expr, encoding, regs)?;
            read_stack(val).ok()
        }
        RegisterRule::ValExpression(expr) => {
            let expr = expr.get(section).ok()?;
            eval_expr::<R, UR, S>(expr, encoding, regs)
        }
        RegisterRule::Architectural => {
            // Unimplemented
            // TODO: Find out what the architectural rules for x86_64 and for aarch64 are, if any.
            None
        }
        _ => None,
    }
}
