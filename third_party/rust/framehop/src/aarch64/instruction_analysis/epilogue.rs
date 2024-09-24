use super::super::unwind_rule::UnwindRuleAarch64;

struct EpilogueDetectorAarch64 {
    sp_offset: i32,
    fp_offset_from_initial_sp: Option<i32>,
    lr_offset_from_initial_sp: Option<i32>,
}

enum EpilogueStepResult {
    NeedMore,
    FoundBodyInstruction(UnexpectedInstructionType),
    FoundReturn,
    FoundTailCall,
    CouldBeAuthTailCall,
}

#[derive(Clone, Debug, PartialEq, Eq)]
enum EpilogueResult {
    ProbablyStillInBody(UnexpectedInstructionType),
    ReachedFunctionEndWithoutReturn,
    FoundReturnOrTailCall {
        sp_offset: i32,
        fp_offset_from_initial_sp: Option<i32>,
        lr_offset_from_initial_sp: Option<i32>,
    },
}

#[derive(Clone, Debug, PartialEq, Eq)]
enum UnexpectedInstructionType {
    LoadOfWrongSize,
    LoadReferenceRegisterNotSp,
    AddSubNotOperatingOnSp,
    AutibspNotFollowedByExpectedTailCall,
    BranchWithUnadjustedStackPointer,
    Unknown,
}

#[derive(Clone, Debug, PartialEq, Eq)]
enum EpilogueInstructionType {
    NotExpectedInEpilogue,
    CouldBeTailCall {
        /// If auth tail call, the offset in bytes where the autibsp would be.
        /// If regular tail call, we just check if the previous instruction
        /// adjusts the stack pointer.
        offset_of_expected_autibsp: u8,
    },
    CouldBePartOfAuthTailCall {
        /// In bytes
        offset_of_expected_autibsp: u8,
    },
    VeryLikelyPartOfEpilogue,
}

impl EpilogueDetectorAarch64 {
    pub fn new() -> Self {
        Self {
            sp_offset: 0,
            fp_offset_from_initial_sp: None,
            lr_offset_from_initial_sp: None,
        }
    }

