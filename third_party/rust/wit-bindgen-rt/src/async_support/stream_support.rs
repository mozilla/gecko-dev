extern crate std;

use {
    super::Handle,
    futures::{
        channel::oneshot,
        future::{self, FutureExt},
        sink::Sink,
        stream::Stream,
    },
    std::{
        boxed::Box,
        collections::hash_map::Entry,
        convert::Infallible,
        fmt,
        future::Future,
        iter,
        mem::{self, MaybeUninit},
        pin::Pin,
        sync::atomic::{AtomicU32, Ordering::Relaxed},
        task::{Context, Poll},
        vec::Vec,
    },
};

fn ceiling(x: usize, y: usize) -> usize {
    (x / y) + if x % y == 0 { 0 } else { 1 }
}

#[doc(hidden)]
pub struct StreamVtable<T> {
    pub write: fn(future: u32, values: &[T]) -> Pin<Box<dyn Future<Output = usize> + '_>>,
    pub read: fn(
        future: u32,
        values: &mut [MaybeUninit<T>],
    ) -> Pin<Box<dyn Future<Output = Option<usize>> + '_>>,
    pub cancel_write: fn(future: u32),
    pub cancel_read: fn(future: u32),
    pub close_writable: fn(future: u32),
    pub close_readable: fn(future: u32),
}

struct CancelWriteOnDrop<T: 'static> {
    handle: Option<u32>,
    vtable: &'static StreamVtable<T>,
}

impl<T> Drop for CancelWriteOnDrop<T> {
    fn drop(&mut self) {
        if let Some(handle) = self.handle.take() {
            super::with_entry(handle, |entry| match entry {
                Entry::Vacant(_) => unreachable!(),
                Entry::Occupied(mut entry) => match entry.get() {
                    Handle::LocalOpen
                    | Handle::LocalWaiting(_)
                    | Handle::Read
                    | Handle::LocalClosed => unreachable!(),
                    Handle::LocalReady(..) => {
                        entry.insert(Handle::LocalOpen);
                    }
                    Handle::Write => (self.vtable.cancel_write)(handle),
                },
            });
        }
    }
}

/// Represents the writable end of a Component Model `stream`.
pub struct StreamWriter<T: 'static> {
    handle: u32,
    future: Option<Pin<Box<dyn Future<Output = ()> + 'static>>>,
    vtable: &'static StreamVtable<T>,
}

impl<T> StreamWriter<T> {
    #[doc(hidden)]
    pub fn new(handle: u32, vtable: &'static StreamVtable<T>) -> Self {
        Self {
            handle,
            future: None,
            vtable,
        }
    }

    /// Cancel the current pending write operation.
    ///
    /// This will panic if no such operation is pending.
    pub fn cancel(&mut self) {
        assert!(self.future.is_some());
        self.future = None;
    }
}

impl<T> fmt::Debug for StreamWriter<T> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("StreamWriter")
            .field("handle", &self.handle)
            .finish()
    }
}

impl<T> Sink<Vec<T>> for StreamWriter<T> {
    type Error = Infallible;

    fn poll_ready(self: Pin<&mut Self>, cx: &mut Context) -> Poll<Result<(), Self::Error>> {
        let me = self.get_mut();

        if let Some(future) = &mut me.future {
            match future.as_mut().poll(cx) {
                Poll::Ready(_) => {
                    me.future = None;
                    Poll::Ready(Ok(()))
                }
                Poll::Pending => Poll::Pending,
            }
        } else {
            Poll::Ready(Ok(()))
        }
    }

