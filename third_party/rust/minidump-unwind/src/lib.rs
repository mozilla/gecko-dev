// Copyright 2015 Ted Mielczarek. See the COPYRIGHT
// file at the top-level directory of this distribution.

//! Unwind stack frames for a thread.

#[cfg(all(doctest, feature = "http"))]
doc_comment::doctest!("../README.md");

mod amd64;
mod arm;
mod arm64;
mod arm64_old;
mod mips;
pub mod symbols;
pub mod system_info;
mod x86;

use minidump::*;
use minidump_common::utils::basename;
use scroll::ctx::{SizeWith, TryFromCtx};
use std::borrow::Cow;
use std::collections::{BTreeMap, BTreeSet, HashSet};
use std::convert::TryFrom;
use std::io::{self, Write};
use tracing::trace;

pub use crate::symbols::*;
pub use crate::system_info::*;

#[derive(Clone, Copy)]
struct GetCallerFrameArgs<'a, P> {
    callee_frame: &'a StackFrame,
    grand_callee_frame: Option<&'a StackFrame>,
    stack_memory: UnifiedMemory<'a, 'a>,
    modules: &'a MinidumpModuleList,
    system_info: &'a SystemInfo,
    symbol_provider: &'a P,
}

impl<P> GetCallerFrameArgs<'_, P> {
    fn valid(&self) -> &MinidumpContextValidity {
        &self.callee_frame.context.valid
    }
}

mod impl_prelude {
    pub(crate) use super::{
        CfiStackWalker, FrameTrust, GetCallerFrameArgs, StackFrame, SymbolProvider,
    };
}

/// Indicates how well the instruction pointer derived during
/// stack walking is trusted. Since the stack walker can resort to
/// stack scanning, it can wind up with dubious frames.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum FrameTrust {
    /// Unknown
    None,
    /// Scanned the stack, found this.
    Scan,
    /// Found while scanning stack using call frame info.
    CfiScan,
    /// Derived from frame pointer.
    FramePointer,
    /// Derived from call frame info.
    CallFrameInfo,
    /// Explicitly provided by some external stack walker.
    PreWalked,
    /// Given as instruction pointer in a context.
    Context,
}

impl FrameTrust {
    /// Return a string describing how a stack frame was found
    /// by the stackwalker.
    pub fn description(&self) -> &'static str {
        match *self {
            FrameTrust::Context => "given as instruction pointer in context",
            FrameTrust::PreWalked => "recovered by external stack walker",
            FrameTrust::CallFrameInfo => "call frame info",
            FrameTrust::CfiScan => "call frame info with scanning",
            FrameTrust::FramePointer => "previous frame's frame pointer",
            FrameTrust::Scan => "stack scanning",
            FrameTrust::None => "unknown",
        }
    }

    pub fn as_str(&self) -> &'static str {
        match *self {
            FrameTrust::Context => "context",
            FrameTrust::PreWalked => "prewalked",
            FrameTrust::CallFrameInfo => "cfi",
            FrameTrust::CfiScan => "cfi_scan",
            FrameTrust::FramePointer => "frame_pointer",
            FrameTrust::Scan => "scan",
            FrameTrust::None => "non",
        }
    }
}

/// The calling convention of a function.
#[derive(Debug, Clone)]
pub enum CallingConvention {
    Cdecl,
    WindowsThisCall,
    OtherThisCall,
}

/// Arguments for this function
#[derive(Debug, Clone)]
pub struct FunctionArgs {
    /// What we assumed the calling convention was.
    pub calling_convention: CallingConvention,

    /// The actual arguments.
    pub args: Vec<FunctionArg>,
}

/// A function argument.
#[derive(Debug, Clone)]
pub struct FunctionArg {
    /// The name of the argument (usually actually just the type).
    pub name: String,
    /// The value of the argument.
    pub value: Option<u64>,
}

