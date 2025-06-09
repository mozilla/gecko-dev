/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pub type Result<T> = std::result::Result<T, ()>;

pub type ApiResult<T> = std::result::Result<T, ApiError>;

/// Public error class
#[derive(Debug, thiserror::Error, uniffi::Error)]
pub enum ApiError {
    #[error("Other error: {reason}")]
    Other { reason: String },
}
