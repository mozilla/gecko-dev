/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the SVG marker styling is updated when devtools theme changes.
 */

function spawnTest() {
  let [target, debuggee, panel] = yield initWebAudioEditor(SIMPLE_CONTEXT_URL);
  let { panelWin } = panel;
  let { gFront, $, $$, EVENTS, MARKER_STYLING } = panelWin;

  let currentTheme = Services.prefs.getCharPref("devtools.theme");

  ok(MARKER_STYLING.light, "Marker styling exists for light theme.");
  ok(MARKER_STYLING.dark, "Marker styling exists for dark theme.");

  let started = once(gFront, "start-context");

  reload(target);

  let [actors] = yield Promise.all([
    get3(gFront, "create-node"),
    waitForGraphRendered(panelWin, 3, 2)
  ]);

  is(getFill($("#arrowhead")), MARKER_STYLING[currentTheme],
    "marker initially matches theme.");

  // Switch to light
  setTheme("light");
  is(getFill($("#arrowhead")), MARKER_STYLING.light,
    "marker styling matches light theme on change.");

  // Switch to dark
  setTheme("dark");
  is(getFill($("#arrowhead")), MARKER_STYLING.dark,
    "marker styling matches dark theme on change.");

  // Switch to dark again
  setTheme("dark");
  is(getFill($("#arrowhead")), MARKER_STYLING.dark,
    "marker styling remains dark.");

  // Switch to back to light again
  setTheme("light");
  is(getFill($("#arrowhead")), MARKER_STYLING.light,
    "marker styling switches back to light once again.");

  yield teardown(panel);
  finish();
}

/**
 * Returns a hex value found in styling for an element. So parses
 * <marker style="fill: #abcdef"> and returns "#abcdef"
 */
function getFill (el) {
  return el.getAttribute("style").match(/(#.*)$/)[1];
}

/**
 * Mimics selecting the theme selector in the toolbox;
 * sets the preference and emits an event on gDevTools to trigger
 * the themeing.
 */
function setTheme (newTheme) {
  let oldTheme = Services.prefs.getCharPref("devtools.theme");
  info("Setting `devtools.theme` to \"" + newTheme + "\"");
  Services.prefs.setCharPref("devtools.theme", newTheme);
  gDevTools.emit("pref-changed", {
    pref: "devtools.theme",
    newValue: newTheme,
    oldValue: oldTheme
  });
}
