use super::super::unwind_rule::UnwindRuleAarch64;

struct PrologueDetectorAarch64 {
    sp_offset: i32,
}

#[derive(Clone, Debug, PartialEq, Eq)]
enum PrologueStepResult {
    UnexpectedInstruction(UnexpectedInstructionType),
    ValidPrologueInstruction,
}

#[derive(Clone, Debug, PartialEq, Eq)]
enum PrologueResult {
    ProbablyAlreadyInBody(UnexpectedInstructionType),
    FoundFunctionStart { sp_offset: i32 },
}

#[derive(Clone, Debug, PartialEq, Eq)]
enum PrologueInstructionType {
    NotExpectedInPrologue,
    CouldBePartOfPrologueIfThereIsAlsoAStackPointerSub,
    VeryLikelyPartOfPrologue,
}

#[derive(Clone, Debug, PartialEq, Eq)]
enum UnexpectedInstructionType {
    StoreOfWrongSize,
    StoreReferenceRegisterNotSp,
    AddSubNotOperatingOnSp,
    NoNextInstruction,
    NoStackPointerSubBeforeStore,
    Unknown,
}

impl PrologueDetectorAarch64 {
    pub fn new() -> Self {
        Self { sp_offset: 0 }
    }

    pub fn analyze_slices(
        &mut self,
        slice_from_start: &[u8],
        slice_to_end: &[u8],
    ) -> PrologueResult {
        // There are at least two options of what we could do here:
        //  - We could walk forwards from the function start to the instruction pointer.
        //  - We could walk backwards from the instruction pointer to the function start.
        // Walking backwards is fine on arm64 because instructions are fixed size.
        // Walking forwards requires that we have a useful function start address.
        //
        // Unfortunately, we can't rely on having a useful function start address.
        // We get the funcion start address from the __unwind_info, which often collapses
        // consecutive functions with the same unwind rules into a single entry, discarding
        // the original function start addresses.
        // Concretely, this means that `slice_from_start` may start much earlier than the
        // current function.
        //
        // So we walk backwards. We first check the next instruction, and then
        // go backwards from the instruction pointer to the function start.
        // If the instruction we're about to execute is one that we'd expect to find in a prologue,
        // then we assume that we're in a prologue. Then we single-step backwards until we
        // either run out of instructions (which means we've definitely hit the start of the
        // function), or until we find an instruction that we would not expect in a prologue.
        // At that point we guess that this instruction must be belonging to the previous
        // function, and that we've succesfully found the start of the current function.
        if slice_to_end.len() < 4 {
            return PrologueResult::ProbablyAlreadyInBody(
                UnexpectedInstructionType::NoNextInstruction,
            );
        }
        let next_instruction = u32::from_le_bytes([
            slice_to_end[0],
            slice_to_end[1],
            slice_to_end[2],
            slice_to_end[3],
        ]);
        let next_instruction_type = Self::analyze_prologue_instruction_type(next_instruction);
        if next_instruction_type == PrologueInstructionType::NotExpectedInPrologue {
            return PrologueResult::ProbablyAlreadyInBody(UnexpectedInstructionType::Unknown);
        }
        let instructions = slice_from_start
            .chunks_exact(4)
            .map(|c| u32::from_le_bytes([c[0], c[1], c[2], c[3]]))
            .rev();
        for instruction in instructions {
            if let PrologueStepResult::UnexpectedInstruction(_) =
                self.reverse_step_instruction(instruction)
            {
                break;
            }
        }
        if next_instruction_type
            == PrologueInstructionType::CouldBePartOfPrologueIfThereIsAlsoAStackPointerSub
            && self.sp_offset == 0
        {
            return PrologueResult::ProbablyAlreadyInBody(
                UnexpectedInstructionType::NoStackPointerSubBeforeStore,
            );
        }
        PrologueResult::FoundFunctionStart {
            sp_offset: self.sp_offset,
        }
    }

