/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for bug 613280";

function test() {
  loadTab(TEST_URI).then(() => {
    openConsole().then((HUD) => {
      ContentTask.spawn(gBrowser.selectedBrowser, null, function*(){
        content.console.log("foobarBazBug613280");
      });
      waitForMessages({
        webconsole: HUD,
        messages: [{
          text: "foobarBazBug613280",
          category: CATEGORY_WEBDEV,
          severity: SEVERITY_LOG,
        }],
      }).then(performTest.bind(null, HUD));
    });
  });
}

function performTest(HUD, [result]) {
  let msg = [...result.matched][0];
  let input = HUD.jsterm.inputNode;

  let clipboardSetup = function () {
    goDoCommand("cmd_copy");
  };

  let clipboardCopyDone = function () {
    finishTest();
  };

  let controller = top.document.commandDispatcher
                               .getControllerForCommand("cmd_copy");
  is(controller.isCommandEnabled("cmd_copy"), false, "cmd_copy is disabled");

  HUD.ui.output.selectMessage(msg);
  HUD.outputNode.focus();

  goUpdateCommand("cmd_copy");

  controller = top.document.commandDispatcher
                           .getControllerForCommand("cmd_copy");
  is(controller.isCommandEnabled("cmd_copy"), true, "cmd_copy is enabled");

  // Remove new lines and whitespace since getSelection() includes
  // a new line between message and line number, but the clipboard doesn't
  // @see bug 1119503
  let selectionText = (HUD.iframeWindow.getSelection() + "")
    .replace(/\r?\n|\r| /g, "");
  isnot(selectionText.indexOf("foobarBazBug613280"), -1,
        "selection text includes 'foobarBazBug613280'");

  waitForClipboard((str) => {
    return selectionText.trim() === str.trim().replace(/ /g, "");
  }, clipboardSetup, clipboardCopyDone, clipboardCopyDone);
}
