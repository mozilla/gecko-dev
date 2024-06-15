/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check that chat sidebar renders
 */
add_task(async function test_sidebar_render() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.ml.chat.enabled", true],
      ["browser.ml.chat.provider", "http://mochi.test:8888"],
    ],
  });

  await SidebarController.show("viewGenaiChatSidebar");

  const provider =
    SidebarController.browser.contentWindow.document.getElementById("provider");
  Assert.ok(provider, "Rendered provider select");

  SidebarController.hide();
});