    /// Check if the instruction indicates that we're likely in a prologue.
    pub fn analyze_prologue_instruction_type(word: u32) -> PrologueInstructionType {
        // Detect pacibsp (verify stack pointer authentication) and `mov x29, sp`.
        if word == 0xd503237f || word == 0x910003fd {
            return PrologueInstructionType::VeryLikelyPartOfPrologue;
        }

        let bits_22_to_32 = word >> 22;

        // Detect stores of register pairs to the stack.
        if bits_22_to_32 & 0b1011111001 == 0b1010100000 {
            // Section C3.3, Loads and stores.
            // Only stores that are commonly seen in prologues (bits 22, 29 and 31 are set)
            let writeback_bits = bits_22_to_32 & 0b110;
            let reference_reg = ((word >> 5) & 0b11111) as u16;
            if writeback_bits == 0b000 || reference_reg != 31 {
                return PrologueInstructionType::NotExpectedInPrologue;
            }
            // We are storing a register pair to the stack. This is something that
            // can happen in a prologue but it can also happen in the body of a
            // function.
            if writeback_bits == 0b100 {
                // No writeback.
                return PrologueInstructionType::CouldBePartOfPrologueIfThereIsAlsoAStackPointerSub;
            }
            return PrologueInstructionType::VeryLikelyPartOfPrologue;
        }
        // Detect sub instructions operating on the stack pointer.
        // Detect `add fp, sp, #0xXX` instructions
        if bits_22_to_32 & 0b1011111110 == 0b1001000100 {
            // Section C3.4, Data processing - immediate
            // unsigned add / sub imm, size class X (8 bytes)
            let result_reg = (word & 0b11111) as u16;
            let input_reg = ((word >> 5) & 0b11111) as u16;
            let is_sub = ((word >> 30) & 0b1) == 0b1;
            let expected_result_reg = if is_sub { 31 } else { 29 };
            if input_reg != 31 || result_reg != expected_result_reg {
                return PrologueInstructionType::NotExpectedInPrologue;
            }
            return PrologueInstructionType::VeryLikelyPartOfPrologue;
        }
        PrologueInstructionType::NotExpectedInPrologue
    }

    /// Step backwards over one (already executed) instruction.
    pub fn reverse_step_instruction(&mut self, word: u32) -> PrologueStepResult {
        // Detect pacibsp (verify stack pointer authentication)
        if word == 0xd503237f {
            return PrologueStepResult::ValidPrologueInstruction;
        }

        // Detect stores of register pairs to the stack.
        if (word >> 22) & 0b1011111001 == 0b1010100000 {
            // Section C3.3, Loads and stores.
            // but only those that are commonly seen in prologues / prologues (bits 29 and 31 are set)
            let writeback_bits = (word >> 23) & 0b11;
            if writeback_bits == 0b00 {
                // Not 64-bit load/store.
                return PrologueStepResult::UnexpectedInstruction(
                    UnexpectedInstructionType::StoreOfWrongSize,
                );
            }
            let reference_reg = ((word >> 5) & 0b11111) as u16;
            if reference_reg != 31 {
                return PrologueStepResult::UnexpectedInstruction(
                    UnexpectedInstructionType::StoreReferenceRegisterNotSp,
                );
            }
            let is_preindexed_writeback = writeback_bits == 0b11;
            let is_postindexed_writeback = writeback_bits == 0b01; // TODO: are there postindexed stores? What do they mean?
            if is_preindexed_writeback || is_postindexed_writeback {
                let imm7 = (((((word >> 15) & 0b1111111) as i16) << 9) >> 6) as i32;
                self.sp_offset -= imm7; // - to undo the instruction
            }
            return PrologueStepResult::ValidPrologueInstruction;
        }
        // Detect sub instructions operating on the stack pointer.
        if (word >> 23) & 0b111111111 == 0b110100010 {
            // Section C3.4, Data processing - immediate
            // unsigned sub imm, size class X (8 bytes)
            let result_reg = (word & 0b11111) as u16;
            let input_reg = ((word >> 5) & 0b11111) as u16;
            if result_reg != 31 || input_reg != 31 {
                return PrologueStepResult::UnexpectedInstruction(
                    UnexpectedInstructionType::AddSubNotOperatingOnSp,
                );
            }
            let mut imm12 = ((word >> 10) & 0b111111111111) as i32;
            let shift_immediate_by_12 = ((word >> 22) & 0b1) == 0b1;
            if shift_immediate_by_12 {
                imm12 <<= 12
            }
            self.sp_offset += imm12; // + to undo the sub instruction
            return PrologueStepResult::ValidPrologueInstruction;
        }
        PrologueStepResult::UnexpectedInstruction(UnexpectedInstructionType::Unknown)
    }
}

