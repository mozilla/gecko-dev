//! Encoding tables for x86 ISAs.

use super::registers::*;
use bitset::BitSet;
use cursor::{Cursor, FuncCursor};
use flowgraph::ControlFlowGraph;
use ir::condcodes::IntCC;
use ir::{self, Function, Inst, InstBuilder};
use isa;
use isa::constraints::*;
use isa::enc_tables::*;
use isa::encoding::base_size;
use isa::encoding::RecipeSizing;
use isa::RegUnit;
use regalloc::RegDiversions;

include!(concat!(env!("OUT_DIR"), "/encoding-x86.rs"));
include!(concat!(env!("OUT_DIR"), "/legalize-x86.rs"));

pub fn needs_sib_byte(reg: RegUnit) -> bool {
    reg == RU::r12 as RegUnit || reg == RU::rsp as RegUnit
}
pub fn needs_offset(reg: RegUnit) -> bool {
    reg == RU::r13 as RegUnit || reg == RU::rbp as RegUnit
}
pub fn needs_sib_byte_or_offset(reg: RegUnit) -> bool {
    needs_sib_byte(reg) || needs_offset(reg)
}

fn additional_size_if(
    op_index: usize,
    inst: Inst,
    divert: &RegDiversions,
    func: &Function,
    condition_func: fn(RegUnit) -> bool,
) -> u8 {
    let addr_reg = divert.reg(func.dfg.inst_args(inst)[op_index], &func.locations);
    if condition_func(addr_reg) {
        1
    } else {
        0
    }
}

fn size_plus_maybe_offset_for_in_reg_0(
    sizing: &RecipeSizing,
    inst: Inst,
    divert: &RegDiversions,
    func: &Function,
) -> u8 {
    sizing.base_size + additional_size_if(0, inst, divert, func, needs_offset)
}
fn size_plus_maybe_offset_for_in_reg_1(
    sizing: &RecipeSizing,
    inst: Inst,
    divert: &RegDiversions,
    func: &Function,
) -> u8 {
    sizing.base_size + additional_size_if(1, inst, divert, func, needs_offset)
}
fn size_plus_maybe_sib_for_in_reg_0(
    sizing: &RecipeSizing,
    inst: Inst,
    divert: &RegDiversions,
    func: &Function,
) -> u8 {
    sizing.base_size + additional_size_if(0, inst, divert, func, needs_sib_byte)
}
fn size_plus_maybe_sib_for_in_reg_1(
    sizing: &RecipeSizing,
    inst: Inst,
    divert: &RegDiversions,
    func: &Function,
) -> u8 {
    sizing.base_size + additional_size_if(1, inst, divert, func, needs_sib_byte)
}
fn size_plus_maybe_sib_or_offset_for_in_reg_0(
    sizing: &RecipeSizing,
    inst: Inst,
    divert: &RegDiversions,
    func: &Function,
) -> u8 {
    sizing.base_size + additional_size_if(0, inst, divert, func, needs_sib_byte_or_offset)
}
fn size_plus_maybe_sib_or_offset_for_in_reg_1(
    sizing: &RecipeSizing,
    inst: Inst,
    divert: &RegDiversions,
    func: &Function,
) -> u8 {
    sizing.base_size + additional_size_if(1, inst, divert, func, needs_sib_byte_or_offset)
}

