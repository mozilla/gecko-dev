// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use core::fmt::Debug;
use core::ops::Deref;

use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};

/// Wrapper type representing a proposal type identifier along with default
/// values defined by the MLS RFC.
#[derive(
    Clone, Copy, Eq, Hash, PartialOrd, Ord, PartialEq, MlsSize, MlsEncode, MlsDecode, Debug,
)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
// #[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::ffi_type)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[repr(transparent)]
pub struct ProposalType(u16);

impl ProposalType {
    pub const fn new(value: u16) -> ProposalType {
        ProposalType(value)
    }

    pub const fn raw_value(&self) -> u16 {
        self.0
    }
}

impl From<ProposalType> for u16 {
    fn from(value: ProposalType) -> Self {
        value.0
    }
}

impl From<u16> for ProposalType {
    fn from(value: u16) -> Self {
        ProposalType(value)
    }
}

impl Deref for ProposalType {
    type Target = u16;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl ProposalType {
    pub const ADD: ProposalType = ProposalType(1);
    pub const UPDATE: ProposalType = ProposalType(2);
    pub const REMOVE: ProposalType = ProposalType(3);
    pub const PSK: ProposalType = ProposalType(4);
    pub const RE_INIT: ProposalType = ProposalType(5);
    pub const EXTERNAL_INIT: ProposalType = ProposalType(6);
    pub const GROUP_CONTEXT_EXTENSIONS: ProposalType = ProposalType(7);

    /// Default proposal types defined
    /// in [RFC 9420](https://www.rfc-editor.org/rfc/rfc9420.html#name-leaf-node-contents)
    pub const DEFAULT: &'static [ProposalType] = &[
        ProposalType::ADD,
        ProposalType::UPDATE,
        ProposalType::REMOVE,
        ProposalType::PSK,
        ProposalType::RE_INIT,
        ProposalType::EXTERNAL_INIT,
        ProposalType::GROUP_CONTEXT_EXTENSIONS,
    ];
}
