// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use core::{
    convert::Infallible,
    fmt::{self, Debug},
};

use alloc::vec::Vec;
use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};

use super::{Credential, CredentialType, MlsCredential};

#[derive(Clone, PartialEq, Eq, Hash, PartialOrd, Ord, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
// #[cfg_attr(
//     all(feature = "ffi", not(test)),
//     safer_ffi_gen::ffi_type(clone, opaque)
// )]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
/// Bare assertion of an identity without any additional information.
///
/// The format of the encoded identity is defined by the application.
///
///
/// # Warning
///
/// Basic credentials are inherently insecure since they can not be
/// properly validated. It is not recommended to use [`BasicCredential`]
/// in production applications.
pub struct BasicCredential {
    /// Underlying identifier as raw bytes.
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    #[cfg_attr(feature = "serde", serde(with = "crate::vec_serde"))]
    pub identifier: Vec<u8>,
}

impl Debug for BasicCredential {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        crate::debug::pretty_bytes(&self.identifier)
            .named("BasicCredential")
            .fmt(f)
    }
}

// #[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::safer_ffi_gen)]
impl BasicCredential {
    /// Create a new basic credential with raw bytes.
    pub fn new(identifier: Vec<u8>) -> BasicCredential {
        BasicCredential { identifier }
    }

    /// Underlying identifier as raw bytes.
    #[cfg(feature = "ffi")]
    pub fn identifier(&self) -> &[u8] {
        &self.identifier
    }
}

impl BasicCredential {
    pub fn credential_type() -> CredentialType {
        CredentialType::BASIC
    }

    pub fn into_credential(self) -> Credential {
        Credential::Basic(self)
    }
}

impl MlsCredential for BasicCredential {
    type Error = Infallible;

    fn credential_type() -> CredentialType {
        Self::credential_type()
    }

    fn into_credential(self) -> Result<Credential, Self::Error> {
        Ok(self.into_credential())
    }
}
