/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests devtools API

const Cu = Components.utils;
const toolId = "test-tool";

let tempScope = {};
Cu.import("resource://gre/modules/devtools/event-emitter.js", tempScope);
let EventEmitter = tempScope.EventEmitter;

function test() {
  addTab("about:blank", function(aBrowser, aTab) {
    runTests(aTab);
  });
}

function runTests(aTab) {
  let toolDefinition = {
    id: toolId,
    isTargetSupported: function() true,
    visibilityswitch: "devtools.test-tool.enabled",
    url: "about:blank",
    label: "someLabel",
    build: function(iframeWindow, toolbox) {
      let panel = new DevToolPanel(iframeWindow, toolbox);
      return panel.open();
    },
  };

  ok(gDevTools, "gDevTools exists");
  is(gDevTools.getToolDefinitionMap().has(toolId), false,
    "The tool is not registered");

  gDevTools.registerTool(toolDefinition);
  is(gDevTools.getToolDefinitionMap().has(toolId), true,
    "The tool is registered");

  let target = TargetFactory.forTab(gBrowser.selectedTab);

  gDevTools.showToolbox(target, toolId).then(function(toolbox) {
    // Wait for the test tool to be visible and selected.
    let { promise: testToolShown, resolve } = promise.defer();
    toolbox.once("test-tool-selected", resolve);

    return testToolShown.then(() => toolbox);
  }).then(function (toolbox) {
    is(toolbox.target, target, "toolbox target is correct");
    is(toolbox._host.hostTab, gBrowser.selectedTab, "toolbox host is correct");
    continueTests(toolbox);
  });
}

function continueTests(toolbox, panel) {
  ok(toolbox.getCurrentPanel(), "panel value is correct");
  is(toolbox.currentToolId, toolId, "toolbox _currentToolId is correct");

  ok(!toolbox.doc.getElementById("toolbox-tab-" + toolId).hasAttribute("icon-invertable"),
    "The tool tab does not have the invertable attribute");

  ok(toolbox.doc.getElementById("toolbox-tab-inspector").hasAttribute("icon-invertable"),
    "The builtin tool tabs do have the invertable attribute");

  let toolDefinitions = gDevTools.getToolDefinitionMap();
  is(toolDefinitions.has(toolId), true, "The tool is in gDevTools");

  let toolDefinition = toolDefinitions.get(toolId);
  is(toolDefinition.id, toolId, "toolDefinition id is correct");

  gDevTools.unregisterTool(toolId);
  is(gDevTools.getToolDefinitionMap().has(toolId), false,
    "The tool is no longer registered");

  // Wait for unregisterTool to select the next tool before
  // attempting to destroy.
  toolbox.on("select", function selectListener (_, id) {
    if (id !== "test-tool") {
      toolbox.off("select", selectListener);
      destroyToolbox(toolbox);
    }
  });
}

function destroyToolbox(toolbox) {
  toolbox.destroy().then(function() {
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    ok(gDevTools._toolboxes.get(target) == null, "gDevTools doesn't know about target");
    ok(toolbox._target == null, "toolbox doesn't know about target.");
    finishUp();
  });
}

function finishUp() {
  tempScope = null;
  gBrowser.removeCurrentTab();
  finish();
}

/**
* When a Toolbox is started it creates a DevToolPanel for each of the tools
* by calling toolDefinition.build(). The returned object should
* at least implement these functions. They will be used by the ToolBox.
*
* There may be no benefit in doing this as an abstract type, but if nothing
* else gives us a place to write documentation.
*/
function DevToolPanel(iframeWindow, toolbox) {
  EventEmitter.decorate(this);

  this._toolbox = toolbox;

  /*let doc = iframeWindow.document
  let label = doc.createElement("label");
  let textNode = doc.createTextNode("Some Tool");

  label.appendChild(textNode);
  doc.body.appendChild(label);*/
}

DevToolPanel.prototype = {
  open: function() {
    let deferred = promise.defer();

    executeSoon(() => {
      this._isReady = true;
      this.emit("ready");
      deferred.resolve(this);
    });

    return deferred.promise;
  },

  get target() this._toolbox.target,

  get toolbox() this._toolbox,

  get isReady() this._isReady,

  _isReady: false,

  destroy: function DTI_destroy() {
    return promise.defer(null);
  },
};
