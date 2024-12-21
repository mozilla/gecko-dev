/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::config::RemoteSettingsConfig;
use crate::error::{Error, Result};
#[cfg(feature = "jexl")]
use crate::jexl_filter::JexlFilter;
use crate::storage::Storage;
#[cfg(feature = "jexl")]
use crate::RemoteSettingsContext;
use crate::{
    packaged_attachments, packaged_collections, RemoteSettingsServer, UniffiCustomTypeConverter,
};
use parking_lot::Mutex;
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use std::{
    borrow::Cow,
    time::{Duration, Instant},
};
use url::Url;
use viaduct::{Request, Response};

const HEADER_BACKOFF: &str = "Backoff";
const HEADER_ETAG: &str = "ETag";
const HEADER_RETRY_AFTER: &str = "Retry-After";

#[derive(Debug, Clone, Deserialize)]
struct CollectionData {
    data: Vec<RemoteSettingsRecord>,
    timestamp: u64,
}

/// Internal Remote settings client API
///
/// This stores an ApiClient implementation.  In the real-world, this is always ViaductApiClient,
/// but the tests use a mock client.
pub struct RemoteSettingsClient<C = ViaductApiClient> {
    // This is immutable, so it can be outside the mutex
    collection_name: String,
    #[cfg(feature = "jexl")]
    jexl_filter: JexlFilter,
    inner: Mutex<RemoteSettingsClientInner<C>>,
}

struct RemoteSettingsClientInner<C> {
    storage: Storage,
    api_client: C,
}

// Add your local packaged data you want to work with here
impl<C: ApiClient> RemoteSettingsClient<C> {
    // One line per bucket + collection
    packaged_collections! {
        ("main", "search-telemetry-v2"),
        ("main", "regions"),
    }

    // You have to specify
    // - bucket + collection_name: ("main", "regions")
    // - One line per file you want to add (e.g. "world")
    //
    // This will automatically also include the NAME.meta.json file
    // for internal validation against hash and size
    //
    // The entries line up with the `Attachment::filename` field,
    // and check for the folder + name in
    // `remote_settings/dumps/{bucket}/attachments/{collection}/{filename}
    packaged_attachments! {
        ("main", "regions") => [
            "world",
            "world-buffered",
        ],
    }
}

impl<C: ApiClient> RemoteSettingsClient<C> {
    pub fn new_from_parts(
        collection_name: String,
        storage: Storage,
        #[cfg(feature = "jexl")] jexl_filter: JexlFilter,
        api_client: C,
    ) -> Self {
        Self {
            collection_name,
            #[cfg(feature = "jexl")]
            jexl_filter,
            inner: Mutex::new(RemoteSettingsClientInner {
                storage,
                api_client,
            }),
        }
    }

    pub fn collection_name(&self) -> &str {
        &self.collection_name
    }

    fn load_packaged_data(&self) -> Option<CollectionData> {
        // Using the macro generated `get_packaged_data` in macros.rs
        Self::get_packaged_data(&self.collection_name)
            .and_then(|data| serde_json::from_str(data).ok())
    }

    fn load_packaged_attachment(&self, filename: &str) -> Option<(&'static [u8], &'static str)> {
        // Using the macro generated `get_packaged_attachment` in macros.rs
        Self::get_packaged_attachment(&self.collection_name, filename)
    }

    /// Filters records based on the presence and evaluation of `filter_expression`.
    #[cfg(feature = "jexl")]
    fn filter_records(&self, records: Vec<RemoteSettingsRecord>) -> Vec<RemoteSettingsRecord> {
        records
            .into_iter()
            .filter(|record| match record.fields.get("filter_expression") {
                Some(serde_json::Value::String(filter_expr)) => {
                    self.jexl_filter.evaluate(filter_expr).unwrap_or(false)
                }
                _ => true, // Include records without a valid filter expression by default
            })
            .collect()
    }

    #[cfg(not(feature = "jexl"))]
    fn filter_records(&self, records: Vec<RemoteSettingsRecord>) -> Vec<RemoteSettingsRecord> {
        records
    }

    /// Get the current set of records.
    ///
    /// If records are not present in storage this will normally return None.  Use `sync_if_empty =
    /// true` to change this behavior and perform a network request in this case.
    pub fn get_records(&self, sync_if_empty: bool) -> Result<Option<Vec<RemoteSettingsRecord>>> {
        let mut inner = self.inner.lock();
        let collection_url = inner.api_client.collection_url();
        let is_prod = inner.api_client.is_prod_server()?;
        let packaged_data = if is_prod {
            self.load_packaged_data()
        } else {
            None
        };

        // Case 1: The packaged data is more recent than the cache
        //
        // This happens when there's no cached data or when we get new packaged data because of a
        // product update
        if let Some(packaged_data) = packaged_data {
            let cached_timestamp = inner
                .storage
                .get_last_modified_timestamp(&collection_url)?
                .unwrap_or(0);
            if packaged_data.timestamp > cached_timestamp {
                inner
                    .storage
                    .set_records(&collection_url, &packaged_data.data)?;
                return Ok(Some(self.filter_records(packaged_data.data)));
            }
        }

        let cached_records = inner.storage.get_records(&collection_url)?;

        Ok(match (cached_records, sync_if_empty) {
            // Case 2: We have cached records
            //
            // Note: we should return these even if it's an empty list and `sync_if_empty=true`.
            // The "if empty" part refers to the cache being empty, not the list.
            (Some(cached_records), _) => Some(self.filter_records(cached_records)),
            // Case 3: sync_if_empty=true
            (None, true) => {
                let records = inner.api_client.get_records(None)?;
                inner.storage.set_records(&collection_url, &records)?;
                Some(self.filter_records(records))
            }
            // Case 4: Nothing to return
            (None, false) => None,
        })
    }

    pub fn sync(&self) -> Result<()> {
        let mut inner = self.inner.lock();
        let collection_url = inner.api_client.collection_url();
        let mtime = inner.storage.get_last_modified_timestamp(&collection_url)?;
        let records = inner.api_client.get_records(mtime)?;
        inner.storage.merge_records(&collection_url, &records)
    }

    /// Downloads an attachment from [attachment_location]. NOTE: there are no guarantees about a
    /// maximum size, so use care when fetching potentially large attachments.
    pub fn get_attachment(&self, record: RemoteSettingsRecord) -> Result<Vec<u8>> {
        let metadata = record
            .attachment
            .ok_or_else(|| Error::RecordAttachmentMismatchError("No attachment metadata".into()))?;

        let mut inner = self.inner.lock();
        let collection_url = inner.api_client.collection_url();

        // First try storage - it will only return data that matches our metadata
        if let Some(data) = inner
            .storage
            .get_attachment(&collection_url, metadata.clone())?
        {
            return Ok(data);
        }

        // Then try packaged data if we're in prod
        if inner.api_client.is_prod_server()? {
            if let Some((data, manifest)) = self.load_packaged_attachment(&metadata.location) {
                if let Ok(manifest_data) = serde_json::from_str::<serde_json::Value>(manifest) {
                    if metadata.hash == manifest_data["hash"].as_str().unwrap_or_default()
                        && metadata.size == manifest_data["size"].as_u64().unwrap_or_default()
                    {
                        // Store valid packaged data in storage because it was either empty or outdated
                        inner
                            .storage
                            .set_attachment(&collection_url, &metadata.location, data)?;
                        return Ok(data.to_vec());
                    }
                }
            }
        }

        // Try to download the attachment because neither the storage nor the local data had it
        let attachment = inner.api_client.get_attachment(&metadata.location)?;

        // Verify downloaded data
        if attachment.len() as u64 != metadata.size {
            return Err(Error::RecordAttachmentMismatchError(
                "Downloaded attachment size mismatch".into(),
            ));
        }
        let hash = format!("{:x}", Sha256::digest(&attachment));
        if hash != metadata.hash {
            return Err(Error::RecordAttachmentMismatchError(
                "Downloaded attachment hash mismatch".into(),
            ));
        }

        // Store verified download in storage
        inner
            .storage
            .set_attachment(&collection_url, &metadata.location, &attachment)?;
        Ok(attachment)
    }
}

impl RemoteSettingsClient<ViaductApiClient> {
    pub fn new(
        server_url: Url,
        bucket_name: String,
        collection_name: String,
        #[cfg(feature = "jexl")] context: Option<RemoteSettingsContext>,
        storage: Storage,
    ) -> Result<Self> {
        let api_client = ViaductApiClient::new(server_url, &bucket_name, &collection_name)?;
        #[cfg(feature = "jexl")]
        let jexl_filter = JexlFilter::new(context);

        Ok(Self::new_from_parts(
            collection_name,
            storage,
            #[cfg(feature = "jexl")]
            jexl_filter,
            api_client,
        ))
    }

    pub fn update_config(&self, server_url: Url, bucket_name: String) -> Result<()> {
        let mut inner = self.inner.lock();
        inner.api_client = ViaductApiClient::new(server_url, &bucket_name, &self.collection_name)?;
        inner.storage.empty()
    }
}

