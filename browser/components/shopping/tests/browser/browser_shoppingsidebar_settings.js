/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the settings component is rendered as expected when
 * `browser.shopping.experience2023.autoOpen.enabled` is false.
 */
add_task(async function test_shopping_settings_experiment_auto_open_disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.autoOpen.enabled", false],
      ["browser.shopping.experience2023.active", true],
    ],
  });

  await BrowserTestUtils.withNewTab(
    {
      url: PRODUCT_TEST_URL,
      gBrowser,
    },
    async browser => {
      let sidebar = gBrowser
        .getPanel(browser)
        .querySelector("shopping-sidebar");
      await promiseSidebarUpdated(sidebar, PRODUCT_TEST_URL);

      await SpecialPowers.spawn(
        sidebar.querySelector("browser"),
        [MOCK_ANALYZED_PRODUCT_RESPONSE],
        async mockData => {
          let shoppingContainer =
            content.document.querySelector(
              "shopping-container"
            ).wrappedJSObject;

          shoppingContainer.data = Cu.cloneInto(mockData, content);
          await shoppingContainer.updateComplete;

          let shoppingSettings = shoppingContainer.settingsEl;
          ok(shoppingSettings, "Got the shopping-settings element");
          ok(
            !shoppingSettings.wrapperEl.className.includes(
              "shopping-settings-auto-open-ui-enabled"
            ),
            "Settings card should not have a special classname with autoOpen pref disabled"
          );
          is(
            shoppingSettings.shoppingCardEl?.type,
            "accordion",
            "shopping-card type should be accordion"
          );

          /* Verify control treatment UI */
          ok(
            !shoppingSettings.autoOpenToggleEl,
            "There should be no auto-open toggle"
          );
          ok(
            !shoppingSettings.autoOpenToggleDescriptionEl,
            "There should be no description for the auto-open toggle"
          );
          ok(!shoppingSettings.dividerEl, "There should be no divider");
          ok(
            !shoppingSettings.sidebarEnabledStateEl,
            "There should be no message about the sidebar active state"
          );

          ok(
            shoppingSettings.optOutButtonEl,
            "There should be an opt-out button"
          );
        }
      );
    }
  );

  await SpecialPowers.popPrefEnv();
});

/**
 * Tests that the settings component is rendered as expected when
 * `browser.shopping.experience2023.autoOpen.enabled` is true and
 * `browser.shopping.experience2023.ads.enabled is true`.
 */
add_task(
  async function test_shopping_settings_experiment_auto_open_enabled_with_ads() {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.shopping.experience2023.autoOpen.enabled", true],
        ["browser.shopping.experience2023.autoOpen.userEnabled", true],
        ["browser.shopping.experience2023.ads.enabled", true],
      ],
    });

    await BrowserTestUtils.withNewTab(
      {
        url: PRODUCT_TEST_URL,
        gBrowser,
      },
      async browser => {
        let sidebar = gBrowser
          .getPanel(browser)
          .querySelector("shopping-sidebar");
        await promiseSidebarUpdated(sidebar, PRODUCT_TEST_URL);

        await SpecialPowers.spawn(
          sidebar.querySelector("browser"),
          [MOCK_ANALYZED_PRODUCT_RESPONSE],
          async () => {
            let shoppingContainer =
              content.document.querySelector(
                "shopping-container"
              ).wrappedJSObject;

            await shoppingContainer.updateComplete;

            let shoppingSettings = shoppingContainer.settingsEl;
            ok(shoppingSettings, "Got the shopping-settings element");
            ok(
              shoppingSettings.wrapperEl.className.includes(
                "shopping-settings-auto-open-ui-enabled"
              ),
              "Settings card should have a special classname with autoOpen pref enabled"
            );

            ok(
              shoppingSettings.recommendationsToggleEl,
              "There should be an ads toggle"
            );

            /* Verify auto-open experiment UI */
            ok(
              shoppingSettings.autoOpenToggleEl,
              "There should be an auto-open toggle"
            );
            ok(
              shoppingSettings.autoOpenToggleDescriptionEl,
              "There should be a description for the auto-open toggle"
            );
            ok(shoppingSettings.dividerEl, "There should be a divider");

            ok(
              shoppingSettings.optOutButtonEl,
              "There should be an opt-out button"
            );
          }
        );
      }
    );

    await SpecialPowers.popPrefEnv();
    await SpecialPowers.popPrefEnv();
    await SpecialPowers.popPrefEnv();
  }
);

/**
 * Tests that the settings component is rendered as expected when
 * `browser.shopping.experience2023.autoOpen.enabled` is true and
 * `browser.shopping.experience2023.ads.enabled is false`.
 */
