use super::arch::ArchAarch64;
use super::unwind_rule::UnwindRuleAarch64;
use crate::instruction_analysis::InstructionAnalysis;
use crate::macho::{CompactUnwindInfoUnwinderError, CompactUnwindInfoUnwinding, CuiUnwindResult};
use macho_unwind_info::opcodes::OpcodeArm64;
use macho_unwind_info::Function;

impl CompactUnwindInfoUnwinding for ArchAarch64 {
    fn unwind_frame(
        function: Function,
        is_first_frame: bool,
        address_offset_within_function: usize,
        function_bytes: Option<&[u8]>,
    ) -> Result<CuiUnwindResult<UnwindRuleAarch64>, CompactUnwindInfoUnwinderError> {
        let opcode = OpcodeArm64::parse(function.opcode);
        if is_first_frame {
            if opcode == OpcodeArm64::Null {
                return Ok(CuiUnwindResult::ExecRule(UnwindRuleAarch64::NoOp));
            }
            // The pc might be in a prologue or an epilogue. The compact unwind info format ignores
            // prologues and epilogues; the opcodes only describe the function body. So we do some
            // instruction analysis to check for prologues and epilogues.
            if let Some(function_bytes) = function_bytes {
                if let Some(rule) = Self::rule_from_instruction_analysis(
                    function_bytes,
                    address_offset_within_function,
                ) {
                    // We are inside a prologue / epilogue. Ignore the opcode and use the rule from
                    // instruction analysis.
                    return Ok(CuiUnwindResult::ExecRule(rule));
                }
            }
        }

        // At this point we know with high certainty that we are in a function body.
        let r = match opcode {
            OpcodeArm64::Null => {
                return Err(CompactUnwindInfoUnwinderError::FunctionHasNoInfo);
            }
            OpcodeArm64::Frameless {
                stack_size_in_bytes,
            } => {
                if is_first_frame {
                    if stack_size_in_bytes == 0 {
                        CuiUnwindResult::ExecRule(UnwindRuleAarch64::NoOp)
                    } else {
                        CuiUnwindResult::ExecRule(UnwindRuleAarch64::OffsetSp {
                            sp_offset_by_16: stack_size_in_bytes / 16,
                        })
                    }
                } else {
                    return Err(CompactUnwindInfoUnwinderError::CallerCannotBeFrameless);
                }
            }
            OpcodeArm64::Dwarf { eh_frame_fde } => CuiUnwindResult::NeedDwarf(eh_frame_fde),
            OpcodeArm64::FrameBased { .. } => {
                CuiUnwindResult::ExecRule(UnwindRuleAarch64::UseFramePointer)
            }
            OpcodeArm64::UnrecognizedKind(kind) => {
                return Err(CompactUnwindInfoUnwinderError::BadOpcodeKind(kind))
            }
        };
        Ok(r)
    }

    fn rule_for_stub_helper(
        offset: u32,
    ) -> Result<CuiUnwindResult<UnwindRuleAarch64>, CompactUnwindInfoUnwinderError> {
        //    shared:
        //  +0x0  1d309c  B1 94 48 10        adr        x17, #0x100264330
        //  +0x4  1d30a0  1F 20 03 D5        nop
        //  +0x8  1d30a4  F0 47 BF A9        stp        x16, x17, [sp, #-0x10]!
        //  +0xc  1d30a8  1F 20 03 D5        nop
        // +0x10  1d30ac  F0 7A 32 58        ldr        x16, #dyld_stub_binder_100238008
        // +0x14  1d30b0  00 02 1F D6        br         x16
        //     first stub:
        // +0x18  1d30b4  50 00 00 18        ldr        w16, =0x1800005000000000
        // +0x1c  1d30b8  F9 FF FF 17        b          0x1001d309c
        // +0x20  1d30bc  00 00 00 00        (padding)
        //     second stub:
        // +0x24  1d30c0  50 00 00 18        ldr        w16, =0x1800005000000012
        // +0x28  1d30c4  F6 FF FF 17        b          0x1001d309c
        // +0x2c  1d30c8  00 00 00 00        (padding)
        let rule = if offset < 0xc {
            // Stack pointer hasn't been touched, just follow lr
            UnwindRuleAarch64::NoOp
        } else if offset < 0x18 {
            // Add 0x10 to the stack pointer and follow lr
            UnwindRuleAarch64::OffsetSp { sp_offset_by_16: 1 }
        } else {
            // Stack pointer hasn't been touched, just follow lr
            UnwindRuleAarch64::NoOp
        };
        Ok(CuiUnwindResult::ExecRule(rule))
    }
}
