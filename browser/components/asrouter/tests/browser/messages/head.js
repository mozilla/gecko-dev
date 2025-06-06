"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ASRouter: "resource:///modules/asrouter/ASRouter.sys.mjs",
  AboutWelcomeParent: "resource:///actors/AboutWelcomeParent.sys.mjs",
});

// We import sinon here to make it available across all mochitest test files
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
