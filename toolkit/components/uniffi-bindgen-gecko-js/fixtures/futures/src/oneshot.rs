/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Simple oneshot channel implementation.
//!
//! In practice, we would probably use the `oneshot` crate for this, but it's not worth bringing in
//! that dependency for this test fixture.

use std::{
    future::Future,
    pin::Pin,
    sync::{Arc, Mutex},
    task::{Context, Poll, Waker},
};

struct Channel<T> {
    value: Option<T>,
    waker: Option<Waker>,
}

pub struct Sender<T>(Arc<Mutex<Channel<T>>>);
pub struct Receiver<T>(Arc<Mutex<Channel<T>>>);

pub fn channel<T>() -> (Sender<T>, Receiver<T>) {
    let channel = Arc::new(Mutex::new(Channel {
        value: None,
        waker: None,
    }));
    (Sender(channel.clone()), Receiver(channel))
}

impl<T> Sender<T> {
    pub fn send(self, value: T) {
        let mut channel = self.0.lock().unwrap();
        channel.value = Some(value);
        if let Some(waker) = channel.waker.take() {
            waker.wake();
        }
    }

    // Wake all receivers, without sending a value.
    //
    // This causes them to poll again and receive another `Poll::Pending` result
    pub fn wake(&self) {
        let mut channel = self.0.lock().unwrap();
        if let Some(waker) = channel.waker.take() {
            waker.wake();
        }
    }
}

impl<T> Future for Receiver<T> {
    type Output = T;

    fn poll(self: Pin<&mut Self>, context: &mut Context) -> Poll<T> {
        let mut channel = self.0.lock().unwrap();
        match channel.value.take() {
            Some(v) => Poll::Ready(v),
            None => {
                channel.waker = Some(context.waker().clone());
                Poll::Pending
            }
        }
    }
}
