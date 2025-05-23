/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{
    collections::{HashMap, HashSet},
    sync::{Arc, Weak},
};

use camino::Utf8PathBuf;
use error_support::trace;
use parking_lot::Mutex;
use serde::Deserialize;
use viaduct::Request;

use crate::{
    client::RemoteState, config::BaseUrl, error::Error, storage::Storage, RemoteSettingsClient,
    RemoteSettingsConfig2, RemoteSettingsContext, RemoteSettingsServer, Result,
};

/// Internal Remote settings service API
pub struct RemoteSettingsService {
    inner: Mutex<RemoteSettingsServiceInner>,
}

struct RemoteSettingsServiceInner {
    storage_dir: Utf8PathBuf,
    base_url: BaseUrl,
    bucket_name: String,
    app_context: Option<RemoteSettingsContext>,
    remote_state: RemoteState,
    /// Weakrefs for all clients that we've created.  Note: this stores the
    /// top-level/public `RemoteSettingsClient` structs rather than `client::RemoteSettingsClient`.
    /// The reason for this is that we return Arcs to the public struct to the foreign code, so we
    /// need to use the same type for our weakrefs.  The alternative would be to create 2 Arcs for
    /// each client, which is wasteful.
    clients: Vec<Weak<RemoteSettingsClient>>,
}

impl RemoteSettingsService {
    /// Construct a [RemoteSettingsService]
    ///
    /// This is typically done early in the application-startup process
    pub fn new(storage_dir: String, config: RemoteSettingsConfig2) -> Self {
        let storage_dir = storage_dir.into();
        let base_url = config
            .server
            .unwrap_or(RemoteSettingsServer::Prod)
            .get_base_url_with_prod_fallback();
        let bucket_name = config.bucket_name.unwrap_or_else(|| String::from("main"));

        Self {
            inner: Mutex::new(RemoteSettingsServiceInner {
                storage_dir,
                base_url,
                bucket_name,
                app_context: config.app_context,
                remote_state: RemoteState::default(),
                clients: vec![],
            }),
        }
    }

    pub fn make_client(&self, collection_name: String) -> Arc<RemoteSettingsClient> {
        let mut inner = self.inner.lock();
        // Allow using in-memory databases for testing of external crates.
        let storage = if inner.storage_dir == ":memory:" {
            Storage::new(inner.storage_dir.clone())
        } else {
            Storage::new(inner.storage_dir.join(format!("{collection_name}.sql")))
        };

        let client = Arc::new(RemoteSettingsClient::new(
            inner.base_url.clone(),
            inner.bucket_name.clone(),
            collection_name.clone(),
            inner.app_context.clone(),
            storage,
        ));
        inner.clients.push(Arc::downgrade(&client));
        client
    }

    /// Sync collections for all active clients
    pub fn sync(&self) -> Result<Vec<String>> {
        // Make sure we only sync each collection once, even if there are multiple clients
        let mut synced_collections = HashSet::new();

        let mut inner = self.inner.lock();
        let changes = inner.fetch_changes()?;
        let change_map: HashMap<_, _> = changes
            .changes
            .iter()
            .map(|c| ((c.collection.as_str(), &c.bucket), c.last_modified))
            .collect();
        let bucket_name = inner.bucket_name.clone();

        for client in inner.active_clients() {
            let client = &client.internal;
            let collection_name = client.collection_name();
            if let Some(client_last_modified) = client.get_last_modified_timestamp()? {
                if let Some(server_last_modified) = change_map.get(&(collection_name, &bucket_name))
                {
                    if client_last_modified != *server_last_modified {
                        trace!("skipping up-to-date collection: {collection_name}");
                        continue;
                    }
                }
            }
            if synced_collections.insert(collection_name.to_string()) {
                trace!("syncing collection: {collection_name}");
                client.sync()?;
            }
        }
        Ok(synced_collections.into_iter().collect())
    }

    /// Update the remote settings config
    ///
    /// This will cause all current and future clients to use new config and will delete any stored
    /// records causing the clients to return new results from the new config.
    pub fn update_config(&self, config: RemoteSettingsConfig2) -> Result<()> {
        let base_url = config
            .server
            .unwrap_or(RemoteSettingsServer::Prod)
            .get_base_url()?;
        let bucket_name = config.bucket_name.unwrap_or_else(|| String::from("main"));
        let mut inner = self.inner.lock();
        for client in inner.active_clients() {
            client.internal.update_config(
                base_url.clone(),
                bucket_name.clone(),
                config.app_context.clone(),
            )?;
        }
        inner.base_url = base_url;
        inner.bucket_name = bucket_name;
        inner.app_context = config.app_context;
        Ok(())
    }
}

impl RemoteSettingsServiceInner {
    // Find live clients in self.clients
    //
    // Also, drop dead weakrefs from the vec
    fn active_clients(&mut self) -> Vec<Arc<RemoteSettingsClient>> {
        let mut active_clients = vec![];
        self.clients.retain(|weak| {
            if let Some(client) = weak.upgrade() {
                active_clients.push(client);
                true
            } else {
                false
            }
        });
        active_clients
    }

    fn fetch_changes(&mut self) -> Result<Changes> {
        let mut url = self.base_url.clone();
        url.path_segments_mut()
            .push("buckets")
            .push("monitor")
            .push("collections")
            .push("changes")
            .push("changeset");
        // For now, always use `0` for the expected value.  This means we'll get updates based on
        // the default TTL of 1 hour.
        //
        // Eventually, we should add support for push notifications and use the timestamp from the
        // notification.
        url.query_pairs_mut().append_pair("_expected", "0");
        let url = url.into_inner();
        trace!("make_request: {url}");
        self.remote_state.ensure_no_backoff()?;

        let req = Request::get(url);
        let resp = req.send()?;

        self.remote_state.handle_backoff_hint(&resp)?;

        if resp.is_success() {
            Ok(resp.json()?)
        } else {
            Err(Error::ResponseError(format!(
                "status code: {}",
                resp.status
            )))
        }
    }
}

/// Data from the changes endpoint
///
/// https://remote-settings.readthedocs.io/en/latest/client-specifications.html#endpoints
#[derive(Debug, Deserialize)]
struct Changes {
    changes: Vec<ChangesCollection>,
}

#[derive(Debug, Deserialize)]
struct ChangesCollection {
    collection: String,
    bucket: String,
    last_modified: u64,
}
