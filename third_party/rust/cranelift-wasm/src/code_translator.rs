//! This module contains the bulk of the interesting code performing the translation between
//! WebAssembly and Cranelift IR.
//!
//! The translation is done in one pass, opcode by opcode. Two main data structures are used during
//! code translations: the value stack and the control stack. The value stack mimics the execution
//! of the WebAssembly stack machine: each instruction result is pushed onto the stack and
//! instruction arguments are popped off the stack. Similarly, when encountering a control flow
//! block, it is pushed onto the control stack and popped off when encountering the corresponding
//! `End`.
//!
//! Another data structure, the translation state, records information concerning unreachable code
//! status and about if inserting a return at the end of the function is necessary.
//!
//! Some of the WebAssembly instructions need information about the environment for which they
//! are being translated:
//!
//! - the loads and stores need the memory base address;
//! - the `get_global` et `set_global` instructions depends on how the globals are implemented;
//! - `memory.size` and `memory.grow` are runtime functions;
//! - `call_indirect` has to translate the function index into the address of where this
//!    is;
//!
//! That is why `translate_function_body` takes an object having the `WasmRuntime` trait as
//! argument.
use super::{hash_map, HashMap};
use crate::environ::{FuncEnvironment, GlobalVariable, ReturnMode, WasmResult};
use crate::state::{ControlStackFrame, ElseData, FuncTranslationState, ModuleTranslationState};
use crate::translation_utils::{
    blocktype_params_results, ebb_with_params, f32_translation, f64_translation,
};
use crate::translation_utils::{FuncIndex, MemoryIndex, SignatureIndex, TableIndex};
use crate::wasm_unsupported;
use core::{i32, u32};
use cranelift_codegen::ir::condcodes::{FloatCC, IntCC};
use cranelift_codegen::ir::types::*;
use cranelift_codegen::ir::{
    self, ConstantData, InstBuilder, JumpTableData, MemFlags, Value, ValueLabel,
};
use cranelift_codegen::packed_option::ReservedValue;
use cranelift_frontend::{FunctionBuilder, Variable};
use wasmparser::{MemoryImmediate, Operator};

