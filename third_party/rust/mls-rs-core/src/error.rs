// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use core::fmt::{self, Display};

#[cfg(feature = "std")]
#[derive(Debug)]
/// Generic error used to wrap errors produced by providers.
pub struct AnyError(Box<dyn std::error::Error + Send + Sync>);

#[cfg(not(feature = "std"))]
#[derive(Debug)]
pub struct AnyError;

#[cfg(feature = "std")]
impl Display for AnyError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.0.fmt(f)
    }
}

#[cfg(feature = "std")]
impl std::error::Error for AnyError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        self.0.source()
    }
}

#[cfg(not(feature = "std"))]
impl Display for AnyError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        core::fmt::Debug::fmt(self, f)
    }
}

/// Trait to convert a provider specific error into [`AnyError`]
pub trait IntoAnyError: core::fmt::Debug + Sized {
    #[cfg(feature = "std")]
    fn into_any_error(self) -> AnyError {
        self.into_dyn_error()
            .map_or_else(|this| AnyError(format!("{this:?}").into()), AnyError)
    }

    #[cfg(not(feature = "std"))]
    fn into_any_error(self) -> AnyError {
        AnyError
    }

    #[cfg(feature = "std")]
    fn into_dyn_error(self) -> Result<Box<dyn std::error::Error + Send + Sync>, Self> {
        Err(self)
    }
}

impl IntoAnyError for mls_rs_codec::Error {
    #[cfg(feature = "std")]
    fn into_dyn_error(self) -> Result<Box<dyn std::error::Error + Send + Sync>, Self> {
        Ok(self.into())
    }
}

impl IntoAnyError for core::convert::Infallible {}
