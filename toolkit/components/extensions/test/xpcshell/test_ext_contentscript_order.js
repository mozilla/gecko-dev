"use strict";

const server = createHttpServer({ hosts: ["example.com"] });
server.registerPathHandler("/beforehey", () => {
  // Dummy page that does not match any content script. This exists so that
  // disableContentScriptPreloading can run in the process that will be used
  // for the actual URL of the test.
});
server.registerPathHandler("/hey", (request, response) => {
  response.setHeader("Content-Type", "text/html; charset=utf-8");
  response.write("<html class='start'>");
});

// Running content scripts / styles consists of two phases:
// 1. Asynchronous loading/compilation of content scripts and styles.
// 2. Synchronous execution/applying of content scripts and styles.
//
// To make sure that scripts are executed in order, we need to verify that the
// first phase does not result in out-of-order execution.
//
// When preloading is enabled, the first phase could complete before the point
// of execution is reached, which would result in trivially passing tests.
// Therefore we disable preloading.
async function disableContentScriptPreloading(contentPage) {
  await contentPage.spawn([], () => {
    const { ExtensionProcessScript } = ChromeUtils.importESModule(
      "resource://gre/modules/ExtensionProcessScript.sys.mjs"
    );
    Assert.equal(
      typeof ExtensionProcessScript.preloadContentScript,
      "function",
      "ExtensionProcessScript.preloadContentScript available to monkey-patch"
    );
    ExtensionProcessScript.preloadContentScript = () => {};
  });
}

async function loadContentPageWithoutPreloading(path) {
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/beforehey"
  );
  await disableContentScriptPreloading(contentPage);
  await contentPage.loadURL(new URL(path, "http://example.com/").href);
  return contentPage;
}

add_task(async function test_js_order() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        // document_end scripts, should execute after document_start (below).
        {
          matches: ["*://example.com/hey"],
          js: ["3.js"],
          run_at: "document_end",
        },
        {
          matches: ["*://example.com/hey"],
          // 2.js is likely already cached because it ran at document_start.
          //
          // This test case can detect out-of-order bugs as follows:
          // If scripts were to run in the order of compilation completion
          // instead of extension-defined order, then the execution order at
          // document_end would be [2.js (from cache), 3.js, 4.js] instead of
          // the extension-defined order [3.js, 2.js, 4.js].
          js: ["2.js"],
          run_at: "document_end",
        },
        {
          matches: ["*://example.com/hey"],
          js: ["4.js"],
          run_at: "document_end",
        },
        // document_start scripts - should execute first.
        {
          matches: ["*://example.com/hey"],
          js: ["1.js"],
          run_at: "document_start",
        },
        {
          matches: ["*://example.com/hey"],
          js: ["2.js"],
          run_at: "document_start",
        },
        // document_idle scripts - should execute last.
        {
          matches: ["*://example.com/hey"],
          js: ["done_check_js.js"],
          run_at: "document_idle",
        },
      ],
    },
    files: {
      "1.js": "document.documentElement.className += ' _1';",
      "2.js": "document.documentElement.className += ' _2';",
      "3.js": "document.documentElement.className += ' _3';",
      "4.js": "document.documentElement.className += ' _4';",
      "done_check_js.js": () => {
        browser.test.assertEq(
          "start _1 _2 _3 _2 _4",
          document.documentElement.className,
          "Content script execution order should be: run_at, then array order"
        );
        browser.test.sendMessage("done");
      },
    },
  });

  await extension.startup();

  let contentPage = await loadContentPageWithoutPreloading("/hey");
  await extension.awaitMessage("done");
  await contentPage.close();
  await extension.unload();
});

