/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = "data:text/html;charset=utf-8,";
const NEW_USER_AGENT = "Mozilla/5.0 (Mobile; rv:39.0) Gecko/39.0 Firefox/39.0";

const testDevice = {
  name: "Fake Phone RDM Test",
  width: 320,
  height: 570,
  pixelRatio: 5.5,
  userAgent: "Mozilla/5.0 (Mobile; rv:39.0) Gecko/39.0 Firefox/39.0",
  touch: true,
  firefoxOS: true,
  os: "custom",
  featured: true,
};

// Add the new device to the list
addDeviceForTest(testDevice);

addRDMTask(TEST_URL, async function ({ ui }) {
  reloadOnUAChange(true);

  info("Check the default state of the user agent input");
  await testUserAgent(ui, DEFAULT_UA);

  info(`Change the user agent input to ${NEW_USER_AGENT} and press Escape`);
  await changeUserAgentInput(ui, NEW_USER_AGENT, "VK_ESCAPE");
  await testUserAgent(ui, DEFAULT_UA);

  info(`Change the user agent input to ${NEW_USER_AGENT}`);
  await changeUserAgentInput(ui, NEW_USER_AGENT);
  await testUserAgent(ui, NEW_USER_AGENT);

  info("Reset the user agent input back to the default UA");
  await changeUserAgentInput(ui, "");
  await testUserAgent(ui, DEFAULT_UA);

  info("Test selecting Fenix user agent");
  const firefoxVersion = AppConstants.MOZ_APP_VERSION.replace(/[ab]\d+/, "");
  await changeUserAgentFromSelector(
    ui,
    "Firefox for Android",
    `Mozilla/5.0 (Android 15; Mobile; rv:${firefoxVersion}) Gecko/${firefoxVersion} Firefox/${firefoxVersion}`
  );

  info(
    "Verify that device user agent isn't shown before the device is selected"
  );
  const { toolWindow } = ui;
  const { document } = toolWindow;
  const userAgentSelector = document.querySelector("#user-agent-selector");
  await testMenuItems(toolWindow, userAgentSelector, items => {
    const menuItem = findMenuItem(items, testDevice.name);
    ok(
      !menuItem,
      "Before selecting the device, it isn't shown in the user agent dropdown"
    );
  });

  info("Test selecting the selected device user agent");
  const waitForReload = await watchForDevToolsReload(ui.getViewportBrowser());
  await selectDevice(ui, testDevice.name);
  await waitForReload();
  // We don't expect a reload here because the user agent is already set to the
  // device's user agent when the device is selected. Selecting the user agent
  // of the device won't trigger a reload here because the user agent isn't
  // changed.
  await changeUserAgentFromSelector(
    ui,
    testDevice.name,
    testDevice.userAgent,
    false
  );

  await testMenuItems(toolWindow, userAgentSelector, items => {
    const menuItem = findMenuItem(items, testDevice.name);
    ok(menuItem, "Found the item for the device's user agent");
    is(menuItem.getAttribute("aria-checked"), "true", "The item is checked");
  });

  info("Test selecting the default user agent");
  await changeUserAgentFromSelector(ui, "Firefox Desktop", "");

  info(
    "Test changing to a custom agent string, no user agent should be selected in the dropdrown menu"
  );
  await changeUserAgentInput(ui, "Custom user agent");
  await testMenuItems(toolWindow, userAgentSelector, items => {
    for (const menuItem of items) {
      is(
        menuItem.getAttribute("aria-checked"),
        null,
        "None of the user agent items are checked"
      );
    }
  });

  reloadOnUAChange(false);
});

async function changeUserAgentFromSelector(
  ui,
  browserName,
  expectedUserAgent,
  expectReload = true
) {
  const { document } = ui.toolWindow;
  const browser = ui.getViewportBrowser();

  const changed = once(ui, "user-agent-changed");
  let waitForDevToolsReload;
  if (expectReload) {
    waitForDevToolsReload = await watchForDevToolsReload(browser);
  } else {
    waitForDevToolsReload = async () => {};
  }
  await selectMenuItem(ui, "#user-agent-selector", browserName);
  await changed;
  await waitForDevToolsReload();

  const userAgentInput = document.getElementById("user-agent-input");
  is(userAgentInput.value, expectedUserAgent);
}
