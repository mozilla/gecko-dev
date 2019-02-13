/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

///////////////////
//
// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed.
//
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("TypeError: Assert is null");


let SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

let tabsToRemove = [];


function removeAllProviders(callback) {
  // all the providers may have been added.
  function removeProviders() {
    if (Social.providers.length < 1) {
      executeSoon(function() {
        is(Social.providers.length, 0, "all providers removed");
        executeSoon(callback);
      });
      return;
    }

    // a full install sets the manifest into a pref, addProvider alone doesn't,
    // make sure we uninstall if the manifest was added.
    if (Social.providers[0].manifest) {
      SocialService.uninstallProvider(Social.providers[0].origin, removeProviders);
    } else {
      SocialService.disableProvider(Social.providers[0].origin, removeProviders);
    }
  }
  removeProviders();
}

function postTestCleanup(callback) {
  // any tabs opened by the test.
  for (let tab of tabsToRemove)
    gBrowser.removeTab(tab);
  tabsToRemove = [];
  // theses tests use the notification panel but don't bother waiting for it
  // to fully open - the end result is that the panel might stay open
  //SocialUI.activationPanel.hidePopup();

  // all the providers may have been added.
  removeAllProviders(callback);
}

function addTab(url, callback) {
  let tab = gBrowser.selectedTab = gBrowser.addTab(url, {skipAnimation: true});
  tab.linkedBrowser.addEventListener("load", function tabLoad(event) {
    tab.linkedBrowser.removeEventListener("load", tabLoad, true);
    tabsToRemove.push(tab);
    executeSoon(function() {callback(tab)});
  }, true);
}

function sendActivationEvent(tab, callback, nullManifest) {
  // hack Social.lastEventReceived so we don't hit the "too many events" check.
  Social.lastEventReceived = 0;
  BrowserTestUtils.synthesizeMouseAtCenter("#activation", {}, tab.linkedBrowser);
  executeSoon(callback);
}

function activateProvider(domain, callback, nullManifest) {
  let activationURL = domain+"/browser/browser/base/content/test/social/social_activate_basic.html"
  addTab(activationURL, function(tab) {
    sendActivationEvent(tab, callback, nullManifest);
  });
}

function activateIFrameProvider(domain, callback) {
  let activationURL = domain+"/browser/browser/base/content/test/social/social_activate_iframe.html"
  addTab(activationURL, function(tab) {
    sendActivationEvent(tab, callback, false);
  });
}

function waitForProviderLoad(cb) {
    waitForCondition(function() {
      let sbrowser = document.getElementById("social-sidebar-browser");
      let provider = SocialSidebar.provider;
      let postActivation = provider && gBrowser.contentDocument &&
                            gBrowser.contentDocument.location.href == provider.origin + "/browser/browser/base/content/test/social/social_postActivation.html";

      return postActivation && sbrowser.docShellIsActive;
    }, function() {
      // executeSoon to let the browser UI observers run first
      executeSoon(cb);
    },
    "waitForProviderLoad: provider was not loaded");
}


function getAddonItemInList(aId, aList) {
  var item = aList.firstChild;
  while (item) {
    if ("mAddon" in item && item.mAddon.id == aId) {
      aList.ensureElementIsVisible(item);
      return item;
    }
    item = item.nextSibling;
  }
  return null;
}

function clickAddonRemoveButton(tab, aCallback) {
  AddonManager.getAddonsByTypes(["service"], function(aAddons) {
    let addon = aAddons[0];

    let doc = tab.linkedBrowser.contentDocument;;
    let list = doc.getElementById("addon-list");

    let item = getAddonItemInList(addon.id, list);
    let button = item._removeBtn;
    isnot(button, null, "Should have a remove button");
    ok(!button.disabled, "Button should not be disabled");

    // uninstall happens after about:addons tab is closed, so we wait on
    // disabled
    promiseObserverNotified("social:provider-disabled").then(() => {
      is(item.getAttribute("pending"), "uninstall", "Add-on should be uninstalling");
      executeSoon(function() { aCallback(addon); });
    });

    BrowserTestUtils.synthesizeMouseAtCenter(button, {}, tab.linkedBrowser);
  });
}

function activateOneProvider(manifest, finishActivation, aCallback) {
  let panel = document.getElementById("servicesInstall-notification");
  PopupNotifications.panel.addEventListener("popupshown", function onpopupshown() {
    PopupNotifications.panel.removeEventListener("popupshown", onpopupshown);
    ok(!panel.hidden, "servicesInstall-notification panel opened");
    if (finishActivation)
      panel.button.click();
    else
      panel.closebutton.click();
  });
  PopupNotifications.panel.addEventListener("popuphidden", function _hidden() {
    PopupNotifications.panel.removeEventListener("popuphidden", _hidden);
    ok(panel.hidden, "servicesInstall-notification panel hidden");
    if (!finishActivation) {
      ok(panel.hidden, "activation panel is not showing");
      executeSoon(aCallback);
    } else {
      waitForProviderLoad(function() {
        is(SocialSidebar.provider.origin, manifest.origin, "new provider is active");
        ok(SocialSidebar.opened, "sidebar is open");
        checkSocialUI();
        executeSoon(aCallback);
      });
    }
  });

  // the test will continue as the popup events fire...
  activateProvider(manifest.origin, function() {
    info("waiting on activation panel to open/close...");
  });
}

