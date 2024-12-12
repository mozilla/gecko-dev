/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the Partitioned cookies are displayed.

SpecialPowers.pushPrefEnv({
  set: [["security.allow_eval_with_system_principal", true]],
});

const doctype = `<!DOCTYPE html>`;
const cookieAttrs = `Secure;SameSite=None;Path=/`;

const NESTED_EXAMPLE_COM_URI =
  "https://example.com/document-builder.sjs?" +
  new URLSearchParams({
    html: `${doctype}
      <h1>Example.com iframe<h1>
      <script>
        document.cookie = "foo=partitioned-nested; Partitioned; ${cookieAttrs}";
      </script>`,
  });

const EXAMPLE_ORG_URI =
  "https://example.org/document-builder.sjs?" +
  new URLSearchParams({
    html: `${doctype}
      <h1>Example.org iframe<h1>
      <iframe src="${NESTED_EXAMPLE_COM_URI}"></iframe>
      <script>
        document.cookie = "fooThirdPartyPartitioned=partitioned-third-party; Partitioned; ${cookieAttrs}";
      </script>`,
  });

const EXAMPLE_COM_URI =
  "https://example.com/document-builder.sjs?" +
  new URLSearchParams({
    html: `${doctype}<h1>Top example.com<h1><iframe src="${EXAMPLE_ORG_URI}"></iframe>
      <script>
        document.cookie = "foo=unpartitioned-top; ${cookieAttrs}";
      </script>`,
  });

add_task(async function () {
  await openTabAndSetupStorage(EXAMPLE_COM_URI);
  gUI.tree.expandAll();

  info("Check that we get both partitioned and unpartitioned cookies");
  await selectTreeItem(["cookies", "https://example.com"]);
  const topLevelExampleComFooId = getCookieId("foo", "example.com", "/");
  await selectTableItem(topLevelExampleComFooId);
  checkCell(topLevelExampleComFooId, "value", "unpartitioned-top");

  const nestedExampleComFooId = getCookieId(
    "foo",
    "example.com",
    "/",
    "(https,example.com,f)"
  );
  await selectTableItem(nestedExampleComFooId);
  checkCell(nestedExampleComFooId, "value", "partitioned-nested");

  await selectTreeItem(["cookies", "https://example.org"]);
  const thirdPartyPartitionedId = getCookieId(
    "fooThirdPartyPartitioned",
    "example.org",
    "/",
    "(https,example.com)"
  );
  await selectTableItem(thirdPartyPartitionedId);
  checkCell(thirdPartyPartitionedId, "value", "partitioned-third-party");

  info("Edit unpartitioned cookie value");
  await selectTreeItem(["cookies", "https://example.com"]);

  await editCell(topLevelExampleComFooId, "value", "unpartitioned-top-updated");
  is(
    await getTopLevelContentPageCookie(),
    "foo=unpartitioned-top-updated",
    "top-level, unpartitioned cookie value was updated as expected"
  );
  is(
    await getNestedExampleComContentPageCookie(),
    "foo=partitioned-nested",
    "partitioned cookie value was not updated"
  );

  info("Edit partitioned cookie value");
  await editCell(nestedExampleComFooId, "value", "partitioned-nested-updated");
  is(
    await getNestedExampleComContentPageCookie(),
    "foo=partitioned-nested-updated",
    "partitioned cookie value was updated as expected"
  );

  info("Remove unpartitioned cookie");
  await removeCookieWithKeyboard(topLevelExampleComFooId);
  is(
    await getTopLevelContentPageCookie(),
    "",
    "top-level, unpartitioned cookie was removed"
  );
  is(
    await getNestedExampleComContentPageCookie(),
    "foo=partitioned-nested-updated",
    "partitioned cookie was not removed"
  );

  info("Remove partitioned cookie");
  await removeCookieWithKeyboard(nestedExampleComFooId);
  is(
    await getNestedExampleComContentPageCookie(),
    "",
    "partitioned cookie value was updated as expected"
  );
});

add_task(async function checkRemoveAll() {
  await openTabAndSetupStorage(EXAMPLE_COM_URI);
  gUI.tree.expandAll();

  info(
    "Check that remove all does remove all partitioned and unpartitioned cookies"
  );
  await selectTreeItem(["cookies", "https://example.com"]);

  const contextMenu =
    gPanelWindow.document.getElementById("storage-tree-popup");
  const menuDeleteAllItem = contextMenu.querySelector(
    "#storage-tree-popup-delete-all"
  );

  const eventName = "store-objects-edit";
  const onRemoved = gUI.once(eventName);

  const selector = `[data-id='${JSON.stringify([
    "cookies",
    "https://example.com",
  ])}'] > .tree-widget-item`;
  const target = gPanelWindow.document.querySelector(selector);
  await waitForContextMenu(contextMenu, target, () => {
    contextMenu.activateItem(menuDeleteAllItem);
  });
  await onRemoved;

  is(
    await getTopLevelContentPageCookie(),
    "",
    "top-level, unpartitioned cookie was removed"
  );
  is(
    await getNestedExampleComContentPageCookie(),
    "",
    "partitioned cookie was removed as well"
  );
});

function getTopLevelContentPageCookie() {
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => content.window.document.cookie
  );
}

async function getNestedExampleComContentPageCookie() {
  const remoteIframeBc = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => content.document.querySelector("iframe").browsingContext
  );
  return SpecialPowers.spawn(remoteIframeBc, [], async () => {
    const iframe = content.document.querySelector("iframe");
    // evalute code in nested remote frame
    const res = await SpecialPowers.spawn(
      iframe,
      [],
      () => content.document.cookie
    );
    return res;
  });
}

async function removeCookieWithKeyboard(cookieId) {
  await selectTableItem(cookieId);
  const onDelete = gUI.once("store-objects-edit");
  EventUtils.synthesizeKey("VK_BACK_SPACE", {}, gPanelWindow);
  await onDelete;
}
