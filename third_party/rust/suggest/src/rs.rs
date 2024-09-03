/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//! Crate-internal types for interacting with Remote Settings (`rs`). Types in
//! this module describe records and attachments in the Suggest Remote Settings
//! collection.
//!
//! To add a new suggestion `T` to this component, you'll generally need to:
//!
//!  1. Add a variant named `T` to [`SuggestRecord`]. The variant must have a
//!     `#[serde(rename)]` attribute that matches the suggestion record's
//!     `type` field.
//!  2. Define a `DownloadedTSuggestion` type with the new suggestion's fields,
//!     matching their attachment's schema. Your new type must derive or
//!     implement [`serde::Deserialize`].
//!  3. Update the database schema in the [`schema`] module to store the new
//!     suggestion.
//!  4. Add an `insert_t_suggestions()` method to [`db::SuggestDao`] that
//!     inserts `DownloadedTSuggestion`s into the database.
//!  5. Update [`store::SuggestStoreInner::ingest()`] to download, deserialize,
//!     and store the new suggestion.
//!  6. Add a variant named `T` to [`suggestion::Suggestion`], with the fields
//!     that you'd like to expose to the application. These can be the same
//!     fields as `DownloadedTSuggestion`, or slightly different, depending on
//!     what the application needs to show the suggestion.
//!  7. Update the `Suggestion` enum definition in `suggest.udl` to match your
//!     new [`suggestion::Suggestion`] variant.
//!  8. Update any [`db::SuggestDao`] methods that query the database to include
//!     the new suggestion in their results, and return `Suggestion::T` variants
//!     as needed.

use std::fmt;

use remote_settings::{Attachment, RemoteSettingsRecord};
use serde::{Deserialize, Deserializer};

use crate::{db::SuggestDao, error::Error, provider::SuggestionProvider, Result};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum Collection {
    Quicksuggest,
    Fakespot,
}

impl Collection {
    pub fn name(&self) -> &'static str {
        match self {
            Self::Quicksuggest => "quicksuggest",
            Self::Fakespot => "fakespot-suggest-products",
        }
    }
}

/// A trait for a client that downloads suggestions from Remote Settings.
///
/// This trait lets tests use a mock client.
pub(crate) trait Client {
    /// Get all records from the server
    ///
    /// We use this plus client-side filtering rather than any server-side filtering, as
    /// recommended by the remote settings docs
    /// (https://remote-settings.readthedocs.io/en/stable/client-specifications.html). This is
    /// relatively inexpensive since we use a cache and don't fetch attachments until after the
    /// client-side filtering.
    ///
    /// Records that can't be parsed as [SuggestRecord] are ignored.
    fn get_records(&self, collection: Collection, dao: &mut SuggestDao) -> Result<Vec<Record>>;

    fn download_attachment(&self, record: &Record) -> Result<Vec<u8>>;
}

/// Implements the [Client] trait using a real remote settings client
pub struct RemoteSettingsClient {
    // Create a separate client for each collection name
    quicksuggest_client: remote_settings::Client,
    fakespot_client: remote_settings::Client,
}

impl RemoteSettingsClient {
    pub fn new(
        server: Option<remote_settings::RemoteSettingsServer>,
        bucket_name: Option<String>,
        server_url: Option<String>,
    ) -> Result<Self> {
        Ok(Self {
            quicksuggest_client: remote_settings::Client::new(
                remote_settings::RemoteSettingsConfig {
                    server: server.clone(),
                    bucket_name: bucket_name.clone(),
                    collection_name: "quicksuggest".to_owned(),
                    server_url: server_url.clone(),
                },
            )?,
            fakespot_client: remote_settings::Client::new(remote_settings::RemoteSettingsConfig {
                server,
                bucket_name,
                collection_name: "fakespot-suggest-products".to_owned(),
                server_url,
            })?,
        })
    }

    fn client_for_collection(&self, collection: Collection) -> &remote_settings::Client {
        match collection {
            Collection::Fakespot => &self.fakespot_client,
            Collection::Quicksuggest => &self.quicksuggest_client,
        }
    }
}

