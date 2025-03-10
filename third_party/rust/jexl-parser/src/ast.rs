/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[derive(Debug, PartialEq)]
pub enum Expression {
    Number(f64),
    String(String),
    Boolean(bool),
    Array(Vec<Box<Expression>>),
    Object(Vec<(String, Box<Expression>)>),
    Identifier(String),
    Null,

    BinaryOperation {
        operation: OpCode,
        left: Box<Expression>,
        right: Box<Expression>,
    },
    Transform {
        name: String,
        subject: Box<Expression>,
        args: Option<Vec<Box<Expression>>>,
    },
    DotOperation {
        subject: Box<Expression>,
        ident: String,
    },
    IndexOperation {
        subject: Box<Expression>,
        index: Box<Expression>,
    },

    Conditional {
        left: Box<Expression>,
        truthy: Box<Expression>,
        falsy: Box<Expression>,
    },

    Filter {
        ident: String,
        op: OpCode,
        right: Box<Expression>,
    },
}

#[derive(Debug, PartialEq, Eq, Copy, Clone)]
pub enum OpCode {
    Add,
    Subtract,
    Multiply,
    Divide,
    FloorDivide,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    Equal,
    NotEqual,
    And,
    Or,
    Modulus,
    Exponent,
    In,
}

impl std::fmt::Display for OpCode {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{}",
            match self {
                OpCode::Add => "Add",
                OpCode::Subtract => "Subtract",
                OpCode::Multiply => "Multiply",
                OpCode::Divide => "Divide",
                OpCode::FloorDivide => "Floor division",
                OpCode::Less => "Less than",
                OpCode::LessEqual => "Less than or equal to",
                OpCode::Greater => "Greater than",
                OpCode::GreaterEqual => "Greater than or equal to",
                OpCode::Equal => "Equal",
                OpCode::NotEqual => "Not equal",
                OpCode::And => "Bitwise And",
                OpCode::Or => "Bitwise Or",
                OpCode::Modulus => "Modulus",
                OpCode::Exponent => "Exponent",
                OpCode::In => "In",
            }
        )
    }
}
