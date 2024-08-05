// Copyright 2015 Ted Mielczarek. See the COPYRIGHT
// file at the top-level directory of this distribution.

use range_map::{Range, RangeMap};
use std::collections::HashMap;

/// A publicly visible linker symbol.
#[derive(Debug, Eq, PartialEq, Ord, PartialOrd)]
pub struct PublicSymbol {
    /// The symbol's address relative to the module's load address.
    ///
    /// This field is declared first so that the derived Ord implementation sorts
    /// by address first. We take advantage of the sort order during address lookup.
    pub address: u64,
    /// The name of the symbol.
    pub name: String,
    /// The size of parameters passed to the function.
    pub parameter_size: u32,
}

/// A mapping from machine code bytes to source line and file.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct SourceLine {
    /// The start address relative to the module's load address.
    pub address: u64,
    /// The size of this range of instructions in bytes.
    pub size: u32,
    /// The source file name that generated this machine code.
    ///
    /// This is an index into `SymbolFile::files`.
    pub file: u32,
    /// The line number in `file` that generated this machine code.
    pub line: u32,
}

/// A single range which is covered by an inlined function call.
#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub struct Inlinee {
    /// The depth of the inline call.
    pub depth: u32,
    /// The start address relative to the module's load address.
    pub address: u64,
    /// The size of this range of instructions in bytes.
    pub size: u32,
    /// The source file which contains the function call.
    ///
    /// This is an index into `SymbolFile::files`.
    pub call_file: u32,
    /// The line number in `call_file` for the function call.
    pub call_line: u32,
    /// The function name, as an index into `SymbolFile::inline_origins`.
    pub origin_id: u32,
}

/// A source-language function.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Function {
    /// The function's start address relative to the module's load address.
    pub address: u64,
    /// The size of the function in bytes.
    pub size: u32,
    /// The size of parameters passed to the function.
    pub parameter_size: u32,
    /// The name of the function as declared in the source.
    pub name: String,
    /// Source line information for this function.
    pub lines: RangeMap<u64, SourceLine>,
    /// Inlinee information for this function, sorted by (depth, address).
    ///
    /// Essentially this can be considered as "one vec per depth", just with
    /// all those vecs concatenated into one.
    ///
    /// Inlinees form a nested structure, you can think of them like a flame graph.
    pub inlinees: Vec<Inlinee>,
}

impl Function {
    pub fn memory_range(&self) -> Option<Range<u64>> {
        if self.size == 0 {
            return None;
        }
        Some(Range::new(
            self.address,
            self.address.checked_add(self.size as u64)? - 1,
        ))
    }

    /// Returns `(file_id, line, address, inline_origin)` of the line or inline record that
    /// covers the given address at the outermost level (i.e. not inside any
    /// inlined calls).
    pub fn get_outermost_sourceloc(&self, addr: u64) -> Option<(u32, u32, u64, Option<u32>)> {
        if let Some((call_file, call_line, address, origin)) = self.get_inlinee_at_depth(0, addr) {
            return Some((call_file, call_line, address, Some(origin)));
        }
        // Otherwise we return the line record covering this address.
        let line = self.lines.get(addr)?;
        Some((line.file, line.line, line.address, None))
    }

    /// Returns `(file_id, line, address)` of the line record that covers the
    /// given address. Line records describe locations at the deepest level of
    /// inlining at that address.
    ///
    /// For example, if we have an "inline call stack" A -> B -> C at this
    /// address, i.e. both the call to B and the call to C have been inlined all
    /// the way into A (A being the "outer function"), then this method reports
    /// locations in C.
    pub fn get_innermost_sourceloc(&self, addr: u64) -> Option<(u32, u32, u64)> {
        let line = self.lines.get(addr)?;
        Some((line.file, line.line, line.address))
    }

    /// Returns `(call_file_id, call_line, address, inline_origin)` of the
    /// inlinee record that covers the given address at the given depth.
    ///
    /// We start at depth zero. For example, if we have an "inline call stack"
    /// A -> B -> C at an address, i.e. both the call to B and the call to C have
    /// been inlined all the way into A (A being the "outer function"), then the
    /// call A -> B is at level zero, and the call B -> C is at level one.
    pub fn get_inlinee_at_depth(&self, depth: u32, addr: u64) -> Option<(u32, u32, u64, u32)> {
        let inlinee = match self
            .inlinees
            .binary_search_by_key(&(depth, addr), |inlinee| (inlinee.depth, inlinee.address))
        {
            // Exact match
            Ok(index) => &self.inlinees[index],
            // No match, insertion index is zero => before first element
            Err(0) => return None,
            // No exact match, insertion index points after inlinee whose (depth, addr) is < what were looking for
            // => subtract 1 to get candidate
            Err(index) => &self.inlinees[index - 1],
        };
        if inlinee.depth != depth {
            return None;
        }
        let end_address = inlinee.address.checked_add(inlinee.size as u64)?;
        if addr < end_address {
            Some((
                inlinee.call_file,
                inlinee.call_line,
                inlinee.address,
                inlinee.origin_id,
            ))
        } else {
            None
        }
    }
}

/// Extra metadata that can be safely ignored, but may contain useful facts.
#[derive(Clone, Debug, Eq, Ord, PartialEq, PartialOrd)]
pub enum Info {
    /// The URL this file was downloaded from. This is added to symbol files
    /// by HttpSymbolSupplier when it stores them in its cache, so that we
    /// can populate that info even on a cache hit.
    Url(String),
    /// An info line we either don't know about or don't care about.
    Unknown,
}

