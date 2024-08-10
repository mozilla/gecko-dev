/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Multi-store coordinator.

use std::{
    collections::{BTreeMap, BTreeSet},
    fmt::Debug,
    num::NonZeroUsize,
    ops::Bound,
    sync::{Arc, Mutex, OnceLock},
};

// `std::collections::HashMap::extract_if` is unstable [1],
// so we use Hashbrown.
//
// [1]: https://github.com/rust-lang/rust/issues/59618
use hashbrown::hash_map::{Entry, HashMap};

use crate::skv::{
    abort::{AbortController, AbortSignal},
    store::{Store, StorePath},
};

/// Manages access to all persistent stores.
///
/// The coordinator is a singleton that lives for the lifetime of
/// the process. It has two main responsibilities:
///
/// 1. Decoupling the lifecycle of the stores from the
///    lifecycle of the [`crate::skv::interface`] objects.
/// 2. Keeping track of which objects are accessing which stores, and
///    closing stores once they're no longer needed.
///
/// ## Why have a coordinator?
///
/// Interface objects are a little cumbersome to work with in pure Rust:
/// they don't have predictable lifetimes; they can only be held by an
/// [`xpcom::RefPtr`]; and they can unintentionally retain the resources
/// they're managing beyond their intended use.
///
/// The coordinator introduces a layer of indirection to help
/// with these challenges.
///
/// ## Using the coordinator
///
/// When an interface object is instantiated, it either requests a new client
/// from the coordinator, or creates a child of an existing client. The object
/// holds on to the client, and uses it to access any stores it needs
/// during its lifecycle.
///
/// When the object is done accessing its stores, it invalidates its client.
/// The coordinator detaches the invalidated client, and its children,
/// from any open stores. This prevents the object from accessing any stores
/// again, even if the [`xpcom::RefPtr`] holding the object isn't released
/// right away.
///
/// ## Object hierarchies
///
/// Clients can have multiple children, grandchildren, great-grandchildren,
/// and so on; recursively. This is useful for managing hierarchies of
/// interface objects, where the child's lifecycle is the same or shorter
/// than its parent's.
///
/// Check out [`crate::skv::interface::KeyValueService`] and
/// [`crate::skv::interface::KeyValueDatabase`] for an example of
/// hierarchies in action.
#[derive(Debug)]
pub struct Coordinator {
    state: Mutex<CoordinatorState>,
}

impl Coordinator {
    fn new() -> Self {
        Coordinator {
            state: Mutex::new(CoordinatorState {
                clients: BTreeMap::new(),
                stores: HashMap::new(),
            }),
        }
    }

    /// Returns the singleton coordinator.
    pub fn get_or_create() -> &'static Self {
        static COORDINATOR: OnceLock<Coordinator> = OnceLock::new();
        COORDINATOR.get_or_init(|| Coordinator::new())
    }

    /// Creates a new coordinator client.
    pub fn client_with_name(&self, name: &'static str) -> CoordinatorClient<'_> {
        let mut state = self.state.lock().unwrap();

        // Make this client the new last sibling by incrementing
        // the first bud of the current last sibling's key.
        let last_sibling_key = state.clients.last_key_value().map(|(key, _)| key);
        let key = ClientKey::new(
            last_sibling_key
                .map(|key| key.first().increment())
                .unwrap_or(ClientKeyBud::MIN),
        );
        let controller = AbortController::new();
        let signal = controller.signal();
        state
            .clients
            .insert(key.clone(), ActiveClient { controller });

        CoordinatorClient {
            coordinator: self,
            name,
            key,
            signal,
        }
    }
}

/// A client owned by an interface object.
#[derive(Clone, Debug)]
pub struct CoordinatorClient<'a> {
    coordinator: &'a Coordinator,
    name: &'static str,
    key: ClientKey,
    signal: AbortSignal,
}

