// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  HiddenFrame: "resource://gre/modules/HiddenFrame.sys.mjs",
  Preferences: "resource://gre/modules/Preferences.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
  clearTimeout: "resource://gre/modules/Timer.sys.mjs",
});

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    prefix: "UserCharacteristicsPage",
    maxLogLevelPref: "toolkit.telemetry.user_characteristics_ping.logLevel",
  });
});

ChromeUtils.defineLazyGetter(lazy, "contentPrefs", () => {
  return Cc["@mozilla.org/content-pref/service;1"].getService(
    Ci.nsIContentPrefService2
  );
});

const BACKGROUND_WIDTH = 1024;
const BACKGROUND_HEIGHT = 768;

/**
 * A manager for hidden browsers. Responsible for creating and destroying a
 * hidden frame to hold them.
 * All of this is copied from PageDataService.sys.mjs
 */
class HiddenBrowserManager {
  /**
   * The hidden frame if one has been created.
   *
   * @type {HiddenFrame | null}
   */
  #frame = null;
  /**
   * The number of hidden browser elements currently in use.
   *
   * @type {number}
   */
  #browsers = 0;

  /**
   * Creates and returns a new hidden browser.
   *
   * @returns {Browser}
   */
  async #acquireBrowser() {
    this.#browsers++;
    if (!this.#frame) {
      this.#frame = new lazy.HiddenFrame();
    }

    let frame = await this.#frame.get();
    let doc = frame.document;
    let browser = doc.createXULElement("browser");
    browser.setAttribute("remote", "true");
    browser.setAttribute("type", "content");
    browser.setAttribute(
      "style",
      `
        width: ${BACKGROUND_WIDTH}px;
        min-width: ${BACKGROUND_WIDTH}px;
        height: ${BACKGROUND_HEIGHT}px;
        min-height: ${BACKGROUND_HEIGHT}px;
      `
    );
    browser.setAttribute("maychangeremoteness", "true");
    doc.documentElement.appendChild(browser);

    return browser;
  }

  /**
   * Releases the given hidden browser.
   *
   * @param {Browser} browser
   *   The hidden browser element.
   */
  #releaseBrowser(browser) {
    browser.remove();

    this.#browsers--;
    if (this.#browsers == 0) {
      this.#frame.destroy();
      this.#frame = null;
    }
  }

  /**
   * Calls a callback function with a new hidden browser.
   * This function will return whatever the callback function returns.
   *
   * @param {Callback} callback
   *   The callback function will be called with the browser element and may
   *   be asynchronous.
   * @returns {T}
   */
  async withHiddenBrowser(callback) {
    let browser = await this.#acquireBrowser();
    try {
      return await callback(browser);
    } finally {
      this.#releaseBrowser(browser);
    }
  }
}

export class UserCharacteristicsPageService {
  classId = Components.ID("{ce3e9659-e311-49fb-b18b-7f27c6659b23}");
  QueryInterface = ChromeUtils.generateQI([
    "nsIUserCharacteristicsPageService",
  ]);

  _initialized = false;
  _isParentProcess = false;

  /**
   * A manager for hidden browsers.
   *
   * @type {HiddenBrowserManager}
   */
  _browserManager = new HiddenBrowserManager();

  /**
   * A map of hidden browsers to a resolve function that should be passed the
   * actor that was created for the browser.
   *
   * @type {WeakMap<Browser, function(PageDataParent): void>}
   */
  _backgroundBrowsers = new WeakMap();

  constructor() {
    lazy.console.debug("Init");

    if (
      Services.appinfo.processType !== Services.appinfo.PROCESS_TYPE_DEFAULT
    ) {
      throw new Error(
        "Shouldn't init UserCharacteristicsPage in content processes."
      );
    }

    // Return if we have initiated.
    if (this._initialized) {
      lazy.console.warn("preventing re-initilization...");
      return;
    }
    this._initialized = true;
  }

  shutdown() {}

