use super::super::unwind_rule::UnwindRuleX86_64;

pub fn unwind_rule_from_detected_prologue(
    text_bytes: &[u8],
    pc_offset: usize,
) -> Option<UnwindRuleX86_64> {
    let (slice_from_start, slice_to_end) = text_bytes.split_at(pc_offset);
    if !is_next_instruction_expected_in_prologue(slice_to_end) {
        return None;
    }
    // We're in a prologue. Find the current stack depth of this frame by
    // walking backwards. This is risky business, because x86 is a variable
    // length encoding so you never know what you're looking at if you look
    // backwards.
    // Let's do it anyway and hope our heuristics are good enough so that
    // they work in more cases than they fail in.
    let mut cursor = slice_from_start.len();
    let mut sp_offset_by_8 = 0;
    loop {
        if cursor >= 4 {
            // Detect push rbp; mov rbp, rsp [0x55, 0x48 0x89 0xe5]
            if slice_from_start[cursor - 4..cursor] == [0x55, 0x48, 0x89, 0xe5] {
                return Some(UnwindRuleX86_64::UseFramePointer);
            }
        }
        if cursor >= 1 {
            // Detect push rXX with optional prefix
            let byte = slice_from_start[cursor - 1];
            if byte & 0xf8 == 0x50 {
                sp_offset_by_8 += 1;
                cursor -= 1;

                // Consume prefix, if present
                if cursor >= 1 && slice_from_start[cursor - 1] & 0xfe == 0x40 {
                    cursor -= 1;
                }

                continue;
            }
        }
        break;
    }
    sp_offset_by_8 += 1; // Add one for popping the return address.
    Some(UnwindRuleX86_64::OffsetSp { sp_offset_by_8 })
}

fn is_next_instruction_expected_in_prologue(bytes: &[u8]) -> bool {
    if bytes.len() < 4 {
        return false;
    }

    // Detect push rXX
    if bytes[0] & 0xf8 == 0x50 {
        return true;
    }
    // Detect push rXX with prefix
    if bytes[0] & 0xfe == 0x40 && bytes[1] & 0xf8 == 0x50 {
        return true;
    }
    // Detect sub rsp, 0xXX (8-bit immediate operand)
    if bytes[0..2] == [0x83, 0xec] {
        return true;
    }
    // Detect sub rsp, 0xXX with prefix (8-bit immediate operand)
    if bytes[0..3] == [0x48, 0x83, 0xec] {
        return true;
    }
    // Detect sub rsp, 0xXX (32-bit immediate operand)
    if bytes[0..2] == [0x81, 0xec] {
        return true;
    }
    // Detect sub rsp, 0xXX with prefix (32-bit immediate operand)
    if bytes[0..3] == [0x48, 0x81, 0xec] {
        return true;
    }
    // Detect mov rbp, rsp [0x48 0x89 0xe5]
    if bytes[0..3] == [0x48, 0x89, 0xe5] {
        return true;
    }

    false
}

// TODO: Write tests for different "sub" types
// 4e88e40  41 57                 push  r15
// 4e88e42  41 56                 push  r14
// 4e88e44  53                    push  rbx
// 4e88e45  48 81 EC 80 00 00 00  sub  rsp, 0x80
// 4e88e4c  48 89 F3              mov  rbx, rsi
//
//
// 4423f9  55           push  rbp
// 4423fa  48 89 E5     mov  rbp, rsp
// 4423fd  41 57        push  r15
// 4423ff  41 56        push  r14
// 442401  41 55        push  r13
// 442403  41 54        push  r12
// 442405  53           push  rbx
// 442406  48 83 EC 18  sub  rsp, 0x18
// 44240a  48 8B 07     mov  rax, qword [rdi]
