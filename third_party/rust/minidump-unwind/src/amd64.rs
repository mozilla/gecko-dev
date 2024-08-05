// Copyright 2015 Ted Mielczarek. See the COPYRIGHT
// file at the top-level directory of this distribution.

// Note since x86 and Amd64 have basically the same ABI, this implementation
// is written to largely erase the details of the two wherever possible,
// so that it can be copied between the two with minimal changes. It's not
// worth the effort to *actually* unify the implementations.

use super::impl_prelude::*;
use minidump::format::CONTEXT_AMD64;
use minidump::system_info::Os;
use minidump::{
    MinidumpContext, MinidumpContextValidity, MinidumpModuleList, MinidumpRawContext, UnifiedMemory,
};
use std::collections::HashSet;
use tracing::trace;

type Pointer = u64;
const POINTER_WIDTH: Pointer = 8;
const INSTRUCTION_REGISTER: &str = "rip";
const STACK_POINTER_REGISTER: &str = "rsp";
const FRAME_POINTER_REGISTER: &str = "rbp";
// FIXME: rdi and rsi are also preserved on windows (but not in sysv) -- we should handle that?
const CALLEE_SAVED_REGS: &[&str] = &["rbx", "rbp", "r12", "r13", "r14", "r15"];

async fn get_caller_by_cfi<P>(
    ctx: &CONTEXT_AMD64,
    args: &GetCallerFrameArgs<'_, P>,
) -> Option<StackFrame>
where
    P: SymbolProvider + Sync,
{
    trace!("trying cfi");

    if let MinidumpContextValidity::Some(ref which) = args.valid() {
        if !which.contains(STACK_POINTER_REGISTER) {
            return None;
        }
    }

    let mut stack_walker = CfiStackWalker::from_ctx_and_args(ctx, args, callee_forwarded_regs)?;

    args.symbol_provider
        .walk_frame(stack_walker.module, &mut stack_walker)
        .await?;
    let caller_ip = stack_walker.caller_ctx.rip;
    let caller_sp = stack_walker.caller_ctx.rsp;

    trace!(
        "cfi evaluation was successful -- caller_ip: 0x{:016x}, caller_sp: 0x{:016x}",
        caller_ip,
        caller_sp,
    );

    // Do absolutely NO validation! Yep! As long as CFI evaluation succeeds
    // (which does include ip and sp resolving), just blindly assume the
    // values are correct. I Don't Like This, but it's what breakpad does and
    // we should start with a baseline of parity.

    trace!("cfi result seems valid");

    let context = MinidumpContext {
        raw: MinidumpRawContext::Amd64(stack_walker.caller_ctx),
        valid: MinidumpContextValidity::Some(stack_walker.caller_validity),
    };
    Some(StackFrame::from_context(context, FrameTrust::CallFrameInfo))
}

fn callee_forwarded_regs(valid: &MinidumpContextValidity) -> HashSet<&'static str> {
    match valid {
        MinidumpContextValidity::All => CALLEE_SAVED_REGS.iter().copied().collect(),
        MinidumpContextValidity::Some(ref which) => CALLEE_SAVED_REGS
            .iter()
            .filter(|&reg| which.contains(reg))
            .copied()
            .collect(),
    }
}

