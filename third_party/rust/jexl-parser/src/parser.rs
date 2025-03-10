// auto-generated: "lalrpop 0.19.8"
// sha3: bceacaa24ba21e6d6ec6b336c5fdd6fcdaa08bf5094824af45d3f7273cd647e7
use crate::ast::{Expression, OpCode};
use std::str::FromStr;
#[allow(unused_extern_crates)]
extern crate lalrpop_util as __lalrpop_util;
#[allow(unused_imports)]
use self::__lalrpop_util::state_machine as __state_machine;
extern crate alloc;
extern crate core;

#[cfg_attr(rustfmt, rustfmt_skip)]
mod __parse__Expression {
    #![allow(non_snake_case, non_camel_case_types, unused_mut, unused_variables, unused_imports, unused_parens, clippy::all)]

    use std::str::FromStr;
    use crate::ast::{Expression, OpCode};
    #[allow(unused_extern_crates)]
    extern crate lalrpop_util as __lalrpop_util;
    #[allow(unused_imports)]
    use self::__lalrpop_util::state_machine as __state_machine;
    extern crate core;
    extern crate alloc;
    use self::__lalrpop_util::lexer::Token;
    #[allow(dead_code)]
    pub(crate) enum __Symbol<'input>
     {
        Variant0(&'input str),
        Variant1((String, Box<Expression>)),
        Variant2(alloc::vec::Vec<(String, Box<Expression>)>),
        Variant3(Box<Expression>),
        Variant4(alloc::vec::Vec<Box<Expression>>),
        Variant5(core::option::Option<(String, Box<Expression>)>),
        Variant6(Vec<Box<Expression>>),
        Variant7(core::option::Option<Vec<Box<Expression>>>),
        Variant8(bool),
        Variant9(Vec<(String, Box<Expression>)>),
        Variant10(core::option::Option<Box<Expression>>),
        Variant11(String),
        Variant12(Option<Box<Expression>>),
        Variant13(f64),
        Variant14(OpCode),
    }
    const __ACTION: &[i8] = &[
        // State 0
        0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 51, 52, 9, 0, 0, 53, 0, 54, 55, 10, 56, 0, 0, 0, 57, 58, 59, 60, 61, 62,
        // State 1
        63, 0, -29, 0, -29, 0, 0, -29, 0, 0, 0, 0, 0, 64, 65, 66, 67, 68, 0, 0, 0, 0, -29, 0, 0, 69, 0, 0, 0, 0, 0, -29, -29, 0, 0, 0, 0, 0, 0,
        // State 2
        -31, 0, -31, 0, -31, 0, 70, -31, 71, 0, 0, 0, 0, -31, -31, -31, -31, -31, 0, 0, 0, 0, -31, 0, 0, -31, 0, 0, 0, 0, 0, -31, -31, 0, 0, 0, 0, 0, 0,
        // State 3
        -33, 0, -33, 0, -33, 72, -33, -33, -33, 0, 73, 74, 0, -33, -33, -33, -33, -33, 0, 0, 0, 0, -33, 0, 0, -33, 0, 0, 0, 0, 0, -33, -33, 0, 0, 0, 0, 0, 0,
        // State 4
        -35, 75, -35, 0, -35, -35, -35, -35, -35, 0, -35, -35, 0, -35, -35, -35, -35, -35, 0, 0, 0, 0, -35, 76, 0, -35, 0, 0, 0, 0, 0, -35, -35, 0, 0, 0, 0, 0, 0,
        // State 5
        -42, -42, -42, 0, -42, -42, -42, -42, -42, 17, -42, -42, -42, -42, -42, -42, -42, -42, -42, 0, 0, 18, -42, -42, 0, -42, 0, 0, 0, 0, -42, -42, -42, 0, 0, 0, 0, 0, 0,
        // State 6
        0, 0, 78, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 79, 0, 0, 0, 0, 0, 0, 0,
        // State 7
        0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 51, 52, 9, 0, 0, 53, 0, 54, 55, 10, 56, 0, 0, 0, 57, 58, 59, 60, 61, 62,
        // State 8
        0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 51, 52, 9, -25, 0, 53, 0, 54, 55, 10, 56, 0, 0, 0, 57, 58, 59, 60, 61, 62,
        // State 9
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -21, 57, 58, 0, 0, 61, 0,
        // State 10
        0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 51, 52, 9, 0, 0, 53, 0, 54, 55, 10, 56, 0, 0, 0, 57, 58, 59, 60, 61, 62,
        // State 11
        0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 51, 52, 9, 0, 0, 53, 0, 54, 55, 10, 56, 0, 0, 0, 57, 58, 59, 60, 61, 62,
        // State 12
        0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 51, 52, 9, 0, 0, 53, 0, 54, 55, 10, 56, 0, 0, 0, 57, 58, 59, 60, 61, 62,
        // State 13
        0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 51, 52, 9, 0, 0, 53, 0, 54, 55, 10, 56, 0, 0, 0, 57, 58, 59, 60, 61, 62,
        // State 14
        0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 51, 52, 9, 0, 0, 53, 0, 54, 55, 10, 56, 0, 0, 0, 57, 58, 59, 60, 61, 62,
        // State 15
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 61, 0,
        // State 16
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 61, 0,
        // State 17
        0, 0, 0, 8, 0, 0, 0, 0, 0, 29, 0, 0, 0, 0, 0, 0, 0, 0, 0, 51, 52, 9, 0, 0, 53, 0, 54, 55, 10, 56, 0, 0, 0, 57, 58, 59, 60, 61, 62,
        // State 18
        0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 51, 52, 9, 0, 0, 53, 0, 54, 55, 10, 56, 0, 0, 0, 57, 58, 59, 60, 61, 62,
        // State 19
        0, 0, 78, 0, 88, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 79, 0, 0, 0, 0, 0, 0, 0,
        // State 20
        0, 0, 0, 8, -27, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 51, 52, 9, -27, 0, 53, 0, 54, 55, 10, 56, 0, 0, 0, 57, 58, 59, 60, 61, 62,
        // State 21
        0, 0, 78, 0, -24, 0, 0, 90, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -24, 0, 0, 0, 0, 0, 0, 0, 0, 79, 0, 0, 0, 0, 0, 0, 0,
        // State 22
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -23, 57, 58, 0, 0, 61, 0,
        // State 23
        -30, 0, -30, 0, -30, 0, 70, -30, 71, 0, 0, 0, 0, -30, -30, -30, -30, -30, 0, 0, 0, 0, -30, 0, 0, -30, 0, 0, 0, 0, 0, -30, -30, 0, 0, 0, 0, 0, 0,
        // State 24
        -32, 0, -32, 0, -32, 72, -32, -32, -32, 0, 73, 74, 0, -32, -32, -32, -32, -32, 0, 0, 0, 0, -32, 0, 0, -32, 0, 0, 0, 0, 0, -32, -32, 0, 0, 0, 0, 0, 0,
        // State 25
        -34, 75, -34, 0, -34, -34, -34, -34, -34, 0, -34, -34, 0, -34, -34, -34, -34, -34, 0, 0, 0, 0, -34, 76, 0, -34, 0, 0, 0, 0, 0, -34, -34, 0, 0, 0, 0, 0, 0,
        // State 26
        -41, -41, -41, 34, -41, -41, -41, -41, -41, 0, -41, -41, -41, -41, -41, -41, -41, -41, -41, 0, 0, 0, -41, -41, 0, -41, 0, 0, 0, 0, -41, -41, -41, 0, 0, 0, 0, 0, 0,
        // State 27
        0, 0, 78, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 94, 0, 0, 0, 0, 0, 0, 0, 0, 79, 0, 0, 0, 0, 0, 0, 0,
        // State 28
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 61, 0,
        // State 29
        63, 0, -28, 0, -28, 0, 0, -28, 0, 0, 0, 0, 0, 64, 65, 66, 67, 68, 0, 0, 0, 0, -28, 0, 0, 69, 0, 0, 0, 0, 0, -28, -28, 0, 0, 0, 0, 0, 0,
        // State 30
        0, 0, 78, 0, -26, 0, 0, 95, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -26, 0, 0, 0, 0, 0, 0, 0, 0, 79, 0, 0, 0, 0, 0, 0, 0,
        // State 31
        0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 51, 52, 9, 0, 0, 53, 0, 54, 55, 10, 56, 0, 0, 0, 57, 58, 59, 60, 61, 62,
        // State 32
        0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 51, 52, 9, 0, 0, 53, 0, 54, 55, 10, 56, 0, 0, 0, 57, 58, 59, 60, 61, 62,
        // State 33
        0, 0, 0, 8, -25, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 51, 52, 9, 0, 0, 53, 0, 54, 55, 10, 56, 0, 0, 0, 57, 58, 59, 60, 61, 62,
        // State 34
        63, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 64, 65, 66, 67, 68, 0, 0, 0, 0, 0, 0, 0, 69, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        // State 35
        0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 51, 52, 9, 0, 0, 53, 0, 54, 55, 10, 56, 0, 0, 0, 57, 58, 59, 60, 61, 62,
        // State 36
        0, 0, 78, 0, 0, 0, 0, 98, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 79, -20, 0, 0, 0, 0, 0, 0,
        // State 37
        0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 51, 52, 9, 0, 0, 53, 0, 54, 55, 10, 56, 0, 0, 0, 57, 58, 59, 60, 61, 62,
        // State 38
        0, 0, 78, 0, 0, 0, 0, 101, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 79, -22, 0, 0, 0, 0, 0, 0,
        // State 39
        -49, -49, -49, 0, -49, -49, -49, -49, -49, -49, -49, -49, -49, -49, -49, -49, -49, -49, -49, 0, 0, -49, -49, -49, 0, -49, 0, 0, 0, 0, -49, -49, -49, 0, 0, 0, 0, 0, 0,
        // State 40
        -47, -47, -47, 0, -47, -47, -47, -47, -47, -47, -47, -47, -47, -47, -47, -47, -47, -47, -47, 0, 0, -47, -47, -47, 0, -47, 0, 0, 0, 0, -47, -47, -47, 0, 0, 0, 0, 0, 0,
        // State 41
        0, 0, -54, 0, -54, 0, 0, -54, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -54, 0, 0, 0, 0, 0, 0, 0, 0, -54, -54, 0, 0, 0, 0, 0, 0,
        // State 42
        -37, -37, -37, 0, -37, -37, -37, -37, -37, 0, -37, -37, 0, -37, -37, -37, -37, -37, 15, 0, 0, 0, -37, -37, 0, -37, 0, 0, 0, 0, 0, -37, -37, 0, 0, 0, 0, 0, 0,
        // State 43
        -39, -39, -39, 0, -39, -39, -39, -39, -39, 0, -39, -39, 0, -39, -39, -39, -39, -39, -39, 0, 0, 0, -39, -39, 0, -39, 0, 0, 0, 0, 16, -39, -39, 0, 0, 0, 0, 0, 0,
        // State 44
        -45, -45, -45, 0, -45, -45, -45, -45, -45, -45, -45, -45, -45, -45, -45, -45, -45, -45, -45, 0, 0, -45, -45, -45, 0, -45, 0, 0, 0, 0, -45, -45, -45, 0, 0, 0, 0, 0, 0,
        // State 45
        -52, -52, -52, 0, -52, -52, -52, -52, -52, -52, -52, -52, -52, -52, -52, -52, -52, -52, -52, 0, 0, -52, -52, -52, 0, -52, 0, 0, 0, 0, -52, -52, -52, 0, 0, 0, 0, 0, 0,
        // State 46
        -51, -51, -51, 0, -51, -51, -51, -51, -51, -51, -51, -51, -51, -51, -51, -51, -51, -51, -51, 0, 0, -51, -51, -51, 0, -51, 0, 0, 0, 0, -51, -51, -51, 0, 0, 0, 0, 0, 0,
        // State 47
        -46, -46, -46, 0, -46, -46, -46, -46, -46, -46, -46, -46, -46, -46, -46, -46, -46, -46, -46, 0, 0, -46, -46, -46, 0, -46, 0, 0, 0, 0, -46, -46, -46, 0, 0, 0, 0, 0, 0,
        // State 48
        -50, -50, -50, 0, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, 0, 0, -50, -50, -50, 0, -50, 0, 0, 0, 0, -50, -50, -50, 0, 0, 0, 0, 0, 0,
        // State 49
        -48, -48, -48, 0, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, 0, 0, -48, -48, -48, 0, -48, 0, 0, 0, 0, -48, -48, -48, 0, 0, 0, 0, 0, 0,
        // State 50
        -62, -62, -62, 0, -62, -62, -62, -62, -62, -62, -62, -62, -62, -62, -62, -62, -62, -62, -62, 0, 0, -62, -62, -62, 0, -62, 0, 0, 0, 0, -62, -62, -62, 0, 0, 0, 0, 0, 0,
        // State 51
        -60, -60, -60, 0, -60, -60, -60, -60, -60, -60, -60, -60, -60, -60, -60, -60, -60, -60, -60, 0, 0, -60, -60, -60, 0, -60, 0, 0, 0, 0, -60, -60, -60, 0, 0, 0, 0, 0, 0,
        // State 52
        -19, -19, -19, 0, -19, -19, -19, -19, -19, -19, -19, -19, -19, -19, -19, -19, -19, -19, -19, 0, 0, -19, -19, -19, 0, -19, 0, 0, 0, 0, -19, -19, -19, 0, 0, 0, 0, 0, 0,
        // State 53
        -61, -61, -61, 0, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, -61, 0, 0, -61, -61, -61, 0, -61, 0, 0, 0, 0, -61, -61, -61, 0, 0, 0, 0, 0, 0,
        // State 54
        -18, -18, -18, 0, -18, -18, -18, -18, -18, -18, -18, -18, -18, -18, -18, -18, -18, -18, -18, 0, 0, -18, -18, -18, 0, -18, 0, 0, 0, 0, -18, -18, -18, 0, 0, 0, 0, 0, 0,
        // State 55
        -67, -67, -67, 0, -67, -67, -67, -67, -67, -67, -67, -67, -67, -67, -67, -67, -67, -67, -67, 0, 0, -67, -67, -67, 0, -67, 0, 0, 0, 0, -67, -67, -67, 0, 0, 0, 0, 0, 0,
        // State 56
        -86, -86, -86, 0, -86, -86, -86, -86, -86, -86, -86, -86, -86, -86, -86, -86, -86, -86, -86, 0, 0, -86, -86, -86, 0, -86, 0, 0, 0, 0, -86, -86, -86, 0, 0, 0, 0, 0, 0,
        // State 57
        -87, -87, -87, 0, -87, -87, -87, -87, -87, -87, -87, -87, -87, -87, -87, -87, -87, -87, -87, 0, 0, -87, -87, -87, 0, -87, 0, 0, 0, 0, -87, -87, -87, 0, 0, 0, 0, 0, 0,
        // State 58
        -63, -63, -63, 0, -63, -63, -63, -63, -63, -63, -63, -63, -63, -63, -63, -63, -63, -63, -63, 0, 0, -63, -63, -63, 0, -63, 0, 0, 0, 0, -63, -63, -63, 0, 0, 0, 0, 0, 0,
        // State 59
        -64, -64, -64, 0, -64, -64, -64, -64, -64, -64, -64, -64, -64, -64, -64, -64, -64, -64, -64, 0, 0, -64, -64, -64, 0, -64, 0, 0, 0, 0, -64, -64, -64, 0, 0, 0, 0, 0, 0,
        // State 60
        -57, -57, -57, -57, -57, -57, -57, -57, -57, -57, -57, -57, -57, -57, -57, -57, -57, -57, -57, 0, 0, -57, -57, -57, 0, -57, 0, 0, 0, 0, -57, -57, -57, 0, 0, 0, 0, 0, 0,
        // State 61
        -65, -65, -65, 0, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, -65, 0, 0, -65, -65, -65, 0, -65, 0, 0, 0, 0, -65, -65, -65, 0, 0, 0, 0, 0, 0,
        // State 62
        0, 0, 0, -73, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -73, -73, -73, 0, 0, -73, 0, -73, -73, -73, -73, 0, 0, 0, -73, -73, -73, -73, -73, -73,
        // State 63
        0, 0, 0, -77, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -77, -77, -77, 0, 0, -77, 0, -77, -77, -77, -77, 0, 0, 0, -77, -77, -77, -77, -77, -77,
        // State 64
        0, 0, 0, -75, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -75, -75, -75, 0, 0, -75, 0, -75, -75, -75, -75, 0, 0, 0, -75, -75, -75, -75, -75, -75,
        // State 65
        0, 0, 0, -72, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -72, -72, -72, 0, 0, -72, 0, -72, -72, -72, -72, 0, 0, 0, -72, -72, -72, -72, -72, -72,
        // State 66
        0, 0, 0, -76, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -76, -76, -76, 0, 0, -76, 0, -76, -76, -76, -76, 0, 0, 0, -76, -76, -76, -76, -76, -76,
        // State 67
        0, 0, 0, -74, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -74, -74, -74, 0, 0, -74, 0, -74, -74, -74, -74, 0, 0, 0, -74, -74, -74, -74, -74, -74,
        // State 68
        0, 0, 0, -78, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -78, -78, -78, 0, 0, -78, 0, -78, -78, -78, -78, 0, 0, 0, -78, -78, -78, -78, -78, -78,
        // State 69
        0, 0, 0, -79, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -79, -79, -79, 0, 0, -79, 0, -79, -79, -79, -79, 0, 0, 0, -79, -79, -79, -79, -79, -79,
        // State 70
        0, 0, 0, -80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -80, -80, -80, 0, 0, -80, 0, -80, -80, -80, -80, 0, 0, 0, -80, -80, -80, -80, -80, -80,
        // State 71
        0, 0, 0, -81, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -81, -81, -81, 0, 0, -81, 0, -81, -81, -81, -81, 0, 0, 0, -81, -81, -81, -81, -81, -81,
        // State 72
        0, 0, 0, -83, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -83, -83, -83, 0, 0, -83, 0, -83, -83, -83, -83, 0, 0, 0, -83, -83, -83, -83, -83, -83,
        // State 73
        0, 0, 0, -82, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -82, -82, -82, 0, 0, -82, 0, -82, -82, -82, -82, 0, 0, 0, -82, -82, -82, -82, -82, -82,
        // State 74
        0, 0, 0, -84, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -84, -84, -84, 0, 0, -84, 0, -84, -84, -84, -84, 0, 0, 0, -84, -84, -84, -84, -84, -84,
        // State 75
        0, 0, 0, -85, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -85, -85, -85, 0, 0, -85, 0, -85, -85, -85, -85, 0, 0, 0, -85, -85, -85, -85, -85, -85,
        // State 76
        -43, -43, -43, 0, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, -43, 0, 0, -43, -43, -43, 0, -43, 0, 0, 0, 0, -43, -43, -43, 0, 0, 0, 0, 0, 0,
        // State 77
        0, 0, 0, -70, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -70, -70, -70, 0, 0, -70, 0, -70, -70, -70, -70, 0, 0, 0, -70, -70, -70, -70, -70, -70,
        // State 78
        0, 0, 0, -71, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -71, -71, -71, 0, 0, -71, 0, -71, -71, -71, -71, 0, 0, 0, -71, -71, -71, -71, -71, -71,
        // State 79
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 89, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        // State 80
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 92, 0, 0, 0, 0, 0, 0,
        // State 81
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -69, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        // State 82
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        // State 83
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -68, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        // State 84
        -36, -36, -36, 0, -36, -36, -36, -36, -36, 0, -36, -36, 0, -36, -36, -36, -36, -36, 15, 0, 0, 0, -36, -36, 0, -36, 0, 0, 0, 0, 0, -36, -36, 0, 0, 0, 0, 0, 0,
        // State 85
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 33, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 0,
        // State 86
        -44, -44, -44, 0, -44, -44, -44, -44, -44, -44, -44, -44, -44, -44, -44, -44, -44, -44, -44, 0, 0, -44, -44, -44, 0, -44, 0, 0, 0, 0, -44, -44, -44, 0, 0, 0, 0, 0, 0,
        // State 87
        -53, -53, -53, 0, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, -53, 0, 0, -53, -53, -53, 0, -53, 0, 0, 0, 0, -53, -53, -53, 0, 0, 0, 0, 0, 0,
        // State 88
        -17, -17, -17, 0, -17, -17, -17, -17, -17, -17, -17, -17, -17, -17, -17, -17, -17, -17, -17, 0, 0, -17, -17, -17, 0, -17, 0, 0, 0, 0, -17, -17, -17, 0, 0, 0, 0, 0, 0,
        // State 89
        0, 0, 0, -9, -9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -9, -9, -9, -9, 0, -9, 0, -9, -9, -9, -9, 0, 0, 0, -9, -9, -9, -9, -9, -9,
        // State 90
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 36, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        // State 91
        -66, -66, -66, 0, -66, -66, -66, -66, -66, -66, -66, -66, -66, -66, -66, -66, -66, -66, -66, 0, 0, -66, -66, -66, 0, -66, 0, 0, 0, 0, -66, -66, -66, 0, 0, 0, 0, 0, 0,
        // State 92
        -40, -40, -40, 0, -40, -40, -40, -40, -40, 0, -40, -40, -40, -40, -40, -40, -40, -40, -40, 0, 0, 0, -40, -40, 0, -40, 0, 0, 0, 0, -40, -40, -40, 0, 0, 0, 0, 0, 0,
        // State 93
        -59, -59, -59, 0, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, -59, 0, 0, -59, -59, -59, 0, -59, 0, 0, 0, 0, -59, -59, -59, 0, 0, 0, 0, 0, 0,
        // State 94
        0, 0, 0, -10, -10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -10, -10, -10, -10, 0, -10, 0, -10, -10, -10, -10, 0, 0, 0, -10, -10, -10, -10, -10, -10,
        // State 95
        -38, -38, -38, 0, -38, -38, -38, -38, -38, 0, -38, -38, 0, -38, -38, -38, -38, -38, -38, 0, 0, 0, -38, -38, 0, -38, 0, 0, 0, 0, 16, -38, -38, 0, 0, 0, 0, 0, 0,
        // State 96
        0, 0, 0, 0, 99, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        // State 97
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -4, -4, -4, 0, 0, -4, 0,
        // State 98
        -14, -14, -14, 0, -14, -14, -14, -14, -14, 0, -14, -14, -14, -14, -14, -14, -14, -14, -14, 0, 0, 0, -14, -14, 0, -14, 0, 0, 0, 0, -14, -14, -14, 0, 0, 0, 0, 0, 0,
        // State 99
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 102, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        // State 100
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -5, -5, -5, 0, 0, -5, 0,
        // State 101
        -58, -58, -58, 0, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, -58, 0, 0, -58, -58, -58, 0, -58, 0, 0, 0, 0, -58, -58, -58, 0, 0, 0, 0, 0, 0,
    ];
    fn __action(state: i8, integer: usize) -> i8 {
        __ACTION[(state as usize) * 39 + integer]
    }
    const __EOF_ACTION: &[i8] = &[
        // State 0
        0,
        // State 1
        -29,
        // State 2
        -31,
        // State 3
        -33,
        // State 4
        -35,
        // State 5
        -42,
        // State 6
        -88,
        // State 7
        0,
        // State 8
        0,
        // State 9
        0,
        // State 10
        0,
        // State 11
        0,
        // State 12
        0,
        // State 13
        0,
        // State 14
        0,
        // State 15
        0,
        // State 16
        0,
        // State 17
        0,
        // State 18
        0,
        // State 19
        0,
        // State 20
        0,
        // State 21
        0,
        // State 22
        0,
        // State 23
        -30,
        // State 24
        -32,
        // State 25
        -34,
        // State 26
        -41,
        // State 27
        0,
        // State 28
        0,
        // State 29
        -28,
        // State 30
        0,
        // State 31
        0,
        // State 32
        0,
        // State 33
        0,
        // State 34
        0,
        // State 35
        0,
        // State 36
        0,
        // State 37
        0,
        // State 38
        0,
        // State 39
        -49,
        // State 40
        -47,
        // State 41
        -54,
        // State 42
        -37,
        // State 43
        -39,
        // State 44
        -45,
        // State 45
        -52,
        // State 46
        -51,
        // State 47
        -46,
        // State 48
        -50,
        // State 49
        -48,
        // State 50
        -62,
        // State 51
        -60,
        // State 52
        -19,
        // State 53
        -61,
        // State 54
        -18,
        // State 55
        -67,
        // State 56
        -86,
        // State 57
        -87,
        // State 58
        -63,
        // State 59
        -64,
        // State 60
        -57,
        // State 61
        -65,
        // State 62
        0,
        // State 63
        0,
        // State 64
        0,
        // State 65
        0,
        // State 66
        0,
        // State 67
        0,
        // State 68
        0,
        // State 69
        0,
        // State 70
        0,
        // State 71
        0,
        // State 72
        0,
        // State 73
        0,
        // State 74
        0,
        // State 75
        0,
        // State 76
        -43,
        // State 77
        0,
        // State 78
        0,
        // State 79
        0,
        // State 80
        0,
        // State 81
        0,
        // State 82
        0,
        // State 83
        0,
        // State 84
        -36,
        // State 85
        0,
        // State 86
        -44,
        // State 87
        -53,
        // State 88
        -17,
        // State 89
        0,
        // State 90
        0,
        // State 91
        -66,
        // State 92
        -40,
        // State 93
        -59,
        // State 94
        0,
        // State 95
        -38,
        // State 96
        0,
        // State 97
        0,
        // State 98
        -14,
        // State 99
        0,
        // State 100
        0,
        // State 101
        -58,
    ];
    fn __goto(state: i8, nt: usize) -> i8 {
        match nt {
            2 => 22,
            5 => 20,
            8 => 92,
            10 => 39,
            11 => 40,
            12 => 80,
            13 => match state {
                33 => 96,
                _ => 79,
            },
            14 => 41,
            15 => match state {
                18 => 29,
                _ => 1,
            },
            16 => match state {
                10 => 23,
                _ => 2,
            },
            17 => match state {
                11 => 24,
                _ => 3,
            },
            18 => match state {
                12 => 25,
                _ => 4,
            },
            19 => match state {
                13 => 84,
                _ => 42,
            },
            20 => match state {
                14 => 85,
                32 => 95,
                _ => 43,
            },
            21 => 5,
            22 => match state {
                37 => 99,
                _ => 44,
            },
            23 => match state {
                0 => 6,
                7 => 19,
                17 => 27,
                20 => 30,
                31 => 36,
                35 => 38,
                _ => 21,
            },
            25 => match state {
                15 => 26,
                28 => 34,
                9 | 22 => 81,
                16 => 86,
                _ => 45,
            },
            26 => 76,
            27 => 46,
            28 => 47,
            29 => 48,
            30 => match state {
                22 => 90,
                _ => 82,
            },
            31 => 18,
            32 => match state {
                34 => 37,
                _ => 10,
            },
            33 => 11,
            34 => 12,
            35 => 13,
            36 => match state {
                9 | 22 => 83,
                _ => 49,
            },
            _ => 0,
        }
    }
    fn __expected_tokens(__state: i8) -> alloc::vec::Vec<alloc::string::String> {
        const __TERMINAL: &[&str] = &[
            r###""!=""###,
            r###""%""###,
            r###""&&""###,
            r###""(""###,
            r###"")""###,
            r###""*""###,
            r###""+""###,
            r###"",""###,
            r###""-""###,
            r###"".""###,
            r###""/""###,
            r###""//""###,
            r###"":""###,
            r###""<""###,
            r###""<=""###,
            r###""==""###,
            r###"">""###,
            r###"">=""###,
            r###""?""###,
            r###""NULL""###,
            r###""Null""###,
            r###""[""###,
            r###""]""###,
            r###""^""###,
            r###""false""###,
            r###""in""###,
            r###""null""###,
            r###""true""###,
            r###""{""###,
            r###""{}""###,
            r###""|""###,
            r###""||""###,
            r###""}""###,
            r###"r#"\"([^\"\\\\]*(\\\\\")?)*\""#"###,
            r###"r#"'([^'\\\\]*(\\\\')?)*'"#"###,
            r###"r#"[0-9]+"#"###,
            r###"r#"[0-9]+\\.[0-9]*"#"###,
            r###"r#"[a-zA-Z_][a-zA-Z0-9_]*"#"###,
            r###"r#"\\.[0-9]+"#"###,
        ];
        __TERMINAL.iter().enumerate().filter_map(|(index, terminal)| {
            let next_state = __action(__state, index);
            if next_state == 0 {
                None
            } else {
                Some(alloc::string::ToString::to_string(terminal))
            }
        }).collect()
    }
    pub(crate) struct __StateMachine<'input>
    where
    {
        input: &'input str,
        __phantom: core::marker::PhantomData<(&'input ())>,
    }
    impl<'input> __state_machine::ParserDefinition for __StateMachine<'input>
    where
    {
        type Location = usize;
        type Error = &'static str;
        type Token = Token<'input>;
        type TokenIndex = usize;
        type Symbol = __Symbol<'input>;
        type Success = Box<Expression>;
        type StateIndex = i8;
        type Action = i8;
        type ReduceIndex = i8;
        type NonterminalIndex = usize;

        #[inline]
        fn start_location(&self) -> Self::Location {
              Default::default()
        }

        #[inline]
        fn start_state(&self) -> Self::StateIndex {
              0
        }

        #[inline]
        fn token_to_index(&self, token: &Self::Token) -> Option<usize> {
            __token_to_integer(token, core::marker::PhantomData::<(&())>)
        }

        #[inline]
        fn action(&self, state: i8, integer: usize) -> i8 {
            __action(state, integer)
        }

        #[inline]
        fn error_action(&self, state: i8) -> i8 {
            __action(state, 39 - 1)
        }

        #[inline]
        fn eof_action(&self, state: i8) -> i8 {
            __EOF_ACTION[state as usize]
        }

        #[inline]
        fn goto(&self, state: i8, nt: usize) -> i8 {
            __goto(state, nt)
        }

        fn token_to_symbol(&self, token_index: usize, token: Self::Token) -> Self::Symbol {
            __token_to_symbol(token_index, token, core::marker::PhantomData::<(&())>)
        }

        fn expected_tokens(&self, state: i8) -> alloc::vec::Vec<alloc::string::String> {
            __expected_tokens(state)
        }

        #[inline]
        fn uses_error_recovery(&self) -> bool {
            false
        }

        #[inline]
        fn error_recovery_symbol(
            &self,
            recovery: __state_machine::ErrorRecovery<Self>,
        ) -> Self::Symbol {
            panic!("error recovery not enabled for this grammar")
        }

        fn reduce(
            &mut self,
            action: i8,
            start_location: Option<&Self::Location>,
            states: &mut alloc::vec::Vec<i8>,
            symbols: &mut alloc::vec::Vec<__state_machine::SymbolTriple<Self>>,
        ) -> Option<__state_machine::ParseResult<Self>> {
            __reduce(
                self.input,
                action,
                start_location,
                states,
                symbols,
                core::marker::PhantomData::<(&())>,
            )
        }

        fn simulate_reduce(&self, action: i8) -> __state_machine::SimulatedReduce<Self> {
            panic!("error recovery not enabled for this grammar")
        }
    }
    fn __token_to_integer<
        'input,
    >(
        __token: &Token<'input>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> Option<usize>
    {
        match *__token {
            Token(6, _) if true => Some(0),
            Token(7, _) if true => Some(1),
            Token(8, _) if true => Some(2),
            Token(9, _) if true => Some(3),
            Token(10, _) if true => Some(4),
            Token(11, _) if true => Some(5),
            Token(12, _) if true => Some(6),
            Token(13, _) if true => Some(7),
            Token(14, _) if true => Some(8),
            Token(15, _) if true => Some(9),
            Token(16, _) if true => Some(10),
            Token(17, _) if true => Some(11),
            Token(18, _) if true => Some(12),
            Token(19, _) if true => Some(13),
            Token(20, _) if true => Some(14),
            Token(21, _) if true => Some(15),
            Token(22, _) if true => Some(16),
            Token(23, _) if true => Some(17),
            Token(24, _) if true => Some(18),
            Token(25, _) if true => Some(19),
            Token(26, _) if true => Some(20),
            Token(27, _) if true => Some(21),
            Token(28, _) if true => Some(22),
            Token(29, _) if true => Some(23),
            Token(30, _) if true => Some(24),
            Token(31, _) if true => Some(25),
            Token(32, _) if true => Some(26),
            Token(33, _) if true => Some(27),
            Token(34, _) if true => Some(28),
            Token(35, _) if true => Some(29),
            Token(36, _) if true => Some(30),
            Token(37, _) if true => Some(31),
            Token(38, _) if true => Some(32),
            Token(0, _) if true => Some(33),
            Token(1, _) if true => Some(34),
            Token(2, _) if true => Some(35),
            Token(3, _) if true => Some(36),
            Token(4, _) if true => Some(37),
            Token(5, _) if true => Some(38),
            _ => None,
        }
    }
    fn __token_to_symbol<
        'input,
    >(
        __token_index: usize,
        __token: Token<'input>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> __Symbol<'input>
    {
        match __token_index {
            0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 | 20 | 21 | 22 | 23 | 24 | 25 | 26 | 27 | 28 | 29 | 30 | 31 | 32 | 33 | 34 | 35 | 36 | 37 | 38 => match __token {
                Token(6, __tok0) | Token(7, __tok0) | Token(8, __tok0) | Token(9, __tok0) | Token(10, __tok0) | Token(11, __tok0) | Token(12, __tok0) | Token(13, __tok0) | Token(14, __tok0) | Token(15, __tok0) | Token(16, __tok0) | Token(17, __tok0) | Token(18, __tok0) | Token(19, __tok0) | Token(20, __tok0) | Token(21, __tok0) | Token(22, __tok0) | Token(23, __tok0) | Token(24, __tok0) | Token(25, __tok0) | Token(26, __tok0) | Token(27, __tok0) | Token(28, __tok0) | Token(29, __tok0) | Token(30, __tok0) | Token(31, __tok0) | Token(32, __tok0) | Token(33, __tok0) | Token(34, __tok0) | Token(35, __tok0) | Token(36, __tok0) | Token(37, __tok0) | Token(38, __tok0) | Token(0, __tok0) | Token(1, __tok0) | Token(2, __tok0) | Token(3, __tok0) | Token(4, __tok0) | Token(5, __tok0) if true => __Symbol::Variant0(__tok0),
                _ => unreachable!(),
            },
            _ => unreachable!(),
        }
    }
    pub struct ExpressionParser {
        builder: __lalrpop_util::lexer::MatcherBuilder,
        _priv: (),
    }

    impl ExpressionParser {
        pub fn new() -> ExpressionParser {
            let __builder = super::__intern_token::new_builder();
            ExpressionParser {
                builder: __builder,
                _priv: (),
            }
        }

        #[allow(dead_code)]
        pub fn parse<
            'input,
        >(
            &self,
            input: &'input str,
        ) -> Result<Box<Expression>, __lalrpop_util::ParseError<usize, Token<'input>, &'static str>>
        {
            let mut __tokens = self.builder.matcher(input);
            __state_machine::Parser::drive(
                __StateMachine {
                    input,
                    __phantom: core::marker::PhantomData::<(&())>,
                },
                __tokens,
            )
        }
    }
    pub(crate) fn __reduce<
        'input,
    >(
        input: &'input str,
        __action: i8,
        __lookahead_start: Option<&usize>,
        __states: &mut alloc::vec::Vec<i8>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> Option<Result<Box<Expression>,__lalrpop_util::ParseError<usize, Token<'input>, &'static str>>>
    {
        let (__pop_states, __nonterminal) = match __action {
            0 => {
                __reduce0(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            1 => {
                __reduce1(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            2 => {
                __reduce2(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            3 => {
                __reduce3(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            4 => {
                __reduce4(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            5 => {
                __reduce5(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            6 => {
                __reduce6(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            7 => {
                __reduce7(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            8 => {
                __reduce8(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            9 => {
                __reduce9(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            10 => {
                __reduce10(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            11 => {
                __reduce11(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            12 => {
                __reduce12(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            13 => {
                __reduce13(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            14 => {
                __reduce14(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            15 => {
                __reduce15(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            16 => {
                __reduce16(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            17 => {
                __reduce17(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            18 => {
                __reduce18(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            19 => {
                __reduce19(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            20 => {
                __reduce20(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            21 => {
                __reduce21(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            22 => {
                __reduce22(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            23 => {
                __reduce23(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            24 => {
                __reduce24(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            25 => {
                __reduce25(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            26 => {
                __reduce26(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            27 => {
                __reduce27(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            28 => {
                __reduce28(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            29 => {
                __reduce29(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            30 => {
                __reduce30(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            31 => {
                __reduce31(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            32 => {
                __reduce32(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            33 => {
                __reduce33(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            34 => {
                __reduce34(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            35 => {
                __reduce35(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            36 => {
                __reduce36(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            37 => {
                __reduce37(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            38 => {
                __reduce38(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            39 => {
                __reduce39(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            40 => {
                __reduce40(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            41 => {
                __reduce41(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            42 => {
                __reduce42(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            43 => {
                __reduce43(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            44 => {
                __reduce44(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            45 => {
                __reduce45(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            46 => {
                __reduce46(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            47 => {
                __reduce47(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            48 => {
                __reduce48(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            49 => {
                __reduce49(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            50 => {
                __reduce50(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            51 => {
                __reduce51(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            52 => {
                __reduce52(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            53 => {
                __reduce53(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            54 => {
                __reduce54(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            55 => {
                __reduce55(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            56 => {
                __reduce56(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            57 => {
                __reduce57(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            58 => {
                __reduce58(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            59 => {
                __reduce59(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            60 => {
                __reduce60(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            61 => {
                __reduce61(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            62 => {
                __reduce62(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            63 => {
                __reduce63(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            64 => {
                __reduce64(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            65 => {
                __reduce65(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            66 => {
                __reduce66(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            67 => {
                __reduce67(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            68 => {
                __reduce68(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            69 => {
                __reduce69(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            70 => {
                __reduce70(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            71 => {
                __reduce71(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            72 => {
                __reduce72(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            73 => {
                __reduce73(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            74 => {
                __reduce74(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            75 => {
                __reduce75(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            76 => {
                __reduce76(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            77 => {
                __reduce77(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            78 => {
                __reduce78(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            79 => {
                __reduce79(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            80 => {
                __reduce80(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            81 => {
                __reduce81(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            82 => {
                __reduce82(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            83 => {
                __reduce83(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            84 => {
                __reduce84(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            85 => {
                __reduce85(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            86 => {
                __reduce86(input, __lookahead_start, __symbols, core::marker::PhantomData::<(&())>)
            }
            87 => {
                // __Expression = Expression => ActionFn(0);
                let __sym0 = __pop_Variant3(__symbols);
                let __start = __sym0.0.clone();
                let __end = __sym0.2.clone();
                let __nt = super::__action0::<>(input, __sym0);
                return Some(Ok(__nt));
            }
            _ => panic!("invalid action code {}", __action)
        };
        let __states_len = __states.len();
        __states.truncate(__states_len - __pop_states);
        let __state = *__states.last().unwrap();
        let __next_state = __goto(__state, __nonterminal);
        __states.push(__next_state);
        None
    }
    #[inline(never)]
    fn __symbol_type_mismatch() -> ! {
        panic!("symbol type mismatch")
    }
    fn __pop_Variant1<
      'input,
    >(
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>
    ) -> (usize, (String, Box<Expression>), usize)
     {
        match __symbols.pop() {
            Some((__l, __Symbol::Variant1(__v), __r)) => (__l, __v, __r),
            _ => __symbol_type_mismatch()
        }
    }
    fn __pop_Variant3<
      'input,
    >(
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>
    ) -> (usize, Box<Expression>, usize)
     {
        match __symbols.pop() {
            Some((__l, __Symbol::Variant3(__v), __r)) => (__l, __v, __r),
            _ => __symbol_type_mismatch()
        }
    }
    fn __pop_Variant14<
      'input,
    >(
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>
    ) -> (usize, OpCode, usize)
     {
        match __symbols.pop() {
            Some((__l, __Symbol::Variant14(__v), __r)) => (__l, __v, __r),
            _ => __symbol_type_mismatch()
        }
    }
    fn __pop_Variant12<
      'input,
    >(
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>
    ) -> (usize, Option<Box<Expression>>, usize)
     {
        match __symbols.pop() {
            Some((__l, __Symbol::Variant12(__v), __r)) => (__l, __v, __r),
            _ => __symbol_type_mismatch()
        }
    }
    fn __pop_Variant11<
      'input,
    >(
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>
    ) -> (usize, String, usize)
     {
        match __symbols.pop() {
            Some((__l, __Symbol::Variant11(__v), __r)) => (__l, __v, __r),
            _ => __symbol_type_mismatch()
        }
    }
    fn __pop_Variant9<
      'input,
    >(
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>
    ) -> (usize, Vec<(String, Box<Expression>)>, usize)
     {
        match __symbols.pop() {
            Some((__l, __Symbol::Variant9(__v), __r)) => (__l, __v, __r),
            _ => __symbol_type_mismatch()
        }
    }
    fn __pop_Variant6<
      'input,
    >(
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>
    ) -> (usize, Vec<Box<Expression>>, usize)
     {
        match __symbols.pop() {
            Some((__l, __Symbol::Variant6(__v), __r)) => (__l, __v, __r),
            _ => __symbol_type_mismatch()
        }
    }
    fn __pop_Variant2<
      'input,
    >(
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>
    ) -> (usize, alloc::vec::Vec<(String, Box<Expression>)>, usize)
     {
        match __symbols.pop() {
            Some((__l, __Symbol::Variant2(__v), __r)) => (__l, __v, __r),
            _ => __symbol_type_mismatch()
        }
    }
    fn __pop_Variant4<
      'input,
    >(
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>
    ) -> (usize, alloc::vec::Vec<Box<Expression>>, usize)
     {
        match __symbols.pop() {
            Some((__l, __Symbol::Variant4(__v), __r)) => (__l, __v, __r),
            _ => __symbol_type_mismatch()
        }
    }
    fn __pop_Variant8<
      'input,
    >(
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>
    ) -> (usize, bool, usize)
     {
        match __symbols.pop() {
            Some((__l, __Symbol::Variant8(__v), __r)) => (__l, __v, __r),
            _ => __symbol_type_mismatch()
        }
    }
    fn __pop_Variant5<
      'input,
    >(
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>
    ) -> (usize, core::option::Option<(String, Box<Expression>)>, usize)
     {
        match __symbols.pop() {
            Some((__l, __Symbol::Variant5(__v), __r)) => (__l, __v, __r),
            _ => __symbol_type_mismatch()
        }
    }
    fn __pop_Variant10<
      'input,
    >(
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>
    ) -> (usize, core::option::Option<Box<Expression>>, usize)
     {
        match __symbols.pop() {
            Some((__l, __Symbol::Variant10(__v), __r)) => (__l, __v, __r),
            _ => __symbol_type_mismatch()
        }
    }
    fn __pop_Variant7<
      'input,
    >(
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>
    ) -> (usize, core::option::Option<Vec<Box<Expression>>>, usize)
     {
        match __symbols.pop() {
            Some((__l, __Symbol::Variant7(__v), __r)) => (__l, __v, __r),
            _ => __symbol_type_mismatch()
        }
    }
    fn __pop_Variant13<
      'input,
    >(
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>
    ) -> (usize, f64, usize)
     {
        match __symbols.pop() {
            Some((__l, __Symbol::Variant13(__v), __r)) => (__l, __v, __r),
            _ => __symbol_type_mismatch()
        }
    }
    fn __pop_Variant0<
      'input,
    >(
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>
    ) -> (usize, &'input str, usize)
     {
        match __symbols.pop() {
            Some((__l, __Symbol::Variant0(__v), __r)) => (__l, __v, __r),
            _ => __symbol_type_mismatch()
        }
    }
    pub(crate) fn __reduce0<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // (<(<ObjectIdentifier> ":" <Expression>)> ",") = ObjectIdentifier, ":", Expression, "," => ActionFn(81);
        assert!(__symbols.len() >= 4);
        let __sym3 = __pop_Variant0(__symbols);
        let __sym2 = __pop_Variant3(__symbols);
        let __sym1 = __pop_Variant0(__symbols);
        let __sym0 = __pop_Variant11(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym3.2.clone();
        let __nt = super::__action81::<>(input, __sym0, __sym1, __sym2, __sym3);
        __symbols.push((__start, __Symbol::Variant1(__nt), __end));
        (4, 0)
    }
    pub(crate) fn __reduce1<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // (<(<ObjectIdentifier> ":" <Expression>)> ",")* =  => ActionFn(74);
        let __start = __lookahead_start.cloned().or_else(|| __symbols.last().map(|s| s.2.clone())).unwrap_or_default();
        let __end = __start.clone();
        let __nt = super::__action74::<>(input, &__start, &__end);
        __symbols.push((__start, __Symbol::Variant2(__nt), __end));
        (0, 1)
    }
    pub(crate) fn __reduce2<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // (<(<ObjectIdentifier> ":" <Expression>)> ",")* = (<(<ObjectIdentifier> ":" <Expression>)> ",")+ => ActionFn(75);
        let __sym0 = __pop_Variant2(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action75::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant2(__nt), __end));
        (1, 1)
    }
    pub(crate) fn __reduce3<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // (<(<ObjectIdentifier> ":" <Expression>)> ",")+ = ObjectIdentifier, ":", Expression, "," => ActionFn(83);
        assert!(__symbols.len() >= 4);
        let __sym3 = __pop_Variant0(__symbols);
        let __sym2 = __pop_Variant3(__symbols);
        let __sym1 = __pop_Variant0(__symbols);
        let __sym0 = __pop_Variant11(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym3.2.clone();
        let __nt = super::__action83::<>(input, __sym0, __sym1, __sym2, __sym3);
        __symbols.push((__start, __Symbol::Variant2(__nt), __end));
        (4, 2)
    }
    pub(crate) fn __reduce4<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // (<(<ObjectIdentifier> ":" <Expression>)> ",")+ = (<(<ObjectIdentifier> ":" <Expression>)> ",")+, ObjectIdentifier, ":", Expression, "," => ActionFn(84);
        assert!(__symbols.len() >= 5);
        let __sym4 = __pop_Variant0(__symbols);
        let __sym3 = __pop_Variant3(__symbols);
        let __sym2 = __pop_Variant0(__symbols);
        let __sym1 = __pop_Variant11(__symbols);
        let __sym0 = __pop_Variant2(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym4.2.clone();
        let __nt = super::__action84::<>(input, __sym0, __sym1, __sym2, __sym3, __sym4);
        __symbols.push((__start, __Symbol::Variant2(__nt), __end));
        (5, 2)
    }
    pub(crate) fn __reduce5<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // (<Expression> ",") = Expression, "," => ActionFn(71);
        assert!(__symbols.len() >= 2);
        let __sym1 = __pop_Variant0(__symbols);
        let __sym0 = __pop_Variant3(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym1.2.clone();
        let __nt = super::__action71::<>(input, __sym0, __sym1);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (2, 3)
    }
    pub(crate) fn __reduce6<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // (<Expression> ",")* =  => ActionFn(69);
        let __start = __lookahead_start.cloned().or_else(|| __symbols.last().map(|s| s.2.clone())).unwrap_or_default();
        let __end = __start.clone();
        let __nt = super::__action69::<>(input, &__start, &__end);
        __symbols.push((__start, __Symbol::Variant4(__nt), __end));
        (0, 4)
    }
    pub(crate) fn __reduce7<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // (<Expression> ",")* = (<Expression> ",")+ => ActionFn(70);
        let __sym0 = __pop_Variant4(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action70::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant4(__nt), __end));
        (1, 4)
    }
    pub(crate) fn __reduce8<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // (<Expression> ",")+ = Expression, "," => ActionFn(87);
        assert!(__symbols.len() >= 2);
        let __sym1 = __pop_Variant0(__symbols);
        let __sym0 = __pop_Variant3(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym1.2.clone();
        let __nt = super::__action87::<>(input, __sym0, __sym1);
        __symbols.push((__start, __Symbol::Variant4(__nt), __end));
        (2, 5)
    }
    pub(crate) fn __reduce9<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // (<Expression> ",")+ = (<Expression> ",")+, Expression, "," => ActionFn(88);
        assert!(__symbols.len() >= 3);
        let __sym2 = __pop_Variant0(__symbols);
        let __sym1 = __pop_Variant3(__symbols);
        let __sym0 = __pop_Variant4(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym2.2.clone();
        let __nt = super::__action88::<>(input, __sym0, __sym1, __sym2);
        __symbols.push((__start, __Symbol::Variant4(__nt), __end));
        (3, 5)
    }
    pub(crate) fn __reduce10<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // (<ObjectIdentifier> ":" <Expression>) = ObjectIdentifier, ":", Expression => ActionFn(63);
        assert!(__symbols.len() >= 3);
        let __sym2 = __pop_Variant3(__symbols);
        let __sym1 = __pop_Variant0(__symbols);
        let __sym0 = __pop_Variant11(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym2.2.clone();
        let __nt = super::__action63::<>(input, __sym0, __sym1, __sym2);
        __symbols.push((__start, __Symbol::Variant1(__nt), __end));
        (3, 6)
    }
    pub(crate) fn __reduce11<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // (<ObjectIdentifier> ":" <Expression>)? = ObjectIdentifier, ":", Expression => ActionFn(82);
        assert!(__symbols.len() >= 3);
        let __sym2 = __pop_Variant3(__symbols);
        let __sym1 = __pop_Variant0(__symbols);
        let __sym0 = __pop_Variant11(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym2.2.clone();
        let __nt = super::__action82::<>(input, __sym0, __sym1, __sym2);
        __symbols.push((__start, __Symbol::Variant5(__nt), __end));
        (3, 7)
    }
    pub(crate) fn __reduce12<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // (<ObjectIdentifier> ":" <Expression>)? =  => ActionFn(73);
        let __start = __lookahead_start.cloned().or_else(|| __symbols.last().map(|s| s.2.clone())).unwrap_or_default();
        let __end = __start.clone();
        let __nt = super::__action73::<>(input, &__start, &__end);
        __symbols.push((__start, __Symbol::Variant5(__nt), __end));
        (0, 7)
    }
    pub(crate) fn __reduce13<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Args = "(", Comma<Expression>, ")" => ActionFn(27);
        assert!(__symbols.len() >= 3);
        let __sym2 = __pop_Variant0(__symbols);
        let __sym1 = __pop_Variant6(__symbols);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym2.2.clone();
        let __nt = super::__action27::<>(input, __sym0, __sym1, __sym2);
        __symbols.push((__start, __Symbol::Variant6(__nt), __end));
        (3, 8)
    }
    pub(crate) fn __reduce14<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Args? = Args => ActionFn(65);
        let __sym0 = __pop_Variant6(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action65::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant7(__nt), __end));
        (1, 9)
    }
    pub(crate) fn __reduce15<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Args? =  => ActionFn(66);
        let __start = __lookahead_start.cloned().or_else(|| __symbols.last().map(|s| s.2.clone())).unwrap_or_default();
        let __end = __start.clone();
        let __nt = super::__action66::<>(input, &__start, &__end);
        __symbols.push((__start, __Symbol::Variant7(__nt), __end));
        (0, 9)
    }
    pub(crate) fn __reduce16<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Array = "[", Comma<Expression>, "]" => ActionFn(57);
        assert!(__symbols.len() >= 3);
        let __sym2 = __pop_Variant0(__symbols);
        let __sym1 = __pop_Variant6(__symbols);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym2.2.clone();
        let __nt = super::__action57::<>(input, __sym0, __sym1, __sym2);
        __symbols.push((__start, __Symbol::Variant6(__nt), __end));
        (3, 10)
    }
    pub(crate) fn __reduce17<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Boolean = "true" => ActionFn(55);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action55::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant8(__nt), __end));
        (1, 11)
    }
    pub(crate) fn __reduce18<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Boolean = "false" => ActionFn(56);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action56::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant8(__nt), __end));
        (1, 11)
    }
    pub(crate) fn __reduce19<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Comma<(<ObjectIdentifier> ":" <Expression>)> = ObjectIdentifier, ":", Expression => ActionFn(91);
        assert!(__symbols.len() >= 3);
        let __sym2 = __pop_Variant3(__symbols);
        let __sym1 = __pop_Variant0(__symbols);
        let __sym0 = __pop_Variant11(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym2.2.clone();
        let __nt = super::__action91::<>(input, __sym0, __sym1, __sym2);
        __symbols.push((__start, __Symbol::Variant9(__nt), __end));
        (3, 12)
    }
    pub(crate) fn __reduce20<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Comma<(<ObjectIdentifier> ":" <Expression>)> =  => ActionFn(92);
        let __start = __lookahead_start.cloned().or_else(|| __symbols.last().map(|s| s.2.clone())).unwrap_or_default();
        let __end = __start.clone();
        let __nt = super::__action92::<>(input, &__start, &__end);
        __symbols.push((__start, __Symbol::Variant9(__nt), __end));
        (0, 12)
    }
    pub(crate) fn __reduce21<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Comma<(<ObjectIdentifier> ":" <Expression>)> = (<(<ObjectIdentifier> ":" <Expression>)> ",")+, ObjectIdentifier, ":", Expression => ActionFn(93);
        assert!(__symbols.len() >= 4);
        let __sym3 = __pop_Variant3(__symbols);
        let __sym2 = __pop_Variant0(__symbols);
        let __sym1 = __pop_Variant11(__symbols);
        let __sym0 = __pop_Variant2(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym3.2.clone();
        let __nt = super::__action93::<>(input, __sym0, __sym1, __sym2, __sym3);
        __symbols.push((__start, __Symbol::Variant9(__nt), __end));
        (4, 12)
    }
    pub(crate) fn __reduce22<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Comma<(<ObjectIdentifier> ":" <Expression>)> = (<(<ObjectIdentifier> ":" <Expression>)> ",")+ => ActionFn(94);
        let __sym0 = __pop_Variant2(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action94::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant9(__nt), __end));
        (1, 12)
    }
    pub(crate) fn __reduce23<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Comma<Expression> = Expression => ActionFn(97);
        let __sym0 = __pop_Variant3(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action97::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant6(__nt), __end));
        (1, 13)
    }
    pub(crate) fn __reduce24<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Comma<Expression> =  => ActionFn(98);
        let __start = __lookahead_start.cloned().or_else(|| __symbols.last().map(|s| s.2.clone())).unwrap_or_default();
        let __end = __start.clone();
        let __nt = super::__action98::<>(input, &__start, &__end);
        __symbols.push((__start, __Symbol::Variant6(__nt), __end));
        (0, 13)
    }
    pub(crate) fn __reduce25<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Comma<Expression> = (<Expression> ",")+, Expression => ActionFn(99);
        assert!(__symbols.len() >= 2);
        let __sym1 = __pop_Variant3(__symbols);
        let __sym0 = __pop_Variant4(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym1.2.clone();
        let __nt = super::__action99::<>(input, __sym0, __sym1);
        __symbols.push((__start, __Symbol::Variant6(__nt), __end));
        (2, 13)
    }
    pub(crate) fn __reduce26<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Comma<Expression> = (<Expression> ",")+ => ActionFn(100);
        let __sym0 = __pop_Variant4(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action100::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant6(__nt), __end));
        (1, 13)
    }
    pub(crate) fn __reduce27<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Expr00 = Expression, Op10, Expr10 => ActionFn(2);
        assert!(__symbols.len() >= 3);
        let __sym2 = __pop_Variant3(__symbols);
        let __sym1 = __pop_Variant14(__symbols);
        let __sym0 = __pop_Variant3(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym2.2.clone();
        let __nt = super::__action2::<>(input, __sym0, __sym1, __sym2);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (3, 14)
    }
    pub(crate) fn __reduce28<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Expr00 = Expr10 => ActionFn(3);
        let __sym0 = __pop_Variant3(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action3::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (1, 14)
    }
    pub(crate) fn __reduce29<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Expr10 = Expr10, Op20, Expr20 => ActionFn(4);
        assert!(__symbols.len() >= 3);
        let __sym2 = __pop_Variant3(__symbols);
        let __sym1 = __pop_Variant14(__symbols);
        let __sym0 = __pop_Variant3(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym2.2.clone();
        let __nt = super::__action4::<>(input, __sym0, __sym1, __sym2);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (3, 15)
    }
    pub(crate) fn __reduce30<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Expr10 = Expr20 => ActionFn(5);
        let __sym0 = __pop_Variant3(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action5::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (1, 15)
    }
    pub(crate) fn __reduce31<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Expr20 = Expr20, Op30, Expr30 => ActionFn(6);
        assert!(__symbols.len() >= 3);
        let __sym2 = __pop_Variant3(__symbols);
        let __sym1 = __pop_Variant14(__symbols);
        let __sym0 = __pop_Variant3(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym2.2.clone();
        let __nt = super::__action6::<>(input, __sym0, __sym1, __sym2);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (3, 16)
    }
    pub(crate) fn __reduce32<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Expr20 = Expr30 => ActionFn(7);
        let __sym0 = __pop_Variant3(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action7::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (1, 16)
    }
    pub(crate) fn __reduce33<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Expr30 = Expr30, Op40, Expr40 => ActionFn(8);
        assert!(__symbols.len() >= 3);
        let __sym2 = __pop_Variant3(__symbols);
        let __sym1 = __pop_Variant14(__symbols);
        let __sym0 = __pop_Variant3(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym2.2.clone();
        let __nt = super::__action8::<>(input, __sym0, __sym1, __sym2);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (3, 17)
    }
    pub(crate) fn __reduce34<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Expr30 = Expr40 => ActionFn(9);
        let __sym0 = __pop_Variant3(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action9::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (1, 17)
    }
    pub(crate) fn __reduce35<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Expr40 = Expr40, Op50, Expr50 => ActionFn(10);
        assert!(__symbols.len() >= 3);
        let __sym2 = __pop_Variant3(__symbols);
        let __sym1 = __pop_Variant14(__symbols);
        let __sym0 = __pop_Variant3(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym2.2.clone();
        let __nt = super::__action10::<>(input, __sym0, __sym1, __sym2);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (3, 18)
    }
    pub(crate) fn __reduce36<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Expr40 = Expr50 => ActionFn(11);
        let __sym0 = __pop_Variant3(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action11::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (1, 18)
    }
    pub(crate) fn __reduce37<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Expr50 = Expr50, "?", Expr60, ":", Expr60 => ActionFn(12);
        assert!(__symbols.len() >= 5);
        let __sym4 = __pop_Variant3(__symbols);
        let __sym3 = __pop_Variant0(__symbols);
        let __sym2 = __pop_Variant3(__symbols);
        let __sym1 = __pop_Variant0(__symbols);
        let __sym0 = __pop_Variant3(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym4.2.clone();
        let __nt = super::__action12::<>(input, __sym0, __sym1, __sym2, __sym3, __sym4);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (5, 19)
    }
    pub(crate) fn __reduce38<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Expr50 = Expr60 => ActionFn(13);
        let __sym0 = __pop_Variant3(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action13::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (1, 19)
    }
    pub(crate) fn __reduce39<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Expr60 = Expr60, "|", Identifier, Args => ActionFn(95);
        assert!(__symbols.len() >= 4);
        let __sym3 = __pop_Variant6(__symbols);
        let __sym2 = __pop_Variant11(__symbols);
        let __sym1 = __pop_Variant0(__symbols);
        let __sym0 = __pop_Variant3(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym3.2.clone();
        let __nt = super::__action95::<>(input, __sym0, __sym1, __sym2, __sym3);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (4, 20)
    }
    pub(crate) fn __reduce40<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Expr60 = Expr60, "|", Identifier => ActionFn(96);
        assert!(__symbols.len() >= 3);
        let __sym2 = __pop_Variant11(__symbols);
        let __sym1 = __pop_Variant0(__symbols);
        let __sym0 = __pop_Variant3(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym2.2.clone();
        let __nt = super::__action96::<>(input, __sym0, __sym1, __sym2);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (3, 20)
    }
    pub(crate) fn __reduce41<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Expr60 = Expr70 => ActionFn(15);
        let __sym0 = __pop_Variant3(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action15::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (1, 20)
    }
    pub(crate) fn __reduce42<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Expr70 = Expr70, Index => ActionFn(16);
        assert!(__symbols.len() >= 2);
        let __sym1 = __pop_Variant3(__symbols);
        let __sym0 = __pop_Variant3(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym1.2.clone();
        let __nt = super::__action16::<>(input, __sym0, __sym1);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (2, 21)
    }
    pub(crate) fn __reduce43<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Expr70 = Expr70, ".", Identifier => ActionFn(17);
        assert!(__symbols.len() >= 3);
        let __sym2 = __pop_Variant11(__symbols);
        let __sym1 = __pop_Variant0(__symbols);
        let __sym0 = __pop_Variant3(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym2.2.clone();
        let __nt = super::__action17::<>(input, __sym0, __sym1, __sym2);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (3, 21)
    }
    pub(crate) fn __reduce44<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Expr70 = Expr80 => ActionFn(18);
        let __sym0 = __pop_Variant3(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action18::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (1, 21)
    }
    pub(crate) fn __reduce45<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Expr80 = Number => ActionFn(19);
        let __sym0 = __pop_Variant13(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action19::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (1, 22)
    }
    pub(crate) fn __reduce46<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Expr80 = Boolean => ActionFn(20);
        let __sym0 = __pop_Variant8(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action20::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (1, 22)
    }
    pub(crate) fn __reduce47<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Expr80 = String => ActionFn(21);
        let __sym0 = __pop_Variant11(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action21::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (1, 22)
    }
    pub(crate) fn __reduce48<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Expr80 = Array => ActionFn(22);
        let __sym0 = __pop_Variant6(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action22::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (1, 22)
    }
    pub(crate) fn __reduce49<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Expr80 = Object => ActionFn(23);
        let __sym0 = __pop_Variant9(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action23::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (1, 22)
    }
    pub(crate) fn __reduce50<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Expr80 = Null => ActionFn(24);
        let __sym0 = __pop_Variant12(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action24::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (1, 22)
    }
    pub(crate) fn __reduce51<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Expr80 = Identifier => ActionFn(25);
        let __sym0 = __pop_Variant11(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action25::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (1, 22)
    }
    pub(crate) fn __reduce52<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Expr80 = "(", Expression, ")" => ActionFn(26);
        assert!(__symbols.len() >= 3);
        let __sym2 = __pop_Variant0(__symbols);
        let __sym1 = __pop_Variant3(__symbols);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym2.2.clone();
        let __nt = super::__action26::<>(input, __sym0, __sym1, __sym2);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (3, 22)
    }
    pub(crate) fn __reduce53<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Expression = Expr00 => ActionFn(1);
        let __sym0 = __pop_Variant3(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action1::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (1, 23)
    }
    pub(crate) fn __reduce54<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Expression? = Expression => ActionFn(67);
        let __sym0 = __pop_Variant3(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action67::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant10(__nt), __end));
        (1, 24)
    }
    pub(crate) fn __reduce55<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Expression? =  => ActionFn(68);
        let __start = __lookahead_start.cloned().or_else(|| __symbols.last().map(|s| s.2.clone())).unwrap_or_default();
        let __end = __start.clone();
        let __nt = super::__action68::<>(input, &__start, &__end);
        __symbols.push((__start, __Symbol::Variant10(__nt), __end));
        (0, 24)
    }
    pub(crate) fn __reduce56<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Identifier = r#"[a-zA-Z_][a-zA-Z0-9_]*"# => ActionFn(52);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action52::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant11(__nt), __end));
        (1, 25)
    }
    pub(crate) fn __reduce57<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Index = "[", ".", Identifier, Op20, Expr80, "]" => ActionFn(53);
        assert!(__symbols.len() >= 6);
        let __sym5 = __pop_Variant0(__symbols);
        let __sym4 = __pop_Variant3(__symbols);
        let __sym3 = __pop_Variant14(__symbols);
        let __sym2 = __pop_Variant11(__symbols);
        let __sym1 = __pop_Variant0(__symbols);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym5.2.clone();
        let __nt = super::__action53::<>(input, __sym0, __sym1, __sym2, __sym3, __sym4, __sym5);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (6, 26)
    }
    pub(crate) fn __reduce58<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Index = "[", Expression, "]" => ActionFn(54);
        assert!(__symbols.len() >= 3);
        let __sym2 = __pop_Variant0(__symbols);
        let __sym1 = __pop_Variant3(__symbols);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym2.2.clone();
        let __nt = super::__action54::<>(input, __sym0, __sym1, __sym2);
        __symbols.push((__start, __Symbol::Variant3(__nt), __end));
        (3, 26)
    }
    pub(crate) fn __reduce59<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Null = "Null" => ActionFn(49);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action49::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant12(__nt), __end));
        (1, 27)
    }
    pub(crate) fn __reduce60<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Null = "null" => ActionFn(50);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action50::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant12(__nt), __end));
        (1, 27)
    }
    pub(crate) fn __reduce61<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Null = "NULL" => ActionFn(51);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action51::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant12(__nt), __end));
        (1, 27)
    }
    pub(crate) fn __reduce62<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Number = r#"[0-9]+"# => ActionFn(44);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action44::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant13(__nt), __end));
        (1, 28)
    }
    pub(crate) fn __reduce63<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Number = r#"[0-9]+\\.[0-9]*"# => ActionFn(45);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action45::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant13(__nt), __end));
        (1, 28)
    }
    pub(crate) fn __reduce64<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Number = r#"\\.[0-9]+"# => ActionFn(46);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action46::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant13(__nt), __end));
        (1, 28)
    }
    pub(crate) fn __reduce65<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Object = "{", Comma<(<ObjectIdentifier> ":" <Expression>)>, "}" => ActionFn(58);
        assert!(__symbols.len() >= 3);
        let __sym2 = __pop_Variant0(__symbols);
        let __sym1 = __pop_Variant9(__symbols);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym2.2.clone();
        let __nt = super::__action58::<>(input, __sym0, __sym1, __sym2);
        __symbols.push((__start, __Symbol::Variant9(__nt), __end));
        (3, 29)
    }
    pub(crate) fn __reduce66<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Object = "{}" => ActionFn(59);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action59::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant9(__nt), __end));
        (1, 29)
    }
    pub(crate) fn __reduce67<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // ObjectIdentifier = String => ActionFn(60);
        let __sym0 = __pop_Variant11(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action60::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant11(__nt), __end));
        (1, 30)
    }
    pub(crate) fn __reduce68<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // ObjectIdentifier = Identifier => ActionFn(61);
        let __sym0 = __pop_Variant11(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action61::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant11(__nt), __end));
        (1, 30)
    }
    pub(crate) fn __reduce69<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Op10 = "&&" => ActionFn(28);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action28::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant14(__nt), __end));
        (1, 31)
    }
    pub(crate) fn __reduce70<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Op10 = "||" => ActionFn(29);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action29::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant14(__nt), __end));
        (1, 31)
    }
    pub(crate) fn __reduce71<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Op20 = "==" => ActionFn(30);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action30::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant14(__nt), __end));
        (1, 32)
    }
    pub(crate) fn __reduce72<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Op20 = "!=" => ActionFn(31);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action31::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant14(__nt), __end));
        (1, 32)
    }
    pub(crate) fn __reduce73<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Op20 = ">=" => ActionFn(32);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action32::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant14(__nt), __end));
        (1, 32)
    }
    pub(crate) fn __reduce74<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Op20 = "<=" => ActionFn(33);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action33::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant14(__nt), __end));
        (1, 32)
    }
    pub(crate) fn __reduce75<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Op20 = ">" => ActionFn(34);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action34::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant14(__nt), __end));
        (1, 32)
    }
    pub(crate) fn __reduce76<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Op20 = "<" => ActionFn(35);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action35::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant14(__nt), __end));
        (1, 32)
    }
    pub(crate) fn __reduce77<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Op20 = "in" => ActionFn(36);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action36::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant14(__nt), __end));
        (1, 32)
    }
    pub(crate) fn __reduce78<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Op30 = "+" => ActionFn(37);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action37::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant14(__nt), __end));
        (1, 33)
    }
    pub(crate) fn __reduce79<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Op30 = "-" => ActionFn(38);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action38::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant14(__nt), __end));
        (1, 33)
    }
    pub(crate) fn __reduce80<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Op40 = "*" => ActionFn(39);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action39::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant14(__nt), __end));
        (1, 34)
    }
    pub(crate) fn __reduce81<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Op40 = "//" => ActionFn(40);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action40::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant14(__nt), __end));
        (1, 34)
    }
    pub(crate) fn __reduce82<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Op40 = "/" => ActionFn(41);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action41::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant14(__nt), __end));
        (1, 34)
    }
    pub(crate) fn __reduce83<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Op50 = "%" => ActionFn(42);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action42::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant14(__nt), __end));
        (1, 35)
    }
    pub(crate) fn __reduce84<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // Op50 = "^" => ActionFn(43);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action43::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant14(__nt), __end));
        (1, 35)
    }
    pub(crate) fn __reduce85<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // String = r#"\"([^\"\\\\]*(\\\\\")?)*\""# => ActionFn(47);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action47::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant11(__nt), __end));
        (1, 36)
    }
    pub(crate) fn __reduce86<
        'input,
    >(
        input: &'input str,
        __lookahead_start: Option<&usize>,
        __symbols: &mut alloc::vec::Vec<(usize,__Symbol<'input>,usize)>,
        _: core::marker::PhantomData<(&'input ())>,
    ) -> (usize, usize)
    {
        // String = r#"'([^'\\\\]*(\\\\')?)*'"# => ActionFn(48);
        let __sym0 = __pop_Variant0(__symbols);
        let __start = __sym0.0.clone();
        let __end = __sym0.2.clone();
        let __nt = super::__action48::<>(input, __sym0);
        __symbols.push((__start, __Symbol::Variant11(__nt), __end));
        (1, 36)
    }
}
pub use self::__parse__Expression::ExpressionParser;
#[cfg_attr(rustfmt, rustfmt_skip)]
mod __intern_token {
    #![allow(unused_imports)]
    use std::str::FromStr;
    use crate::ast::{Expression, OpCode};
    #[allow(unused_extern_crates)]
    extern crate lalrpop_util as __lalrpop_util;
    #[allow(unused_imports)]
    use self::__lalrpop_util::state_machine as __state_machine;
    extern crate core;
    extern crate alloc;
    pub fn new_builder() -> __lalrpop_util::lexer::MatcherBuilder {
        let __strs: &[(&str, bool)] = &[
            ("^(\"([\0-!\\#-\\[\\]-\u{10ffff}]*(\\\\\")?)*\")", false),
            ("^('([\0-\\&\\(-\\[\\]-\u{10ffff}]*(\\\\')?)*')", false),
            ("^([0-9]+)", false),
            ("^([0-9]+\\.[0-9]*)", false),
            ("^([A-Z_a-z][0-9A-Z_a-z]*)", false),
            ("^(\\.[0-9]+)", false),
            ("^(!=)", false),
            ("^(%)", false),
            ("^(\\&\\&)", false),
            ("^(\\()", false),
            ("^(\\))", false),
            ("^(\\*)", false),
            ("^(\\+)", false),
            ("^(,)", false),
            ("^(\\-)", false),
            ("^(\\.)", false),
            ("^(/)", false),
            ("^(//)", false),
            ("^(:)", false),
            ("^(<)", false),
            ("^(<=)", false),
            ("^(==)", false),
            ("^(>)", false),
            ("^(>=)", false),
            ("^(\\?)", false),
            ("^(NULL)", false),
            ("^(Null)", false),
            ("^(\\[)", false),
            ("^(\\])", false),
            ("^(\\^)", false),
            ("^(false)", false),
            ("^(in)", false),
            ("^(null)", false),
            ("^(true)", false),
            ("^(\\{)", false),
            ("^(\\{\\})", false),
            ("^(\\|)", false),
            ("^(\\|\\|)", false),
            ("^(\\})", false),
            (r"^(\s*)", true),
        ];
        __lalrpop_util::lexer::MatcherBuilder::new(__strs.iter().copied()).unwrap()
    }
}
pub use self::__lalrpop_util::lexer::Token;

#[allow(unused_variables)]
fn __action0<'input>(
    input: &'input str,
    (_, __0, _): (usize, Box<Expression>, usize),
) -> Box<Expression> {
    __0
}

#[allow(unused_variables)]
fn __action1<'input>(
    input: &'input str,
    (_, __0, _): (usize, Box<Expression>, usize),
) -> Box<Expression> {
    __0
}

#[allow(unused_variables)]
fn __action2<'input>(
    input: &'input str,
    (_, left, _): (usize, Box<Expression>, usize),
    (_, operation, _): (usize, OpCode, usize),
    (_, right, _): (usize, Box<Expression>, usize),
) -> Box<Expression> {
    Box::new(Expression::BinaryOperation {
        left,
        right,
        operation,
    })
}

#[allow(unused_variables)]
fn __action3<'input>(
    input: &'input str,
    (_, __0, _): (usize, Box<Expression>, usize),
) -> Box<Expression> {
    __0
}

#[allow(unused_variables)]
fn __action4<'input>(
    input: &'input str,
    (_, left, _): (usize, Box<Expression>, usize),
    (_, operation, _): (usize, OpCode, usize),
    (_, right, _): (usize, Box<Expression>, usize),
) -> Box<Expression> {
    Box::new(Expression::BinaryOperation {
        left,
        right,
        operation,
    })
}

#[allow(unused_variables)]
fn __action5<'input>(
    input: &'input str,
    (_, __0, _): (usize, Box<Expression>, usize),
) -> Box<Expression> {
    __0
}

#[allow(unused_variables)]
fn __action6<'input>(
    input: &'input str,
    (_, left, _): (usize, Box<Expression>, usize),
    (_, operation, _): (usize, OpCode, usize),
    (_, right, _): (usize, Box<Expression>, usize),
) -> Box<Expression> {
    Box::new(Expression::BinaryOperation {
        left,
        right,
        operation,
    })
}

#[allow(unused_variables)]
fn __action7<'input>(
    input: &'input str,
    (_, __0, _): (usize, Box<Expression>, usize),
) -> Box<Expression> {
    __0
}

#[allow(unused_variables)]
fn __action8<'input>(
    input: &'input str,
    (_, left, _): (usize, Box<Expression>, usize),
    (_, operation, _): (usize, OpCode, usize),
    (_, right, _): (usize, Box<Expression>, usize),
) -> Box<Expression> {
    Box::new(Expression::BinaryOperation {
        left,
        right,
        operation,
    })
}

#[allow(unused_variables)]
fn __action9<'input>(
    input: &'input str,
    (_, __0, _): (usize, Box<Expression>, usize),
) -> Box<Expression> {
    __0
}

#[allow(unused_variables)]
fn __action10<'input>(
    input: &'input str,
    (_, left, _): (usize, Box<Expression>, usize),
    (_, operation, _): (usize, OpCode, usize),
    (_, right, _): (usize, Box<Expression>, usize),
) -> Box<Expression> {
    Box::new(Expression::BinaryOperation {
        left,
        right,
        operation,
    })
}

#[allow(unused_variables)]
fn __action11<'input>(
    input: &'input str,
    (_, __0, _): (usize, Box<Expression>, usize),
) -> Box<Expression> {
    __0
}

#[allow(unused_variables)]
fn __action12<'input>(
    input: &'input str,
    (_, left, _): (usize, Box<Expression>, usize),
    (_, _, _): (usize, &'input str, usize),
    (_, truthy, _): (usize, Box<Expression>, usize),
    (_, _, _): (usize, &'input str, usize),
    (_, falsy, _): (usize, Box<Expression>, usize),
) -> Box<Expression> {
    Box::new(Expression::Conditional {
        left,
        truthy,
        falsy,
    })
}

#[allow(unused_variables)]
fn __action13<'input>(
    input: &'input str,
    (_, __0, _): (usize, Box<Expression>, usize),
) -> Box<Expression> {
    __0
}

#[allow(unused_variables)]
fn __action14<'input>(
    input: &'input str,
    (_, subject, _): (usize, Box<Expression>, usize),
    (_, _, _): (usize, &'input str, usize),
    (_, name, _): (usize, String, usize),
    (_, args, _): (usize, core::option::Option<Vec<Box<Expression>>>, usize),
) -> Box<Expression> {
    Box::new(Expression::Transform {
        name,
        subject,
        args,
    })
}

#[allow(unused_variables)]
fn __action15<'input>(
    input: &'input str,
    (_, __0, _): (usize, Box<Expression>, usize),
) -> Box<Expression> {
    __0
}

#[allow(unused_variables)]
fn __action16<'input>(
    input: &'input str,
    (_, subject, _): (usize, Box<Expression>, usize),
    (_, index, _): (usize, Box<Expression>, usize),
) -> Box<Expression> {
    Box::new(Expression::IndexOperation { subject, index })
}

#[allow(unused_variables)]
fn __action17<'input>(
    input: &'input str,
    (_, subject, _): (usize, Box<Expression>, usize),
    (_, _, _): (usize, &'input str, usize),
    (_, ident, _): (usize, String, usize),
) -> Box<Expression> {
    Box::new(Expression::DotOperation { subject, ident })
}

#[allow(unused_variables)]
fn __action18<'input>(
    input: &'input str,
    (_, __0, _): (usize, Box<Expression>, usize),
) -> Box<Expression> {
    __0
}

#[allow(unused_variables)]
fn __action19<'input>(input: &'input str, (_, __0, _): (usize, f64, usize)) -> Box<Expression> {
    Box::new(Expression::Number(__0))
}

#[allow(unused_variables)]
fn __action20<'input>(input: &'input str, (_, __0, _): (usize, bool, usize)) -> Box<Expression> {
    Box::new(Expression::Boolean(__0))
}

#[allow(unused_variables)]
fn __action21<'input>(input: &'input str, (_, __0, _): (usize, String, usize)) -> Box<Expression> {
    Box::new(Expression::String(__0))
}

#[allow(unused_variables)]
fn __action22<'input>(
    input: &'input str,
    (_, __0, _): (usize, Vec<Box<Expression>>, usize),
) -> Box<Expression> {
    Box::new(Expression::Array(__0))
}

#[allow(unused_variables)]
fn __action23<'input>(
    input: &'input str,
    (_, __0, _): (usize, Vec<(String, Box<Expression>)>, usize),
) -> Box<Expression> {
    Box::new(Expression::Object(__0))
}

#[allow(unused_variables)]
fn __action24<'input>(
    input: &'input str,
    (_, __0, _): (usize, Option<Box<Expression>>, usize),
) -> Box<Expression> {
    Box::new(Expression::Null)
}

#[allow(unused_variables)]
fn __action25<'input>(input: &'input str, (_, __0, _): (usize, String, usize)) -> Box<Expression> {
    Box::new(Expression::Identifier(__0))
}

#[allow(unused_variables)]
fn __action26<'input>(
    input: &'input str,
    (_, _, _): (usize, &'input str, usize),
    (_, __0, _): (usize, Box<Expression>, usize),
    (_, _, _): (usize, &'input str, usize),
) -> Box<Expression> {
    __0
}

#[allow(unused_variables)]
fn __action27<'input>(
    input: &'input str,
    (_, _, _): (usize, &'input str, usize),
    (_, __0, _): (usize, Vec<Box<Expression>>, usize),
    (_, _, _): (usize, &'input str, usize),
) -> Vec<Box<Expression>> {
    __0
}

#[allow(unused_variables)]
fn __action28<'input>(input: &'input str, (_, __0, _): (usize, &'input str, usize)) -> OpCode {
    OpCode::And
}

#[allow(unused_variables)]
fn __action29<'input>(input: &'input str, (_, __0, _): (usize, &'input str, usize)) -> OpCode {
    OpCode::Or
}

#[allow(unused_variables)]
fn __action30<'input>(input: &'input str, (_, __0, _): (usize, &'input str, usize)) -> OpCode {
    OpCode::Equal
}

#[allow(unused_variables)]
fn __action31<'input>(input: &'input str, (_, __0, _): (usize, &'input str, usize)) -> OpCode {
    OpCode::NotEqual
}

#[allow(unused_variables)]
fn __action32<'input>(input: &'input str, (_, __0, _): (usize, &'input str, usize)) -> OpCode {
    OpCode::GreaterEqual
}

#[allow(unused_variables)]
fn __action33<'input>(input: &'input str, (_, __0, _): (usize, &'input str, usize)) -> OpCode {
    OpCode::LessEqual
}

#[allow(unused_variables)]
fn __action34<'input>(input: &'input str, (_, __0, _): (usize, &'input str, usize)) -> OpCode {
    OpCode::Greater
}

#[allow(unused_variables)]
fn __action35<'input>(input: &'input str, (_, __0, _): (usize, &'input str, usize)) -> OpCode {
    OpCode::Less
}

#[allow(unused_variables)]
fn __action36<'input>(input: &'input str, (_, __0, _): (usize, &'input str, usize)) -> OpCode {
    OpCode::In
}

#[allow(unused_variables)]
fn __action37<'input>(input: &'input str, (_, __0, _): (usize, &'input str, usize)) -> OpCode {
    OpCode::Add
}

#[allow(unused_variables)]
fn __action38<'input>(input: &'input str, (_, __0, _): (usize, &'input str, usize)) -> OpCode {
    OpCode::Subtract
}

#[allow(unused_variables)]
fn __action39<'input>(input: &'input str, (_, __0, _): (usize, &'input str, usize)) -> OpCode {
    OpCode::Multiply
}

#[allow(unused_variables)]
fn __action40<'input>(input: &'input str, (_, __0, _): (usize, &'input str, usize)) -> OpCode {
    OpCode::FloorDivide
}

#[allow(unused_variables)]
fn __action41<'input>(input: &'input str, (_, __0, _): (usize, &'input str, usize)) -> OpCode {
    OpCode::Divide
}

#[allow(unused_variables)]
fn __action42<'input>(input: &'input str, (_, __0, _): (usize, &'input str, usize)) -> OpCode {
    OpCode::Modulus
}

#[allow(unused_variables)]
fn __action43<'input>(input: &'input str, (_, __0, _): (usize, &'input str, usize)) -> OpCode {
    OpCode::Exponent
}

#[allow(unused_variables)]
fn __action44<'input>(input: &'input str, (_, __0, _): (usize, &'input str, usize)) -> f64 {
    f64::from_str(__0).unwrap()
}

#[allow(unused_variables)]
fn __action45<'input>(input: &'input str, (_, __0, _): (usize, &'input str, usize)) -> f64 {
    f64::from_str(__0).unwrap()
}

#[allow(unused_variables)]
fn __action46<'input>(input: &'input str, (_, __0, _): (usize, &'input str, usize)) -> f64 {
    f64::from_str(__0).unwrap()
}

#[allow(unused_variables)]
fn __action47<'input>(input: &'input str, (_, s, _): (usize, &'input str, usize)) -> String {
    s[1..s.len() - 1].to_string().replace("\\\"", "\"")
}

#[allow(unused_variables)]
fn __action48<'input>(input: &'input str, (_, s, _): (usize, &'input str, usize)) -> String {
    s[1..s.len() - 1].to_string().replace("\\'", "'")
}

#[allow(unused_variables)]
fn __action49<'input>(
    input: &'input str,
    (_, __0, _): (usize, &'input str, usize),
) -> Option<Box<Expression>> {
    None
}

#[allow(unused_variables)]
fn __action50<'input>(
    input: &'input str,
    (_, __0, _): (usize, &'input str, usize),
) -> Option<Box<Expression>> {
    None
}

#[allow(unused_variables)]
fn __action51<'input>(
    input: &'input str,
    (_, __0, _): (usize, &'input str, usize),
) -> Option<Box<Expression>> {
    None
}

#[allow(unused_variables)]
fn __action52<'input>(input: &'input str, (_, __0, _): (usize, &'input str, usize)) -> String {
    __0.to_string()
}

#[allow(unused_variables)]
fn __action53<'input>(
    input: &'input str,
    (_, _, _): (usize, &'input str, usize),
    (_, _, _): (usize, &'input str, usize),
    (_, ident, _): (usize, String, usize),
    (_, op, _): (usize, OpCode, usize),
    (_, right, _): (usize, Box<Expression>, usize),
    (_, _, _): (usize, &'input str, usize),
) -> Box<Expression> {
    Box::new(Expression::Filter { ident, op, right })
}

#[allow(unused_variables)]
fn __action54<'input>(
    input: &'input str,
    (_, _, _): (usize, &'input str, usize),
    (_, __0, _): (usize, Box<Expression>, usize),
    (_, _, _): (usize, &'input str, usize),
) -> Box<Expression> {
    __0
}

#[allow(unused_variables)]
fn __action55<'input>(input: &'input str, (_, __0, _): (usize, &'input str, usize)) -> bool {
    true
}

#[allow(unused_variables)]
fn __action56<'input>(input: &'input str, (_, __0, _): (usize, &'input str, usize)) -> bool {
    false
}

#[allow(unused_variables)]
fn __action57<'input>(
    input: &'input str,
    (_, _, _): (usize, &'input str, usize),
    (_, __0, _): (usize, Vec<Box<Expression>>, usize),
    (_, _, _): (usize, &'input str, usize),
) -> Vec<Box<Expression>> {
    __0
}

#[allow(unused_variables)]
fn __action58<'input>(
    input: &'input str,
    (_, _, _): (usize, &'input str, usize),
    (_, __0, _): (usize, Vec<(String, Box<Expression>)>, usize),
    (_, _, _): (usize, &'input str, usize),
) -> Vec<(String, Box<Expression>)> {
    __0
}

#[allow(unused_variables)]
fn __action59<'input>(
    input: &'input str,
    (_, __0, _): (usize, &'input str, usize),
) -> Vec<(String, Box<Expression>)> {
    vec![]
}

#[allow(unused_variables)]
fn __action60<'input>(input: &'input str, (_, __0, _): (usize, String, usize)) -> String {
    __0
}

#[allow(unused_variables)]
fn __action61<'input>(input: &'input str, (_, __0, _): (usize, String, usize)) -> String {
    __0
}

#[allow(unused_variables)]
fn __action62<'input>(
    input: &'input str,
    (_, v, _): (usize, alloc::vec::Vec<(String, Box<Expression>)>, usize),
    (_, e, _): (
        usize,
        core::option::Option<(String, Box<Expression>)>,
        usize,
    ),
) -> Vec<(String, Box<Expression>)> {
    match e {
        None => v,
        Some(e) => {
            let mut v = v;
            v.push(e);
            v
        }
    }
}

#[allow(unused_variables)]
fn __action63<'input>(
    input: &'input str,
    (_, __0, _): (usize, String, usize),
    (_, _, _): (usize, &'input str, usize),
    (_, __1, _): (usize, Box<Expression>, usize),
) -> (String, Box<Expression>) {
    (__0, __1)
}

#[allow(unused_variables)]
fn __action64<'input>(
    input: &'input str,
    (_, v, _): (usize, alloc::vec::Vec<Box<Expression>>, usize),
    (_, e, _): (usize, core::option::Option<Box<Expression>>, usize),
) -> Vec<Box<Expression>> {
    match e {
        None => v,
        Some(e) => {
            let mut v = v;
            v.push(e);
            v
        }
    }
}

#[allow(unused_variables)]
fn __action65<'input>(
    input: &'input str,
    (_, __0, _): (usize, Vec<Box<Expression>>, usize),
) -> core::option::Option<Vec<Box<Expression>>> {
    Some(__0)
}

#[allow(unused_variables)]
fn __action66<'input>(
    input: &'input str,
    __lookbehind: &usize,
    __lookahead: &usize,
) -> core::option::Option<Vec<Box<Expression>>> {
    None
}

#[allow(unused_variables)]
fn __action67<'input>(
    input: &'input str,
    (_, __0, _): (usize, Box<Expression>, usize),
) -> core::option::Option<Box<Expression>> {
    Some(__0)
}

#[allow(unused_variables)]
fn __action68<'input>(
    input: &'input str,
    __lookbehind: &usize,
    __lookahead: &usize,
) -> core::option::Option<Box<Expression>> {
    None
}

#[allow(unused_variables)]
fn __action69<'input>(
    input: &'input str,
    __lookbehind: &usize,
    __lookahead: &usize,
) -> alloc::vec::Vec<Box<Expression>> {
    alloc::vec![]
}

#[allow(unused_variables)]
fn __action70<'input>(
    input: &'input str,
    (_, v, _): (usize, alloc::vec::Vec<Box<Expression>>, usize),
) -> alloc::vec::Vec<Box<Expression>> {
    v
}

#[allow(unused_variables)]
fn __action71<'input>(
    input: &'input str,
    (_, __0, _): (usize, Box<Expression>, usize),
    (_, _, _): (usize, &'input str, usize),
) -> Box<Expression> {
    __0
}

#[allow(unused_variables)]
fn __action72<'input>(
    input: &'input str,
    (_, __0, _): (usize, (String, Box<Expression>), usize),
) -> core::option::Option<(String, Box<Expression>)> {
    Some(__0)
}

#[allow(unused_variables)]
fn __action73<'input>(
    input: &'input str,
    __lookbehind: &usize,
    __lookahead: &usize,
) -> core::option::Option<(String, Box<Expression>)> {
    None
}

#[allow(unused_variables)]
fn __action74<'input>(
    input: &'input str,
    __lookbehind: &usize,
    __lookahead: &usize,
) -> alloc::vec::Vec<(String, Box<Expression>)> {
    alloc::vec![]
}

#[allow(unused_variables)]
fn __action75<'input>(
    input: &'input str,
    (_, v, _): (usize, alloc::vec::Vec<(String, Box<Expression>)>, usize),
) -> alloc::vec::Vec<(String, Box<Expression>)> {
    v
}

#[allow(unused_variables)]
fn __action76<'input>(
    input: &'input str,
    (_, __0, _): (usize, (String, Box<Expression>), usize),
    (_, _, _): (usize, &'input str, usize),
) -> (String, Box<Expression>) {
    __0
}

#[allow(unused_variables)]
fn __action77<'input>(
    input: &'input str,
    (_, __0, _): (usize, (String, Box<Expression>), usize),
) -> alloc::vec::Vec<(String, Box<Expression>)> {
    alloc::vec![__0]
}

#[allow(unused_variables)]
fn __action78<'input>(
    input: &'input str,
    (_, v, _): (usize, alloc::vec::Vec<(String, Box<Expression>)>, usize),
    (_, e, _): (usize, (String, Box<Expression>), usize),
) -> alloc::vec::Vec<(String, Box<Expression>)> {
    {
        let mut v = v;
        v.push(e);
        v
    }
}

#[allow(unused_variables)]
fn __action79<'input>(
    input: &'input str,
    (_, __0, _): (usize, Box<Expression>, usize),
) -> alloc::vec::Vec<Box<Expression>> {
    alloc::vec![__0]
}

#[allow(unused_variables)]
fn __action80<'input>(
    input: &'input str,
    (_, v, _): (usize, alloc::vec::Vec<Box<Expression>>, usize),
    (_, e, _): (usize, Box<Expression>, usize),
) -> alloc::vec::Vec<Box<Expression>> {
    {
        let mut v = v;
        v.push(e);
        v
    }
}

#[allow(unused_variables)]
fn __action81<'input>(
    input: &'input str,
    __0: (usize, String, usize),
    __1: (usize, &'input str, usize),
    __2: (usize, Box<Expression>, usize),
    __3: (usize, &'input str, usize),
) -> (String, Box<Expression>) {
    let __start0 = __0.0.clone();
    let __end0 = __2.2.clone();
    let __temp0 = __action63(input, __0, __1, __2);
    let __temp0 = (__start0, __temp0, __end0);
    __action76(input, __temp0, __3)
}

#[allow(unused_variables)]
fn __action82<'input>(
    input: &'input str,
    __0: (usize, String, usize),
    __1: (usize, &'input str, usize),
    __2: (usize, Box<Expression>, usize),
) -> core::option::Option<(String, Box<Expression>)> {
    let __start0 = __0.0.clone();
    let __end0 = __2.2.clone();
    let __temp0 = __action63(input, __0, __1, __2);
    let __temp0 = (__start0, __temp0, __end0);
    __action72(input, __temp0)
}

#[allow(unused_variables)]
fn __action83<'input>(
    input: &'input str,
    __0: (usize, String, usize),
    __1: (usize, &'input str, usize),
    __2: (usize, Box<Expression>, usize),
    __3: (usize, &'input str, usize),
) -> alloc::vec::Vec<(String, Box<Expression>)> {
    let __start0 = __0.0.clone();
    let __end0 = __3.2.clone();
    let __temp0 = __action81(input, __0, __1, __2, __3);
    let __temp0 = (__start0, __temp0, __end0);
    __action77(input, __temp0)
}

#[allow(unused_variables)]
fn __action84<'input>(
    input: &'input str,
    __0: (usize, alloc::vec::Vec<(String, Box<Expression>)>, usize),
    __1: (usize, String, usize),
    __2: (usize, &'input str, usize),
    __3: (usize, Box<Expression>, usize),
    __4: (usize, &'input str, usize),
) -> alloc::vec::Vec<(String, Box<Expression>)> {
    let __start0 = __1.0.clone();
    let __end0 = __4.2.clone();
    let __temp0 = __action81(input, __1, __2, __3, __4);
    let __temp0 = (__start0, __temp0, __end0);
    __action78(input, __0, __temp0)
}

#[allow(unused_variables)]
fn __action85<'input>(
    input: &'input str,
    __0: (
        usize,
        core::option::Option<(String, Box<Expression>)>,
        usize,
    ),
) -> Vec<(String, Box<Expression>)> {
    let __start0 = __0.0.clone();
    let __end0 = __0.0.clone();
    let __temp0 = __action74(input, &__start0, &__end0);
    let __temp0 = (__start0, __temp0, __end0);
    __action62(input, __temp0, __0)
}

#[allow(unused_variables)]
fn __action86<'input>(
    input: &'input str,
    __0: (usize, alloc::vec::Vec<(String, Box<Expression>)>, usize),
    __1: (
        usize,
        core::option::Option<(String, Box<Expression>)>,
        usize,
    ),
) -> Vec<(String, Box<Expression>)> {
    let __start0 = __0.0.clone();
    let __end0 = __0.2.clone();
    let __temp0 = __action75(input, __0);
    let __temp0 = (__start0, __temp0, __end0);
    __action62(input, __temp0, __1)
}

#[allow(unused_variables)]
fn __action87<'input>(
    input: &'input str,
    __0: (usize, Box<Expression>, usize),
    __1: (usize, &'input str, usize),
) -> alloc::vec::Vec<Box<Expression>> {
    let __start0 = __0.0.clone();
    let __end0 = __1.2.clone();
    let __temp0 = __action71(input, __0, __1);
    let __temp0 = (__start0, __temp0, __end0);
    __action79(input, __temp0)
}

#[allow(unused_variables)]
fn __action88<'input>(
    input: &'input str,
    __0: (usize, alloc::vec::Vec<Box<Expression>>, usize),
    __1: (usize, Box<Expression>, usize),
    __2: (usize, &'input str, usize),
) -> alloc::vec::Vec<Box<Expression>> {
    let __start0 = __1.0.clone();
    let __end0 = __2.2.clone();
    let __temp0 = __action71(input, __1, __2);
    let __temp0 = (__start0, __temp0, __end0);
    __action80(input, __0, __temp0)
}

#[allow(unused_variables)]
fn __action89<'input>(
    input: &'input str,
    __0: (usize, core::option::Option<Box<Expression>>, usize),
) -> Vec<Box<Expression>> {
    let __start0 = __0.0.clone();
    let __end0 = __0.0.clone();
    let __temp0 = __action69(input, &__start0, &__end0);
    let __temp0 = (__start0, __temp0, __end0);
    __action64(input, __temp0, __0)
}

#[allow(unused_variables)]
fn __action90<'input>(
    input: &'input str,
    __0: (usize, alloc::vec::Vec<Box<Expression>>, usize),
    __1: (usize, core::option::Option<Box<Expression>>, usize),
) -> Vec<Box<Expression>> {
    let __start0 = __0.0.clone();
    let __end0 = __0.2.clone();
    let __temp0 = __action70(input, __0);
    let __temp0 = (__start0, __temp0, __end0);
    __action64(input, __temp0, __1)
}

#[allow(unused_variables)]
fn __action91<'input>(
    input: &'input str,
    __0: (usize, String, usize),
    __1: (usize, &'input str, usize),
    __2: (usize, Box<Expression>, usize),
) -> Vec<(String, Box<Expression>)> {
    let __start0 = __0.0.clone();
    let __end0 = __2.2.clone();
    let __temp0 = __action82(input, __0, __1, __2);
    let __temp0 = (__start0, __temp0, __end0);
    __action85(input, __temp0)
}

#[allow(unused_variables)]
fn __action92<'input>(
    input: &'input str,
    __lookbehind: &usize,
    __lookahead: &usize,
) -> Vec<(String, Box<Expression>)> {
    let __start0 = __lookbehind.clone();
    let __end0 = __lookahead.clone();
    let __temp0 = __action73(input, &__start0, &__end0);
    let __temp0 = (__start0, __temp0, __end0);
    __action85(input, __temp0)
}

#[allow(unused_variables)]
fn __action93<'input>(
    input: &'input str,
    __0: (usize, alloc::vec::Vec<(String, Box<Expression>)>, usize),
    __1: (usize, String, usize),
    __2: (usize, &'input str, usize),
    __3: (usize, Box<Expression>, usize),
) -> Vec<(String, Box<Expression>)> {
    let __start0 = __1.0.clone();
    let __end0 = __3.2.clone();
    let __temp0 = __action82(input, __1, __2, __3);
    let __temp0 = (__start0, __temp0, __end0);
    __action86(input, __0, __temp0)
}

#[allow(unused_variables)]
fn __action94<'input>(
    input: &'input str,
    __0: (usize, alloc::vec::Vec<(String, Box<Expression>)>, usize),
) -> Vec<(String, Box<Expression>)> {
    let __start0 = __0.2.clone();
    let __end0 = __0.2.clone();
    let __temp0 = __action73(input, &__start0, &__end0);
    let __temp0 = (__start0, __temp0, __end0);
    __action86(input, __0, __temp0)
}

#[allow(unused_variables)]
fn __action95<'input>(
    input: &'input str,
    __0: (usize, Box<Expression>, usize),
    __1: (usize, &'input str, usize),
    __2: (usize, String, usize),
    __3: (usize, Vec<Box<Expression>>, usize),
) -> Box<Expression> {
    let __start0 = __3.0.clone();
    let __end0 = __3.2.clone();
    let __temp0 = __action65(input, __3);
    let __temp0 = (__start0, __temp0, __end0);
    __action14(input, __0, __1, __2, __temp0)
}

#[allow(unused_variables)]
fn __action96<'input>(
    input: &'input str,
    __0: (usize, Box<Expression>, usize),
    __1: (usize, &'input str, usize),
    __2: (usize, String, usize),
) -> Box<Expression> {
    let __start0 = __2.2.clone();
    let __end0 = __2.2.clone();
    let __temp0 = __action66(input, &__start0, &__end0);
    let __temp0 = (__start0, __temp0, __end0);
    __action14(input, __0, __1, __2, __temp0)
}

#[allow(unused_variables)]
fn __action97<'input>(
    input: &'input str,
    __0: (usize, Box<Expression>, usize),
) -> Vec<Box<Expression>> {
    let __start0 = __0.0.clone();
    let __end0 = __0.2.clone();
    let __temp0 = __action67(input, __0);
    let __temp0 = (__start0, __temp0, __end0);
    __action89(input, __temp0)
}

#[allow(unused_variables)]
fn __action98<'input>(
    input: &'input str,
    __lookbehind: &usize,
    __lookahead: &usize,
) -> Vec<Box<Expression>> {
    let __start0 = __lookbehind.clone();
    let __end0 = __lookahead.clone();
    let __temp0 = __action68(input, &__start0, &__end0);
    let __temp0 = (__start0, __temp0, __end0);
    __action89(input, __temp0)
}

#[allow(unused_variables)]
fn __action99<'input>(
    input: &'input str,
    __0: (usize, alloc::vec::Vec<Box<Expression>>, usize),
    __1: (usize, Box<Expression>, usize),
) -> Vec<Box<Expression>> {
    let __start0 = __1.0.clone();
    let __end0 = __1.2.clone();
    let __temp0 = __action67(input, __1);
    let __temp0 = (__start0, __temp0, __end0);
    __action90(input, __0, __temp0)
}

#[allow(unused_variables)]
fn __action100<'input>(
    input: &'input str,
    __0: (usize, alloc::vec::Vec<Box<Expression>>, usize),
) -> Vec<Box<Expression>> {
    let __start0 = __0.2.clone();
    let __end0 = __0.2.clone();
    let __temp0 = __action68(input, &__start0, &__end0);
    let __temp0 = (__start0, __temp0, __end0);
    __action90(input, __0, __temp0)
}

pub trait __ToTriple<'input> {
    #[allow(dead_code)]
    fn to_triple(
        value: Self,
    ) -> Result<
        (usize, Token<'input>, usize),
        __lalrpop_util::ParseError<usize, Token<'input>, &'static str>,
    >;
}

impl<'input> __ToTriple<'input> for (usize, Token<'input>, usize) {
    fn to_triple(
        value: Self,
    ) -> Result<
        (usize, Token<'input>, usize),
        __lalrpop_util::ParseError<usize, Token<'input>, &'static str>,
    > {
        Ok(value)
    }
}
impl<'input> __ToTriple<'input> for Result<(usize, Token<'input>, usize), &'static str> {
    fn to_triple(
        value: Self,
    ) -> Result<
        (usize, Token<'input>, usize),
        __lalrpop_util::ParseError<usize, Token<'input>, &'static str>,
    > {
        match value {
            Ok(v) => Ok(v),
            Err(error) => Err(__lalrpop_util::ParseError::User { error }),
        }
    }
}
