/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


function test() {
  let inspector, toolbox;

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    waitForFocus(function() {
      let target = TargetFactory.forTab(gBrowser.selectedTab);
      gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
        startInspectorTests(toolbox);
      }).then(null, console.error);
    }, content);
  }, true);

  // Reload should reselect the currently selected markup view element.
  // This should work even when an element whose selector needs escaping
  // is selected (bug 100228).
  content.location = "data:text/html,<p id='1'>p</p>";

  function startInspectorTests(aToolbox)
  {
    toolbox = aToolbox;
    inspector = toolbox.getCurrentPanel();
    info("Inspector started");
    let p = content.document.querySelector("p");
    inspector.selection.setNode(p);
    inspector.once("inspector-updated", () => {
      is(inspector.selection.node, p, "Node selected.");
      inspector.once("markuploaded", onReload);
      content.location.reload();
    });
  }

  function onReload() {
    info("Page reloaded");
    let p = content.document.querySelector("p");
    inspector.selection.setNode(p);
    inspector.once("inspector-updated", () => {
      is(inspector.selection.node, p, "Node re-selected.");
      toolbox.destroy();
      toolbox = inspector = null;
      gBrowser.removeCurrentTab();
      finish();
    });
  }
}
