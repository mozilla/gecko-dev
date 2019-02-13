/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let instance;

  let computedView;
  let inspector;

  waitForExplicitFinish();
  let mgr = ResponsiveUI.ResponsiveUIManager;

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    startTest();
  }, true);

  content.location = "data:text/html;charset=utf-8,<html><style>" +
    "div {" +
    "  width: 500px;" +
    "  height: 10px;" +
    "  background: purple;" +
    "} " +
    "@media screen and (max-width: 200px) {" +
    "  div { " +
    "    width: 100px;" +
    "  }" +
    "};" +
    "</style><div></div></html>"

  function computedWidth() {
    for (let prop of computedView.propertyViews) {
      if (prop.name === "width") {
        return prop.valueNode.textContent;
      }
    }
    return null;
  }

  function startTest() {
    document.getElementById("Tools:ResponsiveUI").doCommand();
    executeSoon(onUIOpen);
  }

  function onUIOpen() {
    instance = mgr.getResponsiveUIForTab(gBrowser.selectedTab);
    ok(instance, "instance of the module is attached to the tab.");

    instance.stack.setAttribute("notransition", "true");
    registerCleanupFunction(function() {
      instance.stack.removeAttribute("notransition");
    });

    instance.setSize(500, 500);

    openComputedView().then(onInspectorUIOpen);
  }

  function onInspectorUIOpen(args) {
    inspector = args.inspector;
    computedView = args.view;
    ok(inspector, "Got inspector instance");

    let div = content.document.getElementsByTagName("div")[0];

    inspector.selection.setNode(div);
    inspector.once("inspector-updated", testShrink);
  }

  function testShrink() {
    is(computedWidth(), "500px", "Should show 500px initially.");

    inspector.once("computed-view-refreshed", function onShrink() {
      is(computedWidth(), "100px", "div should be 100px after shrinking.");
      testGrow();
    });

    instance.setSize(100, 100);
  }

  function testGrow() {
    inspector.once("computed-view-refreshed", function onGrow() {
      is(computedWidth(), "500px", "Should be 500px after growing.");
      finishUp();
    });

    instance.setSize(500, 500);
  }

  function finishUp() {
    document.getElementById("Tools:ResponsiveUI").doCommand();

    // Menus are correctly updated?
    is(document.getElementById("Tools:ResponsiveUI").getAttribute("checked"), "false", "menu unchecked");

    gBrowser.removeCurrentTab();
    finish();
  }
}
