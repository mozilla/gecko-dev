// Copyright 2015 Ted Mielczarek. See the COPYRIGHT
// file at the top-level directory of this distribution.

//! A library for working with [Google Breakpad][breakpad]'s
//! text-format [symbol files][symbolfiles].
//!
//! See the [walker][] module for documentation on CFI evaluation.
//!
//! The highest-level API provided by this crate is to use the
//! [`Symbolizer`][symbolizer] struct.
//!
//! [breakpad]: https://chromium.googlesource.com/breakpad/breakpad/+/master/
//! [symbolfiles]: https://chromium.googlesource.com/breakpad/breakpad/+/master/docs/symbol_files.md
//! [symbolizer]: struct.Symbolizer.html
//!
//! # Examples
//!
//! ```
//! # std::env::set_current_dir(env!("CARGO_MANIFEST_DIR"));
//! use breakpad_symbols::{SimpleSymbolSupplier, Symbolizer, SimpleFrame, SimpleModule};
//! use debugid::DebugId;
//! use std::path::PathBuf;
//! use std::str::FromStr;
//!
//! #[tokio::main]
//! async fn main() {
//!     let paths = vec!(PathBuf::from("../testdata/symbols/"));
//!     let supplier = SimpleSymbolSupplier::new(paths);
//!     let symbolizer = Symbolizer::new(supplier);
//!
//!     // Simple function name lookup with debug file, debug id, address.
//!     let debug_id = DebugId::from_str("5A9832E5287241C1838ED98914E9B7FF1").unwrap();
//!     assert_eq!(symbolizer.get_symbol_at_address("test_app.pdb", debug_id, 0x1010)
//!         .await
//!         .unwrap(),
//!         "vswprintf");
//! }
//! ```

use async_trait::async_trait;
use cachemap2::CacheMap;
use debugid::{CodeId, DebugId};
use futures_util::lock::Mutex as FutMutex;
use tracing::trace;

use std::collections::HashMap;
use std::fs;
use std::path::PathBuf;
use std::sync::Mutex;
use std::{borrow::Cow, sync::Arc};

pub use minidump_common::{traits::Module, utils::basename};
pub use sym_file::walker;

pub use crate::sym_file::{CfiRules, SymbolFile};

#[cfg(feature = "http")]
pub mod http;
mod sym_file;

#[cfg(feature = "http")]
pub use http::*;

// Re-exports for the purposes of the cfi_eval fuzzer. Not public API.
#[doc(hidden)]
#[cfg(feature = "fuzz")]
pub mod fuzzing_private_exports {
    pub use crate::sym_file::walker::{eval_win_expr_for_fuzzer, walk_with_stack_cfi};
    pub use crate::sym_file::{StackInfoWin, WinStackThing};
}

/// Statistics on the symbols of a module.
#[derive(Default, Debug, Clone)]
pub struct SymbolStats {
    /// If the module's symbols were downloaded, this is the url used.
    pub symbol_url: Option<String>,
    /// If the symbols were found and loaded into memory.
    pub loaded_symbols: bool,
    /// If we tried to parse the symbols, but failed.
    pub corrupt_symbols: bool,
    /// If the module's debug info had to be looked up, this is the debug info used.
    pub extra_debug_info: Option<DebugInfoResult>,
}

/// Statistics on pending symbols.
///
/// Fetched with [`Symbolizer::pending_stats`].
#[derive(Default, Debug, Clone)]
pub struct PendingSymbolStats {
    /// The number of symbols we have finished processing
    /// (could be either successful or not, either way is fine).
    pub symbols_processed: u64,
    /// The number of symbols we have been asked to process.
    pub symbols_requested: u64,
}

/// A `Module` implementation that holds arbitrary data.
///
/// This can be useful for getting symbols for a module when you
/// have a debug id and filename but not an actual minidump. If you have a
/// minidump, you should be using [`MinidumpModule`][minidumpmodule].
///
/// [minidumpmodule]: ../minidump/struct.MinidumpModule.html
#[derive(Default)]
pub struct SimpleModule {
    pub base_address: Option<u64>,
    pub size: Option<u64>,
    pub code_file: Option<String>,
    pub code_identifier: Option<CodeId>,
    pub debug_file: Option<String>,
    pub debug_id: Option<DebugId>,
    pub version: Option<String>,
}

impl SimpleModule {
    /// Create a `SimpleModule` with the given `debug_file` and `debug_id`.
    ///
    /// Uses `default` for the remaining fields.
    pub fn new(debug_file: &str, debug_id: DebugId) -> SimpleModule {
        SimpleModule {
            debug_file: Some(String::from(debug_file)),
            debug_id: Some(debug_id),
            ..SimpleModule::default()
        }
    }

    /// Create a `SimpleModule` with `debug_file`, `debug_id`, `code_file`, and `code_identifier`.
    ///
    /// Uses `default` for the remaining fields.
    pub fn from_basic_info(
        debug_file: Option<String>,
        debug_id: Option<DebugId>,
        code_file: Option<String>,
        code_identifier: Option<CodeId>,
    ) -> SimpleModule {
        SimpleModule {
            debug_file,
            debug_id,
            code_file,
            code_identifier,
            ..SimpleModule::default()
        }
    }
}

impl Module for SimpleModule {
    fn base_address(&self) -> u64 {
        self.base_address.unwrap_or(0)
    }
    fn size(&self) -> u64 {
        self.size.unwrap_or(0)
    }
    fn code_file(&self) -> Cow<str> {
        self.code_file
            .as_ref()
            .map_or(Cow::from(""), |s| Cow::Borrowed(&s[..]))
    }
    fn code_identifier(&self) -> Option<CodeId> {
        self.code_identifier.as_ref().cloned()
    }
    fn debug_file(&self) -> Option<Cow<str>> {
        self.debug_file.as_ref().map(|s| Cow::Borrowed(&s[..]))
    }
    fn debug_identifier(&self) -> Option<DebugId> {
        self.debug_id
    }
    fn version(&self) -> Option<Cow<str>> {
        self.version.as_ref().map(|s| Cow::Borrowed(&s[..]))
    }
}

