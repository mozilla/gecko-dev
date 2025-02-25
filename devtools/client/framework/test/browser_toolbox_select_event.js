/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PAGE_URL = "data:text/html;charset=utf-8,test select events";

requestLongerTimeout(2);

add_task(async function () {
  const tab = await addTab(PAGE_URL);

  let toolbox = await openToolboxForTab(tab, "webconsole", "bottom");
  await testSelectEvent("inspector");
  await testSelectEvent("webconsole");
  await testSelectEvent("styleeditor");
  await testSelectEvent("inspector");
  await testSelectEvent("webconsole");
  await testSelectEvent("styleeditor");

  await testToolSelectEvent("inspector");
  await testToolSelectEvent("webconsole");
  await testToolSelectEvent("styleeditor");
  await toolbox.destroy();

  toolbox = await openToolboxForTab(tab, "webconsole", "right");
  await testSelectEvent("inspector");
  await testSelectEvent("webconsole");
  await testSelectEvent("styleeditor");
  await testSelectEvent("inspector");
  await testSelectEvent("webconsole");
  await testSelectEvent("styleeditor");
  await toolbox.destroy();

  toolbox = await openToolboxForTab(tab, "webconsole", "window");
  await testSelectEvent("inspector");
  await testSelectEvent("webconsole");
  await testSelectEvent("styleeditor");
  await testSelectEvent("inspector");
  await testSelectEvent("webconsole");
  await testSelectEvent("styleeditor");
  await toolbox.destroy();

  await testSelectToolSamePanelRace();
  await testSelectToolDistinctPanelRace();

  /**
   * Assert that selecting the given toolId raises a select event
   * @param {toolId} Id of the tool to test
   */
  async function testSelectEvent(toolId) {
    const onSelect = toolbox.once("select");
    toolbox.selectTool(toolId);
    const id = await onSelect;
    is(id, toolId, toolId + " selected");
  }

  /**
   * Assert that selecting the given toolId raises its corresponding
   * selected event
   * @param {toolId} Id of the tool to test
   */
  async function testToolSelectEvent(toolId) {
    const onSelected = toolbox.once(toolId + "-selected");
    toolbox.selectTool(toolId);
    await onSelected;
    is(toolbox.currentToolId, toolId, toolId + " tool selected");
  }

  /**
   * Assert that two calls to selectTool won't race
   */
  async function testSelectToolSamePanelRace() {
    const toolbox = await openToolboxForTab(tab, "webconsole");
    let selected = false;
    const onSelect = () => {
      if (selected) {
        ok(false, "Got more than one 'select' event");
      } else {
        selected = true;
      }
    };
    toolbox.on("select", onSelect);
    const p1 = toolbox.selectTool("inspector");
    const p2 = toolbox.selectTool("inspector");
    // Check that both promises don't resolve too early
    const checkSelectToolResolution = panel => {
      ok(selected, "selectTool resolves only after 'select' event is fired");
      const inspector = toolbox.getPanel("inspector");
      is(panel, inspector, "selecTool resolves to the panel instance");
    };
    p1.then(checkSelectToolResolution);
    p2.then(checkSelectToolResolution);
    await p1;
    await p2;
    const selectedPanels = toolbox.doc.querySelectorAll(
      `.toolbox-panel[aria-selected="true"]`
    );
    is(selectedPanels.length, 1);
    is(
      selectedPanels[0].id,
      "toolbox-panel-inspector",
      "Ensure that the inspector is actually selected"
    );
    toolbox.off("select", onSelect);

    await toolbox.destroy();
  }

  /**
   * Assert that two calls to selectTool won't race
   */
  async function testSelectToolDistinctPanelRace() {
    const toolbox = await openToolboxForTab(tab, "inspector");
    let selected = false;
    const onSelect = () => {
      if (selected) {
        ok(false, "Got more than one 'select' event");
      } else {
        selected = true;
      }
    };
    toolbox.on("select", onSelect);
    // The load of the debugger will take some time, while the load of the inspector will be immediate
    const p1 = toolbox.selectTool("jsdebugger");
    const p2 = toolbox.selectTool("inspector");
    // Check that both promises don't resolve too early
    const checkSelectToolResolution = () => {
      ok(selected, "selectTool resolves only after 'select' event is fired");
    };
    p1.then(checkSelectToolResolution);
    p2.then(checkSelectToolResolution);
    await p1;
    await p2;
    const selectedPanels = toolbox.doc.querySelectorAll(
      `.toolbox-panel[aria-selected="true"]`
    );
    is(selectedPanels.length, 1);
    is(
      selectedPanels[0].id,
      "toolbox-panel-inspector",
      "Ensure that the inspector is actually selected"
    );
    toolbox.off("select", onSelect);

    await toolbox.destroy();
  }
});
