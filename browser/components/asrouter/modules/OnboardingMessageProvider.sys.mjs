/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// We use importESModule here instead of static import so that
// the Karma test environment won't choke on this module. This
// is because the Karma test environment already stubs out
// XPCOMUtils and AppConstants, and overrides importESModule
// to be a no-op (which can't be done for a static import statement).

// eslint-disable-next-line mozilla/use-static-import
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

// eslint-disable-next-line mozilla/use-static-import
const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

import { FeatureCalloutMessages } from "resource:///modules/asrouter/FeatureCalloutMessages.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BrowserUtils: "resource://gre/modules/BrowserUtils.sys.mjs",
  ShellService: "resource:///modules/ShellService.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "usesFirefoxSync",
  "services.sync.username"
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "mobileDevices",
  "services.sync.clients.devices.mobile",
  0
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "hidePrivatePin",
  "browser.startup.upgradeDialog.pinPBM.disabled",
  false
);

const L10N = new Localization([
  "branding/brand.ftl",
  "browser/newtab/onboarding.ftl",
  "toolkit/branding/brandings.ftl",
]);

const HOMEPAGE_PREF = "browser.startup.homepage";
const NEWTAB_PREF = "browser.newtabpage.enabled";
const FOURTEEN_DAYS_IN_MS = 14 * 24 * 60 * 60 * 1000;
const isMSIX =
  AppConstants.platform === "win" &&
  Services.sysinfo.getProperty("hasWinPackageId", false);

