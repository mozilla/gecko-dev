use super::arch::ArchAarch64;
use crate::instruction_analysis::InstructionAnalysis;

mod epilogue;
mod prologue;

use epilogue::unwind_rule_from_detected_epilogue;
use prologue::unwind_rule_from_detected_prologue;

impl InstructionAnalysis for ArchAarch64 {
    fn rule_from_prologue_analysis(
        text_bytes: &[u8],
        pc_offset: usize,
    ) -> Option<Self::UnwindRule> {
        let (slice_from_start, slice_to_end) = text_bytes.split_at(pc_offset);
        unwind_rule_from_detected_prologue(slice_from_start, slice_to_end)
    }

    fn rule_from_epilogue_analysis(
        text_bytes: &[u8],
        pc_offset: usize,
    ) -> Option<Self::UnwindRule> {
        unwind_rule_from_detected_epilogue(text_bytes, pc_offset)
    }
}
