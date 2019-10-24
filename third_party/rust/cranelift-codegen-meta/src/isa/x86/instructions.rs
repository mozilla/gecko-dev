#![allow(non_snake_case)]

use crate::cdsl::formats::FormatRegistry;
use crate::cdsl::instructions::{
    AllInstructions, InstructionBuilder as Inst, InstructionGroup, InstructionGroupBuilder,
};
use crate::cdsl::operands::{create_operand as operand, create_operand_doc as operand_doc};
use crate::cdsl::types::ValueType;
use crate::cdsl::typevar::{Interval, TypeSetBuilder, TypeVar};
use crate::shared::immediates::Immediates;
use crate::shared::types;

pub(crate) fn define(
    mut all_instructions: &mut AllInstructions,
    format_registry: &FormatRegistry,
    immediates: &Immediates,
) -> InstructionGroup {
    let mut ig = InstructionGroupBuilder::new(
        "x86",
        "x86 specific instruction set",
        &mut all_instructions,
        format_registry,
    );

    let iflags: &TypeVar = &ValueType::Special(types::Flag::IFlags.into()).into();

    let iWord = &TypeVar::new(
        "iWord",
        "A scalar integer machine word",
        TypeSetBuilder::new().ints(32..64).build(),
    );
    let nlo = &operand_doc("nlo", iWord, "Low part of numerator");
    let nhi = &operand_doc("nhi", iWord, "High part of numerator");
    let d = &operand_doc("d", iWord, "Denominator");
    let q = &operand_doc("q", iWord, "Quotient");
    let r = &operand_doc("r", iWord, "Remainder");

    ig.push(
        Inst::new(
            "x86_udivmodx",
            r#"
        Extended unsigned division.

        Concatenate the bits in `nhi` and `nlo` to form the numerator.
        Interpret the bits as an unsigned number and divide by the unsigned
        denominator `d`. Trap when `d` is zero or if the quotient is larger
        than the range of the output.

        Return both quotient and remainder.
        "#,
        )
        .operands_in(vec![nlo, nhi, d])
        .operands_out(vec![q, r])
        .can_trap(true),
    );

    ig.push(
        Inst::new(
            "x86_sdivmodx",
            r#"
        Extended signed division.

        Concatenate the bits in `nhi` and `nlo` to form the numerator.
        Interpret the bits as a signed number and divide by the signed
        denominator `d`. Trap when `d` is zero or if the quotient is outside
        the range of the output.

        Return both quotient and remainder.
        "#,
        )
        .operands_in(vec![nlo, nhi, d])
        .operands_out(vec![q, r])
        .can_trap(true),
    );

    let argL = &operand("argL", iWord);
    let argR = &operand("argR", iWord);
    let resLo = &operand("resLo", iWord);
    let resHi = &operand("resHi", iWord);

    ig.push(
        Inst::new(
            "x86_umulx",
            r#"
        Unsigned integer multiplication, producing a double-length result.

        Polymorphic over all scalar integer types, but does not support vector
        types.
        "#,
        )
        .operands_in(vec![argL, argR])
        .operands_out(vec![resLo, resHi]),
    );

    ig.push(
        Inst::new(
            "x86_smulx",
            r#"
        Signed integer multiplication, producing a double-length result.

        Polymorphic over all scalar integer types, but does not support vector
        types.
        "#,
        )
        .operands_in(vec![argL, argR])
        .operands_out(vec![resLo, resHi]),
    );

    let Float = &TypeVar::new(
        "Float",
        "A scalar or vector floating point number",
        TypeSetBuilder::new()
            .floats(Interval::All)
            .simd_lanes(Interval::All)
            .build(),
    );
    let IntTo = &TypeVar::new(
        "IntTo",
        "An integer type with the same number of lanes",
        TypeSetBuilder::new()
            .ints(32..64)
            .simd_lanes(Interval::All)
            .build(),
    );
    let x = &operand("x", Float);
    let a = &operand("a", IntTo);

    ig.push(
        Inst::new(
            "x86_cvtt2si",
            r#"
        Convert with truncation floating point to signed integer.

        The source floating point operand is converted to a signed integer by
        rounding towards zero. If the result can't be represented in the output
        type, returns the smallest signed value the output type can represent.

        This instruction does not trap.
        "#,
        )
        .operands_in(vec![x])
        .operands_out(vec![a]),
    );

    let x = &operand("x", Float);
    let a = &operand("a", Float);
    let y = &operand("y", Float);

    ig.push(
        Inst::new(
            "x86_fmin",
            r#"
        Floating point minimum with x86 semantics.

        This is equivalent to the C ternary operator `x < y ? x : y` which
        differs from `fmin` when either operand is NaN or when comparing
        +0.0 to -0.0.

        When the two operands don't compare as LT, `y` is returned unchanged,
        even if it is a signalling NaN.
        "#,
        )
        .operands_in(vec![x, y])
        .operands_out(vec![a]),
    );

    ig.push(
        Inst::new(
            "x86_fmax",
            r#"
        Floating point maximum with x86 semantics.

        This is equivalent to the C ternary operator `x > y ? x : y` which
        differs from `fmax` when either operand is NaN or when comparing
        +0.0 to -0.0.

        When the two operands don't compare as GT, `y` is returned unchanged,
        even if it is a signalling NaN.
        "#,
        )
        .operands_in(vec![x, y])
        .operands_out(vec![a]),
    );

    let x = &operand("x", iWord);

    ig.push(
        Inst::new(
            "x86_push",
            r#"
    Pushes a value onto the stack.

    Decrements the stack pointer and stores the specified value on to the top.

    This is polymorphic in i32 and i64. However, it is only implemented for i64
    in 64-bit mode, and only for i32 in 32-bit mode.
    "#,
        )
        .operands_in(vec![x])
        .other_side_effects(true)
        .can_store(true),
    );

    ig.push(
        Inst::new(
            "x86_pop",
            r#"
    Pops a value from the stack.

    Loads a value from the top of the stack and then increments the stack
    pointer.

    This is polymorphic in i32 and i64. However, it is only implemented for i64
    in 64-bit mode, and only for i32 in 32-bit mode.
    "#,
        )
        .operands_out(vec![x])
        .other_side_effects(true)
        .can_load(true),
    );

    let y = &operand("y", iWord);
    let rflags = &operand("rflags", iflags);

    ig.push(
        Inst::new(
            "x86_bsr",
            r#"
    Bit Scan Reverse -- returns the bit-index of the most significant 1
    in the word. Result is undefined if the argument is zero. However, it
    sets the Z flag depending on the argument, so it is at least easy to
    detect and handle that case.

    This is polymorphic in i32 and i64. It is implemented for both i64 and
    i32 in 64-bit mode, and only for i32 in 32-bit mode.
    "#,
        )
        .operands_in(vec![x])
        .operands_out(vec![y, rflags]),
    );

    ig.push(
        Inst::new(
            "x86_bsf",
            r#"
    Bit Scan Forwards -- returns the bit-index of the least significant 1
    in the word. Is otherwise identical to 'bsr', just above.
    "#,
        )
        .operands_in(vec![x])
        .operands_out(vec![y, rflags]),
    );

    let uimm8 = &immediates.uimm8;
    let TxN = &TypeVar::new(
        "TxN",
        "A SIMD vector type",
        TypeSetBuilder::new()
            .ints(Interval::All)
            .floats(Interval::All)
            .bools(Interval::All)
            .simd_lanes(Interval::All)
            .includes_scalars(false)
            .build(),
    );
    let a = &operand_doc("a", TxN, "A vector value (i.e. held in an XMM register)");
    let b = &operand_doc("b", TxN, "A vector value (i.e. held in an XMM register)");
    let i = &operand_doc("i", uimm8, "An ordering operand controlling the copying of data from the source to the destination; see PSHUFD in Intel manual for details");

    ig.push(
        Inst::new(
            "x86_pshufd",
            r#"
    Packed Shuffle Doublewords -- copies data from either memory or lanes in an extended
    register and re-orders the data according to the passed immediate byte.
    "#,
        )
        .operands_in(vec![a, i]) // TODO allow copying from memory here (need more permissive type than TxN)
        .operands_out(vec![a]),
    );

    ig.push(
        Inst::new(
            "x86_pshufb",
            r#"
    Packed Shuffle Bytes -- re-orders data in an extended register using a shuffle
    mask from either memory or another extended register
    "#,
        )
        .operands_in(vec![a, b]) // TODO allow re-ordering from memory here (need more permissive type than TxN)
        .operands_out(vec![a]),
    );

    let Idx = &operand_doc("Idx", uimm8, "Lane index");
    let x = &operand("x", TxN);
    let a = &operand("a", &TxN.lane_of());

    ig.push(
        Inst::new(
            "x86_pextr",
            r#"
        Extract lane ``Idx`` from ``x``.
        The lane index, ``Idx``, is an immediate value, not an SSA value. It
        must indicate a valid lane index for the type of ``x``.
        "#,
        )
        .operands_in(vec![x, Idx])
        .operands_out(vec![a]),
    );

    let IBxN = &TypeVar::new(
        "IBxN",
        "A SIMD vector type containing only booleans and integers",
        TypeSetBuilder::new()
            .ints(Interval::All)
            .bools(Interval::All)
            .simd_lanes(Interval::All)
            .includes_scalars(false)
            .build(),
    );
    let x = &operand("x", IBxN);
    let y = &operand_doc("y", &IBxN.lane_of(), "New lane value");
    let a = &operand("a", IBxN);

    ig.push(
        Inst::new(
            "x86_pinsr",
            r#"
        Insert ``y`` into ``x`` at lane ``Idx``.
        The lane index, ``Idx``, is an immediate value, not an SSA value. It
        must indicate a valid lane index for the type of ``x``.
        "#,
        )
        .operands_in(vec![x, Idx, y])
        .operands_out(vec![a]),
    );

    let FxN = &TypeVar::new(
        "FxN",
        "A SIMD vector type containing floats",
        TypeSetBuilder::new()
            .floats(Interval::All)
            .simd_lanes(Interval::All)
            .includes_scalars(false)
            .build(),
    );
    let x = &operand("x", FxN);
    let y = &operand_doc("y", &FxN.lane_of(), "New lane value");
    let a = &operand("a", FxN);

    ig.push(
        Inst::new(
            "x86_insertps",
            r#"
        Insert a lane of ``y`` into ``x`` at using ``Idx`` to encode both which lane the value is 
        extracted from and which it is inserted to. This is similar to x86_pinsr but inserts 
        floats, which are already stored in an XMM register.
        "#,
        )
        .operands_in(vec![x, Idx, y])
        .operands_out(vec![a]),
    );

    let x = &operand("x", FxN);
    let y = &operand("y", FxN);
    let a = &operand("a", FxN);

    ig.push(
        Inst::new(
            "x86_movsd",
            r#"
        Move the low 64 bits of the float vector ``y`` to the low 64 bits of float vector ``x``
        "#,
        )
        .operands_in(vec![x, y])
        .operands_out(vec![a]),
    );

    ig.push(
        Inst::new(
            "x86_movlhps",
            r#"
        Move the low 64 bits of the float vector ``y`` to the high 64 bits of float vector ``x``
        "#,
        )
        .operands_in(vec![x, y])
        .operands_out(vec![a]),
    );

    let IxN = &TypeVar::new(
        "IxN",
        "A SIMD vector type containing integers",
        TypeSetBuilder::new()
            .ints(Interval::All)
            .simd_lanes(Interval::All)
            .includes_scalars(false)
            .build(),
    );
    let I64x2 = &TypeVar::new(
        "I64x2",
        "A SIMD vector type containing one large integer (the upper lane is concatenated with \
         the lower lane to form the integer)",
        TypeSetBuilder::new()
            .ints(64..64)
            .simd_lanes(2..2)
            .includes_scalars(false)
            .build(),
    );
    let x = &operand_doc("x", IxN, "Vector value to shift");
    let y = &operand_doc("y", I64x2, "Number of bits to shift");
    let a = &operand("a", IxN);
    ig.push(
        Inst::new(
            "x86_psll",
            r#"
        Shift Packed Data Left Logical -- This implements the behavior of the shared instruction 
        ``ishl`` but alters the shift operand to live in an XMM register as expected by the PSLL*
        family of instructions.
        "#,
        )
        .operands_in(vec![x, y])
        .operands_out(vec![a]),
    );
    ig.push(
        Inst::new(
            "x86_psrl",
            r#"
        Shift Packed Data Right Logical -- This implements the behavior of the shared instruction 
        ``ushr`` but alters the shift operand to live in an XMM register as expected by the PSRL*
        family of instructions.
        "#,
        )
        .operands_in(vec![x, y])
        .operands_out(vec![a]),
    );
    ig.push(
        Inst::new(
            "x86_psra",
            r#"
        Shift Packed Data Right Arithmetic -- This implements the behavior of the shared 
        instruction ``sshr`` but alters the shift operand to live in an XMM register as expected by 
        the PSRA* family of instructions.
        "#,
        )
        .operands_in(vec![x, y])
        .operands_out(vec![a]),
    );

    ig.build()
}
