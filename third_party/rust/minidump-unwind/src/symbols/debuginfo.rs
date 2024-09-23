//! This module provides a `SymbolProvider` which uses local binary debuginfo.

use super::{async_trait, FileError, FileKind, FillSymbolError, FrameSymbolizer, FrameWalker};
use cachemap2::CacheMap;
use framehop::Unwinder;
use memmap2::Mmap;
use minidump::{MinidumpModuleList, MinidumpSystemInfo, Module};
use object::read::{macho::FatArch, Architecture};
use std::cell::UnsafeCell;
use std::fs::File;
use std::path::{Path, PathBuf};

/// A symbol provider which gets information from the minidump modules on the local system.
///
/// Note: this symbol provider will currently only restore the registers necessary for unwinding
/// the given platform. In the future this may be extended to restore all registers.
pub struct DebugInfoSymbolProvider {
    unwinder: Box<dyn UnwinderInterface + Send + Sync>,
    symbols: Box<dyn SymbolInterface + Send + Sync>,
    /// The caches and unwinder operate on the memory held by the mapped modules, so this field
    /// must not be dropped until after they are dropped.
    _mapped_modules: Box<[Mmap]>,
}

pub struct DebugInfoSymbolProviderBuilder {
    #[cfg(feature = "debuginfo-symbols")]
    enable_symbols: bool,
}

type ModuleData = std::borrow::Cow<'static, [u8]>;
type FHModule = framehop::Module<ModuleData>;

struct UnwinderImpl<U: Unwinder> {
    unwinder: U,
    unwind_cache: PerThread<U::Cache>,
}

impl<U: Unwinder + Default> Default for UnwinderImpl<U> {
    fn default() -> Self {
        UnwinderImpl {
            unwinder: Default::default(),
            unwind_cache: Default::default(),
        }
    }
}

impl UnwinderImpl<framehop::x86_64::UnwinderX86_64<ModuleData>> {
    pub fn x86_64() -> Box<dyn UnwinderInterface + Send + Sync> {
        Box::<Self>::default()
    }
}

impl UnwinderImpl<framehop::aarch64::UnwinderAarch64<ModuleData>> {
    pub fn aarch64() -> Box<dyn UnwinderInterface + Send + Sync> {
        Box::<Self>::default()
    }
}

trait WalkerRegs: Sized {
    fn regs_from_walker(walker: &(dyn FrameWalker + Send)) -> Option<Self>;
    fn update_walker(self, walker: &mut (dyn FrameWalker + Send)) -> Option<()>;
}

impl WalkerRegs for framehop::x86_64::UnwindRegsX86_64 {
    fn regs_from_walker(walker: &(dyn FrameWalker + Send)) -> Option<Self> {
        let sp = walker.get_callee_register("rsp")?;
        let bp = walker.get_callee_register("rbp")?;
        let ip = walker.get_callee_register("rip")?;
        Some(Self::new(ip, sp, bp))
    }

    fn update_walker(self, walker: &mut (dyn FrameWalker + Send)) -> Option<()> {
        walker.set_cfa(self.sp())?;
        walker.set_caller_register("rbp", self.bp())?;
        Some(())
    }
}

impl WalkerRegs for framehop::aarch64::UnwindRegsAarch64 {
    fn regs_from_walker(walker: &(dyn FrameWalker + Send)) -> Option<Self> {
        let lr = walker.get_callee_register("lr")?;
        let sp = walker.get_callee_register("sp")?;
        let fp = walker.get_callee_register("fp")?;
        // TODO PtrAuthMask on MacOS?
        Some(Self::new(lr, sp, fp))
    }

    fn update_walker(self, walker: &mut (dyn FrameWalker + Send)) -> Option<()> {
        walker.set_cfa(self.sp())?;
        walker.set_caller_register("lr", self.lr())?;
        walker.set_caller_register("fp", self.fp())?;
        Some(())
    }
}

trait UnwinderInterface {
    fn add_module(&mut self, module: FHModule);
    fn unwind_frame(&self, walker: &mut (dyn FrameWalker + Send)) -> Option<()>;
}