impl Client for RemoteSettingsClient {
    fn get_records(&self, collection: Collection, dao: &mut SuggestDao) -> Result<Vec<Record>> {
        // For now, handle the cache manually.  Once 6328 is merged, we should be able to delegate
        // this to remote_settings.
        let client = self.client_for_collection(collection);
        let cache = dao.read_cached_rs_data(collection.name());
        let last_modified = match &cache {
            Some(response) => response.last_modified,
            None => 0,
        };
        let response = match cache {
            None => client.get_records()?,
            Some(cache) => remote_settings::cache::merge_cache_and_response(
                cache,
                client.get_records_since(last_modified)?,
            ),
        };
        if last_modified != response.last_modified {
            dao.write_cached_rs_data(collection.name(), &response);
        }

        Ok(response
            .records
            .into_iter()
            .filter_map(|r| Record::new(r, collection).ok())
            .collect())
    }

    fn download_attachment(&self, record: &Record) -> Result<Vec<u8>> {
        match &record.attachment {
            Some(a) => Ok(self
                .client_for_collection(record.collection)
                .get_attachment(&a.location)?),
            None => Err(Error::MissingAttachment(record.id.to_string())),
        }
    }
}

/// Remote settings record for suggest.
///
/// This is a `remote_settings::RemoteSettingsRecord` parsed for suggest.
#[derive(Clone, Debug)]
pub(crate) struct Record {
    pub id: SuggestRecordId,
    pub last_modified: u64,
    pub attachment: Option<Attachment>,
    pub payload: SuggestRecord,
    pub collection: Collection,
}

impl Record {
    pub fn new(record: RemoteSettingsRecord, collection: Collection) -> Result<Self> {
        Ok(Self {
            id: SuggestRecordId::new(record.id),
            last_modified: record.last_modified,
            attachment: record.attachment,
            payload: serde_json::from_value(serde_json::Value::Object(record.fields))?,
            collection,
        })
    }

    pub fn record_type(&self) -> SuggestRecordType {
        (&self.payload).into()
    }
}

/// A record in the Suggest Remote Settings collection.
///
/// Most Suggest records don't carry inline fields except for `type`.
/// Suggestions themselves are typically stored in each record's attachment.
#[derive(Clone, Debug, Deserialize)]
#[serde(tag = "type")]
pub(crate) enum SuggestRecord {
    #[serde(rename = "icon")]
    Icon,
    #[serde(rename = "data")]
    AmpWikipedia,
    #[serde(rename = "amo-suggestions")]
    Amo,
    #[serde(rename = "pocket-suggestions")]
    Pocket,
    #[serde(rename = "yelp-suggestions")]
    Yelp,
    #[serde(rename = "mdn-suggestions")]
    Mdn,
    #[serde(rename = "weather")]
    Weather(DownloadedWeatherData),
    #[serde(rename = "configuration")]
    GlobalConfig(DownloadedGlobalConfig),
    #[serde(rename = "amp-mobile-suggestions")]
    AmpMobile,
    #[serde(rename = "fakespot-suggestions")]
    Fakespot,
    #[serde(rename = "exposure-suggestions")]
    Exposure(DownloadedExposureRecord),
}

/// Enum for the different record types that can be consumed.
/// Extracting this from the serialization enum so that we can
/// extend it to get type metadata.
#[derive(Copy, Clone, PartialEq, PartialOrd, Eq, Ord, Hash)]
pub enum SuggestRecordType {
    Icon,
    AmpWikipedia,
    Amo,
    Pocket,
    Yelp,
    Mdn,
    Weather,
    GlobalConfig,
    AmpMobile,
    Fakespot,
    Exposure,
}

impl From<&SuggestRecord> for SuggestRecordType {
    fn from(suggest_record: &SuggestRecord) -> Self {
        match suggest_record {
            SuggestRecord::Amo => Self::Amo,
            SuggestRecord::AmpWikipedia => Self::AmpWikipedia,
            SuggestRecord::Icon => Self::Icon,
            SuggestRecord::Mdn => Self::Mdn,
            SuggestRecord::Pocket => Self::Pocket,
            SuggestRecord::Weather(_) => Self::Weather,
            SuggestRecord::Yelp => Self::Yelp,
            SuggestRecord::GlobalConfig(_) => Self::GlobalConfig,
            SuggestRecord::AmpMobile => Self::AmpMobile,
            SuggestRecord::Fakespot => Self::Fakespot,
            SuggestRecord::Exposure(_) => Self::Exposure,
        }
    }
}