// Clippy warns about "flags: _" but its important to document that the flags field is ignored
#[cfg_attr(feature = "cargo-clippy", allow(clippy::unneeded_field_pattern))]
/// Translates wasm operators into Cranelift IR instructions. Returns `true` if it inserted
/// a return.
pub fn translate_operator<FE: FuncEnvironment + ?Sized>(
    module_translation_state: &ModuleTranslationState,
    op: &Operator,
    builder: &mut FunctionBuilder,
    state: &mut FuncTranslationState,
    environ: &mut FE,
) -> WasmResult<()> {
    if !state.reachable {
        translate_unreachable_operator(module_translation_state, &op, builder, state)?;
        return Ok(());
    }

    // This big match treats all Wasm code operators.
    match op {
        /********************************** Locals ****************************************
         *  `get_local` and `set_local` are treated as non-SSA variables and will completely
         *  disappear in the Cranelift Code
         ***********************************************************************************/
        Operator::GetLocal { local_index } => {
            let val = builder.use_var(Variable::with_u32(*local_index));
            state.push1(val);
            let label = ValueLabel::from_u32(*local_index);
            builder.set_val_label(val, label);
        }
        Operator::SetLocal { local_index } => {
            let val = state.pop1();
            builder.def_var(Variable::with_u32(*local_index), val);
            let label = ValueLabel::from_u32(*local_index);
            builder.set_val_label(val, label);
        }
        Operator::TeeLocal { local_index } => {
            let val = state.peek1();
            builder.def_var(Variable::with_u32(*local_index), val);
            let label = ValueLabel::from_u32(*local_index);
            builder.set_val_label(val, label);
        }
        /********************************** Globals ****************************************
         *  `get_global` and `set_global` are handled by the environment.
         ***********************************************************************************/
        Operator::GetGlobal { global_index } => {
            let val = match state.get_global(builder.func, *global_index, environ)? {
                GlobalVariable::Const(val) => val,
                GlobalVariable::Memory { gv, offset, ty } => {
                    let addr = builder.ins().global_value(environ.pointer_type(), gv);
                    let flags = ir::MemFlags::trusted();
                    builder.ins().load(ty, flags, addr, offset)
                }
            };
            state.push1(val);
        }
        Operator::SetGlobal { global_index } => {
            match state.get_global(builder.func, *global_index, environ)? {
                GlobalVariable::Const(_) => panic!("global #{} is a constant", *global_index),
                GlobalVariable::Memory { gv, offset, ty } => {
                    let addr = builder.ins().global_value(environ.pointer_type(), gv);
                    let flags = ir::MemFlags::trusted();
                    let val = state.pop1();
                    debug_assert_eq!(ty, builder.func.dfg.value_type(val));
                    builder.ins().store(flags, val, addr, offset);
                }
            }
        }
        /********************************* Stack misc ***************************************
         *  `drop`, `nop`, `unreachable` and `select`.
         ***********************************************************************************/
        Operator::Drop => {
            state.pop1();
        }
        Operator::Select => {
            let (arg1, arg2, cond) = state.pop3();
            state.push1(builder.ins().select(cond, arg1, arg2));
        }
        Operator::Nop => {
            // We do nothing
        }
        Operator::Unreachable => {
            builder.ins().trap(ir::TrapCode::UnreachableCodeReached);
            state.reachable = false;
        }
        /***************************** Control flow blocks **********************************
         *  When starting a control flow block, we create a new `Ebb` that will hold the code
         *  after the block, and we push a frame on the control stack. Depending on the type
         *  of block, we create a new `Ebb` for the body of the block with an associated
         *  jump instruction.
         *
         *  The `End` instruction pops the last control frame from the control stack, seals
         *  the destination block (since `br` instructions targeting it only appear inside the
         *  block and have already been translated) and modify the value stack to use the
         *  possible `Ebb`'s arguments values.
         ***********************************************************************************/
        Operator::Block { ty } => {
            let (params, results) = blocktype_params_results(module_translation_state, *ty)?;
            let next = ebb_with_params(builder, results)?;
            state.push_block(next, params.len(), results.len());
        }
        Operator::Loop { ty } => {
            let (params, results) = blocktype_params_results(module_translation_state, *ty)?;
            let loop_body = ebb_with_params(builder, params)?;
            let next = ebb_with_params(builder, results)?;
            builder.ins().jump(loop_body, state.peekn(params.len()));
            state.push_loop(loop_body, next, params.len(), results.len());

            // Pop the initial `Ebb` actuals and replace them with the `Ebb`'s
            // params since control flow joins at the top of the loop.
            state.popn(params.len());
            state.stack.extend_from_slice(builder.ebb_params(loop_body));

            builder.switch_to_block(loop_body);
            environ.translate_loop_header(builder.cursor())?;
        }
        Operator::If { ty } => {
            let val = state.pop1();

            let (params, results) = blocktype_params_results(module_translation_state, *ty)?;
            let (destination, else_data) = if params == results {
                // It is possible there is no `else` block, so we will only
                // allocate an ebb for it if/when we find the `else`. For now,
                // we if the condition isn't true, then we jump directly to the
                // destination ebb following the whole `if...end`. If we do end
                // up discovering an `else`, then we will allocate an ebb for it
                // and go back and patch the jump.
                let destination = ebb_with_params(builder, results)?;
                let branch_inst = builder
                    .ins()
                    .brz(val, destination, state.peekn(params.len()));
                (destination, ElseData::NoElse { branch_inst })
            } else {
                // The `if` type signature is not valid without an `else` block,
                // so we eagerly allocate the `else` block here.
                let destination = ebb_with_params(builder, results)?;
                let else_block = ebb_with_params(builder, params)?;
                builder
                    .ins()
                    .brz(val, else_block, state.peekn(params.len()));
                builder.seal_block(else_block);
                (destination, ElseData::WithElse { else_block })
            };

            #[cfg(feature = "basic-blocks")]
            {
                let next_ebb = builder.create_ebb();
                builder.ins().jump(next_ebb, &[]);
                builder.seal_block(next_ebb); // Only predecessor is the current block.
                builder.switch_to_block(next_ebb);
            }

            // Here we append an argument to an Ebb targeted by an argumentless jump instruction
            // But in fact there are two cases:
            // - either the If does not have a Else clause, in that case ty = EmptyBlock
            //   and we add nothing;
            // - either the If have an Else clause, in that case the destination of this jump
            //   instruction will be changed later when we translate the Else operator.
            state.push_if(destination, else_data, params.len(), results.len(), *ty);
        }
        Operator::Else => {
            let i = state.control_stack.len() - 1;
            match state.control_stack[i] {
                ControlStackFrame::If {
                    ref else_data,
                    head_is_reachable,
                    ref mut consequent_ends_reachable,
                    num_return_values,
                    blocktype,
                    destination,
                    ..
                } => {
                    // We finished the consequent, so record its final
                    // reachability state.
                    debug_assert!(consequent_ends_reachable.is_none());
                    *consequent_ends_reachable = Some(state.reachable);

                    if head_is_reachable {
                        // We have a branch from the head of the `if` to the `else`.
                        state.reachable = true;

                        // Ensure we have an ebb for the `else` block (it may have
                        // already been pre-allocated, see `ElseData` for details).
                        let else_ebb = match *else_data {
                            ElseData::NoElse { branch_inst } => {
                                let (params, _results) =
                                    blocktype_params_results(module_translation_state, blocktype)?;
                                debug_assert_eq!(params.len(), num_return_values);
                                let else_ebb = ebb_with_params(builder, params)?;
                                builder.ins().jump(destination, state.peekn(params.len()));
                                state.popn(params.len());

                                builder.change_jump_destination(branch_inst, else_ebb);
                                builder.seal_block(else_ebb);
                                else_ebb
                            }
                            ElseData::WithElse { else_block } => {
                                builder
                                    .ins()
                                    .jump(destination, state.peekn(num_return_values));
                                state.popn(num_return_values);
                                else_block
                            }
                        };

                        // You might be expecting that we push the parameters for this
                        // `else` block here, something like this:
                        //
                        //     state.pushn(&control_stack_frame.params);
                        //
                        // We don't do that because they are already on the top of the stack
                        // for us: we pushed the parameters twice when we saw the initial
                        // `if` so that we wouldn't have to save the parameters in the
                        // `ControlStackFrame` as another `Vec` allocation.

                        builder.switch_to_block(else_ebb);

                        // We don't bother updating the control frame's `ElseData`
                        // to `WithElse` because nothing else will read it.
                    }
                }
                _ => unreachable!(),
            }
        }
        Operator::End => {
            let frame = state.control_stack.pop().unwrap();

            if !builder.is_unreachable() || !builder.is_pristine() {
                let return_count = frame.num_return_values();
                builder
                    .ins()
                    .jump(frame.following_code(), state.peekn(return_count));
                // You might expect that if we just finished an `if` block that
                // didn't have a corresponding `else` block, then we would clean
                // up our duplicate set of parameters that we pushed earlier
                // right here. However, we don't have to explicitly do that,
                // since we truncate the stack back to the original height
                // below.
            }
            builder.switch_to_block(frame.following_code());
            builder.seal_block(frame.following_code());
            // If it is a loop we also have to seal the body loop block
            if let ControlStackFrame::Loop { header, .. } = frame {
                builder.seal_block(header)
            }
            state.stack.truncate(frame.original_stack_size());
            state
                .stack
                .extend_from_slice(builder.ebb_params(frame.following_code()));
        }
        /**************************** Branch instructions *********************************
         * The branch instructions all have as arguments a target nesting level, which
         * corresponds to how many control stack frames do we have to pop to get the
         * destination `Ebb`.
         *
         * Once the destination `Ebb` is found, we sometimes have to declare a certain depth
         * of the stack unreachable, because some branch instructions are terminator.
         *
         * The `br_table` case is much more complicated because Cranelift's `br_table` instruction
         * does not support jump arguments like all the other branch instructions. That is why, in
         * the case where we would use jump arguments for every other branch instructions, we
         * need to split the critical edges leaving the `br_tables` by creating one `Ebb` per
         * table destination; the `br_table` will point to these newly created `Ebbs` and these
         * `Ebb`s contain only a jump instruction pointing to the final destination, this time with
         * jump arguments.
         *
         * This system is also implemented in Cranelift's SSA construction algorithm, because
         * `use_var` located in a destination `Ebb` of a `br_table` might trigger the addition
         * of jump arguments in each predecessor branch instruction, one of which might be a
         * `br_table`.
         ***********************************************************************************/
        Operator::Br { relative_depth } => {
            let i = state.control_stack.len() - 1 - (*relative_depth as usize);
            let (return_count, br_destination) = {
                let frame = &mut state.control_stack[i];
                // We signal that all the code that follows until the next End is unreachable
                frame.set_branched_to_exit();
                let return_count = if frame.is_loop() {
                    0
                } else {
                    frame.num_return_values()
                };
                (return_count, frame.br_destination())
            };
            builder
                .ins()
                .jump(br_destination, state.peekn(return_count));
            state.popn(return_count);
            state.reachable = false;
        }
        Operator::BrIf { relative_depth } => translate_br_if(*relative_depth, builder, state),
        Operator::BrTable { table } => {
            let (depths, default) = table.read_table()?;
            let mut min_depth = default;
            for depth in &*depths {
                if *depth < min_depth {
                    min_depth = *depth;
                }
            }
            let jump_args_count = {
                let i = state.control_stack.len() - 1 - (min_depth as usize);
                let min_depth_frame = &state.control_stack[i];
                if min_depth_frame.is_loop() {
                    0
                } else {
                    min_depth_frame.num_return_values()
                }
            };
            let val = state.pop1();
            let mut data = JumpTableData::with_capacity(depths.len());
            if jump_args_count == 0 {
                // No jump arguments
                for depth in &*depths {
                    let ebb = {
                        let i = state.control_stack.len() - 1 - (*depth as usize);
                        let frame = &mut state.control_stack[i];
                        frame.set_branched_to_exit();
                        frame.br_destination()
                    };
                    data.push_entry(ebb);
                }
                let jt = builder.create_jump_table(data);
                let ebb = {
                    let i = state.control_stack.len() - 1 - (default as usize);
                    let frame = &mut state.control_stack[i];
                    frame.set_branched_to_exit();
                    frame.br_destination()
                };
                builder.ins().br_table(val, ebb, jt);
            } else {
                // Here we have jump arguments, but Cranelift's br_table doesn't support them
                // We then proceed to split the edges going out of the br_table
                let return_count = jump_args_count;
                let mut dest_ebb_sequence = vec![];
                let mut dest_ebb_map = HashMap::new();
                for depth in &*depths {
                    let branch_ebb = match dest_ebb_map.entry(*depth as usize) {
                        hash_map::Entry::Occupied(entry) => *entry.get(),
                        hash_map::Entry::Vacant(entry) => {
                            let ebb = builder.create_ebb();
                            dest_ebb_sequence.push((*depth as usize, ebb));
                            *entry.insert(ebb)
                        }
                    };
                    data.push_entry(branch_ebb);
                }
                let default_branch_ebb = match dest_ebb_map.entry(default as usize) {
                    hash_map::Entry::Occupied(entry) => *entry.get(),
                    hash_map::Entry::Vacant(entry) => {
                        let ebb = builder.create_ebb();
                        dest_ebb_sequence.push((default as usize, ebb));
                        *entry.insert(ebb)
                    }
                };
                let jt = builder.create_jump_table(data);
                builder.ins().br_table(val, default_branch_ebb, jt);
                for (depth, dest_ebb) in dest_ebb_sequence {
                    builder.switch_to_block(dest_ebb);
                    builder.seal_block(dest_ebb);
                    let real_dest_ebb = {
                        let i = state.control_stack.len() - 1 - depth;
                        let frame = &mut state.control_stack[i];
                        frame.set_branched_to_exit();
                        frame.br_destination()
                    };
                    builder.ins().jump(real_dest_ebb, state.peekn(return_count));
                }
                state.popn(return_count);
            }
            state.reachable = false;
        }
        Operator::Return => {
            let (return_count, br_destination) = {
                let frame = &mut state.control_stack[0];
                frame.set_branched_to_exit();
                let return_count = frame.num_return_values();
                (return_count, frame.br_destination())
            };
            {
                let args = state.peekn(return_count);
                match environ.return_mode() {
                    ReturnMode::NormalReturns => builder.ins().return_(args),
                    ReturnMode::FallthroughReturn => builder.ins().jump(br_destination, args),
                };
            }
            state.popn(return_count);
            state.reachable = false;
        }
        /************************************ Calls ****************************************
         * The call instructions pop off their arguments from the stack and append their
         * return values to it. `call_indirect` needs environment support because there is an
         * argument referring to an index in the external functions table of the module.
         ************************************************************************************/
        Operator::Call { function_index } => {
            let (fref, num_args) = state.get_direct_func(builder.func, *function_index, environ)?;
            let call = environ.translate_call(
                builder.cursor(),
                FuncIndex::from_u32(*function_index),
                fref,
                state.peekn(num_args),
            )?;
            let inst_results = builder.inst_results(call);
            debug_assert_eq!(
                inst_results.len(),
                builder.func.dfg.signatures[builder.func.dfg.ext_funcs[fref].signature]
                    .returns
                    .len(),
                "translate_call results should match the call signature"
            );
            state.popn(num_args);
            state.pushn(inst_results);
        }
        Operator::CallIndirect { index, table_index } => {
            // `index` is the index of the function's signature and `table_index` is the index of
            // the table to search the function in.
            let (sigref, num_args) = state.get_indirect_sig(builder.func, *index, environ)?;
            let table = state.get_table(builder.func, *table_index, environ)?;
            let callee = state.pop1();
            let call = environ.translate_call_indirect(
                builder.cursor(),
                TableIndex::from_u32(*table_index),
                table,
                SignatureIndex::from_u32(*index),
                sigref,
                callee,
                state.peekn(num_args),
            )?;
            let inst_results = builder.inst_results(call);
            debug_assert_eq!(
                inst_results.len(),
                builder.func.dfg.signatures[sigref].returns.len(),
                "translate_call_indirect results should match the call signature"
            );
            state.popn(num_args);
            state.pushn(inst_results);
        }
        /******************************* Memory management ***********************************
         * Memory management is handled by environment. It is usually translated into calls to
         * special functions.
         ************************************************************************************/
        Operator::MemoryGrow { reserved } => {
            // The WebAssembly MVP only supports one linear memory, but we expect the reserved
            // argument to be a memory index.
            let heap_index = MemoryIndex::from_u32(*reserved);
            let heap = state.get_heap(builder.func, *reserved, environ)?;
            let val = state.pop1();
            state.push1(environ.translate_memory_grow(builder.cursor(), heap_index, heap, val)?)
        }
        Operator::MemorySize { reserved } => {
            let heap_index = MemoryIndex::from_u32(*reserved);
            let heap = state.get_heap(builder.func, *reserved, environ)?;
            state.push1(environ.translate_memory_size(builder.cursor(), heap_index, heap)?);
        }
        /******************************* Load instructions ***********************************
         * Wasm specifies an integer alignment flag but we drop it in Cranelift.
         * The memory base address is provided by the environment.
         ************************************************************************************/
        Operator::I32Load8U {
            memarg: MemoryImmediate { flags: _, offset },
        } => {
            translate_load(*offset, ir::Opcode::Uload8, I32, builder, state, environ)?;
        }
        Operator::I32Load16U {
            memarg: MemoryImmediate { flags: _, offset },
        } => {
            translate_load(*offset, ir::Opcode::Uload16, I32, builder, state, environ)?;
        }
        Operator::I32Load8S {
            memarg: MemoryImmediate { flags: _, offset },
        } => {
            translate_load(*offset, ir::Opcode::Sload8, I32, builder, state, environ)?;
        }
        Operator::I32Load16S {
            memarg: MemoryImmediate { flags: _, offset },
        } => {
            translate_load(*offset, ir::Opcode::Sload16, I32, builder, state, environ)?;
        }
        Operator::I64Load8U {
            memarg: MemoryImmediate { flags: _, offset },
        } => {
            translate_load(*offset, ir::Opcode::Uload8, I64, builder, state, environ)?;
        }
        Operator::I64Load16U {
            memarg: MemoryImmediate { flags: _, offset },
        } => {
            translate_load(*offset, ir::Opcode::Uload16, I64, builder, state, environ)?;
        }
        Operator::I64Load8S {
            memarg: MemoryImmediate { flags: _, offset },
        } => {
            translate_load(*offset, ir::Opcode::Sload8, I64, builder, state, environ)?;
        }
        Operator::I64Load16S {
            memarg: MemoryImmediate { flags: _, offset },
        } => {
            translate_load(*offset, ir::Opcode::Sload16, I64, builder, state, environ)?;
        }
        Operator::I64Load32S {
            memarg: MemoryImmediate { flags: _, offset },
        } => {
            translate_load(*offset, ir::Opcode::Sload32, I64, builder, state, environ)?;
        }
        Operator::I64Load32U {
            memarg: MemoryImmediate { flags: _, offset },
        } => {
            translate_load(*offset, ir::Opcode::Uload32, I64, builder, state, environ)?;
        }
        Operator::I32Load {
            memarg: MemoryImmediate { flags: _, offset },
        } => {
            translate_load(*offset, ir::Opcode::Load, I32, builder, state, environ)?;
        }
        Operator::F32Load {
            memarg: MemoryImmediate { flags: _, offset },
        } => {
            translate_load(*offset, ir::Opcode::Load, F32, builder, state, environ)?;
        }
        Operator::I64Load {
            memarg: MemoryImmediate { flags: _, offset },
        } => {
            translate_load(*offset, ir::Opcode::Load, I64, builder, state, environ)?;
        }
        Operator::F64Load {
            memarg: MemoryImmediate { flags: _, offset },
        } => {
            translate_load(*offset, ir::Opcode::Load, F64, builder, state, environ)?;
        }
        Operator::V128Load {
            memarg: MemoryImmediate { flags: _, offset },
        } => {
            translate_load(*offset, ir::Opcode::Load, I8X16, builder, state, environ)?;
        }
        /****************************** Store instructions ***********************************
         * Wasm specifies an integer alignment flag but we drop it in Cranelift.
         * The memory base address is provided by the environment.
         ************************************************************************************/
        Operator::I32Store {
            memarg: MemoryImmediate { flags: _, offset },
        }
        | Operator::I64Store {
            memarg: MemoryImmediate { flags: _, offset },
        }
        | Operator::F32Store {
            memarg: MemoryImmediate { flags: _, offset },
        }
        | Operator::F64Store {
            memarg: MemoryImmediate { flags: _, offset },
        } => {
            translate_store(*offset, ir::Opcode::Store, builder, state, environ)?;
        }
        Operator::I32Store8 {
            memarg: MemoryImmediate { flags: _, offset },
        }
        | Operator::I64Store8 {
            memarg: MemoryImmediate { flags: _, offset },
        } => {
            translate_store(*offset, ir::Opcode::Istore8, builder, state, environ)?;
        }
        Operator::I32Store16 {
            memarg: MemoryImmediate { flags: _, offset },
        }
        | Operator::I64Store16 {
            memarg: MemoryImmediate { flags: _, offset },
        } => {
            translate_store(*offset, ir::Opcode::Istore16, builder, state, environ)?;
        }
        Operator::I64Store32 {
            memarg: MemoryImmediate { flags: _, offset },
        } => {
            translate_store(*offset, ir::Opcode::Istore32, builder, state, environ)?;
        }
        Operator::V128Store {
            memarg: MemoryImmediate { flags: _, offset },
        } => {
            translate_store(*offset, ir::Opcode::Store, builder, state, environ)?;
        }
        /****************************** Nullary Operators ************************************/
        Operator::I32Const { value } => state.push1(builder.ins().iconst(I32, i64::from(*value))),
        Operator::I64Const { value } => state.push1(builder.ins().iconst(I64, *value)),
        Operator::F32Const { value } => {
            state.push1(builder.ins().f32const(f32_translation(*value)));
        }
        Operator::F64Const { value } => {
            state.push1(builder.ins().f64const(f64_translation(*value)));
        }
        /******************************* Unary Operators *************************************/
        Operator::I32Clz | Operator::I64Clz => {
            let arg = state.pop1();
            state.push1(builder.ins().clz(arg));
        }
        Operator::I32Ctz | Operator::I64Ctz => {
            let arg = state.pop1();
            state.push1(builder.ins().ctz(arg));
        }
        Operator::I32Popcnt | Operator::I64Popcnt => {
            let arg = state.pop1();
            state.push1(builder.ins().popcnt(arg));
        }
        Operator::I64ExtendSI32 => {
            let val = state.pop1();
            state.push1(builder.ins().sextend(I64, val));
        }
        Operator::I64ExtendUI32 => {
            let val = state.pop1();
            state.push1(builder.ins().uextend(I64, val));
        }
        Operator::I32WrapI64 => {
            let val = state.pop1();
            state.push1(builder.ins().ireduce(I32, val));
        }
        Operator::F32Sqrt | Operator::F64Sqrt => {
            let arg = state.pop1();
            state.push1(builder.ins().sqrt(arg));
        }
        Operator::F32Ceil | Operator::F64Ceil => {
            let arg = state.pop1();
            state.push1(builder.ins().ceil(arg));
        }
        Operator::F32Floor | Operator::F64Floor => {
            let arg = state.pop1();
            state.push1(builder.ins().floor(arg));
        }
        Operator::F32Trunc | Operator::F64Trunc => {
            let arg = state.pop1();
            state.push1(builder.ins().trunc(arg));
        }
        Operator::F32Nearest | Operator::F64Nearest => {
            let arg = state.pop1();
            state.push1(builder.ins().nearest(arg));
        }
        Operator::F32Abs | Operator::F64Abs => {
            let val = state.pop1();
            state.push1(builder.ins().fabs(val));
        }
        Operator::F32Neg | Operator::F64Neg => {
            let arg = state.pop1();
            state.push1(builder.ins().fneg(arg));
        }
        Operator::F64ConvertUI64 | Operator::F64ConvertUI32 => {
            let val = state.pop1();
            state.push1(builder.ins().fcvt_from_uint(F64, val));
        }
        Operator::F64ConvertSI64 | Operator::F64ConvertSI32 => {
            let val = state.pop1();
            state.push1(builder.ins().fcvt_from_sint(F64, val));
        }
        Operator::F32ConvertSI64 | Operator::F32ConvertSI32 => {
            let val = state.pop1();
            state.push1(builder.ins().fcvt_from_sint(F32, val));
        }
        Operator::F32ConvertUI64 | Operator::F32ConvertUI32 => {
            let val = state.pop1();
            state.push1(builder.ins().fcvt_from_uint(F32, val));
        }
        Operator::F64PromoteF32 => {
            let val = state.pop1();
            state.push1(builder.ins().fpromote(F64, val));
        }
        Operator::F32DemoteF64 => {
            let val = state.pop1();
            state.push1(builder.ins().fdemote(F32, val));
        }
        Operator::I64TruncSF64 | Operator::I64TruncSF32 => {
            let val = state.pop1();
            state.push1(builder.ins().fcvt_to_sint(I64, val));
        }
        Operator::I32TruncSF64 | Operator::I32TruncSF32 => {
            let val = state.pop1();
            state.push1(builder.ins().fcvt_to_sint(I32, val));
        }
        Operator::I64TruncUF64 | Operator::I64TruncUF32 => {
            let val = state.pop1();
            state.push1(builder.ins().fcvt_to_uint(I64, val));
        }
        Operator::I32TruncUF64 | Operator::I32TruncUF32 => {
            let val = state.pop1();
            state.push1(builder.ins().fcvt_to_uint(I32, val));
        }
        Operator::I64TruncSSatF64 | Operator::I64TruncSSatF32 => {
            let val = state.pop1();
            state.push1(builder.ins().fcvt_to_sint_sat(I64, val));
        }
        Operator::I32TruncSSatF64 | Operator::I32TruncSSatF32 => {
            let val = state.pop1();
            state.push1(builder.ins().fcvt_to_sint_sat(I32, val));
        }
        Operator::I64TruncUSatF64 | Operator::I64TruncUSatF32 => {
            let val = state.pop1();
            state.push1(builder.ins().fcvt_to_uint_sat(I64, val));
        }
        Operator::I32TruncUSatF64 | Operator::I32TruncUSatF32 => {
            let val = state.pop1();
            state.push1(builder.ins().fcvt_to_uint_sat(I32, val));
        }
        Operator::F32ReinterpretI32 => {
            let val = state.pop1();
            state.push1(builder.ins().bitcast(F32, val));
        }
        Operator::F64ReinterpretI64 => {
            let val = state.pop1();
            state.push1(builder.ins().bitcast(F64, val));
        }
        Operator::I32ReinterpretF32 => {
            let val = state.pop1();
            state.push1(builder.ins().bitcast(I32, val));
        }
        Operator::I64ReinterpretF64 => {
            let val = state.pop1();
            state.push1(builder.ins().bitcast(I64, val));
        }
        Operator::I32Extend8S => {
            let val = state.pop1();
            state.push1(builder.ins().ireduce(I8, val));
            let val = state.pop1();
            state.push1(builder.ins().sextend(I32, val));
        }
        Operator::I32Extend16S => {
            let val = state.pop1();
            state.push1(builder.ins().ireduce(I16, val));
            let val = state.pop1();
            state.push1(builder.ins().sextend(I32, val));
        }
        Operator::I64Extend8S => {
            let val = state.pop1();
            state.push1(builder.ins().ireduce(I8, val));
            let val = state.pop1();
            state.push1(builder.ins().sextend(I64, val));
        }
        Operator::I64Extend16S => {
            let val = state.pop1();
            state.push1(builder.ins().ireduce(I16, val));
            let val = state.pop1();
            state.push1(builder.ins().sextend(I64, val));
        }
        Operator::I64Extend32S => {
            let val = state.pop1();
            state.push1(builder.ins().ireduce(I32, val));
            let val = state.pop1();
            state.push1(builder.ins().sextend(I64, val));
        }
        /****************************** Binary Operators ************************************/
        Operator::I32Add | Operator::I64Add => {
            let (arg1, arg2) = state.pop2();
            state.push1(builder.ins().iadd(arg1, arg2));
        }
        Operator::I32And | Operator::I64And => {
            let (arg1, arg2) = state.pop2();
            state.push1(builder.ins().band(arg1, arg2));
        }
        Operator::I32Or | Operator::I64Or => {
            let (arg1, arg2) = state.pop2();
            state.push1(builder.ins().bor(arg1, arg2));
        }
        Operator::I32Xor | Operator::I64Xor => {
            let (arg1, arg2) = state.pop2();
            state.push1(builder.ins().bxor(arg1, arg2));
        }
        Operator::I32Shl | Operator::I64Shl => {
            let (arg1, arg2) = state.pop2();
            state.push1(builder.ins().ishl(arg1, arg2));
        }
        Operator::I32ShrS | Operator::I64ShrS => {
            let (arg1, arg2) = state.pop2();
            state.push1(builder.ins().sshr(arg1, arg2));
        }
        Operator::I32ShrU | Operator::I64ShrU => {
            let (arg1, arg2) = state.pop2();
            state.push1(builder.ins().ushr(arg1, arg2));
        }
        Operator::I32Rotl | Operator::I64Rotl => {
            let (arg1, arg2) = state.pop2();
            state.push1(builder.ins().rotl(arg1, arg2));
        }
        Operator::I32Rotr | Operator::I64Rotr => {
            let (arg1, arg2) = state.pop2();
            state.push1(builder.ins().rotr(arg1, arg2));
        }
        Operator::F32Add | Operator::F64Add => {
            let (arg1, arg2) = state.pop2();
            state.push1(builder.ins().fadd(arg1, arg2));
        }
        Operator::I32Sub | Operator::I64Sub => {
            let (arg1, arg2) = state.pop2();
            state.push1(builder.ins().isub(arg1, arg2));
        }
        Operator::F32Sub | Operator::F64Sub => {
            let (arg1, arg2) = state.pop2();
            state.push1(builder.ins().fsub(arg1, arg2));
        }
        Operator::I32Mul | Operator::I64Mul => {
            let (arg1, arg2) = state.pop2();
            state.push1(builder.ins().imul(arg1, arg2));
        }
        Operator::F32Mul | Operator::F64Mul => {
            let (arg1, arg2) = state.pop2();
            state.push1(builder.ins().fmul(arg1, arg2));
        }
        Operator::F32Div | Operator::F64Div => {
            let (arg1, arg2) = state.pop2();
            state.push1(builder.ins().fdiv(arg1, arg2));
        }
        Operator::I32DivS | Operator::I64DivS => {
            let (arg1, arg2) = state.pop2();
            state.push1(builder.ins().sdiv(arg1, arg2));
        }
        Operator::I32DivU | Operator::I64DivU => {
            let (arg1, arg2) = state.pop2();
            state.push1(builder.ins().udiv(arg1, arg2));
        }
        Operator::I32RemS | Operator::I64RemS => {
            let (arg1, arg2) = state.pop2();
            state.push1(builder.ins().srem(arg1, arg2));
        }
        Operator::I32RemU | Operator::I64RemU => {
            let (arg1, arg2) = state.pop2();
            state.push1(builder.ins().urem(arg1, arg2));
        }
        Operator::F32Min | Operator::F64Min => {
            let (arg1, arg2) = state.pop2();
            state.push1(builder.ins().fmin(arg1, arg2));
        }
        Operator::F32Max | Operator::F64Max => {
            let (arg1, arg2) = state.pop2();
            state.push1(builder.ins().fmax(arg1, arg2));
        }
        Operator::F32Copysign | Operator::F64Copysign => {
            let (arg1, arg2) = state.pop2();
            state.push1(builder.ins().fcopysign(arg1, arg2));
        }
        /**************************** Comparison Operators **********************************/
        Operator::I32LtS | Operator::I64LtS => {
            translate_icmp(IntCC::SignedLessThan, builder, state)
        }
        Operator::I32LtU | Operator::I64LtU => {
            translate_icmp(IntCC::UnsignedLessThan, builder, state)
        }
        Operator::I32LeS | Operator::I64LeS => {
            translate_icmp(IntCC::SignedLessThanOrEqual, builder, state)
        }
        Operator::I32LeU | Operator::I64LeU => {
            translate_icmp(IntCC::UnsignedLessThanOrEqual, builder, state)
        }
        Operator::I32GtS | Operator::I64GtS => {
            translate_icmp(IntCC::SignedGreaterThan, builder, state)
        }
        Operator::I32GtU | Operator::I64GtU => {
            translate_icmp(IntCC::UnsignedGreaterThan, builder, state)
        }
        Operator::I32GeS | Operator::I64GeS => {
            translate_icmp(IntCC::SignedGreaterThanOrEqual, builder, state)
        }
        Operator::I32GeU | Operator::I64GeU => {
            translate_icmp(IntCC::UnsignedGreaterThanOrEqual, builder, state)
        }
        Operator::I32Eqz | Operator::I64Eqz => {
            let arg = state.pop1();
            let val = builder.ins().icmp_imm(IntCC::Equal, arg, 0);
            state.push1(builder.ins().bint(I32, val));
        }
        Operator::I32Eq | Operator::I64Eq => translate_icmp(IntCC::Equal, builder, state),
        Operator::F32Eq | Operator::F64Eq => translate_fcmp(FloatCC::Equal, builder, state),
        Operator::I32Ne | Operator::I64Ne => translate_icmp(IntCC::NotEqual, builder, state),
        Operator::F32Ne | Operator::F64Ne => translate_fcmp(FloatCC::NotEqual, builder, state),
        Operator::F32Gt | Operator::F64Gt => translate_fcmp(FloatCC::GreaterThan, builder, state),
        Operator::F32Ge | Operator::F64Ge => {
            translate_fcmp(FloatCC::GreaterThanOrEqual, builder, state)
        }
        Operator::F32Lt | Operator::F64Lt => translate_fcmp(FloatCC::LessThan, builder, state),
        Operator::F32Le | Operator::F64Le => {
            translate_fcmp(FloatCC::LessThanOrEqual, builder, state)
        }
        Operator::RefNull => state.push1(builder.ins().null(environ.reference_type())),
        Operator::RefIsNull => {
            let arg = state.pop1();
            let val = builder.ins().is_null(arg);
            state.push1(val);
        }
        Operator::Wake { .. }
        | Operator::I32Wait { .. }
        | Operator::I64Wait { .. }
        | Operator::I32AtomicLoad { .. }
        | Operator::I64AtomicLoad { .. }
        | Operator::I32AtomicLoad8U { .. }
        | Operator::I32AtomicLoad16U { .. }
        | Operator::I64AtomicLoad8U { .. }
        | Operator::I64AtomicLoad16U { .. }
        | Operator::I64AtomicLoad32U { .. }
        | Operator::I32AtomicStore { .. }
        | Operator::I64AtomicStore { .. }
        | Operator::I32AtomicStore8 { .. }
        | Operator::I32AtomicStore16 { .. }
        | Operator::I64AtomicStore8 { .. }
        | Operator::I64AtomicStore16 { .. }
        | Operator::I64AtomicStore32 { .. }
        | Operator::I32AtomicRmwAdd { .. }
        | Operator::I64AtomicRmwAdd { .. }
        | Operator::I32AtomicRmw8UAdd { .. }
        | Operator::I32AtomicRmw16UAdd { .. }
        | Operator::I64AtomicRmw8UAdd { .. }
        | Operator::I64AtomicRmw16UAdd { .. }
        | Operator::I64AtomicRmw32UAdd { .. }
        | Operator::I32AtomicRmwSub { .. }
        | Operator::I64AtomicRmwSub { .. }
        | Operator::I32AtomicRmw8USub { .. }
        | Operator::I32AtomicRmw16USub { .. }
        | Operator::I64AtomicRmw8USub { .. }
        | Operator::I64AtomicRmw16USub { .. }
        | Operator::I64AtomicRmw32USub { .. }
        | Operator::I32AtomicRmwAnd { .. }
        | Operator::I64AtomicRmwAnd { .. }
        | Operator::I32AtomicRmw8UAnd { .. }
        | Operator::I32AtomicRmw16UAnd { .. }
        | Operator::I64AtomicRmw8UAnd { .. }
        | Operator::I64AtomicRmw16UAnd { .. }
        | Operator::I64AtomicRmw32UAnd { .. }
        | Operator::I32AtomicRmwOr { .. }
        | Operator::I64AtomicRmwOr { .. }
        | Operator::I32AtomicRmw8UOr { .. }
        | Operator::I32AtomicRmw16UOr { .. }
        | Operator::I64AtomicRmw8UOr { .. }
        | Operator::I64AtomicRmw16UOr { .. }
        | Operator::I64AtomicRmw32UOr { .. }
        | Operator::I32AtomicRmwXor { .. }
        | Operator::I64AtomicRmwXor { .. }
        | Operator::I32AtomicRmw8UXor { .. }
        | Operator::I32AtomicRmw16UXor { .. }
        | Operator::I64AtomicRmw8UXor { .. }
        | Operator::I64AtomicRmw16UXor { .. }
        | Operator::I64AtomicRmw32UXor { .. }
        | Operator::I32AtomicRmwXchg { .. }
        | Operator::I64AtomicRmwXchg { .. }
        | Operator::I32AtomicRmw8UXchg { .. }
        | Operator::I32AtomicRmw16UXchg { .. }
        | Operator::I64AtomicRmw8UXchg { .. }
        | Operator::I64AtomicRmw16UXchg { .. }
        | Operator::I64AtomicRmw32UXchg { .. }
        | Operator::I32AtomicRmwCmpxchg { .. }
        | Operator::I64AtomicRmwCmpxchg { .. }
        | Operator::I32AtomicRmw8UCmpxchg { .. }
        | Operator::I32AtomicRmw16UCmpxchg { .. }
        | Operator::I64AtomicRmw8UCmpxchg { .. }
        | Operator::I64AtomicRmw16UCmpxchg { .. }
        | Operator::I64AtomicRmw32UCmpxchg { .. }
        | Operator::Fence { .. } => {
            return Err(wasm_unsupported!("proposed thread operator {:?}", op));
        }
        Operator::MemoryInit { .. }
        | Operator::DataDrop { .. }
        | Operator::MemoryCopy
        | Operator::MemoryFill
        | Operator::TableInit { .. }
        | Operator::ElemDrop { .. }
        | Operator::TableCopy
        | Operator::TableGet { .. }
        | Operator::TableSet { .. }
        | Operator::TableGrow { .. }
        | Operator::TableSize { .. } => {
            return Err(wasm_unsupported!("proposed bulk memory operator {:?}", op));
        }
        Operator::V128Const { value } => {
            let data = value.bytes().to_vec().into();
            let handle = builder.func.dfg.constants.insert(data);
            let value = builder.ins().vconst(I8X16, handle);
            // the v128.const is typed in CLIF as a I8x16 but raw_bitcast to a different type before use
            state.push1(value)
        }
        Operator::I8x16Splat
        | Operator::I16x8Splat
        | Operator::I32x4Splat
        | Operator::I64x2Splat
        | Operator::F32x4Splat
        | Operator::F64x2Splat => {
            let value_to_splat = state.pop1();
            let ty = type_of(op);
            let splatted = builder.ins().splat(ty, value_to_splat);
            state.push1(splatted)
        }
        Operator::I8x16ExtractLaneS { lane } | Operator::I16x8ExtractLaneS { lane } => {
            let vector = optionally_bitcast_vector(state.pop1(), type_of(op), builder);
            let extracted = builder.ins().extractlane(vector, lane.clone());
            state.push1(builder.ins().sextend(I32, extracted))
        }
        Operator::I8x16ExtractLaneU { lane } | Operator::I16x8ExtractLaneU { lane } => {
            let vector = optionally_bitcast_vector(state.pop1(), type_of(op), builder);
            state.push1(builder.ins().extractlane(vector, lane.clone()));
            // on x86, PEXTRB zeroes the upper bits of the destination register of extractlane so uextend is elided; of course, this depends on extractlane being legalized to a PEXTRB
        }
        Operator::I32x4ExtractLane { lane }
        | Operator::I64x2ExtractLane { lane }
        | Operator::F32x4ExtractLane { lane }
        | Operator::F64x2ExtractLane { lane } => {
            let vector = optionally_bitcast_vector(state.pop1(), type_of(op), builder);
            state.push1(builder.ins().extractlane(vector, lane.clone()))
        }
        Operator::I8x16ReplaceLane { lane }
        | Operator::I16x8ReplaceLane { lane }
        | Operator::I32x4ReplaceLane { lane }
        | Operator::I64x2ReplaceLane { lane }
        | Operator::F32x4ReplaceLane { lane }
        | Operator::F64x2ReplaceLane { lane } => {
            let (vector, replacement_value) = state.pop2();
            let original_vector_type = builder.func.dfg.value_type(vector);
            let vector = optionally_bitcast_vector(vector, type_of(op), builder);
            let replaced_vector = builder
                .ins()
                .insertlane(vector, lane.clone(), replacement_value);
            state.push1(optionally_bitcast_vector(
                replaced_vector,
                original_vector_type,
                builder,
            ))
        }
        Operator::V8x16Shuffle { lanes, .. } => {
            let (vector_a, vector_b) = state.pop2();
            let a = optionally_bitcast_vector(vector_a, I8X16, builder);
            let b = optionally_bitcast_vector(vector_b, I8X16, builder);
            let lanes = ConstantData::from(lanes.as_ref());
            let mask = builder.func.dfg.immediates.push(lanes);
            let shuffled = builder.ins().shuffle(a, b, mask);
            state.push1(shuffled)
            // At this point the original types of a and b are lost; users of this value (i.e. this
            // WASM-to-CLIF translator) may need to raw_bitcast for type-correctness. This is due
            // to WASM using the less specific v128 type for certain operations and more specific
            // types (e.g. i8x16) for others.
        }
        Operator::I8x16Add | Operator::I16x8Add | Operator::I32x4Add | Operator::I64x2Add => {
            let (a, b) = state.pop2();
            state.push1(builder.ins().iadd(a, b))
        }
        Operator::I8x16AddSaturateS | Operator::I16x8AddSaturateS => {
            let (a, b) = state.pop2();
            state.push1(builder.ins().sadd_sat(a, b))
        }
        Operator::I8x16AddSaturateU | Operator::I16x8AddSaturateU => {
            let (a, b) = state.pop2();
            state.push1(builder.ins().uadd_sat(a, b))
        }
        Operator::I8x16Sub | Operator::I16x8Sub | Operator::I32x4Sub | Operator::I64x2Sub => {
            let (a, b) = state.pop2();
            state.push1(builder.ins().isub(a, b))
        }
        Operator::I8x16SubSaturateS | Operator::I16x8SubSaturateS => {
            let (a, b) = state.pop2();
            state.push1(builder.ins().ssub_sat(a, b))
        }
        Operator::I8x16SubSaturateU | Operator::I16x8SubSaturateU => {
            let (a, b) = state.pop2();
            state.push1(builder.ins().usub_sat(a, b))
        }
        Operator::I8x16Neg | Operator::I16x8Neg | Operator::I32x4Neg | Operator::I64x2Neg => {
            let a = state.pop1();
            state.push1(builder.ins().ineg(a))
        }
        Operator::I16x8Mul | Operator::I32x4Mul => {
            let (a, b) = state.pop2();
            state.push1(builder.ins().imul(a, b))
        }
        Operator::V128Not => {
            let a = state.pop1();
            state.push1(builder.ins().bnot(a));
        }
        Operator::V128And => {
            let (a, b) = state.pop2();
            state.push1(builder.ins().band(a, b));
        }
        Operator::V128Or => {
            let (a, b) = state.pop2();
            state.push1(builder.ins().bor(a, b));
        }
        Operator::V128Xor => {
            let (a, b) = state.pop2();
            state.push1(builder.ins().bxor(a, b));
        }
        Operator::I16x8Shl | Operator::I32x4Shl | Operator::I64x2Shl => {
            let (a, b) = state.pop2();
            let bitcast_a = optionally_bitcast_vector(a, type_of(op), builder);
            let bitwidth = i64::from(builder.func.dfg.value_type(a).bits());
            // The spec expects to shift with `b mod lanewidth`; so, e.g., for 16 bit lane-width
            // we do `b AND 15`; this means fewer instructions than `iconst + urem`.
            let b_mod_bitwidth = builder.ins().band_imm(b, bitwidth - 1);
            state.push1(builder.ins().ishl(bitcast_a, b_mod_bitwidth))
        }
        Operator::I16x8ShrU | Operator::I32x4ShrU | Operator::I64x2ShrU => {
            let (a, b) = state.pop2();
            let bitcast_a = optionally_bitcast_vector(a, type_of(op), builder);
            let bitwidth = i64::from(builder.func.dfg.value_type(a).bits());
            // The spec expects to shift with `b mod lanewidth`; so, e.g., for 16 bit lane-width
            // we do `b AND 15`; this means fewer instructions than `iconst + urem`.
            let b_mod_bitwidth = builder.ins().band_imm(b, bitwidth - 1);
            state.push1(builder.ins().ushr(bitcast_a, b_mod_bitwidth))
        }
        Operator::I16x8ShrS | Operator::I32x4ShrS => {
            let (a, b) = state.pop2();
            let bitcast_a = optionally_bitcast_vector(a, type_of(op), builder);
            let bitwidth = i64::from(builder.func.dfg.value_type(a).bits());
            // The spec expects to shift with `b mod lanewidth`; so, e.g., for 16 bit lane-width
            // we do `b AND 15`; this means fewer instructions than `iconst + urem`.
            let b_mod_bitwidth = builder.ins().band_imm(b, bitwidth - 1);
            state.push1(builder.ins().sshr(bitcast_a, b_mod_bitwidth))
        }
        Operator::I8x16Eq
        | Operator::I8x16Ne
        | Operator::I8x16LtS
        | Operator::I8x16LtU
        | Operator::I8x16GtS
        | Operator::I8x16GtU
        | Operator::I8x16LeS
        | Operator::I8x16LeU
        | Operator::I8x16GeS
        | Operator::I8x16GeU
        | Operator::I16x8Eq
        | Operator::I16x8Ne
        | Operator::I16x8LtS
        | Operator::I16x8LtU
        | Operator::I16x8GtS
        | Operator::I16x8GtU
        | Operator::I16x8LeS
        | Operator::I16x8LeU
        | Operator::I16x8GeS
        | Operator::I16x8GeU
        | Operator::I32x4Eq
        | Operator::I32x4Ne
        | Operator::I32x4LtS
        | Operator::I32x4LtU
        | Operator::I32x4GtS
        | Operator::I32x4GtU
        | Operator::I32x4LeS
        | Operator::I32x4LeU
        | Operator::I32x4GeS
        | Operator::I32x4GeU
        | Operator::F32x4Eq
        | Operator::F32x4Ne
        | Operator::F32x4Lt
        | Operator::F32x4Gt
        | Operator::F32x4Le
        | Operator::F32x4Ge
        | Operator::F64x2Eq
        | Operator::F64x2Ne
        | Operator::F64x2Lt
        | Operator::F64x2Gt
        | Operator::F64x2Le
        | Operator::F64x2Ge
        | Operator::V128Bitselect
        | Operator::I8x16AnyTrue
        | Operator::I8x16AllTrue
        | Operator::I8x16Shl
        | Operator::I8x16ShrS
        | Operator::I8x16ShrU
        | Operator::I8x16Mul
        | Operator::I16x8AnyTrue
        | Operator::I16x8AllTrue
        | Operator::I32x4AnyTrue
        | Operator::I32x4AllTrue
        | Operator::I64x2AnyTrue
        | Operator::I64x2AllTrue
        | Operator::I64x2ShrS
        | Operator::F32x4Abs
        | Operator::F32x4Neg
        | Operator::F32x4Sqrt
        | Operator::F32x4Add
        | Operator::F32x4Sub
        | Operator::F32x4Mul
        | Operator::F32x4Div
        | Operator::F32x4Min
        | Operator::F32x4Max
        | Operator::F64x2Abs
        | Operator::F64x2Neg
        | Operator::F64x2Sqrt
        | Operator::F64x2Add
        | Operator::F64x2Sub
        | Operator::F64x2Mul
        | Operator::F64x2Div
        | Operator::F64x2Min
        | Operator::F64x2Max
        | Operator::I32x4TruncSF32x4Sat
        | Operator::I32x4TruncUF32x4Sat
        | Operator::I64x2TruncSF64x2Sat
        | Operator::I64x2TruncUF64x2Sat
        | Operator::F32x4ConvertSI32x4
        | Operator::F32x4ConvertUI32x4
        | Operator::F64x2ConvertSI64x2
        | Operator::F64x2ConvertUI64x2 { .. }
        | Operator::V8x16Swizzle
        | Operator::I8x16LoadSplat { .. }
        | Operator::I16x8LoadSplat { .. }
        | Operator::I32x4LoadSplat { .. }
        | Operator::I64x2LoadSplat { .. } => {
            return Err(wasm_unsupported!("proposed SIMD operator {:?}", op));
        }
    };
    Ok(())
}

