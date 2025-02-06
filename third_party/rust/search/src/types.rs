/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This module defines the types that we export across the UNIFFI interface.

use serde::Deserialize;

/// The list of possible application names that are currently supported.
#[derive(Clone, Debug, Default, Deserialize, PartialEq, uniffi::Enum)]
#[serde(rename_all = "kebab-case")]
pub enum SearchApplicationName {
    FirefoxAndroid = 1,
    FirefoxIos = 2,
    FocusAndroid = 3,
    FocusIos = 4,
    // The default doesn't really matter here, so we pick desktop.
    #[default]
    Firefox = 5,
}

impl SearchApplicationName {
    pub fn as_str(&self) -> &'static str {
        match self {
            SearchApplicationName::Firefox => "firefox",
            SearchApplicationName::FirefoxAndroid => "firefox-android",
            SearchApplicationName::FocusAndroid => "focus-android",
            SearchApplicationName::FirefoxIos => "firefox-ios",
            SearchApplicationName::FocusIos => "focus-ios",
        }
    }
}

/// The list of possible update channels for a user's build.
/// Use `default` for a self-build or an unknown channel.
#[derive(Clone, Debug, Default, Deserialize, PartialEq, uniffi::Enum)]
#[serde(rename_all = "lowercase")]
pub enum SearchUpdateChannel {
    Nightly = 1,
    Aurora = 2,
    Beta = 3,
    Release = 4,
    Esr = 5,
    #[default]
    Default = 6,
}

#[derive(Clone, Debug, Default, Deserialize, PartialEq, uniffi::Enum)]
#[serde(rename_all = "camelCase")]
pub enum SearchDeviceType {
    Smartphone = 1,
    Tablet = 2,
    #[default]
    None = 3,
}

/// The user's environment that is used for filtering the search configuration.
#[derive(Clone, Debug, uniffi::Record, Default)]
pub struct SearchUserEnvironment {
    /// The current locale of the application that the user is using.
    pub locale: String,

    /// The home region that the user is currently identified as being within.
    /// On desktop & android there is a 14 day lag after detecting a region
    /// change before the home region changes. TBD: iOS?
    pub region: String,

    /// The update channel of the user's build.
    pub update_channel: SearchUpdateChannel,

    /// The distribution id for the user's build.
    pub distribution_id: String,

    /// The search related experiment id that the user is included within. On
    /// desktop this is the `searchConfiguration.experiment` variable.
    pub experiment: String,

    /// The application name that the user is using.
    pub app_name: SearchApplicationName,

    /// The application version that the user is using.
    pub version: String,

    /// The device type that the user is using.
    pub device_type: SearchDeviceType,
}

/// Parameter definitions for search engine URLs. The name property is always
/// specified, along with one of value, experiment_config or search_access_point.
#[derive(Debug, uniffi::Record, PartialEq, Deserialize, Clone)]
#[serde(rename_all = "camelCase")]
pub struct SearchUrlParam {
    /// The name of the parameter in the url.
    pub name: String,
    /// The parameter value, this may be a static value, or additionally contain
    /// a parameter replacement, e.g. `{inputEncoding}`. For the partner code
    /// parameter, this field should be `{partnerCode}`.
    pub value: Option<String>,
    /// The value for the parameter will be derived from the equivalent experiment
    /// configuration value.
    /// Only desktop uses this currently.
    pub experiment_config: Option<String>,
}

/// Defines an individual search engine URL.
#[derive(Debug, uniffi::Record, PartialEq, Deserialize, Clone, Default)]
pub struct SearchEngineUrl {
    /// The PrePath and FilePath of the URL. May include variables for engines
    /// which have a variable FilePath, e.g. `{searchTerm}` for when a search
    /// term is within the path of the url.
    pub base: String,

    /// The HTTP method to use to send the request (`GET` or `POST`).
    /// If the engine definition has not specified the method, it defaults to GET.
    pub method: String,

    /// The parameters for this URL.
    pub params: Vec<SearchUrlParam>,

