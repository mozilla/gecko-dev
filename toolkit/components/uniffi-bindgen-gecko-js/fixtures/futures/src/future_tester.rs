/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Test basic interop between Rust futures and JS async code
//!
//! [FutureTester::make_future] returns a future and the other methods manipulate it manually.

use std::sync::Mutex;

use crate::oneshot;

#[derive(uniffi::Object, Default)]
pub struct FutureTester {
    inner: Mutex<Vec<oneshot::Sender<u8>>>,
}

#[uniffi::export]
impl FutureTester {
    #[uniffi::constructor]
    pub fn init() -> Self {
        Self::default()
    }

    /// Make a new future that can be manipulated with the other FutureTester methods
    async fn make_future(&self) -> u8 {
        let (tx, rx) = oneshot::channel();
        self.inner.lock().unwrap().push(tx);
        rx.await
    }

    /// Store a value in all futures created via `make_future()`, then wake up any wakers.
    /// This will cause the C++ code to poll the future and get a `Ready` result.
    ///
    /// Returns the number of futures completed
    fn complete_futures(&self, value: u8) -> u32 {
        let mut count = 0;
        for sender in self.inner.lock().unwrap().drain(..) {
            sender.send(value);
            count += 1;
        }
        count
    }

    /// Wake up the waker for all futures created via `make_future()`.  This will cause the C++
    /// code to poll the future, but it will get another `Pending` result
    fn wake_futures(&self) {
        for sender in self.inner.lock().unwrap().iter() {
            sender.wake()
        }
    }
}
