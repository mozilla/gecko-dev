"use strict";

// Test that the customization menu is rendered.
test_newtab({
  async before() {
    gBrowser.selectedBrowser.focus();
  },
  test: async function test_open_customizeMenu() {
    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelector(".personalize-button"),
      "Wait for personalize button to load on the newtab page"
    );

    let defaultPos = "matrix(1, 0, 0, 1, 0, 0)";
    Assert.notStrictEqual(
      content.getComputedStyle(
        content.document.querySelector(".customize-menu")
      ).transform,
      defaultPos,
      "Customize Menu should be rendered, but not visible"
    );

    let customizeButton = content.document.querySelector(".personalize-button");
    customizeButton.focus();
    await EventUtils.synthesizeKey("KEY_Enter", {}, content.window);

    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelector(".customize-animate-enter-done"),
      "Customize Menu should be rendered now"
    );
  },
});
