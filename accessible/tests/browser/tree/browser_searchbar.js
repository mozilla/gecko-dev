"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

const { CustomizableUITestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/CustomizableUITestUtils.sys.mjs"
);
let gCUITestUtils = new CustomizableUITestUtils(window);

// eslint-disable-next-line camelcase
add_task(async function test_searchbar_a11y_tree() {
  let searchbar = await gCUITestUtils.addSearchBar();

  // Make sure the popup has been rendered so it shows up in the a11y tree.
  let popup = document.getElementById("PopupSearchAutoComplete");
  let promise = Promise.all([
    BrowserTestUtils.waitForEvent(popup, "popupshown", false),
    waitForEvent(EVENT_SHOW, popup),
  ]);
  searchbar.textbox.openPopup();
  await promise;

  let TREE = {
    role: ROLE_EDITCOMBOBOX,

    children: [
      // image button toggling the results list
      {
        role: ROLE_BUTTONMENU,
        children: [],
      },

      // input element
      {
        role: ROLE_ENTRY,
        children: [],
      },

      // context menu
      {
        role: ROLE_COMBOBOX_LIST,
        children: [],
      },

      // result list
      {
        role: ROLE_GROUPING,
        // not testing the structure inside the result list
      },
    ],
  };

  testAccessibleTree(searchbar, TREE);

  promise = Promise.all([
    BrowserTestUtils.waitForEvent(popup, "popuphidden", false),
    waitForEvent(EVENT_HIDE, popup),
  ]);
  searchbar.textbox.closePopup();
  await promise;

  TREE = {
    role: ROLE_EDITCOMBOBOX,

    children: [
      // image button toggling the results list
      {
        role: ROLE_BUTTONMENU,
        children: [],
      },

      // input element
      {
        role: ROLE_ENTRY,
        children: [],
      },

      // context menu
      {
        role: ROLE_COMBOBOX_LIST,
        children: [],
      },

      // the result list should be removed from the tree on popuphidden
    ],
  };

  testAccessibleTree(searchbar, TREE);
});
