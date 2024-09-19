/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Component interface implementation.
//!
//! This module implements the `nsIKeyValue` XPCOM interfaces that are
//! exposed to C++ and chrome JS callers.

use std::{io, ops::Bound, sync::Arc};

use atomic_refcell::AtomicRefCell;
use crossbeam_utils::atomic::AtomicCell;
use nserror::{nsresult, NS_OK};
use nsstring::{nsACString, nsAString, nsCString, nsString};
use rkv::{
    backend::{SafeMode as RkvSafeMode, SafeModeEnvironment as RkvSafeModeEnvironment},
    Rkv,
};
use storage_variant::{HashPropertyBag, VariantType};
use thin_vec::ThinVec;
use xpcom::{
    getter_addrefs,
    interfaces::{
        nsIAsyncShutdownClient, nsIAsyncShutdownService, nsIKeyValueDatabaseCallback,
        nsIKeyValueEnumeratorCallback, nsIKeyValueImporter, nsIKeyValuePair,
        nsIKeyValueVariantCallback, nsIKeyValueVoidCallback, nsIPropertyBag, nsIVariant,
    },
    xpcom, xpcom_method, RefPtr,
};

use crate::skv::{
    abort::AbortError,
    coordinator::{Coordinator, CoordinatorClient, CoordinatorError},
    database::{Database, DatabaseError, GetOptions},
    importer::{CleanupPolicy, ConflictPolicy, ImporterError, RkvImporter},
    key::{Key, KeyError},
    store::{Store, StoreError, StorePath},
    value::{Value, ValueError},
};

#[xpcom(implement(nsIKeyValueService), atomic)]
pub struct KeyValueService {
    client: CoordinatorClient<'static>,
}

impl KeyValueService {
    pub fn new() -> RefPtr<Self> {
        let client = Coordinator::get_or_create().client_with_name("skv:KeyValueService");
        KeyValueServiceShutdownBlocker::register(client.clone());
        KeyValueService::allocate(InitKeyValueService { client })
    }

    xpcom_method!(
        get_or_create => GetOrCreate(
            callback: *const nsIKeyValueDatabaseCallback,
            dir: *const nsAString,
            name: *const nsACString
        )
    );
    fn get_or_create(
        &self,
        callback: &nsIKeyValueDatabaseCallback,
        dir: &nsAString,
        name: &nsACString,
    ) -> Result<(), Infallible> {
        let client = self.client.clone();
        let dir = nsString::from(dir);
        let request =
            moz_task::spawn_blocking("skv:KeyValueService:GetOrCreate:Request", async move {
                let path = if dir == StorePath::IN_MEMORY_DATABASE_NAME {
                    StorePath::for_in_memory()
                } else {
                    // Concurrently accessing the same physical SQLite database
                    // through different links can corrupt its WAL file,
                    // especially when done from multiple processes.
                    // Mitigate that by canonicalizing the path.
                    StorePath::for_storage_dir(
                        crate::fs::canonicalize(&*dir).map_err(InterfaceError::StorageDir)?,
                    )
                };
                Ok((client.child_with_name("skv:KeyValueDatabase")?, path))
            });

        let name = nsCString::from(name);
        let callback = RefPtr::new(callback);
        moz_task::spawn_local("skv:KeyValueService:GetOrCreate:Response", async move {
            match request.await {
                Ok((client, path)) => {
                    let db = KeyValueDatabase::new(client, path, name.to_utf8().into());
                    unsafe { callback.Resolve(db.coerce()) }
                }
                Err::<_, InterfaceError>(err) => unsafe {
                    callback.Reject(&*nsCString::from(err.to_string()))
                },
            }
        })
        .detach();

        Ok(())
    }

    xpcom_method!(
        get_or_create_with_options => GetOrCreateWithOptions(
            callback: *const nsIKeyValueDatabaseCallback,
            path: *const nsAString,
            name: *const nsACString,
            strategy: u8
        )
    );
    fn get_or_create_with_options(
        &self,
        _callback: &nsIKeyValueDatabaseCallback,
        _path: &nsAString,
        _name: &nsACString,
        _strategy: u8,
    ) -> Result<(), nsresult> {
        Err(nserror::NS_ERROR_NOT_IMPLEMENTED)
    }

