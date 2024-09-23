//! This module defines the interface used by minidump-unwind to symbolize stack traces.
//!
//! minidump-unwind uses a series of traits to represent symbolizing functionality and interfaces:
//!
//! * [SymbolProvider][] - provides symbolication, cfi evaluation, and debug statistics
//!     * Implemented by [Symbolizer][] and [debuginfo::DebugInfoSymbolProvider][] (requires the
//!       `debuginfo` feature to be enabled).
//!
//! * [SymbolSupplier][] - maps a [Module][] to a [SymbolFile][]
//!     * minidump-unwind does not directly use this, it's just there so the Symbolizer can
//!       generically handle different symbol fetching strategies.
//!
//! * [FrameSymbolizer][] - callbacks that symbolication uses to return its results.
//!     * Implemented by [StackFrame][crate::StackFrame]
//!     * Implemented by DummyFrame (private, for a stack scanning heuristic)
//! * [FrameWalker][] - callbacks that cfi eval uses to read callee state and write caller state.
//!     * Implemented by CfiStackWalker (private)
//!
//!
//! The following concrete functions are provided to allow configuration of the symbol fetching
//! strategy:
//!
//! * [http_symbol_supplier][] - a [SymbolSupplier][] that can find symbols over HTTP (and cache).
//!   Requires the `http` feature to be enabled.
//! * [simple_symbol_supplier][] - a [SymbolSupplier][] that can find symbols on disk.
//! * [string_symbol_supplier][] - a mock [SymbolSupplier][] for tests.
//!
//!
//! The following concrete types are provided:
//!
//! * [Symbolizer][] - the main interface of the symbolizer, implementing [SymbolProvider][].
//!     * Wraps the [SymbolSupplier][] implementation that is selected.
//!     * Queries the [SymbolSupplier] and manages the SymbolFiles however it pleases.
//! * [SymbolStats][] - debug statistic output.
//! * [SymbolFile][] - part of [LocateSymbolsResult][] that a [SymbolProvider][] returns to the
//!   Symbolizer.
//!     * Never handled by minidump-unwind, public for the trait.
//! * [SymbolError][] - possible errors a [SymbolProvider][] can yield.
//!     * Never handled by minidump-unwind, public for the trait.
//! * [FillSymbolError][] - possible errors for `fill_symbol`.
//!     * While this *is* handled by minidump-unwind, it doesn't actually look at the value. It's
//!       just there to be an Error type for the sake of API design.
//! * [LocateSymbolsResult][] - a payload that a [SymbolProvider][] returns to the Symbolizer.
//!     * Never handled by minidump-unwind, public for the trait.
//! * [DebugInfoResult][] - part of [LocateSymbolsResult][] that a [SymbolProvider][] returns to
//!   the Symbolizer.
//!     * Never handled by minidump-unwind, public for the trait.

use std::collections::HashMap;
use std::path::PathBuf;

use async_trait::async_trait;
use minidump::Module;

pub use breakpad_symbols::{
    DebugInfoResult, FileError, FileKind, FillSymbolError, FrameSymbolizer, FrameWalker,
    LocateSymbolsResult, PendingSymbolStats, SymbolError, SymbolFile, SymbolStats, SymbolSupplier,
    Symbolizer,
};

#[cfg(feature = "debuginfo-unwind")]
pub mod debuginfo;

/// The [`SymbolProvider`] is the main extension point for minidump processing.
///
/// It is primarily used by the `process_minidump` function to do stack
/// unwinding via CFI (call frame information) of a [`Module`] using the
/// `walk_frame` function.
///
/// The `fill_symbol` function is responsible for filling in the source location
/// (function, file, line triple) corresponding to an instruction address, as
/// well as a dual purpose of informing the stack scanning heuristic whether a
/// given instruction address might be valid inside of a [`Module`].
///
/// All the asynchronous trait methods can be called concurrently and need to
/// handle synchronization and request coalescing (based on the [`Module`]).
#[async_trait]
pub trait SymbolProvider {
    /// Fill symbol information in [`FrameSymbolizer`] using the instruction
    /// address from `frame`, and the module information from [`Module`].
    ///
    /// An Error indicates that no symbols could be found for the relevant
    /// module.
    ///
    /// This is used for filling in the resulting source location of the
    /// frame as a (function, file, line) triple, as well as providing the
    /// `parameter_size` which is used during CFI evaluation and stack walking.
    ///
    /// This function also serves a dual purpose in informing the stack scanning
    /// heuristic whether a potential instruction address points to a valid
    /// function or not.
    async fn fill_symbol(
        &self,
        module: &(dyn Module + Sync),
        frame: &mut (dyn FrameSymbolizer + Send),
    ) -> Result<(), FillSymbolError>;

