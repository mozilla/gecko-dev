// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{
    ffi::CStr,
    io::{Error, ErrorKind, Read as _, Result, Write as _},
    marker::PhantomData,
    net::IpAddr,
    num::TryFromIntError,
    ops::Deref,
    ptr, slice,
};

use libc::{
    freeifaddrs, getifaddrs, getpid, if_indextoname, ifaddrs, in6_addr, in_addr, sockaddr,
    sockaddr_dl, sockaddr_in, sockaddr_in6, sockaddr_storage, AF_UNSPEC, PF_ROUTE,
};
use static_assertions::{const_assert, const_assert_eq};

#[allow(
    non_camel_case_types,
    clippy::struct_field_names,
    clippy::too_many_lines,
    clippy::cognitive_complexity,
    dead_code // RTA_IFP is only used on NetBSD and Solaris
)]
mod bindings {
    include!(env!("BINDINGS"));
}

#[cfg(any(target_os = "netbsd", target_os = "solaris"))]
use crate::bsd::bindings::RTA_IFP;
use crate::{
    aligned_by,
    bsd::bindings::{if_data, rt_msghdr, RTAX_MAX, RTA_DST},
    default_err,
    routesocket::RouteSocket,
    unlikely_err,
};

#[cfg(target_os = "macos")]
const ALIGN: usize = std::mem::size_of::<libc::c_int>();

#[cfg(bsd)]
// See https://github.com/freebsd/freebsd-src/blob/524a425d30fce3d5e47614db796046830b1f6a83/sys/net/route.h#L362-L371
// See https://github.com/NetBSD/src/blob/4b50954e98313db58d189dd87b4541929efccb09/sys/net/route.h#L329-L331
// See https://github.com/Arquivotheca/Solaris-8/blob/2ad1d32f9eeed787c5adb07eb32544276e2e2444/osnet_volume/usr/src/cmd/cmd-inet/usr.sbin/route.c#L238-L239
const ALIGN: usize = std::mem::size_of::<libc::c_long>();

#[cfg(any(target_os = "macos", target_os = "freebsd", target_os = "openbsd"))]
asserted_const_with_type!(RTM_ADDRS, i32, RTA_DST, u32);

#[cfg(any(target_os = "netbsd", target_os = "solaris"))]
asserted_const_with_type!(RTM_ADDRS, i32, RTA_DST | RTA_IFP, u32);

#[cfg(not(target_os = "solaris"))]
type AddressFamily = u8;

#[cfg(target_os = "solaris")]
type AddressFamily = u16;

asserted_const_with_type!(AF_INET, AddressFamily, libc::AF_INET, i32);
asserted_const_with_type!(AF_INET6, AddressFamily, libc::AF_INET6, i32);
asserted_const_with_type!(AF_LINK, AddressFamily, libc::AF_LINK, i32);
asserted_const_with_type!(RTM_VERSION, u8, bindings::RTM_VERSION, u32);
asserted_const_with_type!(RTM_GET, u8, bindings::RTM_GET, u32);

const_assert!(std::mem::size_of::<sockaddr_in>() + ALIGN <= u8::MAX as usize);
const_assert!(std::mem::size_of::<sockaddr_in6>() + ALIGN <= u8::MAX as usize);
const_assert!(std::mem::size_of::<rt_msghdr>() <= u8::MAX as usize);

struct IfAddrs(*mut ifaddrs);

impl Default for IfAddrs {
    fn default() -> Self {
        Self(ptr::null_mut())
    }
}

impl IfAddrs {
    fn new() -> Result<Self> {
        let mut ifap = Self::default();
        // getifaddrs allocates memory for the linked list of interfaces that is freed by
        // `IfAddrs::drop`.
        if unsafe { getifaddrs(ptr::from_mut(&mut ifap.0)) } != 0 {
            return Err(Error::last_os_error());
        }
        Ok(ifap)
    }

    const fn iter(&self) -> IfAddrPtr {
        IfAddrPtr {
            ptr: self.0,
            _ref: PhantomData,
        }
    }
}

impl Drop for IfAddrs {
    fn drop(&mut self) {
        if !self.0.is_null() {
            // Free the memory allocated by `getifaddrs`.
            unsafe {
                freeifaddrs(self.0);
            }
        }
    }
}

