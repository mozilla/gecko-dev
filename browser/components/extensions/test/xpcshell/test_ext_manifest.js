/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);

async function testManifest(manifest, expectedError) {
  ExtensionTestUtils.failOnSchemaWarnings(false);
  let normalized = await ExtensionTestUtils.normalizeManifest(manifest);
  ExtensionTestUtils.failOnSchemaWarnings(true);

  if (expectedError) {
    ok(
      expectedError.test(normalized.error),
      `Should have an error for ${JSON.stringify(manifest)}, got ${
        normalized.error
      }`
    );
  } else {
    ok(
      !normalized.error,
      `Should not have an error ${JSON.stringify(manifest)}, ${
        normalized.error
      }`
    );
  }
  return normalized.errors;
}

const all_actions = [
  "action",
  "browser_action",
  "page_action",
  "sidebar_action",
];

add_task(async function test_manifest() {
  let badpaths = ["", " ", "\t", "http://foo.com/icon.png"];
  for (let path of badpaths) {
    for (let action of all_actions) {
      let manifest_version = action == "action" ? 3 : 2;
      let manifest = { manifest_version };
      manifest[action] = { default_icon: path };
      let error = new RegExp(`Error processing ${action}.default_icon`);
      await testManifest(manifest, error);

      manifest[action] = { default_icon: { 16: path } };
      await testManifest(manifest, error);
    }
  }

  let paths = [
    "icon.png",
    "/icon.png",
    "./icon.png",
    "path to an icon.png",
    " icon.png",
  ];
  for (let path of paths) {
    for (let action of all_actions) {
      let manifest_version = action == "action" ? 3 : 2;
      let manifest = { manifest_version };
      manifest[action] = { default_icon: path };
      if (action == "sidebar_action") {
        // Sidebar requires panel.
        manifest[action].default_panel = "foo.html";
      }
      await testManifest(manifest);

      manifest[action] = { default_icon: { 16: path } };
      if (action == "sidebar_action") {
        manifest[action].default_panel = "foo.html";
      }
      await testManifest(manifest);
    }
  }
});

add_task(async function test_action_version() {
  let warnings = await testManifest({
    manifest_version: 3,
    browser_action: {
      default_panel: "foo.html",
    },
  });
  Assert.deepEqual(
    warnings,
    [`Property "browser_action" is unsupported in Manifest Version 3`],
    `Manifest v3 with "browser_action" key logs an error.`
  );

  warnings = await testManifest({
    manifest_version: 2,
    action: {
      default_icon: "",
      default_panel: "foo.html",
    },
  });

  Assert.deepEqual(
    warnings,
    [`Property "action" is unsupported in Manifest Version 2`],
    `Manifest v2 with "action" key first warning is clear.`
  );
});

// Verify that extended F13-F19 keys are invalid when specified in the
// extensions manifest.
//
// Other related tests:
// - browser_manage_shortcuts.js include a test that makes sure the F13-F19 keys are valid when
//   assigned by a user through about:addons "Manage Shortcuts".
// - browser_ext_commands_update.js include a test that makes sure the F13-F19 keys
//   are valid when dynamically assigned by the extension through
//   browser.commands.update API calls.
add_task(async function test_invalid_extended_function_keys() {
  info("Verify F13-19 are invalid when assigned from the extension manifest");
  ExtensionTestUtils.failOnSchemaWarnings(false);
  let normalized = await ExtensionTestUtils.normalizeManifest({
    commands: {
      invalidFnKeyCommand: {
        suggested_key: {
          default: "Alt+F13",
        },
      },
    },
  });
  const EXPECTED_ERROR_REGEX = new RegExp(
    `Value ".*" must not include extended F13-F19 keys. F13-F19 keys can only be used for user-defined keyboard shortcuts`
  );
  ExtensionTestUtils.failOnSchemaWarnings(true);
  ok(
    EXPECTED_ERROR_REGEX.test(normalized.error),
    `Should have reported F13 as an invalid key manifest error, got: ${normalized.error}`
  );
});
