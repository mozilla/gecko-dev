/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {
  roundtripOption,
  roundtripVec,
  roundtripHashMap,
  roundtripComplexCompound,
} = ChromeUtils.importESModule(
  "moz-src:///toolkit/components/uniffi-bindgen-gecko-js/tests/generated/RustUniffiBindingsTests.sys.mjs"
);

Assert.equal(roundtripOption(42), 42);
Assert.equal(roundtripOption(null), null);
Assert.deepEqual(roundtripVec([1, 2, 3]), [1, 2, 3]);
Assert.deepEqual(
  roundtripHashMap(
    new Map([
      ["a", 1],
      ["b", 2],
    ])
  ),
  new Map([
    ["a", 1],
    ["b", 2],
  ])
);
Assert.deepEqual(
  roundtripComplexCompound([
    new Map([
      ["a", 1],
      ["b", 2],
    ]),
  ]),
  [
    new Map([
      ["a", 1],
      ["b", 2],
    ]),
  ]
);
Assert.equal(roundtripComplexCompound(null), null);