struct IfAddrPtr<'a> {
    ptr: *mut ifaddrs,
    _ref: PhantomData<&'a ifaddrs>,
}

impl IfAddrPtr<'_> {
    fn addr(&self) -> sockaddr {
        unsafe { *self.ifa_addr }
    }

    fn name(&self) -> String {
        unsafe { CStr::from_ptr(self.ifa_name).to_string_lossy().to_string() }
    }

    fn data(&self) -> Option<if_data> {
        if self.ifa_data.is_null() {
            None
        } else {
            Some(unsafe { self.ifa_data.cast::<if_data>().read() })
        }
    }
}

impl Deref for IfAddrPtr<'_> {
    type Target = ifaddrs;

    fn deref(&self) -> &Self::Target {
        unsafe { self.ptr.as_ref().expect("can deref") }
    }
}

impl Iterator for IfAddrPtr<'_> {
    type Item = Self;

    fn next(&mut self) -> Option<Self::Item> {
        ptr::NonNull::new(self.ptr).map(|p| {
            self.ptr = unsafe { p.as_ref().ifa_next };
            IfAddrPtr {
                ptr: p.as_ptr(),
                _ref: PhantomData,
            }
        })
    }
}

fn if_name_mtu(idx: u32) -> Result<(String, Option<usize>)> {
    let mut name = [0; libc::IF_NAMESIZE];
    // if_indextoname writes into the provided buffer.
    if unsafe { if_indextoname(idx, name.as_mut_ptr()).is_null() } {
        return Err(Error::last_os_error());
    }
    // Convert to Rust string.
    let name = unsafe {
        CStr::from_ptr(name.as_ptr())
            .to_str()
            .map_err(|err| Error::new(ErrorKind::Other, err))?
    };
    let mtu = IfAddrs::new()?
        .iter()
        .find(|ifa| ifa.addr().sa_family == AF_LINK && ifa.name() == name)
        .and_then(|ifa| ifa.data())
        .and_then(|ifa_data| usize::try_from(ifa_data.ifi_mtu).ok());
    Ok((name.to_string(), mtu))
}

#[repr(C)]
union SockaddrStorage {
    sin: sockaddr_in,
    sin6: sockaddr_in6,
}

fn sockaddr_len(af: AddressFamily) -> Result<usize> {
    let sa_len = match af {
        AF_INET => std::mem::size_of::<sockaddr_in>(),
        AF_INET6 => std::mem::size_of::<sockaddr_in6>(),
        _ => {
            return Err(Error::new(
                ErrorKind::InvalidInput,
                format!("Unsupported address family {af:?}"),
            ))
        }
    };
    Ok(aligned_by(sa_len, ALIGN))
}

impl From<IpAddr> for SockaddrStorage {
    fn from(ip: IpAddr) -> Self {
        match ip {
            IpAddr::V4(ip) => SockaddrStorage {
                sin: sockaddr_in {
                #[cfg(not(target_os = "solaris"))]
                #[allow(clippy::cast_possible_truncation)]
                // `sockaddr_in` len is <= u8::MAX per `const_assert!` above.
                sin_len: std::mem::size_of::<sockaddr_in>() as u8,
                sin_family: AF_INET,
                sin_addr: in_addr {
                    s_addr: u32::from_ne_bytes(ip.octets()),
                },
                sin_port: 0,
                sin_zero: [0; 8],
            },
            },
            IpAddr::V6(ip) => SockaddrStorage {
                sin6: sockaddr_in6 {
                #[cfg(not(target_os = "solaris"))]
                #[allow(clippy::cast_possible_truncation)]
                // `sockaddr_in6` len is <= u8::MAX per `const_assert!` above.
                sin6_len: std::mem::size_of::<sockaddr_in6>() as u8,
                sin6_family: AF_INET6,
                sin6_addr: in6_addr {
                    s6_addr: ip.octets(),
                },
                sin6_port: 0,
                sin6_flowinfo: 0,
                sin6_scope_id: 0,
                #[cfg(target_os = "solaris")]
                __sin6_src_id: 0,
                },
            },
        }
    }
}

#[repr(C)]
struct RouteMessage {
    rtm: rt_msghdr,
    sa: SockaddrStorage,
}