  createContentPage() {
    lazy.console.debug("called createContentPage");

    lazy.console.debug("Registering actor");
    ChromeUtils.registerWindowActor("UserCharacteristics", {
      parent: {
        esModuleURI: "resource://gre/actors/UserCharacteristicsParent.sys.mjs",
      },
      child: {
        esModuleURI: "resource://gre/actors/UserCharacteristicsChild.sys.mjs",
        events: {
          UserCharacteristicsDataDone: { wantUntrusted: true },
        },
      },
      matches: ["about:fingerprintingprotection"],
      remoteTypes: ["privilegedabout"],
    });

    return this._browserManager.withHiddenBrowser(async browser => {
      lazy.console.debug(`In withHiddenBrowser`);
      try {
        let { promise, resolve } = Promise.withResolvers();
        this._backgroundBrowsers.set(browser, resolve);

        let principal = Services.scriptSecurityManager.getSystemPrincipal();
        let loadURIOptions = {
          triggeringPrincipal: principal,
        };

        let userCharacteristicsPageURI = Services.io.newURI(
          "about:fingerprintingprotection" +
            (Cu.isInAutomation ? "#automation" : "")
        );

        browser.loadURI(userCharacteristicsPageURI, loadURIOptions);

        let data = await promise;
        if (data.debug) {
          lazy.console.debug(`Debugging Output:`);
          for (let line of data.debug) {
            lazy.console.debug(line);
          }
          lazy.console.debug(`(debugging output done)`);
        }
        lazy.console.debug(`Data:`, data.output);

        lazy.console.debug("Populating Glean metrics...");

        await this.populateAndCollectErrors(browser, data);

        // Notify test observers that the data has been populated.
        Services.obs.notifyObservers(
          null,
          "user-characteristics-populating-data-done"
        );
      } finally {
        lazy.console.debug("Unregistering actor");
        ChromeUtils.unregisterWindowActor("UserCharacteristics");
        this._backgroundBrowsers.delete(browser);
      }
    });
  }

  async populateAndCollectErrors(browser, data) {
    const populateFuncs = [
      [this.populateIntlLocale, []],
      [this.populateZoomPrefs, []],
      [this.populateDevicePixelRatio, [browser.ownerGlobal]],
      [this.populateDisabledMediaPrefs, []],
      [this.populateMathOps, []],
      [this.populateMapableData, [data.output]],
      [this.populateGamepads, [data.output.gamepads]],
      [this.populateClientInfo, []],
      [this.populateCPUInfo, []],
      [this.populateScreenInfo, []],
    ];
    const results = await Promise.allSettled(
      populateFuncs.map(([f, args]) =>
        timeoutPromise(f(...args), 5 * 60 * 1000)
      )
    );

    const errors = JSON.parse(data?.output?.errors ?? "[]");
    for (const [i, [func]] of populateFuncs.entries()) {
      if (results[i].status == "rejected") {
        const error = `${func.name}: ${await stringifyError(
          results[i].reason
        )}`;
        errors.push(error);
        lazy.console.debug(error);
      }
    }

    Glean.characteristics.jsErrors.set(JSON.stringify(errors));
  }

  async populateScreenInfo() {
    // We use two different methods to get any loaded document.
    // First one is, DOMContentLoaded event. If the user loads
    // a new document after actor registration, we will get it.
    // Second one is, we iterate over all open windows and tabs
    // and try to get the screen info from them.
    // The reason we do both is, for DOMContentLoaded, we can't
    // guarantee that all the documents were not loaded before the
    // actor registration.
    // We could only use the second method and add a load event
    // listener, but that assumes the user won't close already
    // existing tabs and continue on a new one before the page
    // is loaded. This is a rare case, but we want to cover it.

    if (Cu.isInAutomation) {
      // To safeguard against any possible weird empty
      // documents, we check if the document is empty. If it is
      // we wait for a valid document to be loaded.
      // During testing, we load empty.html which doesn't
      // have any body. So, we end up waiting forever.
      // Because of this, we skip this part during automation.
      return;
    }

    const { promise, resolve } = Promise.withResolvers();

    Services.obs.addObserver(function observe(_subject, topic, data) {
      Services.obs.removeObserver(observe, topic);
      ChromeUtils.unregisterWindowActor("UserCharacteristicsScreenInfo");
      resolve(data.split(","));
    }, "user-characteristics-screen-info-done");

    ChromeUtils.registerWindowActor("UserCharacteristicsScreenInfo", {
      parent: {
        esModuleURI: "resource://gre/actors/UserCharacteristicsParent.sys.mjs",
      },
      child: {
        esModuleURI:
          "resource://gre/actors/UserCharacteristicsScreenInfoChild.sys.mjs",
        events: {
          DOMContentLoaded: {},
        },
      },
    });

    for (const win of Services.wm.getEnumerator("navigator:browser")) {
      if (!win.closed) {
        for (const tab of win.gBrowser.tabs) {
          const actor =
            tab.linkedBrowser.browsingContext?.currentWindowGlobal.getActor(
              "UserCharacteristicsScreenInfo"
            );

          if (!actor) {
            continue;
          }

          actor.sendAsyncMessage("ScreenInfo:PopulateFromDocument");
        }
      }
    }

    const result = await promise;
    Glean.characteristics.outerHeight.set(result[0]);
    Glean.characteristics.innerHeight.set(result[1]);
    Glean.characteristics.outerWidth.set(result[2]);
    Glean.characteristics.innerWidth.set(result[3]);
    Glean.characteristics.availHeight.set(result[4]);
    Glean.characteristics.availWidth.set(result[5]);
  }

