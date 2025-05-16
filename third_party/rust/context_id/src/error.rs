/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use error_support::{ErrorHandling, GetErrorHandling};

pub type Result<T> = std::result::Result<T, Error>;
pub type ApiResult<T> = std::result::Result<T, ApiError>;

#[derive(Debug, thiserror::Error, uniffi::Error)]
pub enum ApiError {
    #[error("Something unexpected occurred.")]
    Other { reason: String },
}

#[derive(Debug, thiserror::Error)]
pub enum Error {
    #[error("Timestamp was invalid")]
    InvalidTimestamp { timestamp: i64 },

    #[error("URL parse error: {0}")]
    UrlParseError(#[from] url::ParseError),

    #[error("Viaduct error: {0}")]
    ViaductError(#[from] viaduct::Error),

    #[error("UniFFI callback error: {0}")]
    UniFFICallbackError(#[from] uniffi::UnexpectedUniFFICallbackError),
}

// Define how our internal errors are handled and converted to external errors.
impl GetErrorHandling for Error {
    type ExternalError = ApiError;

    fn get_error_handling(&self) -> ErrorHandling<Self::ExternalError> {
        ErrorHandling::convert(ApiError::Other {
            reason: self.to_string(),
        })
    }
}
