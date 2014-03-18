/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  let manifest = { // normal provider
    name: "provider 1",
    origin: "chrome://mochitests/content/browser/browser/base/content/test/social/",
    sidebarURL: "chrome://mochitests/content/browser/browser/base/content/test/social/social_sidebar_chrome.html",
    iconURL: "chrome://mochitests/content/browser/browser/base/content/test/general/moz.png"
  };
  runSocialTestWithProvider(manifest, doTest);
}

function doTest(finishcb) {
  // Waiting for the observer ensures that the sidebar has chrome access.
  let observer = {
    observe: function(aSubject, aTopic, aData) {
      if (aTopic === "sidebar-loaded") {
        // Check the text is present
        let sidebar = document.getElementById("social-sidebar-box");
        let browser = sidebar.lastChild;

        is(browser.contentDocument.getElementsByTagName("p")[0].textContent,
           "This is a test social sidebar.");

        // Now try opening the chat window
        browser.contentDocument.getElementById("openchat").click();

        Services.obs.removeObserver(observer, "sidebar-loaded");
      } else if (aTopic === "chat-loaded") {
        // Check the text is present
        let chat = document.getElementById("pinnedchats").lastChild;

        is(chat.contentDocument.getElementsByTagName("p")[0].textContent,
           "This is a test social chrome chat window.");

        // Finish
        Services.obs.removeObserver(observer, "chat-loaded", false);
        finishcb();
      }
    }
  };

  Services.obs.addObserver(observer, "sidebar-loaded", false);
  Services.obs.addObserver(observer, "chat-loaded", false);
}
