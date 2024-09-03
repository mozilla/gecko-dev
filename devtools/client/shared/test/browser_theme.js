/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that theme utilities work

const {
  getCssVariableColor,
  getTheme,
  setTheme,
} = require("resource://devtools/client/shared/theme.js");
const { PrefObserver } = require("resource://devtools/client/shared/prefs.js");

add_task(async function () {
  const tab = await addTab("data:text/html;charset=utf-8,Test page");
  const toolbox = await gDevTools.showToolboxForTab(tab);

  testGetTheme();
  testSetTheme();
  await testGetCssVariableColor(toolbox.win);
  await testColorExistence(toolbox.win);
});

function testGetTheme() {
  const originalTheme = getTheme();
  ok(originalTheme, "has some theme to start with.");
  Services.prefs.setCharPref("devtools.theme", "light");
  is(getTheme(), "light", "getTheme() correctly returns light theme");
  Services.prefs.setCharPref("devtools.theme", "dark");
  is(getTheme(), "dark", "getTheme() correctly returns dark theme");
  Services.prefs.setCharPref("devtools.theme", "unknown");
  is(getTheme(), "unknown", "getTheme() correctly returns an unknown theme");
  Services.prefs.setCharPref("devtools.theme", originalTheme);
}

function testSetTheme() {
  const originalTheme = getTheme();
  // Put this in a variable rather than hardcoding it because the default
  // changes between aurora and nightly
  const otherTheme = originalTheme == "dark" ? "light" : "dark";

  const prefObserver = new PrefObserver("devtools.");
  prefObserver.once("devtools.theme", () => {
    const newValue = Services.prefs.getCharPref("devtools.theme");
    is(
      newValue,
      otherTheme,
      "A preference event triggered by setTheme comes after the value is set."
    );
  });
  setTheme(otherTheme);
  is(
    Services.prefs.getCharPref("devtools.theme"),
    otherTheme,
    "setTheme() correctly sets another theme."
  );
  setTheme(originalTheme);
  is(
    Services.prefs.getCharPref("devtools.theme"),
    originalTheme,
    "setTheme() correctly sets the original theme."
  );
  setTheme("unknown");
  is(
    Services.prefs.getCharPref("devtools.theme"),
    "unknown",
    "setTheme() correctly sets an unknown theme."
  );
  Services.prefs.setCharPref("devtools.theme", originalTheme);

  prefObserver.destroy();
}

async function setDarkTheme(win) {
  setTheme("dark");
  await waitFor(() =>
    InspectorUtils.isUsedColorSchemeDark(win.document.documentElement)
  );
}

async function setLightTheme(win) {
  setTheme("light");
  await waitFor(
    () => !InspectorUtils.isUsedColorSchemeDark(win.document.documentElement)
  );
}

async function testGetCssVariableColor(toolboxWin) {
  // getCssVariableColor return the computed color, which are in the rgb() format, so we
  // need to computed them from the original hex values.
  // --theme-highlight-blue: light-dark(var(--blue-55), #75bfff);
  // --blue-55: #0074e8;
  const LIGHT_RGB = InspectorUtils.colorToRGBA("#0074e8");
  const DARK_RGB = InspectorUtils.colorToRGBA("#75bfff");
  const BLUE_LIGHT = `rgb(${LIGHT_RGB.r}, ${LIGHT_RGB.g}, ${LIGHT_RGB.b})`;
  const BLUE_DARK = `rgb(${DARK_RGB.r}, ${DARK_RGB.g}, ${DARK_RGB.b})`;

  // Sanity check
  ok(InspectorUtils.isValidCSSColor(BLUE_LIGHT), `BLUE_LIGHT is a valid color`);
  ok(InspectorUtils.isValidCSSColor(BLUE_DARK), `BLUE_DARK is a valid color`);

  const originalTheme = getTheme();

  await setLightTheme(toolboxWin);
  is(
    getCssVariableColor("--theme-highlight-blue", toolboxWin),
    BLUE_LIGHT,
    "correctly gets color for enabled theme."
  );

  await setDarkTheme(toolboxWin);
  is(
    getCssVariableColor("--theme-highlight-blue", toolboxWin),
    BLUE_DARK,
    "correctly gets color for enabled theme."
  );

  setTheme("metal");
  // wait until we're no longer in dark mode
  await waitFor(
    () =>
      !InspectorUtils.isUsedColorSchemeDark(toolboxWin.document.documentElement)
  );
  is(
    getCssVariableColor("--theme-highlight-blue", toolboxWin),
    BLUE_LIGHT,
    "correctly uses light for default theme if enabled theme not found"
  );

  is(
    getCssVariableColor("--theme-somecomponents", toolboxWin),
    null,
    "if a type cannot be found, should return null."
  );

  setTheme(originalTheme);
}

async function testColorExistence(win) {
  const vars = [
    "--theme-body-background",
    "--theme-sidebar-background",
    "--theme-contrast-background",
    "--theme-tab-toolbar-background",
    "--theme-toolbar-background",
    "--theme-selection-background",
    "--theme-selection-color",
    "--theme-selection-background-hover",
    "--theme-splitter-color",
    "--theme-comment",
    "--theme-body-color",
    "--theme-text-color-alt",
    "--theme-text-color-inactive",
    "--theme-text-color-strong",
    "--theme-highlight-green",
    "--theme-highlight-blue",
    "--theme-highlight-bluegrey",
    "--theme-highlight-purple",
    "--theme-highlight-lightorange",
    "--theme-highlight-orange",
    "--theme-highlight-red",
    "--theme-highlight-pink",
  ];

  const originalTheme = getTheme();
  await setLightTheme(win);
  for (const variableName of vars) {
    const color = getCssVariableColor(variableName, win);
    ok(color, `${variableName} exists in light theme`);
    ok(
      InspectorUtils.isValidCSSColor(color),
      `${variableName} is a valid color in light theme`
    );
  }

  await setDarkTheme(win);
  for (const variableName of vars) {
    const color = getCssVariableColor(variableName, win);
    ok(color, `${variableName} exists in dark theme`);
    ok(
      InspectorUtils.isValidCSSColor(color),
      `${variableName} is a valid color in dark theme`
    );
  }

  setTheme(originalTheme);
}
