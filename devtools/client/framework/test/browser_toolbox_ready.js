/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URL = "data:text/html,test for toolbox being ready";

add_task(function* () {
  let tab = yield addTab(TEST_URL);
  let target = TargetFactory.forTab(tab);

  const toolbox = yield gDevTools.showToolbox(target, "webconsole");
  ok(toolbox.isReady, "toolbox isReady is set");
  ok(toolbox.threadClient, "toolbox has a thread client");

  const toolbox2 = yield gDevTools.showToolbox(toolbox.target, toolbox.toolId);
  is(toolbox2, toolbox, "same toolbox");

  yield toolbox.destroy();
  gBrowser.removeCurrentTab();
});
