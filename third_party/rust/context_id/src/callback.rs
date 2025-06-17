/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[uniffi::export(callback_interface)]
pub trait ContextIdCallback: Sync + Send {
    fn persist(&self, context_id: String, creation_date: i64);
    fn rotated(&self, old_context_id: String);
}

pub struct DefaultContextIdCallback;
impl ContextIdCallback for DefaultContextIdCallback {
    fn persist(&self, _context_id: String, _creation_date: i64) {}
    fn rotated(&self, _old_context_id: String) {}
}

#[cfg(test)]
pub mod utils {
    use super::ContextIdCallback;
    use std::sync::{Arc, Mutex};
    /// A spy callback for tests that counts persist/rotate calls,
    /// captures the last rotated ID, and records the last creation timestamp.
    pub struct SpyCallback {
        pub callback: Box<dyn ContextIdCallback + Send + Sync>,
        pub persist_called: Arc<Mutex<u32>>,
        pub rotated_called: Arc<Mutex<u32>>,
        pub last_rotated_id: Arc<Mutex<Option<String>>>,
        pub last_persist_ts: Arc<Mutex<Option<i64>>>,
    }

    impl SpyCallback {
        pub fn new() -> Self {
            let persist_called = Arc::new(Mutex::new(0));
            let rotated_called = Arc::new(Mutex::new(0));
            let last_rotated_id = Arc::new(Mutex::new(None));
            let last_persist_ts = Arc::new(Mutex::new(None));

            let inner = SpyCallbackInner {
                persist_called: Arc::clone(&persist_called),
                rotated_called: Arc::clone(&rotated_called),
                last_rotated_id: Arc::clone(&last_rotated_id),
                last_persist_ts: Arc::clone(&last_persist_ts),
            };

            SpyCallback {
                callback: Box::new(inner),
                persist_called,
                rotated_called,
                last_rotated_id,
                last_persist_ts,
            }
        }
    }

    struct SpyCallbackInner {
        persist_called: Arc<Mutex<u32>>,
        rotated_called: Arc<Mutex<u32>>,
        last_rotated_id: Arc<Mutex<Option<String>>>,
        last_persist_ts: Arc<Mutex<Option<i64>>>,
    }

    impl ContextIdCallback for SpyCallbackInner {
        fn persist(&self, _ctx: String, ts: i64) {
            *self.persist_called.lock().unwrap() += 1;
            *self.last_persist_ts.lock().unwrap() = Some(ts);
        }
        fn rotated(&self, old_ctx: String) {
            *self.rotated_called.lock().unwrap() += 1;
            *self.last_rotated_id.lock().unwrap() = Some(old_ctx);
        }
    }
}
