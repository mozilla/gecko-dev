// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::cell::{Cell, RefCell};
use std::collections::btree_map::Entry;
use std::collections::BTreeMap;
use std::fs;
use std::io;
use std::num::NonZeroU64;
use std::path::Path;
use std::str;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::RwLock;
use std::time::{Duration, Instant};

use crate::ErrorKind;

use malloc_size_of::MallocSizeOf;
use rkv::{StoreError, StoreOptions};

/// Unwrap a `Result`s `Ok` value or do the specified action.
///
/// This is an alternative to the question-mark operator (`?`),
/// when the other action should not be to return the error.
macro_rules! unwrap_or {
    ($expr:expr, $or:expr) => {
        match $expr {
            Ok(x) => x,
            Err(_) => {
                $or;
            }
        }
    };
}

macro_rules! measure_commit {
    ($this:ident, $expr:expr) => {{
        let now = ::std::time::Instant::now();
        let res = $expr;
        let elapsed = now.elapsed();
        if let Ok(elapsed) = elapsed.as_micros().try_into() {
            let mut samples = $this.write_timings.borrow_mut();
            samples.push(elapsed);
        }
        res
    }};
}

/// cbindgen:ignore
pub type Rkv = rkv::Rkv<rkv::backend::SafeModeEnvironment>;
/// cbindgen:ignore
pub type SingleStore = rkv::SingleStore<rkv::backend::SafeModeDatabase>;
/// cbindgen:ignore
pub type Writer<'t> = rkv::Writer<rkv::backend::SafeModeRwTransaction<'t>>;

#[derive(Debug)]
pub enum RkvLoadState {
    Ok,
    Err(rkv::StoreError),
}

pub fn rkv_new(path: &Path) -> std::result::Result<(Rkv, RkvLoadState), rkv::StoreError> {
    match Rkv::new::<rkv::backend::SafeMode>(path) {
        // An invalid file can mean:
        // 1. An empty file.
        // 2. A corrupted file.
        //
        // In both instances there's not much we can do.
        // Drop the data by removing the file, and start over.
        Err(rkv::StoreError::FileInvalid) => {
            let safebin = path.join("data.safe.bin");
            fs::remove_file(safebin).map_err(|_| rkv::StoreError::FileInvalid)?;
            // Now try again, we only handle that error once.
            let rkv = Rkv::new::<rkv::backend::SafeMode>(path)?;
            Ok((rkv, RkvLoadState::Err(rkv::StoreError::FileInvalid)))
        }
        Err(rkv::StoreError::DatabaseCorrupted) => {
            let safebin = path.join("data.safe.bin");
            fs::remove_file(safebin).map_err(|_| rkv::StoreError::DatabaseCorrupted)?;
            // Try again, only allowing the error once.
            let rkv = Rkv::new::<rkv::backend::SafeMode>(path)?;
            Ok((rkv, RkvLoadState::Err(rkv::StoreError::DatabaseCorrupted)))
        }
        other => {
            let rkv = other?;
            Ok((rkv, RkvLoadState::Ok))
        }
    }
}

use crate::common_metric_data::CommonMetricDataInternal;
use crate::metrics::Metric;
use crate::Glean;
use crate::Lifetime;
use crate::Result;

pub struct Database {
    /// Handle to the database environment.
    rkv: Rkv,

    /// Handles to the "lifetime" stores.
    ///
    /// A "store" is a handle to the underlying database.
    /// We keep them open for fast and frequent access.
    user_store: SingleStore,
    ping_store: SingleStore,
    application_store: SingleStore,

    /// If the `delay_ping_lifetime_io` Glean config option is `true`,
    /// we will save metrics with 'ping' lifetime data in a map temporarily
    /// so as to persist them to disk using rkv in bulk on demand.
    ping_lifetime_data: Option<RwLock<BTreeMap<String, Metric>>>,

    /// A count of how many database writes have been done since the last ping-lifetime flush.
    ///
    /// A ping-lifetime flush is automatically done after `ping_lifetime_threshold` writes.
    ///
    /// Only relevant if `delay_ping_lifetime_io` is set to `true`,
    ping_lifetime_count: AtomicUsize,

    /// Write-count threshold when to auto-flush. `0` disables it.
    ping_lifetime_threshold: usize,

    /// The last time the `lifetime=ping` data was flushed to disk.
    ///
    /// Data is flushed to disk automatically when the last flush was more than
    /// `ping_lifetime_max_time` ago.
    ///
    /// Only relevant if `delay_ping_lifetime_io` is set to `true`,
    ping_lifetime_store_ts: Cell<Instant>,

    /// After what time to auto-flush. 0 disables it.
    ping_lifetime_max_time: Duration,

    /// Initial file size when opening the database.
    file_size: Option<NonZeroU64>,

    /// RKV load state
    rkv_load_state: RkvLoadState,

    /// Times an Rkv write-commit took.
    /// Re-applied as samples in a timing distribution later.
    pub(crate) write_timings: RefCell<Vec<i64>>,
}

impl MallocSizeOf for Database {
    fn size_of(&self, _ops: &mut malloc_size_of::MallocSizeOfOps) -> usize {
        // TODO(bug 1960592): Fill in gaps.

        /*
        let mut n = 0;

        n += 0; // self.rkv.size_of(ops) -- not implemented.
        n += 0; // self.user_store.size_of(ops) -- not implemented.

        n += self
            .ping_lifetime_data
            .as_ref()
            .map(|_data| {
                // TODO(bug 1960592): servo's malloc_size_of implements it for BTreeMap.
                //let lock = data.read().unwrap();
                //(*lock).size_of(ops)
                0
            })
            .unwrap_or(0);

        n
        */

        0
    }
}

impl std::fmt::Debug for Database {
    fn fmt(&self, fmt: &mut std::fmt::Formatter) -> std::fmt::Result {
        fmt.debug_struct("Database")
            .field("rkv", &self.rkv)
            .field("user_store", &"SingleStore")
            .field("ping_store", &"SingleStore")
            .field("application_store", &"SingleStore")
            .field("ping_lifetime_data", &self.ping_lifetime_data)
            .finish()
    }
}

/// Calculate the  database size from all the files in the directory.
///
///  # Arguments
///
///  *`path` - The path to the directory
///
///  # Returns
///
/// Returns the non-zero combined size of all files in a directory,
/// or `None` on error or if the size is `0`.
fn database_size(dir: &Path) -> Option<NonZeroU64> {
    let mut total_size = 0;
    if let Ok(entries) = fs::read_dir(dir) {
        for entry in entries.flatten() {
            if let Ok(file_type) = entry.file_type() {
                if file_type.is_file() {
                    let path = entry.path();
                    if let Ok(metadata) = fs::metadata(path) {
                        total_size += metadata.len();
                    } else {
                        continue;
                    }
                }
            }
        }
    }

    NonZeroU64::new(total_size)
}

