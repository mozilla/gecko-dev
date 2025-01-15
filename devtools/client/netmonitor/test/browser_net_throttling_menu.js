/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Network throttling menu smoke test.

add_task(async function () {
  await pushPref("devtools.cache.disabled", true);

  const { monitor, toolbox } = await initNetMonitor(HTTPS_CUSTOM_GET_URL, {
    requestCount: 1,
  });
  const { document } = monitor.panelWin;

  info("Opening the throttling menu");
  const popup = toolbox.doc.querySelector("#network-throttling-menu");
  document.getElementById("network-throttling").click();

  info("Waiting for the throttling menu to be displayed");
  await BrowserTestUtils.waitForPopupEvent(toolbox.doc, "shown");

  const menuItems = [...popup.querySelectorAll(".menuitem .command")];
  for (const menuItem of menuItems) {
    info(`Check the title for the menu item ${menuItem.id}.`);
    isnot(
      menuItem.getAttribute("title"),
      "",
      "The title is not an empty string"
    );
    info(`Select the throttling profile with id: ${menuItem.id}.`);
    menuItem.click();

    info(`Waiting for the '${menuItem.id}' profile to be applied`);
    await monitor.panelWin.api.once(TEST_EVENTS.THROTTLING_CHANGED);
  }

  await teardown(monitor);
});