#[cfg_attr(test, mockall::automock)]
pub trait ApiClient {
    /// Get the Bucket URL for this client.
    ///
    /// This is a URL that includes the server URL, bucket name, and collection name.  This is used
    /// to check if the application has switched the remote settings config and therefore we should
    /// throw away any cached data
    ///
    /// Returns it as a String, since that's what the storage expects
    fn collection_url(&self) -> String;

    /// Fetch records from the server
    fn get_records(&mut self, timestamp: Option<u64>) -> Result<Vec<RemoteSettingsRecord>>;

    /// Fetch an attachment from the server
    fn get_attachment(&mut self, attachment_location: &str) -> Result<Vec<u8>>;

    /// Check if this client is pointing to the production server
    fn is_prod_server(&self) -> Result<bool>;
}

/// Client for Remote settings API requests
pub struct ViaductApiClient {
    endpoints: RemoteSettingsEndpoints,
    remote_state: RemoteState,
}

impl ViaductApiClient {
    fn new(base_url: Url, bucket_name: &str, collection_name: &str) -> Result<Self> {
        Ok(Self {
            endpoints: RemoteSettingsEndpoints::new(&base_url, bucket_name, collection_name)?,
            remote_state: RemoteState::default(),
        })
    }

    fn make_request(&mut self, url: Url) -> Result<Response> {
        log::trace!("make_request: {url}");
        self.ensure_no_backoff()?;

        let req = Request::get(url);
        let resp = req.send()?;

        self.handle_backoff_hint(&resp)?;

        if resp.is_success() {
            Ok(resp)
        } else {
            Err(Error::ResponseError(format!(
                "status code: {}",
                resp.status
            )))
        }
    }

    fn ensure_no_backoff(&mut self) -> Result<()> {
        if let BackoffState::Backoff {
            observed_at,
            duration,
        } = self.remote_state.backoff
        {
            let elapsed_time = observed_at.elapsed();
            if elapsed_time >= duration {
                self.remote_state.backoff = BackoffState::Ok;
            } else {
                let remaining = duration - elapsed_time;
                return Err(Error::BackoffError(remaining.as_secs()));
            }
        }
        Ok(())
    }

    fn handle_backoff_hint(&mut self, response: &Response) -> Result<()> {
        let extract_backoff_header = |header| -> Result<u64> {
            Ok(response
                .headers
                .get_as::<u64, _>(header)
                .transpose()
                .unwrap_or_default() // Ignore number parsing errors.
                .unwrap_or(0))
        };
        // In practice these two headers are mutually exclusive.
        let backoff = extract_backoff_header(HEADER_BACKOFF)?;
        let retry_after = extract_backoff_header(HEADER_RETRY_AFTER)?;
        let max_backoff = backoff.max(retry_after);

        if max_backoff > 0 {
            self.remote_state.backoff = BackoffState::Backoff {
                observed_at: Instant::now(),
                duration: Duration::from_secs(max_backoff),
            };
        }
        Ok(())
    }
}

impl ApiClient for ViaductApiClient {
    fn collection_url(&self) -> String {
        self.endpoints.collection_url.to_string()
    }

    fn get_records(&mut self, timestamp: Option<u64>) -> Result<Vec<RemoteSettingsRecord>> {
        let mut url = self.endpoints.changeset_url.clone();
        // 0 is used as an arbitrary value for `_expected` because the current implementation does
        // not leverage push timestamps or polling from the monitor/changes endpoint. More
        // details:
        //
        // https://remote-settings.readthedocs.io/en/latest/client-specifications.html#cache-busting
        url.query_pairs_mut().append_pair("_expected", "0");
        if let Some(timestamp) = timestamp {
            url.query_pairs_mut()
                .append_pair("_since", &timestamp.to_string());
        }

        let resp = self.make_request(url)?;

        if resp.is_success() {
            Ok(resp.json::<ChangesetResponse>()?.changes)
        } else {
            Err(Error::ResponseError(format!(
                "status code: {}",
                resp.status
            )))
        }
    }

    fn get_attachment(&mut self, attachment_location: &str) -> Result<Vec<u8>> {
        let attachments_base_url = match &self.remote_state.attachments_base_url {
            Some(attachments_base_url) => attachments_base_url.to_owned(),
            None => {
                let server_info = self
                    .make_request(self.endpoints.root_url.clone())?
                    .json::<ServerInfo>()?;
                let attachments_base_url = match server_info.capabilities.attachments {
                    Some(capability) => Url::parse(&capability.base_url)?,
                    None => Err(Error::AttachmentsUnsupportedError)?,
                };
                self.remote_state.attachments_base_url = Some(attachments_base_url.clone());
                attachments_base_url
            }
        };

        let resp = self.make_request(attachments_base_url.join(attachment_location)?)?;
        Ok(resp.body)
    }

    fn is_prod_server(&self) -> Result<bool> {
        Ok(self
            .endpoints
            .root_url
            .as_str()
            .starts_with(RemoteSettingsServer::Prod.get_url()?.as_str()))
    }
}

/// A simple HTTP client that can retrieve Remote Settings data using the properties by [ClientConfig].
/// Methods defined on this will fetch data from
/// <base_url>/buckets/<bucket_name>/collections/<collection_name>/
pub struct Client {
    endpoints: RemoteSettingsEndpoints,
    pub(crate) remote_state: Mutex<RemoteState>,
}

impl Client {
    /// Create a new [Client] with properties matching config.
    pub fn new(config: RemoteSettingsConfig) -> Result<Self> {
        let server = match (config.server, config.server_url) {
            (Some(server), None) => server,
            (None, Some(server_url)) => RemoteSettingsServer::Custom { url: server_url },
            (None, None) => RemoteSettingsServer::Prod,
            (Some(_), Some(_)) => Err(Error::ConfigError(
                "`RemoteSettingsConfig` takes either `server` or `server_url`, not both".into(),
            ))?,
        };

        let bucket_name = config.bucket_name.unwrap_or_else(|| String::from("main"));
        let endpoints = RemoteSettingsEndpoints::new(
            &server.get_url()?,
            &bucket_name,
            &config.collection_name,
        )?;

        Ok(Self {
            endpoints,
            remote_state: Default::default(),
        })
    }

    /// Fetches all records for a collection that can be found in the server,
    /// bucket, and collection defined by the [ClientConfig] used to generate
    /// this [Client].
    pub fn get_records(&self) -> Result<RemoteSettingsResponse> {
        self.get_records_with_options(&GetItemsOptions::new())
    }

    /// Fetches all records for a collection that can be found in the server,
    /// bucket, and collection defined by the [ClientConfig] used to generate
    /// this [Client]. This function will return the raw network [Response].
    pub fn get_records_raw(&self) -> Result<Response> {
        self.get_records_raw_with_options(&GetItemsOptions::new())
    }

    /// Fetches all records that have been published since provided timestamp
    /// for a collection that can be found in the server, bucket, and
    /// collection defined by the [ClientConfig] used to generate this [Client].
    pub fn get_records_since(&self, timestamp: u64) -> Result<RemoteSettingsResponse> {
        self.get_records_with_options(
            GetItemsOptions::new().filter_gt("last_modified", timestamp.to_string()),
        )
    }

    /// Fetches records from this client's collection with the given options.
    pub fn get_records_with_options(
        &self,
        options: &GetItemsOptions,
    ) -> Result<RemoteSettingsResponse> {
        let resp = self.get_records_raw_with_options(options)?;
        let records = resp.json::<RecordsResponse>()?.data;
        let etag = resp
            .headers
            .get(HEADER_ETAG)
            .ok_or_else(|| Error::ResponseError("no etag header".into()))?;
        // Per https://docs.kinto-storage.org/en/stable/api/1.x/timestamps.html,
        // the `ETag` header value is a quoted integer. Trim the quotes before
        // parsing.
        let last_modified = etag.trim_matches('"').parse().map_err(|_| {
            Error::ResponseError(format!(
                "expected quoted integer in etag header; got `{}`",
                etag
            ))
        })?;
        Ok(RemoteSettingsResponse {
            records,
            last_modified,
        })
    }

    /// Fetches a raw network [Response] for records from this client's
    /// collection with the given options.
    pub fn get_records_raw_with_options(&self, options: &GetItemsOptions) -> Result<Response> {
        let mut url = self.endpoints.records_url.clone();
        for (name, value) in options.iter_query_pairs() {
            url.query_pairs_mut().append_pair(&name, &value);
        }
        self.make_request(url)
    }

    /// Downloads an attachment from [attachment_location]. NOTE: there are no
    /// guarantees about a maximum size, so use care when fetching potentially
    /// large attachments.
    pub fn get_attachment(&self, attachment_location: &str) -> Result<Vec<u8>> {
        Ok(self.get_attachment_raw(attachment_location)?.body)
    }