    fn start_send(self: Pin<&mut Self>, item: Vec<T>) -> Result<(), Self::Error> {
        assert!(self.future.is_none());
        super::with_entry(self.handle, |entry| match entry {
            Entry::Vacant(_) => unreachable!(),
            Entry::Occupied(mut entry) => match entry.get() {
                Handle::LocalOpen => {
                    let handle = self.handle;
                    let mut item = Some(item);
                    let mut cancel_on_drop = Some(CancelWriteOnDrop::<T> {
                        handle: Some(handle),
                        vtable: self.vtable,
                    });
                    self.get_mut().future = Some(Box::pin(future::poll_fn(move |cx| {
                        super::with_entry(handle, |entry| match entry {
                            Entry::Vacant(_) => unreachable!(),
                            Entry::Occupied(mut entry) => match entry.get() {
                                Handle::LocalOpen => {
                                    if let Some(item) = item.take() {
                                        entry.insert(Handle::LocalReady(
                                            Box::new(item),
                                            cx.waker().clone(),
                                        ));
                                        Poll::Pending
                                    } else {
                                        cancel_on_drop.take().unwrap().handle = None;
                                        Poll::Ready(())
                                    }
                                }
                                Handle::LocalReady(..) => Poll::Pending,
                                Handle::LocalClosed => {
                                    cancel_on_drop.take().unwrap().handle = None;
                                    Poll::Ready(())
                                }
                                Handle::LocalWaiting(_) | Handle::Read | Handle::Write => {
                                    unreachable!()
                                }
                            },
                        })
                    })));
                }
                Handle::LocalWaiting(_) => {
                    let Handle::LocalWaiting(tx) = entry.insert(Handle::LocalOpen) else {
                        unreachable!()
                    };
                    _ = tx.send(Box::new(item));
                }
                Handle::LocalClosed => (),
                Handle::Read | Handle::LocalReady(..) => unreachable!(),
                Handle::Write => {
                    let handle = self.handle;
                    let vtable = self.vtable;
                    let mut cancel_on_drop = CancelWriteOnDrop::<T> {
                        handle: Some(handle),
                        vtable,
                    };
                    self.get_mut().future = Some(Box::pin(async move {
                        (vtable.write)(handle, &item).await;
                        cancel_on_drop.handle = None;
                        drop(cancel_on_drop);
                    }));
                }
            },
        });
        Ok(())
    }

    fn poll_flush(self: Pin<&mut Self>, cx: &mut Context) -> Poll<Result<(), Self::Error>> {
        self.poll_ready(cx)
    }

    fn poll_close(self: Pin<&mut Self>, cx: &mut Context) -> Poll<Result<(), Self::Error>> {
        self.poll_ready(cx)
    }
}

impl<T> Drop for StreamWriter<T> {
    fn drop(&mut self) {
        self.future = None;

        super::with_entry(self.handle, |entry| match entry {
            Entry::Vacant(_) => unreachable!(),
            Entry::Occupied(mut entry) => match entry.get_mut() {
                Handle::LocalOpen | Handle::LocalWaiting(_) | Handle::LocalReady(..) => {
                    entry.insert(Handle::LocalClosed);
                }
                Handle::Read => unreachable!(),
                Handle::Write | Handle::LocalClosed => {
                    entry.remove();
                    (self.vtable.close_writable)(self.handle);
                }
            },
        });
    }
}

struct CancelReadOnDrop<T: 'static> {
    handle: Option<u32>,
    vtable: &'static StreamVtable<T>,
}

impl<T> Drop for CancelReadOnDrop<T> {
    fn drop(&mut self) {
        if let Some(handle) = self.handle.take() {
            super::with_entry(handle, |entry| match entry {
                Entry::Vacant(_) => unreachable!(),
                Entry::Occupied(mut entry) => match entry.get() {
                    Handle::LocalOpen
                    | Handle::LocalReady(..)
                    | Handle::Write
                    | Handle::LocalClosed => unreachable!(),
                    Handle::LocalWaiting(_) => {
                        entry.insert(Handle::LocalOpen);
                    }
                    Handle::Read => (self.vtable.cancel_read)(handle),
                },
            });
        }
    }
}

/// Represents the readable end of a Component Model `stream`.
pub struct StreamReader<T: 'static> {
    handle: AtomicU32,
    future: Option<Pin<Box<dyn Future<Output = Option<Vec<T>>> + 'static>>>,
    vtable: &'static StreamVtable<T>,
}

impl<T> StreamReader<T> {
    /// Cancel the current pending read operation.
    ///
    /// This will panic if no such operation is pending.
    pub fn cancel(&mut self) {
        assert!(self.future.is_some());
        self.future = None;
    }
}

impl<T> fmt::Debug for StreamReader<T> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("StreamReader")
            .field("handle", &self.handle)
            .finish()
    }
}

impl<T> StreamReader<T> {
    #[doc(hidden)]
    pub fn new(handle: u32, vtable: &'static StreamVtable<T>) -> Self {
        Self {
            handle: AtomicU32::new(handle),
            future: None,
            vtable,
        }
    }

    #[doc(hidden)]
    pub fn from_handle_and_vtable(handle: u32, vtable: &'static StreamVtable<T>) -> Self {
        super::with_entry(handle, |entry| match entry {
            Entry::Vacant(entry) => {
                entry.insert(Handle::Read);
            }
            Entry::Occupied(mut entry) => match entry.get() {
                Handle::Write => {
                    entry.insert(Handle::LocalOpen);
                }
                Handle::Read
                | Handle::LocalOpen
                | Handle::LocalReady(..)
                | Handle::LocalWaiting(_)
                | Handle::LocalClosed => {
                    unreachable!()
                }
            },
        });

        Self {
            handle: AtomicU32::new(handle),
            future: None,
            vtable,
        }
    }

