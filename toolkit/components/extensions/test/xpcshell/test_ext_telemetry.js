"use strict";

const { TelemetryUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryUtils.sys.mjs"
);
const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

const { TelemetryArchiveTesting } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryArchiveTesting.sys.mjs"
);

const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

// All tests run privileged unless otherwise specified not to.
function createExtension(backgroundScript, permissions, isPrivileged = true) {
  let extensionData = {
    background: backgroundScript,
    manifest: { permissions },
    isPrivileged,
  };

  return ExtensionTestUtils.loadExtension(extensionData);
}

async function testNoOp(methodNames, run) {
  if (!Array.isArray(methodNames)) {
    methodNames = [methodNames];
  }
  ExtensionTestUtils.failOnSchemaWarnings(false);

  const { messages } = await promiseConsoleOutput(run);
  AddonTestUtils.checkMessages(messages, {
    expected: methodNames.map(methodName => ({
      message: new RegExp(
        "`" +
          methodName +
          "` is a no-op since Firefox 134 \\(see bug 1930196\\)"
      ),
    })),
    forbidUnexpected: true,
  });

  ExtensionTestUtils.failOnSchemaWarnings(true);
}

async function run(test) {
  let extension = createExtension(
    test.backgroundScript,
    test.permissions || ["telemetry"],
    test.isPrivileged
  );
  await extension.startup();
  await extension.awaitFinish(test.doneSignal);
  await extension.unload();
}

