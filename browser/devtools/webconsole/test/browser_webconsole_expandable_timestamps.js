/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test for the message timestamps option: check if the preference toggles the
// display of messages in the console output. See bug 722267.

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for bug 722267 - " +
               "preference for toggling timestamps in messages";
const PREF_MESSAGE_TIMESTAMP = "devtools.webconsole.timestampMessages";
let hud;

let test = asyncTest(function* () {
  yield loadTab(TEST_URI);

  hud = yield openConsole();
  let panel = yield consoleOpened();

  yield onOptionsPanelSelected(panel);
  onPrefChanged();

  Services.prefs.clearUserPref(PREF_MESSAGE_TIMESTAMP);
  hud = null;
});

function consoleOpened()
{
  info("console opened");
  let prefValue = Services.prefs.getBoolPref(PREF_MESSAGE_TIMESTAMP);
  ok(!prefValue, "messages have no timestamp by default (pref check)");
  ok(hud.outputNode.classList.contains("hideTimestamps"),
     "messages have no timestamp (class name check)");

  let toolbox = gDevTools.getToolbox(hud.target);
  return toolbox.selectTool("options");
}

function onOptionsPanelSelected(panel)
{
  info("options panel opened");

  let prefChanged = gDevTools.once("pref-changed", onPrefChanged);

  let checkbox = panel.panelDoc.getElementById("webconsole-timestamp-messages");
  checkbox.click();

  return prefChanged;
}

function onPrefChanged()
{
  info("pref changed");
  let prefValue = Services.prefs.getBoolPref(PREF_MESSAGE_TIMESTAMP);
  ok(prefValue, "messages have timestamps (pref check)");
  ok(!hud.outputNode.classList.contains("hideTimestamps"),
     "messages have timestamps (class name check)");
}
