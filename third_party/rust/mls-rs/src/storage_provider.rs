// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

/// Storage providers that operate completely in memory.
pub mod in_memory;
pub(crate) mod key_package;

pub use key_package::*;

#[cfg(feature = "sqlite")]
#[cfg_attr(docsrs, doc(cfg(feature = "sqlite")))]
/// SQLite based storage providers.
pub mod sqlite;
