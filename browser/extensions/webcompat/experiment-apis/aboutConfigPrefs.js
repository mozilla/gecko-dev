/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI, ExtensionCommon, Services */

this.aboutConfigPrefs = class AboutConfigPrefsAPI extends ExtensionAPI {
  static ALLOWED_GLOBAL_PREFS = Object.freeze(
    [
      "layout.css.prefixes.transforms",
      "timer.auto_increase_timer_resolution",
    ].concat(
      Cu.isInAutomation ? ["webcompat.test.pref1", "webcompat.test.pref2"] : []
    )
  );

  getAPI(context) {
    const EventManager = ExtensionCommon.EventManager;
    const extensionIDBase = context.extension.id.split("@")[0];
    const extensionPrefNameBase = `extensions.${extensionIDBase}.`;

    function getSafePref(name) {
      if (AboutConfigPrefsAPI.ALLOWED_GLOBAL_PREFS.includes(name)) {
        return name;
      }
      return `${extensionPrefNameBase}${name}`;
    }

    return {
      aboutConfigPrefs: {
        onPrefChange: new EventManager({
          context,
          name: "aboutConfigPrefs.onUAOverridesPrefChange",
          register: (fire, name) => {
            const prefName = getSafePref(name);
            const callback = () => {
              fire.async(name).catch(() => {}); // ignore Message Manager disconnects
            };
            Services.prefs.addObserver(prefName, callback);
            return () => {
              Services.prefs.removeObserver(prefName, callback);
            };
          },
        }).api(),
        async getBranch(branchName) {
          const branch = `${extensionPrefNameBase}${branchName}.`;
          return Services.prefs.getChildList(branch).map(pref => {
            const name = pref.replace(branch, "");
            return { name, value: Services.prefs.getBoolPref(pref) };
          });
        },
        async getPref(_name) {
          const name = getSafePref(_name);
          try {
            switch (Services.prefs.getPrefType(name)) {
              case Ci.nsIPrefBranch.PREF_BOOL:
                return Services.prefs.getBoolPref(name);
              case Ci.nsIPrefBranch.PREF_INT:
                return Services.prefs.getIntPref(name);
              case Ci.nsIPrefBranch.PREF_STRING:
                return Services.prefs.getStringPref(name);
            }
          } catch (_) {}
          return undefined;
        },
        async setPref(_name, value) {
          const name = getSafePref(_name);
          switch (typeof value) {
            case "boolean":
              Services.prefs.setBoolPref(name, value);
              break;
            case "number":
              Services.prefs.setIntPref(name, value);
              break;
            case "string":
              Services.prefs.setStringPref(name, value);
              break;
          }
        },
      },
    };
  }
};