const BASE_MESSAGES = () => [
  {
    id: "FXA_ACCOUNTS_BADGE",
    template: "toolbar_badge",
    content: {
      delay: 10000, // delay for 10 seconds
      target: "fxa-toolbar-menu-button",
    },
    targeting: "false",
    trigger: { id: "toolbarBadgeUpdate" },
  },
  {
    id: "MILESTONE_MESSAGE_87",
    groups: ["cfr"],
    content: {
      text: "",
      layout: "short_message",
      buttons: {
        primary: {
          event: "PROTECTION",
          label: {
            string_id: "cfr-doorhanger-milestone-ok-button",
          },
          action: {
            type: "OPEN_PROTECTION_REPORT",
          },
        },
        secondary: [
          {
            event: "DISMISS",
            label: {
              string_id: "cfr-doorhanger-milestone-close-button",
            },
            action: {
              type: "CANCEL",
            },
          },
        ],
      },
      category: "cfrFeatures",
      anchor_id: "tracking-protection-icon-container",
      bucket_id: "CFR_MILESTONE_MESSAGE",
      heading_text: {
        string_id: "cfr-doorhanger-milestone-heading2",
      },
      notification_text: "",
      skip_address_bar_notifier: true,
    },
    trigger: {
      id: "contentBlocking",
      params: ["ContentBlockingMilestone"],
    },
    template: "milestone_message",
    frequency: {
      lifetime: 7,
    },
    targeting: "pageLoad >= 4 && userPrefs.cfrFeatures",
  },
  {
    id: "FX_MR_106_UPGRADE",
    template: "spotlight",
    targeting: "true",
    content: {
      template: "multistage",
      id: "FX_MR_106_UPGRADE",
      transitions: true,
      modal: "tab",
      screens: [
        {
          id: "UPGRADE_PIN_FIREFOX",
          content: {
            position: "split",
            split_narrow_bkg_position: "-155px",
            image_alt_text: {
              string_id: "mr2022-onboarding-pin-image-alt",
            },
            progress_bar: "true",
            background:
              "url('chrome://activity-stream/content/data/content/assets/mr-pintaskbar.svg') var(--mr-secondary-position) no-repeat var(--mr-screen-background-color)",
            logo: {},
            title: {
              string_id: "mr2022-onboarding-existing-pin-header",
            },
            subtitle: {
              string_id: "mr2022-onboarding-existing-pin-subtitle",
            },
            primary_button: {
              label: {
                string_id: isMSIX
                  ? "mr2022-onboarding-pin-primary-button-label-msix"
                  : "mr2022-onboarding-pin-primary-button-label",
              },
              action: {
                type: "MULTI_ACTION",
                navigate: true,
                data: {
                  actions: [
                    {
                      type: "PIN_FIREFOX_TO_TASKBAR",
                    },
                    {
                      type: "PIN_FIREFOX_TO_START_MENU",
                    },
                  ],
                },
              },
            },
            checkbox: {
              label: {
                string_id: "mr2022-onboarding-existing-pin-checkbox-label",
              },
              defaultValue: true,
              action: {
                type: "MULTI_ACTION",
                navigate: true,
                data: {
                  actions: [
                    {
                      type: "PIN_FIREFOX_TO_TASKBAR",
                      data: {
                        privatePin: true,
                      },
                    },
                    {
                      type: "PIN_FIREFOX_TO_TASKBAR",
                    },
                    {
                      type: "PIN_FIREFOX_TO_START_MENU",
                    },
                  ],
                },
              },
            },
            secondary_button: {
              label: {
                string_id: "mr2022-onboarding-secondary-skip-button-label",
              },
              action: {
                navigate: true,
              },
              has_arrow_icon: true,
            },
          },
        },
        {
          id: "UPGRADE_SET_DEFAULT",
          content: {
            position: "split",
            split_narrow_bkg_position: "-60px",
            image_alt_text: {
              string_id: "mr2022-onboarding-default-image-alt",
            },
            progress_bar: "true",
            background:
              "url('chrome://activity-stream/content/data/content/assets/mr-settodefault.svg') var(--mr-secondary-position) no-repeat var(--mr-screen-background-color)",
            logo: {},
            title: {
              string_id: "mr2022-onboarding-set-default-title",
            },
            subtitle: {
              string_id: "mr2022-onboarding-set-default-subtitle",
            },
            primary_button: {
              label: {
                string_id: "mr2022-onboarding-set-default-primary-button-label",
              },
              action: {
                navigate: true,
                type: "SET_DEFAULT_BROWSER",
              },
            },
            secondary_button: {
              label: {
                string_id: "mr2022-onboarding-secondary-skip-button-label",
              },
              action: {
                navigate: true,
              },
              has_arrow_icon: true,
            },
          },
        },
        {
          id: "UPGRADE_IMPORT_SETTINGS_EMBEDDED",
          content: {
            tiles: { type: "migration-wizard" },
            position: "split",
            split_narrow_bkg_position: "-42px",
            image_alt_text: {
              string_id: "mr2022-onboarding-import-image-alt",
            },
            background:
              "url('chrome://activity-stream/content/data/content/assets/mr-import.svg') var(--mr-secondary-position) no-repeat var(--mr-screen-background-color)",
            progress_bar: true,
            hide_secondary_section: "responsive",
            migrate_start: {
              action: {},
            },
            migrate_close: {
              action: {
                navigate: true,
              },
            },
            secondary_button: {
              label: {
                string_id: "mr2022-onboarding-secondary-skip-button-label",
              },
              action: {
                navigate: true,
              },
              has_arrow_icon: true,
            },
          },
        },
        {
          id: "UPGRADE_MOBILE_DOWNLOAD",
          content: {
            position: "split",
            split_narrow_bkg_position: "-160px",
            image_alt_text: {
              string_id: "mr2022-onboarding-mobile-download-image-alt",
            },
            background:
              "url('chrome://activity-stream/content/data/content/assets/mr-mobilecrosspromo.svg') var(--mr-secondary-position) no-repeat var(--mr-screen-background-color)",
            progress_bar: true,
            logo: {},
            title: {
              string_id:
                "onboarding-mobile-download-security-and-privacy-title",
            },
            subtitle: {
              string_id:
                "onboarding-mobile-download-security-and-privacy-subtitle",
            },
            hero_image: {
              url: "chrome://activity-stream/content/data/content/assets/mobile-download-qr-existing-user.svg",
            },
            cta_paragraph: {
              text: {
                string_id: "mr2022-onboarding-mobile-download-cta-text",
                string_name: "download-label",
              },
              action: {
                type: "OPEN_URL",
                data: {
                  args: "https://www.mozilla.org/firefox/mobile/get-app/?utm_medium=firefox-desktop&utm_source=onboarding-modal&utm_campaign=mr2022&utm_content=existing-global",
                  where: "tab",
                },
              },
            },
            secondary_button: {
              label: {
                string_id: "mr2022-onboarding-secondary-skip-button-label",
              },
              action: {
                navigate: true,
              },
              has_arrow_icon: true,
            },
          },
        },
        {
          id: "UPGRADE_PIN_PRIVATE_WINDOW",
          content: {
            position: "split",
            split_narrow_bkg_position: "-100px",
            image_alt_text: {
              string_id: "mr2022-onboarding-pin-private-image-alt",
            },
            progress_bar: "true",
            background:
              "url('chrome://activity-stream/content/data/content/assets/mr-pinprivate.svg') var(--mr-secondary-position) no-repeat var(--mr-screen-background-color)",
            logo: {},
            title: {
              string_id: "mr2022-upgrade-onboarding-pin-private-window-header",
            },
            subtitle: {
              string_id:
                "mr2022-upgrade-onboarding-pin-private-window-subtitle",
            },
            primary_button: {
              label: {
                string_id:
                  "mr2022-upgrade-onboarding-pin-private-window-primary-button-label",
              },
              action: {
                type: "PIN_FIREFOX_TO_TASKBAR",
                data: {
                  privatePin: true,
                },
                navigate: true,
              },
            },
            secondary_button: {
              label: {
                string_id: "mr2022-onboarding-secondary-skip-button-label",
              },
              action: {
                navigate: true,
              },
              has_arrow_icon: true,
            },
          },
        },
        {
          id: "UPGRADE_DATA_RECOMMENDATION",
          content: {
            position: "split",
            split_narrow_bkg_position: "-80px",
            image_alt_text: {
              string_id: "mr2022-onboarding-privacy-segmentation-image-alt",
            },
            progress_bar: "true",
            background:
              "url('chrome://activity-stream/content/data/content/assets/mr-privacysegmentation.svg') var(--mr-secondary-position) no-repeat var(--mr-screen-background-color)",
            logo: {},
            title: {
              string_id: "mr2022-onboarding-privacy-segmentation-title",
            },
            subtitle: {
              string_id: "mr2022-onboarding-privacy-segmentation-subtitle",
            },
            cta_paragraph: {
              text: {
                string_id: "mr2022-onboarding-privacy-segmentation-text-cta",
              },
            },
            primary_button: {
              label: {
                string_id:
                  "mr2022-onboarding-privacy-segmentation-button-primary-label",
              },
              action: {
                type: "SET_PREF",
                data: {
                  pref: {
                    name: "browser.dataFeatureRecommendations.enabled",
                    value: true,
                  },
                },
                navigate: true,
              },
            },
            additional_button: {
              label: {
                string_id:
                  "mr2022-onboarding-privacy-segmentation-button-secondary-label",
              },
              style: "secondary",
              action: {
                type: "SET_PREF",
                data: {
                  pref: {
                    name: "browser.dataFeatureRecommendations.enabled",
                    value: false,
                  },
                },
                navigate: true,
              },
            },
          },
        },
        {
          id: "UPGRADE_GRATITUDE",
          content: {
            position: "split",
            progress_bar: "true",
            split_narrow_bkg_position: "-228px",
            image_alt_text: {
              string_id: "mr2022-onboarding-gratitude-image-alt",
            },
            background:
              "url('chrome://activity-stream/content/data/content/assets/mr-gratitude.svg') var(--mr-secondary-position) no-repeat var(--mr-screen-background-color)",
            logo: {},
            title: {
              string_id: "mr2022-onboarding-gratitude-title",
            },
            subtitle: {
              string_id: "mr2022-onboarding-gratitude-subtitle",
            },
            primary_button: {
              label: {
                string_id: "mr2022-onboarding-gratitude-primary-button-label",
              },
              action: {
                type: "OPEN_FIREFOX_VIEW",
                navigate: true,
              },
            },
            secondary_button: {
              label: {
                string_id: "mr2022-onboarding-gratitude-secondary-button-label",
              },
              action: {
                navigate: true,
              },
            },
          },
        },
      ],
    },
  },
  {
    id: "FX_100_UPGRADE",
    template: "spotlight",
    targeting: "false",
    content: {
      template: "multistage",
      id: "FX_100_UPGRADE",
      transitions: true,
      screens: [
        {
          id: "UPGRADE_PIN_FIREFOX",
          content: {
            logo: {
              imageURL:
                "chrome://activity-stream/content/data/content/assets/heart.webp",
              height: "73px",
            },
            has_noodles: true,
            title: {
              fontSize: "36px",
              string_id: "fx100-upgrade-thanks-header",
            },
            title_style: "fancy shine",
            background:
              "url('chrome://activity-stream/content/data/content/assets/confetti.svg') top / 100% no-repeat var(--in-content-page-background)",
            subtitle: {
              string_id: "fx100-upgrade-thanks-keep-body",
            },
            primary_button: {
              label: {
                string_id: "fx100-thank-you-pin-primary-button-label",
              },
              action: {
                type: "MULTI_ACTION",
                navigate: true,
                data: {
                  actions: [
                    {
                      type: "PIN_FIREFOX_TO_TASKBAR",
                    },
                    {
                      type: "PIN_FIREFOX_TO_START_MENU",
                    },
                  ],
                },
              },
            },
            secondary_button: {
              label: {
                string_id: "onboarding-not-now-button-label",
              },
              action: {
                navigate: true,
              },
            },
          },
        },
      ],
    },
  },
  {
    id: "PB_NEWTAB_FOCUS_PROMO",
    type: "default",
    template: "pb_newtab",
    groups: ["pbNewtab"],
    content: {
      infoBody: "fluent:about-private-browsing-info-description-simplified",
      infoEnabled: true,
      infoIcon: "chrome://global/skin/icons/indicator-private-browsing.svg",
      infoLinkText: "fluent:about-private-browsing-learn-more-link",
      infoTitle: "",
      infoTitleEnabled: false,
      promoEnabled: true,
      promoType: "FOCUS",
      promoHeader: "fluent:about-private-browsing-focus-promo-header-c",
      promoImageLarge: "chrome://browser/content/assets/focus-promo.png",
      promoLinkText: "fluent:about-private-browsing-focus-promo-cta",
      promoLinkType: "button",
      promoSectionStyle: "below-search",
      promoTitle: "fluent:about-private-browsing-focus-promo-text-c",
      promoTitleEnabled: true,
      promoButton: {
        action: {
          type: "SHOW_SPOTLIGHT",
          data: {
            content: {
              id: "FOCUS_PROMO",
              template: "multistage",
              modal: "tab",
              backdrop: "transparent",
              screens: [
                {
                  id: "DEFAULT_MODAL_UI",
                  content: {
                    logo: {
                      imageURL:
                        "chrome://browser/content/assets/focus-logo.svg",
                      height: "48px",
                    },
                    title: {
                      string_id: "spotlight-focus-promo-title",
                    },
                    subtitle: {
                      string_id: "spotlight-focus-promo-subtitle",
                    },
                    dismiss_button: {
                      action: {
                        navigate: true,
                      },
                    },
                    ios: {
                      action: {
                        data: {
                          args: "https://app.adjust.com/167k4ih?campaign=firefox-desktop&adgroup=pb&creative=focus-omc172&redirect=https%3A%2F%2Fapps.apple.com%2Fus%2Fapp%2Ffirefox-focus-privacy-browser%2Fid1055677337",
                          where: "tabshifted",
                        },
                        type: "OPEN_URL",
                        navigate: true,
                      },
                    },
                    android: {
                      action: {
                        data: {
                          args: "https://app.adjust.com/167k4ih?campaign=firefox-desktop&adgroup=pb&creative=focus-omc172&redirect=https%3A%2F%2Fplay.google.com%2Fstore%2Fapps%2Fdetails%3Fid%3Dorg.mozilla.focus",
                          where: "tabshifted",
                        },
                        type: "OPEN_URL",
                        navigate: true,
                      },
                    },
                    tiles: {
                      type: "mobile_downloads",
                      data: {
                        QR_code: {
                          image_url:
                            "chrome://browser/content/assets/focus-qr-code.svg",
                          alt_text: {
                            string_id: "spotlight-focus-promo-qr-code",
                          },
                        },
                        marketplace_buttons: ["ios", "android"],
                      },
                    },
                  },
                },
              ],
            },
          },
        },
      },
    },
    priority: 2,
    frequency: {
      custom: [
        {
          cap: 3,
          period: 604800000, // Max 3 per week
        },
      ],
      lifetime: 12,
    },
    // Exclude the next 2 messages: 1) Klar for en 2) Klar for de
    targeting:
      "!(region in [ 'DE', 'AT', 'CH'] && localeLanguageCode == 'en') && localeLanguageCode != 'de'",
  },
  {
    id: "PB_NEWTAB_KLAR_PROMO",
    type: "default",
    template: "pb_newtab",
    groups: ["pbNewtab"],
    content: {
      infoBody: "fluent:about-private-browsing-info-description-simplified",
      infoEnabled: true,
      infoIcon: "chrome://global/skin/icons/indicator-private-browsing.svg",
      infoLinkText: "fluent:about-private-browsing-learn-more-link",
      infoTitle: "",
      infoTitleEnabled: false,
      promoEnabled: true,
      promoType: "FOCUS",
      promoHeader: "fluent:about-private-browsing-focus-promo-header-c",
      promoImageLarge: "chrome://browser/content/assets/focus-promo.png",
      promoLinkText: "Download Firefox Klar",
      promoLinkType: "button",
      promoSectionStyle: "below-search",
      promoTitle:
        "Firefox Klar clears your history every time while blocking ads and trackers.",
      promoTitleEnabled: true,
      promoButton: {
        action: {
          type: "SHOW_SPOTLIGHT",
          data: {
            content: {
              id: "KLAR_PROMO",
              template: "multistage",
              modal: "tab",
              backdrop: "transparent",
              screens: [
                {
                  id: "DEFAULT_MODAL_UI",
                  order: 0,
                  content: {
                    logo: {
                      imageURL:
                        "chrome://browser/content/assets/focus-logo.svg",
                      height: "48px",
                    },
                    title: "Get Firefox Klar",
                    subtitle: {
                      string_id: "spotlight-focus-promo-subtitle",
                    },
                    dismiss_button: {
                      action: {
                        navigate: true,
                      },
                    },
                    ios: {
                      action: {
                        data: {
                          args: "https://app.adjust.com/a8bxj8j?campaign=firefox-desktop&adgroup=pb&creative=focus-omc172&redirect=https%3A%2F%2Fapps.apple.com%2Fde%2Fapp%2Fklar-by-firefox%2Fid1073435754",
                          where: "tabshifted",
                        },
                        type: "OPEN_URL",
                        navigate: true,
                      },
                    },
                    android: {
                      action: {
                        data: {
                          args: "https://app.adjust.com/a8bxj8j?campaign=firefox-desktop&adgroup=pb&creative=focus-omc172&redirect=https%3A%2F%2Fplay.google.com%2Fstore%2Fapps%2Fdetails%3Fid%3Dorg.mozilla.klar",
                          where: "tabshifted",
                        },
                        type: "OPEN_URL",
                        navigate: true,
                      },
                    },
                    tiles: {
                      type: "mobile_downloads",
                      data: {
                        QR_code: {
                          image_url:
                            "chrome://browser/content/assets/klar-qr-code.svg",
                          alt_text: "Scan the QR code to get Firefox Klar",
                        },
                        marketplace_buttons: ["ios", "android"],
                      },
                    },
                  },
                },
              ],
            },
          },
        },
      },
    },
    priority: 2,
    frequency: {
      custom: [
        {
          cap: 3,
          period: 604800000, // Max 3 per week
        },
      ],
      lifetime: 12,
    },
    targeting: "region in [ 'DE', 'AT', 'CH'] && localeLanguageCode == 'en'",
  },
  {
    id: "PB_NEWTAB_KLAR_PROMO_DE",
    type: "default",
    template: "pb_newtab",
    groups: ["pbNewtab"],
    content: {
      infoBody: "fluent:about-private-browsing-info-description-simplified",
      infoEnabled: true,
      infoIcon: "chrome://global/skin/icons/indicator-private-browsing.svg",
      infoLinkText: "fluent:about-private-browsing-learn-more-link",
      infoTitle: "",
      infoTitleEnabled: false,
      promoEnabled: true,
      promoType: "FOCUS",
      promoHeader: "fluent:about-private-browsing-focus-promo-header-c",
      promoImageLarge: "chrome://browser/content/assets/focus-promo.png",
      promoLinkText: "fluent:about-private-browsing-focus-promo-cta",
      promoLinkType: "button",
      promoSectionStyle: "below-search",
      promoTitle: "fluent:about-private-browsing-focus-promo-text-c",
      promoTitleEnabled: true,
      promoButton: {
        action: {
          type: "SHOW_SPOTLIGHT",
          data: {
            content: {
              id: "FOCUS_PROMO",
              template: "multistage",
              modal: "tab",
              backdrop: "transparent",
              screens: [
                {
                  id: "DEFAULT_MODAL_UI",
                  content: {
                    logo: {
                      imageURL:
                        "chrome://browser/content/assets/focus-logo.svg",
                      height: "48px",
                    },
                    title: {
                      string_id: "spotlight-focus-promo-title",
                    },
                    subtitle: {
                      string_id: "spotlight-focus-promo-subtitle",
                    },
                    dismiss_button: {
                      action: {
                        navigate: true,
                      },
                    },
                    ios: {
                      action: {
                        data: {
                          args: "https://app.adjust.com/a8bxj8j?campaign=firefox-desktop&adgroup=pb&creative=focus-omc172&redirect=https%3A%2F%2Fapps.apple.com%2Fde%2Fapp%2Fklar-by-firefox%2Fid1073435754",
                          where: "tabshifted",
                        },
                        type: "OPEN_URL",
                        navigate: true,
                      },
                    },
                    android: {
                      action: {
                        data: {
                          args: "https://app.adjust.com/a8bxj8j?campaign=firefox-desktop&adgroup=pb&creative=focus-omc172&redirect=https%3A%2F%2Fplay.google.com%2Fstore%2Fapps%2Fdetails%3Fid%3Dorg.mozilla.klar",
                          where: "tabshifted",
                        },
                        type: "OPEN_URL",
                        navigate: true,
                      },
                    },
                    tiles: {
                      type: "mobile_downloads",
                      data: {
                        QR_code: {
                          image_url:
                            "chrome://browser/content/assets/klar-qr-code.svg",
                          alt_text: {
                            string_id: "spotlight-focus-promo-qr-code",
                          },
                        },
                        marketplace_buttons: ["ios", "android"],
                      },
                    },
                  },
                },
              ],
            },
          },
        },
      },
    },
    priority: 2,
    frequency: {
      custom: [
        {
          cap: 3,
          period: 604800000, // Max 3 per week
        },
      ],
      lifetime: 12,
    },
    targeting: "localeLanguageCode == 'de'",
  },
  {
    id: "PB_NEWTAB_PIN_PROMO",
    template: "pb_newtab",
    type: "default",
    groups: ["pbNewtab"],
    content: {
      infoBody: "fluent:about-private-browsing-info-description-simplified",
      infoEnabled: true,
      infoIcon: "chrome://global/skin/icons/indicator-private-browsing.svg",
      infoLinkText: "fluent:about-private-browsing-learn-more-link",
      infoTitle: "",
      infoTitleEnabled: false,
      promoEnabled: true,
      promoType: "PIN",
      promoHeader: "fluent:about-private-browsing-pin-promo-header",
      promoImageLarge:
        "chrome://browser/content/assets/private-promo-asset.svg",
      promoLinkText: "fluent:about-private-browsing-pin-promo-link-text",
      promoLinkType: "button",
      promoSectionStyle: "below-search",
      promoTitle: "fluent:about-private-browsing-pin-promo-title",
      promoTitleEnabled: true,
      promoButton: {
        action: {
          type: "MULTI_ACTION",
          data: {
            actions: [
              {
                type: "SET_PREF",
                data: {
                  pref: {
                    name: "browser.privateWindowSeparation.enabled",
                    value: true,
                  },
                },
              },
              {
                type: "PIN_FIREFOX_TO_TASKBAR",
                data: {
                  privatePin: true,
                },
              },
              {
                type: "BLOCK_MESSAGE",
                data: {
                  id: "PB_NEWTAB_PIN_PROMO",
                },
              },
              {
                type: "OPEN_ABOUT_PAGE",
                data: { args: "privatebrowsing", where: "current" },
              },
            ],
          },
        },
      },
    },
    priority: 3,
    frequency: {
      custom: [
        {
          cap: 3,
          period: 604800000, // Max 3 per week
        },
      ],
      lifetime: 12,
    },
    targeting: "doesAppNeedPrivatePin",
  },
  {
    id: "PB_NEWTAB_COOKIE_BANNERS_PROMO",
    template: "pb_newtab",
    type: "default",
    groups: ["pbNewtab"],
    content: {
      infoBody: "fluent:about-private-browsing-info-description-simplified",
      infoEnabled: true,
      infoIcon: "chrome://global/skin/icons/indicator-private-browsing.svg",
      infoLinkText: "fluent:about-private-browsing-learn-more-link",
      infoTitle: "",
      infoTitleEnabled: false,
      promoEnabled: true,
      promoType: "COOKIE_BANNERS",
      promoHeader: "fluent:about-private-browsing-cookie-banners-promo-heading",
      promoImageLarge:
        "chrome://browser/content/assets/cookie-banners-begone.svg",
      promoLinkText: "fluent:about-private-browsing-learn-more-link",
      promoLinkType: "link",
      promoSectionStyle: "below-search",
      promoTitle: "fluent:about-private-browsing-cookie-banners-promo-body",
      promoTitleEnabled: true,
      promoButton: {
        action: {
          type: "MULTI_ACTION",
          data: {
            actions: [
              {
                type: "OPEN_URL",
                data: {
                  args: "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/cookie-banner-reduction",
                  where: "tabshifted",
                },
              },
              {
                type: "BLOCK_MESSAGE",
                data: {
                  id: "PB_NEWTAB_COOKIE_BANNERS_PROMO",
                },
              },
            ],
          },
        },
      },
    },
    priority: 4,
    frequency: {
      custom: [
        {
          cap: 3,
          period: 604800000, // Max 3 per week
        },
      ],
      lifetime: 12,
    },
    targeting: `'cookiebanners.service.mode.privateBrowsing'|preferenceValue != 0 || 'cookiebanners.service.mode'|preferenceValue != 0`,
  },
  {
    id: "INFOBAR_LAUNCH_ON_LOGIN",
    groups: ["cfr"],
    template: "infobar",
    content: {
      type: "global",
      text: {
        string_id: "launch-on-login-infobar-message",
      },
      buttons: [
        {
          label: {
            string_id: "launch-on-login-learnmore",
          },
          supportPage: "make-firefox-automatically-open-when-you-start",
          action: {
            type: "CANCEL",
          },
        },
        {
          label: { string_id: "launch-on-login-infobar-reject-button" },
          action: {
            type: "CANCEL",
          },
        },
        {
          label: { string_id: "launch-on-login-infobar-confirm-button" },
          primary: true,
          action: {
            type: "MULTI_ACTION",
            data: {
              actions: [
                {
                  type: "SET_PREF",
                  data: {
                    pref: {
                      name: "browser.startup.windowsLaunchOnLogin.disableLaunchOnLoginPrompt",
                      value: true,
                    },
                  },
                },
                {
                  type: "CONFIRM_LAUNCH_ON_LOGIN",
                },
              ],
            },
          },
        },
      ],
    },
    frequency: {
      lifetime: 1,
    },
    trigger: { id: "defaultBrowserCheck" },
    targeting: `source == 'newtab'
    && 'browser.startup.windowsLaunchOnLogin.disableLaunchOnLoginPrompt'|preferenceValue == false
    && 'browser.startup.windowsLaunchOnLogin.enabled'|preferenceValue == true && isDefaultBrowser && !activeNotifications
    && !launchOnLoginEnabled`,
  },
  {
    id: "INFOBAR_LAUNCH_ON_LOGIN_FINAL",
    groups: ["cfr"],
    template: "infobar",
    content: {
      type: "global",
      text: {
        string_id: "launch-on-login-infobar-final-message",
      },
      buttons: [
        {
          label: {
            string_id: "launch-on-login-learnmore",
          },
          supportPage: "make-firefox-automatically-open-when-you-start",
          action: {
            type: "CANCEL",
          },
        },
        {
          label: { string_id: "launch-on-login-infobar-final-reject-button" },
          action: {
            type: "SET_PREF",
            data: {
              pref: {
                name: "browser.startup.windowsLaunchOnLogin.disableLaunchOnLoginPrompt",
                value: true,
              },
            },
          },
        },
        {
          label: { string_id: "launch-on-login-infobar-confirm-button" },
          primary: true,
          action: {
            type: "MULTI_ACTION",
            data: {
              actions: [
                {
                  type: "SET_PREF",
                  data: {
                    pref: {
                      name: "browser.startup.windowsLaunchOnLogin.disableLaunchOnLoginPrompt",
                      value: true,
                    },
                  },
                },
                {
                  type: "CONFIRM_LAUNCH_ON_LOGIN",
                },
              ],
            },
          },
        },
      ],
    },
    frequency: {
      lifetime: 1,
    },
    trigger: { id: "defaultBrowserCheck" },
    targeting: `source == 'newtab'
    && 'browser.startup.windowsLaunchOnLogin.disableLaunchOnLoginPrompt'|preferenceValue == false
    && 'browser.startup.windowsLaunchOnLogin.enabled'|preferenceValue == true && isDefaultBrowser && !activeNotifications
    && messageImpressions.INFOBAR_LAUNCH_ON_LOGIN[messageImpressions.INFOBAR_LAUNCH_ON_LOGIN | length - 1]
    && messageImpressions.INFOBAR_LAUNCH_ON_LOGIN[messageImpressions.INFOBAR_LAUNCH_ON_LOGIN | length - 1] <
      currentDate|date - ${FOURTEEN_DAYS_IN_MS}
    && !launchOnLoginEnabled`,
  },
  {
    id: "FOX_DOODLE_SET_DEFAULT",
    template: "spotlight",
    groups: ["eco"],
    skip_in_tests: "it fails unrelated tests",
    content: {
      backdrop: "transparent",
      id: "FOX_DOODLE_SET_DEFAULT",
      screens: [
        {
          id: "FOX_DOODLE_SET_DEFAULT_SCREEN",
          content: {
            logo: {
              height: "125px",
              imageURL:
                "chrome://activity-stream/content/data/content/assets/fox-doodle-waving.gif",
              reducedMotionImageURL:
                "chrome://activity-stream/content/data/content/assets/fox-doodle-waving-static.png",
            },
            title: {
              fontSize: "22px",
              fontWeight: 590,
              letterSpacing: 0,
              paddingInline: "24px",
              paddingBlock: "4px 0",
              string_id: "fox-doodle-pin-headline",
            },
            subtitle: {
              fontSize: "15px",
              letterSpacing: 0,
              lineHeight: "1.4",
              marginBlock: "8px 16px",
              paddingInline: "24px",
              string_id: "fox-doodle-pin-body",
            },
            primary_button: {
              action: {
                navigate: true,
                type: "SET_DEFAULT_BROWSER",
              },
              label: {
                paddingBlock: "0",
                paddingInline: "16px",
                marginBlock: "4px 0",
                string_id: "fox-doodle-pin-primary",
              },
            },
            secondary_button: {
              action: {
                navigate: true,
              },
              label: {
                marginBlock: "0 -20px",
                string_id: "fox-doodle-pin-secondary",
              },
            },
            dismiss_button: {
              action: {
                navigate: true,
              },
            },
          },
        },
      ],
      template: "multistage",
      transitions: true,
    },
    frequency: {
      lifetime: 2,
    },
    targeting: `source == 'startup'
    && !isMajorUpgrade
    && !activeNotifications
    && !isDefaultBrowser
    && !willShowDefaultPrompt
    && 'browser.shell.checkDefaultBrowser'|preferenceValue
    && (currentDate|date - profileAgeCreated|date) / 86400000 >= 28
    && previousSessionEnd
    && userPrefs.cfrFeatures == true`,
    trigger: {
      id: "defaultBrowserCheck",
    },
  },
  {
    id: "TAIL_FOX_SET_DEFAULT",
    template: "spotlight",
    groups: ["eco"],
    skip_in_tests: "it fails unrelated tests",
    content: {
      backdrop: "transparent",
      id: "TAIL_FOX_SET_DEFAULT_CONTENT",
      screens: [
        {
          id: "TAIL_FOX_SET_DEFAULT_SCREEN",
          content: {
            logo: {
              height: "140px",
              imageURL:
                "chrome://activity-stream/content/data/content/assets/fox-doodle-tail.png",
              reducedMotionImageURL:
                "chrome://activity-stream/content/data/content/assets/fox-doodle-tail.png",
            },
            title: {
              fontSize: "22px",
              fontWeight: 590,
              letterSpacing: 0,
              paddingInline: "24px",
              paddingBlock: "4px 0",
              string_id: "tail-fox-spotlight-title",
            },
            subtitle: {
              fontSize: "15px",
              letterSpacing: 0,
              lineHeight: "1.4",
              marginBlock: "8px 16px",
              paddingInline: "24px",
              string_id: "tail-fox-spotlight-subtitle",
            },
            primary_button: {
              action: {
                navigate: true,
                type: "SET_DEFAULT_BROWSER",
              },
              label: {
                paddingBlock: "0",
                paddingInline: "16px",
                marginBlock: "4px 0",
                string_id: "tail-fox-spotlight-primary-button",
              },
            },
            secondary_button: {
              action: {
                navigate: true,
              },
              label: {
                marginBlock: "0 -20px",
                string_id: "tail-fox-spotlight-secondary-button",
              },
            },
            dismiss_button: {
              action: {
                navigate: true,
              },
            },
          },
        },
      ],
      template: "multistage",
      transitions: true,
    },
    frequency: {
      lifetime: 1,
    },
    targeting: `source == 'startup'
    && !isMajorUpgrade
    && !activeNotifications
    && !isDefaultBrowser
    && !willShowDefaultPrompt
    && 'browser.shell.checkDefaultBrowser'|preferenceValue
    && (currentDate|date - profileAgeCreated|date) / 86400000 <= 28
    && (currentDate|date - profileAgeCreated|date) / 86400000 >= 7
    && previousSessionEnd
    && userPrefs.cfrFeatures == true`,
    trigger: {
      id: "defaultBrowserCheck",
    },
  },
  {
    id: "SET_DEFAULT_BROWSER_GUIDANCE_NOTIFICATION_WIN10",
    template: "toast_notification",
    content: {
      title: {
        string_id: "default-browser-guidance-notification-title",
      },
      body: {
        string_id:
          "default-browser-guidance-notification-body-instruction-win10",
      },
      launch_action: {
        type: "OPEN_URL",
        data: {
          args: "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/win-set-firefox-default-browser",
          where: "tabshifted",
        },
      },
      requireInteraction: true,
      actions: [
        {
          action: "info-page",
          title: {
            string_id: "default-browser-guidance-notification-info-page",
          },
          launch_action: {
            type: "OPEN_URL",
            data: {
              args: "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/win-set-firefox-default-browser",
              where: "tabshifted",
            },
          },
        },
        {
          action: "dismiss",
          title: {
            string_id: "default-browser-guidance-notification-dismiss",
          },
          windowsSystemActivationType: true,
        },
      ],
      tag: "set-default-guidance-notification",
    },
    // Both Windows 10 and 11 return `os.windowsVersion == 10.0`. We limit to
    // only Windows 10 with `os.windowsBuildNumber < 22000`. We need this due to
    // Windows 10 and 11 having substantively different UX for Windows Settings.
    targeting:
      "os.isWindows && os.windowsVersion >= 10.0 && os.windowsBuildNumber < 22000",
    trigger: { id: "deeplinkedToWindowsSettingsUI" },
  },
  {
    id: "SET_DEFAULT_BROWSER_GUIDANCE_NOTIFICATION_WIN11",
    template: "toast_notification",
    content: {
      title: {
        string_id: "default-browser-guidance-notification-title",
      },
      body: {
        string_id:
          "default-browser-guidance-notification-body-instruction-win11",
      },
      launch_action: {
        type: "OPEN_URL",
        data: {
          args: "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/win-set-firefox-default-browser",
          where: "tabshifted",
        },
      },
      requireInteraction: true,
      actions: [
        {
          action: "info-page",
          title: {
            string_id: "default-browser-guidance-notification-info-page",
          },
          launch_action: {
            type: "OPEN_URL",
            data: {
              args: "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/win-set-firefox-default-browser",
              where: "tabshifted",
            },
          },
        },
        {
          action: "dismiss",
          title: {
            string_id: "default-browser-guidance-notification-dismiss",
          },
          windowsSystemActivationType: true,
        },
      ],
      tag: "set-default-guidance-notification",
    },
    // Both Windows 10 and 11 return `os.windowsVersion == 10.0`. We limit to
    // only Windows 11 with `os.windowsBuildNumber >= 22000`. We need this due to
    // Windows 10 and 11 having substantively different UX for Windows Settings.
    targeting:
      "os.isWindows && os.windowsVersion >= 10.0 && os.windowsBuildNumber >= 22000",
    trigger: { id: "deeplinkedToWindowsSettingsUI" },
  },
  {
    id: "FXA_ACCOUNTS_BADGE_REVISED",
    template: "toolbar_badge",
    content: {
      delay: 1000,
      target: "fxa-toolbar-menu-button",
    },
    skip_in_tests: "it's covered by browser_asrouter_toolbarbadge.js",
    targeting:
      "source == 'newtab' && !hasAccessedFxAPanel && !usesFirefoxSync && isFxAEnabled && !isFxASignedIn",
    trigger: {
      id: "defaultBrowserCheck",
    },
  },
  {
    id: "INFOBAR_DEFAULT_AND_PIN_87",
    groups: ["cfr"],
    content: {
      text: {
        string_id: "default-browser-notification-message",
      },
      type: "global",
      buttons: [
        {
          label: {
            string_id: "default-browser-notification-button",
          },
          action: {
            type: "PIN_AND_DEFAULT",
          },
          primary: true,
          accessKey: "P",
        },
      ],
      category: "cfrFeatures",
      bucket_id: "INFOBAR_DEFAULT_AND_PIN_87",
    },
    trigger: {
      id: "defaultBrowserCheck",
    },
    template: "infobar",
    frequency: {
      custom: [
        {
          cap: 1,
          period: 3024000000,
        },
      ],
      lifetime: 2,
    },
    targeting:
      "(firefoxVersion >= 138 && source == 'startup' && !isDefaultBrowser && !'browser.shell.checkDefaultBrowser'|preferenceValue && currentDate|date - 'browser.shell.userDisabledDefaultCheck'|preferenceValue * 1000 >= 604800000 && isMajorUpgrade != true && platformName != 'linux' && ((currentDate|date - profileAgeCreated) / 604800000) >= 5 && !activeNotifications && 'browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features'|preferenceValue && ((currentDate|date - profileAgeCreated) / 604800000) < 15",
  },
];

