/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

mod configuration_types;
mod environment_matching;
mod error;
mod filter;
pub use error::SearchApiError;

pub mod selector;
pub mod types;

pub(crate) use crate::configuration_types::*;
pub use crate::types::*;
pub use selector::SearchEngineSelector;
pub type SearchApiResult<T> = std::result::Result<T, error::SearchApiError>;

uniffi::setup_scaffolding!();
