"use strict";

function makeSandbox() {
  return Cu.Sandbox(
    Services.scriptSecurityManager.getSystemPrincipal(),
    {
      wantXrays: false,
      wantGlobalProperties: ["ChromeUtils"],
      sandboxName: `Sandbox type used for ext-*.js  ExtensionAPI subscripts`,
    }
  );
}

// This tests the ESMification transition for extension API scripts.
add_task(function test_import_from_sandbox_transition() {
  let sandbox = makeSandbox();

  Object.assign(sandbox, {
    injected3: ChromeUtils.importESModule("resource://test/esmified-3.sys.mjs"),
  });

  Services.scriptloader.loadSubScript("resource://test/api_script.js", sandbox);
  let tr = sandbox.testResults;

  Assert.equal(tr.injected3, 16, "Injected esmified-3.mjs has correct value.");
  Assert.equal(tr.module3, 16, "Iimported esmified-3.mjs has correct value.");
  Assert.ok(tr.sameInstance3, "Injected and imported are the same instance.");
  Assert.equal(tr.module4, 14, "Iimported esmified-4.mjs has correct value.");
});

// Same as above, just using a PrecompiledScript.
add_task(async function test_import_from_sandbox_transition() {
  let sandbox = makeSandbox();

  Object.assign(sandbox, {
    injected3: ChromeUtils.importESModule("resource://test/esmified-3.sys.mjs"),
  });

  let script = await ChromeUtils.compileScript("resource://test/api_script.js");
  script.executeInGlobal(sandbox);
  let tr = sandbox.testResults;

  Assert.equal(tr.injected3, 22, "Injected esmified-3.mjs has correct value.");
  Assert.equal(tr.module3, 22, "Iimported esmified-3.mjs has correct value.");
  Assert.ok(tr.sameInstance3, "Injected and imported are the same instance.");
  Assert.equal(tr.module4, 18, "Iimported esmified-4.mjs has correct value.");
});