const PREONBOARDING_MESSAGES = () => [
  {
    id: "NEW_USER_TOU_ONBOARDING",
    enabled: true,
    requireAction: true,
    currentPolicyVersion: 3,
    minimumPolicyVersion: 3,
    firstRunURL: "https://www.mozilla.org/privacy/firefox/",
    screens: [
      {
        id: "TOU_ONBOARDING",
        content: {
          action_buttons_above_content: true,
          screen_style: {
            overflow: "auto",
            display: "block",
            padding: "40px 0 0 0",
            width: "560px",
          },
          logo: {
            imageURL: "chrome://branding/content/about-logo.png",
            height: "40px",
            width: "40px",
          },
          title: {
            string_id: "preonboarding-title",
          },
          subtitle: {
            string_id: "preonboarding-subtitle",
            paddingInline: "24px",
          },
          tiles: [
            {
              type: "embedded_browser",
              id: "terms_of_use",
              header: {
                title: {
                  string_id: "preonboarding-terms-of-use-header-button-title",
                },
              },
              data: {
                style: {
                  width: "100%",
                  height: "200px",
                },
                url: "https://mozilla.org/about/legal/terms/firefox/?v=product",
              },
            },
            {
              type: "embedded_browser",
              id: "privacy_notice",
              header: {
                title: {
                  string_id: "preonboarding-privacy-notice-header-button-title",
                },
              },
              data: {
                style: {
                  width: "100%",
                  height: "200px",
                },
                url: "https://mozilla.org/privacy/firefox/?v=product",
              },
            },
            {
              type: "multiselect",
              header: {
                title: {
                  string_id: "preonboarding-manage-data-header-button-title",
                },
              },
              data: [
                {
                  id: "interaction-data",
                  type: "checkbox",
                  defaultValue: true,
                  label: {
                    string_id: "preonboarding-checklist-interaction-data-label",
                  },
                  description: {
                    string_id:
                      "preonboarding-checklist-interaction-data-description",
                  },
                  action: {
                    type: "SET_PREF",
                    data: {
                      pref: {
                        name: "datareporting.healthreport.uploadEnabled",
                        value: true,
                      },
                    },
                  },
                  uncheckedAction: {
                    type: "MULTI_ACTION",
                    data: {
                      orderedExecution: true,
                      actions: [
                        {
                          type: "SET_PREF",
                          data: {
                            pref: {
                              name: "datareporting.healthreport.uploadEnabled",
                              value: false,
                            },
                          },
                        },
                        {
                          type: "SUBMIT_ONBOARDING_OPT_OUT_PING",
                        },
                      ],
                    },
                  },
                },
                {
                  id: "crash-data",
                  type: "checkbox",
                  defaultValue: false,
                  label: {
                    string_id: "preonboarding-checklist-crash-reports-label",
                  },
                  description: {
                    string_id:
                      "preonboarding-checklist-crash-reports-description",
                  },
                  action: {
                    type: "SET_PREF",
                    data: {
                      pref: {
                        name: "browser.crashReports.unsubmittedCheck.autoSubmit2",
                        value: true,
                      },
                    },
                  },
                  uncheckedAction: {
                    type: "SET_PREF",
                    data: {
                      pref: {
                        name: "browser.crashReports.unsubmittedCheck.autoSubmit2",
                        value: false,
                      },
                    },
                  },
                },
              ],
            },
          ],
          primary_button: {
            label: {
              string_id: "preonboarding-primary-cta-v2",
              marginBlock: "24px 0",
            },
            should_focus_button: true,
            action: {
              type: "MULTI_ACTION",
              collectSelect: true,
              data: {
                orderedExecution: true,
                actions: [
                  {
                    type: "DATAREPORTING_NOTIFY_DATA_POLICY_INTERACTED",
                  },
                ],
              },
              dismiss: true,
            },
          },
        },
      },
    ],
  },
];