    xpcom_method!(
        create_importer => CreateImporter(
            type_: *const nsACString,
            dir: *const nsAString
        ) -> *const nsIKeyValueImporter
    );
    fn create_importer(
        &self,
        type_: &nsACString,
        dir: &nsAString,
    ) -> Result<RefPtr<nsIKeyValueImporter>, nsresult> {
        match &*type_.to_utf8() {
            RkvSafeModeKeyValueImporter::TYPE => {
                let client = self
                    .client
                    .child_with_name("skv:RkvSafeModeKeyValueImporter")
                    .map_err(|_| nserror::NS_ERROR_FAILURE)?;
                let importer = RkvSafeModeKeyValueImporter::new(client, nsString::from(dir));
                Ok(RefPtr::new(importer.coerce()))
            }
            _ => Err(nserror::NS_ERROR_INVALID_ARG),
        }
    }
}

#[xpcom(implement(nsIKeyValueDatabase), atomic)]
pub struct KeyValueDatabase {
    client: CoordinatorClient<'static>,
    path: StorePath,
    name: String,
}

impl KeyValueDatabase {
    fn new(client: CoordinatorClient<'static>, path: StorePath, name: String) -> RefPtr<Self> {
        KeyValueDatabase::allocate(InitKeyValueDatabase { client, path, name })
    }

    fn store(&self) -> Result<Arc<Store>, InterfaceError> {
        Ok(self.client.store_for_path(self.path.clone())?)
    }

    xpcom_method!(
        is_empty => IsEmpty(
            callback: *const nsIKeyValueVariantCallback
        )
    );
    fn is_empty(&self, callback: &nsIKeyValueVariantCallback) -> Result<(), Infallible> {
        let store = self.store();
        let name = self.name.clone();
        let request =
            moz_task::spawn_blocking("skv:KeyValueDatabase:IsEmpty:Request", async move {
                let store = store?;
                let db = Database::new(&store, &name);
                Ok(db.is_empty()?)
            });

        let signal = self.client.signal();
        let callback = RefPtr::new(callback);
        moz_task::spawn_local("skv:KeyValueDatabase:IsEmpty:Response", async move {
            match signal.aborting(request).await {
                Ok(b) => unsafe { callback.Resolve(b.into_variant().coerce()) },
                Err(InterfaceError::Abort(_)) => unsafe {
                    callback.Reject(&*nsCString::from("isEmpty: aborted"))
                },
                Err(err) => unsafe { callback.Reject(&*nsCString::from(err.to_string())) },
            }
        })
        .detach();

        Ok(())
    }

    xpcom_method!(
        count => Count(
            callback: *const nsIKeyValueVariantCallback
        )
    );
    fn count(&self, callback: &nsIKeyValueVariantCallback) -> Result<(), Infallible> {
        let store = self.store();
        let name = self.name.clone();
        let request = moz_task::spawn_blocking("skv:KeyValueDatabase:Count:Request", async move {
            let store = store?;
            let db = Database::new(&store, &name);
            Ok(db.count()?)
        });

        let signal = self.client.signal();
        let callback = RefPtr::new(callback);
        moz_task::spawn_local("skv:KeyValueDatabase:Count:Response", async move {
            match signal.aborting(request).await {
                Ok(n) => unsafe { callback.Resolve(n.into_variant().coerce()) },
                Err(InterfaceError::Abort(_)) => unsafe {
                    callback.Reject(&*nsCString::from("count: aborted"))
                },
                Err(err) => unsafe { callback.Reject(&*nsCString::from(err.to_string())) },
            }
        })
        .detach();

        Ok(())
    }

    xpcom_method!(
        size => Size(
            callback: *const nsIKeyValueVariantCallback
        )
    );
    fn size(&self, callback: &nsIKeyValueVariantCallback) -> Result<(), Infallible> {
        let store = self.store();
        let name = self.name.clone();
        let request = moz_task::spawn_blocking("skv:KeyValueDatabase:Size:Request", async move {
            let store = store?;
            let db = Database::new(&store, &name);
            Ok(db.size()?)
        });

        let signal = self.client.signal();
        let callback = RefPtr::new(callback);
        moz_task::spawn_local("skv:KeyValueDatabase:Size:Response", async move {
            match signal.aborting(request).await {
                Ok(n) => unsafe { callback.Resolve(n.into_variant().coerce()) },
                Err(InterfaceError::Abort(_)) => unsafe {
                    callback.Reject(&*nsCString::from("size: aborted"))
                },
                Err(err) => unsafe { callback.Reject(&*nsCString::from(err.to_string())) },
            }
        })
        .detach();

        Ok(())
    }