/// Expand the `sdiv` and `srem` instructions using `x86_sdivmodx`.
fn expand_sdivrem(
    inst: ir::Inst,
    func: &mut ir::Function,
    cfg: &mut ControlFlowGraph,
    isa: &isa::TargetIsa,
) {
    let (x, y, is_srem) = match func.dfg[inst] {
        ir::InstructionData::Binary {
            opcode: ir::Opcode::Sdiv,
            args,
        } => (args[0], args[1], false),
        ir::InstructionData::Binary {
            opcode: ir::Opcode::Srem,
            args,
        } => (args[0], args[1], true),
        _ => panic!("Need sdiv/srem: {}", func.dfg.display_inst(inst, None)),
    };
    let avoid_div_traps = isa.flags().avoid_div_traps();
    let old_ebb = func.layout.pp_ebb(inst);
    let result = func.dfg.first_result(inst);
    let ty = func.dfg.value_type(result);

    let mut pos = FuncCursor::new(func).at_inst(inst);
    pos.use_srcloc(inst);
    pos.func.dfg.clear_results(inst);

    // If we can tolerate native division traps, sdiv doesn't need branching.
    if !avoid_div_traps && !is_srem {
        let xhi = pos.ins().sshr_imm(x, i64::from(ty.lane_bits()) - 1);
        pos.ins().with_result(result).x86_sdivmodx(x, xhi, y);
        pos.remove_inst();
        return;
    }

    // EBB handling the -1 divisor case.
    let minus_one = pos.func.dfg.make_ebb();

    // Final EBB with one argument representing the final result value.
    let done = pos.func.dfg.make_ebb();

    // Move the `inst` result value onto the `done` EBB.
    pos.func.dfg.attach_ebb_param(done, result);

    // Start by checking for a -1 divisor which needs to be handled specially.
    let is_m1 = pos.ins().ifcmp_imm(y, -1);
    pos.ins().brif(IntCC::Equal, is_m1, minus_one, &[]);

    // Put in an explicit division-by-zero trap if the environment requires it.
    if avoid_div_traps {
        pos.ins().trapz(y, ir::TrapCode::IntegerDivisionByZero);
    }

    // Now it is safe to execute the `x86_sdivmodx` instruction which will still trap on division
    // by zero.
    let xhi = pos.ins().sshr_imm(x, i64::from(ty.lane_bits()) - 1);
    let (quot, rem) = pos.ins().x86_sdivmodx(x, xhi, y);
    let divres = if is_srem { rem } else { quot };
    pos.ins().jump(done, &[divres]);

    // Now deal with the -1 divisor case.
    pos.insert_ebb(minus_one);
    let m1_result = if is_srem {
        // x % -1 = 0.
        pos.ins().iconst(ty, 0)
    } else {
        // Explicitly check for overflow: Trap when x == INT_MIN.
        debug_assert!(avoid_div_traps, "Native trapping divide handled above");
        let f = pos.ins().ifcmp_imm(x, -1 << (ty.lane_bits() - 1));
        pos.ins()
            .trapif(IntCC::Equal, f, ir::TrapCode::IntegerOverflow);
        // x / -1 = -x.
        pos.ins().irsub_imm(x, 0)
    };

    // Recycle the original instruction as a jump.
    pos.func.dfg.replace(inst).jump(done, &[m1_result]);

    // Finally insert a label for the completion.
    pos.next_inst();
    pos.insert_ebb(done);

    cfg.recompute_ebb(pos.func, old_ebb);
    cfg.recompute_ebb(pos.func, minus_one);
    cfg.recompute_ebb(pos.func, done);
}

/// Expand the `udiv` and `urem` instructions using `x86_udivmodx`.
fn expand_udivrem(
    inst: ir::Inst,
    func: &mut ir::Function,
    _cfg: &mut ControlFlowGraph,
    isa: &isa::TargetIsa,
) {
    let (x, y, is_urem) = match func.dfg[inst] {
        ir::InstructionData::Binary {
            opcode: ir::Opcode::Udiv,
            args,
        } => (args[0], args[1], false),
        ir::InstructionData::Binary {
            opcode: ir::Opcode::Urem,
            args,
        } => (args[0], args[1], true),
        _ => panic!("Need udiv/urem: {}", func.dfg.display_inst(inst, None)),
    };
    let avoid_div_traps = isa.flags().avoid_div_traps();
    let result = func.dfg.first_result(inst);
    let ty = func.dfg.value_type(result);

    let mut pos = FuncCursor::new(func).at_inst(inst);
    pos.use_srcloc(inst);
    pos.func.dfg.clear_results(inst);

    // Put in an explicit division-by-zero trap if the environment requires it.
    if avoid_div_traps {
        pos.ins().trapz(y, ir::TrapCode::IntegerDivisionByZero);
    }

    // Now it is safe to execute the `x86_udivmodx` instruction.
    let xhi = pos.ins().iconst(ty, 0);
    let reuse = if is_urem {
        [None, Some(result)]
    } else {
        [Some(result), None]
    };
    pos.ins().with_results(reuse).x86_udivmodx(x, xhi, y);
    pos.remove_inst();
}