impl fmt::Display for SuggestRecordType {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.as_str())
    }
}

impl SuggestRecordType {
    /// Get all record types to iterate over
    ///
    /// Currently only used by tests
    #[cfg(test)]
    pub fn all() -> &'static [SuggestRecordType] {
        &[
            Self::Icon,
            Self::AmpWikipedia,
            Self::Amo,
            Self::Pocket,
            Self::Yelp,
            Self::Mdn,
            Self::Weather,
            Self::GlobalConfig,
            Self::AmpMobile,
            Self::Fakespot,
            Self::Exposure,
        ]
    }

    pub fn as_str(&self) -> &str {
        match self {
            Self::Icon => "icon",
            Self::AmpWikipedia => "data",
            Self::Amo => "amo-suggestions",
            Self::Pocket => "pocket-suggestions",
            Self::Yelp => "yelp-suggestions",
            Self::Mdn => "mdn-suggestions",
            Self::Weather => "weather",
            Self::GlobalConfig => "configuration",
            Self::AmpMobile => "amp-mobile-suggestions",
            Self::Fakespot => "fakespot-suggestions",
            Self::Exposure => "exposure-suggestions",
        }
    }

    pub fn collection(&self) -> Collection {
        match self {
            Self::Fakespot => Collection::Fakespot,
            _ => Collection::Quicksuggest,
        }
    }
}

/// Represents either a single value, or a list of values. This is used to
/// deserialize downloaded attachments.
#[derive(Clone, Debug, Deserialize)]
#[serde(untagged)]
enum OneOrMany<T> {
    One(T),
    Many(Vec<T>),
}

/// A downloaded Remote Settings attachment that contains suggestions.
#[derive(Clone, Debug, Deserialize)]
#[serde(transparent)]
pub(crate) struct SuggestAttachment<T>(OneOrMany<T>);

impl<T> SuggestAttachment<T> {
    /// Returns a slice of suggestions to ingest from the downloaded attachment.
    pub fn suggestions(&self) -> &[T] {
        match &self.0 {
            OneOrMany::One(value) => std::slice::from_ref(value),
            OneOrMany::Many(values) => values,
        }
    }
}

/// The ID of a record in the Suggest Remote Settings collection.
#[derive(Clone, Debug, Deserialize, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[serde(transparent)]
pub(crate) struct SuggestRecordId(String);

impl SuggestRecordId {
    pub fn new(id: String) -> Self {
        Self(id)
    }

    pub fn as_str(&self) -> &str {
        &self.0
    }

    /// If this ID is for an icon record, extracts and returns the icon ID.
    ///
    /// The icon ID is the primary key for an ingested icon. Downloaded
    /// suggestions also reference these icon IDs, in
    /// [`DownloadedSuggestion::icon_id`].
    pub fn as_icon_id(&self) -> Option<&str> {
        self.0.strip_prefix("icon-")
    }
}

impl fmt::Display for SuggestRecordId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.0)
    }
}

/// Fields that are common to all downloaded suggestions.
#[derive(Clone, Debug, Default, Deserialize)]
pub(crate) struct DownloadedSuggestionCommonDetails {
    pub keywords: Vec<String>,
    pub title: String,
    pub url: String,
    pub score: Option<f64>,
    #[serde(default)]
    pub full_keywords: Vec<(String, usize)>,
}

/// An AMP suggestion to ingest from an AMP-Wikipedia attachment.
#[derive(Clone, Debug, Default, Deserialize)]
pub(crate) struct DownloadedAmpSuggestion {
    #[serde(flatten)]
    pub common_details: DownloadedSuggestionCommonDetails,
    pub advertiser: String,
    #[serde(rename = "id")]
    pub block_id: i32,
    pub iab_category: String,
    pub click_url: String,
    pub impression_url: String,
    #[serde(rename = "icon")]
    pub icon_id: String,
}

