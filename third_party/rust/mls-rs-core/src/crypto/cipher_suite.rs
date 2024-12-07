// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use core::ops::Deref;

use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};

/// Wrapper type representing a ciphersuite identifier
/// along with default values defined by the MLS RFC. Custom ciphersuites
/// can be defined using a custom [`CryptoProvider`](crate::crypto::CryptoProvider).
///
/// ## Default Ciphersuites
///
/// Note: KEM values are defined by the HPKE standard (RFC 9180).
///
/// |    |             |         |         |                  |
/// |----|-------------|---------|---------|------------------|
/// | ID | KEM         | AEAD  | Hash Function    | Signature Scheme |
/// | 1  | DHKEMX25519 | AES 128 | SHA 256 | Ed25519          |
/// | 2  | DHKEMP256   | AES 128 | SHA 256 | P256             |
/// | 3  | DHKEMX25519 | ChaCha20Poly1305 | SHA 256 | Ed25519 |
/// | 4  | DHKEMX448   | AES 256 | SHA 512 | Ed448            |
/// | 5  | DHKEMP521   | AES 256 | SHA 512 | P521             |
/// | 6  | DHKEMX448   | ChaCha20Poly1305 | SHA 512 | Ed448   |
/// | 7  | DHKEMP384   | AES 256 | SHA 512 | P384             |
#[derive(Debug, Copy, Clone, Eq, PartialEq, MlsSize, MlsEncode, MlsDecode, PartialOrd, Ord)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
// #[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::ffi_type)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[repr(transparent)]
pub struct CipherSuite(u16);

impl From<u16> for CipherSuite {
    fn from(value: u16) -> Self {
        CipherSuite(value)
    }
}

impl From<CipherSuite> for u16 {
    fn from(val: CipherSuite) -> Self {
        val.0
    }
}

impl Deref for CipherSuite {
    type Target = u16;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl CipherSuite {
    /// MLS_128_DHKEMX25519_AES128GCM_SHA256_Ed25519
    pub const CURVE25519_AES128: CipherSuite = CipherSuite(1);
    /// MLS_128_DHKEMP256_AES128GCM_SHA256_P256
    pub const P256_AES128: CipherSuite = CipherSuite(2);
    /// MLS_128_DHKEMX25519_CHACHA20POLY1305_SHA256_Ed25519
    pub const CURVE25519_CHACHA: CipherSuite = CipherSuite(3);
    /// MLS_256_DHKEMX448_AES256GCM_SHA512_Ed448
    pub const CURVE448_AES256: CipherSuite = CipherSuite(4);
    /// MLS_256_DHKEMP521_AES256GCM_SHA512_P521
    pub const P521_AES256: CipherSuite = CipherSuite(5);
    /// MLS_256_DHKEMX448_CHACHA20POLY1305_SHA512_Ed448
    pub const CURVE448_CHACHA: CipherSuite = CipherSuite(6);
    /// MLS_256_DHKEMP384_AES256GCM_SHA384_P384
    pub const P384_AES256: CipherSuite = CipherSuite(7);

    /// Ciphersuite from a raw value.
    pub const fn new(value: u16) -> CipherSuite {
        CipherSuite(value)
    }

    /// Raw numerical value wrapped value.
    pub const fn raw_value(&self) -> u16 {
        self.0
    }

    /// An iterator over all of the default MLS ciphersuites.
    pub fn all() -> impl Iterator<Item = CipherSuite> {
        (1..=7).map(CipherSuite)
    }
}

/// Modes of HPKE operation.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[repr(u8)]
pub enum HpkeModeId {
    /// Base mode of HPKE for key exchange and AEAD cipher
    Base = 0x00,
    /// Base mode with a user provided PSK
    Psk = 0x01,
    /// Authenticated variant that authenticates possession of a KEM private key.
    Auth = 0x02,
    /// Authenticated variant that authenticates possession of a PSK as well as a KEM private key.
    AuthPsk = 0x03,
}
