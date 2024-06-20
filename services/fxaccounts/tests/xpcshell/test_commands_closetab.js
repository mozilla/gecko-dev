/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { CloseRemoteTab } = ChromeUtils.importESModule(
  "resource://gre/modules/FxAccountsCommands.sys.mjs"
);

const { COMMAND_CLOSETAB, COMMAND_CLOSETAB_TAIL } = ChromeUtils.importESModule(
  "resource://gre/modules/FxAccountsCommon.sys.mjs"
);

const { getRemoteCommandStore, RemoteCommand } = ChromeUtils.importESModule(
  "resource://services-sync/TabsStore.sys.mjs"
);

class TelemetryMock {
  constructor() {
    this._events = [];
    this._uuid_counter = 0;
  }

  recordEvent(object, method, value, extra = undefined) {
    this._events.push({ object, method, value, extra });
  }

  generateFlowID() {
    this._uuid_counter += 1;
    return this._uuid_counter.toString();
  }

  sanitizeDeviceId(id) {
    return id + "-san";
  }
}

function FxaInternalMock(recentDeviceList) {
  return {
    telemetry: new TelemetryMock(),
    device: {
      recentDeviceList,
    },
  };
}

add_task(async function test_closetab_isDeviceCompatible() {
  const closeTab = new CloseRemoteTab(null, null);
  let device = { name: "My device" };
  Assert.ok(!closeTab.isDeviceCompatible(device));
  device = { name: "My device", availableCommands: {} };
  Assert.ok(!closeTab.isDeviceCompatible(device));
  device = {
    name: "My device",
    availableCommands: {
      "https://identity.mozilla.com/cmd/close-uri/v1": "payload",
    },
  };
  // Even though the command is available, we're keeping this feature behind a feature
  // flag for now, so it should still show up as "not available"
  Assert.ok(!closeTab.isDeviceCompatible(device));

  // Enable the feature
  Services.prefs.setBoolPref(
    "identity.fxaccounts.commands.remoteTabManagement.enabled",
    true
  );
  Assert.ok(closeTab.isDeviceCompatible(device));

  // clear it for the next test
  Services.prefs.clearUserPref(
    "identity.fxaccounts.commands.remoteTabManagement.enabled"
  );
  closeTab.shutdown();
});

add_task(async function test_closetab_send() {
  const targetDevice = { id: "dev1", name: "Device 1" };

  const fxai = FxaInternalMock([targetDevice]);
  const closeTab = new CloseRemoteTab(null, fxai);
  let mock = sinon.mock(closeTab);

  // freeze "now" to <= when the command was sent.
  let now = Date.now();
  closeTab.now = () => now;

  // Set the delay to 10ms
  closeTab.DELAY = 10;

  // Our second command will be written "now", so becomes due in 10ms. Our timer adds 10ms "slop",
  // so expect a timer in 20ms.
  mock.expects("_ensureTimer").once().withArgs(20);
  mock
    .expects("_sendCloseTabPush")
    .once()
    .withArgs(targetDevice, ["https://foo.bar/early"])
    .resolves(true);

  // In this test we expect no commands sent but a timer instead.
  closeTab.invoke = sinon.spy((cmd, device, payload) => {
    Assert.equal(payload.encrypted, "encryptedpayload");
  });

  const store = await getRemoteCommandStore();
  // queue 2 tabs to close - one before our threshold (ie, so it should be sent) and one
  // recent enough that it remains queued and a new timer is set for it.
  const command1 = new RemoteCommand.CloseTab("https://foo.bar/early");
  Assert.ok(
    await store.addRemoteCommandAt(targetDevice.id, command1, now - 15),
    "adding the remote command should work"
  );
  const command2 = new RemoteCommand.CloseTab("https://foo.bar/late");
  Assert.ok(
    await store.addRemoteCommandAt(targetDevice.id, command2, now),
    "adding the remote command should work"
  );

  // We have the tab queued
  const pending = await store.getUnsentCommands();
  Assert.equal(pending.length, 2);

  Assert.equal(pending[0].deviceId, targetDevice.id);
  Assert.ok(pending[0].command.url, "https://foo.bar/early");
  Assert.equal(pending[1].deviceId, targetDevice.id);
  Assert.ok(pending[1].command.url, "https://foo.bar/late");

  await closeTab.flushQueue();
  // The push has been sent but a timer remains for the one remaining.
  Assert.equal((await store.getUnsentCommands()).length, 1);

  mock.verify();

  // move "now" to be 20ms timer - ie, pretending the timer fired.
  now += 20;
  // We expect the final tab to now qualify, meaning no timer is set.
  mock = sinon.mock(closeTab);
  mock.expects("_ensureTimer").never();
  mock
    .expects("_sendCloseTabPush")
    .once()
    .withArgs(targetDevice, ["https://foo.bar/late"])
    .resolves(true);

  await closeTab.flushQueue();
  // No tabs waiting
  Assert.equal((await store.getUnsentCommands()).length, 0);

  mock.verify();
  mock.restore();
  closeTab.shutdown();
});

