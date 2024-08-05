use gimli::{
    AArch64, CfaRule, Encoding, EvaluationStorage, Reader, ReaderOffset, Register, RegisterRule,
    UnwindContextStorage, UnwindSection, UnwindTableRow,
};

use super::{arch::ArchAarch64, unwind_rule::UnwindRuleAarch64, unwindregs::UnwindRegsAarch64};

use crate::unwind_result::UnwindResult;

use crate::dwarf::{
    eval_cfa_rule, eval_register_rule, ConversionError, DwarfUnwindRegs, DwarfUnwinderError,
    DwarfUnwinding,
};

impl DwarfUnwindRegs for UnwindRegsAarch64 {
    fn get(&self, register: Register) -> Option<u64> {
        match register {
            AArch64::SP => Some(self.sp()),
            AArch64::X29 => Some(self.fp()),
            AArch64::X30 => Some(self.lr()),
            _ => None,
        }
    }
}

impl DwarfUnwinding for ArchAarch64 {
    fn unwind_frame<F, R, UCS, ES>(
        section: &impl UnwindSection<R>,
        unwind_info: &UnwindTableRow<R::Offset, UCS>,
        encoding: Encoding,
        regs: &mut Self::UnwindRegs,
        is_first_frame: bool,
        read_stack: &mut F,
    ) -> Result<UnwindResult<Self::UnwindRule>, DwarfUnwinderError>
    where
        F: FnMut(u64) -> Result<u64, ()>,
        R: Reader,
        UCS: UnwindContextStorage<R::Offset>,
        ES: EvaluationStorage<R>,
    {
        let cfa_rule = unwind_info.cfa();
        let fp_rule = unwind_info.register(AArch64::X29);
        let lr_rule = unwind_info.register(AArch64::X30);

        match translate_into_unwind_rule(cfa_rule, &fp_rule, &lr_rule) {
            Ok(unwind_rule) => return Ok(UnwindResult::ExecRule(unwind_rule)),
            Err(_err) => {
                // Could not translate into a cacheable unwind rule. Fall back to the generic path.
                // eprintln!("Unwind rule translation failed: {:?}", err);
            }
        }

        let cfa = eval_cfa_rule::<R, _, ES>(section, cfa_rule, encoding, regs)
            .ok_or(DwarfUnwinderError::CouldNotRecoverCfa)?;

        let lr = regs.lr();
        let fp = regs.fp();
        let sp = regs.sp();

        let (fp, lr) = if !is_first_frame {
            if cfa <= sp {
                return Err(DwarfUnwinderError::StackPointerMovedBackwards);
            }
            let fp = eval_register_rule::<R, F, _, ES>(
                section, fp_rule, cfa, encoding, fp, regs, read_stack,
            )
            .ok_or(DwarfUnwinderError::CouldNotRecoverFramePointer)?;
            let lr = eval_register_rule::<R, F, _, ES>(
                section, lr_rule, cfa, encoding, lr, regs, read_stack,
            )
            .ok_or(DwarfUnwinderError::CouldNotRecoverReturnAddress)?;
            (fp, lr)
        } else {
            // For the first frame, be more lenient when encountering errors.
            // TODO: Find evidence of what this gives us. I think on macOS the prologue often has Unknown register rules
            // and we only encounter prologues for the first frame.
            let fp = eval_register_rule::<R, F, _, ES>(
                section, fp_rule, cfa, encoding, fp, regs, read_stack,
            )
            .unwrap_or(fp);
            let lr = eval_register_rule::<R, F, _, ES>(
                section, lr_rule, cfa, encoding, lr, regs, read_stack,
            )
            .unwrap_or(lr);
            (fp, lr)
        };

        regs.set_fp(fp);
        regs.set_sp(cfa);
        regs.set_lr(lr);

        Ok(UnwindResult::Uncacheable(lr))
    }

    fn rule_if_uncovered_by_fde() -> Self::UnwindRule {
        UnwindRuleAarch64::NoOpIfFirstFrameOtherwiseFp
    }
}

fn register_rule_to_cfa_offset<RO: ReaderOffset>(
    rule: &RegisterRule<RO>,
) -> Result<Option<i64>, ConversionError> {
    match *rule {
        RegisterRule::Undefined | RegisterRule::SameValue => Ok(None),
        RegisterRule::Offset(offset) => Ok(Some(offset)),
        _ => Err(ConversionError::RegisterNotStoredRelativeToCfa),
    }
}

