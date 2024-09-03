/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

use std::{
    collections::{hash_map::Entry, BTreeSet, HashMap, HashSet},
    path::{Path, PathBuf},
    sync::Arc,
};

use error_support::{breadcrumb, handle_error};
use once_cell::sync::OnceCell;
use parking_lot::Mutex;
use remote_settings::{self, RemoteSettingsConfig, RemoteSettingsServer};

use serde::de::DeserializeOwned;

use crate::{
    config::{SuggestGlobalConfig, SuggestProviderConfig},
    db::{ConnectionType, IngestedRecord, Sqlite3Extension, SuggestDao, SuggestDb},
    error::Error,
    metrics::{DownloadTimer, SuggestIngestionMetrics, SuggestQueryMetrics},
    provider::{SuggestionProvider, SuggestionProviderConstraints, DEFAULT_INGEST_PROVIDERS},
    rs::{
        Client, Collection, Record, RemoteSettingsClient, SuggestAttachment, SuggestRecord,
        SuggestRecordId, SuggestRecordType,
    },
    suggestion::AmpSuggestionType,
    QueryWithMetricsResult, Result, SuggestApiResult, Suggestion, SuggestionQuery,
};

/// Builder for [SuggestStore]
///
/// Using a builder is preferred to calling the constructor directly since it's harder to confuse
/// the data_path and cache_path strings.
pub struct SuggestStoreBuilder(Mutex<SuggestStoreBuilderInner>);

#[derive(Default)]
struct SuggestStoreBuilderInner {
    data_path: Option<String>,
    remote_settings_server: Option<RemoteSettingsServer>,
    remote_settings_bucket_name: Option<String>,
    extensions_to_load: Vec<Sqlite3Extension>,
}

impl Default for SuggestStoreBuilder {
    fn default() -> Self {
        Self::new()
    }
}

impl SuggestStoreBuilder {
    pub fn new() -> SuggestStoreBuilder {
        Self(Mutex::new(SuggestStoreBuilderInner::default()))
    }

    pub fn data_path(self: Arc<Self>, path: String) -> Arc<Self> {
        self.0.lock().data_path = Some(path);
        self
    }

    pub fn cache_path(self: Arc<Self>, _path: String) -> Arc<Self> {
        // We used to use this, but we're not using it anymore, just ignore the call
        self
    }

    pub fn remote_settings_server(self: Arc<Self>, server: RemoteSettingsServer) -> Arc<Self> {
        self.0.lock().remote_settings_server = Some(server);
        self
    }

    pub fn remote_settings_bucket_name(self: Arc<Self>, bucket_name: String) -> Arc<Self> {
        self.0.lock().remote_settings_bucket_name = Some(bucket_name);
        self
    }

    pub fn load_extension(
        self: Arc<Self>,
        library: String,
        entry_point: Option<String>,
    ) -> Arc<Self> {
        self.0.lock().extensions_to_load.push(Sqlite3Extension {
            library,
            entry_point,
        });
        self
    }

    #[handle_error(Error)]
    pub fn build(&self) -> SuggestApiResult<Arc<SuggestStore>> {
        let inner = self.0.lock();
        let extensions_to_load = inner.extensions_to_load.clone();
        let data_path = inner
            .data_path
            .clone()
            .ok_or_else(|| Error::SuggestStoreBuilder("data_path not specified".to_owned()))?;

        let client = RemoteSettingsClient::new(
            inner.remote_settings_server.clone(),
            inner.remote_settings_bucket_name.clone(),
            None,
        )?;

        Ok(Arc::new(SuggestStore {
            inner: SuggestStoreInner::new(data_path, extensions_to_load, client),
        }))
    }
}

/// What should be interrupted when [SuggestStore::interrupt] is called?
#[derive(Clone, Copy, Debug, Eq, PartialEq, Hash)]
pub enum InterruptKind {
    /// Interrupt read operations like [SuggestStore::query]
    Read,
    /// Interrupt write operations.  This mostly means [SuggestStore::ingest], but
    /// [SuggestStore::dismiss_suggestion] may also be interrupted.
    Write,
    /// Interrupt both read and write operations,
    ReadWrite,
}

/// The store is the entry point to the Suggest component. It incrementally
/// downloads suggestions from the Remote Settings service, stores them in a
/// local database, and returns them in response to user queries.
///
/// Your application should create a single store, and manage it as a singleton.
/// The store is thread-safe, and supports concurrent queries and ingests. We
/// expect that your application will call [`SuggestStore::query()`] to show
/// suggestions as the user types into the address bar, and periodically call
/// [`SuggestStore::ingest()`] in the background to update the database with
/// new suggestions from Remote Settings.
///
/// For responsiveness, we recommend always calling `query()` on a worker
/// thread. When the user types new input into the address bar, call
/// [`SuggestStore::interrupt()`] on the main thread to cancel the query
/// for the old input, and unblock the worker thread for the new query.
///
/// The store keeps track of the state needed to support incremental ingestion,
/// but doesn't schedule the ingestion work itself, or decide how many
/// suggestions to ingest at once. This is for two reasons:
///
/// 1. The primitives for scheduling background work vary between platforms, and
///    aren't available to the lower-level Rust layer. You might use an idle
///    timer on Desktop, `WorkManager` on Android, or `BGTaskScheduler` on iOS.
/// 2. Ingestion constraints can change, depending on the platform and the needs
///    of your application. A mobile device on a metered connection might want
///    to request a small subset of the Suggest data and download the rest
///    later, while a desktop on a fast link might download the entire dataset
///    on the first launch.
pub struct SuggestStore {
    inner: SuggestStoreInner<RemoteSettingsClient>,
}

impl SuggestStore {
    /// Creates a Suggest store.
    #[handle_error(Error)]
    pub fn new(
        path: &str,
        settings_config: Option<RemoteSettingsConfig>,
    ) -> SuggestApiResult<Self> {
        let client = match settings_config {
            Some(settings_config) => RemoteSettingsClient::new(
                settings_config.server,
                settings_config.bucket_name,
                settings_config.server_url,
                // Note: collection name is ignored, since we fetch from multiple collections
                // (fakespot-suggest-products and quicksuggest).  No consumer sets it to a
                // non-default value anyways.
            )?,
            None => RemoteSettingsClient::new(None, None, None)?,
        };

        Ok(Self {
            inner: SuggestStoreInner::new(path.to_owned(), vec![], client),
        })
    }

    /// Queries the database for suggestions.
    #[handle_error(Error)]
    pub fn query(&self, query: SuggestionQuery) -> SuggestApiResult<Vec<Suggestion>> {
        Ok(self.inner.query(query)?.suggestions)
    }

    /// Queries the database for suggestions.
    #[handle_error(Error)]
    pub fn query_with_metrics(
        &self,
        query: SuggestionQuery,
    ) -> SuggestApiResult<QueryWithMetricsResult> {
        self.inner.query(query)
    }

    /// Dismiss a suggestion
    ///
    /// Dismissed suggestions will not be returned again
    ///
    /// In the case of AMP suggestions this should be the raw URL.
    #[handle_error(Error)]
    pub fn dismiss_suggestion(&self, suggestion_url: String) -> SuggestApiResult<()> {
        self.inner.dismiss_suggestion(suggestion_url)
    }

    /// Clear dismissed suggestions
    #[handle_error(Error)]
    pub fn clear_dismissed_suggestions(&self) -> SuggestApiResult<()> {
        self.inner.clear_dismissed_suggestions()
    }

    /// Interrupts any ongoing queries.
    ///
    /// This should be called when the user types new input into the address
    /// bar, to ensure that they see fresh suggestions as they type. This
    /// method does not interrupt any ongoing ingests.
    pub fn interrupt(&self, kind: Option<InterruptKind>) {
        self.inner.interrupt(kind)
    }

    /// Ingests new suggestions from Remote Settings.
    #[handle_error(Error)]
    pub fn ingest(
        &self,
        constraints: SuggestIngestionConstraints,
    ) -> SuggestApiResult<SuggestIngestionMetrics> {
        self.inner.ingest(constraints)
    }

    /// Removes all content from the database.
    #[handle_error(Error)]
    pub fn clear(&self) -> SuggestApiResult<()> {
        self.inner.clear()
    }

    // Returns global Suggest configuration data.
    #[handle_error(Error)]
    pub fn fetch_global_config(&self) -> SuggestApiResult<SuggestGlobalConfig> {
        self.inner.fetch_global_config()
    }

    // Returns per-provider Suggest configuration data.
    #[handle_error(Error)]
    pub fn fetch_provider_config(
        &self,
        provider: SuggestionProvider,
    ) -> SuggestApiResult<Option<SuggestProviderConfig>> {
        self.inner.fetch_provider_config(provider)
    }

    pub fn force_reingest(&self) {
        self.inner.force_reingest()
    }
}

/// Constraints limit which suggestions to ingest from Remote Settings.
#[derive(Clone, Default, Debug)]
pub struct SuggestIngestionConstraints {
    pub providers: Option<Vec<SuggestionProvider>>,
    pub provider_constraints: Option<SuggestionProviderConstraints>,
    /// Only run ingestion if the table `suggestions` is empty
    pub empty_only: bool,
}

impl SuggestIngestionConstraints {
    pub fn all_providers() -> Self {
        Self {
            providers: Some(vec![
                SuggestionProvider::Amp,
                SuggestionProvider::Wikipedia,
                SuggestionProvider::Amo,
                SuggestionProvider::Pocket,
                SuggestionProvider::Yelp,
                SuggestionProvider::Mdn,
                SuggestionProvider::Weather,
                SuggestionProvider::AmpMobile,
                SuggestionProvider::Fakespot,
                SuggestionProvider::Exposure,
            ]),
            ..Self::default()
        }
    }
}

/// The implementation of the store. This is generic over the Remote Settings
/// client, and is split out from the concrete [`SuggestStore`] for testing
/// with a mock client.
pub(crate) struct SuggestStoreInner<S> {
    /// Path to the persistent SQL database.
    ///
    /// This stores things that should persist when the user clears their cache.
    /// It's not currently used because not all consumers pass this in yet.
    #[allow(unused)]
    data_path: PathBuf,
    dbs: OnceCell<SuggestStoreDbs>,
    extensions_to_load: Vec<Sqlite3Extension>,
    settings_client: S,
}

impl<S> SuggestStoreInner<S> {
    pub fn new(
        data_path: impl Into<PathBuf>,
        extensions_to_load: Vec<Sqlite3Extension>,
        settings_client: S,
    ) -> Self {
        Self {
            data_path: data_path.into(),
            extensions_to_load,
            dbs: OnceCell::new(),
            settings_client,
        }
    }