    pub fn analyze_slice(&mut self, function_bytes: &[u8], pc_offset: usize) -> EpilogueResult {
        let mut bytes = &function_bytes[pc_offset..];
        if bytes.len() < 4 {
            return EpilogueResult::ReachedFunctionEndWithoutReturn;
        }
        let mut word = u32::from_le_bytes([bytes[0], bytes[1], bytes[2], bytes[3]]);
        bytes = &bytes[4..];
        match Self::analyze_instruction(word) {
            EpilogueInstructionType::NotExpectedInEpilogue => {
                return EpilogueResult::ProbablyStillInBody(UnexpectedInstructionType::Unknown)
            }
            EpilogueInstructionType::CouldBeTailCall {
                offset_of_expected_autibsp,
            } => {
                if pc_offset >= offset_of_expected_autibsp as usize {
                    let auth_tail_call_bytes =
                        &function_bytes[pc_offset - offset_of_expected_autibsp as usize..];
                    if auth_tail_call_bytes[0..4] == [0xff, 0x23, 0x03, 0xd5]
                        && Self::is_auth_tail_call(&auth_tail_call_bytes[4..])
                    {
                        return EpilogueResult::FoundReturnOrTailCall {
                            sp_offset: 0,
                            fp_offset_from_initial_sp: None,
                            lr_offset_from_initial_sp: None,
                        };
                    }
                }
                if pc_offset >= 4 {
                    let prev_b = &function_bytes[pc_offset - 4..pc_offset];
                    let prev_word =
                        u32::from_le_bytes([prev_b[0], prev_b[1], prev_b[2], prev_b[3]]);
                    if Self::instruction_adjusts_stack_pointer(prev_word) {
                        return EpilogueResult::FoundReturnOrTailCall {
                            sp_offset: 0,
                            fp_offset_from_initial_sp: None,
                            lr_offset_from_initial_sp: None,
                        };
                    }
                }
                return EpilogueResult::ProbablyStillInBody(UnexpectedInstructionType::Unknown);
            }
            EpilogueInstructionType::CouldBePartOfAuthTailCall {
                offset_of_expected_autibsp,
            } => {
                if pc_offset >= offset_of_expected_autibsp as usize {
                    let auth_tail_call_bytes =
                        &function_bytes[pc_offset - offset_of_expected_autibsp as usize..];
                    if auth_tail_call_bytes[0..4] == [0xff, 0x23, 0x03, 0xd5]
                        && Self::is_auth_tail_call(&auth_tail_call_bytes[4..])
                    {
                        return EpilogueResult::FoundReturnOrTailCall {
                            sp_offset: 0,
                            fp_offset_from_initial_sp: None,
                            lr_offset_from_initial_sp: None,
                        };
                    }
                }
                return EpilogueResult::ProbablyStillInBody(UnexpectedInstructionType::Unknown);
            }
            EpilogueInstructionType::VeryLikelyPartOfEpilogue => {}
        }

        loop {
            match self.step_instruction(word) {
                EpilogueStepResult::NeedMore => {
                    if bytes.len() < 4 {
                        return EpilogueResult::ReachedFunctionEndWithoutReturn;
                    }
                    word = u32::from_le_bytes([bytes[0], bytes[1], bytes[2], bytes[3]]);
                    bytes = &bytes[4..];
                    continue;
                }
                EpilogueStepResult::FoundBodyInstruction(uit) => {
                    return EpilogueResult::ProbablyStillInBody(uit);
                }
                EpilogueStepResult::FoundReturn | EpilogueStepResult::FoundTailCall => {}
                EpilogueStepResult::CouldBeAuthTailCall => {
                    if !Self::is_auth_tail_call(bytes) {
                        return EpilogueResult::ProbablyStillInBody(
                            UnexpectedInstructionType::AutibspNotFollowedByExpectedTailCall,
                        );
                    }
                }
            }
            return EpilogueResult::FoundReturnOrTailCall {
                sp_offset: self.sp_offset,
                fp_offset_from_initial_sp: self.fp_offset_from_initial_sp,
                lr_offset_from_initial_sp: self.lr_offset_from_initial_sp,
            };
        }
    }

    fn instruction_adjusts_stack_pointer(word: u32) -> bool {
        // Detect load from sp-relative offset with writeback.
        if (word >> 22) & 0b1011111011 == 0b1010100011 && (word >> 5) & 0b11111 == 31 {
            return true;
        }
        // Detect sub sp, sp, 0xXXXX
        if (word >> 23) & 0b111111111 == 0b100100010
            && word & 0b11111 == 31
            && (word >> 5) & 0b11111 == 31
        {
            return true;
        }
        false
    }

