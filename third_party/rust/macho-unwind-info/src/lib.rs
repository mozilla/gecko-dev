//! A zero-copy parser for the contents of the `__unwind_info` section of a
//! mach-O binary.
//!
//! Quickly look up the unwinding opcode for an address. Then parse the opcode to find
//! out how to recover the return address and the caller frame's register values.
//!
//! This crate is intended to be fast enough to be used in a sampling profiler.
//! Re-parsing from scratch is cheap and can be done on every sample.
//!
//! For the full unwinding experience, both `__unwind_info` and `__eh_frame` may need
//! to be consulted. The two sections are complementary: `__unwind_info` handles the
//! easy cases, and refers to an `__eh_frame` FDE for the hard cases. Conversely,
//! `__eh_frame` only includes FDEs for functions whose unwinding info cannot be
//! represented in `__unwind_info`.
//!
//! On x86 and x86_64, `__unwind_info` can represent most functions regardless of
//! whether they were compiled with framepointers or without.
//!
//! On arm64, compiling without framepointers is strongly discouraged, and
//! `__unwind_info` can only represent functions which have framepointers or
//! which don't need to restore any registers. As a result, if you have an arm64
//! binary without framepointers (rare!), then the `__unwind_info` basically just
//! acts as an index for `__eh_frame`, similarly to `.eh_frame_hdr` for ELF.
//!
//! In clang's default configuration for arm64, non-leaf functions have framepointers
//! and leaf functions without stored registers on the stack don't have framepointers.
//! For leaf functions, the return address is kept in the `lr` register for the entire
//! duration of the function. And the unwind info lets you discern between these two
//! types of functions ("frame-based" and "frameless").
//!
//! # Example
//!
//! ```rust
//! use macho_unwind_info::UnwindInfo;
//! use macho_unwind_info::opcodes::OpcodeX86_64;
//!
//! # fn example(data: &[u8]) -> Result<(), macho_unwind_info::Error> {
//! let unwind_info = UnwindInfo::parse(data)?;
//!
//! if let Some(function) = unwind_info.lookup(0x1234)? {
//!     println!("Found function entry covering the address 0x1234:");
//!     let opcode = OpcodeX86_64::parse(function.opcode);
//!     println!("0x{:08x}..0x{:08x}: {}", function.start_address, function.end_address, opcode);
//! }
//! # Ok(())
//! # }
//! ```

mod error;
mod num_display;

/// Provides architecture-specific opcode parsing.
pub mod opcodes;
/// Lower-level structs for interpreting the format data. Can be used if the convenience APIs are too limiting.
pub mod raw;

mod reader;

pub use error::*;
use raw::*;

/// A parsed representation of the unwind info.
///
/// The UnwindInfo contains a list of pages, each of which contain a list of
/// function entries.
pub struct UnwindInfo<'a> {
    /// The full __unwind_info section data.
    data: &'a [u8],

    /// The list of global opcodes.
    global_opcodes: &'a [Opcode],

    /// The list of page entries in this UnwindInfo.
    pages: &'a [PageEntry],
}

/// The information about a single function in the UnwindInfo.
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub struct Function {
    /// The address where this function starts.
    pub start_address: u32,

    /// The address where this function ends. Includes the padding at the end of
    /// the function. In reality, this is the address of the *next* function
    /// entry, or for the last function this is the address of the sentinel page
    /// entry.
    pub end_address: u32,

    /// The opcode which describes the unwinding information for this function.
    /// This opcode needs to be parsed in an architecture-specific manner.
    /// See the [opcodes] module for the facilities to do so.
    pub opcode: u32,
}

impl<'a> UnwindInfo<'a> {
    /// Create an [UnwindInfo] instance which wraps the raw bytes of a mach-O binary's
    /// `__unwind_info` section. The data can have arbitrary alignment. The parsing done
    /// in this function is minimal; it's basically just three bounds checks.
    pub fn parse(data: &'a [u8]) -> Result<Self, Error> {
        let header = CompactUnwindInfoHeader::parse(data)?;
        let global_opcodes = header.global_opcodes(data)?;
        let pages = header.pages(data)?;
        Ok(Self {
            data,
            global_opcodes,
            pages,
        })
    }