    /// Returns this store's database connections, initializing them if
    /// they're not already open.
    fn dbs(&self) -> Result<&SuggestStoreDbs> {
        self.dbs
            .get_or_try_init(|| SuggestStoreDbs::open(&self.data_path, &self.extensions_to_load))
    }

    fn query(&self, query: SuggestionQuery) -> Result<QueryWithMetricsResult> {
        let mut metrics = SuggestQueryMetrics::default();
        let mut suggestions = vec![];

        let unique_providers = query.providers.iter().collect::<HashSet<_>>();
        let reader = &self.dbs()?.reader;
        for provider in unique_providers {
            let new_suggestions = metrics.measure_query(provider.to_string(), || {
                reader.read(|dao| match provider {
                    SuggestionProvider::Amp => {
                        dao.fetch_amp_suggestions(&query, AmpSuggestionType::Desktop)
                    }
                    SuggestionProvider::AmpMobile => {
                        dao.fetch_amp_suggestions(&query, AmpSuggestionType::Mobile)
                    }
                    SuggestionProvider::Wikipedia => dao.fetch_wikipedia_suggestions(&query),
                    SuggestionProvider::Amo => dao.fetch_amo_suggestions(&query),
                    SuggestionProvider::Pocket => dao.fetch_pocket_suggestions(&query),
                    SuggestionProvider::Yelp => dao.fetch_yelp_suggestions(&query),
                    SuggestionProvider::Mdn => dao.fetch_mdn_suggestions(&query),
                    SuggestionProvider::Weather => dao.fetch_weather_suggestions(&query),
                    SuggestionProvider::Fakespot => dao.fetch_fakespot_suggestions(&query),
                    SuggestionProvider::Exposure => dao.fetch_exposure_suggestions(&query),
                })
            })?;
            suggestions.extend(new_suggestions);
        }

        // Note: it's important that this is a stable sort to keep the intra-provider order stable.
        // For example, we can return multiple fakespot-suggestions all with `score=0.245`.  In
        // that case, they must be in the same order that `fetch_fakespot_suggestions` returned
        // them in.
        suggestions.sort();
        if let Some(limit) = query.limit.and_then(|limit| usize::try_from(limit).ok()) {
            suggestions.truncate(limit);
        }
        Ok(QueryWithMetricsResult {
            suggestions,
            query_times: metrics.times,
        })
    }

    fn dismiss_suggestion(&self, suggestion_url: String) -> Result<()> {
        self.dbs()?
            .writer
            .write(|dao| dao.insert_dismissal(&suggestion_url))
    }

    fn clear_dismissed_suggestions(&self) -> Result<()> {
        self.dbs()?.writer.write(|dao| dao.clear_dismissals())?;
        Ok(())
    }

    fn interrupt(&self, kind: Option<InterruptKind>) {
        if let Some(dbs) = self.dbs.get() {
            // Only interrupt if the databases are already open.
            match kind.unwrap_or(InterruptKind::Read) {
                InterruptKind::Read => {
                    dbs.reader.interrupt_handle.interrupt();
                }
                InterruptKind::Write => {
                    dbs.writer.interrupt_handle.interrupt();
                }
                InterruptKind::ReadWrite => {
                    dbs.reader.interrupt_handle.interrupt();
                    dbs.writer.interrupt_handle.interrupt();
                }
            }
        }
    }

    fn clear(&self) -> Result<()> {
        self.dbs()?.writer.write(|dao| dao.clear())
    }

    pub fn fetch_global_config(&self) -> Result<SuggestGlobalConfig> {
        self.dbs()?.reader.read(|dao| dao.get_global_config())
    }

    pub fn fetch_provider_config(
        &self,
        provider: SuggestionProvider,
    ) -> Result<Option<SuggestProviderConfig>> {
        self.dbs()?
            .reader
            .read(|dao| dao.get_provider_config(provider))
    }

    // Cause the next ingestion to re-ingest all data
    pub fn force_reingest(&self) {
        let writer = &self.dbs().unwrap().writer;
        writer.write(|dao| dao.force_reingest()).unwrap();
    }
}

impl<S> SuggestStoreInner<S>
where
    S: Client,
{
    pub fn ingest(
        &self,
        constraints: SuggestIngestionConstraints,
    ) -> Result<SuggestIngestionMetrics> {
        breadcrumb!("Ingestion starting");
        let writer = &self.dbs()?.writer;
        let mut metrics = SuggestIngestionMetrics::default();
        if constraints.empty_only && !writer.read(|dao| dao.suggestions_table_empty())? {
            return Ok(metrics);
        }

        // Figure out which record types we're ingesting and group them by
        // collection. A record type may be used by multiple providers, but we
        // want to ingest each one at most once.
        let mut record_types_by_collection = HashMap::<Collection, BTreeSet<_>>::new();
        for p in constraints
            .providers
            .as_ref()
            .unwrap_or(&DEFAULT_INGEST_PROVIDERS.to_vec())
            .iter()
        {
            record_types_by_collection
                .entry(p.record_type().collection())
                .or_default()
                .insert(p.record_type());
        }

        // Always ingest these record types.
        for rt in [SuggestRecordType::Icon, SuggestRecordType::GlobalConfig] {
            record_types_by_collection
                .entry(rt.collection())
                .or_default()
                .insert(rt);
        }

        // Create a single write scope for all DB operations
        let mut write_scope = writer.write_scope()?;

        // Read the previously ingested records.  We use this to calculate what's changed
        let ingested_records = write_scope.read(|dao| dao.get_ingested_records())?;

        // For each collection, fetch all records
        for (collection, record_types) in record_types_by_collection {
            breadcrumb!("Ingesting collection {}", collection.name());
            let records =
                write_scope.write(|dao| self.settings_client.get_records(collection, dao))?;

            // For each record type in that collection, calculate the changes and pass them to
            // [Self::ingest_records]
            for record_type in record_types {
                breadcrumb!("Ingesting record_type: {record_type}");
                metrics.measure_ingest(record_type.to_string(), |download_timer| {
                    let changes = RecordChanges::new(
                        records.iter().filter(|r| r.record_type() == record_type),
                        ingested_records.iter().filter(|i| {
                            i.record_type == record_type.as_str()
                                && i.collection == collection.name()
                        }),
                    );
                    write_scope.write(|dao| {
                        self.process_changes(dao, collection, changes, &constraints, download_timer)
                    })
                })?;
                write_scope.err_if_interrupted()?;
            }
        }
        breadcrumb!("Ingestion complete");

        Ok(metrics)
    }

    fn process_changes(
        &self,
        dao: &mut SuggestDao,
        collection: Collection,
        changes: RecordChanges<'_>,
        constraints: &SuggestIngestionConstraints,
        download_timer: &mut DownloadTimer,
    ) -> Result<()> {
        for record in &changes.new {
            log::trace!("Ingesting record ID: {}", record.id.as_str());
            self.process_record(dao, record, constraints, download_timer)?;
        }
        for record in &changes.updated {
            // Drop any data that we previously ingested from this record.
            // Suggestions in particular don't have a stable identifier, and
            // determining which suggestions in the record actually changed is
            // more complicated than dropping and re-ingesting all of them.
            log::trace!("Reingesting updated record ID: {}", record.id.as_str());
            dao.delete_record_data(&record.id)?;
            self.process_record(dao, record, constraints, download_timer)?;
        }
        for record in &changes.unchanged {
            if self.should_reprocess_record(dao, record)? {
                log::trace!("Reingesting unchanged record ID: {}", record.id.as_str());
                self.process_record(dao, record, constraints, download_timer)?;
            }
        }
        for record in &changes.deleted {
            log::trace!("Deleting record ID: {:?}", record.id);
            dao.delete_record_data(&record.id)?;
        }
        dao.update_ingested_records(
            collection.name(),
            &changes.new,
            &changes.updated,
            &changes.deleted,
        )?;
        Ok(())
    }

    fn process_record(
        &self,
        dao: &mut SuggestDao,
        record: &Record,
        constraints: &SuggestIngestionConstraints,
        download_timer: &mut DownloadTimer,
    ) -> Result<()> {
        match &record.payload {
            SuggestRecord::AmpWikipedia => {
                self.download_attachment(
                    dao,
                    record,
                    download_timer,
                    |dao, record_id, suggestions| {
                        dao.insert_amp_wikipedia_suggestions(record_id, suggestions)
                    },
                )?;
            }
            SuggestRecord::AmpMobile => {
                self.download_attachment(
                    dao,
                    record,
                    download_timer,
                    |dao, record_id, suggestions| {
                        dao.insert_amp_mobile_suggestions(record_id, suggestions)
                    },
                )?;
            }
            SuggestRecord::Icon => {
                let (Some(icon_id), Some(attachment)) =
                    (record.id.as_icon_id(), record.attachment.as_ref())
                else {
                    // An icon record should have an icon ID and an
                    // attachment. Icons that don't have these are
                    // malformed, so skip to the next record.
                    return Ok(());
                };
                let data = self.settings_client.download_attachment(record)?;
                dao.put_icon(icon_id, &data, &attachment.mimetype)?;
            }
            SuggestRecord::Amo => {
                self.download_attachment(
                    dao,
                    record,
                    download_timer,
                    |dao, record_id, suggestions| {
                        dao.insert_amo_suggestions(record_id, suggestions)
                    },
                )?;
            }
            SuggestRecord::Pocket => {
                self.download_attachment(
                    dao,
                    record,
                    download_timer,
                    |dao, record_id, suggestions| {
                        dao.insert_pocket_suggestions(record_id, suggestions)
                    },
                )?;
            }
            SuggestRecord::Yelp => {
                self.download_attachment(
                    dao,
                    record,
                    download_timer,
                    |dao, record_id, suggestions| match suggestions.first() {
                        Some(suggestion) => dao.insert_yelp_suggestions(record_id, suggestion),
                        None => Ok(()),
                    },
                )?;
            }
            SuggestRecord::Mdn => {
                self.download_attachment(
                    dao,
                    record,
                    download_timer,
                    |dao, record_id, suggestions| {
                        dao.insert_mdn_suggestions(record_id, suggestions)
                    },
                )?;
            }
            SuggestRecord::Weather(data) => dao.insert_weather_data(&record.id, data)?,
            SuggestRecord::GlobalConfig(config) => {
                dao.put_global_config(&SuggestGlobalConfig::from(config))?
            }
            SuggestRecord::Fakespot => {
                self.download_attachment(
                    dao,
                    record,
                    download_timer,
                    |dao, record_id, suggestions| {
                        dao.insert_fakespot_suggestions(record_id, suggestions)
                    },
                )?;
            }
            SuggestRecord::Exposure(r) => {
                // Ingest this record's attachment if its suggestion type
                // matches a type in the constraints.
                if let Some(suggestion_types) = constraints
                    .provider_constraints
                    .as_ref()
                    .and_then(|c| c.exposure_suggestion_types.as_ref())
                {
                    if suggestion_types.iter().any(|t| *t == r.suggestion_type) {
                        self.download_attachment(
                            dao,
                            record,
                            download_timer,
                            |dao, record_id, suggestions| {
                                dao.insert_exposure_suggestions(
                                    record_id,
                                    &r.suggestion_type,
                                    suggestions,
                                )
                            },
                        )?;
                    }
                }
            }
        }
        Ok(())
    }

    fn download_attachment<T>(
        &self,
        dao: &mut SuggestDao,
        record: &Record,
        download_timer: &mut DownloadTimer,
        ingestion_handler: impl FnOnce(&mut SuggestDao<'_>, &SuggestRecordId, &[T]) -> Result<()>,
    ) -> Result<()>
    where
        T: DeserializeOwned,
    {
        if record.attachment.is_none() {
            return Ok(());
        };

        let attachment_data =
            download_timer.measure_download(|| self.settings_client.download_attachment(record))?;
        match serde_json::from_slice::<SuggestAttachment<T>>(&attachment_data) {
            Ok(attachment) => ingestion_handler(dao, &record.id, attachment.suggestions()),
            // If the attachment doesn't match our expected schema, just skip it.  It's possible
            // that we're using an older version.  If so, we'll get the data when we re-ingest
            // after updating the schema.
            Err(_) => Ok(()),
        }
    }

    fn should_reprocess_record(&self, dao: &mut SuggestDao, record: &Record) -> Result<bool> {
        match &record.payload {
            SuggestRecord::Exposure(_) => {
                // Even though the record was previously ingested, its
                // suggestion wouldn't have been if it never matched the
                // provider constraints of any ingest. Return true if the
                // suggestion is not ingested. If the provider constraints of
                // the current ingest do match the suggestion, we'll ingest it.
                Ok(!dao.is_exposure_suggestion_ingested(&record.id)?)
            }
            _ => Ok(false),
        }
    }
}

/// Tracks changes in suggest records since the last ingestion
struct RecordChanges<'a> {
    new: Vec<&'a Record>,
    updated: Vec<&'a Record>,
    deleted: Vec<&'a IngestedRecord>,
    unchanged: Vec<&'a Record>,
}