/// Expand the `fmin` and `fmax` instructions using the x86 `x86_fmin` and `x86_fmax`
/// instructions.
fn expand_minmax(
    inst: ir::Inst,
    func: &mut ir::Function,
    cfg: &mut ControlFlowGraph,
    _isa: &isa::TargetIsa,
) {
    use ir::condcodes::FloatCC;

    let (x, y, x86_opc, bitwise_opc) = match func.dfg[inst] {
        ir::InstructionData::Binary {
            opcode: ir::Opcode::Fmin,
            args,
        } => (args[0], args[1], ir::Opcode::X86Fmin, ir::Opcode::Bor),
        ir::InstructionData::Binary {
            opcode: ir::Opcode::Fmax,
            args,
        } => (args[0], args[1], ir::Opcode::X86Fmax, ir::Opcode::Band),
        _ => panic!("Expected fmin/fmax: {}", func.dfg.display_inst(inst, None)),
    };
    let old_ebb = func.layout.pp_ebb(inst);

    // We need to handle the following conditions, depending on how x and y compare:
    //
    // 1. LT or GT: The native `x86_opc` min/max instruction does what we need.
    // 2. EQ: We need to use `bitwise_opc` to make sure that
    //    fmin(0.0, -0.0) -> -0.0 and fmax(0.0, -0.0) -> 0.0.
    // 3. UN: We need to produce a quiet NaN that is canonical if the inputs are canonical.

    // EBB handling case 3) where one operand is NaN.
    let uno_ebb = func.dfg.make_ebb();

    // EBB that handles the unordered or equal cases 2) and 3).
    let ueq_ebb = func.dfg.make_ebb();

    // Final EBB with one argument representing the final result value.
    let done = func.dfg.make_ebb();

    // The basic blocks are laid out to minimize branching for the common cases:
    //
    // 1) One branch not taken, one jump.
    // 2) One branch taken.
    // 3) Two branches taken, one jump.

    // Move the `inst` result value onto the `done` EBB.
    let result = func.dfg.first_result(inst);
    let ty = func.dfg.value_type(result);
    func.dfg.clear_results(inst);
    func.dfg.attach_ebb_param(done, result);

    // Test for case 1) ordered and not equal.
    let mut pos = FuncCursor::new(func).at_inst(inst);
    pos.use_srcloc(inst);
    let cmp_ueq = pos.ins().fcmp(FloatCC::UnorderedOrEqual, x, y);
    pos.ins().brnz(cmp_ueq, ueq_ebb, &[]);

    // Handle the common ordered, not equal (LT|GT) case.
    let one_inst = pos.ins().Binary(x86_opc, ty, x, y).0;
    let one_result = pos.func.dfg.first_result(one_inst);
    pos.ins().jump(done, &[one_result]);

    // Case 3) Unordered.
    // We know that at least one operand is a NaN that needs to be propagated. We simply use an
    // `fadd` instruction which has the same NaN propagation semantics.
    pos.insert_ebb(uno_ebb);
    let uno_result = pos.ins().fadd(x, y);
    pos.ins().jump(done, &[uno_result]);

    // Case 2) or 3).
    pos.insert_ebb(ueq_ebb);
    // Test for case 3) (UN) one value is NaN.
    // TODO: When we get support for flag values, we can reuse the above comparison.
    let cmp_uno = pos.ins().fcmp(FloatCC::Unordered, x, y);
    pos.ins().brnz(cmp_uno, uno_ebb, &[]);

    // We are now in case 2) where x and y compare EQ.
    // We need a bitwise operation to get the sign right.
    let bw_inst = pos.ins().Binary(bitwise_opc, ty, x, y).0;
    let bw_result = pos.func.dfg.first_result(bw_inst);
    // This should become a fall-through for this second most common case.
    // Recycle the original instruction as a jump.
    pos.func.dfg.replace(inst).jump(done, &[bw_result]);

    // Finally insert a label for the completion.
    pos.next_inst();
    pos.insert_ebb(done);

    cfg.recompute_ebb(pos.func, old_ebb);
    cfg.recompute_ebb(pos.func, ueq_ebb);
    cfg.recompute_ebb(pos.func, uno_ebb);
    cfg.recompute_ebb(pos.func, done);
}

