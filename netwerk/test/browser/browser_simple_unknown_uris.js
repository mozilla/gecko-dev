/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.defineESModuleGetters(this, {
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
});

const {
  checkInputAndSerializationMatch,
  checkInputAndSerializationMatchChild,
  checkSerializationMissingSecondColon,
  checkSerializationMissingSecondColonChild,
  removeSecondColon,
} = ChromeUtils.importESModule(
  "resource://testing-common/simple_unknown_uri_helpers.sys.mjs"
);

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["network.url.useDefaultURI", true],
      ["network.url.simple_uri_unknown_schemes_enabled", true],
      ["network.url.simple_uri_unknown_schemes", "simpleprotocol,otherproto"],
    ],
  });
});

add_task(async function test_bypass_remote_settings_static_parent() {
  // sanity check
  checkInputAndSerializationMatch("https://example.com/");

  // nsStandardURL removes second colon when nesting protocols
  checkSerializationMissingSecondColon("https://https://example.com/");

  // no-bypass unknown protocol uses defaultURI
  checkSerializationMissingSecondColon(
    "nonsimpleprotocol://https://example.com"
  );

  // simpleURI keeps the second colon
  // an unknown protocol in the bypass list will use simpleURI
  // despite network.url.useDefaultURI being enabled
  let same = "simpleprotocol://https://example.com";
  checkInputAndSerializationMatch(same);

  // scheme bypass from static remote-settings
  checkInputAndSerializationMatch("ed2k://https://example.com");

  // check the pref-specified scheme again (remote settings shouldn't overwrite)
  checkInputAndSerializationMatch(same);
});

add_task(async function test_bypass_remote_settings_static_child() {
  const URL_EXAMPLE = "https://example.com";
  const tab = BrowserTestUtils.addTab(gBrowser, URL_EXAMPLE);
  const browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

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
      let removeSecondColon = eval(`(${rscSource})`); // used by check fns
      let checkSerializationMissingSecondColonChild = eval(`(${csmscSource})`);
      let checkInputAndSerializationMatchChild = eval(`(${ciasmcSource})`);
      /* eslint-enable no-eval */

      checkInputAndSerializationMatchChild("https://example.com/");

      // nsStandardURL removes second colon when nesting protocols
      checkSerializationMissingSecondColonChild("https://https://example.com");

      // no-bypass protocol uses defaultURI
      checkSerializationMissingSecondColonChild(
        "nonsimpleprotocol://https://example.com"
      );

      // simpleURI keeps the second colon
      // an unknown protocol in the bypass list will use simpleURI
      // despite network.url.useDefaultURI being enabled
      let same = "simpleprotocol://https://example.com";
      checkInputAndSerializationMatchChild(same);

      // scheme bypass from static remote-settings
      checkInputAndSerializationMatchChild("ed2k://https://example.com");

      // pref-specified scheme shouldn't be overwritten by remote settings schemes
      checkInputAndSerializationMatchChild(same);
    }
  );

  // Cleanup
  BrowserTestUtils.removeTab(tab);
  Services.cookies.removeAll();
});
