/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#[cfg(any(target_os = "android", target_os = "linux"))]
use crate::platform::linux::{
    server_addr, set_socket_cloexec, set_socket_default_flags, unix_socket,
};
#[cfg(target_os = "macos")]
use crate::platform::macos::{
    server_addr, set_socket_cloexec, set_socket_default_flags, unix_socket,
};
use crate::{ignore_eintr, Pid, ProcessHandle, IO_TIMEOUT};

use nix::{
    cmsg_space,
    errno::Errno,
    poll::{poll, PollFd, PollFlags, PollTimeout},
    sys::socket::{connect, recvmsg, sendmsg, ControlMessage, ControlMessageOwned, MsgFlags},
};
use std::{
    ffi::{CStr, CString},
    io::{IoSlice, IoSliceMut},
    os::fd::{AsFd, AsRawFd, BorrowedFd, FromRawFd, IntoRawFd, OwnedFd, RawFd},
    str::FromStr,
};

use crate::{
    errors::IPCError,
    messages::{self, Message},
};

pub type AncillaryData = RawFd;

// This must match `kInvalidHandle` in `mfbt/UniquePtrExt.h`
pub const INVALID_ANCILLARY_DATA: AncillaryData = -1;

pub struct IPCConnector {
    socket: OwnedFd,
}

impl IPCConnector {
    /// Create a new connector from an already connected socket. The
    /// `FD_CLOEXEC` flag will be set on the underlying socket and thus it
    /// will not be possible to inerhit this connector in a child process.
    pub fn from_fd(socket: OwnedFd) -> Result<IPCConnector, IPCError> {
        let connector = IPCConnector::from_fd_inheritable(socket)?;
        set_socket_cloexec(connector.socket.as_fd()).map_err(IPCError::System)?;
        Ok(connector)
    }

    /// Create a new connector from an already connected socket. The
    /// `FD_CLOEXEC` flag will not be set on the underlying socket and thus it
    /// will be possible to inherit this connector in a child process.
    pub fn from_fd_inheritable(socket: OwnedFd) -> Result<IPCConnector, IPCError> {
        set_socket_default_flags(socket.as_fd()).map_err(IPCError::System)?;
        Ok(IPCConnector { socket })
    }

    pub fn from_ancillary(ancillary_data: AncillaryData) -> Result<IPCConnector, IPCError> {
        IPCConnector::from_fd(unsafe { OwnedFd::from_raw_fd(ancillary_data) })
    }

    /// Create a new connector by connecting it to the process specified by
    /// `pid`.  The `FD_CLOEXEC` flag will be set on the underlying socket and
    /// thus it will not be possible to inerhit this connector in a child
    /// process.
    pub fn connect(pid: Pid) -> Result<IPCConnector, IPCError> {
        let socket = unix_socket().map_err(IPCError::ConnectionFailure)?;
        set_socket_default_flags(socket.as_fd()).map_err(IPCError::ConnectionFailure)?;
        set_socket_cloexec(socket.as_fd()).map_err(IPCError::ConnectionFailure)?;

        let server_addr = server_addr(pid).map_err(IPCError::ConnectionFailure)?;

        loop {
            let timeout = PollTimeout::from(IO_TIMEOUT);
            let pollfd = PollFd::new(socket.as_fd(), PollFlags::POLLOUT);
            let res = ignore_eintr!(poll(&mut [pollfd], timeout));
            match res {
                Err(e) => return Err(IPCError::ConnectionFailure(e)),
                Ok(_res @ 0) => return Err(IPCError::ConnectionFailure(Errno::ETIMEDOUT)),
                Ok(_) => {}
            }

            let res = ignore_eintr!(connect(socket.as_raw_fd(), &server_addr));
            match res {
                Ok(_) => break,
                Err(_e @ Errno::EAGAIN) => continue, // Retry, the helper might not be ready yet
                Err(e) => return Err(IPCError::ConnectionFailure(e)),
            }
        }

        Ok(IPCConnector { socket })
    }

    /// Serialize this connector into a string that can be passed on the
    /// command-line to a child process. This only works for newly
    /// created connectors because they are explicitly created as inheritable.
    pub fn serialize(&self) -> CString {
        CString::new(self.socket.as_raw_fd().to_string()).unwrap()
    }

    /// Deserialize a connector from an argument passed on the command-line.
    pub fn deserialize(string: &CStr) -> Result<IPCConnector, IPCError> {
        let string = string.to_str().map_err(|_e| IPCError::ParseError)?;
        let fd = RawFd::from_str(string).map_err(|_e| IPCError::ParseError)?;
        // SAFETY: This is a file descriptor we passed in ourselves.
        let socket = unsafe { OwnedFd::from_raw_fd(fd) };
        Ok(IPCConnector { socket })
    }

