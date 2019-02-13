/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

let manifest = { // builtin provider
  name: "provider example.com",
  origin: "https://example.com",
  sidebarURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar.html",
  workerURL: "https://example.com/browser/browser/base/content/test/social/social_worker.js",
  iconURL: "https://example.com/browser/browser/base/content/test/general/moz.png"
};
let manifest2 = { // used for testing install
  name: "provider test1",
  origin: "https://test1.example.com",
  workerURL: "https://test1.example.com/browser/browser/base/content/test/social/social_worker.js",
  statusURL: "https://test1.example.com/browser/browser/base/content/test/social/social_panel.html",
  iconURL: "https://test1.example.com/browser/browser/base/content/test/general/moz.png",
  version: 1
};
let manifest3 = { // used for testing install
  name: "provider test2",
  origin: "https://test2.example.com",
  sidebarURL: "https://test2.example.com/browser/browser/base/content/test/social/social_sidebar.html",
  iconURL: "https://test2.example.com/browser/browser/base/content/test/general/moz.png",
  version: 1
};


function openWindowAndWaitForInit(callback) {
  let topic = "browser-delayed-startup-finished";
  let w = OpenBrowserWindow();
  Services.obs.addObserver(function providerSet(subject, topic, data) {
    Services.obs.removeObserver(providerSet, topic);
    executeSoon(() => callback(w));
  }, topic, false);
}

function test() {
  waitForExplicitFinish();

  runSocialTestWithProvider(manifest, function (finishcb) {
    runSocialTests(tests, undefined, undefined, function () {
      Services.prefs.clearUserPref("social.remote-install.enabled");
      // just in case the tests failed, clear these here as well
      Services.prefs.clearUserPref("social.whitelist");
      ok(CustomizableUI.inDefaultState, "Should be in the default state when we finish");
      CustomizableUI.reset();
      finishcb();
    });
  });
}

var tests = {
  testNoButtonOnEnable: function(next) {
    // we expect the addon install dialog to appear, we need to accept the
    // install from the dialog.
    let panel = document.getElementById("servicesInstall-notification");
    ensureEventFired(PopupNotifications.panel, "popupshown").then(() => {
      info("servicesInstall-notification panel opened");
      panel.button.click();
    })

    let activationURL = manifest3.origin + "/browser/browser/base/content/test/social/social_activate.html"
    addTab(activationURL, function(tab) {
      let doc = tab.linkedBrowser.contentDocument;
      let data = {
        origin: doc.nodePrincipal.origin,
        url: doc.location.href,
        manifest: manifest3,
        window: window
      }
      Social.installProvider(data, function(addonManifest) {
        // enable the provider so we know the button would have appeared
        SocialService.enableProvider(manifest3.origin, function(provider) {
          is(provider.origin, manifest3.origin, "provider is installed");
          let id = SocialStatus._toolbarHelper.idFromOrigin(provider.origin);
          let widget = CustomizableUI.getWidget(id);
          ok(!widget || !widget.forWindow(window).node, "no button added to widget set");
          Social.uninstallProvider(manifest3.origin, function() {
            gBrowser.removeTab(tab);
            next();
          });
        });
      });
    });
  },
  testButtonOnEnable: function(next) {
    let panel = document.getElementById("servicesInstall-notification");
    ensureEventFired(PopupNotifications.panel, "popupshown").then(() => {
      info("servicesInstall-notification panel opened");
      panel.button.click();
    });

    // enable the provider now
    let activationURL = manifest2.origin + "/browser/browser/base/content/test/social/social_activate.html"
    addTab(activationURL, function(tab) {
      let doc = tab.linkedBrowser.contentDocument;
      let data = {
        origin: doc.nodePrincipal.origin,
        url: doc.location.href,
        manifest: manifest2,
        window: window
      }

      Social.installProvider(data, function(addonManifest) {
        SocialService.enableProvider(manifest2.origin, function(provider) {
          is(provider.origin, manifest2.origin, "provider is installed");
          let id = SocialStatus._toolbarHelper.idFromOrigin(manifest2.origin);
          let widget = CustomizableUI.getWidget(id).forWindow(window);
          ok(widget.node, "button added to widget set");
          checkSocialUI(window);
          gBrowser.removeTab(tab);
          next();
        });
      });
    });
  },
  testStatusPanel: function(next) {
    let icon = {
      name: "testIcon",
      iconURL: "chrome://browser/skin/Info.png",
      counter: 1
    };

    // Disable the transition
    let panel = document.getElementById("social-notification-panel");
    panel.setAttribute("animate", "false");

    // click on panel to open and wait for visibility
    let provider = Social._getProviderFromOrigin(manifest2.origin);
    let id = SocialStatus._toolbarHelper.idFromOrigin(manifest2.origin);
    let widget = CustomizableUI.getWidget(id);
    let btn = widget.forWindow(window).node;
    ok(btn, "got a status button");
    let port = provider.getWorkerPort();

    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "test-init-done":
          ok(true, "test-init-done received");
          ok(provider.profile.userName, "profile was set by test worker");
          btn.click();
          break;
        case "got-social-panel-visibility":
          ok(true, "got the panel message " + e.data.result);
          if (e.data.result == "shown") {
            panel.hidePopup();
            panel.removeAttribute("animate");
          } else {
            port.postMessage({topic: "test-ambient-notification", data: icon});
            port.close();
            waitForCondition(function() { return btn.getAttribute("badge"); },
                       function() {
                         is(btn.style.listStyleImage, "url(\"" + icon.iconURL + "\")", "notification icon updated");
                         next();
                       }, "button updated by notification");
          }
          break;
      }
    };
    port.postMessage({topic: "test-init"});
  },

  testPanelOffline: function(next) {
    // click on panel to open and wait for visibility
    let provider = Social._getProviderFromOrigin(manifest2.origin);
    ok(provider.enabled, "provider is enabled");
    let id = SocialStatus._toolbarHelper.idFromOrigin(manifest2.origin);
    let widget = CustomizableUI.getWidget(id);
    let btn = widget.forWindow(window).node;
    ok(btn, "got a status button");
    let frameId = btn.getAttribute("notificationFrameId");
    let frame = document.getElementById(frameId);
    let port = provider.getWorkerPort();
    port.postMessage({topic: "test-init"});

    goOffline().then(function() {
      info("testing offline error page");
      // wait for popupshown
      let panel = document.getElementById("social-notification-panel");
      ensureEventFired(panel, "popupshown").then(() => {
        ensureFrameLoaded(frame).then(() => {
          is(frame.contentDocument.documentURI.indexOf("about:socialerror?mode=tryAgainOnly"), 0, "social error page is showing "+frame.contentDocument.documentURI);
          panel.hidePopup();
          goOnline().then(next);
        });
      });
      // reload after going offline, wait for unload to open panel
      ensureEventFired(frame, "unload").then(() => {
        btn.click();
      });
      frame.contentDocument.location.reload();
    });
  },

  testButtonOnDisable: function(next) {
    // enable the provider now
    let provider = Social._getProviderFromOrigin(manifest2.origin);
    ok(provider, "provider is installed");
    SocialService.disableProvider(manifest2.origin, function() {
      let id = SocialStatus._toolbarHelper.idFromOrigin(manifest2.origin);
      waitForCondition(function() { return !document.getElementById(id) },
                       function() {
                        Social.uninstallProvider(manifest2.origin, next);
                       }, "button does not exist after disabling the provider");
    });
  }
}
