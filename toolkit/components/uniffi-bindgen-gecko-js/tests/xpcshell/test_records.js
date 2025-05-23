/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {
  SimpleRec,
  ComplexRec,
  RecWithDefault,
  roundtripSimpleRec,
  roundtripComplexRec,
} = ChromeUtils.importESModule(
  "moz-src:///toolkit/components/uniffi-bindgen-gecko-js/tests/generated/RustUniffiBindingsTests.sys.mjs"
);

// roundtripSimpleRec is configured to use the async wrapped call type
add_task(async function testRoundtripSimpleRec() {
  Assert.deepEqual(
    await roundtripSimpleRec(new SimpleRec({ a: 42 })),
    new SimpleRec({ a: 42 })
  );
});
Assert.deepEqual(new RecWithDefault().a, 42);
Assert.deepEqual(new RecWithDefault({}).a, 42);
Assert.deepEqual(new RecWithDefault({ a: 10 }).a, 10);
Assert.deepEqual(
  roundtripComplexRec(
    new ComplexRec({
      fieldU8: 0,
      fieldI8: -1,
      fieldU16: 2,
      fieldI16: -3,
      fieldU32: 4,
      fieldI32: -5,
      fieldU64: 6,
      fieldI64: -7,
      fieldF32: 8.5,
      fieldF64: 9.5,
      fieldRec: new SimpleRec({ a: 42 }),
    })
  ),
  new ComplexRec({
    fieldU8: 0,
    fieldI8: -1,
    fieldU16: 2,
    fieldI16: -3,
    fieldU32: 4,
    fieldI32: -5,
    fieldU64: 6,
    fieldI64: -7,
    fieldF32: 8.5,
    fieldF64: 9.5,
    fieldString: "DefaultString",
    fieldRec: new SimpleRec({ a: 42 }),
  })
);
