// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#![cfg_attr(not(feature = "std"), no_std)]
extern crate alloc;

pub mod aead;
mod ec;
pub mod ec_signer;
pub mod ecdh;
pub mod kdf;
pub mod mac;

use crate::aead::Aead;
use ec_signer::{EcSigner, EcSignerError};
use ecdh::Ecdh;
use kdf::Kdf;
use mac::{Hash, HashError};
use mls_rs_crypto_hpke::{
    context::{ContextR, ContextS},
    dhkem::DhKem,
    hpke::{Hpke, HpkeError},
};
use mls_rs_crypto_traits::{AeadType, KdfType, KemId, KemType};
use rand_core::{OsRng, RngCore};

use mls_rs_core::{
    crypto::{
        CipherSuite, CipherSuiteProvider, CryptoProvider, HpkeCiphertext, HpkePublicKey,
        HpkeSecretKey, SignaturePublicKey, SignatureSecretKey,
    },
    error::{AnyError, IntoAnyError},
};
use zeroize::Zeroizing;

use alloc::vec;
use alloc::vec::Vec;

#[derive(Debug)]
#[cfg_attr(feature = "std", derive(thiserror::Error))]
pub enum NssCryptoError {
    #[cfg_attr(feature = "std", error(transparent))]
    AeadError(AnyError),
    #[cfg_attr(feature = "std", error(transparent))]
    HpkeError(HpkeError),
    #[cfg_attr(feature = "std", error(transparent))]
    KdfError(AnyError),
    #[cfg_attr(feature = "std", error(transparent))]
    HashError(HashError),
    #[cfg_attr(feature = "std", error("rand core error: {0:?}"))]
    RandError(rand_core::Error),
    #[cfg_attr(feature = "std", error(transparent))]
    EcSignerError(EcSignerError),
}

impl From<rand_core::Error> for NssCryptoError {
    fn from(value: rand_core::Error) -> Self {
        NssCryptoError::RandError(value)
    }
}

impl From<HpkeError> for NssCryptoError {
    fn from(e: HpkeError) -> Self {
        NssCryptoError::HpkeError(e)
    }
}

impl From<HashError> for NssCryptoError {
    fn from(e: HashError) -> Self {
        NssCryptoError::HashError(e)
    }
}

impl From<EcSignerError> for NssCryptoError {
    fn from(e: EcSignerError) -> Self {
        NssCryptoError::EcSignerError(e)
    }
}

impl IntoAnyError for NssCryptoError {
    #[cfg(feature = "std")]
    fn into_dyn_error(self) -> Result<Box<dyn std::error::Error + Send + Sync>, Self> {
        Ok(self.into())
    }
}

#[derive(Debug, Clone)]
#[non_exhaustive]
pub struct NssCryptoProvider {
    pub enabled_cipher_suites: Vec<CipherSuite>,
}

impl NssCryptoProvider {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn with_enabled_cipher_suites(enabled_cipher_suites: Vec<CipherSuite>) -> Self {
        Self {
            enabled_cipher_suites,
        }
    }

    pub fn all_supported_cipher_suites() -> Vec<CipherSuite> {
        vec![
            CipherSuite::P256_AES128,
            CipherSuite::CURVE25519_AES128,
            CipherSuite::CURVE25519_CHACHA,
        ]
    }
}

impl Default for NssCryptoProvider {
    fn default() -> Self {
        Self {
            enabled_cipher_suites: Self::all_supported_cipher_suites(),
        }
    }
}

impl CryptoProvider for NssCryptoProvider {
    type CipherSuiteProvider = NssCryptoCipherSuite<DhKem<Ecdh, Kdf>, Kdf, Aead>;

    fn supported_cipher_suites(&self) -> Vec<CipherSuite> {
        self.enabled_cipher_suites.clone()
    }

