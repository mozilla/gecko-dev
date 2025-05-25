/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Those cases should be added into AllJavascriptTypes.mjs (so they can be consumed in
// browser_webconsole_previewers.js) when Bug 1967917 is resolved (so we can have a secure context)

add_task(async function () {
  const hud = await openNewTabAndConsole(URL_ROOT_COM_SSL + "test-blank.html");

  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    const openReq = content.indexedDB.open("idb-bug-demo", 1);
    openReq.onupgradeneeded = e => {
      e.target.result.createObjectStore("demo", {
        keyPath: "id",
      });
    };
    openReq.onsuccess = e => {
      const db = e.target.result;
      const store = db.transaction(["demo"], "readonly").objectStore("demo");

      const idbRequest = store.get("unknown");
      idbRequest.onsuccess = () => {
        content.console.log(idbRequest);
        db.close();
      };
    };
  });

  const message = await waitForMessageByType(hud, "IDBRequest", ".console-api");
  is(
    message.node.querySelector(".message-body").innerText.trim(),
    'IDBRequest { result: undefined, error: null, source: IDBObjectStore, transaction: IDBTransaction, readyState: "done", onsuccess: Restricted, onerror: null }',
    "Got expected IDBRequest object, with undefined result property"
  );

  const oi = message.node.querySelector(".tree");
  // Expand the root node
  await expandObjectInspectorNode(oi.querySelector(".tree-node"));
  const resultPropertyTreeItemEl = Array.from(
    oi.querySelectorAll(".object-node")
  ).find(el => el.querySelector(".object-label")?.innerText === "result");
  is(
    resultPropertyTreeItemEl.innerText,
    "result: undefined",
    "Got expected result property in the object inspector for the IDBRequest"
  );

  // This can't be placed in a registerCleanupFunction because it throws (The operation is insecure)
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    const { promise, resolve } = content.Promise.withResolvers();
    content.indexedDB.deleteDatabase("idb-bug-demo").onsuccess = resolve;
    await promise;
  });
});