    fn is_auth_tail_call(bytes_after_autibsp: &[u8]) -> bool {
        // libsystem_malloc.dylib contains over a hundred of these.
        // At the end of the function, after restoring the registers from the stack,
        // there's an autibsp instruction, followed by some check (not sure what it
        // does), and then a tail call. These instructions should all be counted as
        // part of the epilogue; returning at this point is just "follow lr" instead
        // of "use the frame pointer".
        //
        // 180139058 ff 23 03 d5      autibsp
        //
        // 18013905c d0 07 1e ca      eor        x16, lr, lr, lsl #1
        // 180139060 50 00 f0 b6      tbz        x16, 0x3e, $+0x8
        // 180139064 20 8e 38 d4      brk        #0xc471              ; "breakpoint trap"
        //
        // and then a tail call, of one of these forms:
        //
        // 180139068 13 00 00 14      b          some_outside_function
        //
        // 18013a364 f0 36 88 d2      mov        x16, #0xXXXX
        // 18013a368 70 08 1f d7      braa       xX, x16
        //

        if bytes_after_autibsp.len() < 16 {
            return false;
        }
        let eor_tbz_brk = &bytes_after_autibsp[..12];
        if eor_tbz_brk
            != [
                0xd0, 0x07, 0x1e, 0xca, 0x50, 0x00, 0xf0, 0xb6, 0x20, 0x8e, 0x38, 0xd4,
            ]
        {
            return false;
        }

        let first_tail_call_instruction_opcode = u32::from_le_bytes([
            bytes_after_autibsp[12],
            bytes_after_autibsp[13],
            bytes_after_autibsp[14],
            bytes_after_autibsp[15],
        ]);
        let bits_26_to_32 = first_tail_call_instruction_opcode >> 26;
        if bits_26_to_32 == 0b000101 {
            // This is a `b` instruction. We've found the tail call.
            return true;
        }

        // If we get here, it's either not a recognized instruction sequence,
        // or the tail call is of the form `mov x16, #0xXXXX`, `braa xX, x16`.
        if bytes_after_autibsp.len() < 20 {
            return false;
        }

        let bits_23_to_32 = first_tail_call_instruction_opcode >> 23;
        let is_64_mov = (bits_23_to_32 & 0b111000111) == 0b110000101;
        let result_reg = first_tail_call_instruction_opcode & 0b11111;
        if !is_64_mov || result_reg != 16 {
            return false;
        }

        let braa_opcode = u32::from_le_bytes([
            bytes_after_autibsp[16],
            bytes_after_autibsp[17],
            bytes_after_autibsp[18],
            bytes_after_autibsp[19],
        ]);
        (braa_opcode & 0xff_ff_fc_00) == 0xd7_1f_08_00 && (braa_opcode & 0b11111) == 16
    }

    pub fn analyze_instruction(word: u32) -> EpilogueInstructionType {
        // Detect ret and retab
        if word == 0xd65f03c0 || word == 0xd65f0fff {
            return EpilogueInstructionType::VeryLikelyPartOfEpilogue;
        }
        // Detect autibsp
        if word == 0xd50323ff {
            return EpilogueInstructionType::CouldBePartOfAuthTailCall {
                offset_of_expected_autibsp: 0,
            };
        }
        // Detect `eor x16, lr, lr, lsl #1`
        if word == 0xca1e07d0 {
            return EpilogueInstructionType::CouldBePartOfAuthTailCall {
                offset_of_expected_autibsp: 4,
            };
        }
        // Detect `tbz x16, 0x3e, $+0x8`
        if word == 0xb6f00050 {
            return EpilogueInstructionType::CouldBePartOfAuthTailCall {
                offset_of_expected_autibsp: 8,
            };
        }
        // Detect `brk #0xc471`
        if word == 0xd4388e20 {
            return EpilogueInstructionType::CouldBePartOfAuthTailCall {
                offset_of_expected_autibsp: 12,
            };
        }
        // Detect `b` and `br xX`
        if (word >> 26) == 0b000101 || word & 0xff_ff_fc_1f == 0xd6_1f_00_00 {
            // This could be a branch with a target inside this function, or
            // a tail call outside of this function.
            return EpilogueInstructionType::CouldBeTailCall {
                offset_of_expected_autibsp: 16,
            };
        }
        // Detect `mov x16, #0xXXXX`
        if (word >> 23) & 0b111000111 == 0b110000101 && word & 0b11111 == 16 {
            return EpilogueInstructionType::CouldBePartOfAuthTailCall {
                offset_of_expected_autibsp: 16,
            };
        }
        // Detect `braa xX, x16`
        if word & 0xff_ff_fc_00 == 0xd7_1f_08_00 && word & 0b11111 == 16 {
            return EpilogueInstructionType::CouldBePartOfAuthTailCall {
                offset_of_expected_autibsp: 20,
            };
        }
        if (word >> 22) & 0b1011111001 == 0b1010100001 {
            // Section C3.3, Loads and stores.
            // but only loads that are commonly seen in prologues / epilogues (bits 29 and 31 are set)
            let writeback_bits = (word >> 23) & 0b11;
            if writeback_bits == 0b00 {
                // Not 64-bit load.
                return EpilogueInstructionType::NotExpectedInEpilogue;
            }
            let reference_reg = ((word >> 5) & 0b11111) as u16;
            if reference_reg != 31 {
                return EpilogueInstructionType::NotExpectedInEpilogue;
            }
            return EpilogueInstructionType::VeryLikelyPartOfEpilogue;
        }
        if (word >> 23) & 0b111111111 == 0b100100010 {
            // Section C3.4, Data processing - immediate
            // unsigned add imm, size class X (8 bytes)
            let result_reg = (word & 0b11111) as u16;
            let input_reg = ((word >> 5) & 0b11111) as u16;
            if result_reg != 31 || input_reg != 31 {
                return EpilogueInstructionType::NotExpectedInEpilogue;
            }
            return EpilogueInstructionType::VeryLikelyPartOfEpilogue;
        }
        EpilogueInstructionType::NotExpectedInEpilogue
    }