/// Like `PathBuf::file_name`, but try to work on Windows or POSIX-style paths.
fn leafname(path: &str) -> &str {
    path.rsplit(['/', '\\']).next().unwrap_or(path)
}

/// If `filename` ends with `match_extension`, remove it. Append `new_extension` to the result.
fn replace_or_add_extension(filename: &str, match_extension: &str, new_extension: &str) -> String {
    let mut bits = filename.split('.').collect::<Vec<_>>();
    if bits.len() > 1
        && bits
            .last()
            .map_or(false, |e| e.to_lowercase() == match_extension)
    {
        bits.pop();
    }
    bits.push(new_extension);
    bits.join(".")
}

/// A lookup we would like to perform for some file (sym, exe, pdb, dll, ...)
#[derive(Debug, Clone)]
pub struct FileLookup {
    pub debug_id: String,
    pub debug_file: String,
    pub cache_rel: String,
    pub server_rel: String,
}

/// Get a relative symbol path at which to locate symbols for `module`.
///
/// Symbols are generally stored in the layout used by Microsoft's symbol
/// server and associated tools:
/// `<debug filename>/<debug identifier>/<debug filename>.sym`. If
/// `debug filename` ends with *.pdb* the leaf filename will have that
/// removed.
/// `extension` is the expected extension for the symbol filename, generally
/// *sym* if Breakpad text format symbols are expected.
///
/// The debug filename and debug identifier can be found in the
/// [first line][module_line] of the symbol file output by the dump_syms tool.
/// You can use [this script][packagesymbols] to run dump_syms and put the
/// resulting symbol files in the proper directory structure.
///
/// [module_line]: https://chromium.googlesource.com/breakpad/breakpad/+/master/docs/symbol_files.md#MODULE-records
/// [packagesymbols]: https://gist.github.com/luser/2ad32d290f224782fcfc#file-packagesymbols-py
pub fn breakpad_sym_lookup(module: &(dyn Module + Sync)) -> Option<FileLookup> {
    let debug_file = module.debug_file()?;
    let debug_id = module.debug_identifier()?;

    let leaf = leafname(&debug_file);
    let filename = replace_or_add_extension(leaf, "pdb", "sym");
    let rel_path = [leaf, &debug_id.breakpad().to_string(), &filename[..]].join("/");
    Some(FileLookup {
        cache_rel: rel_path.clone(),
        server_rel: rel_path,
        debug_id: debug_id.breakpad().to_string(),
        debug_file: filename,
    })
}

/// Get a relative symbol path at which to locate symbols for `module` using
/// the code file and code identifier. This is helpful for Microsoft modules
/// where we don't have a valid debug filename and debug id to retrieve the
/// symbol file with and the symbol server supports looking up debug filename
/// and debug id using the code file and code id.
///
/// If `code file` ends with *.dll* the leaf filename will have that removed.
/// `extension` is the expected extension for the symbol filename, generally
/// *sym* if Breakpad text format symbols are expected.
///
/// `<code file>/<code identifier>/<code file>.sym`
pub fn code_info_breakpad_sym_lookup(module: &(dyn Module + Sync)) -> Option<String> {
    let code_file = module.code_file();
    let code_identifier = module.code_identifier()?;

    if code_file.is_empty() {
        return None;
    }
    let leaf = leafname(&code_file);
    let filename = replace_or_add_extension(leaf, "dll", "sym");
    let rel_path = [
        leaf,
        &code_identifier.to_string().to_uppercase(),
        &filename[..],
    ]
    .join("/");

    Some(rel_path)
}

/// Returns a lookup for this module's extra debuginfo (pdb)
pub fn extra_debuginfo_lookup(module: &(dyn Module + Sync)) -> Option<FileLookup> {
    let debug_file = module.debug_file()?;
    let debug_id = module.debug_identifier()?;

    let leaf = leafname(&debug_file);
    let rel_path = [leaf, &debug_id.breakpad().to_string(), leaf].join("/");
    Some(FileLookup {
        cache_rel: rel_path.clone(),
        server_rel: rel_path,
        debug_id: debug_id.to_string(),
        debug_file: leaf.to_string(),
    })
}

/// Returns a lookup for this module's binary (exe, dll, so, dylib, ...)
pub fn binary_lookup(module: &(dyn Module + Sync)) -> Option<FileLookup> {
    // NOTE: to make dump_syms happy we're currently moving the bin
    // to be next to the pdb. This changes where we would naively put it,
    // hence the two different paths!

    let code_file = module.code_file();
    let code_id = module.code_identifier()?;
    let debug_file = module.debug_file()?;
    let debug_id = module.debug_identifier()?;

    let bin_leaf = leafname(&code_file);
    let debug_leaf = leafname(&debug_file);

    Some(FileLookup {
        cache_rel: [debug_leaf, &debug_id.breakpad().to_string(), bin_leaf].join("/"),
        server_rel: [bin_leaf, code_id.as_ref(), bin_leaf].join("/"),
        debug_id: debug_id.to_string(),
        debug_file: debug_file.to_string(),
    })
}

/// Mangles a lookup to mozilla's format where the last char is replaced by an underscore
/// (and the file is wrapped in a CAB, but dump_syms handles that transparently).
pub fn moz_lookup(mut lookup: FileLookup) -> FileLookup {
    lookup.server_rel.pop().unwrap();
    lookup.server_rel.push('_');
    lookup
}

pub fn lookup(module: &(dyn Module + Sync), file_kind: FileKind) -> Option<FileLookup> {
    match file_kind {
        FileKind::BreakpadSym => breakpad_sym_lookup(module),
        FileKind::Binary => binary_lookup(module),
        FileKind::ExtraDebugInfo => extra_debuginfo_lookup(module),
    }
}