impl Database {
    /// Initializes the data store.
    ///
    /// This opens the underlying rkv store and creates
    /// the underlying directory structure.
    ///
    /// It also loads any Lifetime::Ping data that might be
    /// persisted, in case `delay_ping_lifetime_io` is set.
    pub fn new(
        data_path: &Path,
        delay_ping_lifetime_io: bool,
        ping_lifetime_threshold: usize,
        ping_lifetime_max_time: Duration,
    ) -> Result<Self> {
        let path = data_path.join("db");
        log::debug!("Database path: {:?}", path.display());
        let file_size = database_size(&path);

        let (rkv, rkv_load_state) = Self::open_rkv(&path)?;
        let user_store = rkv.open_single(Lifetime::User.as_str(), StoreOptions::create())?;
        let ping_store = rkv.open_single(Lifetime::Ping.as_str(), StoreOptions::create())?;
        let application_store =
            rkv.open_single(Lifetime::Application.as_str(), StoreOptions::create())?;
        let ping_lifetime_data = if delay_ping_lifetime_io {
            Some(RwLock::new(BTreeMap::new()))
        } else {
            None
        };

        // We are gonna write, so we allocate some capacity upfront.
        // The value was chosen at random.
        let write_timings = RefCell::new(Vec::with_capacity(64));

        let now = Instant::now();

        let db = Self {
            rkv,
            user_store,
            ping_store,
            application_store,
            ping_lifetime_data,
            ping_lifetime_count: AtomicUsize::new(0),
            ping_lifetime_threshold,
            ping_lifetime_store_ts: Cell::new(now),
            ping_lifetime_max_time,
            file_size,
            rkv_load_state,
            write_timings,
        };

        db.load_ping_lifetime_data();

        Ok(db)
    }

    /// Get the initial database file size.
    pub fn file_size(&self) -> Option<NonZeroU64> {
        self.file_size
    }

    /// Get the rkv load state.
    pub fn rkv_load_state(&self) -> Option<String> {
        if let RkvLoadState::Err(e) = &self.rkv_load_state {
            Some(e.to_string())
        } else {
            None
        }
    }

    fn get_store(&self, lifetime: Lifetime) -> &SingleStore {
        match lifetime {
            Lifetime::User => &self.user_store,
            Lifetime::Ping => &self.ping_store,
            Lifetime::Application => &self.application_store,
        }
    }

    /// Creates the storage directories and inits rkv.
    fn open_rkv(path: &Path) -> Result<(Rkv, RkvLoadState)> {
        fs::create_dir_all(path)?;

        let (rkv, load_state) = rkv_new(path)?;

        log::info!("Database initialized");
        Ok((rkv, load_state))
    }

    /// Build the key of the final location of the data in the database.
    /// Such location is built using the storage name and the metric
    /// key/name (if available).
    ///
    /// # Arguments
    ///
    /// * `storage_name` - the name of the storage to store/fetch data from.
    /// * `metric_key` - the optional metric key/name.
    ///
    /// # Returns
    ///
    /// A string representing the location in the database.
    fn get_storage_key(storage_name: &str, metric_key: Option<&str>) -> String {
        match metric_key {
            Some(k) => format!("{}#{}", storage_name, k),
            None => format!("{}#", storage_name),
        }
    }

    /// Loads Lifetime::Ping data from rkv to memory,
    /// if `delay_ping_lifetime_io` is set to true.
    ///
    /// Does nothing if it isn't or if there is not data to load.
    fn load_ping_lifetime_data(&self) {
        if let Some(ping_lifetime_data) = &self.ping_lifetime_data {
            let mut data = ping_lifetime_data
                .write()
                .expect("Can't read ping lifetime data");

            let reader = unwrap_or!(self.rkv.read(), return);
            let store = self.get_store(Lifetime::Ping);
            let mut iter = unwrap_or!(store.iter_start(&reader), return);

            while let Some(Ok((metric_id, value))) = iter.next() {
                let metric_id = match str::from_utf8(metric_id) {
                    Ok(metric_id) => metric_id.to_string(),
                    _ => continue,
                };
                let metric: Metric = match value {
                    rkv::Value::Blob(blob) => unwrap_or!(bincode::deserialize(blob), continue),
                    _ => continue,
                };

                data.insert(metric_id, metric);
            }
        }
    }

    /// Iterates with the provided transaction function
    /// over the requested data from the given storage.
    ///
    /// * If the storage is unavailable, the transaction function is never invoked.
    /// * If the read data cannot be deserialized it will be silently skipped.
    ///
    /// # Arguments
    ///
    /// * `lifetime` - The metric lifetime to iterate over.
    /// * `storage_name` - The storage name to iterate over.
    /// * `metric_key` - The metric key to iterate over. All metrics iterated over
    ///   will have this prefix. For example, if `metric_key` is of the form `{category}.`,
    ///   it will iterate over all metrics in the given category. If the `metric_key` is of the
    ///   form `{category}.{name}/`, the iterator will iterate over all specific metrics for
    ///   a given labeled metric. If not provided, the entire storage for the given lifetime
    ///   will be iterated over.
    /// * `transaction_fn` - Called for each entry being iterated over. It is
    ///   passed two arguments: `(metric_id: &[u8], metric: &Metric)`.
    ///
    /// # Panics
    ///
    /// This function will **not** panic on database errors.
    pub fn iter_store_from<F>(
        &self,
        lifetime: Lifetime,
        storage_name: &str,
        metric_key: Option<&str>,
        mut transaction_fn: F,
    ) where
        F: FnMut(&[u8], &Metric),
    {
        let iter_start = Self::get_storage_key(storage_name, metric_key);
        let len = iter_start.len();

        // Lifetime::Ping data is not immediately persisted to disk if
        // Glean has `delay_ping_lifetime_io` set to true
        if lifetime == Lifetime::Ping {
            if let Some(ping_lifetime_data) = &self.ping_lifetime_data {
                let data = ping_lifetime_data
                    .read()
                    .expect("Can't read ping lifetime data");
                for (key, value) in data.iter() {
                    if key.starts_with(&iter_start) {
                        let key = &key[len..];
                        transaction_fn(key.as_bytes(), value);
                    }
                }
                return;
            }
        }

        let reader = unwrap_or!(self.rkv.read(), return);
        let mut iter = unwrap_or!(
            self.get_store(lifetime).iter_from(&reader, &iter_start),
            return
        );

        while let Some(Ok((metric_id, value))) = iter.next() {
            if !metric_id.starts_with(iter_start.as_bytes()) {
                break;
            }

            let metric_id = &metric_id[len..];
            let metric: Metric = match value {
                rkv::Value::Blob(blob) => unwrap_or!(bincode::deserialize(blob), continue),
                _ => continue,
            };
            transaction_fn(metric_id, &metric);
        }
    }

