use crate::{MlsDecode, MlsEncode, MlsSize};
use alloc::{string::String, vec::Vec};

impl MlsSize for str {
    fn mls_encoded_len(&self) -> usize {
        self.as_bytes().mls_encoded_len()
    }
}

impl MlsEncode for str {
    fn mls_encode(&self, writer: &mut Vec<u8>) -> Result<(), crate::Error> {
        self.as_bytes().mls_encode(writer)
    }
}

impl MlsSize for String {
    fn mls_encoded_len(&self) -> usize {
        self.as_str().mls_encoded_len()
    }
}

impl MlsEncode for String {
    fn mls_encode(&self, writer: &mut Vec<u8>) -> Result<(), crate::Error> {
        self.as_str().mls_encode(writer)
    }
}

impl MlsDecode for String {
    fn mls_decode(reader: &mut &[u8]) -> Result<Self, crate::Error> {
        String::from_utf8(Vec::mls_decode(reader)?).map_err(|_| crate::Error::Utf8)
    }
}

#[cfg(test)]
mod tests {
    use crate::{Error, MlsDecode, MlsEncode};
    use alloc::string::String;
    use assert_matches::assert_matches;

    #[cfg(target_arch = "wasm32")]
    use wasm_bindgen_test::wasm_bindgen_test as test;

    #[test]
    fn serialization_works() {
        assert_eq!(
            vec![3, b'b', b'a', b'r'],
            "bar".mls_encode_to_vec().unwrap()
        );
    }

    #[test]
    fn data_round_trips() {
        let val = "foo";
        let x = val.mls_encode_to_vec().unwrap();
        assert_eq!(val, String::mls_decode(&mut &*x).unwrap());
    }

    #[test]
    fn empty_string_can_be_deserialized() {
        assert_eq!(String::new(), String::mls_decode(&mut &[0u8][..]).unwrap());
    }

    #[test]
    fn too_short_string_to_deserialize_gives_an_error() {
        assert_matches!(
            String::mls_decode(&mut &[2, 3][..]),
            Err(Error::UnexpectedEOF)
        );
    }

    #[test]
    fn deserializing_invalid_utf8_fails() {
        assert_matches!(
            String::mls_decode(&mut &[0x02, 0xdf, 0xff][..]),
            Err(Error::Utf8)
        );
    }
}