    /// Tries to use CFI to walk the stack frame of the [`FrameWalker`]
    /// using the symbols of the given [`Module`].
    ///
    /// Output should be written using the [`FrameWalker`]'s `set_caller_*` APIs.
    async fn walk_frame(
        &self,
        module: &(dyn Module + Sync),
        walker: &mut (dyn FrameWalker + Send),
    ) -> Option<()>;

    /// Gets the path to the binary code file for a given module (or an Error).
    ///
    /// This might be used later on to inspect the assembly instructions of
    /// a module.
    async fn get_file_path(
        &self,
        module: &(dyn Module + Sync),
        file_kind: FileKind,
    ) -> Result<PathBuf, FileError>;

    /// Collect various statistics on the symbols.
    ///
    /// Keys are implementation dependent.
    /// For example the file name of the module (code_file's file name).
    ///
    /// This is only really intended to be queried after processing an
    /// entire minidump, and may have non-trivial overhead to compute.
    /// It's als possible we'd want it to also be able to contain stats
    /// that don't really make sense in intermediate states.
    ///
    /// In a world where you might want to have one SymbolSupplier shared
    /// by multiple instances of `process` running in parallel, it's unclear
    /// if this is the right abstraction. Perhaps we should have some kind
    /// of "session" abstraction so you can get stats about each individual
    /// processing task? Of course all pooling/caching between the tasks
    /// muddies things too.
    fn stats(&self) -> HashMap<String, SymbolStats> {
        HashMap::new()
    }

    /// Collect various pending statistics on the symbols.
    ///
    /// This is intended to be queried during processing to give some
    /// interactive feedback to the user, and so is fine to poll as
    /// much as you want, whenever you want.
    fn pending_stats(&self) -> PendingSymbolStats {
        PendingSymbolStats::default()
    }
}

#[async_trait]
impl SymbolProvider for &(dyn SymbolProvider + Sync) {
    async fn fill_symbol(
        &self,
        module: &(dyn Module + Sync),
        frame: &mut (dyn FrameSymbolizer + Send),
    ) -> Result<(), FillSymbolError> {
        (*self).fill_symbol(module, frame).await
    }

    async fn walk_frame(
        &self,
        module: &(dyn Module + Sync),
        walker: &mut (dyn FrameWalker + Send),
    ) -> Option<()> {
        (*self).walk_frame(module, walker).await
    }

    async fn get_file_path(
        &self,
        module: &(dyn Module + Sync),
        file_kind: FileKind,
    ) -> Result<PathBuf, FileError> {
        (*self).get_file_path(module, file_kind).await
    }

    fn stats(&self) -> HashMap<String, SymbolStats> {
        (*self).stats()
    }

    fn pending_stats(&self) -> PendingSymbolStats {
        (*self).pending_stats()
    }
}

#[derive(Default)]
pub struct MultiSymbolProvider {
    providers: Vec<Box<dyn SymbolProvider + Send + Sync>>,
}

impl MultiSymbolProvider {
    pub fn new() -> MultiSymbolProvider {
        Default::default()
    }

    pub fn add(&mut self, provider: Box<dyn SymbolProvider + Send + Sync>) {
        self.providers.push(provider);
    }
}

#[async_trait]
impl SymbolProvider for MultiSymbolProvider {
    async fn fill_symbol(
        &self,
        module: &(dyn Module + Sync),
        frame: &mut (dyn FrameSymbolizer + Send),
    ) -> Result<(), FillSymbolError> {
        // Return Ok if *any* symbol provider came back with Ok, so that the user can
        // distinguish between having no symbols at all and just not being able to
        // symbolize this particular frame.
        let mut best_result = Err(FillSymbolError {});
        for p in self.providers.iter() {
            let new_result = p.fill_symbol(module, frame).await;
            best_result = best_result.or(new_result);
        }
        best_result
    }

    async fn walk_frame(
        &self,
        module: &(dyn Module + Sync),
        walker: &mut (dyn FrameWalker + Send),
    ) -> Option<()> {
        for p in self.providers.iter() {
            let result = p.walk_frame(module, walker).await;
            if result.is_some() {
                return result;
            }
        }
        None
    }

