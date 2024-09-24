use alloc::string::String;
use alloc::sync::Arc;
use alloc::vec::Vec;
use fallible_iterator::FallibleIterator;
use gimli::{EndianSlice, LittleEndian};

use crate::arch::Arch;
use crate::cache::{AllocationPolicy, Cache};
use crate::dwarf::{DwarfCfiIndex, DwarfUnwinder, DwarfUnwinding, UnwindSectionType};
use crate::error::{Error, UnwinderError};
use crate::instruction_analysis::InstructionAnalysis;

#[cfg(feature = "macho")]
use crate::macho::{
    CompactUnwindInfoUnwinder, CompactUnwindInfoUnwinding, CuiUnwindResult, TextBytes,
};
#[cfg(feature = "pe")]
use crate::pe::{DataAtRvaRange, PeUnwinding};
use crate::rule_cache::CacheResult;
use crate::unwind_result::UnwindResult;
use crate::unwind_rule::UnwindRule;
use crate::FrameAddress;

use core::marker::PhantomData;
use core::ops::{Deref, Range};
use core::sync::atomic::{AtomicU16, Ordering};

/// Unwinder is the trait that each CPU architecture's concrete unwinder type implements.
/// This trait's methods are what let you do the actual unwinding.
pub trait Unwinder: Clone {
    /// The unwind registers type for the targeted CPU architecture.
    type UnwindRegs;

    /// The unwind cache for the targeted CPU architecture.
    /// This is an associated type because the cache stores unwind rules, whose concrete
    /// type depends on the CPU arch, and because the cache can support different allocation
    /// policies.
    type Cache;

    /// The module type. This is an associated type because the concrete type varies
    /// depending on the type you use to give the module access to the unwind section data.
    type Module;

    /// Add a module that's loaded in the profiled process. This is how you provide unwind
    /// information and address ranges.
    ///
    /// This should be called whenever a new module is loaded into the process.
    fn add_module(&mut self, module: Self::Module);

    /// Remove a module that was added before using `add_module`, keyed by the start
    /// address of that module's address range. If no match is found, the call is ignored.
    /// This should be called whenever a module is unloaded from the process.
    fn remove_module(&mut self, module_avma_range_start: u64);

    /// Returns the highest code address that is known in this process based on the module
    /// address ranges. Returns 0 if no modules have been added.
    ///
    /// This method can be used together with
    /// [`PtrAuthMask::from_max_known_address`](crate::aarch64::PtrAuthMask::from_max_known_address)
    /// to make an educated guess at a pointer authentication mask for Aarch64 return addresses.
    fn max_known_code_address(&self) -> u64;

    /// Unwind a single frame, to recover return address and caller register values.
    /// This is the main entry point for unwinding.
    fn unwind_frame<F>(
        &self,
        address: FrameAddress,
        regs: &mut Self::UnwindRegs,
        cache: &mut Self::Cache,
        read_stack: &mut F,
    ) -> Result<Option<u64>, Error>
    where
        F: FnMut(u64) -> Result<u64, ()>;

    /// Return an iterator that unwinds frame by frame until the end of the stack is found.
    fn iter_frames<'u, 'c, 'r, F>(
        &'u self,
        pc: u64,
        regs: Self::UnwindRegs,
        cache: &'c mut Self::Cache,
        read_stack: &'r mut F,
    ) -> UnwindIterator<'u, 'c, 'r, Self, F>
    where
        F: FnMut(u64) -> Result<u64, ()>,
    {
        UnwindIterator::new(self, pc, regs, cache, read_stack)
    }
}

/// An iterator for unwinding the entire stack, starting from the initial register values.
///
/// The first yielded frame is the instruction pointer. Subsequent addresses are return
/// addresses.
///
/// This iterator attempts to detect if stack unwinding completed successfully, or if the
/// stack was truncated prematurely. If it thinks that it successfully found the root
/// function, it will complete with `Ok(None)`, otherwise it will complete with `Err(...)`.
/// However, the detection does not work in all cases, so you should expect `Err(...)` to
/// be returned even during normal operation. As a result, it is not recommended to use
/// this iterator as a `FallibleIterator`, because you might lose the entire stack if the
/// last iteration returns `Err(...)`.
///
/// Lifetimes:
///
///  - `'u`: The lifetime of the [`Unwinder`].
///  - `'c`: The lifetime of the unwinder cache.
///  - `'r`: The lifetime of the exclusive access to the `read_stack` callback.
pub struct UnwindIterator<'u, 'c, 'r, U: Unwinder + ?Sized, F: FnMut(u64) -> Result<u64, ()>> {
    unwinder: &'u U,
    state: UnwindIteratorState,
    regs: U::UnwindRegs,
    cache: &'c mut U::Cache,
    read_stack: &'r mut F,
}

enum UnwindIteratorState {
    Initial(u64),
    Unwinding(FrameAddress),
    Done,
}

