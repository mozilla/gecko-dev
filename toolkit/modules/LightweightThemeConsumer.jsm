/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["LightweightThemeConsumer"];

ChromeUtils.import("resource://gre/modules/Services.jsm");

const DEFAULT_THEME_ID = "default-theme@mozilla.org";
const ICONS = Services.prefs.getStringPref("extensions.webextensions.themes.icons.buttons", "").split(",");

const toolkitVariableMap = [
  ["--lwt-accent-color", {
    lwtProperty: "accentcolor",
    processColor(rgbaChannels, element) {
      if (!rgbaChannels || rgbaChannels.a == 0) {
        return "white";
      }
      // Remove the alpha channel
      const {r, g, b} = rgbaChannels;
      return `rgb(${r}, ${g}, ${b})`;
    },
  }],
  ["--lwt-text-color", {
    lwtProperty: "textcolor",
    processColor(rgbaChannels, element) {
      if (!rgbaChannels) {
        rgbaChannels = {r: 0, g: 0, b: 0};
      }
      // Remove the alpha channel
      const {r, g, b} = rgbaChannels;
      element.setAttribute("lwthemetextcolor", _isTextColorDark(r, g, b) ? "dark" : "bright");
      return `rgba(${r}, ${g}, ${b})`;
    },
  }],
  ["--arrowpanel-background", {
    lwtProperty: "popup",
  }],
  ["--arrowpanel-color", {
    lwtProperty: "popup_text",
    processColor(rgbaChannels, element) {
      const disabledColorVariable = "--panel-disabled-color";

      if (!rgbaChannels) {
        element.removeAttribute("lwt-popup-brighttext");
        element.removeAttribute("lwt-popup-darktext");
        element.style.removeProperty(disabledColorVariable);
        return null;
      }

      let {r, g, b, a} = rgbaChannels;

      if (_isTextColorDark(r, g, b)) {
        element.removeAttribute("lwt-popup-brighttext");
        element.setAttribute("lwt-popup-darktext", "true");
      } else {
        element.removeAttribute("lwt-popup-darktext");
        element.setAttribute("lwt-popup-brighttext", "true");
      }

      element.style.setProperty(disabledColorVariable, `rgba(${r}, ${g}, ${b}, 0.5)`);
      return `rgba(${r}, ${g}, ${b}, ${a})`;
    },
  }],
  ["--arrowpanel-border-color", {
    lwtProperty: "popup_border",
  }],
  ["--lwt-toolbar-field-background-color", {
    lwtProperty: "toolbar_field",
  }],
  ["--lwt-toolbar-field-color", {
    lwtProperty: "toolbar_field_text",
    processColor(rgbaChannels, element) {
      if (!rgbaChannels) {
        element.removeAttribute("lwt-toolbar-field-brighttext");
        return null;
      }
      const {r, g, b, a} = rgbaChannels;
      if (_isTextColorDark(r, g, b)) {
        element.removeAttribute("lwt-toolbar-field-brighttext");
      } else {
        element.setAttribute("lwt-toolbar-field-brighttext", "true");
      }
      return `rgba(${r}, ${g}, ${b}, ${a})`;
    },
  }],
  ["--lwt-toolbar-field-border-color", {
    lwtProperty: "toolbar_field_border",
  }],
  ["--lwt-toolbar-field-focus", {
    lwtProperty: "toolbar_field_focus",
  }],
  ["--lwt-toolbar-field-focus-color", {
    lwtProperty: "toolbar_field_text_focus",
  }],
  ["--toolbar-field-focus-border-color", {
    lwtProperty: "toolbar_field_border_focus",
  }],
];

// Get the theme variables from the app resource directory.
// This allows per-app variables.
ChromeUtils.defineModuleGetter(this, "ThemeContentPropertyList",
  "resource:///modules/ThemeVariableMap.jsm");
ChromeUtils.defineModuleGetter(this, "ThemeVariableMap",
  "resource:///modules/ThemeVariableMap.jsm");
