// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use alloc::vec::Vec;
use core::{
    fmt::{self, Debug},
    ops::{Deref, DerefMut},
};
use zeroize::Zeroizing;

// #[cfg_attr(
//     all(feature = "ffi", not(test)),
//     safer_ffi_gen::ffi_type(clone, opaque)
// )]
#[derive(Clone, Eq, PartialEq)]
/// Wrapper struct that represents a zeroize-on-drop `Vec<u8>`
pub struct Secret(Zeroizing<Vec<u8>>);

impl Debug for Secret {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        crate::debug::pretty_bytes(&self.0).named("Secret").fmt(f)
    }
}

// #[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::safer_ffi_gen)]
impl Secret {
    pub fn as_bytes(&self) -> &[u8] {
        &self.0
    }
}

impl From<Vec<u8>> for Secret {
    fn from(bytes: Vec<u8>) -> Self {
        Zeroizing::new(bytes).into()
    }
}

impl From<Zeroizing<Vec<u8>>> for Secret {
    fn from(bytes: Zeroizing<Vec<u8>>) -> Self {
        Self(bytes)
    }
}

impl Deref for Secret {
    type Target = [u8];

    fn deref(&self) -> &[u8] {
        &self.0
    }
}

impl DerefMut for Secret {
    fn deref_mut(&mut self) -> &mut [u8] {
        &mut self.0
    }
}
