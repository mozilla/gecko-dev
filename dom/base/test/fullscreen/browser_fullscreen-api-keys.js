/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// List of key codes which should exit full-screen mode.
const kKeyList = [
  { key: "Escape", keyCode: "VK_ESCAPE", suppressed: true },
  { key: "F11", keyCode: "VK_F11", suppressed: false },
];

function receiveExpectedKeyEvents(aBrowser, aKeyCode, aTrusted) {
  return SpecialPowers.spawn(
    aBrowser,
    [aKeyCode, aTrusted],
    (keyCode, trusted) => {
      return new Promise(resolve => {
        let events = trusted
          ? ["keydown", "keyup"]
          : ["keydown", "keypress", "keyup"];
        if (trusted && keyCode == content.wrappedJSObject.KeyEvent.DOM_VK_F11) {
          // trusted `F11` key shouldn't be fired because of reserved when it's
          // a shortcut key for exiting from the full screen mode.
          events.shift();
        }
        function listener(event) {
          let expected = events.shift();
          Assert.equal(
            event.type,
            expected,
            `Should receive a ${expected} event`
          );
          Assert.equal(
            event.keyCode,
            keyCode,
            `Should receive the event with key code ${keyCode}`
          );
          if (!events.length) {
            content.document.removeEventListener("keydown", listener, true);
            content.document.removeEventListener("keyup", listener, true);
            content.document.removeEventListener("keypress", listener, true);
            resolve();
          }
        }

        content.document.addEventListener("keydown", listener, true);
        content.document.addEventListener("keyup", listener, true);
        content.document.addEventListener("keypress", listener, true);
      });
    }
  );
}

async function requestFullscreenAndWait(aBrowser) {
  await SimpleTest.promiseFocus(aBrowser);
  await SpecialPowers.spawn(aBrowser, [], async () => {
    // Wait for the document being activated, so that
    // fullscreen request won't be denied.
    await ContentTaskUtils.waitForCondition(
      () => content.browsingContext.isActive && content.document.hasFocus(),
      "document is active"
    );

    await new Promise(resolve => {
      content.document.addEventListener("fullscreenchange", resolve, {
        once: true,
      });
      content.document.body.requestFullscreen();
    });
  });
}

const kPage =
  "https://example.org/browser/" +
  "dom/base/test/fullscreen/file_fullscreen-api-keys.html";

add_setup(async function init() {
  await pushPrefs(
    ["full-screen-api.transition-duration.enter", "0 0"],
    ["full-screen-api.transition-duration.leave", "0 0"]
  );
});