/// A Wikipedia suggestion to ingest from an AMP-Wikipedia attachment.
#[derive(Clone, Debug, Default, Deserialize)]
pub(crate) struct DownloadedWikipediaSuggestion {
    #[serde(flatten)]
    pub common_details: DownloadedSuggestionCommonDetails,
    #[serde(rename = "icon")]
    pub icon_id: String,
}

/// A suggestion to ingest from an AMP-Wikipedia attachment downloaded from
/// Remote Settings.
#[derive(Clone, Debug)]
pub(crate) enum DownloadedAmpWikipediaSuggestion {
    Amp(DownloadedAmpSuggestion),
    Wikipedia(DownloadedWikipediaSuggestion),
}

impl DownloadedAmpWikipediaSuggestion {
    /// Returns the details that are common to AMP and Wikipedia suggestions.
    pub fn common_details(&self) -> &DownloadedSuggestionCommonDetails {
        match self {
            Self::Amp(DownloadedAmpSuggestion { common_details, .. }) => common_details,
            Self::Wikipedia(DownloadedWikipediaSuggestion { common_details, .. }) => common_details,
        }
    }

    /// Returns the provider of this suggestion.
    pub fn provider(&self) -> SuggestionProvider {
        match self {
            DownloadedAmpWikipediaSuggestion::Amp(_) => SuggestionProvider::Amp,
            DownloadedAmpWikipediaSuggestion::Wikipedia(_) => SuggestionProvider::Wikipedia,
        }
    }
}

impl DownloadedSuggestionCommonDetails {
    /// Iterate over all keywords for this suggestion
    pub fn keywords(&self) -> impl Iterator<Item = AmpKeyword<'_>> {
        let full_keywords = self
            .full_keywords
            .iter()
            .flat_map(|(full_keyword, repeat_for)| {
                std::iter::repeat(Some(full_keyword.as_str())).take(*repeat_for)
            })
            .chain(std::iter::repeat(None)); // In case of insufficient full keywords, just fill in with infinite `None`s
                                             //
        self.keywords.iter().zip(full_keywords).enumerate().map(
            move |(i, (keyword, full_keyword))| AmpKeyword {
                rank: i,
                keyword,
                full_keyword,
            },
        )
    }
}

#[derive(Debug, PartialEq, Eq)]
pub(crate) struct AmpKeyword<'a> {
    pub rank: usize,
    pub keyword: &'a str,
    pub full_keyword: Option<&'a str>,
}

impl<'de> Deserialize<'de> for DownloadedAmpWikipediaSuggestion {
    fn deserialize<D>(
        deserializer: D,
    ) -> std::result::Result<DownloadedAmpWikipediaSuggestion, D::Error>
    where
        D: Deserializer<'de>,
    {
        // AMP and Wikipedia suggestions use the same schema. To separate them,
        // we use a "maybe tagged" outer enum with tagged and untagged variants,
        // and a "tagged" inner enum.
        //
        // Wikipedia suggestions will deserialize successfully into the tagged
        // variant. AMP suggestions will try the tagged variant, fail, and fall
        // back to the untagged variant.
        //
        // This approach works around serde-rs/serde#912.

        #[derive(Deserialize)]
        #[serde(untagged)]
        enum MaybeTagged {
            Tagged(Tagged),
            Untagged(DownloadedAmpSuggestion),
        }

        #[derive(Deserialize)]
        #[serde(tag = "advertiser")]
        enum Tagged {
            #[serde(rename = "Wikipedia")]
            Wikipedia(DownloadedWikipediaSuggestion),
        }

        Ok(match MaybeTagged::deserialize(deserializer)? {
            MaybeTagged::Tagged(Tagged::Wikipedia(wikipedia)) => Self::Wikipedia(wikipedia),
            MaybeTagged::Untagged(amp) => Self::Amp(amp),
        })
    }
}

