// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{
    io::{Error, Read, Result, Write},
    num::TryFromIntError,
    os::fd::{AsRawFd, FromRawFd as _, OwnedFd},
    sync::atomic::Ordering,
};

use libc::{fsync, read, socket, write, SOCK_RAW};

use crate::unlikely_err;

#[cfg(any(target_os = "linux", target_os = "android"))]
type AtomicRouteSocketSeq = std::sync::atomic::AtomicU32;
#[cfg(any(target_os = "linux", target_os = "android"))]
type RouteSocketSeq = u32;

#[cfg(not(any(target_os = "linux", target_os = "android")))]
type AtomicRouteSocketSeq = std::sync::atomic::AtomicI32;
#[cfg(not(any(target_os = "linux", target_os = "android")))]
type RouteSocketSeq = i32;

static SEQ: AtomicRouteSocketSeq = AtomicRouteSocketSeq::new(0);

pub struct RouteSocket(OwnedFd);

impl RouteSocket {
    pub fn new(domain: libc::c_int, protocol: libc::c_int) -> Result<Self> {
        let fd = unsafe { socket(domain, SOCK_RAW, protocol) };
        if fd == -1 {
            return Err(Error::last_os_error());
        }
        Ok(Self(unsafe { OwnedFd::from_raw_fd(fd) }))
    }

    pub fn new_seq() -> RouteSocketSeq {
        SEQ.fetch_add(1, Ordering::Relaxed)
    }
}

impl AsRawFd for RouteSocket {
    fn as_raw_fd(&self) -> i32 {
        self.0.as_raw_fd()
    }
}

fn check_result(res: isize) -> Result<usize> {
    if res == -1 {
        Err(Error::last_os_error())
    } else {
        Ok(res
            .try_into()
            .map_err(|e: TryFromIntError| unlikely_err(e.to_string()))?)
    }
}

impl Write for RouteSocket {
    fn write(&mut self, buf: &[u8]) -> Result<usize> {
        let res = unsafe { write(self.as_raw_fd(), buf.as_ptr().cast(), buf.len()) };
        check_result(res)
    }

    fn flush(&mut self) -> Result<()> {
        let res = unsafe { fsync(self.as_raw_fd()) };
        check_result(res as isize).and(Ok(()))
    }
}

impl Read for RouteSocket {
    fn read(&mut self, buf: &mut [u8]) -> Result<usize> {
        // If we've written a well-formed message into the kernel via `write`, we should be able to
        // read a well-formed message back out, and not block.
        let res = unsafe { read(self.as_raw_fd(), buf.as_mut_ptr().cast(), buf.len()) };
        check_result(res)
    }
}