    /// The name of the query parameter for the search term. Automatically
    /// appended to the end of the query. This may be skipped if `{searchTerm}`
    /// is included in the base.
    pub search_term_param_name: Option<String>,
}

/// The URLs associated with the search engine.
#[derive(Debug, uniffi::Record, PartialEq, Deserialize, Clone, Default)]
pub struct SearchEngineUrls {
    /// The URL to use for searches.
    pub search: SearchEngineUrl,

    /// The URL to use for suggestions.
    pub suggestions: Option<SearchEngineUrl>,

    /// The URL to use for trending suggestions.
    pub trending: Option<SearchEngineUrl>,
}

/// The list of acceptable classifications for a search engine.
#[derive(Debug, uniffi::Enum, PartialEq, Deserialize, Clone, Default)]
#[serde(rename_all = "lowercase")]
pub enum SearchEngineClassification {
    General = 2,
    #[default]
    Unknown = 1,
}

impl SearchEngineClassification {
    pub fn as_str(&self) -> &'static str {
        match self {
            SearchEngineClassification::Unknown => "unknown",
            SearchEngineClassification::General => "general",
        }
    }
}

/// A definition for an individual search engine to be presented to the user.
#[derive(Debug, uniffi::Record, PartialEq, Clone, Default)]
pub struct SearchEngineDefinition {
    /// A list of aliases for this engine.
    pub aliases: Vec<String>,

    /// The character set this engine uses for queries.
    pub charset: String,

    /// The classification of search engine according to the main search types
    /// (e.g. general, shopping, travel, dictionary). Currently, only marking as
    /// a general search engine is supported.
    /// On Android, only general search engines may be selected as "default"
    /// search engines.
    pub classification: SearchEngineClassification,

    /// The identifier of the search engine. This is used as an internal
    /// identifier, e.g. for saving the user's settings for the engine. It is
    /// also used to form the base telemetry id and may be extended by telemetrySuffix.
    pub identifier: String,

    /// The user visible name of the search engine.
    pub name: String,

    /// This search engine is presented as an option that the user may enable.
    /// The application should not include these in the default list of the
    /// user's engines. If not supported, it should filter them out.
    pub optional: bool,

    /// The partner code for the engine. This will be inserted into parameters
    /// which include `{partnerCode}`. May be the empty string.
    pub partner_code: String,

    /// Optional suffix that is appended to the search engine identifier
    /// following a dash, i.e. `<identifier>-<suffix>`
    pub telemetry_suffix: Option<String>,

    /// The URLs associated with the search engine.
    pub urls: SearchEngineUrls,

    /// A hint to the order that this engine should be in the engine list. This
    /// is derived from the `engineOrders` section of the search configuration.
    /// The higher the number, the nearer to the front it should be.
    /// If the number is not specified, other methods of sorting may be relied
    /// upon (e.g. alphabetical).
    pub order_hint: Option<u8>,
}

/// Details of the search engines to display to the user, generated as a result
/// of processing the search configuration.
#[derive(Debug, uniffi::Record, PartialEq)]
pub struct RefinedSearchConfig {
    /// A sorted list of engines. Clients may use the engine in the order that
    /// this list is specified, or they may implement their own order if they
    /// have other requirements.
    ///
    /// The application default engines should not be assumed from this order in
    /// case of future changes.
    ///
    /// The sort order is:
    ///
    /// * Application Default Engine
    /// * Application Default Engine for Private Mode (if specified & different)
    /// * Engines sorted by descending `SearchEngineDefinition.orderHint`
    /// * Any other engines in alphabetical order (locale based comparison)
    pub engines: Vec<SearchEngineDefinition>,

    /// The identifier of the engine that should be used for the application
    /// default engine. If this is undefined, an error has occurred, and the
    /// application should either default to the first engine in the engines
    /// list or otherwise handle appropriately.
    pub app_default_engine_id: Option<String>,

    /// If specified, the identifier of the engine that should be used for the
    /// application default engine in private browsing mode.
    /// Only desktop uses this currently.
    pub app_private_default_engine_id: Option<String>,
}
