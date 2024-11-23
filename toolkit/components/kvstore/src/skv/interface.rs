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
        nsIKeyValueDatabaseImportOptions, nsIKeyValueEnumeratorCallback,
        nsIKeyValueImportSourceSpec, nsIKeyValueImporter, nsIKeyValuePair,
        nsIKeyValueVariantCallback, nsIKeyValueVoidCallback, nsIPropertyBag, nsIVariant,
    },
    xpcom, xpcom_method, RefPtr,
};

use crate::{
    fs::WidePathBuf,
    skv::{
        abort::AbortError,
        coordinator::{Coordinator, CoordinatorClient, CoordinatorError},
        database::{Database, DatabaseError, GetOptions},
        importer::{
            CleanupPolicy, ConflictPolicy, ImporterError, NamedSourceDatabase, RkvImporter,
            SourceDatabases,
        },
        key::Key,
        store::{Store, StoreError, StorePath},
        value::{Value, ValueError},
    },
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
        let dir = WidePathBuf::new(dir);
        let request =
            moz_task::spawn_blocking("skv:KeyValueService:GetOrCreate:Request", async move {
                Ok((
                    client.child_with_name("skv:KeyValueDatabase")?,
                    StorePath::canonicalizing(dir)?,
                ))
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
                let importer = RkvSafeModeKeyValueImporter::new(client, WidePathBuf::new(dir));
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
            let key = Key::from(key);
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
                    Ok((Key::from(&*key), Value::from_variant(value.coerce())?))
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
            let key = Key::from(key);
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
            let key = Key::from(key);
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
            let key = Key::from(key);
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
                false => Bound::Included(Key::from(from_key)),
            };
            let to_key = match to_key.is_empty() {
                true => Bound::Unbounded,
                false => Bound::Excluded(Key::from(to_key)),
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
                false => Bound::Included(Key::from(from_key)),
            };
            let to_key = match to_key.is_empty() {
                true => Bound::Unbounded,
                false => Bound::Excluded(Key::from(to_key)),
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
    specs: AtomicRefCell<Vec<RefPtr<KeyValueImportSourceSpec>>>,
}

impl RkvSafeModeKeyValueImporter {
    const TYPE: &'static str = "rkv-safe-mode";

    pub fn new(client: CoordinatorClient<'static>, dir: WidePathBuf) -> RefPtr<Self> {
        RkvSafeModeKeyValueImporter::allocate(InitRkvSafeModeKeyValueImporter {
            client,
            // The first spec is always the destination storage directory.
            specs: vec![KeyValueImportSourceSpec::new(dir)].into(),
        })
    }

    xpcom_method!(get_type => GetType() -> nsACString);
    fn get_type(&self) -> Result<nsCString, Infallible> {
        Ok(Self::TYPE.into())
    }

    xpcom_method!(get_path => GetPath() -> nsAString);
    fn get_path(&self) -> Result<nsString, Infallible> {
        self.specs.borrow()[0].get_path()
    }

    xpcom_method!(add_path => AddPath(dir: *const nsAString) -> *const nsIKeyValueImportSourceSpec);
    fn add_path(&self, dir: &nsAString) -> Result<RefPtr<nsIKeyValueImportSourceSpec>, Infallible> {
        let spec = KeyValueImportSourceSpec::new(WidePathBuf::new(dir));
        self.specs.borrow_mut().push(spec.clone());
        Ok(RefPtr::new(spec.coerce()))
    }

    xpcom_method!(
        add_database => AddDatabase(
            name: *const nsACString
        ) -> *const nsIKeyValueDatabaseImportOptions
    );
    fn add_database(
        &self,
        name: &nsACString,
    ) -> Result<RefPtr<nsIKeyValueDatabaseImportOptions>, nsresult> {
        self.specs.borrow()[0].add_database(name)
    }

    xpcom_method!(add_all_databases => AddAllDatabases() -> *const nsIKeyValueDatabaseImportOptions);
    fn add_all_databases(&self) -> Result<RefPtr<nsIKeyValueDatabaseImportOptions>, nsresult> {
        self.specs.borrow()[0].add_all_databases()
    }

    xpcom_method!(
        import => Import(
            callback: *const nsIKeyValueVoidCallback
        )
    );
    fn import(&self, callback: &nsIKeyValueVoidCallback) -> Result<(), Infallible> {
        let client = self.client.clone();
        let specs = self.specs.borrow().clone();
        let request = moz_task::spawn_blocking(
            "skv:RkvSafeModeKeyValueImporter:Import:Request",
            async move {
                let mut manager = rkv::Manager::<RkvSafeModeEnvironment>::singleton()
                    .write()
                    .unwrap();
                let store =
                    client.store_for_path(StorePath::canonicalizing(specs[0].dir.clone())?)?;

                // An Rkv environment is unique to a storage directory;
                // if we're importing from multiple directories, we need to
                // open an environment for each directory.
                let rkvs = {
                    let mut rkvs = Vec::with_capacity(specs.len());
                    for spec in specs {
                        let Some(dbs) = spec.to_source_databases() else {
                            continue;
                        };
                        let dir = spec.dir.canonicalize()?;
                        let builder = Rkv::environment_builder::<RkvSafeMode>();
                        let rkv = manager.get_or_create_from_builder(
                            dir.as_path(),
                            builder,
                            Rkv::from_builder::<RkvSafeMode>,
                        )?;
                        rkvs.push((rkv, dbs));
                    }
                    rkvs
                };

                // Import all databases from each directory, in order.
                for (rkv, dbs) in rkvs {
                    let env = rkv.read().unwrap();
                    let importer = RkvImporter::new(&*env, &store, dbs)?;
                    importer.import()?;
                    importer.cleanup();
                }

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
}

#[derive(Clone)]
enum KeyValueImportSource {
    Databases(Vec<(String, RefPtr<KeyValueDatabaseImportOptions>)>),
    AllDatabases(RefPtr<KeyValueDatabaseImportOptions>),
}

#[xpcom(implement(nsIKeyValueImportSourceSpec), atomic)]
pub struct KeyValueImportSourceSpec {
    dir: WidePathBuf,
    source: AtomicRefCell<Option<KeyValueImportSource>>,
}

impl KeyValueImportSourceSpec {
    fn new(dir: WidePathBuf) -> RefPtr<Self> {
        KeyValueImportSourceSpec::allocate(InitKeyValueImportSourceSpec {
            dir,
            source: AtomicRefCell::default(),
        })
    }

    fn to_source_databases(&self) -> Option<SourceDatabases> {
        self.source.borrow().as_ref().map(|source| match source {
            KeyValueImportSource::Databases(dbs) => SourceDatabases::Named(
                dbs.iter()
                    .map(|(name, options)| NamedSourceDatabase {
                        name: name.clone().into(),
                        conflict_policy: options.conflict_policy.load(),
                        cleanup_policy: options.cleanup_policy.load(),
                    })
                    .collect(),
            ),
            KeyValueImportSource::AllDatabases(options) => SourceDatabases::All {
                conflict_policy: options.conflict_policy.load(),
                cleanup_policy: options.cleanup_policy.load(),
            },
        })
    }

    xpcom_method!(get_path => GetPath() -> nsAString);
    fn get_path(&self) -> Result<nsString, Infallible> {
        Ok(self.dir.as_wide().into())
    }

    xpcom_method!(
        add_database => AddDatabase(
            dir: *const nsACString
        ) -> *const nsIKeyValueDatabaseImportOptions
    );
    fn add_database(
        &self,
        name: &nsACString,
    ) -> Result<RefPtr<nsIKeyValueDatabaseImportOptions>, nsresult> {
        let mut guard = self.source.borrow_mut();
        let source = guard.get_or_insert_with(|| KeyValueImportSource::Databases(Vec::new()));
        let options = match source {
            KeyValueImportSource::Databases(dbs) => {
                let options = KeyValueDatabaseImportOptions::new();
                dbs.push((name.to_utf8().into_owned(), options.clone()));
                options
            }
            KeyValueImportSource::AllDatabases(_) => {
                return Err(nserror::NS_ERROR_ALREADY_INITIALIZED)
            }
        };
        Ok(RefPtr::new(options.coerce()))
    }

    xpcom_method!(add_all_databases => AddAllDatabases() -> *const nsIKeyValueDatabaseImportOptions);
    fn add_all_databases(&self) -> Result<RefPtr<nsIKeyValueDatabaseImportOptions>, nsresult> {
        let mut guard = self.source.borrow_mut();
        let options = match &mut *guard {
            Some(_) => return Err(nserror::NS_ERROR_ALREADY_INITIALIZED),
            None => {
                let options = KeyValueDatabaseImportOptions::new();
                *guard = Some(KeyValueImportSource::AllDatabases(options.clone()));
                options
            }
        };
        Ok(RefPtr::new(options.coerce()))
    }
}

#[xpcom(implement(nsIKeyValueDatabaseImportOptions), atomic)]
pub struct KeyValueDatabaseImportOptions {
    conflict_policy: AtomicCell<ConflictPolicy>,
    cleanup_policy: AtomicCell<CleanupPolicy>,
}

impl KeyValueDatabaseImportOptions {
    fn new() -> RefPtr<Self> {
        KeyValueDatabaseImportOptions::allocate(InitKeyValueDatabaseImportOptions {
            conflict_policy: AtomicCell::new(ConflictPolicy::Error),
            cleanup_policy: AtomicCell::new(CleanupPolicy::Keep),
        })
    }

    xpcom_method!(
        set_conflict_policy => SetConflictPolicy(
            policy: u8
        ) -> *const nsIKeyValueDatabaseImportOptions
    );
    fn set_conflict_policy(
        &self,
        policy: u8,
    ) -> Result<RefPtr<nsIKeyValueDatabaseImportOptions>, nsresult> {
        let policy = ConflictPolicy::try_from(policy).map_err(|_| nserror::NS_ERROR_INVALID_ARG)?;
        self.conflict_policy.store(policy);
        Ok(RefPtr::new(self.coerce()))
    }

    xpcom_method!(
        set_cleanup_policy => SetCleanupPolicy(
            policy: u8
        ) -> *const nsIKeyValueDatabaseImportOptions
    );
    fn set_cleanup_policy(
        &self,
        policy: u8,
    ) -> Result<RefPtr<nsIKeyValueDatabaseImportOptions>, nsresult> {
        let policy = CleanupPolicy::try_from(policy).map_err(|_| nserror::NS_ERROR_INVALID_ARG)?;
        self.cleanup_policy.store(policy);
        Ok(RefPtr::new(self.coerce()))
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
    #[error("database: {0}")]
    Database(#[from] DatabaseError),
    #[error("store: {0}")]
    Store(#[from] StoreError),
    #[error("value: {0}")]
    Value(#[from] ValueError),
    #[error("rkv store: {0}")]
    RkvStore(#[from] rkv::StoreError),
    #[error("importer: {0}")]
    Importer(#[from] ImporterError),
    #[error("I/O: {0}")]
    Io(#[from] io::Error),
    #[error("error code: {0}")]
    Nsresult(#[from] nsresult),
}