/// x86 has no unsigned-to-float conversions. We handle the easy case of zero-extending i32 to
/// i64 with a pattern, the rest needs more code.
fn expand_fcvt_from_uint(
    inst: ir::Inst,
    func: &mut ir::Function,
    cfg: &mut ControlFlowGraph,
    _isa: &isa::TargetIsa,
) {
    use ir::condcodes::IntCC;

    let x;
    match func.dfg[inst] {
        ir::InstructionData::Unary {
            opcode: ir::Opcode::FcvtFromUint,
            arg,
        } => x = arg,
        _ => panic!("Need fcvt_from_uint: {}", func.dfg.display_inst(inst, None)),
    }
    let xty = func.dfg.value_type(x);
    let result = func.dfg.first_result(inst);
    let ty = func.dfg.value_type(result);
    let mut pos = FuncCursor::new(func).at_inst(inst);
    pos.use_srcloc(inst);

    // Conversion from unsigned 32-bit is easy on x86-64.
    // TODO: This should be guarded by an ISA check.
    if xty == ir::types::I32 {
        let wide = pos.ins().uextend(ir::types::I64, x);
        pos.func.dfg.replace(inst).fcvt_from_sint(ty, wide);
        return;
    }

    let old_ebb = pos.func.layout.pp_ebb(inst);

    // EBB handling the case where x < 0.
    let neg_ebb = pos.func.dfg.make_ebb();

    // Final EBB with one argument representing the final result value.
    let done = pos.func.dfg.make_ebb();

    // Move the `inst` result value onto the `done` EBB.
    pos.func.dfg.clear_results(inst);
    pos.func.dfg.attach_ebb_param(done, result);

    // If x as a signed int is not negative, we can use the existing `fcvt_from_sint` instruction.
    let is_neg = pos.ins().icmp_imm(IntCC::SignedLessThan, x, 0);
    pos.ins().brnz(is_neg, neg_ebb, &[]);

    // Easy case: just use a signed conversion.
    let posres = pos.ins().fcvt_from_sint(ty, x);
    pos.ins().jump(done, &[posres]);

    // Now handle the negative case.
    pos.insert_ebb(neg_ebb);

    // Divide x by two to get it in range for the signed conversion, keep the LSB, and scale it
    // back up on the FP side.
    let ihalf = pos.ins().ushr_imm(x, 1);
    let lsb = pos.ins().band_imm(x, 1);
    let ifinal = pos.ins().bor(ihalf, lsb);
    let fhalf = pos.ins().fcvt_from_sint(ty, ifinal);
    let negres = pos.ins().fadd(fhalf, fhalf);

    // Recycle the original instruction as a jump.
    pos.func.dfg.replace(inst).jump(done, &[negres]);

    // Finally insert a label for the completion.
    pos.next_inst();
    pos.insert_ebb(done);

    cfg.recompute_ebb(pos.func, old_ebb);
    cfg.recompute_ebb(pos.func, neg_ebb);
    cfg.recompute_ebb(pos.func, done);
}