    /// Determines if the storage has the given metric.
    ///
    /// If data cannot be read it is assumed that the storage does not have the metric.
    ///
    /// # Arguments
    ///
    /// * `lifetime` - The lifetime of the metric.
    /// * `storage_name` - The storage name to look in.
    /// * `metric_identifier` - The metric identifier.
    ///
    /// # Panics
    ///
    /// This function will **not** panic on database errors.
    pub fn has_metric(
        &self,
        lifetime: Lifetime,
        storage_name: &str,
        metric_identifier: &str,
    ) -> bool {
        let key = Self::get_storage_key(storage_name, Some(metric_identifier));

        // Lifetime::Ping data is not persisted to disk if
        // Glean has `delay_ping_lifetime_io` set to true
        if lifetime == Lifetime::Ping {
            if let Some(ping_lifetime_data) = &self.ping_lifetime_data {
                return ping_lifetime_data
                    .read()
                    .map(|data| data.contains_key(&key))
                    .unwrap_or(false);
            }
        }

        let reader = unwrap_or!(self.rkv.read(), return false);
        self.get_store(lifetime)
            .get(&reader, &key)
            .unwrap_or(None)
            .is_some()
    }

    /// Writes to the specified storage with the provided transaction function.
    ///
    /// If the storage is unavailable, it will return an error.
    ///
    /// # Panics
    ///
    /// * This function will **not** panic on database errors.
    fn write_with_store<F>(&self, store_name: Lifetime, mut transaction_fn: F) -> Result<()>
    where
        F: FnMut(Writer, &SingleStore) -> Result<()>,
    {
        let writer = self.rkv.write().unwrap();
        let store = self.get_store(store_name);
        transaction_fn(writer, store)
    }

    /// Records a metric in the underlying storage system.
    pub fn record(&self, glean: &Glean, data: &CommonMetricDataInternal, value: &Metric) {
        let name = data.identifier(glean);
        for ping_name in data.storage_names() {
            if glean.is_ping_enabled(ping_name) {
                if let Err(e) =
                    self.record_per_lifetime(data.inner.lifetime, ping_name, &name, value)
                {
                    log::error!(
                        "Failed to record metric '{}' into {}: {:?}",
                        data.base_identifier(),
                        ping_name,
                        e
                    );
                }
            }
        }
    }

    /// Records a metric in the underlying storage system, for a single lifetime.
    ///
    /// # Returns
    ///
    /// If the storage is unavailable or the write fails, no data will be stored and an error will be returned.
    ///
    /// Otherwise `Ok(())` is returned.
    ///
    /// # Panics
    ///
    /// This function will **not** panic on database errors.
    fn record_per_lifetime(
        &self,
        lifetime: Lifetime,
        storage_name: &str,
        key: &str,
        metric: &Metric,
    ) -> Result<()> {
        let final_key = Self::get_storage_key(storage_name, Some(key));

        // Lifetime::Ping data is not immediately persisted to disk if
        // Glean has `delay_ping_lifetime_io` set to true
        if lifetime == Lifetime::Ping {
            if let Some(ping_lifetime_data) = &self.ping_lifetime_data {
                let mut data = ping_lifetime_data
                    .write()
                    .expect("Can't read ping lifetime data");
                data.insert(final_key, metric.clone());

                // flush ping lifetime
                self.persist_ping_lifetime_data_if_full(&data)?;
                return Ok(());
            }
        }

        let encoded = bincode::serialize(&metric).expect("IMPOSSIBLE: Serializing metric failed");
        let value = rkv::Value::Blob(&encoded);

        let mut writer = self.rkv.write()?;
        self.get_store(lifetime)
            .put(&mut writer, final_key, &value)?;
        measure_commit!(self, writer.commit())?;
        Ok(())
    }

    /// Records the provided value, with the given lifetime,
    /// after applying a transformation function.
    pub fn record_with<F>(&self, glean: &Glean, data: &CommonMetricDataInternal, mut transform: F)
    where
        F: FnMut(Option<Metric>) -> Metric,
    {
        let name = data.identifier(glean);
        for ping_name in data.storage_names() {
            if glean.is_ping_enabled(ping_name) {
                if let Err(e) = self.record_per_lifetime_with(
                    data.inner.lifetime,
                    ping_name,
                    &name,
                    &mut transform,
                ) {
                    log::error!(
                        "Failed to record metric '{}' into {}: {:?}",
                        data.base_identifier(),
                        ping_name,
                        e
                    );
                }
            }
        }
    }

    /// Records a metric in the underlying storage system,
    /// after applying the given transformation function, for a single lifetime.
    ///
    /// # Returns
    ///
    /// If the storage is unavailable or the write fails, no data will be stored and an error will be returned.
    ///
    /// Otherwise `Ok(())` is returned.
    ///
    /// # Panics
    ///
    /// This function will **not** panic on database errors.
    fn record_per_lifetime_with<F>(
        &self,
        lifetime: Lifetime,
        storage_name: &str,
        key: &str,
        mut transform: F,
    ) -> Result<()>
    where
        F: FnMut(Option<Metric>) -> Metric,
    {
        let final_key = Self::get_storage_key(storage_name, Some(key));

        // Lifetime::Ping data is not persisted to disk if
        // Glean has `delay_ping_lifetime_io` set to true
        if lifetime == Lifetime::Ping {
            if let Some(ping_lifetime_data) = &self.ping_lifetime_data {
                let mut data = ping_lifetime_data
                    .write()
                    .expect("Can't access ping lifetime data as writable");
                let entry = data.entry(final_key);
                match entry {
                    Entry::Vacant(entry) => {
                        entry.insert(transform(None));
                    }
                    Entry::Occupied(mut entry) => {
                        let old_value = entry.get().clone();
                        entry.insert(transform(Some(old_value)));
                    }
                }

                // flush ping lifetime
                self.persist_ping_lifetime_data_if_full(&data)?;
                return Ok(());
            }
        }

        let mut writer = self.rkv.write()?;
        let store = self.get_store(lifetime);
        let new_value: Metric = {
            let old_value = store.get(&writer, &final_key)?;

            match old_value {
                Some(rkv::Value::Blob(blob)) => {
                    let old_value = bincode::deserialize(blob).ok();
                    transform(old_value)
                }
                _ => transform(None),
            }
        };

        let encoded =
            bincode::serialize(&new_value).expect("IMPOSSIBLE: Serializing metric failed");
        let value = rkv::Value::Blob(&encoded);
        store.put(&mut writer, final_key, &value)?;
        measure_commit!(self, writer.commit())?;
        Ok(())
    }

