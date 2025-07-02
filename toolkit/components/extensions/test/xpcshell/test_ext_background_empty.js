"use strict";

async function testExtensionWithBackground({
  background,
  expected_manifest_warnings,
}) {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: { background },
  });

  ExtensionTestUtils.failOnSchemaWarnings(false);
  await extension.startup();
  ExtensionTestUtils.failOnSchemaWarnings(true);

  await extension.unload();

  Assert.deepEqual(
    extension.extension.warnings,
    expected_manifest_warnings,
    "Expected manifest warnings"
  );
}

add_task(async function test_empty_background_scripts() {
  // extensions with an empty background.scripts property should load fine
  // see https://bugzilla.mozilla.org/show_bug.cgi?id=1954637

  await testExtensionWithBackground({
    background: {
      scripts: [],
    },
    expected_manifest_warnings: [
      "Reading manifest: Warning processing background: background.scripts is empty.",
    ],
  });
});

add_task(async function test_empty_background_object() {
  // extensions with an empty background property should load fine
  // see https://github.com/w3c/webextensions/issues/784

  const requireAtLeastOneOfWarning =
    WebExtensionPolicy.backgroundServiceWorkerEnabled
      ? 'Reading manifest: Warning processing background: background requires at least one of "service_worker", "scripts" or "page".'
      : 'Reading manifest: Warning processing background: background requires at least one of "scripts" or "page".';

  await testExtensionWithBackground({
    background: {},
    expected_manifest_warnings: [requireAtLeastOneOfWarning],
  });

  await testExtensionWithBackground({
    background: {
      unknown: true,
    },
    expected_manifest_warnings: [
      "Reading manifest: Warning processing background.unknown: An unexpected property was found in the WebExtension manifest.",
      requireAtLeastOneOfWarning,
    ],
  });

  await testExtensionWithBackground({
    background: null,
    expected_manifest_warnings: [],
  });
});