fn expand_fcvt_to_sint(
    inst: ir::Inst,
    func: &mut ir::Function,
    cfg: &mut ControlFlowGraph,
    _isa: &isa::TargetIsa,
) {
    use ir::condcodes::{FloatCC, IntCC};
    use ir::immediates::{Ieee32, Ieee64};

    let x = match func.dfg[inst] {
        ir::InstructionData::Unary {
            opcode: ir::Opcode::FcvtToSint,
            arg,
        } => arg,
        _ => panic!("Need fcvt_to_sint: {}", func.dfg.display_inst(inst, None)),
    };
    let old_ebb = func.layout.pp_ebb(inst);
    let xty = func.dfg.value_type(x);
    let result = func.dfg.first_result(inst);
    let ty = func.dfg.value_type(result);

    // Final EBB after the bad value checks.
    let done = func.dfg.make_ebb();

    // The `x86_cvtt2si` performs the desired conversion, but it doesn't trap on NaN or overflow.
    // It produces an INT_MIN result instead.
    func.dfg.replace(inst).x86_cvtt2si(ty, x);

    let mut pos = FuncCursor::new(func).after_inst(inst);
    pos.use_srcloc(inst);

    let is_done = pos
        .ins()
        .icmp_imm(IntCC::NotEqual, result, 1 << (ty.lane_bits() - 1));
    pos.ins().brnz(is_done, done, &[]);

    // We now have the following possibilities:
    //
    // 1. INT_MIN was actually the correct conversion result.
    // 2. The input was NaN -> trap bad_toint
    // 3. The input was out of range -> trap int_ovf
    //

    // Check for NaN.
    let is_nan = pos.ins().fcmp(FloatCC::Unordered, x, x);
    pos.ins()
        .trapnz(is_nan, ir::TrapCode::BadConversionToInteger);

    // Check for case 1: INT_MIN is the correct result.
    // Determine the smallest floating point number that would convert to INT_MIN.
    let mut overflow_cc = FloatCC::LessThan;
    let output_bits = ty.lane_bits();
    let flimit = match xty {
        ir::types::F32 =>
        // An f32 can represent `i16::min_value() - 1` exactly with precision to spare, so
        // there are values less than -2^(N-1) that convert correctly to INT_MIN.
        {
            pos.ins().f32const(if output_bits < 32 {
                overflow_cc = FloatCC::LessThanOrEqual;
                Ieee32::fcvt_to_sint_negative_overflow(output_bits)
            } else {
                Ieee32::pow2(output_bits - 1).neg()
            })
        }
        ir::types::F64 =>
        // An f64 can represent `i32::min_value() - 1` exactly with precision to spare, so
        // there are values less than -2^(N-1) that convert correctly to INT_MIN.
        {
            pos.ins().f64const(if output_bits < 64 {
                overflow_cc = FloatCC::LessThanOrEqual;
                Ieee64::fcvt_to_sint_negative_overflow(output_bits)
            } else {
                Ieee64::pow2(output_bits - 1).neg()
            })
        }
        _ => panic!("Can't convert {}", xty),
    };
    let overflow = pos.ins().fcmp(overflow_cc, x, flimit);
    pos.ins().trapnz(overflow, ir::TrapCode::IntegerOverflow);

    // Finally, we could have a positive value that is too large.
    let fzero = match xty {
        ir::types::F32 => pos.ins().f32const(Ieee32::with_bits(0)),
        ir::types::F64 => pos.ins().f64const(Ieee64::with_bits(0)),
        _ => panic!("Can't convert {}", xty),
    };
    let overflow = pos.ins().fcmp(FloatCC::GreaterThanOrEqual, x, fzero);
    pos.ins().trapnz(overflow, ir::TrapCode::IntegerOverflow);

    pos.ins().jump(done, &[]);
    pos.insert_ebb(done);

    cfg.recompute_ebb(pos.func, old_ebb);
    cfg.recompute_ebb(pos.func, done);
}