    /// Clears a storage (only Ping Lifetime).
    ///
    /// # Returns
    ///
    /// * If the storage is unavailable an error is returned.
    /// * If any individual delete fails, an error is returned, but other deletions might have
    ///   happened.
    ///
    /// Otherwise `Ok(())` is returned.
    ///
    /// # Panics
    ///
    /// This function will **not** panic on database errors.
    pub fn clear_ping_lifetime_storage(&self, storage_name: &str) -> Result<()> {
        // Lifetime::Ping data will be saved to `ping_lifetime_data`
        // in case `delay_ping_lifetime_io` is set to true
        if let Some(ping_lifetime_data) = &self.ping_lifetime_data {
            ping_lifetime_data
                .write()
                .expect("Can't access ping lifetime data as writable")
                .retain(|metric_id, _| !metric_id.starts_with(storage_name));
        }

        self.write_with_store(Lifetime::Ping, |mut writer, store| {
            let mut metrics = Vec::new();
            {
                let mut iter = store.iter_from(&writer, storage_name)?;
                while let Some(Ok((metric_id, _))) = iter.next() {
                    if let Ok(metric_id) = std::str::from_utf8(metric_id) {
                        if !metric_id.starts_with(storage_name) {
                            break;
                        }
                        metrics.push(metric_id.to_owned());
                    }
                }
            }

            let mut res = Ok(());
            for to_delete in metrics {
                if let Err(e) = store.delete(&mut writer, to_delete) {
                    log::warn!("Can't delete from store: {:?}", e);
                    res = Err(e);
                }
            }

            measure_commit!(self, writer.commit())?;
            Ok(res?)
        })
    }

    pub fn clear_lifetime_storage(&self, lifetime: Lifetime, storage_name: &str) -> Result<()> {
        self.write_with_store(lifetime, |mut writer, store| {
            let mut metrics = Vec::new();
            {
                let mut iter = store.iter_from(&writer, storage_name)?;
                while let Some(Ok((metric_id, _))) = iter.next() {
                    if let Ok(metric_id) = std::str::from_utf8(metric_id) {
                        if !metric_id.starts_with(storage_name) {
                            break;
                        }
                        metrics.push(metric_id.to_owned());
                    }
                }
            }

            let mut res = Ok(());
            for to_delete in metrics {
                if let Err(e) = store.delete(&mut writer, to_delete) {
                    log::warn!("Can't delete from store: {:?}", e);
                    res = Err(e);
                }
            }

            measure_commit!(self, writer.commit())?;
            Ok(res?)
        })
    }

    /// Removes a single metric from the storage.
    ///
    /// # Arguments
    ///
    /// * `lifetime` - the lifetime of the storage in which to look for the metric.
    /// * `storage_name` - the name of the storage to store/fetch data from.
    /// * `metric_id` - the metric category + name.
    ///
    /// # Returns
    ///
    /// * If the storage is unavailable an error is returned.
    /// * If the metric could not be deleted, an error is returned.
    ///
    /// Otherwise `Ok(())` is returned.
    ///
    /// # Panics
    ///
    /// This function will **not** panic on database errors.
    pub fn remove_single_metric(
        &self,
        lifetime: Lifetime,
        storage_name: &str,
        metric_id: &str,
    ) -> Result<()> {
        let final_key = Self::get_storage_key(storage_name, Some(metric_id));

        // Lifetime::Ping data is not persisted to disk if
        // Glean has `delay_ping_lifetime_io` set to true
        if lifetime == Lifetime::Ping {
            if let Some(ping_lifetime_data) = &self.ping_lifetime_data {
                let mut data = ping_lifetime_data
                    .write()
                    .expect("Can't access app lifetime data as writable");
                data.remove(&final_key);
            }
        }

        self.write_with_store(lifetime, |mut writer, store| {
            if let Err(e) = store.delete(&mut writer, final_key.clone()) {
                if self.ping_lifetime_data.is_some() {
                    // If ping_lifetime_data exists, it might be
                    // that data is in memory, but not yet in rkv.
                    return Ok(());
                }
                return Err(e.into());
            }
            measure_commit!(self, writer.commit())?;
            Ok(())
        })
    }

    /// Clears all the metrics in the database, for the provided lifetime.
    ///
    /// Errors are logged.
    ///
    /// # Panics
    ///
    /// * This function will **not** panic on database errors.
    pub fn clear_lifetime(&self, lifetime: Lifetime) {
        let res = self.write_with_store(lifetime, |mut writer, store| {
            store.clear(&mut writer)?;
            measure_commit!(self, writer.commit())?;
            Ok(())
        });

        if let Err(e) = res {
            // We try to clear everything.
            // If there was no data to begin with we encounter a `NotFound` error.
            // There's no point in logging that.
            if let ErrorKind::Rkv(StoreError::IoError(ioerr)) = e.kind() {
                if let io::ErrorKind::NotFound = ioerr.kind() {
                    log::debug!(
                        "Could not clear store for lifetime {:?}: {:?}",
                        lifetime,
                        ioerr
                    );
                    return;
                }
            }

            log::warn!("Could not clear store for lifetime {:?}: {:?}", lifetime, e);
        }
    }

    /// Clears all metrics in the database.
    ///
    /// Errors are logged.
    ///
    /// # Panics
    ///
    /// * This function will **not** panic on database errors.
    pub fn clear_all(&self) {
        if let Some(ping_lifetime_data) = &self.ping_lifetime_data {
            ping_lifetime_data
                .write()
                .expect("Can't access ping lifetime data as writable")
                .clear();
        }

        for lifetime in [Lifetime::User, Lifetime::Ping, Lifetime::Application].iter() {
            self.clear_lifetime(*lifetime);
        }
    }