    xpcom_method!(
        put => Put(
            callback: *const nsIKeyValueVoidCallback,
            key: *const nsACString,
            value: *const nsIVariant
        )
    );
    fn put(
        &self,
        callback: &nsIKeyValueVoidCallback,
        key: &nsACString,
        value: &nsIVariant,
    ) -> Result<(), Infallible> {
        let inputs = || -> Result<_, InterfaceError> {
            let store = self.store()?;
            let key = Key::try_from(key)?;
            let value = Value::from_variant(value)?;
            Ok((store, key, value))
        }();

        let name = self.name.clone();
        let request = moz_task::spawn_blocking("skv:KeyValueDatabase:Put:Request", async move {
            let (store, key, value) = inputs?;
            let db = Database::new(&store, &name);
            Ok(db.put(&[(key, value)])?)
        });

        let signal = self.client.signal();
        let callback = RefPtr::new(callback);
        moz_task::spawn_local("skv:KeyValueDatabase:Put:Response", async move {
            match signal.aborting(request).await {
                Ok(()) => unsafe { callback.Resolve() },
                Err(InterfaceError::Abort(_)) => unsafe {
                    callback.Reject(&*nsCString::from("put: aborted"))
                },
                Err(err) => unsafe { callback.Reject(&*nsCString::from(err.to_string())) },
            }
        })
        .detach();

        Ok(())
    }

    xpcom_method!(
        write_many => WriteMany(
            callback: *const nsIKeyValueVoidCallback,
            pairs: *const ThinVec<Option<RefPtr<nsIKeyValuePair>>>
        )
    );
    fn write_many(
        &self,
        callback: &nsIKeyValueVoidCallback,
        pairs: &ThinVec<Option<RefPtr<nsIKeyValuePair>>>,
    ) -> Result<(), Infallible> {
        let inputs = || -> Result<_, InterfaceError> {
            let store = self.store()?;
            let pairs = pairs
                .into_iter()
                .flatten()
                .map(|pair| {
                    let mut key = nsCString::new();
                    unsafe { pair.GetKey(&mut *key) }.to_result()?;
                    let value = getter_addrefs(|p| unsafe { pair.GetValue(p) })?;
                    Ok((Key::try_from(&*key)?, Value::from_variant(value.coerce())?))
                })
                .collect::<Result<Vec<_>, InterfaceError>>()?;
            Ok((store, pairs))
        }();

        let name = self.name.clone();
        let request =
            moz_task::spawn_blocking("skv:KeyValueDatabase:WriteMany:Request", async move {
                let (store, pairs) = inputs?;
                let db = Database::new(&store, &name);
                Ok(db.put(pairs.as_slice())?)
            });

        let signal = self.client.signal();
        let callback = RefPtr::new(callback);
        moz_task::spawn_local("skv:KeyValueDatabase:WriteMany:Response", async move {
            match signal.aborting(request).await {
                Ok(()) => unsafe { callback.Resolve() },
                Err(InterfaceError::Abort(_)) => unsafe {
                    callback.Reject(&*nsCString::from("writeMany: aborted"))
                },
                Err(err) => unsafe { callback.Reject(&*nsCString::from(err.to_string())) },
            }
        })
        .detach();

        Ok(())
    }

    xpcom_method!(
        get => Get(
            callback: *const nsIKeyValueVariantCallback,
            key: *const nsACString,
            default_value: *const nsIVariant
        )
    );
    fn get(
        &self,
        callback: &nsIKeyValueVariantCallback,
        key: &nsACString,
        default_value: &nsIVariant,
    ) -> Result<(), Infallible> {
        let inputs = || -> Result<_, InterfaceError> {
            let store = self.store()?;
            let key = Key::try_from(key)?;
            Ok((store, key))
        }();

        let name = self.name.clone();
        let request = moz_task::spawn_blocking("skv:KeyValueDatabase:Get:Request", async move {
            let (store, key) = inputs?;
            let db = Database::new(&store, &name);
            Ok(db.get(&key, &GetOptions::new())?)
        });

        let signal = self.client.signal();
        let default_value = RefPtr::new(default_value);
        let callback = RefPtr::new(callback);
        moz_task::spawn_local("skv:KeyValueDatabase:Get:Response", async move {
            let result = signal.aborting(request).await.and_then(|value| {
                Ok(match value {
                    Some(value) => value.to_variant()?,
                    None => default_value,
                })
            });
            match result {
                Ok(variant) => unsafe { callback.Resolve(variant.coerce()) },
                Err(InterfaceError::Abort(_)) => unsafe {
                    callback.Reject(&*nsCString::from("get: aborted"))
                },
                Err(err) => unsafe { callback.Reject(&*nsCString::from(err.to_string())) },
            }
        })
        .detach();

        Ok(())
    }

