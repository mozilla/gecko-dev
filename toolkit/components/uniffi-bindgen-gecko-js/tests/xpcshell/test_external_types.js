/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { EnumWithData, SimpleRec, TestInterface } = ChromeUtils.importESModule(
  "moz-src:///toolkit/components/uniffi-bindgen-gecko-js/tests/generated/RustUniffiBindingsTests.sys.mjs"
);

const {
  roundtripExtCustomType,
  roundtripExtEnum,
  roundtripExtInterface,
  roundtripExtRecord,
} = ChromeUtils.importESModule(
  "moz-src:///toolkit/components/uniffi-bindgen-gecko-js/tests/generated/RustUniffiBindingsTestsExternalTypes.sys.mjs"
);

Assert.deepEqual(
  roundtripExtRecord(new SimpleRec({ a: 42 })),
  new SimpleRec({ a: 42 })
);
Assert.deepEqual(
  roundtripExtEnum(new EnumWithData.A(10)),
  new EnumWithData.A(10)
);
const interface = TestInterface.init(20);
Assert.equal(roundtripExtInterface(interface).getValue(), 20);
Assert.equal(roundtripExtCustomType(100), 100);
