/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{
    collections::btree_map::BTreeMap,
    fmt::Debug,
    future::Future,
    mem,
    num::NonZeroUsize,
    pin::Pin,
    sync::{Arc, Mutex},
    task::{Context, Poll, Waker},
};

use futures::future::{self, Either};

/// An object that signals when a task, or a group of tasks,
/// should be aborted.
///
/// An [`AbortController`] works like the DOM API of the same name.
/// Each controller has one or more associated [`AbortSignal`]s, which are
/// futures that observe the controller. When the controller tells its signals
/// to abort, any tasks that are waiting on those signals can resume executing.
///
/// [`AbortController`] is similar to [`future::AbortHandle`], but can wake up
/// more than one task that's waiting on an associated [`AbortSignal`].
pub struct AbortController {
    state: Arc<Mutex<AbortState>>,
}

impl AbortController {
    /// Creates a new controller.
    pub fn new() -> Self {
        Self {
            state: Arc::new(Mutex::new(AbortState::Armed {
                next_id: NonZeroUsize::MIN,
                wakers: BTreeMap::new(),
            })),
        }
    }

    /// Creates an associated signal which can be waited on.
    pub fn signal(&self) -> AbortSignal {
        AbortSignal {
            id: AbortSignalId::Unreserved,
            state: self.state.clone(),
        }
    }

    /// Tells all associated signals to abort.
    pub fn abort(&self) {
        let wakers = match mem::replace(&mut *self.state.lock().unwrap(), AbortState::Aborted) {
            AbortState::Aborted => return,
            AbortState::Armed { wakers, .. } => wakers,
        };
        for waker in wakers.into_values() {
            waker.wake()
        }
    }
}

impl Default for AbortController {
    fn default() -> Self {
        Self::new()
    }
}

impl Debug for AbortController {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("AbortController { .. }")
    }
}

/// A future that completes when its associated [`AbortController`]
/// is aborted.
///
/// [`AbortSignal`] is similar to [`future::AbortRegistration`] and
/// [`future::Abortable`], but can be cloned and shared between
/// multiple tasks.
pub struct AbortSignal {
    id: AbortSignalId,
    state: Arc<Mutex<AbortState>>,
}

impl AbortSignal {
    /// Consumes this signal and waits for either `f` to complete, or the
    /// signal to fire; whichever happens first.
    pub async fn aborting<T, E, F>(self, f: F) -> Result<T, E>
    where
        F: Future<Output = Result<T, E>>,
        E: From<AbortError>,
    {
        futures::pin_mut!(f);
        match future::select(f, self).await {
            Either::Left((result, _)) => result,
            Either::Right((_, _)) => Err(AbortError)?,
        }
    }
}

impl Clone for AbortSignal {
    fn clone(&self) -> Self {
        AbortSignal {
            id: AbortSignalId::Unreserved,
            state: Arc::clone(&self.state),
        }
    }
}

impl Future for AbortSignal {
    type Output = ();

    fn poll(self: Pin<&mut Self>, context: &mut Context<'_>) -> Poll<Self::Output> {
        let this = self.get_mut();
        let mut guard = this.state.lock().unwrap();

        let AbortState::Armed { next_id, wakers } = &mut *guard else {
            // Our controller has been aborted. Forget our ID now,
            // so that we can skip re-locking and cleaning up in
            // `drop` later.
            this.id = AbortSignalId::Unreserved;
            return Poll::Ready(());
        };

        let id = match this.id {
            AbortSignalId::Reserved(id) => id,
            AbortSignalId::Unreserved => {
                // A task is waiting on us for the first time.
                // Reserve our ID.
                let this_id = *next_id;
                *next_id = this_id.checked_add(1).unwrap();
                this.id = AbortSignalId::Reserved(this_id);
                this_id
            }
        };

        wakers
            .entry(id)
            .and_modify(|waker| {
                // The `Waker` docs recommend using `clone_from` to
                // avoid cloning a waker that would wake the same task.
                waker.clone_from(context.waker());
            })
            .or_insert_with(|| context.waker().clone());

        Poll::Pending
    }
}

impl Drop for AbortSignal {
    fn drop(&mut self) {
        if let AbortSignalId::Reserved(id) = self.id {
            // We only need to lock and update the shared state if
            // we have at least one task waiting on us.
            if let AbortState::Armed { wakers, .. } = &mut *self.state.lock().unwrap() {
                wakers.remove(&id);
            }
        }
    }
}

impl Debug for AbortSignal {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "AbortSignal {{ id: {:?}, .. }}", self.id)
    }
}

/// Uniquely identifies an [`AbortSignal`].
///
/// ### Notes
///
/// [`AbortSignalId`] takes advantage of Rust's [discriminant elision][1] for
/// option-like enums, so it has the same size and alignment as `usize`.
///
/// [1]: https://rust-lang.github.io/unsafe-code-guidelines/layout/enums.html
#[derive(Clone, Copy, Debug)]
enum AbortSignalId {
    /// Indicates that an ID has not been reserved for this [`AbortSignal`],
    /// because no tasks are currently waiting on it.
    Unreserved,

    /// The ID for an [`AbortSignal`] with one or more tasks waiting on it.
    Reserved(NonZeroUsize),
}

/// State shared between an [`AbortController`] and its [`AbortSignal`]s.
///
/// ### Notes
///
/// Controllers and signals share state using an `Arc<Mutex<AbortState>>`.
/// There are more efficient implementations than a single shared lock,
/// but we're prioritizing readability and correctness at-a-glance over
/// raw performance.
enum AbortState {
    /// The controller has not yet been aborted.
    Armed {
        /// The ID to reserve for the next [`AbortSignalId`].
        next_id: NonZeroUsize,

        /// A map of reserved signal IDs to waiting tasks
        /// that should be woken up on abort.
        wakers: BTreeMap<NonZeroUsize, Waker>,
    },

    /// The controller has been aborted.
    Aborted,
}

#[derive(thiserror::Error, Debug)]
#[error("operation aborted")]
pub struct AbortError;
