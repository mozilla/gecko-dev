// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::{DerCertificate, DerCertificateRequest};

use alloc::vec::Vec;
use mls_rs_core::{crypto::SignaturePublicKey, error::IntoAnyError};

#[cfg(all(test, feature = "std"))]
use mockall::automock;

use alloc::string::String;

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
/// Subject alt name extension values.
pub enum SubjectAltName {
    Email(String),
    Uri(String),
    Dns(String),
    Rid(String),
    Ip(String),
}

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
/// X.509 name components.
pub enum SubjectComponent {
    CommonName(String),
    Surname(String),
    SerialNumber(String),
    CountryName(String),
    Locality(String),
    State(String),
    StreetAddress(String),
    OrganizationName(String),
    OrganizationalUnit(String),
    Title(String),
    GivenName(String),
    EmailAddress(String),
    UserId(String),
    DomainComponent(String),
    Initials(String),
    GenerationQualifier(String),
    DistinguishedNameQualifier(String),
    Pseudonym(String),
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
/// Parameters used to generate certificate requests.
pub struct CertificateRequestParameters {
    pub subject: Vec<SubjectComponent>,
    pub subject_alt_names: Vec<SubjectAltName>,
    pub is_ca: bool,
}

#[cfg_attr(all(test, feature = "std"), automock(type Error = crate::test_utils::TestError;))]
/// Trait for X.509 CSR writing.
pub trait X509RequestWriter {
    type Error: IntoAnyError;

    fn write(
        &self,
        params: CertificateRequestParameters,
    ) -> Result<DerCertificateRequest, Self::Error>;
}

#[cfg_attr(all(test, feature = "std"), automock(type Error = crate::test_utils::TestError;))]
/// Trait for X.509 certificate parsing.
pub trait X509CertificateReader {
    type Error: IntoAnyError;

    /// Der encoded bytes of a certificate subject field.
    fn subject_bytes(&self, certificate: &DerCertificate) -> Result<Vec<u8>, Self::Error>;

    /// Parsed certificate subject field components.
    fn subject_components(
        &self,
        certificate: &DerCertificate,
    ) -> Result<Vec<SubjectComponent>, Self::Error>;

    /// Parsed subject alt name extensions of a certificate.
    fn subject_alt_names(
        &self,
        certificate: &DerCertificate,
    ) -> Result<Vec<SubjectAltName>, Self::Error>;

    /// Get the subject public key of a certificate.
    fn public_key(&self, certificate: &DerCertificate) -> Result<SignaturePublicKey, Self::Error>;
}