impl<'a> RecordChanges<'a> {
    fn new(
        current: impl Iterator<Item = &'a Record>,
        previously_ingested: impl Iterator<Item = &'a IngestedRecord>,
    ) -> Self {
        let mut ingested_map: HashMap<&str, &IngestedRecord> =
            previously_ingested.map(|i| (i.id.as_str(), i)).collect();
        // Iterate through current, finding new/updated records.
        // Remove existing records from ingested_map.
        let mut new = vec![];
        let mut updated = vec![];
        let mut unchanged = vec![];
        for r in current {
            match ingested_map.entry(r.id.as_str()) {
                Entry::Vacant(_) => new.push(r),
                Entry::Occupied(e) => {
                    if e.remove().last_modified != r.last_modified {
                        updated.push(r);
                    } else {
                        unchanged.push(r);
                    }
                }
            }
        }
        // Anything left in ingested_map is a deleted record
        let deleted = ingested_map.into_values().collect();
        Self {
            new,
            deleted,
            updated,
            unchanged,
        }
    }
}

#[cfg(feature = "benchmark_api")]
impl<S> SuggestStoreInner<S>
where
    S: Client,
{
    pub fn into_settings_client(self) -> S {
        self.settings_client
    }

    pub fn ensure_db_initialized(&self) {
        self.dbs().unwrap();
    }

    pub fn ingest_records_by_type(&self, ingest_record_type: SuggestRecordType) {
        let writer = &self.dbs().unwrap().writer;
        let mut timer = DownloadTimer::default();
        let ingested_records = writer.read(|dao| dao.get_ingested_records()).unwrap();
        let records = writer
            .write(|dao| {
                self.settings_client
                    .get_records(ingest_record_type.collection(), dao)
            })
            .unwrap();

        let changes = RecordChanges::new(
            records
                .iter()
                .filter(|r| r.record_type() == ingest_record_type),
            ingested_records
                .iter()
                .filter(|i| i.record_type == ingest_record_type.as_str()),
        );
        writer
            .write(|dao| {
                self.process_changes(
                    dao,
                    ingest_record_type.collection(),
                    changes,
                    &SuggestIngestionConstraints::default(),
                    &mut timer,
                )
            })
            .unwrap();
    }

    pub fn table_row_counts(&self) -> Vec<(String, u32)> {
        use sql_support::ConnExt;

        // Note: since this is just used for debugging, use unwrap to simplify the error handling.
        let reader = &self.dbs().unwrap().reader;
        let conn = reader.conn.lock();
        let table_names: Vec<String> = conn
            .query_rows_and_then(
                "SELECT name FROM sqlite_master where type = 'table'",
                (),
                |row| row.get(0),
            )
            .unwrap();
        let mut table_names_with_counts: Vec<(String, u32)> = table_names
            .into_iter()
            .map(|name| {
                let count: u32 = conn
                    .query_one(&format!("SELECT COUNT(*) FROM {name}"))
                    .unwrap();
                (name, count)
            })
            .collect();
        table_names_with_counts.sort_by(|a, b| (b.1.cmp(&a.1)));
        table_names_with_counts
    }

    pub fn db_size(&self) -> usize {
        use sql_support::ConnExt;

        let reader = &self.dbs().unwrap().reader;
        let conn = reader.conn.lock();
        conn.query_one("SELECT page_size * page_count FROM pragma_page_count(), pragma_page_size()")
            .unwrap()
    }
}

/// Holds a store's open connections to the Suggest database.
struct SuggestStoreDbs {
    /// A read-write connection used to update the database with new data.
    writer: SuggestDb,
    /// A read-only connection used to query the database.
    reader: SuggestDb,
}

impl SuggestStoreDbs {
    fn open(path: &Path, extensions_to_load: &[Sqlite3Extension]) -> Result<Self> {
        // Order is important here: the writer must be opened first, so that it
        // can set up the database and run any migrations.
        let writer = SuggestDb::open(path, extensions_to_load, ConnectionType::ReadWrite)?;
        let reader = SuggestDb::open(path, extensions_to_load, ConnectionType::ReadOnly)?;
        Ok(Self { writer, reader })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    use std::sync::atomic::{AtomicUsize, Ordering};

    use parking_lot::Once;
    use serde_json::json;
    use sql_support::ConnExt;

    use crate::{testing::*, SuggestionProvider};

    /// In-memory Suggest store for testing
    struct TestStore {
        pub inner: SuggestStoreInner<MockRemoteSettingsClient>,
    }

    impl TestStore {
        fn new(client: MockRemoteSettingsClient) -> Self {
            static COUNTER: AtomicUsize = AtomicUsize::new(0);
            let db_path = format!(
                "file:test_store_data_{}?mode=memory&cache=shared",
                COUNTER.fetch_add(1, Ordering::Relaxed),
            );
            Self {
                inner: SuggestStoreInner::new(db_path, vec![], client),
            }
        }

        fn client_mut(&mut self) -> &mut MockRemoteSettingsClient {
            &mut self.inner.settings_client
        }

        fn read<T>(&self, op: impl FnOnce(&SuggestDao) -> Result<T>) -> Result<T> {
            self.inner.dbs().unwrap().reader.read(op)
        }

        fn count_rows(&self, table_name: &str) -> u64 {
            let sql = format!("SELECT count(*) FROM {table_name}");
            self.read(|dao| Ok(dao.conn.query_one(&sql)?))
                .unwrap_or_else(|e| panic!("SQL error in count: {e}"))
        }

        fn ingest(&self, constraints: SuggestIngestionConstraints) {
            self.inner.ingest(constraints).unwrap();
        }

        fn fetch_suggestions(&self, query: SuggestionQuery) -> Vec<Suggestion> {
            self.inner.query(query).unwrap().suggestions
        }

        pub fn fetch_global_config(&self) -> SuggestGlobalConfig {
            self.inner
                .fetch_global_config()
                .expect("Error fetching global config")
        }

        pub fn fetch_provider_config(
            &self,
            provider: SuggestionProvider,
        ) -> Option<SuggestProviderConfig> {
            self.inner
                .fetch_provider_config(provider)
                .expect("Error fetching provider config")
        }
    }

    fn before_each() {
        static ONCE: Once = Once::new();
        ONCE.call_once(|| {
            env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("trace"))
                .is_test(true)
                .init();
        });
    }

    /// Tests that `SuggestStore` is usable with UniFFI, which requires exposed
    /// interfaces to be `Send` and `Sync`.
    #[test]
    fn is_thread_safe() {
        before_each();

        fn is_send_sync<T: Send + Sync>() {}
        is_send_sync::<SuggestStore>();
    }

