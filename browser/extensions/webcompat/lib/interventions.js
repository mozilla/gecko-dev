/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals browser, InterventionHelpers */

class Interventions {
  constructor(availableInterventions, customFunctions) {
    this.INTERVENTION_PREF = "perform_injections";

    this._interventionsEnabled = true;

    this._readyPromise = new Promise(done => (this._resolveReady = done));

    this._availableInterventions = Object.entries(availableInterventions).map(
      ([id, obj]) => {
        obj.id = id;
        return obj;
      }
    );
    this._customFunctions = customFunctions;

    this._activeListenersPerIntervention = new Map();
    this._contentScriptsPerIntervention = new Map();
  }

  ready() {
    return this._readyPromise;
  }

  bindAboutCompatBroker(broker) {
    this._aboutCompatBroker = broker;
  }

  bootup() {
    browser.aboutConfigPrefs.onPrefChange.addListener(() => {
      this.checkInterventionPref();
    }, this.INTERVENTION_PREF);
    this.checkInterventionPref();
  }

  async updateInterventions(_data) {
    const data = structuredClone(_data);
    await this.disableInterventions(data);
    await this.enableInterventions(data);
    for (const intervention of data) {
      const { id } = intervention;
      const i = this._availableInterventions.findIndex(v => v.id === id);
      if (i > -1) {
        this._availableInterventions[i] = intervention;
      } else {
        this._availableInterventions.push(intervention);
      }
    }
    return data;
  }

  checkInterventionPref() {
    navigator.locks.request("pref_check_lock", async () => {
      const value = await browser.aboutConfigPrefs.getPref(
        this.INTERVENTION_PREF
      );
      if (value === undefined) {
        await browser.aboutConfigPrefs.setPref(this.INTERVENTION_PREF, true);
      } else if (value === false) {
        await this.disableInterventions();
      } else {
        await this.enableInterventions();
      }
    });
  }

  checkOverridePref() {
    navigator.locks.request("pref_check_lock", async () => {
      const value = await browser.aboutConfigPrefs.getPref(this.OVERRIDE_PREF);
      if (value === undefined) {
        await browser.aboutConfigPrefs.setPref(this.OVERRIDE_PREF, true);
      } else if (value === false) {
        await this.unregisterUAOverrides();
      } else {
        await this.registerUAOverrides();
      }
    });
  }

  getAvailableInterventions() {
    return this._availableInterventions;
  }

  _getActiveInterventionById(whichId) {
    return this._availableInterventions.find(({ id }) => id === whichId);
  }

  isEnabled() {
    return this._interventionsEnabled;
  }

  async enableInterventions(whichInterventions = this._availableInterventions) {
    return navigator.locks.request("intervention_lock", async () => {
      await this._enableInterventionsNow(whichInterventions);
    });
  }

  async disableInterventions(
    whichInterventions = this._availableInterventions
  ) {
    return navigator.locks.request("intervention_lock", async () => {
      for (const config of whichInterventions) {
        await this._disableInterventionNow(config);
      }

      this._interventionsEnabled = false;
      this._aboutCompatBroker.portsToAboutCompatTabs.broadcast({
        interventionsChanged: false,
      });
    });
  }

  #checkedPrefListeners = new Map();
  #checkedPrefCache = new Map();

  async onCheckedPrefChanged(pref) {
    navigator.locks.request("pref_check_lock", async () => {
      this.#checkedPrefCache.delete(pref);
      const toRecheck = this._availableInterventions.filter(cfg =>
        cfg.interventions.find(i => i.pref_check && pref in i.pref_check)
      );
      await this.updateInterventions(toRecheck);
    });
  }