/// Possible results of locating symbols for a module.
///
/// Because symbols may be found from different sources, symbol providers
/// are usually configured to "cascade" into the next one whenever they report
/// `NotFound`.
///
/// Cascading currently assumes that if any provider finds symbols for
/// a module, all other providers will find the same symbols (if any).
/// Therefore cascading will not be applied if a LoadError or ParseError
/// occurs (because presumably, all the other sources will also fail to
/// load/parse.)
///
/// In theory we could do some interesting things where we attempt to
/// be more robust and actually merge together the symbols from multiple
/// sources, but that would make it difficult to cache symbol files, and
/// would rarely actually improve results.
///
/// Since symbol files can be on the order of a gigabyte(!) and downloaded
/// from the network, aggressive caching is pretty important. The current
/// approach is a nice balance of simple and effective.
#[derive(Debug, thiserror::Error)]
pub enum SymbolError {
    /// Symbol file could not be found.
    ///
    /// In this case other symbol providers may still be able to find it!
    #[error("symbol file not found")]
    NotFound,
    /// The module was lacking either the debug file or debug id, as such the
    /// path of the symbol could not be generated.
    #[error("the debug file or id were missing")]
    MissingDebugFileOrId,
    /// Symbol file could not be loaded into memory.
    #[error("couldn't read input stream")]
    LoadError(#[from] std::io::Error),
    /// Symbol file was too corrupt to be parsed at all.
    ///
    /// Because symbol files are pretty modular, many corruptions/ambiguities
    /// can be either repaired or discarded at a fairly granular level
    /// (e.g. a bad STACK WIN line can be discarded without affecting anything
    /// else). But sometimes we can't make any sense of the symbol file, and
    /// you find yourself here.
    #[error("parse error: {0} at line {1}")]
    ParseError(&'static str, u64),
}

#[derive(Clone, Debug, thiserror::Error)]
pub enum FileError {
    #[error("file not found")]
    NotFound,
}

/// An error produced by fill_symbol.
#[derive(Debug)]
pub struct FillSymbolError {
    // We don't want to yield a full SymbolError for fill_symbol
    // as this would involve cloning bulky Error strings every time
    // someone requested symbols for a missing module.
    //
    // As it turns out there's currently no reason to care about *why*
    // fill_symbol, so for now this is just a dummy type until we have
    // something to put here.
    //
    // The only reason fill_symbol *can* produce an Err is so that
    // the caller can distinguish between "we had symbols, but this address
    // didn't map to a function name" and "we had no symbols for that module"
    // (this is used as a heuristic for stack scanning).
}

impl PartialEq for SymbolError {
    fn eq(&self, other: &SymbolError) -> bool {
        matches!(
            (self, other),
            (SymbolError::NotFound, SymbolError::NotFound)
                | (SymbolError::LoadError(_), SymbolError::LoadError(_))
                | (SymbolError::ParseError(..), SymbolError::ParseError(..))
        )
    }
}

/// The result of a lookup by code_file/code_identifier against a symbol
/// server.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DebugInfoResult {
    pub debug_file: String,
    pub debug_identifier: DebugId,
}

/// The result of locating symbols, with debug info if it had to be looked up.
#[derive(Debug, PartialEq, Eq)]
pub struct LocateSymbolsResult {
    pub symbols: SymbolFile,
    pub extra_debug_info: Option<DebugInfoResult>,
}

/// A trait for things that can locate symbols for a given module.
#[async_trait]
pub trait SymbolSupplier {
    /// Locate and load a symbol file for `module`.
    ///
    /// Implementations may use any strategy for locating and loading
    /// symbols.
    async fn locate_symbols(
        &self,
        module: &(dyn Module + Sync),
    ) -> Result<LocateSymbolsResult, SymbolError>;

    /// Locate a specific file associated with a `module`
    ///
    /// Implementations may use any strategy for locating and loading
    /// symbols.
    async fn locate_file(
        &self,
        module: &(dyn Module + Sync),
        file_kind: FileKind,
    ) -> Result<PathBuf, FileError>;
}

/// An implementation of `SymbolSupplier` that loads Breakpad text-format symbols from local disk
/// paths.
///
/// See [`breakpad_sym_lookup`] for details on how paths are searched.
pub struct SimpleSymbolSupplier {
    /// Local disk paths in which to search for symbols.
    paths: Vec<PathBuf>,
}

impl SimpleSymbolSupplier {
    /// Instantiate a new `SimpleSymbolSupplier` that will search in `paths`.
    pub fn new(paths: Vec<PathBuf>) -> SimpleSymbolSupplier {
        SimpleSymbolSupplier { paths }
    }
}

#[async_trait]
impl SymbolSupplier for SimpleSymbolSupplier {
    #[tracing::instrument(name = "symbols", level = "trace", skip_all, fields(module = crate::basename(&module.code_file())))]
    async fn locate_symbols(
        &self,
        module: &(dyn Module + Sync),
    ) -> Result<LocateSymbolsResult, SymbolError> {
        let file_path = self
            .locate_file(module, FileKind::BreakpadSym)
            .await
            .map_err(|_| SymbolError::NotFound)?;
        let symbols = SymbolFile::from_file(&file_path).map_err(|e| {
            trace!("SimpleSymbolSupplier failed: {}", e);
            e
        })?;
        trace!("SimpleSymbolSupplier parsed file!");
        Ok(LocateSymbolsResult {
            symbols,
            extra_debug_info: None,
        })
    }