// Currently unsupported on Android: blocked on 1220177.
// See 1280234 c67 for discussion.
if (AppConstants.MOZ_BUILD_APP === "browser") {
  AddonTestUtils.init(this);

  add_task(async function test_telemetry_without_telemetry_permission() {
    await run({
      backgroundScript: () => {
        browser.test.assertTrue(
          !browser.telemetry,
          "'telemetry' permission is required"
        );
        browser.test.notifyPass("telemetry_permission");
      },
      permissions: [],
      doneSignal: "telemetry_permission",
      isPrivileged: false,
    });
  });

  add_task(
    async function test_telemetry_without_telemetry_permission_privileged() {
      await run({
        backgroundScript: () => {
          browser.test.assertTrue(
            !browser.telemetry,
            "'telemetry' permission is required"
          );
          browser.test.notifyPass("telemetry_permission");
        },
        permissions: [],
        doneSignal: "telemetry_permission",
      });
    }
  );

  add_task(async function test_telemetry_scalar_add() {
    Services.telemetry.clearScalars();

    await testNoOp("scalarAdd", async () => {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.scalarAdd(
            "telemetry.test.unsigned_int_kind",
            1
          );
          browser.test.notifyPass("scalar_add");
        },
        doneSignal: "scalar_add",
      });

      TelemetryTestUtils.assertScalarUnset(
        TelemetryTestUtils.getProcessScalars("parent", false, true),
        "telemetry.test.unsigned_int_kind"
      );
    });
  });

  add_task(async function test_telemetry_scalar_add_unknown_name() {
    ExtensionTestUtils.failOnSchemaWarnings(false);

    await testNoOp("scalarAdd", async () => {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.scalarAdd("telemetry.test.does_not_exist", 1);
          browser.test.notifyPass("scalar_add_unknown_name");
        },
        doneSignal: "scalar_add_unknown_name",
      });
    });
  });

  add_task(async function test_telemetry_scalar_add_illegal_value() {
    ExtensionTestUtils.failOnSchemaWarnings(false);

    await testNoOp("scalarAdd", async () => {
      await run({
        backgroundScript: () => {
          browser.test.assertThrows(
            () =>
              browser.telemetry.scalarAdd(
                "telemetry.test.unsigned_int_kind",
                {}
              ),
            /Incorrect argument types for telemetry.scalarAdd/,
            "The second 'value' argument to scalarAdd must be an integer, string, or boolean"
          );
          browser.test.notifyPass("scalar_add_illegal_value");
        },
        doneSignal: "scalar_add_illegal_value",
      });
    });
  });

  add_task(async function test_telemetry_scalar_add_invalid_keyed_scalar() {
    ExtensionTestUtils.failOnSchemaWarnings(false);

    await testNoOp("scalarAdd", async () => {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.scalarAdd(
            "telemetry.test.keyed_unsigned_int",
            1
          );
          browser.test.notifyPass("scalar_add_invalid_keyed_scalar");
        },
        doneSignal: "scalar_add_invalid_keyed_scalar",
      });
    });
  });

  add_task(async function test_telemetry_scalar_set_bool_true() {
    Services.telemetry.clearScalars();
    await testNoOp("scalarSet", async () => {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.scalarSet(
            "telemetry.test.boolean_kind",
            true
          );
          browser.test.notifyPass("scalar_set_bool_true");
        },
        doneSignal: "scalar_set_bool_true",
      });
      TelemetryTestUtils.assertScalarUnset(
        TelemetryTestUtils.getProcessScalars("parent", false, true),
        "telemetry.test.boolean_kind"
      );
    });
  });

  add_task(async function test_telemetry_scalar_set_bool_false() {
    Services.telemetry.clearScalars();
    await testNoOp("scalarSet", async () => {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.scalarSet(
            "telemetry.test.boolean_kind",
            false
          );
          browser.test.notifyPass("scalar_set_bool_false");
        },
        doneSignal: "scalar_set_bool_false",
      });
      TelemetryTestUtils.assertScalarUnset(
        TelemetryTestUtils.getProcessScalars("parent", false, true),
        "telemetry.test.boolean_kind"
      );
    });
  });

  add_task(async function test_telemetry_scalar_unset_bool() {
    Services.telemetry.clearScalars();
    TelemetryTestUtils.assertScalarUnset(
      TelemetryTestUtils.getProcessScalars("parent", false, true),
      "telemetry.test.boolean_kind"
    );
  });

  add_task(async function test_telemetry_scalar_set_unknown_name() {
    await testNoOp("scalarSet", async () => {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.scalarSet(
            "telemetry.test.does_not_exist",
            true
          );
          browser.test.notifyPass("scalar_set_unknown_name");
        },
        doneSignal: "scalar_set_unknown_name",
      });
    });
  });

  add_task(async function test_telemetry_scalar_set_zero() {
    Services.telemetry.clearScalars();
    await testNoOp("scalarSet", async () => {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.scalarSet(
            "telemetry.test.unsigned_int_kind",
            0
          );
          browser.test.notifyPass("scalar_set_zero");
        },
        doneSignal: "scalar_set_zero",
      });
      TelemetryTestUtils.assertScalarUnset(
        TelemetryTestUtils.getProcessScalars("parent", false, true),
        "telemetry.test.unsigned_int_kind"
      );
    });
  });

  add_task(async function test_telemetry_scalar_set_maximum() {
    Services.telemetry.clearScalars();
    await testNoOp("scalarSetMaximum", async () => {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.scalarSetMaximum(
            "telemetry.test.unsigned_int_kind",
            123
          );
          browser.test.notifyPass("scalar_set_maximum");
        },
        doneSignal: "scalar_set_maximum",
      });
      TelemetryTestUtils.assertScalarUnset(
        TelemetryTestUtils.getProcessScalars("parent", false, true),
        "telemetry.test.unsigned_int_kind"
      );
    });
  });

  add_task(async function test_telemetry_scalar_set_maximum_unknown_name() {
    await testNoOp("scalarSetMaximum", async () => {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.scalarSetMaximum(
            "telemetry.test.does_not_exist",
            1
          );
          browser.test.notifyPass("scalar_set_maximum_unknown_name");
        },
        doneSignal: "scalar_set_maximum_unknown_name",
      });
    });
  });

  add_task(async function test_telemetry_scalar_set_maximum_illegal_value() {
    await testNoOp("scalarSetMaximum", async () => {
      await run({
        backgroundScript: () => {
          browser.test.assertThrows(
            () =>
              browser.telemetry.scalarSetMaximum(
                "telemetry.test.unsigned_int_kind",
                "string"
              ),
            /Incorrect argument types for telemetry.scalarSetMaximum/,
            "The second 'value' argument to scalarSetMaximum must be a scalar"
          );
          browser.test.notifyPass("scalar_set_maximum_illegal_value");
        },
        doneSignal: "scalar_set_maximum_illegal_value",
      });
    });
  });

  add_task(async function test_telemetry_keyed_scalar_add() {
    Services.telemetry.clearScalars();
    await testNoOp("keyedScalarAdd", async () => {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.keyedScalarAdd(
            "telemetry.test.keyed_unsigned_int",
            "foo",
            1
          );
          browser.test.notifyPass("keyed_scalar_add");
        },
        doneSignal: "keyed_scalar_add",
      });
      TelemetryTestUtils.assertScalarUnset(
        TelemetryTestUtils.getProcessScalars("parent", true, true),
        "telemetry.test.keyed_unsigned_int"
      );
    });
  });

  add_task(async function test_telemetry_keyed_scalar_add_unknown_name() {
    await testNoOp("keyedScalarAdd", async () => {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.keyedScalarAdd(
            "telemetry.test.does_not_exist",
            "foo",
            1
          );
          browser.test.notifyPass("keyed_scalar_add_unknown_name");
        },
        doneSignal: "keyed_scalar_add_unknown_name",
      });
    });
  });

  add_task(async function test_telemetry_keyed_scalar_add_illegal_value() {
    await testNoOp("keyedScalarAdd", async () => {
      await run({
        backgroundScript: () => {
          browser.test.assertThrows(
            () =>
              browser.telemetry.keyedScalarAdd(
                "telemetry.test.keyed_unsigned_int",
                "foo",
                {}
              ),
            /Incorrect argument types for telemetry.keyedScalarAdd/,
            "The second 'value' argument to keyedScalarAdd must be an integer, string, or boolean"
          );
          browser.test.notifyPass("keyed_scalar_add_illegal_value");
        },
        doneSignal: "keyed_scalar_add_illegal_value",
      });
    });
  });

  add_task(async function test_telemetry_keyed_scalar_add_invalid_scalar() {
    await testNoOp("keyedScalarAdd", async () => {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.keyedScalarAdd(
            "telemetry.test.unsigned_int_kind",
            "foo",
            1
          );
          browser.test.notifyPass("keyed_scalar_add_invalid_scalar");
        },
        doneSignal: "keyed_scalar_add_invalid_scalar",
      });
    });
  });

  add_task(async function test_telemetry_keyed_scalar_add_long_key() {
    await testNoOp("keyedScalarAdd", async () => {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.keyedScalarAdd(
            "telemetry.test.keyed_unsigned_int",
            "X".repeat(73),
            1
          );
          browser.test.notifyPass("keyed_scalar_add_long_key");
        },
        doneSignal: "keyed_scalar_add_long_key",
      });
    });
  });

  add_task(async function test_telemetry_keyed_scalar_set() {
    Services.telemetry.clearScalars();
    await testNoOp("keyedScalarSet", async () => {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.keyedScalarSet(
            "telemetry.test.keyed_boolean_kind",
            "foo",
            true
          );
          browser.test.notifyPass("keyed_scalar_set");
        },
        doneSignal: "keyed_scalar_set",
      });
      TelemetryTestUtils.assertScalarUnset(
        TelemetryTestUtils.getProcessScalars("parent", true, true),
        "telemetry.test.keyed_boolean_kind"
      );
    });
  });

  add_task(async function test_telemetry_keyed_scalar_set_unknown_name() {
    await testNoOp("keyedScalarSet", async () => {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.keyedScalarSet(
            "telemetry.test.does_not_exist",
            "foo",
            true
          );
          browser.test.notifyPass("keyed_scalar_set_unknown_name");
        },
        doneSignal: "keyed_scalar_set_unknown_name",
      });
    });
  });

  add_task(async function test_telemetry_keyed_scalar_set_long_key() {
    await testNoOp("keyedScalarSet", async () => {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.keyedScalarSet(
            "telemetry.test.keyed_unsigned_int",
            "X".repeat(73),
            1
          );
          browser.test.notifyPass("keyed_scalar_set_long_key");
        },
        doneSignal: "keyed_scalar_set_long_key",
      });
    });
  });

  add_task(async function test_telemetry_keyed_scalar_set_maximum() {
    Services.telemetry.clearScalars();
    await testNoOp("keyedScalarSetMaximum", async () => {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.keyedScalarSetMaximum(
            "telemetry.test.keyed_unsigned_int",
            "foo",
            123
          );
          browser.test.notifyPass("keyed_scalar_set_maximum");
        },
        doneSignal: "keyed_scalar_set_maximum",
      });
      TelemetryTestUtils.assertScalarUnset(
        TelemetryTestUtils.getProcessScalars("parent", true, true),
        "telemetry.test.keyed_unsigned_int"
      );
    });
  });

  add_task(
    async function test_telemetry_keyed_scalar_set_maximum_unknown_name() {
      await testNoOp("keyedScalarSetMaximum", async () => {
        await run({
          backgroundScript: async () => {
            await browser.telemetry.keyedScalarSetMaximum(
              "telemetry.test.does_not_exist",
              "foo",
              1
            );
            browser.test.notifyPass("keyed_scalar_set_maximum_unknown_name");
          },
          doneSignal: "keyed_scalar_set_maximum_unknown_name",
        });
      });
    }
  );

  add_task(
    async function test_telemetry_keyed_scalar_set_maximum_illegal_value() {
      await testNoOp("keyedScalarSetMaximum", async () => {
        await run({
          backgroundScript: () => {
            browser.test.assertThrows(
              () =>
                browser.telemetry.keyedScalarSetMaximum(
                  "telemetry.test.keyed_unsigned_int",
                  "foo",
                  "string"
                ),
              /Incorrect argument types for telemetry.keyedScalarSetMaximum/,
              "The third 'value' argument to keyedScalarSetMaximum must be a scalar"
            );
            browser.test.notifyPass("keyed_scalar_set_maximum_illegal_value");
          },
          doneSignal: "keyed_scalar_set_maximum_illegal_value",
        });
      });
    }
  );

  add_task(async function test_telemetry_keyed_scalar_set_maximum_long_key() {
    await testNoOp("keyedScalarSetMaximum", async () => {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.keyedScalarSetMaximum(
            "telemetry.test.keyed_unsigned_int",
            "X".repeat(73),
            1
          );
          browser.test.notifyPass("keyed_scalar_set_maximum_long_key");
        },
        doneSignal: "keyed_scalar_set_maximum_long_key",
      });
    });
  });

  add_task(async function test_telemetry_record_event() {
    Services.telemetry.clearEvents();

    ExtensionTestUtils.failOnSchemaWarnings(false);

    await run({
      backgroundScript: async () => {
        await browser.telemetry.recordEvent(
          "telemetry.test",
          "test1",
          "object1"
        );
        browser.test.notifyPass("record_event_ok");
      },
      doneSignal: "record_event_ok",
    });

    const snapshot = Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
      true
    );
    if ("parent" in snapshot) {
      Assert.ok(
        snapshot.parent.every(
          ([, /*timestamp*/ category, method, object /* value, extra */]) =>
            category != "telemetry.test" &&
            method != "test1" &&
            object != "object1"
        )
      );
    }

    ExtensionTestUtils.failOnSchemaWarnings(true);

    Services.telemetry.clearEvents();
  });

  // Bug 1536877
  add_task(async function test_telemetry_record_event_value_must_be_string() {
    Services.telemetry.clearEvents();

    ExtensionTestUtils.failOnSchemaWarnings(false);

    await run({
      backgroundScript: async () => {
        try {
          await browser.telemetry.recordEvent(
            "telemetry.test",
            "test1",
            "object1",
            "value1"
          );
          browser.test.notifyPass("record_event_string_value");
        } catch (ex) {
          browser.test.fail(
            `Unexpected exception raised during record_event_value_must_be_string: ${ex}`
          );
          browser.test.notifyPass("record_event_string_value");
          throw ex;
        }
      },
      doneSignal: "record_event_string_value",
    });

    const snapshot = Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
      true
    );
    const testEvents =
      snapshot.parent?.filter(
        ([, category, method, object]) =>
          category == "telemetry.test" &&
          method == "test1" &&
          object == "object1"
      ) ?? [];
    Assert.equal(
      testEvents.length,
      0,
      "Deprecated telemetry.recordEvent should be no-op."
    );

    ExtensionTestUtils.failOnSchemaWarnings(true);

    Services.telemetry.clearEvents();
  });

  add_task(async function test_telemetry_register_scalars_string() {
    Services.telemetry.clearScalars();
    await testNoOp(["registerScalars", "scalarSet"], async () => {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.registerScalars("telemetry.test.dynamic", {
            webext_string: {
              kind: browser.telemetry.ScalarType.STRING,
              keyed: false,
              record_on_release: true,
            },
          });
          await browser.telemetry.scalarSet(
            "telemetry.test.dynamic.webext_string",
            "hello"
          );
          browser.test.notifyPass("register_scalars_string");
        },
        doneSignal: "register_scalars_string",
      });
      TelemetryTestUtils.assertScalarUnset(
        TelemetryTestUtils.getProcessScalars("dynamic", false, true),
        "telemetry.test.dynamic.webext_string"
      );
    });
  });

  add_task(async function test_telemetry_register_scalars_multiple() {
    Services.telemetry.clearScalars();
    await testNoOp(["registerScalars", "scalarSet", "scalarSet"], async () => {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.registerScalars("telemetry.test.dynamic", {
            webext_string: {
              kind: browser.telemetry.ScalarType.STRING,
              keyed: false,
              record_on_release: true,
            },
            webext_string_too: {
              kind: browser.telemetry.ScalarType.STRING,
              keyed: false,
              record_on_release: true,
            },
          });
          await browser.telemetry.scalarSet(
            "telemetry.test.dynamic.webext_string",
            "hello"
          );
          await browser.telemetry.scalarSet(
            "telemetry.test.dynamic.webext_string_too",
            "world"
          );
          browser.test.notifyPass("register_scalars_multiple");
        },
        doneSignal: "register_scalars_multiple",
      });
      const scalars = TelemetryTestUtils.getProcessScalars(
        "dynamic",
        false,
        true
      );
      TelemetryTestUtils.assertScalarUnset(
        scalars,
        "telemetry.test.dynamic.webext_string"
      );
      TelemetryTestUtils.assertScalarUnset(
        scalars,
        "telemetry.test.dynamic.webext_string_too"
      );
    });
  });

  add_task(async function test_telemetry_register_scalars_boolean() {
    Services.telemetry.clearScalars();
    await testNoOp(["registerScalars", "scalarSet"], async () => {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.registerScalars("telemetry.test.dynamic", {
            webext_boolean: {
              kind: browser.telemetry.ScalarType.BOOLEAN,
              keyed: false,
              record_on_release: true,
            },
          });
          await browser.telemetry.scalarSet(
            "telemetry.test.dynamic.webext_boolean",
            true
          );
          browser.test.notifyPass("register_scalars_boolean");
        },
        doneSignal: "register_scalars_boolean",
      });
      TelemetryTestUtils.assertScalarUnset(
        TelemetryTestUtils.getProcessScalars("dynamic", false, true),
        "telemetry.test.dynamic.webext_boolean"
      );
    });
  });

  add_task(async function test_telemetry_register_scalars_count() {
    Services.telemetry.clearScalars();
    await testNoOp(["registerScalars", "scalarSet"], async () => {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.registerScalars("telemetry.test.dynamic", {
            webext_count: {
              kind: browser.telemetry.ScalarType.COUNT,
              keyed: false,
              record_on_release: true,
            },
          });
          await browser.telemetry.scalarSet(
            "telemetry.test.dynamic.webext_count",
            123
          );
          browser.test.notifyPass("register_scalars_count");
        },
        doneSignal: "register_scalars_count",
      });
      TelemetryTestUtils.assertScalarUnset(
        TelemetryTestUtils.getProcessScalars("dynamic", false, true),
        "telemetry.test.dynamic.webext_count"
      );
    });
  });

  add_task(async function test_telemetry_register_events() {
    Services.telemetry.clearEvents();

    ExtensionTestUtils.failOnSchemaWarnings(false);

    const { messages } = await promiseConsoleOutput(async () => {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.registerEvents("telemetry.test.dynamic", {
            test1: {
              methods: ["test1"],
              objects: ["object1"],
              extra_keys: [],
            },
          });
          await browser.telemetry.recordEvent(
            "telemetry.test.dynamic",
            "test1",
            "object1"
          );
          browser.test.notifyPass("register_events");
        },
        doneSignal: "register_events",
      });
    });

    const expectedRegisterEventsMessage =
      /`registerEvents` is a no-op since Firefox 132 \(see bug 1894533\)/;
    const expectedRecordEventMessage =
      /`recordEvent` is a no-op since Firefox 132 \(see bug 1894533\)/;

    AddonTestUtils.checkMessages(messages, {
      expected: [
        { message: expectedRegisterEventsMessage },
        { message: expectedRecordEventMessage },
      ],
      forbidUnexpected: true,
    });

    ExtensionTestUtils.failOnSchemaWarnings(true);
  });

  add_task(async function test_telemetry_submit_ping() {
    let archiveTester = new TelemetryArchiveTesting.Checker();
    await archiveTester.promiseInit();

    await run({
      backgroundScript: async () => {
        await browser.telemetry.submitPing("webext-test", {}, {});
        browser.test.notifyPass("submit_ping");
      },
      doneSignal: "submit_ping",
    });

    await TestUtils.waitForCondition(
      () => archiveTester.promiseFindPing("webext-test", []),
      "Failed to find the webext-test ping"
    );
  });

  add_task(async function test_telemetry_can_upload_enabled() {
    Services.prefs.setBoolPref(
      TelemetryUtils.Preferences.FhrUploadEnabled,
      true
    );

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
    Services.prefs.setBoolPref(
      TelemetryUtils.Preferences.FhrUploadEnabled,
      false
    );

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
