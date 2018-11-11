"use strict";

/* global windowTracker, EventManager, EventEmitter */

/* eslint-disable complexity */

ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "LightweightThemeManager",
                               "resource://gre/modules/LightweightThemeManager.jsm");

var {
  getWinUtils,
} = ExtensionUtils;

const ICONS = Services.prefs.getStringPref("extensions.webextensions.themes.icons.buttons", "").split(",");

const onUpdatedEmitter = new EventEmitter();

// Represents an empty theme for convenience of use
const emptyTheme = {
  details: {},
};


let defaultTheme = emptyTheme;
// Map[windowId -> Theme instance]
let windowOverrides = new Map();

/**
 * Class representing either a global theme affecting all windows or an override on a specific window.
 * Any extension updating the theme with a new global theme will replace the singleton defaultTheme.
 */
class Theme {
  /**
   * Creates a theme instance.
   *
   * @param {string} extension Extension that created the theme.
   * @param {Integer} windowId The windowId where the theme is applied.
   */
  constructor({extension, details, windowId, experiment}) {
    this.extension = extension;
    this.details = details;
    this.windowId = windowId;

    this.lwtStyles = {
      icons: {},
    };

    if (experiment) {
      const canRunExperiment = AppConstants.MOZ_ALLOW_LEGACY_EXTENSIONS &&
        Services.prefs.getBoolPref("extensions.legacy.enabled");
      if (canRunExperiment) {
        this.lwtStyles.experimental = {
          colors: {},
          images: {},
          properties: {},
        };
        const {baseURI} = this.extension;
        if (experiment.stylesheet) {
          experiment.stylesheet = baseURI.resolve(experiment.stylesheet);
        }
        this.experiment = experiment;
      } else {
        const {logger} = this.extension;
        logger.warn("This extension is not allowed to run theme experiments");
        return;
      }
    }
    this.load();
  }

  /**
   * Loads a theme by reading the properties from the extension's manifest.
   * This method will override any currently applied theme.
   *
   * @param {Object} details Theme part of the manifest. Supported
   *   properties can be found in the schema under ThemeType.
   */
  load() {
    const {extension, details} = this;

    if (details.colors) {
      this.loadColors(details.colors);
    }

    if (details.images) {
      this.loadImages(details.images);
    }

    if (details.icons) {
      this.loadIcons(details.icons);
    }

    if (details.properties) {
      this.loadProperties(details.properties);
    }

    this.loadMetadata(extension);

    let lwtData = {
      theme: this.lwtStyles,
    };

    if (this.windowId) {
      lwtData.window =
        getWinUtils(windowTracker.getWindow(this.windowId)).outerWindowID;
      windowOverrides.set(this.windowId, this);
    } else {
      windowOverrides.clear();
      defaultTheme = this;
    }
    onUpdatedEmitter.emit("theme-updated", this.details, this.windowId);

    if (this.experiment) {
      lwtData.experiment = this.experiment;
    }
    LightweightThemeManager.fallbackThemeData = this.lwtStyles;
    Services.obs.notifyObservers(null,
                                 "lightweight-theme-styling-update",
                                 JSON.stringify(lwtData));
  }

