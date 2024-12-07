// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use core::{
    convert::Infallible,
    fmt::{self, Debug},
    ops::{Deref, DerefMut},
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
/// X.509 certificate in DER format.
pub struct DerCertificate(
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    #[cfg_attr(feature = "serde", serde(with = "crate::vec_serde"))]
    Vec<u8>,
);

impl Debug for DerCertificate {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        crate::debug::pretty_bytes(&self.0)
            .named("DerCertificate")
            .fmt(f)
    }
}

// #[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::safer_ffi_gen)]
impl DerCertificate {
    /// Create a der certificate from raw bytes.
    pub fn new(data: Vec<u8>) -> DerCertificate {
        DerCertificate(data)
    }

    /// Convert this certificate into raw bytes.
    pub fn into_vec(self) -> Vec<u8> {
        self.0
    }
}

impl From<Vec<u8>> for DerCertificate {
    fn from(data: Vec<u8>) -> Self {
        DerCertificate(data)
    }
}

impl Deref for DerCertificate {
    type Target = [u8];

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl AsRef<[u8]> for DerCertificate {
    fn as_ref(&self) -> &[u8] {
        &self.0
    }
}

#[derive(Clone, Debug, PartialEq, Eq, Hash, PartialOrd, Ord, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
// #[cfg_attr(
//     all(feature = "ffi", not(test)),
//     safer_ffi_gen::ffi_type(clone, opaque)
// )]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
/// A chain of [`DerCertificate`] that is ordered from leaf to root.
///
/// Certificate chains MAY leave out root CA's so long as they are
/// provided as input to whatever certificate validator ultimately is
/// verifying the chain.
pub struct CertificateChain(Vec<DerCertificate>);

impl Deref for CertificateChain {
    type Target = Vec<DerCertificate>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl DerefMut for CertificateChain {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}

impl From<Vec<DerCertificate>> for CertificateChain {
    fn from(cert_data: Vec<DerCertificate>) -> Self {
        CertificateChain(cert_data)
    }
}

impl From<Vec<Vec<u8>>> for CertificateChain {
    fn from(value: Vec<Vec<u8>>) -> Self {
        CertificateChain(value.into_iter().map(DerCertificate).collect())
    }
}

impl FromIterator<DerCertificate> for CertificateChain {
    fn from_iter<T: IntoIterator<Item = DerCertificate>>(iter: T) -> Self {
        CertificateChain::from(iter.into_iter().collect::<Vec<_>>())
    }
}

impl CertificateChain {
    /// Get the leaf certificate, which is the first certificate in the chain.
    pub fn leaf(&self) -> Option<&DerCertificate> {
        self.0.first()
    }

    /// Convert this certificate chain into a [`Credential`] enum.
    pub fn into_credential(self) -> Credential {
        Credential::X509(self)
    }
}

impl MlsCredential for CertificateChain {
    type Error = Infallible;

    fn credential_type() -> CredentialType {
        CredentialType::X509
    }

    fn into_credential(self) -> Result<Credential, Self::Error> {
        Ok(self.into_credential())
    }
}
