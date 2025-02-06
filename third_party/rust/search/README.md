# Search

The **Search Rust component** provides various [search service](https://firefox-source-docs.mozilla.org/toolkit/search/index.html) functions for Firefox Desktop, Android and iOS.

It is currently under construction and not yet used.

## Architecture

This module currently has one function, `filter_engine_configuration`, which receives configurations from remote settings and details of the user's environment. It returns a list of search engines to be presented to the user.

This component follows the architecture of the other [Application Services Rust components](https://mozilla.github.io/application-services/book/index.html): a cross-platform Rust core, and platform-specific bindings for Firefox Desktop, Android, and iOS. These bindings are generated automatically using the [UniFFI](https://mozilla.github.io/uniffi-rs/) tool.

### For contributors

This section is a primer for engineers contributing code to the Search Rust component.

`search.udl` describes the component's interface for foreign language consumers. UniFFI uses this file to generate the language bindings for each platform.

`selector.rs` contains the implementation of `SearchEngineSelector`.

## Documentation

Each Rust file contains [inline documentation](https://doc.rust-lang.org/rustdoc/what-is-rustdoc.html) for types, traits, functions, and methods.

Documentation for `pub` symbols is written with application developers in mind: even if you're a Desktop, Android or iOS developer, the Rust documentation is meant to give you an understanding of how the Gecko, Kotlin and Swift bindings work.

You can see the documentation for all public symbols by running from the command line:

```shell
cargo doc --open
```

Please help us keep our documentation useful for everyone, and feel free to file bugs for anything that looks unclear or out-of-date!

## Tests

Tests are run with

```shell
cargo test -p search
```

## Bugs

We use Bugzilla to track bugs and feature work. You can use [this link](https://bugzilla.mozilla.org/enter_bug.cgi?product=Firefox&component=Search) to file bugs in the `Firefox :: Search` bug component.