  /**
   * Helper method for loading colors found in the extension's manifest.
   *
   * @param {Object} colors Dictionary mapping color properties to values.
   */
  loadColors(colors) {
    for (let color of Object.keys(colors)) {
      let val = colors[color];

      if (!val) {
        continue;
      }

      let cssColor = val;
      if (Array.isArray(val)) {
        cssColor = "rgb" + (val.length > 3 ? "a" : "") + "(" + val.join(",") + ")";
      }

      switch (color) {
        case "accentcolor":
        case "frame":
          this.lwtStyles.accentcolor = cssColor;
          break;
        case "frame_inactive":
          this.lwtStyles.accentcolorInactive = cssColor;
          break;
        case "textcolor":
        case "tab_background_text":
          this.lwtStyles.textcolor = cssColor;
          break;
        case "toolbar":
          this.lwtStyles.toolbarColor = cssColor;
          break;
        case "toolbar_text":
        case "bookmark_text":
          this.lwtStyles.toolbar_text = cssColor;
          break;
        case "icons":
          this.lwtStyles.icon_color = cssColor;
          break;
        case "icons_attention":
          this.lwtStyles.icon_attention_color = cssColor;
          break;
        case "tab_background_separator":
        case "tab_loading":
        case "tab_text":
        case "tab_line":
        case "tab_selected":
        case "toolbar_field":
        case "toolbar_field_text":
        case "toolbar_field_border":
        case "toolbar_field_separator":
        case "toolbar_field_focus":
        case "toolbar_field_text_focus":
        case "toolbar_field_border_focus":
        case "toolbar_top_separator":
        case "toolbar_bottom_separator":
        case "toolbar_vertical_separator":
        case "button_background_hover":
        case "button_background_active":
        case "popup":
        case "popup_text":
        case "popup_border":
        case "popup_highlight":
        case "popup_highlight_text":
        case "ntp_background":
        case "ntp_text":
        case "sidebar":
        case "sidebar_border":
        case "sidebar_text":
        case "sidebar_highlight":
        case "sidebar_highlight_text":
          this.lwtStyles[color] = cssColor;
          break;
        default:
          if (this.experiment && this.experiment.colors && color in this.experiment.colors) {
            this.lwtStyles.experimental.colors[color] = cssColor;
          } else {
            const {logger} = this.extension;
            logger.warn(`Unrecognized theme property found: colors.${color}`);
          }
          break;
      }
    }
  }

  /**
   * Helper method for loading images found in the extension's manifest.
   *
   * @param {Object} images Dictionary mapping image properties to values.
   */
  loadImages(images) {
    const {baseURI, logger} = this.extension;

    for (let image of Object.keys(images)) {
      let val = images[image];

      if (!val) {
        continue;
      }

      switch (image) {
        case "additional_backgrounds": {
          let backgroundImages = val.map(img => baseURI.resolve(img));
          this.lwtStyles.additionalBackgrounds = backgroundImages;
          break;
        }
        case "headerURL":
        case "theme_frame": {
          let resolvedURL = baseURI.resolve(val);
          this.lwtStyles.headerURL = resolvedURL;
          break;
        }
        default: {
          if (this.experiment && this.experiment.images && image in this.experiment.images) {
            this.lwtStyles.experimental.images[image] = baseURI.resolve(val);
          } else {
            logger.warn(`Unrecognized theme property found: images.${image}`);
          }
          break;
        }
      }
    }
  }

  /**
   * Helper method for loading icons found in the extension's manifest.
   *
   * @param {Object} icons Dictionary mapping icon properties to extension URLs.
   */
  loadIcons(icons) {
    const {baseURI} = this.extension;

    if (!Services.prefs.getBoolPref("extensions.webextensions.themes.icons.enabled")) {
      // Return early if icons are disabled.
      return;
    }

    for (let icon of Object.getOwnPropertyNames(icons)) {
      let val = icons[icon];
      // We also have to compare against the baseURI spec because
      // `val` might have been resolved already. Resolving "" against
      // the baseURI just produces that URI, so check for equality.
      if (!val || val == baseURI.spec || !ICONS.includes(icon)) {
        continue;
      }
      let variableName = `--${icon}-icon`;
      let resolvedURL = baseURI.resolve(val);
      this.lwtStyles.icons[variableName] = resolvedURL;
    }
  }

