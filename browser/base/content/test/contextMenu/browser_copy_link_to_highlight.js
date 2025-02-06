/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let listService;

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.query_stripping.strip_list", "stripParam"],
      ["privacy.query_stripping.strip_on_share.enabled", true],
      ["privacy.query_stripping.strip_on_share.canDisable", false],
      ["dom.text_fragments.create_text_fragment.enabled", true],
    ],
  });

  // Get the list service so we can wait for it to be fully initialized before running tests.
  listService = Cc["@mozilla.org/query-stripping-list-service;1"].getService(
    Ci.nsIURLQueryStrippingListService
  );

  await listService.testWaitForInit();
});

/*
  Tests for the "copy link to highlight" options in the browser content context menu
*/

// Menu items should not be visible if no text is selected
add_task(async function notVisibleIfNoSelection() {
  await testCopyLinkToHighlight({
    isTextSelected: false,
    runTests: async ({ copyLinkToHighlight, copyCleanLinkToHighlight }) => {
      Assert.ok(
        !BrowserTestUtils.isVisible(copyLinkToHighlight),
        "Copy Link to Highlight Menu item is not visible"
      );
      Assert.ok(
        !BrowserTestUtils.isVisible(copyCleanLinkToHighlight),
        "Copy Clean Link to Highlight Menu item is not visible"
      );
    },
  });
});

// Menu items should be visible and not disabled if text is selected
add_task(async function isVisibleIfSelection() {
  await testCopyLinkToHighlight({
    isTextSelected: true,
    runTests: async ({ copyLinkToHighlight, copyCleanLinkToHighlight }) => {
      // tests for visibility
      Assert.ok(
        BrowserTestUtils.isVisible(copyLinkToHighlight),
        "Copy Link to Highlight Menu item is visible"
      );
      Assert.ok(
        BrowserTestUtils.isVisible(copyCleanLinkToHighlight),
        "Copy Clean Link to Highlight Menu item is visible"
      );

      // tests for enabled menu items
      Assert.ok(
        !copyLinkToHighlight.hasAttribute("disabled") ||
          copyLinkToHighlight.getAttribute("disabled") === "false",
        "Copy Link to Highlight Menu item is enabled"
      );
    },
  });
});

// Clicking "Copy Link to Highlight" copies the URL with text fragment to the clipboard
add_task(async function copiesToClipboard() {
  await testCopyLinkToHighlight({
    isTextSelected: true,
    runTests: async ({ copyLinkToHighlight }) => {
      await SimpleTest.promiseClipboardChange(
        "https://www.example.com/?stripParam=1234#:~:text=eiusmod%20tempor%20incididunt",
        () => {
          copyLinkToHighlight
            .closest("menupopup")
            .activateItem(copyLinkToHighlight);
        }
      );
    },
  });
});

// Clicking "Copy Clean Link to Highlight" copies the URL with text fragment and without tracking query params to the clipboard
add_task(async function copiesToClipboard() {
  await testCopyLinkToHighlight({
    isTextSelected: true,
    runTests: async ({ copyCleanLinkToHighlight }) => {
      await SimpleTest.promiseClipboardChange(
        "https://www.example.com/#:~:text=eiusmod%20tempor%20incididunt",
        () => {
          copyCleanLinkToHighlight
            .closest("menupopup")
            .activateItem(copyCleanLinkToHighlight);
        }
      );
    },
  });
});

/**
 * Opens a new tab with lorem ipsum text, optionally selects some of the text,
 * opens the context menu and checks if "Copy Link to Highlight" items are
 * visible, enabled, and functioning as expected.
 *
 * @param {boolean} isTextSelected - Whether or not any text on the page is selected
 * @param {Function} runTests - Async callback function for running assertions,
 * receives references to both menu items
 */
async function testCopyLinkToHighlight({ isTextSelected, runTests }) {
  await BrowserTestUtils.withNewTab(
    "www.example.com?stripParam=1234",
    async function (browser) {
      // Add some text to the page, optionally select some of it
      await SpecialPowers.spawn(
        browser,
        [isTextSelected],
        async function (selectText) {
          const textBegin = content.document.createTextNode(
            "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
          );
          const textMiddle = content.document.createTextNode(
            "eiusmod tempor incididunt"
          );
          const textEnd = content.document.createTextNode(
            " ut labore et dolore magna aliqua. Est nulla nostrud velit dolore aliquip ipsum do sint cillum excepteur adipisicing ipsum irure. Sit sunt reprehenderit laboris labore magna exercitation amet fugiat nisi ad laborum veniam nisi. Est ex proident anim eiusmod veniam ipsum officia in ipsum deserunt voluptate. Enim anim cillum elit tempor consequat esse exercitation."
          );

          const paragraph = content.document.createElement("p");
          const span = content.document.createElement("span");
          span.appendChild(textMiddle);
          span.id = "span";

          paragraph.appendChild(textBegin);
          paragraph.appendChild(span);
          paragraph.appendChild(textEnd);

          paragraph.id = "text";
          content.document.body.prepend(paragraph);

          if (selectText) {
            const selection = content.getSelection();
            const range = content.document.createRange();
            range.selectNodeContents(span);
            selection.addRange(range);
          }
        }
      );

      let contextMenu = document.getElementById("contentAreaContextMenu");
      // Open the context menu
      let awaitPopupShown = BrowserTestUtils.waitForEvent(
        contextMenu,
        "popupshown"
      );
      await BrowserTestUtils.synthesizeMouseAtCenter(
        "#text",
        { type: "contextmenu", button: 2 },
        browser
      );
      await awaitPopupShown;
      let awaitPopupHidden = BrowserTestUtils.waitForEvent(
        contextMenu,
        "popuphidden"
      );

      // Run some tests with each menu item
      let copyLinkToHighlight = contextMenu.querySelector(
        "#context-copy-link-to-highlight"
      );
      let copyCleanLinkToHighlight = contextMenu.querySelector(
        "#context-copy-clean-link-to-highlight"
      );

      await runTests({ copyLinkToHighlight, copyCleanLinkToHighlight });

      contextMenu.hidePopup();
      await awaitPopupHidden;
    }
  );
}
