/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check that chat sidebar renders
 */
add_task(async function test_sidebar_render() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.chat.provider", "http://mochi.test:8888"]],
  });

  await SidebarController.show("viewGenaiChatSidebar");

  const provider =
    SidebarController.browser.contentWindow.document.getElementById("provider");
  Assert.ok(provider, "Rendered provider select");

  SidebarController.hide();
});

/**
 * Check that chat sidebar renders providers
 */
add_task(async function test_sidebar_providers() {
  const countVisible = () =>
    [
      ...SidebarController.browser.contentWindow.document.getElementById(
        "provider"
      ).options,
    ].filter(option => !option.hidden).length;

  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.chat.provider", ""]],
  });
  await SidebarController.show("viewGenaiChatSidebar");

  const origCount = countVisible();
  Assert.ok(origCount, "Rendered provider options");

  SidebarController.hide();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.chat.hideLocalhost", false]],
  });
  await SidebarController.show("viewGenaiChatSidebar");

  Assert.equal(countVisible(), origCount + 1, "Added localhost option");

  SidebarController.hide();
});
