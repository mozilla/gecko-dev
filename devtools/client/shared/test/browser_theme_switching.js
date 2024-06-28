/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const THEME_PREF = "devtools.theme";

add_task(async function () {
  await pushPref(THEME_PREF, "light");
  await pushPref("devtools.high-contrast-mode-support", true);
  // force HCM
  await pushPref("browser.display.document_color_use", 2);
  await pushPref("ui.useAccessibilityTheme", 1);

  // For some reason, mochitest spawn a very special default tab,
  // whose WindowGlobal is still the initial about:blank document.
  // This seems to be specific to mochitest, this doesn't reproduce
  // in regular firefox run. Even having about:blank as home page,
  // force loading another final about:blank document (which isn't the initial one)
  //
  // To workaround this, force opening a dedicated test tab
  const tab = await addTab("data:text/html;charset=utf-8,Test page");

  const toolbox = await gDevTools.showToolboxForTab(tab);
  const doc = toolbox.doc;
  const root = doc.documentElement;

  const platform = root.getAttribute("platform");
  const expectedPlatform = getPlatform();
  is(platform, expectedPlatform, ":root[platform] is correct");
  checkTheme(root, {
    theme: "light",
    className: "theme-light",
    forcedColorsActive: false,
  });

  await switchTheme(root, "dark");
  checkTheme(root, {
    theme: "dark",
    className: "theme-dark",
    forcedColorsActive: false,
  });

  // When setting theme to "auto", we rely on Services.appinfo.chromeColorSchemeIsDark.
  const isDark = Services.appinfo.chromeColorSchemeIsDark;
  await switchTheme(root, "auto", isDark ? "dark" : "light");
  checkTheme(root, {
    theme: isDark ? "dark" : "light",
    className: isDark ? "theme-dark" : "theme-light",
    forcedColorsActive: true,
  });

  info(
    "Check that disabling HCM will remove the forced-colors-active attribute"
  );
  await pushPref("browser.display.document_color_use", 0);
  await pushPref("ui.useAccessibilityTheme", 0);
  checkTheme(root, {
    theme: isDark ? "dark" : "light",
    className: isDark ? "theme-dark" : "theme-light",
    forcedColorsActive: false,
  });

  await toolbox.destroy();
});

function switchTheme(root, themePrefValue, appliedTheme = themePrefValue) {
  const ac = new AbortController();
  const onThemeSwitched = new Promise(res =>
    gDevTools.on(
      "theme-switched",
      (win, newTheme) => {
        if (win === root.ownerGlobal && newTheme === appliedTheme) {
          res();
          ac.abort();
        }
      },
      { signal: ac.signal }
    )
  );
  pushPref(THEME_PREF, themePrefValue);
  return onThemeSwitched;
}

function checkTheme(root, { theme, className, forcedColorsActive }) {
  const themePrefValue = Services.prefs.getCharPref(THEME_PREF);
  ok(
    root.classList.contains(className),
    `:root has ${className} class for ${themePrefValue} theme (${root.className})`
  );

  is(
    root.hasAttribute("forced-colors-active"),
    forcedColorsActive,
    `high contrast mode is ${
      !forcedColorsActive ? "not " : ""
    }supported in ${themePrefValue} theme`
  );

  const sheetsInDOM = Array.from(
    root.ownerDocument.querySelectorAll("link[rel='stylesheet']"),
    l => l.href
  );

  const sheetsFromTheme = gDevTools.getThemeDefinition(theme).stylesheets;
  info("Checking for existence of " + sheetsInDOM.length + " sheets");
  for (const themeSheet of sheetsFromTheme) {
    ok(
      sheetsInDOM.some(s => s.includes(themeSheet)),
      "There is a stylesheet for " + themeSheet
    );
  }
}

function getPlatform() {
  const { OS } = Services.appinfo;
  if (OS == "WINNT") {
    return "win";
  } else if (OS == "Darwin") {
    return "mac";
  }
  return "linux";
}
