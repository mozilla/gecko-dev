//! A verifier for ensuring that functions are well formed.
//! It verifies:
//!
//! EBB integrity
//!
//! - All instructions reached from the `ebb_insts` iterator must belong to
//!   the EBB as reported by `inst_ebb()`.
//! - Every EBB must end in a terminator instruction, and no other instruction
//!   can be a terminator.
//! - Every value in the `ebb_params` iterator belongs to the EBB as reported by `value_ebb`.
//!
//! Instruction integrity
//!
//! - The instruction format must match the opcode.
//! - All result values must be created for multi-valued instructions.
//! - All referenced entities must exist. (Values, EBBs, stack slots, ...)
//! - Instructions must not reference (eg. branch to) the entry block.
//!
//! SSA form
//!
//! - Values must be defined by an instruction that exists and that is inserted in
//!   an EBB, or be an argument of an existing EBB.
//! - Values used by an instruction must dominate the instruction.
//!
//! Control flow graph and dominator tree integrity:
//!
//! - All predecessors in the CFG must be branches to the EBB.
//! - All branches to an EBB must be present in the CFG.
//! - A recomputed dominator tree is identical to the existing one.
//!
//! Type checking
//!
//! - Compare input and output values against the opcode's type constraints.
//!   For polymorphic opcodes, determine the controlling type variable first.
//! - Branches and jumps must pass arguments to destination EBBs that match the
//!   expected types exactly. The number of arguments must match.
//! - All EBBs in a jump table must take no arguments.
//! - Function calls are type checked against their signature.
//! - The entry block must take arguments that match the signature of the current
//!   function.
//! - All return instructions must have return value operands matching the current
//!   function signature.
//!
//! Global values
//!
//! - Detect cycles in global values.
//! - Detect use of 'vmctx' global value when no corresponding parameter is defined.
//!
//! TODO:
//! Ad hoc checking
//!
//! - Stack slot loads and stores must be in-bounds.
//! - Immediate constraints for certain opcodes, like `udiv_imm v3, 0`.
//! - `Insertlane` and `extractlane` instructions have immediate lane numbers that must be in
//!   range for their polymorphic type.
//! - Swizzle and shuffle instructions take a variable number of lane arguments. The number
//!   of arguments must match the destination type, and the lane indexes must be in range.

use self::flags::verify_flags;
use dbg::DisplayList;
use dominator_tree::DominatorTree;
use entity::SparseSet;
use flowgraph::{BasicBlock, ControlFlowGraph};
use ir;
use ir::entities::AnyEntity;
use ir::instructions::{BranchInfo, CallInfo, InstructionFormat, ResolvedConstraint};
use ir::{
    types, ArgumentLoc, Ebb, FuncRef, Function, GlobalValue, Inst, JumpTable, Opcode, SigRef,
    StackSlot, StackSlotKind, Type, Value, ValueDef, ValueList, ValueLoc,
};
use isa::TargetIsa;
use iterators::IteratorExtras;
use settings::FlagsOrIsa;
use std::cmp::Ordering;
use std::collections::BTreeSet;
use std::fmt::{self, Display, Formatter, Write};
use std::string::String;
use std::vec::Vec;
use timing;

pub use self::cssa::verify_cssa;
pub use self::liveness::verify_liveness;
pub use self::locations::verify_locations;

/// Report an error.
///
/// The first argument must be a `&mut VerifierErrors` reference, and the following
/// argument defines the location of the error and must implement `Into<AnyEntity>`.
/// Finally, subsequent arguments will be formatted using `format!()` and set
/// as the error message.
macro_rules! report {
    ( $errors: expr, $loc: expr, $msg: tt ) => {
        $errors.0.push(::verifier::VerifierError {
            location: $loc.into(),
            message: String::from($msg),
        })
    };

    ( $errors: expr, $loc: expr, $fmt: tt, $( $arg: expr ),+ ) => {
        $errors.0.push(::verifier::VerifierError {
            location: $loc.into(),
            message: format!( $fmt, $( $arg ),+ ),
        })
    };
}

/// Diagnose a fatal error, and return `Err`.
macro_rules! fatal {
    ( $( $arg: expr ),+ ) => ({
        report!( $( $arg ),+ );
        Err(())
    });
}

/// Diagnose a non-fatal error, and return `Ok`.
macro_rules! nonfatal {
    ( $( $arg: expr ),+ ) => ({
        report!( $( $arg ),+ );
        Ok(())
    });
}

mod cssa;
mod flags;
mod liveness;
mod locations;

/// A verifier error.
#[derive(Fail, Debug, PartialEq, Eq)]
pub struct VerifierError {
    /// The entity causing the verifier error.
    pub location: AnyEntity,
    /// The error message.
    pub message: String,
}

impl Display for VerifierError {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        write!(f, "{}: {}", self.location, self.message)
    }
}

/// Result of a step in the verification process.
///
/// Functions that return `VerifierStepResult<()>` should also take a
/// mutable reference to `VerifierErrors` as argument in order to report
/// errors.
///
/// Here, `Ok` represents a step that **did not lead to a fatal error**,
/// meaning that the verification process may continue. However, other (non-fatal)
/// errors might have been reported through the previously mentioned `VerifierErrors`
/// argument.
pub type VerifierStepResult<T> = Result<T, ()>;

/// Result of a verification operation.
///
/// Unlike `VerifierStepResult<()>` which may be `Ok` while still having reported
/// errors, this type always returns `Err` if an error (fatal or not) was reported.
pub type VerifierResult<T> = Result<T, VerifierErrors>;

/// List of verifier errors.
#[derive(Fail, Debug, Default, PartialEq, Eq)]
pub struct VerifierErrors(pub Vec<VerifierError>);

impl VerifierErrors {
    /// Return a new `VerifierErrors` struct.
    #[inline]
    pub fn new() -> Self {
        VerifierErrors(Vec::new())
    }

    /// Return whether no errors were reported.
    #[inline]
    pub fn is_empty(&self) -> bool {
        self.0.is_empty()
    }

    /// Return whether one or more errors were reported.
    #[inline]
    pub fn has_error(&self) -> bool {
        !self.0.is_empty()
    }

    /// Return a `VerifierStepResult` that is fatal if at least one error was reported,
    /// and non-fatal otherwise.
    #[inline]
    pub fn as_result(&self) -> VerifierStepResult<()> {
        if self.is_empty() {
            Ok(())
        } else {
            Err(())
        }
    }
}

