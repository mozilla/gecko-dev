/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let tests = [
  {
    description: "Test single click doesn't untrim",
    untrimmedValue: BrowserUIUtils.trimURLProtocol + "www.example.com/",
    execute() {
      EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
      Assert.equal(gURLBar.selectionStart, 0, "Selection start is 0.");
      Assert.equal(
        gURLBar.selectionEnd,
        gURLBar.value.length,
        "Selection end is at and of text."
      );
    },
    shouldUntrim: false,
  },
  {
    description: "Test CTRL+L untrims",
    untrimmedValue: BrowserUIUtils.trimURLProtocol + "www.example.com/",
    execute() {
      EventUtils.synthesizeKey("l", { accelKey: true });
      Assert.equal(gURLBar.selectionStart, 0, "Selection start is 0.");
      Assert.equal(
        gURLBar.selectionEnd,
        gURLBar.value.length,
        "Selection end is at and of text."
      );
    },
    shouldUntrim: true,
  },
  {
    description: "Test drag selection untrims",
    untrimmedValue: BrowserUIUtils.trimURLProtocol + "www.example.com/",
    execute() {
      selectWithMouseDrag(100, 200);
      Assert.greater(gURLBar.selectionStart, 0, "Selection start is positive.");
      Assert.greater(
        gURLBar.selectionEnd,
        gURLBar.selectionStart,
        "Selection is not empty."
      );
    },
    shouldUntrim: true,
  },
  {
    description: "Test double click selection untrims",
    untrimmedValue: BrowserUIUtils.trimURLProtocol + "www.example.com/",
    execute() {
      selectWithDoubleClick(200);
      Assert.greater(gURLBar.selectionStart, 0, "Selection start is positive.");
      Assert.greater(
        gURLBar.selectionEnd,
        gURLBar.selectionStart,
        "Selection is not empty."
      );
    },
    shouldUntrim: true,
  },
  {
    description: "Test click, LEFT untrims",
    untrimmedValue: BrowserUIUtils.trimURLProtocol + "www.example.com/",
    execute() {
      EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
      EventUtils.synthesizeKey("KEY_ArrowLeft");
    },
    shouldUntrim: true,
  },
  {
    description: "Test HOME untrims",
    untrimmedValue: BrowserUIUtils.trimURLProtocol + "www.example.com/",
    execute() {
      EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
      if (AppConstants.platform == "macosx") {
        EventUtils.synthesizeKey("KEY_ArrowLeft", { metaKey: true });
      } else {
        EventUtils.synthesizeKey("KEY_Home");
      }
    },
    shouldUntrim: true,
  },
  {
    description: "Test SHIFT+LEFT untrims - partial host",
    untrimmedValue: BrowserUIUtils.trimURLProtocol + "www.example.com/",
    async execute() {
      EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
      EventUtils.synthesizeKey("KEY_ArrowLeft", { shiftKey: true });
      Assert.equal(
        gURLBar.selectionStart,
        BrowserUIUtils.trimURLProtocol.length,
        "Selection start is 0."
      );
      Assert.less(
        gURLBar.selectionEnd,
        gURLBar.value.length,
        "Selection skips last characters."
      );
    },
    shouldUntrim: true,
  },
  {
    description: "Test SHIFT+LEFT untrims - full host",
    untrimmedValue: BrowserUIUtils.trimURLProtocol + "www.example.com/test",
    async execute() {
      EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
      EventUtils.synthesizeKey("KEY_ArrowLeft", { shiftKey: true });
      Assert.equal(gURLBar.selectionStart, 0, "Selection start is 0.");
      Assert.less(
        gURLBar.selectionEnd,
        gURLBar.value.length,
        "Selection skips last characters."
      );
    },
    shouldUntrim: true,
  },
  {
    description: "Test repeated HOME untrims",
    untrimmedValue: BrowserUIUtils.trimURLProtocol + "www.example.com/",
    execute() {
      EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
      if (AppConstants.platform == "macosx") {
        EventUtils.synthesizeKey("KEY_ArrowLeft", {
          metaKey: true,
          repeat: 3,
        });
      } else {
        EventUtils.synthesizeKey("KEY_Home", { repeat: 3 });
      }
    },
    shouldUntrim: true,
  },
  {
    description: "Test CTRL+L untrims",
    untrimmedValue: BrowserUIUtils.trimURLProtocol + "www.example.com/",
    async execute() {
      await synthesizeKeyAndWaitFocus("l", { accelKey: true });
    },
    shouldUntrim: true,
  },
  {
    description: "Test CTRL+L untrims even after a revert and blur",
    untrimmedValue: BrowserUIUtils.trimURLProtocol + "www.example.com/",
    inNewTab: true,
    async execute() {
      await synthesizeKeyAndWaitFocus("l", { accelKey: true });
      gURLBar.handleRevert();
      Assert.equal(gURLBar.value, "www.example.com");
      gURLBar.blur();
      await synthesizeKeyAndWaitFocus("l", { accelKey: true });
    },
    shouldUntrim: true,
  },
  {
    description: "Test F6 untrims",
    untrimmedValue: BrowserUIUtils.trimURLProtocol + "www.example.com/",
    inNewTab: true,
    async execute() {
      await synthesizeKeyAndWaitFocus("VK_F6");
    },
    shouldUntrim: true,
  },
];

add_task(async function test_untrim() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.untrimOnUserInteraction.featureGate", true]],
  });

  for (let test of tests) {
    info(test.description);

    if (test.inNewTab) {
      await BrowserTestUtils.withNewTab(
        {
          gBrowser,
          url: test.untrimmedValue,
        },
        async function () {
          await test.execute();
          Assert.equal(
            gURLBar.value,
            test.shouldUntrim ? test.untrimmedValue : trimmedValue,
            `Value should be ${test.shouldUntrim ? "un" : ""}trimmed`
          );
        }
      );
      continue;
    }

    let trimmedValue = UrlbarTestUtils.trimURL(test.untrimmedValue);
    gURLBar._setValue(test.untrimmedValue, {
      allowTrim: true,
      valueIsTyped: false,
    });
    gURLBar.blur();
    Assert.equal(gURLBar.value, trimmedValue, "Value has been trimmed");
    await test.execute();
    Assert.equal(
      gURLBar.value,
      test.shouldUntrim ? test.untrimmedValue : trimmedValue,
      `Value should be ${test.shouldUntrim ? "un" : ""}trimmed`
    );
    gURLBar.handleRevert();
  }
});

async function synthesizeKeyAndWaitFocus(key, options = {}, win = window) {
  let focusPromise = BrowserTestUtils.waitForEvent(window, "focus", true);
  EventUtils.synthesizeKey(
    key,
    Object.assign({ type: "keydown" }, options),
    win
  );
  await focusPromise;
  EventUtils.synthesizeKey(key, Object.assign({ type: "keyup" }, options), win);
}
