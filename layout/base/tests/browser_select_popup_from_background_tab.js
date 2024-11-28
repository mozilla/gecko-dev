/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/base/content/test/forms/head.js",
  this
);

const PAGECONTENT =
  "<html><body>" +
  "<select id=select><option>foo foo</option><option>bar bar</option><option>baz baz</option></select><input>" +
  "</body></html>";

let selectPopup;
async function test_clicking_select_window_open(aIsPopup) {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "data:text/html," + escape(PAGECONTENT),
    },
    async browser => {
      let listener = () => {
        Assert.ok(false, "popup should not be shown at all");
      };
      selectPopup.addEventListener("popupshown", listener);

      let openedPromise = aIsPopup
        ? BrowserTestUtils.waitForNewWindow({
            url: "https://example.org/",
          })
        : BrowserTestUtils.waitForNewTab(
            gBrowser,
            "https://example.org/",
            true
          );

      await SpecialPowers.spawn(browser, [aIsPopup], async function (isPopup) {
        let select = content.document.querySelector("select");
        select.addEventListener("mousedown", () => {
          content.window.open(
            "https://example.org/",
            "",
            isPopup ? "popup" : ""
          );
        });
      });
      BrowserTestUtils.synthesizeMouseAtCenter("select", {}, browser);

      let newTabOrPopup = await openedPromise;
      Assert.equal(selectPopup.state, "closed", "popup should not appear");

      selectPopup.removeEventListener("popupshown", listener);
      await (aIsPopup
        ? BrowserTestUtils.closeWindow(newTabOrPopup)
        : BrowserTestUtils.removeTab(newTabOrPopup));
    }
  );
}

// The select dropdown is created lazily, so initial it first.
add_setup(async function init_select_popup() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "data:text/html," + escape(PAGECONTENT),
    },
    async browser => {
      selectPopup = await openSelectPopup("click");
      await hideSelectPopup();
    }
  );
});

// Test for bug 1909535.
add_task(async function test_clicking_select_opens_new_tab() {
  await test_clicking_select_window_open(false);
});

add_task(async function test_clicking_select_opens_new_window() {
  await test_clicking_select_window_open(true);
});
