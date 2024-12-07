// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use mls_rs_core::{
    crypto::{HpkeContextR, HpkeContextS},
    error::IntoAnyError,
};
use mls_rs_crypto_traits::{AeadType, KdfType};

use crate::{hpke::HpkeError, kdf::HpkeKdf};

use alloc::vec::Vec;
use core::fmt::{self, Debug};

/// A type representing an HPKE context
#[derive(Clone)]
pub(super) struct Context<KDF: KdfType, AEAD: AeadType> {
    exporter_secret: Vec<u8>,
    encryption_context: Option<EncryptionContext<AEAD>>,
    kdf: HpkeKdf<KDF>,
}

impl<KDF: KdfType + Debug, AEAD: AeadType + Debug> Debug for Context<KDF, AEAD> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Context")
            .field(
                "exporter_secret",
                &mls_rs_core::debug::pretty_bytes(&self.exporter_secret),
            )
            .field("encryption_context", &self.encryption_context)
            .field("kdf", &self.kdf)
            .finish()
    }
}

impl<KDF: KdfType, AEAD: AeadType> Context<KDF, AEAD> {
    #[inline]
    pub(super) fn new(
        encryption_context: Option<EncryptionContext<AEAD>>,
        exporter_secret: Vec<u8>,
        kdf: HpkeKdf<KDF>,
    ) -> Self {
        Self {
            exporter_secret,
            encryption_context,
            kdf,
        }
    }
}

impl<KDF: KdfType, AEAD: AeadType> Context<KDF, AEAD> {
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn seal(&mut self, aad: Option<&[u8]>, data: &[u8]) -> Result<Vec<u8>, HpkeError> {
        self.encryption_context
            .as_mut()
            .ok_or(HpkeError::ExportOnlyMode)?
            .seal(aad, data)
            .await
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn open(&mut self, aad: Option<&[u8]>, ciphertext: &[u8]) -> Result<Vec<u8>, HpkeError> {
        self.encryption_context
            .as_mut()
            .ok_or(HpkeError::ExportOnlyMode)?
            .open(aad, ciphertext)
            .await
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn export(&self, exporter_context: &[u8], len: usize) -> Result<Vec<u8>, HpkeError> {
        self.kdf
            .labeled_expand(&self.exporter_secret, b"sec", exporter_context, len)
            .await
            .map_err(|e| HpkeError::KdfError(e.into_any_error()))
    }
}

#[derive(Debug, Clone)]
pub struct ContextS<KDF: KdfType, AEAD: AeadType>(pub(super) Context<KDF, AEAD>);

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
#[cfg_attr(all(target_arch = "wasm32", mls_build_async), maybe_async::must_be_async(?Send))]
#[cfg_attr(
    all(not(target_arch = "wasm32"), mls_build_async),
    maybe_async::must_be_async
)]
impl<KDF: KdfType, AEAD: AeadType> HpkeContextS for ContextS<KDF, AEAD> {
    type Error = HpkeError;

    async fn export(&self, exporter_context: &[u8], len: usize) -> Result<Vec<u8>, Self::Error> {
        self.0.export(exporter_context, len).await
    }

    /// # Errors
    ///
    /// Returns [SequenceNumberOverflow](HpkeError::SequenceNumberOverflow)
    /// in the event that the sequence number overflows. The sequence number is a u64 and starts
    /// at 0.
    async fn seal(&mut self, aad: Option<&[u8]>, data: &[u8]) -> Result<Vec<u8>, Self::Error> {
        self.0.seal(aad, data).await
    }
}

#[derive(Debug, Clone)]
pub struct ContextR<KDF: KdfType, AEAD: AeadType>(pub(super) Context<KDF, AEAD>);

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
#[cfg_attr(all(target_arch = "wasm32", mls_build_async), maybe_async::must_be_async(?Send))]
#[cfg_attr(
    all(not(target_arch = "wasm32"), mls_build_async),
    maybe_async::must_be_async
)]
impl<KDF: KdfType, AEAD: AeadType> HpkeContextR for ContextR<KDF, AEAD> {
    type Error = HpkeError;

