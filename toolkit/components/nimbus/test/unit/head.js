"use strict";

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const { ExperimentFakes, ExperimentTestUtils, NimbusTestUtils } =
  ChromeUtils.importESModule(
    "resource://testing-common/NimbusTestUtils.sys.mjs"
  );

ChromeUtils.defineESModuleGetters(this, {
  ObjectUtils: "resource://gre/modules/ObjectUtils.sys.mjs",
  RegionTestUtils: "resource://testing-common/RegionTestUtils.sys.mjs",
});

NimbusTestUtils.init(this);

function assertEmptyStore(store) {
  NimbusTestUtils.assert.storeIsEmpty(store);
}
