// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};

#[derive(Debug, Clone, PartialEq, Eq, MlsSize, MlsEncode, MlsDecode)]
struct TestTupleStruct(u64);

#[derive(Debug, Clone, PartialEq, Eq, MlsSize, MlsEncode, MlsDecode)]
struct TestFieldStruct {
    item1: Option<u8>,
    item2: u64,
}

#[derive(Debug, Clone, PartialEq, Eq, MlsSize, MlsEncode, MlsDecode)]
struct TestType {
    field_a: u8,
    field_b: Vec<u8>,
    field_c: u16,
    field_d: Option<Vec<u8>>,
    field_e: u32,
    field_f: Option<u16>,
    field_g: Vec<TestTupleStruct>,
    field_h: TestFieldStruct,
}

#[repr(u16)]
#[derive(Debug, Clone, PartialEq, Eq, MlsSize, MlsEncode, MlsDecode)]
enum TestEnum {
    Case1 = 1u16,
    Case2(TestFieldStruct) = 200u16,
    Case3(TestTupleStruct) = 42u16,
}

#[derive(Debug, Clone, PartialEq, Eq, MlsSize, MlsEncode, MlsDecode)]
#[repr(u8)]
enum TestEnumWithoutSuffixedLiterals {
    Case1 = 1,
    Case2(TestFieldStruct) = 200,
    Case3(TestTupleStruct) = 42,
}

#[derive(Debug, Clone, PartialEq, Eq, MlsSize, MlsEncode, MlsDecode)]
struct TestGeneric<T: MlsSize + MlsEncode + MlsDecode>(T);

#[test]
fn round_trip_struct_encode() {
    let item = TestType {
        field_a: 42,
        field_b: vec![1, 3, 5, 7, 9],
        field_c: 65000,
        field_d: Some(vec![0, 2, 4, 6, 8]),
        field_e: 1000000,
        field_f: None,
        field_g: vec![
            TestTupleStruct(100),
            TestTupleStruct(200),
            TestTupleStruct(300),
        ],
        field_h: TestFieldStruct {
            item1: Some(42),
            item2: 84,
        },
    };

    let data = item.mls_encode_to_vec().unwrap();
    let restored = TestType::mls_decode(&mut &*data).unwrap();

    assert_eq!(restored, item);
}

#[test]
fn round_trip_generic_encode() {
    let item = TestGeneric(42u16);
    let data = item.mls_encode_to_vec().unwrap();
    let restored = TestGeneric::mls_decode(&mut &*data).unwrap();

    assert_eq!(restored, item);
}

#[test]
fn round_trip_enum_encode_simple() {
    let item = TestEnum::Case1;

    let serialized = item.mls_encode_to_vec().unwrap();
    let decoded = TestEnum::mls_decode(&mut &*serialized).unwrap();

    assert_eq!(serialized, 1u16.mls_encode_to_vec().unwrap());
    assert_eq!(decoded, item);
}

#[test]
fn round_trip_enum_encode_one_field() {
    let item = TestEnum::Case2(TestFieldStruct {
        item1: None,
        item2: 42,
    });

    let serialized = item.mls_encode_to_vec().unwrap();
    let decoded = TestEnum::mls_decode(&mut &*serialized).unwrap();

    assert_eq!(decoded, item);
}

#[test]
fn round_trip_enum_encode_one_tuple() {
    let item = TestEnum::Case3(TestTupleStruct(42));

    let serialized = item.mls_encode_to_vec().unwrap();
    let decoded = TestEnum::mls_decode(&mut &*serialized).unwrap();

    assert_eq!(decoded, item);
}

#[test]
fn round_trip_custom_module_struct() {
    #[derive(Debug, PartialEq, Eq, Clone, MlsSize, MlsEncode, MlsDecode)]
    struct TestCustomStruct {
        #[mls_codec(with = "self::test_with")]
        value: u8,
    }

    let item = TestCustomStruct { value: 33 };

    let serialized = item.mls_encode_to_vec().unwrap();
    assert_eq!(serialized.len(), 2);

    let decoded = TestCustomStruct::mls_decode(&mut &*serialized).unwrap();
    assert_eq!(item, decoded);
}

#[test]
fn round_trip_custom_module_enum() {
    #[derive(Debug, PartialEq, Eq, Clone, MlsSize, MlsEncode, MlsDecode)]
    #[repr(u16)]
    enum TestCustomEnum {
        CustomCase(#[mls_codec(with = "self::test_with")] u8) = 2u16,
    }

    let item = TestCustomEnum::CustomCase(33);

    let serialized = item.mls_encode_to_vec().unwrap();
    assert_eq!(serialized.len(), 4);

    let decoded = TestCustomEnum::mls_decode(&mut &*serialized).unwrap();
    assert_eq!(item, decoded)
}

mod test_with {
    use mls_rs_codec::MlsDecode;

    pub fn mls_encoded_len(_val: &u8) -> usize {
        2
    }

    pub fn mls_encode(val: &u8, writer: &mut Vec<u8>) -> Result<(), mls_rs_codec::Error> {
        writer.extend([*val, 42]);
        Ok(())
    }

    pub fn mls_decode(reader: &mut &[u8]) -> Result<u8, mls_rs_codec::Error> {
        Ok(<[u8; 2]>::mls_decode(reader)?[0])
    }
}
