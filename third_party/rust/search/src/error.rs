/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Defines the error types for this module.

/// The error type for all Search component operations. These errors are
/// exposed to your application, which should handle them as needed.
use error_support::{ErrorHandling, GetErrorHandling};

/// A list of errors that are internal to the component. This is the error
/// type for private and crate-internal methods, and is never returned to the
/// application.
#[derive(Debug, thiserror::Error)]
pub enum Error {
    #[error("Search configuration not specified")]
    SearchConfigNotSpecified,
    #[error("JSON error: {0}")]
    Json(#[from] serde_json::Error),
}

// #[non_exhaustive]
#[derive(Debug, thiserror::Error, uniffi::Error)]
pub enum SearchApiError {
    #[error("Other error: {reason}")]
    Other { reason: String },
}

// Define how our internal errors are handled and converted to external errors.
impl GetErrorHandling for Error {
    type ExternalError = SearchApiError;

    fn get_error_handling(&self) -> ErrorHandling<Self::ExternalError> {
        ErrorHandling::convert(SearchApiError::Other {
            reason: self.to_string(),
        })
        .report_error("search-unexpected")
    }
}
