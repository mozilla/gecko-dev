# uniffi-bindgen-gecko-js Developers Guide

This section describes the internal workings of `uniffi-bindgen-gecko-js` for developers working with this code.
It builds on the [UniFFI user guide](https://mozilla.github.io/uniffi-rs/latest/) internals documentation, which is recommended reading before diving into these specifics.

## UniFFI bindings pipeline

`uniffi-bindgen-gecko-js` uses the bindings pipeline ([docs](https://github.com/mozilla/uniffi-rs/blob/main/docs/manual/src/internals/bindings_ir.md) [more docs](https://github.com/mozilla/uniffi-rs/blob/main/docs/manual/src/internals/bindings_ir_pipeline.md)).
`uniffi-bindgen-gecko-js` is the first bindings generator to adopt this code, although we hope to eventually move all bindings to this system.
This means the `uniffi-bindgen-gecko-js` code may look different than other bindings, but we hope it's only temporary.

```{toctree}
:titlesonly:
:maxdepth: 1

js-and-cpp-layers
lifting-and-lowering
rust-calls
js-callback-interfaces
