/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {
  invokeTestCallbackInterfaceNoop,
  invokeTestCallbackInterfaceSetValue,
  UniffiSkipJsTypeCheck,
  UnitTestObjs,
} = ChromeUtils.importESModule(
  "moz-src:///toolkit/components/uniffi-bindgen-gecko-js/tests/generated/RustUniffiBindingsTests.sys.mjs"
);

/**
 *
 */
class Callback {
  constructor(value) {
    this.value = value;
  }

  noop() {}

  getValue() {
    return this.value;
  }

  setValue(value) {
    this.value = value;
  }
}

add_task(async () => {
  const cbi = new Callback(42);
  // Call the noop method, wait a while and make sure it doesn't crash
  invokeTestCallbackInterfaceNoop(cbi);
  do_test_pending();
  do_timeout(100, do_test_finished);
});

add_task(async () => {
  const cbi = new Callback(42);
  // Call the setValue method, wait a while and make that it went into effect
  invokeTestCallbackInterfaceSetValue(cbi, 43);
  do_test_pending();
  do_timeout(100, async () => {
    Assert.equal(await cbi.getValue(), 43);
    do_test_finished();
  });
});

// We can't test other functionality like return values/exceptions since we always wrap sync methods
// to be fire-and-forget.

// Test that if we fail to lower all arguments, we don't leave a callback interface handle left in
// the handle map.
add_task(async function testCleanupAfterFailedLower() {
  const cbi = new Callback(42);
  Assert.equal(
    UnitTestObjs.uniffiCallbackHandlerTestCallbackInterface.hasRegisteredCallbacks(),
    false
  );
  // Call `invokeTestCallbackInterfaceSetValue` with an invalid second argument.
  // We hack things in the pipeline code to skip the JS type checks, so both arguments are passed to
  // C++.
  // The test is if the C++ code cleans up afterwards and frees the handle to the callback interface
  const invalidU32Value = 2 ** 48;
  invokeTestCallbackInterfaceSetValue(cbi, invalidU32Value)
    // Errors are expected
    .catch(() => null);
  Assert.equal(
    UnitTestObjs.uniffiCallbackHandlerTestCallbackInterface.hasRegisteredCallbacks(),
    false
  );
});

// Similar test as `testCleanupAfterFailedLower`, however this one hacks things so that we lower the
// arguments to C++.
add_task(async function testCleanupAfterFailedCppLower() {
  const cbi = new Callback(42);
  Assert.equal(
    UnitTestObjs.uniffiCallbackHandlerTestCallbackInterface.hasRegisteredCallbacks(),
    false
  );
  // Call `invokeTestCallbackInterfaceSetValue` with an invalid second argument.
  // We hack things in the pipeline code to skip the JS type checks, so both arguments are passed to
  // C++.
  // The test is if the C++ code cleans up afterwards and frees the handle to the callback interface
  const invalidU32Value = 2 ** 48;
  invokeTestCallbackInterfaceSetValue(
    cbi,
    new UniffiSkipJsTypeCheck(invalidU32Value)
  )
    // Errors are expected
    .catch(() => null);
  // Cleanup happens in a scheduled call, so wait a bit before checking
  do_test_pending();
  do_timeout(100, () => {
    Assert.equal(
      UnitTestObjs.uniffiCallbackHandlerTestCallbackInterface.hasRegisteredCallbacks(),
      false
    );
    do_test_finished();
  });
});