add_task(async function test_multiple_devices() {
  const device1 = {
    id: "dev1",
    name: "Device 1",
  };
  const device2 = {
    id: "dev2",
    name: "Device 2",
  };
  const fxai = FxaInternalMock([device1, device2]);
  const closeTab = new CloseRemoteTab(null, fxai);
  const store = await getRemoteCommandStore();

  const tab1 = "https://foo.bar";
  const tab2 = "https://example.com";

  let mock = sinon.mock(closeTab);

  let now = Date.now();
  closeTab.now = () => now;

  // Set the delay to 10ms
  closeTab.DELAY = 10;

  mock.expects("_sendCloseTabPush").twice().resolves(true);

  // In this test we expect no commands sent but a timer instead.
  closeTab.invoke = sinon.spy((cmd, device, payload) => {
    Assert.equal(payload.encrypted, "encryptedpayload");
  });

  let command1 = new RemoteCommand.CloseTab(tab1);
  Assert.ok(
    await store.addRemoteCommandAt(device1.id, command1, now - 15),
    "adding the remote command should work"
  );

  let command2 = new RemoteCommand.CloseTab(tab2);
  Assert.ok(
    await store.addRemoteCommandAt(device2.id, command2, now),
    "adding the remote command should work"
  );

  // both tabs should remain pending.
  let unsentCommands = await store.getUnsentCommands();
  Assert.equal(unsentCommands.length, 2);

  // Verify both unsent commands looks as expected for each device
  Assert.equal(unsentCommands[0].deviceId, "dev1");
  Assert.equal(unsentCommands[0].command.url, "https://foo.bar");
  Assert.equal(unsentCommands[1].deviceId, "dev2");
  Assert.equal(unsentCommands[1].command.url, "https://example.com");

  // move "now" to be 20ms timer - ie, pretending the timer fired.
  now += 20;

  await closeTab.flushQueue();

  // no more in queue
  unsentCommands = await store.getUnsentCommands();
  Assert.equal(unsentCommands.length, 0);

  // This will verify the expectation set after the mock init
  mock.verify();
  mock.restore();
  closeTab.shutdown();
});

add_task(async function test_timer_reset_on_new_tab() {
  const targetDevice = {
    id: "dev1",
    name: "Device 1",
    availableCommands: { [COMMAND_CLOSETAB]: "payload" },
  };
  const fxai = FxaInternalMock([targetDevice]);
  const closeTab = new CloseRemoteTab(null, fxai);
  const store = await getRemoteCommandStore();

  const tab1 = "https://foo.bar/";
  const tab2 = "https://example.com/";

  let mock = sinon.mock(closeTab);

  let now = Date.now();
  closeTab.now = () => now;

  // Set the delay to 10ms
  closeTab.DELAY = 10;

  const ensureTimerSpy = sinon.spy(closeTab, "_ensureTimer");

  mock.expects("_sendCloseTabPush").never();

  let command1 = new RemoteCommand.CloseTab(tab1);
  Assert.ok(
    await store.addRemoteCommandAt(targetDevice.id, command1, now - 5),
    "adding the remote command should work"
  );
  await closeTab.flushQueue();

  let command2 = new RemoteCommand.CloseTab(tab2);
  Assert.ok(
    await store.addRemoteCommandAt(targetDevice.id, command2, now),
    "adding the remote command should work"
  );
  await closeTab.flushQueue();

  // both tabs should remain pending.
  let unsentCmds = await store.getUnsentCommands();
  Assert.equal(unsentCmds.length, 2);

  // _ensureTimer should've been called at least twice
  Assert.ok(ensureTimerSpy.callCount > 1);
  mock.verify();
  mock.restore();

  // Clean up any unsent commands for future tests
  for await (const cmd of unsentCmds) {
    console.log(cmd);
    await store.removeRemoteCommand(cmd.deviceId, cmd.command);
  }
  closeTab.shutdown();
});

add_task(async function test_telemetry_on_sendCloseTabPush() {
  const targetDevice = {
    id: "dev1",
    name: "Device 1",
    availableCommands: { [COMMAND_CLOSETAB]: "payload" },
  };
  const fxai = FxaInternalMock([targetDevice]);

  // Stub out invoke and _encrypt since we're mainly testing
  // the telemetry gets called okay
  const commands = {
    _invokes: [],
    invoke(cmd, device, payload) {
      this._invokes.push({ cmd, device, payload });
    },
  };
  const closeTab = new CloseRemoteTab(commands, fxai);
  closeTab._encrypt = () => "encryptedpayload";

  // freeze "now" to <= when the command was sent.
  let now = Date.now();
  closeTab.now = () => now;

  // Set the delay to 10ms
  closeTab.DELAY = 10;

  let command1 = new RemoteCommand.CloseTab("https://foo.bar/");

  const store = await getRemoteCommandStore();
  Assert.ok(
    await store.addRemoteCommandAt(targetDevice.id, command1, now - 15),
    "adding the remote command should work"
  );

  await closeTab.flushQueue();
  // Validate that _sendCloseTabPush was called correctly
  Assert.deepEqual(fxai.telemetry._events, [
    {
      object: "command-sent",
      method: COMMAND_CLOSETAB_TAIL,
      value: "dev1-san",
      extra: { flowID: "1", streamID: "2" },
    },
  ]);

  closeTab.shutdown();
});
