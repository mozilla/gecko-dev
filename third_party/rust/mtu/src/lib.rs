// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! A crate to return the name and maximum transmission unit (MTU) of the local network interface
//! towards a given destination `SocketAddr`, optionally from a given local `SocketAddr`.
//!
//! # Usage
//!
//! This crate exports a single function `interface_and_mtu` that returns the name and
//! [maximum transmission unit (MTU)](https://en.wikipedia.org/wiki/Maximum_transmission_unit)
//! of the outgoing network interface towards a remote destination identified by an `IpAddr`.
//!
//! # Example
//! ```
//! # use std::net::{IpAddr, Ipv4Addr};
//! let destination = IpAddr::V4(Ipv4Addr::LOCALHOST);
//! let (name, mtu): (String, usize) = mtu::interface_and_mtu(destination).unwrap();
//! println!("MTU towards {destination} is {mtu} on {name}");
//! ```
//!
//! # Supported Platforms
//!
//! * Linux
//! * macOS
//! * Windows
//! * FreeBSD
//! * NetBSD
//! * OpenBSD
//! * Solaris
//!
//! # Notes
//!
//! The returned MTU may exceed the maximum IP packet size of 65,535 bytes on some platforms for
//! some remote destinations. (For example, loopback destinations on Windows.)
//!
//! The returned interface name is obtained from the operating system.
//!
//! # Contributing
//!
//! We're happy to receive PRs that improve this crate. Please take a look at our [community
//! guidelines](CODE_OF_CONDUCT.md) beforehand.

use std::{
    io::{Error, ErrorKind, Result},
    net::IpAddr,
};

#[cfg(not(target_os = "windows"))]
macro_rules! asserted_const_with_type {
    ($name:ident, $t1:ty, $e:expr, $t2:ty) => {
        #[allow(clippy::cast_possible_truncation, clippy::cast_possible_wrap)] // Guarded by the following `const_assert_eq!`.
        const $name: $t1 = $e as $t1;
        const_assert_eq!($name as $t2, $e);
    };
}

#[cfg(any(target_os = "macos", bsd))]
mod bsd;

#[cfg(any(target_os = "linux", target_os = "android"))]
mod linux;

#[cfg(target_os = "windows")]
mod windows;

#[cfg(not(target_os = "windows"))]
mod routesocket;

#[cfg(any(target_os = "macos", bsd))]
use bsd::interface_and_mtu_impl;
#[cfg(any(target_os = "linux", target_os = "android"))]
use linux::interface_and_mtu_impl;
#[cfg(target_os = "windows")]
use windows::interface_and_mtu_impl;

/// Prepare a default error.
fn default_err() -> Error {
    Error::new(ErrorKind::NotFound, "Local interface MTU not found")
}

/// Prepare an error for cases that "should never happen".
#[cfg(not(target_os = "windows"))]
fn unlikely_err(msg: String) -> Error {
    debug_assert!(false, "{msg}");
    Error::new(ErrorKind::Other, msg)
}

/// Align `size` to the next multiple of `align` (which needs to be a power of two).
#[cfg(not(target_os = "windows"))]
const fn aligned_by(size: usize, align: usize) -> usize {
    if size == 0 {
        align
    } else {
        1 + ((size - 1) | (align - 1))
    }
}

// Platforms currently not supported.
//
// See <https://github.com/mozilla/mtu/issues/82>.
#[cfg(any(target_os = "ios", target_os = "tvos", target_os = "visionos"))]
pub fn interface_and_mtu_impl(remote: IpAddr) -> Result<(String, usize)> {
    return Err(default_err());
}

/// Return the name and maximum transmission unit (MTU) of the outgoing network interface towards a
/// remote destination identified by an [`IpAddr`],
///
/// The returned MTU may exceed the maximum IP packet size of 65,535 bytes on some platforms for
/// some remote destinations. (For example, loopback destinations on Windows.)
///
/// The returned interface name is obtained from the operating system.
///
/// # Errors
///
/// This function returns an error if the local interface MTU cannot be determined.
pub fn interface_and_mtu(remote: IpAddr) -> Result<(String, usize)> {
    interface_and_mtu_impl(remote)
}

#[cfg(test)]
mod test {
    use std::{
        env,
        net::{IpAddr, Ipv4Addr, Ipv6Addr},
    };

    use crate::interface_and_mtu;

    #[derive(Debug)]
    struct NameMtu<'a>(Option<&'a str>, usize);

    impl PartialEq<NameMtu<'_>> for (String, usize) {
        fn eq(&self, other: &NameMtu<'_>) -> bool {
            other.0.map_or(true, |name| name == self.0) && other.1 == self.1
        }
    }

    #[cfg(any(target_os = "macos", target_os = "freebsd",))]
    const LOOPBACK: &[NameMtu] = &[NameMtu(Some("lo0"), 16_384), NameMtu(Some("lo0"), 16_384)];
    #[cfg(any(target_os = "linux", target_os = "android"))]
    const LOOPBACK: &[NameMtu] = &[NameMtu(Some("lo"), 65_536), NameMtu(Some("lo"), 65_536)];
    #[cfg(target_os = "windows")]
    const LOOPBACK: &[NameMtu] = &[
        NameMtu(Some("loopback_0"), 4_294_967_295),
        NameMtu(Some("loopback_0"), 4_294_967_295),
    ];
    #[cfg(target_os = "openbsd")]
    const LOOPBACK: &[NameMtu] = &[NameMtu(Some("lo0"), 32_768), NameMtu(Some("lo0"), 32_768)];
    #[cfg(target_os = "netbsd")]
    const LOOPBACK: &[NameMtu] = &[NameMtu(Some("lo0"), 33_624), NameMtu(Some("lo0"), 33_624)];
    #[cfg(target_os = "solaris")]
    // Note: Different loopback MTUs for IPv4 and IPv6?!
    const LOOPBACK: &[NameMtu] = &[NameMtu(Some("lo0"), 8_232), NameMtu(Some("lo0"), 8_252)];

    // Non-loopback interface names are unpredictable, so we only check the MTU.
    const INET: NameMtu = NameMtu(None, 1_500);

    #[test]
    fn loopback_v4() {
        assert_eq!(
            interface_and_mtu(IpAddr::V4(Ipv4Addr::LOCALHOST)).unwrap(),
            LOOPBACK[0]
        );
    }

    #[test]
    fn loopback_v6() {
        assert_eq!(
            interface_and_mtu(IpAddr::V6(Ipv6Addr::LOCALHOST)).unwrap(),
            LOOPBACK[1]
        );
    }

    #[test]
    fn inet_v4() {
        assert_eq!(
            interface_and_mtu(IpAddr::V4(Ipv4Addr::new(
                104, 16, 132, 229 // cloudflare.com
            )))
            .unwrap(),
            INET
        );
    }

    #[test]
    fn inet_v6() {
        match interface_and_mtu(IpAddr::V6(Ipv6Addr::new(
            0x2606, 0x4700, 0, 0, 0, 0, 0x6810, 0x84e5, // cloudflare.com
        ))) {
            Ok(res) => assert_eq!(res, INET),
            // The GitHub CI environment does not have IPv6 connectivity.
            Err(_) => assert!(env::var("GITHUB_ACTIONS").is_ok()),
        }
    }
}