impl<U: Unwinder<Module = FHModule>> UnwinderInterface for UnwinderImpl<U>
where
    U::UnwindRegs: WalkerRegs,
    U::Cache: Default,
{
    fn add_module(&mut self, module: FHModule) {
        self.unwinder.add_module(module);
    }

    fn unwind_frame(&self, walker: &mut (dyn FrameWalker + Send)) -> Option<()> {
        let mut regs = U::UnwindRegs::regs_from_walker(walker)?;
        let instruction = walker.get_instruction();
        let result = self.unwind_cache.with(|cache| {
            self.unwinder.unwind_frame(
                if walker.has_grand_callee() {
                    framehop::FrameAddress::from_return_address(instruction + 1).unwrap()
                } else {
                    framehop::FrameAddress::from_instruction_pointer(instruction)
                },
                &mut regs,
                cache,
                &mut |addr| walker.get_register_at_address(addr).ok_or(()),
            )
        });
        let ra = match result {
            Ok(ra) => ra,
            Err(e) => {
                tracing::error!("failed to unwind frame: {e}");
                return None;
            }
        };
        if let Some(ra) = ra {
            walker.set_ra(ra);
        }
        regs.update_walker(walker)?;
        Some(())
    }
}

#[async_trait]
trait SymbolInterface {
    async fn fill_symbol(
        &self,
        module: &(dyn Module + Sync),
        frame: &mut (dyn FrameSymbolizer + Send),
    ) -> Result<(), FillSymbolError>;
}

/// A SymbolInterface that always returns `Ok(())` without doing anything.
struct NoSymbols;

#[async_trait]
impl SymbolInterface for NoSymbols {
    async fn fill_symbol(
        &self,
        _module: &(dyn Module + Sync),
        _frame: &mut (dyn FrameSymbolizer + Send),
    ) -> Result<(), FillSymbolError> {
        Ok(())
    }
}

#[cfg(feature = "debuginfo-symbols")]
mod wholesym_symbol_interface {
    use super::*;
    use futures_util::lock::Mutex;
    use std::collections::HashMap;
    use wholesym::{LookupAddress, SymbolManager, SymbolManagerConfig, SymbolMap};

    pub struct Impl {
        /// Indexed by module base address.
        symbols: HashMap<ModuleKey, Mutex<SymbolMap>>,
    }

    impl Impl {
        pub async fn new(modules: &MinidumpModuleList) -> Self {
            let mut symbols = HashMap::new();
            let symbol_manager = SymbolManager::with_config(SymbolManagerConfig::new());
            for module in modules.iter() {
                let path = effective_debug_file(module, false);
                if let Ok(sm) = symbol_manager
                    .load_symbol_map_for_binary_at_path(&path, None)
                    .await
                {
                    symbols.insert(module.into(), Mutex::new(sm));
                }
            }
            Impl { symbols }
        }
    }

    #[async_trait]
    impl SymbolInterface for Impl {
        async fn fill_symbol(
            &self,
            module: &(dyn Module + Sync),
            frame: &mut (dyn FrameSymbolizer + Send),
        ) -> Result<(), FillSymbolError> {
            let key = ModuleKey::for_module(module);
            let symbol_map = self.symbols.get(&key).ok_or(FillSymbolError {})?;

            use std::convert::TryInto;
            let addr = match (frame.get_instruction() - module.base_address()).try_into() {
                Ok(a) => a,
                Err(e) => {
                    tracing::error!("failed to downcast relative address offset: {e}");
                    return Ok(());
                }
            };

            let address_info = symbol_map
                .lock()
                .await
                .lookup(LookupAddress::Relative(addr))
                .await;

            if let Some(address_info) = address_info {
                frame.set_function(
                    &address_info.symbol.name,
                    module.base_address() + address_info.symbol.address as u64,
                    0,
                );

                if let Some(frames) = address_info.frames {
                    let mut iter = frames.into_iter().rev();
                    if let Some(f) = iter.next() {
                        if let Some(path) = f.file_path {
                            frame.set_source_file(
                                path.raw_path(),
                                f.line_number.unwrap_or(0),
                                module.base_address() + address_info.symbol.address as u64,
                            );
                        }
                    }
                    for f in iter {
                        frame.add_inline_frame(
                            f.function.as_deref().unwrap_or(""),
                            f.file_path.as_ref().map(|p| p.raw_path()),
                            f.line_number,
                        );
                    }
                }
            }
            Ok(())
        }
    }
}

#[derive(Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
struct ModuleKey(u64);

impl ModuleKey {
    /// Create a module key for the given module.
    pub fn for_module(module: &dyn Module) -> Self {
        ModuleKey(module.base_address())
    }
}

impl From<&dyn Module> for ModuleKey {
    fn from(module: &dyn Module) -> Self {
        Self::for_module(module)
    }
}

impl From<&minidump::MinidumpModule> for ModuleKey {
    fn from(module: &minidump::MinidumpModule) -> Self {
        Self::for_module(module)
    }
}

