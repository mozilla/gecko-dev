/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { testFunc } = ChromeUtils.importESModule(
  "resource://gre/modules/RustUniffiBindingsTests.sys.mjs"
);

// Can we call a Rust function without throwing?
testFunc();
