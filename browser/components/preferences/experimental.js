/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from preferences.js */

ChromeUtils.defineESModuleGetters(this, {
  ExperimentAPI: "resource://nimbus/ExperimentAPI.sys.mjs",
  FirefoxLabs: "resource://nimbus/FirefoxLabs.sys.mjs",
});

const gExperimentalPane = {
  inited: false,
  _featureGatesContainer: null,
  _firefoxLabs: null,

  async init() {
    if (this.inited) {
      return;
    }

    this.inited = true;
    this._featureGatesContainer = document.getElementById(
      "pane-experimental-featureGates"
    );

    this._onCheckboxChanged = this._onCheckboxChanged.bind(this);
    this._onNimbusUpdate = this._onNimbusUpdate.bind(this);
    this._onStudiesEnabledChanged = this._onStudiesEnabledChanged.bind(this);
    this._resetAllFeatures = this._resetAllFeatures.bind(this);

    setEventListener(
      "experimentalCategory-reset",
      "click",
      this._resetAllFeatures
    );

    Services.obs.addObserver(
      this._onStudiesEnabledChanged,
      ExperimentAPI.STUDIES_ENABLED_CHANGED
    );
    window.addEventListener("unload", () => this._removeObservers());

    await this._maybeRenderLabsRecipes();
  },

  async _maybeRenderLabsRecipes() {
    this._firefoxLabs = await FirefoxLabs.create();

    const shouldHide = this._firefoxLabs.count === 0;
    this._setCategoryVisibility(shouldHide);

    if (shouldHide) {
      return;
    }

    const frag = document.createDocumentFragment();

    const groups = new Map();
    for (const optIn of this._firefoxLabs.all()) {
      if (!groups.has(optIn.firefoxLabsGroup)) {
        groups.set(optIn.firefoxLabsGroup, []);
      }

      groups.get(optIn.firefoxLabsGroup).push(optIn);
    }

    for (const [group, optIns] of groups) {
      const card = document.createElement("moz-card");
      card.classList.add("featureGate");

      const fieldset = document.createElement("moz-fieldset");
      document.l10n.setAttributes(fieldset, group);

      card.append(fieldset);

      for (const optIn of optIns) {
        const checkbox = document.createElement("moz-checkbox");
        checkbox.dataset.nimbusSlug = optIn.slug;
        checkbox.dataset.nimbusBranchSlug = optIn.branches[0].slug;
        const description = document.createElement("div");
        description.slot = "description";
        description.id = `${optIn.slug}-description`;
        description.classList.add("featureGateDescription");

        for (const [key, value] of Object.entries(
          optIn.firefoxLabsDescriptionLinks ?? {}
        )) {
          const link = document.createElement("a");
          link.setAttribute("data-l10n-name", key);
          link.setAttribute("href", value);
          link.setAttribute("target", "_blank");

          description.append(link);
        }

        document.l10n.setAttributes(description, optIn.firefoxLabsDescription);
        checkbox.id = optIn.slug;
        checkbox.setAttribute("aria-describedby", description.id);
        document.l10n.setAttributes(checkbox, optIn.firefoxLabsTitle);

        checkbox.checked =
          ExperimentAPI.manager.store.get(optIn.slug)?.active ?? false;
        checkbox.addEventListener("change", this._onCheckboxChanged);

        checkbox.append(description);
        fieldset.append(checkbox);
      }

      frag.append(card);
    }

    this._featureGatesContainer.appendChild(frag);

    ExperimentAPI.manager.store.on("update", this._onNimbusUpdate);

    Services.obs.notifyObservers(window, "experimental-pane-loaded");
  },

  _removeLabsRecipes() {
    ExperimentAPI.manager.store.off("update", this._onNimbusUpdate);

    this._featureGatesContainer
      .querySelectorAll(".featureGate")
      .forEach(el => el.remove());
  },

  async _onCheckboxChanged(event) {
    const target = event.target;

    const slug = target.dataset.nimbusSlug;
    const branchSlug = target.dataset.nimbusBranchSlug;

    const enrolling = !(ExperimentAPI.manager.store.get(slug)?.active ?? false);

    let shouldRestart = false;
    if (this._firefoxLabs.get(slug).requiresRestart) {
      const buttonIndex = await confirmRestartPrompt(enrolling, 1, true, false);
      shouldRestart = buttonIndex === CONFIRM_RESTART_PROMPT_RESTART_NOW;

      if (!shouldRestart) {
        // The user declined to restart, so we will not enroll in the opt-in.
        target.checked = false;
        return;
      }
    }

    // Disable the checkbox so that the user cannot interact with it during enrollment.
    target.disabled = true;

    if (enrolling) {
      await this._firefoxLabs.enroll(slug, branchSlug);
    } else {
      await this._firefoxLabs.unenroll(slug);
    }

    target.disabled = false;

    if (shouldRestart) {
      Services.startup.quit(
        Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart
      );
    }
  },

  _onNimbusUpdate(_event, { slug, active }) {
    if (this._firefoxLabs.get(slug)) {
      document.getElementById(slug).checked = active;
    }
  },

  async _onStudiesEnabledChanged() {
    const studiesEnabled = ExperimentAPI.studiesEnabled;

    if (studiesEnabled) {
      await this._maybeRenderLabsRecipes();
    } else {
      this._setCategoryVisibility(true);
      this._removeLabsRecipes();
      this._firefoxLabs = null;
    }
  },

  _removeObservers() {
    ExperimentAPI.manager.store.off("update", this._onNimbusUpdate);
    Services.obs.removeObserver(
      this._onStudiesEnabledChanged,
      ExperimentAPI.STUDIES_ENABLED_CHANGED
    );
  },

  // Reset the features to their default values
  async _resetAllFeatures() {
    for (const optIn of this._firefoxLabs.all()) {
      const enrolled =
        (await ExperimentAPI.manager.store.get(optIn.slug)?.active) ?? false;
      if (enrolled) {
        await this._firefoxLabs.unenroll(optIn.slug);
      }
    }
  },

  _setCategoryVisibility(shouldHide) {
    document.getElementById("category-experimental").hidden = shouldHide;

    // Cache the visibility so we can show it quicker in subsequent loads.
    Services.prefs.setBoolPref(
      "browser.preferences.experimental.hidden",
      shouldHide
    );

    if (
      shouldHide &&
      document.getElementById("categories").selectedItem?.id ==
        "category-experimental"
    ) {
      // Leave the 'experimental' category if there are no available features
      gotoPref("general");
    }
  },
};
