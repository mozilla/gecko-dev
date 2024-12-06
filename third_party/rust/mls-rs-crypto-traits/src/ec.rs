// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use mls_rs_core::crypto::CipherSuite;

/// Elliptic curve types
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[repr(u8)]
#[non_exhaustive]
pub enum Curve {
    /// NIST Curve-P256
    P256,
    /// NIST Curve-P384
    P384,
    /// NIST Curve-P521
    P521,
    /// Elliptic-curve Diffie-Hellman key exchange Curve25519
    X25519,
    /// Edwards-curve Digital Signature Algorithm Curve25519
    Ed25519,
    /// Elliptic-curve Diffie-Hellman key exchange Curve448
    X448,
    /// Edwards-curve Digital Signature Algorithm Curve448
    Ed448,
}

impl Curve {
    /// Returns the amount of bytes of a secret key using this curve
    #[inline(always)]
    pub fn secret_key_size(&self) -> usize {
        match self {
            Curve::P256 => 32,
            Curve::P384 => 48,
            Curve::P521 => 66,
            Curve::X25519 => 32,
            Curve::Ed25519 => 64,
            Curve::X448 => 56,
            Curve::Ed448 => 114,
        }
    }

    #[inline(always)]
    pub fn public_key_size(&self) -> usize {
        match self {
            Curve::P256 | Curve::P384 | Curve::P521 => 2 * self.secret_key_size() + 1,
            Curve::X25519 | Curve::Ed25519 | Curve::X448 | Curve::Ed448 => self.secret_key_size(),
        }
    }

    pub fn from_ciphersuite(cipher_suite: CipherSuite, for_sig: bool) -> Option<Self> {
        match cipher_suite {
            CipherSuite::P256_AES128 => Some(Curve::P256),
            CipherSuite::P384_AES256 => Some(Curve::P384),
            CipherSuite::P521_AES256 => Some(Curve::P521),
            CipherSuite::CURVE25519_AES128 | CipherSuite::CURVE25519_CHACHA if for_sig => {
                Some(Curve::Ed25519)
            }
            CipherSuite::CURVE25519_AES128 | CipherSuite::CURVE25519_CHACHA => Some(Curve::X25519),
            CipherSuite::CURVE448_AES256 | CipherSuite::CURVE448_CHACHA if for_sig => {
                Some(Curve::Ed448)
            }
            CipherSuite::CURVE448_AES256 | CipherSuite::CURVE448_CHACHA => Some(Curve::X448),
            _ => None,
        }
    }

    #[inline(always)]
    pub fn curve_bitmask(&self) -> Option<u8> {
        match self {
            Curve::P256 | Curve::P384 => Some(0xFF),
            Curve::P521 => Some(0x01),
            _ => None,
        }
    }
}
