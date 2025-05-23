/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {
  roundtripEnumNoData,
  roundtripEnumWithData,
  roundtripComplexEnum,
  EnumNoData,
  EnumWithData,
  ComplexEnum,
  SimpleRec,
} = ChromeUtils.importESModule(
  "moz-src:///toolkit/components/uniffi-bindgen-gecko-js/tests/generated/RustUniffiBindingsTests.sys.mjs"
);

Assert.deepEqual(roundtripEnumNoData(EnumNoData.B), EnumNoData.B);

Assert.deepEqual(
  roundtripEnumWithData(new EnumWithData.A(10)),
  new EnumWithData.A(10)
);
Assert.deepEqual(
  roundtripEnumWithData(new EnumWithData.B("Ten")),
  new EnumWithData.B("Ten")
);
Assert.deepEqual(
  roundtripEnumWithData(new EnumWithData.C()),
  new EnumWithData.C()
);

Assert.deepEqual(
  roundtripComplexEnum(new ComplexEnum.A(EnumNoData.C)),
  new ComplexEnum.A(EnumNoData.C)
);
Assert.deepEqual(
  roundtripComplexEnum(new ComplexEnum.B(new EnumWithData.A(20))),
  new ComplexEnum.B(new EnumWithData.A(20))
);
Assert.deepEqual(
  roundtripComplexEnum(new ComplexEnum.C(new SimpleRec({ a: 30 }))),
  new ComplexEnum.C(new SimpleRec({ a: 30 }))
);