/// DWARF CFI rules for recovering registers at a specific address.
#[derive(Clone, Debug, Eq, Ord, PartialEq, PartialOrd)]
pub struct CfiRules {
    /// The address in question.
    pub address: u64,
    /// Postfix expressions to evaluate to recover register values.
    pub rules: String,
}

/// Information used for unwinding stack frames using DWARF CFI.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct StackInfoCfi {
    /// The initial rules for this address range.
    pub init: CfiRules,
    /// The size of this entire address range.
    pub size: u32,
    /// Additional rules to use at specified addresses.
    pub add_rules: Vec<CfiRules>,
}

impl StackInfoCfi {
    pub fn memory_range(&self) -> Option<Range<u64>> {
        if self.size == 0 {
            return None;
        }
        Some(Range::new(
            self.init.address,
            self.init.address.checked_add(self.size as u64)? - 1,
        ))
    }
}

/// Specific details about whether the frame uses a base pointer or has a program string to
/// evaluate.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum WinFrameType {
    /// This frame uses FPO-style data.
    Fpo(StackInfoWin),
    /// This frame uses new-style frame data, has a program string.
    FrameData(StackInfoWin),
    /// Some other type of frame.
    Unhandled,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum WinStackThing {
    ProgramString(String),
    AllocatesBasePointer(bool),
}

/// Information used for unwinding stack frames using Windows frame info.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct StackInfoWin {
    /// The address in question.
    pub address: u64,
    /// The size of the address range covered.
    pub size: u32,
    /// The size of the function's prologue.
    pub prologue_size: u32,
    /// The size of the function's epilogue.
    pub epilogue_size: u32,
    /// The size of arguments passed to this function.
    pub parameter_size: u32,
    /// The number of bytes in the stack frame for callee-saves registers.
    pub saved_register_size: u32,
    /// The number of bytes in the stack frame for local variables.
    pub local_size: u32,
    /// The maximum number of bytes pushed onto the stack by this frame.
    pub max_stack_size: u32,
    /// A program string or boolean regarding a base pointer.
    pub program_string_or_base_pointer: WinStackThing,
}

impl StackInfoWin {
    pub fn memory_range(&self) -> Option<Range<u64>> {
        if self.size == 0 {
            return None;
        }
        Some(Range::new(
            self.address,
            self.address.checked_add(self.size as u64)? - 1,
        ))
    }
}

/// A parsed .sym file containing debug symbols.
#[derive(Debug, PartialEq, Eq)]
pub struct SymbolFile {
    pub module_id: String,
    pub debug_file: String,
    /// The set of source files involved in compilation.
    pub files: HashMap<u32, String>,
    /// Publicly visible symbols.
    pub publics: Vec<PublicSymbol>,
    /// Functions.
    pub functions: RangeMap<u64, Function>,
    /// Function names for inlined functions.
    pub inline_origins: HashMap<u32, String>,
    /// DWARF CFI unwind information.
    pub cfi_stack_info: RangeMap<u64, StackInfoCfi>,
    /// Windows unwind information (frame data).
    pub win_stack_framedata_info: RangeMap<u64, StackInfoWin>,
    /// Windows unwind information (FPO data).
    pub win_stack_fpo_info: RangeMap<u64, StackInfoWin>,

    // Statistics which are strictly best-effort. Generally this
    // means we might undercount in situations where we forgot to
    // log an event.
    /// If the symbol file was loaded from a URL, this is the url
    pub url: Option<String>,
    /// The number of times the parser found that the symbol file was
    /// strictly ambiguous but simple heuristics repaired it. (e.g.
    /// two STACK WIN entries overlapped, but the second was a suffix of
    /// the first, so we just truncated the first.)
    ///
    /// Ideally dump_syms would never output this kind of thing, but it's
    /// tolerable.
    pub ambiguities_repaired: u64,
    /// The number of times the parser found that the symbol file was
    /// ambiguous and just randomly picked one of the options to make
    /// progress.
    ///
    /// e.g. two STACK WIN entries with identical ranges but
    /// different values, so one was discarded arbitrarily.
    pub ambiguities_discarded: u64,
    /// The number of times the parser found that a section of the file
    /// (generally a line) was corrupt and discarded it.
    ///
    /// e.g. a STACK WIN entry where the `type` and `has_program` fields
    /// have inconsistent values.
    pub corruptions_discarded: u64,
    /// The number of times the cfi evaluator failed out in a way that
    /// implies the cfi entry is fundamentally corrupt.
    ///
    /// This isn't detectected during parsing for two reasons:
    ///
    /// * We don't parse cfi program strings until we are requested to
    ///   execute them (there's ~millions of program strings which will
    ///   never need to be parsed, so eagerly parsing them would be
    ///   horribly expensive and pointless for anything but debug stats.)
    ///
    /// * A program string may technically parse but still be impossible
    ///   to fully evaluate. For instance, it might try to pop values from
    ///   its internal stack when there are none left.
    ///
    /// This number may be inflated if a corrupt cfi entry occurs in multiple
    /// frames, as each attempted eval will be counted.
    ///
    /// This number does not include cfi evaluations that failed in ways that
    /// may be a result of incorrect input memory/registers (e.g. failing
    /// to evaluate a "dereference pointer" instruction because the pointer
    /// was not mapped memory). In these situations the cfi entry *may*
    /// still be correct.
    pub cfi_eval_corruptions: u64,
}
