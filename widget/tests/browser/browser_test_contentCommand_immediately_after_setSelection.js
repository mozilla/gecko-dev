/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function () {
  await BrowserTestUtils.withNewTab(
    "data:text/html,<html><body><textarea></textarea></body></html>",
    async function (browser) {
      await SimpleTest.promiseFocus(browser);

      for (const test of [
        { command: "cut" },
        { command: "delete" },
        { command: "insertText", data: "B" },
      ]) {
        // ab[]c
        await SpecialPowers.spawn(browser, [], () => {
          const textarea = content.document.querySelector("textarea");
          textarea.value = "abc";
          textarea.focus();
          textarea.selectionStart = "ab".length;
          textarea.selectionEnd = textarea.selectionStart;
          content.wrappedJSObject.promiseInput = new Promise(resolve =>
            textarea.addEventListener("input", () => resolve(textarea.value), {
              once: true,
            })
          );
        });

        // a[b]c
        window.windowUtils.sendSelectionSetEvent("a".length, "b".length);
        // a[]c or a[B]c
        window.windowUtils.sendContentCommandEvent(
          test.command,
          null,
          test.data
        );

        info("Waiting for input event on <textarea>...");
        const textareaValue = await SpecialPowers.spawn(browser, [], () => {
          return content.wrappedJSObject.promiseInput;
        });
        Assert.equal(
          textareaValue,
          `a${test.data || ""}c`,
          `${test.command} immediately after setting selection should affect to the focused <textarea> in the remote process`
        );

        // wait for a while to flush content cache.
        for (let i = 0; i < 10; i++) {
          await new Promise(resolve => requestAnimationFrame(resolve));
        }
      }
    }
  );
});
