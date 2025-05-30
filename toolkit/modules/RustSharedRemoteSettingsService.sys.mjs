/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Import these directly, since we're going to be using them immediately to construct SharedRemoteSettingsService
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { Region } from "resource://gre/modules/Region.sys.mjs";
import {
  RemoteSettingsConfig2,
  RemoteSettingsContext,
  RemoteSettingsServer,
  RemoteSettingsService,
} from "moz-src:///toolkit/components/uniffi-bindgen-gecko-js/components/generated/RustRemoteSettings.sys.mjs";
import { Utils } from "resource://services-settings/Utils.sys.mjs";

/**
 * Rust RemoteSettingsService singleton
 *
 * This component manages the app-wide Rust RemoteSetingsService that's
 * shared by various other Rust components.
 *
 * This is only intended to be passed to Rust code. If you want a
 * general-purpose Remote settings client, use the JS one:
 *
 * - https://firefox-source-docs.mozilla.org/services/settings/index.html
 * - https://searchfox.org/mozilla-central/source/services/settings/remote-settings.sys.mjs
 */
class _SharedRemoteSettingsService {
  #config;
  #rustService;

  constructor() {
    const storageDir = PathUtils.join(
      Services.dirsvc.get("ProfLD", Ci.nsIFile).path,
      "remote-settings"
    );

    this.#config = new RemoteSettingsConfig2({
      server: new RemoteSettingsServer.Custom({ url: Utils.SERVER_URL }),
      bucketName: Utils.actualBucketName("main"),
      appContext: new RemoteSettingsContext({
        formFactor: "desktop",
        appId: Services.appinfo.ID || "",
        channel: AppConstants.IS_ESR ? "esr" : AppConstants.MOZ_UPDATE_CHANNEL,
        appVersion: Services.appinfo.version,
        locale: Services.locale.appLocaleAsBCP47,
        os: AppConstants.platform,
        osVersion: Services.sysinfo.get("version"),
        country: Region.home ?? undefined,
      }),
    });

    Services.obs.addObserver(this, Region.REGION_TOPIC);

    this.#rustService = RemoteSettingsService.init(storageDir, this.#config);
  }

  /**
   * Update the Remote Settings server
   *
   * @param {object} opts object with the following fields:
   * - `url`: server URL (defaults to the production URL)
   * - `bucketName`: bucket name (defaults to "main")
   */
  updateServer(opts) {
    this.#config.server = opts.url
      ? new RemoteSettingsServer.Custom({ url: opts.url })
      : undefined;
    this.#config.bucketName = opts.bucketName ?? Utils.actualBucketName("main");
    this.#rustService.updateConfig(this.#config);
  }

  /**
   * Get a reference to the Rust RemoteSettingsService object
   */
  rustService() {
    return this.#rustService;
  }

  /**
   * Sync server data for all active clients
   */
  async sync() {
    // TODO (1966163): Hook this up to a timer.  There's currently no mechanism that calls this.
    await this.#rustService.sync();
  }

  observe(subj, topic) {
    if (topic == Region.REGION_TOPIC) {
      const newCountry = subj.data;
      if (newCountry != this.#config.appContext.country) {
        this.#config.appContext.country = newCountry;
        this.#rustService.updateConfig(this.#config);
      }
    }
  }
}

export const SharedRemoteSettingsService = new _SharedRemoteSettingsService();