/// A stack frame for an inlined function.
///
/// See [`StackFrame::inlines`][] for more details.
#[derive(Debug, Clone)]
pub struct InlineFrame {
    /// The name of the function
    pub function_name: String,
    /// The file name of the stack frame
    pub source_file_name: Option<String>,
    /// The line number of the stack frame
    pub source_line: Option<u32>,
}

/// A single stack frame produced from unwinding a thread's stack.
#[derive(Debug, Clone)]
pub struct StackFrame {
    /// The program counter location as an absolute virtual address.
    ///
    /// - For the innermost called frame in a stack, this will be an exact
    ///   program counter or instruction pointer value.
    ///
    /// - For all other frames, this address is within the instruction that
    ///   caused execution to branch to this frame's callee (although it may
    ///   not point to the exact beginning of that instruction). This ensures
    ///   that, when we look up the source code location for this frame, we
    ///   get the source location of the call, not of the point at which
    ///   control will resume when the call returns, which may be on the next
    ///   line. (If the compiler knows the callee never returns, it may even
    ///   place the call instruction at the very end of the caller's machine
    ///   code, such that the "return address" (which will never be used)
    ///   immediately after the call instruction is in an entirely different
    ///   function, perhaps even from a different source file.)
    ///
    /// On some architectures, the return address as saved on the stack or in
    /// a register is fine for looking up the point of the call. On others, it
    /// requires adjustment.
    pub instruction: u64,

    /// The instruction address (program counter) that execution of this function
    /// would resume at, if the callee returns.
    ///
    /// This is exactly **the return address of the of the callee**. We use this
    /// nonstandard terminology because just calling this "return address"
    /// would be ambiguous and too easy to mix up.
    ///
    /// **Note:** you should strongly prefer using [`StackFrame::instruction`][], which should
    /// be the address of the instruction before this one which called the callee.
    /// That is the instruction that this function was logically "executing" when the
    /// program's state was captured, and therefore what people expect from
    /// backtraces.
    ///
    /// This is more than a matter of user expections: **there are situations
    /// where this value is nonsensical but the [`StackFrame::instruction`][] is valid.**
    ///
    /// Specifically, if the callee is "noreturn" then *this function should
    /// never resume execution*. The compiler has no obligation to emit any
    /// instructions after such a CALL, but CALL still implicitly pushes the
    /// instruction after itself to the stack. Such a return address may
    /// therefore be outside the "bounds" of this function!!!
    ///
    /// Yes, compilers *can* just immediately jump into the callee for
    /// noreturn calls, but it's genuinely very helpful for them to emit a
    /// CALL because it keeps the stack reasonable for backtraces and
    /// debuggers, which are more interested in [`StackFrame::instruction`][] anyway!
    ///
    /// (If this is the top frame of the call stack, then `resume_address`
    /// and `instruction` are exactly equal and should reflect the actual
    /// program counter of this thread.)
    pub resume_address: u64,

    /// The module in which the instruction resides.
    pub module: Option<MinidumpModule>,

    /// Any unloaded modules which overlap with this address.
    ///
    /// This is currently only populated if `module` is None.
    ///
    /// Since unloaded modules may overlap, there may be more than
    /// one module. Since a module may be unloaded and reloaded at
    /// multiple positions, we keep track of all the offsets that
    /// apply. BTrees are used to produce a more stable output.
    ///
    /// So this is a `BTreeMap<module_name, Set<offsets>>`.
    pub unloaded_modules: BTreeMap<String, BTreeSet<u64>>,

    /// The function name, may be omitted if debug symbols are not available.
    pub function_name: Option<String>,

    /// The start address of the function, may be omitted if debug symbols
    /// are not available.
    pub function_base: Option<u64>,

    /// The size, in bytes, of the arguments pushed on the stack for this function.
    /// WIN STACK unwinding needs this value to work; it's otherwise uninteresting.
    pub parameter_size: Option<u32>,