impl<'u, 'c, 'r, U: Unwinder + ?Sized, F: FnMut(u64) -> Result<u64, ()>>
    UnwindIterator<'u, 'c, 'r, U, F>
{
    /// Create a new iterator. You'd usually use [`Unwinder::iter_frames`] instead.
    pub fn new(
        unwinder: &'u U,
        pc: u64,
        regs: U::UnwindRegs,
        cache: &'c mut U::Cache,
        read_stack: &'r mut F,
    ) -> Self {
        Self {
            unwinder,
            state: UnwindIteratorState::Initial(pc),
            regs,
            cache,
            read_stack,
        }
    }
}

impl<'u, 'c, 'r, U: Unwinder + ?Sized, F: FnMut(u64) -> Result<u64, ()>>
    UnwindIterator<'u, 'c, 'r, U, F>
{
    /// Yield the next frame in the stack.
    ///
    /// The first frame is `Ok(Some(FrameAddress::InstructionPointer(...)))`.
    /// Subsequent frames are `Ok(Some(FrameAddress::ReturnAddress(...)))`.
    ///
    /// If a root function has been reached, this iterator completes with `Ok(None)`.
    /// Otherwise it completes with `Err(...)`, usually indicating that a certain stack
    /// address could not be read.
    #[allow(clippy::should_implement_trait)]
    pub fn next(&mut self) -> Result<Option<FrameAddress>, Error> {
        let next = match self.state {
            UnwindIteratorState::Initial(pc) => {
                self.state = UnwindIteratorState::Unwinding(FrameAddress::InstructionPointer(pc));
                return Ok(Some(FrameAddress::InstructionPointer(pc)));
            }
            UnwindIteratorState::Unwinding(address) => {
                self.unwinder
                    .unwind_frame(address, &mut self.regs, self.cache, self.read_stack)?
            }
            UnwindIteratorState::Done => return Ok(None),
        };
        match next {
            Some(return_address) => {
                let return_address = FrameAddress::from_return_address(return_address)
                    .ok_or(Error::ReturnAddressIsNull)?;
                self.state = UnwindIteratorState::Unwinding(return_address);
                Ok(Some(return_address))
            }
            None => {
                self.state = UnwindIteratorState::Done;
                Ok(None)
            }
        }
    }
}

impl<'u, 'c, 'r, U: Unwinder + ?Sized, F: FnMut(u64) -> Result<u64, ()>> FallibleIterator
    for UnwindIterator<'u, 'c, 'r, U, F>
{
    type Item = FrameAddress;
    type Error = Error;

    fn next(&mut self) -> Result<Option<FrameAddress>, Error> {
        self.next()
    }
}

/// This global generation counter makes it so that the cache can be shared
/// between multiple unwinders.
/// This is a u16, so if you make it wrap around by adding / removing modules
/// more than 65535 times, then you risk collisions in the cache; meaning:
/// unwinding might not work properly if an old unwind rule was found in the
/// cache for the same address and the same (pre-wraparound) modules_generation.
static GLOBAL_MODULES_GENERATION: AtomicU16 = AtomicU16::new(0);

fn next_global_modules_generation() -> u16 {
    GLOBAL_MODULES_GENERATION.fetch_add(1, Ordering::Relaxed)
}

cfg_if::cfg_if! {
    if #[cfg(all(feature = "macho", feature = "pe"))] {
        pub trait Unwinding:
            Arch + DwarfUnwinding + InstructionAnalysis + CompactUnwindInfoUnwinding + PeUnwinding {}
        impl<T: Arch + DwarfUnwinding + InstructionAnalysis + CompactUnwindInfoUnwinding + PeUnwinding>
            Unwinding for T {}
    } else if #[cfg(feature = "macho")] {
        pub trait Unwinding:
            Arch + DwarfUnwinding + InstructionAnalysis + CompactUnwindInfoUnwinding {}
        impl<T: Arch + DwarfUnwinding + InstructionAnalysis + CompactUnwindInfoUnwinding> Unwinding for T {}
    } else if #[cfg(feature = "pe")] {
        pub trait Unwinding:
            Arch + DwarfUnwinding + InstructionAnalysis  + PeUnwinding {}
        impl<T: Arch + DwarfUnwinding + InstructionAnalysis + PeUnwinding> Unwinding for T {}
    } else {
        pub trait Unwinding: Arch + DwarfUnwinding + InstructionAnalysis {}
        impl<T: Arch + DwarfUnwinding + InstructionAnalysis> Unwinding for T {}
    }
}

pub struct UnwinderInternal<D, A, P> {
    /// sorted by avma_range.start
    modules: Vec<Module<D>>,
    /// Incremented every time modules is changed.
    modules_generation: u16,
    _arch: PhantomData<A>,
    _allocation_policy: PhantomData<P>,
}

impl<D, A, P> Default for UnwinderInternal<D, A, P> {
    fn default() -> Self {
        Self::new()
    }
}

