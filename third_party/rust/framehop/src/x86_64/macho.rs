use super::arch::ArchX86_64;
use super::unwind_rule::UnwindRuleX86_64;
use crate::instruction_analysis::InstructionAnalysis;
use crate::macho::{CompactUnwindInfoUnwinderError, CompactUnwindInfoUnwinding, CuiUnwindResult};
use macho_unwind_info::opcodes::{OpcodeX86_64, RegisterNameX86_64};
use macho_unwind_info::Function;

impl CompactUnwindInfoUnwinding for ArchX86_64 {
    fn unwind_frame(
        function: Function,
        is_first_frame: bool,
        address_offset_within_function: usize,
        function_bytes: Option<&[u8]>,
    ) -> Result<CuiUnwindResult<UnwindRuleX86_64>, CompactUnwindInfoUnwinderError> {
        let opcode = OpcodeX86_64::parse(function.opcode);
        if is_first_frame {
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
                if opcode == OpcodeX86_64::Null
                    && function_bytes.starts_with(&[0x55, 0x48, 0x89, 0xe5])
                {
                    // The function is uncovered but it has a `push rbp; mov rbp, rsp` prologue.
                    return Ok(CuiUnwindResult::ExecRule(UnwindRuleX86_64::UseFramePointer));
                }
            }
            if opcode == OpcodeX86_64::Null {
                return Ok(CuiUnwindResult::ExecRule(UnwindRuleX86_64::JustReturn));
            }
        }

        // At this point we know with high certainty that we are in a function body.
        let r = match opcode {
            OpcodeX86_64::Null => {
                return Err(CompactUnwindInfoUnwinderError::FunctionHasNoInfo);
            }
            OpcodeX86_64::FramelessImmediate {
                stack_size_in_bytes,
                saved_regs,
            } => {
                if stack_size_in_bytes == 8 {
                    CuiUnwindResult::ExecRule(UnwindRuleX86_64::JustReturn)
                } else {
                    let bp_positon_from_outside = saved_regs
                        .iter()
                        .rev()
                        .flatten()
                        .position(|r| *r == RegisterNameX86_64::Rbp);
                    match bp_positon_from_outside {
                        Some(pos) => {
                            let bp_offset_from_sp =
                                stack_size_in_bytes as i32 - 2 * 8 - pos as i32 * 8;
                            let bp_storage_offset_from_sp_by_8 =
                                i16::try_from(bp_offset_from_sp / 8).map_err(|_| {
                                    CompactUnwindInfoUnwinderError::BpOffsetDoesNotFit
                                })?;
                            CuiUnwindResult::ExecRule(UnwindRuleX86_64::OffsetSpAndRestoreBp {
                                sp_offset_by_8: stack_size_in_bytes / 8,
                                bp_storage_offset_from_sp_by_8,
                            })
                        }
                        None => CuiUnwindResult::ExecRule(UnwindRuleX86_64::OffsetSp {
                            sp_offset_by_8: stack_size_in_bytes / 8,
                        }),
                    }
                }
            }
            OpcodeX86_64::FramelessIndirect {
                immediate_offset_from_function_start,
                stack_adjust_in_bytes,
                saved_regs,
            } => {
                let function_bytes = function_bytes.ok_or(
                    CompactUnwindInfoUnwinderError::NoTextBytesToLookUpIndirectStackOffset,
                )?;
                let sub_immediate_bytes = function_bytes
                    .get(
                        immediate_offset_from_function_start as usize
                            ..immediate_offset_from_function_start as usize + 4,
                    )
                    .ok_or(CompactUnwindInfoUnwinderError::IndirectStackOffsetOutOfBounds)?;
                let sub_immediate = u32::from_le_bytes([
                    sub_immediate_bytes[0],
                    sub_immediate_bytes[1],
                    sub_immediate_bytes[2],
                    sub_immediate_bytes[3],
                ]);
                let stack_size_in_bytes =
                    sub_immediate
                        .checked_add(stack_adjust_in_bytes.into())
                        .ok_or(CompactUnwindInfoUnwinderError::StackAdjustOverflow)?;
                let sp_offset_by_8 = u16::try_from(stack_size_in_bytes / 8)
                    .map_err(|_| CompactUnwindInfoUnwinderError::StackSizeDoesNotFit)?;
                let bp_positon_from_outside = saved_regs
                    .iter()
                    .rev()
                    .flatten()
                    .position(|r| *r == RegisterNameX86_64::Rbp);
                match bp_positon_from_outside {
                    Some(pos) => {
                        let bp_offset_from_sp = stack_size_in_bytes as i32 - 2 * 8 - pos as i32 * 8;
                        let bp_storage_offset_from_sp_by_8 =
                            i16::try_from(bp_offset_from_sp / 8)
                                .map_err(|_| CompactUnwindInfoUnwinderError::BpOffsetDoesNotFit)?;
                        CuiUnwindResult::ExecRule(UnwindRuleX86_64::OffsetSpAndRestoreBp {
                            sp_offset_by_8,
                            bp_storage_offset_from_sp_by_8,
                        })
                    }
                    None => {
                        CuiUnwindResult::ExecRule(UnwindRuleX86_64::OffsetSp { sp_offset_by_8 })
                    }
                }
            }
            OpcodeX86_64::Dwarf { eh_frame_fde } => CuiUnwindResult::NeedDwarf(eh_frame_fde),
            OpcodeX86_64::FrameBased { .. } => {
                CuiUnwindResult::ExecRule(UnwindRuleX86_64::UseFramePointer)
            }
            OpcodeX86_64::UnrecognizedKind(kind) => {
                return Err(CompactUnwindInfoUnwinderError::BadOpcodeKind(kind))
            }
            OpcodeX86_64::InvalidFrameless => {
                return Err(CompactUnwindInfoUnwinderError::InvalidFrameless)
            }
        };
        Ok(r)
    }

    fn rule_for_stub_helper(
        offset: u32,
    ) -> Result<CuiUnwindResult<UnwindRuleX86_64>, CompactUnwindInfoUnwinderError> {
        //     shared:
        //  +0x0 235cc4  4C 8D 1D 3D 03 04 00   lea  r11, qword [dyld_stub_binder_276000+8]
        //  +0x7 235ccb  41 53                  push  r11
        //  +0x9 235ccd  FF 25 2D 03 04 00      jmp  qword [dyld_stub_binder_276000] ; tail call
        //  +0xf 235cd3  90                     nop
        //    first stub:
        // +0x10 235cd4  68 F1 61 00 00         push  0x61f1
        // +0x15 235cd9  E9 E6 FF FF FF         jmp  0x235cc4 ; jump to shared
        //    second stub:
        // +0x1a 235cde  68 38 62 00 00         push  0x6238
        // +0x1f 235ce3  E9 DC FF FF FF         jmp  0x235cc4 ; jump to shared
        let rule = if offset < 0x7 {
            // pop 1 and return
            UnwindRuleX86_64::OffsetSp { sp_offset_by_8: 2 }
        } else if offset < 0x10 {
            // pop 2 and return
            UnwindRuleX86_64::OffsetSp { sp_offset_by_8: 3 }
        } else {
            let offset_after_shared = offset - 0x10;
            let offset_within_stub = offset_after_shared % 10;
            if offset_within_stub < 5 {
                UnwindRuleX86_64::JustReturn
                // just return
            } else {
                // pop 1 and return
                UnwindRuleX86_64::OffsetSp { sp_offset_by_8: 2 }
            }
        };
        Ok(CuiUnwindResult::ExecRule(rule))
    }
}
