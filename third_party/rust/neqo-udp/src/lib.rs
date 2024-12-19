// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(clippy::missing_errors_doc)] // Functions simply delegate to tokio and quinn-udp.

use std::{
    io::{self, IoSliceMut},
    net::SocketAddr,
    slice::{self, Chunks},
};

use neqo_common::{qdebug, qtrace, Datagram, IpTos};
use quinn_udp::{EcnCodepoint, RecvMeta, Transmit, UdpSocketState};

/// Socket receive buffer size.
///
/// Allows reading multiple datagrams in a single [`Socket::recv`] call.
//
// TODO: Experiment with different values across platforms.
pub const RECV_BUF_SIZE: usize = u16::MAX as usize;

pub fn send_inner(
    state: &UdpSocketState,
    socket: quinn_udp::UdpSockRef<'_>,
    d: &Datagram,
) -> io::Result<()> {
    let transmit = Transmit {
        destination: d.destination(),
        ecn: EcnCodepoint::from_bits(Into::<u8>::into(d.tos())),
        contents: d,
        segment_size: None,
        src_ip: None,
    };

    state.try_send(socket, &transmit)?;

    qtrace!(
        "sent {} bytes from {} to {}",
        d.len(),
        d.source(),
        d.destination()
    );

    Ok(())
}

#[cfg(unix)]
use std::os::fd::AsFd as SocketRef;
#[cfg(windows)]
use std::os::windows::io::AsSocket as SocketRef;

pub fn recv_inner<'a>(
    local_address: SocketAddr,
    state: &UdpSocketState,
    socket: impl SocketRef,
    recv_buf: &'a mut [u8],
) -> Result<DatagramIter<'a>, io::Error> {
    let mut meta;

    let data = loop {
        meta = RecvMeta::default();

        state.recv(
            (&socket).into(),
            &mut [IoSliceMut::new(recv_buf)],
            slice::from_mut(&mut meta),
        )?;

        if meta.len == 0 || meta.stride == 0 {
            qdebug!(
                "ignoring datagram from {} to {} len {} stride {}",
                meta.addr,
                local_address,
                meta.len,
                meta.stride
            );
            continue;
        }

        break &recv_buf[..meta.len];
    };

    qtrace!(
        "received {} bytes from {} to {} in {} segments",
        data.len(),
        meta.addr,
        local_address,
        data.len().div_ceil(meta.stride),
    );

    Ok(DatagramIter {
        meta,
        datagrams: data.chunks(meta.stride),
        local_address,
    })
}

pub struct DatagramIter<'a> {
    meta: RecvMeta,
    datagrams: Chunks<'a, u8>,
    local_address: SocketAddr,
}

impl std::fmt::Debug for DatagramIter<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("DatagramIter")
            .field("meta", &self.meta)
            .field("local_address", &self.local_address)
            .finish()
    }
}

impl<'a> Iterator for DatagramIter<'a> {
    type Item = Datagram<&'a [u8]>;

    fn next(&mut self) -> Option<Self::Item> {
        self.datagrams.next().map(|d| {
            Datagram::from_slice(
                self.meta.addr,
                self.local_address,
                self.meta
                    .ecn
                    .map(|n| IpTos::from(n as u8))
                    .unwrap_or_default(),
                d,
            )
        })
    }
}

impl ExactSizeIterator for DatagramIter<'_> {
    fn len(&self) -> usize {
        self.datagrams.len()
    }
}

/// A wrapper around a UDP socket, sending and receiving [`Datagram`]s.
pub struct Socket<S> {
    state: UdpSocketState,
    inner: S,
}

impl<S: SocketRef> Socket<S> {
    /// Create a new [`Socket`] given a raw file descriptor managed externally.
    pub fn new(socket: S) -> Result<Self, io::Error> {
        Ok(Self {
            state: quinn_udp::UdpSocketState::new((&socket).into())?,
            inner: socket,
        })
    }

    /// Send a [`Datagram`] on the given [`Socket`].
    pub fn send(&self, d: &Datagram) -> io::Result<()> {
        send_inner(&self.state, (&self.inner).into(), d)
    }

    /// Receive a batch of [`Datagram`]s on the given [`Socket`], each
    /// set with the provided local address.
    pub fn recv<'a>(
        &self,
        local_address: SocketAddr,
        recv_buf: &'a mut [u8],
    ) -> Result<DatagramIter<'a>, io::Error> {
        recv_inner(local_address, &self.state, &self.inner, recv_buf)
    }
}