fn translate_into_unwind_rule<RO: ReaderOffset>(
    cfa_rule: &CfaRule<RO>,
    fp_rule: &RegisterRule<RO>,
    lr_rule: &RegisterRule<RO>,
) -> Result<UnwindRuleAarch64, ConversionError> {
    match cfa_rule {
        CfaRule::RegisterAndOffset { register, offset } => match *register {
            AArch64::SP => {
                let sp_offset_by_16 =
                    u16::try_from(offset / 16).map_err(|_| ConversionError::SpOffsetDoesNotFit)?;
                let lr_cfa_offset = register_rule_to_cfa_offset(lr_rule)?;
                let fp_cfa_offset = register_rule_to_cfa_offset(fp_rule)?;
                match (lr_cfa_offset, fp_cfa_offset) {
                    (None, Some(_)) => Err(ConversionError::RestoringFpButNotLr),
                    (None, None) => {
                        if let RegisterRule::Undefined = lr_rule {
                            // If the return address is undefined, this could have two reasons:
                            //  - The column for the return address may have been manually set to "undefined"
                            //    using DW_CFA_undefined. This usually means that the function never returns
                            //    and can be treated as the root of the stack.
                            //  - The column for the return may have been omitted from the DWARF CFI table.
                            //    Per spec (at least as of DWARF >= 3), this means that it should be treated
                            //    as undefined. But it seems that compilers often do this when they really mean
                            //    "same value".
                            // Gimli follows DWARF 3 and does not differentiate between "omitted" and "undefined".
                            Ok(
                                UnwindRuleAarch64::OffsetSpIfFirstFrameOtherwiseStackEndsHere {
                                    sp_offset_by_16,
                                },
                            )
                        } else {
                            Ok(UnwindRuleAarch64::OffsetSp { sp_offset_by_16 })
                        }
                    }
                    (Some(lr_cfa_offset), None) => {
                        let lr_storage_offset_from_sp_by_8 =
                            i16::try_from((offset + lr_cfa_offset) / 8)
                                .map_err(|_| ConversionError::LrStorageOffsetDoesNotFit)?;
                        Ok(UnwindRuleAarch64::OffsetSpAndRestoreLr {
                            sp_offset_by_16,
                            lr_storage_offset_from_sp_by_8,
                        })
                    }
                    (Some(lr_cfa_offset), Some(fp_cfa_offset)) => {
                        let lr_storage_offset_from_sp_by_8 =
                            i16::try_from((offset + lr_cfa_offset) / 8)
                                .map_err(|_| ConversionError::LrStorageOffsetDoesNotFit)?;
                        let fp_storage_offset_from_sp_by_8 =
                            i16::try_from((offset + fp_cfa_offset) / 8)
                                .map_err(|_| ConversionError::FpStorageOffsetDoesNotFit)?;
                        Ok(UnwindRuleAarch64::OffsetSpAndRestoreFpAndLr {
                            sp_offset_by_16,
                            fp_storage_offset_from_sp_by_8,
                            lr_storage_offset_from_sp_by_8,
                        })
                    }
                }
            }
            AArch64::X29 => {
                let lr_cfa_offset = register_rule_to_cfa_offset(lr_rule)?
                    .ok_or(ConversionError::FramePointerRuleDoesNotRestoreLr)?;
                let fp_cfa_offset = register_rule_to_cfa_offset(fp_rule)?
                    .ok_or(ConversionError::FramePointerRuleDoesNotRestoreFp)?;
                if *offset == 16 && fp_cfa_offset == -16 && lr_cfa_offset == -8 {
                    Ok(UnwindRuleAarch64::UseFramePointer)
                } else {
                    let sp_offset_from_fp_by_8 = u16::try_from(offset / 8)
                        .map_err(|_| ConversionError::SpOffsetFromFpDoesNotFit)?;
                    let lr_storage_offset_from_fp_by_8 =
                        i16::try_from((offset + lr_cfa_offset) / 8)
                            .map_err(|_| ConversionError::LrStorageOffsetDoesNotFit)?;
                    let fp_storage_offset_from_fp_by_8 =
                        i16::try_from((offset + fp_cfa_offset) / 8)
                            .map_err(|_| ConversionError::FpStorageOffsetDoesNotFit)?;
                    Ok(UnwindRuleAarch64::UseFramepointerWithOffsets {
                        sp_offset_from_fp_by_8,
                        fp_storage_offset_from_fp_by_8,
                        lr_storage_offset_from_fp_by_8,
                    })
                }
            }
            _ => Err(ConversionError::CfaIsOffsetFromUnknownRegister),
        },
        CfaRule::Expression(_) => Err(ConversionError::CfaIsExpression),
    }
}
