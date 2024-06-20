/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Like most of the mochitest-browser devtools test,
// on debug test machine, it takes about 50s to run the test.
requestLongerTimeout(4);

loadTestSubscript("head_devtools.js");

function createPage(script) {
  return `<!DOCTYPE html><html><head><meta charset="utf-8"><script src="${script}"></script></head><body></body></html>`;
}

async function loadExtensionAndPanel(toolbox) {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      devtools_page: "devtools_page.html",
    },
    files: {
      async "devtools_page.js"() {
        await browser.devtools.panels.create(
          "Test Panel",
          "fake-icon.png",
          "devtools_panel.html"
        );
        browser.test.sendMessage("devtools_panel_created");
      },
      "devtools_panel.js"() {
        browser.test.sendMessage("devtools_panel_loaded");
      },
      "devtools_page.html": createPage("devtools_page.js"),
      "devtools_panel.html": createPage("devtools_panel.js"),
    },
  });
  await extension.startup();

  await extension.awaitMessage("devtools_panel_created");
  let [panel] = toolbox.getAdditionalTools();

  await toolbox.selectTool(panel.id);
  await extension.awaitMessage("devtools_panel_loaded");

  return { extension, panel };
}

function getPanelBrowser(toolbox, panel) {
  let panelFrame = toolbox.doc.getElementById(
    "toolbox-panel-iframe-" + panel.id
  );
  return panelFrame.contentDocument.getElementById("webext-panels-browser");
}

function synthesizeZoomShortcut(key, { toolbox, panelBrowser }) {
  let zoomChanged = new Promise(resolve =>
    panelBrowser.addEventListener("FullZoomChange", resolve, {
      once: true,
    })
  );
  EventUtils.synthesizeKey(key, { accelKey: true }, toolbox.win);
  return zoomChanged;
}

add_task(async function test_devtools_panel_zoom_initial() {
  await BrowserTestUtils.withNewTab("https://example.com", async browser => {
    let tab = gBrowser.getTabForBrowser(browser);
    let toolbox = await openToolboxForTab(tab);

    info("Zoom the toolbox before we load the extension and panel");
    await synthesizeZoomShortcut("+", {
      toolbox,
      panelBrowser: toolbox.win.browsingContext.embedderElement,
    });

    let { extension, panel } = await loadExtensionAndPanel(toolbox);
    let panelBrowser = getPanelBrowser(toolbox, panel);

    Assert.equal(
      panelBrowser.fullZoom,
      toolbox.win.browsingContext.fullZoom,
      "Panel should initially be zoomed when toolbox was already zoomed"
    );

    await closeToolboxForTab(tab);
    await extension.unload();
  });
});

add_task(async function test_devtools_panel_zoom_dynamic() {
  await BrowserTestUtils.withNewTab("https://example.com", async browser => {
    let tab = gBrowser.getTabForBrowser(browser);
    let toolbox = await openToolboxForTab(tab);

    let { extension, panel } = await loadExtensionAndPanel(toolbox);
    let panelBrowser = getPanelBrowser(toolbox, panel);

    let oldZoom = panelBrowser.fullZoom;

    await synthesizeZoomShortcut("+", { toolbox, panelBrowser });
    Assert.greater(
      panelBrowser.fullZoom,
      oldZoom,
      "Panel zoom should increase after Ctrl++"
    );

    oldZoom = panelBrowser.fullZoom;

    await synthesizeZoomShortcut("+", { toolbox, panelBrowser });
    Assert.greater(
      panelBrowser.fullZoom,
      oldZoom,
      "Panel zoom should increase after another Ctrl++"
    );

    oldZoom = panelBrowser.fullZoom;

    await synthesizeZoomShortcut("-", { toolbox, panelBrowser });
    Assert.less(
      panelBrowser.fullZoom,
      oldZoom,
      "Panel zoom should decrease after Ctrl+-"
    );

    await synthesizeZoomShortcut("0", { toolbox, panelBrowser });
    Assert.equal(
      panelBrowser.fullZoom,
      1.0,
      "Panel zoom should be reset to 1.0 after Ctrl+0"
    );

    await closeToolboxForTab(tab);
    await extension.unload();
  });
});