impl From<Vec<VerifierError>> for VerifierErrors {
    fn from(v: Vec<VerifierError>) -> Self {
        VerifierErrors(v)
    }
}

impl Into<Vec<VerifierError>> for VerifierErrors {
    fn into(self) -> Vec<VerifierError> {
        self.0
    }
}

impl Into<VerifierResult<()>> for VerifierErrors {
    fn into(self) -> VerifierResult<()> {
        if self.is_empty() {
            Ok(())
        } else {
            Err(self)
        }
    }
}

impl Display for VerifierErrors {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        for err in &self.0 {
            writeln!(f, "- {}", err)?;
        }
        Ok(())
    }
}

/// Verify `func`.
pub fn verify_function<'a, FOI: Into<FlagsOrIsa<'a>>>(
    func: &Function,
    fisa: FOI,
) -> VerifierResult<()> {
    let _tt = timing::verifier();
    let mut errors = VerifierErrors::default();
    let verifier = Verifier::new(func, fisa.into());
    let result = verifier.run(&mut errors);
    if errors.is_empty() {
        result.unwrap();
        Ok(())
    } else {
        Err(errors)
    }
}

/// Verify `func` after checking the integrity of associated context data structures `cfg` and
/// `domtree`.
pub fn verify_context<'a, FOI: Into<FlagsOrIsa<'a>>>(
    func: &Function,
    cfg: &ControlFlowGraph,
    domtree: &DominatorTree,
    fisa: FOI,
    errors: &mut VerifierErrors,
) -> VerifierStepResult<()> {
    let _tt = timing::verifier();
    let verifier = Verifier::new(func, fisa.into());
    if cfg.is_valid() {
        verifier.cfg_integrity(cfg, errors)?;
    }
    if domtree.is_valid() {
        verifier.domtree_integrity(domtree, errors)?;
    }
    verifier.run(errors)
}

struct Verifier<'a> {
    func: &'a Function,
    expected_cfg: ControlFlowGraph,
    expected_domtree: DominatorTree,
    isa: Option<&'a TargetIsa>,
}

impl<'a> Verifier<'a> {
    pub fn new(func: &'a Function, fisa: FlagsOrIsa<'a>) -> Self {
        let expected_cfg = ControlFlowGraph::with_function(func);
        let expected_domtree = DominatorTree::with_function(func, &expected_cfg);
        Self {
            func,
            expected_cfg,
            expected_domtree,
            isa: fisa.isa,
        }
    }

    // Check for:
    //  - cycles in the global value declarations.
    //  - use of 'vmctx' when no special parameter declares it.
    fn verify_global_values(&self, errors: &mut VerifierErrors) -> VerifierStepResult<()> {
        let mut cycle_seen = false;
        let mut seen = SparseSet::new();

        'gvs: for gv in self.func.global_values.keys() {
            seen.clear();
            seen.insert(gv);

            let mut cur = gv;
            loop {
                match self.func.global_values[cur] {
                    ir::GlobalValueData::Load { base, .. }
                    | ir::GlobalValueData::IAddImm { base, .. } => {
                        if seen.insert(base).is_some() {
                            if !cycle_seen {
                                report!(
                                    errors,
                                    gv,
                                    "global value cycle: {}",
                                    DisplayList(seen.as_slice())
                                );
                                // ensures we don't report the cycle multiple times
                                cycle_seen = true;
                            }
                            continue 'gvs;
                        }

                        cur = base;
                    }
                    _ => break,
                }
            }

            match self.func.global_values[gv] {
                ir::GlobalValueData::VMContext { .. } => {
                    if self
                        .func
                        .special_param(ir::ArgumentPurpose::VMContext)
                        .is_none()
                    {
                        report!(errors, gv, "undeclared vmctx reference {}", gv);
                    }
                }
                ir::GlobalValueData::IAddImm {
                    base, global_type, ..
                } => {
                    if !global_type.is_int() {
                        report!(
                            errors,
                            gv,
                            "iadd_imm global value with non-int type {}",
                            global_type
                        );
                    } else if let Some(isa) = self.isa {
                        let base_type = self.func.global_values[base].global_type(isa);
                        if global_type != base_type {
                            report!(
                                errors,
                                gv,
                                "iadd_imm type {} differs from operand type {}",
                                global_type,
                                base_type
                            );
                        }
                    }
                }
                ir::GlobalValueData::Load { base, .. } => {
                    if let Some(isa) = self.isa {
                        let base_type = self.func.global_values[base].global_type(isa);
                        let pointer_type = isa.pointer_type();
                        if base_type != pointer_type {
                            report!(
                                errors,
                                gv,
                                "base {} has type {}, which is not the pointer type {}",
                                base,
                                base_type,
                                pointer_type
                            );
                        }
                    }
                }
                _ => {}
            }
        }