impl<'a> CoordinatorClient<'a> {
    /// Gets or opens a store for this client.
    ///
    /// Stores connect to the underlying physical database lazily,
    /// so this does _not_ block on I/O.
    pub fn store_for_path(&self, path: StorePath) -> Result<Arc<Store>, CoordinatorError> {
        let mut state = self.coordinator.state.lock().unwrap();
        if !state.clients.contains_key(&self.key) {
            return Err(CoordinatorError::Invalidated(self.name));
        }
        match state.stores.entry(path) {
            Entry::Occupied(mut entry) => Ok(entry.get_mut().attach(self.key.clone())),
            Entry::Vacant(entry) => {
                let store = InUseStore::new(Store::new(entry.key().clone()));
                Ok(entry.insert(store).attach(self.key.clone()))
            }
        }
    }

    /// Creates a child of this client.
    pub fn child_with_name(
        &self,
        name: &'static str,
    ) -> Result<CoordinatorClient<'a>, CoordinatorError> {
        let mut state = self.coordinator.state.lock().unwrap();
        if !state.clients.contains_key(&self.key) {
            return Err(CoordinatorError::Invalidated(self.name));
        }

        let child_key = {
            // Make this child the new last child by incrementing
            // the last bud of the current last child's key.
            let last_child_key = {
                let max_child_key = self.key.clone().appending(ClientKeyBud::MAX);
                let children = state
                    .clients
                    .range((
                        // Include all our descendants, but not ourselves.
                        Bound::Excluded(&self.key),
                        Bound::Included(&max_child_key),
                    ))
                    .map(|(key, _)| key)
                    .filter(|key| {
                        // Narrow down our descendants to immediate children.
                        key.len() <= max_child_key.len()
                    });
                children.last()
            };
            self.key.clone().appending(
                last_child_key
                    .map(|key| key.last().increment())
                    .unwrap_or(ClientKeyBud::MIN),
            )
        };
        let controller = AbortController::new();
        let signal = controller.signal();
        state
            .clients
            .insert(child_key.clone(), ActiveClient { controller });

        Ok(CoordinatorClient {
            coordinator: self.coordinator,
            name,
            key: child_key,
            signal,
        })
    }

    /// Invalidates this client.
    ///
    /// Invalidation is recursive: if the client has descendants
    /// (children, grandchildren, etc.), they'll be invalidated, too.
    ///
    /// If the client or any of its descendants are the last clients
    /// of a store, invalidating the client will also close those stores.
    /// **This can block on disk I/O**, so clients should not be
    /// invalidated on the main thread.
    pub fn invalidate(&self) {
        let (abortable_clients, closeable_stores) = {
            let mut state = self.coordinator.state.lock().unwrap();
            let keys = {
                let max_child_key = self.key.clone().appending(ClientKeyBud::MAX);
                state
                    .clients
                    .range(
                        // Include ourselves and all our descendants.
                        &self.key..=&max_child_key,
                    )
                    .map(|(key, _)| key)
                    .cloned()
                    .collect::<Vec<_>>()
            };
            if keys.is_empty() {
                // Already invalidated; no need to do anything.
                return;
            }
            let abortable_clients = keys
                .iter()
                .map(|key| {
                    // Invariant: `keys` only contains keys that exist
                    // in `clients`.
                    state.clients.remove(key).expect("invariant violation")
                })
                .collect::<Vec<_>>();
            let closeable_stores = state
                .stores
                .extract_if(|_, store| {
                    // Detach ourselves from every store.
                    match store.detach(&keys) {
                        DetachResult::StillInUse => false,
                        // If we detached all clients from this store, also
                        // remove the store from the map, so that we can
                        // close it.
                        DetachResult::Closeable => true,
                    }
                })
                .map(|(_, store)| store)
                .collect::<Vec<_>>();
            (abortable_clients, closeable_stores)
        };

        for client in abortable_clients {
            client.controller.abort();
        }

        for store in closeable_stores {
            // Invariant: `into_inner` always succeeds for closeable stores.
            let store = store.into_inner().expect("invariant violation");
            store.close();
        }
    }

    /// Returns a shared reference to this client's cancellation signal.
    ///
    /// The signal will fire when the client, or any of its ancestors,
    /// are invalidated.
    pub fn signal(&self) -> AbortSignal {
        self.signal.clone()
    }
}

