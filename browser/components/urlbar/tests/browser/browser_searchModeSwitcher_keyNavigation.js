/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.defineESModuleGetters(this, {
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.scotchBonnet.enableOverride", true]],
  });
});

add_task(
  async function test_focus_by_tab_with_no_selected_element_with_urlbar_focused_by_key() {
    for (const shiftKey of [false, true]) {
      info(`Test for shifrKey:${shiftKey}`);

      info("Focus on urlbar by key");
      await focusOnURLbar(() => {
        EventUtils.synthesizeKey("l", { accelKey: true });
      });
      Assert.ok(!gURLBar.view.selectedElement);

      let ok = false;
      for (let i = 0; i < 10; i++) {
        EventUtils.synthesizeKey("KEY_Tab", { shiftKey });

        ok =
          document.activeElement.id != "urlbar-input" &&
          document.activeElement.id != "urlbar-searchmode-switcher";
        if (ok) {
          break;
        }
      }

      Assert.ok(ok, "Focus was moved to a component other than the urlbar");
      Assert.ok(!gURLBar.view.isOpen);
    }
  }
);

add_task(
  async function test_focus_by_tab_with_no_selected_element_with_urlbar_focused_by_click() {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.urlbar.suggest.topsites", false]],
    });

    let results = [
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.HISTORY,
        {
          url: "https://mozilla.org/a",
        }
      ),
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.HISTORY,
        {
          url: "https://mozilla.org/b",
        }
      ),
    ];

    let provider = new UrlbarTestUtils.TestProvider({ results, priority: 1 });
    UrlbarProvidersManager.registerProvider(provider);

    const FOCUS_ORDER_ASSERTIONS = [
      () =>
        Assert.equal(
          gURLBar.view.selectedElement,
          gURLBar.view.getFirstSelectableElement()
        ),
      () =>
        Assert.equal(
          gURLBar.view.selectedElement,
          gURLBar.view.getLastSelectableElement()
        ),
      () =>
        Assert.equal(
          document.activeElement,
          document.getElementById("urlbar-searchmode-switcher")
        ),
    ];

    for (const shiftKey of [true, false]) {
      info("Focus on urlbar by click");
      await focusOnURLbar(() => {
        EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
      });
      Assert.ok(!gURLBar.view.selectedElement);

      await BrowserTestUtils.waitForCondition(async () => {
        if (UrlbarTestUtils.getResultCount(window) != 2) {
          return false;
        }
        let { result } = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
        return result.providerName == provider.name;
      });
      Assert.ok(true, "This test needs exact 2 results");

      for (const assert of shiftKey
        ? [...FOCUS_ORDER_ASSERTIONS].reverse()
        : FOCUS_ORDER_ASSERTIONS) {
        EventUtils.synthesizeKey("KEY_Tab", { shiftKey });
        assert();
      }

      Assert.ok(gURLBar.view.isOpen);
      gURLBar.view.close();
      gURLBar.handleRevert();
    }

    UrlbarProvidersManager.unregisterProvider(provider);
    await SpecialPowers.popPrefEnv();
  }
);

async function focusOnURLbar(focus) {
  // We intentionally turn off this a11y check, because the following click is
  // purposefully targeting a non-interactive element.
  AccessibilityUtils.setEnv({ mustHaveAccessibleRule: false });
  EventUtils.synthesizeMouseAtCenter(document.getElementById("browser"), {});
  AccessibilityUtils.resetEnv();
  await BrowserTestUtils.waitForCondition(() =>
    document.activeElement.closest("#browser")
  );

  focus();

  await BrowserTestUtils.waitForCondition(
    () => document.activeElement.id == "urlbar-input" && gURLBar.view.isOpen,
    "Wait for urlbar gets focus"
  );
}

/**
 * Test we can open the SearchModeSwitcher with various keys
 *
 * @param {string} openKey - The keyboard character used to open the popup.
 */
async function test_open_switcher(openKey) {
  let popup = UrlbarTestUtils.searchModeSwitcherPopup(window);
  let promiseMenuOpen = BrowserTestUtils.waitForEvent(popup, "popupshown");

  info(`Open the urlbar and open the switcher via keyboard (${openKey})`);
  await focusSwitcher();
  EventUtils.synthesizeKey(openKey);
  await promiseMenuOpen;

  EventUtils.synthesizeKey("KEY_Escape");
}

