#![allow(dead_code, missing_docs)]

use core::fmt::Debug;
use expect_test::Expect;
use pretty_assertions::assert_eq;
use serde::{de::DeserializeOwned, Serialize};

#[track_caller]
pub fn is_equal<T>(value: T, expected: Expect)
where
    T: Debug + DeserializeOwned + PartialEq + Serialize,
{
    let serialized = serde_json::to_string_pretty(&value).unwrap();
    expected.assert_eq(&serialized);
    assert_eq!(
        value,
        serde_json::from_str::<T>(&serialized).unwrap(),
        "Deserialization differs from expected value."
    );
}

/// Like [`is_equal`] but not pretty-print
#[track_caller]
pub fn is_equal_compact<T>(value: T, expected: Expect)
where
    T: Debug + DeserializeOwned + PartialEq + Serialize,
{
    let serialized = serde_json::to_string(&value).unwrap();
    expected.assert_eq(&serialized);
    assert_eq!(
        value,
        serde_json::from_str::<T>(&serialized).unwrap(),
        "Deserialization differs from expected value."
    );
}

#[track_caller]
pub fn check_deserialization<T>(value: T, deserialize_from: &str)
where
    T: Debug + DeserializeOwned + PartialEq,
{
    assert_eq!(
        value,
        serde_json::from_str::<T>(deserialize_from).unwrap(),
        "Deserialization differs from expected value."
    );
}

#[track_caller]
pub fn check_serialization<T>(value: T, serialize_to: Expect)
where
    T: Debug + Serialize,
{
    serialize_to.assert_eq(&serde_json::to_string_pretty(&value).unwrap());
}

#[track_caller]
pub fn check_error_serialization<T>(value: T, error_msg: Expect)
where
    T: Debug + Serialize,
{
    error_msg.assert_eq(
        &serde_json::to_string_pretty(&value)
            .unwrap_err()
            .to_string(),
    );
}

#[track_caller]
pub fn check_error_deserialization<T>(deserialize_from: &str, error_msg: Expect)
where
    T: Debug + DeserializeOwned,
{
    error_msg.assert_eq(
        &serde_json::from_str::<T>(deserialize_from)
            .unwrap_err()
            .to_string(),
    );
}

#[track_caller]
pub fn check_matches_schema<T>(value: &serde_json::Value)
where
    T: schemars_0_8::JsonSchema,
{
    use jsonschema::Validator;
    use std::fmt::Write;

    if cfg!(feature = "schemars_0_8") {
        let schema_object = serde_json::to_value(schemars_0_8::schema_for!(T))
            .expect("schema for T could not be serialized to json");
        let schema = match Validator::new(&schema_object) {
            Ok(schema) => schema,
            Err(e) => panic!("schema for T was not a valid JSON schema: {e}"),
        };

        if let Err(err) = schema.validate(value) {
            let mut message = String::new();

            let _ = writeln!(
                &mut message,
                "Object was not valid according to its own schema:"
            );

            let _ = writeln!(&mut message, "  -> {}", err);
            let _ = writeln!(&mut message);
            let _ = writeln!(&mut message, "Object Value:");
            let _ = writeln!(
                &mut message,
                "{}",
                serde_json::to_string_pretty(&value).unwrap_or_else(|e| format!("> error: {e}"))
            );
            let _ = writeln!(&mut message);
            let _ = writeln!(&mut message, "JSON Schema:");
            let _ = writeln!(
                &mut message,
                "{}",
                serde_json::to_string_pretty(&schema_object)
                    .unwrap_or_else(|e| format!("> error: {e}"))
            );

            panic!("{}", message);
        };
    }
}

#[track_caller]
pub fn check_valid_json_schema<T>(value: &T)
where
    T: schemars_0_8::JsonSchema + Serialize,
{
    let value = serde_json::to_value(value).expect("could not serialize T to json");

    check_matches_schema::<T>(&value);
}
