/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Eventually, make this a messaging system
// provider instead of adding these message
// into OnboardingMessageProvider.sys.mjs
const PDFJS_PREF = "browser.pdfjs.feature-tour";
// Empty screens are included as placeholders to ensure step
// indicator shows the correct number of total steps in the tour
const ONE_DAY_IN_MS = 24 * 60 * 60 * 1000;

// Generate a JEXL targeting string based on the `complete` property being true
// in a given Feature Callout tour progress preference value (which is JSON).
const matchIncompleteTargeting = (prefName, defaultValue = false) => {
  // regExpMatch() is a JEXL filter expression. Here we check if 'complete'
  // exists in the pref's value, and returns true if the tour is incomplete.
  const prefVal = `'${prefName}' | preferenceValue`;
  // prefVal might be null if the preference doesn't exist. in this case, don't
  // try to pipe into regExpMatch.
  const completeMatch = `${prefVal} | regExpMatch('(?<=complete":)(.*)(?=})')`;
  return `((${prefVal}) ? ((${completeMatch}) ? (${completeMatch}[1] != "true") : ${String(
    defaultValue
  )}) : ${String(defaultValue)})`;
};

// Generate a JEXL targeting string based on the current screen id found in a
// given Feature Callout tour progress preference.
const matchCurrentScreenTargeting = (prefName, screenIdRegEx = ".*") => {
  // regExpMatch() is a JEXL filter expression. Here we check if 'screen' exists
  // in the pref's value, and if it matches the screenIdRegEx. Returns
  // null otherwise.
  const prefVal = `'${prefName}' | preferenceValue`;
  const screenMatch = `${prefVal} | regExpMatch('(?<=screen"\s*:)\s*"(${screenIdRegEx})(?="\s*,)')`;
  const screenValMatches = `(${screenMatch}) ? !!(${screenMatch}[1]) : false`;
  return `(${screenValMatches})`;
};

/**
 * add24HourImpressionJEXLTargeting -
 * Creates a "hasn't been viewed in > 24 hours"
 * JEXL string and adds it to each message specified
 *
 * @param {array} messageIds - IDs of messages that the targeting string will be added to
 * @param {string} prefix - The prefix of messageIDs that will used to create the JEXL string
 * @param {array} messages - The array of messages that will be edited
 * @returns {array} - The array of messages with the appropriate targeting strings edited
 */
function add24HourImpressionJEXLTargeting(
  messageIds,
  prefix,
  uneditedMessages
) {
  let noImpressionsIn24HoursString = uneditedMessages
    .filter(message => message.id.startsWith(prefix))
    .map(
      message =>
        // If the last impression is null or if epoch time
        // of the impression is < current time - 24hours worth of MS
        `(messageImpressions.${message.id}[messageImpressions.${
          message.id
        } | length - 1] == null || messageImpressions.${
          message.id
        }[messageImpressions.${message.id} | length - 1] < ${
          Date.now() - ONE_DAY_IN_MS
        })`
    )
    .join(" && ");

  // We're appending the string here instead of using
  // template strings to avoid a recursion error from
  // using the 'messages' variable within itself
  return uneditedMessages.map(message => {
    if (messageIds.includes(message.id)) {
      message.targeting += `&& ${noImpressionsIn24HoursString}`;
    }

    return message;
  });
}