/// An incrementable and totally ordered scalar value.
///
/// These are called "buds" because they can form leaves and branches.
#[derive(Clone, Copy, Eq, Hash, Ord, PartialEq, PartialOrd)]
struct ClientKeyBud(NonZeroUsize);

impl ClientKeyBud {
    const MIN: ClientKeyBud = ClientKeyBud(NonZeroUsize::MIN);
    const MAX: ClientKeyBud = ClientKeyBud(NonZeroUsize::MAX);

    fn increment(self) -> ClientKeyBud {
        Self(self.0.checked_add(1).unwrap())
    }
}

impl Debug for ClientKeyBud {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "#{:?}", self.0)
    }
}

/// A hierarchical key uniquely identifying a coordinator client.
///
/// A key comprises one or more "buds". If clients M and N are siblings,
/// then their keys only differ in the last bud. If N is a child of M,
/// then N's key has one more bud than M's key.
#[derive(Clone, Eq, Hash, Ord, PartialEq, PartialOrd)]
struct ClientKey(ClientKeyBud, Box<[ClientKeyBud]>);

impl ClientKey {
    fn new(id: ClientKeyBud) -> Self {
        Self(id, Box::default())
    }

    /// Returns the first bud of the key.
    fn first(&self) -> ClientKeyBud {
        self.0
    }

    /// Returns the last bud of the key.
    fn last(&self) -> ClientKeyBud {
        self.1.last().copied().unwrap_or(self.0)
    }

    /// Returns the number of buds in the key.
    fn len(&self) -> usize {
        1usize.checked_add(self.1.len()).unwrap()
    }

    /// Consumes this key, and returns a new key with
    /// the given bud added to the end.
    fn appending(self, bud: ClientKeyBud) -> Self {
        let mut buds = self.1.into_vec();
        buds.push(bud);
        ClientKey(self.0, buds.into_boxed_slice())
    }
}

impl Debug for ClientKey {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_set().entry(&self.0).entries(self.1.iter()).finish()
    }
}

#[derive(Debug)]
struct InUseStore {
    attached_client_keys: BTreeSet<ClientKey>,
    store: Arc<Store>,
}

impl InUseStore {
    fn new(store: Store) -> Self {
        Self {
            attached_client_keys: BTreeSet::new(),
            store: Arc::new(store),
        }
    }

    /// Notes that a client with the given key
    /// is now using this store.
    ///
    /// If the client is already using this store,
    /// attaching it again is a no-op.
    fn attach(&mut self, key: ClientKey) -> Arc<Store> {
        self.attached_client_keys.insert(key);
        Arc::clone(&self.store)
    }

    /// Notes that all the clients with the given keys
    /// are no longer using this store.
    fn detach(&mut self, keys: &[ClientKey]) -> DetachResult {
        for key in keys {
            self.attached_client_keys.remove(key);
        }
        if self.attached_client_keys.is_empty() {
            DetachResult::Closeable
        } else {
            DetachResult::StillInUse
        }
    }

    fn into_inner(self) -> Result<Arc<Store>, Self> {
        if self.attached_client_keys.is_empty() {
            Ok(self.store)
        } else {
            Err(self)
        }
    }
}

#[derive(Clone, Copy, Debug)]
enum DetachResult {
    StillInUse,
    Closeable,
}

#[derive(Debug)]
struct ActiveClient {
    controller: AbortController,
}

#[derive(Debug)]
struct CoordinatorState {
    clients: BTreeMap<ClientKey, ActiveClient>,
    stores: HashMap<StorePath, InUseStore>,
}

#[derive(thiserror::Error, Debug)]
pub enum CoordinatorError {
    #[error("`{0}` invalidated")]
    Invalidated(&'static str),
}