    /// Tests ingesting suggestions into an empty database.
    #[test]
    fn ingest_suggestions() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(
            MockRemoteSettingsClient::default()
                .with_record("data", "1234", json![los_pollos_amp()])
                .with_icon(los_pollos_icon()),
        );
        store.ingest(SuggestIngestionConstraints::all_providers());
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amp("lo")),
            vec![los_pollos_suggestion("los")],
        );
        Ok(())
    }

    /// Tests ingesting suggestions into an empty database.
    #[test]
    fn ingest_empty_only() -> anyhow::Result<()> {
        before_each();

        let mut store = TestStore::new(MockRemoteSettingsClient::default().with_record(
            "data",
            "1234",
            json![los_pollos_amp()],
        ));
        // suggestions_table_empty returns true before the ingestion is complete
        assert!(store.read(|dao| dao.suggestions_table_empty())?);
        // This ingestion should run, since the DB is empty
        store.ingest(SuggestIngestionConstraints {
            empty_only: true,
            ..SuggestIngestionConstraints::all_providers()
        });
        // suggestions_table_empty returns false after the ingestion is complete
        assert!(!store.read(|dao| dao.suggestions_table_empty())?);

        // This ingestion should not run since the DB is no longer empty
        store.client_mut().update_record(
            "data",
            "1234",
            json!([los_pollos_amp(), good_place_eats_amp()]),
        );
        store.ingest(SuggestIngestionConstraints {
            empty_only: true,
            ..SuggestIngestionConstraints::all_providers()
        });
        // "la" should not match the good place eats suggestion, since that should not have been
        // ingested.
        assert_eq!(store.fetch_suggestions(SuggestionQuery::amp("la")), vec![]);

        Ok(())
    }

    /// Tests ingesting suggestions with icons.
    #[test]
    fn ingest_amp_icons() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(
            MockRemoteSettingsClient::default()
                .with_record(
                    "data",
                    "1234",
                    json!([los_pollos_amp(), good_place_eats_amp()]),
                )
                .with_icon(los_pollos_icon())
                .with_icon(good_place_eats_icon()),
        );
        // This ingestion should run, since the DB is empty
        store.ingest(SuggestIngestionConstraints::all_providers());

        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amp("lo")),
            vec![los_pollos_suggestion("los")]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amp("la")),
            vec![good_place_eats_suggestion("lasagna")]
        );

        Ok(())
    }

    #[test]
    fn ingest_full_keywords() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(MockRemoteSettingsClient::default()
            .with_record("data", "1234", json!([
                // AMP attachment with full keyword data
                los_pollos_amp().merge(json!({
                    "keywords": ["lo", "los", "los p", "los pollos", "los pollos h", "los pollos hermanos"],
                    "full_keywords": [
                        // Full keyword for the first 4 keywords
                        ("los pollos", 4),
                        // Full keyword for the next 2 keywords
                        ("los pollos hermanos (restaurant)", 2),
                    ],
                })),
                // AMP attachment without full keyword data
                good_place_eats_amp(),
                // Wikipedia attachment with full keyword data.  We should ignore the full
                // keyword data for Wikipedia suggestions
                california_wiki(),
                // california_wiki().merge(json!({
                //     "keywords": ["cal", "cali", "california"],
                //     "full_keywords": [("california institute of technology", 3)],
                // })),
            ]))
            .with_record("amp-mobile-suggestions", "2468", json!([
                // Amp mobile attachment with full keyword data
                a1a_amp_mobile().merge(json!({
                    "keywords": ["a1a", "ca", "car", "car wash"],
                    "full_keywords": [
                        ("A1A Car Wash", 1),
                        ("car wash", 3),
                    ],
                })),
            ]))
            .with_icon(los_pollos_icon())
            .with_icon(good_place_eats_icon())
            .with_icon(california_icon())
        );
        store.ingest(SuggestIngestionConstraints::all_providers());

        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amp("lo")),
            // This keyword comes from the provided full_keywords list
            vec![los_pollos_suggestion("los pollos")],
        );

        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amp("la")),
            // Good place eats did not have full keywords, so this one is calculated with the
            // keywords.rs code
            vec![good_place_eats_suggestion("lasagna")],
        );

        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::wikipedia("cal")),
            // Even though this had a full_keywords field, we should ignore it since it's a
            // wikipedia suggestion and use the keywords.rs code instead
            vec![california_suggestion("california")],
        );

        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amp_mobile("a1a")),
            // This keyword comes from the provided full_keywords list.
            vec![a1a_suggestion("A1A Car Wash")],
        );

        Ok(())
    }

    /// Tests ingesting a data attachment containing a single suggestion,
    /// instead of an array of suggestions.
    #[test]
    fn ingest_one_suggestion_in_data_attachment() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(
            MockRemoteSettingsClient::default()
                // This record contains just one JSON object, rather than an array of them
                .with_record("data", "1234", los_pollos_amp())
                .with_icon(los_pollos_icon()),
        );
        store.ingest(SuggestIngestionConstraints::all_providers());
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amp("lo")),
            vec![los_pollos_suggestion("los")],
        );

        Ok(())
    }

    /// Tests re-ingesting suggestions from an updated attachment.
    #[test]
    fn reingest_amp_suggestions() -> anyhow::Result<()> {
        before_each();

        let mut store = TestStore::new(MockRemoteSettingsClient::default().with_record(
            "data",
            "1234",
            json!([los_pollos_amp(), good_place_eats_amp()]),
        ));
        // Ingest once
        store.ingest(SuggestIngestionConstraints::all_providers());
        // Update the snapshot with new suggestions: Los pollos has a new name and Good place eats
        // is now serving Penne
        store.client_mut().update_record(
            "data",
            "1234",
            json!([
                los_pollos_amp().merge(json!({
                    "title": "Los Pollos Hermanos - Now Serving at 14 Locations!",
                })),
                good_place_eats_amp().merge(json!({
                    "keywords": ["pe", "pen", "penne", "penne for your thoughts"],
                    "title": "Penne for Your Thoughts",
                    "url": "https://penne.biz",
                }))
            ]),
        );
        store.ingest(SuggestIngestionConstraints::all_providers());

        assert!(matches!(
            store.fetch_suggestions(SuggestionQuery::amp("lo")).as_slice(),
            [ Suggestion::Amp { title, .. } ] if title == "Los Pollos Hermanos - Now Serving at 14 Locations!",
        ));

        assert_eq!(store.fetch_suggestions(SuggestionQuery::amp("la")), vec![]);
        assert!(matches!(
            store.fetch_suggestions(SuggestionQuery::amp("pe")).as_slice(),
            [ Suggestion::Amp { title, url, .. } ] if title == "Penne for Your Thoughts" && url == "https://penne.biz"
        ));

        Ok(())
    }

    /// Tests re-ingesting icons from an updated attachment.
    #[test]
    fn reingest_icons() -> anyhow::Result<()> {
        before_each();

        let mut store = TestStore::new(
            MockRemoteSettingsClient::default()
                .with_record(
                    "data",
                    "1234",
                    json!([los_pollos_amp(), good_place_eats_amp()]),
                )
                .with_icon(los_pollos_icon())
                .with_icon(good_place_eats_icon()),
        );
        // This ingestion should run, since the DB is empty
        store.ingest(SuggestIngestionConstraints::all_providers());

        // Reingest with updated icon data
        //  - Los pollos gets new data and a new id
        //  - Good place eats gets new data only
        store
            .client_mut()
            .update_record(
                "data",
                "1234",
                json!([
                    los_pollos_amp().merge(json!({"icon": "1000"})),
                    good_place_eats_amp()
                ]),
            )
            .delete_icon(los_pollos_icon())
            .add_icon(MockIcon {
                id: "1000",
                data: "new-los-pollos-icon",
                ..los_pollos_icon()
            })
            .update_icon(MockIcon {
                data: "new-good-place-eats-icon",
                ..good_place_eats_icon()
            });
        store.ingest(SuggestIngestionConstraints::all_providers());

        assert!(matches!(
            store.fetch_suggestions(SuggestionQuery::amp("lo")).as_slice(),
            [ Suggestion::Amp { icon, .. } ] if *icon == Some("new-los-pollos-icon".as_bytes().to_vec())
        ));

        assert!(matches!(
            store.fetch_suggestions(SuggestionQuery::amp("la")).as_slice(),
            [ Suggestion::Amp { icon, .. } ] if *icon == Some("new-good-place-eats-icon".as_bytes().to_vec())
        ));

        Ok(())
    }

    /// Tests re-ingesting AMO suggestions from an updated attachment.
    #[test]
    fn reingest_amo_suggestions() -> anyhow::Result<()> {
        before_each();

        let mut store = TestStore::new(
            MockRemoteSettingsClient::default()
                .with_record("amo-suggestions", "data-1", json!([relay_amo()]))
                .with_record(
                    "amo-suggestions",
                    "data-2",
                    json!([dark_mode_amo(), foxy_guestures_amo()]),
                ),
        );

        store.ingest(SuggestIngestionConstraints::all_providers());

        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amo("masking e")),
            vec![relay_suggestion()],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amo("night")),
            vec![dark_mode_suggestion()],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amo("grammar")),
            vec![foxy_guestures_suggestion()],
        );

        // Update the snapshot with new suggestions: update the second, drop the
        // third, and add the fourth.
        store
            .client_mut()
            .update_record("amo-suggestions", "data-1", json!([relay_amo()]))
            .update_record(
                "amo-suggestions",
                "data-2",
                json!([
                    dark_mode_amo().merge(json!({"title": "Updated second suggestion"})),
                    new_tab_override_amo(),
                ]),
            );
        store.ingest(SuggestIngestionConstraints::all_providers());

        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amo("masking e")),
            vec![relay_suggestion()],
        );
        assert!(matches!(
            store.fetch_suggestions(SuggestionQuery::amo("night")).as_slice(),
            [Suggestion::Amo { title, .. } ] if title == "Updated second suggestion"
        ));
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amo("grammar")),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amo("image search")),
            vec![new_tab_override_suggestion()],
        );

        Ok(())
    }

    /// Tests ingestion when previously-ingested suggestions/icons have been deleted.
    #[test]
    fn ingest_with_deletions() -> anyhow::Result<()> {
        before_each();

        let mut store = TestStore::new(
            MockRemoteSettingsClient::default()
                .with_record("data", "data-1", json!([los_pollos_amp()]))
                .with_record("data", "data-2", json!([good_place_eats_amp()]))
                .with_icon(los_pollos_icon())
                .with_icon(good_place_eats_icon()),
        );
        store.ingest(SuggestIngestionConstraints::all_providers());
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amp("lo")),
            vec![los_pollos_suggestion("los")],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amp("la")),
            vec![good_place_eats_suggestion("lasagna")],
        );
        // Re-ingest without los-pollos and good place eat's icon.  The suggest store should
        // recognize that they're missing and delete them.
        store
            .client_mut()
            .delete_record("quicksuggest", "data-1")
            .delete_icon(good_place_eats_icon());
        store.ingest(SuggestIngestionConstraints::all_providers());

        assert_eq!(store.fetch_suggestions(SuggestionQuery::amp("lo")), vec![]);
        assert!(matches!(
            store.fetch_suggestions(SuggestionQuery::amp("la")).as_slice(),
            [
                Suggestion::Amp { icon, icon_mimetype, .. }
            ] if icon.is_none() && icon_mimetype.is_none(),
        ));
        Ok(())
    }

    /// Tests clearing the store.
    #[test]
    fn clear() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(
            MockRemoteSettingsClient::default()
                .with_record("data", "data-1", json!([los_pollos_amp()]))
                .with_record("data", "data-2", json!([good_place_eats_amp()]))
                .with_icon(los_pollos_icon())
                .with_icon(good_place_eats_icon()),
        );
        store.ingest(SuggestIngestionConstraints::all_providers());
        assert!(store.count_rows("suggestions") > 0);
        assert!(store.count_rows("keywords") > 0);
        assert!(store.count_rows("icons") > 0);

        store.inner.clear()?;
        assert!(store.count_rows("suggestions") == 0);
        assert!(store.count_rows("keywords") == 0);
        assert!(store.count_rows("icons") == 0);

        Ok(())
    }

    /// Tests querying suggestions.
    #[test]
    fn query() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(
            MockRemoteSettingsClient::default()
                .with_record(
                    "data",
                    "data-1",
                    json!([
                        good_place_eats_amp(),
                        california_wiki(),
                        caltech_wiki(),
                        multimatch_wiki(),
                    ]),
                )
                .with_record(
                    "amo-suggestions",
                    "data-2",
                    json!([relay_amo(), multimatch_amo(),]),
                )
                .with_record(
                    "pocket-suggestions",
                    "data-3",
                    json!([burnout_pocket(), multimatch_pocket(),]),
                )
                .with_record("yelp-suggestions", "data-4", json!([ramen_yelp(),]))
                .with_record("mdn-suggestions", "data-5", json!([array_mdn(),]))
                .with_icon(good_place_eats_icon())
                .with_icon(california_icon())
                .with_icon(caltech_icon())
                .with_icon(yelp_favicon())
                .with_icon(multimatch_wiki_icon()),
        );

        store.ingest(SuggestIngestionConstraints::all_providers());

        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::all_providers("")),
            vec![]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::all_providers("la")),
            vec![good_place_eats_suggestion("lasagna"),]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::all_providers("multimatch")),
            vec![
                multimatch_pocket_suggestion(true),
                multimatch_amo_suggestion(),
                multimatch_wiki_suggestion(),
            ]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::all_providers("MultiMatch")),
            vec![
                multimatch_pocket_suggestion(true),
                multimatch_amo_suggestion(),
                multimatch_wiki_suggestion(),
            ]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::all_providers("multimatch").limit(2)),
            vec![
                multimatch_pocket_suggestion(true),
                multimatch_amo_suggestion(),
            ],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amp("la")),
            vec![good_place_eats_suggestion("lasagna")],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::all_providers_except(
                "la",
                SuggestionProvider::Amp
            )),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::with_providers("la", vec![])),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::with_providers(
                "cal",
                vec![
                    SuggestionProvider::Amp,
                    SuggestionProvider::Amo,
                    SuggestionProvider::Pocket,
                ]
            )),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::wikipedia("cal")),
            vec![
                california_suggestion("california"),
                caltech_suggestion("california"),
            ],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::wikipedia("cal").limit(1)),
            vec![california_suggestion("california"),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::with_providers("cal", vec![])),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amo("spam")),
            vec![relay_suggestion()],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amo("masking")),
            vec![relay_suggestion()],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amo("masking e")),
            vec![relay_suggestion()],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amo("masking s")),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::with_providers(
                "soft",
                vec![SuggestionProvider::Amp, SuggestionProvider::Wikipedia]
            )),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::pocket("soft")),
            vec![burnout_suggestion(false),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::pocket("soft l")),
            vec![burnout_suggestion(false),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::pocket("sof")),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::pocket("burnout women")),
            vec![burnout_suggestion(true),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::pocket("burnout person")),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("best spicy ramen delivery in tokyo")),
            vec![ramen_suggestion(
                "best spicy ramen delivery in tokyo",
                "https://www.yelp.com/search?find_desc=best+spicy+ramen+delivery&find_loc=tokyo"
            ),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("BeSt SpIcY rAmEn DeLiVeRy In ToKyO")),
            vec![ramen_suggestion(
                "BeSt SpIcY rAmEn DeLiVeRy In ToKyO",
                "https://www.yelp.com/search?find_desc=BeSt+SpIcY+rAmEn+DeLiVeRy&find_loc=ToKyO"
            ),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("best ramen delivery in tokyo")),
            vec![ramen_suggestion(
                "best ramen delivery in tokyo",
                "https://www.yelp.com/search?find_desc=best+ramen+delivery&find_loc=tokyo"
            ),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp(
                "best invalid_ramen delivery in tokyo"
            )),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("best in tokyo")),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("super best ramen in tokyo")),
            vec![ramen_suggestion(
                "super best ramen in tokyo",
                "https://www.yelp.com/search?find_desc=super+best+ramen&find_loc=tokyo"
            ),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("invalid_best ramen in tokyo")),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen delivery in tokyo")),
            vec![ramen_suggestion(
                "ramen delivery in tokyo",
                "https://www.yelp.com/search?find_desc=ramen+delivery&find_loc=tokyo"
            ),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen super delivery in tokyo")),
            vec![ramen_suggestion(
                "ramen super delivery in tokyo",
                "https://www.yelp.com/search?find_desc=ramen+super+delivery&find_loc=tokyo"
            ),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen invalid_delivery in tokyo")),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen in tokyo")),
            vec![ramen_suggestion(
                "ramen in tokyo",
                "https://www.yelp.com/search?find_desc=ramen&find_loc=tokyo"
            ),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen near tokyo")),
            vec![ramen_suggestion(
                "ramen near tokyo",
                "https://www.yelp.com/search?find_desc=ramen&find_loc=tokyo"
            ),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen invalid_in tokyo")),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen in San Francisco")),
            vec![ramen_suggestion(
                "ramen in San Francisco",
                "https://www.yelp.com/search?find_desc=ramen&find_loc=San+Francisco"
            ),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen in")),
            vec![ramen_suggestion(
                "ramen in",
                "https://www.yelp.com/search?find_desc=ramen"
            ),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen near by")),
            vec![ramen_suggestion(
                "ramen near by",
                "https://www.yelp.com/search?find_desc=ramen+near+by"
            )
            .has_location_sign(false),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen near me")),
            vec![ramen_suggestion(
                "ramen near me",
                "https://www.yelp.com/search?find_desc=ramen+near+me"
            )
            .has_location_sign(false),],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen near by tokyo")),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen")),
            vec![
                ramen_suggestion("ramen", "https://www.yelp.com/search?find_desc=ramen")
                    .has_location_sign(false),
            ],
        );
        // Test an extremely long yelp query
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp(
                "012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
            )),
            vec![
                ramen_suggestion(
                    "012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789",
                    "https://www.yelp.com/search?find_desc=012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
                ).has_location_sign(false),
            ],
        );
        // This query is over the limit and no suggestions should be returned
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp(
                "012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789Z"
            )),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("best delivery")),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("same_modifier same_modifier")),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("same_modifier ")),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("yelp ramen")),
            vec![
                ramen_suggestion("ramen", "https://www.yelp.com/search?find_desc=ramen")
                    .has_location_sign(false),
            ],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("yelp keyword ramen")),
            vec![
                ramen_suggestion("ramen", "https://www.yelp.com/search?find_desc=ramen")
                    .has_location_sign(false),
            ],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen in tokyo yelp")),
            vec![ramen_suggestion(
                "ramen in tokyo",
                "https://www.yelp.com/search?find_desc=ramen&find_loc=tokyo"
            )],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen in tokyo yelp keyword")),
            vec![ramen_suggestion(
                "ramen in tokyo",
                "https://www.yelp.com/search?find_desc=ramen&find_loc=tokyo"
            )],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("yelp ramen yelp")),
            vec![
                ramen_suggestion("ramen", "https://www.yelp.com/search?find_desc=ramen")
                    .has_location_sign(false)
            ],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("best yelp ramen")),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("Spicy R")),
            vec![ramen_suggestion(
                "Spicy Ramen",
                "https://www.yelp.com/search?find_desc=Spicy+Ramen"
            )
            .has_location_sign(false)
            .subject_exact_match(false)],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("BeSt             Ramen")),
            vec![ramen_suggestion(
                "BeSt Ramen",
                "https://www.yelp.com/search?find_desc=BeSt+Ramen"
            )
            .has_location_sign(false)],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("BeSt             Spicy R")),
            vec![ramen_suggestion(
                "BeSt Spicy Ramen",
                "https://www.yelp.com/search?find_desc=BeSt+Spicy+Ramen"
            )
            .has_location_sign(false)
            .subject_exact_match(false)],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("BeSt             R")),
            vec![],
        );
        assert_eq!(store.fetch_suggestions(SuggestionQuery::yelp("r")), vec![],);
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ra")),
            vec![
                ramen_suggestion("rats", "https://www.yelp.com/search?find_desc=rats")
                    .has_location_sign(false)
                    .subject_exact_match(false)
            ],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("ram")),
            vec![
                ramen_suggestion("ramen", "https://www.yelp.com/search?find_desc=ramen")
                    .has_location_sign(false)
                    .subject_exact_match(false)
            ],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("rac")),
            vec![
                ramen_suggestion("raccoon", "https://www.yelp.com/search?find_desc=raccoon")
                    .has_location_sign(false)
                    .subject_exact_match(false)
            ],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("best r")),
            vec![],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("best ra")),
            vec![ramen_suggestion(
                "best rats",
                "https://www.yelp.com/search?find_desc=best+rats"
            )
            .has_location_sign(false)
            .subject_exact_match(false)],
        );

        Ok(())
    }

    // Tests querying AMP / Wikipedia / Pocket
    #[test]
    fn query_with_multiple_providers_and_diff_scores() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(
            // Create a data set where one keyword matches multiple suggestions from each provider
            // where the scores are manually set.  We will test that the fetched suggestions are in
            // the correct order.
            MockRemoteSettingsClient::default()
                .with_record(
                    "data",
                    "data-1",
                    json!([
                        los_pollos_amp().merge(json!({
                            "keywords": ["amp wiki match"],
                            "score": 0.3,
                        })),
                        good_place_eats_amp().merge(json!({
                            "keywords": ["amp wiki match"],
                            "score": 0.1,
                        })),
                        california_wiki().merge(json!({
                            "keywords": ["amp wiki match", "pocket wiki match"],
                        })),
                    ]),
                )
                .with_record(
                    "pocket-suggestions",
                    "data-3",
                    json!([
                        burnout_pocket().merge(json!({
                            "lowConfidenceKeywords": ["work-life balance", "pocket wiki match"],
                            "score": 0.05,
                        })),
                        multimatch_pocket().merge(json!({
                            "highConfidenceKeywords": ["pocket wiki match"],
                            "score": 0.88,
                        })),
                    ]),
                )
                .with_icon(los_pollos_icon())
                .with_icon(good_place_eats_icon())
                .with_icon(california_icon()),
        );

        store.ingest(SuggestIngestionConstraints::all_providers());
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::all_providers("amp wiki match")),
            vec![
                los_pollos_suggestion("amp wiki match").with_score(0.3),
                // Wikipedia entries default to a 0.2 score
                california_suggestion("amp wiki match"),
                good_place_eats_suggestion("amp wiki match").with_score(0.1),
            ]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::all_providers("amp wiki match").limit(2)),
            vec![
                los_pollos_suggestion("amp wiki match").with_score(0.3),
                california_suggestion("amp wiki match"),
            ]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::all_providers("pocket wiki match")),
            vec![
                multimatch_pocket_suggestion(true).with_score(0.88),
                california_suggestion("pocket wiki match"),
                burnout_suggestion(false).with_score(0.05),
            ]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::all_providers("pocket wiki match").limit(1)),
            vec![multimatch_pocket_suggestion(true).with_score(0.88),]
        );
        // test duplicate providers
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::with_providers(
                "work-life balance",
                vec![SuggestionProvider::Pocket, SuggestionProvider::Pocket],
            )),
            vec![burnout_suggestion(false).with_score(0.05),]
        );

        Ok(())
    }

    // Tests querying multiple suggestions with multiple keywords with same prefix keyword
    #[test]
    fn query_with_amp_mobile_provider() -> anyhow::Result<()> {
        before_each();

        // Use the exact same data for both the Amp and AmpMobile record
        let store = TestStore::new(
            MockRemoteSettingsClient::default()
                .with_record(
                    "amp-mobile-suggestions",
                    "amp-mobile-1",
                    json!([good_place_eats_amp()]),
                )
                .with_record("data", "data-1", json!([good_place_eats_amp()]))
                // This icon is shared by both records which is kind of weird and probably not how
                // things would work in practice, but it's okay for the tests.
                .with_icon(good_place_eats_icon()),
        );
        store.ingest(SuggestIngestionConstraints::all_providers());
        // The query results should be exactly the same for both the Amp and AmpMobile data
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amp_mobile("las")),
            vec![good_place_eats_suggestion("lasagna")]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amp("las")),
            vec![good_place_eats_suggestion("lasagna")]
        );
        Ok(())
    }

    /// Tests ingesting malformed Remote Settings records that we understand,
    /// but that are missing fields, or aren't in the format we expect.
    #[test]
    fn ingest_malformed() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(
            MockRemoteSettingsClient::default()
                // Amp/Wikipedia record without an attachment.
                .with_record_but_no_attachment("data", "data-1")
                // Icon record without an attachment.
                .with_record_but_no_attachment("icon", "icon-1")
                // Icon record with an ID that's not `icon-{id}`, so suggestions in
                // the data attachment won't be able to reference it.
                .with_record("icon", "bad-icon-id", json!("i-am-an-icon")),
        );

        store.ingest(SuggestIngestionConstraints::all_providers());

        store.read(|dao| {
            assert_eq!(
                dao.conn
                    .query_one::<i64>("SELECT count(*) FROM suggestions")?,
                0
            );
            assert_eq!(dao.conn.query_one::<i64>("SELECT count(*) FROM icons")?, 0);

            Ok(())
        })?;

        Ok(())
    }

    /// Tests that we only ingest providers that we're concerned with.
    #[test]
    fn ingest_constraints_provider() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(
            MockRemoteSettingsClient::default()
                .with_record("data", "data-1", json!([los_pollos_amp()]))
                .with_record("yelp-suggestions", "yelp-1", json!([ramen_yelp()]))
                .with_icon(los_pollos_icon()),
        );

        let constraints = SuggestIngestionConstraints {
            providers: Some(vec![SuggestionProvider::Amp, SuggestionProvider::Pocket]),
            ..SuggestIngestionConstraints::all_providers()
        };
        store.ingest(constraints);

        // This should have been ingested
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amp("lo")),
            vec![los_pollos_suggestion("los")]
        );
        // This should not have been ingested, since it wasn't in the providers list
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::yelp("best ramen")),
            vec![]
        );

        Ok(())
    }

    /// Tests that records with invalid attachments are ignored
    #[test]
    fn skip_over_invalid_records() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(
            MockRemoteSettingsClient::default()
                // valid record
                .with_record("data", "data-1", json!([good_place_eats_amp()]))
                // This attachment is missing the `title` field and is invalid
                .with_record(
                    "data",
                    "data-2",
                    json!([{
                            "id": 1,
                            "advertiser": "Los Pollos Hermanos",
                            "iab_category": "8 - Food & Drink",
                            "keywords": ["lo", "los", "los pollos"],
                            "url": "https://www.lph-nm.biz",
                            "icon": "5678",
                            "impression_url": "https://example.com/impression_url",
                            "click_url": "https://example.com/click_url",
                            "score": 0.3
                    }]),
                )
                .with_icon(good_place_eats_icon()),
        );

        store.ingest(SuggestIngestionConstraints::all_providers());

        // Test that the valid record was read
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amp("la")),
            vec![good_place_eats_suggestion("lasagna")]
        );
        // Test that the invalid record was skipped
        assert_eq!(store.fetch_suggestions(SuggestionQuery::amp("lo")), vec![]);

        Ok(())
    }

    #[test]
    fn query_mdn() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(MockRemoteSettingsClient::default().with_record(
            "mdn-suggestions",
            "mdn-1",
            json!([array_mdn()]),
        ));
        store.ingest(SuggestIngestionConstraints::all_providers());
        // prefix
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::mdn("array")),
            vec![array_suggestion(),]
        );
        // prefix + partial suffix
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::mdn("array java")),
            vec![array_suggestion(),]
        );
        // prefix + entire suffix
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::mdn("javascript array")),
            vec![array_suggestion(),]
        );
        // partial prefix word
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::mdn("wild")),
            vec![]
        );
        // single word
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::mdn("wildcard")),
            vec![array_suggestion()]
        );
        Ok(())
    }

    #[test]
    fn query_no_yelp_icon_data() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(
            MockRemoteSettingsClient::default().with_record(
                "yelp-suggestions",
                "yelp-1",
                json!([ramen_yelp()]),
            ), // Note: yelp_favicon() is missing
        );
        store.ingest(SuggestIngestionConstraints::all_providers());
        assert!(matches!(
            store.fetch_suggestions(SuggestionQuery::yelp("ramen")).as_slice(),
            [Suggestion::Yelp { icon, icon_mimetype, .. }] if icon.is_none() && icon_mimetype.is_none()
        ));

        Ok(())
    }

    #[test]
    fn weather() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(MockRemoteSettingsClient::default().with_inline_record(
            "weather",
            "weather-1",
            json!({
                "weather": {
                    "min_keyword_length": 3,
                    "keywords": ["ab", "xyz", "weather"],
                    "score": "0.24"
                },
            }),
        ));
        store.ingest(SuggestIngestionConstraints::all_providers());
        // No match since the query doesn't match any keyword
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("xab")),
            vec![]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("abx")),
            vec![]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("xxyz")),
            vec![]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("xyzx")),
            vec![]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("weatherx")),
            vec![]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("xweather")),
            vec![]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("xwea")),
            vec![]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("x   weather")),
            vec![]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("   weather x")),
            vec![]
        );
        // No match since the query is too short
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("xy")),
            vec![]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("ab")),
            vec![]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("we")),
            vec![]
        );
        // Matches
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("xyz")),
            vec![Suggestion::Weather { score: 0.24 },]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("wea")),
            vec![Suggestion::Weather { score: 0.24 },]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("weat")),
            vec![Suggestion::Weather { score: 0.24 },]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("weath")),
            vec![Suggestion::Weather { score: 0.24 },]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("weathe")),
            vec![Suggestion::Weather { score: 0.24 },]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("weather")),
            vec![Suggestion::Weather { score: 0.24 },]
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::weather("  weather  ")),
            vec![Suggestion::Weather { score: 0.24 },]
        );

        assert_eq!(
            store.fetch_provider_config(SuggestionProvider::Weather),
            Some(SuggestProviderConfig::Weather {
                min_keyword_length: 3,
            })
        );

        Ok(())
    }

    #[test]
    fn fetch_global_config() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(MockRemoteSettingsClient::default().with_inline_record(
            "configuration",
            "configuration-1",
            json!({
                "configuration": {
                    "show_less_frequently_cap": 3,
                },
            }),
        ));
        store.ingest(SuggestIngestionConstraints::all_providers());
        assert_eq!(
            store.fetch_global_config(),
            SuggestGlobalConfig {
                show_less_frequently_cap: 3,
            }
        );

        Ok(())
    }

    #[test]
    fn fetch_global_config_default() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(MockRemoteSettingsClient::default());
        store.ingest(SuggestIngestionConstraints::all_providers());
        assert_eq!(
            store.fetch_global_config(),
            SuggestGlobalConfig {
                show_less_frequently_cap: 0,
            }
        );

        Ok(())
    }

    #[test]
    fn fetch_provider_config_none() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(MockRemoteSettingsClient::default());
        store.ingest(SuggestIngestionConstraints::all_providers());
        assert_eq!(store.fetch_provider_config(SuggestionProvider::Amp), None);
        assert_eq!(
            store.fetch_provider_config(SuggestionProvider::Weather),
            None
        );

        Ok(())
    }

    #[test]
    fn fetch_provider_config_other() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(MockRemoteSettingsClient::default().with_inline_record(
            "weather",
            "weather-1",
            json!({
                "weather": {
                    "min_keyword_length": 3,
                    "keywords": ["weather"],
                    "score": "0.24"
                },
            }),
        ));
        store.ingest(SuggestIngestionConstraints::all_providers());
        // Getting the config for a different provider should return None.
        assert_eq!(store.fetch_provider_config(SuggestionProvider::Amp), None);
        Ok(())
    }

    #[test]
    fn remove_dismissed_suggestions() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(
            MockRemoteSettingsClient::default()
                .with_record(
                    "data",
                    "data-1",
                    json!([
                        good_place_eats_amp().merge(json!({"keywords": ["cats"]})),
                        california_wiki().merge(json!({"keywords": ["cats"]})),
                    ]),
                )
                .with_record(
                    "amo-suggestions",
                    "amo-1",
                    json!([relay_amo().merge(json!({"keywords": ["cats"]})),]),
                )
                .with_record(
                    "pocket-suggestions",
                    "pocket-1",
                    json!([burnout_pocket().merge(json!({
                        "lowConfidenceKeywords": ["cats"],
                    }))]),
                )
                .with_record(
                    "mdn-suggestions",
                    "mdn-1",
                    json!([array_mdn().merge(json!({"keywords": ["cats"]})),]),
                )
                .with_record(
                    "amp-mobile-suggestions",
                    "amp-mobile-1",
                    json!([a1a_amp_mobile().merge(json!({"keywords": ["cats"]})),]),
                )
                .with_icon(good_place_eats_icon())
                .with_icon(caltech_icon()),
        );
        store.ingest(SuggestIngestionConstraints::all_providers());

        // A query for cats should return all suggestions
        let query = SuggestionQuery::all_providers("cats");
        let results = store.fetch_suggestions(query.clone());
        assert_eq!(results.len(), 6);

        for result in results {
            store
                .inner
                .dismiss_suggestion(result.raw_url().unwrap().to_string())?;
        }

        // After dismissing the suggestions, the next query shouldn't return them
        assert_eq!(store.fetch_suggestions(query.clone()).len(), 0);

        // Clearing the dismissals should cause them to be returned again
        store.inner.clear_dismissed_suggestions()?;
        assert_eq!(store.fetch_suggestions(query.clone()).len(), 6);

        Ok(())
    }

    #[test]
    fn query_fakespot() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(
            MockRemoteSettingsClient::default()
                .with_record(
                    "fakespot-suggestions",
                    "fakespot-1",
                    json!([snowglobe_fakespot(), simpsons_fakespot()]),
                )
                .with_icon(fakespot_amazon_icon()),
        );
        store.ingest(SuggestIngestionConstraints::all_providers());
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::fakespot("globe")),
            vec![snowglobe_suggestion().with_fakespot_product_type_bonus(0.5)],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::fakespot("simpsons")),
            vec![simpsons_suggestion()],
        );
        // The snowglobe suggestion should come before the simpsons one, since `snow` is a partial
        // match on the product_type field.
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::fakespot("snow")),
            vec![
                snowglobe_suggestion().with_fakespot_product_type_bonus(0.5),
                simpsons_suggestion(),
            ],
        );
        // Test FTS by using a query where the keywords are separated in the source text
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::fakespot("simpsons snow")),
            vec![simpsons_suggestion()],
        );
        // Special characters should be stripped out
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::fakespot("simpsons + snow")),
            vec![simpsons_suggestion()],
        );

        Ok(())
    }

    #[test]
    fn fakespot_keywords() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(
            MockRemoteSettingsClient::default()
                .with_record(
                    "fakespot-suggestions",
                    "fakespot-1",
                    json!([
                        // Snow normally returns the snowglobe first.  Test using the keyword field
                        // to force the simpsons result first.
                        snowglobe_fakespot(),
                        simpsons_fakespot().merge(json!({"keywords": "snow"})),
                    ]),
                )
                .with_icon(fakespot_amazon_icon()),
        );
        store.ingest(SuggestIngestionConstraints::all_providers());
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::fakespot("snow")),
            vec![
                simpsons_suggestion().with_fakespot_keyword_bonus(),
                snowglobe_suggestion().with_fakespot_product_type_bonus(0.5),
            ],
        );
        Ok(())
    }

    #[test]
    fn fakespot_prefix_matching() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(
            MockRemoteSettingsClient::default()
                .with_record(
                    "fakespot-suggestions",
                    "fakespot-1",
                    json!([snowglobe_fakespot(), simpsons_fakespot()]),
                )
                .with_icon(fakespot_amazon_icon()),
        );
        store.ingest(SuggestIngestionConstraints::all_providers());
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::fakespot("simp")),
            vec![simpsons_suggestion()],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::fakespot("simps")),
            vec![simpsons_suggestion()],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::fakespot("simpson")),
            vec![simpsons_suggestion()],
        );

        Ok(())
    }

    #[test]
    fn fakespot_updates_and_deletes() -> anyhow::Result<()> {
        before_each();

        let mut store = TestStore::new(
            MockRemoteSettingsClient::default()
                .with_record(
                    "fakespot-suggestions",
                    "fakespot-1",
                    json!([snowglobe_fakespot(), simpsons_fakespot()]),
                )
                .with_icon(fakespot_amazon_icon()),
        );
        store.ingest(SuggestIngestionConstraints::all_providers());

        // Update the snapshot so that:
        //   - The Simpsons entry is deleted
        //   - Snow globes now use sea glass instead of glitter
        store.client_mut().update_record(
            "fakespot-suggestions",
            "fakespot-1",
            json!([
                snowglobe_fakespot().merge(json!({"title": "Make Your Own Sea Glass Snow Globes"}))
            ]),
        );
        store.ingest(SuggestIngestionConstraints::all_providers());

        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::fakespot("glitter")),
            vec![],
        );
        assert!(matches!(
            store.fetch_suggestions(SuggestionQuery::fakespot("sea glass")).as_slice(),
            [
                Suggestion::Fakespot { title, .. }
            ]
            if title == "Make Your Own Sea Glass Snow Globes"
        ));

        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::fakespot("simpsons")),
            vec![],
        );

        Ok(())
    }

    /// Test the pathological case where we ingest records with the same id, but from different
    /// collections
    #[test]
    fn same_record_id_different_collections() -> anyhow::Result<()> {
        before_each();

        let mut store = TestStore::new(
            MockRemoteSettingsClient::default()
                // This record is in the fakespot-suggest-products collection
                .with_record(
                    "fakespot-suggestions",
                    "fakespot-1",
                    json!([snowglobe_fakespot()]),
                )
                // This record is in the quicksuggest collection, but it has a fakespot record ID
                // for some reason.
                .with_record("data", "fakespot-1", json![los_pollos_amp()])
                .with_icon(los_pollos_icon())
                .with_icon(fakespot_amazon_icon()),
        );
        store.ingest(SuggestIngestionConstraints::all_providers());
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::fakespot("globe")),
            vec![snowglobe_suggestion().with_fakespot_product_type_bonus(0.5)],
        );
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::amp("lo")),
            vec![los_pollos_suggestion("los")],
        );
        // Test deleting one of the records
        store
            .client_mut()
            .delete_record("quicksuggest", "fakespot-1")
            .delete_icon(los_pollos_icon());
        store.ingest(SuggestIngestionConstraints::all_providers());
        // FIXME(Bug 1912283): this setup currently deletes both suggestions, since
        // `drop_suggestions` only checks against record ID.
        //
        // assert_eq!(
        //     store.fetch_suggestions(SuggestionQuery::fakespot("globe")),
        //     vec![snowglobe_suggestion()],
        // );
        // assert_eq!(
        //     store.fetch_suggestions(SuggestionQuery::amp("lo")),
        //     vec![],
        // );

        // However, we can test that the ingested records table has the correct entries

        let record_keys = store
            .read(|dao| dao.get_ingested_records())
            .unwrap()
            .into_iter()
            .map(|r| format!("{}:{}", r.collection, r.id.as_str()))
            .collect::<Vec<_>>();
        assert_eq!(
            record_keys
                .iter()
                .map(String::as_str)
                .collect::<HashSet<_>>(),
            HashSet::from([
                "quicksuggest:icon-fakespot-amazon",
                "fakespot-suggest-products:fakespot-1"
            ]),
        );
        Ok(())
    }

    #[test]
    fn exposure_basic() -> anyhow::Result<()> {
        before_each();

        let store = TestStore::new(
            MockRemoteSettingsClient::default()
                .with_full_record(
                    "exposure-suggestions",
                    "exposure-0",
                    Some(json!({
                        "suggestion_type": "aaa",
                    })),
                    Some(json!({
                        "keywords": [
                            "aaa keyword",
                            "both keyword",
                            ["common prefix", [" aaa"]],
                            ["choco", ["bo", "late"]],
                            ["dup", ["licate 1", "licate 2"]],
                        ],
                    })),
                )
                .with_full_record(
                    "exposure-suggestions",
                    "exposure-1",
                    Some(json!({
                        "suggestion_type": "bbb",
                    })),
                    Some(json!({
                        "keywords": [
                            "bbb keyword",
                            "both keyword",
                            ["common prefix", [" bbb"]],
                        ],
                    })),
                ),
        );
        store.ingest(SuggestIngestionConstraints {
            providers: Some(vec![SuggestionProvider::Exposure]),
            provider_constraints: Some(SuggestionProviderConstraints {
                exposure_suggestion_types: Some(vec!["aaa".to_string(), "bbb".to_string()]),
            }),
            ..SuggestIngestionConstraints::all_providers()
        });

        let no_matches = vec!["aaa", "both", "common prefi", "choc", "chocolate extra"];
        for query in &no_matches {
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["aaa"])),
                vec![],
            );
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["bbb"])),
                vec![],
            );
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["aaa", "bbb"])),
                vec![],
            );
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["aaa", "zzz"])),
                vec![],
            );
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["zzz"])),
                vec![],
            );
        }

        let aaa_only_matches = vec![
            "aaa keyword",
            "common prefix a",
            "common prefix aa",
            "common prefix aaa",
            "choco",
            "chocob",
            "chocobo",
            "chocol",
            "chocolate",
            "dup",
            "dupl",
            "duplicate",
            "duplicate ",
            "duplicate 1",
            "duplicate 2",
        ];
        for query in &aaa_only_matches {
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["aaa"])),
                vec![Suggestion::Exposure {
                    suggestion_type: "aaa".into(),
                    score: 1.0,
                }],
            );
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["aaa", "bbb"])),
                vec![Suggestion::Exposure {
                    suggestion_type: "aaa".into(),
                    score: 1.0,
                }],
            );
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["bbb", "aaa"])),
                vec![Suggestion::Exposure {
                    suggestion_type: "aaa".into(),
                    score: 1.0,
                }],
            );
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["aaa", "zzz"])),
                vec![Suggestion::Exposure {
                    suggestion_type: "aaa".into(),
                    score: 1.0,
                }],
            );
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["zzz", "aaa"])),
                vec![Suggestion::Exposure {
                    suggestion_type: "aaa".into(),
                    score: 1.0,
                }],
            );
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["bbb"])),
                vec![],
            );
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["zzz"])),
                vec![],
            );
        }

        let bbb_only_matches = vec![
            "bbb keyword",
            "common prefix b",
            "common prefix bb",
            "common prefix bbb",
        ];
        for query in &bbb_only_matches {
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["bbb"])),
                vec![Suggestion::Exposure {
                    suggestion_type: "bbb".into(),
                    score: 1.0,
                }],
            );
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["bbb", "aaa"])),
                vec![Suggestion::Exposure {
                    suggestion_type: "bbb".into(),
                    score: 1.0,
                }],
            );
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["aaa", "bbb"])),
                vec![Suggestion::Exposure {
                    suggestion_type: "bbb".into(),
                    score: 1.0,
                }],
            );
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["bbb", "zzz"])),
                vec![Suggestion::Exposure {
                    suggestion_type: "bbb".into(),
                    score: 1.0,
                }],
            );
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["zzz", "bbb"])),
                vec![Suggestion::Exposure {
                    suggestion_type: "bbb".into(),
                    score: 1.0,
                }],
            );
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["aaa"])),
                vec![],
            );
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["zzz"])),
                vec![],
            );
        }

        let both_matches = vec!["both keyword", "common prefix", "common prefix "];
        for query in &both_matches {
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["aaa"])),
                vec![Suggestion::Exposure {
                    suggestion_type: "aaa".into(),
                    score: 1.0,
                }],
            );
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["bbb"])),
                vec![Suggestion::Exposure {
                    suggestion_type: "bbb".into(),
                    score: 1.0,
                }],
            );
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["aaa", "bbb"])),
                vec![
                    Suggestion::Exposure {
                        suggestion_type: "aaa".into(),
                        score: 1.0,
                    },
                    Suggestion::Exposure {
                        suggestion_type: "bbb".into(),
                        score: 1.0,
                    },
                ],
            );
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["bbb", "aaa"])),
                vec![
                    Suggestion::Exposure {
                        suggestion_type: "aaa".into(),
                        score: 1.0,
                    },
                    Suggestion::Exposure {
                        suggestion_type: "bbb".into(),
                        score: 1.0,
                    },
                ],
            );
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["aaa", "zzz"])),
                vec![Suggestion::Exposure {
                    suggestion_type: "aaa".into(),
                    score: 1.0,
                }],
            );
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["zzz", "aaa"])),
                vec![Suggestion::Exposure {
                    suggestion_type: "aaa".into(),
                    score: 1.0,
                }],
            );
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["bbb", "zzz"])),
                vec![Suggestion::Exposure {
                    suggestion_type: "bbb".into(),
                    score: 1.0,
                }],
            );
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["zzz", "bbb"])),
                vec![Suggestion::Exposure {
                    suggestion_type: "bbb".into(),
                    score: 1.0,
                }],
            );
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["aaa", "zzz", "bbb"])),
                vec![
                    Suggestion::Exposure {
                        suggestion_type: "aaa".into(),
                        score: 1.0,
                    },
                    Suggestion::Exposure {
                        suggestion_type: "bbb".into(),
                        score: 1.0,
                    },
                ],
            );
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["zzz"])),
                vec![],
            );
        }

        Ok(())
    }

    #[test]
    fn exposure_spread_across_multiple_records() -> anyhow::Result<()> {
        before_each();

        let mut store = TestStore::new(
            MockRemoteSettingsClient::default()
                .with_full_record(
                    "exposure-suggestions",
                    "exposure-0",
                    Some(json!({
                        "suggestion_type": "aaa",
                    })),
                    Some(json!({
                        "keywords": [
                            "record 0 keyword",
                            ["sug", ["gest"]],
                        ],
                    })),
                )
                .with_full_record(
                    "exposure-suggestions",
                    "exposure-1",
                    Some(json!({
                        "suggestion_type": "aaa",
                    })),
                    Some(json!({
                        "keywords": [
                            "record 1 keyword",
                            ["sug", ["arplum"]],
                        ],
                    })),
                ),
        );
        store.ingest(SuggestIngestionConstraints {
            providers: Some(vec![SuggestionProvider::Exposure]),
            provider_constraints: Some(SuggestionProviderConstraints {
                exposure_suggestion_types: Some(vec!["aaa".to_string()]),
            }),
            ..SuggestIngestionConstraints::all_providers()
        });

        let matches = vec![
            "record 0 keyword",
            "sug",
            "sugg",
            "sugge",
            "sugges",
            "suggest",
            "record 1 keyword",
            "suga",
            "sugar",
            "sugarp",
            "sugarpl",
            "sugarplu",
            "sugarplum",
        ];
        for query in &matches {
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["aaa"])),
                vec![Suggestion::Exposure {
                    suggestion_type: "aaa".into(),
                    score: 1.0,
                }],
            );
        }

        // Delete the first record.
        store
            .client_mut()
            .delete_record(Collection::Quicksuggest.name(), "exposure-0");
        store.ingest(SuggestIngestionConstraints {
            providers: Some(vec![SuggestionProvider::Exposure]),
            provider_constraints: Some(SuggestionProviderConstraints {
                exposure_suggestion_types: Some(vec!["aaa".to_string()]),
            }),
            ..SuggestIngestionConstraints::all_providers()
        });

        // Keywords from the second record should still return the suggestion.
        let record_1_matches = vec![
            "record 1 keyword",
            "sug",
            "suga",
            "sugar",
            "sugarp",
            "sugarpl",
            "sugarplu",
            "sugarplum",
        ];
        for query in &record_1_matches {
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["aaa"])),
                vec![Suggestion::Exposure {
                    suggestion_type: "aaa".into(),
                    score: 1.0,
                }],
            );
        }

        // Keywords from the first record should not return the suggestion.
        let record_0_matches = vec!["record 0 keyword", "sugg", "sugge", "sugges", "suggest"];
        for query in &record_0_matches {
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, &["exposure-test"])),
                vec![]
            );
        }

        Ok(())
    }

    #[test]
    fn exposure_ingest() -> anyhow::Result<()> {
        before_each();

        // Create suggestions with types "aaa" and "bbb".
        let store = TestStore::new(
            MockRemoteSettingsClient::default()
                .with_full_record(
                    "exposure-suggestions",
                    "exposure-0",
                    Some(json!({
                        "suggestion_type": "aaa",
                    })),
                    Some(json!({
                        "keywords": ["aaa keyword", "both keyword"],
                    })),
                )
                .with_full_record(
                    "exposure-suggestions",
                    "exposure-1",
                    Some(json!({
                        "suggestion_type": "bbb",
                    })),
                    Some(json!({
                        "keywords": ["bbb keyword", "both keyword"],
                    })),
                ),
        );

        // Ingest but don't pass in any provider constraints. The records will
        // be ingested but their attachments won't be, so fetches shouldn't
        // return any suggestions.
        store.ingest(SuggestIngestionConstraints {
            providers: Some(vec![SuggestionProvider::Exposure]),
            provider_constraints: None,
            ..SuggestIngestionConstraints::all_providers()
        });

        let ingest_1_queries = [
            ("aaa keyword", vec!["aaa"]),
            ("aaa keyword", vec!["bbb"]),
            ("aaa keyword", vec!["aaa", "bbb"]),
            ("bbb keyword", vec!["aaa"]),
            ("bbb keyword", vec!["bbb"]),
            ("bbb keyword", vec!["aaa", "bbb"]),
            ("both keyword", vec!["aaa"]),
            ("both keyword", vec!["bbb"]),
            ("both keyword", vec!["aaa", "bbb"]),
        ];
        for (query, types) in &ingest_1_queries {
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, types)),
                vec![],
            );
        }

        // Ingest only the "bbb" suggestion. The "bbb" attachment should be
        // ingested, so "bbb" fetches should return the "bbb" suggestion.
        store.ingest(SuggestIngestionConstraints {
            providers: Some(vec![SuggestionProvider::Exposure]),
            provider_constraints: Some(SuggestionProviderConstraints {
                exposure_suggestion_types: Some(vec!["bbb".to_string()]),
            }),
            ..SuggestIngestionConstraints::all_providers()
        });

        let ingest_2_queries = [
            ("aaa keyword", vec!["aaa"], vec![]),
            ("aaa keyword", vec!["bbb"], vec![]),
            ("aaa keyword", vec!["aaa", "bbb"], vec![]),
            ("bbb keyword", vec!["aaa"], vec![]),
            ("bbb keyword", vec!["bbb"], vec!["bbb"]),
            ("bbb keyword", vec!["aaa", "bbb"], vec!["bbb"]),
            ("both keyword", vec!["aaa"], vec![]),
            ("both keyword", vec!["bbb"], vec!["bbb"]),
            ("both keyword", vec!["aaa", "bbb"], vec!["bbb"]),
        ];
        for (query, types, expected_types) in &ingest_2_queries {
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, types)),
                expected_types
                    .iter()
                    .map(|t| Suggestion::Exposure {
                        suggestion_type: t.to_string(),
                        score: 1.0,
                    })
                    .collect::<Vec<Suggestion>>(),
            );
        }

        // Now ingest the "aaa" suggestion.
        store.ingest(SuggestIngestionConstraints {
            providers: Some(vec![SuggestionProvider::Exposure]),
            provider_constraints: Some(SuggestionProviderConstraints {
                exposure_suggestion_types: Some(vec!["aaa".to_string()]),
            }),
            ..SuggestIngestionConstraints::all_providers()
        });

        let ingest_3_queries = [
            ("aaa keyword", vec!["aaa"], vec!["aaa"]),
            ("aaa keyword", vec!["bbb"], vec![]),
            ("aaa keyword", vec!["aaa", "bbb"], vec!["aaa"]),
            ("bbb keyword", vec!["aaa"], vec![]),
            ("bbb keyword", vec!["bbb"], vec!["bbb"]),
            ("bbb keyword", vec!["aaa", "bbb"], vec!["bbb"]),
            ("both keyword", vec!["aaa"], vec!["aaa"]),
            ("both keyword", vec!["bbb"], vec!["bbb"]),
            ("both keyword", vec!["aaa", "bbb"], vec!["aaa", "bbb"]),
        ];
        for (query, types, expected_types) in &ingest_3_queries {
            assert_eq!(
                store.fetch_suggestions(SuggestionQuery::exposure(query, types)),
                expected_types
                    .iter()
                    .map(|t| Suggestion::Exposure {
                        suggestion_type: t.to_string(),
                        score: 1.0,
                    })
                    .collect::<Vec<Suggestion>>(),
            );
        }

        Ok(())
    }

    #[test]
    fn exposure_ingest_new_record() -> anyhow::Result<()> {
        before_each();

        // Create an exposure suggestion and ingest it.
        let mut store = TestStore::new(MockRemoteSettingsClient::default().with_full_record(
            "exposure-suggestions",
            "exposure-0",
            Some(json!({
                "suggestion_type": "aaa",
            })),
            Some(json!({
                "keywords": ["old keyword"],
            })),
        ));
        store.ingest(SuggestIngestionConstraints {
            providers: Some(vec![SuggestionProvider::Exposure]),
            provider_constraints: Some(SuggestionProviderConstraints {
                exposure_suggestion_types: Some(vec!["aaa".to_string()]),
            }),
            ..SuggestIngestionConstraints::all_providers()
        });

        // Add a new record of the same exposure type.
        store.client_mut().add_full_record(
            "exposure-suggestions",
            "exposure-1",
            Some(json!({
                "suggestion_type": "aaa",
            })),
            Some(json!({
                "keywords": ["new keyword"],
            })),
        );

        // Ingest, but don't ingest the exposure type. The store will download
        // the new record but shouldn't ingest its attachment.
        store.ingest(SuggestIngestionConstraints {
            providers: Some(vec![SuggestionProvider::Exposure]),
            provider_constraints: None,
            ..SuggestIngestionConstraints::all_providers()
        });
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::exposure("new keyword", &["aaa"])),
            vec![],
        );

        // Ingest again with the exposure type. The new record will be
        // unchanged, but the store should now ingest its attachment.
        store.ingest(SuggestIngestionConstraints {
            providers: Some(vec![SuggestionProvider::Exposure]),
            provider_constraints: Some(SuggestionProviderConstraints {
                exposure_suggestion_types: Some(vec!["aaa".to_string()]),
            }),
            ..SuggestIngestionConstraints::all_providers()
        });

        // The keyword in the new attachment should match the suggestion,
        // confirming that the new record's attachment was ingested.
        assert_eq!(
            store.fetch_suggestions(SuggestionQuery::exposure("new keyword", &["aaa"])),
            vec![Suggestion::Exposure {
                suggestion_type: "aaa".to_string(),
                score: 1.0,
            }]
        );

        Ok(())
    }
}
