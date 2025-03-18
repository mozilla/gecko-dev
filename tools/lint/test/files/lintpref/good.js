// Fake prefs.
pref("foo.bar", 1);
pref("foo.baz", "1.234");

// Real test pref, different value.
pref("dom.webidl.test1", false);

// Real string, different value.
pref("editor.background_color", "#F00D1E");

// Real float, in static prefs as "34.0f".
pref("layout.accessiblecaret.width", 34);
pref("layout.accessiblecaret.height", "34.0f");

// Sticky, in IGNORE_PREFS, matching and non-matching value.
pref("devtools.console.stdout.content", true, sticky);
pref("devtools.console.stdout.content", false, sticky);
// Locked, in IGNORE_PREFS, matching and non-matching value.
pref("devtools.console.stdout.content", true, locked);
pref("devtools.console.stdout.content", false, locked);

// Sticky, not in IGNORE_PREFS, different value.
pref("browser.theme.content-theme", 1, sticky);

// Locked, not in IGNORE_PREFS, different value.
pref("browser.theme.toolbar-theme", 1, locked);
