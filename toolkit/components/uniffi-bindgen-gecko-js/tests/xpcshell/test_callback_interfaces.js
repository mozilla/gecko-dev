/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { invokeTestCallbackInterfaceMethod } = ChromeUtils.importESModule(
  "moz-src:///toolkit/components/uniffi-bindgen-gecko-js/tests/generated/RustUniffiBindingsTests.sys.mjs"
);

// Test fire-and-forget callbacks interfaces.  This schedule a callack interface call, but don't
// wait for a return.  This is all we can currently test

add_task(async function testFireAndForgetCallbackInterfaces() {
  /**
   *
   */
  class Callback {
    constructor(value) {
      this.value = value;
      this.getValueCount = 0;
    }

    getValue() {
      this.getValueCount += 1;
      return this.value;
    }
  }

  const cbi = new Callback(42);
  Assert.equal(cbi.getValueCount, 0);
  invokeTestCallbackInterfaceMethod(cbi);
  do_test_pending();
  await do_timeout(100, () => {
    Assert.equal(cbi.getValueCount, 1);
    do_test_finished();
  });
});
