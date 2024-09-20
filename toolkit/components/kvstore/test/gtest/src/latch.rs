/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

use std::{
    sync::{Condvar, Mutex},
    time::Duration,
};

/// A countdown latch is like an [`std::sync::Barrier`] that lets M threads
/// wait on N operations to complete. Unlike a barrier, a countdown latch is
/// acyclic: once the latch has counted down to zero, it can't be reset.
pub struct CountDownLatch {
    count: Mutex<usize>,
    cvar: Condvar,
}

impl CountDownLatch {
    /// Creates a new latch that waits for N operations to complete.
    pub const fn new(count: usize) -> Self {
        Self {
            count: Mutex::new(count),
            cvar: Condvar::new(),
        }
    }

    /// Decrements the count of the latch, waking up any waiting threads
    /// if the count reaches zero. Returns without effect if the count is
    /// already zero.
    pub fn count_down(&self) {
        let mut count = self.count.lock().unwrap();
        if *count > 0 {
            *count -= 1;
            if *count == 0 {
                self.cvar.notify_all();
            }
        }
    }

    /// Waits for either the count of the latch to reach zero, or the `timeout`
    /// to elapse; whichever happens first. Returns `true` if the count is
    /// already zero, or reaches zero within the `timeout`.
    pub fn wait_timeout(&self, timeout: Duration) -> bool {
        let mut count = self.count.lock().unwrap();
        while *count > 0 {
            let (guard, result) = self.cvar.wait_timeout(count, timeout).unwrap();
            if result.timed_out() {
                return false;
            }
            count = guard;
        }
        true
    }
}
