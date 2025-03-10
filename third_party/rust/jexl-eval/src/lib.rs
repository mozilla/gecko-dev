/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! A JEXL evaluator written in Rust
//! This crate depends on a JEXL parser crate that handles all the parsing
//! and is a part of the same workspace.
//! JEXL is an expression language used by Mozilla, you can find more information here: https://github.com/mozilla/mozjexl
//!
//! # How to use
//! The access point for this crate is the `eval` functions of the Evaluator Struct
//! You can use the `eval` function directly to evaluate standalone statements
//!
//! For example:
//! ```rust
//! use jexl_eval::Evaluator;
//! use serde_json::json as value;
//! let evaluator = Evaluator::new();
//! assert_eq!(evaluator.eval("'Hello ' + 'World'").unwrap(), value!("Hello World"));
//! ```
//!
//! You can also run the statements against a context using the `eval_in_context` function
//! The context can be any type that implements the `serde::Serializable` trait
//! and the function will return errors if the statement doesn't match the context
//!
//! For example:
//! ```rust
//! use jexl_eval::Evaluator;
//! use serde_json::json as value;
//! let context = value!({"a": {"b": 2.0}});
//! let evaluator = Evaluator::new();
//! assert_eq!(evaluator.eval_in_context("a.b", context).unwrap(), value!(2.0));
//! ```
//!

use jexl_parser::{
    ast::{Expression, OpCode},
    Parser,
};
use serde_json::{json as value, Value};

pub mod error;
use error::*;
use std::collections::HashMap;

const EPSILON: f64 = 0.000001f64;

trait Truthy {
    fn is_truthy(&self) -> bool;
}

impl Truthy for Value {
    fn is_truthy(&self) -> bool {
        match self {
            Value::Bool(b) => *b,
            Value::Null => false,
            Value::Number(f) => f.as_f64().unwrap() != 0.0,
            Value::String(s) => !s.is_empty(),
            // It would be better if these depended on the contents of the
            // object (empty array/object is falsey, non-empty is truthy, like
            // in Python) but this matches JS semantics. Is it worth changing?
            Value::Array(_) => true,
            Value::Object(_) => true,
        }
    }
}

impl<'b> Truthy for Result<'b, Value> {
    fn is_truthy(&self) -> bool {
        match self {
            Ok(v) => v.is_truthy(),
            _ => false,
        }
    }
}

type Context = Value;

/// TransformFn represents an arbitrary transform function
/// Transform functions take an arbitrary number of `serde_json::Value`to represent their arguments
/// and return a `serde_json::Value`.
/// the transform function itself is responsible for checking if the format and number of
/// the arguments is correct
///
/// Returns a Result with an `anyhow::Error`. This allows consumers to return their own custom errors
/// in the closure, and use `.into` to convert it into an `anyhow::Error`. The error message will be perserved
pub type TransformFn<'a> = Box<dyn Fn(&[Value]) -> Result<Value, anyhow::Error> + Send + Sync + 'a>;

#[derive(Default)]
pub struct Evaluator<'a> {
    transforms: HashMap<String, TransformFn<'a>>,
}

impl<'a> Evaluator<'a> {
    pub fn new() -> Self {
        Self::default()
    }

    /// Adds a custom transform function
    /// This is meant as a way to allow consumers to add their own custom functionality
    /// to the expression language.
    /// Note that the name added here has to match with
    /// the name that the transform will have when it's a part of the expression statement
    ///
    /// # Arguments:
    /// - `name`: The name of the transfrom
    /// - `transform`: The actual function. A closure the implements Fn(&[serde_json::Value]) -> Result<Value, anyhow::Error>
    ///
    /// # Example:
    ///
    /// ```rust
    /// use jexl_eval::Evaluator;
    /// use serde_json::{json as value, Value};
    ///
    /// let mut evaluator = Evaluator::new().with_transform("lower", |v: &[Value]| {
    ///    let s = v
    ///            .first()
    ///            .expect("Should have 1 argument!")
    ///            .as_str()
    ///            .expect("Should be a string!");
    ///       Ok(value!(s.to_lowercase()))
    ///  });
    ///
    /// assert_eq!(evaluator.eval("'JOHN DOe'|lower").unwrap(), value!("john doe"))
    /// ```
    pub fn with_transform<F>(mut self, name: &str, transform: F) -> Self
    where
        F: Fn(&[Value]) -> Result<Value, anyhow::Error> + Send + Sync + 'a,
    {
        self.transforms
            .insert(name.to_string(), Box::new(transform));
        self
    }