impl<D, A, P> Clone for UnwinderInternal<D, A, P> {
    fn clone(&self) -> Self {
        Self {
            modules: self.modules.clone(),
            modules_generation: self.modules_generation,
            _arch: PhantomData,
            _allocation_policy: PhantomData,
        }
    }
}

impl<D, A, P> UnwinderInternal<D, A, P> {
    pub fn new() -> Self {
        Self {
            modules: Vec::new(),
            modules_generation: next_global_modules_generation(),
            _arch: PhantomData,
            _allocation_policy: PhantomData,
        }
    }
}

impl<D: Deref<Target = [u8]>, A: Unwinding, P: AllocationPolicy> UnwinderInternal<D, A, P> {
    pub fn add_module(&mut self, module: Module<D>) {
        let insertion_index = match self
            .modules
            .binary_search_by_key(&module.avma_range.start, |module| module.avma_range.start)
        {
            Ok(i) => {
                #[cfg(feature = "std")]
                eprintln!(
                    "Now we have two modules at the same start address 0x{:x}. This can't be good.",
                    module.avma_range.start
                );
                i
            }
            Err(i) => i,
        };
        self.modules.insert(insertion_index, module);
        self.modules_generation = next_global_modules_generation();
    }

    pub fn remove_module(&mut self, module_address_range_start: u64) {
        if let Ok(index) = self
            .modules
            .binary_search_by_key(&module_address_range_start, |module| {
                module.avma_range.start
            })
        {
            self.modules.remove(index);
            self.modules_generation = next_global_modules_generation();
        };
    }

    pub fn max_known_code_address(&self) -> u64 {
        self.modules.last().map_or(0, |m| m.avma_range.end)
    }

    fn find_module_for_address(&self, address: u64) -> Option<(usize, u32)> {
        let (module_index, module) = match self
            .modules
            .binary_search_by_key(&address, |m| m.avma_range.start)
        {
            Ok(i) => (i, &self.modules[i]),
            Err(insertion_index) => {
                if insertion_index == 0 {
                    // address is before first known module
                    return None;
                }
                let i = insertion_index - 1;
                let module = &self.modules[i];
                if module.avma_range.end <= address {
                    // address is after this module
                    return None;
                }
                (i, module)
            }
        };
        if address < module.base_avma {
            // Invalid base address
            return None;
        }
        let relative_address = u32::try_from(address - module.base_avma).ok()?;
        Some((module_index, relative_address))
    }

    fn with_cache<F, G>(
        &self,
        address: FrameAddress,
        regs: &mut A::UnwindRegs,
        cache: &mut Cache<A::UnwindRule, P>,
        read_stack: &mut F,
        callback: G,
    ) -> Result<Option<u64>, Error>
    where
        F: FnMut(u64) -> Result<u64, ()>,
        G: FnOnce(
            &Module<D>,
            FrameAddress,
            u32,
            &mut A::UnwindRegs,
            &mut Cache<A::UnwindRule, P>,
            &mut F,
        ) -> Result<UnwindResult<A::UnwindRule>, UnwinderError>,
    {
        let lookup_address = address.address_for_lookup();
        let is_first_frame = !address.is_return_address();
        let cache_handle = match cache
            .rule_cache
            .lookup(lookup_address, self.modules_generation)
        {
            CacheResult::Hit(unwind_rule) => {
                return unwind_rule.exec(is_first_frame, regs, read_stack);
            }
            CacheResult::Miss(handle) => handle,
        };

        let unwind_rule = match self.find_module_for_address(lookup_address) {
            None => A::UnwindRule::fallback_rule(),
            Some((module_index, relative_lookup_address)) => {
                let module = &self.modules[module_index];
                match callback(
                    module,
                    address,
                    relative_lookup_address,
                    regs,
                    cache,
                    read_stack,
                ) {
                    Ok(UnwindResult::ExecRule(rule)) => rule,
                    Ok(UnwindResult::Uncacheable(return_address)) => {
                        return Ok(Some(return_address))
                    }
                    Err(_err) => {
                        // eprintln!("Unwinder error: {}", err);
                        A::UnwindRule::fallback_rule()
                    }
                }
            }
        };
        cache.rule_cache.insert(cache_handle, unwind_rule);
        unwind_rule.exec(is_first_frame, regs, read_stack)
    }

    pub fn unwind_frame<F>(
        &self,
        address: FrameAddress,
        regs: &mut A::UnwindRegs,
        cache: &mut Cache<A::UnwindRule, P>,
        read_stack: &mut F,
    ) -> Result<Option<u64>, Error>
    where
        F: FnMut(u64) -> Result<u64, ()>,
    {
        self.with_cache(address, regs, cache, read_stack, Self::unwind_frame_impl)
    }

