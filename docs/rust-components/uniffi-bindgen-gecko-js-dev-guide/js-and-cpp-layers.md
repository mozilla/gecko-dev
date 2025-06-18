# The JS and C++ layers

`uniffi-bindgen-gecko-js` is split into 2 layers: JavaScript and C++.
The main reason for this is that JavaScript doesn't have a way of calling the Rust FFI functions.
On Kotlin we can use JNA and on Python we can use `ctypes`, but there's nothing equivalent in JavaScript.
(technically there is the `js-ctypes` library however this has been deprecated for a long time).

The C++ layer provides the JavaScript layer with a way to call the Rust FFI functions.
This can be thought of as a `ctypes` library specialized for UniFFI.
In many ways it does less than `ctypes`, for example can only call UniFFI-generated FFI functions.
In some ways however it does more, for example much of the async logic is built in to the C++ code.

## UniFFI.webidl and WebIDL code generation

The interface between the C++ and JavaScript code is defined in [UniFFI.webidl](https://searchfox.org/mozilla-central/source/dom/chrome-webidl/UniFFI.webidl).
This brings a second system of code generation, which auto-generates the C++ glue code needed to expose this interface to JavaScript.
See [Web IDL bindings](https://firefox-source-docs.mozilla.org/dom/webIdlBindings/index.html) for details.

Two different code-generators can be a bit mind-bending, but the high-level view is relatively simple.
The C++ code needs to implement the interface defined in `UniFFI.webidl`.
The JavaScript code uses that interface to handle the low-level FFI details.

The JavaScript code mostly looks like normal UniFFI bindings, while the C++ code is very unique to `uniffi-bindgen-gecko-js`.
