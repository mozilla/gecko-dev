// Copyright 2015 Ted Mielczarek. See the COPYRIGHT
// file at the top-level directory of this distribution.

// NOTE: arm64_old.rs and arm64.rs should be identical except for the names of
// their context types.

use super::impl_prelude::*;
use minidump::{
    CpuContext, MinidumpContext, MinidumpContextValidity, MinidumpModuleList, MinidumpRawContext,
    Module,
};
use std::collections::HashSet;
use tracing::trace;

type ArmContext = minidump::format::CONTEXT_ARM64;
type Pointer = <ArmContext as CpuContext>::Register;
type Registers = minidump::format::Arm64RegisterNumbers;

const POINTER_WIDTH: Pointer = std::mem::size_of::<Pointer>() as Pointer;
const FRAME_POINTER: &str = Registers::FramePointer.name();
const LINK_REGISTER: &str = Registers::LinkRegister.name();
const STACK_POINTER: &str = "sp";
const PROGRAM_COUNTER: &str = "pc";
const CALLEE_SAVED_REGS: &[&str] = &[
    "x19", "x20", "x21", "x22", "x23", "x24", "x25", "x26", "x27", "x28", "fp",
];

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
    let new_valid = MinidumpContextValidity::Some(stack_walker.caller_validity);

    // Apply ptr auth stripping
    let caller_pc = ptr_auth_strip(args.modules, caller_pc);
    stack_walker
        .caller_ctx
        .set_register(PROGRAM_COUNTER, caller_pc);
    // Nothing should really ever restore lr, but CFI is more magic so whatever sure
    if let Some(lr) = stack_walker
        .caller_ctx
        .get_register(LINK_REGISTER, &new_valid)
    {
        stack_walker
            .caller_ctx
            .set_register(LINK_REGISTER, ptr_auth_strip(args.modules, lr));
    }
    if let Some(fp) = stack_walker
        .caller_ctx
        .get_register(FRAME_POINTER, &new_valid)
    {
        stack_walker
            .caller_ctx
            .set_register(FRAME_POINTER, ptr_auth_strip(args.modules, fp));
    }

    trace!(
        "cfi evaluation was successful -- caller_pc: 0x{:016x}, caller_sp: 0x{:016x}",
        caller_pc,
        caller_sp,
    );

    // Do absolutely NO validation! Yep! As long as CFI evaluation succeeds
    // (which does include pc and sp resolving), just blindly assume the
    // values are correct. I Don't Like This, but it's what breakpad does and
    // we should start with a baseline of parity.

    // FIXME?: for whatever reason breakpad actually does block on the address
    // being canonical *ONLY* for arm64, which actually rejects null pc early!
    // Let's not do that to keep our code more uniform.

    let context = MinidumpContext {
        raw: MinidumpRawContext::Arm64(stack_walker.caller_ctx),
        valid: new_valid,
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
    trace!("trying frame pointer");
    // Ok so there exists 3 kinds of stackframes in ARM64:
    //
    // * stackless leaves
    // * stackful leaves
    // * normal frames
    //
    //
    // # Normal Frames
    //
    // Let's start with normal frames. In the standard calling convention, the following happens:
    //
    // lr := return_address   (performed implicitly by ARM's function call instruction)
    // PUSH fp, lr            (save fp and lr to the stack -- ARM64 pushes in pairs)
    // fp := sp               (update the frame pointer to the current stack pointer)
    //
    // So to restore the caller's registers, we have:
    //
    // pc := *(fp + ptr)      (this will get the return address, usual offset caveats apply)
    // sp := fp + ptr*2
    // fp := *fp
    //
    // Note that although we push lr, we don't restore lr. That's because lr is just our
    // return address, and is therefore essentially a "saved" pc. lr is caller-saved *and*
    // automatically overwritten by every CALL, so the callee (the frame we're unwinding right now)
    // has no business ever knowing it, let alone restoring it. lr is generally just saved
    // immediately and then used as a free general purpose register, and therefore will generally
    // contain random garbage unrelated to unwinding.
    //
    //
    // # Leaf Functions
    //
    // Now leaf functions are a bit messier. These are functions which don't call other functions
    // and therefore don't actually ever need to save lr or fp. As such, they can be entirely
    // stackless, although they don't have to be. So calling a leaf function is just:
    //
    // lr := return_address
    // <possibly some pushes, but maybe not>
    //
    // And to restore the caller's registers, we have:
    //
    // pc := lr
    // sp := sp - <some arbitrary value>
    // fp := fp
    //
    // Unfortunately, we're unaware of any way to "detect" that a function is a leaf or not
    // without symbols/cfi just telling you that. Since we're in frame pointer unwinding,
    // we probably don't have those available! And even if we did, we still wouldn't know if
    // the frame was stackless or not, so we wouldn't know how to restore sp reliably and might
    // get the stack in a weird state for subsequent (possibly CFI-based) frames.
    // Also, if we incorrectly guess a frame is a leaf, we'll also use a probably-random-garbage
    // lr as a pc and potentially halluncinate a bunch.
    //
    //
    // # Conclusion
    //
    // At the moment we think it's safest/best to just always assume we're unwinding a normal
    // frame. Statistically this is true (most frames are, even if they happen to be at the
    // top of the stack when we crash), and if the frame *is* a leaf then our `fp` is likely
    // to be the correct fp of the next frame. This will effectively result in us unwinding
    // our caller instead of ourselves, causing the caller to be omitted from the backtrace
    // but otherwise perfectly syncing up for the rest of the frames.
    let last_fp = ctx.get_register(FRAME_POINTER, args.valid())?;
    let last_sp = ctx.get_register(STACK_POINTER, args.valid())?;

    if last_fp >= u64::MAX - POINTER_WIDTH * 2 {
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
            args.stack_memory.get_memory_at_address(last_fp)?,
            args.stack_memory
                .get_memory_at_address(last_fp + POINTER_WIDTH)?,
            last_fp + POINTER_WIDTH * 2,
        )
    };
    let caller_fp = ptr_auth_strip(args.modules, caller_fp);
    let caller_pc = ptr_auth_strip(args.modules, caller_pc);

    // Don't accept obviously wrong instruction pointers.
    if is_non_canonical(caller_pc) {
        trace!("rejecting frame pointer result for unreasonable instruction pointer");
        return None;
    }

    // Don't actually validate that the stack makes sense (duplicating breakpad behaviour).

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
        raw: MinidumpRawContext::Arm64(caller_ctx),
        valid: MinidumpContextValidity::Some(valid),
    };
    Some(StackFrame::from_context(context, FrameTrust::FramePointer))
}

