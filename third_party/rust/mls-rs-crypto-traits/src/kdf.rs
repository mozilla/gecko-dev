// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#[cfg(feature = "mock")]
use mockall::automock;

use alloc::vec::Vec;
use mls_rs_core::{crypto::CipherSuite, error::IntoAnyError};

/// A trait that provides the required KDF functions
#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
#[cfg_attr(all(target_arch = "wasm32", mls_build_async), maybe_async::must_be_async(?Send))]
#[cfg_attr(
    all(not(target_arch = "wasm32"), mls_build_async),
    maybe_async::must_be_async
)]
#[cfg_attr(feature = "mock", automock(type Error = crate::mock::TestError;))]
pub trait KdfType: Send + Sync {
    type Error: IntoAnyError + Send + Sync;

    /// KDF Id, as specified in RFC 9180, Section 5.1 and Table 3.
    fn kdf_id(&self) -> u16;

    async fn expand(&self, prk: &[u8], info: &[u8], len: usize) -> Result<Vec<u8>, Self::Error>;
    async fn extract(&self, salt: &[u8], ikm: &[u8]) -> Result<Vec<u8>, Self::Error>;
    fn extract_size(&self) -> usize;
}

/// Aead KDF as specified in RFC 9180, Table 3.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[repr(u16)]
#[non_exhaustive]
pub enum KdfId {
    HkdfSha256 = 0x0001,
    HkdfSha384 = 0x0002,
    HkdfSha512 = 0x0003,
}

impl KdfId {
    pub fn new(cipher_suite: CipherSuite) -> Option<Self> {
        match cipher_suite {
            CipherSuite::CURVE25519_AES128
            | CipherSuite::P256_AES128
            | CipherSuite::CURVE25519_CHACHA => Some(KdfId::HkdfSha256),
            CipherSuite::P384_AES256 => Some(KdfId::HkdfSha384),
            CipherSuite::CURVE448_CHACHA
            | CipherSuite::CURVE448_AES256
            | CipherSuite::P521_AES256 => Some(KdfId::HkdfSha512),
            _ => None,
        }
    }

    pub fn extract_size(&self) -> usize {
        match self {
            KdfId::HkdfSha256 => 32,
            KdfId::HkdfSha384 => 48,
            KdfId::HkdfSha512 => 64,
        }
    }
}