fn expand_fcvt_to_sint_sat(
    inst: ir::Inst,
    func: &mut ir::Function,
    cfg: &mut ControlFlowGraph,
    _isa: &isa::TargetIsa,
) {
    use ir::condcodes::{FloatCC, IntCC};
    use ir::immediates::{Ieee32, Ieee64};

    let x = match func.dfg[inst] {
        ir::InstructionData::Unary {
            opcode: ir::Opcode::FcvtToSintSat,
            arg,
        } => arg,
        _ => panic!(
            "Need fcvt_to_sint_sat: {}",
            func.dfg.display_inst(inst, None)
        ),
    };

    let old_ebb = func.layout.pp_ebb(inst);
    let xty = func.dfg.value_type(x);
    let result = func.dfg.first_result(inst);
    let ty = func.dfg.value_type(result);

    // Final EBB after the bad value checks.
    let done_ebb = func.dfg.make_ebb();
    func.dfg.clear_results(inst);
    func.dfg.attach_ebb_param(done_ebb, result);

    let mut pos = FuncCursor::new(func).at_inst(inst);
    pos.use_srcloc(inst);

    // The `x86_cvtt2si` performs the desired conversion, but it doesn't trap on NaN or
    // overflow. It produces an INT_MIN result instead.
    let cvtt2si = pos.ins().x86_cvtt2si(ty, x);

    let is_done = pos
        .ins()
        .icmp_imm(IntCC::NotEqual, cvtt2si, 1 << (ty.lane_bits() - 1));
    pos.ins().brnz(is_done, done_ebb, &[cvtt2si]);

    // We now have the following possibilities:
    //
    // 1. INT_MIN was actually the correct conversion result.
    // 2. The input was NaN -> replace the result value with 0.
    // 3. The input was out of range -> saturate the result to the min/max value.

    // Check for NaN, which is truncated to 0.
    let zero = pos.ins().iconst(ty, 0);
    let is_nan = pos.ins().fcmp(FloatCC::Unordered, x, x);
    pos.ins().brnz(is_nan, done_ebb, &[zero]);

    // Check for case 1: INT_MIN is the correct result.
    // Determine the smallest floating point number that would convert to INT_MIN.
    let mut overflow_cc = FloatCC::LessThan;
    let output_bits = ty.lane_bits();
    let flimit = match xty {
        ir::types::F32 =>
        // An f32 can represent `i16::min_value() - 1` exactly with precision to spare, so
        // there are values less than -2^(N-1) that convert correctly to INT_MIN.
        {
            pos.ins().f32const(if output_bits < 32 {
                overflow_cc = FloatCC::LessThanOrEqual;
                Ieee32::fcvt_to_sint_negative_overflow(output_bits)
            } else {
                Ieee32::pow2(output_bits - 1).neg()
            })
        }
        ir::types::F64 =>
        // An f64 can represent `i32::min_value() - 1` exactly with precision to spare, so
        // there are values less than -2^(N-1) that convert correctly to INT_MIN.
        {
            pos.ins().f64const(if output_bits < 64 {
                overflow_cc = FloatCC::LessThanOrEqual;
                Ieee64::fcvt_to_sint_negative_overflow(output_bits)
            } else {
                Ieee64::pow2(output_bits - 1).neg()
            })
        }
        _ => panic!("Can't convert {}", xty),
    };

    let overflow = pos.ins().fcmp(overflow_cc, x, flimit);
    let min_imm = match ty {
        ir::types::I32 => i32::min_value() as i64,
        ir::types::I64 => i64::min_value(),
        _ => panic!("Don't know the min value for {}", ty),
    };
    let min_value = pos.ins().iconst(ty, min_imm);
    pos.ins().brnz(overflow, done_ebb, &[min_value]);

    // Finally, we could have a positive value that is too large.
    let fzero = match xty {
        ir::types::F32 => pos.ins().f32const(Ieee32::with_bits(0)),
        ir::types::F64 => pos.ins().f64const(Ieee64::with_bits(0)),
        _ => panic!("Can't convert {}", xty),
    };

    let max_imm = match ty {
        ir::types::I32 => i32::max_value() as i64,
        ir::types::I64 => i64::max_value(),
        _ => panic!("Don't know the max value for {}", ty),
    };
    let max_value = pos.ins().iconst(ty, max_imm);

    let overflow = pos.ins().fcmp(FloatCC::GreaterThanOrEqual, x, fzero);
    pos.ins().brnz(overflow, done_ebb, &[max_value]);

    // Recycle the original instruction.
    pos.func.dfg.replace(inst).jump(done_ebb, &[cvtt2si]);

    // Finally insert a label for the completion.
    pos.next_inst();
    pos.insert_ebb(done_ebb);

    cfg.recompute_ebb(pos.func, old_ebb);
    cfg.recompute_ebb(pos.func, done_ebb);
}

