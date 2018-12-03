"use strict";

ChromeUtils.import("resource://gre/modules/TelemetryArchive.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryUtils.jsm", this);

function createExtension(backgroundScript, permissions) {
  let extensionData = {
    background: backgroundScript,
    manifest: {permissions},
  };
  return ExtensionTestUtils.loadExtension(extensionData);
}

async function run(test) {
  let extension = createExtension(test.backgroundScript, test.permissions || ["telemetry"]);
  await extension.startup();
  await extension.awaitFinish(test.doneSignal);
  await extension.unload();
}

// Currently unsupported on Android: blocked on 1220177.
// See 1280234 c67 for discussion.
if (AppConstants.MOZ_BUILD_APP === "browser") {
  add_task(async function test_telemetry_without_telemetry_permission() {
    await run({
      backgroundScript: () => {
        browser.test.assertTrue(!browser.telemetry, "'telemetry' permission is required");
        browser.test.notifyPass("telemetry_permission");
      },
      permissions: [],
      doneSignal: "telemetry_permission",
    });
  });

  add_task(async function test_telemetry_scalar_add() {
    Services.telemetry.clearScalars();

    await run({
      backgroundScript: async () => {
        await browser.telemetry.scalarAdd("telemetry.test.unsigned_int_kind", 1);
        browser.test.notifyPass("scalar_add");
      },
      doneSignal: "scalar_add",
    });

    const scalars = Services.telemetry.getSnapshotForScalars("main", false);
    equal(scalars.parent["telemetry.test.unsigned_int_kind"], 1);

    Services.telemetry.clearScalars();
  });

  add_task(async function test_telemetry_scalar_add_unknown_name() {
    let {messages} = await promiseConsoleOutput(async () => {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.scalarAdd("telemetry.test.does_not_exist", 1);
          browser.test.notifyPass("scalar_add_unknown_name");
        },
        doneSignal: "scalar_add_unknown_name",
      });
    });

    messages = messages.filter(msg => /telemetry.test.does_not_exist - Unknown scalar./);
    equal(messages.length, 1, "Telemetry should throw if an unknown scalar is incremented");
  });

  add_task(async function test_telemetry_scalar_add_illegal_value() {
    await run({
      backgroundScript: () => {
        browser.test.assertThrows(
          () => browser.telemetry.scalarAdd("telemetry.test.unsigned_int_kind", {}),
          /Incorrect argument types for telemetry.scalarAdd/,
          "The second 'value' argument to scalarAdd must be an integer, string, or boolean"
        );
        browser.test.notifyPass("scalar_add_illegal_value");
      },
      doneSignal: "scalar_add_illegal_value",
    });
  });

  add_task(async function test_telemetry_scalar_add_invalid_keyed_scalar() {
    let {messages} = await promiseConsoleOutput(async function() {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.scalarAdd("telemetry.test.keyed_unsigned_int", 1);
          browser.test.notifyPass("scalar_add_invalid_keyed_scalar");
        },
        doneSignal: "scalar_add_invalid_keyed_scalar",
      });
    });

    messages = messages.filter(msg => /Attempting to manage a keyed scalar as a scalar/);
    equal(messages.length, 1, "Telemetry should throw if a keyed scalar is incremented");
  });

  add_task(async function test_telemetry_scalar_set() {
    Services.telemetry.clearScalars();

    await run({
      backgroundScript: async () => {
        await browser.telemetry.scalarSet("telemetry.test.boolean_kind", true);
        browser.test.notifyPass("scalar_set");
      },
      doneSignal: "scalar_set",
    });

    const scalars = Services.telemetry.getSnapshotForScalars("main", false);
    equal(scalars.parent["telemetry.test.boolean_kind"], true);

    Services.telemetry.clearScalars();
  });

  add_task(async function test_telemetry_scalar_set_unknown_name() {
    let {messages} = await promiseConsoleOutput(async function() {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.scalarSet("telemetry.test.does_not_exist", true);
          browser.test.notifyPass("scalar_set_unknown_name");
        },
        doneSignal: "scalar_set_unknown_name",
      });
    });

    messages = messages.filter(msg => /Unknown scalar/);
    equal(messages.length, 1, "Telemetry should throw if an unknown scalar is set");
  });

  add_task(async function test_telemetry_scalar_set_maximum() {
    Services.telemetry.clearScalars();

    await run({
      backgroundScript: async () => {
        await browser.telemetry.scalarSetMaximum("telemetry.test.unsigned_int_kind", 123);
        browser.test.notifyPass("scalar_set_maximum");
      },
      doneSignal: "scalar_set_maximum",
    });

    const scalars = Services.telemetry.getSnapshotForScalars("main", false);
    equal(scalars.parent["telemetry.test.unsigned_int_kind"], 123);

    Services.telemetry.clearScalars();
  });

  add_task(async function test_telemetry_scalar_set_maximum_unknown_name() {
    let {messages} = await promiseConsoleOutput(async function() {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.scalarSetMaximum("telemetry.test.does_not_exist", 1);
          browser.test.notifyPass("scalar_set_maximum_unknown_name");
        },
        doneSignal: "scalar_set_maximum_unknown_name",
      });
    });

    messages = messages.filter(msg => /Unknown scalar/);
    equal(messages.length, 1, "Telemetry should throw if an unknown scalar is set");
  });

  add_task(async function test_telemetry_scalar_set_maximum_illegal_value() {
    await run({
      backgroundScript: () => {
        browser.test.assertThrows(
          () => browser.telemetry.scalarSetMaximum("telemetry.test.unsigned_int_kind", "string"),
          /Incorrect argument types for telemetry.scalarSetMaximum/,
          "The second 'value' argument to scalarSetMaximum must be a scalar");
        browser.test.notifyPass("scalar_set_maximum_illegal_value");
      },
      doneSignal: "scalar_set_maximum_illegal_value",
    });
  });

  add_task(async function test_telemetry_record_event() {
    Services.telemetry.clearEvents();
    Services.telemetry.setEventRecordingEnabled("telemetry.test", true);

    await run({
      backgroundScript: async () => {
        await browser.telemetry.recordEvent("telemetry.test", "test1", "object1");
        browser.test.notifyPass("record_event_ok");
      },
      doneSignal: "record_event_ok",
    });

    let events = Services.telemetry.snapshotEvents(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true);
    equal(events.parent.length, 1);
    equal(events.parent[0][1], "telemetry.test");

    Services.telemetry.setEventRecordingEnabled("telemetry.test", false);
    Services.telemetry.clearEvents();
  });

  add_task(async function test_telemetry_register_scalars_string() {
    Services.telemetry.clearScalars();

    await run({
      backgroundScript: async () => {
        await browser.telemetry.registerScalars("telemetry.test.dynamic", {
          "webext_string": {
            kind: browser.telemetry.ScalarType.STRING,
            keyed: false,
            record_on_release: true,
          },
        });
        await browser.telemetry.scalarSet("telemetry.test.dynamic.webext_string", "hello");
        browser.test.notifyPass("register_scalars_string");
      },
      doneSignal: "register_scalars_string",
    });

    const scalars = Services.telemetry.getSnapshotForScalars("main", false);
    equal(scalars.dynamic["telemetry.test.dynamic.webext_string"], "hello");

    Services.telemetry.clearScalars();
  });

  add_task(async function test_telemetry_register_scalars_multiple() {
    Services.telemetry.clearScalars();

    await run({
      backgroundScript: async () => {
        await browser.telemetry.registerScalars("telemetry.test.dynamic", {
          "webext_string": {
            kind: browser.telemetry.ScalarType.STRING,
            keyed: false,
            record_on_release: true,
          },
          "webext_string_too": {
            kind: browser.telemetry.ScalarType.STRING,
            keyed: false,
            record_on_release: true,
          },
        });
        await browser.telemetry.scalarSet("telemetry.test.dynamic.webext_string", "hello");
        await browser.telemetry.scalarSet("telemetry.test.dynamic.webext_string_too", "world");
        browser.test.notifyPass("register_scalars_multiple");
      },
      doneSignal: "register_scalars_multiple",
    });

    const scalars = Services.telemetry.getSnapshotForScalars("main", false);
    equal(scalars.dynamic["telemetry.test.dynamic.webext_string"], "hello");
    equal(scalars.dynamic["telemetry.test.dynamic.webext_string_too"], "world");

    Services.telemetry.clearScalars();
  });

  add_task(async function test_telemetry_register_scalars_boolean() {
    Services.telemetry.clearScalars();

    await run({
      backgroundScript: async () => {
        await browser.telemetry.registerScalars("telemetry.test.dynamic", {
          "webext_boolean": {
            kind: browser.telemetry.ScalarType.BOOLEAN,
            keyed: false,
            record_on_release: true,
          },
        });
        await browser.telemetry.scalarSet("telemetry.test.dynamic.webext_boolean", true);
        browser.test.notifyPass("register_scalars_boolean");
      },
      doneSignal: "register_scalars_boolean",
    });

    const scalars = Services.telemetry.getSnapshotForScalars("main", false);
    equal(scalars.dynamic["telemetry.test.dynamic.webext_boolean"], true);

    Services.telemetry.clearScalars();
  });

  add_task(async function test_telemetry_register_scalars_count() {
    Services.telemetry.clearScalars();

    await run({
      backgroundScript: async () => {
        await browser.telemetry.registerScalars("telemetry.test.dynamic", {
          "webext_count": {
            kind: browser.telemetry.ScalarType.COUNT,
            keyed: false,
            record_on_release: true,
          },
        });
        await browser.telemetry.scalarSet("telemetry.test.dynamic.webext_count", 123);
        browser.test.notifyPass("register_scalars_count");
      },
      doneSignal: "register_scalars_count",
    });

    const scalars = Services.telemetry.getSnapshotForScalars("main", false);
    equal(scalars.dynamic["telemetry.test.dynamic.webext_count"], 123);

    Services.telemetry.clearScalars();
  });

  add_task(async function test_telemetry_register_events() {
    Services.telemetry.clearEvents();

    await run({
      backgroundScript: async () => {
        await browser.telemetry.registerEvents("telemetry.test.dynamic", {
          "test1": {
            methods: ["test1"],
            objects: ["object1"],
            extra_keys: [],
          },
        });
        await browser.telemetry.recordEvent("telemetry.test.dynamic", "test1", "object1");
        browser.test.notifyPass("register_events");
      },
      doneSignal: "register_events",
    });

    let events = Services.telemetry.snapshotEvents(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);
    let expected = [["telemetry.test.dynamic", "test1", "object1"]];
    equal(events.dynamic.length, expected.length);
    deepEqual(events.dynamic.map(e => e.slice(1)), expected);

    Services.telemetry.clearEvents();
  });

  add_task(async function test_telemetry_submit_ping() {
    await run({
      backgroundScript: async () => {
        await browser.telemetry.submitPing("webext-test", {}, {});
        browser.test.notifyPass("submit_ping");
      },
      doneSignal: "submit_ping",
    });

    let pings = await TelemetryArchive.promiseArchivedPingList();
    equal(pings.length, 1);
    equal(pings[0].type, "webext-test");
  });

  add_task(async function test_telemetry_can_upload_enabled() {
    Services.prefs.setBoolPref(TelemetryUtils.Preferences.FhrUploadEnabled, true);

    await run({
      backgroundScript: async () => {
        const result = await browser.telemetry.canUpload();
        browser.test.assertTrue(result);
        browser.test.notifyPass("can_upload_enabled");
      },
      doneSignal: "can_upload_enabled",
    });

    Services.prefs.clearUserPref(TelemetryUtils.Preferences.FhrUploadEnabled);
  });

  add_task(async function test_telemetry_can_upload_disabled() {
    Services.prefs.setBoolPref(TelemetryUtils.Preferences.FhrUploadEnabled, false);

    await run({
      backgroundScript: async () => {
        const result = await browser.telemetry.canUpload();
        browser.test.assertFalse(result);
        browser.test.notifyPass("can_upload_disabled");
      },
      doneSignal: "can_upload_disabled",
    });

    Services.prefs.clearUserPref(TelemetryUtils.Preferences.FhrUploadEnabled);
  });
}