  /**
   * Helper method for preparing properties found in the extension's manifest.
   * Properties are commonly used to specify more advanced behavior of colors,
   * images or icons.
   *
   * @param {Object} properties Dictionary mapping properties to values.
   */
  loadProperties(properties) {
    let additionalBackgroundsCount = (this.lwtStyles.additionalBackgrounds &&
      this.lwtStyles.additionalBackgrounds.length) || 0;
    const assertValidAdditionalBackgrounds = (property, valueCount) => {
      const {logger} = this.extension;
      if (!additionalBackgroundsCount) {
        logger.warn(`The '${property}' property takes effect only when one ` +
          `or more additional background images are specified using the 'additional_backgrounds' property.`);
        return false;
      }
      if (additionalBackgroundsCount !== valueCount) {
        logger.warn(`The amount of values specified for '${property}' ` +
          `(${valueCount}) is not equal to the amount of additional background ` +
          `images (${additionalBackgroundsCount}), which may lead to unexpected results.`);
      }
      return true;
    };

    for (let property of Object.getOwnPropertyNames(properties)) {
      let val = properties[property];

      if (!val) {
        continue;
      }

      switch (property) {
        case "additional_backgrounds_alignment": {
          if (!assertValidAdditionalBackgrounds(property, val.length)) {
            break;
          }

          this.lwtStyles.backgroundsAlignment = val.join(",");
          break;
        }
        case "additional_backgrounds_tiling": {
          if (!assertValidAdditionalBackgrounds(property, val.length)) {
            break;
          }

          let tiling = [];
          for (let i = 0, l = this.lwtStyles.additionalBackgrounds.length; i < l; ++i) {
            tiling.push(val[i] || "no-repeat");
          }
          this.lwtStyles.backgroundsTiling = tiling.join(",");
          break;
        }
        default: {
          if (this.experiment && this.experiment.properties && property in this.experiment.properties) {
            this.lwtStyles.experimental.properties[property] = val;
          } else {
            const {logger} = this.extension;
            logger.warn(`Unrecognized theme property found: properties.${property}`);
          }
          break;
        }
      }
    }
  }

  /**
   * Helper method for loading extension metadata required by downstream
   * consumers.
   *
   * @param {Object} extension Extension object.
   */
  loadMetadata(extension) {
    this.lwtStyles.id = extension.id;
    this.lwtStyles.version = extension.version;
  }

  static unload(windowId) {
    let lwtData = {
      theme: null,
    };

    if (windowId) {
      lwtData.window = getWinUtils(windowTracker.getWindow(windowId)).outerWindowID;
      windowOverrides.set(windowId, emptyTheme);
    } else {
      windowOverrides.clear();
      defaultTheme = emptyTheme;
    }
    onUpdatedEmitter.emit("theme-updated", {}, windowId);

    LightweightThemeManager.fallbackThemeData = null;
    Services.obs.notifyObservers(null,
                                 "lightweight-theme-styling-update",
                                 JSON.stringify(lwtData));
  }
}

this.theme = class extends ExtensionAPI {
  onManifestEntry(entryName) {
    let {extension} = this;
    let {manifest} = extension;
    let {theme, theme_experiment} = manifest;

    defaultTheme = new Theme({
      extension,
      details: theme,
      experiment: theme_experiment,
    });
  }

  onShutdown(reason) {
    if (reason === "APP_SHUTDOWN") {
      return;
    }

    let {extension} = this;
    for (let [windowId, theme] of windowOverrides) {
      if (theme.extension === extension) {
        Theme.unload(windowId);
      }
    }

    if (defaultTheme.extension === extension) {
      Theme.unload();
    }
  }

  getAPI(context) {
    let {extension} = context;

    return {
      theme: {
        getCurrent: (windowId) => {
          // Take last focused window when no ID is supplied.
          if (!windowId) {
            windowId = windowTracker.getId(windowTracker.topWindow);
          }

          if (windowOverrides.has(windowId)) {
            return Promise.resolve(windowOverrides.get(windowId).details);
          }
          return Promise.resolve(defaultTheme.details);
        },
        update: (windowId, details) => {
          if (windowId) {
            const browserWindow = windowTracker.getWindow(windowId, context);
            if (!browserWindow) {
              return Promise.reject(`Invalid window ID: ${windowId}`);
            }
          }

          new Theme({
            extension,
            details,
            windowId,
            experiment: this.extension.manifest.theme_experiment,
          });
        },
        reset: (windowId) => {
          if (windowId) {
            const browserWindow = windowTracker.getWindow(windowId, context);
            if (!browserWindow) {
              return Promise.reject(`Invalid window ID: ${windowId}`);
            }
          }

          if (!defaultTheme && !windowOverrides.has(windowId)) {
            // If no theme has been initialized, nothing to do.
            return;
          }

          Theme.unload(windowId);
        },
        onUpdated: new EventManager({
          context,
          name: "theme.onUpdated",
          register: fire => {
            let callback = (event, theme, windowId) => {
              if (windowId) {
                fire.async({theme, windowId});
              } else {
                fire.async({theme});
              }
            };

            onUpdatedEmitter.on("theme-updated", callback);
            return () => {
              onUpdatedEmitter.off("theme-updated", callback);
            };
          },
        }).api(),
      },
    };
  }
};