impl RouteMessage {
    fn new(remote: IpAddr, seq: i32) -> Result<Self> {
        let sa = SockaddrStorage::from(remote);
        let sa_len = sockaddr_len(match remote {
            IpAddr::V4(_) => AF_INET,
            IpAddr::V6(_) => AF_INET6,
        })?;
        Ok(Self {
            rtm: rt_msghdr {
                #[allow(clippy::cast_possible_truncation)]
                // `rt_msghdr` len + `ALIGN` is <= u8::MAX per `const_assert!` above.
                rtm_msglen: (std::mem::size_of::<rt_msghdr>() + sa_len) as u16,
                rtm_version: RTM_VERSION,
                rtm_type: RTM_GET,
                rtm_seq: seq,
                rtm_addrs: RTM_ADDRS,
                ..Default::default()
            },
            sa,
        })
    }

    const fn version(&self) -> u8 {
        self.rtm.rtm_version
    }

    const fn kind(&self) -> u8 {
        self.rtm.rtm_type
    }

    const fn len(&self) -> usize {
        self.rtm.rtm_msglen as usize
    }
}

impl From<&RouteMessage> for &[u8] {
    fn from(value: &RouteMessage) -> Self {
        debug_assert!(value.len() >= std::mem::size_of::<Self>());
        unsafe { slice::from_raw_parts(ptr::from_ref(value).cast(), value.len()) }
    }
}

impl From<&[u8]> for rt_msghdr {
    fn from(value: &[u8]) -> Self {
        debug_assert!(value.len() >= std::mem::size_of::<Self>());
        unsafe { ptr::read_unaligned(value.as_ptr().cast()) }
    }
}

fn if_index_mtu(remote: IpAddr) -> Result<(u16, Option<usize>)> {
    // Open route socket.
    let mut fd = RouteSocket::new(PF_ROUTE, AF_UNSPEC)?;

    // Send route message.
    let query_seq = RouteSocket::new_seq();
    let query = RouteMessage::new(remote, query_seq)?;
    let query_version = query.version();
    let query_type = query.kind();
    fd.write_all((&query).into())?;

    // Read route messages.
    let pid = unsafe { getpid() };
    loop {
        let mut buf = vec![
            0u8;
            std::mem::size_of::<rt_msghdr>() +
        // There will never be `RTAX_MAX` sockaddrs attached, but it's a safe upper bound.
         (RTAX_MAX as usize * std::mem::size_of::<sockaddr_storage>())
        ];
        let len = fd.read(&mut buf[..])?;
        if len < std::mem::size_of::<rt_msghdr>() {
            return Err(default_err());
        }
        let (reply, mut sa) = buf.split_at(std::mem::size_of::<rt_msghdr>());
        let reply: rt_msghdr = reply.into();
        if !(reply.rtm_version == query_version
            && reply.rtm_pid == pid
            && reply.rtm_seq == query_seq)
        {
            continue;
        }
        if reply.rtm_type != query_type {
            return Err(default_err());
        }

        // This is a reply to our query.
        // This is the reply we are looking for.
        // Some BSDs let us get the interface index and MTU directly from the reply.
        let mtu = (reply.rtm_rmx.rmx_mtu != 0)
            .then(|| usize::try_from(reply.rtm_rmx.rmx_mtu))
            .transpose()
            .map_err(|e: TryFromIntError| unlikely_err(e.to_string()))?;
        if reply.rtm_index != 0 {
            // Some BSDs return the interface index directly.
            return Ok((reply.rtm_index, mtu));
        }
        // For others, we need to extract it from the sockaddrs.
        for i in 0..RTAX_MAX {
            if (reply.rtm_addrs & (1 << i)) == 0 {
                continue;
            }
            let saddr = unsafe { ptr::read_unaligned(sa.as_ptr().cast::<sockaddr>()) };
            if saddr.sa_family != AF_LINK {
                (_, sa) = sa.split_at(sockaddr_len(saddr.sa_family)?);
                continue;
            }
            let sdl = unsafe { ptr::read_unaligned(sa.as_ptr().cast::<sockaddr_dl>()) };
            return Ok((sdl.sdl_index, mtu));
        }
    }
}

pub fn interface_and_mtu_impl(remote: IpAddr) -> Result<(String, usize)> {
    let (if_index, mtu1) = if_index_mtu(remote)?;
    let (if_name, mtu2) = if_name_mtu(if_index.into())?;
    Ok((if_name, mtu1.or(mtu2).ok_or_else(default_err)?))
}
