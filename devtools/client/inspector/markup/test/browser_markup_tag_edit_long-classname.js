/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that editing long classnames shows the whole class attribute without scrollbars.

const classname =
  "this-long-class-attribute-should-be-displayed " +
  "without-overflow-when-switching-to-edit-mode " +
  "AAAAAAAAAAAA-BBBBBBBBBBBBB-CCCCCCCCCCCCC-DDDDDDDDDDDDDD-EEEEEEEEEEEEE";

// Make sure it's only one word so it can make the markup overflow horizontally
const longAttribute =
  "thislongattributenameshouldmakethemarkupviewoverflow".repeat(3);
const TEST_URL = `data:text/html;charset=utf8, <div class="${classname}" ${longAttribute}></div>`;

add_task(async function () {
  const { inspector } = await openInspectorForURL(TEST_URL);

  await selectNode("div", inspector);

  // We trigger a click on the container which is not a button, so that would make the
  // test fail on a11y_checks. Since we're handling element selection from the keyboard
  // just fine, we can disable the accessibility check to avoid the test failure.
  AccessibilityUtils.setEnv({ mustHaveAccessibleRule: false });
  await clickContainer("div", inspector);
  AccessibilityUtils.resetEnv();

  const container = await focusNode("div", inspector);
  ok(container && container.editor, "The markup-container was found");

  info("Listening for the markupmutation event");
  const nodeMutated = inspector.once("markupmutation");
  const attr = container.editor.attrElements
    .get("class")
    .querySelector(".editable");

  attr.focus();
  EventUtils.sendKey("return", inspector.panelWin);
  const input = inplaceEditor(attr).input;
  ok(input, "Found editable field for class attribute");

  is(
    input.scrollHeight,
    input.clientHeight,
    "input should not have vertical scrollbars"
  );
  is(
    input.scrollWidth,
    input.clientWidth,
    "input should not have horizontal scrollbars"
  );
  input.value = 'class="other value"';

  info("Commit the new class value");
  EventUtils.sendKey("return", inspector.panelWin);

  info("Wait for the markup-mutation event");
  await nodeMutated;

  info("Check that clicking on the node doesn't cause the overflow to change");
  const horizontalScroll = 100;
  inspector.markup.doc.documentElement.scrollLeft = horizontalScroll;

  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    inspector.markup.getSelectedContainer().elt,
    inspector.markup.win
  );
  await wait(100);
  is(
    inspector.markup.doc.documentElement.scrollLeft,
    horizontalScroll,
    "horizontal scroll didn't change when clicking on the node"
  );
});
