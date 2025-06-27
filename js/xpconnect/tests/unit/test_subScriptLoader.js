"use strict";

add_task(async function test_executeScriptAfterNuked() {
  let scriptUrl = Services.io.newFileURI(do_get_file("file_simple_script.js")).spec;

  // Load the script for the first time into a sandbox, and then nuke
  // that sandbox.
  let sandbox = Cu.Sandbox(Services.scriptSecurityManager.getSystemPrincipal());
  Services.scriptloader.loadSubScript(scriptUrl, sandbox);
  Cu.nukeSandbox(sandbox);

  // Load the script again into a new sandbox, and make sure it
  // succeeds.
  sandbox = Cu.Sandbox(Services.scriptSecurityManager.getSystemPrincipal());
  Services.scriptloader.loadSubScript(scriptUrl, sandbox);
});


add_task(function test_disallowed_scheme() {
  const URLs = [
    "data:text/javascript,1",
    "blob:https://example.org/aa99b0a0-25cd-44b2-840d-641c5c55f0fd",
  ]

  for (let url of URLs) {
    Assert.throws(() => Services.scriptloader.loadSubScript(url, {}), /Trying to load a non-local URI/);
  }
})
