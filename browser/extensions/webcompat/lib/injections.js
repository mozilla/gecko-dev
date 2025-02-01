/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals browser, InterventionHelpers, module */

class Injections {
  constructor(availableInjections, customFunctions) {
    this.INJECTION_PREF = "perform_injections";

    this._injectionsEnabled = true;

    this._availableInjections = Object.entries(availableInjections).map(
      ([id, obj]) => {
        obj.id = id;
        return obj;
      }
    );
    this._customFunctions = customFunctions;

    // We do not try to enable/disable until we finish any such previous operation.
    this._enablingOrDisablingOperationInProgress = Promise.resolve();
  }

  bindAboutCompatBroker(broker) {
    this._aboutCompatBroker = broker;
  }

  bootup() {
    browser.aboutConfigPrefs.onPrefChange.addListener(() => {
      this.checkInjectionPref();
    }, this.INJECTION_PREF);
    this.checkInjectionPref();
  }

  checkInjectionPref() {
    browser.aboutConfigPrefs.getPref(this.INJECTION_PREF).then(value => {
      if (value === undefined) {
        browser.aboutConfigPrefs.setPref(this.INJECTION_PREF, true);
      } else if (value === false) {
        this.disableInjections();
      } else {
        this.enableInjections();
      }
    });
  }

  getAvailableInjections() {
    return this._availableInjections;
  }

  isEnabled() {
    return this._injectionsEnabled;
  }

  async enableInjections() {
    await this._enablingOrDisablingOperationInProgress;

    this._enablingOrDisablingOperationInProgress = new Promise(done => {
      this._enableInjectionsNow();
      done();
    });

    return this._enablingOrDisablingOperationInProgress;
  }

  async _enableInjectionsNow() {
    await this._enablingOrDisablingOperationInProgress;
    for (const injection of this._availableInjections) {
      for (const intervention of injection.interventions) {
        const { content_scripts } = intervention;
        if (!content_scripts) {
          continue;
        }
        if (await InterventionHelpers.shouldSkip(intervention)) {
          console.warn(`Skipping un-needed injection ${injection.label}`);
          continue;
        }
        if (!(await InterventionHelpers.checkPlatformMatches(intervention))) {
          continue;
        }
        intervention.enabled = true;
        injection.availableOnPlatform = true;
      }

      if (!injection.availableOnPlatform) {
        continue;
      }

      try {
        await this._enableInjectionNow(injection);
      } catch (e) {
        console.error("Error enabling injection for", injection.domain, e);
      }
    }

    this._injectionsEnabled = true;
    this._aboutCompatBroker.portsToAboutCompatTabs.broadcast({
      interventionsChanged: this._aboutCompatBroker.filterOverrides(
        this._availableInjections
      ),
    });
  }

  async enableInjection(injection) {
    await this._enablingOrDisablingOperationInProgress;

    this._enablingOrDisablingOperationInProgress = new Promise(done => {
      this._enableInjectionNow(injection);
      done();
    });

    return this._enablingOrDisablingOperationInProgress;
  }

  async _enableInjectionNow(injection) {
    if (injection.active) {
      return;
    }

    const { bugs, label } = injection;
    const matches = Object.values(bugs)
      .map(bug => bug.matches)
      .flat();

    for (const intervention of injection.interventions) {
      for (const [customFuncName, customFunc] of Object.entries(
        this._customFunctions
      )) {
        if (customFuncName in intervention) {
          try {
            await customFunc.enable(injection, intervention);
          } catch (e) {
            console.trace(
              `Error while enabling custom function ${customFuncName} for ${label}:`,
              e
            );
          }
        }
      }

      if ("content_scripts" in intervention) {
        this._buildContentScriptRegistrations(label, intervention, matches);

        // Try to avoid re-registering scripts already registered
        // (e.g. if the webcompat background page is restarted
        // after an extension process crash, after having registered
        // the content scripts already once), but do not prevent
        // to try registering them again if the getRegisteredContentScripts
        // method returns an unexpected rejection.

        const scriptsToReg = intervention._registrations;
        const ids = scriptsToReg.map(s => s.id);
        try {
          const alreadyRegged =
            await browser.scripting.getRegisteredContentScripts({ ids });
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
    }

    injection.active = true;
  }

  _buildContentScriptRegistrations(label, intervention, matches) {
    if (intervention._registrations) {
      return;
    }

    const registrations = [];
    let { content_scripts } = intervention;

    if (!Array.isArray(content_scripts)) {
      content_scripts = [content_scripts];
    }

    for (const [index, scriptConfig] of content_scripts.entries()) {
      const registration = {
        id: `webcompat intervention for ${label} #${index + 1}`,
        matches,
        persistAcrossSessions: false,
      };

      let { all_frames, css, js, run_at } = scriptConfig;
      if (!css && !js) {
        console.error(`Missing js or css for content_script in ${label}`);
        continue;
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
      registrations.push(registration);
    }

    intervention._registrations = registrations;
  }

  async disableInjections() {
    await this._enablingOrDisablingOperationInProgress;

    this._enablingOrDisablingOperationInProgress = new Promise(done => {
      for (const injection of this._availableInjections) {
        this._disableInjectionNow(injection);
      }

      this._injectionsEnabled = false;
      this._aboutCompatBroker.portsToAboutCompatTabs.broadcast({
        interventionsChanged: false,
      });

      done();
    });

    return this._enablingOrDisablingOperationInProgress;
  }

  async disableInjection(injection) {
    await this._enablingOrDisablingOperationInProgress;

    this._enablingOrDisablingOperationInProgress = new Promise(done => {
      this._disableInjectionNow(injection);
      done();
    });

    return this._enablingOrDisablingOperationInProgress;
  }

  async _disableInjectionNow(injection) {
    const { active, label, interventions } = injection;

    if (!active) {
      return;
    }

    for (const intervention of interventions) {
      for (const [customFuncName, customFunc] of Object.entries(
        this._customFunctions
      )) {
        if (customFuncName in intervention) {
          try {
            await customFunc.disable(injection, intervention);
          } catch (e) {
            console.trace(
              `Error while disabling custom function ${customFuncName} for ${label}:`,
              e
            );
          }
        }
      }

      if ("content_scripts" in intervention) {
        const ids = intervention._registrations.map(s => s.id);
        await browser.scripting.unregisterContentScripts({ ids });
      }
    }

    injection.active = false;
  }
}

module.exports = Injections;