/**
 * Test that not all characters will open the SearchModeSwitcher
 *
 * @param {string} dontOpenKey - The keyboard character we will ignore.
 */
async function test_dont_open_switcher(dontOpenKey) {
  let popup = UrlbarTestUtils.searchModeSwitcherPopup(window);

  let popupOpened = false;
  let opened = () => {
    popupOpened = true;
  };
  info("Pressing key that should not open the switcher");
  popup.addEventListener("popupshown", opened);
  await focusSwitcher();
  EventUtils.synthesizeKey(dontOpenKey);

  /* eslint-disable mozilla/no-arbitrary-setTimeout */
  await new Promise(r => setTimeout(r, 50));
  Assert.ok(!popupOpened, "The popup was not opened");
  popup.removeEventListener("popupshown", opened);
}

/**
 * Test we can navigate the SearchModeSwitcher with various keys
 *
 * @param {string} navKey - The keyboard character used to navigate.
 * @param {Int} navTimes - The number of times we press that key.
 * @param {object} searchMode - The searchMode that we expect to select.
 */
async function test_navigate_switcher(navKey, navTimes, searchMode) {
  let popup = UrlbarTestUtils.searchModeSwitcherPopup(window);
  let promiseMenuOpen = BrowserTestUtils.waitForEvent(popup, "popupshown");

  info("Open the urlbar and open the switcher via Enter key");
  await focusSwitcher();
  EventUtils.synthesizeKey("KEY_Enter");
  await promiseMenuOpen;

  info("Select first result and enter search mode");
  for (let i = 0; i < navTimes; i++) {
    EventUtils.synthesizeKey(navKey);
  }
  EventUtils.synthesizeKey("KEY_Enter");
  await UrlbarTestUtils.promiseSearchComplete(window);

  await UrlbarTestUtils.assertSearchMode(window, searchMode);

  info("Exit the search mode");
  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeKey("KEY_Escape");
  });
  EventUtils.synthesizeKey("KEY_Escape");
  await UrlbarTestUtils.assertSearchMode(window, null);
}

// TODO: Don't let tests depend on the actual search config.
let amazonSearchMode = {
  engineName: "Amazon.com",
  entry: "searchbutton",
  isPreview: false,
  isGeneralPurposeEngine: true,
};
let bingSearchMode = {
  engineName: "Bing",
  isGeneralPurposeEngine: true,
  source: 3,
  isPreview: false,
  entry: "searchbutton",
};

add_task(async function test_keyboard_nav() {
  await test_open_switcher("KEY_Enter");
  await test_open_switcher("KEY_ArrowDown");
  await test_open_switcher(" ");

  await test_dont_open_switcher("a");
  await test_dont_open_switcher("KEY_ArrowUp");
  await test_dont_open_switcher("x");

  await test_navigate_switcher("KEY_Tab", 1, amazonSearchMode);
  await test_navigate_switcher("KEY_ArrowDown", 1, amazonSearchMode);
  await test_navigate_switcher("KEY_Tab", 2, bingSearchMode);
  await test_navigate_switcher("KEY_ArrowDown", 2, bingSearchMode);
});

add_task(async function test_focus_on_switcher_by_tab() {
  const input = "abc";
  info(`Open urlbar view with query [${input}]`);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: input,
  });

  info("Focus on Unified Search Button by tab");
  EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
  await TestUtils.waitForCondition(
    () => document.activeElement.id == "urlbar-searchmode-switcher"
  );
  Assert.ok(true, "Unified Search Button gets the focus");

  let popup = UrlbarTestUtils.searchModeSwitcherPopup(window);
  Assert.equal(popup.state, "closed", "Switcher popup should not be opened");
  Assert.ok(gURLBar.view.isOpen, "Urlbar view panel has been opening");
  Assert.equal(gURLBar.value, input, "Inputted value still be on urlbar");

  info("Open the switcher popup by key");
  let promiseMenuOpen = BrowserTestUtils.waitForEvent(popup, "popupshown");
  EventUtils.synthesizeKey("KEY_Enter");
  await promiseMenuOpen;
  Assert.notEqual(
    document.activeElement.id,
    "urlbar-searchmode-switcher",
    "Dedicated Search button loses the focus"
  );
  Assert.equal(
    gURLBar.view.panel.hasAttribute("hide-temporarily"),
    true,
    "Urlbar view panel is closed"
  );
  Assert.equal(gURLBar.value, input, "Inputted value still be on urlbar");

  info("Close the switcher popup by Escape");
  let promiseMenuClose = BrowserTestUtils.waitForEvent(popup, "popuphidden");
  EventUtils.synthesizeKey("KEY_Escape");
  await promiseMenuClose;
  Assert.equal(
    document.activeElement.id,
    "urlbar-input",
    "Urlbar gets the focus"
  );
  Assert.equal(
    gURLBar.view.panel.hasAttribute("hide-temporarily"),
    false,
    "Urlbar view panel is opened"
  );
  Assert.equal(gURLBar.value, input, "Inputted value still be on urlbar");
});