    /// The source file name, may be omitted if debug symbols are not available.
    pub source_file_name: Option<String>,

    /// The (1-based) source line number, may be omitted if debug symbols are
    /// not available.
    pub source_line: Option<u32>,

    /// The start address of the source line, may be omitted if debug symbols
    /// are not available.
    pub source_line_base: Option<u64>,

    /// Any inline frames that cover the frame address, ordered "inside to outside",
    /// or "deepest callee to shallowest callee". This is the same order that StackFrames
    /// appear in.
    ///
    /// These frames are "fake" in that they don't actually exist at runtime, and are only
    /// known because the compiler added debuginfo saying they exist.
    ///
    /// As a result, many properties of these frames either don't exist or are
    /// in some sense "inherited" from the parent real frame. For instance they
    /// have the same instruction/module by definiton.
    ///
    /// If you were to print frames you would want to do something like:
    ///
    /// ```ignore
    /// let mut frame_num = 0;
    /// for frame in &thread.frames {
    ///     // Inlines come first
    ///     for inline in &frame.inlines {
    ///         print_inline(frame_num, frame, inline);
    ///         frame_num += 1;
    ///     }
    ///     print_frame(frame_num, frame);
    ///     frame_num += 1;
    /// }
    /// ```
    pub inlines: Vec<InlineFrame>,

    /// Amount of trust the stack walker has in the instruction pointer
    /// of this frame.
    pub trust: FrameTrust,

    /// The CPU context containing register state for this frame.
    pub context: MinidumpContext,

    /// Any function args we recovered.
    pub arguments: Option<FunctionArgs>,
}

impl StackFrame {
    /// Create a `StackFrame` from a `MinidumpContext`.
    pub fn from_context(context: MinidumpContext, trust: FrameTrust) -> StackFrame {
        StackFrame {
            instruction: context.get_instruction_pointer(),
            // Initialized the same as `instruction`, but left unmodified during stack walking.
            resume_address: context.get_instruction_pointer(),
            module: None,
            unloaded_modules: BTreeMap::new(),
            function_name: None,
            function_base: None,
            parameter_size: None,
            source_file_name: None,
            source_line: None,
            source_line_base: None,
            inlines: Vec::new(),
            arguments: None,
            trust,
            context,
        }
    }
}

impl FrameSymbolizer for StackFrame {
    fn get_instruction(&self) -> u64 {
        self.instruction
    }
    fn set_function(&mut self, name: &str, base: u64, parameter_size: u32) {
        self.function_name = Some(String::from(name));
        self.function_base = Some(base);
        self.parameter_size = Some(parameter_size);
    }
    fn set_source_file(&mut self, file: &str, line: u32, base: u64) {
        self.source_file_name = Some(String::from(file));
        self.source_line = Some(line);
        self.source_line_base = Some(base);
    }
    /// This function can be called multiple times, for the inlines that cover the
    /// address at various levels of inlining. The call order is from outside to
    /// inside.
    fn add_inline_frame(&mut self, name: &str, file: Option<&str>, line: Option<u32>) {
        self.inlines.push(InlineFrame {
            function_name: name.to_string(),
            source_file_name: file.map(ToString::to_string),
            source_line: line,
        })
    }
}

/// Information about the results of unwinding a thread's stack.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum CallStackInfo {
    /// Everything went great.
    Ok,
    /// No `MinidumpContext` was provided, couldn't do anything.
    MissingContext,
    /// No stack memory was provided, couldn't unwind past the top frame.
    MissingMemory,
    /// The CPU type is unsupported.
    UnsupportedCpu,
    /// This thread wrote the minidump, it was skipped.
    DumpThreadSkipped,
}

