// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use mls_rs_crypto_traits::{DhType, KdfType, KemResult, KemType};

use mls_rs_core::{
    crypto::{HpkePublicKey, HpkeSecretKey},
    error::{AnyError, IntoAnyError},
};
use zeroize::Zeroizing;

use crate::kdf::HpkeKdf;

use alloc::vec::Vec;

#[derive(Debug)]
#[cfg_attr(feature = "std", derive(thiserror::Error))]
pub enum DhKemError {
    #[cfg_attr(feature = "std", error(transparent))]
    KdfError(AnyError),
    #[cfg_attr(feature = "std", error(transparent))]
    DhError(AnyError),
    /// NIST key derivation from bytes failure. This is statistically unlikely
    #[cfg_attr(
        feature = "std",
        error("Failed to derive nist keypair from raw bytes after 255 attempts")
    )]
    KeyDerivationError,
}

impl IntoAnyError for DhKemError {
    #[cfg(feature = "std")]
    fn into_dyn_error(self) -> Result<Box<dyn std::error::Error + Send + Sync>, Self> {
        Ok(self.into())
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct DhKem<DH: DhType, KDF: KdfType> {
    dh: DH,
    kdf: HpkeKdf<KDF>,
    kem_id: u16,
    n_secret: usize,
    #[cfg(feature = "test_utils")]
    test_key_data: Vec<u8>,
}

impl<DH: DhType, KDF: KdfType> DhKem<DH, KDF> {
    pub fn new(dh: DH, kdf: KDF, kem_id: u16, n_secret: usize) -> Self {
        let suite_id = [b"KEM", &kem_id.to_be_bytes() as &[u8]].concat();
        let kdf = HpkeKdf::new(suite_id, kdf);

        Self {
            dh,
            kdf,
            kem_id,
            n_secret,
            #[cfg(feature = "test_utils")]
            test_key_data: alloc::vec![],
        }
    }
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
#[cfg_attr(all(target_arch = "wasm32", mls_build_async), maybe_async::must_be_async(?Send))]
#[cfg_attr(
    all(not(target_arch = "wasm32"), mls_build_async),
    maybe_async::must_be_async
)]
impl<DH: DhType, KDF: KdfType> KemType for DhKem<DH, KDF> {
    type Error = DhKemError;

    fn kem_id(&self) -> u16 {
        self.kem_id
    }

    async fn derive(&self, ikm: &[u8]) -> Result<(HpkeSecretKey, HpkePublicKey), DhKemError> {
        let dkp_prk = self
            .kdf
            .labeled_extract(&[], b"dkp_prk", ikm)
            .await
            .map_err(|e| DhKemError::KdfError(e.into_any_error()))?;

        if let Some(bitmask) = self.dh.bitmask_for_rejection_sampling() {
            self.derive_with_rejection_sampling(&dkp_prk, bitmask).await
        } else {
            self.derive_without_rejection_sampling(&dkp_prk).await
        }
    }

    async fn generate(&self) -> Result<(HpkeSecretKey, HpkePublicKey), Self::Error> {
        #[cfg(feature = "test_utils")]
        if !self.test_key_data.is_empty() {
            return self.derive(&self.test_key_data).await;
        }

        self.dh
            .generate()
            .await
            .map_err(|e| DhKemError::DhError(e.into_any_error()))
    }

    async fn encap(&self, remote_pk: &HpkePublicKey) -> Result<KemResult, Self::Error> {
        let (ephemeral_sk, ephemeral_pk) = self.generate().await?;

        let ecdh_ss = self
            .dh
            .dh(&ephemeral_sk, remote_pk)
            .await
            .map(Zeroizing::new)
            .map_err(|e| DhKemError::DhError(e.into_any_error()))?;

        let kem_context = [ephemeral_pk.as_ref(), remote_pk.as_ref()].concat();

        let shared_secret = self
            .kdf
            .labeled_extract_then_expand(&ecdh_ss, &kem_context, self.n_secret)
            .await
            .map_err(|e| DhKemError::KdfError(e.into_any_error()))?;

        Ok(KemResult::new(shared_secret, ephemeral_pk.into()))
    }

    async fn decap(
        &self,
        enc: &[u8],
        secret_key: &HpkeSecretKey,
        public_key: &HpkePublicKey,
    ) -> Result<Vec<u8>, Self::Error> {
        let remote_pk = enc.to_vec().into();

        let ecdh_ss = self
            .dh
            .dh(secret_key, &remote_pk)
            .await
            .map(Zeroizing::new)
            .map_err(|e| DhKemError::DhError(e.into_any_error()))?;

        let kem_context = [enc, public_key].concat();

        self.kdf
            .labeled_extract_then_expand(&ecdh_ss, &kem_context, self.n_secret)
            .await
            .map_err(|e| DhKemError::KdfError(e.into_any_error()))
    }

    fn public_key_validate(&self, key: &HpkePublicKey) -> Result<(), Self::Error> {
        self.dh
            .public_key_validate(key)
            .map_err(|e| DhKemError::DhError(e.into_any_error()))
    }
}

impl<DH: DhType, KDF: KdfType> DhKem<DH, KDF> {
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn derive_with_rejection_sampling(
        &self,
        dkp_prk: &[u8],
        bitmask: u8,
    ) -> Result<(HpkeSecretKey, HpkePublicKey), DhKemError> {
        // The RFC specifies we get 255 chances to generate bytes that will be within range of the order for the curve
        for i in 0u8..255 {
            let mut secret_key = self
                .kdf
                .labeled_expand(dkp_prk, b"candidate", &[i], self.dh.secret_key_size())
                .await
                .map_err(|e| DhKemError::KdfError(e.into_any_error()))?;

            secret_key[0] &= bitmask;
            let secret_key = secret_key.into();

            // Compute the public key and if it succeeds, return the key pair
            if let Ok(pair) = self
                .dh
                .to_public(&secret_key)
                .await
                .map(|pk| (secret_key, pk))
            {
                return Ok(pair);
            }
        }

        // If we never generate bytes that work, throw an error
        Err(DhKemError::KeyDerivationError)
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn derive_without_rejection_sampling(
        &self,
        dkp_prk: &[u8],
    ) -> Result<(HpkeSecretKey, HpkePublicKey), DhKemError> {
        let sk = self
            .kdf
            .labeled_expand(dkp_prk, b"sk", &[], self.dh.secret_key_size())
            .await
            .map_err(|e| DhKemError::KdfError(e.into_any_error()))?
            .into();

        let pk = self
            .dh
            .to_public(&sk)
            .await
            .map_err(|e| DhKemError::DhError(e.into_any_error()))?;

        Ok((sk, pk))
    }

    #[cfg(feature = "test_utils")]
    pub fn set_test_data(&mut self, test_data: Vec<u8>) {
        self.test_key_data = test_data
    }
}