    async fn export(&self, exporter_context: &[u8], len: usize) -> Result<Vec<u8>, Self::Error> {
        self.0.export(exporter_context, len).await
    }

    /// # Errors
    ///
    /// Returns [SequenceNumberOverflow](HpkeError::SequenceNumberOverflow)
    /// in the event that the sequence number overflows. The sequence number is a u64 and starts
    /// at 0.
    ///
    /// Returns [AeadError](HpkeError::AeadError) if decryption fails due to either an invalid
    /// `aad` value, or incorrect cipher key.
    async fn open(
        &mut self,
        aad: Option<&[u8]>,
        ciphertext: &[u8],
    ) -> Result<Vec<u8>, Self::Error> {
        self.0.open(aad, ciphertext).await
    }
}

#[derive(PartialEq, Eq, Clone)]
pub(super) struct EncryptionContext<AEAD: AeadType> {
    base_nonce: Vec<u8>,
    seq_number: u64,
    aead: AEAD,
    aead_key: Vec<u8>,
}

impl<AEAD: AeadType + Debug> Debug for EncryptionContext<AEAD> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("EncryptionContext")
            .field(
                "base_nonce",
                &mls_rs_core::debug::pretty_bytes(&self.base_nonce),
            )
            .field("seq_number", &self.seq_number)
            .field("aead", &self.aead)
            .field(
                "aead_key",
                &mls_rs_core::debug::pretty_bytes(&self.aead_key),
            )
            .finish()
    }
}

impl<AEAD: AeadType> EncryptionContext<AEAD> {
    pub fn new(base_nonce: Vec<u8>, aead: AEAD, aead_key: Vec<u8>) -> Result<Self, HpkeError> {
        (base_nonce.len() == aead.nonce_size())
            .then_some(())
            .ok_or(HpkeError::IncorrectNonceLen(
                base_nonce.len(),
                aead.nonce_size(),
            ))?;

        (aead_key.len() == aead.key_size())
            .then_some(())
            .ok_or(HpkeError::IncorrectKeyLen(aead_key.len(), aead.key_size()))?;

        Ok(EncryptionContext {
            base_nonce,
            seq_number: 0,
            aead,
            aead_key,
        })
    }
}

impl<AEAD: AeadType> EncryptionContext<AEAD> {
    //draft-irtf-cfrg-hpke Section 5.2.  Encryption and Decryption
    fn compute_nonce(&self) -> Vec<u8> {
        let mut nonce = self.base_nonce.clone();

        // XOR the sequence number into the last 4 bytes of the nonce
        nonce
            .iter_mut()
            .rev()
            .zip(self.seq_number.to_le_bytes())
            .for_each(|(n, s)| *n ^= s);

        nonce
    }

    #[inline]
    fn increment_seq(&mut self) -> Result<(), HpkeError> {
        // If the sequence number is going to roll over just throw an error
        self.seq_number = self
            .seq_number
            .checked_add(1)
            .ok_or(HpkeError::SequenceNumberOverflow)?;

        Ok(())
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn seal(&mut self, aad: Option<&[u8]>, pt: &[u8]) -> Result<Vec<u8>, HpkeError> {
        let ct = self
            .aead
            .seal(&self.aead_key, pt, aad, &self.compute_nonce())
            .await
            .map_err(|e| HpkeError::AeadError(e.into_any_error()))?;

        self.increment_seq()?;

        Ok(ct)
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn open(&mut self, aad: Option<&[u8]>, ct: &[u8]) -> Result<Vec<u8>, HpkeError> {
        let pt = self
            .aead
            .open(&self.aead_key, ct, aad, &self.compute_nonce())
            .await
            .map_err(|e| HpkeError::AeadError(e.into_any_error()))?;

        self.increment_seq()?;

        Ok(pt)
    }
}