    xpcom_method!(
        has => Has(callback: *const nsIKeyValueVariantCallback, key: *const nsACString)
    );
    fn has(
        &self,
        callback: &nsIKeyValueVariantCallback,
        key: &nsACString,
    ) -> Result<(), Infallible> {
        let inputs = || -> Result<_, InterfaceError> {
            let store = self.store()?;
            let key = Key::try_from(key)?;
            Ok((store, key))
        }();

        let name = self.name.clone();
        let request = moz_task::spawn_blocking("skv:KeyValueDatabase:Has:Request", async move {
            let (store, key) = inputs?;
            let db = Database::new(&store, &name);
            Ok(db.has(&key, &GetOptions::new())?)
        });

        let signal = self.client.signal();
        let callback = RefPtr::new(callback);
        moz_task::spawn_local("skv:KeyValueDatabase:Has:Response", async move {
            match signal.aborting(request).await {
                Ok(b) => unsafe { callback.Resolve(b.into_variant().coerce()) },
                Err(InterfaceError::Abort(_)) => unsafe {
                    callback.Reject(&*nsCString::from("has: aborted"))
                },
                Err(err) => unsafe { callback.Reject(&*nsCString::from(err.to_string())) },
            }
        })
        .detach();

        Ok(())
    }

    xpcom_method!(
        delete => Delete(callback: *const nsIKeyValueVoidCallback, key: *const nsACString)
    );
    fn delete(
        &self,
        callback: &nsIKeyValueVoidCallback,
        key: &nsACString,
    ) -> Result<(), Infallible> {
        let inputs = || -> Result<_, InterfaceError> {
            let store = self.store()?;
            let key = Key::try_from(key)?;
            Ok((store, key))
        }();

        let name = self.name.clone();
        let request = moz_task::spawn_blocking("skv:KeyValueDatabase:Delete:Request", async move {
            let (store, key) = inputs?;
            let db = Database::new(&store, &name);
            Ok(db.delete(&key)?)
        });

        let signal = self.client.signal();
        let callback = RefPtr::new(callback);
        moz_task::spawn_local("skv:KeyValueDatabase:Delete:Response", async move {
            match signal.aborting(request).await {
                Ok(()) => unsafe { callback.Resolve() },
                Err(InterfaceError::Abort(_)) => unsafe {
                    callback.Reject(&*nsCString::from("delete: aborted"))
                },
                Err(err) => unsafe { callback.Reject(&*nsCString::from(err.to_string())) },
            }
        })
        .detach();

        Ok(())
    }

    xpcom_method!(
        delete_range => DeleteRange(
            callback: *const nsIKeyValueVoidCallback,
            from_key: *const nsACString,
            to_key: *const nsACString
        )
    );
    fn delete_range(
        &self,
        callback: &nsIKeyValueVoidCallback,
        from_key: &nsACString,
        to_key: &nsACString,
    ) -> Result<(), Infallible> {
        let inputs = || -> Result<_, InterfaceError> {
            let store = self.store()?;
            let from_key = match from_key.is_empty() {
                true => Bound::Unbounded,
                false => Bound::Included(Key::try_from(from_key)?),
            };
            let to_key = match to_key.is_empty() {
                true => Bound::Unbounded,
                false => Bound::Excluded(Key::try_from(to_key)?),
            };
            Ok((store, from_key, to_key))
        }();

        let name = self.name.clone();
        let request =
            moz_task::spawn_blocking("skv:KeyValueDatabase:DeleteRange:Request", async move {
                let (store, from_key, to_key) = inputs?;
                let db = Database::new(&store, &name);
                Ok(db.delete_range((from_key, to_key))?)
            });

        let signal = self.client.signal();
        let callback = RefPtr::new(callback);
        moz_task::spawn_local("skv:KeyValueDatabase:DeleteRange:Response", async move {
            match signal.aborting(request).await {
                Ok(()) => unsafe { callback.Resolve() },
                Err(InterfaceError::Abort(_)) => unsafe {
                    callback.Reject(&*nsCString::from("deleteRange: aborted"))
                },
                Err(err) => unsafe { callback.Reject(&*nsCString::from(err.to_string())) },
            }
        })
        .detach();

        Ok(())
    }

