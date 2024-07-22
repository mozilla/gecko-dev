/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check evaluating and expanding getters in the console.
const TEST_URI =
  "data:text/html;charset=utf8,<!DOCTYPE html>" +
  "<h1>Object Inspector on deeply nested proxies</h1>";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    let proxy = new Proxy({}, {});
    for (let i = 0; i < 1e5; ++i) {
      proxy = new Proxy(proxy, proxy);
    }
    content.wrappedJSObject.console.log("oi-test", proxy);
  });

  const node = await waitFor(() => findConsoleAPIMessage(hud, "oi-test"));
  const oi = node.querySelector(".tree");
  const [proxyNode] = getObjectInspectorNodes(oi);

  await expandObjectInspectorNode(proxyNode);
  checkChildren(proxyNode, [`<target>`, `<handler>`]);

  const targetNode = findObjectInspectorNode(oi, "<target>");
  await expandObjectInspectorNode(targetNode);
  checkChildren(targetNode, [`<target>`, `<handler>`]);

  const handlerNode = findObjectInspectorNode(oi, "<handler>");
  await expandObjectInspectorNode(handlerNode);
  checkChildren(handlerNode, [`<target>`, `<handler>`]);
});

function checkChildren(node, expectedChildren) {
  const children = getObjectInspectorChildrenNodes(node);
  is(
    children.length,
    expectedChildren.length,
    "There is the expected number of children"
  );
  children.forEach((child, index) => {
    ok(
      child.textContent.includes(expectedChildren[index]),
      `Expected "${expectedChildren[index]}" child`
    );
  });
}
