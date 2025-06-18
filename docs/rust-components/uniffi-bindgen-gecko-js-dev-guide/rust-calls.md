# Rust Calls from JavaScript

## Background

* [UniFFI general documentation](https://mozilla.github.io/uniffi-rs/latest/internals/rust_calls.html).
* [Threading concerns](../developing-rust-components/threading.md)
* [Lifting and Lowering](./lifting-and-lowering.md)

## Sync Rust calls

* The generated JavaScript code calls [UniffiScaffolding.callSync()](https://searchfox.org/mozilla-central/source/dom/chrome-webidl/UniFFI.webidl).
    * The first argument to `callSync()` is a numeric identifier for the FFI call, known to both the JavaScript and C++ code generators.
    * That is followed by lowered argument from the JavaScript call.
* The generated C++ code then performs the second phase of argument lowering and makes the Rust call.
  This code is statically linked to the Rust code, so it can make this call directly.
* The C++ code then returns a `UniFFIScaffoldingCallResult` to the JavaScript code, which is essentially a `UniffiCallStatus` plus a return value.
* The generated JavaScript code inspects the `UniFFIScaffoldingCallResult` and either returns a value or raises an exception.

## Wrapped-async calls

* `UniffiScaffolding.callAsyncWrapper` is called instead of `UniffiScaffolding.callSync`.
* The generated C++ code schedules the Rust call in a worker thread.
* The generated C++ code returns a `Promise` to JavaScript
* The generated C++ code resolves that promise using the returned Rust value.
* The C++ code resolves the promise with a `UniFFIScaffoldingCallResult` value.

## Async calls

* Use `UniffiScaffolding.callAsync()` is used to make the Rust call
* The generated C++ code returns a `Promise` to JavaScript
* The generated C++ code implements the future callback and polls the Rust function as described in the [UniFFI async overview](https://mozilla.github.io/uniffi-rs/latest/internals/async-overview.html).
* Once the Rust future is complete, the C++ code resolves the JavaScript promise with the result
* The C++ code resolves the promise with a `UniFFIScaffoldingCallResult` value.

Note: The generated code does not handle cancellation or the `foreign_future_dropped_callback`.
In JavaScript once an async task has started running, there's no way to force it to stop.
Other JavaScript bindings have handled this by passing an [Abort Controller](https://developer.mozilla.org/en-US/docs/Web/API/AbortController) as an extra argument to async functions, maybe we could also do this in the future.