/// A stack of `StackFrame`s produced as a result of unwinding a thread.
#[derive(Debug, Clone)]
pub struct CallStack {
    /// The stack frames.
    /// By convention, the stack frame at index 0 is the innermost callee frame,
    /// and the frame at the highest index in a call stack is the outermost
    /// caller.
    pub frames: Vec<StackFrame>,
    /// Information about this `CallStack`.
    pub info: CallStackInfo,
    /// The identifier of the thread.
    pub thread_id: u32,
    /// The name of the thread, if known.
    pub thread_name: Option<String>,
    /// The GetLastError() value stored in the TEB.
    pub last_error_value: Option<CrashReason>,
}

impl CallStack {
    /// Construct a CallStack that just has the unsymbolicated context frame.
    ///
    /// This is the desired input for the stack walker.
    pub fn with_context(context: MinidumpContext) -> Self {
        Self {
            frames: vec![StackFrame::from_context(context, FrameTrust::Context)],
            info: CallStackInfo::Ok,
            thread_id: 0,
            thread_name: None,
            last_error_value: None,
        }
    }

    /// Create a `CallStack` with `info` and no frames.
    pub fn with_info(id: u32, info: CallStackInfo) -> CallStack {
        CallStack {
            info,
            frames: vec![],
            thread_id: id,
            thread_name: None,
            last_error_value: None,
        }
    }

    /// Write a human-readable description of the call stack to `f`.
    ///
    /// This is very verbose, it implements the output format used by
    /// minidump_stackwalk.
    pub fn print<T: Write>(&self, f: &mut T) -> io::Result<()> {
        fn print_registers<T: Write>(f: &mut T, ctx: &MinidumpContext) -> io::Result<()> {
            let registers: Cow<HashSet<&str>> = match ctx.valid {
                MinidumpContextValidity::All => {
                    let gpr = ctx.general_purpose_registers();
                    let set: HashSet<&str> = gpr.iter().cloned().collect();
                    Cow::Owned(set)
                }
                MinidumpContextValidity::Some(ref which) => Cow::Borrowed(which),
            };

            // Iterate over registers in a known order.
            let mut output = String::new();
            for reg in ctx.general_purpose_registers() {
                if registers.contains(reg) {
                    let reg_val = ctx.format_register(reg);
                    let next = format!(" {reg: >6} = {reg_val}");
                    if output.chars().count() + next.chars().count() > 80 {
                        // Flush the buffer.
                        writeln!(f, " {output}")?;
                        output.truncate(0);
                    }
                    output.push_str(&next);
                }
            }
            if !output.is_empty() {
                writeln!(f, " {output}")?;
            }
            Ok(())
        }

        if self.frames.is_empty() {
            writeln!(f, "<no frames>")?;
        }
        let mut frame_count = 0;
        for frame in &self.frames {
            // First print out inlines
            for inline in &frame.inlines {
                // Frame number
                let frame_idx = frame_count;
                frame_count += 1;
                write!(f, "{frame_idx:2}  ")?;

                // Module name
                if let Some(ref module) = frame.module {
                    write!(f, "{}", basename(&module.code_file()))?;
                }

                // Function name
                write!(f, "!{}", inline.function_name)?;

                // Source file and line
                if let (Some(source_file), Some(source_line)) =
                    (&inline.source_file_name, &inline.source_line)
                {
                    write!(f, " [{} : {}]", basename(source_file), source_line,)?;
                }
                writeln!(f)?;
                // A fake `trust`
                writeln!(f, "    Found by: inlining")?;
            }

            // Now print out the "real frame"
            let frame_idx = frame_count;
            frame_count += 1;
            let addr = frame.instruction;

            // Frame number
            write!(f, "{frame_idx:2}  ")?;
            if let Some(module) = &frame.module {
                // Module name
                write!(f, "{}", basename(&module.code_file()))?;

                if let (Some(func_name), Some(func_base)) =
                    (&frame.function_name, &frame.function_base)
                {
                    // Function name
                    write!(f, "!{func_name}")?;

                    if let (Some(src_file), Some(src_line), Some(src_base)) = (
                        &frame.source_file_name,
                        &frame.source_line,
                        &frame.source_line_base,
                    ) {
                        // Source file, line, and offset
                        write!(
                            f,
                            " [{} : {} + {:#x}]",
                            basename(src_file),
                            src_line,
                            addr - src_base
                        )?;
                    } else {
                        // We didn't have source info, so just give a byte offset from the func
                        write!(f, " + {:#x}", addr - func_base)?;
                    }
                } else {
                    // We didn't have a function name, so just give a byte offset from the module
                    write!(f, " + {:#x}", addr - module.base_address())?;
                }
            } else {
                // We didn't even find a module, so just print the raw address
                write!(f, "{addr:#x}")?;

                // List off overlapping unloaded modules.

                // First we need to collect them up by name so that we can print
                // all the overlaps from one module together and dedupe them.
                // (!!! was that code deleted?)
                for (name, offsets) in &frame.unloaded_modules {
                    write!(f, " (unloaded {name}@")?;
                    let mut first = true;
                    for offset in offsets {
                        if first {
                            write!(f, "{offset:#x}")?;
                        } else {
                            // `|` is our separator for multiple entries
                            write!(f, "|{offset:#x}")?;
                        }
                        first = false;
                    }
                    write!(f, ")")?;
                }
            }

            // Print the valid registers
            writeln!(f)?;
            print_registers(f, &frame.context)?;

            // And the trust we have of this result
            writeln!(f, "    Found by: {}", frame.trust.description())?;

            // Now print out recovered args
            if let Some(args) = &frame.arguments {
                use MinidumpRawContext::*;
                let pointer_width = match &frame.context.raw {
                    X86(_) | Ppc(_) | Sparc(_) | Arm(_) | Mips(_) => 4,
                    Ppc64(_) | Amd64(_) | Arm64(_) | OldArm64(_) => 8,
                };

                let cc_summary = match args.calling_convention {
                    CallingConvention::Cdecl => "cdecl [static function]",
                    CallingConvention::WindowsThisCall => "windows thiscall [C++ member function]",
                    CallingConvention::OtherThisCall => {
                        "non-windows thiscall [C++ member function]"
                    }
                };

                writeln!(f, "    Arguments (assuming {cc_summary})")?;
                for (idx, arg) in args.args.iter().enumerate() {
                    if let Some(val) = arg.value {
                        if pointer_width == 4 {
                            writeln!(f, "        arg {} ({}) = 0x{:08x}", idx, arg.name, val)?;
                        } else {
                            writeln!(f, "        arg {} ({}) = 0x{:016x}", idx, arg.name, val)?;
                        }
                    } else {
                        writeln!(f, "        arg {} ({}) = <unknown>", idx, arg.name)?;
                    }
                }
                // Add an extra new-line between frames when there's function arguments to make
                // it more readable.
                writeln!(f)?;
            }
        }
        Ok(())
    }
}

