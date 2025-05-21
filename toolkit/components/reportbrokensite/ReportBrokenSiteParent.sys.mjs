/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Troubleshoot } from "resource://gre/modules/Troubleshoot.sys.mjs";

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

export class ReportBrokenSiteParent extends JSWindowActorParent {
  #getAntitrackingBlockList() {
    // If content-track-digest256 is in the tracking table,
    // the user has enabled the strict list.
    const trackingTable = Services.prefs.getCharPref(
      "urlclassifier.trackingTable"
    );
    return trackingTable.includes("content") ? "strict" : "basic";
  }

  #getETPCategory() {
    // Note that the pref will be set to "custom" if the user disables ETP on
    // mobile.
    const etpState = Services.prefs.getStringPref(
      "browser.contentblocking.category",
      "standard"
    );
    return etpState;
  }

  #getAntitrackingInfo(browsingContext) {
    // Ask BounceTrackingProtection whether it has recently purged state for the
    // site in the current top level context.
    let btpHasPurgedSite = false;
    if (
      Services.prefs.getIntPref("privacy.bounceTrackingProtection.mode") !=
      Ci.nsIBounceTrackingProtection.MODE_DISABLED
    ) {
      let bounceTrackingProtection = Cc[
        "@mozilla.org/bounce-tracking-protection;1"
      ].getService(Ci.nsIBounceTrackingProtection);

      let { currentWindowGlobal } = browsingContext;
      if (currentWindowGlobal) {
        let { documentPrincipal } = currentWindowGlobal;
        let { baseDomain } = documentPrincipal;
        btpHasPurgedSite =
          bounceTrackingProtection.hasRecentlyPurgedSite(baseDomain);
      }
    }

    return {
      blockList: this.#getAntitrackingBlockList(),
      isPrivateBrowsing: browsingContext.usePrivateBrowsing,
      hasTrackingContentBlocked: !!(
        browsingContext.currentWindowGlobal.contentBlockingEvents &
        Ci.nsIWebProgressListener.STATE_BLOCKED_TRACKING_CONTENT
      ),
      hasMixedActiveContentBlocked: !!(
        browsingContext.secureBrowserUI.state &
        Ci.nsIWebProgressListener.STATE_BLOCKED_MIXED_ACTIVE_CONTENT
      ),
      hasMixedDisplayContentBlocked: !!(
        browsingContext.secureBrowserUI.state &
        Ci.nsIWebProgressListener.STATE_BLOCKED_MIXED_DISPLAY_CONTENT
      ),
      btpHasPurgedSite,
      etpCategory: this.#getETPCategory(),
    };
  }

  #parseGfxInfo(info) {
    const get = name => {
      try {
        return info[name];
      } catch (e) {}
      return undefined;
    };

    const clean = rawObj => {
      const obj = JSON.parse(JSON.stringify(rawObj));
      if (!Object.keys(obj).length) {
        return undefined;
      }
      return obj;
    };

    const cleanDevice = (vendorID, deviceID, subsysID) => {
      return clean({ vendorID, deviceID, subsysID });
    };

    const d1 = cleanDevice(
      get("adapterVendorID"),
      get("adapterDeviceID"),
      get("adapterSubsysID")
    );
    const d2 = cleanDevice(
      get("adapterVendorID2"),
      get("adapterDeviceID2"),
      get("adapterSubsysID2")
    );
    const devices = (get("isGPU2Active") ? [d2, d1] : [d1, d2]).filter(
      v => v !== undefined
    );

    return clean({
      direct2DEnabled: get("direct2DEnabled"),
      directWriteEnabled: get("directWriteEnabled"),
      directWriteVersion: get("directWriteVersion"),
      hasTouchScreen: info.ApzTouchInput == 1,
      clearTypeParameters: get("clearTypeParameters"),
      targetFrameRate: get("targetFrameRate"),
      devices,
    });
  }

  #parseCodecSupportInfo(codecSupportInfo) {
    if (!codecSupportInfo) {
      return undefined;
    }

    const codecs = {};
    for (const item of codecSupportInfo.split("\n")) {
      const [codec, ...types] = item.split(" ");
      if (!codecs[codec]) {
        codecs[codec] = { hardware: false, software: false };
      }
      codecs[codec].software ||= types.includes("SW");
      codecs[codec].hardware ||= types.includes("HW");
    }
    return codecs;
  }

  #parseFeatureLog(featureLog = {}) {
    const { features } = featureLog;
    if (!features) {
      return undefined;
    }

    const parsedFeatures = {};
    for (let { name, log, status } of features) {
      for (const item of log.reverse()) {
        if (!item.failureId || item.status != status) {
          continue;
        }
        status = `${status} (${item.message || item.failureId})`;
      }
      parsedFeatures[name] = status;
    }
    return parsedFeatures;
  }

  #getGraphicsInfo(troubleshoot) {
    const { graphics, media } = troubleshoot;
    const { featureLog } = graphics;
    const data = this.#parseGfxInfo(graphics);
    data.drivers = [
      {
        renderer: graphics.webgl1Renderer,
        version: graphics.webgl1Version,
      },
      {
        renderer: graphics.webgl2Renderer,
        version: graphics.webgl2Version,
      },
    ].filter(({ version }) => version && version != "-");

    data.codecSupport = this.#parseCodecSupportInfo(media.codecSupportInfo);
    data.features = this.#parseFeatureLog(featureLog);

    const gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);
    data.monitors = gfxInfo.getMonitors();

    return data;
  }

  #getAppInfo(troubleshootingInfo) {
    const { application } = troubleshootingInfo;
    return {
      applicationName: application.name,
      buildId: application.buildID,
      defaultUserAgent: application.userAgent,
      updateChannel: application.updateChannel,
      version: application.version,
    };
  }

  #getSysinfoProperty(propertyName, defaultValue) {
    try {
      return Services.sysinfo.getProperty(propertyName);
    } catch (e) {}
    return defaultValue;
  }

  #getPrefs() {
    const prefs = {};
    for (const name of [
      "layers.acceleration.force-enabled",
      "gfx.webrender.software",
      "browser.opaqueResponseBlocking",
      "extensions.InstallTrigger.enabled",
      "layout.css.h1-in-section-ua-styles.enabled",
      "privacy.resistFingerprinting",
      "privacy.globalprivacycontrol.enabled",
      "network.cookie.cookieBehavior.optInPartitioning",
      "network.cookie.cookieBehavior.optInPartitioning.pbmode",
    ]) {
      prefs[name] = Services.prefs.getBoolPref(name, undefined);
    }
    const cookieBehavior = "network.cookie.cookieBehavior";
    prefs[cookieBehavior] = Services.prefs.getIntPref(cookieBehavior, -1);
    return prefs;
  }

  async #getPlatformInfo(troubleshootingInfo) {
    const { application } = troubleshootingInfo;
    const { memorySizeBytes, fissionAutoStart } = application;

    let memoryMB = memorySizeBytes;
    if (memoryMB) {
      memoryMB = Math.round(memoryMB / 1024 / 1024);
    }

    const info = {
      fissionEnabled: fissionAutoStart,
      memoryMB,
      osArchitecture: this.#getSysinfoProperty("arch", null),
      osName: this.#getSysinfoProperty("name", null),
      osVersion: this.#getSysinfoProperty("version", null),
      name: AppConstants.platform,
    };
    if (info.os === "android") {
      info.device = this.#getSysinfoProperty("device", null);
      info.isTablet = this.#getSysinfoProperty("tablet", false);
    }
    if (
      info.osName == "Windows_NT" &&
      (await Services.sysinfo.processInfo).isWindowsSMode
    ) {
      info.osVersion += " S";
    }
    return info;
  }

  #getSecurityInfo(troubleshootingInfo) {
    const result = {};
    for (const [k, v] of Object.entries(troubleshootingInfo.securitySoftware)) {
      result[k.replace("registered", "").toLowerCase()] = v
        ? v.split(";")
        : null;
    }

    // Right now, security data is only available for Windows builds, and
    // we might as well not return anything at all if no data is available.
    if (!Object.values(result).filter(e => e).length) {
      return undefined;
    }

    return result;
  }

  static AUTOMATION_ADDON_IDS = [
    "mochikit@mozilla.org",
    "special-powers@mozilla.org",
  ];

  static WANTED_ADDON_LOCATIONS = ["app-profile", "app-temporary"];

  #getActiveAddons(troubleshootingInfo) {
    const { addons } = troubleshootingInfo;
    if (!addons) {
      return [];
    }
    // We only care about enabled addons (not themes) the user
    // installed, not ones bundled with Firefox.
    const toReport = addons.filter(
      ({ id, isActive, type, locationName }) =>
        (!Cu.isInAutomation ||
          !ReportBrokenSiteParent.AUTOMATION_ADDON_IDS.includes(id)) &&
        isActive &&
        type === "extension" &&
        ReportBrokenSiteParent.WANTED_ADDON_LOCATIONS.includes(locationName)
    );
    return toReport.map(({ id, name, version, locationName }) => {
      return {
        id,
        name,
        temporary: locationName === "app-temporary",
        version,
      };
    });
  }

  #getActiveExperiments(troubleshootingInfo) {
    if (!troubleshootingInfo?.normandy) {
      return [];
    }
    const {
      normandy: { nimbusExperiments, nimbusRollouts },
    } = troubleshootingInfo;
    return [
      nimbusExperiments.map(({ slug, branch }) => {
        return { slug, branch: branch.slug, kind: "nimbusExperiment" };
      }),
      nimbusRollouts.map(({ slug, branch }) => {
        return { slug, branch: branch.slug, kind: "nimbusRollout" };
      }),
    ]
      .flat()
      .sort((a, b) => a.slug.localeCompare(b.slug));
  }

  async #getBrowserInfo() {
    const troubleshootingInfo = await Troubleshoot.snapshot();
    return {
      addons: this.#getActiveAddons(troubleshootingInfo),
      app: this.#getAppInfo(troubleshootingInfo),
      experiments: this.#getActiveExperiments(troubleshootingInfo),
      graphics: this.#getGraphicsInfo(troubleshootingInfo),
      locales: troubleshootingInfo.intl.localeService.available,
      prefs: this.#getPrefs(),
      platform: await this.#getPlatformInfo(troubleshootingInfo),
      security: this.#getSecurityInfo(troubleshootingInfo),
    };
  }

  async #getScreenshot(browsingContext, format, quality) {
    const zoom = browsingContext.fullZoom;
    const scale = browsingContext.topChromeWindow?.devicePixelRatio || 1;
    const wgp = browsingContext.currentWindowGlobal;

    const image = await wgp.drawSnapshot(
      undefined, // rect
      scale * zoom,
      "white",
      undefined // resetScrollPosition
    );

    const canvas = new OffscreenCanvas(image.width, image.height);

    const ctx = canvas.getContext("bitmaprenderer", { alpha: false });
    ctx.transferFromImageBitmap(image);

    const blob = await canvas.convertToBlob({
      type: `image/${format}`,
      quality: quality / 100,
    });

    const dataURL = await new Promise((resolve, reject) => {
      let reader = new FileReader();
      reader.onload = () => resolve(reader.result);
      reader.onerror = () => reject(reader.error);
      reader.readAsDataURL(blob);
    });

    return dataURL;
  }

  async receiveMessage(msg) {
    switch (msg.name) {
      case "GetWebcompatInfoFromParentProcess": {
        const { browsingContext } = msg.target;
        const { format, quality } = msg.data;
        const screenshot = await this.#getScreenshot(
          browsingContext,
          format,
          quality
        ).catch(e => {
          console.error("Report Broken Site: getting a screenshot failed", e);
          return Promise.resolve(undefined);
        });

        const zoom = browsingContext.fullZoom;
        const scale = browsingContext.topChromeWindow?.devicePixelRatio || 1;
        const devicePixelRatio = scale * zoom;

        return {
          antitracking: this.#getAntitrackingInfo(msg.target.browsingContext),
          browser: await this.#getBrowserInfo(),
          devicePixelRatio,
          screenshot,
        };
      }
    }
    return null;
  }
}