    xpcom_method!(
        clear => Clear(callback: *const nsIKeyValueVoidCallback)
    );
    fn clear(&self, callback: &nsIKeyValueVoidCallback) -> Result<(), Infallible> {
        let store = self.store();
        let name = self.name.clone();
        let request = moz_task::spawn_blocking("skv:KeyValueDatabase:Clear:Request", async move {
            let store = store?;
            let db = Database::new(&store, &name);
            Ok(db.clear()?)
        });

        let signal = self.client.signal();
        let callback = RefPtr::new(callback);
        moz_task::spawn_local("skv:KeyValueDatabase:Clear:Response", async move {
            match signal.aborting(request).await {
                Ok(()) => unsafe { callback.Resolve() },
                Err(InterfaceError::Abort(_)) => unsafe {
                    callback.Reject(&*nsCString::from("clear: aborted"))
                },
                Err(err) => unsafe { callback.Reject(&*nsCString::from(err.to_string())) },
            }
        })
        .detach();

        Ok(())
    }

    xpcom_method!(
        enumerate => Enumerate(
            callback: *const nsIKeyValueEnumeratorCallback,
            from_key: *const nsACString,
            to_key: *const nsACString
        )
    );
    fn enumerate(
        &self,
        callback: &nsIKeyValueEnumeratorCallback,
        from_key: &nsACString,
        to_key: &nsACString,
    ) -> Result<(), Infallible> {
        let inputs = || -> Result<_, InterfaceError> {
            let store = self.store()?;
            let from_key = match from_key.is_empty() {
                true => Bound::Unbounded,
                false => Bound::Included(Key::try_from(from_key)?),
            };
            let to_key = match to_key.is_empty() {
                true => Bound::Unbounded,
                false => Bound::Excluded(Key::try_from(to_key)?),
            };
            Ok((store, from_key, to_key))
        }();

        let name = self.name.clone();
        let request =
            moz_task::spawn_blocking("skv:KeyValueDatabase:Enumerate:Request", async move {
                let (store, from_key, to_key) = inputs?;
                let db = Database::new(&store, &name);
                Ok(db.enumerate((from_key, to_key), GetOptions::new().concurrent(true))?)
            });

        let signal = self.client.signal();
        let callback = RefPtr::new(callback);
        moz_task::spawn_local("skv:KeyValueDatabase:Enumerate:Response", async move {
            match signal.aborting(request).await {
                Ok(pairs) => {
                    let enumerator = KeyValueEnumerator::new(pairs);
                    unsafe { callback.Resolve(enumerator.coerce()) }
                }
                Err(InterfaceError::Abort(_)) => unsafe {
                    callback.Reject(&*nsCString::from("enumerate: aborted"))
                },
                Err(err) => unsafe { callback.Reject(&*nsCString::from(err.to_string())) },
            }
        })
        .detach();

        Ok(())
    }

    xpcom_method!(
        close => Close(
            callback: *const nsIKeyValueVoidCallback
        )
    );
    fn close(&self, callback: &nsIKeyValueVoidCallback) -> Result<(), Infallible> {
        let client = self.client.clone();
        let request = moz_task::spawn_blocking("skv:KeyValueDatabase:Close:Request", async move {
            client.invalidate()
        });

        let callback = RefPtr::new(callback);
        moz_task::spawn_local("skv:KeyValueDatabase:Close:Response", async move {
            request.await;
            unsafe { callback.Resolve() };
        })
        .detach();

        Ok(())
    }
}

#[xpcom(implement(nsIKeyValueEnumerator), atomic)]
pub struct KeyValueEnumerator {
    iter: AtomicRefCell<std::vec::IntoIter<(Key, Value)>>,
}