    fn unwind_frame_impl<F>(
        module: &Module<D>,
        address: FrameAddress,
        rel_lookup_address: u32,
        regs: &mut A::UnwindRegs,
        cache: &mut Cache<A::UnwindRule, P>,
        read_stack: &mut F,
    ) -> Result<UnwindResult<A::UnwindRule>, UnwinderError>
    where
        F: FnMut(u64) -> Result<u64, ()>,
    {
        let is_first_frame = !address.is_return_address();
        let unwind_result = match &*module.unwind_data {
            #[cfg(feature = "macho")]
            ModuleUnwindDataInternal::CompactUnwindInfoAndEhFrame {
                unwind_info,
                eh_frame,
                stubs_svma: stubs,
                stub_helper_svma: stub_helper,
                base_addresses,
                text_data,
            } => {
                // eprintln!("unwinding with cui and eh_frame in module {}", module.name);
                let text_bytes = text_data.as_ref().and_then(|data| {
                    let offset_from_base =
                        u32::try_from(data.svma_range.start.checked_sub(module.base_svma)?).ok()?;
                    Some(TextBytes::new(offset_from_base, &data.bytes[..]))
                });
                let stubs_range = if let Some(stubs_range) = stubs {
                    (
                        (stubs_range.start - module.base_svma) as u32,
                        (stubs_range.end - module.base_svma) as u32,
                    )
                } else {
                    (0, 0)
                };
                let stub_helper_range = if let Some(stub_helper_range) = stub_helper {
                    (
                        (stub_helper_range.start - module.base_svma) as u32,
                        (stub_helper_range.end - module.base_svma) as u32,
                    )
                } else {
                    (0, 0)
                };
                let mut unwinder = CompactUnwindInfoUnwinder::<A>::new(
                    &unwind_info[..],
                    text_bytes,
                    stubs_range,
                    stub_helper_range,
                );

                let unwind_result = unwinder.unwind_frame(rel_lookup_address, is_first_frame)?;
                match unwind_result {
                    CuiUnwindResult::ExecRule(rule) => UnwindResult::ExecRule(rule),
                    CuiUnwindResult::NeedDwarf(fde_offset) => {
                        let eh_frame_data =
                            eh_frame.as_deref().ok_or(UnwinderError::NoDwarfData)?;
                        let mut dwarf_unwinder = DwarfUnwinder::<_, A, _>::new(
                            EndianSlice::new(eh_frame_data, LittleEndian),
                            UnwindSectionType::EhFrame,
                            None,
                            &mut cache.gimli_unwind_context,
                            base_addresses.clone(),
                            module.base_svma,
                        );
                        dwarf_unwinder.unwind_frame_with_fde::<_, P::GimliEvaluationStorage<_>>(
                            regs,
                            is_first_frame,
                            rel_lookup_address,
                            fde_offset,
                            read_stack,
                        )?
                    }
                }
            }
            ModuleUnwindDataInternal::EhFrameHdrAndEhFrame {
                eh_frame_hdr,
                eh_frame,
                base_addresses,
            } => {
                let eh_frame_hdr_data = &eh_frame_hdr[..];
                let mut dwarf_unwinder = DwarfUnwinder::<_, A, _>::new(
                    EndianSlice::new(eh_frame, LittleEndian),
                    UnwindSectionType::EhFrame,
                    Some(eh_frame_hdr_data),
                    &mut cache.gimli_unwind_context,
                    base_addresses.clone(),
                    module.base_svma,
                );
                let fde_offset = dwarf_unwinder
                    .get_fde_offset_for_relative_address(rel_lookup_address)
                    .ok_or(UnwinderError::EhFrameHdrCouldNotFindAddress)?;
                dwarf_unwinder.unwind_frame_with_fde::<_, P::GimliEvaluationStorage<_>>(
                    regs,
                    is_first_frame,
                    rel_lookup_address,
                    fde_offset,
                    read_stack,
                )?
            }
            ModuleUnwindDataInternal::DwarfCfiIndexAndEhFrame {
                index,
                eh_frame,
                base_addresses,
            } => {
                let mut dwarf_unwinder = DwarfUnwinder::<_, A, _>::new(
                    EndianSlice::new(eh_frame, LittleEndian),
                    UnwindSectionType::EhFrame,
                    None,
                    &mut cache.gimli_unwind_context,
                    base_addresses.clone(),
                    module.base_svma,
                );
                let fde_offset = index
                    .fde_offset_for_relative_address(rel_lookup_address)
                    .ok_or(UnwinderError::DwarfCfiIndexCouldNotFindAddress)?;
                dwarf_unwinder.unwind_frame_with_fde::<_, P::GimliEvaluationStorage<_>>(
                    regs,
                    is_first_frame,
                    rel_lookup_address,
                    fde_offset,
                    read_stack,
                )?
            }
            ModuleUnwindDataInternal::DwarfCfiIndexAndDebugFrame {
                index,
                debug_frame,
                base_addresses,
            } => {
                let mut dwarf_unwinder = DwarfUnwinder::<_, A, _>::new(
                    EndianSlice::new(debug_frame, LittleEndian),
                    UnwindSectionType::DebugFrame,
                    None,
                    &mut cache.gimli_unwind_context,
                    base_addresses.clone(),
                    module.base_svma,
                );
                let fde_offset = index
                    .fde_offset_for_relative_address(rel_lookup_address)
                    .ok_or(UnwinderError::DwarfCfiIndexCouldNotFindAddress)?;
                dwarf_unwinder.unwind_frame_with_fde::<_, P::GimliEvaluationStorage<_>>(
                    regs,
                    is_first_frame,
                    rel_lookup_address,
                    fde_offset,
                    read_stack,
                )?
            }
            #[cfg(feature = "pe")]
            ModuleUnwindDataInternal::PeUnwindInfo {
                pdata,
                rdata,
                xdata,
                text,
            } => <A as PeUnwinding>::unwind_frame(
                crate::pe::PeSections {
                    pdata,
                    rdata: rdata.as_ref(),
                    xdata: xdata.as_ref(),
                    text: text.as_ref(),
                },
                rel_lookup_address,
                regs,
                is_first_frame,
                read_stack,
            )?,
            ModuleUnwindDataInternal::None => return Err(UnwinderError::NoModuleUnwindData),
        };
        Ok(unwind_result)
    }
}

