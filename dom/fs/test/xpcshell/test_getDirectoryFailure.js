/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This test doesn't use the shared module system (running the same test in
// multiple test suites) on purpose because it needs to create an unprivileged
// sandbox which is not possible if the test is already running in a sandbox.

const { FileUtils } = ChromeUtils.importESModule(
  "resource://testing-common/dom/quota/test/modules/FileUtils.sys.mjs"
);

const { PrincipalUtils } = ChromeUtils.importESModule(
  "resource://testing-common/dom/quota/test/modules/PrincipalUtils.sys.mjs"
);

add_task(async function init() {
  const principal = PrincipalUtils.createPrincipal("https://example.com");
  const clientDirectoryPath = "storage/default/https+++example.com/fs";

  info("Creating invalid client directory");

  const clientDirectory = FileUtils.getFile(clientDirectoryPath);
  clientDirectory.create(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("0644", 8));

  info("Starting database opening");

  const openPromise = new Promise(function (resolve, reject) {
    const sandbox = new Cu.Sandbox(principal, {
      wantGlobalProperties: ["storage"],
      forceSecureContext: true,
    });
    sandbox.resolve = resolve;
    sandbox.reject = reject;
    Cu.evalInSandbox(`storage.getDirectory().then(resolve, reject);`, sandbox);
  });

  info("Waiting for database to finish opening");

  try {
    await openPromise;
    ok(false, "Should have thrown");
  } catch (e) {
    ok(true, "Should have thrown");
    Assert.strictEqual(e.name, "UnknownError", "Threw right result code");
  }
});
