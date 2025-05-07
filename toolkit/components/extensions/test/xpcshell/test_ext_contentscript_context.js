"use strict";

/* eslint-disable mozilla/balanced-listeners */

const server = createHttpServer({ hosts: ["example.com", "example.org"] });

server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
  response.write("<!DOCTYPE html><html></html>");
});

function delayContentProcessTermination() {
  Services.prefs.setIntPref("dom.ipc.processReuse.unusedGraceMs", 30_000);
  registerCleanupFunction(undoDelayedContentProcessTermination);
}

function undoDelayedContentProcessTermination() {
  Services.prefs.clearUserPref("dom.ipc.processReuse.unusedGraceMs");
  Services.ppmm.releaseCachedProcesses();
}

function loadExtension() {
  function contentScript() {
    browser.test.sendMessage("content-script-ready");

    window.addEventListener(
      "pagehide",
      () => {
        browser.test.sendMessage("content-script-hide");
      },
      true
    );
    window.addEventListener("pageshow", () => {
      browser.test.sendMessage("content-script-show");
    });
  }

  return ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://example.com/dummy*"],
          js: ["content_script.js"],
          run_at: "document_start",
        },
      ],
    },

    files: {
      "content_script.js": contentScript,
    },
  });
}

add_task(async function test_contentscript_context() {
  let extension = loadExtension();
  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy"
  );
  // When a cross-origin navigation happens, the contentPage's process may
  // change. Get a handle to the spawn helper to communicate with the process.
  // When the page is in the bfcache, the process is guaranteed to be around.
  const initialProcessSP = contentPage.getCurrentContentProcessSpecialPowers();

  await extension.awaitMessage("content-script-ready");
  await extension.awaitMessage("content-script-show");

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
  await contentPage.loadURL("http://example.org/dummy");

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
  });

  await contentPage.spawn([], async () => {
    // Navigate back so the content page is resurrected from the bfcache.
    this.content.history.back();
  });

  await extension.awaitMessage("content-script-show");

  await contentPage.spawn([], async () => {
    const { ExtensionContent } = ChromeUtils.importESModule(
      "resource://gre/modules/ExtensionContent.sys.mjs"
    );
    let context = ExtensionContent._rememberedContextForTest;
    Assert.equal(
      context.contentWindow,
      this.content,
      "Context's contentWindow property is correct"
    );
  });

  await contentPage.close();
  await extension.awaitMessage("content-script-hide");
  await extension.unload();
  await initialProcessSP.destroy();
});

add_task(async function test_contentscript_context_incognito_not_allowed() {
  async function background() {
    await browser.contentScripts.register({
      js: [{ file: "registered_script.js" }],
      matches: ["http://example.com/dummy"],
      runAt: "document_start",
    });

    browser.test.sendMessage("background-ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://example.com/dummy"],
          js: ["content_script.js"],
          run_at: "document_start",
        },
      ],
      permissions: ["http://example.com/*"],
    },
    background,
    files: {
      "content_script.js": () => {
        browser.test.notifyFail("content_script_loaded");
      },
      "registered_script.js": () => {
        browser.test.notifyFail("registered_script_loaded");
      },
    },
  });

  await extension.startup();
  await extension.awaitMessage("background-ready");

  // xpcshell test server does not support https (bug 1742061), so prevent
  // https-first in PBM. Without this pref, contentPage.spawn() below fails
  // with: "Actor 'SpecialPowers' destroyed before query 'Spawn' was resolved".
  // There was also an Android-specific issue, see bug 1715801.
  Services.prefs.setBoolPref("dom.security.https_first_pbm", false);
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy",
    { privateBrowsing: true }
  );

  Services.prefs.clearUserPref("dom.security.https_first_pbm");

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
      null,
      "Extension unable to use content_script in private browsing window"
    );
  });

  await contentPage.close();
  await extension.unload();
});

