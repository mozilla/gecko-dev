"use strict";

/* globals browser */

const ORIG_UA = navigator.userAgent;
const ORIG_UA_VER = ORIG_UA.match("Firefox/((\d|\.)+)")[1];

let UA = ORIG_UA;

function shimUA() {
  Object.defineProperty(navigator.__proto__, "userAgent", {
    configurable: true,
    get: () => UA,
  });
}

const WEBKIT = "AppleWebKit/537.36 (KHTML, like Gecko)";
const SAFARI = " Safari/537.36";

const PREFIX_WIN = "Mozilla/5.0 (Windows NT 10.0; Win64; x64)";
const PREFIX_LIN = "Mozilla/5.0 (X11; Ubuntu; Linux x86_64)";
const PREFIX_MAC = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7)";
const PREFIX_AND = "Mozilla/5.0 (Linux; Android 6.0; Nexus 7 Build/JSS15Q)";

const PHONE = "Nexus 5 Build/MRA58N";
const TABLET = "Nexus 7 Build/JSS15Q";

const DEVICE_APPROPRIATE_TESTS = [
  // test that the OS is carried over if none is specified in the config
  {
    ua: "Linux",
    config: { noFxQuantum: true },
    expected: `${PREFIX_LIN} ${WEBKIT} Chrome/130.0.0.0${SAFARI}`,
  },
  {
    ua: "Android",
    config: { noFxQuantum: true },
    expected: `${PREFIX_AND} ${WEBKIT} Chrome/130.0.0.0${SAFARI}`,
  },
  {
    ua: "Macintosh",
    config: { noFxQuantum: true },
    expected: `${PREFIX_MAC} ${WEBKIT} Chrome/130.0.0.0${SAFARI}`,
  },

  // test OS overrides
  {
    ua: "Windows",
    config: { OS: "android", noFxQuantum: true },
    expected: `${PREFIX_AND} ${WEBKIT} Chrome/130.0.0.0${SAFARI}`,
  },
  {
    ua: "Windows",
    config: { OS: "linux", noFxQuantum: true },
    expected: `${PREFIX_LIN} ${WEBKIT} Chrome/130.0.0.0${SAFARI}`,
  },
  {
    ua: "Windows",
    config: { OS: "macOS", noFxQuantum: true },
    expected: `${PREFIX_MAC} ${WEBKIT} Chrome/130.0.0.0${SAFARI}`,
  },
  {
    ua: "Linux",
    config: { OS: "android", noFxQuantum: true },
    expected: `${PREFIX_AND} ${WEBKIT} Chrome/130.0.0.0${SAFARI}`,
  },
  {
    ua: "Linux",
    config: { OS: "windows", noFxQuantum: true },
    expected: `${PREFIX_WIN} ${WEBKIT} Chrome/130.0.0.0${SAFARI}`,
  },
  {
    ua: "Linux",
    config: { OS: "macOS", noFxQuantum: true },
    expected: `${PREFIX_MAC} ${WEBKIT} Chrome/130.0.0.0${SAFARI}`,
  },
  {
    ua: "Macintosh",
    config: { OS: "android", noFxQuantum: true },
    expected: `${PREFIX_AND} ${WEBKIT} Chrome/130.0.0.0${SAFARI}`,
  },
  {
    ua: "Macintosh",
    config: { OS: "windows", noFxQuantum: true },
    expected: `${PREFIX_WIN} ${WEBKIT} Chrome/130.0.0.0${SAFARI}`,
  },
  {
    ua: "Macintosh",
    config: { OS: "linux", noFxQuantum: true },
    expected: `${PREFIX_LIN} ${WEBKIT} Chrome/130.0.0.0${SAFARI}`,
  },
  {
    ua: "Android",
    config: { OS: "windows", noFxQuantum: true },
    expected: `${PREFIX_WIN} ${WEBKIT} Chrome/130.0.0.0${SAFARI}`,
  },
  {
    ua: "Android",
    config: { OS: "macOS", noFxQuantum: true },
    expected: `${PREFIX_MAC} ${WEBKIT} Chrome/130.0.0.0${SAFARI}`,
  },
  {
    ua: "Android",
    config: { OS: "linux", noFxQuantum: true },
    expected: `${PREFIX_LIN} ${WEBKIT} Chrome/130.0.0.0${SAFARI}`,
  },

  // test that if we don't know what the OS is, we just use Windows
  {
    ua: "X",
    config: {},
    expected: `${PREFIX_WIN} FxQuantum/58.0 ${WEBKIT} Chrome/130.0.0.0${SAFARI}`,
  },

  // test noFxQuantum config option
  {
    ua: "X",
    config: { noFxQuantum: false },
    expected: `${PREFIX_WIN} FxQuantum/58.0 ${WEBKIT} Chrome/130.0.0.0${SAFARI}`,
  },
  {
    ua: "X",
    config: { noFxQuantum: true },
    expected: `${PREFIX_WIN} ${WEBKIT} Chrome/130.0.0.0${SAFARI}`,
  },

  // test OS=nonLinux
  {
    ua: "Linux",
    config: { OS: "nonLinux", noFxQuantum: true },
    expected: `${PREFIX_WIN} ${WEBKIT} Chrome/130.0.0.0${SAFARI}`,
  },
  {
    ua: "Android",
    config: { OS: "nonLinux", noFxQuantum: true },
    expected: `${PREFIX_AND} ${WEBKIT} Chrome/130.0.0.0${SAFARI}`,
  },
  {
    ua: "Macintosh",
    config: { OS: "nonLinux", noFxQuantum: true },
    expected: `${PREFIX_MAC} ${WEBKIT} Chrome/130.0.0.0${SAFARI}`,
  },
  {
    ua: "Windows",
    config: { OS: "nonLinux", noFxQuantum: true },
    expected: `${PREFIX_WIN} ${WEBKIT} Chrome/130.0.0.0${SAFARI}`,
  },

  // test version config option
  {
    ua: "X",
    config: { version: "VER", noFxQuantum: true },
    expected: `${PREFIX_WIN} ${WEBKIT} Chrome/VER${SAFARI}`,
  },

  // test a desktop OS spoofing a phone and tablet
  {
    ua: "Windows",
    config: { OS: "android", phone: "PHONE", noFxQuantum: true },
    expected: `Mozilla/5.0 (Linux; Android 6.0; PHONE) ${WEBKIT} Chrome/130.0.0.0 Mobile${SAFARI}`,
  },
  {
    ua: "Windows",
    config: { OS: "android", tablet: "TABLET", noFxQuantum: true },
    expected: `Mozilla/5.0 (Linux; Android 6.0; TABLET) ${WEBKIT} Chrome/130.0.0.0${SAFARI}`,
  },

  // test an android phone spoofing a tablet and vice versa
  {
    ua: "Android 8.8.8 Mobile",
    config: { noFxQuantum: true, tablet: "TABLET" },
    expected: `Mozilla/5.0 (Linux; Android 8.8.8; TABLET) ${WEBKIT} Chrome/130.0.0.0${SAFARI}`,
  },
  {
    ua: "Android 8.8.8 Mobile",
    config: { noFxQuantum: true, tablet: true },
    expected: `Mozilla/5.0 (Linux; Android 8.8.8; ${TABLET}) ${WEBKIT} Chrome/130.0.0.0${SAFARI}`,
  },
  {
    ua: "Android 8.8.8",
    config: { noFxQuantum: true, phone: "PHONE" },
    expected: `Mozilla/5.0 (Linux; Android 8.8.8; PHONE) ${WEBKIT} Chrome/130.0.0.0 Mobile${SAFARI}`,
  },
  {
    ua: "Android 8.8.8",
    config: { noFxQuantum: true, phone: true },
    expected: `Mozilla/5.0 (Linux; Android 8.8.8; ${PHONE}) ${WEBKIT} Chrome/130.0.0.0 Mobile${SAFARI}`,
  },

  // test that accidentally spoofing both phone and tablet just picks a phone
  {
    ua: "Android 8.8.8",
    config: { noFxQuantum: true, phone: true, tablet: true },
    expected: `Mozilla/5.0 (Linux; Android 8.8.8; ${PHONE}) ${WEBKIT} Chrome/130.0.0.0 Mobile${SAFARI}`,
  },

  // test android version number option
  {
    ua: "Android 5.0 Mobile",
    config: { OS: "android", androidVersion: "VER", noFxQuantum: true },
    expected: `Mozilla/5.0 (Linux; Android VER; ${PHONE}) ${WEBKIT} Chrome/130.0.0.0 Mobile${SAFARI}`,
  },
  {
    ua: "Android 5.0",
    config: { OS: "android", androidVersion: "VER", noFxQuantum: true },
    expected: `Mozilla/5.0 (Linux; Android VER; ${TABLET}) ${WEBKIT} Chrome/130.0.0.0${SAFARI}`,
  },

  // test android version numbers are detected if not given
  {
    ua: "Android 8.8.8",
    config: { OS: "android", phone: "DEV", noFxQuantum: true },
    expected: `Mozilla/5.0 (Linux; Android 8.8.8; DEV) ${WEBKIT} Chrome/130.0.0.0 Mobile${SAFARI}`,
  },
  {
    ua: "Android 8.8.8 (tablet)",
    config: { OS: "android", noFxQuantum: true },
    expected: `Mozilla/5.0 (Linux; Android 8.8.8; ${TABLET}) ${WEBKIT} Chrome/130.0.0.0${SAFARI}`,
  },
];