  async populateZoomPrefs() {
    const zoomPrefsCount = await new Promise(resolve => {
      lazy.contentPrefs.getByName("browser.content.full-zoom", null, {
        _result: 0,
        handleResult(_) {
          this._result++;
        },
        handleCompletion() {
          resolve(this._result);
        },
      });
    });

    Glean.characteristics.zoomCount.set(zoomPrefsCount);
  }

  async populateDevicePixelRatio(window) {
    Glean.characteristics.pixelRatio.set(
      (window.browsingContext.overrideDPPX || window.devicePixelRatio) * 100
    );
  }

  async populateIntlLocale() {
    const locale = new Intl.DisplayNames(undefined, {
      type: "region",
    }).resolvedOptions().locale;
    Glean.characteristics.intlLocale.set(locale);
  }

  async populateGamepads(gamepads) {
    for (let gamepad of gamepads) {
      Glean.characteristics.gamepads.add(gamepad);
    }
  }

  async populateMapableData(data) {
    // We set data from usercharacteristics.js
    // We could do Object.keys(data), but this
    // is more explicit and provides better
    // readability and control.
    // Keys must match to data returned from
    // usercharacteristics.js and the metric defined
    const metrics = {
      set: [
        "canvasdata1",
        "canvasdata2",
        "canvasdata3",
        "canvasdata4",
        "canvasdata5",
        "canvasdata6",
        "canvasdata7",
        "canvasdata8",
        "canvasdata9",
        "canvasdata10",
        "canvasdata11Webgl",
        "canvasdata12Fingerprintjs1",
        "canvasdata13Fingerprintjs2",
        "voices",
        "mediaCapabilities",
        "audioFingerprint",
        "jsErrors",
        "pointerType",
        "anyPointerType",
        "iceFoundations",
        "motionDecimals",
        "orientationDecimals",
        "orientationabsDecimals",
        "motionFreq",
        "orientationFreq",
        "orientationabsFreq",
        "mathml1",
        "mathml2",
        "mathml3",
        "mathml4",
        "mathml5",
        "mathml6",
        "mathml7",
        "mathml8",
        "mathml9",
        "mathml10",
        "monochrome",
        "oscpu",
        "pdfViewer",
        "platform",
      ],
    };

    for (const type in metrics) {
      for (const metric of metrics[type]) {
        Glean.characteristics[metric][type](data[metric]);
      }
    }
  }

  async populateMathOps() {
    // Taken from https://github.com/fingerprintjs/fingerprintjs/blob/da64ad07a9c1728af595068e4a306a4151c5d503/src/sources/math.ts
    // At the time, fingerprintjs was licensed under MIT. Slightly modified to reduce payload size.
    const ops = [
      // Native
      [Math.acos, 0.123124234234234242],
      [Math.acosh, 1e308],
      [Math.asin, 0.123124234234234242],
      [Math.asinh, 1],
      [Math.atanh, 0.5],
      [Math.atan, 0.5],
      [Math.sin, -1e300],
      [Math.sinh, 1],
      [Math.cos, 10.000000000123],
      [Math.cosh, 1],
      [Math.tan, -1e300],
      [Math.tanh, 1],
      [Math.exp, 1],
      [Math.expm1, 1],
      [Math.log1p, 10],
      // Polyfills (I'm not sure if we need polyfills since firefox seem to have all of these operations, but I'll leave it here just in case they yield different values due to chaining)
      [value => Math.pow(Math.PI, value), -100],
      [value => Math.log(value + Math.sqrt(value * value - 1)), 1e154],
      [value => Math.log(value + Math.sqrt(value * value + 1)), 1],
      [value => Math.log((1 + value) / (1 - value)) / 2, 0.5],
      [value => Math.exp(value) - 1 / Math.exp(value) / 2, 1],
      [value => (Math.exp(value) + 1 / Math.exp(value)) / 2, 1],
      [value => Math.exp(value) - 1, 1],
      [value => (Math.exp(2 * value) - 1) / (Math.exp(2 * value) + 1), 1],
      [value => Math.log(1 + value), 10],
    ].map(([op, value]) => [op || (() => 0), value]);

    Glean.characteristics.mathOps.set(
      JSON.stringify(ops.map(([op, value]) => op(value)))
    );
  }