    #[tracing::instrument(level = "trace", skip(self, module), fields(module = crate::basename(&module.code_file())))]
    async fn locate_file(
        &self,
        module: &(dyn Module + Sync),
        file_kind: FileKind,
    ) -> Result<PathBuf, FileError> {
        trace!("SimpleSymbolSupplier search");
        if let Some(lookup) = lookup(module, file_kind) {
            for path in self.paths.iter() {
                if path.is_file() && file_kind == FileKind::BreakpadSym {
                    if let Ok(sf) = SymbolFile::from_file(path) {
                        if sf.module_id == lookup.debug_id {
                            trace!("SimpleSymbolSupplier found file {}", path.display());
                            return Ok(path.to_path_buf());
                        }
                    }
                } else if path.is_dir() {
                    let test_path = path.join(lookup.cache_rel.clone());
                    if fs::metadata(&test_path).ok().map_or(false, |m| m.is_file()) {
                        trace!("SimpleSymbolSupplier found file {}", test_path.display());
                        return Ok(test_path);
                    }
                }
            }
        } else {
            trace!("SimpleSymbolSupplier could not build symbol_path");
        }
        Err(FileError::NotFound)
    }
}

/// A SymbolSupplier that maps module names (code_files) to an in-memory string.
///
/// Intended for mocking symbol files in tests.
#[derive(Default, Debug, Clone)]
pub struct StringSymbolSupplier {
    modules: HashMap<String, String>,
    code_info_to_debug_info: HashMap<String, DebugInfoResult>,
}

impl StringSymbolSupplier {
    /// Make a new StringSymbolSupplier with no modules.
    pub fn new(modules: HashMap<String, String>) -> Self {
        Self {
            modules,
            code_info_to_debug_info: HashMap::new(),
        }
    }

    /// Perform a code_file/code_identifier lookup for a specific symbol server.
    async fn lookup_debug_info_by_code_info(
        &self,
        module: &(dyn Module + Sync),
    ) -> Option<DebugInfoResult> {
        let lookup_path = code_info_breakpad_sym_lookup(module)?;
        self.code_info_to_debug_info.get(&lookup_path).cloned()
    }
}

#[async_trait]
impl SymbolSupplier for StringSymbolSupplier {
    #[tracing::instrument(name = "symbols", level = "trace", skip_all, fields(file = crate::basename(&module.code_file())))]
    async fn locate_symbols(
        &self,
        module: &(dyn Module + Sync),
    ) -> Result<LocateSymbolsResult, SymbolError> {
        trace!("StringSymbolSupplier search");
        if let Some(symbols) = self.modules.get(&*module.code_file()) {
            trace!("StringSymbolSupplier found file");
            let file = SymbolFile::from_bytes(symbols.as_bytes())?;
            trace!("StringSymbolSupplier parsed file!");
            return Ok(LocateSymbolsResult {
                symbols: file,
                extra_debug_info: self.lookup_debug_info_by_code_info(module).await,
            });
        }
        trace!("StringSymbolSupplier could not find file");
        Err(SymbolError::NotFound)
    }

    async fn locate_file(
        &self,
        _module: &(dyn Module + Sync),
        _file_kind: FileKind,
    ) -> Result<PathBuf, FileError> {
        // StringSymbolSupplier can never find files, is for testing
        Err(FileError::NotFound)
    }
}

/// A trait for setting symbol information on something like a stack frame.
pub trait FrameSymbolizer {
    /// Get the program counter value for this frame.
    fn get_instruction(&self) -> u64;
    /// Set the name, base address, and parameter size of the function in
    /// which this frame is executing.
    fn set_function(&mut self, name: &str, base: u64, parameter_size: u32);
    /// Set the source file and (1-based) line number this frame represents.
    fn set_source_file(&mut self, file: &str, line: u32, base: u64);
    /// Add an inline frame. This method can be called multiple times, in the
    /// order "outside to inside".
    fn add_inline_frame(&mut self, _name: &str, _file: Option<&str>, _line: Option<u32>) {}
}

pub trait FrameWalker {
    /// Get the instruction address that we're trying to unwind from.
    fn get_instruction(&self) -> u64;
    /// Check whether the callee has a callee of its own.
    fn has_grand_callee(&self) -> bool;
    /// Get the number of bytes the callee's callee's parameters take up
    /// on the stack (or 0 if unknown/invalid). This is needed for
    /// STACK WIN unwinding.
    fn get_grand_callee_parameter_size(&self) -> u32;
    /// Get a register-sized value stored at this address.
    fn get_register_at_address(&self, address: u64) -> Option<u64>;
    /// Get the value of a register from the callee's frame.
    fn get_callee_register(&self, name: &str) -> Option<u64>;
    /// Set the value of a register for the caller's frame.
    fn set_caller_register(&mut self, name: &str, val: u64) -> Option<()>;
    /// Explicitly mark one of the caller's registers as invalid.
    fn clear_caller_register(&mut self, name: &str);
    /// Set whatever registers in the caller should be set based on the cfa (e.g. rsp).
    fn set_cfa(&mut self, val: u64) -> Option<()>;
    /// Set whatever registers in the caller should be set based on the return address (e.g. rip).
    fn set_ra(&mut self, val: u64) -> Option<()>;
}

/// A simple implementation of `FrameSymbolizer` that just holds data.
#[derive(Debug, Default)]
pub struct SimpleFrame {
    /// The program counter value for this frame.
    pub instruction: u64,
    /// The name of the function in which the current instruction is executing.
    pub function: Option<String>,
    /// The offset of the start of `function` from the module base.
    pub function_base: Option<u64>,
    /// The size, in bytes, that this function's parameters take up on the stack.
    pub parameter_size: Option<u32>,
    /// The name of the source file in which the current instruction is executing.
    pub source_file: Option<String>,
    /// The 1-based index of the line number in `source_file` in which the current instruction is
    /// executing.
    pub source_line: Option<u32>,
    /// The offset of the start of `source_line` from the function base.
    pub source_line_base: Option<u64>,
}

impl SimpleFrame {
    /// Instantiate a `SimpleFrame` with instruction pointer `instruction`.
    pub fn with_instruction(instruction: u64) -> SimpleFrame {
        SimpleFrame {
            instruction,
            ..SimpleFrame::default()
        }
    }
}

impl FrameSymbolizer for SimpleFrame {
    fn get_instruction(&self) -> u64 {
        self.instruction
    }
    fn set_function(&mut self, name: &str, base: u64, parameter_size: u32) {
        self.function = Some(String::from(name));
        self.function_base = Some(base);
        self.parameter_size = Some(parameter_size);
    }
    fn set_source_file(&mut self, file: &str, line: u32, base: u64) {
        self.source_file = Some(String::from(file));
        self.source_line = Some(line);
        self.source_line_base = Some(base);
    }
}

/// A type of file related to a module that you might want downloaded.
#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum FileKind {
    /// A Breakpad symbol (.sym) file
    BreakpadSym,
    /// The native binary of a module ("code file") (.exe/.dll/.so/.dylib...)
    Binary,
    /// Extra debuginfo for a module ("debug file") (.pdb/...?)
    ExtraDebugInfo,
}

