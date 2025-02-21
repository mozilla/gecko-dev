# Developing Shared Rust Components

Shared Rust Components is a strategy to create a single component that provides the same functionality for Firefox Desktop, Firefox Android and Firefox iOS.

This strategy was first adopted for sync in response to issues stemming from separate sync implementations for each application. Separate sync implementations lead to duplicate logic across platforms, which was difficult to maintain.  Furthermore, the likelihood of errors was very high -- especially since each implementation had to sync with the other implementations. Since then more components have adopted the Rust components strategy and some non-Firefox projects have started using the components (for example nimbus-experimenter).

The basic idea is to write the component in Rust then generate a set of Kotlin/Swift/JS bindings that can be consumed by the Firefox applications.
Rust was chosen because it supports a large number of target platforms, including Android and iOS.
The [UniFFI](https://mozilla.github.io/uniffi-rs/latest/) library was created to handle bindings
generation.

Creating a shared Rust component requires extra work compared to writing a component for a single application, however it's usually faster to develop then 3 separate implementations.  Furthermore, it's almost always easier to maintain a single Rust codebase then a Kotlin, Swift, and JS one. This section describes how to develop shared Rust components.

```{toctree}
:titlesonly:
:maxdepth: 1

example-component
uniffi
