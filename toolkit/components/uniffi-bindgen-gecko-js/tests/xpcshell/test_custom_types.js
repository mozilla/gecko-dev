/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { roundtripCustomType } = ChromeUtils.importESModule(
  "moz-src:///toolkit/components/uniffi-bindgen-gecko-js/tests/generated/RustUniffiBindingsTests.sys.mjs"
);

// The custom type here is a Handle, that is converted to an int on the JS side
Assert.equal(roundtripCustomType(100), 100);

// TODO: implement the other half of this -- allow the int to be wrapped into a Handle on the JS
// side.