// Can't make Module derive Hash, since then it can't be used as a trait
// object (because the hash method is generic), so this is a hacky workaround.
/// A key that uniquely identifies a module:
///
/// * code_file
/// * code_id
/// * debug_file
/// * debug_id
type ModuleKey = (String, Option<String>, Option<String>, Option<String>);

/// Helper for deriving a hash key from a `Module` for `Symbolizer`.
fn module_key(module: &(dyn Module + Sync)) -> ModuleKey {
    (
        module.code_file().to_string(),
        module.code_identifier().map(|s| s.to_string()),
        module.debug_file().map(|s| s.to_string()),
        module.debug_identifier().map(|s| s.to_string()),
    )
}

struct CachedAsyncResult<T, E> {
    inner: FutMutex<Option<Arc<Result<T, E>>>>,
}

impl<T, E> Default for CachedAsyncResult<T, E> {
    fn default() -> Self {
        CachedAsyncResult {
            inner: FutMutex::new(None),
        }
    }
}

impl<T, E> CachedAsyncResult<T, E> {
    pub async fn get<'a, F, Fut>(&self, f: F) -> Arc<Result<T, E>>
    where
        F: FnOnce() -> Fut + 'a,
        Fut: std::future::Future<Output = Result<T, E>> + 'a,
    {
        let mut guard = self.inner.lock().await;
        if guard.is_none() {
            *guard = Some(Arc::new(f().await));
        }
        guard.as_ref().unwrap().clone()
    }
}

/// Symbolicate stack frames.
///
/// A `Symbolizer` manages loading symbols and looking up symbols in them
/// including caching so that symbols for a given module are only loaded once.
///
/// Call [`Symbolizer::new`][new] to instantiate a `Symbolizer`. A Symbolizer
/// requires a [`SymbolSupplier`][supplier] to locate symbols. If you have
/// symbols on disk in the [customary directory layout][breakpad_sym_lookup], a
/// [`SimpleSymbolSupplier`][simple] will work.
///
/// Use [`get_symbol_at_address`][get_symbol] or [`fill_symbol`][fill_symbol] to
/// do symbol lookup.
///
/// [new]: struct.Symbolizer.html#method.new
/// [supplier]: trait.SymbolSupplier.html
/// [simple]: struct.SimpleSymbolSupplier.html
/// [get_symbol]: struct.Symbolizer.html#method.get_symbol_at_address
/// [fill_symbol]: struct.Symbolizer.html#method.fill_symbol

pub struct Symbolizer {
    /// Symbol supplier for locating symbols.
    supplier: Box<dyn SymbolSupplier + Send + Sync + 'static>,
    /// Cache of symbol locating results.
    // TODO?: use lru-cache: https://crates.io/crates/lru-cache/
    // note that using an lru-cache would mess up the fact that we currently
    // use this for statistics collection. Splitting out statistics would be
    // way messier but not impossible.
    symbols: CacheMap<ModuleKey, CachedAsyncResult<SymbolFile, SymbolError>>,
    pending_stats: Mutex<PendingSymbolStats>,
    stats: Mutex<HashMap<String, SymbolStats>>,
}

