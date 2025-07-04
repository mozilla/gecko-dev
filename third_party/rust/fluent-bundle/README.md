# Fluent Bundle

[![crates.io](https://img.shields.io/crates/v/fluent-bundle.svg)](https://crates.io/crates/fluent-bundle)
[![docs.rs](https://img.shields.io/docsrs/fluent-bundle)](https://docs.rs/fluent-bundle)
[![Build](https://github.com/projectfluent/fluent-rs/actions/workflows/test.yaml/badge.svg)](https://github.com/projectfluent/fluent-rs/actions/workflows/test.yaml)
[![Coverage Status](https://coveralls.io/repos/github/projectfluent/fluent-rs/badge.svg?branch=main)](https://coveralls.io/github/projectfluent/fluent-rs?branch=main)

The `fluent-rs` workspace is a collection of Rust crates implementing [Project Fluent][],
a localization system designed to unleash the entire expressive power of natural language translations.

This crate is a low-level implementation of a collection of localization messages for a single locale.

[Project Fluent]: https://projectfluent.org

Usage
-----

```rust
use fluent_bundle::{FluentBundle, FluentResource};
use unic_langid::langid;

fn main() {
    let ftl_string = "hello-world = Hello, world!".to_owned();
    let res = FluentResource::try_new(ftl_string)
        .expect("Could not parse an FTL string.");

    let langid_en = langid!("en");
    let mut bundle = FluentBundle::new(vec![langid_en]);

    bundle.add_resource(&res)
        .expect("Failed to add FTL resources to the bundle.");

    let msg = bundle.get_message("hello-world")
        .expect("Failed to retrieve a message.");
    let val = msg.value.expect("Message has no value.");

    let mut errors = vec![];
    let value = bundle.format_pattern(val, None, &mut errors);

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