// Clippy warns us of some fields we are deliberately ignoring
#[cfg_attr(feature = "cargo-clippy", allow(clippy::unneeded_field_pattern))]
/// Deals with a Wasm instruction located in an unreachable portion of the code. Most of them
/// are dropped but special ones like `End` or `Else` signal the potential end of the unreachable
/// portion so the translation state must be updated accordingly.
fn translate_unreachable_operator(
    module_translation_state: &ModuleTranslationState,
    op: &Operator,
    builder: &mut FunctionBuilder,
    state: &mut FuncTranslationState,
) -> WasmResult<()> {
    debug_assert!(!state.reachable);
    match *op {
        Operator::If { ty } => {
            // Push a placeholder control stack entry. The if isn't reachable,
            // so we don't have any branches anywhere.
            state.push_if(
                ir::Ebb::reserved_value(),
                ElseData::NoElse {
                    branch_inst: ir::Inst::reserved_value(),
                },
                0,
                0,
                ty,
            );
        }
        Operator::Loop { ty: _ } | Operator::Block { ty: _ } => {
            state.push_block(ir::Ebb::reserved_value(), 0, 0);
        }
        Operator::Else => {
            let i = state.control_stack.len() - 1;
            match state.control_stack[i] {
                ControlStackFrame::If {
                    ref else_data,
                    head_is_reachable,
                    ref mut consequent_ends_reachable,
                    blocktype,
                    ..
                } => {
                    debug_assert!(consequent_ends_reachable.is_none());
                    *consequent_ends_reachable = Some(state.reachable);

                    if head_is_reachable {
                        // We have a branch from the head of the `if` to the `else`.
                        state.reachable = true;

                        let else_ebb = match *else_data {
                            ElseData::NoElse { branch_inst } => {
                                let (params, _results) =
                                    blocktype_params_results(module_translation_state, blocktype)?;
                                let else_ebb = ebb_with_params(builder, params)?;

                                // We change the target of the branch instruction.
                                builder.change_jump_destination(branch_inst, else_ebb);
                                builder.seal_block(else_ebb);
                                else_ebb
                            }
                            ElseData::WithElse { else_block } => else_block,
                        };

                        builder.switch_to_block(else_ebb);

                        // Again, no need to push the parameters for the `else`,
                        // since we already did when we saw the original `if`. See
                        // the comment for translating `Operator::Else` in
                        // `translate_operator` for details.
                    }
                }
                _ => unreachable!(),
            }
        }
        Operator::End => {
            let stack = &mut state.stack;
            let control_stack = &mut state.control_stack;
            let frame = control_stack.pop().unwrap();

            // Now we have to split off the stack the values not used
            // by unreachable code that hasn't been translated
            stack.truncate(frame.original_stack_size());

            let reachable_anyway = match frame {
                // If it is a loop we also have to seal the body loop block
                ControlStackFrame::Loop { header, .. } => {
                    builder.seal_block(header);
                    // And loops can't have branches to the end.
                    false
                }
                // If we never set `consequent_ends_reachable` then that means
                // we are finishing the consequent now, and there was no
                // `else`. Whether the following block is reachable depends only
                // on if the head was reachable.
                ControlStackFrame::If {
                    head_is_reachable,
                    consequent_ends_reachable: None,
                    ..
                } => head_is_reachable,
                // Since we are only in this function when in unreachable code,
                // we know that the alternative just ended unreachable. Whether
                // the following block is reachable depends on if the consequent
                // ended reachable or not.
                ControlStackFrame::If {
                    head_is_reachable,
                    consequent_ends_reachable: Some(consequent_ends_reachable),
                    ..
                } => head_is_reachable && consequent_ends_reachable,
                // All other control constructs are already handled.
                _ => false,
            };

            if frame.exit_is_branched_to() || reachable_anyway {
                builder.switch_to_block(frame.following_code());
                builder.seal_block(frame.following_code());

                // And add the return values of the block but only if the next block is reachable
                // (which corresponds to testing if the stack depth is 1)
                stack.extend_from_slice(builder.ebb_params(frame.following_code()));
                state.reachable = true;
            }
        }
        _ => {
            // We don't translate because this is unreachable code
        }
    }

    Ok(())
}