    #[doc(hidden)]
    pub fn take_handle(&self) -> u32 {
        let handle = self.handle.swap(u32::MAX, Relaxed);
        super::with_entry(handle, |entry| match entry {
            Entry::Vacant(_) => unreachable!(),
            Entry::Occupied(mut entry) => match entry.get() {
                Handle::LocalOpen => {
                    entry.insert(Handle::Write);
                }
                Handle::Read | Handle::LocalClosed => {
                    entry.remove();
                }
                Handle::LocalReady(..) | Handle::LocalWaiting(_) | Handle::Write => unreachable!(),
            },
        });

        handle
    }
}

impl<T> Stream for StreamReader<T> {
    type Item = Vec<T>;

    fn poll_next(self: Pin<&mut Self>, cx: &mut Context) -> Poll<Option<Self::Item>> {
        let me = self.get_mut();

        if me.future.is_none() {
            me.future = Some(super::with_entry(
                me.handle.load(Relaxed),
                |entry| match entry {
                    Entry::Vacant(_) => unreachable!(),
                    Entry::Occupied(mut entry) => match entry.get() {
                        Handle::Write | Handle::LocalWaiting(_) => unreachable!(),
                        Handle::Read => {
                            let handle = me.handle.load(Relaxed);
                            let vtable = me.vtable;
                            let mut cancel_on_drop = CancelReadOnDrop::<T> {
                                handle: Some(handle),
                                vtable,
                            };
                            Box::pin(async move {
                                let mut buffer = iter::repeat_with(MaybeUninit::uninit)
                                    .take(ceiling(64 * 1024, mem::size_of::<T>().max(1)))
                                    .collect::<Vec<_>>();

                                let result =
                                    if let Some(count) = (vtable.read)(handle, &mut buffer).await {
                                        buffer.truncate(count);
                                        Some(unsafe {
                                            mem::transmute::<Vec<MaybeUninit<T>>, Vec<T>>(buffer)
                                        })
                                    } else {
                                        None
                                    };
                                cancel_on_drop.handle = None;
                                drop(cancel_on_drop);
                                result
                            }) as Pin<Box<dyn Future<Output = _>>>
                        }
                        Handle::LocalOpen => {
                            let (tx, rx) = oneshot::channel();
                            entry.insert(Handle::LocalWaiting(tx));
                            let mut cancel_on_drop = CancelReadOnDrop::<T> {
                                handle: Some(me.handle.load(Relaxed)),
                                vtable: me.vtable,
                            };
                            Box::pin(async move {
                                let result =
                                    rx.map(|v| v.ok().map(|v| *v.downcast().unwrap())).await;
                                cancel_on_drop.handle = None;
                                drop(cancel_on_drop);
                                result
                            })
                        }
                        Handle::LocalClosed => Box::pin(future::ready(None)),
                        Handle::LocalReady(..) => {
                            let Handle::LocalReady(v, waker) = entry.insert(Handle::LocalOpen)
                            else {
                                unreachable!()
                            };
                            waker.wake();
                            Box::pin(future::ready(Some(*v.downcast().unwrap())))
                        }
                    },
                },
            ));
        }

        match me.future.as_mut().unwrap().as_mut().poll(cx) {
            Poll::Ready(v) => {
                me.future = None;
                Poll::Ready(v)
            }
            Poll::Pending => Poll::Pending,
        }
    }
}

impl<T> Drop for StreamReader<T> {
    fn drop(&mut self) {
        self.future = None;

        match self.handle.load(Relaxed) {
            u32::MAX => {}
            handle => {
                super::with_entry(handle, |entry| match entry {
                    Entry::Vacant(_) => unreachable!(),
                    Entry::Occupied(mut entry) => match entry.get_mut() {
                        Handle::LocalReady(..) => {
                            let Handle::LocalReady(_, waker) = entry.insert(Handle::LocalClosed)
                            else {
                                unreachable!()
                            };
                            waker.wake();
                        }
                        Handle::LocalOpen | Handle::LocalWaiting(_) => {
                            entry.insert(Handle::LocalClosed);
                        }
                        Handle::Read | Handle::LocalClosed => {
                            entry.remove();
                            (self.vtable.close_readable)(handle);
                        }
                        Handle::Write => unreachable!(),
                    },
                });
            }
        }
    }
}
