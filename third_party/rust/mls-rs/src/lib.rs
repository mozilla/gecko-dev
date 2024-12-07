// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

//! An implementation of the [IETF Messaging Layer Security](https://messaginglayersecurity.rocks)
//! end-to-end encryption (E2EE) protocol.
//!
//! ## What is MLS?
//!
//! MLS is a new IETF end-to-end encryption standard that is designed to
//! provide transport agnostic, asynchronous, and highly performant
//! communication between a group of clients.
//!
//! ## MLS Protocol Features
//!
//! - Multi-party E2EE [group evolution](https://www.rfc-editor.org/rfc/rfc9420.html#name-cryptographic-state-and-evo)
//! via a propose-then-commit mechanism.
//! - Asynchronous by design with pre-computed [key packages](https://www.rfc-editor.org/rfc/rfc9420.html#name-key-packages),
//!   allowing members to be added to a group while offline.
//! - Customizable credential system with built in support for X.509 certificates.
//! - [Extension system](https://www.rfc-editor.org/rfc/rfc9420.html#name-extensions)
//!   allowing for application specific data to be negotiated via the protocol.
//! - Strong forward secrecy and post compromise security.
//! - Crypto agility via support for multiple [cipher suites](https://www.rfc-editor.org/rfc/rfc9420.html#name-cipher-suites).
//! - Pre-shared key support.
//! - Subgroup branching.
//! - Group reinitialization for breaking changes such as protocol upgrades.
//!
//! ## Features
//!
//! - Easy to use client interface that can manage multiple MLS identities and groups.
//! - 100% RFC 9420 conformance with support for all default credential, proposal,
//!   and extension types.
//! - Support for WASM builds.
//! - Configurable storage for key packages, secrets and group state
//!   via traits along with provided "in memory" and SQLite implementations.
//! - Support for custom user proposal and extension types.
//! - Ability to create user defined credentials with custom validation
//!   routines that can bridge to existing credential schemes.
//! - OpenSSL and Rust Crypto based cipher suite implementations.
//! - Crypto agility with support for user defined cipher suite.
//! - Extensive test suite including security and interop focused tests against
//!   pre-computed test vectors.
//!
//! ## Crypto Providers
//!
//! For cipher suite descriptions see the RFC documentation [here](https://www.rfc-editor.org/rfc/rfc9420.html#name-mls-cipher-suites)
//!
//! | Name | Cipher Suites | X509 Support |
//! |------|---------------|--------------|
//! | OpenSSL | 1-7 | Stable |
//! | AWS-LC | 1,2,3,5,7 | Stable |
//! | Rust Crypto | 1,2,3 | ⚠️ Experimental |
//!
//! ## Security Notice
//!
//! This library has been validated for conformance to the RFC 9420 specification but has not yet received a full security audit by a 3rd party.

#![allow(clippy::enum_variant_names)]
#![allow(clippy::result_large_err)]
#![allow(clippy::nonstandard_macro_braces)]
#![cfg_attr(not(feature = "std"), no_std)]
#![cfg_attr(docsrs, feature(doc_cfg))]
#![cfg_attr(coverage_nightly, feature(coverage_attribute))]
extern crate alloc;

#[cfg(all(test, target_arch = "wasm32"))]
wasm_bindgen_test::wasm_bindgen_test_configure!(run_in_browser);

#[cfg(all(test, target_arch = "wasm32"))]
use wasm_bindgen_test::wasm_bindgen_test as futures_test;

#[cfg(all(test, mls_build_async, not(target_arch = "wasm32")))]
use futures_test::test as futures_test;

#[cfg(test)]
macro_rules! hex {
    ($input:literal) => {
        hex::decode($input).expect("invalid hex value")
    };
}

#[cfg(test)]
macro_rules! load_test_case_json {
    ($name:ident, $generate:expr) => {
        load_test_case_json!($name, $generate, to_vec_pretty)
    };
    ($name:ident, $generate:expr, $to_json:ident) => {{
        #[cfg(any(target_arch = "wasm32", not(feature = "std")))]
        {
            // Do not remove `async`! (The goal of this line is to remove warnings
            // about `$generate` not being used. Actually calling it will make tests fail.)
            let _ = async { $generate };
            serde_json::from_slice(include_bytes!(concat!(
                env!("CARGO_MANIFEST_DIR"),
                "/test_data/",
                stringify!($name),
                ".json"
            )))
            .unwrap()
        }

        #[cfg(all(not(target_arch = "wasm32"), feature = "std"))]
        {
            let path = concat!(
                env!("CARGO_MANIFEST_DIR"),
                "/test_data/",
                stringify!($name),
                ".json"
            );
            if !std::path::Path::new(path).exists() {
                std::fs::write(path, serde_json::$to_json(&$generate).unwrap()).unwrap();
            }
            serde_json::from_slice(&std::fs::read(path).unwrap()).unwrap()
        }
    }};
}

mod cipher_suite {
    pub use mls_rs_core::crypto::CipherSuite;
}

pub use cipher_suite::CipherSuite;

mod protocol_version {
    pub use mls_rs_core::protocol_version::ProtocolVersion;
}

pub use protocol_version::ProtocolVersion;

pub mod client;
pub mod client_builder;
mod client_config;
/// Dependencies of [`CryptoProvider`] and [`CipherSuiteProvider`]
pub mod crypto;
/// Extension utilities and built-in extension types.
pub mod extension;
/// Tools to observe groups without being a member, useful
/// for server implementations.
#[cfg(feature = "external_client")]
#[cfg_attr(docsrs, doc(cfg(feature = "external_client")))]
pub mod external_client;
mod grease;
/// E2EE group created by a [`Client`].
pub mod group;
mod hash_reference;
/// Identity providers to use with [`ClientBuilder`](client_builder::ClientBuilder).
pub mod identity;
mod iter;
mod key_package;
/// Pre-shared key support.
pub mod psk;
mod signer;
/// Storage providers to use with
/// [`ClientBuilder`](client_builder::ClientBuilder).
pub mod storage_provider;

pub use mls_rs_core::{
    crypto::{CipherSuiteProvider, CryptoProvider},
    group::GroupStateStorage,
    identity::IdentityProvider,
    key_package::KeyPackageStorage,
    psk::PreSharedKeyStorage,
};

/// Dependencies of [`MlsRules`].
pub mod mls_rules {
    pub use crate::group::{
        mls_rules::{
            CommitDirection, CommitOptions, CommitSource, DefaultMlsRules, EncryptionOptions,
        },
        proposal_filter::{ProposalBundle, ProposalInfo, ProposalSource},
    };

    #[cfg(feature = "by_ref_proposal")]
    pub use crate::group::proposal_ref::ProposalRef;
}

pub use mls_rs_core::extension::{Extension, ExtensionList};

pub use crate::{
    client::Client,
    group::{
        framing::{MlsMessage, WireFormat},
        mls_rules::MlsRules,
        Group,
    },
    key_package::{KeyPackage, KeyPackageRef},
};

/// Error types.
pub mod error {
    pub use crate::client::MlsError;
    pub use mls_rs_core::error::{AnyError, IntoAnyError};
    pub use mls_rs_core::extension::ExtensionError;
}

/// WASM compatible timestamp.
pub mod time {
    pub use mls_rs_core::time::*;
}

mod tree_kem;

pub use mls_rs_codec;

mod private {
    pub trait Sealed {}
}

use private::Sealed;

#[cfg(any(test, feature = "test_util"))]
#[doc(hidden)]
pub mod test_utils;

#[cfg(feature = "ffi")]
pub use safer_ffi_gen;
