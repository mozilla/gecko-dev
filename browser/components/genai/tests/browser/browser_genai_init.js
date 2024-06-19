/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check that chat sidebar auto opens
 */
add_task(async function test_chat_autoopen() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.ml.chat.openSidebarOnProviderChange", true],
      ["browser.ml.chat.provider", "http://localhost:8080"],
    ],
  });

  Assert.ok(SidebarController.isOpen, "Pref change opened sidebar");
  SidebarController.hide();
});

/**
 * Check that chat sidebar doesn't open if disabled
 */
add_task(async function test_chat_no_open() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.ml.chat.openSidebarOnProviderChange", false],
      ["browser.ml.chat.provider", "http://localhost:8080"],
    ],
  });

  Assert.ok(!SidebarController.isOpen, "Pref changes didn't open sidebar");
});