add_task(
  async function test_shopping_settings_experiment_auto_open_enabled_no_ads() {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.shopping.experience2023.autoOpen.enabled", true],
        ["browser.shopping.experience2023.autoOpen.userEnabled", true],
        ["browser.shopping.experience2023.ads.enabled", false],
      ],
    });

    await BrowserTestUtils.withNewTab(
      {
        url: PRODUCT_TEST_URL,
        gBrowser,
      },
      async browser => {
        let sidebar = gBrowser
          .getPanel(browser)
          .querySelector("shopping-sidebar");
        await promiseSidebarUpdated(sidebar, PRODUCT_TEST_URL);

        await SpecialPowers.spawn(
          sidebar.querySelector("browser"),
          [MOCK_ANALYZED_PRODUCT_RESPONSE],
          async () => {
            let shoppingContainer =
              content.document.querySelector(
                "shopping-container"
              ).wrappedJSObject;

            await shoppingContainer.updateComplete;

            let shoppingSettings = shoppingContainer.settingsEl;
            ok(shoppingSettings, "Got the shopping-settings element");
            ok(
              shoppingSettings.wrapperEl.className.includes(
                "shopping-settings-auto-open-ui-enabled"
              ),
              "Settings card should have a special classname with autoOpen pref enabled"
            );

            ok(
              !shoppingSettings.recommendationsToggleEl,
              "There should be no ads toggle"
            );

            /* Verify auto-open experiment UI */
            ok(
              shoppingSettings.autoOpenToggleEl,
              "There should be an auto-open toggle"
            );
            ok(
              shoppingSettings.autoOpenToggleDescriptionEl,
              "There should be a description for the auto-open toggle"
            );
            ok(shoppingSettings.dividerEl, "There should be a divider");

            ok(
              shoppingSettings.optOutButtonEl,
              "There should be an opt-out button"
            );
          }
        );
      }
    );

    await SpecialPowers.popPrefEnv();
    await SpecialPowers.popPrefEnv();
    await SpecialPowers.popPrefEnv();
  }
);

/**
 * Tests that auto-open toggle state and autoOpen.userEnabled pref update correctly.
 */
add_task(async function test_settings_auto_open_toggle() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.autoOpen.enabled", true],
      ["browser.shopping.experience2023.autoOpen.userEnabled", true],
    ],
  });

  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    PRODUCT_TEST_URL
  );
  let browser = tab1.linkedBrowser;

  let mockArgs = {
    mockData: MOCK_ANALYZED_PRODUCT_RESPONSE,
  };

  let sidebar = gBrowser.getPanel(browser).querySelector("shopping-sidebar");
  await promiseSidebarUpdated(sidebar, PRODUCT_TEST_URL);

  await SpecialPowers.spawn(
    sidebar.querySelector("browser"),
    [mockArgs],
    async args => {
      const { mockData } = args;
      let shoppingContainer =
        content.document.querySelector("shopping-container").wrappedJSObject;

      shoppingContainer.data = Cu.cloneInto(mockData, content);
      await shoppingContainer.updateComplete;

      let shoppingSettings = shoppingContainer.settingsEl;
      ok(shoppingSettings, "Got the shopping-settings element");

      let autoOpenToggle = shoppingSettings.autoOpenToggleEl;
      ok(autoOpenToggle, "There should be an auto-open toggle");
      ok(
        autoOpenToggle.hasAttribute("pressed"),
        "Toggle should have enabled state"
      );

      let toggleStateChangePromise = ContentTaskUtils.waitForCondition(() => {
        return !autoOpenToggle.hasAttribute("pressed");
      }, "Waiting for auto-open toggle state to be disabled");
      let autoOpenUserEnabledPromise = ContentTaskUtils.waitForEvent(
        content.document,
        "autoOpenEnabledByUserChanged"
      );
      let activePrefChange = ContentTaskUtils.waitForCondition(
        () =>
          !SpecialPowers.getBoolPref("browser.shopping.experience2023.active"),
        "Sidebar active pref should be false, but isn't"
      );

      autoOpenToggle.click();

      Promise.all([
        await toggleStateChangePromise,
        await autoOpenUserEnabledPromise,
        await activePrefChange,
      ]);

      ok(
        !SpecialPowers.getBoolPref(
          "browser.shopping.experience2023.autoOpen.userEnabled"
        ),
        "autoOpen.userEnabled pref should be false"
      );
      ok(
        SpecialPowers.getBoolPref(
          "browser.shopping.experience2023.autoOpen.enabled"
        ),
        "autoOpen.enabled pref should still be true"
      );
      ok(
        !SpecialPowers.getBoolPref("browser.shopping.experience2023.active"),
        "Sidebar active pref should be false after pressing auto-open toggle to close the sidebar"
      );

      // Now try updating the pref directly to see if toggle will change state immediately
      await SpecialPowers.popPrefEnv();
      toggleStateChangePromise = ContentTaskUtils.waitForCondition(() => {
        return autoOpenToggle.hasAttribute("pressed");
      }, "Waiting for auto-open toggle to be enabled");

      await SpecialPowers.pushPrefEnv({
        set: [
          ["browser.shopping.experience2023.autoOpen.userEnabled", true],
          ["browser.shopping.experience2023.active", true],
        ],
      });

      await toggleStateChangePromise;
    }
  );

  BrowserTestUtils.removeTab(tab1);

  await SpecialPowers.popPrefEnv();
  await SpecialPowers.popPrefEnv();
  await SpecialPowers.popPrefEnv();
});
