/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Interface for making UniFFI scaffolding calls
//
// Gecko uses UniFFI to generate privileged JS bindings for Rust components.
// UniFFI defines a C-ABI FFI layer for calling into Rust, called the
// scaffolding. This interface is a bridge that allows the JS code to make
// scaffolding calls
//
// See https://mozilla.github.io/uniffi-rs/ for details.

// Define some ID types to identify various parts of the UDL interfaces.  Using
// these IDs, allow this .webidl file to remain static, but support new
// interfaces.  Using IDs is that the C++ and JS code need to agree on their
// meaning, which is handled by
// toolkit/components/uniffi-bindgen-gecko-js/src/ci_list.rs.

// Identifies a scaffolding function.
typedef unsigned long long UniFFIFunctionId;

// Identifies a pointer type
typedef unsigned long long UniFFIPointerId;

// Identifies a callback interface
typedef unsigned long UniFFICallbackInterfaceId;

// Handle for a callback interface instance
typedef unsigned long long UniFFICallbackObjectHandle;

// Opaque type used to represent a pointer from Rust
[ChromeOnly, Exposed=Window]
interface UniFFIPointer { };

// Types that can be passed or returned from scaffolding functions
//
// - double is used for all numeric types and types which the JS code coerces
//   to an int including Boolean and CallbackInterface.
// - ArrayBuffer is used for RustBuffer
// - UniFFIPointer is used for Arc pointers
typedef (double or ArrayBuffer or UniFFIPointer) UniFFIScaffoldingValue;

// The result of a call into UniFFI scaffolding call
enum UniFFIScaffoldingCallCode {
   "success",         // Successful return
   "error",           // Rust Err return
   "internal-error",  // Internal/unexpected error
};

dictionary UniFFIScaffoldingCallResult {
    required UniFFIScaffoldingCallCode code;
    // For success, this will be the return value for non-void returns
    // For error, this will be an ArrayBuffer storing the serialized error value
    UniFFIScaffoldingValue data;
};

// JS handler for a callback interface
//
// These are responsible for invoking callback interface calls.  Internally, these map handles to
// objects that implement the callback interface.
//
// Before the JS code returns a callback-interface-implementing object Rust, it first sends the
// object to a UniFFICallbackHandler, which adds an entry in the map.  The handle is then what's
// sent to Rust.
//
// When the Rust code wants to invoke a method, it calls into the C++ layer and passes the handle
// along with all arguments.  The C++ layer then calls `UniFFICallbackHandler.call()` which then
// looks up the object in the map and invokes the actual method.
//
// Finally, when the Rust code frees the object, it calls into the C++ layer, which then calls
// `UniFFICallbackHandler.release()` to remove the entry in the map.
[Exposed=Window]
callback interface UniFFICallbackHandler {
    UniFFIScaffoldingValue? call(UniFFICallbackObjectHandle objectHandle, unsigned long methodIndex, UniFFIScaffoldingValue... args);
    undefined destroy(UniFFICallbackObjectHandle objectHandle);
};

// Functions to facilitate UniFFI scaffolding calls
[ChromeOnly, Exposed=Window]
namespace UniFFIScaffolding {
  // Call a sync Rust function
  //
  // id is a unique identifier for the function, known to both the C++ and JS code
  [Throws]
  UniFFIScaffoldingCallResult callSync(UniFFIFunctionId id, UniFFIScaffoldingValue... args);

  // Call a sync Rust function, but wrap it to so that it behaves in JS as an async function
  //
  // id is a unique identifier for the function, known to both the C++ and JS code
  [Throws]
  Promise<UniFFIScaffoldingCallResult> callAsyncWrapper(UniFFIFunctionId id, UniFFIScaffoldingValue... args);


  // Read a UniFFIPointer from an ArrayBuffer
  //
  // id is a unique identifier for the pointer type, known to both the C++ and JS code
  [Throws]
  UniFFIPointer readPointer(UniFFIPointerId id, ArrayBuffer buff, long position);

  // Write a UniFFIPointer to an ArrayBuffer
  //
  // id is a unique identifier for the pointer type, known to both the C++ and JS code
  [Throws]
  undefined writePointer(UniFFIPointerId id, UniFFIPointer ptr, ArrayBuffer buff, long position);

  // Register the global calblack handler
  //
  // This will be used to invoke all calls for a CallbackInterface.
  // interfaceId is a unique identifier for the callback interface, known to both the C++ and JS code
  [Throws]
  undefined registerCallbackHandler(UniFFICallbackInterfaceId interfaceId, UniFFICallbackHandler handler);

  // Deregister the global calblack handler
  //
  // This is called at shutdown to clear out the reference to the JS function.
  [Throws]
  undefined deregisterCallbackHandler(UniFFICallbackInterfaceId interfaceId);
};