fn get_caller_by_frame_pointer<P>(
    ctx: &CONTEXT_AMD64,
    args: &GetCallerFrameArgs<'_, P>,
) -> Option<StackFrame>
where
    P: SymbolProvider + Sync,
{
    let stack_memory = args.stack_memory;
    // On Windows x64, frame-pointer unwinding purely with the data on the stack
    // is not possible, as proper unwinding requires access to `UNWIND_INFO`,
    // because the frame pointer does not necessarily point to the end of the
    // frame.
    // In particular, the docs state that:
    // > [The frame register] offset permits pointing the FP register into the
    // > middle of the local stack allocation [...]
    // https://docs.microsoft.com/en-us/cpp/build/exception-handling-x64
    if args.system_info.os == Os::Windows {
        return None;
    }

    trace!("trying frame pointer");
    if let MinidumpContextValidity::Some(ref which) = args.valid() {
        if !which.contains(FRAME_POINTER_REGISTER) {
            return None;
        }
        if !which.contains(STACK_POINTER_REGISTER) {
            return None;
        }
    }

    let last_bp = ctx.rbp;
    let last_sp = ctx.rsp;
    // Assume that the standard %bp-using x64 calling convention is in
    // use.
    //
    // The typical x64 calling convention, when frame pointers are present,
    // is for the calling procedure to use CALL, which pushes the return
    // address onto the stack and sets the instruction pointer (%ip) to
    // the entry point of the called routine.  The called routine then
    // PUSHes the calling routine's frame pointer (%bp) onto the stack
    // before copying the stack pointer (%sp) to the frame pointer (%bp).
    // Therefore, the calling procedure's frame pointer is always available
    // by dereferencing the called procedure's frame pointer, and the return
    // address is always available at the memory location immediately above
    // the address pointed to by the called procedure's frame pointer.  The
    // calling procedure's stack pointer (%sp) is 2 pointers higher than the
    // value of the called procedure's frame pointer at the time the calling
    // procedure made the CALL: 1 pointer for the return address pushed by the
    // CALL itself, and 1 pointer for the callee's` PUSH of the caller's frame
    // pointer.
    //
    // %ip_new = *(%bp_old + ptr)
    // %bp_new = *(%bp_old)
    // %sp_new = %bp_old + ptr*2

    if last_bp >= u64::MAX - POINTER_WIDTH * 2 {
        // Although this code generally works fine if the pointer math overflows,
        // debug builds will still panic, and this guard protects against it without
        // drowning the rest of the code in checked_add.
        return None;
    }
    let caller_ip = stack_memory.get_memory_at_address(last_bp + POINTER_WIDTH)?;
    let caller_bp = stack_memory.get_memory_at_address(last_bp)?;
    let caller_sp = last_bp + POINTER_WIDTH * 2;

    // If the recovered ip is not a canonical address it can't be
    // the return address, so bp must not have been a frame pointer.

    // Since we're assuming coherent frame pointers, check that the frame pointers
    // and stack pointers are well-ordered.
    if caller_sp <= last_bp || caller_bp < caller_sp {
        trace!("rejecting frame pointer result for unreasonable frame pointer");
        return None;
    }
    // Since we're assuming coherent frame pointers, check that the resulting
    // frame pointer is still inside stack memory.
    let _unused: Pointer = stack_memory.get_memory_at_address(caller_bp)?;
    // Don't accept obviously wrong instruction pointers.
    if is_non_canonical(caller_ip) {
        trace!("rejecting frame pointer result for unreasonable instruction pointer");
        return None;
    }
    // Don't accept obviously wrong stack pointers.
    if !stack_seems_valid(caller_sp, last_sp, stack_memory) {
        trace!("rejecting frame pointer result for unreasonable stack pointer");
        return None;
    }

    trace!(
        "frame pointer seems valid -- caller_ip: 0x{:016x}, caller_sp: 0x{:016x}",
        caller_ip,
        caller_sp,
    );

    let caller_ctx = CONTEXT_AMD64 {
        rip: caller_ip,
        rsp: caller_sp,
        rbp: caller_bp,
        ..CONTEXT_AMD64::default()
    };
    let mut valid = HashSet::new();
    valid.insert(INSTRUCTION_REGISTER);
    valid.insert(STACK_POINTER_REGISTER);
    valid.insert(FRAME_POINTER_REGISTER);
    let context = MinidumpContext {
        raw: MinidumpRawContext::Amd64(caller_ctx),
        valid: MinidumpContextValidity::Some(valid),
    };
    Some(StackFrame::from_context(context, FrameTrust::FramePointer))
}

