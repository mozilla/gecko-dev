/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use crate::Pid;

use nix::{
    errno::Errno,
    fcntl::{
        fcntl,
        FcntlArg::{F_GETFL, F_SETFD, F_SETFL},
        FdFlag, OFlag,
    },
    libc::{setsockopt, SOL_SOCKET, SO_NOSIGPIPE},
    sys::socket::{socket, socketpair, AddressFamily, SockFlag, SockType, UnixAddr},
    Result,
};
use std::{
    mem::size_of,
    os::fd::{AsRawFd, BorrowedFd, OwnedFd},
    path::PathBuf,
    str::FromStr,
};

pub type ProcessHandle = ();

pub(crate) fn unix_socket() -> Result<OwnedFd> {
    socket(
        AddressFamily::Unix,
        SockType::Stream,
        SockFlag::empty(),
        None,
    )
}

pub(crate) fn unix_socketpair() -> Result<(OwnedFd, OwnedFd)> {
    socketpair(
        AddressFamily::Unix,
        SockType::Stream,
        None,
        SockFlag::empty(),
    )
}

pub(crate) fn set_socket_default_flags(socket: BorrowedFd) -> Result<()> {
    // All our sockets are in non-blocking mode.
    let fd = socket.as_raw_fd();
    let flags = OFlag::from_bits_retain(fcntl(fd, F_GETFL)?);
    fcntl(fd, F_SETFL(flags.union(OFlag::O_NONBLOCK)))?;

    // TODO: nix doesn't have a safe wrapper for SO_NOSIGPIPE yet, but we need
    // to set this flag because we're using stream sockets unlike Linux, where
    // we use sequential packets that don't raise SIGPIPE.
    let res = unsafe {
        setsockopt(
            fd,
            SOL_SOCKET,
            SO_NOSIGPIPE,
            (&1 as *const i32).cast(),
            size_of::<i32>() as _,
        )
    };

    if res < 0 {
        return Err(Errno::last());
    }

    Ok(())
}

pub(crate) fn set_socket_cloexec(socket: BorrowedFd) -> Result<()> {
    fcntl(socket.as_raw_fd(), F_SETFD(FdFlag::FD_CLOEXEC)).map(|_res| ())
}

pub fn server_addr(pid: Pid) -> Result<UnixAddr> {
    // macOS doesn't seem to support abstract paths as addresses for Unix
    // protocol sockets, so this needs to be the path of an actual file.
    let server_name = format!("/tmp/gecko-crash-helper-pipe.{pid:}");
    let server_path = PathBuf::from_str(&server_name).unwrap();
    UnixAddr::new(&server_path)
}