    async fn get_file_path(
        &self,
        module: &(dyn Module + Sync),
        file_kind: FileKind,
    ) -> Result<PathBuf, FileError> {
        // Return Ok if *any* symbol provider came back with Ok
        let mut best_result = Err(FileError::NotFound);
        for p in self.providers.iter() {
            let new_result = p.get_file_path(module, file_kind).await;
            best_result = best_result.or(new_result);
        }
        best_result
    }

    fn stats(&self) -> HashMap<String, SymbolStats> {
        let mut result = HashMap::new();
        for p in self.providers.iter() {
            // FIXME: do more intelligent merging of the stats
            // (currently doesn't matter as only one provider reports non-empty stats).
            result.extend(p.stats());
        }
        result
    }

    fn pending_stats(&self) -> PendingSymbolStats {
        let mut result = PendingSymbolStats::default();
        for p in self.providers.iter() {
            // FIXME: do more intelligent merging of the stats
            // (currently doesn't matter as only one provider reports non-empty stats).
            result = p.pending_stats();
        }
        result
    }
}

#[async_trait]
impl SymbolProvider for Symbolizer {
    async fn fill_symbol(
        &self,
        module: &(dyn Module + Sync),
        frame: &mut (dyn FrameSymbolizer + Send),
    ) -> Result<(), FillSymbolError> {
        self.fill_symbol(module, frame).await
    }
    async fn walk_frame(
        &self,
        module: &(dyn Module + Sync),
        walker: &mut (dyn FrameWalker + Send),
    ) -> Option<()> {
        self.walk_frame(module, walker).await
    }
    async fn get_file_path(
        &self,
        module: &(dyn Module + Sync),
        file_kind: FileKind,
    ) -> Result<PathBuf, FileError> {
        self.get_file_path(module, file_kind).await
    }
    fn stats(&self) -> HashMap<String, SymbolStats> {
        self.stats()
    }
    fn pending_stats(&self) -> PendingSymbolStats {
        self.pending_stats()
    }
}

/// Gets a SymbolSupplier that looks up symbols by path or with urls.
///
/// * `symbols_paths` is a list of paths to check for symbol files. Paths
///   are searched in order until one returns a payload. If none do, then
///   urls are used.
///
/// * `symbols_urls` is a list of "base urls" that should all point to Tecken
///   servers. urls are queried in order until one returns a payload. If none
///   do, then it's an error.
///
/// * `symbols_cache` is a directory where an on-disk cache should be located.
///   This should be assumed to be a "temp" directory that another process
///   you don't control is garbage-collecting old files from (to provide an LRU cache).
///   The cache is queried before paths and urls (otherwise it wouldn't be much of a cache).
///
/// * `symbols_tmp` is a directory where symbol files should be downloaded to
///   before atomically swapping them into the cache. Has the same "temp"
///   assumptions as symbols_cache.
///
/// * `timeout` a maximum time limit for a symbol file download. This
///   is primarily defined to avoid getting stuck on buggy infinite downloads.
///   As of this writing, minidump-stackwalk defaults this to 1000 seconds. In
///   the event of a timeout, the supplier may still try to parse the truncated
///   download.
#[cfg(feature = "http")]
pub fn http_symbol_supplier(
    symbol_paths: Vec<PathBuf>,
    symbol_urls: Vec<String>,
    symbols_cache: PathBuf,
    symbols_tmp: PathBuf,
    timeout: std::time::Duration,
) -> impl SymbolSupplier {
    breakpad_symbols::HttpSymbolSupplier::new(
        symbol_urls,
        symbols_cache,
        symbols_tmp,
        symbol_paths,
        timeout,
    )
}

/// Gets a SymbolSupplier that looks up symbols by path.
///
/// Paths are queried in order until one returns a payload.
pub fn simple_symbol_supplier(symbol_paths: Vec<PathBuf>) -> impl SymbolSupplier {
    breakpad_symbols::SimpleSymbolSupplier::new(symbol_paths)
}

/// Gets a mock SymbolSupplier that just maps module names
/// to a string containing an entire breakpad .sym file, for tests.
pub fn string_symbol_supplier(modules: HashMap<String, String>) -> impl SymbolSupplier {
    breakpad_symbols::StringSymbolSupplier::new(modules)
}
