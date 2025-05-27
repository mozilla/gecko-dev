/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {
  roundtripU8,
  roundtripI8,
  roundtripU16,
  roundtripI16,
  roundtripU32,
  roundtripI32,
  roundtripU64,
  roundtripI64,
  roundtripF32,
  roundtripF64,
  roundtripBool,
  roundtripString,
  sumWithManyTypes,
} = ChromeUtils.importESModule(
  "moz-src:///toolkit/components/uniffi-bindgen-gecko-js/tests/generated/RustUniffiBindingsTests.sys.mjs"
);

// Test calling and returning a single argument
Assert.equal(roundtripU8(42), 42);
Assert.equal(roundtripI8(-42), -42);
Assert.equal(roundtripU16(42), 42);
Assert.equal(roundtripI16(-42), -42);
Assert.equal(roundtripU32(42), 42);
Assert.equal(roundtripI32(-42), -42);
Assert.equal(roundtripU64(42), 42);
Assert.equal(roundtripI64(-42), -42);
Assert.equal(roundtripF32(0.5), 0.5);
Assert.equal(roundtripF64(-3.5), -3.5);
Assert.equal(roundtripBool(true), true);
Assert.equal(roundtripString("ABC"), "ABC");
// Test calling a function with lots of args
// This function will sum up all the numbers, then negate the value since we passed in `true`
Assert.equal(sumWithManyTypes(1, -2, 3, -4, 5, -6, 7, -8, 9.5, -10.5, true), 5);