/// The unwind data that should be used when unwinding addresses inside this module.
/// Unwind data describes how to recover register values of the caller frame.
///
/// The type of unwind information you use depends on the platform and what's available
/// in the binary.
///
/// Type arguments:
///
///  - `D`: The type for unwind section data. This allows carrying owned data on the
///    module, e.g. `Vec<u8>`. But it could also be a wrapper around mapped memory from
///    a file or a different process, for example. It just needs to provide a slice of
///    bytes via its `Deref` implementation.
enum ModuleUnwindDataInternal<D> {
    /// Used on macOS, with mach-O binaries. Compact unwind info is in the `__unwind_info`
    /// section and is sometimes supplemented with DWARF CFI information in the `__eh_frame`
    /// section. `__stubs` and `__stub_helper` ranges are used by the unwinder.
    #[cfg(feature = "macho")]
    CompactUnwindInfoAndEhFrame {
        unwind_info: D,
        eh_frame: Option<D>,
        stubs_svma: Option<Range<u64>>,
        stub_helper_svma: Option<Range<u64>>,
        base_addresses: crate::dwarf::BaseAddresses,
        text_data: Option<TextByteData<D>>,
    },
    /// Used with ELF binaries (Linux and friends), in the `.eh_frame_hdr` and `.eh_frame`
    /// sections. Contains an index and DWARF CFI.
    EhFrameHdrAndEhFrame {
        eh_frame_hdr: D,
        eh_frame: D,
        base_addresses: crate::dwarf::BaseAddresses,
    },
    /// Used with ELF binaries (Linux and friends), in the `.eh_frame` section. Contains
    /// DWARF CFI. We create a binary index for the FDEs when a module with this unwind
    /// data type is added.
    DwarfCfiIndexAndEhFrame {
        index: DwarfCfiIndex,
        eh_frame: D,
        base_addresses: crate::dwarf::BaseAddresses,
    },
    /// Used with ELF binaries (Linux and friends), in the `.debug_frame` section. Contains
    /// DWARF CFI. We create a binary index for the FDEs when a module with this unwind
    /// data type is added.
    DwarfCfiIndexAndDebugFrame {
        index: DwarfCfiIndex,
        debug_frame: D,
        base_addresses: crate::dwarf::BaseAddresses,
    },
    /// Used with PE binaries (Windows).
    #[cfg(feature = "pe")]
    PeUnwindInfo {
        pdata: D,
        rdata: Option<DataAtRvaRange<D>>,
        xdata: Option<DataAtRvaRange<D>>,
        text: Option<DataAtRvaRange<D>>,
    },
    /// No unwind information is used. Unwinding in this module will use a fallback rule
    /// (usually frame pointer unwinding).
    None,
}