impl KeyValueEnumerator {
    fn new(pairs: Vec<(Key, Value)>) -> RefPtr<Self> {
        KeyValueEnumerator::allocate(InitKeyValueEnumerator {
            iter: AtomicRefCell::new(pairs.into_iter()),
        })
    }

    xpcom_method!(has_more_elements => HasMoreElements() -> bool);
    fn has_more_elements(&self) -> Result<bool, Infallible> {
        Ok(!self.iter.borrow().as_slice().is_empty())
    }

    xpcom_method!(get_next => GetNext() -> *const nsIKeyValuePair);
    fn get_next(&self) -> Result<RefPtr<nsIKeyValuePair>, nsresult> {
        let mut iter = self.iter.borrow_mut();
        let (key, value) = iter.next().ok_or(nserror::NS_ERROR_FAILURE)?;
        let pair = KeyValuePair::new(key, value);
        Ok(RefPtr::new(pair.coerce()))
    }
}

#[xpcom(implement(nsIKeyValuePair), atomic)]
pub struct KeyValuePair {
    key: Key,
    value: Value,
}

impl KeyValuePair {
    fn new(key: Key, value: Value) -> RefPtr<Self> {
        KeyValuePair::allocate(InitKeyValuePair { key, value })
    }

    xpcom_method!(get_key => GetKey() -> nsACString);
    fn get_key(&self) -> Result<nsCString, Infallible> {
        Ok(self.key.clone().into())
    }

    xpcom_method!(get_value => GetValue() -> *const nsIVariant);
    fn get_value(&self) -> Result<RefPtr<nsIVariant>, nsresult> {
        self.value
            .to_variant()
            .map_err(|_| nserror::NS_ERROR_FAILURE)
    }
}

#[xpcom(implement(nsIAsyncShutdownBlocker), nonatomic)]
pub struct KeyValueServiceShutdownBlocker {
    client: CoordinatorClient<'static>,
}

impl KeyValueServiceShutdownBlocker {
    fn new(client: CoordinatorClient<'static>) -> RefPtr<Self> {
        assert!(moz_task::is_main_thread());
        KeyValueServiceShutdownBlocker::allocate(InitKeyValueServiceShutdownBlocker { client })
    }

    /// Registers a blocker to close all open databases on shutdown.
    ///
    /// This function may be called on any thread, but the blocker itself is
    /// registered on the main thread, because `nsIAsyncShutdownService` is
    /// main-thread-only.
    fn register(client: CoordinatorClient<'static>) {
        let Ok(main_thread) = moz_task::get_main_thread() else {
            return;
        };
        moz_task::spawn_onto(
            "skv:KeyValueServiceShutdownBlocker:Register",
            main_thread.coerce(),
            async move {
                let _ = || -> Result<(), InterfaceError> {
                    let service =
                        xpcom::components::AsyncShutdown::service::<nsIAsyncShutdownService>()?;
                    let barrier = getter_addrefs(|p| unsafe { service.GetProfileBeforeChange(p) })?;
                    let blocker = Self::new(client);
                    unsafe {
                        barrier.AddBlocker(
                            blocker.coerce(),
                            &*nsString::from(file!()),
                            line!() as i32,
                            &*nsString::new(),
                        )
                    }
                    .to_result()?;
                    Ok(())
                }();
            },
        )
        .detach();
    }

    xpcom_method!(
        block_shutdown => BlockShutdown(
            barrier: *const nsIAsyncShutdownClient
        )
    );
    fn block_shutdown(&self, barrier: &nsIAsyncShutdownClient) -> Result<(), Infallible> {
        assert!(moz_task::is_main_thread());

        let client = self.client.clone();
        let request = moz_task::spawn_blocking(
            "skv:KeyValueServiceShutdownBlocker:BlockShutdown:Request",
            async move { client.invalidate() },
        );

        let barrier = RefPtr::new(barrier);
        let blocker = RefPtr::new(self);
        moz_task::spawn_local(
            "skv:KeyValueServiceShutdownBlocker:BlockShutdown:Response",
            async move {
                request.await;
                unsafe { barrier.RemoveBlocker(blocker.coerce()) };
            },
        )
        .detach();

        Ok(())
    }

