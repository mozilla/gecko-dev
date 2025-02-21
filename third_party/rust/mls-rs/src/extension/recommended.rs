// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

//! Recommended MLS extensions.
//!
//! Optional, but recommended extensions from [The Messaging Layer
//! Security (MLS) Extensions][1].
//!
//! [1]: https://datatracker.ietf.org/doc/html/draft-ietf-mls-extensions-04

use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};
use mls_rs_core::extension::{ExtensionType, MlsCodecExtension};

/// Last resort key packages.
///
/// The extension allows clients that pre-publish key packages to
/// signal to the Delivery Service which key packages are meant to be
/// used as last resort key packages.
#[cfg(feature = "last_resort_key_package_ext")]
#[derive(Debug, Clone, PartialEq, Eq, MlsSize, MlsEncode, MlsDecode)]
pub struct LastResortKeyPackageExt;

#[cfg(feature = "last_resort_key_package_ext")]
impl MlsCodecExtension for LastResortKeyPackageExt {
    fn extension_type() -> ExtensionType {
        ExtensionType::LAST_RESORT_KEY_PACKAGE
    }
}