add_task(async function test_focus_order_by_tab() {
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: "https://example.com/",
    title: "abc",
  });

  const FOCUS_ORDER_ASSERTIONS = [
    () =>
      Assert.equal(
        gURLBar.view.selectedElement,
        gURLBar.view.getLastSelectableElement()
      ),
    () =>
      Assert.equal(
        document.activeElement,
        document.getElementById("urlbar-searchmode-switcher")
      ),
    () =>
      Assert.equal(
        gURLBar.view.selectedElement,
        gURLBar.view.getFirstSelectableElement()
      ),
    () =>
      Assert.equal(
        gURLBar.view.selectedElement,
        gURLBar.view.getLastSelectableElement()
      ),
    () =>
      Assert.equal(
        document.activeElement,
        document.getElementById("urlbar-searchmode-switcher")
      ),
  ];

  for (const shiftKey of [false, true]) {
    info("Open urlbar view");
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "abc",
    });
    Assert.equal(document.activeElement, gURLBar.inputField);
    Assert.equal(
      gURLBar.view.selectedElement,
      gURLBar.view.getFirstSelectableElement()
    );

    await BrowserTestUtils.waitForCondition(
      () => UrlbarTestUtils.getResultCount(window) == 2
    );
    Assert.ok(true, "This test needs exact 2 results");

    for (const assert of shiftKey
      ? [...FOCUS_ORDER_ASSERTIONS].reverse()
      : FOCUS_ORDER_ASSERTIONS) {
      EventUtils.synthesizeKey("KEY_Tab", { shiftKey });
      assert();
    }

    gURLBar.handleRevert();
  }

  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function test_focus_order_by_tab_with_no_results() {
  for (const scotchBonnetEnabled of [true, false]) {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.urlbar.scotchBonnet.enableOverride", scotchBonnetEnabled],
      ],
    });

    await test_focus_order_with_no_results({ input: "", shiftKey: false });
    await test_focus_order_with_no_results({ input: "", shiftKey: true });
    await test_focus_order_with_no_results({ input: "test", shiftKey: false });
    await test_focus_order_with_no_results({ input: "test", shiftKey: true });

    await SpecialPowers.popPrefEnv();
  }
});

async function test_focus_order_with_no_results({ input, shiftKey }) {
  const scotchBonnetEnabled = UrlbarPrefs.get("scotchBonnet.enableOverride");
  info(`Test for ${JSON.stringify({ scotchBonnetEnabled, input, shiftKey })}`);

  info("Open urlbar results");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });

  info("Enter Tabs mode");
  const keywordToEnterTabsMode = scotchBonnetEnabled ? "@tabs " : "% ";
  keywordToEnterTabsMode.split("").forEach(c => EventUtils.synthesizeKey(c));
  await BrowserTestUtils.waitForCondition(
    () => UrlbarTestUtils.getResultCount(window) == 0,
    "Wait until updating the results"
  );
  Assert.equal(document.activeElement.id, "urlbar-input");

  info("Enter extra value");
  input.split("").forEach(c => EventUtils.synthesizeKey(c));

  let ok = false;
  for (let i = 0; i < 10; i++) {
    EventUtils.synthesizeKey("KEY_Tab", { shiftKey });

    ok =
      document.activeElement.id != "urlbar-input" &&
      document.activeElement.id != "urlbar-searchmode-switcher";
    if (ok) {
      break;
    }
  }
  Assert.ok(ok, "Focus was moved to a component other than the urlbar");

  info("Clean up");
  gURLBar.searchMode = null;
}