async fn get_caller_by_scan<P>(
    ctx: &CONTEXT_AMD64,
    args: &GetCallerFrameArgs<'_, P>,
) -> Option<StackFrame>
where
    P: SymbolProvider + Sync,
{
    trace!("trying scan");
    let stack_memory = args.stack_memory;

    // Stack scanning is just walking from the end of the frame until we encounter
    // a value on the stack that looks like a pointer into some code (it's an address
    // in a range covered by one of our modules). If we find such an instruction,
    // we assume it's an ip value that was pushed by the CALL instruction that created
    // the current frame. The next frame is then assumed to end just before that
    // ip value.
    let last_bp = match args.valid() {
        MinidumpContextValidity::All => Some(ctx.rbp),
        MinidumpContextValidity::Some(ref which) => {
            if !which.contains(STACK_POINTER_REGISTER) {
                trace!("cannot scan without stack pointer");
                return None;
            }
            if which.contains(FRAME_POINTER_REGISTER) {
                Some(ctx.rbp)
            } else {
                None
            }
        }
    };
    let last_sp = ctx.rsp;

    // Number of pointer-sized values to scan through in our search.
    let default_scan_range = 40;
    let extended_scan_range = default_scan_range * 4;

    // Breakpad devs found that the first frame of an unwind can be really messed up,
    // and therefore benefits from a longer scan. Let's do it too.
    let scan_range = if let FrameTrust::Context = args.callee_frame.trust {
        extended_scan_range
    } else {
        default_scan_range
    };

    for i in 0..scan_range {
        let address_of_ip = last_sp.checked_add(i * POINTER_WIDTH)?;
        let caller_ip = stack_memory.get_memory_at_address(address_of_ip)?;
        if instruction_seems_valid(caller_ip, args.modules, args.symbol_provider).await {
            // ip is pushed by CALL, so sp is just address_of_ip + ptr
            let caller_sp = address_of_ip.checked_add(POINTER_WIDTH)?;

            // Try to restore bp as well. This can be possible in two cases:
            //
            // 1. This function has the standard prologue that pushes bp and
            //    sets bp = sp. If this is the case, then the current bp should be
            //    immediately after (before in memory) address_of_ip.
            //
            // 2. This function does not use bp, and has just preserved it
            //    from the caller. If this is the case, bp should be before
            //    (after in memory) address_of_ip.
            //
            // We then try our best to eliminate bogus-looking bp's with some
            // simple heuristics like "is a valid stack address".
            let mut caller_bp = None;

            // This value was specifically computed for x86 frames (see the x86
            // impl for details), but 128 KB is still an extremely generous
            // frame size on x64.
            const MAX_REASONABLE_GAP_BETWEEN_FRAMES: Pointer = 128 * 1024;

            // NOTE: minor divergence from the x86 impl here: for whatever
            // reason the x64 breakpad tests only work if we gate option (1) on
            // having a valid `bp` that points next to address_of_ip already.
            // It's unclear why, perhaps the test is buggy, but for now we
            // preserve that behaviour.
            if let Some(last_bp) = last_bp {
                // If we're on the first iteration of the scan, there can't possibly be a
                // frame pointer, because the entire stack frame is taken up by the return
                // pointer. And if we're not on the first iteration, then the last iteration
                // already loaded the location we expect the frame pointer to be in, so we can
                // unconditionally load it here.
                if i > 0 {
                    let address_of_bp = address_of_ip - POINTER_WIDTH;
                    // Can assume this resolves because we already walked over it when
                    // checking address_of_ip values.
                    let bp = stack_memory.get_memory_at_address(address_of_bp)?;
                    if last_bp == address_of_bp
                        && bp > address_of_ip
                        && bp - address_of_bp <= MAX_REASONABLE_GAP_BETWEEN_FRAMES
                    {
                        // Final sanity check that resulting bp is still inside stack memory.
                        if stack_memory.get_memory_at_address::<Pointer>(bp).is_some() {
                            caller_bp = Some(bp);
                        }
                    } else if last_bp >= caller_sp {
                        // Don't sanity check that the address is inside the stack? Hmm.
                        caller_bp = Some(last_bp);
                    }
                }
            }

            trace!(
                "scan seems valid -- caller_ip: 0x{:08x}, caller_sp: 0x{:08x}",
                caller_ip,
                caller_sp,
            );

            let caller_ctx = CONTEXT_AMD64 {
                rip: caller_ip,
                rsp: caller_sp,
                rbp: caller_bp.unwrap_or(0),
                ..CONTEXT_AMD64::default()
            };
            let mut valid = HashSet::new();
            valid.insert(INSTRUCTION_REGISTER);
            valid.insert(STACK_POINTER_REGISTER);
            if caller_bp.is_some() {
                valid.insert(FRAME_POINTER_REGISTER);
            }
            let context = MinidumpContext {
                raw: MinidumpRawContext::Amd64(caller_ctx),
                valid: MinidumpContextValidity::Some(valid),
            };
            return Some(StackFrame::from_context(context, FrameTrust::Scan));
        }
    }

    None
}