impl Symbolizer {
    /// Create a `Symbolizer` that uses `supplier` to locate symbols.
    pub fn new<T: SymbolSupplier + Send + Sync + 'static>(supplier: T) -> Symbolizer {
        Symbolizer {
            supplier: Box::new(supplier),
            symbols: CacheMap::default(),
            pending_stats: Mutex::default(),
            stats: Mutex::default(),
        }
    }

    /// Helper method for non-minidump-using callers.
    ///
    /// Pass `debug_file` and `debug_id` describing a specific module,
    /// and `address`, a module-relative address, and get back
    /// a symbol in that module that covers that address, or `None`.
    ///
    /// See [the module-level documentation][module] for an example.
    ///
    /// [module]: index.html
    pub async fn get_symbol_at_address(
        &self,
        debug_file: &str,
        debug_id: DebugId,
        address: u64,
    ) -> Option<String> {
        let k = (debug_file, debug_id);
        let mut frame = SimpleFrame::with_instruction(address);
        self.fill_symbol(&k, &mut frame).await.ok()?;
        frame.function
    }

    /// Fill symbol information in `frame` using the instruction address
    /// from `frame`, and the module information from `module`. If you're not
    /// using a minidump module, you can use [`SimpleModule`][simplemodule] and
    /// [`SimpleFrame`][simpleframe].
    ///
    /// An Error indicates that no symbols could be found for the relevant
    /// module.
    ///
    /// # Examples
    ///
    /// ```
    /// # std::env::set_current_dir(env!("CARGO_MANIFEST_DIR"));
    /// use std::str::FromStr;
    /// use debugid::DebugId;
    /// use breakpad_symbols::{SimpleSymbolSupplier,Symbolizer,SimpleFrame,SimpleModule};
    ///
    /// #[tokio::main]
    /// async fn main() {
    ///     use std::path::PathBuf;
    ///     let paths = vec!(PathBuf::from("../testdata/symbols/"));
    ///     let supplier = SimpleSymbolSupplier::new(paths);
    ///     let symbolizer = Symbolizer::new(supplier);
    ///     let debug_id = DebugId::from_str("5A9832E5287241C1838ED98914E9B7FF1").unwrap();
    ///     let m = SimpleModule::new("test_app.pdb", debug_id);
    ///     let mut f = SimpleFrame::with_instruction(0x1010);
    ///     let _ = symbolizer.fill_symbol(&m, &mut f).await;
    ///     assert_eq!(f.function.unwrap(), "vswprintf");
    ///     assert_eq!(f.source_file.unwrap(),
    ///         r"c:\program files\microsoft visual studio 8\vc\include\swprintf.inl");
    ///     assert_eq!(f.source_line.unwrap(), 51);
    /// }
    /// ```
    ///
    /// [simplemodule]: struct.SimpleModule.html
    /// [simpleframe]: struct.SimpleFrame.html
    pub async fn fill_symbol(
        &self,
        module: &(dyn Module + Sync),
        frame: &mut (dyn FrameSymbolizer + Send),
    ) -> Result<(), FillSymbolError> {
        let cached_sym = self.get_symbols(module).await;
        let sym = cached_sym
            .as_ref()
            .as_ref()
            .map_err(|_| FillSymbolError {})?;
        sym.fill_symbol(module, frame);
        Ok(())
    }

    /// Collect various statistics on the symbols.
    ///
    /// Keys are the file name of the module (code_file's file name).
    pub fn stats(&self) -> HashMap<String, SymbolStats> {
        self.stats.lock().unwrap().clone()
    }

    /// Get live symbol stats for interactive updates.
    pub fn pending_stats(&self) -> PendingSymbolStats {
        self.pending_stats.lock().unwrap().clone()
    }

    /// Tries to use CFI to walk the stack frame of the FrameWalker
    /// using the symbols of the given Module. Output will be written
    /// using the FrameWalker's `set_caller_*` APIs.
    pub async fn walk_frame(
        &self,
        module: &(dyn Module + Sync),
        walker: &mut (dyn FrameWalker + Send),
    ) -> Option<()> {
        let cached_sym = self.get_symbols(module).await;
        let sym = cached_sym.as_ref();
        if let Ok(sym) = sym {
            trace!("found symbols for address, searching for cfi entries");
            sym.walk_frame(module, walker)
        } else {
            trace!("couldn't find symbols for address, cannot use cfi");
            None
        }
    }

    /// Gets the fully parsed SymbolFile for a given module (or an Error).
    ///
    /// This returns a CachedOperation which is guaranteed to already be resolved (lifetime stuff).
    async fn get_symbols(
        &self,
        module: &(dyn Module + Sync),
    ) -> Arc<Result<SymbolFile, SymbolError>> {
        self.symbols
            .cache_default(module_key(module))
            .get(|| async {
                trace!("locating symbols for module {}", module.code_file());
                self.pending_stats.lock().unwrap().symbols_requested += 1;
                let result = self.supplier.locate_symbols(module).await;
                self.pending_stats.lock().unwrap().symbols_processed += 1;

                let mut stats = SymbolStats::default();
                match &result {
                    Ok(res) => {
                        stats.symbol_url.clone_from(&res.symbols.url);
                        stats.loaded_symbols = true;
                        stats.corrupt_symbols = false;
                        stats.extra_debug_info.clone_from(&res.extra_debug_info);
                    }
                    Err(SymbolError::NotFound) => {
                        stats.loaded_symbols = false;
                    }
                    Err(SymbolError::MissingDebugFileOrId) => {
                        stats.loaded_symbols = false;
                    }
                    Err(SymbolError::LoadError(_)) => {
                        stats.loaded_symbols = false;
                    }
                    Err(SymbolError::ParseError(..)) => {
                        stats.loaded_symbols = true;
                        stats.corrupt_symbols = true;
                    }
                }
                let key = leafname(module.code_file().as_ref()).to_string();
                self.stats.lock().unwrap().insert(key, stats);

                result.map(|r| r.symbols)
            })
            .await
    }

    /// Gets the path to a file for a given module (or an Error).
    ///
    /// This returns a CachedOperation which is guaranteed to already be resolved (lifetime stuff).
    pub async fn get_file_path(
        &self,
        module: &(dyn Module + Sync),
        file_kind: FileKind,
    ) -> Result<PathBuf, FileError> {
        self.supplier.locate_file(module, file_kind).await
    }
}

#[test]
fn test_leafname() {
    assert_eq!(leafname("c:\\foo\\bar\\test.pdb"), "test.pdb");
    assert_eq!(leafname("c:/foo/bar/test.pdb"), "test.pdb");
    assert_eq!(leafname("test.pdb"), "test.pdb");
    assert_eq!(leafname("test"), "test");
    assert_eq!(leafname("/path/to/test"), "test");
}

#[test]
fn test_replace_or_add_extension() {
    assert_eq!(
        replace_or_add_extension("test.pdb", "pdb", "sym"),
        "test.sym"
    );
    assert_eq!(
        replace_or_add_extension("TEST.PDB", "pdb", "sym"),
        "TEST.sym"
    );
    assert_eq!(replace_or_add_extension("test", "pdb", "sym"), "test.sym");
    assert_eq!(
        replace_or_add_extension("test.x", "pdb", "sym"),
        "test.x.sym"
    );
    assert_eq!(replace_or_add_extension("", "pdb", "sym"), ".sym");
    assert_eq!(replace_or_add_extension("test.x", "x", "y"), "test.y");
}

#[cfg(test)]
mod test {

    use super::*;
    use std::fs::File;
    use std::io::Write;
    use std::path::Path;
    use std::str::FromStr;

