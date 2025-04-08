/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* globals browser */ // extension scripts have access to browser global.

function hasTranslationActor(browsingContext) {
  return !!browsingContext.currentWindowGlobal.getExistingActor("Translations");
}

add_task(async function test_actor_at_https() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/"
  );
  Assert.ok(
    hasTranslationActor(tab.linkedBrowser.browsingContext),
    "Tab with https URL has actor"
  );
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_actor_at_data_url() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "data:,");
  Assert.ok(
    !hasTranslationActor(tab.linkedBrowser.browsingContext),
    "Tab with data:-URL does not have actor"
  );
  BrowserTestUtils.removeTab(tab);
});

// Confirms that the Translations actor is only available in moz-extension pages
// but otherwise unavailable in extension documents, including child frames
// (even if these are from https).
add_task(async function test_actor_at_moz_extension() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      background: {
        page: "withframe.html",
      },
    },
    files: {
      "withframe.html": `<body><script src="withframe.js"></script>`,
      "withframe.js": () => {
        let f = document.createElement("iframe");
        f.src = "https://example.com/?bg";
        f.onload = () => {
          browser.test.sendMessage("frame_loaded_in_extpage");
        };
        document.body.append(f);
      },
    },
  });
  await extension.startup();
  await extension.awaitMessage("frame_loaded_in_extpage");

  info("Verifying actor availability in extension background page");
  {
    let bgBrowsingContext;
    for (let view of WebExtensionPolicy.getByID(extension.id).extension.views) {
      if (view.viewType === "background") {
        bgBrowsingContext = view.browsingContext;
      }
    }
    Assert.ok(
      !hasTranslationActor(bgBrowsingContext),
      "Extension background pages do not have an actor"
    );
    Assert.ok(
      !hasTranslationActor(bgBrowsingContext.children[0]),
      "https iframe in extension background page does not have an actor"
    );
  }

  info("Verifying actor availability in extension tab");
  {
    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      `moz-extension://${extension.uuid}/withframe.html`
    );
    await extension.awaitMessage("frame_loaded_in_extpage");

    Assert.ok(
      hasTranslationActor(tab.linkedBrowser.browsingContext),
      "moz-extension:-page in tab has actor"
    );
    Assert.ok(
      !hasTranslationActor(tab.linkedBrowser.browsingContext.children[0]),
      "https iframe in moz-extension:-page in tab does not have actor"
    );

    BrowserTestUtils.removeTab(tab);
  }

  await extension.unload();
});

// Verifies that an extension sidebar does not have an actor,
// even after navigation to a https:-document.
add_task(async function test_actor_at_moz_extension_sidebar_action() {
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary", // To automatically show sidebar on load.
    manifest: {
      sidebar_action: {
        default_panel: "sidebar.html",
      },
      content_scripts: [
        {
          matches: ["https://example.com/?sidebar"],
          js: ["contentscript_in_sidebar.js"],
        },
      ],
    },
    files: {
      "sidebar.html": `<script src="sidebar.js"></script>`,
      "sidebar.js": () => {
        browser.test.onMessage.addListener(msg => {
          browser.test.assertEq("navigate_to_https", msg, "Expected message");
          location.href = "https://example.com/?sidebar";
        });
        window.onload = () => {
          browser.test.sendMessage("sidebar-ready");
        };
      },
      "contentscript_in_sidebar.js": () => {
        browser.test.sendMessage("contentscript_in_sidebar_loaded");
      },
    },
  });
  await extension.startup();

  await extension.awaitMessage("sidebar-ready");
  let sidebarBrowser =
    SidebarController.browser.contentWindow.gBrowser.selectedBrowser;
  Assert.equal(
    sidebarBrowser.currentURI.spec,
    `moz-extension://${extension.uuid}/sidebar.html`,
    "Sidebar is showing extension sidebar"
  );
  Assert.ok(
    !hasTranslationActor(sidebarBrowser.browsingContext),
    "Extension sidebar does not have actor"
  );

  extension.sendMessage("navigate_to_https");
  await extension.awaitMessage("contentscript_in_sidebar_loaded");
  Assert.equal(
    sidebarBrowser.currentURI.spec,
    "https://example.com/?sidebar",
    "Sidebar is showing the https document"
  );
  Assert.ok(
    !hasTranslationActor(sidebarBrowser.browsingContext),
    "Extension sidebar does not have actor, despite it showing a https page"
  );

  await extension.unload();
});
