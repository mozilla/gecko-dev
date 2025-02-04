/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use crate::{ignore_eintr, AncillaryData, Pid};

use nix::{
    cmsg_space,
    errno::Errno,
    sys::socket::{
        getsockopt, recvmsg, sendmsg, sockopt::PeerCredentials, ControlMessage,
        ControlMessageOwned, MsgFlags, SockFlag, SockType, UnixAddr,
    },
    Result,
};
use std::{
    io::{IoSlice, IoSliceMut},
    os::fd::{BorrowedFd, OwnedFd, RawFd},
};

pub(crate) const SOCKET_TYPE: SockType = SockType::SeqPacket;

pub(crate) const SOCKET_FLAGS: SockFlag = SockFlag::SOCK_NONBLOCK.union(SockFlag::SOCK_CLOEXEC);

pub(crate) fn set_socket_flags(_socket: &OwnedFd) -> Result<()> {
    // Nothing to do here
    Ok(())
}

pub(crate) fn server_addr(pid: Pid) -> Result<UnixAddr> {
    let server_name = format!("gecko-crash-helper-pipe.{pid:}");
    UnixAddr::new_abstract(server_name.as_bytes())
}

// Return the pid of the process connected to this socket.
pub(crate) fn connected_process_pid(socket: BorrowedFd) -> Result<Pid> {
    let pid = getsockopt(&socket, PeerCredentials)?.pid();

    Ok(pid)
}

pub(crate) fn send_nonblock(socket: RawFd, buff: &[u8], fd: Option<AncillaryData>) -> Result<()> {
    let iov = [IoSlice::new(buff)];
    let scm_fds: Vec<i32> = fd.map_or(vec![], |fd| vec![fd]);
    let scm = ControlMessage::ScmRights(&scm_fds);

    let res = ignore_eintr!(sendmsg::<()>(socket, &iov, &[scm], MsgFlags::empty(), None));

    match res {
        Ok(bytes_sent) => {
            if bytes_sent == buff.len() {
                Ok(())
            } else {
                // TODO: This should never happen but we might want to put a
                // better error message here.
                Err(Errno::EMSGSIZE)
            }
        }
        Err(code) => Err(code),
    }
}

pub(crate) fn recv_nonblock(
    socket: RawFd,
    expected_size: usize,
) -> Result<(Vec<u8>, Option<AncillaryData>)> {
    let mut buff: Vec<u8> = vec![0; expected_size];
    let mut cmsg_buffer = cmsg_space!(RawFd);
    let mut iov = [IoSliceMut::new(&mut buff)];

    let res = ignore_eintr!(recvmsg::<()>(
        socket,
        &mut iov,
        Some(&mut cmsg_buffer),
        MsgFlags::empty(),
    ))?;

    let fd = if let Some(cmsg) = res.cmsgs()?.next() {
        if let ControlMessageOwned::ScmRights(fds) = cmsg {
            fds.first().copied()
        } else {
            return Err(Errno::EBADMSG);
        }
    } else {
        None
    };

    if res.bytes != expected_size {
        // TODO: This should only ever happen if the other side has gone rogue,
        // we need a better error message here.
        return Err(Errno::EBADMSG);
    }

    Ok((buff, fd))
}
