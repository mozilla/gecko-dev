// Copyright 2015 Ted Mielczarek. See the COPYRIGHT
// file at the top-level directory of this distribution.

// NOTE: arm64_old.rs and arm64.rs should be identical except for the names of
// their context types.

use super::impl_prelude::*;
use minidump::system_info::Os;
use minidump::{
    CpuContext, MinidumpContext, MinidumpContextValidity, MinidumpModuleList, MinidumpRawContext,
};
use std::collections::HashSet;
use tracing::trace;

type ArmContext = minidump::format::CONTEXT_ARM;
type Pointer = <ArmContext as CpuContext>::Register;
type Registers = minidump::format::ArmRegisterNumbers;

const POINTER_WIDTH: Pointer = std::mem::size_of::<Pointer>() as Pointer;
const FRAME_POINTER: &str = Registers::FramePointer.name();
const STACK_POINTER: &str = Registers::StackPointer.name();
const PROGRAM_COUNTER: &str = Registers::ProgramCounter.name();
const _LINK_REGISTER: &str = Registers::LinkRegister.name();
const CALLEE_SAVED_REGS: &[&str] = &["r4", "r5", "r6", "r7", "r8", "r9", "r10", "fp"];

async fn get_caller_by_cfi<P>(
    ctx: &ArmContext,
    args: &GetCallerFrameArgs<'_, P>,
) -> Option<StackFrame>
where
    P: SymbolProvider + Sync,
{
    trace!("trying cfi");

    let _last_sp = ctx.get_register(STACK_POINTER, args.valid())?;

    let mut stack_walker = CfiStackWalker::from_ctx_and_args(ctx, args, callee_forwarded_regs)?;

    args.symbol_provider
        .walk_frame(stack_walker.module, &mut stack_walker)
        .await?;
    let caller_pc = stack_walker.caller_ctx.get_register_always(PROGRAM_COUNTER);
    let caller_sp = stack_walker.caller_ctx.get_register_always(STACK_POINTER);

    trace!(
        "cfi evaluation was successful -- caller_pc: 0x{:016x}, caller_sp: 0x{:016x}",
        caller_pc,
        caller_sp,
    );

    // Do absolutely NO validation! Yep! As long as CFI evaluation succeeds
    // (which does include pc and sp resolving), just blindly assume the
    // values are correct. I Don't Like This, but it's what breakpad does and
    // we should start with a baseline of parity.

    let context = MinidumpContext {
        raw: MinidumpRawContext::Arm(stack_walker.caller_ctx),
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
    ctx: &ArmContext,
    args: &GetCallerFrameArgs<'_, P>,
) -> Option<StackFrame>
where
    P: SymbolProvider + Sync,
{
    // The ARM manual states that:
    // > LR can be used for other purposes when it is not required to support
    // > a return from a subroutine.
    // In other words, we need to be conservative and treat it as a general
    // purpose register. Except on iOS, which has stricter conventions around
    // register use, and does guarantee that LR contains a valid return addr.
    if args.system_info.os != Os::Ios {
        return None;
    }

    trace!("trying frame pointer");
    // Assume that the standard %fp-using ARM calling convention is in use.
    // The main quirk of this ABI is that the return address doesn't need to
    // be restored from the stack -- it's already in the link register (lr).
    // But that means we need to save/restore lr itself so that the *caller's*
    // return address can be recovered.
    //
    // In the standard calling convention, the following happens:
    //
    // lr := return_address   (done implicitly by a call)
    // PUSH fp, lr            (save fp and lr to the stack -- ARM pushes in pairs)
    // fp := sp               (update the frame pointer to the current stack pointer)
    //
    // So to restore the caller's registers, we have:
    //
    // sp := fp + ptr*2
    // pc := *(fp + ptr)
    // fp := *fp
    let last_fp = ctx.get_register(FRAME_POINTER, args.valid())?;
    let last_sp = ctx.get_register(STACK_POINTER, args.valid())?;

    if last_fp >= u32::MAX - POINTER_WIDTH * 2 {
        // Although this code generally works fine if the pointer math overflows,
        // debug builds will still panic, and this guard protects against it without
        // drowning the rest of the code in checked_add.
        return None;
    }
    let (caller_fp, caller_pc, caller_sp) = if last_fp == 0 {
        // In this case we want unwinding to stop. One of the termination conditions in get_caller_frame
        // is that caller_sp <= last_sp. Therefore we can force termination by setting caller_sp = last_sp.
        (0, 0, last_sp)
    } else {
        (
            args.stack_memory.get_memory_at_address(last_fp as u64)?,
            args.stack_memory
                .get_memory_at_address(last_fp as u64 + POINTER_WIDTH as u64)?,
            last_fp + POINTER_WIDTH * 2,
        )
    };

    // Don't do any more validation, just assume it worked.

    trace!(
        "frame pointer seems valid -- caller_pc: 0x{:016x}, caller_sp: 0x{:016x}",
        caller_pc,
        caller_sp,
    );

    let mut caller_ctx = ArmContext::default();
    caller_ctx.set_register(PROGRAM_COUNTER, caller_pc);
    caller_ctx.set_register(FRAME_POINTER, caller_fp);
    caller_ctx.set_register(STACK_POINTER, caller_sp);

    let mut valid = HashSet::new();
    valid.insert(PROGRAM_COUNTER);
    valid.insert(FRAME_POINTER);
    valid.insert(STACK_POINTER);

    let context = MinidumpContext {
        raw: MinidumpRawContext::Arm(caller_ctx),
        valid: MinidumpContextValidity::Some(valid),
    };
    Some(StackFrame::from_context(context, FrameTrust::FramePointer))
}

async fn get_caller_by_scan<P>(
    ctx: &ArmContext,
    args: &GetCallerFrameArgs<'_, P>,
) -> Option<StackFrame>
where
    P: SymbolProvider + Sync,
{
    trace!("trying scan");
    // Stack scanning is just walking from the end of the frame until we encounter
    // a value on the stack that looks like a pointer into some code (it's an address
    // in a range covered by one of our modules). If we find such an instruction,
    // we assume it's an pc value that was pushed by the CALL instruction that created
    // the current frame. The next frame is then assumed to end just before that
    // pc value.
    let last_sp = ctx.get_register(STACK_POINTER, args.valid())?;

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
        let address_of_pc = last_sp.checked_add(i * POINTER_WIDTH)?;
        let caller_pc = args
            .stack_memory
            .get_memory_at_address(address_of_pc as u64)?;
        if instruction_seems_valid(caller_pc, args.modules, args.symbol_provider).await {
            // pc is pushed by CALL, so sp is just address_of_pc + ptr
            let caller_sp = address_of_pc.checked_add(POINTER_WIDTH)?;

            // Don't do any more validation, and don't try to restore fp
            // (that's what breakpad does!)

            trace!(
                "scan seems valid -- caller_pc: 0x{:08x}, caller_sp: 0x{:08x}",
                caller_pc,
                caller_sp,
            );

            let mut caller_ctx = ArmContext::default();
            caller_ctx.set_register(PROGRAM_COUNTER, caller_pc);
            caller_ctx.set_register(STACK_POINTER, caller_sp);

            let mut valid = HashSet::new();
            valid.insert(PROGRAM_COUNTER);
            valid.insert(STACK_POINTER);

            let context = MinidumpContext {
                raw: MinidumpRawContext::Arm(caller_ctx),
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
    super::instruction_seems_valid_by_symbols(instruction as u64, modules, symbol_provider).await
}

/*
// ARM is currently hyper-permissive, so we don't use this,
// but here it is in case we change our minds!
fn stack_seems_valid(
    caller_sp: Pointer,
    callee_sp: Pointer,
    stack_memory: UnifiedMemory<'_, '_>,
) -> bool {
    // The stack shouldn't *grow* when we unwind
    if caller_sp < callee_sp {
        return false;
    }

    // The stack pointer should be in the stack
    stack_memory
        .get_memory_at_address::<Pointer>(caller_sp as u64)
        .is_some()
}
*/

pub async fn get_caller_frame<P>(
    ctx: &ArmContext,
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
    let sp = frame.context.get_stack_pointer();
    let last_sp = ctx.get_register_always("sp") as u64;
    if sp <= last_sp {
        // Arm leaf functions may not actually touch the stack (thanks
        // to the link register allowing you to "push" the return address
        // to a register), so we need to permit the stack pointer to not
        // change for the first frame of the unwind. After that we need
        // more strict validation to avoid infinite loops.
        let is_leaf = args.callee_frame.trust == FrameTrust::Context && sp == last_sp;
        if !is_leaf {
            trace!("stack pointer went backwards, assuming unwind complete");
            return None;
        }
    }

    // Ok, the frame now seems well and truly valid, do final cleanup.

    // A caller's ip is the return address, which is the instruction
    // *after* the CALL that caused us to arrive at the callee. Set
    // the value to 2 less than that, so it points to the CALL instruction
    // (arm instructions are all 2 bytes wide). This is important because
    // we use this value to lookup the CFI we need to unwind the next frame.
    let ip = frame.context.get_instruction_pointer();
    frame.instruction = ip - 2;

    Some(frame)
}
