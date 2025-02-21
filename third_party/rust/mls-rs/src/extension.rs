// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

pub use mls_rs_core::extension::{ExtensionType, MlsCodecExtension, MlsExtension};

pub(crate) use built_in::*;
#[cfg(feature = "last_resort_key_package_ext")]
pub(crate) use recommended::*;

/// Default extension types required by the MLS RFC.
pub mod built_in;

/// Extension types which are not mandatory, but still recommended.
#[cfg(feature = "last_resort_key_package_ext")]
pub mod recommended;

#[cfg(test)]
pub(crate) mod test_utils {
    use alloc::vec::Vec;
    use core::convert::Infallible;
    use core::fmt::Debug;
    use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};
    use mls_rs_core::extension::MlsExtension;

    use super::*;

    pub const TEST_EXTENSION_TYPE: u16 = 42;

    #[derive(MlsSize, MlsEncode, MlsDecode, Clone, Debug, PartialEq)]
    pub(crate) struct TestExtension {
        pub(crate) foo: u8,
    }

    impl From<u8> for TestExtension {
        fn from(value: u8) -> Self {
            Self { foo: value }
        }
    }

    impl MlsExtension for TestExtension {
        type SerializationError = Infallible;

        type DeserializationError = Infallible;

        fn extension_type() -> ExtensionType {
            ExtensionType::from(TEST_EXTENSION_TYPE)
        }

        fn to_bytes(&self) -> Result<Vec<u8>, Self::SerializationError> {
            Ok([self.foo].to_vec())
        }

        fn from_bytes(data: &[u8]) -> Result<Self, Self::DeserializationError> {
            Ok(TestExtension { foo: data[0] })
        }
    }
}
