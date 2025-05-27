/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { funcWithDefault, funcWithMultiWordArg, ComplexMethods } =
  ChromeUtils.importESModule(
    "moz-src:///toolkit/components/uniffi-bindgen-gecko-js/tests/generated/RustUniffiBindingsTests.sys.mjs"
  );

const complexMethods = ComplexMethods.init();
Assert.stringMatches(
  complexMethods.methodWithMultiWordArg.toString(),
  /theArgument/
);

Assert.equal(complexMethods.methodWithDefault(), "DEFAULT");
Assert.equal(complexMethods.methodWithDefault("NON-DEFAULT"), "NON-DEFAULT");

Assert.equal(funcWithDefault(), "DEFAULT");
Assert.equal(funcWithDefault("NON-DEFAULT"), "NON-DEFAULT");

// Test that argument names are in camelCase
Assert.stringMatches(funcWithMultiWordArg.toString(), /theArgument/);
