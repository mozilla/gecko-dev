// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(clippy::missing_errors_doc)] // Functions simply delegate to tokio and quinn-udp.

use std::{
    array,
    io::{self, IoSliceMut},
    iter,
    net::SocketAddr,
    slice::{self, Chunks},
};

use log::{log_enabled, Level};
use neqo_common::{qdebug, qtrace, Datagram, IpTos};
use quinn_udp::{EcnCodepoint, RecvMeta, Transmit, UdpSocketState};

/// Receive buffer size
///
/// Fits a maximum size UDP datagram, or, on platforms with segmentation
/// offloading, multiple smaller datagrams.
const RECV_BUF_SIZE: usize = u16::MAX as usize;

/// The number of buffers to pass to the OS on [`Socket::recv`].
///
/// Platforms without segmentation offloading, i.e. platforms not able to read
/// multiple datagrams into a single buffer, can benefit from using multiple
/// buffers instead.
///
/// Platforms with segmentation offloading have not shown performance
/// improvements when additionally using multiple buffers.
///
/// - Linux/Android: use segmentation offloading via GRO
/// - Windows: use segmentation offloading via URO (caveat see <https://github.com/quinn-rs/quinn/issues/2041>)
/// - Apple: no segmentation offloading available, use multiple buffers
#[cfg(not(apple))]
const NUM_BUFS: usize = 1;
#[cfg(apple)]
// Value approximated based on neqo-bin "Download" benchmark only.
const NUM_BUFS: usize = 16;

/// A UDP receive buffer.
pub struct RecvBuf(Vec<Vec<u8>>);

impl RecvBuf {
    #[must_use]
    pub fn new() -> Self {
        Self(vec![vec![0; RECV_BUF_SIZE]; NUM_BUFS])
    }
}

impl Default for RecvBuf {
    fn default() -> Self {
        Self::new()
    }
}

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

#[allow(clippy::missing_panics_doc)]
pub fn recv_inner<'a>(
    local_address: SocketAddr,
    state: &UdpSocketState,
    socket: impl SocketRef,
    recv_buf: &'a mut RecvBuf,
) -> Result<DatagramIter<'a>, io::Error> {
    let mut metas = [RecvMeta::default(); NUM_BUFS];
    let mut iovs: [IoSliceMut; NUM_BUFS] = {
        let mut bufs = recv_buf.0.iter_mut().map(|b| IoSliceMut::new(b));
        array::from_fn(|_| bufs.next().expect("NUM_BUFS elements"))
    };

    let n = state.recv((&socket).into(), &mut iovs, &mut metas)?;

    if log_enabled!(Level::Trace) {
        for meta in metas.iter().take(n) {
            qtrace!(
                "received {} bytes from {} to {local_address} in {} segments",
                meta.len,
                meta.addr,
                if meta.stride == 0 {
                    0
                } else {
                    meta.len.div_ceil(meta.stride)
                }
            );
        }
    }

    Ok(DatagramIter {
        current_buffer: None,
        remaining_buffers: metas.into_iter().zip(recv_buf.0.iter()).take(n),
        local_address,
    })
}

pub struct DatagramIter<'a> {
    /// The current buffer, containing zero or more datagrams, each sharing the
    /// same [`RecvMeta`].
    current_buffer: Option<(RecvMeta, Chunks<'a, u8>)>,
    /// Remaining buffers, each containing zero or more datagrams, one
    /// [`RecvMeta`] per buffer.
    remaining_buffers:
        iter::Take<iter::Zip<array::IntoIter<RecvMeta, NUM_BUFS>, slice::Iter<'a, Vec<u8>>>>,
    /// The local address of the UDP socket used to receive the datagrams.
    local_address: SocketAddr,
}

impl<'a> Iterator for DatagramIter<'a> {
    type Item = Datagram<&'a [u8]>;

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            // Return the next datagram in the current buffer, if any.
            if let Some((meta, d)) = self
                .current_buffer
                .as_mut()
                .and_then(|(meta, ds)| ds.next().map(|d| (meta, d)))
            {
                return Some(Datagram::from_slice(
                    meta.addr,
                    self.local_address,
                    meta.ecn.map(|n| IpTos::from(n as u8)).unwrap_or_default(),
                    d,
                ));
            }

            // There are no more datagrams in the current buffer. Try promoting
            // one of the remaining buffers, if any, to be the current buffer.
            let Some((meta, buf)) = self.remaining_buffers.next() else {
                // Handled all buffers. No more datagrams. Iterator is empty.
                return None;
            };

            // Ignore empty datagrams.
            if meta.len == 0 || meta.stride == 0 {
                qdebug!(
                    "ignoring empty datagram from {} to {} len {} stride {}",
                    meta.addr,
                    self.local_address,
                    meta.len,
                    meta.stride
                );
                continue;
            }

            // Got another buffer. Let's chunk it into datagrams and return the
            // first datagram in the next loop iteration.
            self.current_buffer = Some((meta, buf[0..meta.len].chunks(meta.stride)));
        }
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
            state: UdpSocketState::new((&socket).into())?,
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
        recv_buf: &'a mut RecvBuf,
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
    fn handle_empty_datagram() -> Result<(), io::Error> {
        // quinn-udp doesn't support sending emtpy datagrams across all
        // platforms. Use `std` socket instead.  See also
        // <https://github.com/quinn-rs/quinn/pull/2123>.
        let sender = std::net::UdpSocket::bind("127.0.0.1:0")?;
        let receiver = socket()?;
        let receiver_addr: SocketAddr = "127.0.0.1:0".parse().unwrap();

        sender.send_to(&[], receiver.inner.local_addr()?)?;
        let mut recv_buf = RecvBuf::new();
        let mut datagrams = receiver.recv(receiver_addr, &mut recv_buf)?;

        assert_eq!(datagrams.next(), None);

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

        let mut recv_buf = RecvBuf::new();
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
        let mut recv_buf = RecvBuf::new();
        while num_received < max_gso_segments {
            receiver
                .recv(receiver_addr, &mut recv_buf)
                .expect("receive to succeed")
                .for_each(|d| {
                    assert_eq!(
                        SEGMENT_SIZE,
                        d.len(),
                        "Expect received datagrams to have same length as sent datagrams"
                    );
                    num_received += 1;
                });
        }

        Ok(())
    }
}
