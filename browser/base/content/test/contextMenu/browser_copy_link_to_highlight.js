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
    testPage: loremIpsumTestPage(false),
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

// Menu items should not be visible if selection is in a contenteditable.
add_task(async function notVisibleInEditable() {
  await testCopyLinkToHighlight({
    testPage: editableTestPage(),
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
    testPage: loremIpsumTestPage(true),
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
    testPage: loremIpsumTestPage(true),
    runTests: async ({ copyLinkToHighlight }) => {
      await SimpleTest.promiseClipboardChange(
        "https://www.example.com/?stripParam=1234#:~:text=eiusmod%20tempor%20incididunt&text=labore",
        async () => {
          await BrowserTestUtils.waitForCondition(
            () =>
              !copyLinkToHighlight.hasAttribute("disabled") ||
              copyLinkToHighlight.getAttribute("disabled") === "false",
            "Waiting for copyLinkToHighlight to become enabled"
          );
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
    testPage: loremIpsumTestPage(true),
    runTests: async ({ copyCleanLinkToHighlight }) => {
      await SimpleTest.promiseClipboardChange(
        "https://www.example.com/#:~:text=eiusmod%20tempor%20incididunt&text=labore",
        async () => {
          await BrowserTestUtils.waitForCondition(
            () =>
              !copyCleanLinkToHighlight.hasAttribute("disabled") ||
              copyCleanLinkToHighlight.getAttribute("disabled") === "false",
            "Waiting for copyLinkToHighlight to become enabled"
          );
          copyCleanLinkToHighlight
            .closest("menupopup")
            .activateItem(copyCleanLinkToHighlight);
        }
      );
    },
  });
});

/**
 * Tests the "Remove all Highlights" context menu item.
 *
 * This test checks that the menu item is present and enabled if there is a
 * text fragment in the URL.
 * It also checks that after removing the highlights the URL in the URL bar
 * does not contain the text fragment anymore. In this test, there is no fragment
 * in the URL, so the URL must not have a hash (not even an empty one), because
 * this would trigger a hashchange event and the page would scroll to the top.
 */
add_task(async function removesAllHighlightsWithEmptyFragment() {
  await BrowserTestUtils.withNewTab(
    "https://www.example.com/",
    async function (browser) {
      await loremIpsumTestPage(false)(browser);
      await SpecialPowers.spawn(browser, [], async function () {
        content.location.hash = ":~:text=lorem";
      });

      is(
        gURLBar.value,
        "www.example.com/#:~:text=lorem",
        "URL bar does contain a hash after adding a text fragment"
      );
      let contextMenu = document.getElementById("contentAreaContextMenu");

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

      let removeAllHighlights = contextMenu.querySelector(
        "#context-remove-all-highlights"
      );
      ok(removeAllHighlights, '"Remove all Highlights" menu item is present');
      ok(
        BrowserTestUtils.isVisible(removeAllHighlights),
        '"Remove all Highlights" menu item is visible'
      );
      let awaitLocationChange = BrowserTestUtils.waitForLocationChange(
        gBrowser,
        "https://www.example.com/"
      );
      removeAllHighlights
        .closest("menupopup")
        .activateItem(removeAllHighlights);

      await awaitPopupHidden;
      await awaitLocationChange;

      is(
        gURLBar.value,
        "www.example.com",
        "The URL does not contain a text fragment anymore, and also no fragment (not even an empty one)"
      );
    }
  );
});

/**
 * Tests the "Remove all Highlights" context menu item for a page which has both
 * a fragment and a text fragment in the URL. After removing the highlights,
 * the text fragment should be removed from the URL, but the fragment must still
 * be there.
 */
add_task(async function removesAllHighlightsWithNonEmptyFragment() {
  await BrowserTestUtils.withNewTab(
    "https://www.example.com/",
    async function (browser) {
      await loremIpsumTestPage(false)(browser);
      await SpecialPowers.spawn(browser, [], async function () {
        content.location.hash = "foo:~:text=lorem";
      });

      is(
        gURLBar.value,
        "www.example.com/#foo:~:text=lorem",
        "URL bar does contain a fragment and a text fragment"
      );
      let contextMenu = document.getElementById("contentAreaContextMenu");
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

      let removeAllHighlights = contextMenu.querySelector(
        "#context-remove-all-highlights"
      );
      ok(removeAllHighlights, '"Remove all Highlights" menu item is present');
      ok(
        BrowserTestUtils.isVisible(removeAllHighlights),
        '"Remove all Highlights" menu item is visible'
      );
      let awaitLocationChange = BrowserTestUtils.waitForLocationChange(
        gBrowser,
        "https://www.example.com/#foo"
      );
      removeAllHighlights
        .closest("menupopup")
        .activateItem(removeAllHighlights);

      await awaitPopupHidden;
      await awaitLocationChange;

      is(
        gURLBar.value,
        "www.example.com/#foo",
        "Text Fragment is removed from the URL, fragment is still there"
      );
    }
  );
});

/**
 * Creates a document which contains a contenteditable element with some content.
 * Additionally selects the editable content.
 *
 * @returns Returns an async function which creates the content.
 */
function editableTestPage() {
  return async function (browser) {
    await SpecialPowers.spawn(browser, [], async function () {
      const editable = content.document.createElement("div");
      editable.contentEditable = true;
      editable.textContent = "This is editable";
      const range = content.document.createRange();
      range.selectNodeContents(editable);
      content.getSelection().addRange(range);
    });
  };
}

/**
 * Provides an async function that creates a document with some text nodes,
 * and (if `isTextSelected == true`) also creates some selection ranges.
 *
 * @param {boolean} isTextSelected If true, two ranges are created in the
 *                                 document and added to the selection.
 * @returns Async function which creates the content.
 */
function loremIpsumTestPage(isTextSelected) {
  return async function (browser) {
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
          const range2 = content.document.createRange();
          range2.setStart(textEnd, 4);
          range2.setEnd(textEnd, 10);
          selection.addRange(range2);
        }
      }
    );
  };
}

/**
 * Opens a new tab with `testPage` as content,
 * opens the context menu and checks if "Copy Link to Highlight" items are
 * visible, enabled, and functioning as expected.
 *
 * @param {Function} testPage - Content of the test page to load.
 * @param {Function} runTests - Async callback function for running assertions,
 * receives references to both menu items
 */
async function testCopyLinkToHighlight({ testPage, runTests }) {
  await BrowserTestUtils.withNewTab(
    "www.example.com?stripParam=1234",
    async function (browser) {
      // Add some text to the page, optionally select some of it
      await testPage(browser);

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
