/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const THEME_PREF = "devtools.theme";

/* eslint-disable no-unused-vars */

/**
 * Returns the "auto" theme.
 */
const getAutoTheme = (exports.getAutoTheme = () => {
  return Services.appinfo.chromeColorSchemeIsDark ? "dark" : "light";
});

/**
 * Returns the string value of the current theme,
 * like "dark" or "light".
 */
const getTheme = (exports.getTheme = () => {
  const theme = getThemePrefValue();
  if (theme == "auto") {
    return getAutoTheme();
  }
  return theme;
});

/**
 * Returns the value of the pref of the current theme,
 * like "auto", "dark" or "light".
 */
const getThemePrefValue = (exports.getThemePrefValue = () => {
  return Services.prefs.getCharPref(THEME_PREF, "");
});

/**
 * Returns the color of a CSS variable (--theme-toolbar-background, --theme-highlight-red),
 * for the current toolbox theme, or null if the variable does not exist, or it's not a
 * registered property, or doesn't have a <color> syntax.
 *
 * @param {String} variableName
 * @param {Window} win: The window into which the variable should be defined.
 * @returns {String|null}
 */
const getCssVariableColor = (exports.getCssVariableColor = (
  variableName,
  win
) => {
  const value = win
    .getComputedStyle(win.document.documentElement)
    .getPropertyValue(variableName);

  if (!value) {
    console.warn("Unknown", variableName, "CSS variable");
    return null;
  }

  return value;
});

/**
 * Set the theme preference.
 */
const setTheme = (exports.setTheme = newTheme => {
  Services.prefs.setCharPref(THEME_PREF, newTheme);
});

/**
 * Add an observer for theme changes.
 */
const addThemeObserver = (exports.addThemeObserver = observer => {
  Services.obs.addObserver(observer, "look-and-feel-changed");
  Services.prefs.addObserver(THEME_PREF, observer);
});

/**
 * Remove an observer for theme changes.
 */
const removeThemeObserver = (exports.removeThemeObserver = observer => {
  Services.obs.removeObserver(observer, "look-and-feel-changed");
  Services.prefs.removeObserver(THEME_PREF, observer);
});
/* eslint-enable */