    /// Fetches a raw network [Response] for an attachment.
    pub fn get_attachment_raw(&self, attachment_location: &str) -> Result<Response> {
        // Important: We use a `let` binding here to ensure that the mutex is
        // unlocked immediately after cloning the URL. If we matched directly on
        // the `.lock()` expression, the mutex would stay locked until the end
        // of the `match`, causing a deadlock.
        let maybe_attachments_base_url = self.remote_state.lock().attachments_base_url.clone();

        let attachments_base_url = match maybe_attachments_base_url {
            Some(attachments_base_url) => attachments_base_url,
            None => {
                let server_info = self
                    .make_request(self.endpoints.root_url.clone())?
                    .json::<ServerInfo>()?;
                let attachments_base_url = match server_info.capabilities.attachments {
                    Some(capability) => Url::parse(&capability.base_url)?,
                    None => Err(Error::AttachmentsUnsupportedError)?,
                };
                self.remote_state.lock().attachments_base_url = Some(attachments_base_url.clone());
                attachments_base_url
            }
        };

        self.make_request(attachments_base_url.join(attachment_location)?)
    }

    fn make_request(&self, url: Url) -> Result<Response> {
        let mut current_remote_state = self.remote_state.lock();
        self.ensure_no_backoff(&mut current_remote_state.backoff)?;
        drop(current_remote_state);

        let req = Request::get(url);
        let resp = req.send()?;

        let mut current_remote_state = self.remote_state.lock();
        self.handle_backoff_hint(&resp, &mut current_remote_state.backoff)?;

        if resp.is_success() {
            Ok(resp)
        } else {
            Err(Error::ResponseError(format!(
                "status code: {}",
                resp.status
            )))
        }
    }

    fn ensure_no_backoff(&self, current_state: &mut BackoffState) -> Result<()> {
        if let BackoffState::Backoff {
            observed_at,
            duration,
        } = *current_state
        {
            let elapsed_time = observed_at.elapsed();
            if elapsed_time >= duration {
                *current_state = BackoffState::Ok;
            } else {
                let remaining = duration - elapsed_time;
                return Err(Error::BackoffError(remaining.as_secs()));
            }
        }
        Ok(())
    }

    fn handle_backoff_hint(
        &self,
        response: &Response,
        current_state: &mut BackoffState,
    ) -> Result<()> {
        let extract_backoff_header = |header| -> Result<u64> {
            Ok(response
                .headers
                .get_as::<u64, _>(header)
                .transpose()
                .unwrap_or_default() // Ignore number parsing errors.
                .unwrap_or(0))
        };
        // In practice these two headers are mutually exclusive.
        let backoff = extract_backoff_header(HEADER_BACKOFF)?;
        let retry_after = extract_backoff_header(HEADER_RETRY_AFTER)?;
        let max_backoff = backoff.max(retry_after);

        if max_backoff > 0 {
            *current_state = BackoffState::Backoff {
                observed_at: Instant::now(),
                duration: Duration::from_secs(max_backoff),
            };
        }
        Ok(())
    }
}

/// Stores all the endpoints for a Remote Settings server
///
/// There's actually not to many of these, so we can just pack them all into a struct
struct RemoteSettingsEndpoints {
    /// Root URL for Remote Settings server
    ///
    /// This has the form `[base-url]/`. It's where we get the attachment base url from.
    root_url: Url,
    /// URL for the collections endpoint
    ///
    /// This has the form:
    /// `[base-url]/buckets/[bucket-name]/collections/[collection-name]`.
    ///
    /// It can be used to fetch some metadata about the collection, but the real reason we use it
    /// is to get a URL that uniquely identifies the server + bucket name.  This is used by the
    /// [Storage] component to know when to throw away cached records because the user has changed
    /// one of these,
    collection_url: Url,
    /// URL for the changeset request
    ///
    /// This has the form:
    /// `[base-url]/buckets/[bucket-name]/collections/[collection-name]/changeset`.
    ///
    /// This is the URL for fetching records and changes to records
    changeset_url: Url,
    /// URL for the records request
    ///
    /// This has the form:
    /// `[base-url]/buckets/[bucket-name]/collections/[collection-name]/records`.
    ///
    /// This is the old/deprecated way to get records
    records_url: Url,
}

impl RemoteSettingsEndpoints {
    /// Construct a new RemoteSettingsEndpoints
    ///
    /// `base_url` should have the form `https://[domain]/v1` (no trailing slash).
    fn new(base_url: &Url, bucket_name: &str, collection_name: &str) -> Result<Self> {
        let mut root_url = base_url.clone();
        // Push the empty string to add the trailing slash.
        Self::path_segments_mut(&mut root_url)?.push("");

        let mut collection_url = base_url.clone();
        Self::path_segments_mut(&mut collection_url)?
            .push("buckets")
            .push(bucket_name)
            .push("collections")
            .push(collection_name);

        let mut records_url = collection_url.clone();
        Self::path_segments_mut(&mut records_url)?.push("records");

        let mut changeset_url = collection_url.clone();
        Self::path_segments_mut(&mut changeset_url)?.push("changeset");

        Ok(Self {
            root_url,
            collection_url,
            records_url,
            changeset_url,
        })
    }

    /// Utility method for calling [Url::path_segments_mut]
    ///
    /// The issue we're working around is that path_segments_mut uses `()` as the error type, which
    /// can't be converted into our `Error` type.
    fn path_segments_mut(url: &mut Url) -> Result<url::PathSegmentsMut<'_>> {
        url.path_segments_mut()
            // path_segments_mut uses `()` as the error type, but the docs say that it only will
            // error for cannot-be-a-base URLs.
            .map_err(|_| Error::UrlParsingError(url::ParseError::RelativeUrlWithCannotBeABaseBase))
    }
}

/// Data structure representing the top-level response from the Remote Settings.
/// [last_modified] will be extracted from the etag header of the response.
#[derive(Clone, Debug, Eq, PartialEq, Deserialize, Serialize, uniffi::Record)]
pub struct RemoteSettingsResponse {
    pub records: Vec<RemoteSettingsRecord>,
    pub last_modified: u64,
}

#[derive(Deserialize, Serialize)]
struct RecordsResponse {
    data: Vec<RemoteSettingsRecord>,
}

#[derive(Deserialize, Serialize)]
struct ChangesetResponse {
    changes: Vec<RemoteSettingsRecord>,
}

/// A parsed Remote Settings record. Records can contain arbitrary fields, so clients
/// are required to further extract expected values from the [fields] member.
#[derive(Clone, Debug, Deserialize, Serialize, Eq, PartialEq, uniffi::Record)]
pub struct RemoteSettingsRecord {
    pub id: String,
    pub last_modified: u64,
    #[serde(default)]
    pub deleted: bool,
    pub attachment: Option<Attachment>,
    #[serde(flatten)]
    pub fields: RsJsonObject,
}

/// Attachment metadata that can be optionally attached to a [Record]. The [location] should
/// included in calls to [Client::get_attachment].
#[derive(Clone, Debug, Default, Deserialize, Serialize, Eq, PartialEq, uniffi::Record)]
pub struct Attachment {
    pub filename: String,
    pub mimetype: String,
    pub location: String,
    pub hash: String,
    pub size: u64,
}

// Define a UniFFI custom types to pass JSON objects across the FFI as a string
//
// This is named `RsJsonObject` because, UniFFI cannot currently rename iOS bindings and JsonObject
// conflicted with the declaration in Nimbus. This shouldn't really impact Android, since the type
// is converted into the platform JsonObject thanks to the UniFFI binding.
pub type RsJsonObject = serde_json::Map<String, serde_json::Value>;
uniffi::custom_type!(RsJsonObject, String);

impl UniffiCustomTypeConverter for RsJsonObject {
    type Builtin = String;
    fn into_custom(val: Self::Builtin) -> uniffi::Result<Self> {
        let json: serde_json::Value = serde_json::from_str(&val)?;

        match json {
            serde_json::Value::Object(obj) => Ok(obj),
            _ => Err(uniffi::deps::anyhow::anyhow!(
                "Unexpected JSON-non-object in the bagging area"
            )),
        }
    }

    fn from_custom(obj: Self) -> Self::Builtin {
        serde_json::Value::Object(obj).to_string()
    }
}

#[derive(Clone, Debug)]
pub(crate) struct RemoteState {
    attachments_base_url: Option<Url>,
    backoff: BackoffState,
}

impl Default for RemoteState {
    fn default() -> Self {
        Self {
            attachments_base_url: None,
            backoff: BackoffState::Ok,
        }
    }
}

/// Used in handling backoff responses from the Remote Settings server.
#[derive(Clone, Copy, Debug)]
pub(crate) enum BackoffState {
    Ok,
    Backoff {
        observed_at: Instant,
        duration: Duration,
    },
}

#[derive(Deserialize)]
struct ServerInfo {
    capabilities: Capabilities,
}