struct CfiStackWalker<'a, C: CpuContext> {
    instruction: u64,
    has_grand_callee: bool,
    grand_callee_parameter_size: u32,

    callee_ctx: &'a C,
    callee_validity: &'a MinidumpContextValidity,

    caller_ctx: C,
    caller_validity: HashSet<&'static str>,

    module: &'a MinidumpModule,
    stack_memory: UnifiedMemory<'a, 'a>,
}

impl<'a, C> CfiStackWalker<'a, C>
where
    C: CpuContext + Clone,
{
    fn from_ctx_and_args<P, R>(
        ctx: &'a C,
        args: &'a GetCallerFrameArgs<'a, P>,
        callee_forwarded_regs: R,
    ) -> Option<Self>
    where
        R: Fn(&MinidumpContextValidity) -> HashSet<&'static str>,
    {
        let module = args
            .modules
            .module_at_address(args.callee_frame.instruction)?;
        let grand_callee = args.grand_callee_frame;
        Some(Self {
            instruction: args.callee_frame.instruction,
            has_grand_callee: grand_callee.is_some(),
            grand_callee_parameter_size: grand_callee.and_then(|f| f.parameter_size).unwrap_or(0),

            callee_ctx: ctx,
            callee_validity: args.valid(),

            // Default to forwarding all callee-saved regs verbatim.
            // The CFI evaluator may clear or overwrite these values.
            // The stack pointer and instruction pointer are not included.
            caller_ctx: ctx.clone(),
            caller_validity: callee_forwarded_regs(args.valid()),

            module,
            stack_memory: args.stack_memory,
        })
    }
}

