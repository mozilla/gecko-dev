# JavaScript Callback Interfaces

## Background

* [UniFFI general documentation](https://mozilla.github.io/uniffi-rs/latest/internals/foreign_calls.html)
* [Threading concerns](../developing-rust-components/threading.md)
* [Lifting and Lowering](./lifting-and-lowering.md)

## JavaScript layer

The generated JavaScript bindings create a [UniFFICallbackHandler](https://searchfox.org/mozilla-central/rev/b5ab48b8c33faf53817cb2ef64c8523a469ef695/dom/chrome-webidl/UniFFI.webidl#75) for each callback interface.
This stores the callback interface implementations that are manually written by Firefox engineers.
These are stored in a map, where the key is an integer handle for the callback interface.

`UniFFICallbackHandler.callAsync` is used by the C++ layer to invoke callback interface methods.
See below for why we only currently have `callAsync()`.
`UniFFICallbackHandler.callAsync()` inputs:

* The object handle
* The method index
* Each argument for the callback method, after being lowered by JavaScript.

`UniFFICallbackHandler.callAsync()` returns a `UniFFIScaffoldingCallResult`.
Like with [Rust calls](./rust-calls.md), this is a `UniffiCallStatus` combined with a return value.

For each callback interface, the JavaScript layer calls `UniFFIScaffolding.registerCallbackHandler()` with the `UniFFICallbackHandler` for that interface.
Like with Rust calls, the bindings code generates a unique ID to identify each callback interface.

## C++ layer

The C++ layer acts as a bridge between the generated Rust code and the generated JavaScript code.
It registers a vtable with the Rust code where each field points to a generated C function that:

* Looks up the `UniFFICallbackHandler` registered with `UniFFIScaffolding.registerCallbackHandler()`
* Lifts all passed arguments and passes them to the `UniFFICallbackHandler`.
* For fire-and-forget calls:
  * Calls `UniFFICallbackHandler.callAsync()` with the lifted arguments then discards the returned Promise.
  * Note: sync calls are currently always wrapped to be "fire-and-forget" callbacks
* For async calls:
  * Calls `UniFFICallbackHandler.callAsync()` with the lifted arguments getting back a Promise object.
  * Appends a [PromiseNativeHandler](https://searchfox.org/mozilla-central/source/dom/promise/PromiseNativeHandler.h) to promise object.
  * The `PromiseNativeHandler` completes the promise by calling the complete callback as described in the [UniFFI FFI internals doc](https://mozilla.github.io/uniffi-rs/latest/internals/async-ffi.html#completing-async-methods-with-complete_func).
  * The `PromiseNativeHandler` also has code to handle a rejected promise by calling the complete callback with `RustCallStatusCode::UnexpectedError`.

## Freeing Callback Interface Objects

Each VTable also has a `uniffi_free` method.
When the Rust code drops the callback interface object, the generated UniFFI code arranges for `uniffi_free` to be called.
When this happens, the C++ generated function calls `UniFFICallbackHandler.destroy()`.
The generated JavaScript handles that by removing the entry from the callback interface map.
