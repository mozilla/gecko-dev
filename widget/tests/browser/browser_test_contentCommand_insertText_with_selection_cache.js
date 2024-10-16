/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../file_ime_state_test_helper.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/widget/tests/browser/file_ime_state_test_helper.js",
  this
);

add_task(async () => {
  await BrowserTestUtils.withNewTab(
    "data:text/html,<html><body><textarea></textarea></body></html>",
    async function (browser) {
      const tipWrapper = new TIPWrapper(window);
      ok(
        tipWrapper.isAvailable(),
        "TextInputProcessor should've been initialized"
      );

      function promiseSelectionChange() {
        return new Promise(resolve => {
          tipWrapper.onSelectionChange = aSelection => {
            tipWrapper.onSelectionChange = null;
            resolve(aSelection);
          };
        });
      }

      function waitForTicks() {
        return new Promise(resolve =>
          requestAnimationFrame(() => requestAnimationFrame(resolve))
        );
      }

      function promiseGetTextAreaValue() {
        return SpecialPowers.spawn(browser, [], () => {
          const textarea = content.document.querySelector("textarea");
          return content.Promise.resolve(textarea.value);
        });
      }

      await SpecialPowers.spawn(browser, [], () => {
        const textarea = content.document.querySelector("textarea");
        textarea.focus();
      });
      await waitForTicks();
      const windowUtils = window.windowUtils;

      // <textarea>a[]</textarea>
      {
        const waitForSelectionChange = promiseSelectionChange();
        await BrowserTestUtils.synthesizeKey("a", {}, browser);
        info(`Waiting for a selection change after inserting "a"...`);
        const selectionCache = await waitForSelectionChange;
        Assert.equal(
          selectionCache?.offset,
          1,
          `Selection offset should be 1 after inserting "a"`
        );
      }
      // <textarea>[a]</textarea>
      {
        const waitForSelectionChange = promiseSelectionChange();
        windowUtils.sendSelectionSetEvent(
          Math.max(tipWrapper.selectionCache.offset - 1, 0),
          1
        );
        info(`Waiting for a selection change of selecting "a"...`);
        await waitForSelectionChange;
      }
      // <textarea>A[]</textarea>
      {
        const waitForSelectionChange = promiseSelectionChange();
        windowUtils.sendContentCommandEvent("insertText", null, "A");
        info(`Waiting for a selection change after inserting "A"...`);
        await waitForSelectionChange;
        info(`Waiting for <textarea> value...`);
        Assert.equal(
          await promiseGetTextAreaValue(),
          "A",
          `"a" should be replaced with "A" by insertText command`
        );
      }
      // <textarea>A []</textarea>
      {
        const waitForSelectionChange = promiseSelectionChange();
        await BrowserTestUtils.synthesizeKey(" ", {}, browser);
        info(`Waiting for a selection change after inserting " "...`);
        const selectionCache = await waitForSelectionChange;
        Assert.equal(
          selectionCache?.offset,
          2,
          `Selection offset should be 2 after inserting " "`
        );
      }
      // <textarea>A b[]</textarea>
      {
        const waitForSelectionChange = promiseSelectionChange();
        await BrowserTestUtils.synthesizeKey("b", {}, browser);
        info(`Waiting for a selection change after inserting "b"...`);
        const selectionCache = await waitForSelectionChange;
        Assert.equal(
          selectionCache?.offset,
          3,
          `Selection offset should be 3 after inserting "b"`
        );
      }
      // <textarea>A [b]</textarea>
      {
        const waitForSelectionChange = promiseSelectionChange();
        windowUtils.sendSelectionSetEvent(
          Math.max(tipWrapper.selectionCache.offset - 1, 0),
          1
        );
        info(`Waiting for a selection change of selecting "b"...`);
        await waitForSelectionChange;
      }
      // <textarea>A B[]</textarea>
      {
        const waitForSelectionChange = promiseSelectionChange();
        windowUtils.sendContentCommandEvent("insertText", null, "B");
        info(`Waiting for a selection change after inserting "B"...`);
        await waitForSelectionChange;
        info(`Waiting for <textarea> value...`);
        Assert.equal(
          await promiseGetTextAreaValue(),
          "A B",
          `"b" should be replaced with "B" by insertText command`
        );
      }
    }
  );
});
