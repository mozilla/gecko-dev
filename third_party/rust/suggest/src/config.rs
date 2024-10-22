/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

use serde::{Deserialize, Serialize};

use crate::rs::DownloadedGlobalConfig;

/// Global Suggest configuration data.
#[derive(Clone, Default, Debug, Deserialize, Serialize, PartialEq, Eq, uniffi::Record)]
pub struct SuggestGlobalConfig {
    pub show_less_frequently_cap: i32,
}

impl From<&DownloadedGlobalConfig> for SuggestGlobalConfig {
    fn from(config: &DownloadedGlobalConfig) -> Self {
        Self {
            show_less_frequently_cap: config.configuration.show_less_frequently_cap,
        }
    }
}

/// Per-provider configuration data.
#[derive(Clone, Debug, Deserialize, Serialize, PartialEq, uniffi::Enum)]
pub enum SuggestProviderConfig {
    Weather {
        /// Weather suggestion score.
        score: f64,
        /// Threshold for weather keyword prefix matching when a weather keyword
        /// is the first term in a query. Zero means prefix matching is disabled
        /// and weather keywords must be typed in full when they are first in
        /// the query. (Ideally this would be an `Option` and `None` would mean
        /// full keywords are required, but it's probably not worth the breaking
        /// API change.) This threshold does not apply to city and region names.
        min_keyword_length: i32,
    },
}