/// Get the address+offset to use for a heap access.
fn get_heap_addr(
    heap: ir::Heap,
    addr32: ir::Value,
    offset: u32,
    addr_ty: Type,
    builder: &mut FunctionBuilder,
) -> (ir::Value, i32) {
    use core::cmp::min;

    let mut adjusted_offset = u64::from(offset);
    let offset_guard_size: u64 = builder.func.heaps[heap].offset_guard_size.into();

    // Generate `heap_addr` instructions that are friendly to CSE by checking offsets that are
    // multiples of the offset-guard size. Add one to make sure that we check the pointer itself
    // is in bounds.
    if offset_guard_size != 0 {
        adjusted_offset = adjusted_offset / offset_guard_size * offset_guard_size;
    }

    // For accesses on the outer skirts of the offset-guard pages, we expect that we get a trap
    // even if the access goes beyond the offset-guard pages. This is because the first byte
    // pointed to is inside the offset-guard pages.
    let check_size = min(u64::from(u32::MAX), 1 + adjusted_offset) as u32;
    let base = builder.ins().heap_addr(addr_ty, heap, addr32, check_size);

    // Native load/store instructions take a signed `Offset32` immediate, so adjust the base
    // pointer if necessary.
    if offset > i32::MAX as u32 {
        // Offset doesn't fit in the load/store instruction.
        let adj = builder.ins().iadd_imm(base, i64::from(i32::MAX) + 1);
        (adj, (offset - (i32::MAX as u32 + 1)) as i32)
    } else {
        (base, offset as i32)
    }
}