/// An AMO suggestion to ingest from an attachment
#[derive(Clone, Debug, Deserialize)]
pub(crate) struct DownloadedAmoSuggestion {
    pub description: String,
    pub url: String,
    pub guid: String,
    #[serde(rename = "icon")]
    pub icon_url: String,
    pub rating: Option<String>,
    pub number_of_ratings: i64,
    pub title: String,
    pub keywords: Vec<String>,
    pub score: f64,
}
/// A Pocket suggestion to ingest from a Pocket Suggestion Attachment
#[derive(Clone, Debug, Deserialize)]
pub(crate) struct DownloadedPocketSuggestion {
    pub url: String,
    pub title: String,
    #[serde(rename = "lowConfidenceKeywords")]
    pub low_confidence_keywords: Vec<String>,
    #[serde(rename = "highConfidenceKeywords")]
    pub high_confidence_keywords: Vec<String>,
    pub score: f64,
}
/// A location sign for Yelp to ingest from a Yelp Attachment
#[derive(Clone, Debug, Deserialize)]
pub(crate) struct DownloadedYelpLocationSign {
    pub keyword: String,
    #[serde(rename = "needLocation")]
    pub need_location: bool,
}
/// A Yelp suggestion to ingest from a Yelp Attachment
#[derive(Clone, Debug, Deserialize)]
pub(crate) struct DownloadedYelpSuggestion {
    pub subjects: Vec<String>,
    #[serde(rename = "preModifiers")]
    pub pre_modifiers: Vec<String>,
    #[serde(rename = "postModifiers")]
    pub post_modifiers: Vec<String>,
    #[serde(rename = "locationSigns")]
    pub location_signs: Vec<DownloadedYelpLocationSign>,
    #[serde(rename = "yelpModifiers")]
    pub yelp_modifiers: Vec<String>,
    #[serde(rename = "icon")]
    pub icon_id: String,
    pub score: f64,
}

/// An MDN suggestion to ingest from an attachment
#[derive(Clone, Debug, Deserialize)]
pub(crate) struct DownloadedMdnSuggestion {
    pub url: String,
    pub title: String,
    pub description: String,
    pub keywords: Vec<String>,
    pub score: f64,
}

/// A Fakespot suggestion to ingest from an attachment
#[derive(Clone, Debug, Deserialize)]
pub(crate) struct DownloadedFakespotSuggestion {
    pub fakespot_grade: String,
    pub product_id: String,
    pub keywords: String,
    pub product_type: String,
    pub rating: f64,
    pub score: f64,
    pub title: String,
    pub total_reviews: i64,
    pub url: String,
}

/// An exposure suggestion record's inline data
#[derive(Clone, Debug, Deserialize)]
pub(crate) struct DownloadedExposureRecord {
    pub suggestion_type: String,
}

/// An exposure suggestion to ingest from an attachment
#[derive(Clone, Debug, Deserialize)]
pub(crate) struct DownloadedExposureSuggestion {
    keywords: Vec<FullOrPrefixKeywords<String>>,
}

impl DownloadedExposureSuggestion {
    /// Iterate over all keywords for this suggestion. Iteration may contain
    /// duplicate keywords depending on the structure of the data, so do not
    /// assume keywords are unique. Duplicates are not filtered out because
    /// doing so would require O(number of keywords) space, and the number of
    /// keywords can be very large. If you are inserting into the store, rely on
    /// uniqueness constraints and use `INSERT OR IGNORE`.
    pub fn keywords(&self) -> impl Iterator<Item = String> + '_ {
        self.keywords.iter().flat_map(|e| e.keywords())
    }
}

/// A single full keyword or a `(prefix, suffixes)` tuple representing multiple
/// prefix keywords. Prefix keywords are enumerated by appending to `prefix`
/// each possible prefix of each suffix, including the full suffix. The prefix
/// is also enumerated by itself. Examples:
///
/// `FullOrPrefixKeywords::Full("some full keyword")`
/// => "some full keyword"
///
/// `FullOrPrefixKeywords::Prefix(("sug", vec!["gest", "arplum"]))`
/// => "sug"
///    "sugg"
///    "sugge"
///    "sugges"
///    "suggest"
///    "suga"
///    "sugar"
///    "sugarp"
///    "sugarpl"
///    "sugarplu"
///    "sugarplum"
#[derive(Clone, Debug, Deserialize)]
#[serde(untagged)]
enum FullOrPrefixKeywords<T> {
    Full(T),
    Prefix((T, Vec<T>)),
}

impl<T> From<T> for FullOrPrefixKeywords<T> {
    fn from(full_keyword: T) -> Self {
        Self::Full(full_keyword)
    }
}