  async populateClientInfo() {
    const buildID = Services.appinfo.appBuildID;
    const buildDate =
      new Date(
        buildID.slice(0, 4),
        buildID.slice(4, 6) - 1,
        buildID.slice(6, 8),
        buildID.slice(8, 10),
        buildID.slice(10, 12),
        buildID.slice(12, 14)
      ).getTime() / 1000;

    Glean.characteristics.version.set(Services.appinfo.version);
    Glean.characteristics.channel.set(AppConstants.MOZ_UPDATE_CHANNEL);
    Glean.characteristics.osName.set(Services.appinfo.OS);
    Glean.characteristics.osVersion.set(
      Services.sysinfo.getProperty("version")
    );
    Glean.characteristics.buildDate.set(buildDate);
  }

  async populateCPUInfo() {
    Glean.characteristics.cpuModel.set(
      await Services.sysinfo.processInfo.then(r => r.name)
    );
  }

  async pageLoaded(browsingContext, data) {
    lazy.console.debug(
      `pageLoaded browsingContext=${browsingContext} data=${data}`
    );

    let browser = browsingContext.embedderElement;

    let backgroundResolve = this._backgroundBrowsers.get(browser);
    if (backgroundResolve) {
      backgroundResolve(data);
      return;
    }
    throw new Error(`No backround resolve for ${browser} found`);
  }

  async populateDisabledMediaPrefs() {
    const PREFS = [
      "media.wave.enabled",
      "media.ogg.enabled",
      "media.opus.enabled",
      "media.mp4.enabled",
      "media.wmf.hevc.enabled",
      "media.webm.enabled",
      "media.av1.enabled",
      "media.encoder.webm.enabled",
      "media.mediasource.enabled",
      "media.mediasource.mp4.enabled",
      "media.mediasource.webm.enabled",
      "media.mediasource.vp9.enabled",
    ];

    const defaultPrefs = new lazy.Preferences({ defaultBranch: true });
    const changedPrefs = {};
    for (const pref of PREFS) {
      const value = lazy.Preferences.get(pref);
      if (lazy.Preferences.isSet(pref) && defaultPrefs.get(pref) !== value) {
        const key = pref.substring(6).substring(0, pref.length - 8 - 6);
        changedPrefs[key] = value;
      }
    }
    Glean.characteristics.changedMediaPrefs.set(JSON.stringify(changedPrefs));
  }
}

// =============================================================
// Utility Functions

async function stringifyError(error) {
  if (error instanceof Error) {
    const stack = (error.stack ?? "").replaceAll(
      /@chrome.+?UserCharacteristicsPageService.sys.mjs:/g,
      ""
    );
    return `${error.toString()} ${stack}`;
  }
  // A hacky attempt to extract as much as info from error
  const errStr = await (async () => {
    const asStr = await (async () => error.toString())().catch(() => "");
    const asJson = await (async () => JSON.stringify(error))().catch(() => "");
    return asStr.length > asJson.len ? asStr : asJson;
  })();
  return errStr;
}

function timeoutPromise(promise, ms) {
  return new Promise((resolve, reject) => {
    const timeoutId = lazy.setTimeout(() => {
      reject(new Error("TIMEOUT"));
    }, ms);

    promise.then(
      value => {
        lazy.clearTimeout(timeoutId);
        resolve(value);
      },
      error => {
        lazy.clearTimeout(timeoutId);
        reject(error);
      }
    );
  });
}
