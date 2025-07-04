# IntlMemoizer

[![crates.io](https://img.shields.io/crates/v/intl-memoizer.svg)](https://crates.io/crates/intl-memoizer)
[![docs.rs](https://img.shields.io/docsrs/intl-memoizer)](https://docs.rs/intl-memoizer)
[![Build](https://github.com/projectfluent/fluent-rs/actions/workflows/test.yaml/badge.svg)](https://github.com/projectfluent/fluent-rs/actions/workflows/test.yaml)
[![Coverage Status](https://coveralls.io/repos/github/projectfluent/fluent-rs/badge.svg?branch=main)](https://coveralls.io/github/projectfluent/fluent-rs?branch=main)

The `fluent-rs` workspace is a collection of Rust crates implementing [Project Fluent][],
a localization system designed to unleash the entire expressive power of natural language translations.

This crate is a memoizer specifically tailored for storing lazy-initialized intl formatters.

[Project Fluent]: https://projectfluent.org

Usage
-----

The assumption is that allocating a new formatter instance is costly, and such
instance is read-only during its life time, with constructor being expensive, and
`format`/`select` calls being cheap.

In result it pays off to use a singleton to manage memoization of all instances of intl
APIs such as `PluralRules`, `DateTimeFormat` etc. between all `FluentBundle` instances.

The following is a high-level example of how this works, for running examples see
the [docs](https://docs.rs/intl-memoizer/)

```rust
/// Internationalization formatter should implement the Memoizable trait.
impl Memoizable for NumberFormat {
  ...
}

// The main memoizer has weak references to all of the per-language memoizers.
let mut memoizer = IntlMemoizer::default();

// The formatter memoziation happens per-locale.
let lang = "en-US".parse().expect("Failed to parse.");
let lang_memoizer: Rc<IntlLangMemoizer> = memoizer.get_for_lang(en_us);

// Run the formatter

let options: NumberFormatOptions {
    minimum_fraction_digits: 3,
    maximum_fraction_digits: 5,
};

// Format pi with the options. This will lazily construct the NumberFormat.
let pi = lang_memoizer
    .with_try_get::<NumberFormat, _, _>((options,), |nf| nf.format(3.141592653))
    .unwrap()

// The example formatter constructs a string with diagnostic information about
// the configuration.
assert_eq!(text, "3.14159");

// Running it again will use the previous formatter.
let two = lang_memoizer
    .with_try_get::<NumberFormat, _, _>((options,), |nf| nf.format(2.0))
    .unwrap()

assert_eq!(text, "2.000");
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
