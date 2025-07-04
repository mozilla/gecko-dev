# Fluent Fallback

[![crates.io](https://img.shields.io/crates/v/fluent-fallback.svg)](https://crates.io/crates/fluent-fallback)
[![docs.rs](https://img.shields.io/docsrs/fluent-fallback)](https://docs.rs/fluent-fallback)
[![Build](https://github.com/projectfluent/fluent-rs/actions/workflows/test.yaml/badge.svg)](https://github.com/projectfluent/fluent-rs/actions/workflows/test.yaml)
[![Coverage Status](https://coveralls.io/repos/github/projectfluent/fluent-rs/badge.svg?branch=main)](https://coveralls.io/github/projectfluent/fluent-rs?branch=main)

The `fluent-rs` workspace is a collection of Rust crates implementing [Project Fluent][],
a localization system designed to unleash the entire expressive power of natural language translations.

This crate is a high-level implementation of a collection of locale bundles including fallback between locales.

[Project Fluent]: https://projectfluent.org

Usage
-----

The `Localization` struct encapsulates a persistent localization context providing language fallbacking.
The instance remains available throughout the whole life cycle of the corresponding UI,
reacting to events such as locale changes, resource updates etc.

The API can be used directly, or can serve as an example of state manager for `fluent-bundle` and `fluent-resmgr`.

```rust
use fluent_fallback::Localization;

fn main() {
    // generate_messages is a closure that returns an iterator over FluentBundle
    // instances.
    let loc = Localization::new(vec!["simple.ftl".into()], generate_messages);

    let value = bundle.format_value("hello-world", None);

    assert_eq!(&value, "Hello, world!");
}
```

Get Involved
------------

`fluent-rs` is open-source, licensed under both the Apache 2.0 and MIT licenses.  We
encourage everyone to take a look at our code and we'll listen to your
feedback.


Discuss
-------

We'd love to hear your thoughts on Project Fluent! Whether you're a localizer
looking for a better way to express yourself in your language, or a developer
trying to make your app localizable and multilingual, or a hacker looking for
a project to contribute to, please do get in touch on the mailing list and the
IRC channel.

 - Discourse: https://discourse.mozilla.org/c/fluent
 - Matrix channel: <a href="https://chat.mozilla.org/#/room/#fluent:mozilla.org">#fluent:mozilla.org</a>