const TESTS = {
  addChrome(helper) {
    UA = "X";
    is(helper(), `X ${WEBKIT} Chrome/130.0.0.0 Safari/537.36`);
    is(helper(UA, "VER"), `X ${WEBKIT} Chrome/VER Safari/537.36`);

    UA = "X Mobile";
    is(helper(), `X Mobile ${WEBKIT} Chrome/130.0.0.0 Mobile Safari/537.36`);
    is(helper(UA, "VER"), `X Mobile ${WEBKIT} Chrome/VER Mobile Safari/537.36`);

    UA = "X Tablet";
    is(helper(), `X Tablet ${WEBKIT} Chrome/130.0.0.0 Mobile Safari/537.36`);
    is(helper(UA, "VER"), `X Tablet ${WEBKIT} Chrome/VER Mobile Safari/537.36`);
  },
  addGecko(helper) {
    is(helper(), `${ORIG_UA} Gecko/${ORIG_UA_VER}`);
    is(helper("X"), `X Gecko/${ORIG_UA_VER}`);
    is(helper("X", "13"), "X Gecko/13");

    UA = "Firefox/13.0.0.0";
    is(helper(), `${UA} Gecko/13.0.0.0`);

    UA = "X";
    is(helper(), `X Gecko/58.0`);
    is(helper(undefined, "13"), "X Gecko/13");
  },
  addSamsungForSamsungDevices(helper) {
    is(helper(), ORIG_UA);

    UA = "X";
    let MANUFACTURER = "X";
    browser.systemManufacturer = {
      getManufacturer: () => MANUFACTURER,
    };
    is(helper(), UA);

    UA = "X";
    MANUFACTURER = "Samsung";
    is(helper(), UA);

    UA = "X Mobile; Y";
    is(helper(), "X Mobile; Samsung; Y");

    delete browser.systemManufacturer;
  },
  androidHotspot2Device(helper) {
    is(helper("X"), "X");
    is(helper("X (Y) Z"), "X (Linux; Android 10; K) Z");
  },
  capRvTo109(helper) {
    is(helper("X"), "X");
    is(helper("X (rv:-13.0)"), "X (rv:-13.0)");
    is(helper("X (rv:109)"), "X (rv:109)");
    is(helper("X (rv:110)"), "X (rv:110)");
    is(helper("X (rv:109.1)"), "X (rv:109.0)");
    is(helper("X (rv:109.10)"), "X (rv:109.0)");
    is(helper("X (rv:110.0)"), "X (rv:109.0)");
  },
  capVersionTo99(helper) {
    is(helper("X"), "X");
    is(helper("X Firefox/99 Y"), "X Firefox/99 Y");
    is(helper("X Firefox/100 Y"), "X Firefox/100 Y");
    is(helper("X Firefox/99.1 Y"), "X Firefox/99.1 Y");
    is(helper("X Firefox/99.100 Y"), "X Firefox/99.100 Y");
    is(helper("X Firefox/100.1 Y"), "X Firefox/99.0 Y");
    is(helper("X Firefox/199.99 Y"), "X Firefox/99.0 Y");
  },
  capVersionToNumber(helper) {
    is(helper("X"), "X");

    is(helper("X Firefox/199 Y"), "X Firefox/199 Y");
    is(helper("X Firefox/120 Y"), "X Firefox/120 Y");
    is(helper("X Firefox/119.1 Y"), "X Firefox/119.1 Y");
    is(helper("X Firefox/119.120 Y"), "X Firefox/119.120 Y");
    is(helper("X Firefox/120.1 Y"), "X Firefox/120.0 Y");
    is(helper("X Firefox/199.99 Y"), "X Firefox/120.0 Y");

    is(helper("X Firefox/99 Y", 99), "X Firefox/99 Y");
    is(helper("X Firefox/100 Y", 99), "X Firefox/100 Y");
    is(helper("X Firefox/99.1 Y", 99), "X Firefox/99.0 Y");
    is(helper("X Firefox/99.100 Y", 99), "X Firefox/99.0 Y");
    is(helper("X Firefox/100.1 Y", 99), "X Firefox/99.0 Y");
    is(helper("X Firefox/199.99 Y", 99), "X Firefox/99.0 Y");
  },
  changeFirefoxToFireFox(helper) {
    UA = "X firefox Y";
    is(helper(), UA);

    UA = "X Firefox Y";
    is(helper(), "X FireFox Y");

    is(helper("X firefox Y"), "X firefox Y");
    is(helper("X Firefox Y"), "X FireFox Y");
  },
  desktopUA(helper) {
    UA = "X Mobile; Y";
    is(helper(), "X Y");

    UA = "X Tablet; Y";
    is(helper(), "X Y");

    UA = "X Mobile ; Y";
    is(helper(), "X Mobile ; Y");

    is(helper("X Mobile; Y"), "X Y");
    is(helper("X Tablet; Y"), "X Y");
    is(helper("X Mobile ; Y"), "X Mobile ; Y");
  },
  getDeviceAppropriateChromeUA(helper) {
    for (const test of DEVICE_APPROPRIATE_TESTS) {
      const { config, expected, ua } = test;
      UA = ua ?? UA;
      is(
        helper(Object.assign({ noCache: true }, config)),
        expected,
        `getDeviceAppropriateChromeUA(${JSON.stringify(test)})`
      );
    }
  },
  getFxQuantumSegment(helper) {
    is(helper(), `FxQuantum/${ORIG_UA_VER} `);

    UA = "Firefox/13.0.0.0";
    is(helper(), `FxQuantum/13.0.0.0 `);

    UA = "X";
    is(helper(), `FxQuantum/58.0 `);
  },
  getMacOSXUA(helper) {
    is(helper("X () Y"), "X () Y");
    is(helper("X (x) Y"), "X (Macintosh; Intel Mac OS X 10.15) Y");
    is(helper("X (x) Y", "ARCH", "VER"), "X (Macintosh; ARCH Mac OS X VER) Y");
  },
  getPrefix(helper) {
    is(helper("X () Y () Z"), "X ()");
  },
  getRunningFirefoxVersion(helper) {
    is(helper(), ORIG_UA_VER);
  },
  getWindowsUA(helper) {
    is(
      helper("X rv:1.1 Z Firefox/1.2 Y"),
      "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:1.1) Gecko/20100101 Firefox/1.2"
    );
  },
  overrideWithDeviceAppropriateChromeUA(helper) {
    navigator.wrappedJSObject = navigator;
    window.exportFunction = x => x;
    for (const test of DEVICE_APPROPRIATE_TESTS) {
      const { config, expected, ua } = test;
      UA = ua ?? UA;
      helper(Object.assign({ noCache: true }, config));
      is(
        navigator.userAgent,
        expected,
        `overrideWithDeviceAppropriateChromeUA(${JSON.stringify(test)})`
      );
      shimUA();
    }
    delete window.exportFunction;
  },
  safari(helper) {
    UA = "Firefox/1.0";
    is(
      helper(),
      "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) Firefox/1.0 AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.1 Safari/605.1.15"
    );
    is(
      helper({ osVersion: "1.2", version: "VER", webkitVersion: "WKVER" }),
      "Mozilla/5.0 (Macintosh; Intel Mac OS X 1_2) Firefox/1.0 AppleWebKit/WKVER (KHTML, like Gecko) Version/VER Safari/WKVER"
    );
  },
  windows(helper) {
    UA = "X rv:1.1 Y";
    is(helper("X () Y"), "X () Y");
    is(helper("X (x) Y"), "X (Windows NT 10.0; Win64; x64; rv:1.1) Y");
  },
};

add_task(async function test_ua_helpers() {
  shimUA();

  const addon = await AddonManager.getAddonByID("webcompat@mozilla.org");
  const addonURI = addon.getResourceURI();

  const exports = {};
  Services.scriptloader.loadSubScript(
    addonURI.resolve("lib/ua_helpers.js"),
    exports
  );
  const helpers = exports.UAHelpers;

  for (const helper of Object.keys(helpers)) {
    if (!helper.startsWith("_")) {
      ok(helper in TESTS, `tests have been written for ${helper}`);
    }
  }

  for (const [name, helper] of Object.entries(helpers)) {
    UA = ORIG_UA;
    TESTS[name]?.(helper);
  }
});