    #[tokio::test]
    async fn test_relative_symbol_path() {
        let debug_id = DebugId::from_str("abcd1234-abcd-1234-abcd-abcd12345678-a").unwrap();
        let m = SimpleModule::new("foo.pdb", debug_id);
        assert_eq!(
            &breakpad_sym_lookup(&m).unwrap().cache_rel,
            "foo.pdb/ABCD1234ABCD1234ABCDABCD12345678a/foo.sym"
        );

        let m2 = SimpleModule::new("foo.pdb", debug_id);
        assert_eq!(
            &breakpad_sym_lookup(&m2).unwrap().cache_rel,
            "foo.pdb/ABCD1234ABCD1234ABCDABCD12345678a/foo.sym"
        );

        let m3 = SimpleModule::new("foo.xyz", debug_id);
        assert_eq!(
            &breakpad_sym_lookup(&m3).unwrap().cache_rel,
            "foo.xyz/ABCD1234ABCD1234ABCDABCD12345678a/foo.xyz.sym"
        );

        let m4 = SimpleModule::new("foo.xyz", debug_id);
        assert_eq!(
            &breakpad_sym_lookup(&m4).unwrap().cache_rel,
            "foo.xyz/ABCD1234ABCD1234ABCDABCD12345678a/foo.xyz.sym"
        );

        let bad = SimpleModule::default();
        assert!(breakpad_sym_lookup(&bad).is_none());

        let bad2 = SimpleModule {
            debug_file: Some("foo".to_string()),
            ..SimpleModule::default()
        };
        assert!(breakpad_sym_lookup(&bad2).is_none());

        let bad3 = SimpleModule {
            debug_id: Some(debug_id),
            ..SimpleModule::default()
        };
        assert!(breakpad_sym_lookup(&bad3).is_none());
    }

    #[tokio::test]
    async fn test_relative_symbol_path_abs_paths() {
        let debug_id = DebugId::from_str("abcd1234-abcd-1234-abcd-abcd12345678-a").unwrap();
        {
            let m = SimpleModule::new("/path/to/foo.bin", debug_id);
            assert_eq!(
                &breakpad_sym_lookup(&m).unwrap().cache_rel,
                "foo.bin/ABCD1234ABCD1234ABCDABCD12345678a/foo.bin.sym"
            );
        }

        {
            let m = SimpleModule::new("c:/path/to/foo.pdb", debug_id);
            assert_eq!(
                &breakpad_sym_lookup(&m).unwrap().cache_rel,
                "foo.pdb/ABCD1234ABCD1234ABCDABCD12345678a/foo.sym"
            );
        }

        {
            let m = SimpleModule::new("c:\\path\\to\\foo.pdb", debug_id);
            assert_eq!(
                &breakpad_sym_lookup(&m).unwrap().cache_rel,
                "foo.pdb/ABCD1234ABCD1234ABCDABCD12345678a/foo.sym"
            );
        }
    }

    #[tokio::test]
    async fn test_code_info_breakpad_sym_lookup() {
        // Test normal data
        let m = SimpleModule {
            code_file: Some("foo.dll".to_string()),
            code_identifier: Some(CodeId::from_str("64E782C570C4000").unwrap()),
            ..SimpleModule::default()
        };
        assert_eq!(
            &code_info_breakpad_sym_lookup(&m).unwrap(),
            "foo.dll/64E782C570C4000/foo.sym"
        );

        let bad = SimpleModule::default();
        assert!(code_info_breakpad_sym_lookup(&bad).is_none());

        let bad2 = SimpleModule {
            code_file: Some("foo".to_string()),
            ..SimpleModule::default()
        };
        assert!(code_info_breakpad_sym_lookup(&bad2).is_none());

        let bad3 = SimpleModule {
            code_identifier: Some(CodeId::from_str("64E782C570C4000").unwrap()),
            ..SimpleModule::default()
        };
        assert!(code_info_breakpad_sym_lookup(&bad3).is_none());
    }

    fn mksubdirs(path: &Path, dirs: &[&str]) -> Vec<PathBuf> {
        dirs.iter()
            .map(|dir| {
                let new_path = path.join(dir);
                fs::create_dir(&new_path).unwrap();
                new_path
            })
            .collect()
    }

    fn write_symbol_file(path: &Path, contents: &[u8]) {
        let dir = path.parent().unwrap();
        if !fs::metadata(dir).ok().map_or(false, |m| m.is_dir()) {
            fs::create_dir_all(dir).unwrap();
        }
        let mut f = File::create(path).unwrap();
        f.write_all(contents).unwrap();
    }

    fn write_good_symbol_file(path: &Path) {
        write_symbol_file(path, b"MODULE Linux x86 abcd1234 foo\n");
    }

    fn write_bad_symbol_file(path: &Path) {
        write_symbol_file(path, b"this is not a symbol file\n");
    }

