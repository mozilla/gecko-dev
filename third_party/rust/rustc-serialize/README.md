# rustc-serialize

> **NOTE**: This crate is deprecated in favor of [`serde`]. No new feature
> development will happen in this crate, although bug fixes proposed through PRs
> will still be merged. It is very highly recommended by the Rust Library Team
> that you use [`serde`], not this crate.

[`serde`]: https://serde.rs

Serialization and deserialization support provided by the compiler in the form
of `derive(RustcEncodable, RustcDecodable)`.

[![Linux Build Status](https://travis-ci.org/rust-lang-nursery/rustc-serialize.svg?branch=master)](https://travis-ci.org/rust-lang-nursery/rustc-serialize)
[![Windows Build Status](https://ci.appveyor.com/api/projects/status/ka194de75aapwpft?svg=true)](https://ci.appveyor.com/project/alexcrichton/rustc-serialize)

[Documentation](https://doc.rust-lang.org/rustc-serialize)

## Usage

Add this to your `Cargo.toml`:

```toml
[dependencies]
rustc-serialize = "0.3"
```

and this to your crate root:

```rust
extern crate rustc_serialize;
```