ChromeUtils.defineModuleGetter(this, "LightweightThemeImageOptimizer",
  "resource://gre/modules/addons/LightweightThemeImageOptimizer.jsm");

function LightweightThemeConsumer(aDocument) {
  this._doc = aDocument;
  this._win = aDocument.defaultView;
  this._winId = this._win.windowUtils.outerWindowID;

  Services.obs.addObserver(this, "lightweight-theme-styling-update");

  var temp = {};
  ChromeUtils.import("resource://gre/modules/LightweightThemeManager.jsm", temp);
  this._update(temp.LightweightThemeManager.currentThemeWithPersistedData);

  this._win.addEventListener("resolutionchange", this);
  this._win.addEventListener("unload", this, { once: true });

  let darkThemeMediaQuery = this._win.matchMedia("(-moz-system-dark-theme)");
  darkThemeMediaQuery.addListener(temp.LightweightThemeManager);
  temp.LightweightThemeManager.systemThemeChanged(darkThemeMediaQuery);
}

LightweightThemeConsumer.prototype = {
  _lastData: null,
  // Whether a lightweight theme is enabled.
  _active: false,

  observe(aSubject, aTopic, aData) {
    if (aTopic != "lightweight-theme-styling-update")
      return;

    let parsedData = JSON.parse(aData);
    if (!parsedData) {
      parsedData = { theme: null, experiment: null };
    }

    if (parsedData.window && parsedData.window !== this._winId) {
      return;
    }

    this._update(parsedData.theme, parsedData.experiment);
  },

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "resolutionchange":
        if (this._active) {
          this._update(this._lastData);
        }
        break;
      case "unload":
        Services.obs.removeObserver(this, "lightweight-theme-styling-update");
        Services.ppmm.sharedData.delete(`theme/${this._winId}`);
        this._win.removeEventListener("resolutionchange", this);
        this._win = this._doc = null;
        break;
    }
  },

  _update(theme, experiment) {
    this._lastData = theme;
    if (theme) {
      theme = LightweightThemeImageOptimizer.optimize(theme, this._win.screen);
    }

    let active = this._active = theme && theme.id !== DEFAULT_THEME_ID;

    if (!theme) {
      theme = {};
    }

    let root = this._doc.documentElement;

    if (active && theme.headerURL) {
      root.setAttribute("lwtheme-image", "true");
    } else {
      root.removeAttribute("lwtheme-image");
    }

    if (active && theme.icons) {
      let activeIcons = Object.keys(theme.icons).join(" ");
      root.setAttribute("lwthemeicons", activeIcons);
    } else {
      root.removeAttribute("lwthemeicons");
    }

    for (let icon of ICONS) {
      let value = theme.icons ? theme.icons[`--${icon}-icon`] : null;
      _setImage(root, active, `--${icon}-icon`, value);
    }

    this._setExperiment(active, experiment, theme.experimental);
    _setImage(root, active, "--lwt-header-image", theme.headerURL);
    _setImage(root, active, "--lwt-additional-images", theme.additionalBackgrounds);
    _setProperties(root, active, theme);

    if (active) {
      root.setAttribute("lwtheme", "true");
    } else {
      root.removeAttribute("lwtheme");
      root.removeAttribute("lwthemetextcolor");
    }

    let contentThemeData = _getContentProperties(this._doc, active, theme);
    Services.ppmm.sharedData.set(`theme/${this._winId}`, contentThemeData);
  },

  _setExperiment(active, experiment, properties) {
    const root = this._doc.documentElement;
    if (this._lastExperimentData) {
      const { stylesheet, usedVariables } = this._lastExperimentData;
      if (stylesheet) {
        stylesheet.remove();
      }
      if (usedVariables) {
        for (const variable of usedVariables) {
          _setProperty(root, false, variable);
        }
      }
    }
    if (active && experiment) {
      this._lastExperimentData = {};
      if (experiment.stylesheet) {
        /* Stylesheet URLs are validated using WebExtension schemas */
        let stylesheetAttr = `href="${experiment.stylesheet}" type="text/css"`;
        let stylesheet = this._doc.createProcessingInstruction("xml-stylesheet",
          stylesheetAttr);
        this._doc.insertBefore(stylesheet, root);
        this._lastExperimentData.stylesheet = stylesheet;
      }
      let usedVariables = [];
      if (properties.colors) {
        for (const property in properties.colors) {
          const cssVariable = experiment.colors[property];
          const value = _sanitizeCSSColor(root.ownerDocument, properties.colors[property]);
          _setProperty(root, active, cssVariable, value);
          usedVariables.push(cssVariable);
        }
      }
      if (properties.images) {
        for (const property in properties.images) {
          const cssVariable = experiment.images[property];
          _setProperty(root, active, cssVariable, `url(${properties.images[property]})`);
          usedVariables.push(cssVariable);
        }
      }
      if (properties.properties) {
        for (const property in properties.properties) {
          const cssVariable = experiment.properties[property];
          _setProperty(root, active, cssVariable, properties.properties[property]);
          usedVariables.push(cssVariable);
        }
      }
      this._lastExperimentData.usedVariables = usedVariables;
    } else {
      this._lastExperimentData = null;
    }
  },
};