#[cfg(test)]
mod tests {
    use neqo_common::{IpTosDscp, IpTosEcn};

    use super::*;

    fn socket() -> Result<Socket<std::net::UdpSocket>, io::Error> {
        let socket = Socket::new(std::net::UdpSocket::bind("127.0.0.1:0")?)?;
        // Reverse non-blocking flag set by `UdpSocketState` to make the test non-racy.
        socket.inner.set_nonblocking(false)?;
        Ok(socket)
    }

    #[test]
    fn ignore_empty_datagram() -> Result<(), io::Error> {
        let sender = socket()?;
        let receiver = Socket::new(std::net::UdpSocket::bind("127.0.0.1:0")?)?;
        let receiver_addr: SocketAddr = "127.0.0.1:0".parse().unwrap();

        let datagram = Datagram::new(
            sender.inner.local_addr()?,
            receiver.inner.local_addr()?,
            IpTos::default(),
            vec![],
        );

        sender.send(&datagram)?;
        let mut recv_buf = vec![0; RECV_BUF_SIZE];
        let res = receiver.recv(receiver_addr, &mut recv_buf);
        assert_eq!(res.unwrap_err().kind(), std::io::ErrorKind::WouldBlock);

        Ok(())
    }

    #[test]
    fn datagram_tos() -> Result<(), io::Error> {
        let sender = socket()?;
        let receiver = socket()?;
        let receiver_addr: SocketAddr = "127.0.0.1:0".parse().unwrap();

        let datagram = Datagram::new(
            sender.inner.local_addr()?,
            receiver.inner.local_addr()?,
            IpTos::from((IpTosDscp::Le, IpTosEcn::Ect1)),
            b"Hello, world!".to_vec(),
        );

        sender.send(&datagram)?;

        let mut recv_buf = vec![0; RECV_BUF_SIZE];
        let mut received_datagrams = receiver
            .recv(receiver_addr, &mut recv_buf)
            .expect("receive to succeed");

        // Assert that the ECN is correct.
        assert_eq!(
            IpTosEcn::from(datagram.tos()),
            IpTosEcn::from(received_datagrams.next().unwrap().tos())
        );

        Ok(())
    }

    /// Expect [`Socket::recv`] to handle multiple [`Datagram`]s on GRO read.
    #[test]
    #[cfg_attr(not(any(target_os = "linux", target_os = "windows")), ignore)]
    fn many_datagrams_through_gro() -> Result<(), io::Error> {
        const SEGMENT_SIZE: usize = 128;

        let sender = socket()?;
        let receiver = socket()?;
        let receiver_addr: SocketAddr = "127.0.0.1:0".parse().unwrap();

        // `neqo_udp::Socket::send` does not yet
        // (https://github.com/mozilla/neqo/issues/1693) support GSO. Use
        // `quinn_udp` directly.
        let max_gso_segments = sender.state.max_gso_segments();
        let msg = vec![0xAB; SEGMENT_SIZE * max_gso_segments];
        let transmit = Transmit {
            destination: receiver.inner.local_addr()?,
            ecn: EcnCodepoint::from_bits(Into::<u8>::into(IpTos::from((
                IpTosDscp::Le,
                IpTosEcn::Ect1,
            )))),
            contents: &msg,
            segment_size: Some(SEGMENT_SIZE),
            src_ip: None,
        };
        sender.state.try_send((&sender.inner).into(), &transmit)?;

        // Allow for one GSO sendmmsg to result in multiple GRO recvmmsg.
        let mut num_received = 0;
        let mut recv_buf = vec![0; RECV_BUF_SIZE];
        while num_received < max_gso_segments {
            receiver
                .recv(receiver_addr, &mut recv_buf)
                .expect("receive to succeed")
                .for_each(|d| {
                    assert_eq!(
                        SEGMENT_SIZE,
                        d.len(),
                        "Expect received datagrams to have same length as sent datagrams."
                    );
                    num_received += 1;
                });
        }

        Ok(())
    }

    #[test]
    fn fmt_datagram_iter() {
        let dgrams = [];

        let i = DatagramIter {
            meta: RecvMeta::default(),
            datagrams: dgrams.chunks(1),
            local_address: "[::]:0".parse().unwrap(),
        };

        assert_eq!(
            &format!("{i:?}"),
            "DatagramIter { meta: RecvMeta { addr: [::]:0, len: 0, stride: 0, ecn: None, dst_ip: None }, local_address: [::]:0 }"
        );
    }
}