    /// Returns an iterator over all the functions in this UnwindInfo.
    pub fn functions(&self) -> FunctionIter<'a> {
        FunctionIter {
            data: self.data,
            global_opcodes: self.global_opcodes,
            pages: self.pages,
            cur_page: None,
        }
    }

    /// Returns the range of addresses covered by unwind information.
    pub fn address_range(&self) -> core::ops::Range<u32> {
        if self.pages.is_empty() {
            return 0..0;
        }
        let first_page = self.pages.first().unwrap();
        let last_page = self.pages.last().unwrap();
        first_page.first_address()..last_page.first_address()
    }

    /// Looks up the unwind information for the function that covers the given address.
    /// Returns `Ok(Some(function))` if a function was found.
    /// Returns `Ok(None)` if the address was outside of the range of addresses covered
    /// by the unwind info.
    /// Returns `Err(error)` if there was a problem with the format of the `__unwind_info`
    /// data.
    ///
    /// This lookup is architecture agnostic. The opcode is returned as a u32.
    /// To actually perform unwinding, the opcode needs to be parsed in an
    /// architecture-specific manner.
    ///
    /// The design of the compact unwinding format makes this lookup extremely cheap.
    /// It's just two binary searches: First to find the right page, end then to find
    /// the right function within a page. The search happens inside the wrapped data,
    /// with no extra copies.
    pub fn lookup(&self, pc: u32) -> Result<Option<Function>, Error> {
        let Self {
            pages,
            data,
            global_opcodes,
        } = self;
        let page_index = match pages.binary_search_by_key(&pc, PageEntry::first_address) {
            Ok(i) => i,
            Err(insertion_index) => {
                if insertion_index == 0 {
                    return Ok(None);
                }
                insertion_index - 1
            }
        };
        if page_index == pages.len() - 1 {
            // We found the sentinel last page, which just marks the end of the range.
            // So the looked up address is at or after the end address, i.e. outside the
            // range of addresses covered by this UnwindInfo.
            return Ok(None);
        }
        let page_entry = &pages[page_index];
        let next_page_entry = &pages[page_index + 1];
        let page_offset = page_entry.page_offset();
        match page_entry.page_kind(data)? {
            consts::PAGE_KIND_REGULAR => {
                let page = RegularPage::parse(data, page_offset.into())?;
                let functions = page.functions(data, page_offset)?;
                let function_index =
                    match functions.binary_search_by_key(&pc, RegularFunctionEntry::address) {
                        Ok(i) => i,
                        Err(insertion_index) => {
                            if insertion_index == 0 {
                                return Err(Error::InvalidPageEntryFirstAddress);
                            }
                            insertion_index - 1
                        }
                    };
                let entry = &functions[function_index];
                let fun_address = entry.address();
                let next_fun_address = if let Some(next_entry) = functions.get(function_index + 1) {
                    next_entry.address()
                } else {
                    next_page_entry.first_address()
                };
                Ok(Some(Function {
                    start_address: fun_address,
                    end_address: next_fun_address,
                    opcode: entry.opcode(),
                }))
            }
            consts::PAGE_KIND_COMPRESSED => {
                let page = CompressedPage::parse(data, page_offset.into())?;
                let functions = page.functions(data, page_offset)?;
                let page_address = page_entry.first_address();
                let rel_pc = pc - page_address;
                let function_index = match functions.binary_search_by_key(&rel_pc, |&entry| {
                    CompressedFunctionEntry::new(entry.into()).relative_address()
                }) {
                    Ok(i) => i,
                    Err(insertion_index) => {
                        if insertion_index == 0 {
                            return Err(Error::InvalidPageEntryFirstAddress);
                        }
                        insertion_index - 1
                    }
                };

                let entry = CompressedFunctionEntry::new(functions[function_index].into());
                let fun_address = page_address + entry.relative_address();
                let next_fun_address = if let Some(next_entry) = functions.get(function_index + 1) {
                    let next_entry = CompressedFunctionEntry::new((*next_entry).into());
                    page_address + next_entry.relative_address()
                } else {
                    next_page_entry.first_address()
                };

                let opcode_index: usize = entry.opcode_index().into();
                let opcode = if opcode_index < global_opcodes.len() {
                    global_opcodes[opcode_index].opcode()
                } else {
                    let local_opcodes = page.local_opcodes(data, page_offset)?;
                    let local_index = opcode_index - global_opcodes.len();
                    local_opcodes[local_index].opcode()
                };
                Ok(Some(Function {
                    start_address: fun_address,
                    end_address: next_fun_address,
                    opcode,
                }))
            }
            consts::PAGE_KIND_SENTINEL => {
                // Only the last page should be a sentinel page, and we've already checked earlier
                // that we're not in the last page.
                Err(Error::UnexpectedSentinelPage)
            }
            _ => Err(Error::InvalidPageKind),
        }
    }
}

