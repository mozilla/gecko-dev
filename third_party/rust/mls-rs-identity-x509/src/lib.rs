// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#![cfg_attr(not(feature = "std"), no_std)]
extern crate alloc;

mod error;
mod identity_extractor;
mod provider;
mod traits;
mod util;

use alloc::vec::Vec;
use core::fmt::{self, Debug};

pub use error::*;
pub use identity_extractor::*;
pub use provider::*;
pub use traits::*;

pub use mls_rs_core::identity::{CertificateChain, DerCertificate};

#[derive(Clone, PartialEq, Eq)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
/// X.509 certificate request in DER format.
pub struct DerCertificateRequest(Vec<u8>);

impl Debug for DerCertificateRequest {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        mls_rs_core::debug::pretty_bytes(&self.0)
            .named("DerCertificateRequest")
            .fmt(f)
    }
}

#[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::safer_ffi_gen)]
impl DerCertificateRequest {
    /// Create a DER certificate request from raw bytes.
    pub fn new(data: Vec<u8>) -> DerCertificateRequest {
        DerCertificateRequest(data)
    }

    /// Convert this certificate request into raw bytes.
    pub fn into_vec(self) -> Vec<u8> {
        self.0
    }
}

impl From<Vec<u8>> for DerCertificateRequest {
    fn from(data: Vec<u8>) -> Self {
        DerCertificateRequest(data)
    }
}

impl AsRef<[u8]> for DerCertificateRequest {
    fn as_ref(&self) -> &[u8] {
        &self.0
    }
}

#[cfg(all(test, feature = "std"))]
pub(crate) mod test_utils {

    use alloc::vec;
    use mls_rs_core::{crypto::SignaturePublicKey, error::IntoAnyError, identity::SigningIdentity};
    use rand::{thread_rng, Rng};

    use crate::{CertificateChain, DerCertificate};

    #[derive(Debug, thiserror::Error)]
    #[error("test error")]
    pub struct TestError;

    impl IntoAnyError for TestError {
        fn into_dyn_error(self) -> Result<Box<dyn std::error::Error + Send + Sync>, Self> {
            Ok(self.into())
        }
    }

    pub fn test_certificate_chain() -> CertificateChain {
        (0..3)
            .map(|_| {
                let mut data = [0u8; 32];
                thread_rng().fill(&mut data);
                DerCertificate::from(data.to_vec())
            })
            .collect::<CertificateChain>()
    }

    pub fn test_signing_identity() -> SigningIdentity {
        let chain = test_certificate_chain();
        test_signing_identity_with_chain(chain)
    }

    pub fn test_signing_identity_with_chain(chain: CertificateChain) -> SigningIdentity {
        SigningIdentity {
            signature_key: SignaturePublicKey::from(vec![0u8; 128]),
            credential: chain.into_credential(),
        }
    }
}
