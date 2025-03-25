"use strict";

async function testExtensionWithBackground({
  background = {},
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
