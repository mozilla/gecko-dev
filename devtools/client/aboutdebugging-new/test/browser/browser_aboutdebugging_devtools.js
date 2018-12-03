/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that DevTools are not closed when leaving This Firefox runtime page.
 */

add_task(async function() {
  info("Force all debug target panes to be expanded");
  prepareCollapsibilitiesTest();

  const { document, tab, window } = await openAboutDebugging();

  const connectSidebarItem = findSidebarItemByText("Connect", document);
  const connectLink = connectSidebarItem.querySelector(".js-sidebar-link");
  ok(connectSidebarItem, "Found the Connect sidebar item");

  info("Open devtools on the current about:debugging tab");
  const toolbox = await openToolboxForTab(tab, "inspector");
  const inspector = toolbox.getPanel("inspector");

  info("DevTools starts workers, wait for requests to settle");
  const store = window.AboutDebugging.store;
  await waitForDispatch(store, "REQUEST_WORKERS_SUCCESS");
  await waitForRequestsToSettle(store);

  info("Click on the Connect item in the sidebar");
  connectLink.click();
  await waitForDispatch(store, "UNWATCH_RUNTIME_SUCCESS");

  info("Wait until Connect page is displayed");
  await waitUntil(() => document.querySelector(".js-connect-page"));

  const markupViewElement = inspector.panelDoc.getElementById("markup-box");
  ok(markupViewElement, "Inspector is still rendered");

  await removeTab(tab);
});
