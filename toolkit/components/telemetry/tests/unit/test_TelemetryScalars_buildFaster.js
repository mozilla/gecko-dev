/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

/**
 * Return the path to the definitions file for the scalars.
 */
function getDefinitionsPath() {
  // Write the scalar definition to the spec file in the binary directory.
  let definitionFile = Cc["@mozilla.org/file/local;1"].createInstance(
    Ci.nsIFile
  );
  definitionFile = Services.dirsvc.get("GreD", Ci.nsIFile);
  definitionFile.append("ScalarArtifactDefinitions.json");
  return definitionFile.path;
}

add_task(async function test_setup() {
  do_get_profile();
});

add_task(
  {
    // The test needs to write a file, and that fails in tests on Android.
    // We don't really need the Android coverage, so skip on Android.
    skip_if: () => AppConstants.platform == "android",
  },
  async function test_invalidJSON() {
    const INVALID_JSON = "{ invalid,JSON { {1}";
    const FILE_PATH = getDefinitionsPath();

    // Write a corrupted JSON file.
    await IOUtils.writeUTF8(FILE_PATH, INVALID_JSON, {
      mode: "overwrite",
    });

    // Simulate Firefox startup. This should not throw!
    await TelemetryController.testSetup();
    await TelemetryController.testPromiseJsProbeRegistration();

    // Cleanup.
    await TelemetryController.testShutdown();
    await IOUtils.remove(FILE_PATH);
  }
);

add_task(
  {
    // The test needs to write a file, and that fails in tests on Android.
    // We don't really need the Android coverage, so skip on Android.
    skip_if: () => AppConstants.platform == "android",
  },
  async function test_dynamicBuiltin() {
    const DYNAMIC_SCALAR_SPEC = {
      "telemetry.test": {
        builtin_dynamic: {
          kind: "nsITelemetry::SCALAR_TYPE_COUNT",
          expires: "never",
          record_on_release: false,
          keyed: false,
        },
        builtin_dynamic_other: {
          kind: "nsITelemetry::SCALAR_TYPE_BOOLEAN",
          expires: "never",
          record_on_release: false,
          keyed: false,
        },
        builtin_dynamic_expired: {
          kind: "nsITelemetry::SCALAR_TYPE_BOOLEAN",
          expires: AppConstants.MOZ_APP_VERSION,
          record_on_release: false,
          keyed: false,
        },
        builtin_dynamic_multi: {
          kind: "nsITelemetry::SCALAR_TYPE_COUNT",
          expired: false,
          record_on_release: false,
          keyed: false,
          stores: ["main", "sync"],
        },
        builtin_dynamic_sync_only: {
          kind: "nsITelemetry::SCALAR_TYPE_COUNT",
          expired: false,
          record_on_release: false,
          keyed: false,
          stores: ["sync"],
        },
      },
    };

    Telemetry.clearScalars();

    // Let's write to the definition file to also cover the file
    // loading part.
    const FILE_PATH = getDefinitionsPath();
    await IOUtils.writeJSON(FILE_PATH, DYNAMIC_SCALAR_SPEC);

    // Start TelemetryController to trigger loading the specs.
    await TelemetryController.testReset();
    await TelemetryController.testPromiseJsProbeRegistration();

    // Clean up.
    await TelemetryController.testShutdown();
    await IOUtils.remove(FILE_PATH);
  }
);

add_task(async function test_keyedDynamicBuiltin() {
  Telemetry.clearScalars();

  // Register the built-in scalars (let's not take the I/O hit).
  Telemetry.registerBuiltinScalars("telemetry.test", {
    builtin_dynamic_keyed: {
      kind: Ci.nsITelemetry.SCALAR_TYPE_COUNT,
      expired: false,
      record_on_release: false,
      keyed: true,
    },
  });
});