    xpcom_method!(
        get_name => GetName() -> nsAString
    );
    fn get_name(&self) -> Result<nsString, Infallible> {
        Ok("KeyValueService: shutdown".into())
    }

    xpcom_method!(
        get_state => GetState() -> *const nsIPropertyBag
    );
    fn get_state(&self) -> Result<RefPtr<nsIPropertyBag>, Infallible> {
        let state = HashPropertyBag::new();
        Ok(RefPtr::new(state.bag().coerce()))
    }
}

#[xpcom(implement(nsIKeyValueImporter), atomic)]
pub struct RkvSafeModeKeyValueImporter {
    client: CoordinatorClient<'static>,
    dir: nsString,
    conflict_policy: AtomicCell<ConflictPolicy>,
    cleanup_policy: AtomicCell<CleanupPolicy>,
}

impl RkvSafeModeKeyValueImporter {
    const TYPE: &'static str = "rkv-safe-mode";

    pub fn new(client: CoordinatorClient<'static>, dir: nsString) -> RefPtr<Self> {
        RkvSafeModeKeyValueImporter::allocate(InitRkvSafeModeKeyValueImporter {
            client,
            dir,
            conflict_policy: ConflictPolicy::Error.into(),
            cleanup_policy: CleanupPolicy::Keep.into(),
        })
    }

    xpcom_method!(get_type => GetType() -> nsACString);
    fn get_type(&self) -> Result<nsCString, Infallible> {
        Ok(Self::TYPE.into())
    }

    xpcom_method!(get_path => GetPath() -> nsAString);
    fn get_path(&self) -> Result<nsString, Infallible> {
        Ok(self.dir.clone())
    }

    xpcom_method!(get_conflict_policy => GetConflictPolicy() -> u8);
    fn get_conflict_policy(&self) -> Result<u8, Infallible> {
        Ok(self.conflict_policy.load() as u8)
    }

    xpcom_method!(set_conflict_policy => SetConflictPolicy(policy: u8));
    fn set_conflict_policy(&self, policy: u8) -> Result<(), nsresult> {
        self.conflict_policy
            .store(ConflictPolicy::try_from(policy).map_err(|_| nserror::NS_ERROR_INVALID_ARG)?);
        Ok(())
    }

    xpcom_method!(get_cleanup_policy => GetCleanupPolicy() -> u8);
    fn get_cleanup_policy(&self) -> Result<u8, Infallible> {
        Ok(self.cleanup_policy.load() as u8)
    }

    xpcom_method!(set_cleanup_policy => SetCleanupPolicy(policy: u8));
    fn set_cleanup_policy(&self, policy: u8) -> Result<(), nsresult> {
        self.cleanup_policy
            .store(CleanupPolicy::try_from(policy).map_err(|_| nserror::NS_ERROR_INVALID_ARG)?);
        Ok(())
    }

    xpcom_method!(
        import => Import(
            callback: *const nsIKeyValueVoidCallback,
            name: *const nsACString
        )
    );
    fn import(
        &self,
        callback: &nsIKeyValueVoidCallback,
        name: &nsACString,
    ) -> Result<(), Infallible> {
        let client = self.client.clone();
        let dir = self.dir.clone();
        let name = name.to_utf8().into_owned();
        let conflict_policy = self.conflict_policy.load();
        let cleanup_policy = self.cleanup_policy.load();
        let request = moz_task::spawn_blocking(
            "skv:RkvSafeModeKeyValueImporter:Import:Request",
            async move {
                let dir = crate::fs::canonicalize(&*dir).map_err(InterfaceError::StorageDir)?;
                let store = client.store_for_path(StorePath::for_storage_dir(dir.clone()))?;
                let mut manager = rkv::Manager::<RkvSafeModeEnvironment>::singleton()
                    .write()
                    .unwrap();

                let builder = Rkv::environment_builder::<RkvSafeMode>();
                let rkv = manager.get_or_create_from_builder(
                    dir.as_path(),
                    builder,
                    Rkv::from_builder::<RkvSafeMode>,
                )?;
                let env = rkv.read().unwrap();

                let mut importer = RkvImporter::for_database(&*env, &store, &name)?;
                importer
                    .conflict_policy(conflict_policy)
                    .cleanup_policy(cleanup_policy);
                importer.import()?;
                importer.cleanup();

                Ok(())
            },
        );

        let signal = self.client.signal();
        let callback = RefPtr::new(callback);
        moz_task::spawn_local(
            "skv:RkvSafeModeKeyValueImporter:Import:Response",
            async move {
                match signal.aborting(request).await {
                    Ok(()) => unsafe { callback.Resolve() },
                    Err(InterfaceError::Abort(_)) => unsafe {
                        callback.Reject(&*nsCString::from("import: aborted"))
                    },
                    Err(err) => unsafe { callback.Reject(&*nsCString::from(err.to_string())) },
                }
            },
        )
        .detach();

        Ok(())
    }

