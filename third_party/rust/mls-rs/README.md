# mls-rs &emsp; [![Build Status]][actions] [![Latest Version]][crates.io] [![API Documentation]][docs.rs] [![codecov](https://codecov.io/gh/awslabs/mls-rs/graph/badge.svg?token=6655ESMTZT)](https://codecov.io/gh/awslabs/mls-rs)

[build status]: https://img.shields.io/github/checks-status/awslabs/mls-rs/main
[actions]: https://github.com/awslabs/mls-rs/actions?query=branch%3Amain++
[latest version]: https://img.shields.io/crates/v/mls-rs.svg
[crates.io]: https://crates.io/crates/mls-rs
[api documentation]: https://docs.rs/mls-rs/badge.svg
[docs.rs]: https://docs.rs/mls-rs

<!-- cargo-sync-readme start -->

An implementation of the [IETF Messaging Layer Security](https://messaginglayersecurity.rocks)
end-to-end encryption (E2EE) protocol.

## What is MLS?

MLS is a new IETF end-to-end encryption standard that is designed to
provide transport agnostic, asynchronous, and highly performant
communication between a group of clients.

## MLS Protocol Features

- Multi-party E2EE [group evolution](https://www.rfc-editor.org/rfc/rfc9420.html#name-cryptographic-state-and-evo)
  via a propose-then-commit mechanism.
- Asynchronous by design with pre-computed [key packages](https://www.rfc-editor.org/rfc/rfc9420.html#name-key-packages),
  allowing members to be added to a group while offline.
- Customizable credential system with built in support for X.509 certificates.
- [Extension system](https://www.rfc-editor.org/rfc/rfc9420.html#name-extensions)
  allowing for application specific data to be negotiated via the protocol.
- Strong forward secrecy and post compromise security.
- Crypto agility via support for multiple [cipher suites](https://www.rfc-editor.org/rfc/rfc9420.html#name-cipher-suites).
- Pre-shared key support.
- Subgroup branching.
- Group reinitialization for breaking changes such as protocol upgrades.

## Features

- Easy to use client interface that can manage multiple MLS identities and groups.
- 100% RFC 9420 conformance with support for all default credential, proposal,
  and extension types.
- Support for WASM builds.
- Configurable storage for key packages, secrets and group state
  via traits along with provided "in memory" and SQLite implementations.
- Support for custom user proposal and extension types.
- Ability to create user defined credentials with custom validation
  routines that can bridge to existing credential schemes.
- OpenSSL and Rust Crypto based cipher suite implementations.
- Crypto agility with support for user defined cipher suite.
- Extensive test suite including security and interop focused tests against
  pre-computed test vectors.

## Crypto Providers

For cipher suite descriptions see the RFC documentation [here](https://www.rfc-editor.org/rfc/rfc9420.html#name-mls-cipher-suites)

| Name        | Cipher Suites | X509 Support    |
| ----------- | ------------- | --------------- |
| OpenSSL     | 1-7           | Stable          |
| AWS-LC      | 1,2,3,5,7     | Stable          |
| Rust Crypto | 1,2,3         | ⚠️ Experimental |
| Web Crypto  | ⚠️ Experimental 2,5,7 | Unsupported |
| CryptoKit   | 1,2,3,5,7     | Unsupported     |

## Security Notice

This library has been validated for conformance to the RFC 9420 specification but has not yet received a full security audit by a 3rd party.

<!-- cargo-sync-readme end -->

## License

This library is licensed under the Apache-2.0 or the MIT License.
