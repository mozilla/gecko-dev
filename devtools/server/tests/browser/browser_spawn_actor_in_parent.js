/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test DebuggerServerConnection.spawnActorInParentProcess.
// This test instanciates a first test actor "InContentActor" that uses
// spawnActorInParentProcess to instanciate the second test actor "InParentActor"

const ACTOR_URL = "chrome://mochitests/content/browser/devtools/server/tests/browser/test-spawn-actor-in-parent.js";

const { InContentFront, InParentFront } = require(ACTOR_URL);

add_task(async function() {
  const browser = await addTab("data:text/html;charset=utf-8,foo");

  info("Register target-scoped actor in the content process");
  await registerActorInContentProcess(ACTOR_URL, {
    prefix: "inContent",
    constructor: "InContentActor",
    type: { target: true },
  });

  const tab = gBrowser.getTabForBrowser(browser);
  const target = await TargetFactory.forTab(tab);
  await target.attach();
  const targetFront = target.activeTab;
  const { client } = target;
  const form = targetFront.targetForm;

  const inContentFront = InContentFront(client, form);
  const isInContent = await inContentFront.isInContent();
  ok(isInContent, "ContentActor really runs in the content process");
  const formSpawn = await inContentFront.spawnInParent(ACTOR_URL);
  const inParentFront = InParentFront(client, formSpawn);
  const {
    args,
    isInParent,
    conn,
    mm,
  } = await inParentFront.test();
  is(args[0], 1, "first actor constructor arg is correct");
  is(args[1], 2, "first actor constructor arg is correct");
  is(args[2], 3, "first actor constructor arg is correct");
  ok(isInParent, "The ParentActor really runs in the parent process");
  ok(conn, "`conn`, first contructor argument is a DebuggerServerConnection instance");
  is(mm, "ChromeMessageSender", "`mm`, last constructor argument is a message manager");

  await target.destroy();
  gBrowser.removeCurrentTab();
});
