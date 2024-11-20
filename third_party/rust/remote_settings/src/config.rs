/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This module defines the custom configurations that consumers can set.
//! Those configurations override default values and can be used to set a custom server,
//! collection name, and bucket name.
//! The purpose of the configuration parameters are to allow consumers an easy debugging option,
//! and the ability to be explicit about the server.

use url::Url;

use crate::{ApiResult, Error, Result};

/// Remote settings configuration
///
/// This is the version used in the new API, hence the `2` at the end.  The plan is to move
/// consumers to the new API, remove the RemoteSettingsConfig struct, then remove the `2` from this
/// name.
#[derive(Debug, Clone, uniffi::Record)]
pub struct RemoteSettingsConfig2 {
    /// The Remote Settings server to use. Defaults to [RemoteSettingsServer::Prod],
    #[uniffi(default = None)]
    pub server: Option<RemoteSettingsServer>,
    /// Bucket name to use, defaults to "main".  Use "main-preview" for a preview bucket
    #[uniffi(default = None)]
    pub bucket_name: Option<String>,
}

/// Custom configuration for the client.
/// Currently includes the following:
/// - `server`: The Remote Settings server to use. If not specified, defaults to the production server (`RemoteSettingsServer::Prod`).
/// - `server_url`: An optional custom Remote Settings server URL. Deprecated; please use `server` instead.
/// - `bucket_name`: The optional name of the bucket containing the collection on the server. If not specified, the standard bucket will be used.
/// - `collection_name`: The name of the collection for the settings server.
#[derive(Debug, Clone, uniffi::Record)]
pub struct RemoteSettingsConfig {
    pub collection_name: String,
    #[uniffi(default = None)]
    pub bucket_name: Option<String>,
    #[uniffi(default = None)]
    pub server_url: Option<String>,
    #[uniffi(default = None)]
    pub server: Option<RemoteSettingsServer>,
}

/// The Remote Settings server that the client should use.
#[derive(Debug, Clone, uniffi::Enum)]
pub enum RemoteSettingsServer {
    Prod,
    Stage,
    Dev,
    Custom { url: String },
}

impl RemoteSettingsServer {
    /// Get the [url::Url] for this server
    #[error_support::handle_error(Error)]
    pub fn url(&self) -> ApiResult<Url> {
        self.get_url()
    }

    /// Internal version of `url()`.
    ///
    /// The difference is that it uses `Error` instead of `ApiError`.  This is what we need to use
    /// inside the crate.
    pub fn get_url(&self) -> Result<Url> {
        Ok(match self {
            Self::Prod => Url::parse("https://firefox.settings.services.mozilla.com/v1")?,
            Self::Stage => Url::parse("https://firefox.settings.services.allizom.org/v1")?,
            Self::Dev => Url::parse("https://remote-settings-dev.allizom.org/v1")?,
            Self::Custom { url } => {
                let mut url = Url::parse(url)?;
                // Custom URLs are weird and require a couple tricks for backwards compatibility.
                // Normally we append `v1/` to match how this has historically worked.  However,
                // don't do this for file:// schemes which normally don't make any sense, but it's
                // what Nimbus uses to indicate they want to use the file-based client, rather than
                // a remote-settings based one.
                if url.scheme() != "file" {
                    url = url.join("v1")?
                }
                url
            }
        })
    }
}