impl<T> From<(T, Vec<T>)> for FullOrPrefixKeywords<T> {
    fn from(prefix_suffixes: (T, Vec<T>)) -> Self {
        Self::Prefix(prefix_suffixes)
    }
}

impl FullOrPrefixKeywords<String> {
    pub fn keywords(&self) -> Box<dyn Iterator<Item = String> + '_> {
        match self {
            FullOrPrefixKeywords::Full(kw) => Box::new(std::iter::once(kw.to_owned())),
            FullOrPrefixKeywords::Prefix((prefix, suffixes)) => Box::new(
                std::iter::once(prefix.to_owned()).chain(suffixes.iter().flat_map(|suffix| {
                    let mut kw = prefix.clone();
                    suffix.chars().map(move |c| {
                        kw.push(c);
                        kw.clone()
                    })
                })),
            ),
        }
    }
}

/// Weather data to ingest from a weather record
#[derive(Clone, Debug, Deserialize)]
pub(crate) struct DownloadedWeatherData {
    pub weather: DownloadedWeatherDataInner,
}
#[derive(Clone, Debug, Deserialize)]
pub(crate) struct DownloadedWeatherDataInner {
    pub min_keyword_length: i32,
    pub keywords: Vec<String>,
    // Remote settings doesn't support floats in record JSON so we use a
    // stringified float instead. If a float can't be parsed, this will be None.
    #[serde(default, deserialize_with = "de_stringified_f64")]
    pub score: Option<f64>,
}

/// Global Suggest configuration data to ingest from a configuration record
#[derive(Clone, Debug, Deserialize)]
pub(crate) struct DownloadedGlobalConfig {
    pub configuration: DownloadedGlobalConfigInner,
}
#[derive(Clone, Debug, Deserialize)]
pub(crate) struct DownloadedGlobalConfigInner {
    /// The maximum number of times the user can click "Show less frequently"
    /// for a suggestion in the UI.
    pub show_less_frequently_cap: i32,
}

