/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  actionTypes as at,
  actionCreators as ac,
} from "resource://newtab/common/Actions.mjs";

export const PREFERENCES_LOADED_EVENT = "home-pane-loaded";

// These "section" objects are formatted in a way to be similar to the ones from
// SectionsManager to construct the preferences view.
const PREFS_FOR_SETTINGS = () => [
  {
    id: "search",
    pref: {
      feed: "showSearch",
      titleString: "home-prefs-search-header",
    },
  },
  {
    id: "weather",
    pref: {
      feed: "showWeather",
      titleString: "home-prefs-weather-header",
      descString: "home-prefs-weather-description",
      learnMore: {
        link: {
          href: "https://support.mozilla.org/kb/customize-items-on-firefox-new-tab-page",
          id: "home-prefs-weather-learn-more-link",
        },
      },
    },
    eventSource: "WEATHER",
    shouldHidePref: !Services.prefs.getBoolPref(
      "browser.newtabpage.activity-stream.system.showWeather",
      false
    ),
  },
  {
    id: "trending-searches",
    pref: {
      feed: "trendingSearch.enabled",
      titleString: "home-prefs-trending-search-header",
      descString: "home-prefs-trending-search-description",
    },
    eventSource: "TRENDING_SEARCH",
    shouldHidePref:
      // Hide if Trending Search experiment is not enabled for this user
      !Services.prefs.getBoolPref(
        "browser.newtabpage.activity-stream.system.trendingSearch.enabled",
        false
      ) ||
      // Also hide if it's enabled but the user doesn't have Google as their default search engine
      (Services.prefs.getBoolPref(
        "browser.newtabpage.activity-stream.system.trendingSearch.enabled",
        false
      ) &&
        Services.prefs.getStringPref("browser.urlbar.placeholderName", "") !==
          "Google"),
  },
  {
    id: "topsites",
    pref: {
      feed: "feeds.topsites",
      titleString: "home-prefs-shortcuts-header",
      descString: "home-prefs-shortcuts-description",
      nestedPrefs: [
        {
          name: "showSponsoredTopSites",
          titleString: "home-prefs-shortcuts-by-option-sponsored",
          eventSource: "SPONSORED_TOP_SITES",
          // Hide this nested pref if "Support Firefox" checkbox is enabled
          shouldHidePref: Services.prefs.getBoolPref(
            "browser.newtabpage.activity-stream.system.showSponsoredCheckboxes",
            false
          ),
        },
      ],
    },
    maxRows: 4,
    rowsPref: "topSitesRows",
    eventSource: "TOP_SITES",
  },
  {
    id: "topstories",
    pref: {
      feed: "feeds.section.topstories",
      titleString: {
        id: "home-prefs-recommended-by-header-generic",
      },
      descString: {
        id: "home-prefs-recommended-by-description-generic",
      },
      nestedPrefs: [
        ...(Services.prefs.getBoolPref(
          "browser.newtabpage.activity-stream.system.showSponsored",
          true
        ) && // Hide this nested pref if "Support Firefox" checkbox is enabled
        !Services.prefs.getBoolPref(
          "browser.newtabpage.activity-stream.system.showSponsoredCheckboxes",
          false
        )
          ? [
              {
                name: "showSponsored",
                titleString:
                  "home-prefs-recommended-by-option-sponsored-stories",
                icon: "icon-info",
                eventSource: "POCKET_SPOCS",
              },
            ]
          : []),
      ],
    },
    shouldHidePref: !Services.prefs.getBoolPref(
      "browser.newtabpage.activity-stream.feeds.system.topstories",
      true
    ),
    eventSource: "TOP_STORIES",
  },
  {
    id: "support-firefox",
    pref: {
      feed: "showSponsoredCheckboxes",
      titleString: "home-prefs-support-firefox-header",
      nestedPrefs: [
        {
          name: "showSponsoredTopSites",
          titleString: "home-prefs-shortcuts-by-option-sponsored",
          eventSource: "SPONSORED_TOP_SITES",
        },
        {
          name: "showSponsored",
          titleString: "home-prefs-recommended-by-option-sponsored-stories",
          eventSource: "POCKET_SPOCS",
          shouldHidePref: !Services.prefs.getBoolPref(
            "browser.newtabpage.activity-stream.feeds.system.topstories",
            true
          ),
          shouldDisablePref: !Services.prefs.getBoolPref(
            "browser.newtabpage.activity-stream.feeds.section.topstories",
            true
          ),
        },
      ],
    },
    shouldHidePref: !Services.prefs.getBoolPref(
      "browser.newtabpage.activity-stream.system.showSponsoredCheckboxes",
      false
    ),
  },
];