add_task(async function test_focus_order_by_tab_with_no_selected_element() {
  for (const shiftKey of [false, true]) {
    info(`Test for shiftKey:${shiftKey}`);

    info("Open urlbar results");
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "",
    });
    Assert.equal(document.activeElement.id, "urlbar-input");
    Assert.ok(gURLBar.view.isOpen);
    Assert.ok(!gURLBar.view.selectedElement);

    let ok = false;
    for (let i = 0; i < 10; i++) {
      EventUtils.synthesizeKey("KEY_Tab", { shiftKey });

      ok =
        document.activeElement.id != "urlbar-input" &&
        document.activeElement.id != "urlbar-searchmode-switcher";
      if (ok) {
        break;
      }
    }
    Assert.ok(ok, "Focus was moved to a component other than the urlbar");
  }
});

add_task(async function test_urlbar_focus_after_switcher_lost() {
  info("Open the urlbar");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "abc",
  });
  Assert.ok(
    BrowserTestUtils.isVisible(gURLBar.view.panel),
    "The UrlbarView is opened"
  );
  Assert.ok(
    gURLBar.hasAttribute("focused"),
    "The #urlbar element has 'focused' attribute"
  );

  info("Move the focus to the switcher button");
  await focusSwitcher();

  info("Move the focus to browser element");
  // We intentionally turn off this a11y check, because the following click is
  // purposefully targeting a non-interactive element.
  AccessibilityUtils.setEnv({ mustHaveAccessibleRule: false });
  EventUtils.synthesizeMouseAtCenter(document.getElementById("browser"), {});
  AccessibilityUtils.resetEnv();
  Assert.ok(
    !gURLBar.hasAttribute("focused"),
    "The #urlbar element does not have 'focused' attribute"
  );

  info("Clean up");
  gURLBar.handleRevert();
});

add_task(async function test_esc_on_UnifiedSearchButton() {
  info("Open urlbar results");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "abc",
  });
  Assert.equal(document.activeElement.id, "urlbar-input");

  info("Focus on Unified Search Button by tab");
  EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
  await TestUtils.waitForCondition(
    () => document.activeElement.id == "urlbar-searchmode-switcher"
  );
  Assert.ok(true, "Unified Search Button gets the focus");

  info("Press ESC key");
  EventUtils.synthesizeKey("KEY_Escape");
  await TestUtils.waitForCondition(
    () => document.activeElement.id == "urlbar-input"
  );
  Assert.equal(
    gURLBar.view.isOpen,
    false,
    "The urlbar result view should be closed"
  );

  gURLBar.handleRevert();
});

add_task(async function test_ctrl_tab() {
  info("Prepare multiple tabs");
  let mainTab = gBrowser.selectedTab;
  let newTab1 = BrowserTestUtils.addTab(gBrowser);
  let newTab2 = BrowserTestUtils.addTab(gBrowser);

  info("Open urlbar results");
  const query = "abc";
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: query,
  });

  const testData = [
    {
      shiftKey: false,
      expectedOrder: [newTab1, newTab2, mainTab],
    },
    {
      shiftKey: true,
      expectedOrder: [newTab2, newTab1, mainTab],
    },
  ];

  for (let { shiftKey, expectedOrder } of testData) {
    for (let nextTab of expectedOrder) {
      EventUtils.synthesizeKey("KEY_Tab", { ctrlKey: true, shiftKey });
      await BrowserTestUtils.waitForCondition(
        () => gBrowser.selectedTab == nextTab
      );
      Assert.ok(true, "Expected tab is selected");
      let expectedInput = nextTab == mainTab ? query : "";
      Assert.equal(gURLBar.value, expectedInput, "Urlbar value is correct");
      Assert.equal(
        gURLBar.view.isOpen,
        !!expectedInput,
        "Urlbar view is opened as expected"
      );
    }
  }

  gBrowser.removeTab(newTab1);
  gBrowser.removeTab(newTab2);
  gURLBar.view.close();
  gURLBar.handleRevert();
});