fn expand_fcvt_to_uint(
    inst: ir::Inst,
    func: &mut ir::Function,
    cfg: &mut ControlFlowGraph,
    _isa: &isa::TargetIsa,
) {
    use ir::condcodes::{FloatCC, IntCC};
    use ir::immediates::{Ieee32, Ieee64};

    let x = match func.dfg[inst] {
        ir::InstructionData::Unary {
            opcode: ir::Opcode::FcvtToUint,
            arg,
        } => arg,
        _ => panic!("Need fcvt_to_uint: {}", func.dfg.display_inst(inst, None)),
    };

    let old_ebb = func.layout.pp_ebb(inst);
    let xty = func.dfg.value_type(x);
    let result = func.dfg.first_result(inst);
    let ty = func.dfg.value_type(result);

    // EBB handling numbers >= 2^(N-1).
    let large = func.dfg.make_ebb();

    // Final EBB after the bad value checks.
    let done = func.dfg.make_ebb();

    // Move the `inst` result value onto the `done` EBB.
    func.dfg.clear_results(inst);
    func.dfg.attach_ebb_param(done, result);

    let mut pos = FuncCursor::new(func).at_inst(inst);
    pos.use_srcloc(inst);

    // Start by materializing the floating point constant 2^(N-1) where N is the number of bits in
    // the destination integer type.
    let pow2nm1 = match xty {
        ir::types::F32 => pos.ins().f32const(Ieee32::pow2(ty.lane_bits() - 1)),
        ir::types::F64 => pos.ins().f64const(Ieee64::pow2(ty.lane_bits() - 1)),
        _ => panic!("Can't convert {}", xty),
    };
    let is_large = pos.ins().ffcmp(x, pow2nm1);
    pos.ins()
        .brff(FloatCC::GreaterThanOrEqual, is_large, large, &[]);

    // We need to generate a specific trap code when `x` is NaN, so reuse the flags from the
    // previous comparison.
    pos.ins().trapff(
        FloatCC::Unordered,
        is_large,
        ir::TrapCode::BadConversionToInteger,
    );

    // Now we know that x < 2^(N-1) and not NaN.
    let sres = pos.ins().x86_cvtt2si(ty, x);
    let is_neg = pos.ins().ifcmp_imm(sres, 0);
    pos.ins()
        .brif(IntCC::SignedGreaterThanOrEqual, is_neg, done, &[sres]);
    pos.ins().trap(ir::TrapCode::IntegerOverflow);

    // Handle the case where x >= 2^(N-1) and not NaN.
    pos.insert_ebb(large);
    let adjx = pos.ins().fsub(x, pow2nm1);
    let lres = pos.ins().x86_cvtt2si(ty, adjx);
    let is_neg = pos.ins().ifcmp_imm(lres, 0);
    pos.ins()
        .trapif(IntCC::SignedLessThan, is_neg, ir::TrapCode::IntegerOverflow);
    let lfinal = pos.ins().iadd_imm(lres, 1 << (ty.lane_bits() - 1));

    // Recycle the original instruction as a jump.
    pos.func.dfg.replace(inst).jump(done, &[lfinal]);

    // Finally insert a label for the completion.
    pos.next_inst();
    pos.insert_ebb(done);

    cfg.recompute_ebb(pos.func, old_ebb);
    cfg.recompute_ebb(pos.func, large);
    cfg.recompute_ebb(pos.func, done);
}

