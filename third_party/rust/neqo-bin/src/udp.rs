// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![expect(clippy::missing_errors_doc, reason = "Passing up tokio errors.")]

use std::{io, net::SocketAddr};

use neqo_common::{qdebug, Datagram};
use neqo_udp::{DatagramIter, RecvBuf};

/// Ideally this would live in [`neqo-udp`]. [`neqo-udp`] is used in Firefox.
///
/// Firefox uses `cargo vet`. [`tokio`] the dependency of [`neqo-udp`] is not
/// audited as `safe-to-deploy`. `cargo vet` will require `safe-to-deploy` for
/// [`tokio`] even when behind a feature flag.
///
/// See <https://github.com/mozilla/cargo-vet/issues/626>.
pub struct Socket {
    state: quinn_udp::UdpSocketState,
    inner: tokio::net::UdpSocket,
}

impl Socket {
    /// Create a new [`Socket`] bound to the provided address, not managed externally.
    pub fn bind<A: std::net::ToSocketAddrs>(addr: A) -> Result<Self, io::Error> {
        const ONE_MB: usize = 1 << 20;
        let socket = std::net::UdpSocket::bind(addr)?;
        let state = quinn_udp::UdpSocketState::new((&socket).into())?;

        // FIXME: We need to experiment if increasing this actually improves performance.
        // Also, on BSD and Apple targets, this seems to increase the `net.inet.udp.maxdgram`
        // sysctl, which is not the same as the socket buffer.
        // if send_buf_before < ONE_MB {
        //     state.set_send_buffer_size((&socket).into(), ONE_MB)?;
        //     let send_buf_after = state.send_buffer_size((&socket).into())?;
        //     qdebug!("Increasing socket send buffer size from {send_buf_before} to {ONE_MB}, now:
        // {send_buf_after}"); } else {
        //     qdebug!("Default socket send buffer size is {send_buf_before}, not changing");
        // }
        qdebug!(
            "Default socket send buffer size is {:?}",
            state.send_buffer_size((&socket).into())
        );

        let recv_buf_before = state.recv_buffer_size((&socket).into())?;
        if recv_buf_before < ONE_MB {
            // Same as Firefox.
            // <https://searchfox.org/mozilla-central/rev/fa5b44a4ea5c98b6a15f39638ea4cd04dc271f3d/modules/libpref/init/StaticPrefList.yaml#13474-13477>
            state.set_recv_buffer_size((&socket).into(), ONE_MB)?;
            qdebug!(
                "Increasing socket recv buffer size from {recv_buf_before} to {ONE_MB}, now: {:?}",
                state.recv_buffer_size((&socket).into())
            );
        } else {
            qdebug!("Default socket receive buffer size is {recv_buf_before}, not changing");
        }

        Ok(Self {
            state,
            inner: tokio::net::UdpSocket::from_std(socket)?,
        })
    }

    /// See [`tokio::net::UdpSocket::local_addr`].
    pub fn local_addr(&self) -> io::Result<SocketAddr> {
        self.inner.local_addr()
    }

    /// See [`tokio::net::UdpSocket::writable`].
    pub async fn writable(&self) -> Result<(), io::Error> {
        self.inner.writable().await
    }

    /// See [`tokio::net::UdpSocket::readable`].
    pub async fn readable(&self) -> Result<(), io::Error> {
        self.inner.readable().await
    }

    /// Send a [`Datagram`] on the given [`Socket`].
    pub fn send(&self, d: &Datagram) -> io::Result<()> {
        self.inner.try_io(tokio::io::Interest::WRITABLE, || {
            neqo_udp::send_inner(&self.state, (&self.inner).into(), d)
        })
    }

    /// Receive a batch of [`Datagram`]s on the given [`Socket`], each set with
    /// the provided local address.
    pub fn recv<'a>(
        &self,
        local_address: SocketAddr,
        recv_buf: &'a mut RecvBuf,
    ) -> Result<Option<DatagramIter<'a>>, io::Error> {
        self.inner
            .try_io(tokio::io::Interest::READABLE, || {
                neqo_udp::recv_inner(local_address, &self.state, &self.inner, recv_buf)
            })
            .map(Some)
            .or_else(|e| {
                if e.kind() == io::ErrorKind::WouldBlock {
                    Ok(None)
                } else {
                    Err(e)
                }
            })
    }
}