    pub fn step_instruction(&mut self, word: u32) -> EpilogueStepResult {
        // Detect ret and retab
        if word == 0xd65f03c0 || word == 0xd65f0fff {
            return EpilogueStepResult::FoundReturn;
        }
        // Detect autibsp
        if word == 0xd50323ff {
            return EpilogueStepResult::CouldBeAuthTailCall;
        }
        // Detect b
        if (word >> 26) == 0b000101 {
            // This could be a branch with a target inside this function, or
            // a tail call outside of this function.
            // Let's use the following heuristic: If this instruction is followed
            // by valid epilogue instructions which adjusted the stack pointer, then
            // we treat it as a tail call.
            if self.sp_offset != 0 {
                return EpilogueStepResult::FoundTailCall;
            }
            return EpilogueStepResult::FoundBodyInstruction(
                UnexpectedInstructionType::BranchWithUnadjustedStackPointer,
            );
        }
        if (word >> 22) & 0b1011111001 == 0b1010100001 {
            // Section C3.3, Loads and stores.
            // but only those that are commonly seen in prologues / epilogues (bits 29 and 31 are set)
            let writeback_bits = (word >> 23) & 0b11;
            if writeback_bits == 0b00 {
                // Not 64-bit load/store.
                return EpilogueStepResult::FoundBodyInstruction(
                    UnexpectedInstructionType::LoadOfWrongSize,
                );
            }
            let reference_reg = ((word >> 5) & 0b11111) as u16;
            if reference_reg != 31 {
                return EpilogueStepResult::FoundBodyInstruction(
                    UnexpectedInstructionType::LoadReferenceRegisterNotSp,
                );
            }
            let is_preindexed_writeback = writeback_bits == 0b11; // TODO: are there preindexed loads? What do they mean?
            let is_postindexed_writeback = writeback_bits == 0b01;
            let imm7 = (((((word >> 15) & 0b1111111) as i16) << 9) >> 6) as i32;
            let reg_loc = if is_postindexed_writeback {
                self.sp_offset
            } else {
                self.sp_offset + imm7
            };
            let pair_reg_1 = (word & 0b11111) as u16;
            if pair_reg_1 == 29 {
                self.fp_offset_from_initial_sp = Some(reg_loc);
            } else if pair_reg_1 == 30 {
                self.lr_offset_from_initial_sp = Some(reg_loc);
            }
            let pair_reg_2 = ((word >> 10) & 0b11111) as u16;
            if pair_reg_2 == 29 {
                self.fp_offset_from_initial_sp = Some(reg_loc + 8);
            } else if pair_reg_2 == 30 {
                self.lr_offset_from_initial_sp = Some(reg_loc + 8);
            }
            if is_preindexed_writeback || is_postindexed_writeback {
                self.sp_offset += imm7;
            }
            return EpilogueStepResult::NeedMore;
        }
        if (word >> 23) & 0b111111111 == 0b100100010 {
            // Section C3.4, Data processing - immediate
            // unsigned add imm, size class X (8 bytes)
            let result_reg = (word & 0b11111) as u16;
            let input_reg = ((word >> 5) & 0b11111) as u16;
            if result_reg != 31 || input_reg != 31 {
                return EpilogueStepResult::FoundBodyInstruction(
                    UnexpectedInstructionType::AddSubNotOperatingOnSp,
                );
            }
            let mut imm12 = ((word >> 10) & 0b111111111111) as i32;
            let shift_immediate_by_12 = ((word >> 22) & 0b1) == 0b1;
            if shift_immediate_by_12 {
                imm12 <<= 12
            }
            self.sp_offset += imm12;
            return EpilogueStepResult::NeedMore;
        }
        EpilogueStepResult::FoundBodyInstruction(UnexpectedInstructionType::Unknown)
    }
}

