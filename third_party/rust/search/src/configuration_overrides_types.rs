/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This module defines the structures that we use for serde_json to parse
//! the search configuration overrides.

use crate::JSONEngineUrls;
use serde::Deserialize;

/// Represents search configuration overrides record.
#[derive(Debug, Deserialize, Clone)]
#[serde(rename_all = "camelCase")]
pub struct JSONOverridesRecord {
    /// This is the identifier of the search engine in search-config-v2 that this
    /// record will override. It may be extended by telemetry_suffix.
    pub identifier: String,

    /// The partner code for the engine or variant. This will be inserted into
    /// parameters which include '{partnerCode}
    pub partner_code: String,

    /// Suffix that is appended to the search engine identifier following a
    /// dash, i.e. `<identifier>-<suffix>`. There should always be a suffix
    /// supplied if the partner code is different.
    pub telemetry_suffix: Option<String>,

    /// The url used for reporting clicks.
    pub click_url: String,

    /// The URLs associated with the search engine.
    //pub urls: JSONOverrideEngineUrls,
    pub urls: JSONEngineUrls,
}

/// Represents the search configuration overrides as received from remote settings.
#[derive(Debug, Deserialize)]
pub(crate) struct JSONSearchConfigurationOverrides {
    pub data: Vec<JSONOverridesRecord>,
}
