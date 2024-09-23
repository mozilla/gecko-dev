use crate::dwarf::DwarfUnwinderError;
#[cfg(feature = "macho")]
use crate::macho::CompactUnwindInfoUnwinderError;
#[cfg(feature = "pe")]
use crate::pe::PeUnwinderError;

/// The error type used in this crate.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Error {
    CouldNotReadStack(u64),
    FramepointerUnwindingMovedBackwards,
    DidNotAdvance,
    IntegerOverflow,
    ReturnAddressIsNull,
}

impl core::fmt::Display for Error {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Self::CouldNotReadStack(addr) => write!(f, "Could not read stack memory at 0x{addr:x}"),
            Self::FramepointerUnwindingMovedBackwards => {
                write!(f, "Frame pointer unwinding moved backwards")
            }
            Self::DidNotAdvance => write!(
                f,
                "Neither the code address nor the stack pointer changed, would loop"
            ),
            Self::IntegerOverflow => write!(f, "Unwinding caused integer overflow"),
            Self::ReturnAddressIsNull => write!(f, "Return address is null"),
        }
    }
}

#[cfg(feature = "std")]
impl std::error::Error for Error {}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum UnwinderError {
    #[cfg(feature = "macho")]
    CompactUnwindInfo(CompactUnwindInfoUnwinderError),
    Dwarf(DwarfUnwinderError),
    #[cfg(feature = "pe")]
    Pe(PeUnwinderError),
    #[cfg(feature = "macho")]
    NoDwarfData,
    NoModuleUnwindData,
    EhFrameHdrCouldNotFindAddress,
    DwarfCfiIndexCouldNotFindAddress,
}

impl core::fmt::Display for UnwinderError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            #[cfg(feature = "macho")]
            Self::CompactUnwindInfo(err) => {
                write!(f, "Compact Unwind Info unwinding failed: {err}")
            }
            Self::Dwarf(err) => write!(f, "DWARF unwinding failed: {err}"),
            #[cfg(feature = "pe")]
            Self::Pe(err) => write!(f, "PE unwinding failed: {err}"),
            #[cfg(feature = "macho")]
            Self::NoDwarfData => write!(
                f,
                "__unwind_info referred to DWARF FDE but we do not have __eh_frame data"
            ),
            Self::NoModuleUnwindData => {
                write!(f, "No unwind data for the module containing the address")
            }
            Self::EhFrameHdrCouldNotFindAddress => write!(
                f,
                ".eh_frame_hdr was not successful in looking up the address in the table"
            ),
            Self::DwarfCfiIndexCouldNotFindAddress => write!(
                f,
                "Failed to look up the address in the DwarfCfiIndex search table"
            ),
        }
    }
}

impl From<DwarfUnwinderError> for UnwinderError {
    fn from(e: DwarfUnwinderError) -> Self {
        Self::Dwarf(e)
    }
}

#[cfg(feature = "pe")]
impl From<PeUnwinderError> for UnwinderError {
    fn from(e: PeUnwinderError) -> Self {
        Self::Pe(e)
    }
}

#[cfg(feature = "macho")]
impl From<CompactUnwindInfoUnwinderError> for UnwinderError {
    fn from(e: CompactUnwindInfoUnwinderError) -> Self {
        match e {
            CompactUnwindInfoUnwinderError::BadDwarfUnwinding(e) => UnwinderError::Dwarf(e),
            e => UnwinderError::CompactUnwindInfo(e),
        }
    }
}

#[cfg(feature = "std")]
impl std::error::Error for UnwinderError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            #[cfg(feature = "macho")]
            Self::CompactUnwindInfo(e) => Some(e),
            Self::Dwarf(e) => Some(e),
            #[cfg(feature = "pe")]
            Self::Pe(e) => Some(e),
            _ => None,
        }
    }
}