pub fn unwind_rule_from_detected_epilogue(
    bytes: &[u8],
    pc_offset: usize,
) -> Option<UnwindRuleAarch64> {
    let mut detector = EpilogueDetectorAarch64::new();
    match detector.analyze_slice(bytes, pc_offset) {
        EpilogueResult::ProbablyStillInBody(_)
        | EpilogueResult::ReachedFunctionEndWithoutReturn => None,
        EpilogueResult::FoundReturnOrTailCall {
            sp_offset,
            fp_offset_from_initial_sp,
            lr_offset_from_initial_sp,
        } => {
            let sp_offset_by_16 = u16::try_from(sp_offset / 16).ok()?;
            let rule = match (fp_offset_from_initial_sp, lr_offset_from_initial_sp) {
                (None, None) if sp_offset_by_16 == 0 => UnwindRuleAarch64::NoOp,
                (None, None) => UnwindRuleAarch64::OffsetSp { sp_offset_by_16 },
                (None, Some(lr_offset)) => UnwindRuleAarch64::OffsetSpAndRestoreLr {
                    sp_offset_by_16,
                    lr_storage_offset_from_sp_by_8: i16::try_from(lr_offset / 8).ok()?,
                },
                (Some(_), None) => return None,
                (Some(fp_offset), Some(lr_offset)) => {
                    UnwindRuleAarch64::OffsetSpAndRestoreFpAndLr {
                        sp_offset_by_16,
                        fp_storage_offset_from_sp_by_8: i16::try_from(fp_offset / 8).ok()?,
                        lr_storage_offset_from_sp_by_8: i16::try_from(lr_offset / 8).ok()?,
                    }
                }
            };
            Some(rule)
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_epilogue_1() {
        // 1000e0d18 fd 7b 44 a9     ldp        fp, lr, [sp, #0x40]
        // 1000e0d1c f4 4f 43 a9     ldp        x20, x19, [sp, #0x30]
        // 1000e0d20 f6 57 42 a9     ldp        x22, x21, [sp, #0x20]
        // 1000e0d24 ff 43 01 91     add        sp, sp, #0x50
        // 1000e0d28 c0 03 5f d6     ret

        let bytes = &[
            0xfd, 0x7b, 0x44, 0xa9, 0xf4, 0x4f, 0x43, 0xa9, 0xf6, 0x57, 0x42, 0xa9, 0xff, 0x43,
            0x01, 0x91, 0xc0, 0x03, 0x5f, 0xd6,
        ];
        assert_eq!(
            unwind_rule_from_detected_epilogue(bytes, 0),
            Some(UnwindRuleAarch64::OffsetSpAndRestoreFpAndLr {
                sp_offset_by_16: 5,
                fp_storage_offset_from_sp_by_8: 8,
                lr_storage_offset_from_sp_by_8: 9,
            })
        );
        assert_eq!(
            unwind_rule_from_detected_epilogue(bytes, 4),
            Some(UnwindRuleAarch64::OffsetSp { sp_offset_by_16: 5 })
        );
        assert_eq!(
            unwind_rule_from_detected_epilogue(bytes, 8),
            Some(UnwindRuleAarch64::OffsetSp { sp_offset_by_16: 5 })
        );
        assert_eq!(
            unwind_rule_from_detected_epilogue(bytes, 12),
            Some(UnwindRuleAarch64::OffsetSp { sp_offset_by_16: 5 })
        );
        assert_eq!(
            unwind_rule_from_detected_epilogue(bytes, 16),
            Some(UnwindRuleAarch64::NoOp)
        );
        assert_eq!(unwind_rule_from_detected_epilogue(bytes, 20), None);
    }

    #[test]
    fn test_epilogue_with_retab() {
        //         _malloc_zone_realloc epilogue
        // 18012466c e0 03 16 aa     mov        x0,x22
        // 180124670 fd 7b 43 a9     ldp        x29=>local_10,x30,[sp, #0x30]
        // 180124674 f4 4f 42 a9     ldp        x20,x19,[sp, #local_20]
        // 180124678 f6 57 41 a9     ldp        x22,x21,[sp, #local_30]
        // 18012467c f8 5f c4 a8     ldp        x24,x23,[sp], #0x40
        // 180124680 ff 0f 5f d6     retab
        // 180124684 a0 01 80 52     mov        w0,#0xd
        // 180124688 20 60 a6 72     movk       w0,#0x3301, LSL #16

        let bytes = &[
            0xe0, 0x03, 0x16, 0xaa, 0xfd, 0x7b, 0x43, 0xa9, 0xf4, 0x4f, 0x42, 0xa9, 0xf6, 0x57,
            0x41, 0xa9, 0xf8, 0x5f, 0xc4, 0xa8, 0xff, 0x0f, 0x5f, 0xd6, 0xa0, 0x01, 0x80, 0x52,
            0x20, 0x60, 0xa6, 0x72,
        ];
        assert_eq!(unwind_rule_from_detected_epilogue(bytes, 0), None);
        assert_eq!(
            unwind_rule_from_detected_epilogue(bytes, 4),
            Some(UnwindRuleAarch64::OffsetSpAndRestoreFpAndLr {
                sp_offset_by_16: 4,
                fp_storage_offset_from_sp_by_8: 6,
                lr_storage_offset_from_sp_by_8: 7
            })
        );
        assert_eq!(
            unwind_rule_from_detected_epilogue(bytes, 8),
            Some(UnwindRuleAarch64::OffsetSp { sp_offset_by_16: 4 })
        );
        assert_eq!(
            unwind_rule_from_detected_epilogue(bytes, 12),
            Some(UnwindRuleAarch64::OffsetSp { sp_offset_by_16: 4 })
        );
        assert_eq!(
            unwind_rule_from_detected_epilogue(bytes, 16),
            Some(UnwindRuleAarch64::OffsetSp { sp_offset_by_16: 4 })
        );
        assert_eq!(
            unwind_rule_from_detected_epilogue(bytes, 20),
            Some(UnwindRuleAarch64::NoOp)
        );
        assert_eq!(unwind_rule_from_detected_epilogue(bytes, 24), None);
    }

    #[test]
    fn test_epilogue_with_retab_2() {
        // _tiny_free_list_add_ptr:
        // ...
        // 18013e114 28 01 00 79     strh       w8, [x9]
        // 18013e118 fd 7b c1 a8     ldp        fp, lr, [sp], #0x10
        // 18013e11c ff 0f 5f d6     retab
        // 18013e120 e2 03 08 aa     mov        x2, x8
        // 18013e124 38 76 00 94     bl         _free_list_checksum_botch
        // ...

        let bytes = &[
            0x28, 0x01, 0x00, 0x79, 0xfd, 0x7b, 0xc1, 0xa8, 0xff, 0x0f, 0x5f, 0xd6, 0xe2, 0x03,
            0x08, 0xaa, 0x38, 0x76, 0x00, 0x94,
        ];
        assert_eq!(unwind_rule_from_detected_epilogue(bytes, 0), None);
        assert_eq!(
            unwind_rule_from_detected_epilogue(bytes, 4),
            Some(UnwindRuleAarch64::OffsetSpAndRestoreFpAndLr {
                sp_offset_by_16: 1,
                fp_storage_offset_from_sp_by_8: 0,
                lr_storage_offset_from_sp_by_8: 1
            })
        );
        assert_eq!(
            unwind_rule_from_detected_epilogue(bytes, 8),
            Some(UnwindRuleAarch64::NoOp)
        );
        assert_eq!(unwind_rule_from_detected_epilogue(bytes, 12), None);
        assert_eq!(unwind_rule_from_detected_epilogue(bytes, 16), None);
    }

    #[test]
    fn test_epilogue_with_regular_tail_call() {
        // (in rustup) __ZN126_$LT$$LT$toml..value..Value$u20$as$u20$serde..de..Deserialize$GT$..deserialize..ValueVisitor$u20$as$u20$serde..de..Visitor$GT$9visit_map17h0afd4b269ef00eebE
        // ...
        // 1002566b4 fc 6f c6 a8     ldp        x28, x27, [sp], #0x60
        // 1002566b8 bc ba ff 17     b          __ZN4core3ptr41drop_in_place$LT$toml..de..MapVisitor$GT$17hd4556de1a4edab42E
        // ...
        let bytes = &[0xfc, 0x6f, 0xc6, 0xa8, 0xbc, 0xba, 0xff, 0x17];
        assert_eq!(
            unwind_rule_from_detected_epilogue(bytes, 0),
            Some(UnwindRuleAarch64::OffsetSp { sp_offset_by_16: 6 })
        );
    }

    // This test fails at the moment.
    #[test]
    fn test_epilogue_with_register_tail_call() {
        // This test requires lookbehind in the epilogue detection.
        // We want to detect the `br` as a tail call. We should do this
        // based on the fact that the previous instruction adjusted the
        // stack pointer.
        //
        // (in rustup) __ZN4core3fmt9Formatter3pad17h3f40041e7f99f180E
        // ...
        // 1000500bc fa 67 c5 a8     ldp        x26, x25, [sp], #0x50
        // 1000500c0 60 00 1f d6     br         x3
        // ...
        let bytes = &[0xfa, 0x67, 0xc5, 0xa8, 0x60, 0x00, 0x1f, 0xd6];
        assert_eq!(
            unwind_rule_from_detected_epilogue(bytes, 4),
            Some(UnwindRuleAarch64::NoOp)
        );
    }

    #[test]
    fn test_epilogue_with_auth_tail_call() {
        // _nanov2_free_definite_size
        // ...
        // 180139048 e1 03 13 aa      mov        x1, x19
        // 18013904c fd 7b 42 a9      ldp        fp, lr, [sp, #0x20]
        // 180139050 f4 4f 41 a9      ldp        x20, x19, [sp, #0x10]
        // 180139054 f6 57 c3 a8      ldp        x22, x21, [sp], #0x30
        // 180139058 ff 23 03 d5      autibsp
        // 18013905c d0 07 1e ca      eor        x16, lr, lr, lsl #1
        // 180139060 50 00 f0 b6      tbz        x16, 0x3e, loc_180139068
        // 180139064 20 8e 38 d4      brk        #0xc471
        //                       loc_180139068:
        // 180139068 13 00 00 14      b          _nanov2_free_to_block
        //                       loc_18013906c:
        // 18013906c a0 16 78 f9      ldr        x0, [x21, #0x7028]
        // 180139070 03 3c 40 f9      ldr        x3, [x0, #0x78]
        // ...
        let bytes = &[
            0xe1, 0x03, 0x13, 0xaa, 0xfd, 0x7b, 0x42, 0xa9, 0xf4, 0x4f, 0x41, 0xa9, 0xf6, 0x57,
            0xc3, 0xa8, 0xff, 0x23, 0x03, 0xd5, 0xd0, 0x07, 0x1e, 0xca, 0x50, 0x00, 0xf0, 0xb6,
            0x20, 0x8e, 0x38, 0xd4, 0x13, 0x00, 0x00, 0x14, 0xa0, 0x16, 0x78, 0xf9, 0x03, 0x3c,
            0x40, 0xf9,
        ];
        assert_eq!(unwind_rule_from_detected_epilogue(bytes, 0), None);
        assert_eq!(
            unwind_rule_from_detected_epilogue(bytes, 4),
            Some(UnwindRuleAarch64::OffsetSpAndRestoreFpAndLr {
                sp_offset_by_16: 3,
                fp_storage_offset_from_sp_by_8: 4,
                lr_storage_offset_from_sp_by_8: 5
            })
        );
        assert_eq!(
            unwind_rule_from_detected_epilogue(bytes, 8),
            Some(UnwindRuleAarch64::OffsetSp { sp_offset_by_16: 3 })
        );
        assert_eq!(
            unwind_rule_from_detected_epilogue(bytes, 12),
            Some(UnwindRuleAarch64::OffsetSp { sp_offset_by_16: 3 })
        );
        assert_eq!(
            unwind_rule_from_detected_epilogue(bytes, 16),
            Some(UnwindRuleAarch64::NoOp)
        );
        assert_eq!(
            unwind_rule_from_detected_epilogue(bytes, 20),
            Some(UnwindRuleAarch64::NoOp)
        );
        assert_eq!(
            unwind_rule_from_detected_epilogue(bytes, 24),
            Some(UnwindRuleAarch64::NoOp)
        );
        assert_eq!(
            unwind_rule_from_detected_epilogue(bytes, 28),
            Some(UnwindRuleAarch64::NoOp)
        );
    }

    #[test]
    fn test_epilogue_with_auth_tail_call_2() {
        // _malloc_zone_claimed_addres
        // ...
        // 1801457ac e1 03 13 aa     mov        x1, x19
        // 1801457b0 fd 7b 41 a9     ldp        fp, lr, [sp, #0x10]
        // 1801457b4 f4 4f c2 a8     ldp        x20, x19, [sp], #0x20
        // 1801457b8 ff 23 03 d5     autibsp
        // 1801457bc d0 07 1e ca     eor        x16, lr, lr, lsl #1
        // 1801457c0 50 00 f0 b6     tbz        x16, 0x3e, loc_1801457c8
        // 1801457c4 20 8e 38 d4     brk        #0xc471
        //                       loc_1801457c8:
        // 1801457c8 f0 77 9c d2     mov        x16, #0xe3bf
        // 1801457cc 50 08 1f d7     braa       x2, x16
        // ...
        let bytes = &[
            0xe1, 0x03, 0x13, 0xaa, 0xfd, 0x7b, 0x41, 0xa9, 0xf4, 0x4f, 0xc2, 0xa8, 0xff, 0x23,
            0x03, 0xd5, 0xd0, 0x07, 0x1e, 0xca, 0x50, 0x00, 0xf0, 0xb6, 0x20, 0x8e, 0x38, 0xd4,
            0xf0, 0x77, 0x9c, 0xd2, 0x50, 0x08, 0x1f, 0xd7,
        ];
        assert_eq!(unwind_rule_from_detected_epilogue(bytes, 0), None);
        assert_eq!(
            unwind_rule_from_detected_epilogue(bytes, 4),
            Some(UnwindRuleAarch64::OffsetSpAndRestoreFpAndLr {
                sp_offset_by_16: 2,
                fp_storage_offset_from_sp_by_8: 2,
                lr_storage_offset_from_sp_by_8: 3
            })
        );
        assert_eq!(
            unwind_rule_from_detected_epilogue(bytes, 8),
            Some(UnwindRuleAarch64::OffsetSp { sp_offset_by_16: 2 })
        );
        assert_eq!(
            unwind_rule_from_detected_epilogue(bytes, 12),
            Some(UnwindRuleAarch64::NoOp)
        );
        assert_eq!(
            unwind_rule_from_detected_epilogue(bytes, 16),
            Some(UnwindRuleAarch64::NoOp)
        );
        assert_eq!(
            unwind_rule_from_detected_epilogue(bytes, 20),
            Some(UnwindRuleAarch64::NoOp)
        );
        assert_eq!(
            unwind_rule_from_detected_epilogue(bytes, 24),
            Some(UnwindRuleAarch64::NoOp)
        );
        assert_eq!(
            unwind_rule_from_detected_epilogue(bytes, 28),
            Some(UnwindRuleAarch64::NoOp)
        );
    }
}
