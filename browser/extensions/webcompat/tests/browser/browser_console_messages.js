"use strict";

function promiseWebCompatAddonReady() {
  return TestUtils.waitForCondition(() => {
    return (
      Services.ppmm.sharedData.get("WebCompatTests:InterventionsStatus") ===
      "active"
    );
  });
}

function sendWebCompatAddonCommand(name, data) {
  return new Promise(done => {
    const listener = {
      receiveMessage(message) {
        Services.cpmm.removeMessageListener(`WebCompat:${name}:Done`, listener);
        done(message.data);
      },
    };
    Services.cpmm.addMessageListener(`WebCompat:${name}:Done`, listener);
    Services.ppmm.broadcastAsyncMessage("WebCompat", { name, data });
  });
}

async function verifyConsoleMessage(interventions, expectedString) {
  const config = {
    id: "bugnumber_test",
    label: "test intervention",
    bugs: {
      issue1: {
        matches: ["*://example.com/*"],
      },
    },
    interventions: interventions.map(i =>
      Object.assign({ platforms: ["all"] }, i)
    ),
  };
  const results = await sendWebCompatAddonCommand("UpdateInterventions", [
    config,
  ]);
  ok(results[0].active, "Test intervention is active");

  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com"
  );
  const browser = gBrowser.selectedBrowser;
  const innerWindowId = browser.innerWindowID;
  const msgs = await ContentTask.spawn(
    browser,
    { innerWindowId },
    async function (args) {
      const ConsoleAPIStorage = Cc[
        "@mozilla.org/consoleAPI-storage;1"
      ].getService(Ci.nsIConsoleAPIStorage);
      return ConsoleAPIStorage.getEvents(args.innerWindowId);
    }
  );

  is(
    msgs.map(m => `${m.level}:${m.arguments[0]}`).join("\n"),
    expectedString
      ? `info:${expectedString} See https://bugzilla.mozilla.org/show_bug.cgi?id=bugnumber for details.`
      : "",
    `The expected message was logged for ${JSON.stringify(interventions)}`
  );
  BrowserTestUtils.removeTab(tab);
}

add_task(async function test_messages() {
  await promiseWebCompatAddonReady();

  // If no CSS, JS or UA intervention is provided, we default to "interventions".
  await verifyConsoleMessage(
    [{}],
    "Custom interventions being applied for compatibility reasons."
  );

  // If no JS intervention is provided, but CSS and/or UA ones are, we list them.
  await verifyConsoleMessage(
    [{ content_scripts: { css: [] } }],
    "Custom CSS being applied for compatibility reasons."
  );
  await verifyConsoleMessage(
    [{ ua_string: ["add_Chrome"] }],
    "Custom user-agent string being applied for compatibility reasons."
  );
  await verifyConsoleMessage(
    [{ ua_string: ["add_Chrome"], content_scripts: { css: [] } }],
    "Custom CSS and user-agent string being applied for compatibility reasons."
  );

  // If there is a JS intervention, it is responsible for logging any messages..
  await verifyConsoleMessage(
    [
      {
        ua_string: ["add_Chrome"],
        content_scripts: { css: [], js: ["log_message.js#bugnumber:css"] },
      },
    ],
    "Custom CSS being applied for compatibility reasons."
  );

  // ..and it is allowed to log nothing at all.
  await verifyConsoleMessage([{ content_scripts: { js: [] } }], undefined);
});
