/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests tabbing through attributes on a node

const TEST_URL = "data:text/html;charset=utf8,<div a b c d e id='test'></div>";

add_task(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);

  info("Focusing the tag editor of the test element");
  let {editor} = yield getContainerForSelector("div", inspector);
  editor.tag.focus();

  info("Pressing tab and expecting to focus the ID attribute, always first");
  EventUtils.sendKey("tab", inspector.panelWin);
  checkFocusedAttribute("id");

  info("Hit enter to turn the attribute to edit mode");
  EventUtils.sendKey("return", inspector.panelWin);
  checkFocusedAttribute("id", true);

  // Check the order of the other attributes in the DOM to the check they appear
  // correctly in the markup-view
  let attributes = [...getNode("div").attributes].filter(attr => attr.name !== "id");

  info("Tabbing forward through attributes in edit mode");
  for (let {name} of attributes) {
    collapseSelectionAndTab(inspector);
    checkFocusedAttribute(name, true);
  }

  info("Tabbing backward through attributes in edit mode");

  // Just reverse the attributes other than id and remove the first one since
  // it's already focused now.
  let reverseAttributes = attributes.reverse();
  reverseAttributes.shift();

  for (let {name} of reverseAttributes) {
    collapseSelectionAndShiftTab(inspector);
    checkFocusedAttribute(name, true);
  }
});
