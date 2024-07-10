/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
});

// Name of the RemoteSettings collection containing the rules.
const COLLECTION_NAME = "bounce-tracking-protection-exceptions";

export class BTPRemoteExceptionList {
  classId = Components.ID("{06F13674-FB28-4DFC-BF25-342C83705B2F}");
  QueryInterface = ChromeUtils.generateQI(["nsIBTPRemoteExceptionList"]);

  #rs = null;
  // Stores the this-wrapped on-sync callback so it can be unregistered on
  // shutdown.
  #onSyncCallback = null;

  // Weak reference to nsIBounceTrackingProtection.
  #btp = null;

  constructor() {
    this.#rs = lazy.RemoteSettings(COLLECTION_NAME);
  }

  async init(bounceTrackingProtection) {
    if (!bounceTrackingProtection) {
      throw new Error("Missing required argument bounceTrackingProtection");
    }
    // Get a weak ref to BounceTrackingProtection to avoid a reference cycle.
    this.#btp = Cu.getWeakReference(bounceTrackingProtection);

    await this.importAllExceptions();

    // Register callback for collection changes.
    // Only register if not already registered.
    if (!this.#onSyncCallback) {
      this.#onSyncCallback = this.onSync.bind(this);
      this.#rs.on("sync", this.#onSyncCallback);
    }
  }

  shutdown() {
    // Unregister callback for collection changes.
    if (this.#onSyncCallback) {
      this.#rs.off("sync", this.#onSyncCallback);
      this.#onSyncCallback = null;
    }
  }

  /**
   * Called for remote settings "sync" events.
   */
  onSync({ data: { created = [], updated = [], deleted = [] } }) {
    // Check if feature is still active before attempting to send updates.
    let btp = this.#btp?.get();
    if (!btp) {
      return;
    }

    let siteHostsToRemove = deleted.map(ex => ex.siteHost);
    let siteHostsToAdd = created.map(ex => ex.siteHost);

    updated.forEach(ex => {
      // We only care about site host changes.
      if (ex.old.siteHost == ex.new.siteHost) {
        return;
      }
      siteHostsToRemove.push(ex.old.siteHost);
      siteHostsToAdd.push(ex.new.siteHost);
    });

    if (siteHostsToRemove.length) {
      btp.removeSiteHostExceptions(siteHostsToRemove);
    }
    if (siteHostsToAdd.length) {
      btp.addSiteHostExceptions(siteHostsToAdd);
    }
  }

  async importAllExceptions() {
    try {
      let exceptions = await this.#rs.get();
      if (!exceptions.length) {
        return;
      }
      this.onSync({ data: { created: exceptions } });
    } catch (error) {
      console.error(
        "Error while importing BTP exceptions from RemoteSettings",
        error
      );
    }
  }
}