        // Invalid global values shouldn't stop us from verifying the rest of the function
        Ok(())
    }

    fn verify_heaps(&self, errors: &mut VerifierErrors) -> VerifierStepResult<()> {
        if let Some(isa) = self.isa {
            for (heap, heap_data) in &self.func.heaps {
                let base = heap_data.base;
                if !self.func.global_values.is_valid(base) {
                    return nonfatal!(errors, heap, "invalid base global value {}", base);
                }

                let pointer_type = isa.pointer_type();
                let base_type = self.func.global_values[base].global_type(isa);
                if base_type != pointer_type {
                    report!(
                        errors,
                        heap,
                        "heap base has type {}, which is not the pointer type {}",
                        base_type,
                        pointer_type
                    );
                }

                if let ir::HeapStyle::Dynamic { bound_gv, .. } = heap_data.style {
                    if !self.func.global_values.is_valid(bound_gv) {
                        return nonfatal!(errors, heap, "invalid bound global value {}", bound_gv);
                    }

                    let index_type = heap_data.index_type;
                    let bound_type = self.func.global_values[bound_gv].global_type(isa);
                    if index_type != bound_type {
                        report!(
                            errors,
                            heap,
                            "heap index type {} differs from the type of its bound, {}",
                            index_type,
                            bound_type
                        );
                    }
                }
            }
        }

        Ok(())
    }

    fn verify_tables(&self, errors: &mut VerifierErrors) -> VerifierStepResult<()> {
        if let Some(isa) = self.isa {
            for (table, table_data) in &self.func.tables {
                let base = table_data.base_gv;
                if !self.func.global_values.is_valid(base) {
                    return nonfatal!(errors, table, "invalid base global value {}", base);
                }

                let pointer_type = isa.pointer_type();
                let base_type = self.func.global_values[base].global_type(isa);
                if base_type != pointer_type {
                    report!(
                        errors,
                        table,
                        "table base has type {}, which is not the pointer type {}",
                        base_type,
                        pointer_type
                    );
                }

                let bound_gv = table_data.bound_gv;
                if !self.func.global_values.is_valid(bound_gv) {
                    return nonfatal!(errors, table, "invalid bound global value {}", bound_gv);
                }

                let index_type = table_data.index_type;
                let bound_type = self.func.global_values[bound_gv].global_type(isa);
                if index_type != bound_type {
                    report!(
                        errors,
                        table,
                        "table index type {} differs from the type of its bound, {}",
                        index_type,
                        bound_type
                    );
                }
            }
        }

        Ok(())
    }

    fn ebb_integrity(
        &self,
        ebb: Ebb,
        inst: Inst,
        errors: &mut VerifierErrors,
    ) -> VerifierStepResult<()> {
        let is_terminator = self.func.dfg[inst].opcode().is_terminator();
        let is_last_inst = self.func.layout.last_inst(ebb) == Some(inst);

        if is_terminator && !is_last_inst {
            // Terminating instructions only occur at the end of blocks.
            return fatal!(
                errors,
                inst,
                "a terminator instruction was encountered before the end of {}",
                ebb
            );
        }
        if is_last_inst && !is_terminator {
            return fatal!(
                errors,
                ebb,
                "block does not end in a terminator instruction"
            );
        }

        // Instructions belong to the correct ebb.
        let inst_ebb = self.func.layout.inst_ebb(inst);
        if inst_ebb != Some(ebb) {
            return fatal!(errors, inst, "should belong to {} not {:?}", ebb, inst_ebb);
        }

        // Parameters belong to the correct ebb.
        for &arg in self.func.dfg.ebb_params(ebb) {
            match self.func.dfg.value_def(arg) {
                ValueDef::Param(arg_ebb, _) => {
                    if ebb != arg_ebb {
                        return fatal!(errors, arg, "does not belong to {}", ebb);
                    }
                }
                _ => {
                    return fatal!(errors, arg, "expected an argument, found a result");
                }
            }
        }

        Ok(())
    }

    fn instruction_integrity(
        &self,
        inst: Inst,
        errors: &mut VerifierErrors,
    ) -> VerifierStepResult<()> {
        let inst_data = &self.func.dfg[inst];
        let dfg = &self.func.dfg;

        // The instruction format matches the opcode
        if inst_data.opcode().format() != InstructionFormat::from(inst_data) {
            return fatal!(
                errors,
                inst,
                "instruction opcode doesn't match instruction format"
            );
        }

        let num_fixed_results = inst_data.opcode().constraints().num_fixed_results();
        // var_results is 0 if we aren't a call instruction
        let var_results = dfg
            .call_signature(inst)
            .map_or(0, |sig| dfg.signatures[sig].returns.len());
        let total_results = num_fixed_results + var_results;

        // All result values for multi-valued instructions are created
        let got_results = dfg.inst_results(inst).len();
        if got_results != total_results {
            return fatal!(
                errors,
                inst,
                "expected {} result values, found {}",
                total_results,
                got_results
            );
        }

        self.verify_entity_references(inst, errors)
    }

    fn verify_entity_references(
        &self,
        inst: Inst,
        errors: &mut VerifierErrors,
    ) -> VerifierStepResult<()> {
        use ir::instructions::InstructionData::*;

        for &arg in self.func.dfg.inst_args(inst) {
            self.verify_inst_arg(inst, arg, errors)?;

            // All used values must be attached to something.
            let original = self.func.dfg.resolve_aliases(arg);
            if !self.func.dfg.value_is_attached(original) {
                report!(
                    errors,
                    inst,
                    "argument {} -> {} is not attached",
                    arg,
                    original
                );
            }
        }

        for &res in self.func.dfg.inst_results(inst) {
            self.verify_inst_result(inst, res, errors).is_ok();
        }

        match self.func.dfg[inst] {
            MultiAry { ref args, .. } => {
                self.verify_value_list(inst, args, errors)?;
            }
            Jump {
                destination,
                ref args,
                ..
            }
            | Branch {
                destination,
                ref args,
                ..
            }
            | BranchInt {
                destination,
                ref args,
                ..
            }
            | BranchFloat {
                destination,
                ref args,
                ..
            }
            | BranchIcmp {
                destination,
                ref args,
                ..
            } => {
                self.verify_ebb(inst, destination, errors)?;
                self.verify_value_list(inst, args, errors)?;
            }
            BranchTable { table, .. }
            | BranchTableBase { table, .. }
            | BranchTableEntry { table, .. }
            | IndirectJump { table, .. } => {
                self.verify_jump_table(inst, table, errors)?;
            }
            Call {
                func_ref, ref args, ..
            } => {
                self.verify_func_ref(inst, func_ref, errors)?;
                self.verify_value_list(inst, args, errors)?;
            }
            CallIndirect {
                sig_ref, ref args, ..
            } => {
                self.verify_sig_ref(inst, sig_ref, errors)?;
                self.verify_value_list(inst, args, errors)?;
            }
            FuncAddr { func_ref, .. } => {
                self.verify_func_ref(inst, func_ref, errors)?;
            }
            StackLoad { stack_slot, .. } | StackStore { stack_slot, .. } => {
                self.verify_stack_slot(inst, stack_slot, errors)?;
            }
            UnaryGlobalValue { global_value, .. } => {
                self.verify_global_value(inst, global_value, errors)?;
            }
            HeapAddr { heap, .. } => {
                self.verify_heap(inst, heap, errors)?;
            }
            TableAddr { table, .. } => {
                self.verify_table(inst, table, errors)?;
            }
            RegSpill { dst, .. } => {
                self.verify_stack_slot(inst, dst, errors)?;
            }
            RegFill { src, .. } => {
                self.verify_stack_slot(inst, src, errors)?;
            }
            LoadComplex { ref args, .. } => {
                self.verify_value_list(inst, args, errors)?;
            }
            StoreComplex { ref args, .. } => {
                self.verify_value_list(inst, args, errors)?;
            }

            // Exhaustive list so we can't forget to add new formats
            Unary { .. }
            | UnaryImm { .. }
            | UnaryIeee32 { .. }
            | UnaryIeee64 { .. }
            | UnaryBool { .. }
            | Binary { .. }
            | BinaryImm { .. }
            | Ternary { .. }
            | InsertLane { .. }
            | ExtractLane { .. }
            | IntCompare { .. }
            | IntCompareImm { .. }
            | IntCond { .. }
            | FloatCompare { .. }
            | FloatCond { .. }
            | IntSelect { .. }
            | Load { .. }
            | Store { .. }
            | RegMove { .. }
            | CopySpecial { .. }
            | Trap { .. }
            | CondTrap { .. }
            | IntCondTrap { .. }
            | FloatCondTrap { .. }
            | NullAry { .. } => {}
        }

        Ok(())
    }

    fn verify_ebb(
        &self,
        inst: Inst,
        e: Ebb,
        errors: &mut VerifierErrors,
    ) -> VerifierStepResult<()> {
        if !self.func.dfg.ebb_is_valid(e) || !self.func.layout.is_ebb_inserted(e) {
            return fatal!(errors, inst, "invalid ebb reference {}", e);
        }
        if let Some(entry_block) = self.func.layout.entry_block() {
            if e == entry_block {
                return fatal!(errors, inst, "invalid reference to entry ebb {}", e);
            }
        }
        Ok(())
    }

    fn verify_sig_ref(
        &self,
        inst: Inst,
        s: SigRef,
        errors: &mut VerifierErrors,
    ) -> VerifierStepResult<()> {
        if !self.func.dfg.signatures.is_valid(s) {
            fatal!(errors, inst, "invalid signature reference {}", s)
        } else {
            Ok(())
        }
    }

    fn verify_func_ref(
        &self,
        inst: Inst,
        f: FuncRef,
        errors: &mut VerifierErrors,
    ) -> VerifierStepResult<()> {
        if !self.func.dfg.ext_funcs.is_valid(f) {
            nonfatal!(errors, inst, "invalid function reference {}", f)
        } else {
            Ok(())
        }
    }

    fn verify_stack_slot(
        &self,
        inst: Inst,
        ss: StackSlot,
        errors: &mut VerifierErrors,
    ) -> VerifierStepResult<()> {
        if !self.func.stack_slots.is_valid(ss) {
            nonfatal!(errors, inst, "invalid stack slot {}", ss)
        } else {
            Ok(())
        }
    }

    fn verify_global_value(
        &self,
        inst: Inst,
        gv: GlobalValue,
        errors: &mut VerifierErrors,
    ) -> VerifierStepResult<()> {
        if !self.func.global_values.is_valid(gv) {
            nonfatal!(errors, inst, "invalid global value {}", gv)
        } else {
            Ok(())
        }
    }

    fn verify_heap(
        &self,
        inst: Inst,
        heap: ir::Heap,
        errors: &mut VerifierErrors,
    ) -> VerifierStepResult<()> {
        if !self.func.heaps.is_valid(heap) {
            nonfatal!(errors, inst, "invalid heap {}", heap)
        } else {
            Ok(())
        }
    }

    fn verify_table(
        &self,
        inst: Inst,
        table: ir::Table,
        errors: &mut VerifierErrors,
    ) -> VerifierStepResult<()> {
        if !self.func.tables.is_valid(table) {
            nonfatal!(errors, inst, "invalid table {}", table)
        } else {
            Ok(())
        }
    }

    fn verify_value_list(
        &self,
        inst: Inst,
        l: &ValueList,
        errors: &mut VerifierErrors,
    ) -> VerifierStepResult<()> {
        if !l.is_valid(&self.func.dfg.value_lists) {
            nonfatal!(errors, inst, "invalid value list reference {:?}", l)
        } else {
            Ok(())
        }
    }

    fn verify_jump_table(
        &self,
        inst: Inst,
        j: JumpTable,
        errors: &mut VerifierErrors,
    ) -> VerifierStepResult<()> {
        if !self.func.jump_tables.is_valid(j) {
            nonfatal!(errors, inst, "invalid jump table reference {}", j)
        } else {
            Ok(())
        }
    }

    fn verify_value(
        &self,
        loc_inst: Inst,
        v: Value,
        errors: &mut VerifierErrors,
    ) -> VerifierStepResult<()> {
        let dfg = &self.func.dfg;
        if !dfg.value_is_valid(v) {
            nonfatal!(errors, loc_inst, "invalid value reference {}", v)
        } else {
            Ok(())
        }
    }

    fn verify_inst_arg(
        &self,
        loc_inst: Inst,
        v: Value,
        errors: &mut VerifierErrors,
    ) -> VerifierStepResult<()> {
        self.verify_value(loc_inst, v, errors)?;

        let dfg = &self.func.dfg;
        let loc_ebb = self.func.layout.pp_ebb(loc_inst);
        let is_reachable = self.expected_domtree.is_reachable(loc_ebb);

        // SSA form
        match dfg.value_def(v) {
            ValueDef::Result(def_inst, _) => {
                // Value is defined by an instruction that exists.
                if !dfg.inst_is_valid(def_inst) {
                    return fatal!(
                        errors,
                        loc_inst,
                        "{} is defined by invalid instruction {}",
                        v,
                        def_inst
                    );
                }
                // Defining instruction is inserted in an EBB.
                if self.func.layout.inst_ebb(def_inst) == None {
                    return fatal!(
                        errors,
                        loc_inst,
                        "{} is defined by {} which has no EBB",
                        v,
                        def_inst
                    );
                }
                // Defining instruction dominates the instruction that uses the value.
                if is_reachable {
                    if !self
                        .expected_domtree
                        .dominates(def_inst, loc_inst, &self.func.layout)
                    {
                        return fatal!(
                            errors,
                            loc_inst,
                            "uses value {} from non-dominating {}",
                            v,
                            def_inst
                        );
                    }
                    if def_inst == loc_inst {
                        return fatal!(errors, loc_inst, "uses value {} from itself", v);
                    }
                }
            }
            ValueDef::Param(ebb, _) => {
                // Value is defined by an existing EBB.
                if !dfg.ebb_is_valid(ebb) {
                    return fatal!(errors, loc_inst, "{} is defined by invalid EBB {}", v, ebb);
                }
                // Defining EBB is inserted in the layout
                if !self.func.layout.is_ebb_inserted(ebb) {
                    return fatal!(
                        errors,
                        loc_inst,
                        "{} is defined by {} which is not in the layout",
                        v,
                        ebb
                    );
                }
                // The defining EBB dominates the instruction using this value.
                if is_reachable && !self.expected_domtree.dominates(
                    ebb,
                    loc_inst,
                    &self.func.layout,
                ) {
                    return fatal!(
                        errors,
                        loc_inst,
                        "uses value arg from non-dominating {}",
                        ebb
                    );
                }
            }
        }
        Ok(())
    }

    fn verify_inst_result(
        &self,
        loc_inst: Inst,
        v: Value,
        errors: &mut VerifierErrors,
    ) -> VerifierStepResult<()> {
        self.verify_value(loc_inst, v, errors)?;

        match self.func.dfg.value_def(v) {
            ValueDef::Result(def_inst, _) => {
                if def_inst != loc_inst {
                    fatal!(
                        errors,
                        loc_inst,
                        "instruction result {} is not defined by the instruction",
                        v
                    )
                } else {
                    Ok(())
                }
            }
            ValueDef::Param(_, _) => fatal!(
                errors,
                loc_inst,
                "instruction result {} is not defined by the instruction",
                v
            ),
        }
    }

    fn domtree_integrity(
        &self,
        domtree: &DominatorTree,
        errors: &mut VerifierErrors,
    ) -> VerifierStepResult<()> {
        // We consider two `DominatorTree`s to be equal if they return the same immediate
        // dominator for each EBB. Therefore the current domtree is valid if it matches the freshly
        // computed one.
        for ebb in self.func.layout.ebbs() {
            let expected = self.expected_domtree.idom(ebb);
            let got = domtree.idom(ebb);
            if got != expected {
                return fatal!(
                    errors,
                    ebb,
                    "invalid domtree, expected idom({}) = {:?}, got {:?}",
                    ebb,
                    expected,
                    got
                );
            }
        }
        // We also verify if the postorder defined by `DominatorTree` is sane
        if domtree.cfg_postorder().len() != self.expected_domtree.cfg_postorder().len() {
            return fatal!(
                errors,
                AnyEntity::Function,
                "incorrect number of Ebbs in postorder traversal"
            );
        }
        for (index, (&test_ebb, &true_ebb)) in domtree
            .cfg_postorder()
            .iter()
            .zip(self.expected_domtree.cfg_postorder().iter())
            .enumerate()
        {
            if test_ebb != true_ebb {
                return fatal!(
                    errors,
                    test_ebb,
                    "invalid domtree, postorder ebb number {} should be {}, got {}",
                    index,
                    true_ebb,
                    test_ebb
                );
            }
        }
        // We verify rpo_cmp on pairs of adjacent ebbs in the postorder
        for (&prev_ebb, &next_ebb) in domtree.cfg_postorder().iter().adjacent_pairs() {
            if self
                .expected_domtree
                .rpo_cmp(prev_ebb, next_ebb, &self.func.layout)
                != Ordering::Greater
            {
                return fatal!(
                    errors,
                    next_ebb,
                    "invalid domtree, rpo_cmp does not says {} is greater than {}",
                    prev_ebb,
                    next_ebb
                );
            }
        }
        Ok(())
    }

    fn typecheck_entry_block_params(&self, errors: &mut VerifierErrors) -> VerifierStepResult<()> {
        if let Some(ebb) = self.func.layout.entry_block() {
            let expected_types = &self.func.signature.params;
            let ebb_param_count = self.func.dfg.num_ebb_params(ebb);

            if ebb_param_count != expected_types.len() {
                return fatal!(
                    errors,
                    ebb,
                    "entry block parameters ({}) must match function signature ({})",
                    ebb_param_count,
                    expected_types.len()
                );
            }

            for (i, &arg) in self.func.dfg.ebb_params(ebb).iter().enumerate() {
                let arg_type = self.func.dfg.value_type(arg);
                if arg_type != expected_types[i].value_type {
                    report!(
                        errors,
                        ebb,
                        "entry block parameter {} expected to have type {}, got {}",
                        i,
                        expected_types[i],
                        arg_type
                    );
                }
            }
        }

        errors.as_result()
    }

    fn typecheck(&self, inst: Inst, errors: &mut VerifierErrors) -> VerifierStepResult<()> {
        let inst_data = &self.func.dfg[inst];
        let constraints = inst_data.opcode().constraints();

        let ctrl_type = if let Some(value_typeset) = constraints.ctrl_typeset() {
            // For polymorphic opcodes, determine the controlling type variable first.
            let ctrl_type = self.func.dfg.ctrl_typevar(inst);

            if !value_typeset.contains(ctrl_type) {
                report!(
                    errors,
                    inst,
                    "has an invalid controlling type {}",
                    ctrl_type
                );
            }

            ctrl_type
        } else {
            // Non-polymorphic instructions don't check the controlling type variable, so `Option`
            // is unnecessary and we can just make it `INVALID`.
            types::INVALID
        };

        // Typechecking instructions is never fatal
        self.typecheck_results(inst, ctrl_type, errors).is_ok();
        self.typecheck_fixed_args(inst, ctrl_type, errors).is_ok();
        self.typecheck_variable_args(inst, errors).is_ok();
        self.typecheck_return(inst, errors).is_ok();
        self.typecheck_special(inst, ctrl_type, errors).is_ok();

        Ok(())
    }

    fn typecheck_results(
        &self,
        inst: Inst,
        ctrl_type: Type,
        errors: &mut VerifierErrors,
    ) -> VerifierStepResult<()> {
        let mut i = 0;
        for &result in self.func.dfg.inst_results(inst) {
            let result_type = self.func.dfg.value_type(result);
            let expected_type = self.func.dfg.compute_result_type(inst, i, ctrl_type);
            if let Some(expected_type) = expected_type {
                if result_type != expected_type {
                    report!(
                        errors,
                        inst,
                        "expected result {} ({}) to have type {}, found {}",
                        i,
                        result,
                        expected_type,
                        result_type
                    );
                }
            } else {
                return nonfatal!(errors, inst, "has more result values than expected");
            }
            i += 1;
        }

        // There aren't any more result types left.
        if self.func.dfg.compute_result_type(inst, i, ctrl_type) != None {
            return nonfatal!(errors, inst, "has fewer result values than expected");
        }
        Ok(())
    }

    fn typecheck_fixed_args(
        &self,
        inst: Inst,
        ctrl_type: Type,
        errors: &mut VerifierErrors,
    ) -> VerifierStepResult<()> {
        let constraints = self.func.dfg[inst].opcode().constraints();

        for (i, &arg) in self.func.dfg.inst_fixed_args(inst).iter().enumerate() {
            let arg_type = self.func.dfg.value_type(arg);
            match constraints.value_argument_constraint(i, ctrl_type) {
                ResolvedConstraint::Bound(expected_type) => {
                    if arg_type != expected_type {
                        report!(
                            errors,
                            inst,
                            "arg {} ({}) has type {}, expected {}",
                            i,
                            arg,
                            arg_type,
                            expected_type
                        );
                    }
                }
                ResolvedConstraint::Free(type_set) => {
                    if !type_set.contains(arg_type) {
                        report!(
                            errors,
                            inst,
                            "arg {} ({}) with type {} failed to satisfy type set {:?}",
                            i,
                            arg,
                            arg_type,
                            type_set
                        );
                    }
                }
            }
        }
        Ok(())
    }

    fn typecheck_variable_args(
        &self,
        inst: Inst,
        errors: &mut VerifierErrors,
    ) -> VerifierStepResult<()> {
        match self.func.dfg.analyze_branch(inst) {
            BranchInfo::SingleDest(ebb, _) => {
                let iter = self
                    .func
                    .dfg
                    .ebb_params(ebb)
                    .iter()
                    .map(|&v| self.func.dfg.value_type(v));
                self.typecheck_variable_args_iterator(inst, iter, errors)?;
            }
            BranchInfo::Table(table, ebb) => {
                if let Some(ebb) = ebb {
                    let arg_count = self.func.dfg.num_ebb_params(ebb);
                    if arg_count != 0 {
                        return nonfatal!(
                            errors,
                            inst,
                            "takes no arguments, but had target {} with {} arguments",
                            ebb,
                            arg_count
                        );
                    }
                }
                for ebb in self.func.jump_tables[table].iter() {
                    let arg_count = self.func.dfg.num_ebb_params(*ebb);
                    if arg_count != 0 {
                        return nonfatal!(
                            errors,
                            inst,
                            "takes no arguments, but had target {} with {} arguments",
                            ebb,
                            arg_count
                        );
                    }
                }
            }
            BranchInfo::NotABranch => {}
        }

        match self.func.dfg[inst].analyze_call(&self.func.dfg.value_lists) {
            CallInfo::Direct(func_ref, _) => {
                let sig_ref = self.func.dfg.ext_funcs[func_ref].signature;
                let arg_types = self.func.dfg.signatures[sig_ref]
                    .params
                    .iter()
                    .map(|a| a.value_type);
                self.typecheck_variable_args_iterator(inst, arg_types, errors)?;
                self.check_outgoing_args(inst, sig_ref, errors)?;
            }
            CallInfo::Indirect(sig_ref, _) => {
                let arg_types = self.func.dfg.signatures[sig_ref]
                    .params
                    .iter()
                    .map(|a| a.value_type);
                self.typecheck_variable_args_iterator(inst, arg_types, errors)?;
                self.check_outgoing_args(inst, sig_ref, errors)?;
            }
            CallInfo::NotACall => {}
        }
        Ok(())
    }

    fn typecheck_variable_args_iterator<I: Iterator<Item = Type>>(
        &self,
        inst: Inst,
        iter: I,
        errors: &mut VerifierErrors,
    ) -> VerifierStepResult<()> {
        let variable_args = self.func.dfg.inst_variable_args(inst);
        let mut i = 0;

        for expected_type in iter {
            if i >= variable_args.len() {
                // Result count mismatch handled below, we want the full argument count first though
                i += 1;
                continue;
            }
            let arg = variable_args[i];
            let arg_type = self.func.dfg.value_type(arg);
            if expected_type != arg_type {
                report!(
                    errors,
                    inst,
                    "arg {} ({}) has type {}, expected {}",
                    i,
                    variable_args[i],
                    arg_type,
                    expected_type
                );
            }
            i += 1;
        }
        if i != variable_args.len() {
            return nonfatal!(
                errors,
                inst,
                "mismatched argument count for `{}`: got {}, expected {}",
                self.func.dfg.display_inst(inst, None),
                variable_args.len(),
                i
            );
        }
        Ok(())
    }

    /// Check the locations assigned to outgoing call arguments.
    ///
    /// When a signature has been legalized, all values passed as outgoing arguments on the stack
    /// must be assigned to a matching `OutgoingArg` stack slot.
    fn check_outgoing_args(
        &self,
        inst: Inst,
        sig_ref: SigRef,
        errors: &mut VerifierErrors,
    ) -> VerifierStepResult<()> {
        let sig = &self.func.dfg.signatures[sig_ref];

        let args = self.func.dfg.inst_variable_args(inst);
        let expected_args = &sig.params[..];

        for (&arg, &abi) in args.iter().zip(expected_args) {
            // Value types have already been checked by `typecheck_variable_args_iterator()`.
            if let ArgumentLoc::Stack(offset) = abi.location {
                let arg_loc = self.func.locations[arg];
                if let ValueLoc::Stack(ss) = arg_loc {
                    // Argument value is assigned to a stack slot as expected.
                    self.verify_stack_slot(inst, ss, errors)?;
                    let slot = &self.func.stack_slots[ss];
                    if slot.kind != StackSlotKind::OutgoingArg {
                        return fatal!(
                            errors,
                            inst,
                            "Outgoing stack argument {} in wrong stack slot: {} = {}",
                            arg,
                            ss,
                            slot
                        );
                    }
                    if slot.offset != Some(offset) {
                        return fatal!(
                            errors,
                            inst,
                            "Outgoing stack argument {} should have offset {}: {} = {}",
                            arg,
                            offset,
                            ss,
                            slot
                        );
                    }
                    if slot.size != abi.value_type.bytes() {
                        return fatal!(
                            errors,
                            inst,
                            "Outgoing stack argument {} wrong size for {}: {} = {}",
                            arg,
                            abi.value_type,
                            ss,
                            slot
                        );
                    }
                } else {
                    let reginfo = self.isa.map(|i| i.register_info());
                    return fatal!(
                        errors,
                        inst,
                        "Outgoing stack argument {} in wrong location: {}",
                        arg,
                        arg_loc.display(reginfo.as_ref())
                    );
                }
            }
        }
        Ok(())
    }

    fn typecheck_return(&self, inst: Inst, errors: &mut VerifierErrors) -> VerifierStepResult<()> {
        if self.func.dfg[inst].opcode().is_return() {
            let args = self.func.dfg.inst_variable_args(inst);
            let expected_types = &self.func.signature.returns;
            if args.len() != expected_types.len() {
                return nonfatal!(
                    errors,
                    inst,
                    "arguments of return must match function signature"
                );
            }
            for (i, (&arg, &expected_type)) in args.iter().zip(expected_types).enumerate() {
                let arg_type = self.func.dfg.value_type(arg);
                if arg_type != expected_type.value_type {
                    report!(
                        errors,
                        inst,
                        "arg {} ({}) has type {}, must match function signature of {}",
                        i,
                        arg,
                        arg_type,
                        expected_type
                    );
                }
            }
        }
        Ok(())
    }

    // Check special-purpose type constraints that can't be expressed in the normal opcode
    // constraints.
    fn typecheck_special(
        &self,
        inst: Inst,
        ctrl_type: Type,
        errors: &mut VerifierErrors,
    ) -> VerifierStepResult<()> {
        match self.func.dfg[inst] {
            ir::InstructionData::Unary { opcode, arg } => {
                let arg_type = self.func.dfg.value_type(arg);
                match opcode {
                    Opcode::Bextend | Opcode::Uextend | Opcode::Sextend | Opcode::Fpromote => {
                        if arg_type.lane_count() != ctrl_type.lane_count() {
                            return nonfatal!(
                                errors,
                                inst,
                                "input {} and output {} must have same number of lanes",
                                arg_type,
                                ctrl_type
                            );
                        }
                        if arg_type.lane_bits() >= ctrl_type.lane_bits() {
                            return nonfatal!(
                                errors,
                                inst,
                                "input {} must be smaller than output {}",
                                arg_type,
                                ctrl_type
                            );
                        }
                    }
                    Opcode::Breduce | Opcode::Ireduce | Opcode::Fdemote => {
                        if arg_type.lane_count() != ctrl_type.lane_count() {
                            return nonfatal!(
                                errors,
                                inst,
                                "input {} and output {} must have same number of lanes",
                                arg_type,
                                ctrl_type
                            );
                        }
                        if arg_type.lane_bits() <= ctrl_type.lane_bits() {
                            return nonfatal!(
                                errors,
                                inst,
                                "input {} must be larger than output {}",
                                arg_type,
                                ctrl_type
                            );
                        }
                    }
                    _ => {}
                }
            }
            ir::InstructionData::HeapAddr { heap, arg, .. } => {
                let index_type = self.func.dfg.value_type(arg);
                let heap_index_type = self.func.heaps[heap].index_type;
                if index_type != heap_index_type {
                    return nonfatal!(
                        errors,
                        inst,
                        "index type {} differs from heap index type {}",
                        index_type,
                        heap_index_type
                    );
                }
            }
            ir::InstructionData::TableAddr { table, arg, .. } => {
                let index_type = self.func.dfg.value_type(arg);
                let table_index_type = self.func.tables[table].index_type;
                if index_type != table_index_type {
                    return nonfatal!(
                        errors,
                        inst,
                        "index type {} differs from table index type {}",
                        index_type,
                        table_index_type
                    );
                }
            }
            ir::InstructionData::UnaryGlobalValue { global_value, .. } => {
                if let Some(isa) = self.isa {
                    let inst_type = self.func.dfg.value_type(self.func.dfg.first_result(inst));
                    let global_type = self.func.global_values[global_value].global_type(isa);
                    if inst_type != global_type {
                        return nonfatal!(
                        errors,
                        inst,
                        "global_value instruction with type {} references global value with type {}",
                        inst_type,
                        global_type
                    );
                    }
                }
            }
            _ => {}
        }
        Ok(())
    }

    fn cfg_integrity(
        &self,
        cfg: &ControlFlowGraph,
        errors: &mut VerifierErrors,
    ) -> VerifierStepResult<()> {
        let mut expected_succs = BTreeSet::<Ebb>::new();
        let mut got_succs = BTreeSet::<Ebb>::new();
        let mut expected_preds = BTreeSet::<Inst>::new();
        let mut got_preds = BTreeSet::<Inst>::new();

        for ebb in self.func.layout.ebbs() {
            expected_succs.extend(self.expected_cfg.succ_iter(ebb));
            got_succs.extend(cfg.succ_iter(ebb));

            let missing_succs: Vec<Ebb> = expected_succs.difference(&got_succs).cloned().collect();
            if !missing_succs.is_empty() {
                report!(
                    errors,
                    ebb,
                    "cfg lacked the following successor(s) {:?}",
                    missing_succs
                );
                continue;
            }

            let excess_succs: Vec<Ebb> = got_succs.difference(&expected_succs).cloned().collect();
            if !excess_succs.is_empty() {
                report!(
                    errors,
                    ebb,
                    "cfg had unexpected successor(s) {:?}",
                    excess_succs
                );
                continue;
            }

            expected_preds.extend(
                self.expected_cfg
                    .pred_iter(ebb)
                    .map(|BasicBlock { inst, .. }| inst),
            );
            got_preds.extend(cfg.pred_iter(ebb).map(|BasicBlock { inst, .. }| inst));

            let missing_preds: Vec<Inst> = expected_preds.difference(&got_preds).cloned().collect();
            if !missing_preds.is_empty() {
                report!(
                    errors,
                    ebb,
                    "cfg lacked the following predecessor(s) {:?}",
                    missing_preds
                );
                continue;
            }

            let excess_preds: Vec<Inst> = got_preds.difference(&expected_preds).cloned().collect();
            if !excess_preds.is_empty() {
                report!(
                    errors,
                    ebb,
                    "cfg had unexpected predecessor(s) {:?}",
                    excess_preds
                );
                continue;
            }

            expected_succs.clear();
            got_succs.clear();
            expected_preds.clear();
            got_preds.clear();
        }
        errors.as_result()
    }

    /// If the verifier has been set up with an ISA, make sure that the recorded encoding for the
    /// instruction (if any) matches how the ISA would encode it.
    fn verify_encoding(&self, inst: Inst, errors: &mut VerifierErrors) -> VerifierStepResult<()> {
        // When the encodings table is empty, we don't require any instructions to be encoded.
        //
        // Once some instructions are encoded, we require all side-effecting instructions to have a
        // legal encoding.
        if self.func.encodings.is_empty() {
            return Ok(());
        }

        let isa = match self.isa {
            Some(isa) => isa,
            None => return Ok(()),
        };

        let encoding = self.func.encodings[inst];
        if encoding.is_legal() {
            if self.func.dfg[inst].opcode().is_ghost() {
                return nonfatal!(
                    errors,
                    inst,
                    "Ghost instruction has an encoding: {}",
                    isa.encoding_info().display(encoding)
                );
            }

            let mut encodings = isa
                .legal_encodings(
                    &self.func,
                    &self.func.dfg[inst],
                    self.func.dfg.ctrl_typevar(inst),
                ).peekable();

            if encodings.peek().is_none() {
                return nonfatal!(
                    errors,
                    inst,
                    "Instruction failed to re-encode {}",
                    isa.encoding_info().display(encoding)
                );
            }

            let has_valid_encoding = encodings.any(|possible_enc| encoding == possible_enc);

            if !has_valid_encoding {
                let mut possible_encodings = String::new();
                let mut multiple_encodings = false;

                for enc in isa.legal_encodings(
                    &self.func,
                    &self.func.dfg[inst],
                    self.func.dfg.ctrl_typevar(inst),
                ) {
                    if !possible_encodings.is_empty() {
                        possible_encodings.push_str(", ");
                        multiple_encodings = true;
                    }
                    possible_encodings
                        .write_fmt(format_args!("{}", isa.encoding_info().display(enc)))
                        .unwrap();
                }

                return nonfatal!(
                    errors,
                    inst,
                    "encoding {} should be {}{}",
                    isa.encoding_info().display(encoding),
                    if multiple_encodings { "one of: " } else { "" },
                    possible_encodings
                );
            }
            return Ok(());
        }

        // Instruction is not encoded, so it is a ghost instruction.
        // Instructions with side effects are not allowed to be ghost instructions.
        let opcode = self.func.dfg[inst].opcode();

        // The `fallthrough` and `fallthrough_return` instructions are marked as terminators and
        // branches, but they are not required to have an encoding.
        if opcode == Opcode::Fallthrough || opcode == Opcode::FallthroughReturn {
            return Ok(());
        }

        // Check if this opcode must be encoded.
        let mut needs_enc = None;
        if opcode.is_branch() {
            needs_enc = Some("Branch");
        } else if opcode.is_call() {
            needs_enc = Some("Call");
        } else if opcode.is_return() {
            needs_enc = Some("Return");
        } else if opcode.can_store() {
            needs_enc = Some("Store");
        } else if opcode.can_trap() {
            needs_enc = Some("Trapping instruction");
        } else if opcode.other_side_effects() {
            needs_enc = Some("Instruction with side effects");
        }

        if let Some(text) = needs_enc {
            // This instruction needs an encoding, so generate an error.
            // Provide the ISA default encoding as a hint.
            match self.func.encode(inst, isa) {
                Ok(enc) => {
                    return nonfatal!(
                        errors,
                        inst,
                        "{} must have an encoding (e.g., {})",
                        text,
                        isa.encoding_info().display(enc)
                    )
                }
                Err(_) => return nonfatal!(errors, inst, "{} must have an encoding", text),
            }
        }

        Ok(())
    }

    fn immediate_constraints(
        &self,
        inst: Inst,
        errors: &mut VerifierErrors,
    ) -> VerifierStepResult<()> {
        let inst_data = &self.func.dfg[inst];

        // If this is some sort of a store instruction, get the memflags, else, just return.
        let memflags = match *inst_data {
            ir::InstructionData::Store { flags, .. }
            | ir::InstructionData::StoreComplex { flags, .. } => flags,
            _ => return Ok(()),
        };

        if memflags.readonly() {
            fatal!(
                errors,
                inst,
                "A store instruction cannot have the `readonly` MemFlag"
            )
        } else {
            Ok(())
        }
    }

    pub fn run(&self, errors: &mut VerifierErrors) -> VerifierStepResult<()> {
        self.verify_global_values(errors)?;
        self.verify_heaps(errors)?;
        self.verify_tables(errors)?;
        self.typecheck_entry_block_params(errors)?;

        for ebb in self.func.layout.ebbs() {
            for inst in self.func.layout.ebb_insts(ebb) {
                self.ebb_integrity(ebb, inst, errors)?;
                self.instruction_integrity(inst, errors)?;
                self.typecheck(inst, errors)?;
                self.verify_encoding(inst, errors)?;
                self.immediate_constraints(inst, errors)?;
            }
        }

        verify_flags(self.func, &self.expected_cfg, self.isa, errors)?;

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::{Verifier, VerifierError, VerifierErrors};
    use entity::EntityList;
    use ir::instructions::{InstructionData, Opcode};
    use ir::Function;
    use settings;

    macro_rules! assert_err_with_msg {
        ($e:expr, $msg:expr) => {
            match $e.0.get(0) {
                None => panic!("Expected an error"),
                Some(&VerifierError { ref message, .. }) => {
                    if !message.contains($msg) {
                        #[cfg(feature = "std")]
                        panic!(format!(
                            "'{}' did not contain the substring '{}'",
                            message, $msg
                        ));
                        #[cfg(not(feature = "std"))]
                        panic!("error message did not contain the expected substring");
                    }
                }
            }
        };
    }

    #[test]
    fn empty() {
        let func = Function::new();
        let flags = &settings::Flags::new(settings::builder());
        let verifier = Verifier::new(&func, flags.into());
        let mut errors = VerifierErrors::default();

        assert_eq!(verifier.run(&mut errors), Ok(()));
        assert!(errors.0.is_empty());
    }

    #[test]
    fn bad_instruction_format() {
        let mut func = Function::new();
        let ebb0 = func.dfg.make_ebb();
        func.layout.append_ebb(ebb0);
        let nullary_with_bad_opcode = func.dfg.make_inst(InstructionData::UnaryImm {
            opcode: Opcode::F32const,
            imm: 0.into(),
        });
        func.layout.append_inst(nullary_with_bad_opcode, ebb0);
        func.layout.append_inst(
            func.dfg.make_inst(InstructionData::Jump {
                opcode: Opcode::Jump,
                destination: ebb0,
                args: EntityList::default(),
            }),
            ebb0,
        );
        let flags = &settings::Flags::new(settings::builder());
        let verifier = Verifier::new(&func, flags.into());
        let mut errors = VerifierErrors::default();

        let _ = verifier.run(&mut errors);

        assert_err_with_msg!(errors, "instruction format");
    }
}