impl<D: Deref<Target = [u8]>> ModuleUnwindDataInternal<D> {
    fn new(section_info: &mut impl ModuleSectionInfo<D>) -> Self {
        use crate::dwarf::base_addresses_for_sections;

        #[cfg(feature = "macho")]
        if let Some(unwind_info) = section_info.section_data(b"__unwind_info") {
            let eh_frame = section_info.section_data(b"__eh_frame");
            let stubs = section_info.section_svma_range(b"__stubs");
            let stub_helper = section_info.section_svma_range(b"__stub_helper");
            // Get the bytes of the executable code (instructions).
            //
            // In mach-O objects, executable code is stored in the `__TEXT` segment, which contains
            // multiple executable sections such as `__text`, `__stubs`, and `__stub_helper`. If we
            // don't have the full `__TEXT` segment contents, we can fall back to the contents of
            // just the `__text` section.
            let text_data = if let (Some(bytes), Some(svma_range)) = (
                section_info.segment_data(b"__TEXT"),
                section_info.segment_svma_range(b"__TEXT"),
            ) {
                Some(TextByteData { bytes, svma_range })
            } else if let (Some(bytes), Some(svma_range)) = (
                section_info.section_data(b"__text"),
                section_info.section_svma_range(b"__text"),
            ) {
                Some(TextByteData { bytes, svma_range })
            } else {
                None
            };
            return ModuleUnwindDataInternal::CompactUnwindInfoAndEhFrame {
                unwind_info,
                eh_frame,
                stubs_svma: stubs,
                stub_helper_svma: stub_helper,
                base_addresses: base_addresses_for_sections(section_info),
                text_data,
            };
        }

        #[cfg(feature = "pe")]
        if let Some(pdata) = section_info.section_data(b".pdata") {
            let mut range_and_data = |name| {
                let rva_range = section_info.section_svma_range(name).and_then(|range| {
                    Some(Range {
                        start: (range.start - section_info.base_svma()).try_into().ok()?,
                        end: (range.end - section_info.base_svma()).try_into().ok()?,
                    })
                })?;
                let data = section_info.section_data(name)?;
                Some(DataAtRvaRange { data, rva_range })
            };
            return ModuleUnwindDataInternal::PeUnwindInfo {
                pdata,
                rdata: range_and_data(b".rdata"),
                xdata: range_and_data(b".xdata"),
                text: range_and_data(b".text"),
            };
        }

        if let Some(eh_frame) = section_info
            .section_data(b".eh_frame")
            .or_else(|| section_info.section_data(b"__eh_frame"))
        {
            if let Some(eh_frame_hdr) = section_info
                .section_data(b".eh_frame_hdr")
                .or_else(|| section_info.section_data(b"__eh_frame_hdr"))
            {
                ModuleUnwindDataInternal::EhFrameHdrAndEhFrame {
                    eh_frame_hdr,
                    eh_frame,
                    base_addresses: base_addresses_for_sections(section_info),
                }
            } else {
                match DwarfCfiIndex::try_new_eh_frame(&eh_frame, section_info) {
                    Ok(index) => ModuleUnwindDataInternal::DwarfCfiIndexAndEhFrame {
                        index,
                        eh_frame,
                        base_addresses: base_addresses_for_sections(section_info),
                    },
                    Err(_) => ModuleUnwindDataInternal::None,
                }
            }
        } else if let Some(debug_frame) = section_info.section_data(b".debug_frame") {
            match DwarfCfiIndex::try_new_debug_frame(&debug_frame, section_info) {
                Ok(index) => ModuleUnwindDataInternal::DwarfCfiIndexAndDebugFrame {
                    index,
                    debug_frame,
                    base_addresses: base_addresses_for_sections(section_info),
                },
                Err(_) => ModuleUnwindDataInternal::None,
            }
        } else {
            ModuleUnwindDataInternal::None
        }
    }
}

/// Used to supply raw instruction bytes to the unwinder, which uses it to analyze
/// instructions in order to provide high quality unwinding inside function prologues and
/// epilogues.
///
/// This is only needed on macOS, because mach-O `__unwind_info` and `__eh_frame` only
/// cares about accuracy in function bodies, not in function prologues and epilogues.
///
/// On Linux, compilers produce `.eh_frame` and `.debug_frame` which provides correct
/// unwind information for all instructions including those in function prologues and
/// epilogues, so instruction analysis is not needed.
///
/// Type arguments:
///
///  - `D`: The type for unwind section data. This allows carrying owned data on the
///    module, e.g. `Vec<u8>`. But it could also be a wrapper around mapped memory from
///    a file or a different process, for example. It just needs to provide a slice of
///    bytes via its `Deref` implementation.
#[cfg(feature = "macho")]
struct TextByteData<D> {
    pub bytes: D,
    pub svma_range: Range<u64>,
}

/// Information about a module that is loaded in a process. You might know this under a
/// different name, for example: (Shared) library, binary image, DSO ("Dynamic shared object")
///
/// The unwinder needs to have an up-to-date list of modules so that it can match an
/// absolute address to the right module, and so that it can find that module's unwind
/// information.
///
/// Type arguments:
///
///  - `D`: The type for unwind section data. This allows carrying owned data on the
///    module, e.g. `Vec<u8>`. But it could also be a wrapper around mapped memory from
///    a file or a different process, for example. It just needs to provide a slice of
///    bytes via its `Deref` implementation.
pub struct Module<D> {
    /// The name or file path of the module. Unused, it's just there for easier debugging.
    #[allow(unused)]
    name: String,
    /// The address range where this module is mapped into the process.
    avma_range: Range<u64>,
    /// The base address of this module, in the process's address space. On Linux, the base
    /// address can sometimes be different from the start address of the mapped range.
    base_avma: u64,
    /// The base address of this module, according to the module.
    base_svma: u64,
    /// The unwind data that should be used for unwinding addresses from this module.
    unwind_data: Arc<ModuleUnwindDataInternal<D>>,
}