fn ptr_auth_strip(modules: &MinidumpModuleList, ptr: Pointer) -> Pointer {
    // ARMv8.3 introduced a code hardening system called "Pointer Authentication"
    // which is used on Apple platforms. It adds some extra high bits to the
    // several pointers when they get pushed to memory, including the return
    // address (lr) and frame pointer (fp), which both get pushed at the start
    // of most non-leaf functions.
    //
    // We lack some of the proper context to implement the "strip" primitive, because
    // the amount of bits that are "real" pointer depends on various extensions like
    // pointer tagging and how big page tables are. If we allocate too many bits to
    // "real" then we can get ptr_auth bits in our pointers, and if we allocate too
    // few we can end up truncating our pointers. Thankfully we'll usually have a bit
    // of margin from pointers not having the highest real bits set.
    //
    // To help us guess, we have a few pieces of information:
    //
    // * Apple seems to default to a 17/47 split, so 47 bits for "real" is a good baseline
    // * We know the address ranges of various loaded (and unloaded modules)
    // * We know the address range of the stacks
    // * We *can* know the address range of some sections of the heap (MemoryList)
    // * We *can* know the page mappings (MemoryInfo)
    //
    // Right now we only incorporate the first two. Ideally we would process all those sources
    // once at the start of stack walking and pass it down to the ARM stackwalker but that's
    // a lot of annoying rewiring that won't necessarily improve results.
    let apple_default_max_addr = (1 << 47) - 1;
    let max_module_addr = modules
        .by_addr()
        .next_back()
        .map(|last_module| {
            last_module
                .base_address()
                .saturating_add(last_module.size())
        })
        .unwrap_or(0);
    let max_addr = u64::max(apple_default_max_addr, max_module_addr);

    // We can convert a "highest" address into a suitable mask by getting the next_power_of_two
    // (a single bit >= the max) and subtracting one from it (producing all 1's <= that bit).
    // There are two corner cases to this:
    //
    // * the next_power_of_two being 2^65, in which case our mask should be !0 (all ones)
    // * the max addr being a power of two already means we will actually lose that one value
    //
    // The first case is handled by using checked_next_power_of_two. The second case isn't really
    // handled by it very improbable. We do however make sure the apple max isn't a power of two.
    let mask = max_addr
        .checked_next_power_of_two()
        .map(|high_bit| high_bit - 1)
        .unwrap_or(!0);

    // In principle, if we've done a good job of computing the mask, we can apply it regardless
    // of if there's any ptr auth bits. Either it will clear the auth or be a noop. We don't
    // check if this messes up, because there's too many subtleties like JITed code to reliably
    // detect this going awry.
    ptr & mask
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
        let caller_pc = args.stack_memory.get_memory_at_address(address_of_pc)?;
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
                raw: MinidumpRawContext::Arm64(caller_ctx),
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

fn is_non_canonical(instruction: Pointer) -> bool {
    // Reject instructions in the first page or above the user-space threshold.
    !(0x1000..=0x000fffffffffffff).contains(&instruction)
}

/*
// ARM64 is currently hyper-permissive, so we don't use this,
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
    let last_sp = ctx.get_register_always("sp");
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
    // the value to 4 less than that, so it points to the CALL instruction
    // (arm64 instructions are all 4 bytes wide). This is important because
    // we use this value to lookup the CFI we need to unwind the next frame.
    let ip = frame.context.get_instruction_pointer();
    frame.instruction = ip - 4;

    Some(frame)
}
