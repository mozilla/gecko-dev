/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals browser, exportFunction, module */

var UAHelpers = {
  _deviceAppropriateChromeUAs: {},
  getRunningFirefoxVersion() {
    return (navigator.userAgent.match(/Firefox\/([0-9.]+)/) || ["", "58.0"])[1];
  },
  getFxQuantumSegment() {
    return `FxQuantum/${UAHelpers.getRunningFirefoxVersion()} `;
  },
  getDeviceAppropriateChromeUA(config = {}) {
    const { version = "130.0.0.0", androidDevice, desktopOS } = config;
    const key = `${version}:${androidDevice}:${desktopOS}`;
    if (!UAHelpers._deviceAppropriateChromeUAs[key]) {
      const userAgent =
        config.ua ||
        (typeof navigator !== "undefined" ? navigator.userAgent : "");
      const fxQuantum = config.noFxQuantum
        ? ""
        : UAHelpers.getFxQuantumSegment();
      if (userAgent.includes("Android")) {
        const RunningAndroidVersion =
          userAgent.match(/Android [0-9.]+/) || "Android 6.0";
        if (androidDevice) {
          UAHelpers._deviceAppropriateChromeUAs[key] =
            `Mozilla/5.0 (Linux; ${RunningAndroidVersion}; ${androidDevice}) ${fxQuantum}AppleWebKit/537.36 (KHTML, like Gecko) Chrome/${version} Mobile Safari/537.36`;
        } else {
          const ChromePhoneUA = `Mozilla/5.0 (Linux; ${RunningAndroidVersion}; Nexus 5 Build/MRA58N) ${fxQuantum}AppleWebKit/537.36 (KHTML, like Gecko) Chrome/${version} Mobile Safari/537.36`;
          const ChromeTabletUA = `Mozilla/5.0 (Linux; ${RunningAndroidVersion}; Nexus 7 Build/JSS15Q) ${fxQuantum}AppleWebKit/537.36 (KHTML, like Gecko) Chrome/${version} Safari/537.36`;
          const IsPhone = userAgent.includes("Mobile");
          UAHelpers._deviceAppropriateChromeUAs[key] = IsPhone
            ? ChromePhoneUA
            : ChromeTabletUA;
        }
      } else {
        let osSegment = "Windows NT 10.0; Win64; x64";
        if (desktopOS === "macOS" || userAgent.includes("Macintosh")) {
          osSegment = "Macintosh; Intel Mac OS X 10_15_7";
        }
        if (
          desktopOS !== "nonLinux" &&
          (desktopOS === "linux" || userAgent.includes("Linux"))
        ) {
          osSegment = "X11; Ubuntu; Linux x86_64";
        }

        UAHelpers._deviceAppropriateChromeUAs[key] =
          `Mozilla/5.0 (${osSegment}) ${fxQuantum}AppleWebKit/537.36 (KHTML, like Gecko) Chrome/${version} Safari/537.36`;
      }
    }
    return UAHelpers._deviceAppropriateChromeUAs[key];
  },
  addGecko(
    ua = navigator.userAgent,
    version = UAHelpers.getRunningFirefoxVersion()
  ) {
    return `${ua} Gecko/${version}`;
  },
  addChrome(ua = navigator.userAgent, version = "130.0.0.0") {
    const isMobile =
      navigator.userAgent.includes("Mobile") ||
      navigator.userAgent.includes("Tablet");
    return `${ua} AppleWebKit/537.36 (KHTML, like Gecko) Chrome/${version} ${isMobile ? "Mobile " : ""}Safari/537.36`;
  },
  safari(config = {}) {
    const version = config.version || "18.1";
    const webkitVersion = config.webkitVersion || "605.1.15";
    const osVersion = config.osVersion?.replace(".", "_") || "10_15_7";
    const arch = config.arch || "Intel";
    let firefox = "";
    switch (config.firefox || true) {
      case "firefox":
      case true:
        firefox = `Firefox/${UAHelpers.getRunningFirefoxVersion()} `;
        break;
      case "fxQuantum":
        firefox = UAHelpers.getFxQuantumSegment();
        break;
    }
    return `Mozilla/5.0 (Macintosh; ${arch} Mac OS X ${osVersion}) ${firefox}AppleWebKit/${webkitVersion} (KHTML, like Gecko) Version/${version} Safari/${webkitVersion}`;
  },
  androidHotspot2Device(originalUA) {
    return originalUA.replace(/\(.+?\)/, "(Linux; Android 10; K)");
  },
  changeFirefoxToFireFox(ua = navigator.userAgent) {
    return ua.replace("Firefox", "FireFox");
  },
  windows(ua = navigator.userAgent) {
    const rv = navigator.userAgent.match("rv:[0-9]+.[0-9]+")[0];
    return ua.replace(/\(.+?\)/, `(Windows NT 10.0; Win64; x64; ${rv})`);
  },
  desktopUA(ua = navigator.userAgent) {
    return ua.replace(/ (Mobile|Tablet);/, "");
  },
  addSamsungForSamsungDevices(ua = navigator.userAgent) {
    if (!browser.systemManufacturer) {
      return ua;
    }

    const manufacturer = browser.systemManufacturer.getManufacturer();
    if (manufacturer && manufacturer.toLowerCase() === "samsung") {
      return ua.replace("Mobile;", "Mobile; Samsung;");
    }

    return ua;
  },
  firefoxPlusChrome(ua = navigator.userAgent, version = "130.0.0.0") {
    return `${ua} AppleWebKit/537.36 (KHTML, like Gecko) Chrome/${version} Mobile Safari/537.36`;
  },
  getPrefix(originalUA) {
    return originalUA.substr(0, originalUA.indexOf(")") + 1);
  },
  overrideWithDeviceAppropriateChromeUA(config) {
    const chromeUA = UAHelpers.getDeviceAppropriateChromeUA(config);
    Object.defineProperty(window.navigator.wrappedJSObject, "userAgent", {
      get: exportFunction(() => chromeUA, window),
      set: exportFunction(function () {}, window),
    });
  },
  capVersionTo99(originalUA) {
    const ver = originalUA.match(/Firefox\/(\d+\.\d+)/);
    if (!ver || parseFloat(ver[1]) < 100) {
      return originalUA;
    }
    return originalUA
      .replace(`Firefox/${ver[1]}`, "Firefox/99.0")
      .replace(`rv:${ver[1]}`, "rv:99.0");
  },
  capRvTo109(originalUA) {
    const ver = originalUA.match(/rv:(\d+\.\d+)/);
    if (!ver || parseFloat(ver[1]) <= 109) {
      return originalUA;
    }
    return originalUA.replace(`rv:${ver[1]}`, "rv:109.0");
  },
  capVersionToNumber(originalUA, cap = 120) {
    const ver = originalUA.match(/Firefox\/(\d+\.\d+)/);
    if (!ver || parseFloat(ver[1]) <= cap) {
      return originalUA;
    }
    const capped = `Firefox/${cap}.0`;
    return originalUA.replace(`Firefox/${ver[1]}`, capped);
  },
  getMacOSXUA(originalUA, arch = "Intel", version = "10.15") {
    return originalUA.replace(
      /\(.+?\)/,
      `(Macintosh; ${arch} Mac OS X ${version})`
    );
  },
  getWindowsUA(originalUA) {
    const rv = originalUA.match("rv:[0-9]+.[0-9]+")[0];
    const ver = originalUA.match("Firefox/[0-9]+.[0-9]+")[0];
    return `Mozilla/5.0 (Windows NT 10.0; Win64; x64; ${rv}) Gecko/20100101 ${ver}`;
  },
};

if (typeof module !== "undefined") {
  module.exports = UAHelpers;
}
