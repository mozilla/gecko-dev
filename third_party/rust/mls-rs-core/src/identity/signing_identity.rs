// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};

use crate::crypto::SignaturePublicKey;

use super::Credential;

#[derive(Debug, Clone, Eq, Hash, PartialEq, PartialOrd, Ord, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
// #[cfg_attr(
//     all(feature = "ffi", not(test)),
//     safer_ffi_gen::ffi_type(clone, opaque)
// )]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
/// MLS group member identity represented as a combination of a
/// public [`SignaturePublicKey`] and [`Credential`].
pub struct SigningIdentity {
    pub signature_key: SignaturePublicKey,
    pub credential: Credential,
}

impl SigningIdentity {
    /// Create a new signing identity from `credential` and `signature_key`
    pub fn new(credential: Credential, signature_key: SignaturePublicKey) -> SigningIdentity {
        SigningIdentity {
            credential,
            signature_key,
        }
    }
}
