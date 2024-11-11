/* Copyright 2017 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//! A simple event-driven library for parsing WebAssembly binary files
//! (or streams).
//!
//! The parser library reports events as they happen and only stores
//! parsing information for a brief period of time, making it very fast
//! and memory-efficient. The event-driven model, however, has some drawbacks.
//! If you need random access to the entire WebAssembly data-structure,
//! this is not the right library for you. You could however, build such
//! a data-structure using this library.
//!
//! To get started, create a [`Parser`] using [`Parser::new`] and then follow
//! the examples documented for [`Parser::parse`] or [`Parser::parse_all`].

#![deny(missing_docs)]
#![no_std]
#![cfg_attr(docsrs, feature(doc_auto_cfg))]

extern crate alloc;
#[cfg(feature = "std")]
#[macro_use]
extern crate std;

/// A small "prelude" to use throughout this crate.
///
/// This crate is tagged with `#![no_std]` meaning that we get libcore's prelude
/// by default. This crate also uses `alloc`, however, and common types there
/// like `String`. This custom prelude helps bring those types into scope to
/// avoid having to import each of them manually.
mod prelude {
    pub use alloc::borrow::ToOwned;
    pub use alloc::boxed::Box;
    pub use alloc::format;
    pub use alloc::string::{String, ToString};
    pub use alloc::vec;
    pub use alloc::vec::Vec;

    #[cfg(all(feature = "validate", feature = "component-model"))]
    pub use crate::collections::IndexSet;
    #[cfg(feature = "validate")]
    pub use crate::collections::{IndexMap, Map, Set};
}

/// A helper macro to conveniently iterate over all opcodes recognized by this
/// crate. This can be used to work with either the [`Operator`] enumeration or
/// the [`VisitOperator`] trait if your use case uniformly handles all operators
/// the same way.
///
/// It is also possible to specialize handling of operators depending on the
/// Wasm proposal from which they are originating.
///
/// This is an "iterator macro" where this macro is invoked with the name of
/// another macro, and then that macro is invoked with the list of all
/// operators. An example invocation of this looks like:
///
/// The list of specializable Wasm proposals is as follows:
///
/// - `@mvp`: Denoting a Wasm operator from the initial Wasm MVP version.
/// - `@exceptions`: [Wasm `exception-handling` proposal]
/// - `@tail_call`: [Wasm `tail-calls` proposal]
/// - `@reference_types`: [Wasm `reference-types` proposal]
/// - `@sign_extension`: [Wasm `sign-extension-ops` proposal]
/// - `@saturating_float_to_int`: [Wasm `non_trapping_float-to-int-conversions` proposal]
/// - `@bulk_memory `:[Wasm `bulk-memory` proposal]
/// - `@threads`: [Wasm `threads` proposal]
/// - `@simd`: [Wasm `simd` proposal]
/// - `@relaxed_simd`: [Wasm `relaxed-simd` proposal]
/// - `@gc`: [Wasm `gc` proposal]
/// - `@stack_switching`: [Wasm `stack-switching` proposal]
/// - `@wide_arithmetic`: [Wasm `wide-arithmetic` proposal]
///
/// [Wasm `exception-handling` proposal]:
/// https://github.com/WebAssembly/exception-handling
///
/// [Wasm `tail-calls` proposal]:
/// https://github.com/WebAssembly/tail-call
///
/// [Wasm `reference-types` proposal]:
/// https://github.com/WebAssembly/reference-types
///
/// [Wasm `sign-extension-ops` proposal]:
/// https://github.com/WebAssembly/sign-extension-ops
///
/// [Wasm `non_trapping_float-to-int-conversions` proposal]:
/// https://github.com/WebAssembly/nontrapping-float-to-int-conversions
///
/// [Wasm `bulk-memory` proposal]:
/// https://github.com/WebAssembly/bulk-memory-operations
///
/// [Wasm `threads` proposal]:
/// https://github.com/webassembly/threads
///
/// [Wasm `simd` proposal]:
/// https://github.com/webassembly/simd
///
/// [Wasm `relaxed-simd` proposal]:
/// https://github.com/WebAssembly/relaxed-simd
///
/// [Wasm `gc` proposal]:
/// https://github.com/WebAssembly/gc
///
/// [Wasm `stack-switching` proposal]:
/// https://github.com/WebAssembly/stack-switching
///
/// [Wasm `wide-arithmetic` proposal]:
/// https://github.com/WebAssembly/wide-arithmetic
///
/// ```
/// macro_rules! define_visit_operator {
///     // The outer layer of repetition represents how all operators are
///     // provided to the macro at the same time.
///     //
///     // The `$proposal` identifier indicates the Wasm proposals from which
///     // the Wasm operator is originating.
///     // For example to specialize the macro match arm for Wasm SIMD proposal
///     // operators you could write `@simd` instead of `@$proposal:ident` to
///     // only catch those operators.
///     //
///     // The `$op` name is bound to the `Operator` variant name. The
///     // payload of the operator is optionally specified (the `$(...)?`
///     // clause) since not all instructions have payloads. Within the payload
///     // each argument is named and has its type specified.
///     //
///     // The `$visit` name is bound to the corresponding name in the
///     // `VisitOperator` trait that this corresponds to.
///     //
///     // The `$ann` annotations give information about the operator's type (e.g. binary i32 or arity 2 -> 1).
///     ($( @$proposal:ident $op:ident $({ $($arg:ident: $argty:ty),* })? => $visit:ident ($($ann:tt)*))*) => {
///         $(
///             fn $visit(&mut self $($(,$arg: $argty)*)?) {
///                 // do nothing for this example
///             }
///         )*
///     }
/// }
///
/// pub struct VisitAndDoNothing;
///
/// impl<'a> wasmparser::VisitOperator<'a> for VisitAndDoNothing {
///     type Output = ();
///
///     wasmparser::for_each_operator!(define_visit_operator);
/// }
/// ```
///
/// If you only wanted to visit the initial base set of wasm instructions, for
/// example, you could do:
///
/// ```
/// macro_rules! visit_only_mvp {
///     // delegate the macro invocation to sub-invocations of this macro to
///     // deal with each instruction on a case-by-case basis.
///     ($( @$proposal:ident $op:ident $({ $($arg:ident: $argty:ty),* })? => $visit:ident ($($ann:tt)*))*) => {
///         $(
///             visit_only_mvp!(visit_one @$proposal $op $({ $($arg: $argty),* })? => $visit);
///         )*
///     };
///
///     // MVP instructions are defined manually, so do nothing.
///     (visit_one @mvp $($rest:tt)*) => {};
///
///     // Non-MVP instructions all return `false` here. The exact type depends
///     // on `type Output` in the trait implementation below. You could change
///     // it to `Result<()>` for example and return an error here too.
///     (visit_one @$proposal:ident $op:ident $({ $($arg:ident: $argty:ty),* })? => $visit:ident) => {
///         fn $visit(&mut self $($(,$arg: $argty)*)?) -> bool {
///             false
///         }
///     }
/// }
/// # // to get this example to compile another macro is used here to define
/// # // visit methods for all mvp oeprators.
/// # macro_rules! visit_mvp {
/// #     ($( @$proposal:ident $op:ident $({ $($arg:ident: $argty:ty),* })? => $visit:ident ($($ann:tt)*))*) => {
/// #         $(
/// #             visit_mvp!(visit_one @$proposal $op $({ $($arg: $argty),* })? => $visit);
/// #         )*
/// #     };
/// #     (visit_one @mvp $op:ident $({ $($arg:ident: $argty:ty),* })? => $visit:ident) => {
/// #         fn $visit(&mut self $($(,$arg: $argty)*)?) -> bool {
/// #             true
/// #         }
/// #     };
/// #     (visit_one @$proposal:ident $($rest:tt)*) => {};
/// # }
///
/// pub struct VisitOnlyMvp;
///
/// impl<'a> wasmparser::VisitOperator<'a> for VisitOnlyMvp {
///     type Output = bool;
///
///     wasmparser::for_each_operator!(visit_only_mvp);
/// #   wasmparser::for_each_operator!(visit_mvp);
///
///     // manually define `visit_*` for all MVP operators here
/// }
/// ```
#[macro_export]
macro_rules! for_each_operator {
    ($mac:ident) => {
        $mac! {
            @mvp Unreachable => visit_unreachable (arity 0 -> 0)
            @mvp Nop => visit_nop (arity 0 -> 0)
            @mvp Block { blockty: $crate::BlockType } => visit_block (arity block -> ~block)
            @mvp Loop { blockty: $crate::BlockType } => visit_loop (arity block -> ~block)
            @mvp If { blockty: $crate::BlockType } => visit_if (arity 1 block -> ~block)
            @mvp Else => visit_else (arity ~end -> ~end)
            @exceptions TryTable { try_table: $crate::TryTable } => visit_try_table (arity try_table -> ~try_table)
            @exceptions Throw { tag_index: u32 } => visit_throw (arity tag -> 0)
            @exceptions ThrowRef => visit_throw_ref (arity 1 -> 0)
            // Deprecated old instructions from the exceptions proposal
            @legacy_exceptions Try { blockty: $crate::BlockType } => visit_try (arity block -> ~block)
            @legacy_exceptions Catch { tag_index: u32 } => visit_catch (arity ~end -> ~tag)
            @legacy_exceptions Rethrow { relative_depth: u32 } => visit_rethrow (arity 0 -> 0)
            @legacy_exceptions Delegate { relative_depth: u32 } => visit_delegate (arity ~end -> end)
            @legacy_exceptions CatchAll => visit_catch_all (arity ~end -> 0)
            @mvp End => visit_end (arity implicit_else ~end -> implicit_else end)
            @mvp Br { relative_depth: u32 } => visit_br (arity br -> 0)
            @mvp BrIf { relative_depth: u32 } => visit_br_if (arity 1 br -> br)
            @mvp BrTable { targets: $crate::BrTable<'a> } => visit_br_table (arity 1 br_table -> 0)
            @mvp Return => visit_return (arity ~ret -> 0)
            @mvp Call { function_index: u32 } => visit_call (arity func -> func)
            @mvp CallIndirect { type_index: u32, table_index: u32 } => visit_call_indirect (arity 1 type -> type)
            @tail_call ReturnCall { function_index: u32 } => visit_return_call (arity func -> 0)
            @tail_call ReturnCallIndirect { type_index: u32, table_index: u32 } => visit_return_call_indirect (arity 1 type -> 0)
            @mvp Drop => visit_drop (arity 1 -> 0)
            @mvp Select => visit_select (arity 3 -> 1)
            @reference_types TypedSelect { ty: $crate::ValType } => visit_typed_select (arity 3 -> 1)
            @mvp LocalGet { local_index: u32 } => visit_local_get (arity 0 -> 1)
            @mvp LocalSet { local_index: u32 } => visit_local_set (arity 1 -> 0)
            @mvp LocalTee { local_index: u32 } => visit_local_tee (arity 1 -> 1)
            @mvp GlobalGet { global_index: u32 } => visit_global_get (arity 0 -> 1)
            @mvp GlobalSet { global_index: u32 } => visit_global_set (arity 1 -> 0)
            @mvp I32Load { memarg: $crate::MemArg } => visit_i32_load (load i32)
            @mvp I64Load { memarg: $crate::MemArg } => visit_i64_load (load i64)
            @mvp F32Load { memarg: $crate::MemArg } => visit_f32_load (load f32)
            @mvp F64Load { memarg: $crate::MemArg } => visit_f64_load (load f64)
            @mvp I32Load8S { memarg: $crate::MemArg } => visit_i32_load8_s (load i32)
            @mvp I32Load8U { memarg: $crate::MemArg } => visit_i32_load8_u (load i32)
            @mvp I32Load16S { memarg: $crate::MemArg } => visit_i32_load16_s (load i32)
            @mvp I32Load16U { memarg: $crate::MemArg } => visit_i32_load16_u (load i32)
            @mvp I64Load8S { memarg: $crate::MemArg } => visit_i64_load8_s (load i64)
            @mvp I64Load8U { memarg: $crate::MemArg } => visit_i64_load8_u (load i64)
            @mvp I64Load16S { memarg: $crate::MemArg } => visit_i64_load16_s (load i64)
            @mvp I64Load16U { memarg: $crate::MemArg } => visit_i64_load16_u (load i64)
            @mvp I64Load32S { memarg: $crate::MemArg } => visit_i64_load32_s (load i64)
            @mvp I64Load32U { memarg: $crate::MemArg } => visit_i64_load32_u (load i64)
            @mvp I32Store { memarg: $crate::MemArg } => visit_i32_store (store i32)
            @mvp I64Store { memarg: $crate::MemArg } => visit_i64_store (store i64)
            @mvp F32Store { memarg: $crate::MemArg } => visit_f32_store (store f32)
            @mvp F64Store { memarg: $crate::MemArg } => visit_f64_store (store f64)
            @mvp I32Store8 { memarg: $crate::MemArg } => visit_i32_store8 (store i32)
            @mvp I32Store16 { memarg: $crate::MemArg } => visit_i32_store16 (store i32)
            @mvp I64Store8 { memarg: $crate::MemArg } => visit_i64_store8 (store i64)
            @mvp I64Store16 { memarg: $crate::MemArg } => visit_i64_store16 (store i64)
            @mvp I64Store32 { memarg: $crate::MemArg } => visit_i64_store32 (store i64)
            @mvp MemorySize { mem: u32 } => visit_memory_size (arity 0 -> 1)
            @mvp MemoryGrow { mem: u32 } => visit_memory_grow (arity 1 -> 1)
            @mvp I32Const { value: i32 } => visit_i32_const (push i32)
            @mvp I64Const { value: i64 } => visit_i64_const (push i64)
            @mvp F32Const { value: $crate::Ieee32 } => visit_f32_const (push f32)
            @mvp F64Const { value: $crate::Ieee64 } => visit_f64_const (push f64)
            @reference_types RefNull { hty: $crate::HeapType } => visit_ref_null (arity 0 -> 1)
            @reference_types RefIsNull => visit_ref_is_null (arity 1 -> 1)
            @reference_types RefFunc { function_index: u32 } => visit_ref_func (arity 0 -> 1)
            @gc RefEq => visit_ref_eq (arity 2 -> 1)
            @mvp I32Eqz => visit_i32_eqz (test i32)
            @mvp I32Eq => visit_i32_eq (cmp i32)
            @mvp I32Ne => visit_i32_ne (cmp i32)
            @mvp I32LtS => visit_i32_lt_s (cmp i32)
            @mvp I32LtU => visit_i32_lt_u (cmp i32)
            @mvp I32GtS => visit_i32_gt_s (cmp i32)
            @mvp I32GtU => visit_i32_gt_u (cmp i32)
            @mvp I32LeS => visit_i32_le_s (cmp i32)
            @mvp I32LeU => visit_i32_le_u (cmp i32)
            @mvp I32GeS => visit_i32_ge_s (cmp i32)
            @mvp I32GeU => visit_i32_ge_u (cmp i32)
            @mvp I64Eqz => visit_i64_eqz (test i64)
            @mvp I64Eq => visit_i64_eq (cmp i64)
            @mvp I64Ne => visit_i64_ne (cmp i64)
            @mvp I64LtS => visit_i64_lt_s (cmp i64)
            @mvp I64LtU => visit_i64_lt_u (cmp i64)
            @mvp I64GtS => visit_i64_gt_s (cmp i64)
            @mvp I64GtU => visit_i64_gt_u (cmp i64)
            @mvp I64LeS => visit_i64_le_s (cmp i64)
            @mvp I64LeU => visit_i64_le_u (cmp i64)
            @mvp I64GeS => visit_i64_ge_s (cmp i64)
            @mvp I64GeU => visit_i64_ge_u (cmp i64)
            @mvp F32Eq => visit_f32_eq (cmp f32)
            @mvp F32Ne => visit_f32_ne (cmp f32)
            @mvp F32Lt => visit_f32_lt (cmp f32)
            @mvp F32Gt => visit_f32_gt (cmp f32)
            @mvp F32Le => visit_f32_le (cmp f32)
            @mvp F32Ge => visit_f32_ge (cmp f32)
            @mvp F64Eq => visit_f64_eq (cmp f64)
            @mvp F64Ne => visit_f64_ne (cmp f64)
            @mvp F64Lt => visit_f64_lt (cmp f64)
            @mvp F64Gt => visit_f64_gt (cmp f64)
            @mvp F64Le => visit_f64_le (cmp f64)
            @mvp F64Ge => visit_f64_ge (cmp f64)
            @mvp I32Clz => visit_i32_clz (unary i32)
            @mvp I32Ctz => visit_i32_ctz (unary i32)
            @mvp I32Popcnt => visit_i32_popcnt (unary i32)
            @mvp I32Add => visit_i32_add (binary i32)
            @mvp I32Sub => visit_i32_sub (binary i32)
            @mvp I32Mul => visit_i32_mul (binary i32)
            @mvp I32DivS => visit_i32_div_s (binary i32)
            @mvp I32DivU => visit_i32_div_u (binary i32)
            @mvp I32RemS => visit_i32_rem_s (binary i32)
            @mvp I32RemU => visit_i32_rem_u (binary i32)
            @mvp I32And => visit_i32_and (binary i32)
            @mvp I32Or => visit_i32_or (binary i32)
            @mvp I32Xor => visit_i32_xor (binary i32)
            @mvp I32Shl => visit_i32_shl (binary i32)
            @mvp I32ShrS => visit_i32_shr_s (binary i32)
            @mvp I32ShrU => visit_i32_shr_u (binary i32)
            @mvp I32Rotl => visit_i32_rotl (binary i32)
            @mvp I32Rotr => visit_i32_rotr (binary i32)
            @mvp I64Clz => visit_i64_clz (unary i64)
            @mvp I64Ctz => visit_i64_ctz (unary i64)
            @mvp I64Popcnt => visit_i64_popcnt (unary i64)
            @mvp I64Add => visit_i64_add (binary i64)
            @mvp I64Sub => visit_i64_sub (binary i64)
            @mvp I64Mul => visit_i64_mul (binary i64)
            @mvp I64DivS => visit_i64_div_s (binary i64)
            @mvp I64DivU => visit_i64_div_u (binary i64)
            @mvp I64RemS => visit_i64_rem_s (binary i64)
            @mvp I64RemU => visit_i64_rem_u (binary i64)
            @mvp I64And => visit_i64_and (binary i64)
            @mvp I64Or => visit_i64_or (binary i64)
            @mvp I64Xor => visit_i64_xor (binary i64)
            @mvp I64Shl => visit_i64_shl (binary i64)
            @mvp I64ShrS => visit_i64_shr_s (binary i64)
            @mvp I64ShrU => visit_i64_shr_u (binary i64)
            @mvp I64Rotl => visit_i64_rotl (binary i64)
            @mvp I64Rotr => visit_i64_rotr (binary i64)
            @mvp F32Abs => visit_f32_abs (unary f32)
            @mvp F32Neg => visit_f32_neg (unary f32)
            @mvp F32Ceil => visit_f32_ceil (unary f32)
            @mvp F32Floor => visit_f32_floor (unary f32)
            @mvp F32Trunc => visit_f32_trunc (unary f32)
            @mvp F32Nearest => visit_f32_nearest (unary f32)
            @mvp F32Sqrt => visit_f32_sqrt (unary f32)
            @mvp F32Add => visit_f32_add (binary f32)
            @mvp F32Sub => visit_f32_sub (binary f32)
            @mvp F32Mul => visit_f32_mul (binary f32)
            @mvp F32Div => visit_f32_div (binary f32)
            @mvp F32Min => visit_f32_min (binary f32)
            @mvp F32Max => visit_f32_max (binary f32)
            @mvp F32Copysign => visit_f32_copysign (binary f32)
            @mvp F64Abs => visit_f64_abs (unary f64)
            @mvp F64Neg => visit_f64_neg (unary f64)
            @mvp F64Ceil => visit_f64_ceil (unary f64)
            @mvp F64Floor => visit_f64_floor (unary f64)
            @mvp F64Trunc => visit_f64_trunc (unary f64)
            @mvp F64Nearest => visit_f64_nearest (unary f64)
            @mvp F64Sqrt => visit_f64_sqrt (unary f64)
            @mvp F64Add => visit_f64_add (binary f64)
            @mvp F64Sub => visit_f64_sub (binary f64)
            @mvp F64Mul => visit_f64_mul (binary f64)
            @mvp F64Div => visit_f64_div (binary f64)
            @mvp F64Min => visit_f64_min (binary f64)
            @mvp F64Max => visit_f64_max (binary f64)
            @mvp F64Copysign => visit_f64_copysign (binary f64)
            @mvp I32WrapI64 => visit_i32_wrap_i64 (conversion i32 i64)
            @mvp I32TruncF32S => visit_i32_trunc_f32_s (conversion i32 f32)
            @mvp I32TruncF32U => visit_i32_trunc_f32_u (conversion i32 f32)
            @mvp I32TruncF64S => visit_i32_trunc_f64_s (conversion i32 f64)
            @mvp I32TruncF64U => visit_i32_trunc_f64_u (conversion i32 f64)
            @mvp I64ExtendI32S => visit_i64_extend_i32_s (conversion i64 i32)
            @mvp I64ExtendI32U => visit_i64_extend_i32_u (conversion i64 i32)
            @mvp I64TruncF32S => visit_i64_trunc_f32_s (conversion i64 f32)
            @mvp I64TruncF32U => visit_i64_trunc_f32_u (conversion i64 f32)
            @mvp I64TruncF64S => visit_i64_trunc_f64_s (conversion i64 f64)
            @mvp I64TruncF64U => visit_i64_trunc_f64_u (conversion i64 f64)
            @mvp F32ConvertI32S => visit_f32_convert_i32_s (conversion f32 i32)
            @mvp F32ConvertI32U => visit_f32_convert_i32_u (conversion f32 i32)
            @mvp F32ConvertI64S => visit_f32_convert_i64_s (conversion f32 i64)
            @mvp F32ConvertI64U => visit_f32_convert_i64_u (conversion f32 i64)
            @mvp F32DemoteF64 => visit_f32_demote_f64 (conversion f32 f64)
            @mvp F64ConvertI32S => visit_f64_convert_i32_s (conversion f64 i32)
            @mvp F64ConvertI32U => visit_f64_convert_i32_u (conversion f64 i32)
            @mvp F64ConvertI64S => visit_f64_convert_i64_s (conversion f64 i64)
            @mvp F64ConvertI64U => visit_f64_convert_i64_u (conversion f64 i64)
            @mvp F64PromoteF32 => visit_f64_promote_f32 (conversion f64 f32)
            @mvp I32ReinterpretF32 => visit_i32_reinterpret_f32 (conversion i32 f32)
            @mvp I64ReinterpretF64 => visit_i64_reinterpret_f64 (conversion i64 f64)
            @mvp F32ReinterpretI32 => visit_f32_reinterpret_i32 (conversion f32 i32)
            @mvp F64ReinterpretI64 => visit_f64_reinterpret_i64 (conversion f64 i64)
            @sign_extension I32Extend8S => visit_i32_extend8_s (unary i32)
            @sign_extension I32Extend16S => visit_i32_extend16_s (unary i32)
            @sign_extension I64Extend8S => visit_i64_extend8_s (unary i64)
            @sign_extension I64Extend16S => visit_i64_extend16_s (unary i64)
            @sign_extension I64Extend32S => visit_i64_extend32_s (unary i64)

            // 0xFB prefixed operators
            // Garbage Collection
            // http://github.com/WebAssembly/gc
            @gc StructNew { struct_type_index: u32 } => visit_struct_new (arity type -> 1)
            @gc StructNewDefault { struct_type_index: u32 } => visit_struct_new_default (arity 0 -> 1)
            @gc StructGet { struct_type_index: u32, field_index: u32 } => visit_struct_get (arity 1 -> 1)
            @gc StructGetS { struct_type_index: u32, field_index: u32 } => visit_struct_get_s (arity 1 -> 1)
            @gc StructGetU { struct_type_index: u32, field_index: u32 } => visit_struct_get_u (arity 1 -> 1)
                @gc StructSet { struct_type_index: u32, field_index: u32 } => visit_struct_set (arity 2 -> 0)
            @gc ArrayNew { array_type_index: u32 } => visit_array_new (arity 2 -> 1)
            @gc ArrayNewDefault { array_type_index: u32 } => visit_array_new_default (arity 1 -> 1)
            @gc ArrayNewFixed { array_type_index: u32, array_size: u32 } => visit_array_new_fixed (arity size -> 1)
            @gc ArrayNewData { array_type_index: u32, array_data_index: u32 } => visit_array_new_data (arity 2 -> 1)
            @gc ArrayNewElem { array_type_index: u32, array_elem_index: u32 } => visit_array_new_elem (arity 2 -> 1)
            @gc ArrayGet { array_type_index: u32 } => visit_array_get (arity 2 -> 1)
            @gc ArrayGetS { array_type_index: u32 } => visit_array_get_s (arity 2 -> 1)
            @gc ArrayGetU { array_type_index: u32 } => visit_array_get_u (arity 2 -> 1)
            @gc ArraySet { array_type_index: u32 } => visit_array_set (arity 3 -> 0)
            @gc ArrayLen => visit_array_len (arity 1 -> 1)
            @gc ArrayFill { array_type_index: u32 } => visit_array_fill (arity 4 -> 0)
            @gc ArrayCopy { array_type_index_dst: u32, array_type_index_src: u32 } => visit_array_copy (arity 5 -> 0)
            @gc ArrayInitData { array_type_index: u32, array_data_index: u32 } => visit_array_init_data (arity 4 -> 0)
            @gc ArrayInitElem { array_type_index: u32, array_elem_index: u32 } => visit_array_init_elem (arity 4 -> 0)
            @gc RefTestNonNull { hty: $crate::HeapType } => visit_ref_test_non_null (arity 1 -> 1)
            @gc RefTestNullable { hty: $crate::HeapType } => visit_ref_test_nullable (arity 1 -> 1)
            @gc RefCastNonNull { hty: $crate::HeapType } => visit_ref_cast_non_null (arity 1 -> 1)
            @gc RefCastNullable { hty: $crate::HeapType } => visit_ref_cast_nullable (arity 1 -> 1)
            @gc BrOnCast {
                relative_depth: u32,
                from_ref_type: $crate::RefType,
                to_ref_type: $crate::RefType
            } => visit_br_on_cast (arity br -> br)
            @gc BrOnCastFail {
                relative_depth: u32,
                from_ref_type: $crate::RefType,
                to_ref_type: $crate::RefType
            } => visit_br_on_cast_fail (arity br -> br)
            @gc AnyConvertExtern => visit_any_convert_extern  (arity 1 -> 1)
            @gc ExternConvertAny => visit_extern_convert_any (arity 1 -> 1)
            @gc RefI31 => visit_ref_i31 (arity 1 -> 1)
            @gc I31GetS => visit_i31_get_s (arity 1 -> 1)
            @gc I31GetU => visit_i31_get_u (arity 1 -> 1)

            // 0xFC operators
            // Non-trapping Float-to-int Conversions
            // https://github.com/WebAssembly/nontrapping-float-to-int-conversions
            @saturating_float_to_int I32TruncSatF32S => visit_i32_trunc_sat_f32_s (conversion i32 f32)
            @saturating_float_to_int I32TruncSatF32U => visit_i32_trunc_sat_f32_u (conversion i32 f32)
            @saturating_float_to_int I32TruncSatF64S => visit_i32_trunc_sat_f64_s (conversion i32 f64)
            @saturating_float_to_int I32TruncSatF64U => visit_i32_trunc_sat_f64_u (conversion i32 f64)
            @saturating_float_to_int I64TruncSatF32S => visit_i64_trunc_sat_f32_s (conversion i64 f32)
            @saturating_float_to_int I64TruncSatF32U => visit_i64_trunc_sat_f32_u (conversion i64 f32)
            @saturating_float_to_int I64TruncSatF64S => visit_i64_trunc_sat_f64_s (conversion i64 f64)
            @saturating_float_to_int I64TruncSatF64U => visit_i64_trunc_sat_f64_u (conversion i64 f64)

            // 0xFC prefixed operators
            // bulk memory operations
            // https://github.com/WebAssembly/bulk-memory-operations
            @bulk_memory MemoryInit { data_index: u32, mem: u32 } => visit_memory_init (arity 3 -> 0)
            @bulk_memory DataDrop { data_index: u32 } => visit_data_drop (arity 0 -> 0)
            @bulk_memory MemoryCopy { dst_mem: u32, src_mem: u32 } => visit_memory_copy (arity 3 -> 0)
            @bulk_memory MemoryFill { mem: u32 } => visit_memory_fill (arity 3 -> 0)
            @bulk_memory TableInit { elem_index: u32, table: u32 } => visit_table_init (arity 3 -> 0)
            @bulk_memory ElemDrop { elem_index: u32 } => visit_elem_drop (arity 0 -> 0)
            @bulk_memory TableCopy { dst_table: u32, src_table: u32 } => visit_table_copy (arity 3 -> 0)

            // 0xFC prefixed operators
            // reference-types
            // https://github.com/WebAssembly/reference-types
            @reference_types TableFill { table: u32 } => visit_table_fill (arity 3 -> 0)
            @reference_types TableGet { table: u32 } => visit_table_get (arity 1 -> 1)
            @reference_types TableSet { table: u32 } => visit_table_set (arity 2 -> 0)
            @reference_types TableGrow { table: u32 } => visit_table_grow (arity 2 -> 1)
            @reference_types TableSize { table: u32 } => visit_table_size (arity 0 -> 1)

            // OxFC prefixed operators
            // memory control (experimental)
            // https://github.com/WebAssembly/design/issues/1439
            @memory_control MemoryDiscard { mem: u32 } => visit_memory_discard (arity 2 -> 0)

            // 0xFE prefixed operators
            // threads
            // https://github.com/WebAssembly/threads
            @threads MemoryAtomicNotify { memarg: $crate::MemArg } => visit_memory_atomic_notify (atomic rmw i32)
            @threads MemoryAtomicWait32 { memarg: $crate::MemArg } => visit_memory_atomic_wait32 (arity 3 -> 1)
            @threads MemoryAtomicWait64 { memarg: $crate::MemArg } => visit_memory_atomic_wait64 (arity 3 -> 1)
            @threads AtomicFence => visit_atomic_fence (arity 0 -> 0)
            @threads I32AtomicLoad { memarg: $crate::MemArg } => visit_i32_atomic_load (load atomic i32)
            @threads I64AtomicLoad { memarg: $crate::MemArg } => visit_i64_atomic_load (load atomic i64)
            @threads I32AtomicLoad8U { memarg: $crate::MemArg } => visit_i32_atomic_load8_u (load atomic i32)
            @threads I32AtomicLoad16U { memarg: $crate::MemArg } => visit_i32_atomic_load16_u (load atomic i32)
            @threads I64AtomicLoad8U { memarg: $crate::MemArg } => visit_i64_atomic_load8_u (load atomic i64)
            @threads I64AtomicLoad16U { memarg: $crate::MemArg } => visit_i64_atomic_load16_u (load atomic i64)
            @threads I64AtomicLoad32U { memarg: $crate::MemArg } => visit_i64_atomic_load32_u (load atomic i64)
            @threads I32AtomicStore { memarg: $crate::MemArg } => visit_i32_atomic_store (store atomic i32)
            @threads I64AtomicStore { memarg: $crate::MemArg } => visit_i64_atomic_store (store atomic i64)
            @threads I32AtomicStore8 { memarg: $crate::MemArg } => visit_i32_atomic_store8 (store atomic i32)
            @threads I32AtomicStore16 { memarg: $crate::MemArg } => visit_i32_atomic_store16 (store atomic i32)
            @threads I64AtomicStore8 { memarg: $crate::MemArg } => visit_i64_atomic_store8 (store atomic i64)
            @threads I64AtomicStore16 { memarg: $crate::MemArg } => visit_i64_atomic_store16 (store atomic i64)
            @threads I64AtomicStore32 { memarg: $crate::MemArg } => visit_i64_atomic_store32 (store atomic i64)
            @threads I32AtomicRmwAdd { memarg: $crate::MemArg } => visit_i32_atomic_rmw_add (atomic rmw i32)
            @threads I64AtomicRmwAdd { memarg: $crate::MemArg } => visit_i64_atomic_rmw_add (atomic rmw i64)
            @threads I32AtomicRmw8AddU { memarg: $crate::MemArg } => visit_i32_atomic_rmw8_add_u (atomic rmw i32)
            @threads I32AtomicRmw16AddU { memarg: $crate::MemArg } => visit_i32_atomic_rmw16_add_u (atomic rmw i32)
            @threads I64AtomicRmw8AddU { memarg: $crate::MemArg } => visit_i64_atomic_rmw8_add_u (atomic rmw i64)
            @threads I64AtomicRmw16AddU { memarg: $crate::MemArg } => visit_i64_atomic_rmw16_add_u (atomic rmw i64)
            @threads I64AtomicRmw32AddU { memarg: $crate::MemArg } => visit_i64_atomic_rmw32_add_u (atomic rmw i64)
            @threads I32AtomicRmwSub { memarg: $crate::MemArg } => visit_i32_atomic_rmw_sub (atomic rmw i32)
            @threads I64AtomicRmwSub { memarg: $crate::MemArg } => visit_i64_atomic_rmw_sub (atomic rmw i64)
            @threads I32AtomicRmw8SubU { memarg: $crate::MemArg } => visit_i32_atomic_rmw8_sub_u (atomic rmw i32)
            @threads I32AtomicRmw16SubU { memarg: $crate::MemArg } => visit_i32_atomic_rmw16_sub_u (atomic rmw i32)
            @threads I64AtomicRmw8SubU { memarg: $crate::MemArg } => visit_i64_atomic_rmw8_sub_u (atomic rmw i64)
            @threads I64AtomicRmw16SubU { memarg: $crate::MemArg } => visit_i64_atomic_rmw16_sub_u (atomic rmw i64)
            @threads I64AtomicRmw32SubU { memarg: $crate::MemArg } => visit_i64_atomic_rmw32_sub_u (atomic rmw i64)
            @threads I32AtomicRmwAnd { memarg: $crate::MemArg } => visit_i32_atomic_rmw_and (atomic rmw i32)
            @threads I64AtomicRmwAnd { memarg: $crate::MemArg } => visit_i64_atomic_rmw_and (atomic rmw i64)
            @threads I32AtomicRmw8AndU { memarg: $crate::MemArg } => visit_i32_atomic_rmw8_and_u (atomic rmw i32)
            @threads I32AtomicRmw16AndU { memarg: $crate::MemArg } => visit_i32_atomic_rmw16_and_u (atomic rmw i32)
            @threads I64AtomicRmw8AndU { memarg: $crate::MemArg } => visit_i64_atomic_rmw8_and_u (atomic rmw i64)
            @threads I64AtomicRmw16AndU { memarg: $crate::MemArg } => visit_i64_atomic_rmw16_and_u (atomic rmw i64)
            @threads I64AtomicRmw32AndU { memarg: $crate::MemArg } => visit_i64_atomic_rmw32_and_u (atomic rmw i64)
            @threads I32AtomicRmwOr { memarg: $crate::MemArg } => visit_i32_atomic_rmw_or (atomic rmw i32)
            @threads I64AtomicRmwOr { memarg: $crate::MemArg } => visit_i64_atomic_rmw_or (atomic rmw i64)
            @threads I32AtomicRmw8OrU { memarg: $crate::MemArg } => visit_i32_atomic_rmw8_or_u (atomic rmw i32)
            @threads I32AtomicRmw16OrU { memarg: $crate::MemArg } => visit_i32_atomic_rmw16_or_u (atomic rmw i32)
            @threads I64AtomicRmw8OrU { memarg: $crate::MemArg } => visit_i64_atomic_rmw8_or_u (atomic rmw i64)
            @threads I64AtomicRmw16OrU { memarg: $crate::MemArg } => visit_i64_atomic_rmw16_or_u (atomic rmw i64)
            @threads I64AtomicRmw32OrU { memarg: $crate::MemArg } => visit_i64_atomic_rmw32_or_u (atomic rmw i64)
            @threads I32AtomicRmwXor { memarg: $crate::MemArg } => visit_i32_atomic_rmw_xor (atomic rmw i32)
            @threads I64AtomicRmwXor { memarg: $crate::MemArg } => visit_i64_atomic_rmw_xor (atomic rmw i64)
            @threads I32AtomicRmw8XorU { memarg: $crate::MemArg } => visit_i32_atomic_rmw8_xor_u (atomic rmw i32)
            @threads I32AtomicRmw16XorU { memarg: $crate::MemArg } => visit_i32_atomic_rmw16_xor_u (atomic rmw i32)
            @threads I64AtomicRmw8XorU { memarg: $crate::MemArg } => visit_i64_atomic_rmw8_xor_u (atomic rmw i64)
            @threads I64AtomicRmw16XorU { memarg: $crate::MemArg } => visit_i64_atomic_rmw16_xor_u (atomic rmw i64)
            @threads I64AtomicRmw32XorU { memarg: $crate::MemArg } => visit_i64_atomic_rmw32_xor_u (atomic rmw i64)
            @threads I32AtomicRmwXchg { memarg: $crate::MemArg } => visit_i32_atomic_rmw_xchg (atomic rmw i32)
            @threads I64AtomicRmwXchg { memarg: $crate::MemArg } => visit_i64_atomic_rmw_xchg (atomic rmw i64)
            @threads I32AtomicRmw8XchgU { memarg: $crate::MemArg } => visit_i32_atomic_rmw8_xchg_u (atomic rmw i32)
            @threads I32AtomicRmw16XchgU { memarg: $crate::MemArg } => visit_i32_atomic_rmw16_xchg_u (atomic rmw i32)
            @threads I64AtomicRmw8XchgU { memarg: $crate::MemArg } => visit_i64_atomic_rmw8_xchg_u (atomic rmw i64)
            @threads I64AtomicRmw16XchgU { memarg: $crate::MemArg } => visit_i64_atomic_rmw16_xchg_u (atomic rmw i64)
            @threads I64AtomicRmw32XchgU { memarg: $crate::MemArg } => visit_i64_atomic_rmw32_xchg_u (atomic rmw i64)
            @threads I32AtomicRmwCmpxchg { memarg: $crate::MemArg } => visit_i32_atomic_rmw_cmpxchg (atomic cmpxchg i32)
            @threads I64AtomicRmwCmpxchg { memarg: $crate::MemArg } => visit_i64_atomic_rmw_cmpxchg (atomic cmpxchg i64)
            @threads I32AtomicRmw8CmpxchgU { memarg: $crate::MemArg } => visit_i32_atomic_rmw8_cmpxchg_u (atomic cmpxchg i32)
            @threads I32AtomicRmw16CmpxchgU { memarg: $crate::MemArg } => visit_i32_atomic_rmw16_cmpxchg_u (atomic cmpxchg i32)
            @threads I64AtomicRmw8CmpxchgU { memarg: $crate::MemArg } => visit_i64_atomic_rmw8_cmpxchg_u (atomic cmpxchg i64)
            @threads I64AtomicRmw16CmpxchgU { memarg: $crate::MemArg } => visit_i64_atomic_rmw16_cmpxchg_u (atomic cmpxchg i64)
            @threads I64AtomicRmw32CmpxchgU { memarg: $crate::MemArg } => visit_i64_atomic_rmw32_cmpxchg_u (atomic cmpxchg i64)

            // Also 0xFE prefixed operators
            // shared-everything threads
            // https://github.com/WebAssembly/shared-everything-threads
            @shared_everything_threads GlobalAtomicGet { ordering: $crate::Ordering, global_index: u32 } => visit_global_atomic_get (arity 0 -> 1)
            @shared_everything_threads GlobalAtomicSet { ordering: $crate::Ordering, global_index: u32 } => visit_global_atomic_set (arity 1 -> 0)
            @shared_everything_threads GlobalAtomicRmwAdd { ordering: $crate::Ordering, global_index: u32 } => visit_global_atomic_rmw_add (unary atomic global)
            @shared_everything_threads GlobalAtomicRmwSub { ordering: $crate::Ordering, global_index: u32 } => visit_global_atomic_rmw_sub (unary atomic global)
            @shared_everything_threads GlobalAtomicRmwAnd { ordering: $crate::Ordering, global_index: u32 } => visit_global_atomic_rmw_and (unary atomic global)
            @shared_everything_threads GlobalAtomicRmwOr { ordering: $crate::Ordering, global_index: u32 } => visit_global_atomic_rmw_or (unary atomic global)
            @shared_everything_threads GlobalAtomicRmwXor { ordering: $crate::Ordering, global_index: u32 } => visit_global_atomic_rmw_xor (unary atomic global)
            @shared_everything_threads GlobalAtomicRmwXchg { ordering: $crate::Ordering, global_index: u32 } => visit_global_atomic_rmw_xchg (arity 1 -> 1)
            @shared_everything_threads GlobalAtomicRmwCmpxchg { ordering: $crate::Ordering, global_index: u32 } => visit_global_atomic_rmw_cmpxchg (arity 2 -> 1)
            @shared_everything_threads TableAtomicGet { ordering: $crate::Ordering, table_index: u32 } => visit_table_atomic_get (arity 1 -> 1)
            @shared_everything_threads TableAtomicSet { ordering: $crate::Ordering, table_index: u32 } => visit_table_atomic_set (arity 2 -> 0)
            @shared_everything_threads TableAtomicRmwXchg { ordering: $crate::Ordering, table_index: u32 } => visit_table_atomic_rmw_xchg (arity 2 -> 1)
            @shared_everything_threads TableAtomicRmwCmpxchg { ordering: $crate::Ordering, table_index: u32 } => visit_table_atomic_rmw_cmpxchg (arity 3 -> 1)
            @shared_everything_threads StructAtomicGet { ordering: $crate::Ordering, struct_type_index: u32, field_index: u32  } => visit_struct_atomic_get (arity 1 -> 1)
            @shared_everything_threads StructAtomicGetS { ordering: $crate::Ordering, struct_type_index: u32, field_index: u32  } => visit_struct_atomic_get_s (arity 1 -> 1)
            @shared_everything_threads StructAtomicGetU { ordering: $crate::Ordering, struct_type_index: u32, field_index: u32  } => visit_struct_atomic_get_u (arity 1 -> 1)
            @shared_everything_threads StructAtomicSet { ordering: $crate::Ordering, struct_type_index: u32, field_index: u32  } => visit_struct_atomic_set (arity 2 -> 0)
            @shared_everything_threads StructAtomicRmwAdd { ordering: $crate::Ordering, struct_type_index: u32, field_index: u32  } => visit_struct_atomic_rmw_add (atomic rmw struct add)
            @shared_everything_threads StructAtomicRmwSub { ordering: $crate::Ordering, struct_type_index: u32, field_index: u32  } => visit_struct_atomic_rmw_sub (atomic rmw struct sub)
            @shared_everything_threads StructAtomicRmwAnd { ordering: $crate::Ordering, struct_type_index: u32, field_index: u32  } => visit_struct_atomic_rmw_and (atomic rmw struct and)
            @shared_everything_threads StructAtomicRmwOr { ordering: $crate::Ordering, struct_type_index: u32, field_index: u32  } => visit_struct_atomic_rmw_or (atomic rmw struct or)
            @shared_everything_threads StructAtomicRmwXor { ordering: $crate::Ordering, struct_type_index: u32, field_index: u32  } => visit_struct_atomic_rmw_xor (atomic rmw struct xor)
            @shared_everything_threads StructAtomicRmwXchg { ordering: $crate::Ordering, struct_type_index: u32, field_index: u32  } => visit_struct_atomic_rmw_xchg (arity 2 -> 1)
            @shared_everything_threads StructAtomicRmwCmpxchg { ordering: $crate::Ordering, struct_type_index: u32, field_index: u32  } => visit_struct_atomic_rmw_cmpxchg (arity 3 -> 1)
            @shared_everything_threads ArrayAtomicGet { ordering: $crate::Ordering, array_type_index: u32 } => visit_array_atomic_get (arity 2 -> 1)
            @shared_everything_threads ArrayAtomicGetS { ordering: $crate::Ordering, array_type_index: u32 } => visit_array_atomic_get_s (arity 2 -> 1)
            @shared_everything_threads ArrayAtomicGetU { ordering: $crate::Ordering, array_type_index: u32 } => visit_array_atomic_get_u (arity 2 -> 1)
            @shared_everything_threads ArrayAtomicSet { ordering: $crate::Ordering, array_type_index: u32 } => visit_array_atomic_set (arity 3 -> 0)
            @shared_everything_threads ArrayAtomicRmwAdd { ordering: $crate::Ordering, array_type_index: u32 } => visit_array_atomic_rmw_add (atomic rmw array add)
            @shared_everything_threads ArrayAtomicRmwSub { ordering: $crate::Ordering, array_type_index: u32 } => visit_array_atomic_rmw_sub (atomic rmw array sub)
            @shared_everything_threads ArrayAtomicRmwAnd { ordering: $crate::Ordering, array_type_index: u32 } => visit_array_atomic_rmw_and (atomic rmw array and)
            @shared_everything_threads ArrayAtomicRmwOr { ordering: $crate::Ordering, array_type_index: u32 } => visit_array_atomic_rmw_or (atomic rmw array or)
            @shared_everything_threads ArrayAtomicRmwXor { ordering: $crate::Ordering, array_type_index: u32 } => visit_array_atomic_rmw_xor (atomic rmw array xor)
            @shared_everything_threads ArrayAtomicRmwXchg { ordering: $crate::Ordering, array_type_index: u32 } => visit_array_atomic_rmw_xchg (arity 3 -> 1)
            @shared_everything_threads ArrayAtomicRmwCmpxchg { ordering: $crate::Ordering, array_type_index: u32 } => visit_array_atomic_rmw_cmpxchg (arity 4 -> 1)
            @shared_everything_threads RefI31Shared => visit_ref_i31_shared (arity 1 -> 1)

            // 0xFD operators
            // 128-bit SIMD
            // - https://github.com/webassembly/simd
            // - https://webassembly.github.io/simd/core/binary/instructions.html
            @simd V128Load { memarg: $crate::MemArg } => visit_v128_load (load v128)
            @simd V128Load8x8S { memarg: $crate::MemArg } => visit_v128_load8x8_s (load v128)
            @simd V128Load8x8U { memarg: $crate::MemArg } => visit_v128_load8x8_u (load v128)
            @simd V128Load16x4S { memarg: $crate::MemArg } => visit_v128_load16x4_s (load v128)
            @simd V128Load16x4U { memarg: $crate::MemArg } => visit_v128_load16x4_u (load v128)
            @simd V128Load32x2S { memarg: $crate::MemArg } => visit_v128_load32x2_s (load v128)
            @simd V128Load32x2U { memarg: $crate::MemArg } => visit_v128_load32x2_u (load v128)
            @simd V128Load8Splat { memarg: $crate::MemArg } => visit_v128_load8_splat (load v128)
            @simd V128Load16Splat { memarg: $crate::MemArg } => visit_v128_load16_splat (load v128)
            @simd V128Load32Splat { memarg: $crate::MemArg } => visit_v128_load32_splat (load v128)
            @simd V128Load64Splat { memarg: $crate::MemArg } => visit_v128_load64_splat (load v128)
            @simd V128Load32Zero { memarg: $crate::MemArg } => visit_v128_load32_zero (load v128)
            @simd V128Load64Zero { memarg: $crate::MemArg } => visit_v128_load64_zero (load v128)
            @simd V128Store { memarg: $crate::MemArg } => visit_v128_store (store v128)
            @simd V128Load8Lane { memarg: $crate::MemArg, lane: u8 } => visit_v128_load8_lane (load lane 16)
            @simd V128Load16Lane { memarg: $crate::MemArg, lane: u8 } => visit_v128_load16_lane (load lane 8)
            @simd V128Load32Lane { memarg: $crate::MemArg, lane: u8 } => visit_v128_load32_lane (load lane 4)
            @simd V128Load64Lane { memarg: $crate::MemArg, lane: u8 } => visit_v128_load64_lane (load lane 2)
            @simd V128Store8Lane { memarg: $crate::MemArg, lane: u8 } => visit_v128_store8_lane (store lane 16)
            @simd V128Store16Lane { memarg: $crate::MemArg, lane: u8 } => visit_v128_store16_lane (store lane 8)
            @simd V128Store32Lane { memarg: $crate::MemArg, lane: u8 } => visit_v128_store32_lane (store lane 4)
            @simd V128Store64Lane { memarg: $crate::MemArg, lane: u8 } => visit_v128_store64_lane (store lane 2)
            @simd V128Const { value: $crate::V128 } => visit_v128_const (push v128)
            @simd I8x16Shuffle { lanes: [u8; 16] } => visit_i8x16_shuffle (arity 2 -> 1)
            @simd I8x16ExtractLaneS { lane: u8 } => visit_i8x16_extract_lane_s (extract i32 16)
            @simd I8x16ExtractLaneU { lane: u8 } => visit_i8x16_extract_lane_u (extract i32 16)
            @simd I8x16ReplaceLane { lane: u8 } => visit_i8x16_replace_lane (replace i32 16)
            @simd I16x8ExtractLaneS { lane: u8 } => visit_i16x8_extract_lane_s (extract i32 8)
            @simd I16x8ExtractLaneU { lane: u8 } => visit_i16x8_extract_lane_u (extract i32 8)
            @simd I16x8ReplaceLane { lane: u8 } => visit_i16x8_replace_lane (replace i32 8)
            @simd I32x4ExtractLane { lane: u8 } => visit_i32x4_extract_lane (extract i32 4)
            @simd I32x4ReplaceLane { lane: u8 } => visit_i32x4_replace_lane (replace i32 4)
            @simd I64x2ExtractLane { lane: u8 } => visit_i64x2_extract_lane (extract i64 2)
            @simd I64x2ReplaceLane { lane: u8 } => visit_i64x2_replace_lane (replace i64 2)
            @simd F32x4ExtractLane { lane: u8 } => visit_f32x4_extract_lane (extract f32 4)
            @simd F32x4ReplaceLane { lane: u8 } => visit_f32x4_replace_lane (replace f32 4)
            @simd F64x2ExtractLane { lane: u8 } => visit_f64x2_extract_lane (extract f64 2)
            @simd F64x2ReplaceLane { lane: u8 } => visit_f64x2_replace_lane (replace f64 2)
            @simd I8x16Swizzle => visit_i8x16_swizzle (binary v128)
            @simd I8x16Splat => visit_i8x16_splat (splat i32)
            @simd I16x8Splat => visit_i16x8_splat (splat i32)
            @simd I32x4Splat => visit_i32x4_splat (splat i32)
            @simd I64x2Splat => visit_i64x2_splat (splat i64)
            @simd F32x4Splat => visit_f32x4_splat (splat f32)
            @simd F64x2Splat => visit_f64x2_splat (splat f64)
            @simd I8x16Eq => visit_i8x16_eq (binary v128)
            @simd I8x16Ne => visit_i8x16_ne (binary v128)
            @simd I8x16LtS => visit_i8x16_lt_s (binary v128)
            @simd I8x16LtU => visit_i8x16_lt_u (binary v128)
            @simd I8x16GtS => visit_i8x16_gt_s (binary v128)
            @simd I8x16GtU => visit_i8x16_gt_u (binary v128)
            @simd I8x16LeS => visit_i8x16_le_s (binary v128)
            @simd I8x16LeU => visit_i8x16_le_u (binary v128)
            @simd I8x16GeS => visit_i8x16_ge_s (binary v128)
            @simd I8x16GeU => visit_i8x16_ge_u (binary v128)
            @simd I16x8Eq => visit_i16x8_eq (binary v128)
            @simd I16x8Ne => visit_i16x8_ne (binary v128)
            @simd I16x8LtS => visit_i16x8_lt_s (binary v128)
            @simd I16x8LtU => visit_i16x8_lt_u (binary v128)
            @simd I16x8GtS => visit_i16x8_gt_s (binary v128)
            @simd I16x8GtU => visit_i16x8_gt_u (binary v128)
            @simd I16x8LeS => visit_i16x8_le_s (binary v128)
            @simd I16x8LeU => visit_i16x8_le_u (binary v128)
            @simd I16x8GeS => visit_i16x8_ge_s (binary v128)
            @simd I16x8GeU => visit_i16x8_ge_u (binary v128)
            @simd I32x4Eq => visit_i32x4_eq (binary v128)
            @simd I32x4Ne => visit_i32x4_ne (binary v128)
            @simd I32x4LtS => visit_i32x4_lt_s (binary v128)
            @simd I32x4LtU => visit_i32x4_lt_u (binary v128)
            @simd I32x4GtS => visit_i32x4_gt_s (binary v128)
            @simd I32x4GtU => visit_i32x4_gt_u (binary v128)
            @simd I32x4LeS => visit_i32x4_le_s (binary v128)
            @simd I32x4LeU => visit_i32x4_le_u (binary v128)
            @simd I32x4GeS => visit_i32x4_ge_s (binary v128)
            @simd I32x4GeU => visit_i32x4_ge_u (binary v128)
            @simd I64x2Eq => visit_i64x2_eq (binary v128)
            @simd I64x2Ne => visit_i64x2_ne (binary v128)
            @simd I64x2LtS => visit_i64x2_lt_s (binary v128)
            @simd I64x2GtS => visit_i64x2_gt_s (binary v128)
            @simd I64x2LeS => visit_i64x2_le_s (binary v128)
            @simd I64x2GeS => visit_i64x2_ge_s (binary v128)
            @simd F32x4Eq => visit_f32x4_eq (binary v128f)
            @simd F32x4Ne => visit_f32x4_ne (binary v128f)
            @simd F32x4Lt => visit_f32x4_lt (binary v128f)
            @simd F32x4Gt => visit_f32x4_gt (binary v128f)
            @simd F32x4Le => visit_f32x4_le (binary v128f)
            @simd F32x4Ge => visit_f32x4_ge (binary v128f)
            @simd F64x2Eq => visit_f64x2_eq (binary v128f)
            @simd F64x2Ne => visit_f64x2_ne (binary v128f)
            @simd F64x2Lt => visit_f64x2_lt (binary v128f)
            @simd F64x2Gt => visit_f64x2_gt (binary v128f)
            @simd F64x2Le => visit_f64x2_le (binary v128f)
            @simd F64x2Ge => visit_f64x2_ge (binary v128f)
            @simd V128Not => visit_v128_not (unary v128)
            @simd V128And => visit_v128_and (binary v128)
            @simd V128AndNot => visit_v128_andnot (binary v128)
            @simd V128Or => visit_v128_or (binary v128)
            @simd V128Xor => visit_v128_xor (binary v128)
            @simd V128Bitselect => visit_v128_bitselect (ternary v128)
            @simd V128AnyTrue => visit_v128_any_true (test v128)
            @simd I8x16Abs => visit_i8x16_abs (unary v128)
            @simd I8x16Neg => visit_i8x16_neg (unary v128)
            @simd I8x16Popcnt => visit_i8x16_popcnt (unary v128)
            @simd I8x16AllTrue => visit_i8x16_all_true (test v128)
            @simd I8x16Bitmask => visit_i8x16_bitmask (test v128)
            @simd I8x16NarrowI16x8S => visit_i8x16_narrow_i16x8_s (binary v128)
            @simd I8x16NarrowI16x8U => visit_i8x16_narrow_i16x8_u (binary v128)
            @simd I8x16Shl => visit_i8x16_shl (shift v128)
            @simd I8x16ShrS => visit_i8x16_shr_s (shift v128)
            @simd I8x16ShrU => visit_i8x16_shr_u (shift v128)
            @simd I8x16Add => visit_i8x16_add (binary v128)
            @simd I8x16AddSatS => visit_i8x16_add_sat_s (binary v128)
            @simd I8x16AddSatU => visit_i8x16_add_sat_u (binary v128)
            @simd I8x16Sub => visit_i8x16_sub (binary v128)
            @simd I8x16SubSatS => visit_i8x16_sub_sat_s (binary v128)
            @simd I8x16SubSatU => visit_i8x16_sub_sat_u (binary v128)
            @simd I8x16MinS => visit_i8x16_min_s (binary v128)
            @simd I8x16MinU => visit_i8x16_min_u (binary v128)
            @simd I8x16MaxS => visit_i8x16_max_s (binary v128)
            @simd I8x16MaxU => visit_i8x16_max_u (binary v128)
            @simd I8x16AvgrU => visit_i8x16_avgr_u (binary v128)
            @simd I16x8ExtAddPairwiseI8x16S => visit_i16x8_extadd_pairwise_i8x16_s (unary v128)
            @simd I16x8ExtAddPairwiseI8x16U => visit_i16x8_extadd_pairwise_i8x16_u (unary v128)
            @simd I16x8Abs => visit_i16x8_abs (unary v128)
            @simd I16x8Neg => visit_i16x8_neg (unary v128)
            @simd I16x8Q15MulrSatS => visit_i16x8_q15mulr_sat_s (binary v128)
            @simd I16x8AllTrue => visit_i16x8_all_true (test v128)
            @simd I16x8Bitmask => visit_i16x8_bitmask (test v128)
            @simd I16x8NarrowI32x4S => visit_i16x8_narrow_i32x4_s (binary v128)
            @simd I16x8NarrowI32x4U => visit_i16x8_narrow_i32x4_u (binary v128)
            @simd I16x8ExtendLowI8x16S => visit_i16x8_extend_low_i8x16_s (unary v128)
            @simd I16x8ExtendHighI8x16S => visit_i16x8_extend_high_i8x16_s (unary v128)
            @simd I16x8ExtendLowI8x16U => visit_i16x8_extend_low_i8x16_u (unary v128)
            @simd I16x8ExtendHighI8x16U => visit_i16x8_extend_high_i8x16_u (unary v128)
            @simd I16x8Shl => visit_i16x8_shl (shift v128)
            @simd I16x8ShrS => visit_i16x8_shr_s (shift v128)
            @simd I16x8ShrU => visit_i16x8_shr_u (shift v128)
            @simd I16x8Add => visit_i16x8_add (binary v128)
            @simd I16x8AddSatS => visit_i16x8_add_sat_s (binary v128)
            @simd I16x8AddSatU => visit_i16x8_add_sat_u (binary v128)
            @simd I16x8Sub => visit_i16x8_sub (binary v128)
            @simd I16x8SubSatS => visit_i16x8_sub_sat_s (binary v128)
            @simd I16x8SubSatU => visit_i16x8_sub_sat_u (binary v128)
            @simd I16x8Mul => visit_i16x8_mul (binary v128)
            @simd I16x8MinS => visit_i16x8_min_s (binary v128)
            @simd I16x8MinU => visit_i16x8_min_u (binary v128)
            @simd I16x8MaxS => visit_i16x8_max_s (binary v128)
            @simd I16x8MaxU => visit_i16x8_max_u (binary v128)
            @simd I16x8AvgrU => visit_i16x8_avgr_u (binary v128)
            @simd I16x8ExtMulLowI8x16S => visit_i16x8_extmul_low_i8x16_s (binary v128)
            @simd I16x8ExtMulHighI8x16S => visit_i16x8_extmul_high_i8x16_s (binary v128)
            @simd I16x8ExtMulLowI8x16U => visit_i16x8_extmul_low_i8x16_u (binary v128)
            @simd I16x8ExtMulHighI8x16U => visit_i16x8_extmul_high_i8x16_u (binary v128)
            @simd I32x4ExtAddPairwiseI16x8S => visit_i32x4_extadd_pairwise_i16x8_s (unary v128)
            @simd I32x4ExtAddPairwiseI16x8U => visit_i32x4_extadd_pairwise_i16x8_u (unary v128)
            @simd I32x4Abs => visit_i32x4_abs (unary v128)
            @simd I32x4Neg => visit_i32x4_neg (unary v128)
            @simd I32x4AllTrue => visit_i32x4_all_true (test v128)
            @simd I32x4Bitmask => visit_i32x4_bitmask (test v128)
            @simd I32x4ExtendLowI16x8S => visit_i32x4_extend_low_i16x8_s (unary v128)
            @simd I32x4ExtendHighI16x8S => visit_i32x4_extend_high_i16x8_s (unary v128)
            @simd I32x4ExtendLowI16x8U => visit_i32x4_extend_low_i16x8_u (unary v128)
            @simd I32x4ExtendHighI16x8U => visit_i32x4_extend_high_i16x8_u (unary v128)
            @simd I32x4Shl => visit_i32x4_shl (shift v128)
            @simd I32x4ShrS => visit_i32x4_shr_s (shift v128)
            @simd I32x4ShrU => visit_i32x4_shr_u (shift v128)
            @simd I32x4Add => visit_i32x4_add (binary v128)
            @simd I32x4Sub => visit_i32x4_sub (binary v128)
            @simd I32x4Mul => visit_i32x4_mul (binary v128)
            @simd I32x4MinS => visit_i32x4_min_s (binary v128)
            @simd I32x4MinU => visit_i32x4_min_u (binary v128)
            @simd I32x4MaxS => visit_i32x4_max_s (binary v128)
            @simd I32x4MaxU => visit_i32x4_max_u (binary v128)
            @simd I32x4DotI16x8S => visit_i32x4_dot_i16x8_s (binary v128)
            @simd I32x4ExtMulLowI16x8S => visit_i32x4_extmul_low_i16x8_s (binary v128)
            @simd I32x4ExtMulHighI16x8S => visit_i32x4_extmul_high_i16x8_s (binary v128)
            @simd I32x4ExtMulLowI16x8U => visit_i32x4_extmul_low_i16x8_u (binary v128)
            @simd I32x4ExtMulHighI16x8U => visit_i32x4_extmul_high_i16x8_u (binary v128)
            @simd I64x2Abs => visit_i64x2_abs (unary v128)
            @simd I64x2Neg => visit_i64x2_neg (unary v128)
            @simd I64x2AllTrue => visit_i64x2_all_true (test v128)
            @simd I64x2Bitmask => visit_i64x2_bitmask (test v128)
            @simd I64x2ExtendLowI32x4S => visit_i64x2_extend_low_i32x4_s (unary v128)
            @simd I64x2ExtendHighI32x4S => visit_i64x2_extend_high_i32x4_s (unary v128)
            @simd I64x2ExtendLowI32x4U => visit_i64x2_extend_low_i32x4_u (unary v128)
            @simd I64x2ExtendHighI32x4U => visit_i64x2_extend_high_i32x4_u (unary v128)
            @simd I64x2Shl => visit_i64x2_shl (shift v128)
            @simd I64x2ShrS => visit_i64x2_shr_s (shift v128)
            @simd I64x2ShrU => visit_i64x2_shr_u (shift v128)
            @simd I64x2Add => visit_i64x2_add (binary v128)
            @simd I64x2Sub => visit_i64x2_sub (binary v128)
            @simd I64x2Mul => visit_i64x2_mul (binary v128)
            @simd I64x2ExtMulLowI32x4S => visit_i64x2_extmul_low_i32x4_s (binary v128)
            @simd I64x2ExtMulHighI32x4S => visit_i64x2_extmul_high_i32x4_s (binary v128)
            @simd I64x2ExtMulLowI32x4U => visit_i64x2_extmul_low_i32x4_u (binary v128)
            @simd I64x2ExtMulHighI32x4U => visit_i64x2_extmul_high_i32x4_u (binary v128)
            @simd F32x4Ceil => visit_f32x4_ceil (unary v128f)
            @simd F32x4Floor => visit_f32x4_floor (unary v128f)
            @simd F32x4Trunc => visit_f32x4_trunc (unary v128f)
            @simd F32x4Nearest => visit_f32x4_nearest (unary v128f)
            @simd F32x4Abs => visit_f32x4_abs (unary v128f)
            @simd F32x4Neg => visit_f32x4_neg (unary v128f)
            @simd F32x4Sqrt => visit_f32x4_sqrt (unary v128f)
            @simd F32x4Add => visit_f32x4_add (binary v128f)
            @simd F32x4Sub => visit_f32x4_sub (binary v128f)
            @simd F32x4Mul => visit_f32x4_mul (binary v128f)
            @simd F32x4Div => visit_f32x4_div (binary v128f)
            @simd F32x4Min => visit_f32x4_min (binary v128f)
            @simd F32x4Max => visit_f32x4_max (binary v128f)
            @simd F32x4PMin => visit_f32x4_pmin (binary v128f)
            @simd F32x4PMax => visit_f32x4_pmax (binary v128f)
            @simd F64x2Ceil => visit_f64x2_ceil (unary v128f)
            @simd F64x2Floor => visit_f64x2_floor (unary v128f)
            @simd F64x2Trunc => visit_f64x2_trunc (unary v128f)
            @simd F64x2Nearest => visit_f64x2_nearest (unary v128f)
            @simd F64x2Abs => visit_f64x2_abs (unary v128f)
            @simd F64x2Neg => visit_f64x2_neg (unary v128f)
            @simd F64x2Sqrt => visit_f64x2_sqrt (unary v128f)
            @simd F64x2Add => visit_f64x2_add (binary v128f)
            @simd F64x2Sub => visit_f64x2_sub (binary v128f)
            @simd F64x2Mul => visit_f64x2_mul (binary v128f)
            @simd F64x2Div => visit_f64x2_div (binary v128f)
            @simd F64x2Min => visit_f64x2_min (binary v128f)
            @simd F64x2Max => visit_f64x2_max (binary v128f)
            @simd F64x2PMin => visit_f64x2_pmin (binary v128f)
            @simd F64x2PMax => visit_f64x2_pmax (binary v128f)
            @simd I32x4TruncSatF32x4S => visit_i32x4_trunc_sat_f32x4_s (unary v128f)
            @simd I32x4TruncSatF32x4U => visit_i32x4_trunc_sat_f32x4_u (unary v128f)
            @simd F32x4ConvertI32x4S => visit_f32x4_convert_i32x4_s (unary v128f)
            @simd F32x4ConvertI32x4U => visit_f32x4_convert_i32x4_u (unary v128f)
            @simd I32x4TruncSatF64x2SZero => visit_i32x4_trunc_sat_f64x2_s_zero (unary v128f)
            @simd I32x4TruncSatF64x2UZero => visit_i32x4_trunc_sat_f64x2_u_zero (unary v128f)
            @simd F64x2ConvertLowI32x4S => visit_f64x2_convert_low_i32x4_s (unary v128f)
            @simd F64x2ConvertLowI32x4U => visit_f64x2_convert_low_i32x4_u (unary v128f)
            @simd F32x4DemoteF64x2Zero => visit_f32x4_demote_f64x2_zero (unary v128f)
            @simd F64x2PromoteLowF32x4 => visit_f64x2_promote_low_f32x4 (unary v128f)

            // Relaxed SIMD operators
            // https://github.com/WebAssembly/relaxed-simd
            @relaxed_simd I8x16RelaxedSwizzle => visit_i8x16_relaxed_swizzle (binary v128)
            @relaxed_simd I32x4RelaxedTruncF32x4S => visit_i32x4_relaxed_trunc_f32x4_s (unary v128)
            @relaxed_simd I32x4RelaxedTruncF32x4U => visit_i32x4_relaxed_trunc_f32x4_u (unary v128)
            @relaxed_simd I32x4RelaxedTruncF64x2SZero => visit_i32x4_relaxed_trunc_f64x2_s_zero (unary v128)
            @relaxed_simd I32x4RelaxedTruncF64x2UZero => visit_i32x4_relaxed_trunc_f64x2_u_zero (unary v128)
            @relaxed_simd F32x4RelaxedMadd => visit_f32x4_relaxed_madd (ternary v128)
            @relaxed_simd F32x4RelaxedNmadd => visit_f32x4_relaxed_nmadd (ternary v128)
            @relaxed_simd F64x2RelaxedMadd => visit_f64x2_relaxed_madd  (ternary v128)
            @relaxed_simd F64x2RelaxedNmadd => visit_f64x2_relaxed_nmadd (ternary v128)
            @relaxed_simd I8x16RelaxedLaneselect => visit_i8x16_relaxed_laneselect (ternary v128)
            @relaxed_simd I16x8RelaxedLaneselect => visit_i16x8_relaxed_laneselect (ternary v128)
            @relaxed_simd I32x4RelaxedLaneselect => visit_i32x4_relaxed_laneselect (ternary v128)
            @relaxed_simd I64x2RelaxedLaneselect => visit_i64x2_relaxed_laneselect (ternary v128)
            @relaxed_simd F32x4RelaxedMin => visit_f32x4_relaxed_min (binary v128)
            @relaxed_simd F32x4RelaxedMax => visit_f32x4_relaxed_max (binary v128)
            @relaxed_simd F64x2RelaxedMin => visit_f64x2_relaxed_min (binary v128)
            @relaxed_simd F64x2RelaxedMax => visit_f64x2_relaxed_max (binary v128)
            @relaxed_simd I16x8RelaxedQ15mulrS => visit_i16x8_relaxed_q15mulr_s (binary v128)
            @relaxed_simd I16x8RelaxedDotI8x16I7x16S => visit_i16x8_relaxed_dot_i8x16_i7x16_s (binary v128)
            @relaxed_simd I32x4RelaxedDotI8x16I7x16AddS => visit_i32x4_relaxed_dot_i8x16_i7x16_add_s (ternary v128)

            // Typed Function references
            @function_references CallRef { type_index: u32 } => visit_call_ref (arity 1 type -> type)
            @function_references ReturnCallRef { type_index: u32 } => visit_return_call_ref (arity 1 type -> 0)
            @function_references RefAsNonNull => visit_ref_as_non_null (arity 1 -> 1)
            @function_references BrOnNull { relative_depth: u32 } => visit_br_on_null (arity 1 br -> 1 br)
            @function_references BrOnNonNull { relative_depth: u32 } => visit_br_on_non_null (arity br -> br -1)

            // Stack switching
            @stack_switching ContNew { cont_type_index: u32 } => visit_cont_new (arity 1 -> 1)
            @stack_switching ContBind { argument_index: u32, result_index: u32 } => visit_cont_bind (arity type_diff 1 -> 1)
            @stack_switching Suspend { tag_index: u32 } => visit_suspend (arity tag -> tag)
            @stack_switching Resume { cont_type_index: u32, resume_table: $crate::ResumeTable } => visit_resume (arity 1 type -> type)
            @stack_switching ResumeThrow { cont_type_index: u32, tag_index: u32, resume_table: $crate::ResumeTable } => visit_resume_throw (arity 1 tag -> type)
            @stack_switching Switch { cont_type_index: u32, tag_index: u32 } => visit_switch (arity type -> ~switch)

            @wide_arithmetic I64Add128 => visit_i64_add128 (arity 4 -> 2)
            @wide_arithmetic I64Sub128 => visit_i64_sub128 (arity 4 -> 2)
            @wide_arithmetic I64MulWideS => visit_i64_mul_wide_s (arity 2 -> 2)
            @wide_arithmetic I64MulWideU => visit_i64_mul_wide_u (arity 2 -> 2)
        }
    };
}

macro_rules! format_err {
    ($offset:expr, $($arg:tt)*) => {
        crate::BinaryReaderError::fmt(format_args!($($arg)*), $offset)
    }
}

macro_rules! bail {
    ($($arg:tt)*) => {return Err(format_err!($($arg)*))}
}

pub use crate::arity::*;
pub use crate::binary_reader::{BinaryReader, BinaryReaderError, Result};
pub use crate::features::*;
pub use crate::parser::*;
pub use crate::readers::*;

mod arity;
mod binary_reader;
mod features;
mod limits;
mod parser;
mod readers;

#[cfg(feature = "validate")]
mod resources;
#[cfg(feature = "validate")]
mod validator;
#[cfg(feature = "validate")]
pub use crate::resources::*;
#[cfg(feature = "validate")]
pub use crate::validator::*;

#[cfg(feature = "validate")]
pub mod collections;
