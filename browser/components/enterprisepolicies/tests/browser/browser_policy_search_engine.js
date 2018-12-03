/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

ChromeUtils.import("resource://testing-common/CustomizableUITestUtils.jsm", this);
let gCUITestUtils = new CustomizableUITestUtils(window);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("browser.policies.runonce.setDefaultSearchEngine");
  Services.prefs.clearUserPref("browser.policies.runOncePerModification.addSearchEngines");
});

add_task(async function test_setup() {
  await gCUITestUtils.addSearchBar();
  registerCleanupFunction(() => {
    gCUITestUtils.removeSearchBar();
  });
});

// |shouldWork| should be true if opensearch is expected to work and false if
// it is not.
async function test_opensearch(shouldWork) {
  let searchBar = BrowserSearch.searchBar;

  let rootDir = getRootDirectory(gTestPath);
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, rootDir + "opensearch.html");
  let searchPopup = document.getElementById("PopupSearchAutoComplete");
  let promiseSearchPopupShown = BrowserTestUtils.waitForEvent(searchPopup, "popupshown");
  let searchBarButton = searchBar.querySelector(".searchbar-search-button");

  searchBarButton.click();
  await promiseSearchPopupShown;
  let oneOffsContainer = document.getAnonymousElementByAttribute(searchPopup,
                                                                 "anonid",
                                                                 "search-one-off-buttons");
  let engineListElement = oneOffsContainer.querySelector(".search-add-engines");
  if (shouldWork) {
    ok(engineListElement.firstElementChild,
       "There should be search engines available to add");
    ok(searchBar.getAttribute("addengines"),
       "Search bar should have addengines attribute");
  } else {
    is(engineListElement.firstElementChild, null,
       "There should be no search engines available to add");
    ok(!searchBar.getAttribute("addengines"),
       "Search bar should not have addengines attribute");
  }
  await BrowserTestUtils.removeTab(tab);
}

add_task(async function test_install_and_set_default() {
  // Make sure we are starting in an expected state to avoid false positive
  // test results.
  isnot(Services.search.defaultEngine.name, "MozSearch",
        "Default search engine should not be MozSearch when test starts");
  is(Services.search.getEngineByName("Foo"), null,
     "Engine \"Foo\" should not be present when test starts");

  await setupPolicyEngineWithJson({
    "policies": {
      "SearchEngines": {
        "Add": [
          {
            "Name": "MozSearch",
            "URLTemplate": "http://example.com/?q={searchTerms}",
          },
        ],
        "Default": "MozSearch",
      },
    },
  });

  // If this passes, it means that the new search engine was properly installed
  // *and* was properly set as the default.
  is(Services.search.defaultEngine.name, "MozSearch",
     "Specified search engine should be the default");

  // Clean up
  Services.search.removeEngine(Services.search.defaultEngine);
  EnterprisePolicyTesting.resetRunOnceState();
});

// Same as the last test, but with "PreventInstalls" set to true to make sure
// it does not prevent search engines from being installed properly
add_task(async function test_install_and_set_default_prevent_installs() {
  isnot(Services.search.defaultEngine.name, "MozSearch",
        "Default search engine should not be MozSearch when test starts");
  is(Services.search.getEngineByName("Foo"), null,
     "Engine \"Foo\" should not be present when test starts");

  await setupPolicyEngineWithJson({
    "policies": {
      "SearchEngines": {
        "Add": [
          {
            "Name": "MozSearch",
            "URLTemplate": "http://example.com/?q={searchTerms}",
          },
        ],
        "Default": "MozSearch",
        "PreventInstalls": true,
      },
    },
  });

  is(Services.search.defaultEngine.name, "MozSearch",
     "Specified search engine should be the default");

  // Clean up
  Services.search.removeEngine(Services.search.defaultEngine);
  EnterprisePolicyTesting.resetRunOnceState();
});

add_task(async function test_opensearch_works() {
  // Clear out policies so we can test with no policies applied
  await setupPolicyEngineWithJson({
    "policies": {
    },
  });
  // Ensure that opensearch works before we make sure that it can be properly
  // disabled
  await test_opensearch(true);
});

add_task(async function setup_prevent_installs() {
  await setupPolicyEngineWithJson({
    "policies": {
      "SearchEngines": {
        "PreventInstalls": true,
      },
    },
  });
});

add_task(async function test_prevent_install_ui() {
  // Check that about:preferences does not prompt user to install search engines
  // if that feature is disabled
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:preferences");
  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    let linkContainer = content.document.getElementById("addEnginesBox");
    if (!linkContainer.hidden) {
      await new Promise(resolve => {
        let mut = new linkContainer.ownerGlobal.MutationObserver(mutations => {
          mut.disconnect();
          resolve();
        });
        mut.observe(linkContainer, {attributeFilter: ["hidden"]});
      });
    }
    is(linkContainer.hidden, true,
       "\"Find more search engines\" link should be hidden");
  });
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_opensearch_disabled() {
  // Check that search engines cannot be added via opensearch
  await test_opensearch(false);
});

add_task(async function test_AddSearchProvider() {
  if (!Services.prefs.getBoolPref("dom.sidebar.enabled", false)) {
    return;
  }

  // Mock the modal error dialog
  let mockPrompter = {
    promptCount: 0,
    alert() {
      this.promptCount++;
    },
    QueryInterface: ChromeUtils.generateQI([Ci.nsIPrompt]),
  };
  let windowWatcher = {
    getNewPrompter: () => mockPrompter,
    QueryInterface: ChromeUtils.generateQI([Ci.nsIWindowWatcher]),
  };
  let origWindowWatcher = Services.ww;
  Services.ww = windowWatcher;
  registerCleanupFunction(() => {
    Services.ww = origWindowWatcher;
  });

  let engineURL = getRootDirectory(gTestPath) + "opensearchEngine.xml";
  // AddSearchProvider will refuse to take URLs with a "chrome:" scheme
  engineURL = engineURL.replace("chrome://mochitests/content", "http://example.com");
  await ContentTask.spawn(gBrowser.selectedBrowser, {engineURL}, async function(args) {
    content.window.external.AddSearchProvider(args.engineURL);
  });

  is(Services.search.getEngineByName("Foo"), null,
     "Engine should not have been added successfully.");
  is(mockPrompter.promptCount, 1,
     "Should have alerted the user of an error when installing new search engine");
});

add_task(async function test_install_and_remove() {
  let iconURL = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=";

  is(Services.search.getEngineByName("Foo"), null,
     "Engine \"Foo\" should not be present when test starts");

  await setupPolicyEngineWithJson({
  "policies": {
      "SearchEngines": {
        "Add": [
          {
            "Name": "Foo",
            "URLTemplate": "http://example.com/?q={searchTerms}",
            "IconURL": iconURL,
          },
        ],
      },
    },
  });

  // If this passes, it means that the new search engine was properly installed

  let engine = Services.search.getEngineByName("Foo");
  isnot(engine, null,
     "Specified search engine should be installed");

  is(engine.wrappedJSObject.iconURI.spec, iconURL,
     "Icon should be present");

  await setupPolicyEngineWithJson({
  "policies": {
      "SearchEngines": {
        "Remove": ["Foo"],
      },
    },
  });

  // If this passes, it means that the specified engine was properly removed
  is(Services.search.getEngineByName("Foo"), null,
     "Specified search engine should not be installed");

  EnterprisePolicyTesting.resetRunOnceState();
});
