/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "data:text/html;charset=utf8,<p>test Scratchpad panel linking</p>";

let { Task } = Cu.import("resource://gre/modules/Task.jsm", {});
let { Tools } = require("main");
let { isTargetSupported } = Tools.scratchpad;

Tools.scratchpad.isTargetSupported = () => true;

add_task(function*() {
  waitForExplicitFinish();
  yield loadTab(TEST_URI);

  info("Opening toolbox with Scratchpad panel");

  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = yield gDevTools.showToolbox(target, "scratchpad", "window");

  let scratchpadPanel = toolbox.getPanel("scratchpad");
  let { scratchpad } = scratchpadPanel;
  is(toolbox.getCurrentPanel(), scratchpadPanel,
    "Scratchpad is currently selected panel");

  info("Switching to webconsole panel");

  let webconsolePanel = yield toolbox.selectTool("webconsole");
  let { hud } = webconsolePanel;
  is(toolbox.getCurrentPanel(), webconsolePanel,
    "Webconsole is currently selected panel");

  info("console.log()ing from Scratchpad");

  scratchpad.setText("console.log('foobar-from-scratchpad')");
  scratchpad.run();
  let messages = yield waitForMessages({
    webconsole: hud,
    messages: [{ text: "foobar-from-scratchpad" }]
  });

  info("Clicking link to switch to and focus Scratchpad");

  let [matched] = [...messages[0].matched];
  ok(matched, "Found logged message from Scratchpad");
  let anchor = matched.querySelector("a.message-location");

  toolbox.on("scratchpad-selected", function selected() {
    toolbox.off("scratchpad-selected", selected);

    is(toolbox.getCurrentPanel(), scratchpadPanel,
      "Clicking link switches to Scratchpad panel");

    is(Services.ww.activeWindow, toolbox.frame.ownerGlobal,
       "Scratchpad's toolbox is focused");

    Tools.scratchpad.isTargetSupported = isTargetSupported;
    finish();
  });

  EventUtils.synthesizeMouse(anchor, 2, 2, {}, hud.iframeWindow);
});