impl<'a, C> FrameWalker for CfiStackWalker<'a, C>
where
    C: CpuContext,
    C::Register: TryFrom<u64>,
    u64: TryFrom<C::Register>,
    C::Register: TryFromCtx<'a, Endian, [u8], Error = scroll::Error> + SizeWith<Endian>,
{
    fn get_instruction(&self) -> u64 {
        self.instruction
    }
    fn has_grand_callee(&self) -> bool {
        self.has_grand_callee
    }
    fn get_grand_callee_parameter_size(&self) -> u32 {
        self.grand_callee_parameter_size
    }
    fn get_register_at_address(&self, address: u64) -> Option<u64> {
        let result: Option<C::Register> = self.stack_memory.get_memory_at_address(address);
        result.and_then(|val| u64::try_from(val).ok())
    }
    fn get_callee_register(&self, name: &str) -> Option<u64> {
        self.callee_ctx
            .get_register(name, self.callee_validity)
            .and_then(|val| u64::try_from(val).ok())
    }
    fn set_caller_register(&mut self, name: &str, val: u64) -> Option<()> {
        let memoized = self.caller_ctx.memoize_register(name)?;
        let val = C::Register::try_from(val).ok()?;
        self.caller_validity.insert(memoized);
        self.caller_ctx.set_register(name, val)
    }
    fn clear_caller_register(&mut self, name: &str) {
        self.caller_validity.remove(name);
    }
    fn set_cfa(&mut self, val: u64) -> Option<()> {
        // NOTE: some things have alluded to architectures where this isn't
        // how the CFA should be handled, but we apparently don't support them yet?
        let stack_pointer_reg = self.caller_ctx.stack_pointer_register_name();
        let val = C::Register::try_from(val).ok()?;
        self.caller_validity.insert(stack_pointer_reg);
        self.caller_ctx.set_register(stack_pointer_reg, val)
    }
    fn set_ra(&mut self, val: u64) -> Option<()> {
        let instruction_pointer_reg = self.caller_ctx.instruction_pointer_register_name();
        let val = C::Register::try_from(val).ok()?;
        self.caller_validity.insert(instruction_pointer_reg);
        self.caller_ctx.set_register(instruction_pointer_reg, val)
    }
}

#[tracing::instrument(name = "unwind_frame", level = "trace", skip_all, fields(idx = _frame_idx, fname = args.callee_frame.function_name.as_deref().unwrap_or("")))]
async fn get_caller_frame<P>(
    _frame_idx: usize,
    args: &GetCallerFrameArgs<'_, P>,
) -> Option<StackFrame>
where
    P: SymbolProvider + Sync,
{
    match args.callee_frame.context.raw {
        /*
        MinidumpRawContext::PPC(ctx) => ctx.get_caller_frame(stack_memory),
        MinidumpRawContext::PPC64(ctx) => ctx.get_caller_frame(stack_memory),
        MinidumpRawContext::SPARC(ctx) => ctx.get_caller_frame(stack_memory),
         */
        MinidumpRawContext::Arm(ref ctx) => arm::get_caller_frame(ctx, args).await,
        MinidumpRawContext::Arm64(ref ctx) => arm64::get_caller_frame(ctx, args).await,
        MinidumpRawContext::OldArm64(ref ctx) => arm64_old::get_caller_frame(ctx, args).await,
        MinidumpRawContext::Amd64(ref ctx) => amd64::get_caller_frame(ctx, args).await,
        MinidumpRawContext::X86(ref ctx) => x86::get_caller_frame(ctx, args).await,
        MinidumpRawContext::Mips(ref ctx) => mips::get_caller_frame(ctx, args).await,
        _ => None,
    }
}

