/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Test wrapping sync Rust calls and making them async.
//!
//! This is intended to mimic what we do in application-services: create a synchrounous API, then
//! wrap it so that most methods are async and run in a worker queue.
//!
//!
//! This provides the essentially same functionality as the automatic async wrapping that is
//! available by editing `config.toml` so why do it?
//!
//!   - Implementing the async wrapping in Rust leads to better docs.  The auto-generated API
//!   reference will show which functions are sync and which are async.  Telling users that
//!   sync functions are actually actually async because of UniFFI magic would lead to confusion.
//!
//!   - It opens the door to future performance improvements.  Some methods could be async without
//!   this wrapper and without sending anything to a background thread.  Fxa API calls using an
//!   async HTTP client would be one example.  Some methods could be split into a "real" async part
//!   and a wrapped async part.
//!
//!   - Async Rust code allows for new patterns.  Maybe the FxA state machine would be better
//!   modelled as an async actor rather than a bunch of sync methods protected by a Mutex.

use std::{sync::{Arc, Mutex, OnceLock}, thread::sleep, time::Duration};

use crate::oneshot;

/// Worker queue scheduler trait
///
/// This is a trait that the foreign code implements to schedule Rust tasks in a worker queue.  On
/// Swift/Kotlin, we can implement this trait using a DispatchQueue/CoroutineContext.
///
/// On JS we can't implement it directly, since JS is single-threaded and we don't want to start up
/// a web worker.  Instead, we implement the trait in Rust using `moz_task`.
#[uniffi::export]
pub trait WorkerQueue: Send + Sync {
    fn add_task(&self, task: Arc<dyn RustTask>);
}


#[uniffi::export]
pub trait RustTask: Send + Sync {
    fn run(&self);
}

/// Initialize the global worker queue.  The Rust code will use this schedule sync tasks in the
/// background in order to present an async interface.
#[uniffi::export]
pub fn initialize_global_worker_queue(worker_queue: Arc<dyn WorkerQueue>) {
    GLOBAL_WORKER_QUEUE.set(worker_queue)
        .unwrap_or_else(|_| panic!("init_global_worker_queue called twice"));
}

static GLOBAL_WORKER_QUEUE: OnceLock<Arc<dyn WorkerQueue>> = OnceLock::new();

/// Schedule a closure to run in the global worker queue.  Returns the result of the closure
/// asynchronously
pub async fn run_in_background<T: Send + Sync + 'static>(task: impl FnOnce() -> T + Send + Sync + 'static) -> T {
    let (tx, rx) = oneshot::channel();

    GLOBAL_WORKER_QUEUE.get()
        .unwrap_or_else(|| panic!("init_global_worker_queue never called"))
        .add_task(RustTaskContainer::new_arc(move || {
            tx.send(task())
        }));
    rx.await
}

/// Implements RustTask for any closure
///
/// The one tricky part is that the task can only be run once, but the foreign language gets a
/// shared reference to it.  This implementation uses a Mutex, in the real world, `UnsafeCell` +
/// `Once` might be a better option.
struct RustTaskContainer<T: FnOnce() + Send + Sync>(Mutex<Option<T>>);

impl<T: FnOnce() + Send + Sync> RustTaskContainer<T> {
    fn new_arc(task: T) -> Arc<Self> {
        Arc::new(Self(Mutex::new(Some(task))))
    }
}

impl<T: FnOnce() + Send + Sync> RustTask for RustTaskContainer<T> {
    fn run(&self) {
        if let Some(f) = self.0.lock().unwrap().take() {
            f()
        }
    }
}

/// Initialize the global worker for JS.
///
/// All code above here is generalized task code that works for JS, Kotlin, and Swift.
///
/// This function and the GeckoWorkerQueue struct are Gecko-specific.
#[uniffi::export]
pub fn initialize_gecko_global_worker_queue() {
    initialize_global_worker_queue(Arc::new(GeckoWorkerQueue));
}

struct GeckoWorkerQueue;

impl WorkerQueue for GeckoWorkerQueue {
    // This version is what runs when we're linked into libgecko
    #[cfg(feature = "moz_task")]
    fn add_task(&self, task: Arc<dyn RustTask>) {
        if let Err(e) = moz_task::dispatch_background_task("UniFFI task", move || task.run()) {
            log::error!("Failed to dispatch background task: {e}");
        }
    }

    // This is as stub that allows us to build a library for uniffi-bindgen to use, but without
    // depending on gecko.  Gecko can only be built inside of `./mach build` and we want to build
    // the library using plain `cargo build`.
    #[cfg(not(feature = "moz_task"))]
    fn add_task(&self, _task: Arc<dyn RustTask>) {
        panic!("moz_task not enabled");
    }
}

/// Function to test the global worker queue
///
/// This is how Rust components can wrap synchronous tasks to present an async interface.
#[uniffi::export]
pub async fn expensive_computation() -> u32 {
    run_in_background(|| {
        sleep(Duration::from_millis(1));
        1000
    }).await
}