#[derive(Deserialize)]
struct Capabilities {
    attachments: Option<AttachmentsCapability>,
}

#[derive(Deserialize)]
struct AttachmentsCapability {
    base_url: String,
}

/// Options for requests to endpoints that return multiple items.
#[derive(Clone, Debug, Default, Hash, PartialEq, Eq, PartialOrd, Ord)]
pub struct GetItemsOptions {
    filters: Vec<Filter>,
    sort: Vec<Sort>,
    fields: Vec<String>,
    limit: Option<u64>,
}

impl GetItemsOptions {
    /// Creates an empty option set.
    pub fn new() -> Self {
        Self::default()
    }

    /// Sets an option to only return items whose `field` is equal to the given
    /// `value`.
    ///
    /// `field` can be a simple or dotted field name, like `author` or
    /// `author.name`. `value` can be a bare number or string (like
    /// `2` or `Ben`), or a stringified JSON value (`"2.0"`, `[1, 2]`,
    /// `{"checked": true}`).
    pub fn filter_eq(&mut self, field: impl Into<String>, value: impl Into<String>) -> &mut Self {
        self.filters.push(Filter::Eq(field.into(), value.into()));
        self
    }

    /// Sets an option to only return items whose `field` is not equal to the
    /// given `value`.
    pub fn filter_not(&mut self, field: impl Into<String>, value: impl Into<String>) -> &mut Self {
        self.filters.push(Filter::Not(field.into(), value.into()));
        self
    }

    /// Sets an option to only return items whose `field` is an array that
    /// contains the given `value`. If `value` is a stringified JSON array, the
    /// field must contain all its elements.
    pub fn filter_contains(
        &mut self,
        field: impl Into<String>,
        value: impl Into<String>,
    ) -> &mut Self {
        self.filters
            .push(Filter::Contains(field.into(), value.into()));
        self
    }

    /// Sets an option to only return items whose `field` is strictly less
    /// than the given `value`.
    pub fn filter_lt(&mut self, field: impl Into<String>, value: impl Into<String>) -> &mut Self {
        self.filters.push(Filter::Lt(field.into(), value.into()));
        self
    }

    /// Sets an option to only return items whose `field` is strictly greater
    /// than the given `value`.
    pub fn filter_gt(&mut self, field: impl Into<String>, value: impl Into<String>) -> &mut Self {
        self.filters.push(Filter::Gt(field.into(), value.into()));
        self
    }

    /// Sets an option to only return items whose `field` is less than or equal
    /// to the given `value`.
    pub fn filter_max(&mut self, field: impl Into<String>, value: impl Into<String>) -> &mut Self {
        self.filters.push(Filter::Max(field.into(), value.into()));
        self
    }

    /// Sets an option to only return items whose `field` is greater than or
    /// equal to the given `value`.
    pub fn filter_min(&mut self, field: impl Into<String>, value: impl Into<String>) -> &mut Self {
        self.filters.push(Filter::Min(field.into(), value.into()));
        self
    }

    /// Sets an option to only return items whose `field` is a string that
    /// contains the substring `value`. `value` can contain `*` wildcards.
    pub fn filter_like(&mut self, field: impl Into<String>, value: impl Into<String>) -> &mut Self {
        self.filters.push(Filter::Like(field.into(), value.into()));
        self
    }

    /// Sets an option to only return items that have the given `field`.
    pub fn filter_has(&mut self, field: impl Into<String>) -> &mut Self {
        self.filters.push(Filter::Has(field.into()));
        self
    }

    /// Sets an option to only return items that do not have the given `field`.
    pub fn filter_has_not(&mut self, field: impl Into<String>) -> &mut Self {
        self.filters.push(Filter::HasNot(field.into()));
        self
    }

    /// Sets an option to return items in `order` for the given `field`.
    pub fn sort(&mut self, field: impl Into<String>, order: SortOrder) -> &mut Self {
        self.sort.push(Sort(field.into(), order));
        self
    }

    /// Sets an option to only return the given `field` of each item.
    ///
    /// The special `id` and `last_modified` fields are always returned.
    pub fn field(&mut self, field: impl Into<String>) -> &mut Self {
        self.fields.push(field.into());
        self
    }

    /// Sets the option to return at most `count` items.
    pub fn limit(&mut self, count: u64) -> &mut Self {
        self.limit = Some(count);
        self
    }

    /// Returns an iterator of (name, value) query pairs for these options.
    pub fn iter_query_pairs(&self) -> impl Iterator<Item = (Cow<str>, Cow<str>)> {
        self.filters
            .iter()
            .map(Filter::as_query_pair)
            .chain({
                // For sorting (https://docs.kinto-storage.org/en/latest/api/1.x/sorting.html),
                // the query pair syntax is `_sort=field1,-field2`, where the
                // fields to sort by are specified in a comma-separated ordered
                // list, and `-` indicates descending order.
                (!self.sort.is_empty()).then(|| {
                    (
                        "_sort".into(),
                        (self
                            .sort
                            .iter()
                            .map(Sort::as_query_value)
                            .collect::<Vec<_>>()
                            .join(","))
                        .into(),
                    )
                })
            })
            .chain({
                // For selecting fields (https://docs.kinto-storage.org/en/latest/api/1.x/selecting_fields.html),
                // the query pair syntax is `_fields=field1,field2`.
                (!self.fields.is_empty()).then(|| ("_fields".into(), self.fields.join(",").into()))
            })
            .chain({
                // For pagination (https://docs.kinto-storage.org/en/latest/api/1.x/pagination.html),
                // the query pair syntax is `_limit={count}`.
                self.limit
                    .map(|count| ("_limit".into(), count.to_string().into()))
            })
    }
}

/// The order in which to return items.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq, PartialOrd, Ord)]
pub enum SortOrder {
    /// Smaller values first.
    Ascending,
    /// Larger values first.
    Descending,
}

#[derive(Clone, Debug, Hash, PartialEq, Eq, PartialOrd, Ord)]
enum Filter {
    Eq(String, String),
    Not(String, String),
    Contains(String, String),
    Lt(String, String),
    Gt(String, String),
    Max(String, String),
    Min(String, String),
    Like(String, String),
    Has(String),
    HasNot(String),
}

impl Filter {
    fn as_query_pair(&self) -> (Cow<str>, Cow<str>) {
        // For filters (https://docs.kinto-storage.org/en/latest/api/1.x/filtering.html),
        // the query pair syntax is `[operator_]field=value` for each field.
        match self {
            Filter::Eq(field, value) => (field.into(), value.into()),
            Filter::Not(field, value) => (format!("not_{field}").into(), value.into()),
            Filter::Contains(field, value) => (format!("contains_{field}").into(), value.into()),
            Filter::Lt(field, value) => (format!("lt_{field}").into(), value.into()),
            Filter::Gt(field, value) => (format!("gt_{field}").into(), value.into()),
            Filter::Max(field, value) => (format!("max_{field}").into(), value.into()),
            Filter::Min(field, value) => (format!("min_{field}").into(), value.into()),
            Filter::Like(field, value) => (format!("like_{field}").into(), value.into()),
            Filter::Has(field) => (format!("has_{field}").into(), "true".into()),
            Filter::HasNot(field) => (format!("has_{field}").into(), "false".into()),
        }
    }
}

#[derive(Clone, Debug, Hash, PartialEq, Eq, PartialOrd, Ord)]
struct Sort(String, SortOrder);