add_task(async function test_css_order() {
  // This test extension declares stylesheets. See the "files" section below to
  // understand the expected behaviors.
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        // We'll load a top frame to cache two stylesheets.
        {
          matches: ["*://example.com/hey?top"],
          css: ["start_1.css", "start_3.css"],
          js: ["check_top_start.js"],
          run_at: "document_start",
        },
        {
          matches: ["*://example.com/hey?top"],
          css: ["end_1.css", "end_3.css"],
          js: ["check_top_end.js"],
          run_at: "document_end",
        },

        // document_end styles should apply in the following order:
        {
          all_frames: true,
          matches: ["*://example.com/hey?frame"],
          css: ["end_1.css"], // Cached due to load in ?top.
          run_at: "document_end",
        },
        {
          all_frames: true,
          matches: ["*://example.com/hey?frame"],
          css: ["end_2.css"],
          run_at: "document_end",
        },
        {
          all_frames: true,
          matches: ["*://example.com/hey?frame"],
          css: ["end_3.css"], // Cached due to load in ?top.
          run_at: "document_end",
        },

        // document_start styles should apply in the following order (before
        // document_start scripts!):
        {
          all_frames: true,
          matches: ["*://example.com/hey?frame"],
          css: ["start_1.css"], // Cached due to load in ?top.
          run_at: "document_start",
        },
        {
          all_frames: true,
          matches: ["*://example.com/hey?frame"],
          css: ["start_2.css"],
          run_at: "document_start",
        },
        {
          all_frames: true,
          matches: ["*://example.com/hey?frame"],
          css: ["start_3.css"], // Cached due to load in ?top.
          run_at: "document_start",
        },

        // Script runs after above document_start and document_end styles.
        {
          all_frames: true,
          matches: ["*://example.com/hey?frame"],
          js: ["done_check_css.js"],
          run_at: "document_end",
        },
      ],
    },
    files: {
      // Style sheets chosen such that the order of application is reflected in
      // the specified CSS variables. The order of stylesheets is given by
      // content_scripts in manifest above; the expected execution order is
      // checked in check_top_start.js, check_top_end.js and done_check_css.js
      // by reading the value of the CSS variables with getPropertyValue(),
      // which returns the value of the CSS file that applied last.
      "start_1.css": ":root { --css-start-12: 1; --css-start-1: 1; }",
      "start_2.css": ":root { --css-start-12: 2; --css-start-23: 2; }",
      "start_3.css": ":root { --css-start-23: 3; --css-run-at: start; }",
      "end_1.css": ":root { --css-end-12: 1; --css-end-1: 1; }",
      "end_2.css": ":root { --css-end-12: 2; --css-end-23: 2; }",
      "end_3.css": ":root { --css-end-23: 3; --css-run-at: end; }",
      "check_top_start.js": () => {
        const style = getComputedStyle(document.documentElement);
        // Sanity check: non-precompiled style applies.
        browser.test.assertEq(
          "1",
          style.getPropertyValue("--css-start-1"),
          "start_1.css applied at document_start"
        );
        // Sanity check: document_end style does not apply at document_start.
        browser.test.assertEq(
          "",
          style.getPropertyValue("--css-end-1"),
          "end_1.css does not apply at document_start"
        );
        browser.test.sendMessage("done:check_top_start");
      },
      "check_top_end.js": () => {
        const style = getComputedStyle(document.documentElement);
        browser.test.assertEq(
          "1",
          style.getPropertyValue("--css-end-1"),
          "end_1.css applied at document_end"
        );
        browser.test.sendMessage("done:check_top_end");
      },
      "done_check_css.js": () => {
        const style = getComputedStyle(document.documentElement);
        // Reminder: declared order is: (cached) 1, (new) 2, (cached) 3.
        let expectations = [
          // These verify that the style application is in the order of the
          // manifest, unaffected by caching order.
          //
          // These test cases can detect out-of-order bugs as follows:
          // If styles were to be applied in the order of compilation completion
          // instead of extension-defined order, then start_2.css would apply
          // after start_1.css and start_3.css (both of which were cached),
          // and --css-start-23 would unexpectedly become "2" instead of "3".
          // Similarly for (new) end_2.css vs (cached) end_1.css + end_3.css.
          ["--css-start-1", "1"],
          ["--css-start-12", "2"],
          ["--css-start-23", "3"],
          ["--css-end-1", "1"],
          ["--css-end-12", "2"],
          ["--css-end-23", "3"],
          // This verifies that document_end styles apply after document_start,
          // even if the document_end style is listed earlier.
          ["--css-run-at", "end"],
        ];
        for (let [cssVar, expected] of expectations) {
          browser.test.assertEq(
            expected,
            style.getPropertyValue(cssVar),
            `Got expected css order: ${cssVar} should be ${expected}`
          );
        }
        browser.test.sendMessage("done");
      },
    },
  });

  await extension.startup();

  let contentPage = await loadContentPageWithoutPreloading("/hey?top");
  await extension.awaitMessage("done:check_top_start");
  await extension.awaitMessage("done:check_top_end");
  await contentPage.spawn([extension.id], async extensionId => {
    let { document } = this.content;
    let style = this.content.getComputedStyle(document.documentElement);
    Assert.equal(
      style.getPropertyValue("--css-run-at"),
      "end",
      "end_3.css was the last style to apply"
    );

    const { ExtensionProcessScript } = ChromeUtils.importESModule(
      "resource://gre/modules/ExtensionProcessScript.sys.mjs"
    );
    let extChild = ExtensionProcessScript.getExtensionChild(extensionId);
    let cssCache = extChild.authorCSS;
    Assert.deepEqual(
      Array.from(cssCache.keys(), k => k.split("/").pop()),
      ["start_1.css", "start_3.css", "end_1.css", "end_3.css"],
      "CSSCache should contain all stylesheets that loaded in ?top"
    );

    // Now that we have verified that the style sheets have been cached, load
    // the child frame where we confirm that the order of style application is
    // independent of the cache state.
    let f = document.createElement("iframe");
    f.src = "http://example.com/hey?frame";
    await new Promise(resolve => {
      f.onload = resolve;
      document.body.append(f);
    });
    // Note: the following line also serves as a sanity check that the frame is
    // same-origin to the top AND hosted in the same process. Being in the same
    // process is a requirement for the CSSCache to be usable to ?frame.
    style = f.contentWindow.getComputedStyle(f.contentDocument.documentElement);

    // frame.onload should be postponed until document_start styles apply. This
    // is implemented through a document.blockParsing call.
    Assert.equal(
      style.getPropertyValue("--css-start-1"),
      "1",
      "start_1.css (cached) applies before frame.onload"
    );
    Assert.equal(
      style.getPropertyValue("--css-start-12"),
      "2",
      "start_2.css (not cached) applies before frame.onload"
    );
    Assert.equal(
      style.getPropertyValue("--css-start-23"),
      "3",
      "start_3.css (cached, blocked on start_2.css) applies before frame.onload"
    );
    // frame.onload is fired shortly after DOMContentLoaded. DOMContentLoaded
    // is the point where document_end styles should apply, and therefore any
    // already-cached stylesheets should apply immediately.
    Assert.equal(
      style.getPropertyValue("--css-end-1"),
      "1",
      "end_1.css (cached) applies before frame.onload"
    );
    // document_end scripts do not block the parser, so frame.onload can fire
    // while style_2.css and style_3.css are still compiling.
    // TODO: What if style compilation is fast, and beats frame.onload?
    Assert.equal(
      style.getPropertyValue("--css-end-23"),
      "",
      "end_3.css (cached, blocked on end_2.css) does not block frame.onload"
    );
  });
  await extension.awaitMessage("done");
  await contentPage.spawn([extension.id], extensionId => {
    // Verify that the style cache is still populated. Its expiration time is
    // very long (CSS_EXPIRY_TIMEOUT_MS = 30 minutes). Memory pressure could
    // evict stylesheets from the cache, but not if there are still documents
    // referencing them. In this case, ?top and ?frame are still around.
    const { ExtensionProcessScript } = ChromeUtils.importESModule(
      "resource://gre/modules/ExtensionProcessScript.sys.mjs"
    );
    let extChild = ExtensionProcessScript.getExtensionChild(extensionId);
    let cssCache = extChild.authorCSS;
    Assert.deepEqual(
      Array.from(cssCache.keys(), k => k.split("/").pop()),
      [
        // Existing cache items, loaded via ?top:
        "start_1.css",
        "start_3.css",
        "end_1.css",
        "end_3.css",
        // New cache items, loaded via ?frame:
        "start_2.css",
        "end_2.css",
      ],
      "CSSCache should contain all stylesheets that loaded in ?top and ?frame"
    );
  });
  await contentPage.close();
  await extension.unload();
});