impl<D> Clone for Module<D> {
    fn clone(&self) -> Self {
        Self {
            name: self.name.clone(),
            avma_range: self.avma_range.clone(),
            base_avma: self.base_avma,
            base_svma: self.base_svma,
            unwind_data: self.unwind_data.clone(),
        }
    }
}

/// Information about a module's sections (and segments).
///
/// This trait is used as an interface to module information, and each function with `&mut self` is
/// called at most once with a particular argument (e.g., `section_data(b".text")` will be called
/// at most once, so it can move data out of the underlying type if desired).
///
/// Type arguments:
///
///  - `D`: The type for section data. This allows carrying owned data on the module, e.g.
///    `Vec<u8>`. But it could also be a wrapper around mapped memory from a file or a different
///    process, for example.
pub trait ModuleSectionInfo<D> {
    /// Return the base address stated in the module.
    ///
    /// For mach-O objects, this is the vmaddr of the __TEXT segment. For ELF objects, this is
    /// zero. For PE objects, this is the image base address.
    ///
    /// This is used to convert between SVMAs and relative addresses.
    fn base_svma(&self) -> u64;

    /// Get the given section's memory range, as stated in the module.
    fn section_svma_range(&mut self, name: &[u8]) -> Option<Range<u64>>;

    /// Get the given section's data. This will only be called once per section.
    fn section_data(&mut self, name: &[u8]) -> Option<D>;

    /// Get the given segment's memory range, as stated in the module.
    fn segment_svma_range(&mut self, _name: &[u8]) -> Option<Range<u64>> {
        None
    }

    /// Get the given segment's data. This will only be called once per segment.
    fn segment_data(&mut self, _name: &[u8]) -> Option<D> {
        None
    }
}

/// Explicit addresses and data of various sections in the module. This implements
/// the `ModuleSectionInfo` trait.
///
/// Unless otherwise stated, these are SVMAs, "stated virtual memory addresses", i.e. addresses as
/// stated in the object, as opposed to AVMAs, "actual virtual memory addresses", i.e. addresses in
/// the virtual memory of the profiled process.
///
/// Code addresses inside a module's unwind information are usually written down as SVMAs,
/// or as relative addresses. For example, DWARF CFI can have code addresses expressed as
/// relative-to-.text addresses or as absolute SVMAs. And mach-O compact unwind info
/// contains addresses relative to the image base address.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct ExplicitModuleSectionInfo<D> {
    /// The image base address, as stated in the object. For mach-O objects, this is the
    /// vmaddr of the `__TEXT` segment. For ELF objects, this is zero.
    ///
    /// This is used to convert between SVMAs and relative addresses.
    pub base_svma: u64,
    /// The address range of the `__text` or `.text` section. This is where most of the compiled
    /// code is stored.
    ///
    /// This is used to detect whether we need to do instruction analysis for an address.
    pub text_svma: Option<Range<u64>>,
    /// The data of the `__text` or `.text` section. This is where most of the compiled code is
    /// stored. For mach-O binaries, this does not need to be supplied if `text_segment` is supplied.
    ///
    /// This is used to handle function prologues and epilogues in some cases.
    pub text: Option<D>,
    /// The address range of the mach-O `__stubs` section. Contains small pieces of
    /// executable code for calling imported functions. Code inside this section is not
    /// covered by the unwind information in `__unwind_info`.
    ///
    /// This is used to exclude addresses in this section from incorrectly applying
    /// `__unwind_info` opcodes. It is also used to infer unwind rules for the known
    /// structure of stub functions.
    pub stubs_svma: Option<Range<u64>>,
    /// The address range of the mach-O `__stub_helper` section. Contains small pieces of
    /// executable code for calling imported functions. Code inside this section is not
    /// covered by the unwind information in `__unwind_info`.
    ///
    /// This is used to exclude addresses in this section from incorrectly applying
    /// `__unwind_info` opcodes. It is also used to infer unwind rules for the known
    /// structure of stub helper
    /// functions.
    pub stub_helper_svma: Option<Range<u64>>,
    /// The address range of the `.got` section (Global Offset Table). This is used
    /// during DWARF CFI processing, to resolve got-relative addresses.
    pub got_svma: Option<Range<u64>>,
    /// The data of the `__unwind_info` section of mach-O binaries.
    pub unwind_info: Option<D>,
    /// The address range of the `__eh_frame` or `.eh_frame` section. This is used during DWARF CFI
    /// processing, to resolve eh_frame-relative addresses.
    pub eh_frame_svma: Option<Range<u64>>,
    /// The data of the `__eh_frame` or `.eh_frame` section. This is used during DWARF CFI
    /// processing, to resolve eh_frame-relative addresses.
    pub eh_frame: Option<D>,
    /// The address range of the `.eh_frame_hdr` section. This is used during DWARF CFI processing,
    /// to resolve eh_frame_hdr-relative addresses.
    pub eh_frame_hdr_svma: Option<Range<u64>>,
    /// The data of the `.eh_frame_hdr` section. This is used during DWARF CFI processing, to
    /// resolve eh_frame_hdr-relative addresses.
    pub eh_frame_hdr: Option<D>,
    /// The data of the `.debug_frame` section. The related address range is not needed.
    pub debug_frame: Option<D>,
    /// The address range of the `__TEXT` segment of mach-O binaries, if available.
    pub text_segment_svma: Option<Range<u64>>,
    /// The data of the `__TEXT` segment of mach-O binaries, if available.
    pub text_segment: Option<D>,
}