struct PerThread<T> {
    inner: CacheMap<std::thread::ThreadId, UnsafeCell<T>>,
}

impl<T> Default for PerThread<T> {
    fn default() -> Self {
        PerThread {
            inner: Default::default(),
        }
    }
}

impl<T: Default> PerThread<T> {
    pub fn with<F, R>(&self, f: F) -> R
    where
        F: FnOnce(&mut T) -> R,
    {
        // # Safety
        // We guarantee unique access to the mutable reference because the values are indexed by
        // thread id: each thread gets its own value which it can freely mutate. We prevent
        // multiple mutable aliases from being created by requiring a callback function.
        f(unsafe { &mut *self.inner.cache_default(std::thread::current().id()).get() })
    }
}

mod object_section_info {
    use framehop::ModuleSectionInfo;
    use object::read::{Object, ObjectSection, ObjectSegment};
    use std::ops::Range;

    #[repr(transparent)]
    pub struct ObjectSectionInfo<'a, O>(pub &'a O);

    impl<'a, O> std::ops::Deref for ObjectSectionInfo<'a, O> {
        type Target = O;

        fn deref(&self) -> &Self::Target {
            self.0
        }
    }

    impl<'data: 'file, 'file, O, D> ModuleSectionInfo<D> for ObjectSectionInfo<'file, O>
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

/// Get the file path with debug information for the given module.
///
/// If `unwind_info` is true, returns the path that should contain unwind information.
fn effective_debug_file(module: &dyn Module, unwind_info: bool) -> PathBuf {
    // Windows x86_64 always stores the unwind info _only_ in the binary.
    let ignore_debug_file = unwind_info && cfg!(all(windows, target_arch = "x86_64"));

    let code_file = module.code_file();
    let code_file_path: &Path = code_file.as_ref().as_ref();

    if !ignore_debug_file {
        if let Some(file) = module.debug_file() {
            let file_path: &Path = file.as_ref().as_ref();
            // Anchor relative paths in the code file parent.
            if file_path.is_relative() {
                if let Some(parent) = code_file_path.parent() {
                    let path = parent.join(file_path);
                    if path.exists() {
                        return path;
                    }
                }
            }
            if file_path.exists() {
                return file_path.to_owned();
            }
        }
        // else fall back to code file below
    }

    code_file_path.to_owned()
}

fn load_unwind_module(
    module: &dyn Module,
    arch: Architecture,
) -> Option<(Mmap, framehop::Module<ModuleData>)> {
    let path = effective_debug_file(module, true);
    let file = match File::open(&path) {
        Ok(file) => file,
        Err(e) => {
            tracing::warn!("failed to open {} for debug info: {e}", path.display());
            return None;
        }
    };
    // # Safety
    // The file is presumably read-only (being some binary or debug info file).
    let mapped = match unsafe { Mmap::map(&file) } {
        Ok(m) => m,
        Err(e) => {
            tracing::error!("failed to map {} for debug info: {e}", path.display());
            return None;
        }
    };

    // # Safety
    // We broaden the lifetime to static, but ensure that the Mmap which provides the data
    // outlives all references.
    let data = unsafe { std::mem::transmute::<&[u8], &'static [u8]>(mapped.as_ref()) };

    let object_data = match object::read::FileKind::parse(data) {
        Err(e) => {
            // If FileKind parsing fails, File parsing will fail too, so bail out.
            tracing::error!("failed to parse file kind for {}: {e}", path.display());
            return None;
        }
        Ok(object::read::FileKind::MachOFat64) => get_fat_macho_data(
            &path,
            data,
            object::read::macho::MachOFatFile64::parse(data),
            arch,
        )?,
        Ok(object::read::FileKind::MachOFat32) => get_fat_macho_data(
            &path,
            data,
            object::read::macho::MachOFatFile32::parse(data),
            arch,
        )?,
        _ => data,
    };

    let objfile = match object::read::File::parse(object_data) {
        Ok(o) => o,
        Err(e) => {
            tracing::error!("failed to parse object file {}: {e}", path.display());
            return None;
        }
    };

    let base = module.base_address();
    let end = base + module.size();
    let fhmodule = framehop::Module::new(
        path.display().to_string(),
        base..end,
        base,
        object_section_info::ObjectSectionInfo(&objfile),
    );

    Some((mapped, fhmodule))
}

fn get_fat_macho_data<'data, Fat: FatArch>(
    path: &Path,
    fatfile_data: &'data [u8],
    result: object::read::Result<object::read::macho::MachOFatFile<'data, Fat>>,
    arch: Architecture,
) -> Option<&'data [u8]> {
    match result {
        Err(e) => {
            tracing::error!("failed to parse fat macho file {}: {e}", path.display());
            None
        }
        Ok(fatfile) => {
            let Some(arch) = fatfile.arches().iter().find(|a| a.architecture() == arch) else {
                tracing::error!(
                    "failed to find object file for {arch:?} architecture in fat macho file {}",
                    path.display()
                );
                return None;
            };
            arch.data(fatfile_data).map_or_else(
                |e| {
                    tracing::error!(
                        "failed to read data from fat macho file {}: {e}",
                        path.display()
                    );
                    None
                },
                Some,
            )
        }
    }
}