let gTestDomains = ["https://example.com", "https://test1.example.com", "https://test2.example.com"];
let gProviders = [
  {
    name: "provider 1",
    origin: "https://example.com",
    sidebarURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar_empty.html?provider1",
    iconURL: "chrome://branding/content/icon48.png"
  },
  {
    name: "provider 2",
    origin: "https://test1.example.com",
    sidebarURL: "https://test1.example.com/browser/browser/base/content/test/social/social_sidebar_empty.html?provider2",
    iconURL: "chrome://branding/content/icon64.png"
  },
  {
    name: "provider 3",
    origin: "https://test2.example.com",
    sidebarURL: "https://test2.example.com/browser/browser/base/content/test/social/social_sidebar_empty.html?provider2",
    iconURL: "chrome://branding/content/about-logo.png"
  }
];


function test() {
  waitForExplicitFinish();
  runSocialTests(tests, undefined, postTestCleanup);
}

var tests = {
  testActivationWrongOrigin: function(next) {
    // At this stage none of our providers exist, so we expect failure.
    Services.prefs.setBoolPref("social.remote-install.enabled", false);
    activateProvider(gTestDomains[0], function() {
      is(SocialUI.enabled, false, "SocialUI is not enabled");
      let panel = document.getElementById("servicesInstall-notification");
      ok(panel.hidden, "activation panel still hidden");
      checkSocialUI();
      Services.prefs.clearUserPref("social.remote-install.enabled");
      removeAllProviders(next);
    });
  },
  
  testIFrameActivation: function(next) {
    activateIFrameProvider(gTestDomains[0], function() {
      is(SocialUI.enabled, false, "SocialUI is not enabled");
      ok(!SocialSidebar.provider, "provider is not installed");
      let panel = document.getElementById("servicesInstall-notification");
      ok(panel.hidden, "activation panel still hidden");
      checkSocialUI();
      removeAllProviders(next);
    });
  },
  
  testActivationFirstProvider: function(next) {
    // first up we add a manifest entry for a single provider.
    activateOneProvider(gProviders[0], false, function() {
      // we deactivated leaving no providers left, so Social is disabled.
      ok(!SocialSidebar.provider, "should be no provider left after disabling");
      checkSocialUI();
      removeAllProviders(next);
    });
  },
  
  testActivationMultipleProvider: function(next) {
    // The trick with this test is to make sure that Social.providers[1] is
    // the current provider when doing the undo - this makes sure that the
    // Social code doesn't fallback to Social.providers[0], which it will
    // do in some cases (but those cases do not include what this test does)
    // first enable the 2 providers
    SocialService.addProvider(gProviders[0], function() {
      SocialService.addProvider(gProviders[1], function() {
        checkSocialUI();
        // activate the last provider.
        activateOneProvider(gProviders[2], false, function() {
          // we deactivated - the first provider should be enabled.
          is(SocialSidebar.provider.origin, Social.providers[1].origin, "original provider should have been reactivated");
          checkSocialUI();
          removeAllProviders(next);
        });
      });
    });
  },

  testAddonManagerDoubleInstall: function(next) {
    // Create a new tab and load about:addons
    let blanktab = gBrowser.addTab();
    gBrowser.selectedTab = blanktab;
    BrowserOpenAddonsMgr('addons://list/service');

    is(blanktab, gBrowser.selectedTab, "Current tab should be blank tab");

    gBrowser.selectedBrowser.addEventListener("load", function tabLoad() {
      gBrowser.selectedBrowser.removeEventListener("load", tabLoad, true);
      let browser = blanktab.linkedBrowser;
      is(browser.currentURI.spec, "about:addons", "about:addons should load into blank tab.");

      activateOneProvider(gProviders[0], true, function() {
        info("first activation completed");
        is(gBrowser.contentDocument.location.href, gProviders[0].origin + "/browser/browser/base/content/test/social/social_postActivation.html", "postActivationURL loaded");
        gBrowser.removeTab(gBrowser.selectedTab);
        is(gBrowser.contentDocument.location.href, gProviders[0].origin + "/browser/browser/base/content/test/social/social_activate_basic.html", "activation page selected");
        gBrowser.removeTab(gBrowser.selectedTab);
        tabsToRemove.pop();
        // uninstall the provider
        clickAddonRemoveButton(blanktab, function(addon) {
          checkSocialUI();
          activateOneProvider(gProviders[0], true, function() {
            info("second activation completed");
            is(gBrowser.contentDocument.location.href, gProviders[0].origin + "/browser/browser/base/content/test/social/social_postActivation.html", "postActivationURL loaded");
            gBrowser.removeTab(gBrowser.selectedTab);

            // after closing the addons tab, verify provider is still installed
            gBrowser.tabContainer.addEventListener("TabClose", function onTabClose() {
              gBrowser.tabContainer.removeEventListener("TabClose", onTabClose);
              AddonManager.getAddonsByTypes(["service"], function(aAddons) {
                is(aAddons.length, 1, "there can be only one");
                removeAllProviders(next);
              });
            });

            // verify only one provider in list
            AddonManager.getAddonsByTypes(["service"], function(aAddons) {
              is(aAddons.length, 1, "there can be only one");

              let doc = blanktab.linkedBrowser.contentDocument;
              let list = doc.getElementById("addon-list");
              is(list.childNodes.length, 1, "only one addon is displayed");

              gBrowser.removeTab(blanktab);
            });

          });
        });
      });
    }, true);
  }
}
