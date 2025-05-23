/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {
  funcWithError,
  funcWithFlatError,
  Failure1,
  Failure2,
  IoError,
  TestError,
  TestFlatError,
} = ChromeUtils.importESModule(
  "moz-src:///toolkit/components/uniffi-bindgen-gecko-js/tests/generated/RustUniffiBindingsTests.sys.mjs"
);

// Check the error hierarchies
Assert.ok(Failure1.prototype instanceof TestError);
Assert.ok(Failure2.prototype instanceof TestError);
Assert.ok(IoError.prototype instanceof TestFlatError);

// `funcWithError` throws when 0 or 1 is passed to it
Assert.throws(
  () => {
    funcWithError(0);
  },
  e => e instanceof Failure1
);

Assert.throws(
  () => {
    funcWithError(1);
  },
  e => e instanceof Failure2 && e.data == "DATA"
);

funcWithError(2);

// `funcWithflatError` throws when 0 is passed to it
Assert.throws(
  () => {
    funcWithFlatError(0);
  },
  e => e instanceof IoError
);

funcWithFlatError(1);
