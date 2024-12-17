// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#![cfg_attr(not(feature = "std"), no_std)]
#![cfg_attr(coverage_nightly, feature(coverage_attribute))]
extern crate alloc;

#[cfg(all(test, target_arch = "wasm32"))]
wasm_bindgen_test::wasm_bindgen_test_configure!(run_in_browser);

pub mod crypto;
pub mod debug;
pub mod error;
pub mod extension;
pub mod group;
pub mod identity;
pub mod key_package;
pub mod protocol_version;
pub mod psk;
pub mod secret;
pub mod time;

pub use mls_rs_codec;

#[cfg(feature = "arbitrary")]
pub use arbitrary;

#[cfg(feature = "serde")]
pub mod zeroizing_serde {
    use alloc::vec::Vec;
    use serde::{Deserializer, Serializer};
    use zeroize::Zeroizing;

    use crate::vec_serde;

    pub fn serialize<S: Serializer>(v: &Zeroizing<Vec<u8>>, s: S) -> Result<S::Ok, S::Error> {
        vec_serde::serialize(v, s)
    }

    pub fn deserialize<'de, D: Deserializer<'de>>(d: D) -> Result<Zeroizing<Vec<u8>>, D::Error> {
        vec_serde::deserialize(d).map(Zeroizing::new)
    }
}

#[cfg(feature = "serde")]
pub mod vec_serde {
    use alloc::vec::Vec;
    use serde::{Deserializer, Serializer};

    pub fn serialize<S: Serializer>(v: &Vec<u8>, s: S) -> Result<S::Ok, S::Error> {
        if s.is_human_readable() {
            hex::serde::serialize(v, s)
        } else {
            serde_bytes::serialize(v, s)
        }
    }

    pub fn deserialize<'de, D: Deserializer<'de>>(d: D) -> Result<Vec<u8>, D::Error> {
        if d.is_human_readable() {
            hex::serde::deserialize(d)
        } else {
            serde_bytes::deserialize(d)
        }
    }
}