    fn raw_fd(&self) -> RawFd {
        self.socket.as_raw_fd()
    }

    pub fn into_ancillary(
        self,
        _dst_process: &Option<ProcessHandle>,
    ) -> Result<AncillaryData, IPCError> {
        Ok(self.socket.into_raw_fd())
    }

    pub fn as_raw_ref(&self) -> BorrowedFd {
        self.socket.as_fd()
    }

    pub fn poll(&self, flags: PollFlags) -> Result<(), Errno> {
        let timeout = PollTimeout::from(IO_TIMEOUT);
        let pollfd = PollFd::new(self.socket.as_fd(), flags);
        let res = ignore_eintr!(poll(&mut [pollfd], timeout));
        match res {
            Err(e) => Err(e),
            Ok(_res @ 0) => Err(Errno::EAGAIN),
            Ok(_) => Ok(()),
        }
    }

    pub fn send_message(&self, message: &dyn Message) -> Result<(), IPCError> {
        self.send(&message.header(), None)
            .map_err(IPCError::TransmissionFailure)?;
        self.send(&message.payload(), message.ancillary_payload())
            .map_err(IPCError::TransmissionFailure)
    }

    pub fn recv_reply<T>(&self) -> Result<T, IPCError>
    where
        T: Message,
    {
        let header = self.recv_header()?;

        if header.kind != T::kind() {
            return Err(IPCError::ReceptionFailure(Errno::EBADMSG));
        }

        let (data, _) = self.recv(header.size).map_err(IPCError::ReceptionFailure)?;
        T::decode(&data, None).map_err(IPCError::from)
    }

    fn send_nonblock(&self, buff: &[u8], fd: Option<AncillaryData>) -> Result<(), Errno> {
        let iov = [IoSlice::new(buff)];
        let scm_fds: Vec<i32> = fd.map_or(vec![], |fd| vec![fd]);
        let scm = ControlMessage::ScmRights(&scm_fds);

        let res = ignore_eintr!(sendmsg::<()>(
            self.raw_fd(),
            &iov,
            &[scm],
            MsgFlags::empty(),
            None
        ));

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

    fn send(&self, buff: &[u8], fd: Option<AncillaryData>) -> Result<(), Errno> {
        let res = self.send_nonblock(buff, fd);
        match res {
            Err(_code @ Errno::EAGAIN) => {
                // If the socket was not ready to send data wait for it to
                // become unblocked then retry sending just once.
                self.poll(PollFlags::POLLOUT)?;
                self.send_nonblock(buff, fd)
            }
            _ => res,
        }
    }

    pub fn recv_header(&self) -> Result<messages::Header, IPCError> {
        let (header, _) = self
            .recv(messages::HEADER_SIZE)
            .map_err(IPCError::ReceptionFailure)?;
        messages::Header::decode(&header).map_err(IPCError::BadMessage)
    }

    fn recv_nonblock(
        &self,
        expected_size: usize,
    ) -> Result<(Vec<u8>, Option<AncillaryData>), Errno> {
        let mut buff: Vec<u8> = vec![0; expected_size];
        let mut cmsg_buffer = cmsg_space!(RawFd);
        let mut iov = [IoSliceMut::new(&mut buff)];

        let res = ignore_eintr!(recvmsg::<()>(
            self.raw_fd(),
            &mut iov,
            Some(&mut cmsg_buffer),
            MsgFlags::empty(),
        ));

        // I know this looks weird, but bear with me. On macOS 10.15 every
        // other recvmsg() call returns ENOMEM for no apparent reason. But then
        // if works *fine* if you call it again with the same parameters. This
        // makes no sense but OK, I stopped trying to understand macOS a long
        // time ago. on macOS 15+ this isn't needed but since I can't test it
        // everywhere and it doesn't hurt anyway, every version gets the same
        // workaround.
        let res = match res {
            #[cfg(target_os = "macos")]
            Err(_code @ Errno::ENOMEM) => {
              ignore_eintr!(recvmsg::<()>(
                  self.raw_fd(),
                  &mut iov,
                  Some(&mut cmsg_buffer),
                  MsgFlags::empty(),
              ))?
            },
            Err(e) => return Err(e),
            Ok(val) => val,
        };

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

    pub fn recv(&self, expected_size: usize) -> Result<(Vec<u8>, Option<AncillaryData>), Errno> {
        let res = self.recv_nonblock(expected_size);
        match res {
            Err(_code @ Errno::EAGAIN) => {
                // If the socket was not ready to receive data wait for it to
                // become unblocked then retry receiving just once.
                self.poll(PollFlags::POLLIN)?;
                self.recv_nonblock(expected_size)
            }
            _ => res,
        }
    }
}