    #[tokio::test]
    async fn test_simple_symbol_supplier() {
        let t = tempfile::tempdir().unwrap();
        let paths = mksubdirs(t.path(), &["one", "two"]);

        let supplier = SimpleSymbolSupplier::new(paths.clone());
        let bad = SimpleModule::default();
        assert_eq!(
            supplier.locate_symbols(&bad).await,
            Err(SymbolError::NotFound)
        );

        // Try loading symbols for each of two modules in each of the two
        // search paths.
        for &(path, file, id, sym) in [
            (
                &paths[0],
                "foo.pdb",
                DebugId::from_str("abcd1234-0000-0000-0000-abcd12345678-a").unwrap(),
                "foo.pdb/ABCD1234000000000000ABCD12345678a/foo.sym",
            ),
            (
                &paths[1],
                "bar.xyz",
                DebugId::from_str("ff990000-0000-0000-0000-abcd12345678-a").unwrap(),
                "bar.xyz/FF990000000000000000ABCD12345678a/bar.xyz.sym",
            ),
        ]
        .iter()
        {
            let m = SimpleModule::new(file, id);
            // No symbols present yet.
            assert_eq!(
                supplier.locate_symbols(&m).await,
                Err(SymbolError::NotFound)
            );
            write_good_symbol_file(&path.join(sym));
            // Should load OK now that it exists.
            assert!(
                supplier.locate_symbols(&m).await.is_ok(),
                "{}",
                format!("Located symbols for {sym}")
            );
        }

        // Write a malformed symbol file, verify that it's found but fails to load.
        let debug_id = DebugId::from_str("ffff0000-0000-0000-0000-abcd12345678-a").unwrap();
        let mal = SimpleModule::new("baz.pdb", debug_id);
        let sym = "baz.pdb/FFFF0000000000000000ABCD12345678a/baz.sym";
        assert_eq!(
            supplier.locate_symbols(&mal).await,
            Err(SymbolError::NotFound)
        );
        write_bad_symbol_file(&paths[0].join(sym));
        let res = supplier.locate_symbols(&mal).await;
        assert!(
            matches!(res, Err(SymbolError::ParseError(..))),
            "{}",
            format!("Correctly failed to parse {sym}, result: {res:?}")
        );
    }

    #[tokio::test]
    async fn test_symbolizer() {
        let t = tempfile::tempdir().unwrap();
        let path = t.path();

        // TODO: This could really use a MockSupplier
        let supplier = SimpleSymbolSupplier::new(vec![PathBuf::from(path)]);
        let symbolizer = Symbolizer::new(supplier);
        let debug_id = DebugId::from_str("abcd1234-abcd-1234-abcd-abcd12345678-a").unwrap();
        let m1 = SimpleModule::new("foo.pdb", debug_id);
        write_symbol_file(
            &path.join("foo.pdb/ABCD1234ABCD1234ABCDABCD12345678a/foo.sym"),
            b"MODULE Linux x86 ABCD1234ABCD1234ABCDABCD12345678a foo
FILE 1 foo.c
FUNC 1000 30 10 some func
1000 30 100 1
",
        );
        let mut f1 = SimpleFrame::with_instruction(0x1010);
        symbolizer.fill_symbol(&m1, &mut f1).await.unwrap();
        assert_eq!(f1.function.unwrap(), "some func");
        assert_eq!(f1.function_base.unwrap(), 0x1000);
        assert_eq!(f1.source_file.unwrap(), "foo.c");
        assert_eq!(f1.source_line.unwrap(), 100);
        assert_eq!(f1.source_line_base.unwrap(), 0x1000);

        assert_eq!(
            symbolizer
                .get_symbol_at_address("foo.pdb", debug_id, 0x1010)
                .await
                .unwrap(),
            "some func"
        );

        let debug_id = DebugId::from_str("ffff0000-0000-0000-0000-abcd12345678-a").unwrap();
        let m2 = SimpleModule::new("bar.pdb", debug_id);
        let mut f2 = SimpleFrame::with_instruction(0x1010);
        // No symbols present, should not find anything.
        assert!(symbolizer.fill_symbol(&m2, &mut f2).await.is_err());
        assert!(f2.function.is_none());
        assert!(f2.function_base.is_none());
        assert!(f2.source_file.is_none());
        assert!(f2.source_line.is_none());
        // Results should be cached.
        write_symbol_file(
            &path.join("bar.pdb/ffff0000000000000000ABCD12345678a/bar.sym"),
            b"MODULE Linux x86 ffff0000000000000000ABCD12345678a bar
FILE 53 bar.c
FUNC 1000 30 10 another func
1000 30 7 53
",
        );
        assert!(symbolizer.fill_symbol(&m2, &mut f2).await.is_err());
        assert!(f2.function.is_none());
        assert!(f2.function_base.is_none());
        assert!(f2.source_file.is_none());
        assert!(f2.source_line.is_none());
        // This should also use cached results.
        assert!(symbolizer
            .get_symbol_at_address("bar.pdb", debug_id, 0x1010)
            .await
            .is_none());
    }

    #[tokio::test]
    async fn test_extra_debug_info() {
        let debug_info = DebugInfoResult {
            debug_file: String::from_str("foo.pdb").unwrap(),
            debug_identifier: DebugId::from_str("abcd1234-abcd-1234-abcd-abcd12345678-a").unwrap(),
        };

        let mut supplier = StringSymbolSupplier {
            modules: HashMap::new(),
            code_info_to_debug_info: HashMap::new(),
        };
        supplier.modules.insert(
            String::from_str("foo.pdb").unwrap(),
            String::from_str(
                "MODULE Linux x86 ABCD1234ABCD1234ABCDABCD12345678a foo
FILE 1 foo.c
FUNC 1000 30 10 some func
1000 30 100 1
",
            )
            .unwrap(),
        );
        supplier.code_info_to_debug_info.insert(
            String::from_str("foo.pdb/64E782C570C4000/foo.pdb.sym").unwrap(),
            debug_info.clone(),
        );

        let symbolizer = Symbolizer::new(supplier);
        let module = SimpleModule::from_basic_info(
            None,
            None,
            Some(String::from_str("foo.pdb").unwrap()),
            Some(CodeId::from_str("64E782C570C4000").unwrap()),
        );

        let mut f1 = SimpleFrame::with_instruction(0x1010);
        symbolizer.fill_symbol(&module, &mut f1).await.unwrap();
        assert_eq!(f1.function.unwrap(), "some func");
        assert_eq!(f1.function_base.unwrap(), 0x1000);
        assert_eq!(f1.source_file.unwrap(), "foo.c");
        assert_eq!(f1.source_line.unwrap(), 100);
        assert_eq!(f1.source_line_base.unwrap(), 0x1000);

        let sym_stats = symbolizer.stats();
        let stats = sym_stats.get("foo.pdb").unwrap();
        assert_eq!(stats.extra_debug_info, Some(debug_info));
    }
}
