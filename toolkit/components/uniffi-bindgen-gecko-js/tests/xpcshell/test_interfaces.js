/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { TestInterface, TwoTestInterfaces, cloneInterface, swapTestInterfaces } =
  ChromeUtils.importESModule(
    "moz-src:///toolkit/components/uniffi-bindgen-gecko-js/tests/generated/RustUniffiBindingsTests.sys.mjs"
  );

const interface = TestInterface.init(20);
Assert.equal(interface.getValue(), 20);
Assert.equal(cloneInterface(interface).getValue(), 20);

// Test records that store interfaces
//
// The goal is to test if we can read/write interface handles to RustBuffers
const two = new TwoTestInterfaces({
  first: TestInterface.init(1),
  second: TestInterface.init(2),
});
const swapped = swapTestInterfaces(two);
Assert.equal(swapped.first.getValue(), 2);
Assert.equal(swapped.second.getValue(), 1);

// Create 2 references to an interface using a bunch of intermediary objects:
//   * The one passed to `funcThatClonesInterface`
//   * The clones created for each method call

// eslint-disable-next-line no-unused-vars
const interface2 = TestInterface.init(20);
function funcThatClonesInterface(interface) {
  return cloneInterface(interface);
}
// eslint-disable-next-line no-unused-vars
const interface2Clone = funcThatClonesInterface(cloneInterface(interface));
interface.getValue();
// Run GC, then check that only the 2 actual references remain.
Cu.forceGC();
Cu.forceCC();
Assert.equal(interface.refCount(), 2);