for (let { key, keyCode, suppressed } of kKeyList) {
  /** Test for Bug 545812 **/
  add_task(async function testExitFullscreenByKeyboard() {
    let keyCodeValue = KeyEvent["DOM_" + keyCode];
    info(`Test keycode ${key} (${keyCodeValue})`);

    let tab = BrowserTestUtils.addTab(gBrowser, kPage);
    let browser = tab.linkedBrowser;
    gBrowser.selectedTab = tab;
    await waitForDocLoadComplete();

    // Wait for the document being activated, so that
    // fullscreen request won't be denied.
    await SimpleTest.promiseFocus(browser);
    await SpecialPowers.spawn(browser, [], () => {
      return ContentTaskUtils.waitForCondition(
        () => content.browsingContext.isActive && content.document.hasFocus(),
        "document is active"
      );
    });

    // Register listener to capture unexpected events
    let keyEventsCount = 0;
    let fullScreenEventsCount = 0;
    let removeFullScreenListener = BrowserTestUtils.addContentEventListener(
      browser,
      "fullscreenchange",
      () => fullScreenEventsCount++
    );
    let removeKeyDownListener = BrowserTestUtils.addContentEventListener(
      browser,
      "keydown",
      () => keyEventsCount++,
      { wantUntrusted: true }
    );
    let removeKeyPressListener = BrowserTestUtils.addContentEventListener(
      browser,
      "keypress",
      () => keyEventsCount++,
      { wantUntrusted: true }
    );
    let removeKeyUpListener = BrowserTestUtils.addContentEventListener(
      browser,
      "keyup",
      () => keyEventsCount++,
      { wantUntrusted: true }
    );

    let expectedFullScreenEventsCount = 0;
    let expectedKeyEventsCount = 0;

    info("Enter fullscreen");
    let state = new Promise(resolve => {
      let removeFun = BrowserTestUtils.addContentEventListener(
        browser,
        "fullscreenchange",
        async () => {
          removeFun();
          resolve(
            await SpecialPowers.spawn(browser, [], () => {
              return !!content.document.fullscreenElement;
            })
          );
        }
      );
    });
    // request fullscreen
    SpecialPowers.spawn(browser, [], () => {
      content.document.body.requestFullscreen();
    });
    ok(await state, "The content should have entered fullscreen");
    ok(document.fullscreenElement, "The chrome should also be in fullscreen");

    is(
      fullScreenEventsCount,
      ++expectedFullScreenEventsCount,
      "correct number of fullscreen events occurred"
    );

    info("Dispatch untrusted key events from content");
    let promiseExpectedKeyEvents = receiveExpectedKeyEvents(
      browser,
      keyCodeValue,
      false
    );

    SpecialPowers.spawn(browser, [keyCode], keyCodeChild => {
      var evt = new content.CustomEvent("Test:DispatchKeyEvents", {
        detail: Cu.cloneInto({ code: keyCodeChild }, content),
      });
      content.dispatchEvent(evt);
    });
    await promiseExpectedKeyEvents;

    expectedKeyEventsCount += 3;
    is(
      keyEventsCount,
      expectedKeyEventsCount,
      "correct number of key events occurred"
    );

    info("Send trusted key events");

    state = new Promise(resolve => {
      let removeFun = BrowserTestUtils.addContentEventListener(
        browser,
        "fullscreenchange",
        async () => {
          removeFun();
          resolve(
            await SpecialPowers.spawn(browser, [], () => {
              return !!content.document.fullscreenElement;
            })
          );
        }
      );
    });

    promiseExpectedKeyEvents = suppressed
      ? Promise.resolve()
      : receiveExpectedKeyEvents(browser, keyCodeValue, true);
    await SpecialPowers.spawn(browser, [], () => {});

    EventUtils.synthesizeKey("KEY_" + key);
    await promiseExpectedKeyEvents;

    ok(!(await state), "The content should have exited fullscreen");
    ok(
      !document.fullscreenElement,
      "The chrome should also have exited fullscreen"
    );

    is(
      fullScreenEventsCount,
      ++expectedFullScreenEventsCount,
      "correct number of fullscreen events occurred"
    );
    if (!suppressed) {
      expectedKeyEventsCount += keyCode == "VK_F11" ? 1 : 3;
    }
    is(
      keyEventsCount,
      expectedKeyEventsCount,
      "correct number of key events occurred"
    );

    info("Cleanup");
    removeFullScreenListener();
    removeKeyDownListener();
    removeKeyPressListener();
    removeKeyUpListener();
    gBrowser.removeTab(tab);
  });

  /** Test for Bug 1621736 **/
  // macOS places fullscreen windows in their own virtual desktop, and it is not
  // possible to programmatically move focus to another chrome window in a different
  // virtual desktop, so this test doesn't work on macOS.
  if (AppConstants.platform != "macosx") {
    add_task(async function testMultipleFullscreenExitByKeyboard() {
      let keyCodeValue = KeyEvent["DOM_" + keyCode];
      info(`Test keycode ${key} (${keyCodeValue})`);

      await BrowserTestUtils.withNewTab(
        {
          gBrowser,
          url: kPage,
        },
        async function (browser) {
          info("Enter fullscreen");
          await requestFullscreenAndWait(browser);

          info("Open new browser window");
          const win = await BrowserTestUtils.openNewBrowserWindow();
          const tab = await BrowserTestUtils.openNewForegroundTab(
            win.gBrowser,
            kPage
          );

          info("Enter fullscreen on new browser window");
          const newBrowser = tab.linkedBrowser;
          await requestFullscreenAndWait(newBrowser);

          let removeFullScreenListener;
          let promiseFullscreenExit = new Promise(resolve => {
            removeFullScreenListener = BrowserTestUtils.addContentEventListener(
              newBrowser,
              "fullscreenchange",
              resolve,
              {},
              event => {
                return !event.target.fullscreenElement;
              }
            );
          });

          info("Send key event to new browser window");
          EventUtils.synthesizeKey("KEY_" + key, {}, win);
          await promiseFullscreenExit;

          ok(
            await SpecialPowers.spawn(browser, [], () => {
              return (
                content.document.fullscreenElement == content.document.body
              );
            }),
            "First browser window should still in fullscreen mode"
          );

          info("Cleanup");
          removeFullScreenListener();

          // Close opened tab
          let tabClosed = BrowserTestUtils.waitForTabClosing(tab);
          await BrowserTestUtils.removeTab(tab);
          await tabClosed;

          // Close opened window
          await BrowserTestUtils.closeWindow(win);
        }
      );
    });
  }
}