function _getContentProperties(doc, active, data) {
  if (!active) {
    return {};
  }
  let properties = {};
  for (let property in data) {
    if (ThemeContentPropertyList.includes(property)) {
      properties[property] = _parseRGBA(_sanitizeCSSColor(doc, data[property]));
    }
  }
  return properties;
}

function _setImage(aRoot, aActive, aVariableName, aURLs) {
  if (aURLs && !Array.isArray(aURLs)) {
    aURLs = [aURLs];
  }
  _setProperty(aRoot, aActive, aVariableName, aURLs && aURLs.map(v => `url("${v.replace(/"/g, '\\"')}")`).join(","));
}

function _setProperty(elem, active, variableName, value) {
  if (active && value) {
    elem.style.setProperty(variableName, value);
  } else {
    elem.style.removeProperty(variableName);
  }
}

function _setProperties(root, active, themeData) {
  for (let map of [toolkitVariableMap, ThemeVariableMap]) {
    for (let [cssVarName, definition] of map) {
      const {
        lwtProperty,
        optionalElementID,
        processColor,
        isColor = true,
      } = definition;
      let elem = optionalElementID ? root.ownerDocument.getElementById(optionalElementID)
                                   : root;

      let val = themeData[lwtProperty];
      if (isColor) {
        val = _sanitizeCSSColor(root.ownerDocument, val);
        if (processColor) {
          val = processColor(_parseRGBA(val), elem);
        }
      }
      _setProperty(elem, active, cssVarName, val);
    }
  }
}

function _sanitizeCSSColor(doc, cssColor) {
  if (!cssColor) {
    return null;
  }
  const HTML_NS = "http://www.w3.org/1999/xhtml";
  // style.color normalizes color values and makes invalid ones black, so a
  // simple round trip gets us a sanitized color value.
  let div = doc.createElementNS(HTML_NS, "div");
  div.style.color = "black";
  let span = doc.createElementNS(HTML_NS, "span");
  span.style.color = cssColor;
  div.appendChild(span);
  cssColor = doc.defaultView.getComputedStyle(span).color;
  return cssColor;
}

function _parseRGBA(aColorString) {
  if (!aColorString) {
    return null;
  }
  var rgba = aColorString.replace(/(rgba?\()|(\)$)/g, "").split(",");
  rgba = rgba.map(x => parseFloat(x));
  return {
    r: rgba[0],
    g: rgba[1],
    b: rgba[2],
    a: 3 in rgba ? rgba[3] : 1,
  };
}

function _isTextColorDark(r, g, b) {
  return (0.2125 * r + 0.7154 * g + 0.0721 * b) <= 110;
}