  async _check_for_needed_prefs(intervention) {
    if (!intervention.pref_check) {
      return true;
    }
    for (const pref of Object.keys(intervention.pref_check ?? {})) {
      if (!this.#checkedPrefListeners.has(pref)) {
        const listener = () => this.onCheckedPrefChanged(pref);
        this.#checkedPrefListeners.set(pref, listener);
        await browser.aboutConfigPrefs.onPrefChange.addListener(listener, pref);
      }
    }
    for (const [pref, value] of Object.entries(intervention.pref_check ?? {})) {
      if (!this.#checkedPrefCache.has(pref)) {
        this.#checkedPrefCache.set(
          pref,
          await browser.aboutConfigPrefs.getPref(pref)
        );
      }
      if (value !== this.#checkedPrefCache.get(pref)) {
        return false;
      }
    }
    return true;
  }

  async _enableInterventionsNow(whichInterventions) {
    const skipped = [];

    const channel = await browser.appConstants.getEffectiveUpdateChannel();
    const version =
      this.versionForTesting ??
      (await browser.runtime.getBrowserInfo()).version;
    const cleanVersion = parseFloat(version.match(/\d+(\.\d+)?/)[0]);

    const os = await InterventionHelpers.getOS();
    this.currentPlatform = os;

    for (const config of whichInterventions) {
      for (const intervention of config.interventions) {
        intervention.enabled = false;
        if (!(await this._check_for_needed_prefs(intervention))) {
          continue;
        }
        if (
          await InterventionHelpers.shouldSkip(
            intervention,
            cleanVersion,
            channel
          )
        ) {
          continue;
        }
        if (!(await InterventionHelpers.checkPlatformMatches(intervention))) {
          continue;
        }
        intervention.enabled = true;
        config.availableOnPlatform = true;
      }

      if (!config.availableOnPlatform) {
        skipped.push(config.label);
        continue;
      }

      try {
        await this._enableInterventionNow(config);
      } catch (e) {
        console.error("Error enabling intervention(s) for", config.label, e);
      }
    }

    if (skipped.length) {
      console.warn(
        "Skipping",
        skipped.length,
        "un-needed interventions",
        skipped.sort()
      );
    }

    this._interventionsEnabled = true;
    this._aboutCompatBroker.portsToAboutCompatTabs.broadcast({
      interventionsChanged:
        this._aboutCompatBroker.filterInterventions(whichInterventions),
    });

    this._resolveReady();
  }

  async enableIntervention(config) {
    return navigator.locks.request("intervention_lock", async () => {
      await this._enableInterventionNow(config);
    });
  }

  async disableIntervention(config) {
    return navigator.locks.request("intervention_lock", async () => {
      await this._disableInterventionNow(config);
    });
  }

  async _enableInterventionNow(config) {
    if (config.active) {
      return;
    }

    const { bugs, label } = config;
    const blocks = Object.values(bugs)
      .map(bug => bug.blocks)
      .flat()
      .filter(v => v !== undefined);
    const matches = Object.values(bugs)
      .map(bug => bug.matches)
      .flat()
      .filter(v => v !== undefined);

    for (const intervention of config.interventions) {
      if (!intervention.enabled) {
        continue;
      }

      await this._changeCustomFuncs("enable", label, intervention, config);
      if (intervention.content_scripts) {
        await this._enableContentScripts(
          config.id,
          label,
          intervention,
          matches
        );
      }
      await this._enableUAOverrides(label, intervention, matches);
      await this._enableRequestBlocks(label, intervention, blocks);
    }

    if (!this._getActiveInterventionById(config.id)) {
      this._availableInterventions.push(config);
      console.info("Added webcompat intervention", config.id, config);
    } else {
      for (const [index, oldConfig] of this._availableInterventions.entries()) {
        if (oldConfig.id === config.id && oldConfig !== config) {
          console.info("Replaced webcompat intervention", oldConfig.id, config);
          this._availableInterventions[index] = config;
        }
      }
    }

    config.active = true;
  }

  async _disableInterventionNow(_config) {
    const config = this._getActiveInterventionById(_config?.id ?? _config);
    if (!config) {
      return;
    }

    const { active, label, interventions } = config;

    if (!active) {
      return;
    }

    for (const intervention of interventions) {
      if (!intervention.enabled) {
        continue;
      }

      await this._changeCustomFuncs("disable", label, intervention, config);
      if (intervention.content_scripts) {
        await this._disableContentScripts(label, intervention);
      }

      // This covers both request blocks and ua_string cases
      const listeners = this._activeListenersPerIntervention.get(intervention);
      if (listeners) {
        for (const [name, listener] of Object.entries(listeners)) {
          browser.webRequest[name].removeListener(listener);
        }
        this._activeListenersPerIntervention.delete(intervention);
      }
    }

    config.active = false;
  }

  async _changeCustomFuncs(action, label, intervention, config) {
    for (const [customFuncName, customFunc] of Object.entries(
      this._customFunctions
    )) {
      if (customFuncName in intervention) {
        for (const details of intervention[customFuncName]) {
          try {
            await customFunc[action](details, config);
          } catch (e) {
            console.trace(
              `Error while calling custom function ${customFuncName}.${action} for ${label}:`,
              e
            );
          }
        }
      }
    }
  }

  async _enableUAOverrides(label, intervention, matches) {
    if (!("ua_string" in intervention)) {
      return;
    }

    let listeners = this._activeListenersPerIntervention.get(intervention);
    if (!listeners) {
      listeners = {};
      this._activeListenersPerIntervention.set(intervention, listeners);
    }

    const listener = details => {
      const { enabled, ua_string } = intervention;

      // Don't actually override the UA for an experiment if the user is not
      // part of the experiment (unless they force-enabed the override).
      if (
        enabled &&
        (!intervention.experiment || intervention.permanentPrefEnabled === true)
      ) {
        for (const header of details.requestHeaders) {
          if (header.name.toLowerCase() !== "user-agent") {
            continue;
          }

          // Don't override the UA if we're on a mobile device that has the
          // "Request Desktop Site" mode enabled. The UA for the desktop mode
          // is set inside Gecko with a simple string replace, so we can use
          // that as a check, see https://searchfox.org/mozilla-central/rev/89d33e1c3b0a57a9377b4815c2f4b58d933b7c32/mobile/android/chrome/geckoview/GeckoViewSettingsChild.js#23-28
          let isMobileWithDesktopMode =
            this.currentPlatform == "android" &&
            header.value.includes("X11; Linux x86_64");
          if (isMobileWithDesktopMode) {
            continue;
          }

          header.value = InterventionHelpers.applyUAChanges(
            header.value,
            ua_string
          );
        }
      }
      return { requestHeaders: details.requestHeaders };
    };

    browser.webRequest.onBeforeSendHeaders.addListener(
      listener,
      { urls: matches },
      ["blocking", "requestHeaders"]
    );

    listeners.onBeforeSendHeaders = listener;

    console.info(`Enabled UA override for ${label}`);
  }

  async _enableRequestBlocks(label, intervention, blocks) {
    if (!blocks.length) {
      return;
    }

    let listeners = this._activeListenersPerIntervention.get(intervention);
    if (!listeners) {
      listeners = {};
      this._activeListenersPerIntervention.set(intervention, listeners);
    }

    const listener = () => {
      return { cancel: true };
    };

    browser.webRequest.onBeforeRequest.addListener(listener, { urls: blocks }, [
      "blocking",
    ]);

    listeners.onBeforeRequest = listener;
    console.info(`Blocking requests as specified for ${label}`);
  }

  async _enableContentScripts(bug, label, intervention, matches) {
    const scriptsToReg = this._buildContentScriptRegistrations(
      label,
      intervention,
      matches
    );
    this._contentScriptsPerIntervention.set(intervention, scriptsToReg);

    // Try to avoid re-registering scripts already registered
    // (e.g. if the webcompat background page is restarted
    // after an extension process crash, after having registered
    // the content scripts already once), but do not prevent
    // to try registering them again if the getRegisteredContentScripts
    // method returns an unexpected rejection.

    const ids = scriptsToReg.map(s => s.id);
    try {
      const alreadyRegged = await browser.scripting.getRegisteredContentScripts(
        { ids }
      );
      const alreadyReggedIds = alreadyRegged.map(script => script.id);
      const stillNeeded = scriptsToReg.filter(
        ({ id }) => !alreadyReggedIds.includes(id)
      );
      await browser.scripting.registerContentScripts(stillNeeded);
      console.info(
        `Registered still-not-active content scripts for ${label}`,
        stillNeeded
      );
    } catch (e) {
      try {
        await browser.scripting.registerContentScripts(scriptsToReg);
        console.debug(
          `Registered all content scripts for ${label} after error registering just non-active ones`,
          scriptsToReg,
          e
        );
      } catch (e2) {
        console.error(
          `Error while registering content scripts for ${label}:`,
          e2,
          scriptsToReg
        );
      }
    }
  }

  async _disableContentScripts(label, intervention) {
    const contentScripts =
      this._contentScriptsPerIntervention.get(intervention);
    if (contentScripts) {
      const ids = contentScripts.map(s => s.id);
      await browser.scripting.unregisterContentScripts({ ids });
    }
  }

  _buildContentScriptRegistrations(label, intervention, matches) {
    const registration = {
      id: `webcompat intervention for ${label}: ${JSON.stringify(intervention.content_scripts)}`,
      matches,
      persistAcrossSessions: false,
    };

    let { all_frames, css, js, run_at } = intervention.content_scripts;
    if (!css && !js) {
      console.error(`Missing js or css for content_script in ${label}`);
      return [];
    }
    if (all_frames) {
      registration.allFrames = true;
    }
    if (css) {
      registration.css = css.map(item => {
        if (item.includes("/")) {
          return item;
        }
        return `injections/css/${item}`;
      });
    }
    if (js) {
      registration.js = js.map(item => {
        if (item.includes("/")) {
          return item;
        }
        return `injections/js/${item}`;
      });
    }
    if (run_at) {
      registration.runAt = run_at;
    } else {
      registration.runAt = "document_start";
    }

    return [registration];
  }
}