impl Default for DebugInfoSymbolProviderBuilder {
    fn default() -> Self {
        DebugInfoSymbolProviderBuilder {
            #[cfg(feature = "debuginfo-symbols")]
            enable_symbols: true,
        }
    }
}

impl DebugInfoSymbolProviderBuilder {
    /// Create a new builder.
    ///
    /// This returns the default builder.
    pub fn new() -> Self {
        Self::default()
    }

    /// Enable or disable symbolication.
    ///
    /// This saves processing time if desired, only doing unwinding if symbols are disabled. This
    /// option is only available when the `wholesym` feature (usually through the `debuginfo`
    /// feature) is enabled, and defaults to `true`.
    #[cfg(feature = "debuginfo-symbols")]
    pub fn symbols(mut self, enable: bool) -> Self {
        self.enable_symbols = enable;
        self
    }

    /// Create the DebugInfoSymbolProvider.
    pub async fn build(
        self,
        system_info: &MinidumpSystemInfo,
        modules: &MinidumpModuleList,
    ) -> DebugInfoSymbolProvider {
        let mut mapped_modules = Vec::new();
        use minidump::system_info::Cpu;
        let (arch, mut unwinder) = match system_info.cpu {
            Cpu::X86_64 => (Architecture::X86_64, UnwinderImpl::x86_64()),
            Cpu::Arm64 => (Architecture::Aarch64, UnwinderImpl::aarch64()),
            _ => unimplemented!(),
        };

        #[cfg(not(feature = "debuginfo-symbols"))]
        let symbols: Box<dyn SymbolInterface + Send + Sync> = Box::new(NoSymbols);

        #[cfg(feature = "debuginfo-symbols")]
        let symbols: Box<dyn SymbolInterface + Send + Sync> = if self.enable_symbols {
            Box::new(wholesym_symbol_interface::Impl::new(modules).await)
        } else {
            Box::new(NoSymbols)
        };

        for module in modules.iter() {
            if let Some((mapped, fhmodule)) = load_unwind_module(module, arch) {
                mapped_modules.push(mapped);
                unwinder.add_module(fhmodule);
            }
        }
        DebugInfoSymbolProvider {
            unwinder,
            symbols,
            _mapped_modules: mapped_modules.into(),
        }
    }
}

impl DebugInfoSymbolProvider {
    /// Create a builder for the DebugInfoSymbolProvider.
    pub fn builder() -> DebugInfoSymbolProviderBuilder {
        Default::default()
    }

    /// Create a new DebugInfoSymbolProvider with the default builder settings.
    pub async fn new(system_info: &MinidumpSystemInfo, modules: &MinidumpModuleList) -> Self {
        Self::builder().build(system_info, modules).await
    }
}

#[async_trait]
impl super::SymbolProvider for DebugInfoSymbolProvider {
    async fn fill_symbol(
        &self,
        module: &(dyn Module + Sync),
        frame: &mut (dyn FrameSymbolizer + Send),
    ) -> Result<(), FillSymbolError> {
        self.symbols.fill_symbol(module, frame).await
    }

    async fn walk_frame(
        &self,
        _module: &(dyn Module + Sync),
        walker: &mut (dyn FrameWalker + Send),
    ) -> Option<()> {
        self.unwinder.unwind_frame(walker)
    }

    async fn get_file_path(
        &self,
        module: &(dyn Module + Sync),
        file_kind: FileKind,
    ) -> Result<PathBuf, FileError> {
        let path = match file_kind {
            FileKind::BreakpadSym => None,
            FileKind::Binary => Some(PathBuf::from(module.code_file().as_ref())),
            FileKind::ExtraDebugInfo => module.debug_file().map(|p| PathBuf::from(p.as_ref())),
        };
        match path {
            Some(path) if path.exists() => Ok(path),
            _ => Err(FileError::NotFound),
        }
    }
}
