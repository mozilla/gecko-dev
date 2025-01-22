"use strict";

// This is a test script similar to those used by ExtensionAPIs.
// https://searchfox.org/mozilla-central/source/toolkit/components/extensions/parent

let module3 = ChromeUtils.importESModule("resource://test/esmified-3.sys.mjs");
let module4 = ChromeUtils.importESModule("resource://test/esmified-4.sys.mjs");

injected3.obj.value += 3;
module3.obj.value += 3;
module4.obj.value += 4;

this.testResults = {
  injected3: injected3.obj.value,
  module3: module3.obj.value,
  sameInstance3: injected3 === module3,
  module4: module4.obj.value,
};
