# Developing Shared Rust Components

Shared Rust Components is a strategy to create a single component that provides the same functionality for Firefox Desktop, Firefox Android and Firefox iOS.

This strategy was first adopted for sync in response to issues stemming from separate sync implementations for each application. Separate sync implementations lead to duplicate logic across platforms, which was difficult to maintain.  Furthermore, the likelihood of errors was very high -- especially since each implementation had to sync with the other implementations. Since then more components have adopted the Rust components strategy and some non-Firefox projects have started using the components (for example nimbus-experimenter).

The basic idea is to write the component in Rust then generate a set of Kotlin/Swift/JS bindings that can be consumed by the Firefox applications.
Rust was chosen because it supports a large number of target platforms, including Android and iOS.
The [UniFFI](https://mozilla.github.io/uniffi-rs/latest/) library was created to handle bindings
generation.

## Pros and cons of adding a shared Rust component

### Pros

* Less work than creating a multiple implementations.
* More consistent behavior across different applications.
* Better sync integrations (this follows from the last point, since behavior differences often become larger bugs when sync is involved).
* Build JavaScript, Kotlin, and Swift docs from the Rust docstrings.

### Cons

* More work than creating a single implementation.
* Requires engineers to write and understand Rust.
* Still requires some application glue code to integrate the component.
  Often it's trivial like [TabsStore](https://searchfox.org/mozilla-central/source/services/sync/modules/TabsStore.sys.mjs), but sometimes it's larger.

### Neutral

* Performance is usually similar to a JavaScript/Kotlin code.
  FFI overhead typically negates any speedups from using Rust.

## Shared Rust component ownership

Teams that write shared Rust components are responsible for maintaining their health.
This includes monitoring error reports and working with application teams to maintain integrations as the application code changes.
Error reports are currently only available for Android, but we will soon be extending this to all platforms.


```{toctree}
:titlesonly:
:maxdepth: 1

example-component
uniffi