/// Translate a load instruction.
fn translate_load<FE: FuncEnvironment + ?Sized>(
    offset: u32,
    opcode: ir::Opcode,
    result_ty: Type,
    builder: &mut FunctionBuilder,
    state: &mut FuncTranslationState,
    environ: &mut FE,
) -> WasmResult<()> {
    let addr32 = state.pop1();
    // We don't yet support multiple linear memories.
    let heap = state.get_heap(builder.func, 0, environ)?;
    let (base, offset) = get_heap_addr(heap, addr32, offset, environ.pointer_type(), builder);
    // Note that we don't set `is_aligned` here, even if the load instruction's
    // alignment immediate says it's aligned, because WebAssembly's immediate
    // field is just a hint, while Cranelift's aligned flag needs a guarantee.
    let flags = MemFlags::new();
    let (load, dfg) = builder
        .ins()
        .Load(opcode, result_ty, flags, offset.into(), base);
    state.push1(dfg.first_result(load));
    Ok(())
}

/// Translate a store instruction.
fn translate_store<FE: FuncEnvironment + ?Sized>(
    offset: u32,
    opcode: ir::Opcode,
    builder: &mut FunctionBuilder,
    state: &mut FuncTranslationState,
    environ: &mut FE,
) -> WasmResult<()> {
    let (addr32, val) = state.pop2();
    let val_ty = builder.func.dfg.value_type(val);

    // We don't yet support multiple linear memories.
    let heap = state.get_heap(builder.func, 0, environ)?;
    let (base, offset) = get_heap_addr(heap, addr32, offset, environ.pointer_type(), builder);
    // See the comments in `translate_load` about the flags.
    let flags = MemFlags::new();
    builder
        .ins()
        .Store(opcode, val_ty, flags, offset.into(), val, base);
    Ok(())
}

