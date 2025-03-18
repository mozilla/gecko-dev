// Real test pref, matching value.
pref("dom.webidl.test1", true);

// Real string, matching value.
pref("editor.background_color", "#FFFFFF");

// Sticky, not in IGNORE_PREFS, matching value.
pref("browser.theme.content-theme", 2, sticky);

// Locked, not in IGNORE_PREFS, matching value.
pref("browser.theme.toolbar-theme", 2, locked);
