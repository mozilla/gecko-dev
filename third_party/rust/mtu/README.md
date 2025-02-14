[![Coverage Status](https://codecov.io/gh/mozilla/mtu/branch/main/graph/badge.svg)](https://codecov.io/gh/mozilla/mtu)
[![Average time to resolve an issue](https://isitmaintained.com/badge/resolution/mozilla/mtu.svg)](https://isitmaintained.com/project/mozilla/mtu "Average time to resolve an issue")
[![Percentage of issues still open](https://isitmaintained.com/badge/open/mozilla/mtu.svg)](https://isitmaintained.com/project/mozilla/mtu "Percentage of issues still open")
![Maintenance](https://img.shields.io/badge/maintenance-activly--developed-brightgreen.svg)

# mtu

A crate to return the name and maximum transmission unit (MTU) of the local network interface
towards a given destination `SocketAddr`, optionally from a given local `SocketAddr`.

## Usage

This crate exports a single function `interface_and_mtu` that returns the name and
[maximum transmission unit (MTU)](https://en.wikipedia.org/wiki/Maximum_transmission_unit)
of the outgoing network interface towards a remote destination identified by an `IpAddr`.

## Example
```rust
let destination = IpAddr::V4(Ipv4Addr::LOCALHOST);
let (name, mtu): (String, usize) = mtu::interface_and_mtu(destination).unwrap();
println!("MTU towards {destination} is {mtu} on {name}");
```

## Supported Platforms

* Linux
* macOS
* Windows
* FreeBSD
* NetBSD
* OpenBSD
* Solaris

## Notes

The returned MTU may exceed the maximum IP packet size of 65,535 bytes on some platforms for
some remote destinations. (For example, loopback destinations on Windows.)

The returned interface name is obtained from the operating system.

## Contributing

We're happy to receive PRs that improve this crate. Please take a look at our [community
guidelines](CODE_OF_CONDUCT.md) beforehand.

License: MIT OR Apache-2.0
