/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

async function testExtensionWithBackground({
  with_scripts = false,
  with_service_worker = false,
  with_page = false,
  with_preferred_environment,
  expected_background_type,
  expected_manifest_warnings = [],
}) {
  let background = {};
  if (with_scripts) {
    background.scripts = ["scripts.js"];
  }
  if (with_service_worker) {
    background.service_worker = "sw.js";
  }
  if (with_page) {
    background.page = "page.html";
  }
  if (with_preferred_environment) {
    background.preferred_environment = with_preferred_environment;
  }
  let extension = ExtensionTestUtils.loadExtension({
    manifest: { background },
    files: {
      "scripts.js": () => {
        browser.test.sendMessage("from_bg", "scripts");
      },
      "sw.js": () => {
        browser.test.sendMessage("from_bg", "service_worker");
      },
      "page.html": `<!DOCTYPE html><script src="page.js"></script>`,
      "page.js": () => {
        browser.test.sendMessage("from_bg", "page");
      },
    },
  });
  ExtensionTestUtils.failOnSchemaWarnings(false);
  await extension.startup();
  ExtensionTestUtils.failOnSchemaWarnings(true);

  info("Waiting for background to start");

  Assert.equal(
    await extension.awaitMessage("from_bg"),
    expected_background_type,
    "Expected background type"
  );

  Assert.deepEqual(
    extension.extension.warnings,
    expected_manifest_warnings,
    "Expected manifest warnings"
  );

  await extension.unload();
}

add_task(async function test_page_and_scripts() {
  await testExtensionWithBackground({
    with_page: true,
    with_scripts: true,
    // Should be expected_background_type: "scripts", not "page".
    // https://github.com/w3c/webextensions/issues/282#issuecomment-1443332913
    // ... but changing that may potentially affect backcompat of existing
    // Firefox add-ons.
    expected_background_type: "page",
    expected_manifest_warnings: [
      "Reading manifest: Warning processing background: Both background.page and background.scripts specified. background.scripts will be ignored.",
    ],
  });
});

add_task(
  { skip_if: () => !WebExtensionPolicy.backgroundServiceWorkerEnabled },
  async function test_service_worker_and_document() {
    await testExtensionWithBackground({
      with_scripts: true,
      with_service_worker: true,
      expected_background_type: "scripts",
      expected_manifest_warnings: [
        "Reading manifest: Warning processing background: with both background.service_worker and background.scripts, only background.scripts will be loaded. This can be changed with background.preferred_environment.",
      ],
    });
    await testExtensionWithBackground({
      with_page: true,
      with_service_worker: true,
      expected_background_type: "page",
      expected_manifest_warnings: [
        "Reading manifest: Warning processing background: with both background.service_worker and background.page, only background.page will be loaded. This can be changed with background.preferred_environment.",
      ],
    });
    await testExtensionWithBackground({
      with_page: true,
      with_scripts: true,
      with_service_worker: true,
      expected_background_type: "page",
      expected_manifest_warnings: [
        "Reading manifest: Warning processing background: with both background.service_worker and background.page, only background.page will be loaded. This can be changed with background.preferred_environment.",
      ],
    });
  }
);

add_task(async function test_preferred_environment() {
  await testExtensionWithBackground({
    with_scripts: true,
    with_service_worker: true,
    with_preferred_environment: ["document"],
    expected_background_type: "scripts",
  });
  await testExtensionWithBackground({
    with_scripts: true,
    with_preferred_environment: ["service_worker"],
    expected_background_type: "scripts",
  });
  await testExtensionWithBackground({
    with_page: true,
    with_service_worker: true,
    with_preferred_environment: ["document", "service_worker"],
    expected_background_type: "page",
  });
  await testExtensionWithBackground({
    with_page: true,
    with_service_worker: true,
    with_preferred_environment: ["service_worker"],
    expected_background_type: WebExtensionPolicy.backgroundServiceWorkerEnabled
      ? "service_worker"
      : "page",
    expected_manifest_warnings: [],
  });
  await testExtensionWithBackground({
    with_scripts: true,
    with_service_worker: true,
    with_preferred_environment: ["service_worker"],
    expected_background_type: WebExtensionPolicy.backgroundServiceWorkerEnabled
      ? "service_worker"
      : "scripts",
    expected_manifest_warnings: [],
  });
});

add_task(async function test_preferred_environment_unrecognized() {
  await testExtensionWithBackground({
    with_scripts: true,
    with_service_worker: true,
    with_preferred_environment: ["page", "scripts", "service_worker"],
    expected_background_type: WebExtensionPolicy.backgroundServiceWorkerEnabled
      ? "service_worker"
      : "scripts",
    expected_manifest_warnings: [
      'Reading manifest: Warning processing background.preferred_environment: Error processing background.preferred_environment.0: Invalid enumeration value "page"',
      'Reading manifest: Warning processing background.preferred_environment: Error processing background.preferred_environment.1: Invalid enumeration value "scripts"',
    ],
  });
  await testExtensionWithBackground({
    with_page: true,
    with_service_worker: true,
    with_preferred_environment: ["serviceWorker", "serviceworker", "worker"],
    expected_background_type: WebExtensionPolicy.backgroundServiceWorkerEnabled
      ? "service_worker"
      : "page",
    expected_manifest_warnings: [
      'Reading manifest: Warning processing background.preferred_environment: Error processing background.preferred_environment.0: Invalid enumeration value "serviceWorker"',
      'Reading manifest: Warning processing background.preferred_environment: Error processing background.preferred_environment.1: Invalid enumeration value "serviceworker"',
      'Reading manifest: Warning processing background.preferred_environment: Error processing background.preferred_environment.2: Invalid enumeration value "worker"',
    ],
  });
});

add_task(
  { skip_if: () => !WebExtensionPolicy.backgroundServiceWorkerEnabled },
  async function test_preferred_environment_when_sw_enabled() {
    await testExtensionWithBackground({
      with_service_worker: true,
      with_preferred_environment: ["document", "service_worker"],
      expected_background_type: "service_worker",
    });
    await testExtensionWithBackground({
      with_service_worker: true,
      with_preferred_environment: ["document"],
      expected_background_type: "service_worker",
    });
  }
);