async fn fill_source_line_info<P>(
    frame: &mut StackFrame,
    modules: &MinidumpModuleList,
    symbol_provider: &P,
) where
    P: SymbolProvider + Sync,
{
    // Find the module whose address range covers this frame's instruction.
    if let Some(module) = modules.module_at_address(frame.instruction) {
        // FIXME: this shouldn't need to clone, we should be able to use
        // the same lifetime as the module list that's passed in.
        frame.module = Some(module.clone());

        // This is best effort, so ignore any errors.
        let _ = symbol_provider.fill_symbol(module, frame).await;

        // If we got any inlines, reverse them! The symbol format makes it simplest to
        // emit inlines from the shallowest callee to the deepest one ("inner to outer"),
        // but we want inlines to be in the same order as the stackwalk itself, which means
        // we want the deepest frame first (the callee-est frame).
        frame.inlines.reverse();
    }
}

/// An optional callback when walking frames.
///
/// One may convert from other types to this callback type:
/// `FnMut(frame_idx: usize, frame: &StackFrame)` types can be converted to a
/// callback, and `()` can be converted to no callback (do nothing).
pub enum OnWalkedFrame<'a> {
    None,
    #[allow(clippy::type_complexity)]
    Some(Box<dyn FnMut(usize, &StackFrame) + Send + 'a>),
}

impl From<()> for OnWalkedFrame<'_> {
    fn from(_: ()) -> Self {
        Self::None
    }
}

impl<'a, F: FnMut(usize, &StackFrame) + Send + 'a> From<F> for OnWalkedFrame<'a> {
    fn from(f: F) -> Self {
        Self::Some(Box::new(f))
    }
}

#[tracing::instrument(name = "unwind_thread", level = "trace", skip_all, fields(idx = _thread_idx, tid = stack.thread_id, tname = stack.thread_name.as_deref().unwrap_or("")))]
pub async fn walk_stack<P>(
    _thread_idx: usize,
    on_walked_frame: impl Into<OnWalkedFrame<'_>>,
    stack: &mut CallStack,
    stack_memory: Option<UnifiedMemory<'_, '_>>,
    modules: &MinidumpModuleList,
    system_info: &SystemInfo,
    symbol_provider: &P,
) where
    P: SymbolProvider + Sync,
{
    trace!(
        "starting stack unwind of thread {} {}",
        stack.thread_id,
        stack.thread_name.as_deref().unwrap_or(""),
    );

    // All the unwinder code down below in `get_caller_frame` requires a valid `stack_memory`,
    // where _valid_ means that we can actually read something from it. A call to `memory_range` will validate that,
    // as it will reject empty stack memory or one with an overflowing `size`.
    let stack_memory =
        stack_memory.and_then(|stack_memory| stack_memory.memory_range().map(|_| stack_memory));

    // Begin with the context frame, and keep getting callers until there are no more.
    let mut has_new_frame = !stack.frames.is_empty();
    let mut on_walked_frame = on_walked_frame.into();
    while has_new_frame {
        // Symbolicate the new frame
        let frame_idx = stack.frames.len() - 1;
        let frame = stack.frames.last_mut().unwrap();

        fill_source_line_info(frame, modules, symbol_provider).await;

        // Report the frame as walked and symbolicated
        if let OnWalkedFrame::Some(on_walked_frame) = &mut on_walked_frame {
            on_walked_frame(frame_idx, frame);
        }

        let Some(stack_memory) = stack_memory else {
            break;
        };

        // Walk the new frame
        let callee_frame = &stack.frames.last().unwrap();
        let grand_callee_frame = stack
            .frames
            .len()
            .checked_sub(2)
            .and_then(|idx| stack.frames.get(idx));
        match callee_frame.function_name.as_ref() {
            Some(name) => trace!("unwinding {}", name),
            None => trace!("unwinding 0x{:016x}", callee_frame.instruction),
        }
        let new_frame = get_caller_frame(
            frame_idx,
            &GetCallerFrameArgs {
                callee_frame,
                grand_callee_frame,
                stack_memory,
                modules,
                system_info,
                symbol_provider,
            },
        )
        .await;

        // Check if we're done
        if let Some(new_frame) = new_frame {
            stack.frames.push(new_frame);
        } else {
            has_new_frame = false;
        }
    }
    trace!(
        "finished stack unwind of thread {} {}\n",
        stack.thread_id,
        stack.thread_name.as_deref().unwrap_or(""),
    );
}