fn de_stringified_f64<'de, D>(deserializer: D) -> std::result::Result<Option<f64>, D::Error>
where
    D: Deserializer<'de>,
{
    String::deserialize(deserializer).map(|s| s.parse().ok())
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_full_keywords() {
        let suggestion = DownloadedAmpWikipediaSuggestion::Amp(DownloadedAmpSuggestion {
            common_details: DownloadedSuggestionCommonDetails {
                keywords: vec![
                    String::from("f"),
                    String::from("fo"),
                    String::from("foo"),
                    String::from("foo b"),
                    String::from("foo ba"),
                    String::from("foo bar"),
                ],
                full_keywords: vec![(String::from("foo"), 3), (String::from("foo bar"), 3)],
                ..DownloadedSuggestionCommonDetails::default()
            },
            ..DownloadedAmpSuggestion::default()
        });

        assert_eq!(
            Vec::from_iter(suggestion.common_details().keywords()),
            vec![
                AmpKeyword {
                    rank: 0,
                    keyword: "f",
                    full_keyword: Some("foo"),
                },
                AmpKeyword {
                    rank: 1,
                    keyword: "fo",
                    full_keyword: Some("foo"),
                },
                AmpKeyword {
                    rank: 2,
                    keyword: "foo",
                    full_keyword: Some("foo"),
                },
                AmpKeyword {
                    rank: 3,
                    keyword: "foo b",
                    full_keyword: Some("foo bar"),
                },
                AmpKeyword {
                    rank: 4,
                    keyword: "foo ba",
                    full_keyword: Some("foo bar"),
                },
                AmpKeyword {
                    rank: 5,
                    keyword: "foo bar",
                    full_keyword: Some("foo bar"),
                },
            ],
        );
    }

    #[test]
    fn test_missing_full_keywords() {
        let suggestion = DownloadedAmpWikipediaSuggestion::Amp(DownloadedAmpSuggestion {
            common_details: DownloadedSuggestionCommonDetails {
                keywords: vec![
                    String::from("f"),
                    String::from("fo"),
                    String::from("foo"),
                    String::from("foo b"),
                    String::from("foo ba"),
                    String::from("foo bar"),
                ],
                // Only the first 3 keywords have full keywords associated with them
                full_keywords: vec![(String::from("foo"), 3)],
                ..DownloadedSuggestionCommonDetails::default()
            },
            ..DownloadedAmpSuggestion::default()
        });

        assert_eq!(
            Vec::from_iter(suggestion.common_details().keywords()),
            vec![
                AmpKeyword {
                    rank: 0,
                    keyword: "f",
                    full_keyword: Some("foo"),
                },
                AmpKeyword {
                    rank: 1,
                    keyword: "fo",
                    full_keyword: Some("foo"),
                },
                AmpKeyword {
                    rank: 2,
                    keyword: "foo",
                    full_keyword: Some("foo"),
                },
                AmpKeyword {
                    rank: 3,
                    keyword: "foo b",
                    full_keyword: None,
                },
                AmpKeyword {
                    rank: 4,
                    keyword: "foo ba",
                    full_keyword: None,
                },
                AmpKeyword {
                    rank: 5,
                    keyword: "foo bar",
                    full_keyword: None,
                },
            ],
        );
    }

    fn full_or_prefix_keywords_to_owned(
        kws: Vec<FullOrPrefixKeywords<&str>>,
    ) -> Vec<FullOrPrefixKeywords<String>> {
        kws.iter()
            .map(|val| match val {
                FullOrPrefixKeywords::Full(s) => FullOrPrefixKeywords::Full(s.to_string()),
                FullOrPrefixKeywords::Prefix((prefix, suffixes)) => FullOrPrefixKeywords::Prefix((
                    prefix.to_string(),
                    suffixes.iter().map(|s| s.to_string()).collect(),
                )),
            })
            .collect()
    }

    #[test]
    fn test_exposure_keywords() {
        let suggestion = DownloadedExposureSuggestion {
            keywords: full_or_prefix_keywords_to_owned(vec![
                "no suffixes".into(),
                ("empty suffixes", vec![]).into(),
                ("empty string suffix", vec![""]).into(),
                ("choco", vec!["", "bo", "late"]).into(),
                "duplicate 1".into(),
                "duplicate 1".into(),
                ("dup", vec!["licate 1", "licate 2"]).into(),
                ("dup", vec!["lo", "licate 2", "licate 3"]).into(),
                ("duplic", vec!["ate 3", "ar", "ate 4"]).into(),
                ("du", vec!["plicate 4", "plicate 5", "nk"]).into(),
            ]),
        };

        assert_eq!(
            Vec::from_iter(suggestion.keywords()),
            vec![
                "no suffixes",
                "empty suffixes",
                "empty string suffix",
                "choco",
                "chocob",
                "chocobo",
                "chocol",
                "chocola",
                "chocolat",
                "chocolate",
                "duplicate 1",
                "duplicate 1",
                "dup",
                "dupl",
                "dupli",
                "duplic",
                "duplica",
                "duplicat",
                "duplicate",
                "duplicate ",
                "duplicate 1",
                "dupl",
                "dupli",
                "duplic",
                "duplica",
                "duplicat",
                "duplicate",
                "duplicate ",
                "duplicate 2",
                "dup",
                "dupl",
                "duplo",
                "dupl",
                "dupli",
                "duplic",
                "duplica",
                "duplicat",
                "duplicate",
                "duplicate ",
                "duplicate 2",
                "dupl",
                "dupli",
                "duplic",
                "duplica",
                "duplicat",
                "duplicate",
                "duplicate ",
                "duplicate 3",
                "duplic",
                "duplica",
                "duplicat",
                "duplicate",
                "duplicate ",
                "duplicate 3",
                "duplica",
                "duplicar",
                "duplica",
                "duplicat",
                "duplicate",
                "duplicate ",
                "duplicate 4",
                "du",
                "dup",
                "dupl",
                "dupli",
                "duplic",
                "duplica",
                "duplicat",
                "duplicate",
                "duplicate ",
                "duplicate 4",
                "dup",
                "dupl",
                "dupli",
                "duplic",
                "duplica",
                "duplicat",
                "duplicate",
                "duplicate ",
                "duplicate 5",
                "dun",
                "dunk",
            ],
        );
    }
}
