use super::super::unwind_rule::UnwindRuleX86_64;

pub fn unwind_rule_from_detected_epilogue(
    text_bytes: &[u8],
    pc_offset: usize,
) -> Option<UnwindRuleX86_64> {
    let (slice_from_start, slice_to_end) = text_bytes.split_at(pc_offset);

    let mut sp_offset_by_8 = 0;
    let mut bp_offset_by_8 = None;
    let mut bytes = slice_to_end;
    loop {
        if bytes.is_empty() {
            return None;
        }

        // Detect ret
        if bytes[0] == 0xc3 {
            break;
        }
        // Detect jmp
        if bytes[0] == 0xeb || bytes[0] == 0xe9 || bytes[0] == 0xff {
            // This could be a tail call, or just a regular jump inside the current function.
            // Ideally, we would check whether the jump target is inside this function.
            // But this would require having an accurate idea of where the current function
            // starts and ends.
            // For now, we instead use the following heuristic: Any jmp that directly follows
            // a `pop` instruction is treated as a tail call.
            if sp_offset_by_8 != 0 {
                // We have detected a pop in the previous loop iteration.
                break;
            }
            // This must be the first iteration. Look backwards.
            if let Some(potential_pop_byte) = slice_from_start.last() {
                // Get the previous byte. We have no idea how long the previous instruction
                // is, so we might be looking at a random last byte of a wider instruction.
                // Let's just pray that this is not the case.
                if potential_pop_byte & 0xf8 == 0x58 {
                    // Assuming we haven't just misinterpreted the last byte of a wider
                    // instruction, this is a `pop rXX`.
                    break;
                }
            }
            return None;
        }
        // Detect pop rbp
        if bytes[0] == 0x5d {
            bp_offset_by_8 = Some(sp_offset_by_8 as i16);
            sp_offset_by_8 += 1;
            bytes = &bytes[1..];
            continue;
        }
        // Detect pop rXX
        if (0x58..=0x5f).contains(&bytes[0]) {
            sp_offset_by_8 += 1;
            bytes = &bytes[1..];
            continue;
        }
        // Detect pop rXX with prefix
        if bytes.len() >= 2 && bytes[0] & 0xfe == 0x40 && bytes[1] & 0xf8 == 0x58 {
            sp_offset_by_8 += 1;
            bytes = &bytes[2..];
            continue;
        }
        // Unexpected instruction.
        // This probably means that we weren't in an epilogue after all.
        return None;
    }

    // We've found the return or the tail call.
    let rule = if sp_offset_by_8 == 0 {
        UnwindRuleX86_64::JustReturn
    } else {
        sp_offset_by_8 += 1; // Add one for popping the return address.
        if let Some(bp_storage_offset_from_sp_by_8) = bp_offset_by_8 {
            UnwindRuleX86_64::OffsetSpAndRestoreBp {
                sp_offset_by_8,
                bp_storage_offset_from_sp_by_8,
            }
        } else {
            UnwindRuleX86_64::OffsetSp { sp_offset_by_8 }
        }
    };
    Some(rule)
}
