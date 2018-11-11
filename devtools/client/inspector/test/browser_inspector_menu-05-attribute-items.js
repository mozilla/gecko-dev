/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that attribute items work in the context menu

const TEST_URL = URL_ROOT + "doc_inspector_menu.html";

add_task(function* () {
  let { inspector, testActor } = yield openInspectorForURL(TEST_URL);
  yield selectNode("#attributes", inspector);

  yield testAddAttribute();
  yield testEditAttribute();
  yield testRemoveAttribute();

  function* testAddAttribute() {
    info("Triggering 'Add Attribute' and waiting for mutation to occur");
    let addAttribute = getMenuItem("node-menu-add-attribute");
    addAttribute.click();

    EventUtils.synthesizeKey('class="u-hidden"', {});
    let onMutation = inspector.once("markupmutation");
    EventUtils.synthesizeKey("VK_RETURN", {});
    yield onMutation;

    let hasAttribute = testActor.hasNode("#attributes.u-hidden");
    ok(hasAttribute, "attribute was successfully added");
  }

  function* testEditAttribute() {
    info("Testing 'Edit Attribute' menu item");
    let editAttribute = getMenuItem("node-menu-edit-attribute");

    info("Triggering 'Edit Attribute' and waiting for mutation to occur");
    inspector.nodeMenuTriggerInfo = {
      type: "attribute",
      name: "data-edit"
    };
    editAttribute.click();
    EventUtils.synthesizeKey("data-edit='edited'", {});
    let onMutation = inspector.once("markupmutation");
    EventUtils.synthesizeKey("VK_RETURN", {});
    yield onMutation;

    let isAttributeChanged =
      yield testActor.hasNode("#attributes[data-edit='edited']");
    ok(isAttributeChanged, "attribute was successfully edited");
  }

  function* testRemoveAttribute() {
    info("Testing 'Remove Attribute' menu item");
    let removeAttribute = getMenuItem("node-menu-remove-attribute");

    info("Triggering 'Remove Attribute' and waiting for mutation to occur");
    inspector.nodeMenuTriggerInfo = {
      type: "attribute",
      name: "data-remove"
    };
    let onMutation = inspector.once("markupmutation");
    removeAttribute.click();
    yield onMutation;

    let hasAttribute = yield testActor.hasNode("#attributes[data-remove]");
    ok(!hasAttribute, "attribute was successfully removed");
  }

  function getMenuItem(id) {
    let allMenuItems = openContextMenuAndGetAllItems(inspector, {
      target: getContainerForSelector("#attributes", inspector).tagLine,
    });
    let menuItem = allMenuItems.find(i => i.id === id);
    ok(menuItem, "Menu item '" + id + "' found");
    // Close the menu so synthesizing future keys won't select menu items.
    EventUtils.synthesizeKey("VK_ESCAPE", {});
    return menuItem;
  }
});