/// An iterator over the functions in an UnwindInfo page.
pub struct FunctionIter<'a> {
    /// The full __unwind_info section data.
    data: &'a [u8],

    /// The list of global opcodes.
    global_opcodes: &'a [Opcode],

    /// The slice of the remaining to-be-iterated-over pages.
    pages: &'a [PageEntry],

    /// The page whose functions we're iterating over at the moment.
    cur_page: Option<PageWithPartialFunctions<'a>>,
}

/// The current page of the function iterator.
/// The functions field is the slice of the remaining to-be-iterated-over functions.
#[derive(Clone, Copy)]
enum PageWithPartialFunctions<'a> {
    Regular {
        next_page_address: u32,
        functions: &'a [RegularFunctionEntry],
    },
    Compressed {
        page_address: u32,
        next_page_address: u32,
        local_opcodes: &'a [Opcode],
        functions: &'a [U32],
    },
}

impl<'a> FunctionIter<'a> {
    #[allow(clippy::should_implement_trait)]
    pub fn next(&mut self) -> Result<Option<Function>, Error> {
        loop {
            let cur_page = if let Some(cur_page) = self.cur_page.as_mut() {
                cur_page
            } else {
                let cur_page = match self.next_page()? {
                    Some(page) => page,
                    None => return Ok(None),
                };
                self.cur_page.insert(cur_page)
            };

            match cur_page {
                PageWithPartialFunctions::Regular {
                    next_page_address,
                    functions,
                } => {
                    if let Some((entry, remainder)) = functions.split_first() {
                        *functions = remainder;
                        let start_address = entry.address();
                        let end_address = remainder
                            .first()
                            .map(RegularFunctionEntry::address)
                            .unwrap_or(*next_page_address);
                        return Ok(Some(Function {
                            start_address,
                            end_address,
                            opcode: entry.opcode(),
                        }));
                    }
                }
                PageWithPartialFunctions::Compressed {
                    page_address,
                    functions,
                    next_page_address,
                    local_opcodes,
                } => {
                    if let Some((entry, remainder)) = functions.split_first() {
                        *functions = remainder;
                        let entry = CompressedFunctionEntry::new((*entry).into());
                        let start_address = *page_address + entry.relative_address();
                        let end_address = match remainder.first() {
                            Some(next_entry) => {
                                let next_entry = CompressedFunctionEntry::new((*next_entry).into());
                                *page_address + next_entry.relative_address()
                            }
                            None => *next_page_address,
                        };
                        let opcode_index: usize = entry.opcode_index().into();
                        let opcode = if opcode_index < self.global_opcodes.len() {
                            self.global_opcodes[opcode_index].opcode()
                        } else {
                            let local_index = opcode_index - self.global_opcodes.len();
                            local_opcodes[local_index].opcode()
                        };
                        return Ok(Some(Function {
                            start_address,
                            end_address,
                            opcode,
                        }));
                    }
                }
            }
            self.cur_page = None;
        }
    }

    fn next_page(&mut self) -> Result<Option<PageWithPartialFunctions<'a>>, Error> {
        let (page_entry, remainder) = match self.pages.split_first() {
            Some(split) => split,
            None => return Ok(None),
        };

        self.pages = remainder;

        let next_page_entry = match remainder.first() {
            Some(entry) => entry,
            None => return Ok(None),
        };

        let page_offset = page_entry.page_offset();
        let page_address = page_entry.first_address();
        let next_page_address = next_page_entry.first_address();
        let data = self.data;
        let cur_page = match page_entry.page_kind(data)? {
            consts::PAGE_KIND_REGULAR => {
                let page = RegularPage::parse(data, page_offset.into())?;
                PageWithPartialFunctions::Regular {
                    functions: page.functions(data, page_offset)?,
                    next_page_address,
                }
            }
            consts::PAGE_KIND_COMPRESSED => {
                let page = CompressedPage::parse(data, page_offset.into())?;
                PageWithPartialFunctions::Compressed {
                    page_address,
                    next_page_address,
                    functions: page.functions(data, page_offset)?,
                    local_opcodes: page.local_opcodes(data, page_offset)?,
                }
            }
            consts::PAGE_KIND_SENTINEL => return Err(Error::UnexpectedSentinelPage),
            _ => return Err(Error::InvalidPageKind),
        };
        Ok(Some(cur_page))
    }
}