impl Sort {
    fn as_query_value(&self) -> Cow<str> {
        match self.1 {
            SortOrder::Ascending => self.0.as_str().into(),
            SortOrder::Descending => format!("-{}", self.0).into(),
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use expect_test::expect;
    use mockito::{mock, Matcher};
    #[test]
    fn test_defaults() {
        let config = RemoteSettingsConfig {
            server: None,
            server_url: None,
            bucket_name: None,
            collection_name: String::from("the-collection"),
        };
        let client = Client::new(config).unwrap();
        assert_eq!(
            Url::parse("https://firefox.settings.services.mozilla.com/v1/buckets/main/collections/the-collection").unwrap(),
            client.endpoints.collection_url
        );
    }

    #[test]
    fn test_deprecated_server_url() {
        let config = RemoteSettingsConfig {
            server: None,
            server_url: Some("https://example.com".into()),
            bucket_name: None,
            collection_name: String::from("the-collection"),
        };
        let client = Client::new(config).unwrap();
        assert_eq!(
            Url::parse("https://example.com/v1/buckets/main/collections/the-collection").unwrap(),
            client.endpoints.collection_url
        );
    }

    #[test]
    fn test_invalid_config() {
        let config = RemoteSettingsConfig {
            server: Some(RemoteSettingsServer::Prod),
            server_url: Some("https://example.com".into()),
            bucket_name: None,
            collection_name: String::from("the-collection"),
        };
        match Client::new(config) {
            Ok(_) => panic!("Wanted config error; got client"),
            Err(Error::ConfigError(_)) => {}
            Err(err) => panic!("Wanted config error; got {}", err),
        }
    }

    #[test]
    fn test_attachment_can_be_downloaded() {
        viaduct_reqwest::use_reqwest_backend();
        let server_info_m = mock("GET", "/v1/")
            .with_body(attachment_metadata(mockito::server_url()))
            .with_status(200)
            .with_header("content-type", "application/json")
            .create();

        let attachment_location = "123.jpg";
        let attachment_bytes: Vec<u8> = "I'm a JPG, I swear".into();
        let attachment_m = mock(
            "GET",
            format!("/attachments/{}", attachment_location).as_str(),
        )
        .with_body(attachment_bytes.clone())
        .with_status(200)
        .with_header("content-type", "application/json")
        .create();

        let config = RemoteSettingsConfig {
            server: Some(RemoteSettingsServer::Custom {
                url: mockito::server_url(),
            }),
            server_url: None,
            collection_name: String::from("the-collection"),
            bucket_name: None,
        };

        let client = Client::new(config).unwrap();
        let first_resp = client.get_attachment(attachment_location).unwrap();
        let second_resp = client.get_attachment(attachment_location).unwrap();

        server_info_m.expect(1).assert();
        attachment_m.expect(2).assert();
        assert_eq!(first_resp, attachment_bytes);
        assert_eq!(second_resp, attachment_bytes);
    }

    #[test]
    fn test_attachment_errors_if_server_not_configured_for_attachments() {
        viaduct_reqwest::use_reqwest_backend();
        let server_info_m = mock("GET", "/v1/")
            .with_body(NO_ATTACHMENTS_METADATA)
            .with_status(200)
            .with_header("content-type", "application/json")
            .create();

        let attachment_location = "123.jpg";
        let attachment_bytes: Vec<u8> = "I'm a JPG, I swear".into();
        let attachment_m = mock(
            "GET",
            format!("/attachments/{}", attachment_location).as_str(),
        )
        .with_body(attachment_bytes)
        .with_status(200)
        .with_header("content-type", "application/json")
        .create();

        let config = RemoteSettingsConfig {
            server: Some(RemoteSettingsServer::Custom {
                url: mockito::server_url(),
            }),
            server_url: None,
            collection_name: String::from("the-collection"),
            bucket_name: None,
        };

        let client = Client::new(config).unwrap();
        let resp = client.get_attachment(attachment_location);
        server_info_m.expect(1).assert();
        attachment_m.expect(0).assert();
        assert!(matches!(resp, Err(Error::AttachmentsUnsupportedError)))
    }

    #[test]
    fn test_backoff() {
        viaduct_reqwest::use_reqwest_backend();
        let m = mock(
            "GET",
            "/v1/buckets/the-bucket/collections/the-collection/records",
        )
        .with_body(response_body())
        .with_status(200)
        .with_header("content-type", "application/json")
        .with_header("Backoff", "60")
        .with_header("etag", "\"1000\"")
        .create();
        let config = RemoteSettingsConfig {
            server: Some(RemoteSettingsServer::Custom {
                url: mockito::server_url(),
            }),
            server_url: None,
            collection_name: String::from("the-collection"),
            bucket_name: Some(String::from("the-bucket")),
        };
        let http_client = Client::new(config).unwrap();

        assert!(http_client.get_records().is_ok());
        let second_resp = http_client.get_records();
        assert!(matches!(second_resp, Err(Error::BackoffError(_))));
        m.expect(1).assert();
    }

    #[test]
    fn test_500_retry_after() {
        viaduct_reqwest::use_reqwest_backend();
        let m = mock(
            "GET",
            "/v1/buckets/the-bucket/collections/the-collection/records",
        )
        .with_body("Boom!")
        .with_status(500)
        .with_header("Retry-After", "60")
        .create();
        let config = RemoteSettingsConfig {
            server: Some(RemoteSettingsServer::Custom {
                url: mockito::server_url(),
            }),
            server_url: None,
            collection_name: String::from("the-collection"),
            bucket_name: Some(String::from("the-bucket")),
        };
        let http_client = Client::new(config).unwrap();
        assert!(http_client.get_records().is_err());
        let second_request = http_client.get_records();
        assert!(matches!(second_request, Err(Error::BackoffError(_))));
        m.expect(1).assert();
    }

    #[test]
    fn test_options() {
        viaduct_reqwest::use_reqwest_backend();
        let m = mock(
            "GET",
            "/v1/buckets/the-bucket/collections/the-collection/records",
        )
        .match_query(Matcher::AllOf(vec![
            Matcher::UrlEncoded("a".into(), "b".into()),
            Matcher::UrlEncoded("lt_c.d".into(), "5".into()),
            Matcher::UrlEncoded("gt_e".into(), "15".into()),
            Matcher::UrlEncoded("max_f".into(), "20".into()),
            Matcher::UrlEncoded("min_g".into(), "10".into()),
            Matcher::UrlEncoded("not_h".into(), "i".into()),
            Matcher::UrlEncoded("like_j".into(), "*k*".into()),
            Matcher::UrlEncoded("has_l".into(), "true".into()),
            Matcher::UrlEncoded("has_m".into(), "false".into()),
            Matcher::UrlEncoded("contains_n".into(), "o".into()),
            Matcher::UrlEncoded("_sort".into(), "-b,a".into()),
            Matcher::UrlEncoded("_fields".into(), "a,c,b".into()),
            Matcher::UrlEncoded("_limit".into(), "3".into()),
        ]))
        .with_body(response_body())
        .with_status(200)
        .with_header("content-type", "application/json")
        .with_header("etag", "\"1000\"")
        .create();
        let config = RemoteSettingsConfig {
            server: Some(RemoteSettingsServer::Custom {
                url: mockito::server_url(),
            }),
            server_url: None,
            collection_name: String::from("the-collection"),
            bucket_name: Some(String::from("the-bucket")),
        };
        let http_client = Client::new(config).unwrap();
        let mut options = GetItemsOptions::new();
        options
            .field("a")
            .field("c")
            .field("b")
            .filter_eq("a", "b")
            .filter_lt("c.d", "5")
            .filter_gt("e", "15")
            .filter_max("f", "20")
            .filter_min("g", "10")
            .filter_not("h", "i")
            .filter_like("j", "*k*")
            .filter_has("l")
            .filter_has_not("m")
            .filter_contains("n", "o")
            .sort("b", SortOrder::Descending)
            .sort("a", SortOrder::Ascending)
            .limit(3);

        assert!(http_client.get_records_raw_with_options(&options).is_ok());
        expect![[r#"
            RemoteSettingsResponse {
                records: [
                    RemoteSettingsRecord {
                        id: "c5dcd1da-7126-4abb-846b-ec85b0d4d0d7",
                        last_modified: 1677694949407,
                        deleted: false,
                        attachment: Some(
                            Attachment {
                                filename: "jgp-attachment.jpg",
                                mimetype: "image/jpeg",
                                location: "the-bucket/the-collection/d3a5eccc-f0ca-42c3-b0bb-c0d4408c21c9.jpg",
                                hash: "2cbd593f3fd5f1585f92265433a6696a863bc98726f03e7222135ff0d8e83543",
                                size: 1374325,
                            },
                        ),
                        fields: {
                            "title": String(
                                "jpg-attachment",
                            ),
                            "content": String(
                                "content",
                            ),
                            "schema": Number(
                                1677694447771,
                            ),
                        },
                    },
                    RemoteSettingsRecord {
                        id: "ff301910-6bf5-4cfe-bc4c-5c80308661a5",
                        last_modified: 1677694470354,
                        deleted: false,
                        attachment: Some(
                            Attachment {
                                filename: "pdf-attachment.pdf",
                                mimetype: "application/pdf",
                                location: "the-bucket/the-collection/5f7347c2-af92-411d-a65b-f794f9b5084c.pdf",
                                hash: "de1cde3571ef3faa77ea0493276de9231acaa6f6651602e93aa1036f51181e9b",
                                size: 157,
                            },
                        ),
                        fields: {
                            "title": String(
                                "with-attachment",
                            ),
                            "content": String(
                                "content",
                            ),
                            "schema": Number(
                                1677694447771,
                            ),
                        },
                    },
                    RemoteSettingsRecord {
                        id: "7403c6f9-79be-4e0c-a37a-8f2b5bd7ad58",
                        last_modified: 1677694455368,
                        deleted: false,
                        attachment: None,
                        fields: {
                            "title": String(
                                "no-attachment",
                            ),
                            "content": String(
                                "content",
                            ),
                            "schema": Number(
                                1677694447771,
                            ),
                        },
                    },
                    RemoteSettingsRecord {
                        id: "9320f53c-0a39-4997-9120-62ff597ffb26",
                        last_modified: 1690921847416,
                        deleted: true,
                        attachment: None,
                        fields: {},
                    },
                ],
                last_modified: 1000,
            }
        "#]].assert_debug_eq(&http_client
            .get_records_with_options(&options)
            .unwrap());
        m.expect(2).assert();
    }

    #[test]
    fn test_backoff_recovery() {
        viaduct_reqwest::use_reqwest_backend();
        let m = mock(
            "GET",
            "/v1/buckets/the-bucket/collections/the-collection/records",
        )
        .with_body(response_body())
        .with_status(200)
        .with_header("content-type", "application/json")
        .with_header("etag", "\"1000\"")
        .create();
        let config = RemoteSettingsConfig {
            server: Some(RemoteSettingsServer::Custom {
                url: mockito::server_url(),
            }),
            server_url: None,
            collection_name: String::from("the-collection"),
            bucket_name: Some(String::from("the-bucket")),
        };
        let http_client = Client::new(config).unwrap();
        // First, sanity check that manipulating the remote state does something.
        let mut current_remote_state = http_client.remote_state.lock();
        current_remote_state.backoff = BackoffState::Backoff {
            observed_at: Instant::now(),
            duration: Duration::from_secs(30),
        };
        drop(current_remote_state);
        assert!(matches!(
            http_client.get_records(),
            Err(Error::BackoffError(_))
        ));
        // Then do the actual test.
        let mut current_remote_state = http_client.remote_state.lock();
        current_remote_state.backoff = BackoffState::Backoff {
            observed_at: Instant::now() - Duration::from_secs(31),
            duration: Duration::from_secs(30),
        };
        drop(current_remote_state);
        assert!(http_client.get_records().is_ok());
        m.expect(1).assert();
    }

    #[test]
    fn test_record_fields() {
        viaduct_reqwest::use_reqwest_backend();
        let m = mock(
            "GET",
            "/v1/buckets/the-bucket/collections/the-collection/records",
        )
        .with_body(response_body())
        .with_status(200)
        .with_header("content-type", "application/json")
        .with_header("etag", "\"1000\"")
        .create();
        let config = RemoteSettingsConfig {
            server: Some(RemoteSettingsServer::Custom {
                url: mockito::server_url(),
            }),
            server_url: None,
            collection_name: String::from("the-collection"),
            bucket_name: Some(String::from("the-bucket")),
        };
        let http_client = Client::new(config).unwrap();
        let response = http_client.get_records().unwrap();
        expect![[r#"
            RemoteSettingsResponse {
                records: [
                    RemoteSettingsRecord {
                        id: "c5dcd1da-7126-4abb-846b-ec85b0d4d0d7",
                        last_modified: 1677694949407,
                        deleted: false,
                        attachment: Some(
                            Attachment {
                                filename: "jgp-attachment.jpg",
                                mimetype: "image/jpeg",
                                location: "the-bucket/the-collection/d3a5eccc-f0ca-42c3-b0bb-c0d4408c21c9.jpg",
                                hash: "2cbd593f3fd5f1585f92265433a6696a863bc98726f03e7222135ff0d8e83543",
                                size: 1374325,
                            },
                        ),
                        fields: {
                            "title": String(
                                "jpg-attachment",
                            ),
                            "content": String(
                                "content",
                            ),
                            "schema": Number(
                                1677694447771,
                            ),
                        },
                    },
                    RemoteSettingsRecord {
                        id: "ff301910-6bf5-4cfe-bc4c-5c80308661a5",
                        last_modified: 1677694470354,
                        deleted: false,
                        attachment: Some(
                            Attachment {
                                filename: "pdf-attachment.pdf",
                                mimetype: "application/pdf",
                                location: "the-bucket/the-collection/5f7347c2-af92-411d-a65b-f794f9b5084c.pdf",
                                hash: "de1cde3571ef3faa77ea0493276de9231acaa6f6651602e93aa1036f51181e9b",
                                size: 157,
                            },
                        ),
                        fields: {
                            "title": String(
                                "with-attachment",
                            ),
                            "content": String(
                                "content",
                            ),
                            "schema": Number(
                                1677694447771,
                            ),
                        },
                    },
                    RemoteSettingsRecord {
                        id: "7403c6f9-79be-4e0c-a37a-8f2b5bd7ad58",
                        last_modified: 1677694455368,
                        deleted: false,
                        attachment: None,
                        fields: {
                            "title": String(
                                "no-attachment",
                            ),
                            "content": String(
                                "content",
                            ),
                            "schema": Number(
                                1677694447771,
                            ),
                        },
                    },
                    RemoteSettingsRecord {
                        id: "9320f53c-0a39-4997-9120-62ff597ffb26",
                        last_modified: 1690921847416,
                        deleted: true,
                        attachment: None,
                        fields: {},
                    },
                ],
                last_modified: 1000,
            }
        "#]].assert_debug_eq(&response);
        m.expect(1).assert();
    }

    #[test]
    fn test_missing_etag() {
        viaduct_reqwest::use_reqwest_backend();
        let m = mock(
            "GET",
            "/v1/buckets/the-bucket/collections/the-collection/records",
        )
        .with_body(response_body())
        .with_status(200)
        .with_header("content-type", "application/json")
        .create();

        let config = RemoteSettingsConfig {
            server: Some(RemoteSettingsServer::Custom {
                url: mockito::server_url(),
            }),
            server_url: None,
            bucket_name: Some(String::from("the-bucket")),
            collection_name: String::from("the-collection"),
        };
        let client = Client::new(config).unwrap();

        let err = client.get_records().unwrap_err();
        assert!(
            matches!(err, Error::ResponseError(_)),
            "Want response error for missing `ETag`; got {}",
            err
        );
        m.expect(1).assert();
    }

    #[test]
    fn test_invalid_etag() {
        viaduct_reqwest::use_reqwest_backend();
        let m = mock(
            "GET",
            "/v1/buckets/the-bucket/collections/the-collection/records",
        )
        .with_body(response_body())
        .with_status(200)
        .with_header("content-type", "application/json")
        .with_header("etag", "bad!")
        .create();

        let config = RemoteSettingsConfig {
            server: Some(RemoteSettingsServer::Custom {
                url: mockito::server_url(),
            }),
            server_url: None,
            bucket_name: Some(String::from("the-bucket")),
            collection_name: String::from("the-collection"),
        };
        let client = Client::new(config).unwrap();

        let err = client.get_records().unwrap_err();
        assert!(
            matches!(err, Error::ResponseError(_)),
            "Want response error for invalid `ETag`; got {}",
            err
        );
        m.expect(1).assert();
    }

    fn attachment_metadata(base_url: String) -> String {
        format!(
            r#"
            {{
                "capabilities": {{
                    "admin": {{
                        "description": "Serves the admin console.",
                        "url": "https://github.com/Kinto/kinto-admin/",
                        "version": "2.0.0"
                    }},
                    "attachments": {{
                        "description": "Add file attachments to records",
                        "url": "https://github.com/Kinto/kinto-attachment/",
                        "version": "6.3.1",
                        "base_url": "{}/attachments/"
                    }}
                }}
            }}
    "#,
            base_url
        )
    }

    const NO_ATTACHMENTS_METADATA: &str = r#"
    {
      "capabilities": {
          "admin": {
            "description": "Serves the admin console.",
            "url": "https://github.com/Kinto/kinto-admin/",
            "version": "2.0.0"
          }
      }
    }
  "#;

    fn response_body() -> String {
        format!(
            r#"
        {{
            "data": [
                {},
                {},
                {},
                {}
            ]
          }}"#,
            JPG_ATTACHMENT, PDF_ATTACHMENT, NO_ATTACHMENT, TOMBSTONE
        )
    }

    const JPG_ATTACHMENT: &str = r#"
    {
      "title": "jpg-attachment",
      "content": "content",
      "attachment": {
          "filename": "jgp-attachment.jpg",
          "location": "the-bucket/the-collection/d3a5eccc-f0ca-42c3-b0bb-c0d4408c21c9.jpg",
          "hash": "2cbd593f3fd5f1585f92265433a6696a863bc98726f03e7222135ff0d8e83543",
          "mimetype": "image/jpeg",
          "size": 1374325
      },
      "id": "c5dcd1da-7126-4abb-846b-ec85b0d4d0d7",
      "schema": 1677694447771,
      "last_modified": 1677694949407
    }
  "#;

    const PDF_ATTACHMENT: &str = r#"
    {
      "title": "with-attachment",
      "content": "content",
      "attachment": {
          "filename": "pdf-attachment.pdf",
          "location": "the-bucket/the-collection/5f7347c2-af92-411d-a65b-f794f9b5084c.pdf",
          "hash": "de1cde3571ef3faa77ea0493276de9231acaa6f6651602e93aa1036f51181e9b",
          "mimetype": "application/pdf",
          "size": 157
      },
      "id": "ff301910-6bf5-4cfe-bc4c-5c80308661a5",
      "schema": 1677694447771,
      "last_modified": 1677694470354
    }
  "#;

    const NO_ATTACHMENT: &str = r#"
      {
        "title": "no-attachment",
        "content": "content",
        "schema": 1677694447771,
        "id": "7403c6f9-79be-4e0c-a37a-8f2b5bd7ad58",
        "last_modified": 1677694455368
      }
    "#;

    const TOMBSTONE: &str = r#"
    {
      "id": "9320f53c-0a39-4997-9120-62ff597ffb26",
      "last_modified": 1690921847416,
      "deleted": true
    }
  "#;
}

#[cfg(test)]
mod test_new_client {
    use super::*;

    #[cfg(not(feature = "jexl"))]
    use serde_json::json;

    #[test]
    fn test_endpoints() {
        let endpoints = RemoteSettingsEndpoints::new(
            &Url::parse("http://rs.example.com/v1").unwrap(),
            "main",
            "test-collection",
        )
        .unwrap();
        assert_eq!(endpoints.root_url.to_string(), "http://rs.example.com/v1/");
        assert_eq!(
            endpoints.collection_url.to_string(),
            "http://rs.example.com/v1/buckets/main/collections/test-collection",
        );
        assert_eq!(
            endpoints.records_url.to_string(),
            "http://rs.example.com/v1/buckets/main/collections/test-collection/records",
        );
        assert_eq!(
            endpoints.changeset_url.to_string(),
            "http://rs.example.com/v1/buckets/main/collections/test-collection/changeset",
        );
    }

    #[test]
    #[cfg(not(feature = "jexl"))]
    fn test_get_records_none_cached() {
        let mut api_client = MockApiClient::new();
        api_client.expect_collection_url().returning(|| {
            "http://rs.example.com/v1/buckets/main/collections/test-collection".into()
        });
        api_client.expect_is_prod_server().returning(|| Ok(false));

        // Note, don't make any api_client.expect_*() calls, the RemoteSettingsClient should not
        // attempt to make any requests for this scenario
        let storage = Storage::new(":memory:".into()).expect("Error creating storage");

        let rs_client =
            RemoteSettingsClient::new_from_parts("test-collection".into(), storage, api_client);
        assert_eq!(
            rs_client.get_records(false).expect("Error getting records"),
            None
        );
    }

    #[test]
    #[cfg(not(feature = "jexl"))]
    fn test_get_records_none_cached_sync_with_empty() {
        let mut api_client = MockApiClient::new();
        let records = vec![RemoteSettingsRecord {
            id: "record-0001".into(),
            last_modified: 100,
            deleted: false,
            attachment: None,
            fields: json!({"foo": "bar"}).as_object().unwrap().clone(),
        }];
        api_client.expect_collection_url().returning(|| {
            "http://rs.example.com/v1/buckets/main/collections/test-collection".into()
        });
        api_client.expect_get_records().returning({
            let records = records.clone();
            move |timestamp| {
                assert_eq!(timestamp, None);
                Ok(records.clone())
            }
        });
        api_client.expect_is_prod_server().returning(|| Ok(false));
        let storage = Storage::new(":memory:".into()).expect("Error creating storage");

        let rs_client =
            RemoteSettingsClient::new_from_parts("test-collection".into(), storage, api_client);

        assert_eq!(
            rs_client.get_records(true).expect("Error getting records"),
            Some(records)
        );
    }
}

#[cfg(feature = "jexl")]
#[cfg(test)]
mod jexl_tests {
    use super::*;

    #[test]
    fn test_get_records_filtered_app_version_pass() {
        let mut api_client = MockApiClient::new();
        let records = vec![RemoteSettingsRecord {
            id: "record-0001".into(),
            last_modified: 100,
            deleted: false,
            attachment: None,
            fields: serde_json::json!({
                "filter_expression": "env.version|versionCompare(\"128.0a1\") > 0"
            })
            .as_object()
            .unwrap()
            .clone(),
        }];
        api_client.expect_collection_url().returning(|| {
            "http://rs.example.com/v1/buckets/main/collections/test-collection".into()
        });
        api_client.expect_get_records().returning({
            let records = records.clone();
            move |timestamp| {
                assert_eq!(timestamp, None);
                Ok(records.clone())
            }
        });
        api_client.expect_is_prod_server().returning(|| Ok(false));

        let context = RemoteSettingsContext {
            app_version: Some("129.0.0".to_string()),
            ..Default::default()
        };

        let mut storage = Storage::new(":memory:".into()).expect("Error creating storage");
        let _ = storage.set_records(
            "http://rs.example.com/v1/buckets/main/collections/test-collection",
            &records,
        );

        let rs_client = RemoteSettingsClient::new_from_parts(
            "test-collection".into(),
            storage,
            JexlFilter::new(Some(context)),
            api_client,
        );

        assert_eq!(
            rs_client.get_records(false).expect("Error getting records"),
            Some(records)
        );
    }

    #[test]
    fn test_get_records_filtered_app_version_too_low() {
        let mut api_client = MockApiClient::new();
        let records = vec![RemoteSettingsRecord {
            id: "record-0001".into(),
            last_modified: 100,
            deleted: false,
            attachment: None,
            fields: serde_json::json!({
                "filter_expression": "env.version|versionCompare(\"128.0a1\") > 0"
            })
            .as_object()
            .unwrap()
            .clone(),
        }];
        api_client.expect_collection_url().returning(|| {
            "http://rs.example.com/v1/buckets/main/collections/test-collection".into()
        });
        api_client.expect_get_records().returning({
            let records = records.clone();
            move |timestamp| {
                assert_eq!(timestamp, None);
                Ok(records.clone())
            }
        });
        api_client.expect_is_prod_server().returning(|| Ok(false));

        let context = RemoteSettingsContext {
            app_version: Some("127.0.0.".to_string()),
            ..Default::default()
        };

        let mut storage = Storage::new(":memory:".into()).expect("Error creating storage");
        let _ = storage.set_records(
            "http://rs.example.com/v1/buckets/main/collections/test-collection",
            &records,
        );

        let rs_client = RemoteSettingsClient::new_from_parts(
            "test-collection".into(),
            storage,
            JexlFilter::new(Some(context)),
            api_client,
        );

        assert_eq!(
            rs_client.get_records(false).expect("Error getting records"),
            Some(vec![])
        );
    }
}

#[cfg(not(feature = "jexl"))]
#[cfg(test)]
mod cached_data_tests {
    use super::*;

    #[test]
    fn test_no_cached_data_use_packaged_data() -> Result<()> {
        let collection_name = "search-telemetry-v2";

        let file_path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("dumps")
            .join("main")
            .join(format!("{}.json", collection_name));

        assert!(
            file_path.exists(),
            "Packaged data should exist for this test"
        );

        let mut api_client = MockApiClient::new();
        let storage = Storage::new(":memory:".into())?;

        let collection_url = format!(
            "https://firefox.settings.services.mozilla.com/v1/buckets/main/collections/{}",
            collection_name
        );

        api_client
            .expect_collection_url()
            .returning(move || collection_url.clone());
        api_client.expect_is_prod_server().returning(|| Ok(true));

        let rs_client =
            RemoteSettingsClient::new_from_parts(collection_name.to_string(), storage, api_client);

        let records = rs_client.get_records(false)?;
        assert!(records.is_some(), "Records should exist from packaged data");

        Ok(())
    }

    #[test]
    fn test_packaged_data_newer_than_cached() -> Result<()> {
        let api_client = MockApiClient::new();
        let storage = Storage::new(":memory:".into())?;

        let collection_url = "https://firefox.settings.services.mozilla.com/v1/buckets/main/collections/search-telemetry-v2";

        // First get the packaged data to know its timestamp
        let rs_client =
            RemoteSettingsClient::new_from_parts("search-telemetry-v2".into(), storage, api_client);
        let packaged_data = rs_client
            .load_packaged_data()
            .expect("Packaged data should exist");

        // Setup older cached data
        let old_record = RemoteSettingsRecord {
            id: "old".to_string(),
            last_modified: packaged_data.timestamp - 1000, // Ensure it's older
            deleted: false,
            attachment: None,
            fields: serde_json::Map::new(),
        };

        let mut api_client = MockApiClient::new();
        let mut storage = Storage::new(":memory:".into())?;
        storage.set_records(collection_url, &vec![old_record.clone()])?;

        api_client
            .expect_collection_url()
            .returning(|| collection_url.to_string());
        api_client.expect_is_prod_server().returning(|| Ok(true));

        let rs_client =
            RemoteSettingsClient::new_from_parts("search-telemetry-v2".into(), storage, api_client);

        let records = rs_client.get_records(false)?;
        assert!(records.is_some());
        let records = records.unwrap();
        assert!(!records.is_empty());

        // Verify the new records replaced old ones
        let mut inner = rs_client.inner.lock();
        let cached = inner.storage.get_records(collection_url)?.unwrap();
        assert!(cached[0].last_modified > old_record.last_modified);
        assert_eq!(cached.len(), packaged_data.data.len());

        Ok(())
    }

    #[test]
    fn test_no_cached_data_no_packaged_data_sync_if_empty_true() -> Result<()> {
        let collection_name = "nonexistent-collection"; // A collection without packaged data

        // Verify the packaged data file does not exist
        let file_path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("dumps")
            .join("main")
            .join(format!("{}.json", collection_name));

        assert!(
            !file_path.exists(),
            "Packaged data should not exist for this test"
        );

        let mut api_client = MockApiClient::new();
        let storage = Storage::new(":memory:".into())?;

        let collection_url = format!(
            "https://firefox.settings.services.mozilla.com/v1/buckets/main/collections/{}",
            collection_name
        );

        api_client
            .expect_collection_url()
            .returning(move || collection_url.clone());
        api_client.expect_is_prod_server().returning(|| Ok(true));

        // Mock get_records to return some data
        let expected_records = vec![RemoteSettingsRecord {
            id: "remote".to_string(),
            last_modified: 1000,
            deleted: false,
            attachment: None,
            fields: serde_json::Map::new(),
        }];
        api_client
            .expect_get_records()
            .withf(|timestamp| timestamp.is_none())
            .returning(move |_| Ok(expected_records.clone()));

        let rs_client =
            RemoteSettingsClient::new_from_parts(collection_name.to_string(), storage, api_client);

        // Call get_records with sync_if_empty = true
        let records = rs_client.get_records(true)?;
        assert!(
            records.is_some(),
            "Records should be fetched from the remote server"
        );
        let records = records.unwrap();
        assert_eq!(records.len(), 1);
        assert_eq!(records[0].id, "remote");

        Ok(())
    }

    #[test]
    fn test_no_cached_data_no_packaged_data_sync_if_empty_false() -> Result<()> {
        let collection_name = "nonexistent-collection"; // A collection without packaged data

        // Verify the packaged data file does not exist
        let file_path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("dumps")
            .join("main")
            .join(format!("{}.json", collection_name));

        assert!(
            !file_path.exists(),
            "Packaged data should not exist for this test"
        );

        let mut api_client = MockApiClient::new();
        let storage = Storage::new(":memory:".into())?;

        let collection_url = format!(
            "https://firefox.settings.services.mozilla.com/v1/buckets/main/collections/{}",
            collection_name
        );

        api_client
            .expect_collection_url()
            .returning(move || collection_url.clone());
        api_client.expect_is_prod_server().returning(|| Ok(true));

        // Since sync_if_empty is false, get_records should not be called
        // No need to set expectation for api_client.get_records

        let rs_client =
            RemoteSettingsClient::new_from_parts(collection_name.to_string(), storage, api_client);

        // Call get_records with sync_if_empty = false
        let records = rs_client.get_records(false)?;
        assert!(
            records.is_none(),
            "Records should be None when no cache, no packaged data, and sync_if_empty is false"
        );

        Ok(())
    }

    #[test]
    fn test_cached_data_exists_and_not_empty() -> Result<()> {
        let collection_name = "test-collection";
        let mut api_client = MockApiClient::new();
        let mut storage = Storage::new(":memory:".into())?;

        let collection_url = format!(
            "https://firefox.settings.services.mozilla.com/v1/buckets/main/collections/{}",
            collection_name
        );

        // Set up cached records
        let cached_records = vec![RemoteSettingsRecord {
            id: "cached1".to_string(),
            last_modified: 500,
            deleted: false,
            attachment: None,
            fields: serde_json::Map::new(),
        }];
        storage.set_records(&collection_url, &cached_records)?;

        api_client
            .expect_collection_url()
            .returning(move || collection_url.clone());
        api_client.expect_is_prod_server().returning(|| Ok(true));

        let rs_client =
            RemoteSettingsClient::new_from_parts(collection_name.to_string(), storage, api_client);

        // Call get_records with any sync_if_empty value
        let records = rs_client.get_records(true)?;
        assert!(
            records.is_some(),
            "Records should be returned from the cached data"
        );
        let records = records.unwrap();
        assert_eq!(records.len(), 1);
        assert_eq!(records[0].id, "cached1");

        Ok(())
    }

    #[test]
    fn test_cached_data_empty_sync_if_empty_false() -> Result<()> {
        let collection_name = "test-collection";
        let mut api_client = MockApiClient::new();
        let mut storage = Storage::new(":memory:".into())?;

        let collection_url = format!(
            "https://firefox.settings.services.mozilla.com/v1/buckets/main/collections/{}",
            collection_name
        );

        // Set up empty cached records
        let cached_records: Vec<RemoteSettingsRecord> = vec![];
        storage.set_records(&collection_url, &cached_records)?;

        api_client
            .expect_collection_url()
            .returning(move || collection_url.clone());
        api_client.expect_is_prod_server().returning(|| Ok(true));

        let rs_client =
            RemoteSettingsClient::new_from_parts(collection_name.to_string(), storage, api_client);

        // Call get_records with sync_if_empty = false
        let records = rs_client.get_records(false)?;
        assert!(records.is_some(), "Empty cached records should be returned");
        let records = records.unwrap();
        assert!(records.is_empty(), "Cached records should be empty");

        Ok(())
    }
}

#[cfg(not(feature = "jexl"))]
#[cfg(test)]
mod test_packaged_metadata {
    use super::*;
    use std::path::PathBuf;

    #[test]
    fn test_no_cached_data_use_packaged_attachment() -> Result<()> {
        let collection_name = "regions";
        let attachment_name = "world";

        // Verify our packaged attachment exists with its manifest
        let base_path = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .join("dumps")
            .join("main")
            .join("attachments")
            .join(collection_name);

        let file_path = base_path.join(attachment_name);
        let manifest_path = base_path.join(format!("{}.meta.json", attachment_name));

        assert!(
            file_path.exists(),
            "Packaged attachment should exist for this test"
        );
        assert!(
            manifest_path.exists(),
            "Manifest file should exist for this test"
        );

        let manifest_content = std::fs::read_to_string(manifest_path)?;
        let manifest: serde_json::Value = serde_json::from_str(&manifest_content)?;

        let mut api_client = MockApiClient::new();
        let storage = Storage::new(":memory:".into())?;

        let collection_url = format!(
            "https://firefox.settings.services.mozilla.com/v1/buckets/main/collections/{}",
            collection_name
        );

        api_client
            .expect_collection_url()
            .returning(move || collection_url.clone());
        api_client.expect_is_prod_server().returning(|| Ok(true));

        let rs_client =
            RemoteSettingsClient::new_from_parts(collection_name.to_string(), storage, api_client);

        // Create record with metadata from manifest
        let attachment_metadata = Attachment {
            filename: attachment_name.to_string(),
            mimetype: "application/octet-stream".to_string(),
            location: attachment_name.to_string(),
            size: manifest["size"].as_u64().unwrap(),
            hash: manifest["hash"].as_str().unwrap().to_string(),
        };

        let record = RemoteSettingsRecord {
            id: "test-record".to_string(),
            last_modified: 12345,
            deleted: false,
            attachment: Some(attachment_metadata),
            fields: serde_json::json!({}).as_object().unwrap().clone(),
        };

        let attachment_data = rs_client.get_attachment(record)?;

        // Verify we got the expected data
        let expected_data = std::fs::read(file_path)?;
        assert_eq!(attachment_data, expected_data);

        Ok(())
    }

    #[test]
    fn test_packaged_attachment_outdated_fetch_from_api() -> Result<()> {
        let collection_name = "regions";
        let attachment_name = "world";

        let mut api_client = MockApiClient::new();
        let storage = Storage::new(":memory:".into())?;

        let collection_url = format!(
            "https://firefox.settings.services.mozilla.com/v1/buckets/main/collections/{}",
            collection_name
        );

        // Prepare mock data
        let mock_api_data = vec![1, 2, 3, 4, 5];

        // Create metadata that doesn't match our packaged data
        let attachment_metadata = Attachment {
            filename: attachment_name.to_string(),
            mimetype: "application/octet-stream".to_string(),
            location: attachment_name.to_string(),
            size: mock_api_data.len() as u64,
            hash: {
                use sha2::{Digest, Sha256};
                format!("{:x}", Sha256::digest(&mock_api_data))
            },
        };

        api_client
            .expect_collection_url()
            .returning(move || collection_url.clone());
        api_client.expect_is_prod_server().returning(|| Ok(true));
        api_client
            .expect_get_attachment()
            .returning(move |_| Ok(mock_api_data.clone()));

        let rs_client =
            RemoteSettingsClient::new_from_parts(collection_name.to_string(), storage, api_client);

        let record = RemoteSettingsRecord {
            id: "test-record".to_string(),
            last_modified: 12345,
            deleted: false,
            attachment: Some(attachment_metadata),
            fields: serde_json::json!({}).as_object().unwrap().clone(),
        };

        let attachment_data = rs_client.get_attachment(record)?;

        // Verify we got the mock API data, not the packaged data
        assert_eq!(attachment_data, vec![1, 2, 3, 4, 5]);

        Ok(())
    }
}
