/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use crate::Pid;

use nix::{
    fcntl::{
        fcntl,
        FcntlArg::{F_GETFL, F_SETFD, F_SETFL},
        FdFlag, OFlag,
    },
    sys::socket::{socket, socketpair, AddressFamily, SockFlag, SockType, UnixAddr},
    Result,
};
use std::{
    env,
    os::fd::{AsRawFd, BorrowedFd, OwnedFd},
};

pub type ProcessHandle = ();

pub(crate) fn unix_socket() -> Result<OwnedFd> {
    socket(
        AddressFamily::Unix,
        SockType::SeqPacket,
        SockFlag::empty(),
        None,
    )
}

pub(crate) fn unix_socketpair() -> Result<(OwnedFd, OwnedFd)> {
    socketpair(
        AddressFamily::Unix,
        SockType::SeqPacket,
        None,
        SockFlag::empty(),
    )
}

pub(crate) fn set_socket_default_flags(socket: BorrowedFd) -> Result<()> {
    // All our sockets are in non-blocking mode.
    let fd = socket.as_raw_fd();
    let flags = OFlag::from_bits_retain(fcntl(fd, F_GETFL)?);
    fcntl(fd, F_SETFL(flags.union(OFlag::O_NONBLOCK))).map(|_res| ())
}

pub(crate) fn set_socket_cloexec(socket: BorrowedFd) -> Result<()> {
    fcntl(socket.as_raw_fd(), F_SETFD(FdFlag::FD_CLOEXEC)).map(|_res| ())
}

pub fn server_addr(pid: Pid) -> Result<UnixAddr> {
    let server_name = if let Ok(snap_instance_name) = env::var("SNAP_INSTANCE_NAME") {
        format!("snap.{snap_instance_name:}.gecko-crash-helper-pipe.{pid:}")
    } else {
        format!("gecko-crash-helper-pipe.{pid:}")
    };
    UnixAddr::new_abstract(server_name.as_bytes())
}