// Exporting the about:firefoxview messages as a method here
// acts as a safety guard against mutations of the original objects
const MESSAGES = () => {
  let messages = [
    {
      id: "TAB_GROUP_ONBOARDING_CALLOUT",
      template: "feature_callout",
      groups: ["cfr"],
      content: {
        id: "TAB_GROUP_ONBOARDING_CALLOUT",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        screens: [
          {
            id: "TAB_GROUP_ONBOARDING_CALLOUT_HORIZONTAL",
            anchors: [
              {
                selector:
                  "#tabbrowser-tabs:not([overflow]) .tab-content[selected]:not([pinned])",
                panel_position: {
                  anchor_attachment: "bottomcenter",
                  callout_attachment: "topright",
                },
              },
              {
                selector:
                  "#tabbrowser-tabs:not([overflow]) tab:not([pinned]):last-of-type",
                panel_position: {
                  anchor_attachment: "bottomcenter",
                  callout_attachment: "topright",
                },
              },
              {
                selector:
                  "#tabbrowser-tabs:not([overflow]) #tabs-newtab-button",
                panel_position: {
                  anchor_attachment: "bottomcenter",
                  callout_attachment: "topright",
                },
              },
              {
                selector: "#tabbrowser-tabs",
                panel_position: {
                  anchor_attachment: "bottomcenter",
                  callout_attachment: "topright",
                },
              },
            ],
            content: {
              position: "callout",
              width: "333px",
              padding: 16,
              logo: {
                imageURL:
                  "chrome://browser/content/asrouter/assets/tabgroups/hort-animated-light.svg",
                darkModeImageURL:
                  "chrome://browser/content/asrouter/assets/tabgroups/hort-animated-dark.svg",
                reducedMotionImageURL:
                  "chrome://browser/content/asrouter/assets/tabgroups/hort-static-light.svg",
                darkModeReducedMotionImageURL:
                  "chrome://browser/content/asrouter/assets/tabgroups/hort-static-dark.svg",
                height: "172px",
                width: "300px",
              },
              title: {
                string_id: "tab-groups-onboarding-feature-callout-title",
              },
              subtitle: {
                string_id: "tab-groups-onboarding-feature-callout-subtitle",
              },
              dismiss_button: {
                action: {
                  dismiss: true,
                },
                background: true,
                size: "small",
                marginInline: "0 20px",
                marginBlock: "20px 0",
              },
            },
          },
        ],
      },
      targeting:
        "tabsClosedCount >= 1 && currentTabsOpen >= 8 && ('browser.tabs.groups.enabled' | preferenceValue) && (!'sidebar.verticalTabs' | preferenceValue) && currentTabGroups == 0 && savedTabGroups == 0 && !activeNotifications",
      trigger: {
        id: "nthTabClosed",
      },
      frequency: {
        lifetime: 1,
      },
      skip_in_tests: "it's not tested in automation",
    },
    {
      id: "TAB_GROUP_ONBOARDING_CALLOUT",
      template: "feature_callout",
      groups: ["cfr"],
      content: {
        id: "TAB_GROUP_ONBOARDING_CALLOUT",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        screens: [
          {
            id: "TAB_GROUP_ONBOARDING_CALLOUT_VERTICAL",
            anchors: [
              {
                selector:
                  "#tabbrowser-tabs:not([overflow]) .tab-content[selected]:not([pinned])",
                panel_position: {
                  anchor_attachment: "rightcenter",
                  callout_attachment: "topleft",
                },
              },
              {
                selector:
                  "#tabbrowser-tabs:not([overflow]) tab:not([pinned]):last-of-type",
                panel_position: {
                  anchor_attachment: "rightcenter",
                  callout_attachment: "topleft",
                },
              },
              {
                selector:
                  "#tabbrowser-tabs:not([overflow]) #tabs-newtab-button",
                panel_position: {
                  anchor_attachment: "rightcenter",
                  callout_attachment: "topleft",
                },
              },
              {
                selector: "#tabbrowser-tabs",
                panel_position: {
                  anchor_attachment: "rightcenter",
                  callout_attachment: "topleft",
                },
              },
            ],
            content: {
              position: "callout",
              width: "333px",
              padding: 16,
              logo: {
                imageURL:
                  "chrome://browser/content/asrouter/assets/tabgroups/vert-animated-light.svg",
                darkModeImageURL:
                  "chrome://browser/content/asrouter/assets/tabgroups/vert-animated-dark.svg",
                reducedMotionImageURL:
                  "chrome://browser/content/asrouter/assets/tabgroups/vert-static-light.svg",
                darkModeReducedMotionImageURL:
                  "chrome://browser/content/asrouter/assets/tabgroups/vert-static-dark.svg",
                height: "172px",
                width: "300px",
              },
              title: {
                string_id: "tab-groups-onboarding-feature-callout-title",
              },
              subtitle: {
                string_id: "tab-groups-onboarding-feature-callout-subtitle",
              },
              dismiss_button: {
                action: {
                  dismiss: true,
                },
                background: true,
                size: "small",
                marginInline: "0 20px",
                marginBlock: "20px 0",
              },
            },
          },
        ],
      },
      targeting:
        "tabsClosedCount >= 1 && currentTabsOpen >= 8 && ('browser.tabs.groups.enabled' | preferenceValue) && ('sidebar.revamp' | preferenceValue) && ('sidebar.verticalTabs' | preferenceValue) && currentTabGroups == 0 && savedTabGroups == 0 && !activeNotifications",
      trigger: {
        id: "nthTabClosed",
      },
      frequency: {
        lifetime: 1,
      },
      skip_in_tests: "it's not tested in automation",
    },
    {
      id: "DESKTOP_TO_MOBILE_ADOPTION_SIGNED_INTO_ACCOUNT_NON_EU",
      template: "feature_callout",
      groups: ["cfr"],
      content: {
        id: "DESKTOP_TO_MOBILE_ADOPTION_SIGNED_INTO_ACCOUNT_NON_EU",
        padding: "16",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        disableHistoryUpdates: true,
        screens: [
          {
            id: "DESKTOP_TO_MOBILE_ADOPTION_SIGNED_INTO_ACCOUNT_NON_EU",
            anchors: [
              {
                selector: "#fxa-toolbar-menu-button",
                panel_position: {
                  anchor_attachment: "bottomcenter",
                  callout_attachment: "topright",
                },
              },
            ],
            content: {
              position: "callout",
              width: "400px",
              padding: 16,
              title: {
                string_id: "desktop-to-mobile-headline",
                marginInline: "4px 0",
              },
              logo: {
                height: "128px",
                imageURL:
                  "chrome://browser/content/asrouter/assets/desktop-to-mobile-banner.svg",
              },
              subtitle: {
                string_id: "desktop-to-mobile-subtitle",
                marginBlock: "-44px 0",
                marginInline: "84px 0",
              },
              title_logo: {
                height: "103px",
                width: "75px",
                alignment: "top",
                marginBlock: "40px 0",
                marginInline: "32px",
                imageURL:
                  "chrome://browser/content/asrouter/assets/desktop-to-mobile-non-eu-QR.svg",
                alt: {
                  string_id: "desktop-to-mobile-qr-code-alt",
                },
              },
              additional_button: {
                action: {
                  dismiss: true,
                },
                label: {
                  string_id: "dismiss-button-label",
                  fontWeight: "590",
                  fontSize: "11px",
                },
                style: "secondary",
              },
              secondary_button: {
                action: {
                  type: "OPEN_ABOUT_PAGE",
                  data: {
                    args: "preferences?action=pair#sync",
                    where: "tab",
                  },
                  dismiss: true,
                },
                label: {
                  string_id: "sync-to-mobile-button-label",
                  fontWeight: "590",
                  fontSize: "11px",
                },
                style: "secondary",
              },
            },
          },
        ],
      },
      frequency: {
        custom: [
          {
            cap: 1,
            period: 2628000000,
          },
        ],
        lifetime: 3,
      },
      trigger: {
        id: "defaultBrowserCheck",
      },
      targeting:
        "(region in ['CA', 'US']) && isFxASignedIn && previousSessionEnd && !willShowDefaultPrompt && !activeNotifications && userPrefs.cfrFeatures && !(sync || {}).mobileDevices",
      skip_in_tests: "it's not tested in automation",
    },
    {
      id: "DESKTOP_TO_MOBILE_ADOPTION_SIGNED_INTO_ACCOUNT_EU",
      template: "feature_callout",
      groups: ["cfr"],
      content: {
        id: "DESKTOP_TO_MOBILE_ADOPTION_SIGNED_INTO_ACCOUNT_EU",
        padding: "16",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        disableHistoryUpdates: true,
        screens: [
          {
            id: "DESKTOP_TO_MOBILE_ADOPTION_SIGNED_INTO_ACCOUNT_EU",
            anchors: [
              {
                selector: "#fxa-toolbar-menu-button",
                panel_position: {
                  anchor_attachment: "bottomcenter",
                  callout_attachment: "topright",
                },
              },
            ],
            content: {
              position: "callout",
              width: "400px",
              padding: 16,
              title: {
                string_id: "desktop-to-mobile-headline",
                marginInline: "4px 0",
              },
              logo: {
                height: "128px",
                imageURL:
                  "chrome://browser/content/asrouter/assets/desktop-to-mobile-banner.svg",
              },
              subtitle: {
                string_id: "desktop-to-mobile-subtitle",
                marginBlock: "-44px 0",
                marginInline: "84px 0",
              },
              title_logo: {
                height: "103px",
                width: "75px",
                alignment: "top",
                marginBlock: "40px 0",
                marginInline: "32px",
                imageURL:
                  "chrome://browser/content/asrouter/assets/desktop-to-mobile-eu-QR.svg",
                alt: {
                  string_id: "desktop-to-mobile-qr-code-alt",
                },
              },
              additional_button: {
                action: {
                  dismiss: true,
                },
                label: {
                  string_id: "dismiss-button-label",
                  fontWeight: "590",
                  fontSize: "11px",
                },
                style: "secondary",
              },
              secondary_button: {
                action: {
                  type: "OPEN_ABOUT_PAGE",
                  data: {
                    args: "preferences?action=pair#sync",
                    where: "tab",
                  },
                  dismiss: true,
                },
                label: {
                  string_id: "sync-to-mobile-button-label",
                  fontWeight: "590",
                  fontSize: "11px",
                },
                style: "secondary",
              },
            },
          },
        ],
      },
      frequency: {
        custom: [
          {
            cap: 1,
            period: 2628000000,
          },
        ],
        lifetime: 3,
      },
      trigger: {
        id: "defaultBrowserCheck",
      },
      targeting:
        "(locale in ['de', 'en-CA', 'en-GB', 'en-US', 'fr']) && (region in ['DE', 'FR', 'GB']) && isFxASignedIn && previousSessionEnd && !willShowDefaultPrompt && !activeNotifications && userPrefs.cfrFeatures && !(sync || {}).mobileDevices",
      skip_in_tests: "it's not tested in automation",
    },
    // Appears the first time a user uses the "save and close" action on a tab group,
    // anchored to the alltabs-button. Will only show if at least an hour has passed
    // since the CREATE_TAB_GROUP callout showed.
    {
      id: "SAVE_TAB_GROUP_ONBOARDING_CALLOUT",
      template: "feature_callout",
      groups: [],
      content: {
        id: "SAVE_TAB_GROUP_ONBOARDING_CALLOUT",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        screens: [
          {
            id: "SAVE_TAB_GROUP_ONBOARDING_CALLOUT_ALLTABS_BUTTON",
            anchors: [
              {
                selector: "#alltabs-button",
                panel_position: {
                  anchor_attachment: "bottomcenter",
                  callout_attachment: "topright",
                },
              },
            ],
            content: {
              position: "callout",
              padding: 16,
              width: "330px",
              title_logo: {
                imageURL:
                  "chrome://browser/content/asrouter/assets/smiling-fox-icon.svg",
                width: "24px",
                height: "24px",
                marginInline: "0 16px",
                alignment: "top",
              },
              title: {
                string_id: "tab-groups-onboarding-saved-groups-title-2",
              },
              primary_button: {
                label: {
                  string_id: "tab-groups-onboarding-dismiss",
                },
                action: {
                  dismiss: true,
                },
              },
            },
          },
        ],
      },
      targeting:
        "('browser.tabs.groups.enabled' | preferenceValue) && userPrefs.cfrFeatures && (!messageImpressions.CREATE_TAB_GROUP_ONBOARDING_CALLOUT[messageImpressions.CREATE_TAB_GROUP_ONBOARDING_CALLOUT | length - 1] || messageImpressions.CREATE_TAB_GROUP_ONBOARDING_CALLOUT[messageImpressions.CREATE_TAB_GROUP_ONBOARDING_CALLOUT | length - 1] < currentDate|date - 3600000) && alltabsButtonAreaType != null",
      trigger: {
        id: "tabGroupSaved",
      },
      frequency: {
        lifetime: 1,
      },
      skip_in_tests: "it's not tested in automation",
    },
    // Appears the first time a user uses the "save and close" action on a tab group,
    // if the alltabs-button has been removed. Anchored to the urlbar. Will only show
    // if CREATE_TAB_GROUP callout has not shown, or at least an hour has passed since
    // the CREATE_TAB_GROUP callout showed.
    {
      id: "SAVE_TAB_GROUP_ONBOARDING_CALLOUT",
      template: "feature_callout",
      groups: [],
      content: {
        id: "SAVE_TAB_GROUP_ONBOARDING_CALLOUT",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        screens: [
          {
            id: "SAVE_TAB_GROUP_ONBOARDING_CALLOUT_URLBAR",
            anchors: [
              {
                selector: ".urlbar-input-box",
                panel_position: {
                  anchor_attachment: "bottomcenter",
                  callout_attachment: "topcenter",
                },
              },
            ],
            content: {
              position: "callout",
              padding: 16,
              width: "330px",
              title_logo: {
                imageURL:
                  "chrome://browser/content/asrouter/assets/smiling-fox-icon.svg",
                width: "24px",
                height: "24px",
                marginInline: "0 16px",
                alignment: "top",
              },
              title: {
                string_id:
                  "tab-groups-onboarding-saved-groups-no-alltabs-button-title-2",
              },
              primary_button: {
                label: {
                  string_id: "tab-groups-onboarding-dismiss",
                },
                action: {
                  dismiss: true,
                },
              },
            },
          },
        ],
      },
      targeting:
        "('browser.tabs.groups.enabled' | preferenceValue) && userPrefs.cfrFeatures && (!messageImpressions.CREATE_TAB_GROUP_ONBOARDING_CALLOUT[messageImpressions.CREATE_TAB_GROUP_ONBOARDING_CALLOUT | length - 1] || messageImpressions.CREATE_TAB_GROUP_ONBOARDING_CALLOUT[messageImpressions.CREATE_TAB_GROUP_ONBOARDING_CALLOUT | length - 1] < currentDate|date - 3600000) && alltabsButtonAreaType == null",
      trigger: {
        id: "tabGroupSaved",
      },
      frequency: {
        lifetime: 1,
      },
      skip_in_tests: "it's not tested in automation",
    },
    // Appears the first time a user creates a tab group, after clicking the "Done"
    // button. Anchored to the alltabs-button. Will only show if the SAVE_TAB_GROUP
    // callout has not shown, or if at least an hour has passed
    // since the SAVE_TAB_GROUP callout showed.
    {
      id: "CREATE_TAB_GROUP_ONBOARDING_CALLOUT",
      template: "feature_callout",
      groups: [],
      content: {
        id: "CREATE_TAB_GROUP_ONBOARDING_CALLOUT",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        screens: [
          {
            id: "CREATE_TAB_GROUP_ONBOARDING_CALLOUT_ALLTABS_BUTTON",
            anchors: [
              {
                selector: "#alltabs-button",
                panel_position: {
                  anchor_attachment: "bottomcenter",
                  callout_attachment: "topright",
                },
              },
            ],
            content: {
              position: "callout",
              padding: 16,
              width: "330px",
              title_logo: {
                imageURL:
                  "chrome://browser/content/asrouter/assets/smiling-fox-icon.svg",
                width: "24px",
                height: "24px",
                marginInline: "0 16px",
                alignment: "top",
              },
              title: {
                string_id: "tab-groups-onboarding-create-group-title-2",
              },
              primary_button: {
                label: {
                  string_id: "tab-groups-onboarding-dismiss",
                },
                action: {
                  dismiss: true,
                },
              },
            },
          },
        ],
      },
      targeting:
        "('browser.tabs.groups.enabled' | preferenceValue) && userPrefs.cfrFeatures && (!messageImpressions.SAVE_TAB_GROUP_ONBOARDING_CALLOUT[messageImpressions.SAVE_TAB_GROUP_ONBOARDING_CALLOUT | length - 1] || messageImpressions.SAVE_TAB_GROUP_ONBOARDING_CALLOUT[messageImpressions.SAVE_TAB_GROUP_ONBOARDING_CALLOUT | length - 1] < currentDate|date - 3600000) && alltabsButtonAreaType != null",
      trigger: {
        id: "tabGroupCreated",
      },
      frequency: {
        lifetime: 1,
      },
      skip_in_tests: "it's not tested in automation",
    },
    // Appears the first time a user creates a tab group, after clicking the "Done"
    // button, if the alltabs-button has been removed. Anchored to the urlbar. Will
    // only show if the SAVE_TAB_GROUP callout has not shown, or if at least an hour
    // has passed since the SAVE_TAB_GROUP callout showed.
    {
      id: "CREATE_TAB_GROUP_ONBOARDING_CALLOUT",
      template: "feature_callout",
      groups: [],
      content: {
        id: "CREATE_TAB_GROUP_ONBOARDING_CALLOUT",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        screens: [
          {
            id: "CREATE_TAB_GROUP_ONBOARDING_CALLOUT_URLBAR",
            anchors: [
              {
                selector: ".urlbar-input-box",
                panel_position: {
                  anchor_attachment: "bottomcenter",
                  callout_attachment: "topcenter",
                },
              },
            ],
            content: {
              position: "callout",
              padding: 16,
              width: "330px",
              title_logo: {
                imageURL:
                  "chrome://browser/content/asrouter/assets/smiling-fox-icon.svg",
                width: "24px",
                height: "24px",
                marginInline: "0 16px",
                alignment: "top",
              },
              title: {
                string_id:
                  "tab-groups-onboarding-create-group-no-alltabs-button-title",
              },
              primary_button: {
                label: {
                  string_id: "tab-groups-onboarding-dismiss",
                },
                action: {
                  dismiss: true,
                },
              },
            },
          },
        ],
      },
      targeting:
        "('browser.tabs.groups.enabled' | preferenceValue) && userPrefs.cfrFeatures && (!messageImpressions.SAVE_TAB_GROUP_ONBOARDING_CALLOUT[messageImpressions.SAVE_TAB_GROUP_ONBOARDING_CALLOUT | length - 1] || messageImpressions.SAVE_TAB_GROUP_ONBOARDING_CALLOUT[messageImpressions.SAVE_TAB_GROUP_ONBOARDING_CALLOUT | length - 1] < currentDate|date - 3600000) && alltabsButtonAreaType == null",
      trigger: {
        id: "tabGroupCreated",
      },
      frequency: {
        lifetime: 1,
      },
      skip_in_tests: "it's not tested in automation",
    },
    // Appears after a browser restart if Session Restore is disabled, to direct
    // users to tab groups that were saved automatically. Anchored to the alltabs-button.
    {
      id: "SESSION_RESTORE_TAB_GROUP_CALLOUT",
      template: "feature_callout",
      groups: [],
      content: {
        id: "SESSION_RESTORE_TAB_GROUP_CALLOUT",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        screens: [
          {
            id: "SESSION_RESTORE_TAB_GROUP_CALLOUT_ALLTABS_BUTTON",
            anchors: [
              {
                selector: "#alltabs-button",
                panel_position: {
                  anchor_attachment: "bottomcenter",
                  callout_attachment: "topright",
                },
              },
            ],
            content: {
              position: "callout",
              padding: 16,
              width: "330px",
              title_logo: {
                imageURL:
                  "chrome://browser/content/asrouter/assets/smiling-fox-icon.svg",
                width: "24px",
                height: "24px",
                marginInline: "0 16px",
                alignment: "top",
              },
              title: {
                string_id: "tab-groups-onboarding-session-restore-title",
              },
              primary_button: {
                label: {
                  string_id: "tab-groups-onboarding-dismiss",
                },
                action: {
                  dismiss: true,
                },
              },
            },
          },
        ],
      },
      targeting:
        "('browser.tabs.groups.enabled' | preferenceValue) && userPrefs.cfrFeatures && previousSessionEnd && ('browser.startup.page' | preferenceValue != 3) && savedTabGroups >= 1 && alltabsButtonAreaType != null",
      trigger: {
        id: "defaultBrowserCheck",
      },
      priority: 2,
      frequency: {
        lifetime: 1,
      },
      skip_in_tests: "not tested in automation",
    },
    // Appears after a browser restart if Session Restore is disabled, to direct
    // users to tab groups that were saved automatically, for users who have
    // removed the alltabs button. Anchored to the urlbar.
    {
      id: "SESSION_RESTORE_TAB_GROUP_CALLOUT",
      template: "feature_callout",
      groups: [],
      content: {
        id: "SESSION_RESTORE_TAB_GROUP_CALLOUT",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        screens: [
          {
            id: "SESSION_RESTORE_TAB_GROUP_CALLOUT_URLBAR",
            anchors: [
              {
                selector: ".urlbar-input-box",
                panel_position: {
                  anchor_attachment: "bottomcenter",
                  callout_attachment: "topcenter",
                },
              },
            ],
            content: {
              position: "callout",
              padding: 16,
              width: "330px",
              title_logo: {
                imageURL:
                  "chrome://browser/content/asrouter/assets/smiling-fox-icon.svg",
                width: "24px",
                height: "24px",
                marginInline: "0 16px",
                alignment: "top",
              },
              title: {
                string_id:
                  "tab-groups-onboarding-saved-groups-no-alltabs-button-title-2",
              },
              primary_button: {
                label: {
                  string_id: "tab-groups-onboarding-dismiss",
                },
                action: {
                  dismiss: true,
                },
              },
            },
          },
        ],
      },
      targeting:
        "('browser.tabs.groups.enabled' | preferenceValue) && userPrefs.cfrFeatures && previousSessionEnd && ('browser.startup.page' | preferenceValue != 3) && savedTabGroups >= 1 && alltabsButtonAreaType == null",
      trigger: {
        id: "defaultBrowserCheck",
      },
      priority: 2,
      frequency: {
        lifetime: 1,
      },
      skip_in_tests: "not tested in automation",
    },
    {
      id: "ADDONS_STAFF_PICK_PT_2",
      template: "feature_callout",
      groups: ["cfr"],
      content: {
        id: "ADDONS_STAFF_PICK_PT_2",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        screens: [
          {
            id: "ADDONS_STAFF_PICK_PT_2_A",
            anchors: [
              {
                selector: "#unified-extensions-button",
                panel_position: {
                  anchor_attachment: "bottomcenter",
                  callout_attachment: "topright",
                },
                arrow_width: "26.9",
              },
            ],
            content: {
              position: "callout",
              width: "310px",
              padding: 16,
              title_logo: {
                imageURL:
                  "chrome://browser/content/asrouter/assets/smiling-fox-icon.svg",
                width: "24px",
                height: "24px",
                marginInline: "4px 14px",
              },
              title: {
                raw: "Give your browsing a boost",
                marginInline: "0 48px",
              },
              subtitle: {
                raw: "Make browsing faster, safer, or just plain fun with Firefox add-ons. See what our staff recommends!",
                paddingInline: "34px 0",
              },
              primary_button: {
                label: {
                  raw: "Explore add-ons",
                },
                action: {
                  dismiss: true,
                  type: "OPEN_URL",
                  data: {
                    args: "https://addons.mozilla.org/en-US/firefox/collections/4757633/36d285535db74c6986abbeeed3e214/?page=1&collection_sort=added",
                    where: "tabshifted",
                  },
                },
              },
              dismiss_button: {
                action: {
                  dismiss: true,
                },
                size: "small",
                marginInline: "0 14px",
                marginBlock: "14px 0",
              },
            },
          },
        ],
      },
      targeting:
        "userPrefs.cfrAddons && userPrefs.cfrFeatures && localeLanguageCode == 'en' && ((currentDate|date - profileAgeCreated|date) / 86400000 < 28) && !screenImpressions.AW_AMO_INTRODUCE && !willShowDefaultPrompt && !activeNotifications && source == 'newtab' && previousSessionEnd",
      trigger: {
        id: "defaultBrowserCheck",
      },
      frequency: {
        lifetime: 1,
      },
    },
    {
      id: "FIREFOX_VIEW_TAB_PICKUP_REMINDER",
      template: "feature_callout",
      content: {
        id: "FIREFOX_VIEW_TAB_PICKUP_REMINDER",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        disableHistoryUpdates: true,
        screens: [
          {
            id: "FIREFOX_VIEW_TAB_PICKUP_REMINDER",
            anchors: [
              {
                selector: "#tab-pickup-container",
                arrow_position: "top",
              },
            ],
            content: {
              position: "callout",
              title: {
                string_id:
                  "continuous-onboarding-firefox-view-tab-pickup-title",
              },
              subtitle: {
                string_id:
                  "continuous-onboarding-firefox-view-tab-pickup-subtitle",
              },
              logo: {
                imageURL: "chrome://browser/content/callout-tab-pickup.svg",
                darkModeImageURL:
                  "chrome://browser/content/callout-tab-pickup-dark.svg",
                height: "128px",
              },
              primary_button: {
                label: {
                  string_id: "mr1-onboarding-get-started-primary-button-label",
                },
                style: "secondary",
                action: {
                  type: "CLICK_ELEMENT",
                  navigate: true,
                  data: {
                    selector:
                      "#tab-pickup-container button.primary:not(#error-state-button)",
                  },
                },
              },
              dismiss_button: {
                action: {
                  navigate: true,
                },
              },
              page_event_listeners: [
                {
                  params: {
                    type: "toggle",
                    selectors: "#tab-pickup-container",
                  },
                  action: { reposition: true },
                },
              ],
            },
          },
        ],
      },
      priority: 2,
      targeting: `source == "about:firefoxview" && "browser.firefox-view.view-count" | preferenceValue > 2
    && (("identity.fxaccounts.enabled" | preferenceValue == false) || !(("services.sync.engine.tabs" | preferenceValue == true) && ("services.sync.username" | preferenceValue))) && (!messageImpressions.FIREFOX_VIEW_SPOTLIGHT[messageImpressions.FIREFOX_VIEW_SPOTLIGHT | length - 1] || messageImpressions.FIREFOX_VIEW_SPOTLIGHT[messageImpressions.FIREFOX_VIEW_SPOTLIGHT | length - 1] < currentDate|date - ${ONE_DAY_IN_MS})`,
      frequency: {
        lifetime: 1,
      },
      trigger: { id: "featureCalloutCheck" },
    },
    {
      id: "PDFJS_FEATURE_TOUR_A",
      template: "feature_callout",
      content: {
        id: "PDFJS_FEATURE_TOUR",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        disableHistoryUpdates: true,
        tour_pref_name: PDFJS_PREF,
        screens: [
          {
            id: "FEATURE_CALLOUT_1_A",
            anchors: [
              {
                selector: "hbox#browser",
                arrow_position: "top-end",
                absolute_position: { top: "43px", right: "51px" },
              },
            ],
            content: {
              position: "callout",
              title: {
                string_id: "callout-pdfjs-edit-title",
              },
              subtitle: {
                string_id: "callout-pdfjs-edit-body-a",
              },
              primary_button: {
                label: {
                  string_id: "callout-pdfjs-edit-button",
                },
                style: "secondary",
                action: {
                  type: "SET_PREF",
                  data: {
                    pref: {
                      name: PDFJS_PREF,
                      value: JSON.stringify({
                        screen: "FEATURE_CALLOUT_2_A",
                        complete: false,
                      }),
                    },
                  },
                },
              },
              dismiss_button: {
                action: {
                  type: "SET_PREF",
                  data: {
                    pref: {
                      name: PDFJS_PREF,
                      value: JSON.stringify({
                        screen: "",
                        complete: true,
                      }),
                    },
                  },
                },
              },
            },
          },
          {
            id: "FEATURE_CALLOUT_2_A",
            anchors: [
              {
                selector: "hbox#browser",
                arrow_position: "top-end",
                absolute_position: { top: "43px", right: "23px" },
              },
            ],
            content: {
              position: "callout",
              title: {
                string_id: "callout-pdfjs-draw-title",
              },
              subtitle: {
                string_id: "callout-pdfjs-draw-body-a",
              },
              primary_button: {
                label: {
                  string_id: "callout-pdfjs-draw-button",
                },
                style: "secondary",
                action: {
                  type: "SET_PREF",
                  data: {
                    pref: {
                      name: PDFJS_PREF,
                      value: JSON.stringify({
                        screen: "",
                        complete: true,
                      }),
                    },
                  },
                },
              },
              dismiss_button: {
                action: {
                  type: "SET_PREF",
                  data: {
                    pref: {
                      name: PDFJS_PREF,
                      value: JSON.stringify({
                        screen: "",
                        complete: true,
                      }),
                    },
                  },
                },
              },
            },
          },
        ],
      },
      priority: 1,
      targeting: `source == "open" && ${matchCurrentScreenTargeting(
        PDFJS_PREF,
        "FEATURE_CALLOUT_[0-9]_A"
      )} && ${matchIncompleteTargeting(PDFJS_PREF)}`,
      trigger: { id: "pdfJsFeatureCalloutCheck" },
    },
    {
      id: "PDFJS_FEATURE_TOUR_B",
      template: "feature_callout",
      content: {
        id: "PDFJS_FEATURE_TOUR",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        disableHistoryUpdates: true,
        tour_pref_name: PDFJS_PREF,
        screens: [
          {
            id: "FEATURE_CALLOUT_1_B",
            anchors: [
              {
                selector: "hbox#browser",
                arrow_position: "top-end",
                absolute_position: { top: "43px", right: "51px" },
              },
            ],
            content: {
              position: "callout",
              title: {
                string_id: "callout-pdfjs-edit-title",
              },
              subtitle: {
                string_id: "callout-pdfjs-edit-body-b",
              },
              primary_button: {
                label: {
                  string_id: "callout-pdfjs-edit-button",
                },
                style: "secondary",
                action: {
                  type: "SET_PREF",
                  data: {
                    pref: {
                      name: PDFJS_PREF,
                      value: JSON.stringify({
                        screen: "FEATURE_CALLOUT_2_B",
                        complete: false,
                      }),
                    },
                  },
                },
              },
              dismiss_button: {
                action: {
                  type: "SET_PREF",
                  data: {
                    pref: {
                      name: PDFJS_PREF,
                      value: JSON.stringify({
                        screen: "",
                        complete: true,
                      }),
                    },
                  },
                },
              },
            },
          },
          {
            id: "FEATURE_CALLOUT_2_B",
            anchors: [
              {
                selector: "hbox#browser",
                arrow_position: "top-end",
                absolute_position: { top: "43px", right: "23px" },
              },
            ],
            content: {
              position: "callout",
              title: {
                string_id: "callout-pdfjs-draw-title",
              },
              subtitle: {
                string_id: "callout-pdfjs-draw-body-b",
              },
              primary_button: {
                label: {
                  string_id: "callout-pdfjs-draw-button",
                },
                style: "secondary",
                action: {
                  type: "SET_PREF",
                  data: {
                    pref: {
                      name: PDFJS_PREF,
                      value: JSON.stringify({
                        screen: "",
                        complete: true,
                      }),
                    },
                  },
                },
              },
              dismiss_button: {
                action: {
                  type: "SET_PREF",
                  data: {
                    pref: {
                      name: PDFJS_PREF,
                      value: JSON.stringify({
                        screen: "",
                        complete: true,
                      }),
                    },
                  },
                },
              },
            },
          },
        ],
      },
      priority: 1,
      targeting: `source == "open" && ${matchCurrentScreenTargeting(
        PDFJS_PREF,
        "FEATURE_CALLOUT_[0-9]_B"
      )} && ${matchIncompleteTargeting(PDFJS_PREF)}`,
      trigger: { id: "pdfJsFeatureCalloutCheck" },
    },
    // cookie banner reduction onboarding
    {
      id: "CFR_COOKIEBANNER",
      groups: ["cfr"],
      template: "feature_callout",
      content: {
        id: "CFR_COOKIEBANNER",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        disableHistoryUpdates: true,
        screens: [
          {
            id: "COOKIEBANNER_CALLOUT",
            anchors: [
              {
                selector: "#tracking-protection-icon-container",
                panel_position: {
                  callout_attachment: "topleft",
                  anchor_attachment: "bottomcenter",
                },
              },
            ],
            content: {
              position: "callout",
              autohide: true,
              title: {
                string_id: "cookie-banner-blocker-onboarding-header",
                paddingInline: "12px 0",
              },
              subtitle: {
                string_id: "cookie-banner-blocker-onboarding-body",
                paddingInline: "34px 0",
              },
              title_logo: {
                alignment: "top",
                height: "20px",
                width: "20px",
                imageURL:
                  "chrome://browser/skin/controlcenter/3rdpartycookies-blocked.svg",
              },
              dismiss_button: {
                size: "small",
                action: { dismiss: true },
              },
              additional_button: {
                label: {
                  string_id: "cookie-banner-blocker-onboarding-learn-more",
                  marginInline: "34px 0",
                },
                style: "link",
                alignment: "start",
                action: {
                  type: "OPEN_URL",
                  data: {
                    args: "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/cookie-banner-reduction",
                    where: "tabshifted",
                  },
                },
              },
            },
          },
        ],
      },
      frequency: {
        lifetime: 1,
      },
      skip_in_tests: "it's not tested in automation",
      trigger: {
        id: "cookieBannerHandled",
      },
      targeting: `'cookiebanners.ui.desktop.enabled'|preferenceValue == true && 'cookiebanners.ui.desktop.showCallout'|preferenceValue == true && 'browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features' | preferenceValue != false`,
    },
    {
      id: "FX_VIEW_DISCOVERABILITY_ALL_USERS",
      template: "feature_callout",
      groups: ["cfr"],
      content: {
        id: "FX_VIEW_DISCOVERABILITY_ALL_USERS",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        screens: [
          {
            id: "FX_VIEW_DISCOVERABILITY_ALL_USERS_SCREEN",
            anchors: [
              {
                selector: "#firefox-view-button",
                panel_position: {
                  anchor_attachment: "bottomcenter",
                  callout_attachment: "topleft",
                },
                no_open_on_anchor: true,
                arrow_width: "15.5563",
              },
            ],
            content: {
              position: "callout",
              width: "342px",
              padding: 16,
              page_event_listeners: [
                {
                  params: {
                    type: "click",
                    selectors: "#firefox-view-button",
                  },
                  action: {
                    dismiss: true,
                  },
                },
              ],
              title_logo: {
                imageURL:
                  "chrome://browser/content/asrouter/assets/fox-question-mark-icon.svg",
                width: "25px",
                height: "29px",
                marginInline: "4px 14px",
                alignment: "top",
              },
              title: {
                string_id: "fx-view-discoverability-title",
                marginInline: "0 16px",
              },
              subtitle: {
                string_id: "fx-view-discoverability-subtitle",
                paddingInline: "34px 0",
                marginBlock: "-8px -4px",
              },
              additional_button: {
                label: {
                  string_id: "fx-view-discoverability-secondary-button-label",
                },
                style: "secondary",
                action: {
                  type: "BLOCK_MESSAGE",
                  data: {
                    id: "FX_VIEW_DISCOVERABILITY_ALL_USERS",
                  },
                  dismiss: true,
                },
              },
              secondary_button: {
                label: {
                  string_id: "fx-view-discoverability-primary-button-label",
                },
                style: "primary",
                action: {
                  type: "OPEN_FIREFOX_VIEW",
                  navigate: true,
                },
              },
              submenu_button: {
                submenu: [
                  {
                    type: "action",
                    label: {
                      string_id: "split-dismiss-button-dont-show-option",
                    },
                    action: {
                      type: "BLOCK_MESSAGE",
                      data: {
                        id: "FX_VIEW_DISCOVERABILITY_ALL_USERS",
                      },
                      dismiss: true,
                    },
                    id: "block_recommendation",
                  },
                  {
                    type: "action",
                    label: {
                      string_id: "split-dismiss-button-show-fewer-option",
                    },
                    action: {
                      type: "MULTI_ACTION",
                      dismiss: true,
                      data: {
                        actions: [
                          {
                            type: "SET_PREF",
                            data: {
                              pref: {
                                name: "messaging-system-action.firefox-view-recommendations",
                                value: true,
                              },
                            },
                          },
                          {
                            type: "BLOCK_MESSAGE",
                            data: {
                              id: "FX_VIEW_DISCOVERABILITY_ALL_USERS",
                            },
                          },
                        ],
                      },
                    },
                    id: "show_fewer_recommendations",
                  },
                  {
                    type: "separator",
                  },
                  {
                    type: "action",
                    label: {
                      string_id: "split-dismiss-button-manage-settings-option",
                    },
                    action: {
                      type: "OPEN_ABOUT_PAGE",
                      data: {
                        args: "preferences#general-cfrfeatures",
                        where: "tab",
                      },
                      dismiss: true,
                    },
                    id: "manage_settings",
                  },
                ],
                attached_to: "additional_button",
              },
            },
          },
        ],
      },
      frequency: {
        lifetime: 1,
      },
      skip_in_tests: "it's not tested in automation",
      targeting:
        "!isMajorUpgrade && !willShowDefaultPrompt && !activeNotifications && previousSessionEnd && fxViewButtonAreaType != null && tabsClosedCount >= 5 && (currentDate|date - profileAgeCreated|date) / 86400000 >= 7 && 'browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features' | preferenceValue != false",
      trigger: {
        id: "nthTabClosed",
      },
    },
    {
      id: "RECOMMEND_BOOKMARKS_TOOLBAR",
      groups: ["cfr"],
      template: "feature_callout",
      content: {
        id: "RECOMMEND_BOOKMARKS_TOOLBAR",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        disableHistoryUpdates: true,
        screens: [
          {
            id: "RECOMMEND_BOOKMARKS_TOOLBAR_1",
            force_hide_steps_indicator: true,
            anchors: [
              {
                selector:
                  "#tabbrowser-tabs:not([overflow]) %triggerTab%[visuallyselected] .tab-content .tab-icon-stack",
                panel_position: {
                  anchor_attachment: "bottomcenter",
                  callout_attachment: "topleft",
                  offset_x: -3,
                  offset_y: 4,
                },
              },
            ],
            content: {
              position: "callout",
              width: "370px",
              padding: 16,
              logo: {
                imageURL:
                  "chrome://browser/content/asrouter/assets/recommend-bookmarks-toolbar/bookmarks-toolbar-light.svg",
                darkModeImageURL:
                  "chrome://browser/content/asrouter/assets/recommend-bookmarks-toolbar/bookmarks-toolbar-dark.svg",
                height: "170px",
                width: "338px",
              },
              title_logo: {
                imageURL:
                  "chrome://browser/content/asrouter/assets/smiling-fox-icon.svg",
                alignment: "top",
                width: "24px",
                height: "24px",
                marginInline: "0 12px",
              },
              title: {
                string_id: "bookmarks-toolbar-callout-1-title",
              },
              subtitle: {
                string_id: "bookmarks-toolbar-callout-1-subtitle",
                marginInline: "28px 0",
                marginBlock: "-8px 0",
              },
              additional_button: {
                label: {
                  string_id:
                    "bookmarks-toolbar-callout-1-secondary-button-label",
                },
                style: "secondary",
                action: {
                  type: "SET_BOOKMARKS_TOOLBAR_VISIBILITY",
                  data: {
                    visibility: "always",
                  },
                  advance_screens: {
                    id: "RECOMMEND_BOOKMARKS_TOOLBAR_2B_DECLINE",
                  },
                  navigate: true,
                },
              },
              submenu_button: {
                submenu: [
                  {
                    type: "action",
                    label: {
                      string_id: "split-dismiss-button-dont-show-option",
                    },
                    action: {
                      type: "BLOCK_MESSAGE",
                      data: {
                        id: "RECOMMEND_BOOKMARKS_TOOLBAR",
                      },
                      dismiss: true,
                    },
                    id: "block_recommendation",
                  },
                  {
                    type: "action",
                    label: {
                      string_id: "split-dismiss-button-show-fewer-option",
                    },
                    action: {
                      type: "MULTI_ACTION",
                      dismiss: true,
                      data: {
                        actions: [
                          {
                            type: "SET_PREF",
                            data: {
                              pref: {
                                name: "messaging-system-action.show-fewer-bookmarks-recommendations",
                                value: true,
                              },
                            },
                          },
                          {
                            type: "BLOCK_MESSAGE",
                            data: {
                              id: "RECOMMEND_BOOKMARKS_TOOLBAR",
                            },
                          },
                        ],
                      },
                    },
                    id: "show_fewer_recommendations",
                  },
                  {
                    type: "separator",
                  },
                  {
                    type: "action",
                    label: {
                      string_id: "split-dismiss-button-manage-settings-option",
                    },
                    action: {
                      type: "OPEN_ABOUT_PAGE",
                      data: {
                        args: "preferences#general-cfrfeatures",
                        where: "tab",
                      },
                      dismiss: true,
                    },
                    id: "manage_settings",
                  },
                ],
                attached_to: "additional_button",
              },
              secondary_button: {
                label: {
                  string_id: "bookmarks-toolbar-callout-1-primary-button-label",
                },
                style: "primary",
                action: {
                  type: "MULTI_ACTION",
                  advance_screens: {
                    id: "RECOMMEND_BOOKMARKS_TOOLBAR_2A_ACCEPT",
                  },
                  navigate: true,
                  data: {
                    actions: [
                      {
                        type: "BOOKMARK_CURRENT_TAB",
                        data: {
                          shouldHideDialog: true,
                          shouldHideConfirmationHint: true,
                        },
                      },
                      {
                        type: "SET_BOOKMARKS_TOOLBAR_VISIBILITY",
                        data: {
                          visibility: "always",
                        },
                      },
                    ],
                  },
                },
              },
            },
          },
          {
            id: "RECOMMEND_BOOKMARKS_TOOLBAR_2A_ACCEPT",
            force_hide_steps_indicator: true,
            anchors: [
              {
                selector: "%triggeredTabBookmark%",
                panel_position: {
                  offset_x: 12,
                  anchor_attachment: "bottomleft",
                  callout_attachment: "topleft",
                },
              },
              {
                selector: "#PlacesToolbarItems",
                panel_position: {
                  anchor_attachment: "bottomleft",
                  callout_attachment: "topleft",
                },
              },
            ],
            content: {
              position: "callout",
              width: "370px",
              padding: 16,
              logo: {
                imageURL:
                  "chrome://browser/content/asrouter/assets/recommend-bookmarks-toolbar/drag-drop-bookmark-light.svg",
                darkModeImageURL:
                  "chrome://browser/content/asrouter/assets/recommend-bookmarks-toolbar/drag-drop-bookmark-dark.svg",
                height: "170px",
                width: "338px",
              },
              title_logo: {
                imageURL:
                  "chrome://browser/content/asrouter/assets/double-star-icon.svg",
                alignment: "top",
                width: "24px",
                height: "24px",
                marginInline: "0 12px",
              },
              title: {
                string_id: "bookmarks-toolbar-callout-2a-title",
              },
              subtitle: {
                string_id: "bookmarks-toolbar-callout-2a-subtitle",
                marginInline: "28px 0",
                marginBlock: "-8px 0",
              },
              secondary_button: {
                label: {
                  string_id:
                    "bookmarks-toolbar-callout-2a-primary-button-label",
                },
                style: "primary",
                action: {
                  type: "BLOCK_MESSAGE",
                  data: {
                    id: "RECOMMEND_BOOKMARKS_TOOLBAR",
                  },
                  dismiss: true,
                },
              },
              primary_button: {
                label: {
                  string_id:
                    "bookmarks-toolbar-callout-2a-secondary-button-label",
                },
                style: "secondary",
                action: {
                  type: "MULTI_ACTION",
                  dismiss: true,
                  data: {
                    actions: [
                      {
                        type: "SET_BOOKMARKS_TOOLBAR_VISIBILITY",
                        data: {
                          visibility: "newtab",
                        },
                      },
                      {
                        type: "BLOCK_MESSAGE",
                        data: {
                          id: "RECOMMEND_BOOKMARKS_TOOLBAR",
                        },
                      },
                    ],
                  },
                },
              },
            },
          },
          {
            id: "RECOMMEND_BOOKMARKS_TOOLBAR_2B_DECLINE",
            force_hide_steps_indicator: true,
            anchors: [
              {
                selector:
                  "#PersonalToolbar:has(#import-button) #PlacesToolbarItems",
                panel_position: {
                  anchor_attachment: "bottomleft",
                  callout_attachment: "topleft",
                },
              },
              {
                selector: "#PlacesToolbarItems",
                panel_position: {
                  anchor_attachment: "bottomleft",
                  callout_attachment: "topleft",
                  offset_x: 24,
                },
              },
            ],
            content: {
              position: "callout",
              width: "370px",
              padding: 16,
              logo: {
                imageURL:
                  "chrome://browser/content/asrouter/assets/recommend-bookmarks-toolbar/drag-drop-bookmark-light.svg",
                darkModeImageURL:
                  "chrome://browser/content/asrouter/assets/recommend-bookmarks-toolbar/drag-drop-bookmark-dark.svg",
                height: "170px",
                width: "338px",
              },
              title_logo: {
                imageURL:
                  "chrome://browser/content/asrouter/assets/double-star-icon.svg",
                alignment: "top",
                width: "24px",
                height: "24px",
                marginInline: "0 12px",
              },
              title: {
                string_id: "bookmarks-toolbar-callout-2b-title",
              },
              subtitle: {
                string_id: "bookmarks-toolbar-callout-2b-subtitle",
                marginInline: "28px 0",
                marginBlock: "-8px 0",
              },
              secondary_button: {
                label: {
                  string_id:
                    "bookmarks-toolbar-callout-2b-primary-button-label",
                },
                style: "primary",
                action: {
                  type: "BLOCK_MESSAGE",
                  data: {
                    id: "RECOMMEND_BOOKMARKS_TOOLBAR",
                  },
                  dismiss: true,
                },
              },
              primary_button: {
                label: {
                  string_id:
                    "bookmarks-toolbar-callout-2b-secondary-button-label",
                },
                style: "secondary",
                action: {
                  type: "MULTI_ACTION",
                  dismiss: true,
                  data: {
                    actions: [
                      {
                        type: "SET_BOOKMARKS_TOOLBAR_VISIBILITY",
                        data: {
                          visibility: "newtab",
                        },
                      },
                      {
                        type: "BLOCK_MESSAGE",
                        data: {
                          id: "RECOMMEND_BOOKMARKS_TOOLBAR",
                        },
                      },
                    ],
                  },
                },
              },
            },
          },
        ],
      },
      frequency: {
        lifetime: 1,
      },
      priority: 1,
      targeting:
        "'browser.toolbars.bookmarks.visibility'|preferenceIsUserSet == false && visitsCount >= 3 && (currentDate|date - profileAgeCreated|date) / 86400000 >= 7 && !os.isLinux && !willShowDefaultPrompt && !activeNotifications && previousSessionEnd && 'browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features' | preferenceValue != false",
      trigger: {
        id: "openURL",
        patterns: [
          "https://mail.google.com/*",
          "https://mail.aol.com/*",
          "https://outlook.live.com/*",
          "https://app.neo.space/mail/*",
          "https://mail.yahoo.com/*",
          "https://www.icloud.com/mail/*",
          "https://www.zoho.com/mail/*",
          "https://account.proton.me/mail/*",
          "https://navigator-bs.gmx.com/mail/*",
          "https://tuta.com/*",
          "https://mailfence.com/*",
          "https://360.yandex.com/mail/*",
          "https://titan.email/*",
          "https://posteo.de/en/*",
          "https://runbox.com/*",
          "https://webmail.countermail.com/*",
          "https://kolabnow.com/*",
          "https://soverin.net/mail/*",
          "https://calendar.google.com/*",
          "https://www.calendar.com/*",
          "https://www.icloud.com/calendar/*",
          "https://www.zoho.com/calendar/*",
          "https://www.cozi.com/*",
          "https://kalender.digital/*",
          "https://www.kalender.com/*",
          "https://proton.me/de/calendar/*",
          "https://www.stackfield.com/de/*",
          "https://www.any.do/*",
          "https://zeeg.me/en/*",
          "https://www.pandora.com/*",
          "https://open.spotify.com/*",
          "https://tunein.com/radio/home/*",
          "https://www.iheart.com/*",
          "https://www.accuradio.com/*",
          "https://www.siriusxm.com/*",
          "https://www.jango.com/*",
          "https://live365.com/*",
          "https://www.radioguide.fm/*",
          "https://worldwidefm.net/*",
          "https://www.radio.net/s/fip/*",
          "https://www.nts.live/*",
          "https://vintagefm.com.au/*",
          "https://www.kcrw.com/music/shows/eclectic24/*",
          "https://sohoradiolondon.com/*",
          "https://power1051.iheart.com/*",
          "https://www.balamii.com/*",
          "https://www.cinemix.us/*",
          "https://www.kexp.org/*",
          "https://www.dublab.com/*",
          "https://www.facebook.com/*",
          "https://www.reddit.com/*",
          "https://www.instagram.com/*",
          "https://www.TikTok.com/*",
          "https://www.Pinterest.com/*",
          "https://twitter.com/*",
          "https://www.linkedin.com/*",
          "https://www.quora.com/*",
          "https://www.tumblr.com/*",
        ],
      },
      skip_in_tests: "it's not tested in automation",
    },
  ];
  messages = add24HourImpressionJEXLTargeting(
    ["FIREFOX_VIEW_TAB_PICKUP_REMINDER"],
    "FIREFOX_VIEW",
    messages
  );
  return messages;
};

export const FeatureCalloutMessages = {
  getMessages() {
    return MESSAGES();
  },
};
