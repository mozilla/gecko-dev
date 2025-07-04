# Fluent Pseudo

[![crates.io](https://img.shields.io/crates/v/fluent-pseudo.svg)](https://crates.io/crates/fluent-pseudo)
[![docs.rs](https://img.shields.io/docsrs/fluent-pseudo)](https://docs.rs/fluent-pseudo)
[![Build](https://github.com/projectfluent/fluent-rs/actions/workflows/test.yaml/badge.svg)](https://github.com/projectfluent/fluent-rs/actions/workflows/test.yaml)
[![Coverage Status](https://coveralls.io/repos/github/projectfluent/fluent-rs/badge.svg?branch=main)](https://coveralls.io/github/projectfluent/fluent-rs?branch=main)

The `fluent-rs` workspace is a collection of Rust crates implementing [Project Fluent][],
a localization system designed to unleash the entire expressive power of natural language translations.

This crate provides a pseudolocalization and transformation API.

[Project Fluent]: https://projectfluent.org

Usage
-----

```rust
use fluent_bundle::{FluentBundle, FluentResource};
use unic_langid::langid;
use fluent_pseudo::transform;

fn transform_wrapper(s: &str) -> Cow<str> {
    // Not flipped and elongated pseudolocalization.
    transform(s, false, true, false)
}


fn main() {
    let ftl_string = "hello-world = Hello, world!".to_owned();
    let res = FluentResource::try_new(ftl_string)
        .expect("Could not parse an FTL string.");

    let langid_en = langid!("en");
    let mut bundle = FluentBundle::new(vec![langid_en]);

    // Set pseudolocalization
    bundle.set_transform(Some(transform_wrapper));

    bundle.add_resource(&res)
        .expect("Failed to add FTL resources to the bundle.");

    let msg = bundle.get_message("hello-world")
        .expect("Failed to retrieve a message.");
    let val = msg.value.expect("Message has no value.");

    let mut errors = vec![];
    let value = bundle.format_pattern(val, None, &mut errors);

    assert_eq!(&value, "Ħḗḗŀŀǿǿ Ẇǿǿřŀḓ!");
}
```