pub fn unwind_rule_from_detected_prologue(
    slice_from_start: &[u8],
    slice_to_end: &[u8],
) -> Option<UnwindRuleAarch64> {
    let mut detector = PrologueDetectorAarch64::new();
    match detector.analyze_slices(slice_from_start, slice_to_end) {
        PrologueResult::ProbablyAlreadyInBody(_) => None,
        PrologueResult::FoundFunctionStart { sp_offset } => {
            let sp_offset_by_16 = u16::try_from(sp_offset / 16).ok()?;
            let rule = if sp_offset_by_16 == 0 {
                UnwindRuleAarch64::NoOp
            } else {
                UnwindRuleAarch64::OffsetSp { sp_offset_by_16 }
            };
            Some(rule)
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_prologue_1() {
        //         gimli::read::unit::parse_attribute
        // 1000dfeb8 ff 43 01 d1     sub        sp, sp, #0x50
        // 1000dfebc f6 57 02 a9     stp        x22, x21, [sp, #local_30]
        // 1000dfec0 f4 4f 03 a9     stp        x20, x19, [sp, #local_20]
        // 1000dfec4 fd 7b 04 a9     stp        x29, x30, [sp, #local_10]
        // 1000dfec8 fd 03 01 91     add        x29, sp, #0x40
        // 1000dfecc f4 03 04 aa     mov        x20, x4
        // 1000dfed0 f5 03 01 aa     mov        x21, x1

        let bytes = &[
            0xff, 0x43, 0x01, 0xd1, 0xf6, 0x57, 0x02, 0xa9, 0xf4, 0x4f, 0x03, 0xa9, 0xfd, 0x7b,
            0x04, 0xa9, 0xfd, 0x03, 0x01, 0x91, 0xf4, 0x03, 0x04, 0xaa, 0xf5, 0x03, 0x01, 0xaa,
        ];
        assert_eq!(
            unwind_rule_from_detected_prologue(&bytes[..0], &bytes[0..]),
            Some(UnwindRuleAarch64::NoOp)
        );
        assert_eq!(
            unwind_rule_from_detected_prologue(&bytes[..4], &bytes[4..]),
            Some(UnwindRuleAarch64::OffsetSp { sp_offset_by_16: 5 })
        );
        assert_eq!(
            unwind_rule_from_detected_prologue(&bytes[..8], &bytes[8..]),
            Some(UnwindRuleAarch64::OffsetSp { sp_offset_by_16: 5 })
        );
        assert_eq!(
            unwind_rule_from_detected_prologue(&bytes[..12], &bytes[12..]),
            Some(UnwindRuleAarch64::OffsetSp { sp_offset_by_16: 5 })
        );
        assert_eq!(
            unwind_rule_from_detected_prologue(&bytes[..16], &bytes[16..]),
            Some(UnwindRuleAarch64::OffsetSp { sp_offset_by_16: 5 })
        );
        assert_eq!(
            unwind_rule_from_detected_prologue(&bytes[..20], &bytes[20..]),
            None
        );
        assert_eq!(
            unwind_rule_from_detected_prologue(&bytes[..24], &bytes[24..]),
            None
        );
        assert_eq!(
            unwind_rule_from_detected_prologue(&bytes[..28], &bytes[28..]),
            None
        );
    }

    #[test]
    fn test_prologue_with_pacibsp() {
        // 1801245c4 08 58 29 b8     str        w8,[x0, w9, UXTW #0x2]
        // 1801245c8 c0 03 5f d6     ret
        //                       _malloc_zone_realloc
        // 1801245cc 7f 23 03 d5     pacibsp
        // 1801245d0 f8 5f bc a9     stp        x24,x23,[sp, #local_40]!
        // 1801245d4 f6 57 01 a9     stp        x22,x21,[sp, #local_30]
        // 1801245d8 f4 4f 02 a9     stp        x20,x19,[sp, #local_20]
        // 1801245dc fd 7b 03 a9     stp        x29,x30,[sp, #local_10]
        // 1801245e0 fd c3 00 91     add        x29,sp,#0x30
        // 1801245e4 f3 03 02 aa     mov        x19,x2
        // 1801245e8 f4 03 01 aa     mov        x20,x1

        let bytes = &[
            0x08, 0x58, 0x29, 0xb8, 0xc0, 0x03, 0x5f, 0xd6, 0x7f, 0x23, 0x03, 0xd5, 0xf8, 0x5f,
            0xbc, 0xa9, 0xf6, 0x57, 0x01, 0xa9, 0xf4, 0x4f, 0x02, 0xa9, 0xfd, 0x7b, 0x03, 0xa9,
            0xfd, 0xc3, 0x00, 0x91, 0xf3, 0x03, 0x02, 0xaa, 0xf4, 0x03, 0x01, 0xaa,
        ];
        assert_eq!(
            unwind_rule_from_detected_prologue(&bytes[..0], &bytes[0..]),
            None
        );
        assert_eq!(
            unwind_rule_from_detected_prologue(&bytes[..4], &bytes[4..]),
            None
        );
        assert_eq!(
            unwind_rule_from_detected_prologue(&bytes[..8], &bytes[8..]),
            Some(UnwindRuleAarch64::NoOp)
        );
        assert_eq!(
            unwind_rule_from_detected_prologue(&bytes[..12], &bytes[12..]),
            Some(UnwindRuleAarch64::NoOp)
        );
        assert_eq!(
            unwind_rule_from_detected_prologue(&bytes[..16], &bytes[16..]),
            Some(UnwindRuleAarch64::OffsetSp { sp_offset_by_16: 4 })
        );
        assert_eq!(
            unwind_rule_from_detected_prologue(&bytes[..20], &bytes[20..]),
            Some(UnwindRuleAarch64::OffsetSp { sp_offset_by_16: 4 })
        );
        assert_eq!(
            unwind_rule_from_detected_prologue(&bytes[..24], &bytes[24..]),
            Some(UnwindRuleAarch64::OffsetSp { sp_offset_by_16: 4 })
        );
        assert_eq!(
            unwind_rule_from_detected_prologue(&bytes[..28], &bytes[28..]),
            Some(UnwindRuleAarch64::OffsetSp { sp_offset_by_16: 4 })
        );
        assert_eq!(
            unwind_rule_from_detected_prologue(&bytes[..32], &bytes[32..]),
            None
        );
    }

    #[test]
    fn test_prologue_with_mov_fp_sp() {
        //     _tiny_free_list_add_ptr
        // 180126e94 7f 23 03 d5     pacibsp
        // 180126e98 fd 7b bf a9     stp        x29,x30,[sp, #local_10]!
        // 180126e9c fd 03 00 91     mov        x29,sp
        // 180126ea0 68 04 00 51     sub        w8,w3,#0x1

        let bytes = &[
            0x7f, 0x23, 0x03, 0xd5, 0xfd, 0x7b, 0xbf, 0xa9, 0xfd, 0x03, 0x00, 0x91, 0x68, 0x04,
            0x00, 0x51,
        ];
        assert_eq!(
            unwind_rule_from_detected_prologue(&bytes[..0], &bytes[0..]),
            Some(UnwindRuleAarch64::NoOp)
        );
        assert_eq!(
            unwind_rule_from_detected_prologue(&bytes[..4], &bytes[4..]),
            Some(UnwindRuleAarch64::NoOp)
        );
        assert_eq!(
            unwind_rule_from_detected_prologue(&bytes[..8], &bytes[8..]),
            Some(UnwindRuleAarch64::OffsetSp { sp_offset_by_16: 1 })
        );
        assert_eq!(
            unwind_rule_from_detected_prologue(&bytes[..12], &bytes[12..]),
            None
        );
    }

    #[test]
    fn test_no_prologue_despite_stack_store() {
        // We're in the middle of a function and are storing something to the stack.
        // But this is not a prologue, so it shouldn't be detected as one.
        //
        // 1004073d0 e8 17 00 f9     str        x8,[sp, #0x28]
        // 1004073d4 03 00 00 14     b          LAB_1004073e0
        // 1004073d8 ff ff 01 a9     stp        xzr,xzr,[sp, #0x18] ; <-- stores the pair xzr, xzr on the stack
        // 1004073dc ff 17 00 f9     str        xzr,[sp, #0x28]
        // 1004073e0 e0 03 00 91     mov        x0,sp

        let bytes = &[
            0xe8, 0x17, 0x00, 0xf9, 0x03, 0x00, 0x00, 0x14, 0xff, 0xff, 0x01, 0xa9, 0xff, 0x17,
            0x00, 0xf9, 0xe0, 0x03, 0x00, 0x91,
        ];
        assert_eq!(
            unwind_rule_from_detected_prologue(&bytes[..0], &bytes[0..]),
            None
        );
        assert_eq!(
            unwind_rule_from_detected_prologue(&bytes[..4], &bytes[4..]),
            None
        );
        assert_eq!(
            unwind_rule_from_detected_prologue(&bytes[..8], &bytes[8..]),
            None
        );
        assert_eq!(
            unwind_rule_from_detected_prologue(&bytes[..12], &bytes[12..]),
            None
        );
        assert_eq!(
            unwind_rule_from_detected_prologue(&bytes[..16], &bytes[16..]),
            None
        );
    }
}