fn expand_fcvt_to_uint_sat(
    inst: ir::Inst,
    func: &mut ir::Function,
    cfg: &mut ControlFlowGraph,
    _isa: &isa::TargetIsa,
) {
    use ir::condcodes::{FloatCC, IntCC};
    use ir::immediates::{Ieee32, Ieee64};

    let x = match func.dfg[inst] {
        ir::InstructionData::Unary {
            opcode: ir::Opcode::FcvtToUintSat,
            arg,
        } => arg,
        _ => panic!(
            "Need fcvt_to_uint_sat: {}",
            func.dfg.display_inst(inst, None)
        ),
    };

    let old_ebb = func.layout.pp_ebb(inst);
    let xty = func.dfg.value_type(x);
    let result = func.dfg.first_result(inst);
    let ty = func.dfg.value_type(result);

    // EBB handling numbers >= 2^(N-1).
    let large = func.dfg.make_ebb();

    // Final EBB after the bad value checks.
    let done = func.dfg.make_ebb();

    // Move the `inst` result value onto the `done` EBB.
    func.dfg.clear_results(inst);
    func.dfg.attach_ebb_param(done, result);

    let mut pos = FuncCursor::new(func).at_inst(inst);
    pos.use_srcloc(inst);

    // Start by materializing the floating point constant 2^(N-1) where N is the number of bits in
    // the destination integer type.
    let pow2nm1 = match xty {
        ir::types::F32 => pos.ins().f32const(Ieee32::pow2(ty.lane_bits() - 1)),
        ir::types::F64 => pos.ins().f64const(Ieee64::pow2(ty.lane_bits() - 1)),
        _ => panic!("Can't convert {}", xty),
    };
    let zero = pos.ins().iconst(ty, 0);
    let is_large = pos.ins().ffcmp(x, pow2nm1);
    pos.ins()
        .brff(FloatCC::GreaterThanOrEqual, is_large, large, &[]);

    // We need to generate zero when `x` is NaN, so reuse the flags from the previous comparison.
    pos.ins().brff(FloatCC::Unordered, is_large, done, &[zero]);

    // Now we know that x < 2^(N-1) and not NaN. If the result of the cvtt2si is positive, we're
    // done; otherwise saturate to the minimum unsigned value, that is 0.
    let sres = pos.ins().x86_cvtt2si(ty, x);
    let is_neg = pos.ins().ifcmp_imm(sres, 0);
    pos.ins()
        .brif(IntCC::SignedGreaterThanOrEqual, is_neg, done, &[sres]);
    pos.ins().jump(done, &[zero]);

    // Handle the case where x >= 2^(N-1) and not NaN.
    pos.insert_ebb(large);
    let adjx = pos.ins().fsub(x, pow2nm1);
    let lres = pos.ins().x86_cvtt2si(ty, adjx);
    let max_value = pos.ins().iconst(
        ty,
        match ty {
            ir::types::I32 => u32::max_value() as i64,
            ir::types::I64 => u64::max_value() as i64,
            _ => panic!("Can't convert {}", ty),
        },
    );
    let is_neg = pos.ins().ifcmp_imm(lres, 0);
    pos.ins()
        .brif(IntCC::SignedLessThan, is_neg, done, &[max_value]);
    let lfinal = pos.ins().iadd_imm(lres, 1 << (ty.lane_bits() - 1));

    // Recycle the original instruction as a jump.
    pos.func.dfg.replace(inst).jump(done, &[lfinal]);

    // Finally insert a label for the completion.
    pos.next_inst();
    pos.insert_ebb(done);

    cfg.recompute_ebb(pos.func, old_ebb);
    cfg.recompute_ebb(pos.func, large);
    cfg.recompute_ebb(pos.func, done);
}
