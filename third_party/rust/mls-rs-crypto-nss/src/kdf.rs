// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use core::fmt::Debug;

use mls_rs_core::{crypto::CipherSuite, error::IntoAnyError};
use mls_rs_crypto_traits::{KdfId, KdfType};

use alloc::vec::Vec;

#[derive(Debug)]
#[cfg_attr(feature = "std", derive(thiserror::Error))]
pub enum KdfError {
    // #[cfg_attr(feature = "std", error("invalid prk length"))]
    // InvalidPrkLength,
    // #[cfg_attr(feature = "std", error("invalid length"))]
    // InvalidLength,
    #[cfg_attr(
        feature = "std",
        error("the provided length of the key {0} is shorter than the minimum length {1}")
    )]
    TooShortKey(usize, usize),
    #[cfg_attr(feature = "std", error("invalid input"))]
    InvalidInput,
    #[cfg_attr(feature = "std", error("internal error"))]
    InternalError,
    #[cfg_attr(feature = "std", error("unsupported cipher suite"))]
    UnsupportedCipherSuite,
}

impl From<nss_gk_api::hkdf::HkdfError> for KdfError {
    fn from(_value: nss_gk_api::hkdf::HkdfError) -> Self {
        KdfError::InvalidInput
    }
}

impl IntoAnyError for KdfError {
    #[cfg(feature = "std")]
    fn into_dyn_error(self) -> Result<Box<dyn std::error::Error + Send + Sync>, Self> {
        Ok(self.into())
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct Kdf(KdfId);

impl Kdf {
    pub fn new(cipher_suite: CipherSuite) -> Option<Self> {
        KdfId::new(cipher_suite).map(Self)
    }
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
#[cfg_attr(all(target_arch = "wasm32", mls_build_async), maybe_async::must_be_async(?Send))]
#[cfg_attr(
    all(not(target_arch = "wasm32"), mls_build_async),
    maybe_async::must_be_async
)]
impl KdfType for Kdf {
    type Error = KdfError;

    async fn expand(&self, prk: &[u8], info: &[u8], len: usize) -> Result<Vec<u8>, KdfError> {
        if prk.len() < self.extract_size() {
            return Err(KdfError::TooShortKey(prk.len(), self.extract_size()));
        }

        nss_gk_api::init();

        let alg = match self.0 {
            KdfId::HkdfSha256 => Ok(nss_gk_api::hkdf::HkdfAlgorithm::HKDF_SHA2_256),
            KdfId::HkdfSha384 => Ok(nss_gk_api::hkdf::HkdfAlgorithm::HKDF_SHA2_384),
            KdfId::HkdfSha512 => Ok(nss_gk_api::hkdf::HkdfAlgorithm::HKDF_SHA2_512),
            _ => Err(KdfError::UnsupportedCipherSuite),
        }?;

        let hkdf = nss_gk_api::hkdf::Hkdf::new(alg);
        let prk_symkey = hkdf.import_secret(prk).unwrap();

        // Expand
        let r = hkdf
            .expand_data(&prk_symkey, info, len)
            .expect("HkdfError::InternalError");
        Ok(r)
    }

    async fn extract(&self, salt: &[u8], ikm: &[u8]) -> Result<Vec<u8>, KdfError> {
        if ikm.is_empty() {
            return Err(KdfError::TooShortKey(0, 1));
        }

        nss_gk_api::init();

        let alg = match self.0 {
            KdfId::HkdfSha256 => Ok(nss_gk_api::hkdf::HkdfAlgorithm::HKDF_SHA2_256),
            KdfId::HkdfSha384 => Ok(nss_gk_api::hkdf::HkdfAlgorithm::HKDF_SHA2_384),
            KdfId::HkdfSha512 => Ok(nss_gk_api::hkdf::HkdfAlgorithm::HKDF_SHA2_512),
            _ => Err(KdfError::UnsupportedCipherSuite),
        }?;

        let hkdf = nss_gk_api::hkdf::Hkdf::new(alg);
        let ikm_symkey = hkdf.import_secret(ikm).unwrap();

        // Extract
        let prk = hkdf.extract(salt, &ikm_symkey).unwrap();
        let prk_data = prk.key_data().unwrap();
        Ok(prk_data.to_vec())
    }

    fn extract_size(&self) -> usize {
        self.0.extract_size()
    }

    fn kdf_id(&self) -> u16 {
        self.0 as u16
    }
}

#[cfg(all(test, not(mls_build_async)))]
mod test {
    use assert_matches::assert_matches;
    use mls_rs_core::crypto::CipherSuite;
    use mls_rs_crypto_traits::KdfType;

    use crate::kdf::{Kdf, KdfError};

    use alloc::vec;

    #[test]
    fn no_key() {
        let kdf = Kdf::new(CipherSuite::CURVE25519_AES128).unwrap();
        assert!(kdf.extract(b"key", &[]).is_err());
    }

    #[test]
    fn no_salt() {
        let kdf = Kdf::new(CipherSuite::CURVE25519_AES128).unwrap();
        assert!(kdf.extract(&[], b"key").is_ok());
    }

    #[test]
    fn no_info() {
        let kdf = Kdf::new(CipherSuite::CURVE25519_AES128).unwrap();
        let key = vec![0u8; kdf.extract_size()];
        assert!(kdf.expand(&key, &[], 42).is_ok());
    }

    #[test]
    fn test_short_key() {
        let kdf = Kdf::new(CipherSuite::CURVE25519_AES128).unwrap();
        let key = vec![0u8; kdf.extract_size() - 1];

        assert_matches!(kdf.expand(&key, &[], 42), Err(KdfError::TooShortKey(_, _)));
    }
}
