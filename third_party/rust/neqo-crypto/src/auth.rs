// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use strum::FromRepr;

use crate::err::{mozpkix, sec, ssl, PRErrorCode};

/// The outcome of authentication.
#[derive(Clone, Copy, Debug, PartialEq, Eq, FromRepr)]
#[repr(i32)]
pub enum AuthenticationStatus {
    Ok,
    CaInvalid = sec::SEC_ERROR_CA_CERT_INVALID,
    CaNotV3 = mozpkix::MOZILLA_PKIX_ERROR_V1_CERT_USED_AS_CA,
    CertAlgorithmDisabled = sec::SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED,
    CertExpired = sec::SEC_ERROR_EXPIRED_CERTIFICATE,
    CertInvalidTime = sec::SEC_ERROR_INVALID_TIME,
    CertIsCa = mozpkix::MOZILLA_PKIX_ERROR_CA_CERT_USED_AS_END_ENTITY,
    CertKeyUsage = sec::SEC_ERROR_INADEQUATE_KEY_USAGE,
    CertMitm = mozpkix::MOZILLA_PKIX_ERROR_MITM_DETECTED,
    CertNotYetValid = mozpkix::MOZILLA_PKIX_ERROR_NOT_YET_VALID_CERTIFICATE,
    CertRevoked = sec::SEC_ERROR_REVOKED_CERTIFICATE,
    CertSelfSigned = mozpkix::MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT,
    CertSubjectInvalid = ssl::SSL_ERROR_BAD_CERT_DOMAIN,
    CertUntrusted = sec::SEC_ERROR_UNTRUSTED_CERT,
    CertWeakKey = mozpkix::MOZILLA_PKIX_ERROR_INADEQUATE_KEY_SIZE,
    IssuerEmptyName = mozpkix::MOZILLA_PKIX_ERROR_EMPTY_ISSUER_NAME,
    IssuerExpired = sec::SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE,
    IssuerNotYetValid = mozpkix::MOZILLA_PKIX_ERROR_NOT_YET_VALID_ISSUER_CERTIFICATE,
    IssuerUnknown = sec::SEC_ERROR_UNKNOWN_ISSUER,
    IssuerUntrusted = sec::SEC_ERROR_UNTRUSTED_ISSUER,
    PolicyRejection = mozpkix::MOZILLA_PKIX_ERROR_ADDITIONAL_POLICY_CONSTRAINT_FAILED,
    Unknown = sec::SEC_ERROR_LIBRARY_FAILURE,
}

impl From<AuthenticationStatus> for PRErrorCode {
    fn from(v: AuthenticationStatus) -> Self {
        v as Self
    }
}

// Note that this mapping should be removed after gecko eventually learns how to
// map into the enumerated type.
impl From<PRErrorCode> for AuthenticationStatus {
    fn from(v: PRErrorCode) -> Self {
        Self::from_repr(v).unwrap_or(Self::Unknown)
    }
}