    fn cipher_suite_provider(
        &self,
        cipher_suite: CipherSuite,
    ) -> Option<Self::CipherSuiteProvider> {
        if !self.enabled_cipher_suites.contains(&cipher_suite) {
            return None;
        }

        let kdf = Kdf::new(cipher_suite)?;
        let ecdh = Ecdh::new(cipher_suite)?;
        let kem_id = KemId::new(cipher_suite)?;
        let kem = DhKem::new(ecdh, kdf, kem_id as u16, kem_id.n_secret());
        let aead = Aead::new(cipher_suite)?;

        NssCryptoCipherSuite::new(cipher_suite, kem, kdf, aead)
    }
}

#[derive(Clone)]
pub struct NssCryptoCipherSuite<KEM, KDF, AEAD>
where
    KEM: KemType + Clone,
    KDF: KdfType + Clone,
    AEAD: AeadType + Clone,
{
    cipher_suite: CipherSuite,
    aead: AEAD,
    kdf: KDF,
    hash: Hash,
    hpke: Hpke<KEM, KDF, AEAD>,
    ec_signer: EcSigner,
}

impl<KEM, KDF, AEAD> NssCryptoCipherSuite<KEM, KDF, AEAD>
where
    KEM: KemType + Clone,
    KDF: KdfType + Clone,
    AEAD: AeadType + Clone,
{
    pub fn new(cipher_suite: CipherSuite, kem: KEM, kdf: KDF, aead: AEAD) -> Option<Self> {
        let hpke = Hpke::new(kem, kdf.clone(), Some(aead.clone()));

        Some(Self {
            cipher_suite,
            kdf,
            aead,
            hash: Hash::new(cipher_suite).ok()?,
            hpke,
            ec_signer: EcSigner::new(cipher_suite)?,
        })
    }

    pub fn random_bytes(&self, out: &mut [u8]) -> Result<(), NssCryptoError> {
        OsRng.try_fill_bytes(out).map_err(Into::into)
    }
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
#[cfg_attr(all(target_arch = "wasm32", mls_build_async), maybe_async::must_be_async(?Send))]
#[cfg_attr(
    all(not(target_arch = "wasm32"), mls_build_async),
    maybe_async::must_be_async
)]
impl<KEM, KDF, AEAD> CipherSuiteProvider for NssCryptoCipherSuite<KEM, KDF, AEAD>
where
    KEM: KemType + Clone + Send + Sync,
    KDF: KdfType + Clone + Send + Sync,
    AEAD: AeadType + Clone + Send + Sync,
{
    type Error = NssCryptoError;
    // TODO exporter_secret in this struct is not zeroized
    type HpkeContextR = ContextR<KDF, AEAD>;
    type HpkeContextS = ContextS<KDF, AEAD>;

    async fn hash(&self, data: &[u8]) -> Result<Vec<u8>, Self::Error> {
        Ok(self.hash.hash(data))
    }

    async fn mac(&self, key: &[u8], data: &[u8]) -> Result<Vec<u8>, Self::Error> {
        Ok(self.hash.mac(key, data)?)
    }

    async fn aead_seal(
        &self,
        key: &[u8],
        data: &[u8],
        aad: Option<&[u8]>,
        nonce: &[u8],
    ) -> Result<Vec<u8>, Self::Error> {
        self.aead
            .seal(key, data, aad, nonce)
            .await
            .map_err(|e| NssCryptoError::AeadError(e.into_any_error()))
    }

    async fn aead_open(
        &self,
        key: &[u8],
        cipher_text: &[u8],
        aad: Option<&[u8]>,
        nonce: &[u8],
    ) -> Result<Zeroizing<Vec<u8>>, Self::Error> {
        self.aead
            .open(key, cipher_text, aad, nonce)
            .await
            .map_err(|e| NssCryptoError::AeadError(e.into_any_error()))
            .map(Zeroizing::new)
    }

    fn aead_key_size(&self) -> usize {
        self.aead.key_size()
    }

    fn aead_nonce_size(&self) -> usize {
        self.aead.nonce_size()
    }

    async fn kdf_expand(
        &self,
        prk: &[u8],
        info: &[u8],
        len: usize,
    ) -> Result<Zeroizing<Vec<u8>>, Self::Error> {
        self.kdf
            .expand(prk, info, len)
            .await
            .map_err(|e| NssCryptoError::KdfError(e.into_any_error()))
            .map(Zeroizing::new)
    }

    async fn kdf_extract(
        &self,
        salt: &[u8],
        ikm: &[u8],
    ) -> Result<Zeroizing<Vec<u8>>, Self::Error> {
        self.kdf
            .extract(salt, ikm)
            .await
            .map_err(|e| NssCryptoError::KdfError(e.into_any_error()))
            .map(Zeroizing::new)
    }

    fn kdf_extract_size(&self) -> usize {
        self.kdf.extract_size()
    }

    async fn hpke_seal(
        &self,
        remote_key: &HpkePublicKey,
        info: &[u8],
        aad: Option<&[u8]>,
        pt: &[u8],
    ) -> Result<HpkeCiphertext, Self::Error> {
        Ok(self.hpke.seal(remote_key, info, None, aad, pt).await?)
    }

    async fn hpke_open(
        &self,
        ciphertext: &HpkeCiphertext,
        local_secret: &HpkeSecretKey,
        local_public: &HpkePublicKey,
        info: &[u8],
        aad: Option<&[u8]>,
    ) -> Result<Vec<u8>, Self::Error> {
        Ok(self
            .hpke
            .open(ciphertext, local_secret, local_public, info, None, aad)
            .await?)
    }

    async fn hpke_setup_r(
        &self,
        enc: &[u8],
        local_secret: &HpkeSecretKey,
        local_public: &HpkePublicKey,
        info: &[u8],
    ) -> Result<Self::HpkeContextR, Self::Error> {
        Ok(self
            .hpke
            .setup_receiver(enc, local_secret, local_public, info, None)
            .await?)
    }

    async fn hpke_setup_s(
        &self,
        remote_key: &HpkePublicKey,
        info: &[u8],
    ) -> Result<(Vec<u8>, Self::HpkeContextS), Self::Error> {
        Ok(self.hpke.setup_sender(remote_key, info, None).await?)
    }

    async fn kem_derive(&self, ikm: &[u8]) -> Result<(HpkeSecretKey, HpkePublicKey), Self::Error> {
        Ok(self.hpke.derive(ikm).await?)
    }

    async fn kem_generate(&self) -> Result<(HpkeSecretKey, HpkePublicKey), Self::Error> {
        Ok(self.hpke.generate().await?)
    }

    fn kem_public_key_validate(&self, key: &HpkePublicKey) -> Result<(), Self::Error> {
        Ok(self.hpke.public_key_validate(key)?)
    }

    fn random_bytes(&self, out: &mut [u8]) -> Result<(), Self::Error> {
        self.random_bytes(out)
    }

    fn cipher_suite(&self) -> CipherSuite {
        self.cipher_suite
    }

    async fn sign(
        &self,
        secret_key: &SignatureSecretKey,
        data: &[u8],
    ) -> Result<Vec<u8>, Self::Error> {
        Ok(self.ec_signer.sign(secret_key, data)?)
    }

    async fn verify(
        &self,
        public_key: &SignaturePublicKey,
        signature: &[u8],
        data: &[u8],
    ) -> Result<(), Self::Error> {
        Ok(self.ec_signer.verify(public_key, signature, data)?)
    }

    async fn signature_key_generate(
        &self,
    ) -> Result<(SignatureSecretKey, SignaturePublicKey), Self::Error> {
        Ok(self.ec_signer.signature_key_generate()?)
    }

    async fn signature_key_derive_public(
        &self,
        secret_key: &SignatureSecretKey,
    ) -> Result<SignaturePublicKey, Self::Error> {
        Ok(self.ec_signer.signature_key_derive_public(secret_key)?)
    }
}

#[cfg(not(mls_build_async))]
#[test]
fn mls_core_tests() {
    let provider = NssCryptoProvider::new();
    mls_rs_core::crypto::test_suite::verify_tests(&provider, true);

    for cs in NssCryptoProvider::all_supported_cipher_suites() {
        let mut hpke = provider.cipher_suite_provider(cs).unwrap().hpke;

        mls_rs_core::crypto::test_suite::verify_hpke_context_tests(&hpke, cs);
        mls_rs_core::crypto::test_suite::verify_hpke_encap_tests(&mut hpke, cs);
    }
}
