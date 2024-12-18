// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::ec::{
    generate_keypair, private_key_bytes_to_public, private_key_from_bytes,
    pub_key_from_uncompressed, sign_ed25519, sign_p256, verify_ed25519, verify_p256, EcError,
    EcPrivateKey, EcPublicKey,
};
use alloc::vec::Vec;
use core::ops::Deref;
use mls_rs_core::crypto::{CipherSuite, SignaturePublicKey, SignatureSecretKey};
use mls_rs_crypto_traits::Curve;

#[derive(Debug)]
#[cfg_attr(feature = "std", derive(thiserror::Error))]
pub enum EcSignerError {
    #[cfg_attr(feature = "std", error("ec key is not a signature key"))]
    EcKeyNotSignature,
    #[cfg_attr(feature = "std", error(transparent))]
    EcError(EcError),
    #[cfg_attr(feature = "std", error("invalid signature"))]
    InvalidSignature,
}

impl From<EcError> for EcSignerError {
    fn from(e: EcError) -> Self {
        EcSignerError::EcError(e)
    }
}

#[derive(Clone, Debug, Copy, PartialEq, Eq)]
pub struct EcSigner(Curve);

impl Deref for EcSigner {
    type Target = Curve;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl EcSigner {
    pub fn new(cipher_suite: CipherSuite) -> Option<Self> {
        Curve::from_ciphersuite(cipher_suite, true).map(Self)
    }

    pub fn new_from_curve(curve: Curve) -> Self {
        Self(curve)
    }

    pub fn signature_key_generate(
        &self,
    ) -> Result<(SignatureSecretKey, SignaturePublicKey), EcSignerError> {
        let key_pair = generate_keypair(self.0)?;
        Ok((key_pair.secret.into(), key_pair.public.into()))
    }

    pub fn signature_key_derive_public(
        &self,
        secret_key: &SignatureSecretKey,
    ) -> Result<SignaturePublicKey, EcSignerError> {
        Ok(private_key_bytes_to_public(secret_key.to_vec(), self.0)?.into())
    }

    pub fn sign(
        &self,
        secret_key: &SignatureSecretKey,
        data: &[u8],
    ) -> Result<Vec<u8>, EcSignerError> {
        let secret_key = private_key_from_bytes(secret_key.to_vec(), self.0)?;

        match secret_key {
            EcPrivateKey::X25519(_) => Err(EcSignerError::EcKeyNotSignature),
            EcPrivateKey::Ed25519(private_key) => Ok(sign_ed25519(private_key, data)?),
            EcPrivateKey::P256(private_key) => Ok(sign_p256(private_key, data)?),
        }
    }

    pub fn verify(
        &self,
        public_key: &SignaturePublicKey,
        signature: &[u8],
        data: &[u8],
    ) -> Result<(), EcSignerError> {
        let public_key = pub_key_from_uncompressed(public_key.to_vec(), self.0)?;

        let ver = match public_key {
            EcPublicKey::X25519(_) => Err(EcSignerError::EcKeyNotSignature),
            EcPublicKey::Ed25519(key) => Ok(verify_ed25519(key, signature, data)?),
            EcPublicKey::P256(key) => Ok(verify_p256(key, signature, data)?),
        }?;

        ver.then_some(()).ok_or(EcSignerError::InvalidSignature)
    }
}
