"use strict";

const server = createHttpServer({ hosts: ["example.com"] });

server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
  response.setHeader("Integrity-Policy", "blocked-destinations=(script)");
  response.write("<!DOCTYPE html><html></html>");
});

server.registerPathHandler("/script.js", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "application/javascript", false);
  response.write("() => {}");
});

add_setup(() => {
  Services.prefs.setBoolPref("security.integrity_policy.enabled", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("security.integrity_policy.enabled");
  });
});

add_task(async function test_contentscript_integrity_policy() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://example.com/dummy"],
          run_at: "document_idle",
          js: ["content_script.js"],
        },
      ],
    },
    files: {
      "content_script.js"() {
        const script = document.createElement("script");
        script.onload = () => browser.test.sendMessage("script-loaded", true);
        script.onerror = () => browser.test.sendMessage("script-loaded", false);

        script.src = "http://example.com/script.js";
        document.body.appendChild(script);
      },
    },
  });

  await extension.startup();

  const contentPage = await ExtensionTestUtils.loadContentPage(
    `http://example.com/dummy`
  );

  const pageLoadResult = await contentPage.spawn([], () => {
    const { promise, resolve } = Promise.withResolvers();
    const script = content.document.createElement("script");
    script.onload = () => resolve(true);
    script.onerror = () => resolve(false);
    script.src = "http://example.com/script.js";
    content.document.body.appendChild(script);
    return promise;
  });
  equal(
    pageLoadResult,
    false,
    "Verify that script loading is blocked by integrity policy for the page itself"
  );

  const extensionLoadResult = await extension.awaitMessage("script-loaded");
  equal(
    extensionLoadResult,
    true,
    "Content script should load the script despite integrity policy blocking scripts"
  );

  await extension.unload();
  await contentPage.close();
});
