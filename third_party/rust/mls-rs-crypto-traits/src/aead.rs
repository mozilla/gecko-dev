// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#[cfg(feature = "mock")]
use mockall::automock;

use alloc::vec::Vec;
use mls_rs_core::{crypto::CipherSuite, error::IntoAnyError};

pub const AEAD_ID_EXPORT_ONLY: u16 = 0xFFFF;
pub const AES_TAG_LEN: usize = 16;

/// A trait that provides the required AEAD functions
#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
#[cfg_attr(all(target_arch = "wasm32", mls_build_async), maybe_async::must_be_async(?Send))]
#[cfg_attr(
    all(not(target_arch = "wasm32"), mls_build_async),
    maybe_async::must_be_async
)]
#[cfg_attr(feature = "mock", automock(type Error = crate::mock::TestError;))]
pub trait AeadType: Send + Sync {
    type Error: IntoAnyError;

    fn aead_id(&self) -> u16;

    #[allow(clippy::needless_lifetimes)]
    async fn seal<'a>(
        &self,
        key: &[u8],
        data: &[u8],
        aad: Option<&'a [u8]>,
        nonce: &[u8],
    ) -> Result<Vec<u8>, Self::Error>;

    #[allow(clippy::needless_lifetimes)]
    async fn open<'a>(
        &self,
        key: &[u8],
        ciphertext: &[u8],
        aad: Option<&'a [u8]>,
        nonce: &[u8],
    ) -> Result<Vec<u8>, Self::Error>;

    fn key_size(&self) -> usize;
    fn nonce_size(&self) -> usize;
}

/// AEAD Id, as specified in RFC 9180, Section 5.1 and Table 5.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[repr(u16)]
#[non_exhaustive]
pub enum AeadId {
    /// AES-128-GCM: 16 byte key, 12 byte nonce, 16 byte tag
    Aes128Gcm = 0x0001,
    /// AES-256-GCM: 32 byte key, 12 byte nonce, 16 byte tag
    Aes256Gcm = 0x0002,
    /// ChaCha20-Poly1305: 32 byte key, 12 byte nonce, 16 byte tag
    Chacha20Poly1305 = 0x0003,
}

impl AeadId {
    pub fn new(cipher_suite: CipherSuite) -> Option<Self> {
        match cipher_suite {
            CipherSuite::P256_AES128 | CipherSuite::CURVE25519_AES128 => Some(AeadId::Aes128Gcm),
            CipherSuite::CURVE448_AES256 | CipherSuite::P384_AES256 | CipherSuite::P521_AES256 => {
                Some(AeadId::Aes256Gcm)
            }
            CipherSuite::CURVE25519_CHACHA | CipherSuite::CURVE448_CHACHA => {
                Some(AeadId::Chacha20Poly1305)
            }
            _ => None,
        }
    }

    pub fn key_size(&self) -> usize {
        match self {
            AeadId::Aes128Gcm => 16,
            AeadId::Aes256Gcm => 32,
            AeadId::Chacha20Poly1305 => 32,
        }
    }

    pub fn nonce_size(&self) -> usize {
        12
    }
}