    pub fn eval<'b>(&self, input: &'b str) -> Result<'b, Value> {
        let context = value!({});
        self.eval_in_context(input, &context)
    }

    pub fn eval_in_context<'b, T: serde::Serialize>(
        &self,
        input: &'b str,
        context: T,
    ) -> Result<'b, Value> {
        let tree = Parser::parse(input)?;
        let context = serde_json::to_value(context)?;
        if !context.is_object() {
            return Err(EvaluationError::InvalidContext);
        }
        self.eval_ast(tree, &context)
    }

    fn eval_ast<'b>(&self, ast: Expression, context: &Context) -> Result<'b, Value> {
        match ast {
            Expression::Number(n) => Ok(value!(n)),
            Expression::Boolean(b) => Ok(value!(b)),
            Expression::String(s) => Ok(value!(s)),
            Expression::Array(xs) => xs.into_iter().map(|x| self.eval_ast(*x, context)).collect(),
            Expression::Null => Ok(Value::Null),

            Expression::Object(items) => {
                let mut map = serde_json::Map::with_capacity(items.len());
                for (key, expr) in items.into_iter() {
                    if map.contains_key(&key) {
                        return Err(EvaluationError::DuplicateObjectKey(key));
                    }
                    let value = self.eval_ast(*expr, context)?;
                    map.insert(key, value);
                }
                Ok(Value::Object(map))
            }

            Expression::Identifier(inner) => match context.get(&inner) {
                Some(v) => Ok(v.clone()),
                _ => Err(EvaluationError::UndefinedIdentifier(inner.clone())),
            },

            Expression::DotOperation { subject, ident } => {
                let subject = self.eval_ast(*subject, context)?;
                Ok(subject.get(&ident).unwrap_or(&value!(null)).clone())
            }

            Expression::IndexOperation { subject, index } => {
                let subject = self.eval_ast(*subject, context)?;
                if let Expression::Filter { ident, op, right } = *index {
                    let subject_arr = subject.as_array().ok_or(EvaluationError::InvalidFilter)?;
                    let right = self.eval_ast(*right, context)?;
                    let filtered = subject_arr
                        .iter()
                        .filter(|e| {
                            let left = e.get(&ident).unwrap_or(&value!(null));
                            // returns false if any members fail the op, could happen if array members are missing the identifier
                            Self::apply_op(op, left.clone(), right.clone())
                                .unwrap_or(value!(false))
                                .is_truthy()
                        })
                        .collect::<Vec<_>>();
                    return Ok(value!(filtered));
                }

                let index = self.eval_ast(*index, context)?;
                match index {
                    Value::String(inner) => {
                        Ok(subject.get(&inner).unwrap_or(&value!(null)).clone())
                    }
                    Value::Number(inner) => Ok(subject
                        .get(inner.as_f64().unwrap().floor() as usize)
                        .unwrap_or(&value!(null))
                        .clone()),
                    _ => Err(EvaluationError::InvalidIndexType),
                }
            }

            Expression::BinaryOperation {
                left,
                right,
                operation,
            } => self.eval_op(operation, left, right, context),
            Expression::Transform {
                name,
                subject,
                args,
            } => {
                let subject = self.eval_ast(*subject, context)?;
                let mut args_arr = Vec::new();
                args_arr.push(subject);
                if let Some(args) = args {
                    for arg in args {
                        args_arr.push(self.eval_ast(*arg, context)?);
                    }
                }
                let f = self
                    .transforms
                    .get(&name)
                    .ok_or(EvaluationError::UnknownTransform(name))?;
                f(&args_arr).map_err(|e| e.into())
            }

            Expression::Conditional {
                left,
                truthy,
                falsy,
            } => {
                if self.eval_ast(*left, context).is_truthy() {
                    self.eval_ast(*truthy, context)
                } else {
                    self.eval_ast(*falsy, context)
                }
            }

            Expression::Filter {
                ident: _,
                op: _,
                right: _,
            } => {
                // Filters shouldn't be evaluated individually
                // instead, they are evaluated as a part of an IndexOperation
                return Err(EvaluationError::InvalidFilter);
            }
        }
    }

    fn eval_op<'b>(
        &self,
        operation: OpCode,
        left: Box<Expression>,
        right: Box<Expression>,
        context: &Context,
    ) -> Result<'b, Value> {
        let left = self.eval_ast(*left, context);

        // We want to delay evaluating the right hand side in the cases of AND and OR.
        let eval_right = || self.eval_ast(*right, context);
        Ok(match operation {
            OpCode::Or => {
                if left.is_truthy() {
                    left?
                } else {
                    eval_right()?
                }
            }
            OpCode::And => {
                if left.is_truthy() {
                    eval_right()?
                } else {
                    left?
                }
            }
            _ => Self::apply_op(operation, left?, eval_right()?)?,
        })
    }

    fn apply_op<'b>(operation: OpCode, left: Value, right: Value) -> Result<'b, Value> {
        match (operation, left, right) {
            (OpCode::NotEqual, a, b) => {
                // Implement NotEquals as the inverse of Equals.
                let value = Self::apply_op(OpCode::Equal, a, b)?;
                let equality = value
                    .as_bool()
                    .unwrap_or_else(|| unreachable!("Equality always returns a bool"));
                Ok(value!(!equality))
            }

            (OpCode::And, a, b) => Ok(if a.is_truthy() { b } else { a }),
            (OpCode::Or, a, b) => Ok(if a.is_truthy() { a } else { b }),

            (op, Value::Number(a), Value::Number(b)) => {
                let left = a.as_f64().unwrap();
                let right = b.as_f64().unwrap();
                Ok(match op {
                    OpCode::Add => value!(left + right),
                    OpCode::Subtract => value!(left - right),
                    OpCode::Multiply => value!(left * right),
                    OpCode::Divide => value!(left / right),
                    OpCode::FloorDivide => value!((left / right).floor()),
                    OpCode::Modulus => value!(left % right),
                    OpCode::Exponent => value!(left.powf(right)),
                    OpCode::Less => value!(left < right),
                    OpCode::Greater => value!(left > right),
                    OpCode::LessEqual => value!(left <= right),
                    OpCode::GreaterEqual => value!(left >= right),
                    OpCode::Equal => value!((left - right).abs() < EPSILON),
                    OpCode::NotEqual => value!((left - right).abs() >= EPSILON),
                    OpCode::In => value!(false),
                    OpCode::And | OpCode::Or => {
                        unreachable!("Covered by previous case in parent match")
                    }
                })
            }

            (op, Value::String(a), Value::String(b)) => match op {
                OpCode::Equal => Ok(value!(a == b)),

                OpCode::Add => Ok(value!(format!("{}{}", a, b))),
                OpCode::In => Ok(value!(b.contains(&a))),

                OpCode::Less => Ok(value!(a < b)),
                OpCode::Greater => Ok(value!(a > b)),
                OpCode::LessEqual => Ok(value!(a <= b)),
                OpCode::GreaterEqual => Ok(value!(a >= b)),

                _ => Err(EvaluationError::InvalidBinaryOp {
                    operation,
                    left: value!(a),
                    right: value!(b),
                }),
            },

            (OpCode::In, left, Value::Array(v)) => Ok(value!(v.contains(&left))),
            (OpCode::In, Value::String(left), Value::Object(v)) => {
                Ok(value!(v.contains_key(&left)))
            }

            (OpCode::Equal, a, b) => match (a, b) {
                // Number == Number is handled above
                // String == String is handled above
                (Value::Bool(a), Value::Bool(b)) => Ok(value!(a == b)),
                (Value::Null, Value::Null) => Ok(value!(true)),
                (Value::Array(a), Value::Array(b)) => Ok(value!(a == b)),
                (Value::Object(a), Value::Object(b)) => Ok(value!(a == b)),
                // If the types don't match, it's always false
                _ => Ok(value!(false)),
            },

            (operation, left, right) => Err(EvaluationError::InvalidBinaryOp {
                operation,
                left,
                right,
            }),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json as value;

    #[test]
    fn test_literal() {
        assert_eq!(Evaluator::new().eval("1").unwrap(), value!(1.0));
    }

    #[test]
    fn test_binary_expression_addition() {
        assert_eq!(Evaluator::new().eval("1 + 2").unwrap(), value!(3.0));
    }

    #[test]
    fn test_binary_expression_multiplication() {
        assert_eq!(Evaluator::new().eval("2 * 3").unwrap(), value!(6.0));
    }

    #[test]
    fn test_precedence() {
        assert_eq!(Evaluator::new().eval("2 + 3 * 4").unwrap(), value!(14.0));
    }

    #[test]
    fn test_parenthesis() {
        assert_eq!(Evaluator::new().eval("(2 + 3) * 4").unwrap(), value!(20.0));
    }

    #[test]
    fn test_string_concat() {
        assert_eq!(
            Evaluator::new().eval("'Hello ' + 'World'").unwrap(),
            value!("Hello World")
        );
    }

    #[test]
    fn test_true_comparison() {
        assert_eq!(Evaluator::new().eval("2 > 1").unwrap(), value!(true));
    }

    #[test]
    fn test_false_comparison() {
        assert_eq!(Evaluator::new().eval("2 <= 1").unwrap(), value!(false));
    }

    #[test]
    fn test_boolean_logic() {
        assert_eq!(
            Evaluator::new()
                .eval("'foo' && 6 >= 6 && 0 + 1 && true")
                .unwrap(),
            value!(true)
        );
    }

    #[test]
    fn test_identifier() {
        let context = value!({"a": 1.0});
        assert_eq!(
            Evaluator::new().eval_in_context("a", context).unwrap(),
            value!(1.0)
        );
    }

    #[test]
    fn test_identifier_chain() {
        let context = value!({"a": {"b": 2.0}});
        assert_eq!(
            Evaluator::new().eval_in_context("a.b", context).unwrap(),
            value!(2.0)
        );
    }

    #[test]
    fn test_context_filter_arrays() {
        let context = value!({
            "foo": {
                "bar": [
                    {"tek": "hello"},
                    {"tek": "baz"},
                    {"tok": "baz"},
                ]
            }
        });
        assert_eq!(
            Evaluator::new()
                .eval_in_context("foo.bar[.tek == 'baz']", &context)
                .unwrap(),
            value!([{"tek": "baz"}])
        );
    }

    #[test]
    fn test_context_array_index() {
        let context = value!({
            "foo": {
                "bar": [
                    {"tek": "hello"},
                    {"tek": "baz"},
                    {"tok": "baz"},
                ]
            }
        });
        assert_eq!(
            Evaluator::new()
                .eval_in_context("foo.bar[1].tek", context)
                .unwrap(),
            value!("baz")
        );
    }

    #[test]
    fn test_object_expression_properties() {
        let context = value!({"foo": {"baz": {"bar": "tek"}}});
        assert_eq!(
            Evaluator::new()
                .eval_in_context("foo['ba' + 'z'].bar", &context)
                .unwrap(),
            value!("tek")
        );
    }

    #[test]
    fn test_divfloor() {
        assert_eq!(Evaluator::new().eval("7 // 2").unwrap(), value!(3.0));
    }

    #[test]
    fn test_empty_object_literal() {
        assert_eq!(Evaluator::new().eval("{}").unwrap(), value!({}));
    }

    #[test]
    fn test_object_literal_strings() {
        assert_eq!(
            Evaluator::new().eval("{'foo': {'bar': 'tek'}}").unwrap(),
            value!({"foo": {"bar": "tek"}})
        );
    }

    #[test]
    fn test_object_literal_identifiers() {
        assert_eq!(
            Evaluator::new().eval("{foo: {bar: 'tek'}}").unwrap(),
            value!({"foo": {"bar": "tek"}})
        );
    }

    #[test]
    fn test_object_literal_properties() {
        assert_eq!(
            Evaluator::new().eval("{foo: 'bar'}.foo").unwrap(),
            value!("bar")
        );
    }

    #[test]
    fn test_array_literal() {
        assert_eq!(
            Evaluator::new().eval("['foo', 1+2]").unwrap(),
            value!(["foo", 3.0])
        );
    }

    #[test]
    fn test_array_literal_indexing() {
        assert_eq!(Evaluator::new().eval("[1, 2, 3][1]").unwrap(), value!(2.0));
    }

    #[test]
    fn test_in_operator_string() {
        assert_eq!(
            Evaluator::new().eval("'bar' in 'foobartek'").unwrap(),
            value!(true)
        );
        assert_eq!(
            Evaluator::new().eval("'baz' in 'foobartek'").unwrap(),
            value!(false)
        );
    }

    #[test]
    fn test_in_operator_array() {
        assert_eq!(
            Evaluator::new()
                .eval("'bar' in ['foo', 'bar', 'tek']")
                .unwrap(),
            value!(true)
        );
        assert_eq!(
            Evaluator::new()
                .eval("'baz' in ['foo', 'bar', 'tek']")
                .unwrap(),
            value!(false)
        );
    }

    #[test]
    fn test_in_operator_object() {
        assert_eq!(
            Evaluator::new()
                .eval("'bar' in {foo: 1, bar: 2, tek: 3}")
                .unwrap(),
            value!(true)
        );
        assert_eq!(
            Evaluator::new()
                .eval("'baz' in {foo: 1, bar: 2, tek: 3}")
                .unwrap(),
            value!(false)
        );
    }

    #[test]
    fn test_conditional_expression() {
        assert_eq!(
            Evaluator::new().eval("'foo' ? 1 : 2").unwrap(),
            value!(1f64)
        );
        assert_eq!(Evaluator::new().eval("'' ? 1 : 2").unwrap(), value!(2f64));
    }

    #[test]
    fn test_arbitrary_whitespace() {
        assert_eq!(
            Evaluator::new().eval("(\t2\n+\n3) *\n4\n\r\n").unwrap(),
            value!(20.0)
        );
    }

    #[test]
    fn test_non_integer() {
        assert_eq!(Evaluator::new().eval("1.5 * 3.0").unwrap(), value!(4.5));
    }

    #[test]
    fn test_string_literal() {
        assert_eq!(
            Evaluator::new().eval("'hello world'").unwrap(),
            value!("hello world")
        );
        assert_eq!(
            Evaluator::new().eval("\"hello world\"").unwrap(),
            value!("hello world")
        );
    }

    #[test]
    fn test_string_escapes() {
        assert_eq!(Evaluator::new().eval("'a\\'b'").unwrap(), value!("a'b"));
        assert_eq!(Evaluator::new().eval("\"a\\\"b\"").unwrap(), value!("a\"b"));
    }

    #[test]
    // Test a very simple transform that applies to_lowercase to a string
    fn test_simple_transform() {
        let evaluator = Evaluator::new().with_transform("lower", |v: &[Value]| {
            let s = v
                .get(0)
                .expect("There should be one argument!")
                .as_str()
                .expect("Should be a string!");
            Ok(value!(s.to_lowercase()))
        });
        assert_eq!(evaluator.eval("'T_T'|lower").unwrap(), value!("t_t"));
    }

    #[test]
    // Test returning an UnknownTransform error if a transform is unknown
    fn test_missing_transform() {
        let err = Evaluator::new().eval("'hello'|world").unwrap_err();
        if let EvaluationError::UnknownTransform(transform) = err {
            assert_eq!(transform, "world")
        } else {
            panic!("Should have thrown an unknown transform error")
        }
    }

    #[test]
    // Test returning an UndefinedIdentifier error if an identifier is unknown
    fn test_undefined_identifier() {
        let err = Evaluator::new().eval("not_defined").unwrap_err();
        if let EvaluationError::UndefinedIdentifier(id) = err {
            assert_eq!(id, "not_defined")
        } else {
            panic!("Should have thrown an undefined identifier error")
        }
    }

    #[test]
    // Test returning an UndefinedIdentifier error if an identifier is unknown
    fn test_undefined_identifier_truthy_ops() {
        let err = Evaluator::new().eval("not_defined").unwrap_err();
        if let EvaluationError::UndefinedIdentifier(id) = err {
            assert_eq!(id, "not_defined")
        } else {
            panic!("Should have thrown an undefined identifier error")
        }

        let evaluator = Evaluator::new();
        let context = value!({
            "NULL": null,
            "DEFINED": "string",
        });

        let test = |expr: &str, is_ok: bool, exp: Value| {
            let obs = evaluator.eval_in_context(&expr, context.clone());
            if !is_ok {
                assert!(obs.is_err());
                assert!(matches!(
                    obs.unwrap_err(),
                    EvaluationError::UndefinedIdentifier(_)
                ));
            } else {
                assert_eq!(obs.unwrap(), exp,);
            }
        };

        test("UNDEFINED", false, value!(null));
        test("UNDEFINED == 'string'", false, value!(null));
        test("'string' == UNDEFINED", false, value!(null));

        test("UNDEFINED ? 'WRONG' : 'RIGHT'", true, value!("RIGHT"));
        test("DEFINED ? UNDEFINED : 'WRONG'", false, value!(null));

        test("UNDEFINED || 'RIGHT'", true, value!("RIGHT"));
        test("'RIGHT' || UNDEFINED", true, value!("RIGHT"));

        test("'WRONG' && UNDEFINED", false, value!(null));
        test("UNDEFINED && 'WRONG'", false, value!(null));

        test("UNDEFINED && 'WRONG'", false, value!(null));

        test(
            "(UNDEFINED && UNDEFINED == 'string') || (DEFINED && DEFINED == 'string')",
            true,
            value!(true),
        );
    }

    #[test]
    fn test_add_multiple_transforms() {
        let evaluator = Evaluator::new()
            .with_transform("sqrt", |v: &[Value]| {
                let num = v
                    .first()
                    .expect("There should be one argument!")
                    .as_f64()
                    .expect("Should be a valid number!");
                Ok(value!(num.sqrt() as u64))
            })
            .with_transform("square", |v: &[Value]| {
                let num = v
                    .first()
                    .expect("There should be one argument!")
                    .as_f64()
                    .expect("Should be a valid number!");
                Ok(value!((num as u64).pow(2)))
            });

        assert_eq!(evaluator.eval("4|square").unwrap(), value!(16));
        assert_eq!(evaluator.eval("4|sqrt").unwrap(), value!(2));
        assert_eq!(evaluator.eval("4|square|sqrt").unwrap(), value!(4));
    }

    #[test]
    fn test_transform_with_argument() {
        let evaluator = Evaluator::new().with_transform("split", |args: &[Value]| {
            let s = args
                .first()
                .expect("Should be a first argument!")
                .as_str()
                .expect("Should be a string!");
            let c = args
                .get(1)
                .expect("There should be a second argument!")
                .as_str()
                .expect("Should be a string");
            let res: Vec<&str> = s.split_terminator(c).collect();
            Ok(value!(res))
        });

        assert_eq!(
            evaluator.eval("'John Doe'|split(' ')").unwrap(),
            value!(vec!["John", "Doe"])
        );
    }

    #[derive(Debug, thiserror::Error)]
    enum CustomError {
        #[error("Invalid argument in transform!")]
        InvalidArgument,
    }

    #[test]
    fn test_custom_error_message() {
        let evaluator = Evaluator::new().with_transform("error", |_: &[Value]| {
            Err(CustomError::InvalidArgument.into())
        });
        let res = evaluator.eval("1234|error");
        assert!(res.is_err());
        if let EvaluationError::CustomError(e) = res.unwrap_err() {
            assert_eq!(e.to_string(), "Invalid argument in transform!")
        } else {
            panic!("Should have returned a Custom error!")
        }
    }

    #[test]
    fn test_filter_collections_many_returned() {
        let evaluator = Evaluator::new();
        let context = value!({
            "foo": [
                {"bobo": 50, "fofo": 100},
                {"bobo": 60, "baz": 90},
                {"bobo": 10, "bar": 83},
                {"bobo": 20, "yam": 12},
            ]
        });
        let exp = "foo[.bobo >= 50]";
        assert_eq!(
            evaluator.eval_in_context(exp, context).unwrap(),
            value!([{"bobo": 50, "fofo": 100}, {"bobo": 60, "baz": 90}])
        );
    }

    #[test]
    fn test_binary_op_eq_ne() {
        let evaluator = Evaluator::new();
        let context = value!({
            "NULL": null,
            "STRING": "string",
            "BOOLEAN": true,
            "NUMBER": 42,
            "OBJECT": { "x": 1, "y": 2 },
            "ARRAY": [ "string" ]
        });

        let test = |l: &str, r: &str, exp: bool| {
            let expr = format!("{} == {}", l, r);
            assert_eq!(
                evaluator.eval_in_context(&expr, context.clone()).unwrap(),
                value!(exp)
            );

            let expr = format!("{} != {}", l, r);
            assert_eq!(
                evaluator.eval_in_context(&expr, context.clone()).unwrap(),
                value!(!exp)
            );
        };

        test("STRING", "'string'", true);
        test("NUMBER", "42", true);
        test("BOOLEAN", "true", true);
        test("OBJECT", "OBJECT", true);
        test("ARRAY", "[ 'string' ]", true);
        test("NULL", "null", true);

        test("OBJECT", "{ 'x': 1, 'y': 2 }", false);

        test("STRING", "NULL", false);
        test("NUMBER", "NULL", false);
        test("BOOLEAN", "NULL", false);
        // test("NULL", "NULL", false);
        test("OBJECT", "NULL", false);
        test("ARRAY", "NULL", false);

        // test("STRING", "STRING", false);
        test("NUMBER", "STRING", false);
        test("BOOLEAN", "STRING", false);
        test("NULL", "STRING", false);
        test("OBJECT", "STRING", false);
        test("ARRAY", "STRING", false);

        test("STRING", "NUMBER", false);
        // test("NUMBER", "NUMBER", false);
        test("BOOLEAN", "NUMBER", false);
        test("NULL", "NUMBER", false);
        test("OBJECT", "NUMBER", false);
        test("ARRAY", "NUMBER", false);

        test("STRING", "BOOLEAN", false);
        test("NUMBER", "BOOLEAN", false);
        // test("BOOLEAN", "BOOLEAN", false);
        test("NULL", "BOOLEAN", false);
        test("OBJECT", "BOOLEAN", false);
        test("ARRAY", "BOOLEAN", false);

        test("STRING", "OBJECT", false);
        test("NUMBER", "OBJECT", false);
        test("BOOLEAN", "OBJECT", false);
        test("NULL", "OBJECT", false);
        // test("OBJECT", "OBJECT", false);
        test("ARRAY", "OBJECT", false);

        test("STRING", "ARRAY", false);
        test("NUMBER", "ARRAY", false);
        test("BOOLEAN", "ARRAY", false);
        test("NULL", "ARRAY", false);
        test("OBJECT", "ARRAY", false);
        // test("ARRAY", "ARRAY", false);
    }

    #[test]
    fn test_binary_op_string_gt_lt_gte_lte() {
        let evaluator = Evaluator::new();
        let context = value!({
            "A": "A string",
            "B": "B string",
        });

        let test = |l: &str, r: &str, is_gt: bool| {
            let expr = format!("{} > {}", l, r);
            assert_eq!(
                evaluator.eval_in_context(&expr, context.clone()).unwrap(),
                value!(is_gt)
            );

            let expr = format!("{} <= {}", l, r);
            assert_eq!(
                evaluator.eval_in_context(&expr, context.clone()).unwrap(),
                value!(!is_gt)
            );

            // we test equality in another test
            let expr = format!("{} == {}", l, r);
            let is_eq = evaluator
                .eval_in_context(&expr, context.clone())
                .unwrap()
                .as_bool()
                .unwrap();

            if is_eq {
                let expr = format!("{} >= {}", l, r);
                assert_eq!(
                    evaluator.eval_in_context(&expr, context.clone()).unwrap(),
                    value!(true)
                );
            } else {
                let expr = format!("{} < {}", l, r);
                assert_eq!(
                    evaluator.eval_in_context(&expr, context.clone()).unwrap(),
                    value!(!is_gt)
                );
            }
        };

        test("A", "B", false);
        test("B", "A", true);
        test("A", "A", false);
    }

    #[test]
    fn test_lazy_eval_binary_op_and_or() {
        let evaluator = Evaluator::new();
        // error is a missing transform
        let res = evaluator.eval("42 || 0|error");
        assert!(res.is_ok());
        assert_eq!(res.unwrap(), value!(42.0));

        let res = evaluator.eval("false || 0|error");
        assert!(res.is_err());

        let res = evaluator.eval("42 && 0|error");
        assert!(res.is_err());

        let res = evaluator.eval("false && 0|error");
        assert!(res.is_ok());
        assert_eq!(res.unwrap(), value!(false));
    }

    #[test]
    fn test_lazy_eval_trinary_op() {
        let evaluator = Evaluator::new();
        // error is a missing transform
        let res = evaluator.eval("true ? 42 : 0|error");
        assert!(res.is_ok());
        assert_eq!(res.unwrap(), value!(42.0));

        let res = evaluator.eval("true ? 0|error : 42");
        assert!(res.is_err());

        let res = evaluator.eval("true ? 0|error : 42");
        assert!(res.is_err());

        let res = evaluator.eval("false ? 0|error : 42");
        assert!(res.is_ok());
        assert_eq!(res.unwrap(), value!(42.0));
    }
}
