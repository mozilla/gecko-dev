use crate::{arch::Arch, unwind_result::UnwindResult};
use core::ops::Range;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PeUnwinderError {
    MissingUnwindInfoData(u32),
    MissingInstructionData(u32),
    MissingStackData(Option<u64>),
    UnwindInfoParseError,
    Aarch64Unsupported,
}

impl core::fmt::Display for PeUnwinderError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Self::MissingUnwindInfoData(rva) => {
                write!(f, "failed to read unwind info memory at RVA {rva:x}")
            }
            Self::MissingInstructionData(rva) => {
                write!(f, "failed to read instruction memory at RVA {rva:x}")
            }
            Self::MissingStackData(addr) => {
                write!(f, "failed to read stack")?;
                if let Some(addr) = addr {
                    write!(f, " at address {addr:x}")?;
                }
                Ok(())
            }
            Self::UnwindInfoParseError => write!(f, "failed to parse UnwindInfo"),
            Self::Aarch64Unsupported => write!(f, "AArch64 is not yet supported"),
        }
    }
}

#[cfg(feature = "std")]
impl std::error::Error for PeUnwinderError {}

/// Data and the related RVA range within the binary.
///
/// This is only used by PE unwinding.
///
/// Type arguments:
///  - `D`: The type for unwind section data. This allows carrying owned data on the
///    module, e.g. `Vec<u8>`. But it could also be a wrapper around mapped memory from
///    a file or a different process, for example. It just needs to provide a slice of
///    bytes via its `Deref` implementation.
pub struct DataAtRvaRange<D> {
    pub data: D,
    pub rva_range: Range<u32>,
}

pub struct PeSections<'a, D> {
    pub pdata: &'a D,
    pub rdata: Option<&'a DataAtRvaRange<D>>,
    pub xdata: Option<&'a DataAtRvaRange<D>>,
    pub text: Option<&'a DataAtRvaRange<D>>,
}

impl<'a, D> PeSections<'a, D>
where
    D: core::ops::Deref<Target = [u8]>,
{
    pub fn unwind_info_memory_at_rva(&self, rva: u32) -> Result<&'a [u8], PeUnwinderError> {
        [&self.rdata, &self.xdata]
            .into_iter()
            .find_map(|o| o.and_then(|m| memory_at_rva(m, rva)))
            .ok_or(PeUnwinderError::MissingUnwindInfoData(rva))
    }

    pub fn text_memory_at_rva(&self, rva: u32) -> Result<&'a [u8], PeUnwinderError> {
        self.text
            .and_then(|m| memory_at_rva(m, rva))
            .ok_or(PeUnwinderError::MissingInstructionData(rva))
    }
}

fn memory_at_rva<D: core::ops::Deref<Target = [u8]>>(
    DataAtRvaRange { data, rva_range }: &DataAtRvaRange<D>,
    address: u32,
) -> Option<&[u8]> {
    if rva_range.contains(&address) {
        let offset = address - rva_range.start;
        Some(&data[(offset as usize)..])
    } else {
        None
    }
}

pub trait PeUnwinding: Arch {
    fn unwind_frame<F, D>(
        sections: PeSections<D>,
        address: u32,
        regs: &mut Self::UnwindRegs,
        is_first_frame: bool,
        read_stack: &mut F,
    ) -> Result<UnwindResult<Self::UnwindRule>, PeUnwinderError>
    where
        F: FnMut(u64) -> Result<u64, ()>,
        D: core::ops::Deref<Target = [u8]>;
}
