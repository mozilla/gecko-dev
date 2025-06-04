/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

const { JSObjectsTestUtils, CONTEXTS } = ChromeUtils.importESModule(
  "resource://testing-common/JSObjectsTestUtils.sys.mjs"
);
add_setup(function () {
  JSObjectsTestUtils.init(this);
});

const EXPECTED_VALUES_FILE =
  "browser_webconsole_object_inspector_entries.snapshot.mjs";

add_task(async function () {
  // nsHttpServer does not support https
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  const hud = await openNewTabAndConsole("http://example.com");
  const outputScroller = hud.ui.outputScroller;

  let count = 0;
  await JSObjectsTestUtils.runTest(
    EXPECTED_VALUES_FILE,
    async function ({ context, expression }) {
      if (context == CONTEXTS.CHROME) {
        return undefined;
      }

      await SpecialPowers.spawn(
        gBrowser.selectedBrowser,
        [expression, count],
        function (exp, i) {
          let value;
          try {
            value = content.eval(exp);
          } catch (e) {
            value = e;
          }
          content.console.log("test message " + i, value);
        }
      );

      const messageNode = await waitFor(() => {
        // Expanding objects disables the "pinned-to-bottom" state, and because of virtualization
        // we might miss the last emitted message. Let's always scroll to the bottom before
        // checking for the message.
        outputScroller.scrollTop = outputScroller.scrollHeight;
        return findConsoleAPIMessage(hud, "test message " + count);
      });
      count++;
      const oi = messageNode.querySelector(".tree");

      if (oi) {
        const preview = [];

        // Expand the root node, otherwise the object is collapsed by default.
        await expandObjectInspectorNode(oi.querySelector(".tree-node"));

        // Do a first lookup to expand all the "<entries>" nodes,
        // as well as their immediate first entries.
        // This will generate new ".tree-node"'s.
        for (const node of oi.querySelectorAll(".tree-node")) {
          const label = node.textContent.replace(/\u200B/g, "");

          if (label == "<entries>") {
            await expandObjectInspectorNode(node);
            const firstEntry = node.nextSibling;
            if (isObjectInspectorNodeExpandable(firstEntry)) {
              await expandObjectInspectorNode(firstEntry);
            }
          }
        }

        // Generate a human-friendly representation of the state of the object inspector
        for (const node of oi.querySelectorAll(".tree-node")) {
          const label = node.textContent.replace(/\u200B/g, "");

          let icon = "\u251C "; // "|-" character
          if (isObjectInspectorNodeExpandable(node)) {
            icon = node
              .querySelector(".theme-twisty")
              .classList.contains("open")
              ? "▼  "
              : "▶︎  ";
          }
          const level = node.getAttribute("aria-level");
          const indent = "  ".repeat(parseInt(level, 10) - 1);

          preview.push(indent + icon + label);
        }

        // Help debug the test by scrolling to the very last node,
        // so that the object inspector is fully visible.
        const nodes = oi.querySelectorAll(".node");
        const lastNode = nodes[nodes.length - 1];
        lastNode.scrollIntoView();

        return preview;
      }

      // If this is a primitive data type, there won't be an object inspector,
      // but only a simple Rep that we can only stringify.
      const object = messageNode.querySelectorAll(".message-body > *")[1];
      return object.textContent;
    }
  );
});
