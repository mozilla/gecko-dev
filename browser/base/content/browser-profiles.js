/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is loaded into the browser window scope.
/* eslint-env mozilla/browser-window */

var gProfiles = {
  async init() {
    this.handleEvent.bind(this);
    this.launchProfile.bind(this);
    this.toggleProfileButtonVisibility.bind(this);
    this.updateView.bind(this);

    this.bundle = Services.strings.createBundle(
      "chrome://browser/locale/browser.properties"
    );

    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "PROFILES_ENABLED",
      "browser.profiles.enabled",
      false,
      this.toggleProfileButtonVisibility.bind(this)
    );

    if (!this.PROFILES_ENABLED) {
      return;
    }

    await this.toggleProfileButtonVisibility();
  },

  async toggleProfileButtonVisibility() {
    let profilesButton = PanelMultiView.getViewNode(
      document,
      "appMenu-profiles-button"
    );
    let subview = PanelMultiView.getViewNode(document, "PanelUI-profiles");

    profilesButton.hidden = !this.PROFILES_ENABLED;

    if (!this.PROFILES_ENABLED) {
      document.l10n.setAttributes(profilesButton, "appmenu-profiles");
      profilesButton.classList.remove("subviewbutton-iconic");
      profilesButton.removeEventListener("command", this);
      subview.removeEventListener("command", this);
      return;
    }
    profilesButton.addEventListener("command", this);
    subview.addEventListener("command", this);

    // If the feature is preffed on, but we haven't created profiles yet, the
    // service will not be initialized.
    let profiles = SelectableProfileService.initialized
      ? await SelectableProfileService.getAllProfiles()
      : [];
    if (profiles.length < 2) {
      profilesButton.classList.remove("subviewbutton-iconic");
      document.l10n.setAttributes(profilesButton, "appmenu-profiles");
      return;
    }

    let { themeBg, themeFg } = SelectableProfileService.currentProfile.theme;
    profilesButton.style.cssText = `--themeBg: ${themeBg}; --themeFg: ${themeFg};`;

    profilesButton.classList.add("subviewbutton-iconic");
    profilesButton.setAttribute(
      "label",
      SelectableProfileService.currentProfile.name
    );
    let avatar = SelectableProfileService.currentProfile.avatar;
    profilesButton.setAttribute(
      "image",
      `chrome://browser/content/profiles/assets/16_${avatar}.svg`
    );
  },

  updateView(panel) {
    this.populateSubView();
    PanelUI.showSubView("PanelUI-profiles", panel);
  },

  // Note: Not async because the browser-sets.js handler is not async.
  // This will be an issue when we add menubar menuitems.
  launchProfile(aEvent) {
    SelectableProfileService.getProfile(
      aEvent.target.getAttribute("profileid")
    ).then(profile => {
      SelectableProfileService.launchInstance(profile);
    });
  },

  async handleEvent(aEvent) {
    let id = aEvent.target.id;
    switch (aEvent.type) {
      case "command": {
        if (id == "appMenu-profiles-button") {
          this.updateView(aEvent.target);
        } else if (id == "profiles-appmenu-back-button") {
          aEvent.target.closest("panelview").panelMultiView.goBack();
          aEvent.target.blur();
        } else if (id == "profiles-edit-this-profile-button") {
          openTrustedLinkIn("about:editprofile", "tab");
        } else if (id == "profiles-manage-profiles-button") {
          // TODO: (Bug 1924827) Open in a dialog, not a tab.
          openTrustedLinkIn("about:profilemanager", "tab");
        } else if (id == "profiles-create-profile-button") {
          SelectableProfileService.createNewProfile();
        } else if (aEvent.target.classList.contains("profile-item")) {
          // moved to a helper to expose to the menubar commands
          this.launchProfile(aEvent);
        }

        break;
      }
    }
  },

  async populateSubView() {
    let profiles = [];
    let currentProfile = null;

    if (SelectableProfileService.initialized) {
      profiles = await SelectableProfileService.getAllProfiles();
      currentProfile = SelectableProfileService.currentProfile;
    }

    let backButton = PanelMultiView.getViewNode(
      document,
      "profiles-appmenu-back-button"
    );
    backButton.setAttribute(
      "aria-label",
      this.bundle.GetStringFromName("panel.back")
    );

    let currentProfileCard = PanelMultiView.getViewNode(
      document,
      "current-profile"
    );
    currentProfileCard.hidden = !(currentProfile && profiles.length > 1);

    let profilesHeader = PanelMultiView.getViewNode(
      document,
      "PanelUI-profiles-header"
    );

    let editButton = PanelMultiView.getViewNode(
      document,
      "profiles-edit-this-profile-button"
    );

    if (profiles.length < 2) {
      profilesHeader.removeAttribute("style");
      editButton.hidden = true;
    } else {
      profilesHeader.style.backgroundColor = "var(--themeBg)";
      editButton.hidden = false;
    }

    if (currentProfile && profiles.length > 1) {
      let subview = PanelMultiView.getViewNode(document, "PanelUI-profiles");
      let { themeBg, themeFg } = currentProfile.theme;
      subview.style.cssText = `--themeBg: ${themeBg}; --themeFg: ${themeFg};`;

      let headerText = PanelMultiView.getViewNode(
        document,
        "profiles-header-content"
      );
      headerText.textContent = currentProfile.name;

      let profileIconEl = PanelMultiView.getViewNode(
        document,
        "profile-icon-image"
      );
      currentProfileCard.style.cssText = `--themeFg: ${themeFg}; --themeBg: ${themeBg};`;

      let avatar = currentProfile.avatar;
      profileIconEl.style.listStyleImage = `url("chrome://browser/content/profiles/assets/80_${avatar}.svg")`;
    }

    let subtitle = PanelMultiView.getViewNode(document, "profiles-subtitle");
    subtitle.hidden = profiles.length < 2;

    let profilesList = PanelMultiView.getViewNode(document, "profiles-list");
    while (profilesList.lastElementChild) {
      profilesList.lastElementChild.remove();
    }
    for (let profile of profiles) {
      if (profile.id === SelectableProfileService.currentProfile.id) {
        continue;
      }

      let button = document.createXULElement("toolbarbutton");
      button.setAttribute("profileid", profile.id);
      button.setAttribute("label", profile.name);
      button.className = "subviewbutton subviewbutton-iconic profile-item";
      let { themeFg, themeBg } = profile.theme;
      button.style.cssText = `--themeBg: ${themeBg}; --themeFg: ${themeFg};`;
      button.setAttribute(
        "image",
        `chrome://browser/content/profiles/assets/16_${profile.avatar}.svg`
      );

      profilesList.appendChild(button);
    }
  },
};
