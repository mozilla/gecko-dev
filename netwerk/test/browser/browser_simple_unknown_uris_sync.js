/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
});

const {
  checkInputAndSerializationMatch,
  checkInputAndSerializationMatchChild,
  checkSerializationMissingSecondColon,
  checkSerializationMissingSecondColonChild,
  removeSecondColon,
  runParentTestSuite,
} = ChromeUtils.importESModule(
  "resource://testing-common/simple_unknown_uri_helpers.sys.mjs"
);

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["network.url.useDefaultURI", true],
      ["network.url.simple_uri_unknown_schemes_enabled", true],
      ["network.url.simple_uri_unknown_schemes", "simpleprotocol,otherproto"],
    ],
  });
});

const bypassCollectionName = "url-parser-default-unknown-schemes-interventions";

let newData = [
  {
    id: "111",
    scheme: "testinitscheme",
  },
  {
    id: "112",
    scheme: "testsyncscheme",
  },
];

// sync update, test on parent
add_task(async function test_bypass_list_update_sync_parent() {
  const settings = await RemoteSettings(bypassCollectionName);
  let stub = sinon.stub(settings, "get").returns(newData);
  registerCleanupFunction(async function () {
    stub.restore();
  });

  await RemoteSettings(bypassCollectionName).emit("sync", {});

  runParentTestSuite();

  stub.restore();
});

// sync update, test on child
add_task(async function test_bypass_list_update_sync_child() {
  const settings = await RemoteSettings(bypassCollectionName);
  let stub = sinon.stub(settings, "get").returns(newData);
  registerCleanupFunction(async function () {
    stub.restore();
  });

  const URL_EXAMPLE = "https://example.com";
  const tab = BrowserTestUtils.addTab(gBrowser, URL_EXAMPLE);
  const browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  await RemoteSettings(bypassCollectionName).emit("sync", {});

  await SpecialPowers.spawn(
    browser,
    [
      removeSecondColon.toString(),
      checkSerializationMissingSecondColonChild.toString(),
      checkInputAndSerializationMatchChild.toString(),
    ],
    (rscSource, csmscSource, ciasmcSource) => {
      /* eslint-disable no-eval */
      // eslint-disable-next-line no-unused-vars
      let removeSecondColon = eval(`(${rscSource})`); // used by checker fns
      let checkSerializationMissingSecondColonChild = eval(`(${csmscSource})`);
      let checkInputAndSerializationMatchChild = eval(`(${ciasmcSource})`);
      /* eslint-enable no-eval */

      // sanity check
      checkInputAndSerializationMatchChild("https://example.com/");

      // nsStanardURL
      checkSerializationMissingSecondColonChild("https://https://example.com");

      // no-bypass protocol uses defaultURI
      checkSerializationMissingSecondColonChild(
        "defaulturischeme://https://example.com"
      );

      // an unknown protocol in the bypass list (remote settings) uses simpleURI
      checkInputAndSerializationMatchChild(
        "testsyncscheme://https://example.com"
      );

      // pref-specified scheme bypass uses simpleURI
      checkInputAndSerializationMatchChild(
        "simpleprotocol://https://example.com"
      );
    }
  );

  // Cleanup
  stub.restore();
  BrowserTestUtils.removeTab(tab);
});

// long string
add_task(async function test_bypass_list_update_sync_parent_long_string() {
  let longSchemeList = ["testinitscheme", "testsyncscheme"];
  let num = 100;
  for (let i = 0; i <= num; i++) {
    longSchemeList.push(`scheme${i}`);
  }

  let newData = [];
  for (const i in longSchemeList) {
    newData.push({ id: i, scheme: longSchemeList[i] });
  }

  const settings = await RemoteSettings(bypassCollectionName);
  let stub = sinon.stub(settings, "get").returns(newData);
  registerCleanupFunction(async function () {
    stub.restore();
  });

  await RemoteSettings(bypassCollectionName).emit("sync", {});

  runParentTestSuite();

  // another unknown protocol in the bypass list, near the middle of long str
  checkInputAndSerializationMatch("scheme50://https://example.com");

  // another unknown protocol in the bypass list, at the end of the long str
  checkInputAndSerializationMatch("scheme100://https://example.com");

  stub.restore();
});