    /// Persists ping_lifetime_data to disk.
    ///
    /// Does nothing in case there is nothing to persist.
    ///
    /// # Panics
    ///
    /// * This function will **not** panic on database errors.
    pub fn persist_ping_lifetime_data(&self) -> Result<()> {
        if let Some(ping_lifetime_data) = &self.ping_lifetime_data {
            let data = ping_lifetime_data
                .read()
                .expect("Can't read ping lifetime data");

            // We can reset the write-counter. Current data has been persisted.
            self.ping_lifetime_count.store(0, Ordering::Release);
            self.ping_lifetime_store_ts.replace(Instant::now());

            self.write_with_store(Lifetime::Ping, |mut writer, store| {
                for (key, value) in data.iter() {
                    let encoded =
                        bincode::serialize(&value).expect("IMPOSSIBLE: Serializing metric failed");
                    // There is no need for `get_storage_key` here because
                    // the key is already formatted from when it was saved
                    // to ping_lifetime_data.
                    store.put(&mut writer, key, &rkv::Value::Blob(&encoded))?;
                }
                measure_commit!(self, writer.commit())?;
                Ok(())
            })?;
        }
        Ok(())
    }

    pub fn persist_ping_lifetime_data_if_full(
        &self,
        data: &BTreeMap<String, Metric>,
    ) -> Result<()> {
        if self.ping_lifetime_threshold == 0 && self.ping_lifetime_max_time.is_zero() {
            return Ok(());
        }

        let write_count = self.ping_lifetime_count.fetch_add(1, Ordering::Release) + 1;
        let last_write = self.ping_lifetime_store_ts.get();
        let elapsed = last_write.elapsed();

        if (self.ping_lifetime_threshold == 0 || write_count < self.ping_lifetime_threshold)
            && (self.ping_lifetime_max_time.is_zero() || elapsed < self.ping_lifetime_max_time)
        {
            log::trace!(
                "Not flushing. write_count={} (threshold={}), elapsed={:?} (max={:?})",
                write_count,
                self.ping_lifetime_threshold,
                elapsed,
                self.ping_lifetime_max_time
            );
            return Ok(());
        }

        if self.ping_lifetime_threshold > 0 && write_count >= self.ping_lifetime_threshold {
            log::debug!(
                "Flushing database due to threshold of {} reached.",
                self.ping_lifetime_threshold
            )
        } else if !self.ping_lifetime_max_time.is_zero() && elapsed >= self.ping_lifetime_max_time {
            log::debug!(
                "Flushing database due to last write more than {:?} ago",
                self.ping_lifetime_max_time
            );
        }

        self.ping_lifetime_count.store(0, Ordering::Release);
        self.ping_lifetime_store_ts.replace(Instant::now());
        self.write_with_store(Lifetime::Ping, |mut writer, store| {
            for (key, value) in data.iter() {
                let encoded =
                    bincode::serialize(&value).expect("IMPOSSIBLE: Serializing metric failed");
                // There is no need for `get_storage_key` here because
                // the key is already formatted from when it was saved
                // to ping_lifetime_data.
                store.put(&mut writer, key, &rkv::Value::Blob(&encoded))?;
            }
            writer.commit()?;
            Ok(())
        })
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::tests::new_glean;
    use std::collections::HashMap;
    use tempfile::tempdir;

    #[test]
    fn test_panicks_if_fails_dir_creation() {
        let path = Path::new("/!#\"'@#°ç");
        assert!(Database::new(path, false, 0, Duration::ZERO).is_err());
    }

    #[test]
    #[cfg(windows)]
    fn windows_invalid_utf16_panicfree() {
        use std::ffi::OsString;
        use std::os::windows::prelude::*;

        // Here the values 0x0066 and 0x006f correspond to 'f' and 'o'
        // respectively. The value 0xD800 is a lone surrogate half, invalid
        // in a UTF-16 sequence.
        let source = [0x0066, 0x006f, 0xD800, 0x006f];
        let os_string = OsString::from_wide(&source[..]);
        let os_str = os_string.as_os_str();
        let dir = tempdir().unwrap();
        let path = dir.path().join(os_str);

        let res = Database::new(&path, false, 0, Duration::ZERO);

        assert!(
            res.is_ok(),
            "Database should succeed at {}: {:?}",
            path.display(),
            res
        );
    }

    #[test]
    #[cfg(target_os = "linux")]
    fn linux_invalid_utf8_panicfree() {
        use std::ffi::OsStr;
        use std::os::unix::ffi::OsStrExt;

        // Here, the values 0x66 and 0x6f correspond to 'f' and 'o'
        // respectively. The value 0x80 is a lone continuation byte, invalid
        // in a UTF-8 sequence.
        let source = [0x66, 0x6f, 0x80, 0x6f];
        let os_str = OsStr::from_bytes(&source[..]);
        let dir = tempdir().unwrap();
        let path = dir.path().join(os_str);

        let res = Database::new(&path, false, 0, Duration::ZERO);
        assert!(
            res.is_ok(),
            "Database should not fail at {}: {:?}",
            path.display(),
            res
        );
    }

    #[test]
    #[cfg(target_os = "macos")]
    fn macos_invalid_utf8_panicfree() {
        use std::ffi::OsStr;
        use std::os::unix::ffi::OsStrExt;

        // Here, the values 0x66 and 0x6f correspond to 'f' and 'o'
        // respectively. The value 0x80 is a lone continuation byte, invalid
        // in a UTF-8 sequence.
        let source = [0x66, 0x6f, 0x80, 0x6f];
        let os_str = OsStr::from_bytes(&source[..]);
        let dir = tempdir().unwrap();
        let path = dir.path().join(os_str);

        let res = Database::new(&path, false, 0, Duration::ZERO);
        assert!(
            res.is_err(),
            "Database should not fail at {}: {:?}",
            path.display(),
            res
        );
    }

    #[test]
    fn test_data_dir_rkv_inits() {
        let dir = tempdir().unwrap();
        Database::new(dir.path(), false, 0, Duration::ZERO).unwrap();

        assert!(dir.path().exists());
    }

    #[test]
    fn test_ping_lifetime_metric_recorded() {
        // Init the database in a temporary directory.
        let dir = tempdir().unwrap();
        let db = Database::new(dir.path(), false, 0, Duration::ZERO).unwrap();

        assert!(db.ping_lifetime_data.is_none());

        // Attempt to record a known value.
        let test_value = "test-value";
        let test_storage = "test-storage";
        let test_metric_id = "telemetry_test.test_name";
        db.record_per_lifetime(
            Lifetime::Ping,
            test_storage,
            test_metric_id,
            &Metric::String(test_value.to_string()),
        )
        .unwrap();

        // Verify that the data is correctly recorded.
        let mut found_metrics = 0;
        let mut snapshotter = |metric_id: &[u8], metric: &Metric| {
            found_metrics += 1;
            let metric_id = String::from_utf8_lossy(metric_id).into_owned();
            assert_eq!(test_metric_id, metric_id);
            match metric {
                Metric::String(s) => assert_eq!(test_value, s),
                _ => panic!("Unexpected data found"),
            }
        };

        db.iter_store_from(Lifetime::Ping, test_storage, None, &mut snapshotter);
        assert_eq!(1, found_metrics, "We only expect 1 Lifetime.Ping metric.");
    }

    #[test]
    fn test_application_lifetime_metric_recorded() {
        // Init the database in a temporary directory.
        let dir = tempdir().unwrap();
        let db = Database::new(dir.path(), false, 0, Duration::ZERO).unwrap();

        // Attempt to record a known value.
        let test_value = "test-value";
        let test_storage = "test-storage1";
        let test_metric_id = "telemetry_test.test_name";
        db.record_per_lifetime(
            Lifetime::Application,
            test_storage,
            test_metric_id,
            &Metric::String(test_value.to_string()),
        )
        .unwrap();

        // Verify that the data is correctly recorded.
        let mut found_metrics = 0;
        let mut snapshotter = |metric_id: &[u8], metric: &Metric| {
            found_metrics += 1;
            let metric_id = String::from_utf8_lossy(metric_id).into_owned();
            assert_eq!(test_metric_id, metric_id);
            match metric {
                Metric::String(s) => assert_eq!(test_value, s),
                _ => panic!("Unexpected data found"),
            }
        };

        db.iter_store_from(Lifetime::Application, test_storage, None, &mut snapshotter);
        assert_eq!(
            1, found_metrics,
            "We only expect 1 Lifetime.Application metric."
        );
    }

    #[test]
    fn test_user_lifetime_metric_recorded() {
        // Init the database in a temporary directory.
        let dir = tempdir().unwrap();
        let db = Database::new(dir.path(), false, 0, Duration::ZERO).unwrap();

        // Attempt to record a known value.
        let test_value = "test-value";
        let test_storage = "test-storage2";
        let test_metric_id = "telemetry_test.test_name";
        db.record_per_lifetime(
            Lifetime::User,
            test_storage,
            test_metric_id,
            &Metric::String(test_value.to_string()),
        )
        .unwrap();

        // Verify that the data is correctly recorded.
        let mut found_metrics = 0;
        let mut snapshotter = |metric_id: &[u8], metric: &Metric| {
            found_metrics += 1;
            let metric_id = String::from_utf8_lossy(metric_id).into_owned();
            assert_eq!(test_metric_id, metric_id);
            match metric {
                Metric::String(s) => assert_eq!(test_value, s),
                _ => panic!("Unexpected data found"),
            }
        };

        db.iter_store_from(Lifetime::User, test_storage, None, &mut snapshotter);
        assert_eq!(1, found_metrics, "We only expect 1 Lifetime.User metric.");
    }

    #[test]
    fn test_clear_ping_storage() {
        // Init the database in a temporary directory.
        let dir = tempdir().unwrap();
        let db = Database::new(dir.path(), false, 0, Duration::ZERO).unwrap();

        // Attempt to record a known value for every single lifetime.
        let test_storage = "test-storage";
        db.record_per_lifetime(
            Lifetime::User,
            test_storage,
            "telemetry_test.test_name_user",
            &Metric::String("test-value-user".to_string()),
        )
        .unwrap();
        db.record_per_lifetime(
            Lifetime::Ping,
            test_storage,
            "telemetry_test.test_name_ping",
            &Metric::String("test-value-ping".to_string()),
        )
        .unwrap();
        db.record_per_lifetime(
            Lifetime::Application,
            test_storage,
            "telemetry_test.test_name_application",
            &Metric::String("test-value-application".to_string()),
        )
        .unwrap();

        // Take a snapshot for the data, all the lifetimes.
        {
            let mut snapshot: HashMap<String, String> = HashMap::new();
            let mut snapshotter = |metric_id: &[u8], metric: &Metric| {
                let metric_id = String::from_utf8_lossy(metric_id).into_owned();
                match metric {
                    Metric::String(s) => snapshot.insert(metric_id, s.to_string()),
                    _ => panic!("Unexpected data found"),
                };
            };

            db.iter_store_from(Lifetime::User, test_storage, None, &mut snapshotter);
            db.iter_store_from(Lifetime::Ping, test_storage, None, &mut snapshotter);
            db.iter_store_from(Lifetime::Application, test_storage, None, &mut snapshotter);

            assert_eq!(3, snapshot.len(), "We expect all lifetimes to be present.");
            assert!(snapshot.contains_key("telemetry_test.test_name_user"));
            assert!(snapshot.contains_key("telemetry_test.test_name_ping"));
            assert!(snapshot.contains_key("telemetry_test.test_name_application"));
        }

        // Clear the Ping lifetime.
        db.clear_ping_lifetime_storage(test_storage).unwrap();

        // Take a snapshot again and check that we're only clearing the Ping lifetime.
        {
            let mut snapshot: HashMap<String, String> = HashMap::new();
            let mut snapshotter = |metric_id: &[u8], metric: &Metric| {
                let metric_id = String::from_utf8_lossy(metric_id).into_owned();
                match metric {
                    Metric::String(s) => snapshot.insert(metric_id, s.to_string()),
                    _ => panic!("Unexpected data found"),
                };
            };

            db.iter_store_from(Lifetime::User, test_storage, None, &mut snapshotter);
            db.iter_store_from(Lifetime::Ping, test_storage, None, &mut snapshotter);
            db.iter_store_from(Lifetime::Application, test_storage, None, &mut snapshotter);

            assert_eq!(2, snapshot.len(), "We only expect 2 metrics to be left.");
            assert!(snapshot.contains_key("telemetry_test.test_name_user"));
            assert!(snapshot.contains_key("telemetry_test.test_name_application"));
        }
    }

    #[test]
    fn test_remove_single_metric() {
        // Init the database in a temporary directory.
        let dir = tempdir().unwrap();
        let db = Database::new(dir.path(), false, 0, Duration::ZERO).unwrap();

        let test_storage = "test-storage-single-lifetime";
        let metric_id_pattern = "telemetry_test.single_metric";

        // Write sample metrics to the database.
        let lifetimes = [Lifetime::User, Lifetime::Ping, Lifetime::Application];

        for lifetime in lifetimes.iter() {
            for value in &["retain", "delete"] {
                db.record_per_lifetime(
                    *lifetime,
                    test_storage,
                    &format!("{}_{}", metric_id_pattern, value),
                    &Metric::String((*value).to_string()),
                )
                .unwrap();
            }
        }

        // Remove "telemetry_test.single_metric_delete" from each lifetime.
        for lifetime in lifetimes.iter() {
            db.remove_single_metric(
                *lifetime,
                test_storage,
                &format!("{}_delete", metric_id_pattern),
            )
            .unwrap();
        }

        // Verify that "telemetry_test.single_metric_retain" is still around for all lifetimes.
        for lifetime in lifetimes.iter() {
            let mut found_metrics = 0;
            let mut snapshotter = |metric_id: &[u8], metric: &Metric| {
                found_metrics += 1;
                let metric_id = String::from_utf8_lossy(metric_id).into_owned();
                assert_eq!(format!("{}_retain", metric_id_pattern), metric_id);
                match metric {
                    Metric::String(s) => assert_eq!("retain", s),
                    _ => panic!("Unexpected data found"),
                }
            };

            // Check the User lifetime.
            db.iter_store_from(*lifetime, test_storage, None, &mut snapshotter);
            assert_eq!(
                1, found_metrics,
                "We only expect 1 metric for this lifetime."
            );
        }
    }

    #[test]
    fn test_delayed_ping_lifetime_persistence() {
        // Init the database in a temporary directory.
        let dir = tempdir().unwrap();
        let db = Database::new(dir.path(), true, 0, Duration::ZERO).unwrap();
        let test_storage = "test-storage";

        assert!(db.ping_lifetime_data.is_some());

        // Attempt to record a known value.
        let test_value1 = "test-value1";
        let test_metric_id1 = "telemetry_test.test_name1";
        db.record_per_lifetime(
            Lifetime::Ping,
            test_storage,
            test_metric_id1,
            &Metric::String(test_value1.to_string()),
        )
        .unwrap();

        // Attempt to persist data.
        db.persist_ping_lifetime_data().unwrap();

        // Attempt to record another known value.
        let test_value2 = "test-value2";
        let test_metric_id2 = "telemetry_test.test_name2";
        db.record_per_lifetime(
            Lifetime::Ping,
            test_storage,
            test_metric_id2,
            &Metric::String(test_value2.to_string()),
        )
        .unwrap();

        {
            // At this stage we expect `test_value1` to be persisted and in memory,
            // since it was recorded before calling `persist_ping_lifetime_data`,
            // and `test_value2` to be only in memory, since it was recorded after.
            let store: SingleStore = db
                .rkv
                .open_single(Lifetime::Ping.as_str(), StoreOptions::create())
                .unwrap();
            let reader = db.rkv.read().unwrap();

            // Verify that test_value1 is in rkv.
            assert!(store
                .get(&reader, format!("{}#{}", test_storage, test_metric_id1))
                .unwrap_or(None)
                .is_some());
            // Verifiy that test_value2 is **not** in rkv.
            assert!(store
                .get(&reader, format!("{}#{}", test_storage, test_metric_id2))
                .unwrap_or(None)
                .is_none());

            let data = match &db.ping_lifetime_data {
                Some(ping_lifetime_data) => ping_lifetime_data,
                None => panic!("Expected `ping_lifetime_data` to exist here!"),
            };
            let data = data.read().unwrap();
            // Verify that test_value1 is also in memory.
            assert!(data
                .get(&format!("{}#{}", test_storage, test_metric_id1))
                .is_some());
            // Verify that test_value2 is in memory.
            assert!(data
                .get(&format!("{}#{}", test_storage, test_metric_id2))
                .is_some());
        }

        // Attempt to persist data again.
        db.persist_ping_lifetime_data().unwrap();

        {
            // At this stage we expect `test_value1` and `test_value2` to
            // be persisted, since both were created before a call to
            // `persist_ping_lifetime_data`.
            let store: SingleStore = db
                .rkv
                .open_single(Lifetime::Ping.as_str(), StoreOptions::create())
                .unwrap();
            let reader = db.rkv.read().unwrap();

            // Verify that test_value1 is in rkv.
            assert!(store
                .get(&reader, format!("{}#{}", test_storage, test_metric_id1))
                .unwrap_or(None)
                .is_some());
            // Verifiy that test_value2 is also in rkv.
            assert!(store
                .get(&reader, format!("{}#{}", test_storage, test_metric_id2))
                .unwrap_or(None)
                .is_some());

            let data = match &db.ping_lifetime_data {
                Some(ping_lifetime_data) => ping_lifetime_data,
                None => panic!("Expected `ping_lifetime_data` to exist here!"),
            };
            let data = data.read().unwrap();
            // Verify that test_value1 is also in memory.
            assert!(data
                .get(&format!("{}#{}", test_storage, test_metric_id1))
                .is_some());
            // Verify that test_value2 is also in memory.
            assert!(data
                .get(&format!("{}#{}", test_storage, test_metric_id2))
                .is_some());
        }
    }

    #[test]
    fn test_load_ping_lifetime_data_from_memory() {
        // Init the database in a temporary directory.
        let dir = tempdir().unwrap();

        let test_storage = "test-storage";
        let test_value = "test-value";
        let test_metric_id = "telemetry_test.test_name";

        {
            let db = Database::new(dir.path(), true, 0, Duration::ZERO).unwrap();

            // Attempt to record a known value.
            db.record_per_lifetime(
                Lifetime::Ping,
                test_storage,
                test_metric_id,
                &Metric::String(test_value.to_string()),
            )
            .unwrap();

            // Verify that test_value is in memory.
            let data = match &db.ping_lifetime_data {
                Some(ping_lifetime_data) => ping_lifetime_data,
                None => panic!("Expected `ping_lifetime_data` to exist here!"),
            };
            let data = data.read().unwrap();
            assert!(data
                .get(&format!("{}#{}", test_storage, test_metric_id))
                .is_some());

            // Attempt to persist data.
            db.persist_ping_lifetime_data().unwrap();

            // Verify that test_value is now in rkv.
            let store: SingleStore = db
                .rkv
                .open_single(Lifetime::Ping.as_str(), StoreOptions::create())
                .unwrap();
            let reader = db.rkv.read().unwrap();
            assert!(store
                .get(&reader, format!("{}#{}", test_storage, test_metric_id))
                .unwrap_or(None)
                .is_some());
        }

        // Now create a new instace of the db and check if data was
        // correctly loaded from rkv to memory.
        {
            let db = Database::new(dir.path(), true, 0, Duration::ZERO).unwrap();

            // Verify that test_value is in memory.
            let data = match &db.ping_lifetime_data {
                Some(ping_lifetime_data) => ping_lifetime_data,
                None => panic!("Expected `ping_lifetime_data` to exist here!"),
            };
            let data = data.read().unwrap();
            assert!(data
                .get(&format!("{}#{}", test_storage, test_metric_id))
                .is_some());

            // Verify that test_value is also in rkv.
            let store: SingleStore = db
                .rkv
                .open_single(Lifetime::Ping.as_str(), StoreOptions::create())
                .unwrap();
            let reader = db.rkv.read().unwrap();
            assert!(store
                .get(&reader, format!("{}#{}", test_storage, test_metric_id))
                .unwrap_or(None)
                .is_some());
        }
    }

    #[test]
    fn test_delayed_ping_lifetime_clear() {
        // Init the database in a temporary directory.
        let dir = tempdir().unwrap();
        let db = Database::new(dir.path(), true, 0, Duration::ZERO).unwrap();
        let test_storage = "test-storage";

        assert!(db.ping_lifetime_data.is_some());

        // Attempt to record a known value.
        let test_value1 = "test-value1";
        let test_metric_id1 = "telemetry_test.test_name1";
        db.record_per_lifetime(
            Lifetime::Ping,
            test_storage,
            test_metric_id1,
            &Metric::String(test_value1.to_string()),
        )
        .unwrap();

        {
            let data = match &db.ping_lifetime_data {
                Some(ping_lifetime_data) => ping_lifetime_data,
                None => panic!("Expected `ping_lifetime_data` to exist here!"),
            };
            let data = data.read().unwrap();
            // Verify that test_value1 is in memory.
            assert!(data
                .get(&format!("{}#{}", test_storage, test_metric_id1))
                .is_some());
        }

        // Clear ping lifetime storage for a storage that isn't test_storage.
        // Doesn't matter what it's called, just that it isn't test_storage.
        db.clear_ping_lifetime_storage(&(test_storage.to_owned() + "x"))
            .unwrap();

        {
            let data = match &db.ping_lifetime_data {
                Some(ping_lifetime_data) => ping_lifetime_data,
                None => panic!("Expected `ping_lifetime_data` to exist here!"),
            };
            let data = data.read().unwrap();
            // Verify that test_value1 is still in memory.
            assert!(data
                .get(&format!("{}#{}", test_storage, test_metric_id1))
                .is_some());
        }

        // Clear test_storage's ping lifetime storage.
        db.clear_ping_lifetime_storage(test_storage).unwrap();

        {
            let data = match &db.ping_lifetime_data {
                Some(ping_lifetime_data) => ping_lifetime_data,
                None => panic!("Expected `ping_lifetime_data` to exist here!"),
            };
            let data = data.read().unwrap();
            // Verify that test_value1 is no longer in memory.
            assert!(data
                .get(&format!("{}#{}", test_storage, test_metric_id1))
                .is_none());
        }
    }

    #[test]
    fn doesnt_record_when_upload_is_disabled() {
        let (mut glean, dir) = new_glean(None);

        // Init the database in a temporary directory.

        let test_storage = "test-storage";
        let test_data = CommonMetricDataInternal::new("category", "name", test_storage);
        let test_metric_id = test_data.identifier(&glean);

        // Attempt to record metric with the record and record_with functions,
        // this should work since upload is enabled.
        let db = Database::new(dir.path(), true, 0, Duration::ZERO).unwrap();
        db.record(&glean, &test_data, &Metric::String("record".to_owned()));
        db.iter_store_from(
            Lifetime::Ping,
            test_storage,
            None,
            &mut |metric_id: &[u8], metric: &Metric| {
                assert_eq!(
                    String::from_utf8_lossy(metric_id).into_owned(),
                    test_metric_id
                );
                match metric {
                    Metric::String(v) => assert_eq!("record", *v),
                    _ => panic!("Unexpected data found"),
                }
            },
        );

        db.record_with(&glean, &test_data, |_| {
            Metric::String("record_with".to_owned())
        });
        db.iter_store_from(
            Lifetime::Ping,
            test_storage,
            None,
            &mut |metric_id: &[u8], metric: &Metric| {
                assert_eq!(
                    String::from_utf8_lossy(metric_id).into_owned(),
                    test_metric_id
                );
                match metric {
                    Metric::String(v) => assert_eq!("record_with", *v),
                    _ => panic!("Unexpected data found"),
                }
            },
        );

        // Disable upload
        glean.set_upload_enabled(false);

        // Attempt to record metric with the record and record_with functions,
        // this should work since upload is now **disabled**.
        db.record(&glean, &test_data, &Metric::String("record_nop".to_owned()));
        db.iter_store_from(
            Lifetime::Ping,
            test_storage,
            None,
            &mut |metric_id: &[u8], metric: &Metric| {
                assert_eq!(
                    String::from_utf8_lossy(metric_id).into_owned(),
                    test_metric_id
                );
                match metric {
                    Metric::String(v) => assert_eq!("record_with", *v),
                    _ => panic!("Unexpected data found"),
                }
            },
        );
        db.record_with(&glean, &test_data, |_| {
            Metric::String("record_with_nop".to_owned())
        });
        db.iter_store_from(
            Lifetime::Ping,
            test_storage,
            None,
            &mut |metric_id: &[u8], metric: &Metric| {
                assert_eq!(
                    String::from_utf8_lossy(metric_id).into_owned(),
                    test_metric_id
                );
                match metric {
                    Metric::String(v) => assert_eq!("record_with", *v),
                    _ => panic!("Unexpected data found"),
                }
            },
        );
    }

    mod safe_mode {
        use std::fs::File;

        use super::*;

        #[test]
        fn empty_data_file() {
            let dir = tempdir().unwrap();

            // Create database directory structure.
            let database_dir = dir.path().join("db");
            fs::create_dir_all(&database_dir).expect("create database dir");

            // Create empty database file.
            let safebin = database_dir.join("data.safe.bin");
            let f = File::create(safebin).expect("create database file");
            drop(f);

            let db = Database::new(dir.path(), false, 0, Duration::ZERO).unwrap();

            assert!(dir.path().exists());
            assert!(
                matches!(db.rkv_load_state, RkvLoadState::Err(_)),
                "Load error recorded"
            );
        }

        #[test]
        fn corrupted_data_file() {
            let dir = tempdir().unwrap();

            // Create database directory structure.
            let database_dir = dir.path().join("db");
            fs::create_dir_all(&database_dir).expect("create database dir");

            // Create empty database file.
            let safebin = database_dir.join("data.safe.bin");
            fs::write(safebin, "<broken>").expect("write to database file");

            let db = Database::new(dir.path(), false, 0, Duration::ZERO).unwrap();

            assert!(dir.path().exists());
            assert!(
                matches!(db.rkv_load_state, RkvLoadState::Err(_)),
                "Load error recorded"
            );
        }
    }
}
