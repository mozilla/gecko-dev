// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use core::ops::Deref;

use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};

/// Wrapper type representing a protocol version identifier.
#[derive(
    Debug, Clone, Copy, Eq, Hash, Ord, PartialEq, PartialOrd, MlsSize, MlsEncode, MlsDecode,
)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
// #[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::ffi_type)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[repr(transparent)]
pub struct ProtocolVersion(u16);

impl From<u16> for ProtocolVersion {
    fn from(value: u16) -> Self {
        ProtocolVersion(value)
    }
}

impl From<ProtocolVersion> for u16 {
    fn from(value: ProtocolVersion) -> Self {
        value.0
    }
}

impl Deref for ProtocolVersion {
    type Target = u16;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl ProtocolVersion {
    /// MLS version 1.0
    pub const MLS_10: ProtocolVersion = ProtocolVersion(1);

    /// Protocol version from a raw value, useful for testing.
    pub const fn new(value: u16) -> ProtocolVersion {
        ProtocolVersion(value)
    }

    /// Raw numerical wrapped value.
    pub const fn raw_value(&self) -> u16 {
        self.0
    }

    /// An iterator over all of the defined MLS versions.
    pub fn all() -> impl Iterator<Item = ProtocolVersion> {
        [Self::MLS_10].into_iter()
    }
}