impl<D> ModuleSectionInfo<D> for ExplicitModuleSectionInfo<D>
where
    D: Deref<Target = [u8]>,
{
    fn base_svma(&self) -> u64 {
        self.base_svma
    }

    fn section_svma_range(&mut self, name: &[u8]) -> Option<Range<u64>> {
        match name {
            b"__text" | b".text" => self.text_svma.clone(),
            b"__stubs" => self.stubs_svma.clone(),
            b"__stub_helper" => self.stub_helper_svma.clone(),
            b"__eh_frame" | b".eh_frame" => self.eh_frame_svma.clone(),
            b"__eh_frame_hdr" | b".eh_frame_hdr" => self.eh_frame_hdr_svma.clone(),
            b"__got" | b".got" => self.got_svma.clone(),
            _ => None,
        }
    }
    fn section_data(&mut self, name: &[u8]) -> Option<D> {
        match name {
            b"__text" | b".text" => self.text.take(),
            b"__unwind_info" => self.unwind_info.take(),
            b"__eh_frame" | b".eh_frame" => self.eh_frame.take(),
            b"__eh_frame_hdr" | b".eh_frame_hdr" => self.eh_frame_hdr.take(),
            b"__debug_frame" | b".debug_frame" => self.debug_frame.take(),
            _ => None,
        }
    }
    fn segment_svma_range(&mut self, name: &[u8]) -> Option<Range<u64>> {
        match name {
            b"__TEXT" => self.text_segment_svma.clone(),
            _ => None,
        }
    }
    fn segment_data(&mut self, name: &[u8]) -> Option<D> {
        match name {
            b"__TEXT" => self.text_segment.take(),
            _ => None,
        }
    }
}

#[cfg(feature = "object")]
mod object {
    use super::{ModuleSectionInfo, Range};
    use object::read::{Object, ObjectSection, ObjectSegment};

    impl<'data: 'file, 'file, O, D> ModuleSectionInfo<D> for &'file O
    where
        O: Object<'data>,
        D: From<&'data [u8]>,
    {
        fn base_svma(&self) -> u64 {
            if let Some(text_segment) = self.segments().find(|s| s.name() == Ok(Some("__TEXT"))) {
                // This is a mach-O image. "Relative addresses" are relative to the
                // vmaddr of the __TEXT segment.
                return text_segment.address();
            }

            // For PE binaries, relative_address_base() returns the image base address.
            // Otherwise it returns zero. This gives regular ELF images a base address of zero,
            // which is what we want.
            self.relative_address_base()
        }

        fn section_svma_range(&mut self, name: &[u8]) -> Option<Range<u64>> {
            let section = self.section_by_name_bytes(name)?;
            Some(section.address()..section.address() + section.size())
        }

        fn section_data(&mut self, name: &[u8]) -> Option<D> {
            let section = self.section_by_name_bytes(name)?;
            section.data().ok().map(|data| data.into())
        }

        fn segment_svma_range(&mut self, name: &[u8]) -> Option<Range<u64>> {
            let segment = self.segments().find(|s| s.name_bytes() == Ok(Some(name)))?;
            Some(segment.address()..segment.address() + segment.size())
        }

        fn segment_data(&mut self, name: &[u8]) -> Option<D> {
            let segment = self.segments().find(|s| s.name_bytes() == Ok(Some(name)))?;
            segment.data().ok().map(|data| data.into())
        }
    }
}

impl<D: Deref<Target = [u8]>> Module<D> {
    pub fn new(
        name: String,
        avma_range: core::ops::Range<u64>,
        base_avma: u64,
        mut section_info: impl ModuleSectionInfo<D>,
    ) -> Self {
        let unwind_data = ModuleUnwindDataInternal::new(&mut section_info);

        Self {
            name,
            avma_range,
            base_avma,
            base_svma: section_info.base_svma(),
            unwind_data: Arc::new(unwind_data),
        }
    }

    pub fn avma_range(&self) -> core::ops::Range<u64> {
        self.avma_range.clone()
    }

    pub fn base_avma(&self) -> u64 {
        self.base_avma
    }

    pub fn name(&self) -> &str {
        &self.name
    }
}