// Eventually, move Feature Callout messages to their own provider
const ONBOARDING_MESSAGES = () =>
  BASE_MESSAGES().concat(FeatureCalloutMessages.getMessages());

export const OnboardingMessageProvider = {
  async getExtraAttributes() {
    const [header, button_label] = await L10N.formatMessages([
      { id: "onboarding-welcome-header" },
      { id: "onboarding-start-browsing-button-label" },
    ]);
    return { header: header.value, button_label: button_label.value };
  },

  async getMessages() {
    const messages = await this.translateMessages(await ONBOARDING_MESSAGES());
    return messages;
  },

  getPreonboardingMessages() {
    return PREONBOARDING_MESSAGES();
  },

  async getUntranslatedMessages() {
    // This is helpful for jsonSchema testing - since we are localizing in the provider
    const messages = await ONBOARDING_MESSAGES();
    return messages;
  },

  async translateMessages(messages) {
    let translatedMessages = [];
    for (const msg of messages) {
      let translatedMessage = { ...msg };

      // If the message has no content, do not attempt to translate it
      if (!translatedMessage.content) {
        translatedMessages.push(translatedMessage);
        continue;
      }

      // Translate any secondary buttons separately
      if (msg.content.secondary_button) {
        const [secondary_button_string] = await L10N.formatMessages([
          { id: msg.content.secondary_button.label.string_id },
        ]);
        translatedMessage.content.secondary_button.label =
          secondary_button_string.value;
      }
      if (msg.content.header) {
        const [header_string] = await L10N.formatMessages([
          { id: msg.content.header.string_id },
        ]);
        translatedMessage.content.header = header_string.value;
      }
      translatedMessages.push(translatedMessage);
    }
    return translatedMessages;
  },

  async _doesAppNeedPin(privateBrowsing = false) {
    const needPin = await lazy.ShellService.doesAppNeedPin(privateBrowsing);
    return needPin;
  },

  async _doesAppNeedDefault() {
    let checkDefault = Services.prefs.getBoolPref(
      "browser.shell.checkDefaultBrowser",
      false
    );
    let isDefault = await lazy.ShellService.isDefaultBrowser();
    return checkDefault && !isDefault;
  },

  _shouldShowPrivacySegmentationScreen() {
    return Services.prefs.getBoolPref(
      "browser.privacySegmentation.preferences.show"
    );
  },

  _doesHomepageNeedReset() {
    return (
      Services.prefs.prefHasUserValue(HOMEPAGE_PREF) ||
      Services.prefs.prefHasUserValue(NEWTAB_PREF)
    );
  },

  async getUpgradeMessage() {
    let message = (await OnboardingMessageProvider.getMessages()).find(
      ({ id }) => id === "FX_MR_106_UPGRADE"
    );

    let { content } = message;
    // Helper to find screens and remove them where applicable.
    function removeScreens(check) {
      const { screens } = content;
      for (let i = 0; i < screens?.length; i++) {
        if (check(screens[i])) {
          screens.splice(i--, 1);
        }
      }
    }

    // Helper to prepare mobile download screen content
    function prepareMobileDownload() {
      let mobileContent = content.screens.find(
        screen => screen.id === "UPGRADE_MOBILE_DOWNLOAD"
      )?.content;

      if (!mobileContent) {
        return;
      }
      if (!lazy.BrowserUtils.sendToDeviceEmailsSupported()) {
        // If send to device emails are not supported for a user's locale,
        // remove the send to device link and update the screen text
        delete mobileContent.cta_paragraph.action;
        mobileContent.cta_paragraph.text = {
          string_id: "mr2022-onboarding-no-mobile-download-cta-text",
        };
      }
      // Update CN specific QRCode url
      if (lazy.BrowserUtils.isChinaRepack()) {
        mobileContent.hero_image.url = `${mobileContent.hero_image.url.slice(
          0,
          mobileContent.hero_image.url.indexOf(".svg")
        )}-cn.svg`;
      }
    }

    let pinScreen = content.screens?.find(
      screen => screen.id === "UPGRADE_PIN_FIREFOX"
    );
    const needPin = await this._doesAppNeedPin();
    const needDefault = await this._doesAppNeedDefault();
    const needPrivatePin =
      !lazy.hidePrivatePin && (await this._doesAppNeedPin(true));
    const showSegmentation = this._shouldShowPrivacySegmentationScreen();

    //If a user has Firefox as default remove import screen
    if (!needDefault) {
      removeScreens(screen =>
        screen.id?.startsWith("UPGRADE_IMPORT_SETTINGS_EMBEDDED")
      );
    }

    // If already pinned, convert "pin" screen to "welcome" with desired action.
    let removeDefault = !needDefault;
    // If user doesn't need pin, update screen to set "default" or "get started" configuration
    if (!needPin && pinScreen) {
      // don't need to show the checkbox
      delete pinScreen.content.checkbox;

      removeDefault = true;
      let primary = pinScreen.content.primary_button;
      if (needDefault) {
        pinScreen.id = "UPGRADE_ONLY_DEFAULT";
        pinScreen.content.subtitle = {
          string_id: "mr2022-onboarding-existing-set-default-only-subtitle",
        };
        primary.label.string_id =
          "mr2022-onboarding-set-default-primary-button-label";

        // The "pin" screen will now handle "default" so remove other "default."
        primary.action.type = "SET_DEFAULT_BROWSER";
      } else {
        pinScreen.id = "UPGRADE_GET_STARTED";
        pinScreen.content.subtitle = {
          string_id: "mr2022-onboarding-get-started-primary-subtitle",
        };
        primary.label = {
          string_id: "mr2022-onboarding-get-started-primary-button-label",
        };
        delete primary.action.type;
      }
    }

    // If a user has Firefox private pinned remove pin private window screen
    // We also remove standalone pin private window screen if a user doesn't have
    // Firefox pinned in which case the option is shown as checkbox with UPGRADE_PIN_FIREFOX screen
    if (!needPrivatePin || needPin) {
      removeScreens(screen =>
        screen.id?.startsWith("UPGRADE_PIN_PRIVATE_WINDOW")
      );
    }

    if (!showSegmentation) {
      removeScreens(screen =>
        screen.id?.startsWith("UPGRADE_DATA_RECOMMENDATION")
      );
    }

    //If privatePin, remove checkbox from pinscreen
    if (!needPrivatePin) {
      delete content.screens?.find(
        screen => screen.id === "UPGRADE_PIN_FIREFOX"
      )?.content?.checkbox;
    }

    if (removeDefault) {
      removeScreens(screen => screen.id?.startsWith("UPGRADE_SET_DEFAULT"));
    }

    // Remove mobile download screen if user has sync enabled
    if (lazy.usesFirefoxSync && lazy.mobileDevices > 0) {
      removeScreens(screen => screen.id === "UPGRADE_MOBILE_DOWNLOAD");
    } else {
      prepareMobileDownload();
    }

    return message;
  },
};