export class AboutPreferences {
  init() {
    Services.obs.addObserver(this, PREFERENCES_LOADED_EVENT);
  }

  uninit() {
    Services.obs.removeObserver(this, PREFERENCES_LOADED_EVENT);
  }

  onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.init();
        break;
      case at.UNINIT:
        this.uninit();
        break;
      case at.SETTINGS_OPEN:
        action._target.browser.ownerGlobal.openPreferences("paneHome");
        break;
      // This is used to open the web extension settings page for an extension
      case at.OPEN_WEBEXT_SETTINGS:
        action._target.browser.ownerGlobal.BrowserAddonUI.openAddonsMgr(
          `addons://detail/${encodeURIComponent(action.data)}`
        );
        break;
    }
  }

  setupUserEvent(element, eventSource) {
    element.addEventListener("command", e => {
      const { checked } = e.target;
      if (typeof checked === "boolean") {
        this.store.dispatch(
          ac.UserEvent({
            event: "PREF_CHANGED",
            source: eventSource,
            value: { status: checked, menu_source: "ABOUT_PREFERENCES" },
          })
        );
      }
    });
  }

  observe(window) {
    const { document, Preferences } = window;

    // Extract just the "Recent activity" pref info from SectionsManager as we have everything else already
    const highlights = this.store
      .getState()
      .Sections.find(el => el.id === "highlights");

    const allSections = [...PREFS_FOR_SETTINGS(), highlights];

    // Render the preferences
    allSections.forEach(pref => {
      this.renderPreferenceSection(pref, document, Preferences);
    });

    // Update the visibility of the Restore Defaults button based on checked prefs
    this.toggleRestoreDefaults(window.gHomePane);
  }

  /**
   * Render a single preference with all the details, e.g. description, links,
   * more granular preferences.
   *
   * @param sectionData
   * @param document
   * @param Preferences
   */
  renderPreferenceSection(sectionData, document, Preferences) {
    const {
      id,
      pref: prefData,
      maxRows,
      rowsPref,
      shouldHidePref,
      eventSource,
    } = sectionData;
    const {
      feed: name,
      titleString = {},
      descString,
      nestedPrefs = [],
    } = prefData || {};

    // Helper to link a UI element to a preference for updating
    const linkPref = (element, prefName, type) => {
      const fullPref = `browser.newtabpage.activity-stream.${prefName}`;
      element.setAttribute("preference", fullPref);
      Preferences.add({ id: fullPref, type });

      // Prevent changing the UI if the preference can't be changed
      element.disabled = Preferences.get(fullPref).locked;
    };

    // Don't show any sections that we don't want to expose in preferences UI
    if (shouldHidePref) {
      return;
    }

    // Add the main preference for turning on/off a section
    const sectionVbox = document.getElementById(id);
    sectionVbox.setAttribute("data-subcategory", id);
    const checkbox = this.createAppend(document, "checkbox", sectionVbox);
    checkbox.classList.add("section-checkbox");
    // Set up a user event if we have an event source for this pref.
    if (eventSource) {
      this.setupUserEvent(checkbox, eventSource);
    }
    document.l10n.setAttributes(
      checkbox,
      this.getString(titleString),
      titleString.values
    );

    linkPref(checkbox, name, "bool");

    // Specially add a link for Weather
    if (id === "weather") {
      const hboxWithLink = this.createAppend(document, "hbox", sectionVbox);
      hboxWithLink.appendChild(checkbox);
      checkbox.classList.add("tail-with-learn-more");

      const link = this.createAppend(document, "label", hboxWithLink, {
        is: "text-link",
      });
      link.setAttribute("href", sectionData.pref.learnMore.link.href);
      document.l10n.setAttributes(link, sectionData.pref.learnMore.link.id);
    }

    // Add more details for the section (e.g., description, more prefs)
    const detailVbox = this.createAppend(document, "vbox", sectionVbox);
    detailVbox.classList.add("indent");
    if (descString) {
      const description = this.createAppend(
        document,
        "description",
        detailVbox
      );
      description.classList.add("text-deemphasized");
      document.l10n.setAttributes(
        description,
        this.getString(descString),
        descString.values
      );

      // Add a rows dropdown if we have a pref to control and a maximum
      if (rowsPref && maxRows) {
        const detailHbox = this.createAppend(document, "hbox", detailVbox);
        detailHbox.setAttribute("align", "center");
        description.setAttribute("flex", 1);
        detailHbox.appendChild(description);

        // Add box so the search tooltip is positioned correctly
        const tooltipBox = this.createAppend(document, "hbox", detailHbox);

        // Add appropriate number of localized entries to the dropdown
        const menulist = this.createAppend(document, "menulist", tooltipBox);
        menulist.setAttribute("crop", "none");
        const menupopup = this.createAppend(document, "menupopup", menulist);
        for (let num = 1; num <= maxRows; num++) {
          const item = this.createAppend(document, "menuitem", menupopup);
          document.l10n.setAttributes(item, "home-prefs-sections-rows-option", {
            num,
          });
          item.setAttribute("value", num);
        }
        linkPref(menulist, rowsPref, "int");
      }
    }

    const subChecks = [];
    const fullName = `browser.newtabpage.activity-stream.${sectionData.pref.feed}`;
    const pref = Preferences.get(fullName);

    // Add a checkbox pref for any nested preferences
    nestedPrefs.forEach(nested => {
      if (nested.shouldHidePref !== true) {
        const subcheck = this.createAppend(document, "checkbox", detailVbox);
        // Set up a user event if we have an event source for this pref.
        if (nested.eventSource) {
          this.setupUserEvent(subcheck, nested.eventSource);
        }
        document.l10n.setAttributes(subcheck, nested.titleString);

        linkPref(subcheck, nested.name, "bool");

        subChecks.push(subcheck);
        subcheck.disabled = !pref._value;
        if (nested.shouldDisablePref) {
          subcheck.disabled = nested.shouldDisablePref;
        }
        subcheck.hidden = nested.hidden;
      }
    });

    // Special cases to like the nested prefs with another pref,
    // so we can disable it real time.
    if (id === "support-firefox") {
      function setupSupportFirefoxSubCheck(triggerPref, subPref) {
        const subCheckFullName = `browser.newtabpage.activity-stream.${triggerPref}`;
        const subCheckPref = Preferences.get(subCheckFullName);

        subCheckPref?.on("change", () => {
          const showSponsoredFullName = `browser.newtabpage.activity-stream.${subPref}`;
          const showSponsoredSubcheck = subChecks.find(
            subcheck =>
              subcheck.getAttribute("preference") === showSponsoredFullName
          );
          if (showSponsoredSubcheck) {
            showSponsoredSubcheck.disabled = !Services.prefs.getBoolPref(
              subCheckFullName,
              true
            );
          }
        });
      }

      setupSupportFirefoxSubCheck("feeds.section.topstories", "showSponsored");
      setupSupportFirefoxSubCheck("feeds.topsites", "showSponsoredTopSites");
    }

    pref.on("change", () => {
      subChecks.forEach(subcheck => {
        // Update child preferences for the "Support Firefox" checkbox group
        // so that they're turned on and off at the same time.
        if (id === "support-firefox") {
          const subPref = Preferences.get(subcheck.getAttribute("preference"));
          subPref.value = pref.value;
        }

        // Disable any nested checkboxes if the parent pref is not enabled.
        subcheck.disabled = !pref._value;
      });
    });
  }

  /**
   * Update the visibility of the Restore Defaults button based on checked prefs.
   *
   * @param gHomePane
   */
  toggleRestoreDefaults(gHomePane) {
    gHomePane.toggleRestoreDefaultsBtn();
  }

  /**
   * A helper function to append XUL elements on the page.
   *
   * @param document
   * @param tag
   * @param parent
   * @param options
   */
  createAppend(document, tag, parent, options = {}) {
    return parent.appendChild(document.createXULElement(tag, options));
  }

  /**
   * Helper to get fluentIDs sometimes encase in an object
   *
   * @param message
   * @returns string
   */
  getString(message) {
    return typeof message !== "object" ? message : message.id;
  }
}
