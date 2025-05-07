"use strict";

/* globals exportFunction */
/* eslint-disable mozilla/balanced-listeners */

const server = createHttpServer({ hosts: ["example.com", "example.org"] });

server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
  response.write("<!DOCTYPE html><html></html>");
});

server.registerPathHandler("/bfcachetestpage", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html;charset=utf-8", false);
  response.write(`<!DOCTYPE html>
<script>
  window.addEventListener("pageshow", (event) => {
    event.stopImmediatePropagation();
    dump("pageshow fired with persisted=" + event.persisted + "\\n");
    if (window.browserTestSendMessage) {
      browserTestSendMessage("content-script-show");
    }
  });
  window.addEventListener("pagehide", (event) => {
    event.stopImmediatePropagation();
    if (window.browserTestSendMessage) {
      dump("pagehide fired with persisted=" + event.persisted + "\\n");
      if (event.persisted) {
        browserTestSendMessage("content-script-hide");
      } else {
        browserTestSendMessage("content-script-unload");
      }
    }
  }, true);
</script>`);
});

add_setup(() => {
  // A test below wants to prevent a page from entering the bfcache by adding
  // an "unload" listener.
  Services.prefs.setBoolPref(
    "docshell.shistory.bfcache.allow_unload_listeners",
    false
  );

  // When the bfcache is force-disabled, the process would go away upon
  // navigating elsewhere (example.com -> example.org). Keep the process alive
  // for a bit longer to enable us to verify state within the process.
  Services.prefs.setIntPref("dom.ipc.processReuse.unusedGraceMs", 30000);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("dom.ipc.processReuse.unusedGraceMs");
    Services.ppmm.releaseCachedProcesses();
  });
});

add_task(async function test_contentscript_context_isolation() {
  function contentScript() {
    browser.test.sendMessage("content-script-ready");

    exportFunction(browser.test.sendMessage, window, {
      defineAs: "browserTestSendMessage",
    });

    window.addEventListener("pageshow", () => {
      browser.test.fail(
        "pageshow should have been suppressed by stopImmediatePropagation"
      );
    });
    window.addEventListener(
      "pagehide",
      () => {
        browser.test.fail(
          "pagehide should have been suppressed by stopImmediatePropagation"
        );
      },
      true
    );
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://example.com/bfcachetestpage"],
          js: ["content_script.js"],
        },
      ],
    },

    files: {
      "content_script.js": contentScript,
    },
  });

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/bfcachetestpage"
  );
  // When a cross-origin navigation happens, the contentPage's process may
  // change. Get a handle to the spawn helper to communicate with the process.
  // When the page is in the bfcache, the process is guaranteed to be around.
  const initialProcessSP = contentPage.getCurrentContentProcessSpecialPowers();

  await extension.startup();
  await extension.awaitMessage("content-script-ready");

  // Get the content script context and check that it points to the correct window.
  await contentPage.spawn([extension.id], async extensionId => {
    const { ExtensionContent } = ChromeUtils.importESModule(
      "resource://gre/modules/ExtensionContent.sys.mjs"
    );
    let context = ExtensionContent.getContextByExtensionId(
      extensionId,
      this.content
    );

    Assert.ok(context, "Got content script context");

    Assert.equal(
      context.contentWindow,
      this.content,
      "Context's contentWindow property is correct"
    );

    // To allow later contentPage.spawn() calls to verify that the contexts
    // are identical, remember it.
    ExtensionContent._rememberedContextForTest = context;
  });

  // Navigate so that the content page is hidden in the bfcache.
  await contentPage.loadURL("http://example.org/dummy?noscripthere1");
  info("Expecting page to enter bfcache");

  await extension.awaitMessage("content-script-hide");

  await initialProcessSP.spawn([], async () => {
    const { ExtensionContent } = ChromeUtils.importESModule(
      "resource://gre/modules/ExtensionContent.sys.mjs"
    );
    let context = ExtensionContent._rememberedContextForTest;
    Assert.equal(
      context.contentWindow,
      null,
      "Context's contentWindow property is null"
    );
    Assert.ok(context.sandbox, "Context's sandbox exists");
  });

  await contentPage.spawn([], async () => {
    // Navigate back so the content page is resurrected from the bfcache.
    this.content.history.back();
  });

  await extension.awaitMessage("content-script-show");

  await contentPage.spawn([extension.id], async extensionId => {
    const { ExtensionContent } = ChromeUtils.importESModule(
      "resource://gre/modules/ExtensionContent.sys.mjs"
    );
    let context = ExtensionContent.getContextByExtensionId(
      extensionId,
      this.content
    );
    Assert.equal(
      context,
      ExtensionContent._rememberedContextForTest,
      "Same as initial context"
    );
    Assert.equal(
      context.contentWindow,
      this.content,
      "Context's contentWindow property is correct"
    );
    Assert.ok(context.sandbox, "Context's sandbox exists before unload");
  });

  await initialProcessSP.spawn([], async () => {
    const { ExtensionContent } = ChromeUtils.importESModule(
      "resource://gre/modules/ExtensionContent.sys.mjs"
    );
    let context = ExtensionContent._rememberedContextForTest;
    // Note: this is same as "this.content" inside contentPage.spawn():
    let contentOfContentPage = context.contentWindow;

    let contextUnloadedPromise = new Promise(resolve => {
      context.callOnClose({ close: resolve });
    });
    // Now add an "unload" event listener, which should prevent a page from entering the bfcache.
    await new Promise(resolve => {
      contentOfContentPage.addEventListener("unload", () => {
        Assert.equal(
          context.contentWindow,
          contentOfContentPage,
          "Context's contentWindow property should be non-null at unload"
        );
        resolve();
      });
      contentOfContentPage.location = "http://example.org/dummy?noscripthere2";
    });

    await contextUnloadedPromise;
  });

  await extension.awaitMessage("content-script-unload");

  // Note: we set dom.ipc.processReuse.unusedGraceMs before, to make sure that
  // the process stays for a bit longer despite the contentPage having been
  // unloaded without bfcache entry.
  await initialProcessSP.spawn([], async () => {
    const { ExtensionContent } = ChromeUtils.importESModule(
      "resource://gre/modules/ExtensionContent.sys.mjs"
    );
    let context = ExtensionContent._rememberedContextForTest;
    Assert.equal(
      context.sandbox,
      null,
      "Context's sandbox has been destroyed after unload"
    );
  });

  await contentPage.close();
  await initialProcessSP.destroy();
  await extension.unload();
});
