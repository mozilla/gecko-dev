/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
let doc;
let salutation;

function createDocument()
{
  doc.body.innerHTML = '<div id="first" style="{ margin: 10em; ' +
    'font-size: 14pt; font-family: helvetica, sans-serif; color: #AAA}">\n' +
    '<h1>Some header text</h1>\n' +
    '<p id="salutation" style="{font-size: 12pt}">hi.</p>\n' +
    '<p id="body" style="{font-size: 12pt}">I am a test-case. This text exists ' +
    'solely to provide some things to test the inspector initialization.</p>\n' +
    'If you are reading this, you should go do something else instead. Maybe ' +
    'read a book. Or better yet, write some test-cases for another bit of code. ' +
    '<span style="{font-style: italic}">Inspector\'s!</span></p>\n' +
    '<p id="closing">end transmission</p>\n' +
    '</div>';
  doc.title = "Inspector Initialization Test";

  let target = TargetFactory.forTab(gBrowser.selectedTab);
  gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
    startInspectorTests(toolbox);
  }).then(null, console.error);
}

function startInspectorTests(toolbox)
{
  let inspector = toolbox.getCurrentPanel();
  ok(true, "Inspector started, and notification received.");

  ok(inspector, "Inspector instance is accessible");
  ok(inspector.isReady, "Inspector instance is ready");
  is(inspector.target.tab, gBrowser.selectedTab, "Valid target");

  let p = doc.querySelector("p");

  inspector.selection.setNode(p);
  inspector.once("inspector-updated", () => {
    testMarkupView(p);
    testBreadcrumbs(p);

    let span = doc.querySelector("span");
    span.scrollIntoView();

    inspector.selection.setNode(span);
    inspector.once("inspector-updated", () => {
      testMarkupView(span);
      testBreadcrumbs(span);

      toolbox.once("destroyed", function() {
        ok("true", "'destroyed' notification received.");
        let target = TargetFactory.forTab(gBrowser.selectedTab);
        ok(!gDevTools.getToolbox(target), "Toolbox destroyed.");
        executeSoon(runContextMenuTest);
      });
      toolbox.destroy();
    });
  });
}

let callNo = 0;
function testMarkupView(node)
{
  let i = getActiveInspector();
  try {
    is(i.markup._selectedContainer.node.rawNode(), node, "Right node is selected in the markup view");
  } catch(ex) { console.error(ex); }
}

function testBreadcrumbs(node)
{
  let b = getActiveInspector().breadcrumbs;
  let expectedText = b.prettyPrintNodeAsText(getNodeFront(node));
  let button = b.container.querySelector("button[checked=true]");
  ok(button, "A crumbs is checked=true");
  is(button.getAttribute("tooltiptext"), expectedText, "Crumb refers to the right node");
}

function _clickOnInspectMenuItem(node) {
  document.popupNode = node;
  var contentAreaContextMenu = document.getElementById("contentAreaContextMenu");
  var contextMenu = new nsContextMenu(contentAreaContextMenu);
  var {Promise: promise} = Cu.import("resource://gre/modules/Promise.jsm", {});
  var deferred = promise.defer();
  contextMenu.inspectNode().then(() => {
    let i = getActiveInspector();
    i.once("inspector-updated", () => {
      deferred.resolve(undefined);
    });
  });
  return deferred.promise;
}

function runContextMenuTest()
{
  salutation = doc.getElementById("salutation");
  _clickOnInspectMenuItem(salutation).then(testInitialNodeIsSelected);
}

function testInitialNodeIsSelected() {
  testMarkupView(salutation);
  testBreadcrumbs(salutation);
  inspectNodesFromContextTestWhileOpen();
}

function inspectNodesFromContextTestWhileOpen()
{
  let closing = doc.getElementById("closing");
  getActiveInspector().selection.once("new-node", function() {
    ok(true, "Get selection's 'new-node' selection");
    executeSoon(function() {
      testMarkupView(closing);
      testBreadcrumbs(closing);
      getActiveInspector().once("inspector-updated", finishInspectorTests)
    }
  )});
  _clickOnInspectMenuItem(closing);
}

function finishInspectorTests(subject, topic, aWinIdString)
{
  gBrowser.removeCurrentTab();
  finish();
}

function test()
{
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    doc = content.document;
    waitForFocus(createDocument, content);
  }, true);

  content.location = "data:text/html;charset=utf-8,browser_inspector_initialization.js";
}