    xpcom_method!(
        import_all => ImportAll(
            callback: *const nsIKeyValueVoidCallback
        )
    );
    fn import_all(&self, callback: &nsIKeyValueVoidCallback) -> Result<(), Infallible> {
        let client = self.client.clone();
        let dir = self.dir.clone();
        let conflict_policy = self.conflict_policy.load();
        let cleanup_policy = self.cleanup_policy.load();
        let request = moz_task::spawn_blocking(
            "skv:RkvSafeModeKeyValueImporter:ImportAll:Request",
            async move {
                let dir = crate::fs::canonicalize(&*dir).map_err(InterfaceError::StorageDir)?;
                let store = client.store_for_path(StorePath::for_storage_dir(dir.clone()))?;
                let mut manager = rkv::Manager::<RkvSafeModeEnvironment>::singleton()
                    .write()
                    .unwrap();

                {
                    // Scope for the Rkv environment.
                    let builder = Rkv::environment_builder::<RkvSafeMode>();
                    let rkv = manager.get_or_create_from_builder(
                        dir.as_path(),
                        builder,
                        Rkv::from_builder::<RkvSafeMode>,
                    )?;
                    let env = rkv.read().unwrap();

                    let mut importer = RkvImporter::for_all_databases(&*env, &store)?;
                    importer
                        .conflict_policy(conflict_policy)
                        .cleanup_policy(cleanup_policy);
                    importer.import()?;
                    importer.cleanup();
                }

                // At this point, we've already imported all the databases,
                // so if we can't close the environment and optionally
                // delete all its (now-empty) files, that's OK.
                let _ = manager.try_close(
                    dir.as_path(),
                    match cleanup_policy {
                        CleanupPolicy::Keep => rkv::CloseOptions::default(),
                        CleanupPolicy::Delete => rkv::CloseOptions::delete_files_on_disk(),
                    },
                );

                Ok(())
            },
        );

        let signal = self.client.signal();
        let callback = RefPtr::new(callback);
        moz_task::spawn_local(
            "skv:RkvSafeModeKeyValueImporter:ImportAll:Response",
            async move {
                match signal.aborting(request).await {
                    Ok(()) => unsafe { callback.Resolve() },
                    Err(InterfaceError::Abort(_)) => unsafe {
                        callback.Reject(&*nsCString::from("importAll: aborted"))
                    },
                    Err(err) => unsafe { callback.Reject(&*nsCString::from(err.to_string())) },
                }
            },
        )
        .detach();

        Ok(())
    }
}

/// The error type for interface methods that never return an error.
///
/// This is equivalent to [`std::convert::Infallible`], but implements
/// `Into<nsresult>`, which we can't implement on [`std::convert::Infallible`]
/// because of the orphan rule.
enum Infallible {}

impl From<Infallible> for nsresult {
    fn from(_: Infallible) -> Self {
        nserror::NS_ERROR_FAILURE
    }
}

#[derive(Debug, thiserror::Error)]
pub enum InterfaceError {
    #[error("coordinator: {0}")]
    Coordinator(#[from] CoordinatorError),
    #[error(transparent)]
    Abort(#[from] AbortError),
    #[error("storage dir: {0}")]
    StorageDir(io::Error),
    #[error("database: {0}")]
    Database(#[from] DatabaseError),
    #[error("store: {0}")]
    Store(#[from] StoreError),
    #[error("key: {0}")]
    Key(#[from] KeyError),
    #[error("value: {0}")]
    Value(#[from] ValueError),
    #[error("rkv store: {0}")]
    RkvStore(#[from] rkv::StoreError),
    #[error("importer: {0}")]
    Importer(#[from] ImporterError),
    #[error("error code: {0}")]
    Nsresult(#[from] nsresult),
}