/// Checks if we can dismiss the validity of an instruction based on our symbols,
/// to refine the quality of each unwinder's instruction_seems_valid implementation.
async fn instruction_seems_valid_by_symbols<P>(
    instruction: u64,
    modules: &MinidumpModuleList,
    symbol_provider: &P,
) -> bool
where
    P: SymbolProvider + Sync,
{
    // Our input is a candidate return address, but we *really* want to validate the address
    // of the call instruction *before* the return address. In theory this symbol-based
    // analysis shouldn't *care* whether we're looking at the call or the instruction
    // after it, but there is one corner case where the return address can be invalid
    // but the instruction before it isn't: noreturn.
    //
    // If the *callee* is noreturn, then the caller has no obligation to have any instructions
    // after the call! So e.g. on x86 if you CALL a noreturn function, the return address
    // that's implicitly pushed *could* be one-past-the-end of the "function".
    //
    // This has been observed in practice with `+[NSThread exit]`!
    //
    // We don't otherwise need the instruction pointer to be terribly precise, so
    // subtracting 1 from the address should be sufficient to handle this corner case.
    let instruction = instruction.saturating_sub(1);

    // NULL pointer is definitely not valid
    if instruction == 0 {
        return false;
    }

    if let Some(module) = modules.module_at_address(instruction) {
        // Create a dummy frame symbolizing implementation to feed into
        // our symbol provider with the address we're interested in. If
        // it tries to set a non-empty function name, then we can reasonably
        // assume the instruction address is valid.
        //use crate::FrameSymbolizer;

        struct DummyFrame {
            instruction: u64,
            has_name: bool,
        }
        impl FrameSymbolizer for DummyFrame {
            fn get_instruction(&self) -> u64 {
                self.instruction
            }
            fn set_function(&mut self, name: &str, _base: u64, _parameter_size: u32) {
                self.has_name = !name.is_empty();
            }
            fn set_source_file(&mut self, _file: &str, _line: u32, _base: u64) {
                // Do nothing
            }
        }

        let mut frame = DummyFrame {
            instruction,
            has_name: false,
        };

        if symbol_provider
            .fill_symbol(module, &mut frame)
            .await
            .is_ok()
        {
            frame.has_name
        } else {
            // If the symbol provider returns an Error, this means that we
            // didn't have any symbols for the *module*. Just assume the
            // instruction is valid in this case so that scanning works
            // when we have no symbols.
            true
        }
    } else {
        // We couldn't even map this address to a module. Reject the pointer
        // so that we have *some* way to distinguish "normal" pointers
        // from instruction address.
        //
        // FIXME: this will reject any pointer into JITed code which otherwise
        // isn't part of a normal well-defined module. We can potentially use
        // MemoryInfoListStream (windows) and /proc/self/maps (linux) to refine
        // this analysis and allow scans to walk through JITed code.
        false
    }
}

#[cfg(test)]
mod amd64_unittest;
#[cfg(test)]
mod arm64_unittest;
#[cfg(test)]
mod arm_unittest;
#[cfg(test)]
mod x86_unittest;
