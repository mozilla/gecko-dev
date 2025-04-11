# uniffi-js

This directory contains C++ helper code for the UniFFI Rust library
(https://github.com/mozilla/uniffi-rs/).

 - `UniFFIPointer.*` and `UniFFIPointerType.*` implement the `UniFFIPointer` WebIDL class

 - `UniFFI*Scaffolding.cpp` implements the `UniFFIScaffolding` WebIDL class.
   - UniFFIGeneratedScaffolding.cpp contains the generated C++ code
   - UniFFIScaffolding.cpp is a facade that wraps UniFFIFixtureScaffolding, and
     UniFFIGeneratedScaffolding if enabled, to implement the interface.

- `UniFFIFfiConverter.h` Lifts/Lowers JS values into FFI values.

- `OwnedRustBuffer.*` implements a C++ class to help manager ownership of a RustBuffer.

- `UniFFIRust.h` contains definitions for the C functions that UniFFI exports.
