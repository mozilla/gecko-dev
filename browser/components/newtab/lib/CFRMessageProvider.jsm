/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
const FACEBOOK_CONTAINER_PARAMS = {
  existing_addons: ["@contain-facebook", "{bb1b80be-e6b3-40a1-9b6e-9d4073343f0b}", "{a50d61ca-d27b-437a-8b52-5fd801a0a88b}"],
  open_urls: ["www.facebook.com", "facebook.com"],
  sumo_path: "extensionrecommendations",
  min_frecency: 10000,
};
const GOOGLE_TRANSLATE_PARAMS = {
  existing_addons: ["jid1-93WyvpgvxzGATw@jetpack", "{087ef4e1-4286-4be6-9aa3-8d6c420ee1db}", "{4170faaa-ee87-4a0e-b57a-1aec49282887}", "jid1-TMndP6cdKgxLcQ@jetpack",
    "s3google@translator", "{9c63d15c-b4d9-43bd-b223-37f0a1f22e2a}", "translator@zoli.bod", "{8cda9ce6-7893-4f47-ac70-a65215cec288}", "simple-translate@sienori", "@translatenow",
    "{a79fafce-8da6-4685-923f-7ba1015b8748})", "{8a802b5a-eeab-11e2-a41d-b0096288709b}", "jid0-fbHwsGfb6kJyq2hj65KnbGte3yT@jetpack", "storetranslate.plugin@gmail.com",
    "jid1-r2tWDbSkq8AZK1@jetpack", "{b384b75c-c978-4c4d-b3cf-62a82d8f8f12}", "jid1-f7dnBeTj8ElpWQ@jetpack", "{dac8a935-4775-4918-9205-5c0600087dc4}", "gtranslation2@slam.com",
    "{e20e0de5-1667-4df4-bd69-705720e37391}", "{09e26ae9-e9c1-477c-80a6-99934212f2fe}", "mgxtranslator@magemagix.com", "gtranslatewins@mozilla.org"],
  open_urls: ["translate.google.com"],
  sumo_path: "extensionrecommendations",
  min_frecency: 10000,
};
const YOUTUBE_ENHANCE_PARAMS = {
  existing_addons: ["enhancerforyoutube@maximerf.addons.mozilla.org", "{dc8f61ab-5e98-4027-98ef-bb2ff6060d71}", "{7b1bf0b6-a1b9-42b0-b75d-252036438bdc}", "jid0-UVAeBCfd34Kk5usS8A1CBiobvM8@jetpack",
    "iridium@particlecore.github.io", "jid1-ss6kLNCbNz6u0g@jetpack", "{1cf918d2-f4ea-4b4f-b34e-455283fef19f}"],
  open_urls: ["www.youtube.com", "youtube.com"],
  sumo_path: "extensionrecommendations",
  min_frecency: 10000,
};
const WIKIPEDIA_CONTEXT_MENU_SEARCH_PARAMS = {
  existing_addons: ["@wikipediacontextmenusearch", "{ebf47fc8-01d8-4dba-aa04-2118402f4b20}", "{5737a280-b359-4e26-95b0-adec5915a854}", "olivier.debroqueville@gmail.com", "{3923146e-98cb-472b-9c13-f6849d34d6b8}"],
  open_urls: ["www.wikipedia.org", "wikipedia.org"],
  sumo_path: "extensionrecommendations",
  min_frecency: 10000,
};
const REDDIT_ENHANCEMENT_PARAMS = {
  existing_addons: ["jid1-xUfzOsOFlzSOXg@jetpack"],
  open_urls: ["www.reddit.com", "reddit.com"],
  sumo_path: "extensionrecommendations",
  min_frecency: 10000,
};