add_task(async function test_contentscript_context_unload_while_in_bfcache() {
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy?first"
  );
  // When a cross-origin navigation happens, the contentPage's process may
  // change. Get a handle to the spawn helper to communicate with the process.
  // When the page is in the bfcache, the process is guaranteed to be around.
  const initialProcessSP = contentPage.getCurrentContentProcessSpecialPowers();

  let extension = loadExtension();
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

    Assert.equal(
      context.contentWindow,
      this.content,
      "Context's contentWindow property is correct"
    );

    let contextUnloadedPromise = new Promise(resolve => {
      context.callOnClose({ close: resolve });
    });
    let pageshownPromise = new Promise(resolve => {
      this.content.addEventListener(
        "pageshow",
        () => {
          // Yield to the event loop once more to ensure that all pageshow event
          // handlers have been dispatched before fulfilling the promise.
          let { setTimeout } = ChromeUtils.importESModule(
            "resource://gre/modules/Timer.sys.mjs"
          );
          setTimeout(resolve, 0);
        },
        { once: true, mozSystemGroup: true }
      );
    });

    // Save context so we can verify that contentWindow is nulled after unload.
    ExtensionContent._rememberStateForTest = {
      context,
      contextUnloadedPromise,
      pageshownPromise,
    };
  });

  // Navigate so that the content page is hidden in the bfcache.
  await contentPage.loadURL("http://example.org/dummy?second");

  await extension.awaitMessage("content-script-hide");

  await extension.unload();
  await initialProcessSP.spawn([], async () => {
    const { ExtensionContent } = ChromeUtils.importESModule(
      "resource://gre/modules/ExtensionContent.sys.mjs"
    );
    const { context, contextUnloadedPromise } =
      ExtensionContent._rememberStateForTest;
    await contextUnloadedPromise;
    Assert.equal(context.unloaded, true, "Context has been unloaded");

    // Normally, when a page is not in the bfcache, context.contentWindow is
    // not null when the callOnClose handler is invoked (this is checked by the
    // previous subtest).
    // Now wait a little bit and check again to ensure that the contentWindow
    // property is not somehow restored.
    const { setTimeout } = ChromeUtils.importESModule(
      "resource://gre/modules/Timer.sys.mjs"
    );
    await new Promise(resolve => setTimeout(resolve, 0));
    Assert.equal(
      context.contentWindow,
      null,
      "Context's contentWindow property is null"
    );
  });

  await contentPage.spawn([], async () => {
    // Navigate back so the content page is resurrected from the bfcache.
    this.content.history.back();
  });

  await initialProcessSP.spawn([], async () => {
    const { ExtensionContent } = ChromeUtils.importESModule(
      "resource://gre/modules/ExtensionContent.sys.mjs"
    );
    const { context, pageshownPromise } =
      ExtensionContent._rememberStateForTest;

    await pageshownPromise;

    // After restoring from bfcache, the window is valid, but the context
    // is not (due to extension unload).
    Assert.equal(
      context.contentWindow,
      null,
      "Context's contentWindow property is null after restore from bfcache"
    );
  });

  await contentPage.close();
  await initialProcessSP.destroy();
});

add_task(async function test_contentscript_context_valid_during_execution() {
  // This test does the following:
  // - Load page
  // - Load extension; inject content script.
  // - Navigate page; pagehide triggered.
  // - Navigate back; pageshow triggered.
  // - Close page; pagehide, unload triggered.
  // At each of these last four events, the validity of the context is checked.

  function contentScript() {
    browser.test.sendMessage("content-script-ready");
    window.wrappedJSObject.checkContextIsValid("Context is valid on execution");

    window.addEventListener(
      "pagehide",
      () => {
        window.wrappedJSObject.checkContextIsValid(
          "Context is valid on pagehide"
        );
        browser.test.sendMessage("content-script-hide");
      },
      true
    );
    window.addEventListener("pageshow", () => {
      window.wrappedJSObject.checkContextIsValid(
        "Context is valid on pageshow"
      );

      // This unload listener is registered after pageshow, to ensure that the
      // page can be stored in the bfcache at the previous pagehide.
      window.addEventListener("unload", () => {
        window.wrappedJSObject.checkContextIsValid(
          "Context is valid on unload"
        );
        browser.test.sendMessage("content-script-unload");
      });

      browser.test.sendMessage("content-script-show");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://example.com/dummy*"],
          js: ["content_script.js"],
        },
      ],
    },

    files: {
      "content_script.js": contentScript,
    },
  });

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy?first"
  );
  // When a cross-origin navigation happens, the contentPage's process may
  // change. Get a handle to the spawn helper to communicate with the process.
  // When the page is in the bfcache, the process is guaranteed to be around.
  const initialProcessSP = contentPage.getCurrentContentProcessSpecialPowers();
  await initialProcessSP.spawn(
    [contentPage.browsingContext, extension.id],
    (browsingContext, extensionId) => {
      const content = browsingContext.window;
      let context;
      let checkContextIsValid = description => {
        if (!context) {
          const { ExtensionContent } = ChromeUtils.importESModule(
            "resource://gre/modules/ExtensionContent.sys.mjs"
          );
          context = ExtensionContent.getContextByExtensionId(
            extensionId,
            content
          );
        }
        // Note: These Assert calls may happen when the window is about to be
        // destroyed. Because of that, we use initialProcessSP.spawn() instead
        // of contentPage.spawn(), to make sure that the Assert results can be
        // reported.
        Assert.equal(
          context.contentWindow,
          content,
          `${description}: contentWindow`
        );
        Assert.equal(context.active, true, `${description}: active`);
      };
      Cu.exportFunction(checkContextIsValid, content, {
        defineAs: "checkContextIsValid",
      });
    }
  );
  await extension.startup();
  await extension.awaitMessage("content-script-ready");

  // Delay process termination for a little bit, so that the above assertions
  // from checkContextIsValid() have a chance to be run after unload.
  delayContentProcessTermination();

  // Navigate so that the content page is frozen in the bfcache.
  await contentPage.loadURL("http://example.org/dummy?second");

  await extension.awaitMessage("content-script-hide");
  await contentPage.spawn([], async () => {
    // Navigate back so the content page is resurrected from the bfcache.
    this.content.history.back();
  });

  await extension.awaitMessage("content-script-show");
  await contentPage.close();
  await extension.awaitMessage("content-script-hide");
  await extension.awaitMessage("content-script-unload");
  await extension.unload();
  await initialProcessSP.destroy();

  undoDelayedContentProcessTermination();
});
