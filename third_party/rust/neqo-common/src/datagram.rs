// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{net::SocketAddr, ops::Deref};

use crate::{hex_with_len, IpTos};

#[derive(Clone, PartialEq, Eq)]
pub struct Datagram<D = Vec<u8>> {
    src: SocketAddr,
    dst: SocketAddr,
    tos: IpTos,
    d: D,
}

impl<D> Datagram<D> {
    #[must_use]
    pub const fn source(&self) -> SocketAddr {
        self.src
    }

    #[must_use]
    pub const fn destination(&self) -> SocketAddr {
        self.dst
    }

    #[must_use]
    pub const fn tos(&self) -> IpTos {
        self.tos
    }

    pub fn set_tos(&mut self, tos: IpTos) {
        self.tos = tos;
    }
}

impl<D: AsRef<[u8]>> Datagram<D> {
    pub fn len(&self) -> usize {
        self.d.as_ref().len()
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }
}

#[cfg(test)]
impl<D: AsMut<[u8]> + AsRef<[u8]>> AsMut<[u8]> for Datagram<D> {
    fn as_mut(&mut self) -> &mut [u8] {
        self.d.as_mut()
    }
}

impl Datagram<Vec<u8>> {
    pub fn new<V: Into<Vec<u8>>>(src: SocketAddr, dst: SocketAddr, tos: IpTos, d: V) -> Self {
        Self {
            src,
            dst,
            tos,
            d: d.into(),
        }
    }
}

impl<D: AsRef<[u8]>> Deref for Datagram<D> {
    type Target = [u8];
    #[must_use]
    fn deref(&self) -> &Self::Target {
        AsRef::<[u8]>::as_ref(self)
    }
}

impl<D: AsRef<[u8]>> std::fmt::Debug for Datagram<D> {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(
            f,
            "Datagram {:?} {:?}->{:?}: {}",
            self.tos,
            self.src,
            self.dst,
            hex_with_len(&self.d)
        )
    }
}

impl<'a> Datagram<&'a [u8]> {
    #[must_use]
    pub const fn from_slice(src: SocketAddr, dst: SocketAddr, tos: IpTos, d: &'a [u8]) -> Self {
        Self { src, dst, tos, d }
    }

    #[must_use]
    pub fn to_owned(&self) -> Datagram {
        Datagram {
            src: self.src,
            dst: self.dst,
            tos: self.tos,
            d: self.d.to_vec(),
        }
    }
}

impl<D: AsRef<[u8]>> AsRef<[u8]> for Datagram<D> {
    fn as_ref(&self) -> &[u8] {
        self.d.as_ref()
    }
}

#[cfg(test)]
mod tests {
    use test_fixture::datagram;

    #[test]
    fn fmt_datagram() {
        let d = datagram([0; 1].to_vec());
        assert_eq!(
            &format!("{d:?}"),
            "Datagram IpTos(Cs0, Ect0) [fe80::1]:443->[fe80::1]:443: [1]: 00"
        );
    }

    #[test]
    fn is_empty() {
        let d = datagram(vec![]);
        assert_eq!(d.len(), 0);
        assert!(d.is_empty());
    }
}