/// The most strict validation we have for instruction pointers.
///
/// This is only used for stack-scanning, because it's explicitly
/// trying to distinguish between total garbage and correct values.
/// cfi and frame_pointer approaches do not use this validation
/// because by default they're working with plausible/trustworthy
/// data.
///
/// Specifically, not using this validation allows cfi/fp methods
/// to unwind through frames we don't have mapped modules for (such as
/// OS APIs). This may seem confusing since we obviously don't have cfi
/// for unmapped modules!
///
/// The way this works is that we will use cfi to unwind some frame we
/// know about and *end up* in a function we know nothing about, but with
/// all the right register values. At this point, frame pointers will
/// often do the correct thing even though we don't know what code we're
/// in -- until we get back into code we do know about and cfi kicks back in.
/// At worst, this sets scanning up in a better position for success!
///
/// If we applied this more rigorous validation to cfi/fp methods, we
/// would just discard the correct register values from the known frame
/// and immediately start doing unreliable scans.
async fn instruction_seems_valid<P>(
    instruction: Pointer,
    modules: &MinidumpModuleList,
    symbol_provider: &P,
) -> bool
where
    P: SymbolProvider + Sync,
{
    if is_non_canonical(instruction) || instruction == 0 {
        return false;
    }

    super::instruction_seems_valid_by_symbols(instruction, modules, symbol_provider).await
}

fn stack_seems_valid(
    caller_sp: Pointer,
    callee_sp: Pointer,
    stack_memory: UnifiedMemory<'_, '_>,
) -> bool {
    // The stack shouldn't *grow* when we unwind
    if caller_sp <= callee_sp {
        return false;
    }

    // The stack pointer should be in the stack
    stack_memory
        .get_memory_at_address::<Pointer>(caller_sp)
        .is_some()
}

fn is_non_canonical(ptr: Pointer) -> bool {
    // x64 has the notion of a "canonical address", as a result of only 48 bits
    // of a pointer actually being used, because this is all that a 4-level page
    // table can support. A canonical address copies bit 47 to all the otherwise
    // unused high bits. This creates two ranges where no valid pointers should
    // ever exist.
    //
    // Note that as of this writing, 5-level page tables *do* exist, and when enabled
    // 57 bits are used. However modern JS engines rely on only 48 bits being used
    // to perform "NaN boxing" optimizations, so it's reasonable to assume
    // by default that only 4-level page tables are used. (Even if enabled at
    // the system level, Linux only exposes non-48-bit pointers to a process
    // if that process explicitly opts in with a special operation.)
    ptr > 0x7FFFFFFFFFFF && ptr < 0xFFFF800000000000
}

pub async fn get_caller_frame<P>(
    ctx: &CONTEXT_AMD64,
    args: &GetCallerFrameArgs<'_, P>,
) -> Option<StackFrame>
where
    P: SymbolProvider + Sync,
{
    // .await doesn't like closures, so don't use Option chaining
    let mut frame = None;
    if frame.is_none() {
        frame = get_caller_by_cfi(ctx, args).await;
    }
    if frame.is_none() {
        frame = get_caller_by_frame_pointer(ctx, args);
    }
    if frame.is_none() {
        frame = get_caller_by_scan(ctx, args).await;
    }
    let mut frame = frame?;

    // We now check the frame to see if it looks like unwinding is complete,
    // based on the frame we computed having a nonsense value. Returning
    // None signals to the unwinder to stop unwinding.

    // if the instruction is within the first ~page of memory, it's basically
    // null, and we can assume unwinding is complete.
    if frame.context.get_instruction_pointer() < 4096 {
        trace!("instruction pointer was nullish, assuming unwind complete");
        return None;
    }
    // If the new stack pointer is at a lower address than the old,
    // then that's clearly incorrect. Treat this as end-of-stack to
    // enforce progress and avoid infinite loops.
    if frame.context.get_stack_pointer() <= ctx.rsp {
        trace!("stack pointer went backwards, assuming unwind complete");
        return None;
    }

    // Ok, the frame now seems well and truly valid, do final cleanup.

    // A caller's ip is the return address, which is the instruction
    // *after* the CALL that caused us to arrive at the callee. Set
    // the value to one less than that, so it points within the
    // CALL instruction. This is important because we use this value
    // to lookup the CFI we need to unwind the next frame.
    let ip = frame.context.get_instruction_pointer();
    frame.instruction = ip - 1;

    Some(frame)
}