fn translate_icmp(cc: IntCC, builder: &mut FunctionBuilder, state: &mut FuncTranslationState) {
    let (arg0, arg1) = state.pop2();
    let val = builder.ins().icmp(cc, arg0, arg1);
    state.push1(builder.ins().bint(I32, val));
}

fn translate_fcmp(cc: FloatCC, builder: &mut FunctionBuilder, state: &mut FuncTranslationState) {
    let (arg0, arg1) = state.pop2();
    let val = builder.ins().fcmp(cc, arg0, arg1);
    state.push1(builder.ins().bint(I32, val));
}

fn translate_br_if(
    relative_depth: u32,
    builder: &mut FunctionBuilder,
    state: &mut FuncTranslationState,
) {
    let val = state.pop1();
    let (br_destination, inputs) = translate_br_if_args(relative_depth, state);
    builder.ins().brnz(val, br_destination, inputs);

    #[cfg(feature = "basic-blocks")]
    {
        let next_ebb = builder.create_ebb();
        builder.ins().jump(next_ebb, &[]);
        builder.seal_block(next_ebb); // The only predecessor is the current block.
        builder.switch_to_block(next_ebb);
    }
}

fn translate_br_if_args(
    relative_depth: u32,
    state: &mut FuncTranslationState,
) -> (ir::Ebb, &[ir::Value]) {
    let i = state.control_stack.len() - 1 - (relative_depth as usize);
    let (return_count, br_destination) = {
        let frame = &mut state.control_stack[i];
        // The values returned by the branch are still available for the reachable
        // code that comes after it
        frame.set_branched_to_exit();
        let return_count = if frame.is_loop() {
            frame.num_param_values()
        } else {
            frame.num_return_values()
        };
        (return_count, frame.br_destination())
    };
    let inputs = state.peekn(return_count);
    (br_destination, inputs)
}

