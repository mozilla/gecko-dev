/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{
    collections::HashSet,
    sync::{Arc, Weak},
};

use camino::Utf8PathBuf;
use parking_lot::Mutex;
use url::Url;

use crate::{
    storage::Storage, RemoteSettingsClient, RemoteSettingsConfig2, RemoteSettingsContext,
    RemoteSettingsServer, Result,
};

/// Internal Remote settings service API
pub struct RemoteSettingsService {
    inner: Mutex<RemoteSettingsServiceInner>,
}

struct RemoteSettingsServiceInner {
    storage_dir: Utf8PathBuf,
    base_url: Url,
    bucket_name: String,
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
    pub fn new(storage_dir: String, config: RemoteSettingsConfig2) -> Result<Self> {
        let storage_dir = storage_dir.into();
        let base_url = config
            .server
            .unwrap_or(RemoteSettingsServer::Prod)
            .get_url()?;
        let bucket_name = config.bucket_name.unwrap_or_else(|| String::from("main"));

        Ok(Self {
            inner: Mutex::new(RemoteSettingsServiceInner {
                storage_dir,
                base_url,
                bucket_name,
                clients: vec![],
            }),
        })
    }

    /// Create a new Remote Settings client
    #[cfg(feature = "jexl")]
    pub fn make_client(
        &self,
        collection_name: String,
        context: Option<RemoteSettingsContext>,
    ) -> Result<Arc<RemoteSettingsClient>> {
        let mut inner = self.inner.lock();
        let storage = Storage::new(inner.storage_dir.join(format!("{collection_name}.sql")))?;

        let client = Arc::new(RemoteSettingsClient::new(
            inner.base_url.clone(),
            inner.bucket_name.clone(),
            collection_name.clone(),
            context,
            storage,
        )?);
        inner.clients.push(Arc::downgrade(&client));
        Ok(client)
    }

    #[cfg(not(feature = "jexl"))]
    pub fn make_client(
        &self,
        collection_name: String,
        #[allow(unused_variables)] context: Option<RemoteSettingsContext>,
    ) -> Result<Arc<RemoteSettingsClient>> {
        let mut inner = self.inner.lock();
        let storage = Storage::new(inner.storage_dir.join(format!("{collection_name}.sql")))?;
        let client = Arc::new(RemoteSettingsClient::new(
            inner.base_url.clone(),
            inner.bucket_name.clone(),
            collection_name.clone(),
            storage,
        )?);
        inner.clients.push(Arc::downgrade(&client));
        Ok(client)
    }

    /// Sync collections for all active clients
    pub fn sync(&self) -> Result<Vec<String>> {
        // Make sure we only sync each collection once, even if there are multiple clients
        let mut synced_collections = HashSet::new();

        // TODO: poll the server using `/buckets/monitor/collections/changes/changeset` to fetch
        // the current timestamp for all collections.  That way we can avoid fetching collections
        // we know haven't changed and also pass the `?_expected{ts}` param to the server.

        for client in self.inner.lock().active_clients() {
            if synced_collections.insert(client.collection_name()) {
                client.internal.sync()?;
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
            .get_url()?;
        let bucket_name = config.bucket_name.unwrap_or_else(|| String::from("main"));
        let mut inner = self.inner.lock();
        for client in inner.active_clients() {
            client
                .internal
                .update_config(base_url.clone(), bucket_name.clone())?;
        }
        inner.base_url = base_url;
        inner.bucket_name = bucket_name;
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
}
