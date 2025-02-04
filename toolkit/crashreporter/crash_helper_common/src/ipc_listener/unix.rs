/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#[cfg(any(target_os = "android", target_os = "linux"))]
use crate::platform::linux::{server_addr, set_socket_flags, SOCKET_FLAGS, SOCKET_TYPE};
#[cfg(target_os = "macos")]
use crate::platform::macos::{server_addr, set_socket_flags, SOCKET_FLAGS, SOCKET_TYPE};
use crate::{errors::IPCError, IPCConnector, Pid};

use nix::sys::socket::{accept, bind, listen, socket, AddressFamily, Backlog};
use std::os::fd::{AsFd, AsRawFd, BorrowedFd, FromRawFd, OwnedFd};

pub struct IPCListener {
    socket: OwnedFd,
}

impl IPCListener {
    pub fn new(pid: Pid) -> Result<IPCListener, IPCError> {
        let socket = socket(AddressFamily::Unix, SOCKET_TYPE, SOCKET_FLAGS, None)
            .map_err(IPCError::System)?;
        set_socket_flags(&socket).map_err(IPCError::System)?;

        let server_addr = server_addr(pid).map_err(IPCError::System)?;

        bind(socket.as_fd().as_raw_fd(), &server_addr).map_err(IPCError::System)?;

        Ok(IPCListener { socket })
    }

    pub fn as_raw_ref(&self) -> BorrowedFd {
        self.socket.as_fd()
    }

    pub fn listen(&mut self) -> Result<(), IPCError> {
        listen(&self.socket, Backlog::new(1).unwrap()).map_err(IPCError::ListenFailed)
    }

    pub fn accept(&self) -> Result<IPCConnector, IPCError> {
        let socket = accept(self.socket.as_fd().as_raw_fd()).map_err(IPCError::AcceptFailed)?;
        // SAFETY: `socket` is guaranteed to be valid at this point.
        IPCConnector::new(unsafe { OwnedFd::from_raw_fd(socket) })
    }
}