/// Determine the returned value type of a WebAssembly operator
fn type_of(operator: &Operator) -> Type {
    match operator {
        Operator::V128Load { .. }
        | Operator::V128Store { .. }
        | Operator::V128Const { .. }
        | Operator::V128Not
        | Operator::V128And
        | Operator::V128Or
        | Operator::V128Xor
        | Operator::V128Bitselect => I8X16, // default type representing V128

        Operator::V8x16Shuffle { .. }
        | Operator::I8x16Splat
        | Operator::I8x16ExtractLaneS { .. }
        | Operator::I8x16ExtractLaneU { .. }
        | Operator::I8x16ReplaceLane { .. }
        | Operator::I8x16Eq
        | Operator::I8x16Ne
        | Operator::I8x16LtS
        | Operator::I8x16LtU
        | Operator::I8x16GtS
        | Operator::I8x16GtU
        | Operator::I8x16LeS
        | Operator::I8x16LeU
        | Operator::I8x16GeS
        | Operator::I8x16GeU
        | Operator::I8x16Neg
        | Operator::I8x16AnyTrue
        | Operator::I8x16AllTrue
        | Operator::I8x16Shl
        | Operator::I8x16ShrS
        | Operator::I8x16ShrU
        | Operator::I8x16Add
        | Operator::I8x16AddSaturateS
        | Operator::I8x16AddSaturateU
        | Operator::I8x16Sub
        | Operator::I8x16SubSaturateS
        | Operator::I8x16SubSaturateU
        | Operator::I8x16Mul => I8X16,

        Operator::I16x8Splat
        | Operator::I16x8ExtractLaneS { .. }
        | Operator::I16x8ExtractLaneU { .. }
        | Operator::I16x8ReplaceLane { .. }
        | Operator::I16x8Eq
        | Operator::I16x8Ne
        | Operator::I16x8LtS
        | Operator::I16x8LtU
        | Operator::I16x8GtS
        | Operator::I16x8GtU
        | Operator::I16x8LeS
        | Operator::I16x8LeU
        | Operator::I16x8GeS
        | Operator::I16x8GeU
        | Operator::I16x8Neg
        | Operator::I16x8AnyTrue
        | Operator::I16x8AllTrue
        | Operator::I16x8Shl
        | Operator::I16x8ShrS
        | Operator::I16x8ShrU
        | Operator::I16x8Add
        | Operator::I16x8AddSaturateS
        | Operator::I16x8AddSaturateU
        | Operator::I16x8Sub
        | Operator::I16x8SubSaturateS
        | Operator::I16x8SubSaturateU
        | Operator::I16x8Mul => I16X8,

        Operator::I32x4Splat
        | Operator::I32x4ExtractLane { .. }
        | Operator::I32x4ReplaceLane { .. }
        | Operator::I32x4Eq
        | Operator::I32x4Ne
        | Operator::I32x4LtS
        | Operator::I32x4LtU
        | Operator::I32x4GtS
        | Operator::I32x4GtU
        | Operator::I32x4LeS
        | Operator::I32x4LeU
        | Operator::I32x4GeS
        | Operator::I32x4GeU
        | Operator::I32x4Neg
        | Operator::I32x4AnyTrue
        | Operator::I32x4AllTrue
        | Operator::I32x4Shl
        | Operator::I32x4ShrS
        | Operator::I32x4ShrU
        | Operator::I32x4Add
        | Operator::I32x4Sub
        | Operator::I32x4Mul
        | Operator::F32x4ConvertSI32x4
        | Operator::F32x4ConvertUI32x4 => I32X4,

        Operator::I64x2Splat
        | Operator::I64x2ExtractLane { .. }
        | Operator::I64x2ReplaceLane { .. }
        | Operator::I64x2Neg
        | Operator::I64x2AnyTrue
        | Operator::I64x2AllTrue
        | Operator::I64x2Shl
        | Operator::I64x2ShrS
        | Operator::I64x2ShrU
        | Operator::I64x2Add
        | Operator::I64x2Sub
        | Operator::F64x2ConvertSI64x2
        | Operator::F64x2ConvertUI64x2 => I64X2,

        Operator::F32x4Splat
        | Operator::F32x4ExtractLane { .. }
        | Operator::F32x4ReplaceLane { .. }
        | Operator::F32x4Eq
        | Operator::F32x4Ne
        | Operator::F32x4Lt
        | Operator::F32x4Gt
        | Operator::F32x4Le
        | Operator::F32x4Ge
        | Operator::F32x4Abs
        | Operator::F32x4Neg
        | Operator::F32x4Sqrt
        | Operator::F32x4Add
        | Operator::F32x4Sub
        | Operator::F32x4Mul
        | Operator::F32x4Div
        | Operator::F32x4Min
        | Operator::F32x4Max
        | Operator::I32x4TruncSF32x4Sat
        | Operator::I32x4TruncUF32x4Sat => F32X4,

        Operator::F64x2Splat
        | Operator::F64x2ExtractLane { .. }
        | Operator::F64x2ReplaceLane { .. }
        | Operator::F64x2Eq
        | Operator::F64x2Ne
        | Operator::F64x2Lt
        | Operator::F64x2Gt
        | Operator::F64x2Le
        | Operator::F64x2Ge
        | Operator::F64x2Abs
        | Operator::F64x2Neg
        | Operator::F64x2Sqrt
        | Operator::F64x2Add
        | Operator::F64x2Sub
        | Operator::F64x2Mul
        | Operator::F64x2Div
        | Operator::F64x2Min
        | Operator::F64x2Max
        | Operator::I64x2TruncSF64x2Sat
        | Operator::I64x2TruncUF64x2Sat => F64X2,

        _ => unimplemented!(
            "Currently only the SIMD instructions are translated to their return type: {:?}",
            operator
        ),
    }
}

/// Some SIMD operations only operate on I8X16 in CLIF; this will convert them to that type by
/// adding a raw_bitcast if necessary
fn optionally_bitcast_vector(
    value: Value,
    needed_type: Type,
    builder: &mut FunctionBuilder,
) -> Value {
    if builder.func.dfg.value_type(value) != needed_type {
        builder.ins().raw_bitcast(needed_type, value)
    } else {
        value
    }
}
