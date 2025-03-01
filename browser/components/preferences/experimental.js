/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from preferences.js */

var gExperimentalPane = {
  inited: false,
  _featureGatesContainer: null,
  _boundRestartObserver: null,
  _observedPrefs: [],
  _shouldPromptForRestart: true,

  _featureGatePrefTypeToPrefServiceType(featureGatePrefType) {
    if (featureGatePrefType != "boolean") {
      throw new Error("Only boolean FeatureGates are supported");
    }
    return "bool";
  },

  async _observeRestart(aSubject, aTopic, aData) {
    if (!this._shouldPromptForRestart) {
      return;
    }
    let prefValue = Services.prefs.getBoolPref(aData);
    let buttonIndex = await confirmRestartPrompt(prefValue, 1, true, false);
    if (buttonIndex == CONFIRM_RESTART_PROMPT_RESTART_NOW) {
      Services.startup.quit(
        Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart
      );
      return;
    }
    this._shouldPromptForRestart = false;
    Services.prefs.setBoolPref(aData, !prefValue);
    this._shouldPromptForRestart = true;
  },

  addPrefObserver(name, fn) {
    this._observedPrefs.push({ name, fn });
    Services.prefs.addObserver(name, fn);
  },

  removePrefObservers() {
    for (let { name, fn } of this._observedPrefs) {
      Services.prefs.removeObserver(name, fn);
    }
    this._observedPrefs = [];
  },

  // Reset the features to their default values
  async resetAllFeatures() {
    let features = await gExperimentalPane.getFeatures();
    for (let feature of features) {
      Services.prefs.setBoolPref(feature.preference, feature.defaultValue);
    }
  },

  async getFeatures() {
    let searchParams = new URLSearchParams(document.documentURIObject.query);
    let definitionsUrl = searchParams.get("definitionsUrl");
    let features = await FeatureGate.all(definitionsUrl);
    return features.filter(f => f.isPublic);
  },

  async init() {
    if (this.inited) {
      return;
    }
    this.inited = true;

    let features = await this.getFeatures();
    let shouldHide = !features.length;
    document.getElementById("category-experimental").hidden = shouldHide;
    // Cache the visibility so we can show it quicker in subsequent loads.
    Services.prefs.setBoolPref(
      "browser.preferences.experimental.hidden",
      shouldHide
    );
    if (shouldHide) {
      // Remove the 'experimental' category if there are no available features
      document.getElementById("firefoxExperimentalCategory").remove();
      if (
        document.getElementById("categories").selectedItem?.id ==
        "category-experimental"
      ) {
        // Leave the 'experimental' category if there are no available features
        gotoPref("general");
        return;
      }
    }

    setEventListener(
      "experimentalCategory-reset",
      "click",
      gExperimentalPane.resetAllFeatures
    );

    window.addEventListener("unload", () => this.removePrefObservers());
    this._featureGatesContainer = document.getElementById(
      "pane-experimental-featureGates"
    );
    this._boundRestartObserver = this._observeRestart.bind(this);
    let frag = document.createDocumentFragment();
    let groups = new Map();
    for (let feature of features) {
      if (!groups.has(feature.group)) {
        groups.set(feature.group, []);
      }
      groups.get(feature.group).push(feature);
    }
    for (let [group, groupFeatures] of groups) {
      let card = document.createElement("moz-card");
      card.classList.add("featureGate");
      let fieldset = document.createElement("moz-fieldset");
      document.l10n.setAttributes(fieldset, group);
      card.append(fieldset);
      for (let feature of groupFeatures) {
        if (Preferences.get(feature.preference)) {
          console.error(
            "Preference control already exists for experimental feature '" +
              feature.id +
              "' with preference '" +
              feature.preference +
              "'"
          );
          continue;
        }

        if (feature.restartRequired) {
          this.addPrefObserver(feature.preference, this._boundRestartObserver);
        }

        let checkbox = document.createElement("moz-checkbox");
        let description = document.createElement("div");
        description.slot = "description";
        description.id = feature.id + "-description";
        description.classList.add("featureGateDescription");
        checkbox.append(description);
        fieldset.append(checkbox);

        let descriptionLinks = feature.descriptionLinks || {};
        for (let [key, value] of Object.entries(descriptionLinks)) {
          let link = document.createElement("a");
          link.setAttribute("data-l10n-name", key);
          link.setAttribute("href", value);
          link.setAttribute("target", "_blank");
          description.append(link);
        }
        document.l10n.setAttributes(description, feature.description);
        checkbox.setAttribute("preference", feature.preference);
        checkbox.id = feature.id;
        checkbox.setAttribute("aria-describedby", description.id);
        document.l10n.setAttributes(checkbox, feature.title);
        let extraTemplate = document.getElementById(`template-${feature.id}`);
        if (extraTemplate) {
          fieldset.appendChild(extraTemplate.content.cloneNode(true));
        }
        let preference = Preferences.add({
          id: feature.preference,
          type: gExperimentalPane._featureGatePrefTypeToPrefServiceType(
            feature.type
          ),
        });
        preference.setElementValue(checkbox);
      }

      frag.append(card);
    }
    this._featureGatesContainer.appendChild(frag);

    Services.obs.notifyObservers(window, "experimental-pane-loaded");
  },
};
