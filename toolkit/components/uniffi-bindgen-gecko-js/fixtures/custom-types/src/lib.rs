/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::collections::HashMap;
use url::Url;

// Custom Handle type which trivially wraps an i64.
pub struct Handle(pub i64);

// We must implement the UniffiCustomTypeConverter trait for each custom type on the scaffolding side
uniffi::custom_type!(Handle, i64, {
    try_lift: |val| Ok(Handle(val)),
    lower: |obj| obj.0,
});

// Use `url::Url` as a custom type, with `String` as the Builtin
uniffi::custom_type!(Url, String, {
    remote,
    try_lift: |val| Ok(Url::parse(&val)?),
    lower: |obj| obj.to_string(),
});

// And a little struct and function that ties them together.
pub struct CustomTypesDemo {
    url: Url,
    handle: Handle,
}

// Define the enum that matches the UDL definition
// We don't need to derive uniffi::Enum for this one
#[derive(Debug, Clone, Copy)]
pub enum SequentialEnum {
    First,
    Second,
    Fourth,
    Tenth,
    Eleventh,
    Thirteenth,
}

// Test enum with explicit discriminant values and gaps
#[repr(u8)]
#[derive(Debug, Clone, Copy, uniffi::Enum)]
pub enum ExplicitValuedEnum {
    First = 1,
    Second = 2,
    Fourth = 4,
    Tenth = 10,
    Eleventh = 11,
    Thirteenth = 13,
}

// Example with sequential and explicit values mixed
#[repr(u8)]
#[derive(Debug, Clone, Copy, uniffi::Enum)]
pub enum GappedEnum {
    One = 10,
    Two, // should be 11
    Three = 14,
}

pub fn get_custom_types_demo(v: Option<CustomTypesDemo>) -> CustomTypesDemo {
    v.unwrap_or_else(|| CustomTypesDemo {
        url: Url::parse("http://example.com/").unwrap(),
        handle: Handle(123),
    })
}

// Functions for sequential enum (UDL-defined)
pub fn get_sequential_discriminant(value: SequentialEnum) -> u8 {
    value as u8
}

pub fn echo_sequential_value(value: SequentialEnum) -> SequentialEnum {
    value
}

// Export proc-macro based enum to JS via scaffolding
#[uniffi::export]
pub fn get_explicit_enum_values() -> HashMap<String, u8> {
    let mut values = HashMap::new();
    values.insert("First".to_string(), ExplicitValuedEnum::First as u8);
    values.insert("Second".to_string(), ExplicitValuedEnum::Second as u8);
    values.insert("Fourth".to_string(), ExplicitValuedEnum::Fourth as u8);
    values.insert("Tenth".to_string(), ExplicitValuedEnum::Tenth as u8);
    values.insert("Eleventh".to_string(), ExplicitValuedEnum::Eleventh as u8);
    values.insert(
        "Thirteenth".to_string(),
        ExplicitValuedEnum::Thirteenth as u8,
    );
    values
}

#[uniffi::export]
pub fn get_explicit_discriminant(value: ExplicitValuedEnum) -> u8 {
    value as u8
}

#[uniffi::export]
pub fn echo_explicit_value(value: ExplicitValuedEnum) -> ExplicitValuedEnum {
    value
}

// Functions for GappedEnum
#[uniffi::export]
pub fn get_gapped_enum_values() -> HashMap<String, u8> {
    let mut values = HashMap::new();
    values.insert("One".to_string(), GappedEnum::One as u8);
    values.insert("Two".to_string(), GappedEnum::Two as u8);
    values.insert("Three".to_string(), GappedEnum::Three as u8);
    values
}

#[uniffi::export]
pub fn get_gapped_discriminant(value: GappedEnum) -> u8 {
    value as u8
}

#[uniffi::export]
pub fn echo_gapped_value(value: GappedEnum) -> GappedEnum {
    value
}

include!(concat!(env!("OUT_DIR"), "/custom-types.uniffi.rs"));
