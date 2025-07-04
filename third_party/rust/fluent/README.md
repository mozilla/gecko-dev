# Fluent

[![crates.io](https://img.shields.io/crates/v/fluent.svg)](https://crates.io/crates/fluent)
[![docs.rs](https://img.shields.io/docsrs/fluent)](https://docs.rs/fluent)
[![Build](https://github.com/projectfluent/fluent-rs/actions/workflows/test.yaml/badge.svg)](https://github.com/projectfluent/fluent-rs/actions/workflows/test.yaml)
[![Coverage Status](https://coveralls.io/repos/github/projectfluent/fluent-rs/badge.svg?branch=main)](https://coveralls.io/github/projectfluent/fluent-rs?branch=main)

The `fluent-rs` workspace is a collection of Rust crates implementing [Project Fluent][],
a localization system designed to unleash the entire expressive power of natural language translations.

This is an umbrella crate exposing the combined features of `fluent-rs` crates with additional convenience macros.

[Project Fluent]: https://projectfluent.org

Usage
-----

```rust
use fluent::{FluentBundle, FluentResource};
use unic_langid::langid;

fn main() {
    let ftl_string = "hello-world = Hello, world!".to_owned();
    let res = FluentResource::try_new(ftl_string)
        .expect("Failed to parse an FTL string.");

    let langid_en = langid!("en-US");
    let mut bundle = FluentBundle::new(vec![langid_en]);

    bundle.add_resource(&res)
        .expect("Failed to add FTL resources to the bundle.");

    let msg = bundle.get_message("hello-world")
        .expect("Message doesn't exist.");
    let mut errors = vec![];
    let pattern = msg.value
        .expect("Message has no value.");
    let value = bundle.format_pattern(&pattern, None, &mut errors);

    assert_eq!(&value, "Hello, world!");
}
```

`FluentBundle`, which is the main struct at the moment, is intended to remain
a low level API.
There is a number of higher level APIs like [fluent-resmgr][] and
[fluent-fallback][] intended to wrap around it and provide better ergonomics
for bindings and direct usage.
Those higher level APIs are not mature yet enough to be included in this package,
but will get added once they are.

[fluent-resmgr]: https://crates.io/crates/fluent-resmgr
[fluent-fallback]: https://crates.io/crates/fluent-fallback


Learn the FTL syntax
--------------------

FTL is a localization file format used for describing translation resources.
FTL stands for _Fluent Translation List_.

FTL is designed to be simple to read, but at the same time allows to represent
complex concepts from natural languages like gender, plurals, conjugations, and
others.

```fluent
hello-user = Hello, { $username }!
```

[Read the Fluent Syntax Guide][] in order to learn more about the syntax.  If
you're a tool author you may be interested in the formal [EBNF grammar][].

[Read the Fluent Syntax Guide]: http://projectfluent.org/fluent/guide/
[EBNF grammar]: https://github.com/projectfluent/fluent/tree/master/spec


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
