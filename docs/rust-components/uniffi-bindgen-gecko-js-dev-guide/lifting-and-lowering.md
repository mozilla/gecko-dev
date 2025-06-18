# How uniffi-bindgen-gecko-js handles lifting and lowering

UniFFI uses "Lifting" and "Lowering" to describe the process of converting between high-level types, from both JavaScript and Rust, and low-level FFI types used in the generated FFI code.
See the [Lifting and Lowering section of the UniFFI developer's guide](https://mozilla.github.io/uniffi-rs/latest/internals/lifting_and_lowering.html)
for a background on this topic.
This document will describe how `uniffi-bindgen-gecko-js` handles lifting and lowering.

## Lifting/lowering happens in 2 phases

Because of the JavaScript/C++ split, lifting and lowering happens in 2 phases.
For the first phase, the generated JavaScript lowers the high-level type into a lower-level JavaScript type.
For the second phase, the generated C++ lowers that into the actual FFI type.

Here's a table of the types involved:

| FFI type                        | JavaScript lowered type   | Details                                               |
| --------------------------------|-------------------|-------------------------------------------------------|
| Numeric type (`u8`, `f32`, etc) | `number`          |                                                       |
| `RustBuffer`                      | `ArrayBuffer`     | See the RustBuffers and ArrayBuffers section          |
| `void*` (Rust object handle)      | `UniFFIPointer`   | See the UniFFIPointer section                         |
| Callback interface handle       | `number`          | [JavaScript Callback interfaces](./js-callback-interfaces.md) |

Examples:

* Lowering a Boolean
  * The generated JavaScript converts it to `0` or `1`.
  * The generated C++ converts the number to a `uint8_t`
* Lowering a `u16`
  * The generated JavaScript performs a bounds check (i.e. throws unless the value is in the range `[0, 65,535]`);
  * The generated JavaScript lowers the number without any type conversion.
  * The generated C++ converts the JavaScript number to a `uint16_t`.
* Rust struct
  * The generated JavaScript serializes the data into an [ArrayBuffer](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/ArrayBuffer)
  * The C++ copies that data into a `RustBuffer`

Note: in [UniFFI.webIDL](https://searchfox.org/mozilla-central/source/dom/chrome-webidl/UniFFI.webidl), functions input the `UniFFIScaffoldingValue` type which is the union of all types in the JavaScript lowered type column.

## RustBuffers and ArrayBuffers

`RustBuffer` is UniFFI's byte buffer FFI type.
The generated JavaScript doesn't deal with `RustBuffer`s directly, instead it uses an `ArrayBuffer` which is a JavaScript-native byte buffer.
The generated C++ converts between `RustBuffer` and `ArrayBuffer`.
When lifting a RustBuffer, the C++ uses `JS::NewExternalArrayBuffer` to create an array buffer that references the RustBuffer's memory.
When lowering, the generated C++ currently copies the `ArrayBuffer` bytes into a new `RustBuffer`.  Maybe in the future we can find a way to avoid this copy.

## UniFFIPointer object

The `UniFFIPointer` class is used to wrap Rust object pointers passed into JavaScript.
In the JavaScript layer this is simply an opaque type.
The C++ layer can use this to view the underlying pointer, clone it, etc.
The destructor of this class calls the FFI free function for the pointer.

## FFI Value classes

FFI values are often transferred between threads.
For example, when a Rust async function completes we need to pass the return value from an arbitrary thread and pass it to the JavaScript main thread.
The actual lifting can only happen on the JavaScript main thread, since it uses JavaScript objects.
Before that happens, we often need to manage the Rust value.
For example, if something fails before we can lift a `RustBuffer`, we still want to free the data.

The `FfiValue*` family of classes handles this.
We define one of these types for each FFI type (`FfiValueRustBuffer`, `FfiValueInt<T>` for each integer width, etc.).
Some of these are generated in the C++ bindings, for example `FfiValueMyRustObject`.
These classes share a similar API, although they don't actually share a base class, since the exacts type signatures vary.

* `Lower()`: Lower the JavaScript value and store it inside the FFI value class.
  Lower must be called from the main thread since it access JavaScript objects.
* `IntoRust()`: Take the stored value and create a raw value to pass to Rust.
* `FromRust()`: Input a raw Rust value and store it inside the FFI value class.
* `Lift()`:  Take the stored value and lift it into a JavaScript value.
  Lift must be called from the main thread since it access JavaScript objects.
* _destructor_: If the FFI value is destroyed with a live value inside of if, the FFI value class is responsible for cleanup.
  For example if an object handle is lowered, but `IntoRust` was never called, then the FFI value class will free that handle.

These classes have C++ move semantics, meaning they transition from an empty state to a non-empty state during their lifetime.
For example, `Lower()` transitions to a non-empty state, while `IntoRust()` transitions back to the empty state.