const CFR_MESSAGES = [
  {
    id: "FACEBOOK_CONTAINER_1",
    template: "cfr_doorhanger",
    content: {
      bucket_id: "CFR_M1",
      notification_text: {string_id: "cfr-doorhanger-extension-notification"},
      heading_text: {string_id: "cfr-doorhanger-extension-heading"},
      info_icon: {
        label: {string_id: "cfr-doorhanger-extension-sumo-link"},
        sumo_path: FACEBOOK_CONTAINER_PARAMS.sumo_path,
      },
      addon: {
        id: "954390",
        title: "Facebook Container",
        icon: "resource://activity-stream/data/content/assets/cfr_fb_container.png",
        rating: 4.6,
        users: 299019,
        author: "Mozilla",
        amo_url: "https://addons.mozilla.org/en-US/firefox/addon/facebook-container/",
      },
      text: "Stop Facebook from tracking your activity across the web. Use Facebook the way you normally do without annoying ads following you around.",
      buttons: {
        primary: {
          label: {string_id: "cfr-doorhanger-extension-ok-button"},
          action: {
            type: "INSTALL_ADDON_FROM_URL",
            data: {url: null},
          },
        },
        secondary: [{
          label: {string_id: "cfr-doorhanger-extension-cancel-button"},
          action: {type: "CANCEL"},
        }, {
          label: {string_id: "cfr-doorhanger-extension-never-show-recommendation"},
        }, {
          label: {string_id: "cfr-doorhanger-extension-manage-settings-button"},
          action: {
            type: "OPEN_PREFERENCES_PAGE",
            data: {category: "general-cfr", origin: "CFR"},
          },
        }],
      },
    },
    frequency: {lifetime: 1},
    targeting: `
      localeLanguageCode == "en" &&
      (providerCohorts.cfr in ["one_per_day", "nightly"]) &&
      (xpinstallEnabled == true) &&
      (${JSON.stringify(FACEBOOK_CONTAINER_PARAMS.existing_addons)} intersect addonsInfo.addons|keys)|length == 0 &&
      (${JSON.stringify(FACEBOOK_CONTAINER_PARAMS.open_urls)} intersect topFrecentSites[.frecency >= ${FACEBOOK_CONTAINER_PARAMS.min_frecency}]|mapToProperty('host'))|length > 0`,
    trigger: {id: "openURL", params: FACEBOOK_CONTAINER_PARAMS.open_urls},
  },
  {
    id: "FACEBOOK_CONTAINER_3",
    template: "cfr_doorhanger",
    content: {
      bucket_id: "CFR_M1",
      notification_text: {string_id: "cfr-doorhanger-extension-notification"},
      heading_text: {string_id: "cfr-doorhanger-extension-heading"},
      info_icon: {
        label: {string_id: "cfr-doorhanger-extension-sumo-link"},
        sumo_path: FACEBOOK_CONTAINER_PARAMS.sumo_path,
      },
      addon: {
        id: "954390",
        title: "Facebook Container",
        icon: "resource://activity-stream/data/content/assets/cfr_fb_container.png",
        rating: 4.6,
        users: 299019,
        author: "Mozilla",
        amo_url: "https://addons.mozilla.org/en-US/firefox/addon/facebook-container/",
      },
      text: "Stop Facebook from tracking your activity across the web. Use Facebook the way you normally do without annoying ads following you around.",
      buttons: {
        primary: {
          label: {string_id: "cfr-doorhanger-extension-ok-button"},
          action: {
            type: "INSTALL_ADDON_FROM_URL",
            data: {url: null},
          },
        },
        secondary: [{
          label: {string_id: "cfr-doorhanger-extension-cancel-button"},
          action: {type: "CANCEL"},
        }, {
          label: {string_id: "cfr-doorhanger-extension-never-show-recommendation"},
        }, {
          label: {string_id: "cfr-doorhanger-extension-manage-settings-button"},
          action: {
            type: "OPEN_PREFERENCES_PAGE",
            data: {category: "general-cfr", origin: "CFR"},
          },
        }],
      },
    },
    frequency: {lifetime: 3},
    targeting: `
      localeLanguageCode == "en" &&
      (providerCohorts.cfr == "three_per_day") &&
      (xpinstallEnabled == true) &&
      (${JSON.stringify(FACEBOOK_CONTAINER_PARAMS.existing_addons)} intersect addonsInfo.addons|keys)|length == 0 &&
      (${JSON.stringify(FACEBOOK_CONTAINER_PARAMS.open_urls)} intersect topFrecentSites[.frecency >= ${FACEBOOK_CONTAINER_PARAMS.min_frecency}]|mapToProperty('host'))|length > 0`,
    trigger: {id: "openURL", params: FACEBOOK_CONTAINER_PARAMS.open_urls},
  },
  {
    id: "GOOGLE_TRANSLATE_1",
    template: "cfr_doorhanger",
    content: {
      bucket_id: "CFR_M1",
      notification_text: {string_id: "cfr-doorhanger-extension-notification"},
      heading_text: {string_id: "cfr-doorhanger-extension-heading"},
      info_icon: {
        label: {string_id: "cfr-doorhanger-extension-sumo-link"},
        sumo_path: GOOGLE_TRANSLATE_PARAMS.sumo_path,
      },
      addon: {
        id: "445852",
        title: "To Google Translate",
        icon: "resource://activity-stream/data/content/assets/cfr_google_translate.png",
        rating: 4.1,
        users: 313474,
        author: "Juan Escobar",
        amo_url: "https://addons.mozilla.org/en-US/firefox/addon/to-google-translate/",
      },
      text: "Instantly translate any webpage text. Simply highlight the text, right-click to open the context menu, and choose a text or aural translation.",
      buttons: {
        primary: {
          label: {string_id: "cfr-doorhanger-extension-ok-button"},
          action: {
            type: "INSTALL_ADDON_FROM_URL",
            data: {url: null},
          },
        },
        secondary: [{
          label: {string_id: "cfr-doorhanger-extension-cancel-button"},
          action: {type: "CANCEL"},
        }, {
          label: {string_id: "cfr-doorhanger-extension-never-show-recommendation"},
        }, {
          label: {string_id: "cfr-doorhanger-extension-manage-settings-button"},
          action: {
            type: "OPEN_PREFERENCES_PAGE",
            data: {category: "general-cfr", origin: "CFR"},
          },
        }],
      },
    },
    frequency: {lifetime: 1},
    targeting: `
      localeLanguageCode == "en" &&
      (providerCohorts.cfr in ["one_per_day", "nightly"]) &&
      (xpinstallEnabled == true) &&
      (${JSON.stringify(GOOGLE_TRANSLATE_PARAMS.existing_addons)} intersect addonsInfo.addons|keys)|length == 0 &&
      (${JSON.stringify(GOOGLE_TRANSLATE_PARAMS.open_urls)} intersect topFrecentSites[.frecency >= ${GOOGLE_TRANSLATE_PARAMS.min_frecency}]|mapToProperty('host'))|length > 0`,
    trigger: {id: "openURL", params: GOOGLE_TRANSLATE_PARAMS.open_urls},
  },
  {
    id: "GOOGLE_TRANSLATE_3",
    template: "cfr_doorhanger",
    content: {
      bucket_id: "CFR_M1",
      notification_text: {string_id: "cfr-doorhanger-extension-notification"},
      heading_text: {string_id: "cfr-doorhanger-extension-heading"},
      info_icon: {
        label: {string_id: "cfr-doorhanger-extension-sumo-link"},
        sumo_path: GOOGLE_TRANSLATE_PARAMS.sumo_path,
      },
      addon: {
        id: "445852",
        title: "To Google Translate",
        icon: "resource://activity-stream/data/content/assets/cfr_google_translate.png",
        rating: 4.1,
        users: 313474,
        author: "Juan Escobar",
        amo_url: "https://addons.mozilla.org/en-US/firefox/addon/to-google-translate/",
      },
      text: "Instantly translate any webpage text. Simply highlight the text, right-click to open the context menu, and choose a text or aural translation.",
      buttons: {
        primary: {
          label: {string_id: "cfr-doorhanger-extension-ok-button"},
          action: {
            type: "INSTALL_ADDON_FROM_URL",
            data: {url: null},
          },
        },
        secondary: [{
          label: {string_id: "cfr-doorhanger-extension-cancel-button"},
          action: {type: "CANCEL"},
        }, {
          label: {string_id: "cfr-doorhanger-extension-never-show-recommendation"},
        }, {
          label: {string_id: "cfr-doorhanger-extension-manage-settings-button"},
          action: {
            type: "OPEN_PREFERENCES_PAGE",
            data: {category: "general-cfr", origin: "CFR"},
          },
        }],
      },
    },
    frequency: {lifetime: 3},
    targeting: `
      localeLanguageCode == "en" &&
      (providerCohorts.cfr == "three_per_day") &&
      (xpinstallEnabled == true) &&
      (${JSON.stringify(GOOGLE_TRANSLATE_PARAMS.existing_addons)} intersect addonsInfo.addons|keys)|length == 0 &&
      (${JSON.stringify(GOOGLE_TRANSLATE_PARAMS.open_urls)} intersect topFrecentSites[.frecency >= ${GOOGLE_TRANSLATE_PARAMS.min_frecency}]|mapToProperty('host'))|length > 0`,
    trigger: {id: "openURL", params: GOOGLE_TRANSLATE_PARAMS.open_urls},
  },
  {
    id: "YOUTUBE_ENHANCE_1",
    template: "cfr_doorhanger",
    content: {
      bucket_id: "CFR_M1",
      notification_text: {string_id: "cfr-doorhanger-extension-notification"},
      heading_text: {string_id: "cfr-doorhanger-extension-heading"},
      info_icon: {
        label: {string_id: "cfr-doorhanger-extension-sumo-link"},
        sumo_path: YOUTUBE_ENHANCE_PARAMS.sumo_path,
      },
      addon: {
        id: "700308",
        title: "Enhancer for YouTube\u2122",
        icon: "resource://activity-stream/data/content/assets/cfr_enhancer_youtube.png",
        rating: 4.8,
        users: 357328,
        author: "Maxime RF",
        amo_url: "https://addons.mozilla.org/en-US/firefox/addon/enhancer-for-youtube/",
      },
      text: "Take control of your YouTube experience. Automatically block annoying ads, set playback speed and volume, remove annotations, and more.",
      buttons: {
        primary: {
          label: {string_id: "cfr-doorhanger-extension-ok-button"},
          action: {
            type: "INSTALL_ADDON_FROM_URL",
            data: {url: null},
          },
        },
        secondary: [{
          label: {string_id: "cfr-doorhanger-extension-cancel-button"},
          action: {type: "CANCEL"},
        }, {
          label: {string_id: "cfr-doorhanger-extension-never-show-recommendation"},
        }, {
          label: {string_id: "cfr-doorhanger-extension-manage-settings-button"},
          action: {
            type: "OPEN_PREFERENCES_PAGE",
            data: {category: "general-cfr", origin: "CFR"},
          },
        }],
      },
    },
    frequency: {lifetime: 1},
    targeting: `
      localeLanguageCode == "en" &&
      (providerCohorts.cfr in ["one_per_day", "nightly"]) &&
      (xpinstallEnabled == true) &&
      (${JSON.stringify(YOUTUBE_ENHANCE_PARAMS.existing_addons)} intersect addonsInfo.addons|keys)|length == 0 &&
      (${JSON.stringify(YOUTUBE_ENHANCE_PARAMS.open_urls)} intersect topFrecentSites[.frecency >= ${YOUTUBE_ENHANCE_PARAMS.min_frecency}]|mapToProperty('host'))|length > 0`,
    trigger: {id: "openURL", params: YOUTUBE_ENHANCE_PARAMS.open_urls},
  },
  {
    id: "YOUTUBE_ENHANCE_3",
    template: "cfr_doorhanger",
    content: {
      bucket_id: "CFR_M1",
      notification_text: {string_id: "cfr-doorhanger-extension-notification"},
      heading_text: {string_id: "cfr-doorhanger-extension-heading"},
      info_icon: {
        label: {string_id: "cfr-doorhanger-extension-sumo-link"},
        sumo_path: YOUTUBE_ENHANCE_PARAMS.sumo_path,
      },
      addon: {
        id: "700308",
        title: "Enhancer for YouTube\u2122",
        icon: "resource://activity-stream/data/content/assets/cfr_enhancer_youtube.png",
        rating: 4.8,
        users: 357328,
        author: "Maxime RF",
        amo_url: "https://addons.mozilla.org/en-US/firefox/addon/enhancer-for-youtube/",
      },
      text: "Take control of your YouTube experience. Automatically block annoying ads, set playback speed and volume, remove annotations, and more.",
      buttons: {
        primary: {
          label: {string_id: "cfr-doorhanger-extension-ok-button"},
          action: {
            type: "INSTALL_ADDON_FROM_URL",
            data: {url: null},
          },
        },
        secondary: [{
          label: {string_id: "cfr-doorhanger-extension-cancel-button"},
          action: {type: "CANCEL"},
        }, {
          label: {string_id: "cfr-doorhanger-extension-never-show-recommendation"},
        }, {
          label: {string_id: "cfr-doorhanger-extension-manage-settings-button"},
          action: {
            type: "OPEN_PREFERENCES_PAGE",
            data: {category: "general-cfr", origin: "CFR"},
          },
        }],
      },
    },
    frequency: {lifetime: 3},
    targeting: `
      localeLanguageCode == "en" &&
      (providerCohorts.cfr == "three_per_day") &&
      (xpinstallEnabled == true) &&
      (${JSON.stringify(YOUTUBE_ENHANCE_PARAMS.existing_addons)} intersect addonsInfo.addons|keys)|length == 0 &&
      (${JSON.stringify(YOUTUBE_ENHANCE_PARAMS.open_urls)} intersect topFrecentSites[.frecency >= ${YOUTUBE_ENHANCE_PARAMS.min_frecency}]|mapToProperty('host'))|length > 0`,
    trigger: {id: "openURL", params: YOUTUBE_ENHANCE_PARAMS.open_urls},
  },
  {
    id: "WIKIPEDIA_CONTEXT_MENU_SEARCH_1",
    template: "cfr_doorhanger",
    content: {
      bucket_id: "CFR_M1",
      notification_text: {string_id: "cfr-doorhanger-extension-notification"},
      heading_text: {string_id: "cfr-doorhanger-extension-heading"},
      info_icon: {
        label: {string_id: "cfr-doorhanger-extension-sumo-link"},
        sumo_path: WIKIPEDIA_CONTEXT_MENU_SEARCH_PARAMS.sumo_path,
      },
      addon: {
        id: "659026",
        title: "Wikipedia Context Menu Search",
        icon: "resource://activity-stream/data/content/assets/cfr_wiki_search.png",
        rating: 4.9,
        users: 3095,
        author: "Nick Diedrich",
        amo_url: "https://addons.mozilla.org/en-US/firefox/addon/wikipedia-context-menu-search/",
      },
      text: "Get to a Wikipedia page fast, from anywhere on the web. Just highlight any webpage text and right-click to open the context menu to start a Wikipedia search.",
      buttons: {
        primary: {
          label: {string_id: "cfr-doorhanger-extension-ok-button"},
          action: {
            type: "INSTALL_ADDON_FROM_URL",
            data: {url: null},
          },
        },
        secondary: [{
          label: {string_id: "cfr-doorhanger-extension-cancel-button"},
          action: {type: "CANCEL"},
        }, {
          label: {string_id: "cfr-doorhanger-extension-never-show-recommendation"},
        }, {
          label: {string_id: "cfr-doorhanger-extension-manage-settings-button"},
          action: {
            type: "OPEN_PREFERENCES_PAGE",
            data: {category: "general-cfr", origin: "CFR"},
          },
        }],
      },
    },
    frequency: {lifetime: 1},
    targeting: `
      localeLanguageCode == "en" &&
      (providerCohorts.cfr in ["one_per_day", "nightly"]) &&
      (xpinstallEnabled == true) &&
      (${JSON.stringify(WIKIPEDIA_CONTEXT_MENU_SEARCH_PARAMS.existing_addons)} intersect addonsInfo.addons|keys)|length == 0 &&
      (${JSON.stringify(WIKIPEDIA_CONTEXT_MENU_SEARCH_PARAMS.open_urls)} intersect topFrecentSites[.frecency >= ${WIKIPEDIA_CONTEXT_MENU_SEARCH_PARAMS.min_frecency}]|mapToProperty('host'))|length > 0`,
    trigger: {id: "openURL", params: WIKIPEDIA_CONTEXT_MENU_SEARCH_PARAMS.open_urls},
  },
  {
    id: "WIKIPEDIA_CONTEXT_MENU_SEARCH_3",
    template: "cfr_doorhanger",
    content: {
      bucket_id: "CFR_M1",
      notification_text: {string_id: "cfr-doorhanger-extension-notification"},
      heading_text: {string_id: "cfr-doorhanger-extension-heading"},
      info_icon: {
        label: {string_id: "cfr-doorhanger-extension-sumo-link"},
        sumo_path: WIKIPEDIA_CONTEXT_MENU_SEARCH_PARAMS.sumo_path,
      },
      addon: {
        id: "659026",
        title: "Wikipedia Context Menu Search",
        icon: "resource://activity-stream/data/content/assets/cfr_wiki_search.png",
        rating: 4.9,
        users: 3095,
        author: "Nick Diedrich",
        amo_url: "https://addons.mozilla.org/en-US/firefox/addon/wikipedia-context-menu-search/",
      },
      text: "Get to a Wikipedia page fast, from anywhere on the web. Just highlight any webpage text and right-click to open the context menu to start a Wikipedia search.",
      buttons: {
        primary: {
          label: {string_id: "cfr-doorhanger-extension-ok-button"},
          action: {
            type: "INSTALL_ADDON_FROM_URL",
            data: {url: null},
          },
        },
        secondary: [{
          label: {string_id: "cfr-doorhanger-extension-cancel-button"},
          action: {type: "CANCEL"},
        }, {
          label: {string_id: "cfr-doorhanger-extension-never-show-recommendation"},
        }, {
          label: {string_id: "cfr-doorhanger-extension-manage-settings-button"},
          action: {
            type: "OPEN_PREFERENCES_PAGE",
            data: {category: "general-cfr", origin: "CFR"},
          },
        }],
      },
    },
    frequency: {lifetime: 3},
    targeting: `
      localeLanguageCode == "en" &&
      (providerCohorts.cfr == "three_per_day") &&
      (xpinstallEnabled == true) &&
      (${JSON.stringify(WIKIPEDIA_CONTEXT_MENU_SEARCH_PARAMS.existing_addons)} intersect addonsInfo.addons|keys)|length == 0 &&
      (${JSON.stringify(WIKIPEDIA_CONTEXT_MENU_SEARCH_PARAMS.open_urls)} intersect topFrecentSites[.frecency >= ${WIKIPEDIA_CONTEXT_MENU_SEARCH_PARAMS.min_frecency}]|mapToProperty('host'))|length > 0`,
    trigger: {id: "openURL", params: WIKIPEDIA_CONTEXT_MENU_SEARCH_PARAMS.open_urls},
  },
  {
    id: "REDDIT_ENHANCEMENT_1",
    template: "cfr_doorhanger",
    content: {
      bucket_id: "CFR_M1",
      notification_text: {string_id: "cfr-doorhanger-extension-notification"},
      heading_text: {string_id: "cfr-doorhanger-extension-heading"},
      info_icon: {
        label: {string_id: "cfr-doorhanger-extension-sumo-link"},
        sumo_path: REDDIT_ENHANCEMENT_PARAMS.sumo_path,
      },
      addon: {
        id: "387429",
        title: "Reddit Enhancement Suite",
        icon: "resource://activity-stream/data/content/assets/cfr_reddit_enhancement.png",
        rating: 4.6,
        users: 258129,
        author: "honestbleeps",
        amo_url: "https://addons.mozilla.org/en-US/firefox/addon/reddit-enhancement-suite/",
      },
      text: "New features include Inline Image Viewer, Never Ending Reddit (never click 'next page' again), Keyboard Navigation, Account Switcher, and User Tagger.",
      buttons: {
        primary: {
          label: {string_id: "cfr-doorhanger-extension-ok-button"},
          action: {
            type: "INSTALL_ADDON_FROM_URL",
            data: {url: null},
          },
        },
        secondary: [{
          label: {string_id: "cfr-doorhanger-extension-cancel-button"},
          action: {type: "CANCEL"},
        }, {
          label: {string_id: "cfr-doorhanger-extension-never-show-recommendation"},
        }, {
          label: {string_id: "cfr-doorhanger-extension-manage-settings-button"},
          action: {
            type: "OPEN_PREFERENCES_PAGE",
            data: {category: "general-cfr", origin: "CFR"},
          },
        }],
      },
    },
    frequency: {lifetime: 1},
    targeting: `
      localeLanguageCode == "en" &&
      (providerCohorts.cfr in ["one_per_day", "nightly"]) &&
      (xpinstallEnabled == true) &&
      (${JSON.stringify(REDDIT_ENHANCEMENT_PARAMS.existing_addons)} intersect addonsInfo.addons|keys)|length == 0 &&
      (${JSON.stringify(REDDIT_ENHANCEMENT_PARAMS.open_urls)} intersect topFrecentSites[.frecency >= ${REDDIT_ENHANCEMENT_PARAMS.min_frecency}]|mapToProperty('host'))|length > 0`,
    trigger: {id: "openURL", params: REDDIT_ENHANCEMENT_PARAMS.open_urls},
  },
  {
    id: "REDDIT_ENHANCEMENT_3",
    template: "cfr_doorhanger",
    content: {
      bucket_id: "CFR_M1",
      notification_text: {string_id: "cfr-doorhanger-extension-notification"},
      heading_text: {string_id: "cfr-doorhanger-extension-heading"},
      info_icon: {
        label: {string_id: "cfr-doorhanger-extension-sumo-link"},
        sumo_path: REDDIT_ENHANCEMENT_PARAMS.sumo_path,
      },
      addon: {
        id: "387429",
        title: "Reddit Enhancement Suite",
        icon: "resource://activity-stream/data/content/assets/cfr_reddit_enhancement.png",
        rating: 4.6,
        users: 258129,
        author: "honestbleeps",
        amo_url: "https://addons.mozilla.org/en-US/firefox/addon/reddit-enhancement-suite/",
      },
      text: "New features include Inline Image Viewer, Never Ending Reddit (never click 'next page' again), Keyboard Navigation, Account Switcher, and User Tagger.",
      buttons: {
        primary: {
          label: {string_id: "cfr-doorhanger-extension-ok-button"},
          action: {
            type: "INSTALL_ADDON_FROM_URL",
            data: {url: null},
          },
        },
        secondary: [{
          label: {string_id: "cfr-doorhanger-extension-cancel-button"},
          action: {type: "CANCEL"},
        }, {
          label: {string_id: "cfr-doorhanger-extension-never-show-recommendation"},
        }, {
          label: {string_id: "cfr-doorhanger-extension-manage-settings-button"},
          action: {
            type: "OPEN_PREFERENCES_PAGE",
            data: {category: "general-cfr", origin: "CFR"},
          },
        }],
      },
    },
    frequency: {lifetime: 3},
    targeting: `
      localeLanguageCode == "en" &&
      (providerCohorts.cfr == "three_per_day") &&
      (xpinstallEnabled == true) &&
      (${JSON.stringify(REDDIT_ENHANCEMENT_PARAMS.existing_addons)} intersect addonsInfo.addons|keys)|length == 0 &&
      (${JSON.stringify(REDDIT_ENHANCEMENT_PARAMS.open_urls)} intersect topFrecentSites[.frecency >= ${REDDIT_ENHANCEMENT_PARAMS.min_frecency}]|mapToProperty('host'))|length > 0`,
    trigger: {id: "openURL", params: REDDIT_ENHANCEMENT_PARAMS.open_urls},
  },
];

const CFRMessageProvider = {
  getMessages() {
    return CFR_MESSAGES;
  },
};
this.CFRMessageProvider = CFRMessageProvider;

const EXPORTED_SYMBOLS = ["CFRMessageProvider"];
